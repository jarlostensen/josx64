#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef _MSC_VER
#define _JO_ALWAYS_INLINE  __forceinline
#else
#define _JO_ALWAYS_INLINE __attribute__((always_inline))
#endif

typedef uint32_t jo_status_t;
#define _JO_STATUS_WIDTH                     (sizeof(jos_status_t)/8)
#define _JO_STATUS_SUCCESS                   0x80000000
#define _JO_STATUS_ERROR_MASK                0x70000000
#define _JO_SUCCEEDED(status)                (!!((status) & _JO_STATUS_SUCCESS))
#define _JO_FAILED(status)                   (!!((status) & _JO_STATUS_ERROR_MASK))
#define _JO_STATUS_ERROR(error_code)         (((error_code) & 0xfffffff) | _JO_STATUS_ERROR_MASK)

// codes straight out of the Google playbook because I use these elsewhere in my code(s) and they work
#define _JO_STATUS_CANCELLED                 _JO_STATUS_ERROR(1)
#define _JO_STATUS_UNKNOWN                   _JO_STATUS_ERROR(2)
#define _JO_STATUS_DEADLINE_EXCEEDED         _JO_STATUS_ERROR(3)
#define _JO_STATUS_NOT_FOUND                 _JO_STATUS_ERROR(4)
#define _JO_STATUS_ALREADY_EXISTS            _JO_STATUS_ERROR(5)
#define _JO_STATUS_PERMISSION_DENIED         _JO_STATUS_ERROR(6)
#define _JO_STATUS_UNAUTHENTICATED           _JO_STATUS_ERROR(7)
#define _JO_STATUS_RESOURCE_EXHAUSTED        _JO_STATUS_ERROR(8)
#define _JO_STATUS_FAILED_PRECONDITION       _JO_STATUS_ERROR(9)
#define _JO_STATUS_ABORTED                   _JO_STATUS_ERROR(10)
#define _JO_STATUS_OUT_OF_RANGE              _JO_STATUS_ERROR(11)
#define _JO_STATUS_UNIMPLEMENTED             _JO_STATUS_ERROR(12)
#define _JO_STATUS_INTERNAL                  _JO_STATUS_ERROR(13)
#define _JO_STATUS_UNAVAILABLE               _JO_STATUS_ERROR(14)
#define _JO_STATUS_DATA_LOSS                 _JO_STATUS_ERROR(15)
#define _JO_STATUS_INVALID_INPUT             _JO_STATUS_ERROR(16)

