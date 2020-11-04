#pragma once
#ifndef _JOS_LIBC_INTERNAL_STDIO_H
#define _JOS_LIBC_INTERNAL_STDIO_H

#include <stdio.h>
#include <stdarg.h>

typedef int (*putchar_func_t)(void* ctx, int character);
typedef void (*flush_func_t)(void* ctx);
typedef int (*print_func_t)(void* ctx, const char* data, size_t len);
typedef int (*wprint_func_t)(void* ctx, const wchar_t* data, size_t len);

// the various printf/sprintf etc. functions use an implementation template (_vprint_impl), this structure 
// effectively provides the different policies used.
typedef struct _ctx_struct
{
	print_func_t	_print;
	wprint_func_t   _wprint;
	putchar_func_t  _putchar;
	flush_func_t    _flush;
	void*			_that;
} ctx_t;

int _vprint_impl(ctx_t* ctx, const wchar_t* __restrict format, va_list parameters);
int _wvprint_impl(ctx_t* ctx, const wchar_t* __restrict format, va_list parameters);

#endif // _JOS_LIBC_INTERNAL_STDIO_H