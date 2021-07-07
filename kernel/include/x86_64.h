#ifndef _JOS_KERNEL_X86_64_H_
#define _JOS_KERNEL_X86_64_H_

#include <jos.h>

// read MSR
_JOS_INLINE_FUNC void x86_64_rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi)
{
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}
// write MSR
_JOS_INLINE_FUNC void x86_64_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
   __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

_JOS_INLINE_FUNC void x86_64_load_idt(const void* dt) 
{
    __asm__ volatile("lidt %0"::"m" (*dt));
}

_JOS_INLINE_FUNC void x86_64_outb(unsigned short port, uint8_t byte) {
    __asm__ volatile("outb %1, %0" : :  "dN" (port), "a" (byte));
}

_JOS_INLINE_FUNC uint8_t x86_64_inb(unsigned short port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a" (val) : "dN" (port));
    return val;
}

_JOS_INLINE_FUNC void x86_64_read_gs(size_t offset, uint64_t * val) {
    __asm__ volatile("movq %%gs:(%1), %0" : "=r" (*val) : "r" (offset));
}

_JOS_INLINE_FUNC void x86_64_write_gs(size_t offset, uint64_t * val) {
    __asm__ volatile("movq %0, %%gs:(%1)" : : "r" (*val), "r" (offset));
}

//TODO: this is the "hard" way, we need a softer way to do it as well (as what Linux does)
_JOS_INLINE_FUNC void x86_64_cli(void) {
    __asm__ volatile("cli" ::: "memory");
}

_JOS_INLINE_FUNC void x86_64_sti(void) {
    __asm__ volatile("sti" ::: "memory");
}

_JOS_INLINE_FUNC void x86_64_pause_cpu(void) {
    __asm__ volatile ("pause");
}

_JOS_INLINE_FUNC uint64_t x86_64_read_cr0(void)
{
    uint64_t val;
    __asm__ volatile ( "mov %%cr0, %0" : "=r"(val) );
    return val;
}

_JOS_INLINE_FUNC uint64_t x86_64_read_cr2(void)
{
    uint64_t val;
    __asm__ volatile ( "mov %%cr2, %0" : "=r"(val) );
    return val;
}

_JOS_INLINE_FUNC uint64_t x86_64_read_cr3(void)
{
    uint64_t val;
    __asm__ volatile ( "mov %%cr3, %0" : "=r"(val) );
    return val;
}

_JOS_INLINE_FUNC uint64_t x86_64_read_cr4(void)
{
    uint64_t val;
    __asm__ volatile ( "mov %%cr4, %0" : "=r"(val) );
    return val;
}

_JOS_INLINE_FUNC void x86_64_write_cr4(uint64_t val)
{
    __asm__ volatile ( "mov %0, %%cr4" : : "r" (val) );
}

// (safe) dummy write to POST port, this usually provides a ~usecond delay
#define x86_64_io_wait() x86_64_outb(0x80, 0)
#define x86_64_debugbreak() __asm__ volatile( "int $03" )

extern uint16_t x86_64_get_cs(void);
extern uint16_t x86_64_get_ss(void);
extern uint64_t x86_64_get_rsp(void);
extern uint64_t x86_64_get_rflags(void);
extern uint64_t x86_64_get_pml4(void);

#endif // _JOS_KERNEL_X86_64_H_
