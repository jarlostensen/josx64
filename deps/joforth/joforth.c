
#include "joforth.h"
#include "joforth_ir.h"
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include <stdio.h>

// based on https://en.wikipedia.org/wiki/Pearson_hashing#C,_64-bit
// initialised at start up
static unsigned char T[256];
joforth_word_key_t pearson_hash(const char* _x) {
    size_t i;
    size_t j;
    unsigned char h;
    joforth_word_key_t retval = 0;
    const unsigned char* x = (const unsigned char*)_x;

    for (j = 0; j < sizeof(retval); ++j) {
        h = T[(x[0] + j) % 256];
        i = 1;
        while (x[i]) {
            h = T[h ^ x[i++]];
        }
        retval = ((retval << 8) | h);
    }
    return retval;
}

// ================================================================

static uint8_t* _alloc(joforth_t* joforth, size_t bytes) {
    assert(joforth->_mp < (joforth->_memory_size - bytes));
    size_t mp = joforth->_mp;
    joforth->_mp += bytes;
    return joforth->_memory + mp;
}

#define JOFORTH_DICT_BUCKETS    257
static _joforth_dict_entry_t* _add_entry(joforth_t* joforth, const char* word) {
    joforth_word_key_t key = pearson_hash(word);
    //TODO: check it's not already there
    size_t index = key % JOFORTH_DICT_BUCKETS;
    _joforth_dict_entry_t* i = joforth->_dict + index;
    while (i->_next != 0) {
        i = i->_next;
    }

    i->_next = (_joforth_dict_entry_t*)_alloc(joforth, sizeof(_joforth_dict_entry_t));
    memset(i->_next, 0, sizeof(_joforth_dict_entry_t));
    i->_key = key;
    const size_t len = strlen(word)+1;
    char* word_copy = (char*)_alloc(joforth, len);
    memcpy(word_copy, word, len);
    i->_word = word_copy;
    i->_doc = 0;

    return i;
}

static _joforth_dict_entry_t* _find_word(joforth_t* joforth, joforth_word_key_t key) {
    size_t index = key % JOFORTH_DICT_BUCKETS;
    _joforth_dict_entry_t* i = joforth->_dict + index;
    while (i != 0) {
        if (i->_key == key) {
            return i;
        }
        i = i->_next;
    }
    return 0;
}

// ============================================================================
// native build in words

static void _lt(joforth_t* joforth) {
    //NOTE: NOS < TOS
    joforth_push_value(joforth, joforth_pop_value(joforth) > joforth_pop_value(joforth) ? JOFORTH_TRUE : JOFORTH_FALSE);
}

static void _gt(joforth_t* joforth) {
    //NOTE: NOS > TOS
    joforth_push_value(joforth, joforth_pop_value(joforth) < joforth_pop_value(joforth) ? JOFORTH_TRUE : JOFORTH_FALSE);
}

static void _eq(joforth_t* joforth) {
    //NOTE: NOS == TOS
    joforth_push_value(joforth, joforth_pop_value(joforth) == joforth_pop_value(joforth) ? JOFORTH_TRUE : JOFORTH_FALSE);
}

static void _dup(joforth_t* joforth) {
    joforth_push_value(joforth, joforth_top_value(joforth));
}

static void _mul(joforth_t* joforth) {
    joforth_push_value(joforth, joforth_pop_value(joforth) * joforth_pop_value(joforth));
}

static void _plus(joforth_t* joforth) {
    joforth_push_value(joforth, joforth_pop_value(joforth) + joforth_pop_value(joforth));
}

static void _minus(joforth_t* joforth) {
    joforth_value_t val1 = joforth_pop_value(joforth);
    joforth_value_t val2 = joforth_pop_value(joforth);
    //NOTE: explicit reverse polish
    joforth_push_value(joforth, val2 - val1);
}

static void _mod(joforth_t* joforth) {
    joforth_value_t val1 = joforth_pop_value(joforth);
    joforth_value_t val2 = joforth_pop_value(joforth);
    //NOTE: explicit reverse polish
    joforth_push_value(joforth, val2 % val1);
}

static void _swap(joforth_t* joforth) {
    if (joforth->_sp < joforth->_stack_size - 1) {
        joforth_value_t tos = joforth->_stack[joforth->_sp + 1];
        joforth_value_t nos = joforth->_stack[joforth->_sp + 2];
        joforth->_stack[joforth->_sp + 1] = nos;
        joforth->_stack[joforth->_sp + 2] = tos;
    }
    else {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
    }
}

