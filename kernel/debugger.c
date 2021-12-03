#include <wchar.h>

#include <jos.h>
#include <interrupts.h>
#include <hex_dump.h>
#include <kernel.h>
#include <debugger.h>
#include <serial.h>
#include <collections.h>
#include <arena_allocator.h>
#include <extensions/json.h>
#include <extensions/base64.h>
#include <smp.h>
#include <pagetables.h>
#include <cpuid.h>
#include <memory.h>
#include <tasks.h>
#include <pe.h>
#include <internal/_tasks.h>

#include <Zydis/Zydis.h>

#include <string.h>
#include <stdio.h>
#include <output_console.h>

#include <internal/_debugger.h>

static bool _debugger_connected = false;
#define FLAGS_TRAP_FLAG 0x100

static peutil_pe_context_t* _pe_ctx = 0;
static ZydisDecoder _zydis_decoder;
static const char* kDebuggerChannel = "debugger";

typedef struct _debugger_breakpoint {

    uintptr_t   _at;
    uint8_t     _instr_byte;
    bool        _active:1;
    // a transient breakpoint is one which is known to the kernel only and 
    // which will be removed as soon as it's hit. we use this for step-over functionality
    bool 		_transient:1;

} debugger_breakpoint_t;

#define _BREAKPOINT_INSTR 0xcc
static vector_t _breakpoints;
static generic_allocator_t*  _allocator = 0;
// tracks the last runtime bp we've hit so that we can restore it after a trap
static debugger_breakpoint_t _last_rt_bp;
static debugger_packet_id_t _last_command = kDebuggerPacket_End;


#define _CLEAR_TF(isr_stack) isr_stack->rflags &= ~(1<<8)
#define _SET_TF(isr_stack) isr_stack->rflags |= (1<<8)
#define _DISABLE_BP_IF_ACTIVE(bp)\
if (bp->_active) {\
    ((uint8_t*)bp->_at)[0] = bp->_instr_byte;\
        bp->_active = false;\
}
#define _RESTORE_BP_IF_INACTIVE(bp)\
if (!bp->_active) {\
    bp->_instr_byte = ((uint8_t*)bp->_at)[0];\
        ((uint8_t*)bp->_at)[0] = _BREAKPOINT_INSTR;\
            bp->_active = true;\
}

// create a new or update existing breakpoint to ACTIVE
static debugger_breakpoint_t* _set_breakpoint(uintptr_t at) {
    // first check if the breakpoint is already set
    bool existing = false;
    const size_t num_bps = vector_size(&_breakpoints);
    debugger_breakpoint_t* bp = vector_data(&_breakpoints);
    for ( size_t bpi = 0; bpi < num_bps; ++bpi ) {
        if ( bp->_at == at ) {
            // re-activate
            //_JOS_KTRACE_CHANNEL(kDebuggerChannel, "breakpoint re-activated at 0x%llx", at);

            uint8_t instr_byte = ((uint8_t*)at)[0];
            if ( instr_byte!=_BREAKPOINT_INSTR ) {
                bp->_instr_byte = instr_byte;
                ((uint8_t*)at)[0] = _BREAKPOINT_INSTR;
            }            
            bp->_active = true;
            bp->_transient = false;
            existing = true;
            break;
        }
        ++bp;
    }
    if ( !existing ) {
        //_JOS_KTRACE_CHANNEL(kDebuggerChannel, "breakpoint set at 0x%llx", at);

        debugger_breakpoint_t new_bp = { ._at = at, ._instr_byte = ((uint8_t*)at)[0], ._active = true };
        ((uint8_t*)at)[0] = _BREAKPOINT_INSTR;
        vector_push_back(&_breakpoints, &new_bp);
        bp = vector_at(&_breakpoints, vector_size(&_breakpoints)-1);
    }
    return bp;
}

_JOS_API_FUNC void debugger_set_breakpoint(uintptr_t at) {    
    _set_breakpoint(at);
}

static size_t _remove_breakpoint(debugger_breakpoint_t* bp_to_remove) {
    // swap in last element to remove this one	
    debugger_breakpoint_t* bps = vector_data(&_breakpoints);
    const size_t end = vector_size(&_breakpoints);
    memcpy(bp_to_remove, bps + end - 1, sizeof(debugger_breakpoint_t));
    return end - 1;
}

