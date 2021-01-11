#include <wchar.h>

#include <jos.h>
#include <interrupts.h>
#include <hex_dump.h>
#include <debugger.h>

#include <stdio.h>
#include <output_console.h>

static void _int_3_handler(const isr_context_t * context) {

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
    hex_dump_mem((void*)context->rip, 32, k8bitInt);
    output_console_line_break();
}

void debugger_initialise(void) {
    interrupts_set_isr_handler(3, _int_3_handler);
}
