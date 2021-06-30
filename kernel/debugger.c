#include <wchar.h>

#include <jos.h>
#include <interrupts.h>
#include <hex_dump.h>
#include <kernel.h>
#include <debugger.h>
#include <serial.h>
#include <linear_allocator.h>
#include <extensions/json.h>
#include <extensions/base64.h>
#include <smp.h>
#include <pagetables.h>
#include <memory.h>
#include <tasks.h>
#include <pe.h>
#include <internal/_tasks.h>

#include <Zydis/Zydis.h>

#include <string.h>
#include <stdio.h>
#include <output_console.h>

static bool _debugger_connected = false;
#define FLAGS_TRAP_FLAG 0x100

typedef struct _debugger_packet_rw_target_memory {

    uint64_t    _address;
    uint32_t    _length;

} _JOS_PACKED debugger_packet_rw_target_memory_t;

#define INTEL_AMD_MAX_INSTRUCTION_LENGTH 15
typedef struct _debugger_packet_bp {
    interrupt_stack_t   _stack;
    // MAX 15 bytes for Intel/AMD instructions
    uint8_t             _instruction[INTEL_AMD_MAX_INSTRUCTION_LENGTH];
    uint64_t            _cr0;
    uint64_t            _cr2;
    uint64_t            _cr3;
    uint64_t            _cr4;

} _JOS_PACKED debugger_packet_bp_t;

typedef struct _debugger_packet_page_info {

    uintptr_t   _address;    

} _JOS_PACKED debugger_packet_page_info_t;

typedef struct _debugger_packet_page_info_resp {

    uintptr_t   _address;
    //NOTE: assumes 4 level paging!
    uintptr_t   _entries[4];

} _JOS_PACKED debugger_packet_page_info_resp_t;

typedef struct _debugger_task_info_header {
    
    uint32_t    _num_tasks;
    uint32_t    _task_context_size;

} _JOS_PACKED debugger_task_info_header_t;

typedef struct _debugger_task_info {
    // truncated name, 0 terminated
    char                _name[MAX_TASK_NAME_LENGTH+1];
    uint64_t            _entry_pt;
    interrupt_stack_t    _stack;
} _JOS_PACKED debugger_task_info_t;

typedef struct _debugger_packet_rdmsr {
    uint32_t _msr;
} _JOS_PACKED debugger_packet_rdmsr_t;

typedef struct _debugger_packet_rdmsr_resp {
    uint32_t    _msr;
    uint32_t    _lo;
    uint32_t    _hi;
} _JOS_PACKED debugger_packet_rdmsr_resp_t;

static peutil_pe_context_t* _pe_ctx = 0;
static ZydisDecoder _zydis_decoder;
static const char* kDebuggerChannel = "debugger";

// ==============================================
//TODO: TESTING:
typedef struct _debugger_breakpoint {

    bool        _active;
    uintptr_t   _at;
    uint8_t     _instr_byte;    

} debugger_breakpoint_t;
#define _MAX_BREAKPOINTS 16
#define _BREAKPOINT_INSTR 0xcc
static debugger_breakpoint_t _breakpoints[_MAX_BREAKPOINTS];
// tracks the last runtime bp we've hit so that we can restore it after a trap
static debugger_breakpoint_t _last_rt_bp;
static size_t _num_breakpoints = 0;

_JOS_API_FUNC void debugger_set_breakpoint(uintptr_t at) {    
    // first check if the breakpoint is already set
    bool existing = false;
    for ( size_t bp = 0; bp < _num_breakpoints; ++bp ) {
        if ( _breakpoints[bp]._at == at ) {
            // re-activate
            _JOS_KTRACE_CHANNEL(kDebuggerChannel, "breakpoint re-activated at 0x%llx", at);
            uint8_t instr_byte = ((uint8_t*)at)[0];
            if ( instr_byte!=_BREAKPOINT_INSTR ) {
                _breakpoints[bp]._instr_byte = instr_byte;
                ((uint8_t*)at)[0] = _BREAKPOINT_INSTR;
            }            
            _breakpoints[bp]._active = true;
            existing = true;
            break;
        }
    }
    if ( !existing ) {
        _JOS_ASSERT(_num_breakpoints<_MAX_BREAKPOINTS);
        _JOS_KTRACE_CHANNEL(kDebuggerChannel, "breakpoint set at 0x%llx", at);
        _breakpoints[_num_breakpoints]._at = at;
        _breakpoints[_num_breakpoints]._instr_byte = ((uint8_t*)at)[0];
        ((uint8_t*)at)[0] = _BREAKPOINT_INSTR;
        _breakpoints[_num_breakpoints]._active = true;
        ++_num_breakpoints;
    }
}

