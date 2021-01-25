
#ifdef _JOS_KERNEL_BUILD
#include <jos.h>
#include <collections.h>
#include <extensions/slices.h>
#include <extensions/pdb_index.h>
#else
#define _CRT_SECURE_NO_WARNINGS 1
#include <jos.h>
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

        const unsigned child_count = vector_size(&node->_children);
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

static void pdb_index_add_from_node(pdb_index_node_t* node, char_array_slice_t body, const pdb_index_symbol_t* __restrict data) {

    if(vector_is_empty(&node->_children)) {
        vector_create(&node->_children, PDB_INDEX_CHILD_INIT_CAPACITY, sizeof(pdb_index_node_t));
    }    
    pdb_index_node_t * new_node = node;
    while (!slice_is_empty(&body)) {
        pdb_index_node_t child_node;
        child_node._children = kEmptyVector;
        child_node._symbol = kEmptySymbol;
        child_node._prefix = pdb_index_next_token(&body);
        if( !slice_is_empty(&body) ) {
            vector_create(&child_node._children, PDB_INDEX_CHILD_INIT_CAPACITY, sizeof(pdb_index_node_t));
        }
        vector_push_back(&new_node->_children, &child_node);
        new_node = (pdb_index_node_t*)vector_at(&new_node->_children, vector_size(&new_node->_children)-1);
    }
    // store symbol information in the leaf
    new_node->_symbol = *data;
}

pdb_index_match_result pdb_index_insert(pdb_index_node_t* node, char_array_slice_t prefix, char_array_slice_t body, 
        const pdb_index_symbol_t* __restrict data, pdb_index_node_t** leaf) {

    bool is_root_node = false;

    if (slice_is_empty(&node->_prefix)) {

        if(vector_is_empty(&node->_children)) {
            // root node: initialise and build child tree
            //NOTE: keep node->_prefix empty!
            node->_symbol = kEmptySymbol;
            char_array_slice_t combined;
            //ZZZ: assert on these actually being contigous and prefix coming before body etc.
            combined._length = prefix._length + body._length;
            combined._ptr = prefix._ptr;
            pdb_index_add_from_node(node, combined, data);
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
            pdb_index_add_from_node(node, body, data);
            return kPdbIndex_Inserted;
        }

        // node has children; find a matching child tree or insert 

        const unsigned child_count = vector_size(&node->_children);
        char_array_slice_t next_prefix = is_root_node ? prefix : pdb_index_next_token(&body);
        if (slice_is_empty(&next_prefix)) {
            // early out; no more prefixes
            node->_symbol = *data;
            *leaf = node;
            return kPdbIndex_FullMatch;
        }

        // recursive check against each child
        for (unsigned c = 0; c < child_count; ++c) {
            pdb_index_node_t* child = (pdb_index_node_t*)vector_at(&node->_children, c);            
            const pdb_index_match_result m = pdb_index_insert(child, next_prefix, body, data, leaf);
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
        pdb_index_add_from_node(&new_child, body, data);
        vector_push_back(&node->_children, (void*)(&new_child));

        return kPdbIndex_Inserted;
    }

    // if we've recursed this will signal that the prefix needs to be added to the parent node's children
    return kPdbIndex_NoMatch;
}


static pdb_index_node_t     _pdb_index_root;
static size_t               _pdb_index_num_symbols = 0;
static const char* yml_file_path_name = "..\\build\\BOOTX64.PDB.YML";

// stores packed symbol names as 0 terminated strings, 00 terminates the whole thing.
// we allocate pages of 4K as we fill them up (which means there may be a few wasted bytes at the end of each page)
static vector_t     _string_store_pages;
static char*        _string_store = 0;
static size_t       _string_store_wp = 0;
static size_t       _string_store_total_size = 0;
static const size_t kStringStore_PageSize = 0x800;

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

const pdb_index_node_t* load_index_from_pdb_yml(void) {
    
    pdb_index_node_initialise(&_pdb_index_root);

    FILE * yml_file = fopen(yml_file_path_name, "r");
    if (yml_file) {

        // initialise string store
        _string_store_wp = 0;
        vector_create(&_string_store_pages, 16, sizeof(char*));
        vector_push_back_ptr(&_string_store_pages, _string_store = (char*)malloc(kStringStore_PageSize));

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
                                _string_store_wp = 0;
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
                symbol._rva = number;                
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
                symbol._section = number;
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
                    if (pdb_index_insert(&_pdb_index_root, prefix, body, &symbol, &leaf) == kPdbIndex_Inserted) {
                        ++_pdb_index_num_symbols;
                    }
                    //TODO: error checking
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

    return &_pdb_index_root;
}