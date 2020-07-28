#ifndef _JOS_INTERRUPTS_H
#define _JOS_INTERRUPTS_H

#include <stdbool.h>

// ia-32 dev manual 6-12
typedef struct isr_stack_struct
{    
    // pushed by isr handler stub
    uintptr_t    ds;
    uintptr_t    edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uintptr_t    handler_id;
    
    // pushed by CPU 
    uintptr_t    error_code; //< can also be pushed by stub
    uintptr_t    eip;    
    uintptr_t    cs;
    uintptr_t    eflags;
    // iff privilege level switch
    //TODO:uintptr_t    _esp;
    //TODO: uintptr_t    _ss;
} isr_stack_t;

// initialise the interrupt module
void _k_init_isrs();
// effectively load IDT
void _k_load_isrs();

typedef void (*isr_handler_func_t)(uint32_t error_code, uint16_t caller_cs, uint32_t caller_eip);
// register a handler for the given interrupt.
// this  can be done at any time after initialisation and the handler will be effective from the 
// next interrupt.
isr_handler_func_t k_set_isr_handler(int i, isr_handler_func_t handler);

typedef void (*irq_handler_func_t)(int irq_num);
// register a handler for the given IRQ.
// this  can be done at any time after initialisation and the handler will be effective from the next IRQ.
// NOTE: the irq number must be in the range 0..31, i.e. irrespective of PIC remapping
// this does NOT enable the corresponding IRQ, use k_enable_irq for that
void k_set_irq_handler(int irq, irq_handler_func_t handler);

// enable the given irq
void k_enable_irq(int irq);
// disable the given IRQ
void k_disable_ieq(int irq);
// returns true if the given IRQ is enabled
bool k_irq_enabled(int irq);

#endif