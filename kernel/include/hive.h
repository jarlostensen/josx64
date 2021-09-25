#pragma once

/*
A hive is basically a key-value store inspired by redis. 

Usage example:

	hive_t hive;
	hive_create(&hive, &_malloc_allocator);

	// create foo and set values
	hive_set(&hive, "foo", HIVE_VALUE_INT(1001), HIVE_VALUE_STR("bar"), HIVE_VALUELIST_END);
	// this will change foo's values
	hive_set(&hive, "foo", HIVE_VALUE_INT(42), HIVE_VALUE_STR("bar"), HIVE_VALUE_INT(999999), HIVE_VALUELIST_END);

	// create list1 and set some values
	hive_lpush(&hive, "list1",
		HIVE_VALUE_INT(1), HIVE_VALUE_STR("one"),
		HIVE_VALUE_INT(2), HIVE_VALUE_STR("two"),
		HIVE_VALUE_INT(3), HIVE_VALUE_STR("three"),
		HIVE_VALUELIST_END);

	// this will append values to list1
	hive_lpush(&hive, "list1",
		HIVE_VALUE_INT(4), HIVE_VALUE_STR("four"),
		HIVE_VALUE_INT(5), HIVE_VALUE_STR("five"),
		HIVE_VALUE_INT(6), HIVE_VALUE_STR("six"),
		HIVE_VALUELIST_END);

	vector_t values;
	vector_create(&values, 10, sizeof(hive_value_t), &_malloc_allocator);
	hive_lget(&hive, "list1", &values);

	// this prints
	//  "1 one 2 two 3 three 4 four 5 five 6 six"
	
	const size_t num_elements = vector_size(&values);
	for (size_t n = 0; n < num_elements; ++n) {
		hive_value_t* hive_value = (hive_value_t*)vector_at(&values, n);
		hive_value_type_t type = hive_value->type;
		switch (type) {
		case kHiveValue_Int:
		{
			printf("%lld ", hive_value->value.as_int);
		}
		break;
		case kHiveValue_Str:
		{
			printf("%s ", hive_value->value.as_str);
		}
		break;
		default:;
		}
	}

*/

#include <jos.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <collections.h>
#include <string.h>

typedef struct _hive {
	vector_t _keys;
} hive_t;

typedef enum _hive_value_type {
	kHiveValue_Int = 1,
	kHiveValue_Str = 2,
} hive_value_type_t;

_JOS_API_FUNC  void hive_create(hive_t* hive, jos_allocator_t* allocator);
// set/create a key -> value
_JOS_API_FUNC  void hive_set(hive_t* hive, const char* key, ...);
// create or append values to a list
_JOS_API_FUNC void hive_lpush(hive_t* hive, const char* key, ...);
// get the values for a key 
_JOS_API_FUNC jo_status_t hive_get(hive_t* hive, const char* key, vector_t* out_values);
// get the values for a list
_JOS_API_FUNC jo_status_t hive_lget(hive_t* hive, const char* key, vector_t* out_values);
// deletes an item from the hive
_JOS_API_FUNC jo_status_t hive_delete(hive_t* hive, const char* key);

#define _JOS_HIVE_VALUE_INT          (char)kHiveValue_Int
#define _JOS_HIVE_VALUE_STR          (char)kHiveValue_Str
#define HIVE_VALUELIST_END			 (char)(~0)

#define HIVE_VALUE_INT(x)       _JOS_HIVE_VALUE_INT, (long long)(x)
#define HIVE_VALUE_STR(x)       _JOS_HIVE_VALUE_STR, (const char*)(x)

typedef struct _hive_value {

	hive_value_type_t   type;
	union {
		long long   as_int;
		const char* as_str;
	} value;

} hive_value_t;

#if defined(_JOS_IMPLEMENT_HIVE) && !defined(_JOS_HIVE_IMPLEMENTED)
#define _JOS_HIVE_IMPLEMENTED

typedef char _hive_value_t;
#define _JOS_HIVE_SMALL_ENTRY_SIZE   max(sizeof(vector_t), (2*(sizeof(_hive_value_t)+sizeof(long long))))

typedef enum _hive_entry_type_impl {
	kHiveEntry_Key,
	kHiveEntry_List,
} _hive_entry_type_t;

typedef struct _hive_entry_impl {

	const char* _key;
	_hive_entry_type_t _type;
	int         _size;
	//NOTE: this is used for a lot of things; if the number of values stored with this key is 
	//      small enough they live here, if not it's a pointer to allocated memory, or a vector if this is a list
	uint8_t     _storage[_JOS_HIVE_SMALL_ENTRY_SIZE];

} _hive_entry_t;

