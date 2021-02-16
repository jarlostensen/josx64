#pragma once


#include <joforth.h>

//NOTE: these need to fit in a byte
typedef enum _joforth_ir {

    kIr_Null = 0,
    kIr_DefineWord,              // ":"
    kIr_WordPtr,                 // followed by 64 bit pointer to a joforth_dict_t entry
    kIr_ValuePtr,                // followed by 64 bit pointer to a 0 terminated string
    kIr_Value,                   // followed by a 64 bit immediate value
    kIr_Native,                  // followed by 64 bit pointer to native handler routine
    kIr_IfZeroOperator,          // ? prefix to words, like "?dup"
    kIr_If,
    kIr_Else,
    kIr_Endif,
    kIr_Begin,
    kIr_Until,
    kIr_While,
    kIr_Repeat,
    kIr_Do,
    kIr_Loop,
    kIr_EndDefineWord,    
    kIr_Recurse,
    kIr_Dot,                    // . <tos value>
    kIr_DotDot,                 // .<string pointer>
    kIr_True,
    kIr_False,
    kIr_Invert,

} _joforth_ir_t;

typedef struct _joforth_keyword_lut_entry {
    const char*     _id;
    _joforth_ir_t   _ir;
} _joforth_keyword_lut_entry_t;

static const _joforth_keyword_lut_entry_t _joforth_keyword_lut[] = {
    { ._id = ";", ._ir = kIr_EndDefineWord },
    { ._id = "true", ._ir = kIr_True },
    { ._id = "false", ._ir = kIr_False },
    { ._id = "invert", ._ir = kIr_Invert },
    { ._id = "recurse", ._ir = kIr_Recurse },    
    { ._id = "if", ._ir = kIr_If },
    { ._id = "else", ._ir = kIr_Else },
    { ._id = "endif", ._ir = kIr_Endif },
    { ._id = "begin", ._ir = kIr_Begin },
    { ._id = "until", ._ir = kIr_Until },
    { ._id = "while", ._ir = kIr_While },
    { ._id = "repeat", ._ir = kIr_Repeat },
    { ._id = "do", ._ir = kIr_Do },
    { ._id = "loop", ._ir = kIr_Loop },
};
static const size_t _joforth_keyword_lut_size = sizeof(_joforth_keyword_lut)/sizeof(_joforth_keyword_lut_entry_t);

static _JO_ALWAYS_INLINE void _ir_emit(joforth_t* joforth, _joforth_ir_t ir) {
    assert(joforth->_irw < joforth->_ir_buffer_size-1);
    joforth->_ir_buffer[joforth->_irw++] = (uint8_t)(ir & 0xff);
}

static _JO_ALWAYS_INLINE void _ir_emit_ptr(joforth_t* joforth, void* ptr) {
    assert(joforth->_irw < joforth->_ir_buffer_size-sizeof(void*)-1);
    memcpy(joforth->_ir_buffer + joforth->_irw, &ptr, sizeof(void*));
    joforth->_irw += sizeof(void*);
}

static _JO_ALWAYS_INLINE void _ir_emit_value(joforth_t *joforth, joforth_value_t value) {
    assert(joforth->_irw < joforth->_ir_buffer_size-sizeof(value)-1);
    memcpy(joforth->_ir_buffer + joforth->_irw, &value, sizeof(value));
    joforth->_irw += sizeof(value);
}

static _JO_ALWAYS_INLINE uint8_t* _ir_consume(uint8_t* buffer, _joforth_ir_t* ir) {
    *ir = *buffer++;
    return buffer;
}

static _JO_ALWAYS_INLINE uint8_t* _ir_consume_ptr(uint8_t* buffer, void**ptr) {
    memcpy(ptr, buffer, sizeof(void*));
    buffer+=sizeof(void*);
    return buffer;
}

static _JO_ALWAYS_INLINE uint8_t* _ir_consume_value(uint8_t* buffer, joforth_value_t* value) {
    memcpy(value, buffer, sizeof(joforth_value_t));
    buffer += sizeof(joforth_value_t);
    return buffer;
}