static debugger_breakpoint_t* _debugger_breakpoint_at(uintptr_t at)  {
    const size_t num_bps = vector_size(&_breakpoints);
    debugger_breakpoint_t* bp = vector_data(&_breakpoints);
    for ( size_t bpi = 0; bpi < num_bps; ++bpi ) {
        if ( bp->_at == at ) {
            // TODO: strictly this should be the index since the vector can re-scale
            // but we're invoking this function in a controlled serial way so we're good for now...
            return bp;
        }
        ++bp;
    }
    return 0;
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
    bp_info->_call_stack_size = 0;
}

static void _trace_hive_values(const char* key, vector_t* values, void* user_data) {
    (void)user_data;
	const size_t num_elements = vector_size(values);
    if ( !num_elements ) {
        _JOS_KTRACE_CHANNEL("hive", key);
    }
    else {
        _JOS_KTRACE_CHANNEL("hive", "%s : ", key);
        for (size_t n = 0; n < num_elements; ++n) {
            hive_value_t* hive_value = (hive_value_t*)vector_at(values, n);
            hive_value_type_t type = hive_value->type;
            switch (type) {
            case kHiveValue_Int:
            {
                _JOS_KTRACE_CHANNEL("hive", "\t%lld", hive_value->value.as_int);
            }
            break;
            case kHiveValue_Str:
            {
                _JOS_KTRACE_CHANNEL("hive", "\t%s", hive_value->value.as_str);
            }
            break;
            case kHiveValue_Ptr:
            {
                _JOS_KTRACE_CHANNEL("hive", "\t0x%016llx", hive_value->value.as_ptr);
            }
            break;
            default:;
            }
        }
    }
}

