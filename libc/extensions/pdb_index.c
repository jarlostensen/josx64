
#ifdef _JOS_KERNEL_BUILD
#include <jos.h>
#include <collections.h>
#include <extensions/slices.h>
#include <extensions/pdb_index.h>
#else
#pragma warning(disable:4005)
#define _CRT_SECURE_NO_WARNINGS 1
#include <jos.h>
#include <ctype.h>
#include "..\..\kernel\include\collections.h"
#include "..\include\extensions\slices.h"
#include "..\include\extensions\pdb_index.h"
#endif
#include <stdio.h>

const pdb_index_symbol_t kEmptySymbol = { ._rva = 0, ._section = 0};

// _this | _isA | Mixed | _String | Identifier
// another | Mixed | Case | Identifier
// Camels | Are | Ok | Too
char_array_slice_t pdb_index_next_token(char_array_slice_t* body) {
    
    char_array_slice_t slice = {._ptr=0,._length=0};
    if (slice_is_empty(body)) {
        return slice;
    }

    const char* rp = body->_ptr;

    unsigned n = 0u;
    while(rp[n] == '_')
        // skip leading _'s
        ++n;
    
    // "edge detection" for lowercase to Uppercase and vice versa
    int starts_lower = !!islower(rp[n]);
    int starts_upper_to_lower_edge = isupper(rp[n]) && islower(rp[n+1]);
    
    for (; n < body->_length; ++n ) {
        int lower_to_upper_edge = islower(rp[n]) && isupper(rp[n+1]);
        if (rp[n] == ' ' || rp[n] == '_' 
            || 
            starts_upper_to_lower_edge && lower_to_upper_edge 
            ||
            starts_lower && lower_to_upper_edge
            ) {
            slice._ptr = rp;

            if (lower_to_upper_edge) {
                slice._length = n+1u;
                body->_ptr = rp+n+1u;
                body->_length -= (n+1u);
            }
            else {
                slice._length = n;
                body->_ptr = rp+n;
                body->_length -= n;
            }
            break;
        }
    }

    if (!slice._length) {
        //"move"..
        slice = *body;
        body->_length = 0;
        body->_ptr = 0;
    }
    return slice;
}

pdb_index_match_result pdb_index_match_search(pdb_index_node_t* node, char_array_slice_t prefix, char_array_slice_t body, pdb_index_node_t** leaf) {

    if (char_array_slice_equals(node->_prefix, prefix)) {

        if (vector_is_empty(&node->_children)) {
            *leaf = node;
            return kPdbIndex_FullMatch;
        }

        const size_t child_count = vector_size(&node->_children);
        char_array_slice_t next_prefix = pdb_index_next_token(&body);
        if (slice_is_empty(&next_prefix)) {
            *leaf = node;
            return kPdbIndex_FullMatch;
        }

        for (unsigned c = 0; c < child_count; ++c) {
            pdb_index_node_t* child = (pdb_index_node_t*)vector_at(&node->_children, c);
            
            const pdb_index_match_result m = pdb_index_match_search(child, next_prefix, body, leaf);
            if (m != kPdbIndex_NoMatch) {
                return m;
            }
        }
    }

    return kPdbIndex_NoMatch;
}

#define PDB_INDEX_CHILD_INIT_CAPACITY 16

static void pdb_index_add_from_node(pdb_index_node_t* node, char_array_slice_t body, const pdb_index_symbol_t* __restrict data, generic_allocator_t* allocator) {

    if(vector_is_empty(&node->_children)) {
        vector_create(&node->_children, PDB_INDEX_CHILD_INIT_CAPACITY, sizeof(pdb_index_node_t), allocator);
    }    
    pdb_index_node_t * new_node = node;
    while (!slice_is_empty(&body)) {
        pdb_index_node_t child_node;
        child_node._children = kEmptyVector;
        child_node._symbol = kEmptySymbol;
        child_node._prefix = pdb_index_next_token(&body);
        if( !slice_is_empty(&body) ) {
            vector_create(&child_node._children, PDB_INDEX_CHILD_INIT_CAPACITY, sizeof(pdb_index_node_t), allocator);
        }
        vector_push_back(&new_node->_children, &child_node);
        new_node = (pdb_index_node_t*)vector_at(&new_node->_children, vector_size(&new_node->_children)-1);
    }
    // store symbol information in the leaf
    new_node->_symbol = *data;
}