_JOS_INLINE_FUNC size_t _hive_param_pack_size(va_list* pack) {

	size_t size = 0;
	_hive_value_t param = (_hive_value_t)va_arg(*pack, int);
	while (param != HIVE_VALUELIST_END) {

		size += sizeof(_hive_value_t);
		switch (param) {
		case _JOS_HIVE_VALUE_INT:
		{
			(void)va_arg(*pack, long long);
			size += sizeof(long long);
		}
		break;
		case _JOS_HIVE_VALUE_STR:
		{
			(void)va_arg(*pack, const char*);
			size += sizeof(const char*);
		}
		break;
		default:;
		}

		param = (_hive_value_t)va_arg(*pack, int);
	}
	return size;
}

_JOS_INLINE_FUNC void _hive_parse_parameter_pack(va_list* params, char* pack) {

	_hive_value_t param = (_hive_value_t)va_arg(*params, int);
	while (param != HIVE_VALUELIST_END) {

		((_hive_value_t*)pack)[0] = param;
		pack += sizeof(_hive_value_t);

		switch (param) {
		case _JOS_HIVE_VALUE_INT:
		{
			long long value = va_arg(*params, long long);
			*(long long*)pack = value; 
			pack += sizeof(long long);
		}
		break;
		case _JOS_HIVE_VALUE_STR:
		{
			const char* str = va_arg(*params, const char*);
			*(char**)pack = (char*)str;
			pack += sizeof(const char*);
		}
		break;
		default:;
		}
		param = (_hive_value_t)va_arg(*params, int);
	}
}

_JOS_INLINE_FUNC _hive_entry_t* _hive_find(hive_t* hive, const char* key, size_t* out_i) {

	size_t i = hive ? vector_size(&hive->_keys) : 0;
	if (!hive || !key || i == 0)
		return 0;

	do {
		_hive_entry_t* entry = (_hive_entry_t*)vector_at(&hive->_keys, i - 1);
		if (strcmp(entry->_key, key) == 0) {
			if (out_i)
				*out_i = i - 1;
			return entry;
		}
	} while (--i);

	return 0;
}

_JOS_API_FUNC  void hive_create(hive_t* hive, jos_allocator_t* allocator) {
	vector_create(&hive->_keys, 32, sizeof(_hive_entry_t), allocator);
}

_JOS_API_FUNC void hive_set(hive_t* hive, const char* key, ...) {

	va_list args;
	va_start(args, key);
	const size_t pack_size = _hive_param_pack_size(&args);

	if (pack_size == 0) {
		// nothing to store, or support empty keys?
		return;
	}

	va_start(args, key);

	_hive_entry_t* existing = _hive_find(hive, key, NULL);
	if (existing) {
		// update existing 

		char* pack = 0;
		int real_size = (existing->_size < 0 ? -existing->_size : existing->_size);
		if (real_size < pack_size) {

			jos_allocator_t* allocator = vector_allocator(&hive->_keys);			
			if (existing->_size > 0) {
				// original data is allocated, so we need to reallocate to fit the new pack
				pack = *(char**)&existing->_storage;
				pack = allocator->realloc(allocator, pack, pack_size);
				*(char**)&existing->_storage = pack;
				existing->_size = pack_size;
			}
			else {
				// original data is in-place, but it may not be enough space for the new data
				if (pack_size > _JOS_HIVE_SMALL_ENTRY_SIZE) {
					pack = allocator->alloc(allocator, pack_size);
					*(char**)&existing->_storage = pack;
					existing->_size = (int)pack_size;
				}
				else {
					existing->_size = -(int)pack_size;
					pack = (char*)(&existing->_storage[0]);
				}
			}
		}		
		else {
			// we can fit within existing storage
			if (existing->_size > 0) {
				pack = *(char**)&existing->_storage;
				existing->_size = pack_size;
			}
			else {
				existing->_size = -(int)pack_size;
				pack = (char*)(&existing->_storage[0]);
			}
		}

		_hive_parse_parameter_pack(&args, pack);
	}
	else {
		_hive_entry_t entry = { ._key = key, ._type = kHiveEntry_Key };
		char* pack;

		// if we can store the entry in-situ instead of allocating memory we will
		if (pack_size > _JOS_HIVE_SMALL_ENTRY_SIZE) {
			jos_allocator_t* allocator = vector_allocator(&hive->_keys);
			pack = allocator->alloc(allocator, pack_size);
			*(char**)&entry._storage = pack;
			entry._size = (int)pack_size;
		}
		else {
			entry._size = -(int)pack_size;
			pack = (char*)entry._storage;
		}

		_hive_parse_parameter_pack(&args, pack);

		vector_push_back(&hive->_keys, &entry);
	}

	va_end(args);
}

