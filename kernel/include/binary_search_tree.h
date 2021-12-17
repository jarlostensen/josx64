#pragma once

#include <jos.h>

typedef struct _bsearch_tree_node _bsearch_tree_node_t;
/*
* Basic binary search tree with key-value storage.
* The tree is implemented using an AVL tree to ensure balance is maintained.
* Keys are stored in nodes, values are stored separately and indexed from a node only when needed.
*/
typedef struct _binary_search_tree {

	generic_allocator_t* _allocator;
	_bsearch_tree_node_t* _root;
	vector_t			 _data;
	size_t				 _node_size;
	size_t				 _node_count;
} binary_search_tree_t;

_JOS_API_FUNC void binary_search_tree_create(binary_search_tree_t* tree, size_t node_size, alloc_alignment_t node_alignment, generic_allocator_t* allocator);
_JOS_API_FUNC void binary_search_tree_destroy(binary_search_tree_t* tree);

_JOS_API_FUNC _bsearch_tree_node_t* _binary_search_tree_insert_impl(binary_search_tree_t* tree, _bsearch_tree_node_t* parent, uintptr_t key, void* value);
/*
* Insert the given key[value] into the tree if it does not already exist
* Returns true if the node was inserted
*/
_JOS_INLINE_FUNC bool binary_search_tree_insert(binary_search_tree_t* tree, uintptr_t key, void* value) {
	_bsearch_tree_node_t* pnode = _binary_search_tree_insert_impl(tree, tree->_root, key, value);
	// NOTE: if the key was already in the tree the implementation returns NULL
	if (pnode)
	{
		tree->_root = pnode;
		tree->_node_count++;
		return true;
	}
	return false;
}
/*
* Returns the size of the tree (number of nodes)
*/
_JOS_INLINE_FUNC size_t binary_search_tree_size(binary_search_tree_t* tree) {
	_JOS_ASSERT(tree);
	return tree->_node_count;
}
_JOS_INLINE_FUNC bool _binary_search_tree_contains_impl(_bsearch_tree_node_t* node, uintptr_t key);
/*
* Returns true if the given key exists in the tree
*/
_JOS_INLINE_FUNC bool binary_search_tree_contains(binary_search_tree_t* tree, uintptr_t key) {
	return _binary_search_tree_contains_impl(tree->_root, key);
}

_JOS_INLINE_FUNC bool _binary_search_tree_find_impl(binary_search_tree_t* tree, _bsearch_tree_node_t* node, uintptr_t key, void** out_value);
/*
* Returns true if the given key exists and will optionally return the value pointer (if it exists)
*/
_JOS_INLINE_FUNC bool binary_search_tree_find(binary_search_tree_t* tree, uintptr_t key, void** out_value) {
	return _binary_search_tree_find_impl(tree, tree->_root, key, out_value);
}

typedef void (*binary_search_tree_node_visitor_t)(uintptr_t, void*);
_JOS_API_FUNC void _binary_search_tree_sorted_traverse_impl(binary_search_tree_t* tree, _bsearch_tree_node_t* node, binary_search_tree_node_visitor_t visitor);
/*
* Traverse the tree in sorted order (smallest->largest key) and call the visitor function on each node
*/
_JOS_INLINE_FUNC void binary_search_tree_sorted_traverse(binary_search_tree_t* tree, binary_search_tree_node_visitor_t visitor) {
	_JOS_ASSERT(tree);
	_JOS_ASSERT(visitor);
	if (tree->_root) {
		_binary_search_tree_sorted_traverse_impl(tree, tree->_root, visitor);
	}
}

#if defined(_JOS_IMPLEMENT_BINARY_SEARCH_TREE) && !defined(_JOS_BINARY_SEARCH_TREE_IMPLEMENTED_)
#define _JOS_BINARY_SEARCH_TREE_IMPLEMENTED_

/*
* internal node for the search tree
*/
typedef struct _bsearch_tree_node {

	struct _bsearch_tree_node* _left;
	struct _bsearch_tree_node* _right;
	uintptr_t	_key;
	// maximum height of left and right sub-tree + 1
	int			_height;
	// index of value entry for this node in the tree::_data array
	// NOTE: it is legal to create a search tree with no values stored in them, in which case this field is wasted (for now)
	size_t		_value_index;

} _bsearch_tree_node_t;

_JOS_API_FUNC void _binary_search_tree_sorted_traverse_impl(binary_search_tree_t * tree, _bsearch_tree_node_t* node, binary_search_tree_node_visitor_t visitor) {

	if (node->_left) {
		_binary_search_tree_sorted_traverse_impl(tree, node->_left, visitor);
	}
	//TODO: this is very redundant and can be avoided 
	if (tree->_node_size) {
		visitor(node->_key, vector_at(&tree->_data, node->_value_index));
	}
	else {
		visitor(node->_key, NULL);
	}
	if (node->_right) {
		_binary_search_tree_sorted_traverse_impl(tree, node->_right, visitor);
	}
}

_JOS_INLINE_FUNC int _binary_search_tree_height_delta(_bsearch_tree_node_t* node) {
	int lh = node->_left ? node->_left->_height : 0;
	int rh = node->_right ? node->_right->_height : 0;
	return lh - rh;
}

