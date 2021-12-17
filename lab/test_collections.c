
#define _JOS_IMPLEMENT_CONTAINERS
#define _JOS_IMPLEMENT_HIVE
#define _JOS_IMPLEMENT_BINARY_SEARCH_TREE

#pragma warning(disable:4005)

#include <jos.h>
#include "../kernel/include/collections.h"
#include "../kernel/include/hive.h"
#include "../kernel/include/binary_search_tree.h"

#include <stdio.h>

// ===============================================================================================

// ===============================================================================================

typedef struct _test_item {

	int		_a;
	char	_b;

} test_item_t;

void test_vector(generic_allocator_t* allocator) {

	printf("test_vector...");

	vector_t vector;
	vector_create(&vector, 10, sizeof(test_item_t), allocator);

	for (int n = 0; n < 100; ++n) {
		vector_push_back(&vector, &(test_item_t){
			._a = n,
			._b = (char)n
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

void test_vector_aligned(generic_allocator_t* allocator) {

	printf("test_vector_aligned...");

	vector_t	vector;
	vector_create_aligned(&vector, 10, sizeof(test_item_t), 16, allocator);
	assert(vector_element_stride(&vector) == 16);

	for (int n = 0; n < 100; ++n) {
		vector_push_back(&vector, &(test_item_t){
			._a = n,
			._b = (char)n
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

//TODO: change iterator to use at_end kliche from unordered_map instead of has_next
void test_paged_list(generic_allocator_t* allocator) {

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


static void print_hive_values(vector_t* values) {
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

void test_hive(generic_allocator_t* allocator) {

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

	const size_t hive_footprint = hive_memory_footprint(&hive);

	printf("\nHive now %lld bytes\n", hive_footprint);
	vector_reset(&values);
	hive_visit_values(&hive, _print_hive_key, &values, 0);

	hive_delete(&hive, "foo");
	hive_delete(&hive, "list1");

	printf("passed\n");
}

void unordered_map_dump_stats(unordered_map_t* umap) {
	printf("unordered_map:\n");
	for (int i = 0; i < umap->_num_slots; ++i) {
		const size_t count = vector_size(umap->_slots + i);
		if (count) {
			printf("\t");
			for (int j = 0; j < vector_size(umap->_slots + i); ++j) {
				printf("=");
			}
			printf("\n");
		}
		else {
			printf("\t.\n");
		}
	}
}

static bool int_cmp_func(const void* a, const void* b) {
	return *(const int*)a == *(const int*)b;
}

static bool str_cmp_func(const void* a, const void* b) {
	return strcmp((const char*)a, (const char*)b) == 0;
}

typedef struct _test_data {
	int _a;
	char _b;
} test_data_t;

void test_unordered_map(generic_allocator_t* allocator) {

	printf("test_unordered_map...");

	unordered_map_t umap;
	unordered_map_create(&umap, &(unordered_map_create_args_t){
		.value_size = sizeof(test_data_t),
			.key_size = sizeof(const char*),
			.hash_func = map_str_hash_func,
			.cmp_func = str_cmp_func,
			.key_type = kMap_Type_Pointer
	},
	allocator);

	const void* value = unordered_map_find(&umap, (map_key_t)"foo");
	assert(value == NULL);

	size_t inserted = 0;
	for (int n = 0; n < 1000; ++n) {
		char key[64];
		sprintf_s(key, sizeof(key), "foo%d", n);
		test_data_t v = (struct _test_data){ ._a = n + 1, ._b = n & 0xff };
		inserted += unordered_map_insert(&umap, (map_key_t)key, (map_value_t)&v) ? 1 : 0;
		value = unordered_map_find(&umap, (map_key_t)key);
		assert(value != NULL);
		assert(((const test_data_t*)value)->_a == v._a);
	}

	assert(unordered_map_size(&umap) == inserted);
	//unordered_map_dump_stats(&umap);
	unordered_map_destroy(&umap);
	printf("passed\n");
}


typedef struct _my_node {

	uintptr_t		_data;
	bool			_has_value;

} my_node_t;


static void print_node(uintptr_t key, void* value) {
	(void)value;
	printf("%llu ", key);
}

void test_binary_search_tree(generic_allocator_t* allocator) {

	binary_search_tree_t	tree;
	my_node_t node;
	binary_search_tree_create(&tree, sizeof(node), _Alignof(my_node_t), allocator);

	binary_search_tree_insert(&tree, 12345, (void*)(&(my_node_t) {
		._data = 0xdeadc0de,
		._has_value = true
		}));
	binary_search_tree_insert(&tree, 1234, (void*)(&(my_node_t) {
		._data = 0xbaadbeef,
			._has_value = true
	}));
	binary_search_tree_insert(&tree, 123456, (void*)(&(my_node_t) {
		._data = 0xfa11afe1,
			._has_value = true
	}));
	binary_search_tree_insert(&tree, 1234567, (void*)(&(my_node_t) {
		._data = 0xc0de15bad,
			._has_value = true
	}));
	binary_search_tree_insert(&tree, 123, (void*)(&(my_node_t) {
		._data = 0xf00baaa,
			._has_value = true
	}));
	//NOTE: this is a NOP, it won't insert a duplicate or overwrite 
	binary_search_tree_insert(&tree, 123, NULL);

	assert(binary_search_tree_size(&tree) == 5);
	assert(binary_search_tree_contains(&tree, 12345));
	assert(binary_search_tree_contains(&tree, 123456));
	assert(binary_search_tree_contains(&tree, 1234));

	my_node_t* pnode;
	assert(binary_search_tree_find(&tree, 123, &pnode) && pnode->_data == 0xf00baaa);
	assert(binary_search_tree_find(&tree, 1234567, &pnode) && pnode->_data == 0xc0de15bad);
	assert(binary_search_tree_find(&tree, 123456, &pnode) && pnode->_data == 0xfa11afe1);
	assert(binary_search_tree_find(&tree, 12345, &pnode) && pnode->_data == 0xdeadc0de);

	printf("\nsearch tree, sorted elements: ");
	binary_search_tree_sorted_traverse(&tree, print_node);
	printf("\n");

	binary_search_tree_destroy(&tree);
	assert(binary_search_tree_size(&tree) == 0);
}
