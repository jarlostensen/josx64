#ifndef _JOS_arena_allocator_ALLOCATOR_H
#define _JOS_arena_allocator_ALLOCATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <jos.h>

//DEBUG:include <stdio.h>


// ===============================================================
//
// Basic variable size block allocator
// not fast, but works. 

/*
	NOTE: size stored in header is the size of the entire block, with header and tail
		  size stored in tail is the size of the actual allocation (without header and tail)

	size = allocation size
	tail_size = allocation size
	header_size = allocation size + sizeof(header) + sizeof(tail)

 allocated block:
    ------------------------------
    | tail_size                  |
    |----------------------------|
    |                            |
    . size bytes                 .
    .                            .
    |----------------------------|
    | header_size                |
    ------------------------------
 
 free block:
    -------------------------------
    | tail_size | kVmemBlockFree  |
    |-----------------------------|
    |                             |
    .   size bytes                .
    .                             .
    |-----------------------------|
    | next free                   |
    | prev free                   |
    | header_size | kVmemBlockFree|
    -------------------------------
*/
#define kVmemBlockFree ~((~(uintptr_t)0)>>1)

typedef struct vmem_block_head_struct
{
    size_t      _size;
    // used if (_size & kVmemBlockFree)==kVmemBlockFree
    struct vmem_block_head_struct*   _links[];
} vmem_block_head_t;

typedef struct vmem_block_tail_struct
{
    //NOTE: block is free if (_size & kVmemBlockFree)==kVmemBlockFree
    size_t      _size;
} vmem_block_tail_t;

typedef struct arena_allocator_struct
{
	//NOTE: this must be the first entry in this struct as it is used as a super class
	jos_allocator_t _super;

    size_t                  _size;
    size_t                  _capacity;
    vmem_block_head_t*     _free_head;
} arena_allocator_t;

_JOS_API_FUNC arena_allocator_t* arena_allocator_create(void* mem, size_t size);
_JOS_API_FUNC void* arena_allocator_alloc(arena_allocator_t* arena, size_t size);
_JOS_API_FUNC void arena_allocator_free(arena_allocator_t* arena, void* block);
_JOS_API_FUNC void* arena_allocator_realloc(arena_allocator_t* arena, void* block, size_t size);

#if defined(_JOS_IMPLEMENT_ALLOCATORS) && !defined(_JOS_arena_allocator_ALLOCATOR_IMPLEMENTED)
#define _JOS_arena_allocator_ALLOCATOR_IMPLEMENTED

//NOTE: +7 because we align all allocations to nearest 8 bytes
#define _JOS_arena_allocator_ALLOC_OVERHEAD (sizeof(vmem_block_head_t) + sizeof(vmem_block_tail_t) + 7)
#define _JOS_VMEM_ABS_BLOCK_SIZE(size) ((size) & ~kVmemBlockFree)
#define _JOS_VMEM_BLOCK_MIN_SIZE _JOS_arena_allocator_ALLOC_OVERHEAD + 16
#define _JOS_arena_allocator_MIN_SIZE (sizeof(arena_allocator_t) + _JOS_arena_allocator_ALLOC_OVERHEAD)

_JOS_INLINE_FUNC size_t arena_allocator_available(arena_allocator_t* arena) {
    return arena->_size - _JOS_arena_allocator_ALLOC_OVERHEAD;
}

_JOS_INLINE_FUNC vmem_block_tail_t* _vmem_tail_from_head(const vmem_block_head_t* head)
{	
	return (vmem_block_tail_t*)((char*)(head+1) + (_JOS_VMEM_ABS_BLOCK_SIZE(head->_size)) - (_JOS_arena_allocator_ALLOC_OVERHEAD));
}

_JOS_INLINE_FUNC vmem_block_head_t* _vmem_head_from_tail(const vmem_block_tail_t* tail)
{	
	return (vmem_block_head_t*)((char*)(tail) - (_JOS_VMEM_ABS_BLOCK_SIZE(tail->_size) + sizeof(vmem_block_head_t)));
}

_JOS_INLINE_FUNC bool _vmem_block_is_free(size_t blockSize)
{
	return ((blockSize & kVmemBlockFree)==kVmemBlockFree);
}

_JOS_INLINE_FUNC size_t _vmem_tail_size(const vmem_block_head_t* head)
{
	return ((head->_size - (_JOS_arena_allocator_ALLOC_OVERHEAD)));
}

_JOS_INLINE_FUNC size_t _vmem_tail_free_size(const vmem_block_head_t* head)
{
	return ((head->_size - (_JOS_arena_allocator_ALLOC_OVERHEAD)) | kVmemBlockFree);
}

