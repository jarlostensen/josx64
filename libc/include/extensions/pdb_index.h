#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <jos.h>

typedef struct _pdb_index_symbol {
    // rva of symbol relative to section
    uint32_t                _rva;
    uint8_t                 _section;    
} pdb_index_symbol_t;

extern const pdb_index_symbol_t kEmptySymbol;

static _JOS_ALWAYS_INLINE bool symbol_is_empty(const pdb_index_symbol_t* symbol) {
    return symbol->_rva == kEmptySymbol._rva && symbol->_section == kEmptySymbol._section;
}

// the PDB index is a trie with a variable number of children per node
typedef struct _pdb_index_node {

    // the key for this level in the trie
    char_array_slice_t       _prefix;
    // vector of pdb_index_node
    vector_t                _children;
    // symbol information
    pdb_index_symbol_t      _symbol;

} pdb_index_node_t;

static _JOS_ALWAYS_INLINE void pdb_index_node_initialise(pdb_index_node_t* node) {
    node->_symbol = kEmptySymbol;
    node->_prefix = kEmptySlice;
    node->_children = kEmptyVector;
}

typedef enum _pdb_index_match_result {
    kPdbIndex_NoMatch,
    kPdbIndex_PartialMatch,
    kPdbIndex_FullMatch,
    kPdbIndex_Inserted,
} pdb_index_match_result;

typedef struct _pdb_index_insert_args {
    
    pdb_index_node_t* node;
    char_array_slice_t prefix;
    char_array_slice_t body;
    const pdb_index_symbol_t* data;
    pdb_index_node_t** leaf;

} pdb_index_insert_args_t;

pdb_index_match_result pdb_index_insert(pdb_index_node_t* node, char_array_slice_t prefix, char_array_slice_t body,
	const pdb_index_symbol_t* __restrict data, pdb_index_node_t** leaf, generic_allocator_t* allocator);

// _this | _isA | Mixed | _String | Identifier
// another | Mixed | Case | Identifier
// Camels | Are | Ok | Too
char_array_slice_t pdb_index_next_token(char_array_slice_t* body);
const pdb_index_node_t* pdb_index_load_from_pdb_yml(const char* pdb_yml_file_pathname, generic_allocator_t* allocator);
// find the closest function to the given RVA (i.e. the offset of the function relative to the .text segment)
char_array_slice_t pdb_index_symbol_name_for_address(uint32_t rva);