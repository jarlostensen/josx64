#ifndef _JOS_STDIO_H
#define _JOS_STDIO_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../internal/include/_file.h"

extern int swprintf(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int snprintf(char* __restrict buffer, size_t sizeOfBuffer, const char* __restrict format, ...);
extern int vswprintf(wchar_t *__restrict buffer, size_t bufsz, const wchar_t * __restrict format, va_list vlist);
extern int vsnprintf(char *__restrict buffer, size_t bufsz, const char * __restrict format, va_list parameters);
extern int printf(const char* __restrict format, ...);

#ifdef __cplusplus
}
#endif

#endif // _JOS