pdb_index_match_result pdb_index_insert(pdb_index_node_t* node, char_array_slice_t prefix, char_array_slice_t body, 
        const pdb_index_symbol_t* __restrict data, pdb_index_node_t** leaf, generic_allocator_t* allocator) {

    bool is_root_node = false;

    if (slice_is_empty(&node->_prefix)) {

        if(vector_is_empty(&node->_children)) {
            // root node: initialise and build child tree
            //NOTE: keep node->_prefix empty!
            node->_symbol = kEmptySymbol;
            char_array_slice_t combined;
            //ZZZ: assert on these actually being contiguous and prefix coming before body etc.
            combined._length = prefix._length + body._length;
            combined._ptr = prefix._ptr;
            pdb_index_add_from_node(node, combined, data, allocator);
            return kPdbIndex_Inserted;
        }
        // else we bypass the prefix check below and skip straight to matching/inserting as a child
        is_root_node = true;
    }

    if (is_root_node || char_array_slice_equals(node->_prefix, prefix)) {

        // this prefix matches the current node

        if (vector_is_empty(&node->_children)) {
            
            if ( slice_is_empty(&body) ) {
                // no more prefixes to insert; we're done
                *leaf = node;
               return kPdbIndex_FullMatch;
            }

            // use remaining prefixes to build build child tree
            pdb_index_add_from_node(node, body, data, allocator);
            return kPdbIndex_Inserted;
        }

        // node has children; find a matching child tree or insert 

        const size_t child_count = vector_size(&node->_children);
        char_array_slice_t next_prefix = is_root_node ? prefix : pdb_index_next_token(&body);
        if (slice_is_empty(&next_prefix)) {
            // early out; no more prefixes
            node->_symbol = *data;
            *leaf = node;
            return kPdbIndex_FullMatch;
        }

        // recursive check against each child
        for (size_t c = 0; c < child_count; ++c) {
            pdb_index_node_t* child = (pdb_index_node_t*)vector_at(&node->_children, c);            
            const pdb_index_match_result m = pdb_index_insert(child, next_prefix, body, data, leaf, allocator);
            if (m != kPdbIndex_NoMatch) {
                // the rest of the prefixes have been inserted or were already in the index
                return m;
            }
        }

        // if we get here there is no match in any of the children so we have to add the remaining prefixes 
        // as a new child tree
        pdb_index_node_t new_child;
        new_child._prefix = next_prefix;
        new_child._children = kEmptyVector;
        new_child._symbol = kEmptySymbol;
        pdb_index_add_from_node(&new_child, body, data, allocator);
        vector_push_back(&node->_children, (void*)(&new_child));

        return kPdbIndex_Inserted;
    }

    // if we've recursed this will signal that the prefix needs to be added to the parent node's children
    return kPdbIndex_NoMatch;
}


static pdb_index_node_t     _pdb_index_root;
static size_t               _pdb_index_num_symbols = 0;

// stores packed symbol names as 0 terminated strings, 00 terminates the whole thing.
// we allocate pages of 4K as we fill them up (which means there may be a few wasted bytes at the end of each page)
static vector_t     _string_store_pages;
static char*        _string_store = 0;
static size_t       _string_store_wp = 0;
static size_t       _string_store_total_size = 0;
static const size_t kStringStore_PageSize = 0x800;

// packed array of RVAs used for symbol lookups by memory address range
// this array is paralleled by _pdb_symbol_name_ptr_array 
static uint32_t             * _pdb_symbol_rva_array = 0;
// array of pointers to names kept in string store pages
static const char*          * _pdb_symbol_name_ptr_array = 0;

typedef struct _build_rva_index_state {

    size_t      _index;

} build_rva_index_state_t;

static void build_rva_index(pdb_index_node_t* node, build_rva_index_state_t* state) {

    if ( !symbol_is_empty(&node->_symbol) ) {
        
        // find the symbol name for this node by scanning backwards to a 0, see INIT_STRING_STORE
        const char* rp = node->_prefix._ptr;
        while(*rp--!=0) ;
        rp+=2;
        
        size_t i = state->_index++;
        // now insertion sort both arrays keyed on the rva value
        _pdb_symbol_rva_array[i] = node->_symbol._rva;
        _pdb_symbol_name_ptr_array[i] = rp;
        while (i && _pdb_symbol_rva_array[i] < _pdb_symbol_rva_array[i - 1]) {

#define SWAP(a,b)\
        (a) ^= (b);\
        (b) ^= (a);\
        (a) ^= (b)
#define SWAP_PTR(a,b)\
        (uintptr_t)(a) ^= (uintptr_t)(b);\
        (uintptr_t)(b) ^= (uintptr_t)(a);\
        (uintptr_t)(a) ^= (uintptr_t)(b)

            SWAP(_pdb_symbol_rva_array[i], _pdb_symbol_rva_array[i-1]);
            SWAP_PTR(_pdb_symbol_name_ptr_array[i], _pdb_symbol_name_ptr_array[i-1]);
            --i;
        }
    }
    
    if (!vector_is_empty(&node->_children)) {
        const size_t child_count = vector_size(&node->_children);
        for (size_t c = 0; c < child_count; ++c) {
            pdb_index_node_t* child = (pdb_index_node_t*)vector_at(&node->_children, c);
            build_rva_index(child, state);
        }
    }    
}