static void _tuck(joforth_t* joforth) {
    if (joforth->_sp < joforth->_stack_size - 2) {
        joforth_value_t tos = joforth_pop_value(joforth);
        joforth_value_t nos = joforth_pop_value(joforth);
        joforth_push_value(joforth, tos);
        joforth_push_value(joforth, nos);
        joforth_push_value(joforth, tos);
    }
    else {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
    }
}

static void _drop(joforth_t* joforth) {
    /* _ = */ joforth_pop_value(joforth);
}

static void _dot(joforth_t* joforth) {
    printf("%lld", joforth_pop_value(joforth));
}

static void _bang(joforth_t* joforth) {
    // store value at (relative) address
    joforth_value_t address = joforth_pop_value(joforth);
    joforth_value_t* ptr = (joforth_value_t*)(joforth->_memory + address);
    ptr[0] = joforth_pop_value(joforth);
}

static void _at(joforth_t* joforth) {
    // retrieve value at (relative) address
    joforth_value_t address = joforth_pop_value(joforth);
    joforth_value_t* ptr = (joforth_value_t*)(joforth->_memory + address);
    joforth_push_value(joforth, ptr[0]);
}

// simply drop the entire stack
static void _popa(joforth_t* joforth) {
    joforth->_sp = joforth->_stack_size - 1;
}

static void _dec(joforth_t* joforth) {
    joforth->_base = 10;
}

static void _hex(joforth_t* joforth) {
    joforth->_base = 16;
}

static void _create(joforth_t* joforth) {
    // the stack MUST contain the address of the name of a new word we'll create
    char* ptr = (char*)joforth_pop_value(joforth);
    // here goes nothing...
    _joforth_dict_entry_t* entry = _add_entry(joforth, ptr);
    if (entry) {
        entry->_type = kEntryType_Value;
        // next available memory slot, at the time we're creating
        entry->_rep._value = (joforth_value_t)joforth->_mp;
    }
    else {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
    }
}

static void _here(joforth_t* joforth) {
    joforth_push_value(joforth, joforth->_mp);
}

//NOTE: as per standard Forth a CELL is equal to the native value type
static void _cells(joforth_t* joforth) {
    // convert COUNT CELLS to a byte count and push it
    joforth_value_t count = joforth_pop_value(joforth);
    joforth_push_value(joforth, count * sizeof(joforth_value_t));
}

static void _allot(joforth_t* joforth) {
    // expects byte count on stack
    joforth_value_t bytes = joforth_pop_value(joforth);
    if (bytes) {
        //NOTE: we don't save the result, it's expected that the caller 
        //      uses variables or HERE for that
        _alloc(joforth, bytes);
    }
    else {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
    }
}

static void _see(joforth_t* joforth) {
    // the stack MUST contain the address of a word name
    const char* id = (const char*)joforth_pop_value(joforth);
    joforth_word_key_t key = pearson_hash(id);
    _joforth_dict_entry_t* entry = _find_word(joforth, key);
    if (entry) {        
        switch (entry->_type)
        {
        case kEntryType_Native:
        case kEntryType_Prefix:
            printf(" %s", entry->_word);            
            break;
        case kEntryType_Value:
            printf(" value %lld", entry->_rep._value);
            break;
        case kEntryType_Word:
        {
            uint8_t* ir = entry->_rep._ir;
            while (*ir != kIr_Null) {
                switch (*ir++) {
                case kIr_Begin:
                    printf(" begin");
                    break;
                case kIr_Do:
                    printf(" do");
                    break;
                case kIr_Dot:
                case kIr_DotDot:
                    printf(" .");
                    break;
                case kIr_Else:
                    printf(" else");
                    break;
                case kIr_DefineWord:
                    printf(": %s", entry->_word);
                    if(entry->_doc) {
                        printf(" (%s)", entry->_doc);
                    }
                    ir += sizeof(void*);
                    break;
                case kIr_EndDefineWord:
                    printf(" ;");
                    break;
                case kIr_Endif:
                    printf(" endif");
                    break;
                case kIr_False:
                    printf(" false");
                    break;
                case kIr_If:
                    printf(" if");
                    break;
                case kIr_IfZeroOperator:
                    printf(" ?");
                    break;
                case kIr_Invert:
                    printf(" invert");
                    break;
                case kIr_Loop:
                    printf(" loop");
                    break;
                case kIr_Native:
                {
                    _joforth_dict_entry_t* dict_entry = ((_joforth_dict_entry_t**)ir)[0];
                    ir += sizeof(void*);
                    printf(" %s", dict_entry->_word);                    
                }
                break;
                case kIr_Recurse:
                    printf(" recurse");
                    break;
                case kIr_Repeat:
                    printf(" repeat");
                    break;
                case kIr_True:
                    printf(" true");
                    break;
                case kIr_Until:
                    printf(" until");
                    break;
                case kIr_While:
                    printf(" while");
                    break;
                case kIr_Value:
                {
                    joforth_value_t value = *((joforth_value_t*)ir);
                    ir += sizeof(joforth_value_t);
                    printf(" %lld", value);
                }
                break;
                case kIr_ValuePtr:
                case kIr_WordPtr:
                    ir += sizeof(void*);
                    break;
                default:;
                }
            }
        }
        break;
        default:;
        }
        printf("\n: %s, takes %zu parameters\n", entry->_word, entry->_depth);
    }
    else {
        printf("\"%s\" is not in the dictionary\n", id);
    }
}

