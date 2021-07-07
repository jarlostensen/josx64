#pragma once

#include <jos.h>

typedef struct _linear_allocator {

	//NOTE: this must be the first entry in this struct as it is used as a super class
	jos_allocator_t _super;

    void*       _begin;
    char*       _ptr;
    void*       _end;

} linear_allocator_t;

_JOS_API_FUNC linear_allocator_t* linear_allocator_create(void* memory, size_t size_adjusted);
_JOS_API_FUNC void* linear_allocator_alloc(linear_allocator_t* linalloc, size_t size);
_JOS_INLINE_FUNC void linear_allocator_clear(linear_allocator_t* linalloc) {
    linalloc->_ptr = (char*)((char*)linalloc->_begin + sizeof(linear_allocator_t));
}

_JOS_INLINE_FUNC size_t linear_allocator_available(linear_allocator_t* linalloc) {
    return (size_t)linalloc->_end - (size_t)linalloc->_ptr;
}

#if defined(_JOS_IMPLEMENT_ALLOCATORS) && !defined(_JOS_LINEAR_ALLOCATOR_IMPLEMENTED)
#define _JOS_LINEAR_ALLOCATOR_IMPLEMENTED

_JOS_API_FUNC linear_allocator_t* linear_allocator_create(void* memory, size_t size_adjusted) {

    _JOS_ASSERT(size_adjusted>sizeof(linear_allocator_t));
    linear_allocator_t* linalloc = (linear_allocator_t*)memory;
    linalloc->_begin = memory;
    linalloc->_end = (void*)((uintptr_t)memory + size_adjusted);
    linalloc->_ptr = (char*)((char*)memory + sizeof(linear_allocator_t));

    linalloc->_super.alloc = (jos_allocator_alloc_func_t)linear_allocator_alloc;
    linalloc->_super.free = 0;
    linalloc->_super.realloc = 0;

    return linalloc;
}

_JOS_API_FUNC void* linear_allocator_alloc(linear_allocator_t* linalloc, size_t size) {
    size_t capacity = (size_t)linalloc->_end - (size_t)linalloc->_ptr;
    if ( capacity < size ) {
        return NULL;
    }

    char* ptr = linalloc->_ptr;
    linalloc->_ptr += size;
    return ptr;
}
#endif
