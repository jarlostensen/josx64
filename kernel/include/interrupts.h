#ifndef _JOS_KERNEL_INTERRUPTS_H_
#define _JOS_KERNEL_INTERRUPTS_H_

#include "jos.h"

#define _JOS_KERNEL_NUM_EXCEPTIONS      32

// ISR handlers get access to a copy of the interrupt context
typedef struct _isr_context {

    uint64_t        handler_code;

    uint64_t        rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t        r15, r14, r13, r12, r11, r10, r9, r8;

    uint64_t        rip;    
    uint64_t        cs;
    uint64_t        rflags;
    
} isr_context_t;

void interrupts_initialise_early();

typedef enum _interrupt_handler_priority {
    kInterrupt_Critical,
    kInterrupt_NonCritical,
    kInterrupt_NonCritical_Deferrable
} interrupt_handler_priority_t;

typedef void (*isr_handler_func_t)(const isr_context_t * context);
typedef struct _isr_handler_def {

    int                             _isr_number;
    isr_handler_func_t              _handler;
    interrupt_handler_priority_t    _priority;

} isr_handler_def_t;

// register a handler for the given interrupt.
// this  can be done at any time after initialisation and the handler will be effective from the 
// next interrupt.
void interrupts_set_isr_handler(isr_handler_def_t* def);

typedef void (*irq_handler_func_t)(int irq_num);
typedef struct _irq_handler_def {

    int                             _irq_number;
    irq_handler_func_t              _handler;
    interrupt_handler_priority_t    _priority;

} irq_handler_def_t;

// register a handler for the given IRQ.
// this  can be done at any time after initialisation and the handler will be effective from the next IRQ.
// NOTE: the irq number must be in the range 0..31, i.e. irrespective of PIC remapping
// this does NOT enable the corresponding IRQ, use k_enable_irq for that
void interrupts_set_irq_handler(irq_handler_def_t* def);

// enable the given irq
void interrupts_PIC_enable_irq(int irq);
// disable the given IRQ
void interrupts_PIC_disable_irq(int irq);

#endif // _JOS_KERNEL_INTERRUPTS_H_