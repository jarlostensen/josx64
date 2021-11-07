
#define _JOS_IMPLEMENT_CONTAINERS
#define _JOS_IMPLEMENT_HIVE

#include <jos.h>
#include "../kernel/include/collections.h"
#include "../kernel/include/hive.h"

#include <stdio.h>

typedef struct _test_item {

	int		_a;
	char	_b;

} test_item_t;

void test_vector(heap_allocator_t* allocator) {

	printf("test_vector...");

	vector_t vector;
	vector_create(&vector, 10, sizeof(test_item_t), allocator);

	for (int n = 0; n < 100; ++n) {
		vector_push_back(&vector, &(test_item_t){
			._a = n,
			._b = n
		});
	}

	assert(!vector_is_empty(&vector));

	for (int n = 0; n < 100; ++n) {
		test_item_t* t = vector_at(&vector, n);
		assert(t->_a == n && t->_b == n);
	}

	vector_destroy(&vector);

	assert(vector_is_empty(&vector));

	printf("passed\n");
}

void test_vector_aligned(heap_allocator_t* allocator) {

	printf("test_vector_aligned...");

	vector_t	vector;
	vector_create_aligned(&vector, 10, sizeof(test_item_t), 16, allocator);
	assert(vector_element_stride(&vector) == 16);

	for (int n = 0; n < 100; ++n) {
		vector_push_back(&vector, &(test_item_t){
			._a = n,
				._b = n
		});
	}

	assert(!vector_is_empty(&vector));

	for (int n = 0; n < 100; ++n) {
		test_item_t* t = vector_at(&vector, n);
		assert(((uintptr_t)t & 0xf) == 0);
		assert(t->_a == n && t->_b == n); 
	}

	vector_destroy(&vector);

	assert(vector_is_empty(&vector));

	printf("passed\n");
}

void test_paged_list(heap_allocator_t* allocator) {

	printf("test_paged_list...");

	paged_list_t paged_list;
	paged_list_create(&paged_list, 8, 16, allocator);

	for (long long n = 0; n < 100; ++n) {
		paged_list_push_back(&paged_list, &n);
	}

	paged_list_iterator_t iter = paged_list_iterator_begin(&paged_list);
	long long n = 0;
	while (paged_list_iterator_has_next(&iter)) {
		long long sn = *(long long*)paged_list_iterator_value(&iter);
		assert(n++ == sn);
		paged_list_iterator_next(&iter);
	}

	assert(n == 100);
	assert(!paged_list_is_empty(&paged_list));
	assert(paged_list_size(&paged_list) == 100);

	paged_list_destroy(&paged_list);
	assert(paged_list_is_empty(&paged_list));
	assert(paged_list_size(&paged_list) == 0);

	printf("passed\n");
}


static void print_hive_values(const vector_t* values) {
	const size_t num_elements = vector_size(values);
	for (size_t n = 0; n < num_elements; ++n) {
		hive_value_t* hive_value = (hive_value_t*)vector_at(values, n);
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
		case kHiveValue_Ptr:
		{
			printf("0x%016llx", hive_value->value.as_ptr);
		}
		break;
		default:;
		}
	}
}

static void _print_hive_key(const char* key, vector_t* values, void* user_data) {
	(void*)user_data;
	if (vector_size(values) == 0) {
		printf("\t%s\n", key);
	}
	else {
		printf("\t%s : ", key);
		print_hive_values(values);
		printf("\n");
	}
}

void test_hive(heap_allocator_t* allocator) {

	printf("test_hive...");

	hive_t hive;
	hive_create(&hive, allocator);

	hive_set(&hive, "foo",
		HIVE_VALUE_INT(1), HIVE_VALUE_STR("one"),
		HIVE_VALUE_INT(2), HIVE_VALUE_STR("two"),
		HIVE_VALUE_INT(3), HIVE_VALUE_STR("three"),
		HIVE_VALUE_PTR(0xf00baa),
		HIVE_VALUELIST_END);

	hive_set(&hive, "foo",
		HIVE_VALUE_INT(4), HIVE_VALUE_STR("four"),
		HIVE_VALUE_INT(5), HIVE_VALUE_STR("five"),
		HIVE_VALUE_INT(6), HIVE_VALUE_STR("six"),
		HIVE_VALUE_INT(7), HIVE_VALUE_STR("seven"),
		HIVE_VALUE_PTR(0x10101010),
		HIVE_VALUELIST_END);

	hive_set(&hive, "bar", HIVE_VALUELIST_END);
	assert(_JO_SUCCEEDED(hive_get(&hive, "bar", 0)));

	vector_t values;
	vector_create(&values, 10, sizeof(hive_value_t), allocator);
	hive_get(&hive, "foo", &values);
	print_hive_values(&values);
	printf("\n");

	hive_lpush(&hive, "list1",
		HIVE_VALUE_INT(1), HIVE_VALUE_STR("one"),
		HIVE_VALUE_INT(2), HIVE_VALUE_STR("two"),
		HIVE_VALUE_INT(3), HIVE_VALUE_STR("three"),
		HIVE_VALUELIST_END);

	hive_lpush(&hive, "list1",
		HIVE_VALUE_INT(4), HIVE_VALUE_STR("four"),
		HIVE_VALUE_INT(5), HIVE_VALUE_STR("five"),
		HIVE_VALUE_INT(6), HIVE_VALUE_STR("six"),
		HIVE_VALUELIST_END);

	vector_reset(&values);
	hive_lget(&hive, "list1", &values);

	hive_set(&hive, "acpi:config_table_entries", HIVE_VALUE_INT(42), HIVE_VALUELIST_END);

	print_hive_values(&values);

	printf("\n");
	vector_reset(&values);
	hive_visit_values(&hive, _print_hive_key, &values, 0);

	hive_delete(&hive, "foo");
	hive_delete(&hive, "list1");

	printf("passed\n");
}
