#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <jos.h>

// machine sized atomics, these are guaranteed to be readable and writeable atomically even without locks
// but won't automagically provide cache coherence in the SMP case 

typedef struct _atomic_int {
    _JOS_ALIGN(int, value, sizeof(int));
} atomic_int_t;

typedef struct _atomic_long_long {
    _JOS_ALIGN(long long, value, sizeof(long long));
} atomic_long_long_t;


//NOTE: not a standard compliant signature and completely unchecked
#define atomic_store(a_object, new_value)\
    (a_object)->value = new_value


// CAS with full bus lock
_JOS_INLINE_FUNC int atomic_compare_exchange_strong(volatile int *object, int expected, int desired) {
    // if expected == *object
    //    *object = desired
    // else
    //     expected = *object
    //
	__asm__ __volatile__ (
		"lock ; cmpxchg %3, %1"
		: "=a"(expected), "=m"(*object) : "a"(expected), "r"(desired) : "memory" );
	return expected;
}

// weakly check if *object == expected before attempting a bus lock
_JOS_INLINE_FUNC int atomic_compare_exchange_weak(volatile int* object, int expected, int desired) {
    // weak check; we may early out here but that's the point
    if ( *object != expected ) {
        expected = *object;
        return expected;
    }
    // only if we think we may actually have a chance will we issue the full bus lock
    return atomic_compare_exchange_strong(object, expected, desired);
}