static void _cr(joforth_t* joforth) {
    (void)joforth;
    printf("\n");
}

// ================================================================


void    joforth_initialise(joforth_t* joforth) {

    static bool _t_initialised = false;
    if (!_t_initialised) {
        unsigned char source[256];
        for (size_t i = 0; i < 256; ++i) {
            source[i] = i;
        }
        // fill T with a random permutation of 0..255
        for (size_t i = 0; i < 256; ++i) {
            size_t index = rand() % (256 - i);
            T[i] = source[index];
            source[index] = source[255 - i];
        }

        _t_initialised = true;
    }

    // we allocate one block of memory which is used to carve out all subsequent allocations
    joforth->_memory_size = joforth->_memory_size > JOFORTH_DEFAULT_MEMORY_SIZE ? joforth->_memory_size : JOFORTH_DEFAULT_MEMORY_SIZE;
    joforth->_memory = (uint8_t*)joforth->_allocator._alloc(joforth->_memory_size);
    joforth->_mp = 0;

    // value stack
    joforth->_stack_size = joforth->_stack_size > JOFORTH_DEFAULT_STACK_SIZE ? joforth->_stack_size : JOFORTH_DEFAULT_STACK_SIZE;
    joforth->_stack = (joforth_value_t*)_alloc(joforth, joforth->_stack_size * sizeof(joforth_value_t));
    joforth->_sp = joforth->_stack_size - 1;

    // IR buffer    
#define JOFORTH_DEFAULT_IRBUFFER_SIZE    1024
    joforth->_ir_buffer = (uint8_t*)_alloc(joforth, JOFORTH_DEFAULT_IRBUFFER_SIZE);
    joforth->_ir_buffer_size = JOFORTH_DEFAULT_IRBUFFER_SIZE;
    joforth->_irw = 0;

    // ir return stack
    //NOTE: this determines the nesting level
#define JOFORTH_DEFAULT_IRSTACK_SIZE    256
    joforth->_irstack = (uint8_t**)_alloc(joforth, JOFORTH_DEFAULT_IRSTACK_SIZE * sizeof(void*));
    joforth->_irstack_size = JOFORTH_DEFAULT_IRSTACK_SIZE;
    joforth->_irp = joforth->_irstack_size - 1;

    joforth->_dict = (_joforth_dict_entry_t*)_alloc(joforth, JOFORTH_DICT_BUCKETS * sizeof(_joforth_dict_entry_t));
    memset(joforth->_dict, 0, JOFORTH_DICT_BUCKETS * sizeof(_joforth_dict_entry_t));

    // start with decimal
    joforth->_base = 10;

    // all clear
    joforth->_status = _JO_STATUS_SUCCESS;

    // add built-in handlers
    joforth_add_word(joforth, "<", _lt, 2);
    joforth_add_word(joforth, ">", _gt, 2);
    joforth_add_word(joforth, "=", _eq, 2);
    joforth_add_word(joforth, "dup", _dup, 1);
    joforth_add_word(joforth, "*", _mul, 2);
    joforth_add_word(joforth, "+", _plus, 2);
    joforth_add_word(joforth, "-", _minus, 2);
    joforth_add_word(joforth, ".", _dot, 1);
    joforth_add_word(joforth, "swap", _swap, 2);
    joforth_add_word(joforth, "tuck", _tuck, 2);
    joforth_add_word(joforth, "drop", _drop, 1);
    joforth_add_word(joforth, "!", _bang, 2);
    joforth_add_word(joforth, "@", _at, 1);
    joforth_add_word(joforth, "dec", _dec, 0);
    joforth_add_word(joforth, "hex", _hex, 0);
    joforth_add_word(joforth, "popa", _popa, 0);
    joforth_add_word(joforth, "here", _here, 0);
    joforth_add_word(joforth, "allot", _allot, 1);
    joforth_add_word(joforth, "cells", _cells, 1);
    joforth_add_word(joforth, "cr", _cr, 0);
    joforth_add_word(joforth, "mod", _mod, 2);

    // add special words
    _joforth_dict_entry_t* entry = _add_entry(joforth, "create");
    entry->_type = kEntryType_Prefix;
    entry->_rep._handler = _create;
    entry->_depth = 1;

    entry = _add_entry(joforth, "see");
    entry->_type = kEntryType_Prefix;
    entry->_rep._handler = _see;
    entry->_depth = 1;
}

