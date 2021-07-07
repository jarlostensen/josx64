#ifndef _JOS_KERNEL_KERNEL_H
#define _JOS_KERNEL_KERNEL_H

#include <atomic.h>
#include <x86_64.h>
#include <c-efi.h>

_JOS_NORETURN void halt_cpu();

typedef struct _lock {
    atomic_int_t  atomic_val;
} lock_t;

_JO_INLINE_FUNC void lock_initialise(lock_t* lock) {
    lock->atomic_val.value = 0;
}

// a very basic spinlock implementation
_JO_INLINE_FUNC void lock_spinlock(lock_t* lock) {
    static int kZero = 0;
    // it can be weak, we expect to have to spin a few times
    while(!atomic_compare_exchange_weak(&lock->atomic_val.value, kZero, 1)) {
        x86_64_pause_cpu();
    }
}

_JO_INLINE_FUNC void lock_unlock(lock_t* lock) {
    atomic_store(&lock->atomic_val, 0);
}

_JOS_API_FUNC jo_status_t kernel_uefi_init(CEfiBootServices* boot_services);
_JOS_API_FUNC jo_status_t kernel_runtime_init(CEfiHandle h, CEfiBootServices *boot_services);
_JOS_NORETURN void  kernel_runtime_start(void);

_JOS_API_FUNC size_t kernel_memory_available(void);

#endif // _JOS_KERNEL_KERNEL_H
