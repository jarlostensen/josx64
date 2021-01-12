
#include <jos.h>
#include <kernel.h>
#include <serial.h>

__attribute__((__noreturn__)) void halt_cpu() {
    serial_write_str(kCom1, "\n\nkernel halting\n");
    while(1)
    {
        __asm volatile ("pause");
    } 
    __builtin_unreachable();
}
