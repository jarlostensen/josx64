#ifndef _JOS_KERNEL_DEBUGGER_H
#define _JOS_KERNEL_DEBUGGER_H

#include <jos.h>

// to be called after early initialisation of interrupts
void debugger_initialise(void);
// outputs disassembly to the console 
void debugger_disasm(void* at, size_t bytes, wchar_t* output_buffer, size_t output_buffer_length);

#endif // _JOS_KERNEL_DEBUGGER_H