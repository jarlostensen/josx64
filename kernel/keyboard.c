#include "kernel_detail.h"
#include "interrupts.h"
#include <collections.h>
#include <kernel/atomic.h>
#include <stdio.h>

atomic_int_t   _queue_lock;
static queue_t _keys;

static void irq_1_handler(int irq)
{
    (void)irq;
    uint8_t status = k_inb(0x64);
    uint8_t scan = k_inb(0x60);    
    if(status & 1)
    {
        static int kZero = 0;
        if( __atomic_compare_exchange_n(&_queue_lock._val, &kZero, 1, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED) )
        {
            if(!queue_is_full(&_keys))
            {
                queue_push(&_keys, &scan);
            }
            _queue_lock._val = kZero;
        }
        // else we just drop this key...
    }
}

void k_keyboard_init(void)
{
    queue_create(&_keys,64,sizeof(uint8_t));
    k_set_irq_handler(1, irq_1_handler);
    k_enable_irq(1);
}