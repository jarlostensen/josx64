#ifndef _JOS_KERNEL_X86_64_H_
#define _JOS_KERNEL_X86_64_H_

#include <jos.h>

// read MSR
static _JOS_ALWAYS_INLINE void x86_64_rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi)
{
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}
// write MSR
static _JOS_ALWAYS_INLINE void x86_64_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
   asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

static _JOS_ALWAYS_INLINE void x86_64_load_idt(const void* dt) 
{
    asm volatile("lidt %0"::"m" (*dt));
}

static _JOS_ALWAYS_INLINE void x86_64_outb(unsigned short port, uint8_t byte) {
    asm volatile("outb %1, %0" : :  "dN" (port), "a" (byte));
}

static _JOS_ALWAYS_INLINE  uint8_t x86_64_inb(unsigned short port) {
    uint8_t val;
    asm volatile ("inb %1, %0" : "=a" (val) : "dN" (port));
    return val;
}

//TODO: this is the "hard" way, we need a softer way to do it as well (as what Linux does)
static _JOS_ALWAYS_INLINE void x86_64_cli(void) {
    asm volatile("cli" ::: "memory");
}

static _JOS_ALWAYS_INLINE void x86_64_sti(void) {
    asm volatile("sti" ::: "memory");
}

static _JOS_ALWAYS_INLINE void x86_64_pause_cpu(void) {
    asm volatile ("pause");
}

// (safe) dummy write to POST port, this usually provides a ~usecond delay
#define x86_64_io_wait() x86_64_outb(0x80, 0)

extern uint16_t x86_64_get_cs(void);
extern uint16_t x86_64_get_ss(void);
extern uint64_t x86_64_get_rflags(void);

#endif // _JOS_KERNEL_X86_64_H_