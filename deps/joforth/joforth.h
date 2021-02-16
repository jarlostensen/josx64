#pragma once 

// =======================================================================
// joForth
// A very simple, and yet incomplete, Forth interpreter and compiler. 
// 
// http://galileo.phys.virginia.edu/classes/551.jvn.fall01/primer.htm
//

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <jo.h>

#define JOFORTH_MAX_WORD_LENGTH 128

typedef int64_t     joforth_value_t;
typedef uint32_t    joforth_word_address_t;
typedef uint32_t    joforth_word_key_t;
typedef struct _joforth joforth_t;
typedef struct _joforth_dict_entry _joforth_dict_entry_t;

typedef void (*joforth_word_handler_t)(joforth_t* joforth);

// used internally
typedef struct _joforth_dict_entry {
    joforth_word_key_t              _key;
    // id
    const char*                     _word;
    const char*                     _doc;
    enum {
        kEntryType_Null,
        kEntryType_Native,
        kEntryType_Value,
        kEntryType_Word,
        kEntryType_Prefix,
    } _type;
    // value stack depth required (i.e. number of arguments to word)
    size_t                           _depth;
    union {
        // a native callable function 
        joforth_word_handler_t          _handler;
        // a value to push on the stack during execution
        joforth_value_t                 _value;
        // IR sequence, terminated with kIr_Null
        uint8_t*                        _ir;  
    } _rep;

    // for hash table linking only
    struct _joforth_dict_entry*     _next;
} _joforth_dict_entry_t;

#define JOFORTH_DEFAULT_STACK_SIZE      0x400
#define JOFORTH_DEFAULT_MEMORY_SIZE     0x20000
#define JOFORTH_DEFAULT_RSTACK_SIZE     0x100
#define JOFORTH_TRUE                    (~(joforth_value_t)0)
#define JOFORTH_FALSE                   ((joforth_value_t)0)

// used to provide allocator/free functionality, joForth doesn't use any other allocator
typedef struct _joforth_allocator {
    void* (*_alloc)(size_t);
    void (*_free)(void*);
} joforth_allocator_t;

// the joForth VM state
typedef struct _joforth {
    _joforth_dict_entry_t       *   _dict;
    joforth_value_t             *   _stack;
    uint8_t                     *   _memory;

    // buffers IR codes for the parser pass
    uint8_t                     *   _ir_buffer;
    size_t                          _irw;
    size_t                          _ir_buffer_size;

    // return locations for IR code when executing
    uint8_t**                       _irstack;
    size_t                          _irstack_size;
    size_t                          _irp;

    joforth_allocator_t             _allocator;
    
    // current input base
    int                             _base;
    // if 0 then default, in units of joforth_value_t 
    size_t                          _stack_size;
    // if 0 then default, in units of bytes
    size_t                          _memory_size;
    // stack pointers
    size_t                          _sp;
    // memory allocation pointer (we don't do "free")
    size_t                          _mp;
    // status code of last operation
    jo_status_t                     _status;
    
} joforth_t;

// initialise the joForth VM
void    joforth_initialise(joforth_t* joforth);
// shutdown
void    joforth_destroy(joforth_t* joforth);
// add a word to the interpreter with an immediate evaluator (handler) and the required stack depth
void    joforth_add_word(joforth_t* joforth, const char* word, joforth_word_handler_t handler, size_t depth);
// evaluate a sequence of words (sentence)
// for example:
//  joforth_eval(&joforth, ": squared ( a -- a*a ) dup *  ;");
//  joforth_eval(&joforth, "80");
//  joforth_eval(&joforth, "squared");
//
bool    joforth_eval(joforth_t* joforth, const char* word);

// push a value on the stack (use this in your handlers)
// sets the zero flag if the value is 0
static _JO_ALWAYS_INLINE void    joforth_push_value(joforth_t* joforth, joforth_value_t value) {
    assert(joforth->_sp);
    joforth->_stack[joforth->_sp--] = value; 
}

// pop a value off the stack (use this in your handlers)
static _JO_ALWAYS_INLINE joforth_value_t joforth_pop_value(joforth_t* joforth) {
    assert(joforth->_sp < joforth->_stack_size-1);
    return joforth->_stack[++joforth->_sp];
}

// read top value from the stack (use this in your handlers)
static _JO_ALWAYS_INLINE joforth_value_t    joforth_top_value(joforth_t* joforth) {
    assert(joforth->_sp < joforth->_stack_size-1);
    return joforth->_stack[joforth->_sp+1];
}

static _JO_ALWAYS_INLINE joforth_value_t    joforth_stack_is_empty(joforth_t* joforth) {
    return joforth->_sp == joforth->_stack_size-1;
}

// printf dictionary contents
void    joforth_dump_dict(joforth_t* joforth);
// dump current stack
void    joforth_dump_stack(joforth_t* joforth);