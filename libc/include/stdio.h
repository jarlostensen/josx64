#ifndef _JOS_STDIO_H
#define _JOS_STDIO_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif


int swprintf(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
int snprintf(char* __restrict buffer, size_t sizeOfBuffer, const char* __restrict format, ...);
int vswprintf(wchar_t *__restrict buffer, size_t bufsz, const wchar_t * __restrict format, va_list vlist);
int vsnprintf(char *__restrict buffer, size_t bufsz, const char * __restrict format, va_list parameters);

typedef struct _FILE {

    // a string, for example
    struct {
        const uint8_t*     _begin;
        const uint8_t*     _end;
        
    } _buffer;

} FILE;

#ifdef __cplusplus
}
#endif

#endif // _JOS
