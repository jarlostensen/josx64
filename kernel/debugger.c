#include <wchar.h>

#include <jos.h>
#include <interrupts.h>
#include <hex_dump.h>
#include <debugger.h>

#include <stdio.h>
#include <output_console.h>

static void _int_3_handler(const isr_context_t * context) {

    //ZZZ: ============================================================
    wchar_t buf[128];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

    swprintf(buf,bufcount,L"DBGBREAK\n");
    output_console_output_string(buf);
    hex_dump_mem((void*)context->rip, 32, k8bitInt);
}

void debugger_initialise(void) {
    interrupts_set_isr_handler(3, _int_3_handler);
}