char_array_slice_t pdb_index_symbol_name_for_address(uint32_t rva) {
    if(!_pdb_index_num_symbols)
        return kEmptySlice;

    // the rva index is sorted so we can do a binary search
    size_t lo = 0;
    size_t hi = _pdb_index_num_symbols-1;
    size_t mid = (hi+lo)>>1;
    while (lo < hi) {        
        if (_pdb_symbol_rva_array[mid] >= rva) {            
            if (_pdb_symbol_rva_array[mid] == rva) {
                // exact match to the address of a symbol
                break;
            }
            hi = mid-1;
        }
        else {
            if (_pdb_symbol_rva_array[mid + 1] > rva) {
                // we're between two symbols, pick the lower one
                break;
            }
            lo = mid+1;
        }
        mid = (hi+lo)>>1;
    }
    // at this point we're in a range, or we've found an exact match, that's the best we can do
    char_array_slice_t slice;
    slice._ptr = _pdb_symbol_name_ptr_array[mid];
    slice._length = strlen(slice._ptr);
    return slice;
}

typedef enum _parse_state {

    kSkipWhitespace,
    kFindFunction,
    kFindOffset,
    kStoreOffset,
    kFindSegment,
    kStoreSegment,
    kFindName,
    kStoreName,

    kFindTag,
    kReadNumber,
    kReadName

} parse_state_t;