static debugger_breakpoint_t* _debugger_breakpoint_at(uintptr_t at)  {
    for ( size_t bp = 0; bp < _num_breakpoints; ++bp ) {
        if ( _breakpoints[bp]._at == at ) {
            return _breakpoints+bp;
        }
    }
    return 0;
}
// ==============================================

#define _CLEAR_TF(isr_stack) isr_stack->rflags &= ~(1<<8)
#define _SET_TF(isr_stack) isr_stack->rflags |= (1<<8)

// wait for debugger commands.
// if isr_stack == 0 this will not allow continuing or single stepping (used by asserts)
static void _debugger_loop(interrupt_stack_t * isr_stack) {
    if ( !debugger_is_connected() )
        return;
   
    bool continue_run = false;
    while(!continue_run) {
        debugger_serial_packet_t packet;
        debugger_read_packet_header(&packet);
        switch(packet._id) {
            case kDebuggerPacket_ReadTargetMemory:
            {                
                //_JOS_KTRACE_CHANNEL("debugger", "kDebuggerPacket_ReadTargetMemory");
                debugger_packet_rw_target_memory_t rt_packet;
                debugger_read_packet_body(&packet, (void*)&rt_packet, packet._length);
                //_JOS_KTRACE_CHANNEL("debugger", "kDebuggerPacket_ReadTargetMemory 0x%llx, %d bytes", rt_packet._address, rt_packet._length);
                if( rt_packet._length ) {                    
                    // serialise directly from memory
                    debugger_send_packet(kDebuggerPacket_ReadTargetMemory_Resp, (void*)rt_packet._address, rt_packet._length);
                }
            }
            break;            
            case kDebuggerPacket_WriteTargetMemory:
            {
                debugger_packet_rw_target_memory_t rt_packet;
                debugger_read_packet_body(&packet, (void*)&rt_packet, sizeof(rt_packet));
                //TODO: sanity checks!
                if( rt_packet._length ) {
                    // serialise directly to memory
                    serial_read(kCom1, (char*)rt_packet._address, rt_packet._length);
                }
            }
            break;
            case kDebuggerPacket_TraversePageTable:
            {
                debugger_packet_page_info_t page_info_packet;
                debugger_read_packet_body(&packet, (void*)&page_info_packet, sizeof(page_info_packet));
                debugger_packet_page_info_resp_t resp_packet;
                resp_packet._address = page_info_packet._address;
                pagetables_traverse_tables((void*)page_info_packet._address, resp_packet._entries, 4);
                debugger_send_packet(kDebuggerPacket_TraversePageTable_Resp, (void*)&resp_packet, sizeof(resp_packet));
            }
            break;
            case kDebuggerPacket_Get_TaskList:
            {
                //TODO:
            }
            break;
            case kDebuggerPacket_RDMSR:
            {
                debugger_packet_rdmsr_t rdmsr_packet;
                debugger_read_packet_body(&packet, (void*)&rdmsr_packet, sizeof(rdmsr_packet));
                debugger_packet_rdmsr_resp_t resp_packet;
                resp_packet._msr = rdmsr_packet._msr;
                uint32_t lo, hi;
                x86_64_rdmsr(rdmsr_packet._msr, &lo, &hi);
                resp_packet._lo = lo;
                resp_packet._hi = hi;
                debugger_send_packet(kDebuggerPacket_RDMSR_Resp, (void*)&resp_packet, sizeof(resp_packet));
            }
            break;
            case kDebuggerPacket_SingleStep:
            {
                if ( isr_stack ) {
                    // switch on the trap flag so that it will trigger on the next instruction after our iret
                    _SET_TF(isr_stack);
                    continue_run = true;
                }
            }
            break;
            // case kDebuggerPacket_StepOver:
            // {
            //     if ( isr_stack ) {
            //         // check if the next instruction is indeed something to skip, i.e. a call
            //         ZydisDecodedInstruction instruction;
            //         if (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&_zydis_decoder, isr_stack->rip, INTEL_AMD_MAX_INSTRUCTION_LENGTH, &instruction)) ) {
            //             if ( instruction.mnemonic == ZYDIS_MNEMONIC_CALL ) {
            //                 // we can skip this instruction so we'll set a bp after it and continue execution
            //                 debugger_set_breakpoint(isr_stack->rip + instruction.length);
            //                 _CLEAR_TF(isr_stack);
            //             }
            //         }
            //     }
            // }
            // break;
            case kDebuggerPacket_Continue:
            {
                if ( isr_stack ) {
                    _CLEAR_TF(isr_stack);
                    continue_run = true;
                }
            }
            break;
            default:
            {
                _JOS_KTRACE_CHANNEL("debugger", "unhandled packet id %d, length %d", packet._id, packet._length);
            }
            break;
        }
    }
}

