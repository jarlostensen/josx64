#ifndef _JOS_STDIO_H
#define _JOS_STDIO_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//#ifdef wchar_t
//#undef wchar_t
//#endif
//typedef uint16_t	wchar_t;

int swprintf(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
int vswprintf(wchar_t *__restrict buffer, size_t bufsz, const wchar_t * __restrict format, va_list vlist);
int vswprintf(wchar_t *__restrict buffer, size_t bufsz, const wchar_t * __restrict format, va_list parameters);

int printf(const char* __restrict, ...);
int puts(const char*);

typedef void* FILE;

#ifdef __cplusplus
}
#endif

#endif // _JOS
