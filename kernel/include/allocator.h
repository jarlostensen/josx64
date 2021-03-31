#pragma once

#include <jos.h>

// a general allocator/deallocator
typedef struct _allocator {

    // allocate a block of size
    // NOTE: the underlying implementation may not support variable size allocations
    void*   (*alloc)(size_t);
    // release an allocated block
    // NOTE: the underlying implementation may not support this
    void    (*free)(void*);
    // available memory at this point
    size_t  (*available)(void);

} allocator_t;

// an allocator for memory that is never freed
typedef struct _linear_allocator {

    void*   (*alloc)(size_t);
    // available memory at this point
    size_t  (*available)(void);
    // create sub allocator from this allocator's memory range
    struct _linear_allocator* (*factory)(size_t);
    
} linear_allocator_t;

