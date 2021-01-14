#ifndef _JOS_KERNEL_DEBUGGER_H
#define _JOS_KERNEL_DEBUGGER_H

#include <jos.h>

// to be called after early initialisation of interrupts
void debugger_initialise(void);

static _JOS_ALWAYS_INLINE void debug_break(void) {
    asm volatile("int $0x3");
}

#endif // _JOS_KERNEL_DEBUGGER_H