_JOS_INLINE_FUNC void _binary_search_tree_recalc_height(_bsearch_tree_node_t* node) {
	int lh = node->_left ? node->_left->_height : 0;
	int rh = node->_right ? node->_right->_height : 0;
	node->_height = max(lh, rh) + 1;
}

_JOS_INLINE_FUNC _bsearch_tree_node_t* _binary_search_tree_rotate_left(_bsearch_tree_node_t* node) {
	_bsearch_tree_node_t* pivot = node->_left;
	_JOS_ASSERT(pivot);
	_bsearch_tree_node_t* t = pivot->_right;
	pivot->_right = node;
	node->_left = t;
	_binary_search_tree_recalc_height(node);
	return pivot;
}

_JOS_INLINE_FUNC _bsearch_tree_node_t* _binary_search_tree_rotate_right(_bsearch_tree_node_t* node) {
	_bsearch_tree_node_t* pivot = node->_right;
	_JOS_ASSERT(pivot);
	_bsearch_tree_node_t* t = pivot->_left;
	pivot->_left = node;
	node->_right = t;
	_binary_search_tree_recalc_height(node);
	return pivot;
}

_JOS_API_FUNC _bsearch_tree_node_t* _binary_search_tree_insert_impl(binary_search_tree_t* tree, _bsearch_tree_node_t* parent, uintptr_t key, void* value) {
	if (!parent) {
		_bsearch_tree_node_t* node = tree->_allocator->alloc(tree->_allocator, sizeof(_bsearch_tree_node_t));
		node->_height = 1;
		node->_left = node->_right = NULL;
		node->_key = key;
		if (tree->_node_size) {
			vector_push_back(&tree->_data, value);
			node->_value_index = vector_size(&tree->_data) - 1;
		}
		return node;
	}

	if (key == parent->_key) {
		return NULL;
	}

	if (key > parent->_key) {
		_bsearch_tree_node_t* t_n = _binary_search_tree_insert_impl(tree, parent->_right, key, value);
		if (t_n) {
			parent->_right = t_n;
		}
		else {
			// early out if key already in the tree
			return NULL;
		}
	}
	else {
		_bsearch_tree_node_t* t_n = _binary_search_tree_insert_impl(tree, parent->_left, key, value);
		if (t_n) {
			parent->_left = t_n;
		}
		else {
			return NULL;
		}
	}
	_binary_search_tree_recalc_height(parent);
	int hd = _binary_search_tree_height_delta(parent);
	if (hd > 1) {
		// left imbalanced
		return _binary_search_tree_rotate_left(parent);
	}
	else if (hd < -1) {
		// right imbalanced
		return _binary_search_tree_rotate_right(parent);
	}
	// this subtree is already perfectly balanced
	return parent;
}

_JOS_INLINE_FUNC bool _binary_search_tree_contains_impl(_bsearch_tree_node_t* node, uintptr_t key) {
	if (node) {
		if (node->_key == key) {
			return true;
		}
		if (key < node->_key) {
			return _binary_search_tree_contains_impl(node->_left, key);
		}
		return _binary_search_tree_contains_impl(node->_right, key);
	}
	return false;
}

_JOS_INLINE_FUNC bool _binary_search_tree_find_impl(binary_search_tree_t* tree, _bsearch_tree_node_t* node, uintptr_t key, void** out_value) {
	if (node) {
		if (node->_key == key) {
			if (tree->_node_size && out_value) {
				*out_value = vector_at(&tree->_data, node->_value_index);
			}
			return true;
		}
		if (key < node->_key) {
			return _binary_search_tree_find_impl(tree, node->_left, key, out_value);
		}
		return _binary_search_tree_find_impl(tree, node->_right, key, out_value);
	}
	return false;
}

_JOS_API_FUNC void binary_search_tree_create(binary_search_tree_t* tree, size_t node_size, alloc_alignment_t node_alignment, generic_allocator_t* allocator) {
	_JOS_ASSERT(tree);
	_JOS_ASSERT(allocator);

	tree->_allocator = allocator;
	tree->_node_size = node_size;
	tree->_node_count = 0;
	tree->_root = NULL;
	//NOTE: it is perfectly ok to have a tree with keys only!
	if (node_size) {
		//TODO: need a better initial capacity calculation here...
		vector_create_aligned(&tree->_data, 32, node_size, node_alignment, allocator);
	}
}

_JOS_INLINE_FUNC void _binary_search_tree_node_destroy_impl(binary_search_tree_t* tree, _bsearch_tree_node_t* node) {
	// depth first delete
	if (node->_left) {
		_binary_search_tree_node_destroy_impl(tree, node->_left);
	}
	if (node->_right) {
		_binary_search_tree_node_destroy_impl(tree, node->_right);
	}
	tree->_allocator->free(tree->_allocator, node);
}

_JOS_API_FUNC void binary_search_tree_destroy(binary_search_tree_t* tree) {
	if (!tree) {
		return;
	}
	if (tree->_node_size) {
		vector_destroy(&tree->_data);
	}
	if (tree->_root) {
		_binary_search_tree_node_destroy_impl(tree, tree->_root);
	}
	memset(tree, 0, sizeof(binary_search_tree_t));
}

#endif