_JOS_API_FUNC void hive_lpush(hive_t* hive, const char* key, ...) {

	_hive_entry_t* entry = _hive_find(hive, key, NULL);

	_hive_entry_t base;
	vector_t* items = NULL;
	if (entry) {
		assert(entry->_type == kHiveEntry_List);
		items = (vector_t*)&entry->_storage;
	}
	else {
		base._key = key;
		base._type = kHiveEntry_List;
		items = (vector_t*)&base._storage;
		vector_create(items, 16, sizeof(hive_value_t), vector_allocator(&hive->_keys));
	}

	va_list args;
	va_start(args, key);
	_hive_value_t param = (_hive_value_t)va_arg(args, int);
	while (param != HIVE_VALUELIST_END) {

		hive_value_t hive_value;

		switch (param) {
		case _JOS_HIVE_VALUE_INT:
		{
			hive_value.type = kHiveValue_Int;
			hive_value.value.as_int = va_arg(args, long long);
		}
		break;
		case _JOS_HIVE_VALUE_STR:
		{
			hive_value.type = kHiveValue_Str;
			hive_value.value.as_str = va_arg(args, const char*);
		}
		break;
		default:;
		}
		param = (_hive_value_t)va_arg(args, int);

		vector_push_back(items, &hive_value);
	}

	if (!entry) {
		// add as new entry to the hive
		vector_push_back(&hive->_keys, (void*)&base);
	}
}

_JOS_API_FUNC jo_status_t hive_get(hive_t* hive, const char* key, vector_t* out_values) {

	assert(vector_element_size(out_values) == sizeof(hive_value_t));

	_hive_entry_t* entry = _hive_find(hive, key, NULL);
	if (!entry) {
		return _JO_STATUS_NOT_FOUND;
	}

	const char* pack;
	int pack_size;
	if (entry->_size < 0) {
		pack = (const char*)&entry->_storage;
		pack_size = -entry->_size;
	}
	else {
		pack = *(const char**)&entry->_storage;
		pack_size = entry->_size;
	}

	while (pack_size) {

		hive_value_t hive_value;

		_hive_value_t param = *(_hive_value_t*)pack;
		pack += sizeof(_hive_value_t);
		pack_size -= sizeof(_hive_value_t);

		hive_value.type = (hive_value_type_t)param;

		switch (param) {
		case _JOS_HIVE_VALUE_INT:
		{
			long long value = *(long long*)pack;
			hive_value.value.as_int = value;
			pack += sizeof(value);
			pack_size -= sizeof(value);
		}
		break;
		case _JOS_HIVE_VALUE_STR:
		{
			const char* str = *(const char**)pack;
			hive_value.value.as_str = str;
			pack += sizeof(str);
			pack_size -= sizeof(str);
		}
		break;
		default:;
		}

		vector_push_back(out_values, &hive_value);
	}
	return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC jo_status_t hive_lget(hive_t* hive, const char* key, vector_t* out_values) {

	assert(vector_element_size(out_values) == sizeof(hive_value_t));

	_hive_entry_t* entry = _hive_find(hive, key, NULL);
	if (!entry || entry->_type != kHiveEntry_List)
		return _JO_STATUS_NOT_FOUND;

	vector_append(out_values, (vector_t*)&entry->_storage);

	return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC jo_status_t hive_delete(hive_t* hive, const char* key) {
	size_t i;
	_hive_entry_t* entry = _hive_find(hive, key, &i);
	if (!entry)
		return _JO_STATUS_NOT_FOUND;

	jos_allocator_t* allocator = vector_allocator(&hive->_keys);
	switch (entry->_type) {
	case kHiveEntry_Key:
	{
		if ( entry->_size > 0 ) {
			allocator->free(allocator, *(void**)&entry->_storage);
		}
	}
	break;
	case kHiveEntry_List:
	{
		vector_t* values = (vector_t*)entry->_storage;
		vector_destroy(values);
	}
	break;
	default:
		return _JO_STATUS_UNAVAILABLE;
	}

	vector_remove(&hive->_keys, i);

	return _JO_STATUS_SUCCESS;
}

#endif