#define _CRT_SECURE_NO_WARNINGS 1

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define _JOS_IMPLEMENT_ALLOCATORS

#include "../kernel/include/collections.h"
#include "../kernel/include/fixed_allocator.h"
#include "../kernel/include/linear_allocator.h"
#include "../kernel/include/arena_allocator.h"


void test_fixed_allocator(void) {

	char buffer[128];

	fixed_allocator_t* fixed_allocator = fixed_allocator_create(buffer, sizeof(buffer), 4);
	heap_allocator_t* allocator = (heap_allocator_t*)fixed_allocator;

	void* data12b = allocator->alloc(allocator, 12);
	assert(_JOS_PTR_IS_ALIGNED(data12b, kAllocAlign_8));
	memset(data12b, 0xff, 12);	
	allocator->free(allocator, data12b);
}

void test_linear_allocator(void) {

	char buffer[256];

	linear_allocator_t * linear_allocator = linear_allocator_create(buffer, sizeof(buffer));
	heap_allocator_t* allocator = (heap_allocator_t*)linear_allocator;

	void* data1 = allocator->alloc(allocator, 11);
	assert(_JOS_PTR_IS_ALIGNED(data1, kAllocAlign_8));
	memset(data1, 0xff, 11);
	void* data2 = allocator->alloc(allocator, 9);
	assert(_JOS_PTR_IS_ALIGNED(data2, kAllocAlign_8));
	memset(data2, 0, 9);
}

void test_arena_allocator(void) {

	char buffer[1024];

	heap_allocator_t* allocator = arena_allocator_create(buffer, sizeof(buffer));

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