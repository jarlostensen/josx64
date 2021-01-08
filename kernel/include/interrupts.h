#ifndef _JOS_KERNEL_INTERRUPTS_H_
#define _JOS_KERNEL_INTERRUPTS_H_

#include "jos.h"

#define _JOS_KERNEL_NUM_EXCEPTIONS      32

void interrupts_initialise_early();

typedef void (*isr_handler_func_t)(uint32_t error_code, uint16_t caller_cs, uint32_t caller_eip);
// register a handler for the given interrupt.
// this  can be done at any time after initialisation and the handler will be effective from the 
// next interrupt.
void interrupts_set_isr_handler(int i, isr_handler_func_t handler);

typedef void (*irq_handler_func_t)(int irq_num);
// register a handler for the given IRQ.
// this  can be done at any time after initialisation and the handler will be effective from the next IRQ.
// NOTE: the irq number must be in the range 0..31, i.e. irrespective of PIC remapping
// this does NOT enable the corresponding IRQ, use k_enable_irq for that
void interrupts_set_irq_handler(int irq, irq_handler_func_t handler);

// enable the given irq
void interrupts_PIC_enable_irq(int irq);
// disable the given IRQ
void interrupts_PIC_disable_irq(int irq);

#endif // _JOS_KERNEL_INTERRUPTS_H_