_JOS_API_FUNC void debugger_trigger_assert(const char* cond, const char* file, int line) {
    
    char json_buffer[512];
    IO_FILE stream;
    memset(&stream,0,sizeof(FILE));
    _io_file_from_buffer(&stream, json_buffer, sizeof(json_buffer));

    json_writer_context_t ctx;
    json_initialise_writer(&ctx, &stream);

    json_write_object_start(&ctx);
        json_write_key(&ctx, "assert");
            json_write_object_start(&ctx);
                json_write_key(&ctx, "cond");
                json_write_string(&ctx, cond);
                json_write_key(&ctx, "file");
                json_write_string(&ctx, file);
                json_write_key(&ctx, "line");
                json_write_number(&ctx, line);
            json_write_object_end(&ctx);        
    json_write_object_end(&ctx);

    uint32_t json_size = (uint32_t)ftell(&stream);
    debugger_send_packet(kDebuggerPacket_Assert, (void*)json_buffer, json_size);
    _debugger_loop(0);
}

static void _decode_instruction(const void* at, void* buffer) {
    // decode the instruction @ rip so that we can send it to the debugger for display
    ZydisDecodedInstruction instruction;
    if (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&_zydis_decoder, at, INTEL_AMD_MAX_INSTRUCTION_LENGTH, &instruction)) ) {
        memcpy(buffer, (const void*)at, instruction.length);
    }
    else {
        // shouldn't really ever happen...
        memset(buffer, 0, INTEL_AMD_MAX_INSTRUCTION_LENGTH);
    }
}

static void _fill_in_debugger_packet(debugger_packet_bp_t* bp_info, interrupt_stack_t * isr_stack) {
    memcpy(&bp_info->_stack, isr_stack, sizeof(interrupt_stack_t));
    _decode_instruction((const void*)isr_stack->rip, bp_info->_instruction);
    bp_info->_cr0 = x86_64_read_cr0();
    bp_info->_cr2 = x86_64_read_cr2();
    bp_info->_cr3 = x86_64_read_cr3();
    bp_info->_cr4 = x86_64_read_cr4();
}

