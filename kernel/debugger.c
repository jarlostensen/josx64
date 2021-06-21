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
#include <memory.h>

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

typedef struct _debugger_packet_bp {
    interrupt_stack_t   _stack;
    // ..+ control registers etc.

} _JOS_PACKED debugger_packet_bp_t;


static void _debugger_loop(void) {
    if ( !debugger_is_connected() )
        return;

    bool continue_run = false;
    while(!continue_run) {
        debugger_serial_packet_t packet;
        debugger_read_packet_header(&packet);
        switch(packet._id) {
            case kDebuggerPacket_ReadTargetMemory:
            {                
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
            case kDebuggerPacket_Continue:
            {
                _JOS_KTRACE_CHANNEL("debugger", "continuing execution");
                continue_run = true;
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
static void _debugger_isr_handler(const interrupt_stack_t * context) {

    _JOS_KTRACE_CHANNEL("debugger", "breakpoint hit at 0x%016llx", context->rip);

    if ( debugger_is_connected() ) {
        // -------------------------------------- running in debugger

        debugger_packet_bp_t bp_info;
        memcpy(&bp_info._stack, context, sizeof(interrupt_stack_t));

        switch ( context->handler_id ) {
            case 3:
                debugger_send_packet(kDebuggerPacket_Int3, &bp_info, sizeof(bp_info));
                break;
            case 0xd:
                debugger_send_packet(kDebuggerPacket_GPF, &bp_info, sizeof(bp_info));
                break;
            default:;
        }

        // enter loop waiting for further instructions
        _debugger_loop();
    }
    else {
        // -------------------------------------- running without a debugger
        _JOS_KTRACE_CHANNEL("debugger", "breakpoint hit at 0x%016llx\n", context->rip);

        //ZZZ: ============================================================
        wchar_t buf[1024];
        const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

        swprintf(buf,bufcount,L"\nDBGBREAK\n\tat 0x%016llx\n", context->rip);
        output_console_output_string(buf);

        // dump registers
        swprintf(buf, bufcount,
            L"\trax 0x%016llx\trbx 0x%016llx\n\trcx 0x%016llx\trdx 0x%016llx\n"
            L"\trsi 0x%016llx\trdi 0x%016llx\n\trbp 0x%016llx\trsp 0x%016llx\n"
            L"\tr8  0x%016llx\tr9  0x%016llx\n\tr10 0x%016llx\tr11 0x%016llx\n"
            L"\tr12 0x%016llx\tr13 0x%016llx\n\tr14 0x%016llx\tr15 0x%016llx\n"
            L"\tcs 0x%04llx\tss 0x%04llx\n\n",
            context->rax, context->rbx, context->rcx, context->rdx,
            context->rsi, context->rdi, context->rbp, context->rsp,
            context->r8, context->r9, context->r10, context->r11,
            context->r12, context->r13, context->r14, context->r15,
            context->cs, context->ss
        );
        output_console_line_break();
        output_console_output_string(buf);
        
        // stack dump
        const uint64_t* stack = (const uint64_t*)context->rsp;
        swprintf(buf, bufcount, 
            L"stack @ 0x%016llx:\n\t"
            L"0x%016llx\n\t0x%016llx\n\t0x%016llx\n\t0x%016llx\n\t"
            L"0x%016llx\n\t0x%016llx\n\t0x%016llx\n\t0x%016llx\n\n",
            stack,
            stack[0], stack[1], stack[2], stack[3],
            stack[4], stack[5], stack[6], stack[7]
        );    
        output_console_output_string(buf);

        wchar_t output_buffer[512];
        debugger_disasm((void*)context->rip, 50, output_buffer, 512);
        output_console_output_string(output_buffer);
        output_console_line_break();
    }
}

_JOS_API_FUNC void debugger_initialise(void) {
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=0x3, ._handler=_debugger_isr_handler });
    output_console_output_string(L"debug handler initialised\n");
}

_JOS_API_FUNC void debugger_wait_for_connection(peutil_pe_context_t* pe_ctx, uint64_t image_base) {
    
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
    // from this point on we want to catch general protection faults
    interrupts_set_isr_handler(&(isr_handler_def_t){ ._isr_number=0xd, ._handler=_debugger_isr_handler });

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