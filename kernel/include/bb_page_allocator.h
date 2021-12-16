#pragma once

#include <stdint.h>
#include <jos.h>

/*
	This is a version of the Binary Buddy allocator (https://en.wikipedia.org/wiki/Buddy_memory_allocation)
	It differs from Wikipedia's article in that it automatically shrinks and re-distributes page blocks
	according to the allocation request, instead of allocating a full block.
	This means that an allocation of 3 pages, say, will not use up an entire 4 page block but rather
	split it and create a new 1 free page block that will be re-inserted (in this case at order 0).
	This can lead to fragmentation, but frees will coalesce the pages into their original form when possible (i.e. when
	there are no holes such that the original allocation can not be reconstructed).
	TODO: fragmentation stress-test
*/
typedef struct _bb_page_allocator {

	// array of _max_order entries, each pointing to 
	// the first page in a chain of contiguous 2^order page blocks
	uintptr_t* _zones;
	// the highest order for this allocator, i.e. the largest single allocation possible (in number of pages)
	size_t		_max_order;

} bb_page_allocator_t;

_JOS_API_FUNC void bb_page_allocator_create(bb_page_allocator_t* palloc, void* pool_base, size_t pool_num_pages, static_allocation_policy_t* static_allocator);
_JOS_API_FUNC void* bb_page_allocator_allocate(bb_page_allocator_t* allocator, size_t page_count);
_JOS_API_FUNC void bb_page_allocator_free(bb_page_allocator_t* allocator, void* ptr, size_t page_count);

#if defined(_JOS_IMPLEMENT_ALLOCATORS) && !defined(_JOS_BB_PAGE_ALLOCATOR_IMPLEMENTED)
#define _JOS_BB_PAGE_ALLOCATOR_IMPLEMENTED

/*
	The first bytes of each free block of pages are used to store
	a link to the next free block (for this order), and the "buddy tag" which
	is the address of the previous adjacent page xor the address of this page.
	This is what is used to identify pages that can be coalesced when we free allocations.
*/
typedef struct _bb_page_block_header {

	struct _bb_page_block_header* _next_block;
	uintptr_t						_buddy_tag;

} _JOS_PACKED_ _bb_page_block_header_t;

#ifdef _JOS_KERNEL_BUILD
static inline size_t _ullog2(size_t n) {
	return __builtin_clzll((uint32_t)n);
}
#else
static inline size_t _ullog2(size_t n) {
	size_t l = 0;
	_BitScanReverse64(&l, n);
	return l;
}
#endif
static inline size_t _ceil_ullog2(size_t n) {
	size_t d = _ullog2(n);
	// this round-up isn't very efficient...
	return d + ((n & ((1ull << d) - 1)) ? 1 : 0);
}

#define _BB_PAGE_ALLOC_ORDER_LO(page_count)\
	_ullog2(page_count)

#define _BB_PAGE_ALLOC_ORDER_HI(page_count)\
	_ceil_ullog2(page_count)

_JOS_API_FUNC void bb_page_allocator_create(bb_page_allocator_t* palloc, void* pool_base, size_t pool_num_pages, static_allocation_policy_t* static_allocator) {

	_JOS_ASSERT(palloc);
	_JOS_ASSERT(pool_num_pages > 2);

	uintptr_t aligned_pool_base = _JOS_ALIGN(pool_base, kAllocAlign_4k);
	if ((uintptr_t)pool_base < aligned_pool_base) {
		--pool_num_pages;
	}

	if (!static_allocator) {
		// see below; we sacrifice the first page for the allocator itself
		--pool_num_pages;
	}

	const size_t n = (size_t)_BB_PAGE_ALLOC_ORDER_LO(pool_num_pages);
	palloc->_max_order = n;

	if (static_allocator) {
		palloc->_zones = static_allocator->allocator->alloc(static_allocator->allocator, sizeof(uintptr_t) * (n + 1));
	}
	else {
		// if no alternative is provided we use the first page for the allocator itself
		palloc->_zones = (uintptr_t*)aligned_pool_base;
		aligned_pool_base += (uintptr_t)kAllocAlign_4k;
	}

	// populate zones and link buddy blocks so that we can combine them later (when things are freed)
	uintptr_t last_page_block = 0;
	size_t pages = (1ull << n);
	for (int z = (int)n; z >= 0; --z) {
		if (pool_num_pages < pages) {
			palloc->_zones[z] = 0;
		}
		else {
			palloc->_zones[z] = aligned_pool_base;
			_bb_page_block_header_t* page_block_header = ((_bb_page_block_header_t*)aligned_pool_base);
			page_block_header->_next_block = NULL;
			page_block_header->_buddy_tag = last_page_block ^ aligned_pool_base;
			last_page_block = aligned_pool_base;
			aligned_pool_base += (uintptr_t)kAllocAlign_4k * pages;
			pool_num_pages -= pages;
		}
		pages >>= 1;
	}
}

_JOS_INLINE_FUNC void _bb_page_allocator_insert_pages(bb_page_allocator_t* allocator, size_t order, uintptr_t page_block, uintptr_t prev_page_block) {

	_bb_page_block_header_t* page_block_header = ((_bb_page_block_header_t*)allocator->_zones[order]);
	if (!page_block_header)
	{
		allocator->_zones[order] = page_block;
		_bb_page_block_header_t* next_page_block = (_bb_page_block_header_t*)page_block;
		next_page_block->_next_block = NULL;
		next_page_block->_buddy_tag = page_block ^ prev_page_block;
	}
	else {
		while (true) {
			if (!page_block_header->_next_block) {
				_bb_page_block_header_t* next_page_block = (_bb_page_block_header_t*)page_block;
				page_block_header->_next_block = next_page_block;
				next_page_block->_next_block = NULL;
				next_page_block->_buddy_tag = (uintptr_t)next_page_block ^ prev_page_block;
				break;
			}
			prev_page_block = (uintptr_t)page_block_header;
			page_block_header = page_block_header->_next_block;
		};
	}
}

