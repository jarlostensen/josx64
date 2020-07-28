#ifndef _JOS_ATOMIC_H
#define _JOS_ATOMIC_H

#include <stdint.h>
#include <jos.h>

typedef struct _atomic_int
{
    volatile _JOS_ALIGN(int,_val,4);
} atomic_int_t;

typedef struct _atomic_int32
{
    volatile _JOS_ALIGN(int64_t,_val, 4);
} atomic_int32_t;

typedef struct _atomic_uint32
{
    volatile _JOS_ALIGN(uint32_t,_val, 4);
} atomic_uint32_t;

typedef struct _atomic_int64
{
    volatile _JOS_ALIGN(int64_t,_val,8);
} atomic_int64_t;

typedef struct _atomic_uint64
{
    volatile _JOS_ALIGN(uint64_t,_val,8);
} atomic_uint64_t;

#endif // _JOS_ATOMIC_H