const pdb_index_node_t* pdb_index_load_from_pdb_yml(const char* pdb_yml_file_pathname, generic_allocator_t* allocator) {
    
    pdb_index_node_initialise(&_pdb_index_root);

    FILE * yml_file = fopen(pdb_yml_file_pathname, "r");
    if (yml_file) {

        // initialise string store.
        vector_create(&_string_store_pages, 16, sizeof(char*), allocator);
        vector_push_back_ptr(&_string_store_pages, _string_store = (char*)malloc(kStringStore_PageSize));
        // NOTE: we use 0 terminators to scan backwards to find the start of a symbol name (in other functions) 
        // so we have to lead with a 0 as a front terminator as well                
#define INIT_STRING_STORE()\
            _string_store[0] = 0;\
            _string_store_wp = 1

        INIT_STRING_STORE();

        // we don't load the whole file into memory, instead we process it in chunks
        char buffer[4096];
        const size_t buffer_size = sizeof(buffer);

        pdb_index_symbol_t symbol;

        // we parse this sort of thing:
        // Flags:           [ Function ]
        // Offset:          139264
        // Segment:         1
        // Name:            ZydisRegisterGetWidth
        static const char* kFunction_Tag = "[ Function ]";
        static const char* kOffset_Tag = "Offset:";
        static const char* kSegment_Tag = "Segment:";
        static const char* kName_Tag = "Name:";

        // small stack of operations for the parser SM below
        parse_state_t state_stack[6];
        int state_stack_ptr = sizeof(state_stack)/sizeof(state_stack[0]) - 1;

#define PUSH_STATE(state)\
    if(!state_stack_ptr) __debugbreak();\
    state_stack[state_stack_ptr--] = state

#define POP_STATE()\
    state_stack[++state_stack_ptr]

#define STATE_STACK_IS_EMPTY()\
    (state_stack_ptr==(sizeof(state_stack)/sizeof(state_stack[0]) - 1))

        PUSH_STATE(kFindFunction);
        PUSH_STATE(kSkipWhitespace);

        size_t tag_match_index = 0;
        const char* current_tag = kFunction_Tag;

        char number_buffer[10];
        size_t number_wp = 0;
        unsigned long long number = 0;

        char name_buffer[128];
        size_t name_wp = 0;
        size_t name_store_ptr = 0;

        size_t rp = 0;
        size_t read = fread(buffer, 1, buffer_size, yml_file);
        while (read) {
            parse_state_t state = POP_STATE();

            switch (state) {
            case kSkipWhitespace:
            {
                for (; rp < read; ++rp) {
                    if (isalpha((int)buffer[rp])) {
                        break;
                    }
                }
            }
            break;
            case kFindTag:
            {                
                for (; rp < read; ++rp) {
                    if (buffer[rp] != current_tag[tag_match_index]) {
                        tag_match_index = 0;
                    }
                    else {
                        ++tag_match_index;
                        if (current_tag[tag_match_index] == 0) {
                            // complete match                            
                            break;
                        }
                    }
                }
                if (current_tag[tag_match_index] != 0) {
                    // keep searching
                    PUSH_STATE(kFindTag);
                }
            }
            break;
            case kReadNumber:
            {
                for (; rp < read; ++rp) {
                    if (buffer[rp] >= '0' && buffer[rp] <= '9') {
                        number_buffer[number_wp++] = buffer[rp];
                    }
                    else {
                        if (number_wp) {
                            number_buffer[number_wp] = 0;
                            number_wp = 0;
                            number = strtoll(number_buffer, 0, 10);
                            //TODO: ERROR CHECK
                            break;
                        }
                        // else keep looking                            
                    }
                }
                if (number_wp) {
                    // we're at the end of the buffer but there may be more number down stream
                    PUSH_STATE(kReadNumber);
                }
            }
            break;
            case kReadName:
            {
                for (; rp < read; ++rp) {
                    if (isalpha(buffer[rp]) || buffer[rp]=='_' ) {
                        name_buffer[name_wp++] = buffer[rp];
                    }
                    else {
                        if (name_wp) {
                            name_buffer[name_wp] = 0;
                            const size_t space_left = (kStringStore_PageSize - _string_store_wp);
                            if ( space_left < name_wp) {
                                // allocate a new page
                                vector_push_back_ptr(&_string_store_pages, _string_store = (char*)malloc(kStringStore_PageSize));
                                INIT_STRING_STORE();
                            }
                            memcpy(_string_store+_string_store_wp, name_buffer, name_wp+1);
                            name_store_ptr = _string_store_wp;
                            _string_store_total_size += name_wp+1;
                            _string_store_wp += name_wp+1;
                            name_wp = 0;
                            break;
                        }
                    }
                }
                if (name_wp) {
                    PUSH_STATE(kReadName);
                }
            }
            break;
            case kFindFunction:
                PUSH_STATE(kFindOffset);                
                current_tag = kFunction_Tag;
                tag_match_index = 0;
                PUSH_STATE(kFindTag);
                break;
            case kFindOffset:
                PUSH_STATE(kFindSegment);
                current_tag = kOffset_Tag;
                tag_match_index = 0;
                PUSH_STATE(kStoreOffset);
                PUSH_STATE(kReadNumber);
                PUSH_STATE(kFindTag);
                break;
            case kStoreOffset:
                //NOTE: assumes number is valid
                symbol._rva = (uint32_t)number;
                break;            
            case kFindSegment:
            {
                PUSH_STATE(kFindName);
                current_tag = kSegment_Tag;
                tag_match_index = 0;
                PUSH_STATE(kStoreSegment);
                PUSH_STATE(kReadNumber);
                PUSH_STATE(kFindTag);
            }
            break;
            case kStoreSegment:
                symbol._section = (uint8_t)number;
                break;
            case kFindName:
            {
                PUSH_STATE(kStoreName);
                current_tag = kName_Tag;
                tag_match_index = 0;
                PUSH_STATE(kReadName);
                PUSH_STATE(kSkipWhitespace);
                PUSH_STATE(kFindTag);
            }
            break;
            case kStoreName:
            {                
                char_array_slice_t body;
                char_array_slice_create(&body, _string_store + name_store_ptr, 0,0);
                char_array_slice_t prefix = pdb_index_next_token(&body);
                if(!slice_is_empty(&prefix)) {
                    pdb_index_node_t * leaf;
                    if (pdb_index_insert(&_pdb_index_root, prefix, body, &symbol, &leaf, allocator) != kPdbIndex_NoMatch) {
                        ++_pdb_index_num_symbols;
                    }
                    //TODO: error checking
                    else {
                        __debugbreak();
                    }
                }

                if(!STATE_STACK_IS_EMPTY())
                    __debugbreak();

                PUSH_STATE(kFindFunction);
                PUSH_STATE(kSkipWhitespace);
            }
            break;
            default:;
            }

            if(rp == read) {
                read = fread(buffer, 1, buffer_size, yml_file);
                rp = 0;
            }
        }        
    }

    if (_pdb_index_num_symbols) {
        // allocate linear arrays to hold the rva and symbol name index. 
        _pdb_symbol_rva_array = (uint32_t*)malloc(_pdb_index_num_symbols*sizeof(uint32_t));
        _pdb_symbol_name_ptr_array = (const char**)malloc(_pdb_index_num_symbols*sizeof(const char*));
        build_rva_index(&_pdb_index_root, &(build_rva_index_state_t){ ._index = 0 });
    }

    return &_pdb_index_root;
}