_JOS_API_FUNC void debugger_disasm(void* at, size_t bytes, wchar_t* output_buffer, size_t output_buffer_length) {
    
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // Loop over the instructions in our buffer.
    ZyanU64 runtime_address = (ZyanU64)at;
    ZyanUSize offset = 0u;
    const ZyanUSize length = 50u;
    ZydisDecodedInstruction instruction;
    while (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&decoder, (const void*)runtime_address, length - offset, &instruction)))
    {
        // Format & print the binary instruction structure to human readable format
        char instruction_buffer[256];
        ZydisFormatterFormatInstruction(&formatter, &instruction, instruction_buffer, sizeof(instruction_buffer), runtime_address);

#define _CHECK_OUTPUT_LENGTH(w)\
        if( output_buffer_length >= w ) {\
            output_buffer_length -= w;\
        }\
        else {\
            output_buffer[output_buffer_length-2] = L'@';\
            output_buffer[output_buffer_length-1] = 0;\
            return;\
        }

        // write address
        int written = swprintf(output_buffer, output_buffer_length, L"%016llx  ", runtime_address);
        output_buffer += written;
        _CHECK_OUTPUT_LENGTH(written);

        // write instruction bytes
        for(ZyanU8 b = 0; b < instruction.length; ++b) {
            swprintf(output_buffer, output_buffer_length, L"%02x ", ((uint8_t*)runtime_address)[b]);
            output_buffer += 3;
            _CHECK_OUTPUT_LENGTH(3);
        }

        // the maximum width of an instruction is 15 bytes (x86_64), so adjust to the max width
        int gap = 45 - 3*instruction.length;
        if(gap>0) {
            _CHECK_OUTPUT_LENGTH(gap);
            for(int c = 0; c < gap; ++c) {
                *output_buffer++ = L' ';
            }
        }
        
        // write disassembly
        written = swprintf(output_buffer, output_buffer_length, L" %S\n", instruction_buffer);
        output_buffer += written;
        _CHECK_OUTPUT_LENGTH(written);
        
        offset += instruction.length;
        runtime_address += instruction.length;
    }

    output_buffer[output_buffer_length-1] = 0;
}

//NOTE: this handles both int 3 and GPFs if a debugger is connected
static void _debugger_isr_handler(interrupt_stack_t * stack) {
    
    if ( debugger_is_connected() ) {

        debugger_breakpoint_t* bp = 0;

        // -------------------------------------- running in debugger
        switch ( stack->handler_id ) {
            case 1: // TRAP 
            case 3: // breakpoint
            {       
                if ( _last_rt_bp._active ) {
                    // we're trapping after a runtime bp instruction
                    // we now need to restore it by poking back in the 0xcc byte and continue

                    uint8_t instr = ((uint8_t*)_last_rt_bp._at)[0];
                    _JOS_ASSERT(instr!=_BREAKPOINT_INSTR);
                    _last_rt_bp._instr_byte = instr;
                    // reset to int 3
                    ((uint8_t*)_last_rt_bp._at)[0] = _BREAKPOINT_INSTR;
                    _last_rt_bp._active = false;
                    _CLEAR_TF(stack);

                    // we're done here
                    return;
                }
                else {
                    // first check if we've hit a programmatic breakpoint
                    bp = _debugger_breakpoint_at(stack->rip - 1);
                    if ( bp && bp->_active ) {
                        _JOS_KTRACE_CHANNEL(kDebuggerChannel, "hit programmatic bp @ 0x%llx", bp->_at);
                        // re-set instruction
                        ((uint8_t*)bp->_at)[0] = bp->_instr_byte;
                        // go back so that we'll execute the original instruction next
                        --stack->rip;                                            
                    }

                    if ( !bp || bp->_active ) {
                        // _JOS_KTRACE_CHANNEL("debugger", "breakpoint hit at 0x%016llx", context->rip);
                        debugger_packet_bp_t bp_info;
                        _fill_in_debugger_packet(&bp_info, stack);
                        debugger_send_packet(kDebuggerPacket_Breakpoint, &bp_info, sizeof(bp_info));
                    }
                }
            }
            break;
            case 6: // UD#
            {
                debugger_packet_bp_t bp_info;
                _fill_in_debugger_packet(&bp_info, stack);
                debugger_send_packet(kDebuggerPacket_UD, &bp_info, sizeof(bp_info));
            }
            break;
            case 13: // #GPF
            {
                debugger_packet_bp_t bp_info;
                _fill_in_debugger_packet(&bp_info, stack);
                debugger_send_packet(kDebuggerPacket_GPF, &bp_info, sizeof(bp_info));
            }
            break;
            case 14: // #PF
            {
                _JOS_KTRACE_CHANNEL(kDebuggerChannel, "PF# at 0x%016llx", stack->rip);
                //TODO:
            }
            break;
            default:;
        }

        // enter loop waiting for further instructions
        _debugger_loop(stack);

        if ( bp && bp->_active ) {
            // if we are coming out of a runtime breakpoint that's still active we need to re-set it
            _last_rt_bp._active = true;
            _last_rt_bp._at = stack->rip;
            _last_rt_bp._instr_byte = bp->_instr_byte;
            // make sure we trap immediately after this instruction again so that we can restore it
            _SET_TF(stack);
        }
        else {
            _last_rt_bp._active = false;
        }
    }
    // else: trigger assert?       
}

