#ifndef _JOS_KERNEL_INTERRUPTS_H_
#define _JOS_KERNEL_INTERRUPTS_H_

#include <stdint.h>

// IA-32 dev manual Volume 2 6-12
typedef struct _isr_stack
{    
    // bottom of stack (rsp)

    uint64_t        rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t        r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t        handler_id;
    
    uint64_t        error_code; //< will be pushed as 0 by our stub if not done by the CPU
    uint64_t        rip;    
    uint64_t        cs;
    uint64_t        rflags;
    uint64_t        rsp;
    uint64_t        ss;     // <- top of stack (rsp + 184)

} isr_stack_t;


#endif // _JOS_KERNEL_INTERRUPTS_H_