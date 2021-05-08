#ifndef _JOS_KERNEL_KERNEL_H
#define _JOS_KERNEL_KERNEL_H

#include <atomic.h>
#include <x86_64.h>

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


typedef struct _kernel_uefi_init_args {

    size_t      application_memory_size_required;
    void**      application_allocated_memory;

} kernel_uefi_init_args_t;

jo_status_t kernel_uefi_init(kernel_uefi_init_args_t* args);
jo_status_t kernel_runtime_init(void);
_JOS_NORETURN void  kernel_runtime_start(void);

#endif // _JOS_KERNEL_KERNEL_H