// wait for debugger commands.
// if isr_stack == 0 this will not allow continuing or single stepping (used by asserts)
static void _debugger_loop(interrupt_stack_t * isr_stack) {	

    if ( !debugger_is_connected() )
        return;

    //_JOS_KTRACE_CHANNEL(kDebuggerChannel, "entering debugger, stack is 0x%llx", isr_stack);
   
    bool continue_run = false;
    while(!continue_run) {
        debugger_serial_packet_t packet;
        debugger_read_packet_header(&packet);
        _last_command = packet._id;

        switch(packet._id) {
            case kDebuggerPacket_UpdateBreakpoints:
            {
                // NOTE:
                // this assumes that the debugger and kernel maintain a synchronised list of breakoints
                // and that the debugger sends the full list of breakpoints whenever something changes.
                // if the list of breakpoints is large (100's) then this is not efficient but that is an optimisation problem 
                // to be addressed later - if at all required.

                debugger_packet_breakpoint_info_t info;
                int num_packets = packet._length / sizeof(info);
                if (num_packets == 0) {
                    // clear all breakpoints

                    // first restore all instruction bytes
                    const size_t num_bps = vector_size(&_breakpoints);					
                    for (size_t bpi = 0; bpi < num_bps; ++bpi) {
                        debugger_breakpoint_t* bp = (debugger_breakpoint_t*)vector_at(&_breakpoints, bpi);						
                        _DISABLE_BP_IF_ACTIVE(bp);
                    }					
                    vector_clear(&_breakpoints);
                }
                else {
                    // update specific breakpoints
                    void* packet_buffer = _allocator->alloc(_allocator, packet._length);
                    _JOS_ASSERT(packet_buffer);
                    debugger_read_packet_body(&packet, packet_buffer, packet._length);
                    debugger_packet_breakpoint_info_t* bpinfo = (debugger_packet_breakpoint_info_t*)packet_buffer;
                    
                    // _JOS_KTRACE_CHANNEL(kDebuggerChannel, "updating %d breakpoints", num_packets);
                    
                    while (num_packets-- > 0) {

                        debugger_breakpoint_t* bp = _debugger_breakpoint_at(bpinfo->_at);
                        if (bp) {
                            switch (bpinfo->_edc) {
                                case _BREAKPOINT_STATUS_ENABLED:
                                    _RESTORE_BP_IF_INACTIVE(bp);
                                    break;
                                case _BREAKPOINT_STATUS_DISABLED:
                                    _DISABLE_BP_IF_ACTIVE(bp);
                                    break;
                                case _BREAKPOINT_STATUS_CLEARED:
                                {
                                    _DISABLE_BP_IF_ACTIVE(bp);
                                    _remove_breakpoint(bp);
                                }
                                break;
                                default:;
                            }
                        }
                        else {
                            // new breakpoint, just add it to the list
                            debugger_breakpoint_t new_bp = { 
                                ._at = bpinfo->_at, 
                                ._instr_byte = ((uint8_t*)bpinfo->_at)[0], 
                                ._active = bpinfo->_edc == _BREAKPOINT_STATUS_ENABLED 
                            };
                            if (new_bp._active) {
                                ((uint8_t*)bpinfo->_at)[0] = _BREAKPOINT_INSTR;
                            }							
                            vector_push_back(&_breakpoints, &new_bp);
                            
                            //_JOS_KTRACE_CHANNEL(kDebuggerChannel, "adding new breakpoint @ 0x%llx", bpinfo->_at);
                        }
                    }
                    
                    _allocator->free(_allocator, packet_buffer);					
                }
            }
            break;
            case kDebuggerPacket_ReadTargetMemory:
            {                
                //_JOS_KTRACE_CHANNEL("debugger", "kDebuggerPacket_ReadTargetMemory");
                debugger_packet_rw_target_memory_t rt_packet;
                debugger_read_packet_body(&packet, (void*)&rt_packet, packet._length);                
                if( rt_packet._length ) {                    
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
            case kDebuggerPacket_CPUID:
            {
                uint32_t leaf_subleaf[2];
                debugger_read_packet_body(&packet, (void*)leaf_subleaf, sizeof(leaf_subleaf));
                uint32_t leaf_and_regs[6] = { 0, 0, 0, 0, 0, 0 };
                leaf_and_regs[0] = leaf_subleaf[0];
                leaf_and_regs[1] = leaf_subleaf[1];
                __get_cpuid_count(leaf_and_regs[0], leaf_and_regs[1],
                                 leaf_and_regs + 2, leaf_and_regs + 3, leaf_and_regs + 4, leaf_and_regs + 5);
                debugger_send_packet(kDebuggerPacket_CPUID_Resp, (void*)leaf_and_regs, sizeof(leaf_and_regs));
            }
            break;
            case kDebuggerPacket_MemoryMap:
            {
                //TODO: for now this just traces info
                size_t on_boot;
                size_t now;
                kernel_memory_available(&on_boot, &now);
                _JOS_KTRACE_CHANNEL("memory", "free kernel memory on startup %lld MB", on_boot>>20);
                _JOS_KTRACE_CHANNEL("memory", "free kernel memory now %lld MB", now>>20);

                unsigned short total;
                unsigned char lowmem, highmem;
                
                x86_64_outb(0x70, 0x30);
                lowmem = x86_64_inb(0x71);
                x86_64_outb(0x70, 0x31);
                highmem = x86_64_inb(0x71);                
                total = lowmem | highmem << 8;
                _JOS_KTRACE_CHANNEL("memory", "CMOS reports %d", total);

                _memory_debugger_dump_map();
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
            // case kDebuggerPacket_HiveGet:
            // {
            //     debugger_bstr_t key_name;
            //     debugger_read_packet_body(&packet, (void*)&key_name, sizeof(key_name));
            //     static char key_name_buffer[128];
            //     char* key_name_ptr = key_name_buffer;
            //     if ( key_name._len > sizeof(key_name_buffer) ) {
            //         key_name_ptr = _allocator->alloc(_allocator, key_name._len+1);
            //     }
            //     serial_read(kCom1, key_name_ptr, key_name._len);
            //     if ( key_name_ptr != key_name_buffer ) {
            //         _allocator->free(_allocator, key_name_ptr);
            //     }
            //     key_name_ptr[key_name._len] = 0;
            //     hive_get(kernel_hive(), key_name_ptr, values);
            // }
            // break;
            case kDebuggerPacket_HiveDump:
            {
                vector_t value_storage;
                vector_create(&value_storage, 16, sizeof(hive_value_t), _allocator);
                hive_visit_values(kernel_hive(), _trace_hive_values, &value_storage, 0);
                vector_destroy(&value_storage);
            }
            break;
            case kDebuggerPacket_TraceStep:
            {
                if ( isr_stack ) {
                    // switch on the trap flag so that it will trigger on the next instruction after our iret
                    _SET_TF(isr_stack);
                    continue_run = true;
                }
            }
            break;
            case kDebuggerPacket_SingleStep:
            {
                if ( isr_stack ) {
                    // check if the next instruction is indeed something to skip, i.e. a call
                    ZydisDecodedInstruction instruction;
                    if (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&_zydis_decoder, (void*)isr_stack->rip, INTEL_AMD_MAX_INSTRUCTION_LENGTH, &instruction)) ) {
                        if ( instruction.mnemonic == ZYDIS_MNEMONIC_CALL ) {
                            // we can skip this instruction so we'll set a bp after it and continue execution
                            debugger_breakpoint_t* bp = _set_breakpoint(isr_stack->rip + instruction.length);
                            // this is a TRANSIENT breakpoint, i.e. it will be removed as soon as it's hit
                            bp->_transient = true;
                            _CLEAR_TF(isr_stack);
                        }
                        else {
                            // if it's not a CALL we just treat it as a normal single instruction step (and we will in fact pretend it is)							
                            _last_command = kDebuggerPacket_TraceStep;
                            _SET_TF(isr_stack);
                        }
                        continue_run = true;
                    }
                }
            }
            break;
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
                _JOS_KTRACE_CHANNEL(kDebuggerChannel, "unhandled packet id %d, length %d", packet._id, packet._length);
            }
            break;
        }
    }
}


