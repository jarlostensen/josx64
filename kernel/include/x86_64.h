#ifndef _JOS_KERNEL_X86_64_H_
#define _JOS_KERNEL_X86_64_H_

#include <jos.h>


// read MSR
inline void x86_64_rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi)
{
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}
// write MSR
inline void x86_64_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
   asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

#endif // _JOS_KERNEL_X86_64_H_