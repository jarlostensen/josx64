#pragma once
#ifndef _JOS_H
#define _JOS_H

#include <joBase/joBase.h>

#if defined(__clang__) || defined(__GNUC__)
    #define ASM_SYNTAX_ATNT
#else 
    #define ASM_SYNTAX_INTEL
#endif

#if defined(_JO_BARE_METAL_BUILD) && !defined(_JOS_KERNEL_BUILD)
// for backwards compatibility
#define _JOS_KERNEL_BUILD
#endif

// All sub-systems are provided an implementation instance of this interface
// All allocators implement this (using this structure as a basic vtable entry)
struct _jos_allocator;
typedef void* (*jos_allocator_alloc_func_t)(struct _jos_allocator*, size_t);
typedef void  (*jos_allocator_free_func_t)(struct _jos_allocator*, void*);
typedef void* (*jos_allocator_realloc_func_t)(struct _jos_allocator*, void*, size_t);

typedef struct _jos_allocator {
    jos_allocator_alloc_func_t      alloc;
    jos_allocator_free_func_t       free;
    jos_allocator_realloc_func_t    realloc;

} jos_allocator_t;

// ========================================================================= misc types

// generic position
typedef struct _pos {

    size_t      x;
    size_t      y;

} pos_t;
// generic rectangle [top, bottom>, [left, right>
typedef struct _rect {
    size_t      top;
    size_t      left;
    size_t      bottom;
    size_t      right;
} rect_t;

#define _JOS_SWAP(a,b)\
(a) ^= (b);\
(b) ^= (a); \
(a) ^= (b)
#if !defined(__clang__)
#define _JOS_SWAP_PTR(a,b)\
((uintptr_t)(a)) ^= ((uintptr_t)(b));\
((uintptr_t)(b)) ^= ((uintptr_t)(a));\
((uintptr_t)(a)) ^= ((uintptr_t)(b))
#endif

#ifdef _JOS_KERNEL_BUILD

// NOTE: this ties us in with the debugger module which may not be something we want longer term
extern bool debugger_is_connected(void);
extern void debugger_trigger_assert(const char*, const char*, int);

//TODO: this will be removed
extern uint16_t kJosKernelCS;

#define _JOS_MAYBE_UNUSED __attribute__((unused))
#define _JOS_INLINE_FUNC __attribute__((unused)) static
#define _JOS_API_FUNC extern
#define _JOS_ALWAYS_INLINE __attribute__((always_inline))

#define _JOS_PACKED_ __attribute((packed))
#define _JOS_UNREACHABLE() __builtin_unreachable()

void trace(const char* channel, const char* msg,...);
#define _JOS_KTRACE_CHANNEL(channel, msg, ...) trace(channel, msg, ##__VA_ARGS__)
#define _JOS_KTRACE(msg, ...)  trace(0, msg, ##__VA_ARGS__)

void trace_buf(const char* channel, const void* data, size_t length);
#define _JOS_KTRACE_CHANNEL_BUF(channel, data,length) trace_buf(channel, data, length)
#define _JOS_KTRACE_BUF(data,length) trace_buf(0, data, length)

#define _JOS_BOCHS_DBGBREAK() asm volatile ("xchg %bx,%bx")
#define _JOS_GDB_DBGBREAK() __asm__ volatile ("int $03")

#define _JOS_BOCHS_DBGBREAK_TRACE()\
trace(0, "break at %s:%d\n", __FILE__,__LINE__);\
asm volatile ("xchg %bx,%bx")

#define _JOS_ASSERT_COND(cond) #cond
#define _JOS_ASSERT(cond)\
if(!(cond))\
{\
    if ( !debugger_is_connected() ) {\
        trace(0, "assert %s, %s:%d \n", _JOS_ASSERT_COND(cond), __FILE__,__LINE__);\
    }\
    else {\
        debugger_trigger_assert(_JOS_ASSERT_COND(cond), __FILE__, __LINE__);\
    }\
}

#define _JOS_ALIGN(type,name,alignment) type name __attribute__ ((aligned (alignment)))

#define _JOS_PACKED __attribute__((packed))
#define _JOS_NORETURN __attribute__((__noreturn__))

#define _JOS_KERNEL_PANIC()\
    trace(0, "PANIC @ %s:%d \n", __FILE__,__LINE__);\
    k_panic()

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

#else
//TODO: check if this is actually VS, but we're assuming it because we're in control...
#define _JOS_UNREACHABLE()
#define _JOS_MAYBE_UNUSED
#define _JOS_INLINE_FUNC static
#define _JOS_API_FUNC extern
#define _JOS_BOCHS_DBGBREAK() __debugbreak()

#define _JOS_KTRACE_CHANNEL(channel, msg,...)
#define _JOS_KTRACE(msg,...)
#define _JOS_KTRACE_CHANNEL_BUF(channel, data,length)
#define _JOS_KTRACE_BUF(data,length)

#define _JOS_ALWAYS_INLINE

#define _JOS_BOCHS_DBGBREAK_TRACE() __debugbreak()
#define _JOS_GDB_DBGBREAK() __debugbreak()

#define _JOS_PACKED_
#define _JOS_NORETURN

#define _JOS_KERNEL_PANIC()

#ifdef _DEBUG
#define _JOS_ASSERT(cond) if(!(cond)) { __debugbreak(); }
#else
#define _JOS_ASSERT(cond)
#endif
#define _JOS_ALIGN(type,name,alignment) __declspec(align(alignment)) type name
#endif

#endif // _JOS_H
