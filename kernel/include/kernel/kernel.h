
#ifndef _JOS_KERNEL_H
#define _JOS_KERNEL_H

#include <stdint.h>
#include "../jos.h"

#ifdef _JOS_KERNEL_BUILD

// ====================================================================================
// tracing 
void _k_trace(const char* channel, const char* msg,...);
#define _JOS_KTRACE_CHANNEL(channel, msg,...) _k_trace(channel, msg,##__VA_ARGS__)
#define _JOS_KTRACE(msg,...)  _k_trace(0, msg,##__VA_ARGS__)

void _k_trace_buf(const char* channel, const void* data, size_t length);
#define _JOS_KTRACE_CHANNEL_BUF(channel, data,length) _k_trace_buf(channel, data, length)
#define _JOS_KTRACE_BUF(data,length) _k_trace_buf(0, data, length)

// ====================================================================================
// memory

// return physical address or 0 (which is of course valid if virt is 0...)
uintptr_t k_mem_virt_to_phys(uintptr_t virt);

// return the page table entry for the frame containing virt
uintptr_t _k_mem_virt_to_pt_entry(uintptr_t virt);

enum k_mem_valloc_flags_enum
{
    kMemValloc_Reserve = 1,
    kMemValloc_Commit,
};
// allocate size bytes of virtual memory, or 0 if not enough available
void* k_mem_valloc(size_t size, int flags);

void* k_mem_alloc(size_t size);
void k_mem_free(void* ptr);

// ====================================================================================
// I/O

void k_outb(uint16_t port, uint8_t value);
void k_outw(uint16_t port, uint16_t value);
uint8_t k_inb(uint16_t port);
uint16_t k_inw(uint16_t port);

// ====================================================================================
// misc control

__attribute__((__noreturn__)) void k_panic();
// pause the current core (for spin waits)
#define k_pause(void)\
asm volatile ("pause")

// ====================================================================================
// architecture

// read TSC
inline uint64_t __rdtsc()
{
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}

#else // _JOS_KERNEL_BUILD
// ====================================================================================
// Lab build (only defined in the lab visualstudio project)

#define _JOS_KTRACE(...)

#endif

#endif // _JOS_KERNEL_H