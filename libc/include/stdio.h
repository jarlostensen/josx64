#ifndef _JOS_STDIO_H
#define _JOS_STDIO_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef wchar_t
#undef wchar_t
#endif
typedef int16_t	wchar_t;

int printf(const char* __restrict, ...);
int puts(const char*);
int sprintf (char * __restrict, const char * __restrict, ... );
int sprintf_s(char* __restrict buffer, size_t buffercount, const char* __restrict format, ...);
int snprintf ( char * __restrict, size_t n, const char * __restrict, ... );
int vsnprintf(char* buffer, size_t n, const char* format, va_list parameters);

#ifdef __cplusplus
}
#endif

#endif // _JOS
