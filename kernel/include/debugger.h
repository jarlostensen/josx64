#ifndef _JOS_KERNEL_DEBUGGER_H
#define _JOS_KERNEL_DEBUGGER_H

#include <jos.h>

// to be called after early initialisation of interrupts
void debugger_initialise(void);
// outputs disassembly to the console 
void debugger_disasm(void* at, size_t bytes, wchar_t* output_buffer, size_t output_buffer_length);

// waits for an establishes a connection with the remote debugger
// subsequently the "debugger_is_connected" function will return true
void debugger_wait_for_connection(void);

// true if we're connected to a debugger...obviously
bool debugger_is_connected(void);

#endif // _JOS_KERNEL_DEBUGGER_H
