#ifndef _JOS_KERNEL_KERNEL_H
#define _JOS_KERNEL_KERNEL_H

#include <stdatomic.h>
#include <x86_64.h>

__attribute__((__noreturn__)) void halt_cpu();

typedef struct _lock {
    atomic_int  _val;
} lock_t;

static void lock_initialise(lock_t* lock) {
    lock->_val = 0;
}

static void lock_spinlock(lock_t* lock) {
    static int kZero = 0;
    // it can be weak, we expect to have to spin a few times
    while(!atomic_compare_exchange_weak(&lock->_val, &kZero, 1)) {
        x86_64_pause_cpu();
    }
}

static void lock_unlock(lock_t* lock) {
    atomic_store(&lock->_val, 0);
}

#endif // _JOS_KERNEL_KERNEL_H