void    joforth_destroy(joforth_t* joforth) {
    
    joforth->_allocator._free(joforth->_memory);
    memset(joforth, 0, sizeof(joforth_t));
}

void    joforth_add_word(joforth_t* joforth, const char* word, joforth_word_handler_t handler, size_t depth) {

    _joforth_dict_entry_t* i = _add_entry(joforth, word);
    i->_depth = depth;
    i->_type = kEntryType_Native;
    i->_rep._handler = handler;

    //printf("\nadded word \"%s\", key = 0x%x\n", i->_word, i->_key);
}

static _JO_ALWAYS_INLINE void _push_irstack(joforth_t* joforth, uint8_t* loc) {
    assert(joforth->_irp);
    joforth->_irstack[joforth->_irp--] = loc;
}

static _JO_ALWAYS_INLINE uint8_t* _pop_irstack(joforth_t* joforth) {
    assert(joforth->_irp < joforth->_irstack_size - 1);
    return joforth->_irstack[++joforth->_irp];
}

static _JO_ALWAYS_INLINE bool _irstack_is_empty(joforth_t* joforth) {
    return joforth->_irp == joforth->_irstack_size - 1;
}

static joforth_value_t  _str_to_value(joforth_t* joforth, const char* str) {
    joforth_value_t value = (joforth_value_t)strtoll(str, 0, joforth->_base);
    if (errno) {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
        return 0;
    }
    if (!value) {
        // is it 0, or is it wrong...?
        // skip sign first

        if (*str == '-' || *str == '+')
            ++str;

        switch (joforth->_base) {
        case 10:
        {
            while (*str && *str >= '0' && *str <= '9') ++str;
            if (*str && *str != ' ') {
                // some non-decimal character found
                joforth->_status = _JO_STATUS_INVALID_INPUT;
                return 0;
            }
            // otherwise it's a valid 0
            joforth->_status = _JO_STATUS_SUCCESS;
        }
        break;
        case 16:
        {
            if (strlen(str) > 2 && str[1] == 'x')
                str += 2;

            while (*str && *str >= '0' && *str <= '9' && *str >= 'a' && *str <= 'f') ++str;
            if (*str && *str != ' ') {
                // some non-hex character found
                joforth->_status = _JO_STATUS_INVALID_INPUT;
                return 0;
            }
            // otherwise it's a valid 0
            joforth->_status = _JO_STATUS_SUCCESS;
        }
        break;
        default: {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
        }
        }
    }
    return value;
}

static size_t _count_words(const char* word) {
    size_t words = 0;
    do {
        while (word[0] && word[0] == ' ') ++word;
        if (word[0] && word[0] != ';') {

            if (word[0] == '(') {
                while (word[0] && word[0] != ')') ++word;
                if (!word[0]) {
                    return 0;
                }                
                ++word;
                continue;
            }

            while (word[0] && word[0] != ' ' && word[0] != ';') ++word;
            if (word[0] == ' ')
                ++words;
        }
    } while (word[0] && word[0] != ';');

    return words;
}

static const char* _next_word(joforth_t* joforth, 
        char* buffer, size_t buffer_size, 
        const char* word, size_t* wp_,
        const char** comment) {

    if ( comment) {
        *comment = 0;
    }
    *wp_ = 0;
    while (word[0] && word[0] == ' ') ++word;
    if (!word[0]) {
        return 0;
    }
    // skip comments (but return pointer to it so that we can save it)
    if (word[0] == '(') {
        if(comment) {
            *comment = word+1;
        }
        while (word[0] && word[0] != ')') ++word;        
        if (!word[0]) {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return 0;
        }        
        ++word;
        while (word[0] && word[0] == ' ') ++word;
        if (!word[0]) {
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return 0;
        }
    }
    size_t wp = 0;
    bool scan_string = false;
    while (word[0]) {
        if (*word == '\"') {
            scan_string = !scan_string;
        }
        if (!scan_string && *word == ' ') {
            break;
        }
        buffer[wp++] = (char)tolower(*word++);
        if (wp == buffer_size) {
            joforth->_status = _JO_STATUS_RESOURCE_EXHAUSTED;
            return 0;
        }
    }
    buffer[wp] = 0;
    *wp_ = wp;
    return word;
}

// evaluator mode
typedef enum _joforth_eval_mode {

    kEvalMode_Compiling,
    kEvalMode_Interpreting,
    kEvalMode_Skipping,

} _joforth_eval_mode_t;