_JOS_INLINE_FUNC void _arena_allocator_block_insert_as_free(arena_allocator_t* arena, vmem_block_head_t* new_block)
{
    if(!arena || !new_block || arena->_free_head==new_block)
        return;
	
	// full arena?
    if(!arena->_free_head)
    {
        arena->_free_head = new_block;
		new_block->_links[0] = 0;
		new_block->_links[1] = 0;    
    }
	else
	{
		vmem_block_head_t* free = arena->_free_head;
		// quick check; insert before?
		if(_JOS_VMEM_ABS_BLOCK_SIZE(new_block->_size) <= _JOS_VMEM_ABS_BLOCK_SIZE(free->_size))
		{
			arena->_free_head = new_block;
			new_block->_links[0] = 0;
			new_block->_links[1] = free;
			free->_links[0] = new_block;			
		}
		else
		{		
			// traverse and insert
			vmem_block_head_t* prev = free;
			free = free->_links[1];
			while(free)
			{
				if(_JOS_VMEM_ABS_BLOCK_SIZE(new_block->_size) <= _JOS_VMEM_ABS_BLOCK_SIZE(free->_size))
				{
					new_block->_links[0] = prev;
					new_block->_links[1] = free;
					free->_links[0] = new_block;
					prev->_links[1] = new_block;
					return;
				}
				prev = free;
				free = free->_links[1];
			}
			if(prev)
			{
				// attach to the very end
				new_block->_links[0] = prev;
				new_block->_links[1] = 0;
				prev->_links[1] = new_block;				
			}
		}
	}
}

_JOS_INLINE_FUNC void _arena_allocator_disconnect(arena_allocator_t* arena, vmem_block_head_t* block)
{
	if(!arena || !block || (block->_size & kVmemBlockFree)!=kVmemBlockFree)
		return;

	if(arena->_free_head==block)
	{
		arena->_free_head = block->_links[1];		
	}

	if(block->_links[0])
	{
		block->_links[0]->_links[1] = block->_links[1];
	}

	if(block->_links[1])
		block->_links[1]->_links[0] = block->_links[0];

	block->_links[0] = block->_links[1] = 0;
}

// ============================== public API

_JOS_API_FUNC arena_allocator_t*   arena_allocator_create(void* mem, size_t size)
{
    if(!mem || size <= _JOS_arena_allocator_MIN_SIZE)
    {
        return 0;
    }	

	arena_allocator_t*   arena = (arena_allocator_t*)mem;
	arena->_free_head = (vmem_block_head_t*)(arena+1);
	arena->_size = arena->_capacity = size - sizeof(arena_allocator_t);
    arena->_free_head->_size = arena->_size | kVmemBlockFree;
    arena->_free_head->_links[0] = 0;
    arena->_free_head->_links[1] = 0;
	vmem_block_tail_t* tail = _vmem_tail_from_head(arena->_free_head);
	tail->_size = _vmem_tail_free_size(arena->_free_head);

	arena->_super.alloc = (jos_allocator_alloc_func_t)arena_allocator_alloc;
	arena->_super.free = (jos_allocator_free_func_t)arena_allocator_free;
	arena->_super.realloc = (jos_allocator_realloc_func_t)arena_allocator_realloc;
    arena->_super.available = (jos_allocator_avail_func_t)arena_allocator_available;

    return arena;
}

