#ifndef _JOS_KERNEL_DETAIL_H
#define _JOS_KERNEL_DETAIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/kernel.h>


#ifdef __GNUC__
#define _JOS_INLINE_FUNC __attribute__((unused)) static
#else
#define _JOS_INLINE_FUNC static
#endif


int k_is_protected_mode();
uint32_t k_eflags();

void _k_enable_interrupts();
void _k_disable_interrupts();
void _k_halt_cpu();
uint64_t _k_clock_est_cpu_freq();
// wait by doing a nop-write to port 0x80 (POST)
void k_io_wait(void);

#define _JOS_KERNEL_CS_SELECTOR 0x08
#define _JOS_KERNEL_DS_SELECTOR 0x10

typedef enum alignment_enum
{
    kNone = 0,
    kAlign16 = 2,
    kAlign32 = 4,
    kAlign64 = 8,
    kAlign128 = 16,
    kAlign256 = 32,
    kAlign512 = 64,
    kAlign4k = 0x1000,
} alignment_t;
void* k_alloc(size_t bytes, alignment_t alignment);
void k_alloc_init();

//TODO: read current NMI status + CMOS management 
// non-maskable int enable
// inline void _k_enable_nmi() {
//     k_outb(0x70, k_inb(0x70) & 0x7F);
//  }
 
//  // non-maskable int disable
//  inline void _k_disable_nmi() {
//     k_outb(0x70, k_inb(0x70) | 0x80);
//  }

#endif // _JOS_KERNEL_DETAIL_H