#ifndef _JOS_KERNEL_IO_H_
#define _JOS_KERNEL_IO_H_

#include <stdint.h>
#include <jos.h>

_JOS_INLINE_FUNC void io_outb(unsigned short port, uint8_t byte) {
    asm volatile("outb %1, %0" : :  "dN" (port), "a" (byte));
}

_JOS_INLINE_FUNC uint8_t io_inb(unsigned short port) {
    uint8_t val;
    asm volatile ("inb %1, %0" : "=a" (val) : "dN" (port));
    return val;
}

#endif // _JOS_KERNEL_IO_H_