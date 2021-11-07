#pragma once

void test_vector(heap_allocator_t* allocator);
void test_paged_list(heap_allocator_t* allocator);
void test_vector_aligned(heap_allocator_t* allocator);
void test_hive(heap_allocator_t* allocator);

void test_fixed_allocator(void);
void test_linear_allocator(void);
void test_arena_allocator_allocator(void);

