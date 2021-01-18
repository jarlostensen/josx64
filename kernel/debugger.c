#include <wchar.h>

#include <jos.h>
#include <interrupts.h>
#include <hex_dump.h>
#include <debugger.h>

#include <Zydis/Zydis.h>

#include <stdio.h>
#include <output_console.h>

static void _int_3_handler(const isr_context_t * context) {

    _JOS_KTRACE_CHANNEL("debugger", "breakpoint hit at 0x%016llx\n", context->rip);

    //ZZZ: ============================================================
    wchar_t buf[1024];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

    swprintf(buf,bufcount,L"\nDBGBREAK\n\tat 0x%016llx\n", context->rip);
    output_console_output_string(buf);

    // dump registers
    swprintf(buf, bufcount,
        L"\trax 0x%016llx\trbx 0x%016llx\n\trcx 0x%016llx\trdx 0x%016llx\n"
        L"\trsi 0x%016llx\trdi 0x%016llx\n\trbp 0x%016llx\n\trflags 0x%016llx\n"
        L"\tr8  0x%016llx\tr9  0x%016llx\n\tr10 0x%016llx\tr11 0x%016llx\n"
        L"\tr12 0x%016llx\tr13 0x%016llx\n\tr14 0x%016llx\tr15 0x%016llx\n",
        context->rax, context->rbx, context->rcx, context->rdx,
        context->rsi, context->rdi, context->rbp, context->rflags,
        context->r8, context->r9, context->r10, context->r11,
        context->r12, context->r13, context->r14, context->r15
    );

    output_console_line_break();
    output_console_output_string(buf);

    output_console_line_break();

    // Initialize decoder context
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

    // Initialize formatter. Only required when you actually plan to do instruction
    // formatting ("disassembling"), like we do here
    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // Loop over the instructions in our buffer.
    ZyanU64 runtime_address = context->rip;
    ZyanUSize offset = 0;
    const ZyanUSize length = 50;
    ZydisDecodedInstruction instruction;
    while (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&decoder, (const void*)(context->rip + offset), length - offset,
        &instruction)))
    {
        // Format & print the binary instruction structure to human readable format
        char buffer[256];
        ZydisFormatterFormatInstruction(&formatter, &instruction, buffer, sizeof(buffer),
            runtime_address);
        
        swprintf(buf,bufcount, L"%016llx  %s\n", runtime_address, buffer);
        output_console_output_string(buf);

        offset += instruction.length;
        runtime_address += instruction.length;
    }

    output_console_line_break();
}

void debugger_initialise(void) {
    interrupts_set_isr_handler(3, _int_3_handler);
}
