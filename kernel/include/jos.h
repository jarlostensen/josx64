#pragma once
#ifndef _JOS_H
#define _JOS_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t jos_status_t;
#define _JOS_K_STATUS_WIDTH                     (sizeof(jos_status_t)/8)
#define _JOS_K_STATUS_SUCCESS                   0x80000000
#define _JOS_K_STATUS_ERROR_MASK                0x70000000
#define _JOS_K_SUCCEEDED(status)                (!!((status) & _JOS_K_STATUS_SUCCESS))
#define _JOS_K_FAILED(status)                   (!!((status) & _JOS_K_STATUS_ERROR_MASK))
#define _JOS_K_STATUS_ERROR(error_code)         (((error_code) & 0xfffffff) | _JOS_K_STATUS_ERROR_MASK)

// codes straight out of the Google playbook because I use these elsewhere in my code(s) and they work
#define _JOS_K_STATUS_CANCELLED                 _JOS_K_STATUS_ERROR(1)
#define _JOS_K_STATUS_UNKNOWN                   _JOS_K_STATUS_ERROR(2)
#define _JOS_K_STATUS_DEADLINE_EXCEEDED         _JOS_K_STATUS_ERROR(3)
#define _JOS_K_STATUS_NOT_FOUND                 _JOS_K_STATUS_ERROR(4)
#define _JOS_K_STATUS_ALREADY_EXISTS            _JOS_K_STATUS_ERROR(5)
#define _JOS_K_STATUS_PERMISSION_DENIED         _JOS_K_STATUS_ERROR(6)
#define _JOS_K_STATUS_UNAUTHENTICATED           _JOS_K_STATUS_ERROR(7)
#define _JOS_K_STATUS_RESOURCE_EXHAUSTED        _JOS_K_STATUS_ERROR(8)
#define _JOS_K_STATUS_FAILED_PRECONDITION       _JOS_K_STATUS_ERROR(9)
#define _JOS_K_STATUS_ABORTED                   _JOS_K_STATUS_ERROR(10)
#define _JOS_K_STATUS_OUT_OF_RANGE              _JOS_K_STATUS_ERROR(11)
#define _JOS_K_STATUS_UNIMPLEMENTED             _JOS_K_STATUS_ERROR(12)
#define _JOS_K_STATUS_INTERNAL                  _JOS_K_STATUS_ERROR(13)
#define _JOS_K_STATUS_UNAVAILABLE               _JOS_K_STATUS_ERROR(14)
#define _JOS_K_STATUS_DATA_LOSS                 _JOS_K_STATUS_ERROR(15)

extern uint16_t kJosKernelCS;

#ifdef _JOS_KERNEL_BUILD
#define _JOS_MAYBE_UNUSED __attribute__((unused))
#define _JOS_INLINE_FUNC __attribute__((unused)) static

#define _JOS_ALWAYS_INLINE __attribute__((always_inline))

#define _JOS_PACKED_ __attribute((packed))

void _k_trace(const char* channel, const char* msg,...);
#define _JOS_KTRACE_CHANNEL(channel, msg,...) _k_trace(channel, msg,##__VA_ARGS__)
#define _JOS_KTRACE(msg,...)  _k_trace(0, msg,##__VA_ARGS__)

void _k_trace_buf(const char* channel, const void* data, size_t length);
#define _JOS_KTRACE_CHANNEL_BUF(channel, data,length) _k_trace_buf(channel, data, length)
#define _JOS_KTRACE_BUF(data,length) _k_trace_buf(0, data, length)

#define _JOS_BOCHS_DBGBREAK() asm volatile ("xchg %bx,%bx")
#define _JOS_GDB_DBGBREAK() asm volatile ("int $03")

#define _JOS_BOCHS_DBGBREAK_TRACE()\
_k_trace(0, "break at %s:%d\n", __FILE__,__LINE__);\
asm volatile ("xchg %bx,%bx")

#ifdef _DEBUG
#define _JOS_ASSERT_COND(cond) #cond
#define _JOS_ASSERT(cond)\
if(!(cond))\
{\
    _k_trace(0, "assert %s, %s:%d \n", _JOS_ASSERT_COND(cond), __FILE__,__LINE__);\
    asm volatile ("xchg %bx,%bx");\
}
#else
#define _JOS_ASSERT(cond)
#endif

#define _JOS_ALIGN(type,name,alignment) type name __attribute__ ((aligned (alignment)))

#define _JOS_PACKED __attribute__((packed))
#define _JOS_NORETURN __attribute__((__noreturn__))

#define _JOS_KERNEL_PANIC()\
    _k_trace(0, "PANIC @ %s:%d \n", __FILE__,__LINE__);\
    k_panic()

#else
//TODO: check if this is actually VS, but we're assuming it because we're in control...
#define _JOS_MAYBE_UNUSED
#define _JOS_INLINE_FUNC static
#define _JOS_BOCHS_DBGBREAK() __debugbreak()

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
