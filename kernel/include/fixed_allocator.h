#ifndef _JOS_FIXED_ALLOCATOR_H
#define _JOS_FIXED_ALLOCATOR_H


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <jos.h>

// ====================================================================================
// fixed size pool allocator

typedef struct
{
    uint8_t     _size_p2;   // power of two unit allocation size
    size_t      _count;
    uint32_t    _free;      // index of first free
	uintptr_t	_end;		// upper memory bound for pool
} vmem_fixed_t;

vmem_fixed_t*     vmem_fixed_create(void* mem, size_t size, size_t allocUnitPow2)
{
    if(allocUnitPow2 < 3)
        return 0;
    vmem_fixed_t* pool = (vmem_fixed_t*)mem;
    pool->_size_p2 = (uint8_t)allocUnitPow2;
    size -= sizeof(vmem_fixed_t);
    pool->_count = size / (1<<allocUnitPow2);
    pool->_free = 0;
	pool->_end = (uintptr_t)((uintptr_t)(pool+1) + pool->_count*(1<<pool->_size_p2));
    uint32_t* block = (uint32_t*)((uint8_t*)(pool+1));
    const uint32_t unit_size = 1<<pool->_size_p2;
    for(size_t n = 1; n < pool->_count; ++n)
    {
        *block = (uint32_t)n;
        block += (unit_size >> 2);
    }
    *block = ~0;
    return pool;
}

void* vmem_fixed_alloc(vmem_fixed_t* pool, size_t size)
{
    if((size_t)(1<<pool->_size_p2) < size || pool->_free == (uint32_t)~0)
    {
        //printf("failed test: %d < %d || 0x%x == 0xffffffff\n", 1<<pool->_size_p2, size, pool->_free);
        return 0;
    }
    const uint32_t unit_size = 1<<pool->_size_p2;
    uint32_t* block = (uint32_t*)((uint8_t*)(pool+1) + pool->_free*unit_size);
    pool->_free = *block; // whatever next it points to
    return block;
}

void vmem_fixed_free(vmem_fixed_t* pool, void* block)
{
    if(!pool || !block)
        return;	
    const uint32_t unit_size = 1<<pool->_size_p2;    
    uint32_t* fblock = (uint32_t*)block;
    *fblock = pool->_free;
    uint32_t* free = (uint32_t*)((uint8_t*)(pool+1) + pool->_free*unit_size);
    *free = (uint32_t)((uintptr_t)fblock - (uintptr_t)(pool+1))/unit_size;
}

bool vmem_fixed_in_pool(vmem_fixed_t* pool, void* ptr)
{
	const uintptr_t begin = (uintptr_t)(pool+1);
	return (uintptr_t)ptr >= begin && (uintptr_t)ptr < pool->_end;
}

_JOS_INLINE_FUNC void vmem_fixed_clear(vmem_fixed_t* pool)
{
    pool->_free = 0;
    uint32_t* block = (uint32_t*)((uint8_t*)(pool+1));
    const uint32_t unit_size = 1<<pool->_size_p2;
    for(size_t n = 1; n < pool->_count; ++n)
    {
        *block = (uint32_t)n;
        block += (unit_size >> 2);
    }
    *block = ~0;
}


#endif // _JOS_FIXED_ALLOCATOR_H