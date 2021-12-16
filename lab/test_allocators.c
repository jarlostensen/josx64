#define _CRT_SECURE_NO_WARNINGS 1

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define _JOS_IMPLEMENT_ALLOCATORS

#include "../kernel/include/collections.h"
#include "../kernel/include/fixed_allocator.h"
#include "../kernel/include/linear_allocator.h"
#include "../kernel/include/arena_allocator.h"
#include "../kernel/include/bb_page_allocator.h"


static void _dump_bb_allocator(bb_page_allocator_t* allocator) {
	printf("-------------------------------------------------------------------------\n");
	size_t pages_available = 0;
	for (size_t z = 0; z <= allocator->_max_order; ++z) {
		printf("order %llu [", z);
		if (allocator->_zones[z]) {
			const size_t page_block_size = (1ull << z);
			size_t blocks_in_zone = 0;
			uintptr_t prev_page_block = 0;
			_bb_page_block_header_t* next_page_block = (_bb_page_block_header_t*)allocator->_zones[z];
			do {
				++blocks_in_zone;
				pages_available += page_block_size;
				if (prev_page_block && (next_page_block->_buddy_tag ^ prev_page_block) == (uintptr_t)next_page_block) {
					printf(">");
				}
				else {
					printf("%c",178);
				}
				prev_page_block = (uintptr_t)next_page_block;
				next_page_block = next_page_block->_next_block;
			} while(next_page_block);
			printf("] %llu blocks, %llu pages at 0x%llx\n", blocks_in_zone, (blocks_in_zone * page_block_size), allocator->_zones[z]);
		}
		else {
			printf("]\n");
		}
	}
	printf("\t%llu pages, %llu bytes\n", pages_available, pages_available * kAllocAlign_4k);
}

void test_page_allocator(void) {

	static const size_t kPageCount = 1023;
	void* page_pool = malloc(kAllocAlign_4k * kPageCount);

	bb_page_allocator_t allocator;
	bb_page_allocator_create(&allocator, page_pool, kPageCount, NULL);
	_dump_bb_allocator(&allocator);
	printf("\n");

	void* pages = bb_page_allocator_allocate(&allocator, 13);	
	_dump_bb_allocator(&allocator);
	printf("\n");
	void* pages2 = bb_page_allocator_allocate(&allocator, 3);
	_dump_bb_allocator(&allocator);
	bb_page_allocator_free(&allocator, pages, 13);
	_dump_bb_allocator(&allocator);
	bb_page_allocator_free(&allocator, pages2, 3);
	_dump_bb_allocator(&allocator);
	
	free(page_pool);
}

void test_fixed_allocator(void) {

	char buffer[128];

	fixed_allocator_t* fixed_allocator = fixed_allocator_create(buffer, sizeof(buffer), 4);
	generic_allocator_t* allocator = (generic_allocator_t*)fixed_allocator;

	void* data12b = allocator->alloc(allocator, 12);
	assert(_JOS_PTR_IS_ALIGNED(data12b, kAllocAlign_8));
	memset(data12b, 0xff, 12);	
	allocator->free(allocator, data12b);
}

void test_linear_allocator(void) {

	char buffer[256];

	linear_allocator_t * linear_allocator = linear_allocator_create(buffer, sizeof(buffer));
	generic_allocator_t* allocator = (generic_allocator_t*)linear_allocator;

	void* data1 = allocator->alloc(allocator, 11);
	assert(_JOS_PTR_IS_ALIGNED(data1, kAllocAlign_8));
	memset(data1, 0xff, 11);
	void* data2 = allocator->alloc(allocator, 9);
	assert(_JOS_PTR_IS_ALIGNED(data2, kAllocAlign_8));
	memset(data2, 0, 9);
}

void test_arena_allocator(void) {

	char buffer[1024];

	generic_allocator_t* allocator = (generic_allocator_t*)arena_allocator_create(buffer, sizeof(buffer));

	void* allocated = allocator->alloc(allocator, 127);
	void* allocated2 = allocator->alloc(allocator, 129);
	assert(_JOS_PTR_IS_ALIGNED(allocated, kAllocAlign_8));
	assert(_JOS_PTR_IS_ALIGNED(allocated2, kAllocAlign_8));
	memset(allocated, 0x11, 128);
	allocated = allocator->realloc(allocator, allocated, 255);
	assert(_JOS_PTR_IS_ALIGNED(allocated, kAllocAlign_8));
	memset((char*)allocated+128, 0x22, 128);

	allocator->free(allocator, allocated);
}