bool    joforth_eval(joforth_t* joforth, const char* word) {

    if (_JO_FAILED(joforth->_status))
        return false;

    while (word[0] && word[0] == ' ') ++word;
    if (word[0] == 0) {
        joforth->_status = _JO_STATUS_INVALID_INPUT;
        return false;
    }

    _joforth_eval_mode_t mode = (word[0] == ':') ? kEvalMode_Compiling : kEvalMode_Interpreting;

    char buffer[JOFORTH_MAX_WORD_LENGTH];
    size_t wp;
    // reset!
    joforth->_irw = 0;

    // comment (if any); we'll use it if we're compiling
    const char* comment = 0;

    if (mode == kEvalMode_Compiling) {

        _ir_emit(joforth, kIr_DefineWord);
        // skip ":"
        word++;
        // we expect the identifier to be next
        word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp, 0);
        if (!word || _JO_FAILED(joforth->_status)) {
            return false;
        }
        uint8_t* memory = _alloc(joforth, wp + 1);
        memcpy(memory, buffer, wp + 1);
        _ir_emit_ptr(joforth, memory);        
    }

    word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp, &comment);
    if (!word || _JO_FAILED(joforth->_status)) {
        return false;
    }

    // =====================================================================================
    // phase 1: convert the input text to a stream of IR codes    
    // count words
    // =====================================================================================

    size_t word_count = 0;
    size_t target_word_count = 0;   //< used when we parse prefix words    
    do {

        if (target_word_count && word_count >= target_word_count) {
            // this word will be passed, as-is, on the stack to feed a previous 
            // PREFIX word (see kWordType_Prefix)
            char* the_word = (char*)_alloc(joforth, wp + 1);
            memcpy(the_word, buffer, wp + 1);
            _ir_emit(joforth, kIr_ValuePtr);
            _ir_emit_ptr(joforth, the_word);
            target_word_count = word_count > target_word_count ? target_word_count : 0;
        }
        else {
            // first check for language keywords
            bool is_language_keyword = false;
            for (size_t n = 0; n < _joforth_keyword_lut_size; ++n) {
                if (strcmp(buffer, _joforth_keyword_lut[n]._id) == 0) {
                    is_language_keyword = true;
                    _ir_emit(joforth, _joforth_keyword_lut[n]._ir);
                    break;
                }
            }

            if (!is_language_keyword) {
                // then check for special symbols that we can interpret directly
                if (buffer[0] == '.') {
                    // . or .SomeString or ."SomeString"
                    if (wp > 1) {
                        // print a string foll
                        size_t start = 1;
                        size_t end = 2;
                        if (buffer[start] == '\"') {
                            start = 2;
                            end = 3;
                        }
                        while (buffer[end] && buffer[end] != '\"') ++end;
                        buffer[end] = 0;
                        if (start < end) {
                            // emit "dot" and put the allocated string on the value stack 
                            uint8_t* memory = _alloc(joforth, end - start + 1);
                            memcpy(memory, buffer + start, end - start + 1);
                            _ir_emit(joforth, kIr_ValuePtr);
                            _ir_emit_ptr(joforth, memory);
                            _ir_emit(joforth, kIr_DotDot);
                        }
                    }
                    else {
                        // just a dot
                        _ir_emit(joforth, kIr_Dot);
                    }
                }
                else if (buffer[0] == '?') {
                    //TODO: Forth uses this for other things as well, so this shoud 
                    //      be changed to just a special prefix operator and then interpreted 
                    //      during IR execution

                    // ? prefix (if zero)
                    if (wp > 1) {
                        // what follows will be executed if and only if tos!=0
                        _ir_emit(joforth, kIr_IfZeroOperator);
                        // a bit wonky but it works; "shift" the contents of buffer down to hide the leading ? 
                        // so that we can continune as if nothing happened...
                        size_t n = 1;
                        while (n < wp) {
                            buffer[n - 1] = buffer[n];
                            n++;
                        }
                        wp--;
                        buffer[wp] = 0;
                        continue;
                    }
                    else {
                        // just a ? isn't enough...
                        joforth->_status = _JO_STATUS_INVALID_INPUT;
                        return false;
                    }
                }
                else {
                    joforth_word_key_t key = pearson_hash(buffer);
                    _joforth_dict_entry_t* entry = _find_word(joforth, key);

                    //TODO: check for required stack depth at this point

                    if (entry) {

                        switch (entry->_type) {
                        case kEntryType_Prefix:
                        {
                            assert(entry->_depth < 2);
                            _ir_emit(joforth, kIr_WordPtr);
                            _ir_emit_ptr(joforth, entry);
                            // this word + param count
                            target_word_count = word_count + entry->_depth;
                        }
                        break;
                        case kEntryType_Native:
                            _ir_emit(joforth, kIr_Native);
                            _ir_emit_ptr(joforth, entry);
                            break;
                        case kEntryType_Word:
                            _ir_emit(joforth, kIr_WordPtr);
                            _ir_emit_ptr(joforth, entry);
                            break;
                        case kEntryType_Value:
                            _ir_emit(joforth, kIr_Value);
                            _ir_emit_value(joforth, entry->_rep._value);
                            break;
                        default:;
                        }
                    }
                    else {
                        joforth_value_t value = _str_to_value(joforth, buffer);
                        if (_JO_FAILED(joforth->_status)) {
                            uint8_t* memory = _alloc(joforth, wp + 1);
                            memcpy(memory, buffer, wp + 1);
                            _ir_emit(joforth, kIr_ValuePtr);
                            _ir_emit_ptr(joforth, memory);
                            joforth->_status = _JO_STATUS_SUCCESS;
                        }
                        else {
                            _ir_emit(joforth, kIr_Value);
                            _ir_emit_value(joforth, value);
                        }
                    }
                }
            }
        }
        ++word_count;
        word = _next_word(joforth, buffer, JOFORTH_MAX_WORD_LENGTH, word, &wp, 0);

    } while (wp);

    // terminate the ir buffer properly
    _ir_emit(joforth, kIr_Null);

    // =====================================================================================
    // phase 2: interpret or compile
    // =====================================================================================

    size_t irr = 0;
    uint8_t* irbuffer = joforth->_ir_buffer;
    
    // for self reference, i.e. "recurse"
    _joforth_dict_entry_t* self = 0;

    // are we interpreting or compiling? if the latter we need a bit of setup
    if (mode == kEvalMode_Compiling) {
        _joforth_ir_t ir;
        irbuffer = _ir_consume(irbuffer, &ir);
        assert(ir == kIr_DefineWord);
        const char* id;
        irbuffer = _ir_consume_ptr(irbuffer, (void**)&id);
        joforth_word_key_t key = pearson_hash(id);
        self = _find_word(joforth, key);
        if (self) {
            // already exists
            joforth->_status = _JO_STATUS_INVALID_INPUT;
            return false;
        }
        self = _add_entry(joforth, id);
        //ZZZ: perhaps read this from a comment string?
        self->_depth = 0;
        if(comment) {
            size_t comment_length = 0;
            while(comment[comment_length++]!=')') ;
            char* doc_copy = (char*)_alloc(joforth, comment_length);
            memcpy(doc_copy, comment, comment_length);
            doc_copy[comment_length-1] = 0;
            self->_doc = (const char*)doc_copy;

            // read the depth from the comment string
            // we exepct the comment is reliable, i.e. 
            // if the word takes two parameters then there are 
            // two distinct names listed before the '--'
            size_t whitespace_edge = 0;
            // skip any leading whitespace
            while(*doc_copy==' ') ++doc_copy;
            while( *doc_copy!='-' && *doc_copy!=')' ) {
                if ( *doc_copy==' ' ) {
                    ++whitespace_edge;
                    // skip another whitespace
                    while(*doc_copy==' ') ++doc_copy;
                }
                ++doc_copy;
            }
            self->_depth = whitespace_edge;
        }
        // the word is already compiled at this point so we just need to store the IR for it and we're done
        self->_type = kEntryType_Word;
        self->_rep._ir = (uint8_t*)_alloc(joforth, joforth->_irw);
        memcpy(self->_rep._ir, joforth->_ir_buffer, joforth->_irw);

        return true;
    }

    //NOTE: has to be the same depth as the irstack, just in case we hit something really deeply nested
    _joforth_eval_mode_t mode_stack[JOFORTH_DEFAULT_IRSTACK_SIZE];
    size_t msp = JOFORTH_DEFAULT_IRSTACK_SIZE-1;
    // used to skip the next instruction (handling the ? prefix operator)
    bool skip_one = false;
    
    // incremented by one for each IF, decremented by one for ENDIF
    size_t if_nest_level = 0;
    // set to if_nest_level, if != we have an IF-ENDIF inbetween an IF-ELSE....
    size_t else_nest_level = 0;

    // keep going until the irstack is empty
    while (true) {

        // interpret the contents of an IR buffer
        while (*irbuffer != kIr_Null) {

            _joforth_ir_t ir;
            irbuffer = _ir_consume(irbuffer, &ir);

            switch (ir) {
            case kIr_True:
            {
                if (mode != kEvalMode_Skipping) {
                    joforth_push_value(joforth, JOFORTH_TRUE);
                }
            }
            break;
            case kIr_False:
            {
                if (mode != kEvalMode_Skipping) {
                    joforth_push_value(joforth, JOFORTH_FALSE);
                }
            }
            break;
            case kIr_Invert:
            {
                if (mode != kEvalMode_Skipping) {
                    assert(joforth->_sp < joforth->_stack_size - 1);
                    // sends TRUE->FALSE and vice versa.
                    joforth->_stack[joforth->_sp + 1] = ~joforth->_stack[joforth->_sp + 1];
                }
            }
            break;
            case kIr_If:
            {
                ++if_nest_level;

                if( mode != kEvalMode_Skipping ) {
                    // decide what to do based on TOS
                    joforth_value_t tos = joforth_pop_value(joforth);
                    if (tos == JOFORTH_FALSE) {
                        // skip until ELSE
                        mode = kEvalMode_Skipping;
                        // ENDIF will keep interpreting
                        mode_stack[msp--] = kEvalMode_Interpreting;
                        // ELSE will switch to interpreting
                        mode_stack[msp--] = kEvalMode_Interpreting;                        
                    }
                    else {
                        // ENDIF will switch back to interpreting
                        mode_stack[msp--] = kEvalMode_Interpreting;
                        // ELSE will switch to skipping
                        mode_stack[msp--] = kEvalMode_Skipping;                        
                    }
                } 
                else {
                    // this IF is being skipped; ENDIF and ELSE will keep skipping
                    mode_stack[msp--] = kEvalMode_Skipping;
                    mode_stack[msp--] = kEvalMode_Skipping;                    
                }
            }
            break;
            case kIr_IfZeroOperator:
            {
                //TODO: change to general ? operator handler

                if (mode != kEvalMode_Skipping) {
                    joforth_value_t tos = joforth_top_value(joforth);
                    skip_one = tos == 0;
                    if (skip_one) {
                        // skip the next instruction
                        mode_stack[msp--] = mode;
                        mode = kEvalMode_Skipping;
                        continue;
                    }
                }
            }
            break;
            case kIr_Else:
            {
                else_nest_level = if_nest_level;

                // switch to the mode selected by the last IF and continue
                mode = mode_stack[++msp];
            }
            break;
            case kIr_Endif:
            {   
                if ( else_nest_level < if_nest_level ) {
                    // we're inside an if-endif so we need to pop 
                    // the stack as many times as ELSE didn't pop
                    for( size_t level = (if_nest_level-else_nest_level); level>0; --level) {
                        ++msp;
                    }
                } else {
                    --else_nest_level;
                }
                // switch back to the active mode of the leading IF
                mode = mode_stack[++msp];
                --if_nest_level;
            }
            break;
            case kIr_Begin:
            {
                if (mode != kEvalMode_Skipping) {
                    // push the next instruction. 
                    // until will return here and keep pushing the same location until we're done
                    _push_irstack(joforth, irbuffer);
                }
            }
            break;
            case kIr_Until:
            {
                if (mode != kEvalMode_Skipping) {
                    uint8_t* loop_location = _pop_irstack(joforth);
                    joforth_value_t tos = joforth_pop_value(joforth);
                    if (!tos) {
                        // go back to begin
                        irbuffer = loop_location;
                        // and push for the next round
                        _push_irstack(joforth, irbuffer);
                    }
                    // else we're done
                }
            }
            break;
            case kIr_Dot:
            {
                if (mode != kEvalMode_Skipping) {
                    joforth_value_t value = joforth_pop_value(joforth);
                    switch (joforth->_base)
                    {
                    case 10:
                        printf("%lld", value);
                        break;
                    case 16:
                        printf("%llx", value);
                        break;
                    default:
                        printf("NaN");
                        break;
                    }
                }
            }
            break;
            case kIr_DotDot:
            {
                if (mode != kEvalMode_Skipping) {
                    const char* str = (const char*)joforth_pop_value(joforth);
                    printf("%s",str);
                }
            }
            break;
            case kIr_ValuePtr:
            {
                joforth_value_t value;
                irbuffer = _ir_consume_ptr(irbuffer, (void**)&value);
                if (mode != kEvalMode_Skipping) {
                    joforth_push_value(joforth, value);
                }
            }
            break;
            case kIr_Value:
            {
                joforth_value_t value;
                irbuffer = _ir_consume_ptr(irbuffer, (void**)&value);
                if (mode != kEvalMode_Skipping) {
                    joforth_push_value(joforth, value);
                }
            }
            break;
            case kIr_Native:
            {
                _joforth_dict_entry_t* handler_entry;
                irbuffer = _ir_consume_ptr(irbuffer, (void**)&handler_entry);
                if (mode != kEvalMode_Skipping) {
                    handler_entry->_rep._handler(joforth);
                    if (_JO_FAILED(joforth->_status)) {
                        return false;
                    }
                }
            }
            break;
            case kIr_DefineWord:
            {
                const char* id;
                irbuffer = _ir_consume_ptr(irbuffer, (void**)&id);
                //NOTE: here we assume there can only be one, i.e. there is never more than one ":" define statement in a sentence
                if (!self) {
                    // when interpreting we need "self"
                    joforth_word_key_t key = pearson_hash(id);
                    self = _find_word(joforth, key);
                    if (_JO_FAILED(joforth->_status)) {
                        return false;
                    }
                }
            }
            break;
            case kIr_Recurse:
            {
                // simply invoke self again
                if (mode != kEvalMode_Skipping) {
                    _push_irstack(joforth, irbuffer);
                    irbuffer = self->_rep._ir;
                }
            }
            break;
            case kIr_WordPtr:
            {
                // the current word, can be used for self reference 
                _joforth_dict_entry_t* entry = 0;
                irbuffer = _ir_consume_ptr(irbuffer, (void**)&entry);

                if (entry->_type == kEntryType_Prefix) {
                    // first check that we've got enough arguments following this instruction
                            //TODO: can CREATE, VARIABLE, SEE etc. accept the result of another word...?
                            // for now; only accept value and word ptrs 
                    assert(entry->_depth < 2);

                    //ZZZ: this doesn't check softly if we're at the end of the buffer
                    _joforth_ir_t next_ir;
                    irbuffer = _ir_consume(irbuffer, &next_ir);
                    switch (next_ir) {
                    case kIr_WordPtr:
                    case kIr_ValuePtr:
                    {
                        //NOTE: we're not doing any type checking here, if it requires a WordPtr when a ValuePtr 
                        //      is passed then things WILL go wrong...
                        void* ptr;
                        irbuffer = _ir_consume_ptr(irbuffer, (void**)&ptr);
                        if (mode != kEvalMode_Skipping) {
                            joforth_push_value(joforth, (joforth_value_t)ptr);
                            entry->_rep._handler(joforth);
                        }
                    }
                    break;
                    default:
                        joforth->_status = _JO_STATUS_INVALID_INPUT;
                        return false;
                    }
                }
                else {
                    // switch to the entry's ir code and continue executing 
                    _push_irstack(joforth, irbuffer);
                    irbuffer = entry->_rep._ir;
                }
            }
            break;
            case kIr_Do:
            {
                // push the next instruction for loop to pick up                
                _push_irstack(joforth, irbuffer);
            }
            break;
            case kIr_Loop:
            {
                joforth_value_t end = joforth_pop_value(joforth);
                joforth_value_t i = joforth_pop_value(joforth);
                assert(i<end);
                uint8_t* do_loc = _pop_irstack(joforth);
                ++i;
                if(i<end) {
                    // keep going
                    _push_irstack(joforth, irbuffer = do_loc);
                    // push i and end back on the stack
                    joforth_push_value(joforth, i);
                    joforth_push_value(joforth, end);
                }
                // else we're done
            }
            break;
            default:;
            }

            if (skip_one) {
                assert(mode == kEvalMode_Skipping);
                skip_one = false;
                mode = mode_stack[++msp];
            }
        }

        if (_irstack_is_empty(joforth)) {
            // exit outer while
            break;
        }

        irbuffer = _pop_irstack(joforth);

    }; // while(true)

    return true;
}

void    joforth_dump_dict(joforth_t* joforth) {
    printf("joforth dictionary info:\n");
    if (joforth->_dict) {
        for (size_t i = 0u; i < JOFORTH_DICT_BUCKETS; ++i) {
            _joforth_dict_entry_t* entry = joforth->_dict + i;
            while (entry->_key) {
                if ((entry->_type == kEntryType_Prefix) == 0) {
                    printf("\tentry: key 0x%x, word \"%s\", takes %zu parameters\n", entry->_key, entry->_word, entry->_depth);
                }
                else {
                    printf("\tPREFIX entry: word \"%s\", takes %zu parameters\n", entry->_word, entry->_depth);
                }
                entry = entry->_next;
            }
        }
    }
    else {
        printf("\tempty\n");
    }
}

void    joforth_dump_stack(joforth_t* joforth) {
    if (joforth->_sp == (joforth->_stack_size - 1)) {
        printf("joforth: stack is empty\n");
    }
    else {
        printf("joforth stack contents:\n");
        for (size_t sp = joforth->_sp + 1; sp < joforth->_stack_size; ++sp) {
            printf("\t%lld\n", joforth->_stack[sp]);
        }
    }
}