_JOS_API_FUNC void* arena_allocator_alloc(arena_allocator_t* arena, size_t size)
{
	if(!size)
	{		
		return 0;
	}

	// cap to minimum size or align to 8 bytes
	size = (size < _JOS_VMEM_BLOCK_MIN_SIZE) ? _JOS_VMEM_BLOCK_MIN_SIZE : (size+7)&~7;

	if(!arena || arena->_size<size+_JOS_arena_allocator_ALLOC_OVERHEAD )
	{
        return 0;
	}
    
    vmem_block_head_t* free = arena->_free_head;
    while(free)
    {
        if(_JOS_VMEM_ABS_BLOCK_SIZE(free->_size) >= (size+_JOS_arena_allocator_ALLOC_OVERHEAD) )
            break;
        free = free->_links[1];
    }

    if(free)
    {
		//DEBUG:printf("free was 0x%x; prev = 0x%x, next = 0x%x\n", free, free->_links[0], free->_links[1]);

        // unlink this free block (we'll insert a new one below if we split)		
		_arena_allocator_disconnect(arena, free);
		
        size_t org_size = _JOS_VMEM_ABS_BLOCK_SIZE(free->_size);		
        free->_size = (size+_JOS_arena_allocator_ALLOC_OVERHEAD);
		if(org_size - free->_size < _JOS_VMEM_BLOCK_MIN_SIZE)
		{
			// just absorb the whole free chunk to avoid small fragments
			free->_size = org_size;
		}
        vmem_block_tail_t* new_tail = _vmem_tail_from_head(free);
        new_tail->_size = _vmem_tail_size(free);

        // space for at least one more free block?
		org_size -= free->_size;
        if(org_size >= _JOS_VMEM_BLOCK_MIN_SIZE)
        {
            vmem_block_head_t* new_head = (vmem_block_head_t*)_JOS_ALIGN(new_tail+1, kAllocAlign_8);
			// adjust by bytes required by alignment 
			new_head->_size = org_size - ((size_t)new_head - (size_t)(new_tail + 1));
            new_tail = _vmem_tail_from_head(new_head);
            new_tail->_size = _vmem_tail_free_size(new_head);
			new_head->_size |= kVmemBlockFree;
			_arena_allocator_block_insert_as_free(arena, new_head);

			//DEBUG:printf("new free is 0x%x\n", arena->_free_head);
        }
		
		arena->_size -= free->_size;
		// return pointer to area beyond header
		++free;
    }

    return free;
}

_JOS_API_FUNC void* arena_allocator_realloc(arena_allocator_t* arena, void* block, size_t size) {
	//TODO: there may be a cleverer way of doing this, for example by coalescing 
	//		a free block before or after this one, if possible, etc.

	if (size == 0 && block == 0) {
		return 0;
	}

	if (size == 0) {
		arena->_super.free((jos_allocator_t*)arena, block);
		return 0;
	}
	if (block == 0) {
		return arena->_super.alloc((jos_allocator_t*)arena, size);
	}

	vmem_block_head_t* head = (vmem_block_head_t*)block - 1;
	if (_JOS_VMEM_ABS_BLOCK_SIZE(head->_size) >= size) {
		//TODO: if needed; shrink the block
		return block;
	}

	void* new_block = arena->_super.alloc((jos_allocator_t*)arena, size);
	if (new_block) {
		memcpy(new_block, block, _JOS_VMEM_ABS_BLOCK_SIZE(head->_size));
	}

	return new_block;
}

_JOS_API_FUNC void arena_allocator_free(arena_allocator_t* arena, void* block)
{
    if(!arena || !block)
    {
        return;
    }
    
	if(((uint32_t*)block)[0] == 0xcdcdcdcd)
    {
		//zzz: error, double delete
		return;	
    }

	vmem_block_head_t* head = (vmem_block_head_t*)block - 1;
    vmem_block_tail_t* tail = _vmem_tail_from_head(head);
	vmem_block_tail_t* prev_tail = (vmem_block_tail_t*)head-1;

	// basic mark-as-delete...
	((uint32_t*)block)[0] = 0xcdcdcdcd;

	arena->_size += head->_size;
    // if there's a block before this and it's free we need to combine
    if((uintptr_t)prev_tail > (uintptr_t)arena + sizeof(arena_allocator_t) && _vmem_block_is_free(prev_tail->_size))
    {
        vmem_block_head_t* prev_head = _vmem_head_from_tail(prev_tail);
        // absorb previous tail and head sizes (we'll set the free-flag later)
        prev_head->_size += head->_size;
		head = prev_head;
		tail->_size = _vmem_tail_free_size(head);
		// orphan this new block
		_arena_allocator_disconnect(arena, head);
    }

    // also check if there's a free block after, we'll need to combine that one too    
    vmem_block_head_t* next_head = (vmem_block_head_t*)(tail + 1);
	const uintptr_t arena_allocator_end = (uintptr_t)arena + arena->_capacity + sizeof(arena_allocator_t);
    if((uintptr_t)next_head < arena_allocator_end && _vmem_block_is_free(next_head->_size))
    {
        const size_t prev_size = _JOS_VMEM_ABS_BLOCK_SIZE(next_head->_size);
        // absorb previous tail and head sizes
        head->_size += prev_size;
        tail = _vmem_tail_from_head(head);
		_arena_allocator_disconnect(arena, next_head);
    }
	tail->_size = _vmem_tail_free_size(head);
	head->_size |= kVmemBlockFree;
    // now we can insert the newly freed block
    _arena_allocator_block_insert_as_free(arena, head);	
}

#endif // _JOS_arena_allocator_ALLOCATOR_IMPLEMENTED

#endif // _JOS_arena_allocator_ALLOCATOR_H
