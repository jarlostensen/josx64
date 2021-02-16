#pragma once
#include <jos.h>

// i8259a driver

#define _JOS_i8259a_IRQ_BASE_OFFSET 0x20    // the offset we apply to the IRQs to map them outside of the lower reserved ISR range
extern unsigned int _i8259a_irq_mask;

void i8259a_initialise(void);
void i8259a_enable_irq(int i);
void i8259a_disable_irq(int i);

static _JOS_ALWAYS_INLINE bool i8259a_irq_enabled(int i) {
       return (_i8259a_irq_mask & (1<<i))!=0;
}
static _JOS_ALWAYS_INLINE bool i8259a_irqs_muted(void) {
    return _i8259a_irq_mask==0;
}

