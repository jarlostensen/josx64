#pragma once
#ifndef _JOS_COLLECTIONS_H
#define _JOS_COLLECTIONS_H

#include <stdlib.h>
#include <string.h>
#include <jos.h>

////////////////////////////////////////////////////////////////////////////////
// vector

// a vector loosely modelled on std::vector
typedef struct _vector
{
	// when we create a vector we must assign it an allocator which will be used throughout its lifetime
	heap_allocator_t* _allocator;
	// allocated memory (may be different from _data if alignment is >1)
	void*		_memory;
	// vector data 
	void*		_data;
	// current max capacity (now)
	size_t		_capacity;
	// number of elements currently held
	size_t		_size;
	// size of each element stored
	size_t		_element_size;
	// aligned size of each element stored
	size_t		_stride;
	// 
	alloc_alignment_t _alignment;
} vector_t;

// create and initialise a vector with an initial capacity
_JOS_API_FUNC void vector_create(vector_t* vec, size_t capacity, size_t element_size, heap_allocator_t* allocator);
// create and initialise a vector with an initial capacity and allocation units aligned to alignment
_JOS_API_FUNC void vector_create_aligned(vector_t* vec, size_t capacity, size_t element_size, alloc_alignment_t alignment, heap_allocator_t* allocator);
// add element to the end of vector
_JOS_API_FUNC void vector_push_back(vector_t* vec, void* element);
// push one element as two packed elements (used for maps, for example)
// NOTE: alignment is not guaranteed, unless the combined item size ensures it
_JOS_API_FUNC void vector_push_back_pair(vector_t* vec, const void* first, size_t first_size, const void* second, size_t second_size);
_JOS_API_FUNC void vector_set_at(vector_t* vec, size_t i, void* element);
// get element at index n
_JOS_API_FUNC void* vector_at(vector_t* vec, size_t n);
// append src to dest
_JOS_API_FUNC void vector_append(vector_t* dest, const vector_t* src);
// remove an element from the vector by swapping it with the last and shrinking the vector
_JOS_API_FUNC jo_status_t vector_remove(vector_t* vec, const size_t at);

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE heap_allocator_t* vector_allocator(vector_t* vec) {
	return vec->_allocator;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_create_like(vector_t* vec, const vector_t* original) {
	vector_create(vec, original->_capacity, original->_element_size, original->_allocator);
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_reset(vector_t* vec) {
	// simply reset, no clearing of memory!
	vec->_size = 0;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE size_t vector_element_size(vector_t* vec) {
	return vec->_element_size;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE size_t vector_element_stride(vector_t* vec) {
	return vec->_stride;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_destroy(vector_t* vec)
{
	vec->_allocator->free(vec->_allocator, vec->_memory);
	memset(vec, 0, sizeof(vector_t));
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_swap(vector_t* veca, vector_t* vecb) {
#if defined(_JOS_SWAP_PTR)
	_JOS_SWAP_PTR(veca->_allocator, vecb->_allocator);
	_JOS_SWAP_PTR(veca->_data, vecb->_data);
#else
	uintptr_t tmp_a = (uintptr_t)veca->_allocator;
	uintptr_t tmp_b = (uintptr_t)vecb->_allocator;
	_JOS_SWAP(tmp_a, tmp_b);
	veca->_allocator = (heap_allocator_t*)tmp_a;
	vecb->_allocator = (heap_allocator_t*)tmp_b;
	tmp_a = (uintptr_t)veca->_data;
	tmp_b = (uintptr_t)vecb->_data;
	_JOS_SWAP(tmp_a, tmp_b);
	veca->_data = (void*)tmp_a;
	vecb->_data = (void*)tmp_b;
#endif
	_JOS_SWAP(veca->_capacity, vecb->_capacity);
	_JOS_SWAP(veca->_size, vecb->_size);
	_JOS_SWAP(veca->_element_size, vecb->_element_size);
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE size_t vector_size(vector_t* vec)
{
	return vec->_size;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE size_t vector_capacity(vector_t* vec)
{
	return vec->_capacity;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE bool vector_is_empty(vector_t* vec)
{
	return !vec->_size;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE bool vector_is_full(vector_t* vec)
{
	return vec->_size == vec->_capacity;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_clear(vector_t* vec)
{
	vec->_size = 0;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_push_back_ptr(vector_t* vec, void* ptr)
{
	_JOS_ASSERT(sizeof(void*) == vec->_element_size);
	vector_push_back(vec, &ptr);
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void* vector_at_ptr(vector_t* vec, size_t n)
{
	return (void*)(*((uintptr_t**)vector_at(vec, n)));
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void* vector_data(vector_t* vec) {
	return vec->_data;
}

////////////////////////////////////////////////////////////////////////////////
// queue

typedef struct _queue
{
	heap_allocator_t* _allocator;
	vector_t* _elements;
	size_t		_head;
	size_t		_tail;
} queue_t;

_JOS_API_FUNC void queue_create(queue_t* queue, size_t capacity, size_t element_size, heap_allocator_t* allocator);
_JOS_API_FUNC void queue_pop(queue_t* queue);
_JOS_API_FUNC void queue_push(queue_t* queue, void* element);
_JOS_API_FUNC void queue_push_ptr(queue_t* queue, void* ptr);

_JOS_INLINE_FUNC bool queue_is_empty(queue_t* queue)
{
	return vector_is_empty(queue->_elements);
}

_JOS_INLINE_FUNC bool queue_is_full(queue_t* queue)
{
	return vector_is_full(queue->_elements);
}

_JOS_API_FUNC void* queue_front(queue_t* queue);

_JOS_INLINE_FUNC void queue_clear(queue_t* queue)
{
	vector_clear(queue->_elements);
	queue->_head = queue->_tail = 0;
}

_JOS_INLINE_FUNC void queue_destroy(queue_t* queue)
{
	vector_destroy(queue->_elements);
	queue->_allocator->free(queue->_allocator, queue->_elements);
	memset(queue, 0, sizeof(queue_t));
}

_JOS_API_FUNC void* queue_front_ptr(queue_t* queue);

static const vector_t kEmptyVector = { ._data = 0, ._capacity = 0, ._size = 0, ._element_size = 0 };

////////////////////////////////////////////////////////////////////////////////
// unordered_map
// 
// for example:
// 
// typedef struct _test_data {
//	 int _a;
//	 char _b;
// } test_data_t;
// 
// unordered_map_t umap;
// unordered_map_create(&umap, &(unordered_map_create_args_t){
//	.value_size = sizeof(test_data_t),
//		.key_size = sizeof(int),
//		.hash_func = identity_hash_func,
//		.cmp_func = int_cmp_func
// },
// & _malloc_allocator);
//
// int k = rand();
// const void* value = unordered_map_find(&umap, (map_key_t)&k);
// assert(value == NULL);
//
// size_t inserted = 0;
// for (int n = 0; n < 1000; ++n) {
//	 k = rand();
//	 test_data_t v = (struct _test_data){ ._a = rand(), ._b = n & 0xff };
//	 inserted += unordered_map_insert(&umap, (map_key_t)&k, (map_value_t)&v) ? 1 : 0;
//	 value = unordered_map_find(&umap, (map_key_t)&k);
//	 assert(value != NULL);
//	 assert(((const test_data_t*)value)->_a == v._a);
// }
//
// assert(unordered_map_size(&umap) == inserted);
// unordered_map_destroy(&umap);

typedef const void* map_key_t;
typedef const void* map_value_t;

typedef uint32_t(*map_key_hash_func)(map_key_t);
typedef bool(*map_key_cmp_func)(map_key_t, map_key_t);

// helper: returns 32 bit hash (Pearson) for a key as const char*
_JOS_API_FUNC uint32_t map_str_hash_func(const void* key);
// helper: string compare function
_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE bool map_str_cmp_func(const void* a, const void* b) {
	return !strcmp((const char*)a, (const char*)b);
}

// because C: we need to specify if the map KEY is a pointer or a value 
// to ensure it is used correctly internally in the maps
typedef enum _map_type_trait {

	kMap_Type_Value,
	kMap_Type_Pointer,

} map_type_trait_t;

typedef struct _unordered_map {

	vector_t* _slots;
	size_t  _value_size;
	size_t  _key_size;
	map_type_trait_t _key_type;
	size_t  _num_slots;
	size_t  _occupancy;
	heap_allocator_t* _allocator;
	map_key_hash_func _hash_func;
	map_key_cmp_func _cmp_func;

} unordered_map_t;

typedef struct _unordered_map_create_args {

	map_key_hash_func   hash_func;
	map_key_cmp_func    cmp_func;
	size_t          key_size;
	size_t          value_size;
	map_type_trait_t key_type;

} unordered_map_create_args_t;

_JOS_API_FUNC void unordered_map_create(unordered_map_t* umap, unordered_map_create_args_t* args, heap_allocator_t* allocator);
_JOS_API_FUNC void unordered_map_destroy(unordered_map_t* umap);
_JOS_API_FUNC map_value_t unordered_map_find(unordered_map_t* umap, map_key_t key);
_JOS_API_FUNC bool unordered_map_insert(unordered_map_t* umap, map_key_t key, map_value_t item);
_JOS_INLINE_FUNC size_t unordered_map_size(unordered_map_t* umap) {
	return umap->_occupancy;
}
_JOS_INLINE_FUNC heap_allocator_t* unordered_map_allocator(unordered_map_t* umap) {
	return umap->_allocator;
}

/* iterator for unordered_map
* example usage:
*  
* unordered_map_iterator_t iter = unordered_map_iterator_begin(&umap);
* while (!unordered_map_iterator_at_end(&iter)) {
*	const char* key = (const char*)unordered_map_iterator_key(&iter);
*	some_data_t* item = (some_data_t*)unordered_map_iterator_value(&iter);
*		...
*	unordered_map_iterator_next(&iter);
* }
*/
typedef struct _unordered_map_iterator {

	unordered_map_t* _umap;
	size_t	_i;
	size_t	_j;

} unordered_map_iterator_t;

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE bool unordered_map_iterator_at_end(unordered_map_iterator_t* iter) {
	return iter->_i == iter->_umap->_num_slots;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE unordered_map_iterator_t unordered_map_iterator_begin(unordered_map_t* umap) {
	unordered_map_iterator_t iter = {
		._umap = umap
	};
	// make sure we're starting on a non-empty slot
	do {
		if (!unordered_map_iterator_at_end(&iter)
			&&
			vector_size(iter._umap->_slots + iter._i)) {
			break;
		}
		++iter._i;
	} while (!unordered_map_iterator_at_end(&iter));
	return iter;
}

_JOS_API_FUNC void unordered_map_iterator_next(unordered_map_iterator_t* iter);

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE map_key_t unordered_map_iterator_key(unordered_map_iterator_t* iter) {
	if ( iter->_umap->_key_type == kMap_Type_Value )
		return (map_key_t)vector_at(iter->_umap->_slots + iter->_i, iter->_j);
	return *(map_key_t**)vector_at(iter->_umap->_slots + iter->_i, iter->_j);
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE map_value_t unordered_map_iterator_value(unordered_map_iterator_t* iter) {
	return (map_value_t)((uintptr_t)vector_at(iter->_umap->_slots + iter->_i, iter->_j) + iter->_umap->_key_size);
}

////////////////////////////////////////////////////////////////////////////////
// paged_list
// a linked list of pages each holding N items
// ZZZ: Don't rely on this yet, it will be re-worked in the spirit of UE's FMemStack 

typedef struct _paged_list_page_header {

	size_t		_items;

} _paged_list_page_header_t;

typedef struct _paged_list {

	// each page has the following layout:
	// 	  
	// high mem
	// ------------------------------|
	// |  ptr to next page or NULL   |
	// |  ---------------------------|
	// |                             |
	// |  ......... items .......... |
	// | ____________________________|
	// | paged_list_t                |
	// |_____________________________|
	//  lo mem
	//
	_paged_list_page_header_t* _page0;
	size_t _page_slot_count;
	size_t _element_size;
	size_t _items;
	heap_allocator_t* _allocator;

} paged_list_t;

_JOS_INLINE_FUNC void paged_list_create(paged_list_t* paged_list, size_t element_size, size_t items_per_page, heap_allocator_t* allocator) {
	paged_list->_allocator = allocator;
	paged_list->_element_size = element_size;
	paged_list->_page0 = 0;
	paged_list->_page_slot_count = items_per_page;
	paged_list->_items = 0;
}

_JOS_API_FUNC void paged_list_push_back(paged_list_t* paged_list, void* item);

// iterator for paged lists
// usage example:
// 
// paged_list_iterator_t iter = paged_list_iterator_begin(&paged_list);
// while (paged_list_iterator_has_next(&iter)) {
//		some_type_t * sn = (some_type_t*)paged_list_iterator_value(&iter);
//		...
//		paged_list_iterator_next(&iter);
// }
//
typedef struct _paged_list_iterator {

	paged_list_t* _list;
	_paged_list_page_header_t* _header;
	uintptr_t	_offset;
	size_t	_index;

} paged_list_iterator_t;

_JO_INLINE_FUNC paged_list_iterator_t paged_list_iterator_begin(paged_list_t* paged_list) {	
	return (struct _paged_list_iterator) {
		._header = paged_list->_page0,
		._list = paged_list,
		._offset = (uintptr_t)(paged_list->_page0 + 1)
	};
}

_JO_INLINE_FUNC _JOS_ALWAYS_INLINE bool paged_list_iterator_has_next(paged_list_iterator_t* iter) {
	return (iter && iter->_header) && (iter->_index < iter->_header->_items);
}

_JO_INLINE_FUNC _JOS_ALWAYS_INLINE void* paged_list_iterator_value(paged_list_iterator_t* iter) {
	return (void*)iter->_offset;
}

_JOS_API_FUNC void paged_list_iterator_next(paged_list_iterator_t* iter);

_JOS_API_FUNC void paged_list_destroy(paged_list_t* paged_list);

_JO_INLINE_FUNC bool paged_list_is_empty(paged_list_t* paged_list) {
	return !paged_list || !paged_list->_page0 || !paged_list->_items;
}

_JO_INLINE_FUNC size_t paged_list_size(paged_list_t* paged_list) {
	return paged_list!=0 ? paged_list->_items : 0;
}

// ============================================================================================
// implementations

#if defined(_JOS_IMPLEMENT_CONTAINERS) && !defined(_JOS_CONTAINERS_IMPLEMENTED)
#define _JOS_CONTAINERS_IMPLEMENTED

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void _vector_decrease_size(vector_t *vector)
{
	_JOS_ASSERT(vector->_size);
	--vector->_size;
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void _vector_increase_size(vector_t *vector)
{
	_JOS_ASSERT(vector->_size<vector->_capacity);
	++vector->_size;
}

_JOS_INLINE_FUNC void _vector_set_at(vector_t* vec, size_t i, void* element)
{
	_JOS_ASSERT(vec && element && vec->_capacity && vec->_element_size && i < vec->_capacity);
	i *= vec->_stride;
	memcpy((char*)vec->_data+i, element, vec->_element_size);
}

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void* _vector_at(vector_t* vec, size_t n)
{
	return (void*)((char*)vec->_data + n*vec->_stride);
}

_JOS_INLINE_FUNC void _vector_check_grow(vector_t* vec) {
	// is this contrived, or really better for a branch? only profiling will show...
	if (vec->_size == vec->_capacity) {
		if (vec->_alignment == kAllocAlign_None) {
			// optimal growth ratio if we want to stand a chance to re-use memory for future growth		
			vec->_capacity += vec->_capacity >> 1;
			vec->_data = vec->_memory = vec->_allocator->realloc(vec->_allocator, vec->_memory, vec->_capacity * vec->_stride);
		}
		else {
			//ZZZ: inefficient!
			//TODO: realloc should support alignment (all allocations should)
			const size_t data_size = vec->_capacity * vec->_stride;
			vec->_capacity += vec->_capacity >> 1;
			void* base;
			void* aligned;
			aligned_alloc(vec->_allocator, vec->_capacity * vec->_stride, vec->_alignment, &base, &aligned);
			memcpy(aligned, vec->_data, data_size);
			vec->_allocator->free(vec->_allocator, vec->_memory);
			vec->_memory = base;
			vec->_data = aligned;
		}
	}
}

// create a vector able to hold capacity items of element_size
_JOS_API_FUNC void vector_create(vector_t* vec, size_t capacity, size_t element_size, heap_allocator_t* allocator)
{
	assert(vec && element_size && capacity && allocator);

	vec->_allocator = allocator;
	vec->_alignment = kAllocAlign_None;
	vec->_memory = vec->_data = vec->_allocator->alloc(vec->_allocator, capacity * element_size);
	vec->_capacity = capacity;
	vec->_stride = vec->_element_size = element_size;
	vec->_size = 0;	
}

_JOS_API_FUNC void vector_create_aligned(vector_t* vec, size_t capacity, size_t element_size, alloc_alignment_t alignment, heap_allocator_t* allocator) {
	assert(vec && element_size && capacity && allocator);

	vec->_allocator = allocator;	
	vec->_capacity = capacity;
	vec->_element_size = element_size;
	vec->_alignment = alignment;
	// the optimiser(s) won't cancel this, which is nice
	// https://t.co/V2hWdUFGGJ
	vec->_stride = (int)alignment * ((element_size + ((int)alignment - (int)1)) / (int)alignment);
	aligned_alloc(allocator, capacity * vec->_stride, alignment, &vec->_memory, &vec->_data);	
	vec->_size = 0;
}

// add element to the end of vector
_JOS_API_FUNC void vector_push_back(vector_t* vec, void* element)
{
	_JOS_ASSERT(vec && element && vec->_capacity && vec->_element_size);
	_vector_check_grow(vec);
	const size_t i = vec->_size * vec->_stride;
	memcpy((char*)vec->_data + i, element, vec->_element_size);
	_vector_increase_size(vec);
}

_JOS_API_FUNC void vector_push_back_pair(vector_t* vec, 
			const void* first, size_t first_size, const void* second, size_t second_size) {
	_JOS_ASSERT(first_size + second_size == vec->_element_size);

	_vector_check_grow(vec);
	const size_t i = vec->_size * vec->_stride;
	memcpy((char*)vec->_data + i, first, first_size);
	memcpy((char*)vec->_data + i + first_size, second, second_size);
	_vector_increase_size(vec);
}

_JOS_API_FUNC void vector_set_at(vector_t* vec, size_t i, void* element)
{
	_JOS_ASSERT(i < vec->_size);
	_vector_set_at(vec, i, element);
}

// get element at index n
_JOS_API_FUNC void* vector_at(vector_t* vec, size_t n)
{
	_JOS_ASSERT(vec && n < vec->_size);
	return _vector_at(vec, n);
}

_JOS_API_FUNC jo_status_t vector_remove(vector_t* vec, const size_t at) {
	if (!vec || at > vec->_size)
		return _JO_STATUS_OUT_OF_RANGE;

	if (vec->_size > 1) {
		memcpy((char*)vec->_data + at, (char*)vec->_data + ((vec->_size - 1) * vec->_stride), vec->_stride);
	}
	--vec->_size;

	return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC void vector_append(vector_t* dest, const vector_t* src) {
	_JOS_ASSERT(src->_element_size == dest->_element_size);

	size_t chunk_size = min(dest->_capacity - dest->_size, src->_size);
	const char* src_ptr = src->_data;
	if (chunk_size) {
		memcpy((char*)dest->_data + dest->_size, src_ptr, src->_stride * chunk_size);
		dest->_size += chunk_size;
		src_ptr += src->_stride * chunk_size;
		chunk_size = src->_size - chunk_size;
	}
	else {
		chunk_size = src->_size;
	}
	for (size_t n = 0; n < chunk_size; ++n) {
		vector_push_back(dest, (void*)src_ptr);
		src_ptr += src->_stride;
	}
}

_JOS_API_FUNC  void queue_create(queue_t* queue, size_t capacity, size_t element_size, heap_allocator_t* allocator)
{
	queue->_allocator = allocator;
	queue->_elements = (vector_t*)queue->_allocator->alloc(queue->_allocator, sizeof(vector_t));
	vector_create(queue->_elements, capacity, element_size, queue->_allocator);
	queue->_head = queue->_tail = 0;
}

_JOS_INLINE_FUNC void* queue_front(queue_t* queue)
{
	if (queue_is_empty(queue))
		return 0;
	return _vector_at(queue->_elements, queue->_head);
}

_JOS_API_FUNC void* queue_front_ptr(queue_t* queue)
{
	if (queue_is_empty(queue))
		return 0;
	return (void*)(*((uintptr_t**)_vector_at(queue->_elements, queue->_head)));
}

_JOS_API_FUNC void queue_pop(queue_t* queue)
{
	if(queue_is_empty(queue))
		return;
	queue->_head = (queue->_head+1) % vector_capacity(queue->_elements);
	_vector_decrease_size(queue->_elements);
}

_JOS_API_FUNC void queue_push(queue_t* queue, void * element)
{
	_JOS_ASSERT(!queue_is_full(queue));
	_vector_set_at(queue->_elements, queue->_tail, element);
	queue->_tail = (queue->_tail+1) % vector_capacity(queue->_elements);
	_vector_increase_size(queue->_elements);
}

// policy for pointers, pushes the pointer value ptr on to the queue.
_JOS_API_FUNC void queue_push_ptr(queue_t* queue, void * ptr)
{
	_JOS_ASSERT(!queue_is_full(queue));
	_JOS_ASSERT(sizeof(void*)==queue->_elements->_element_size);
	_vector_set_at(queue->_elements, queue->_tail, &ptr);
	queue->_tail = (queue->_tail+1) % vector_capacity(queue->_elements);
	_vector_increase_size(queue->_elements);
}

_JOS_API_FUNC void paged_list_push_back(paged_list_t* paged_list, void* item) {

	if (!paged_list || !item)
		//TODO: let's use results across all these
		return;

	//TODO: item alignment (also for vectors and queues)

	_paged_list_page_header_t* header = paged_list->_page0;
	_paged_list_page_header_t* prev = NULL;
	const size_t page_stride = paged_list->_element_size * paged_list->_page_slot_count;
	while (header && header->_items == paged_list->_page_slot_count) {
		prev = header;
		header = *(_paged_list_page_header_t**)((char*)(header + 1) + page_stride);
	}
	if (!header) {
		// allocate another page and add it to the end of the page list
		header = paged_list->_allocator->alloc(paged_list->_allocator, sizeof(_paged_list_page_header_t) + page_stride + sizeof(void*));
		if (prev) {
			*(_paged_list_page_header_t**)((char*)(prev + 1) + page_stride) = header;
		}
		else {
			paged_list->_page0 = header;
		}
		*(_paged_list_page_header_t**)((char*)(header + 1) + page_stride) = 0;
		header->_items = 0;
	}

	memcpy((char*)(header + 1) + header->_items * paged_list->_element_size, item, paged_list->_element_size);
	++header->_items;
	++paged_list->_items;
}


_JOS_API_FUNC void paged_list_iterator_next(paged_list_iterator_t* iter) {
	if (++iter->_index < iter->_header->_items) {
		iter->_offset += iter->_list->_element_size;
		return;
	}

	if (iter->_header->_items == iter->_list->_page_slot_count) {
		iter->_header = *(_paged_list_page_header_t**)((char*)(iter->_header + 1) + (iter->_list->_page_slot_count * iter->_list->_element_size));
		iter->_index = 0;
		iter->_offset = (uintptr_t)(iter->_header+1);
	}
	// else there are no more items
}

_JOS_API_FUNC void paged_list_destroy(paged_list_t* paged_list) {

	if (!paged_list || !paged_list->_page0)
		return;

	_paged_list_page_header_t* header = paged_list->_page0;
	const size_t page_stride = paged_list->_element_size * paged_list->_page_slot_count;
	while (header && header->_items == paged_list->_page_slot_count) {
		_paged_list_page_header_t* curr = header;
		header = *(_paged_list_page_header_t**)((char*)(header + 1) + page_stride);
		paged_list->_allocator->free(paged_list->_allocator, curr);
	}

	memset(paged_list, 0, sizeof(paged_list_t));
}

_JOS_API_FUNC uint32_t map_str_hash_func(const void* key) {
	const char* str = (const char*)key;
	if (!str)
		return 0;
	const size_t str_len = strlen(str);
	if (!str_len)
		return 0;

	// https://en.wikipedia.org/wiki/Pearson_hashing

	static unsigned char kPermutations[256] = { 0 };
	// first time we need to initialise with random permutations of [0..255]
	if (kPermutations[0] == kPermutations[1]) {
		for (int i = 0; i < 256; ++i) {
			kPermutations[i] = i;
		}
		for (int i = 0; i < 254; ++i) {
			int idx = i + 1 + (rand() % (254 - i));
			_JOS_SWAP(kPermutations[i], kPermutations[idx]);
		}
		// always swap the last two elements
		_JOS_SWAP(kPermutations[254], kPermutations[255]);
	}

	unsigned char hash[4];
	// 8 bit sub hashes
	hash[0] = (str[0] + 0) % 255;
	hash[1] = (str[0] + 1) % 255;
	hash[2] = (str[0] + 2) % 255;
	hash[3] = (str[0] + 3) % 255;
	for (size_t n = 0; n < str_len; ++n) {
		const unsigned char c = (unsigned char)str[n];
		hash[0] = kPermutations[hash[0] ^ c];
		hash[1] = kPermutations[hash[1] ^ c];
		hash[2] = kPermutations[hash[2] ^ c];
		hash[3] = kPermutations[hash[3] ^ c];
	}
	return *(uint32_t*)(hash);
}

_JOS_API_FUNC void unordered_map_create(unordered_map_t* umap, unordered_map_create_args_t* args, heap_allocator_t* allocator) {

	umap->_hash_func = args->hash_func;
	umap->_cmp_func = args->cmp_func;
	umap->_allocator = allocator;
	umap->_value_size = args->value_size;
	umap->_key_size = args->key_size;
	umap->_key_type = args->key_type;
	umap->_num_slots = 511;
	umap->_occupancy = 0;
	umap->_slots = allocator->alloc(allocator, sizeof(vector_t) * umap->_num_slots);
	_JOS_ASSERT(args->key_type == kMap_Type_Value || sizeof(args->key_size) == sizeof(void*));
	_JOS_ASSERT(umap->_slots);
	for (int i = 0; i < umap->_num_slots; ++i) {
		vector_create(umap->_slots + i, 32, args->key_size + args->value_size, allocator);
	}
}

_JOS_API_FUNC void unordered_map_destroy(unordered_map_t* umap) {
	for (int i = 0; i < umap->_num_slots; ++i) {
		vector_destroy(umap->_slots + i);
	}
}

_JOS_INLINE_FUNC map_key_t _unordered_map_typed_key(unordered_map_t* umap, map_key_t key) {
	return umap->_key_type == kMap_Type_Value ? key : *(map_key_t**)key;
}

typedef struct _map_slot_value {
	vector_t* _slot;
	size_t _i;
	void* _value;
} _map_slot_value_t;
_JOS_INLINE_FUNC _map_slot_value_t _unordered_map_find(unordered_map_t* umap, map_key_t key) {	
	uint32_t hash = umap->_hash_func(key);
	size_t i = hash % umap->_num_slots;		
	vector_t* slot_vector = umap->_slots + i;
	for (size_t n = 0u; n < vector_size(slot_vector); ++n) {
		const void* pair = vector_at(slot_vector, n);
		const map_key_t typed_key = _unordered_map_typed_key(umap, pair);
		if (umap->_cmp_func(typed_key, key)) {
			return (struct _map_slot_value) {
				._slot = slot_vector,
					._i = n,
					._value = (void*)((uintptr_t)pair + umap->_key_size)
			};
		}
	}
	return (struct _map_slot_value) {
		._slot = slot_vector
	};
}

_JOS_API_FUNC map_value_t unordered_map_find(unordered_map_t* umap, map_key_t key) {	
	_map_slot_value_t sv = _unordered_map_find(umap, key);
	return (map_value_t)sv._value;
}

_JOS_API_FUNC bool unordered_map_insert(unordered_map_t* umap, map_key_t key, map_value_t item) {
	_map_slot_value_t sv = _unordered_map_find(umap, key);
	if (sv._value) {
		// found, just poke the value
		memcpy(sv._value, item, umap->_value_size);
		return false;
	}
	// otherwise we need to add it 
	if (umap->_key_type == kMap_Type_Value) {
		vector_push_back_pair(sv._slot, key, umap->_key_size, item, umap->_value_size);
	}
	else {
		vector_push_back_pair(sv._slot, &key, umap->_key_size, item, umap->_value_size);
	}
	umap->_occupancy++;
	return true;
}

_JOS_API_FUNC bool unordered_map_remove(unordered_map_t* umap, map_key_t key) {
	_map_slot_value_t sv = _unordered_map_find(umap, key);
	if (sv._value) {
		vector_remove(sv._slot, sv._i);
		return true;
	}
	return false;
}

_JOS_API_FUNC void unordered_map_iterator_next(unordered_map_iterator_t* iter) {
	if ((iter->_j + 1) < vector_size(iter->_umap->_slots + iter->_i)) {
		++iter->_j;
	}
	else {
		do {
			++iter->_i;
			if (!unordered_map_iterator_at_end(iter)
				&&
				vector_size(iter->_umap->_slots + iter->_i)) {
				break;
			}
		} while (!unordered_map_iterator_at_end(iter));
		iter->_j = 0;
	}
}

#endif

#endif // _JOS_COLLECTIONS_H
