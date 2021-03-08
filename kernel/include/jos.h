#pragma once
#ifndef _JOS_H
#define _JOS_H

#include <joBase/joBase.h>

#if defined(_JO_BARE_METAL_BUILD) && !defined(_JOS_KERNEL_BUILD)
// for backwards compatibility
#define _JOS_KERNEL_BUILD
#endif

// all sub-systems are provided an implementation instance of this interface
typedef struct _jos_allocator {
    void*   (*_alloc)(size_t);
    void    (*_free)(void*);

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


#ifdef _JOS_KERNEL_BUILD

//TODO: this will be removed
extern uint16_t kJosKernelCS;

#define _JOS_MAYBE_UNUSED __attribute__((unused))
#define _JOS_INLINE_FUNC __attribute__((unused)) static

#define _JOS_ALWAYS_INLINE __attribute__((always_inline))

#define _JOS_PACKED_ __attribute((packed))
#define _JOS_UNREACHABLE() __builtin_unreachable()

void trace(const char* channel, const char* msg,...);
#define _JOS_KTRACE_CHANNEL(channel, msg,...) trace(channel, msg,##__VA_ARGS__)
#define _JOS_KTRACE(msg,...)  trace(0, msg,##__VA_ARGS__)

void trace_buf(const char* channel, const void* data, size_t length);
#define _JOS_KTRACE_CHANNEL_BUF(channel, data,length) trace_buf(channel, data, length)
#define _JOS_KTRACE_BUF(data,length) trace_buf(0, data, length)

#define _JOS_BOCHS_DBGBREAK() asm volatile ("xchg %bx,%bx")
#define _JOS_GDB_DBGBREAK() asm volatile ("int $03")

#define _JOS_BOCHS_DBGBREAK_TRACE()\
trace(0, "break at %s:%d\n", __FILE__,__LINE__);\
asm volatile ("xchg %bx,%bx")

#define _JOS_ASSERT_COND(cond) #cond
#define _JOS_ASSERT(cond)\
if(!(cond))\
{\
    trace(0, "assert %s, %s:%d \n", _JOS_ASSERT_COND(cond), __FILE__,__LINE__);\
}

#define _JOS_ALIGN(type,name,alignment) type name __attribute__ ((aligned (alignment)))

#define _JOS_PACKED __attribute__((packed))
#define _JOS_NORETURN __attribute__((__noreturn__))

#define _JOS_KERNEL_PANIC()\
    trace(0, "PANIC @ %s:%d \n", __FILE__,__LINE__);\
    k_panic()

#else
//TODO: check if this is actually VS, but we're assuming it because we're in control...
#define _JOS_UNREACHABLE()
#define _JOS_MAYBE_UNUSED
#define _JOS_INLINE_FUNC static
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

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

#endif // _JOS_H