//this is the core of the debugger and handles int3, int1, faults, and breakpoints.
static void _debugger_isr_handler(interrupt_stack_t * stack) {
    
    if ( debugger_is_connected() ) {
        
        debugger_breakpoint_t* bp = 0;
        
        // -------------------------------------- running in debugger
        switch ( stack->handler_id ) {
            case 1: // TRAP 
            case 3: // breakpoint
            {   				
                //TODO: check debugger condition
                //  Intel dev manual 17.2
                uint64_t _JOS_MAYBE_UNUSED dr6 = x86_64_read_dr6();
                
                // see below: we have to restore breakpoint instructions 
                // whenever we pass a dynamic (runtime) breakpoint and that is done here.
                // _last_rt_bp is set to the last bp location and used to restore it
                if ( _last_rt_bp._active ) {

                    uint8_t instr = ((uint8_t*)_last_rt_bp._at)[0];
                    _last_rt_bp._instr_byte = instr;
                    // reset to int 3
                    ((uint8_t*)_last_rt_bp._at)[0] = _BREAKPOINT_INSTR;
                    _last_rt_bp._active = false;

                    // if we're not in the middle of a genuine trace we can clear TF
                    // if we don't do this then a runtime bp followed by a trace/single-step command 
                    // will result in the program continuning execution immediately, instead 
                    // of waiting in the debugger loop. 
                    if (_last_command != kDebuggerPacket_TraceStep) {
                        _CLEAR_TF(stack);
                        // we're done here
                        return;
                    }
                }
                
                // check if we've hit a programmatic breakpoint
                bp = _debugger_breakpoint_at(stack->rip - 1);
                if ( bp && bp->_active ) {
                    //_JOS_KTRACE_CHANNEL(kDebuggerChannel, "hit programmatic bp @ 0x%llx", bp->_at);

                    // re-set instruction byte
                    ((uint8_t*)bp->_at)[0] = bp->_instr_byte;
                    // back up so that we'll execute the full original instruction next
                    --stack->rip;
                }

                if ( !bp || bp->_active ) {
                    //_JOS_KTRACE_CHANNEL(kDebuggerChannel, "breakpoint hit at 0x%016llx", bp->_at);
                    
                    debugger_packet_bp_t bp_info;
                    _fill_in_debugger_packet(&bp_info, stack);
                    
                    // unwind the call stack
                    // here we just look up stack entries to see if they point to executable code.
                    // the debugger will check these for actual call sites
                    task_context_t* this_task = tasks_this_task();
                    uint64_t* rsp = (uint64_t*)stack->rsp;
                    const uint64_t* stack_end = (const uint64_t*)this_task->_stack_top;
                    
                    static vector_t callstack;
                    static bool callstack_initialised = false;
                    if (callstack_initialised) {
                        // re-use
                        vector_reset(&callstack);
                    } else {
                        vector_create(&callstack, 16, sizeof(uint64_t), _allocator);
                        callstack_initialised = true;
                    }
                    while (rsp < stack_end) {
                        if (peutil_phys_is_executable(_pe_ctx, *rsp, 0)) {
                            vector_push_back(&callstack, (void*)rsp);
                        }
                        ++rsp;
                    }
                    
                    bp_info._call_stack_size = (uint16_t)vector_size(&callstack);
                    debugger_send_packet(kDebuggerPacket_Breakpoint, &bp_info, sizeof(bp_info));
                    
                    if (bp_info._call_stack_size) {
                        debugger_send_packet(kDebuggerPacket_BreakpointCallstack, vector_data(&callstack), bp_info._call_stack_size * sizeof(uint64_t));
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
                debugger_packet_bp_t bp_info;
                _fill_in_debugger_packet(&bp_info, stack);
                debugger_send_packet(kDebuggerPacket_PF, &bp_info, sizeof(bp_info));
            }
            break;
            default:;
        }
        
        // enter loop waiting for further instructions from the debugger, exit when we get kDebuggerPacket_Continue 
        _debugger_loop(stack);
        
        if ( bp && bp->_active && !bp->_transient ) {
            // if we are coming out of a runtime breakpoint that's still active we need to re-set it
            _last_rt_bp._active = true;
            _last_rt_bp._at = stack->rip;
            _last_rt_bp._instr_byte = bp->_instr_byte;
            // make sure we trap immediately after this instruction again so that we can restore it
            _SET_TF(stack);
        }
        else {
            if (bp->_transient) {
                // the bp was transient; remove it completely
                _remove_breakpoint(bp);
            }
            _last_rt_bp._active = false;
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

_JOS_API_FUNC void debugger_disasm(void* at, size_t bytes, wchar_t* output_buffer, size_t output_buffer_length) {
    
    (void)bytes;
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
        if( output_buffer_length >= (size_t)(w) ) {\
            output_buffer_length -= (size_t)(w);\
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

_JOS_API_FUNC void debugger_initialise(generic_allocator_t* allocator) {

    // we're good with a 2MB heap for now
#define _DEBUGGER_HEAP_SIZE 2*1024*1024
    _allocator = (generic_allocator_t*)arena_allocator_create(allocator->alloc(allocator, _DEBUGGER_HEAP_SIZE), _DEBUGGER_HEAP_SIZE);

    _last_rt_bp._active = false;
    // we "enter" the debugger with int 3 so we need to register this from the start
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=0x3, ._handler=_debugger_isr_handler });
    
    ZydisDecoderInit(&_zydis_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

    vector_create(&_breakpoints, 16, sizeof(debugger_breakpoint_t), _allocator);

    output_console_output_string_w(L"debug handler initialised\n");    
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

    processor_information_t* this_cpu_info = per_cpu_this_cpu_info();

    json_write_object_start(&ctx);
        json_write_key(&ctx, "version");
            json_write_object_start(&ctx);
                json_write_key(&ctx, "major");
                json_write_number(&ctx, 0);
                json_write_key(&ctx, "minor");
                json_write_number(&ctx, 1);
                json_write_key(&ctx, "patch");
                json_write_number(&ctx, 0);
                //TODO: json_write_key(&ctx, "git");
                //TODO: json_write_number(&ctx, GIT_VERSION);
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
                json_write_key(&ctx, "vendor");
                json_write_string(&ctx, this_cpu_info->_vendor_string);
                if (this_cpu_info->_has_hypervisor) {
                    json_write_key(&ctx, "hypervisor");
                        json_write_object_start(&ctx);
                        json_write_key(&ctx, "id");
                        json_write_string(&ctx, this_cpu_info->_hypervisor_id);                        
                        json_write_object_end(&ctx);
                }
                if (this_cpu_info->_xsave) {
                    json_write_key(&ctx, "xsave");
                        json_write_object_start(&ctx);
                            json_write_key(&ctx, "size");
                            json_write_number(&ctx, this_cpu_info->_xsave_info._xsave_area_size);
                            json_write_key(&ctx, "bitmap");
                            json_write_number(&ctx, this_cpu_info->_xsave_info._xsave_bitmap);
                        json_write_object_end(&ctx);
                }
            json_write_object_end(&ctx);
    json_write_object_end(&ctx);

    uint32_t json_size = (uint32_t)ftell(&stream);
    debugger_send_packet(kDebuggerPacket_KernelConnectionInfo, json_buffer, json_size);

    
    _JOS_KTRACE_CHANNEL(kDebuggerChannel, "connected");
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
