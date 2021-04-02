#pragma once

#include <jos.h>

typedef struct _linear_allocator {

    void*       _begin;
    char*       _ptr;
    void*       _end;

} linear_allocator_t;

_JOS_INLINE_FUNC linear_allocator_t* linear_allocator_create(void* memory, size_t size_adjusted) {

    _JOS_ASSERT(size_adjusted>sizeof(linear_allocator_t));
    linear_allocator_t* linalloc = (linear_allocator_t*)memory;
    linalloc->_begin = memory;
    linalloc->_end = (void*)((uintptr_t)memory + size_adjusted);
    linalloc->_ptr = (char*)((char*)memory + sizeof(linear_allocator_t));
    return linalloc;
}

_JOS_INLINE_FUNC void* linear_allocator_alloc(linear_allocator_t* linalloc, size_t size) {
    size_t capacity = (size_t)linalloc->_end - (size_t)linalloc->_ptr;
    if ( capacity < size ) {
        return NULL;
    }

    char* ptr = linalloc->_ptr;
    linalloc->_ptr += size;
    return ptr;
}
