#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* __restrict, ...);
//zzz: slow and horrible, not used in our code int putchar(int);
int puts(const char*);
int sprintf (char * __restrict, const char * __restrict, ... );
int sprintf_s(char* __restrict buffer, size_t buffercount, const char* __restrict format, ...);
int snprintf ( char * __restrict, size_t n, const char * __restrict, ... );
int vsnprintf(char* buffer, size_t n, const char* format, va_list parameters);

#ifdef __cplusplus
}
#endif

#endif