_JOS_API_FUNC void debugger_initialise(void) {

    _last_rt_bp._active = false;

    // we "enter" the debugger with int 3 so we need to register this from the start
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=0x3, ._handler=_debugger_isr_handler });
    
    ZydisDecoderInit(&_zydis_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

    output_console_output_string(L"debug handler initialised\n");
}

_JOS_API_FUNC void debugger_wait_for_connection(peutil_pe_context_t* pe_ctx, uint64_t image_base) {
    
    _pe_ctx = pe_ctx;

    static const char dbg_conn_id[4] = {'j','o','s','x'};
    int conn_id_pos = 0;
    char in_char = serial_getch(kCom1, 1);
    while(true) {
        if ( in_char == dbg_conn_id[conn_id_pos] ) {
            ++conn_id_pos;
            if ( conn_id_pos == 4 ) {
                break;
            }
        }
        else {
            conn_id_pos = 0;
        }
        in_char = serial_getch(kCom1, 1);
    }
    
    _debugger_connected = true;
    // from this point on we want to control a number of traps and faults (in addition to int 3)

    // debug trap (single step)
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=1, ._handler=_debugger_isr_handler });
    // undefined instruction fault
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=6, ._handler=_debugger_isr_handler });
    // general protection fault
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=13, ._handler=_debugger_isr_handler });
    // page fault
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=14, ._handler=_debugger_isr_handler });

    char json_buffer[1024];
    IO_FILE stream;
    memset(&stream,0,sizeof(FILE));
    _io_file_from_buffer(&stream, json_buffer, sizeof(json_buffer));

    json_writer_context_t ctx;
    json_initialise_writer(&ctx, &stream);

    json_write_object_start(&ctx);
        json_write_key(&ctx, "version");
            json_write_object_start(&ctx);
                json_write_key(&ctx, "major");
                json_write_number(&ctx, 0);
                json_write_key(&ctx, "minor");
                json_write_number(&ctx, 1);
                json_write_key(&ctx, "patch");
                json_write_number(&ctx, 0);
            json_write_object_end(&ctx);
        json_write_key(&ctx, "image_info");
            json_write_object_start(&ctx);
                json_write_key(&ctx, "base");
                json_write_number(&ctx, (long long)image_base);
                json_write_key(&ctx, "entry_point");
                json_write_number(&ctx, (long long)peutil_entry_point(pe_ctx));
            json_write_object_end(&ctx);
        json_write_key(&ctx, "system_info");
            json_write_object_start(&ctx);
                json_write_key(&ctx, "processors");
                json_write_number(&ctx, smp_get_processor_count());
                json_write_key(&ctx, "memory");
                json_write_number(&ctx, memory_get_total());
            json_write_object_end(&ctx);
    json_write_object_end(&ctx);

    uint32_t json_size = (uint32_t)ftell(&stream);
    debugger_send_packet(kDebuggerPacket_KernelConnectionInfo, json_buffer, json_size);
}

_JOS_API_FUNC bool debugger_is_connected(void) {
    return _debugger_connected;
}

_JOS_API_FUNC void debugger_send_packet(debugger_packet_id_t id, void* data, uint32_t length) {
    if ( !_debugger_connected ) {
        return;
    }
    debugger_serial_packet_t packet = { ._id = (uint32_t)id, ._length = length };
    serial_write(kCom1, (const char*)&packet, sizeof(packet));
    if ( length ) {
        serial_write(kCom1, data, length);
    }
}

_JOS_API_FUNC void debugger_read_packet_header(debugger_serial_packet_t* packet) {    
    // simply read 8 raw bytes into the structure
    serial_read(kCom1, (char*)packet, sizeof(debugger_serial_packet_t));
}

_JOS_API_FUNC void debugger_read_packet_body(debugger_serial_packet_t* packet, void* buffer, uint32_t buffer_size) {
    if( !packet->_length )
        return;
    assert(packet->_length <= buffer_size);
    serial_read(kCom1, (char*)buffer, packet->_length);
}

_JOS_API_FUNC void debugger_ext_break(void) {
    _JOS_GDB_DBGBREAK();
}