_JOS_API_FUNC void* bb_page_allocator_allocate(bb_page_allocator_t* allocator, size_t page_count) {
	_JOS_ASSERT(allocator);
	if (!page_count) {
		return NULL;
	}

	const size_t max_pages = (1ull << allocator->_max_order);
	if (page_count > max_pages) {
		return NULL;
	}

	size_t order = (size_t)_BB_PAGE_ALLOC_ORDER_HI(page_count);
	// look for blocks large enough to hold the request
	while (order <= allocator->_max_order) {
		if (allocator->_zones[order]) {
			break;
		}
		++order;
	}
	if (order > allocator->_max_order) {
		// nothing found
		return NULL;
	}


	// the first two uintptr's of a free block of pages is used to 
	// store the link to the next block (or 0) and the address of this block xor'd with the address of the 
	// block immediately before it. This last part is used to determine adjacency when blocks are freed.
	// 
	// zone          page blocks
	// | zone N | -> | next_page_block_of_order_N | addres of prev block ^ address of this block |... | -> | next_page_block_of_order_N ...
	//
	void* ptr = (void*)allocator->_zones[order];

	size_t pages_in_zone = (1ull << order);
	_bb_page_block_header_t* next_page_block = (_bb_page_block_header_t*)allocator->_zones[order];
	allocator->_zones[order] = (uintptr_t)next_page_block->_next_block;
	size_t pages_to_relocate = (pages_in_zone - page_count);
	//printf("\torder %d allocated @ 0x%llx, %d pages left of %d\n", (int)order, (uintptr_t)ptr, (int)pages_to_relocate, (int)page_count);

	if (!pages_to_relocate) {
		// we're done
		return ptr;
	}

	uintptr_t prev_page_ptr = (uintptr_t)(ptr);
	uintptr_t realloc_page_ptr = prev_page_ptr + (page_count * kAllocAlign_4k);
	// distribute the remaining pages to lower zones
	for (int z = (int)order; z >= 0 && pages_to_relocate; z--) {
		if (pages_in_zone <= pages_to_relocate) {
			_bb_page_allocator_insert_pages(allocator, z, realloc_page_ptr, prev_page_ptr);
			//printf("\torder %d, adding 0x%llx, prev is 0x%llx\n", z, realloc_page_ptr, prev_page_ptr);
			pages_to_relocate -= pages_in_zone;
			prev_page_ptr = realloc_page_ptr;
			realloc_page_ptr += (pages_in_zone * kAllocAlign_4k);
		}
		pages_in_zone >>= 1;
	}
	return ptr;
}

_JOS_API_FUNC void bb_page_allocator_free(bb_page_allocator_t* allocator, void* ptr, size_t page_count) {

	if (!allocator || !ptr || !page_count) {
		return;
	}

	size_t order = (size_t)_BB_PAGE_ALLOC_ORDER_HI(page_count);
	_JOS_ASSERT(order <= allocator->_max_order);
	// the number of pages originally relocated
	size_t pages_relocated = (1ull << order) - page_count;
	if (!pages_relocated) {
		// an entire block was allocated
		_bb_page_allocator_insert_pages(allocator, order, (uintptr_t)ptr, 0);
	}
	else {
		// the original allocation split a block into sub-blocks that we now need to re-assemble (if possible)
		uintptr_t last_page_block = (uintptr_t)ptr;
		size_t coalesced_pages = 0ull;
		size_t pages = (1ull << order);
		for (int z = (int)order; z >= 0; --z) {
			bool has_holes = false;
			// this order could satisfy some of the original request
			if (pages_relocated >= pages) {
				has_holes = true;
				if (allocator->_zones[z]) {
					_bb_page_block_header_t* prev_block_header = NULL;
					_bb_page_block_header_t* page_block_header = ((_bb_page_block_header_t*)allocator->_zones[z]);
					do {
						if ((page_block_header->_buddy_tag ^ last_page_block) == (uintptr_t)page_block_header) {
							// coalesce these two blocks by simply removing this one from the block list, it will be absorbed by the 
							// topmost block in the original split hierarchy
							// printf("\t0x%llx and 0x%llx are buddies\n", (uintptr_t)page_block_header, last_page_block);
							last_page_block = (uintptr_t)page_block_header;
							if (!prev_block_header) {

								allocator->_zones[z] = (uintptr_t)page_block_header->_next_block;
							}
							else {
								prev_block_header->_next_block = page_block_header->_next_block;
							}
							has_holes = false;
							break;
						}
						prev_block_header = page_block_header;
						page_block_header = page_block_header->_next_block;
					} while (page_block_header);

					if (!has_holes) {
						coalesced_pages += pages;
						pages_relocated -= pages;
					}
				}
				// else we've hit a hole and can't proceed anymore this way
			}
			if (has_holes) {
				break;
			}

			pages >>= 1;
		}
		if (last_page_block != (uintptr_t)ptr) {
			// we've coalesced pages; re-insert them in the topmost order from whence this allocation came...
			// printf("\twe have coalesced %llu pages from 0x%llx to 0x%llx\n", coalesced_pages, (uintptr_t)ptr, last_page_block + (coalesced_pages * kAllocAlign_4k));
			_bb_page_allocator_insert_pages(allocator, order, (uintptr_t)ptr, 0);
		}
	}
}
#endif 
