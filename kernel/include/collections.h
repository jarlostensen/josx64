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
	jos_allocator_t* _allocator;

	void*		_data;
	size_t		_capacity;
	size_t		_size;
	size_t		_element_size;
} vector_t;

// create and initialise
_JOS_API_FUNC void vector_create(vector_t* vec, size_t capacity, size_t element_size, jos_allocator_t* allocator);
// add element to the end of vector
_JOS_API_FUNC void vector_push_back(vector_t* vec, void* element);
_JOS_API_FUNC void vector_set_at(vector_t* vec, size_t i, void* element);
// get element at index n
_JOS_API_FUNC void* vector_at(vector_t* vec, size_t n);

_JOS_INLINE_FUNC _JOS_ALWAYS_INLINE void vector_destroy(vector_t* vec)
{
	vec->_allocator->free(vec->_allocator, vec->_data);
	memset(vec, 0, sizeof(vector_t));
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
	jos_allocator_t* _allocator;
	vector_t* _elements;
	size_t		_head;
	size_t		_tail;
} queue_t;

_JOS_API_FUNC void queue_create(queue_t* queue, size_t capacity, size_t element_size, jos_allocator_t* allocator);
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

#if defined(_JOS_IMPLEMENT_CONTAINERS) && !defined(_JOS_CONTAINERS_IMPLEMENTED)
#define _JOS_CONTAINERS_IMPLEMENTED

_JOS_INLINE_FUNC void _vector_decrease_size(vector_t *vector)
{
	_JOS_ASSERT(vector->_size);
	--vector->_size;
}

_JOS_INLINE_FUNC void _vector_increase_size(vector_t *vector)
{
	_JOS_ASSERT(vector->_size<vector->_capacity);
	++vector->_size;
}

_JOS_INLINE_FUNC void _vector_set_at(vector_t* vec, size_t i, void* element)
{
	_JOS_ASSERT(vec && element && vec->_capacity && vec->_element_size && i < vec->_capacity);
	i *= vec->_element_size;
	memcpy((char*)vec->_data+i, element, vec->_element_size);
}

_JOS_INLINE_FUNC void* _vector_at(vector_t* vec, size_t n)
{
	return (void*)((char*)vec->_data + n*vec->_element_size);
}

// create a vector able to hold capacity items of element_size
_JOS_API_FUNC void vector_create(vector_t* vec, size_t capacity, size_t element_size, jos_allocator_t* allocator)
{
	assert(vec && element_size && capacity && allocator);

	vec->_allocator = allocator;
	vec->_data = vec->_allocator->alloc(vec->_allocator, capacity * element_size);
	vec->_capacity = capacity;
	vec->_element_size = element_size;
	vec->_size = 0;
}

// add element to the end of vector
_JOS_API_FUNC void vector_push_back(vector_t* vec, void* element)
{
	_JOS_ASSERT(vec && element && vec->_capacity && vec->_element_size);

	if(vec->_size == vec->_capacity)
	{
		// optimal growth ratio if we want to stand a chance to re-use memory for future growth
		vec->_capacity += vec->_capacity>>1;
		vec->_data = vec->_allocator->realloc(vec->_allocator, vec->_data, vec->_capacity*vec->_element_size);
	}

	const size_t i = vec->_size*vec->_element_size;
	memcpy((char*)vec->_data+i, element, vec->_element_size);
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

////////////////////////////////////////////////////////////////////////////////
// queue

_JOS_API_FUNC  void queue_create(queue_t* queue, size_t capacity, size_t element_size, jos_allocator_t* allocator)
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

#endif

#endif // _JOS_COLLECTIONS_H
