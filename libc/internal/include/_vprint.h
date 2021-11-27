#pragma once

#include "libc_internal.h"

typedef int (*putchar_func_t)(void* ctx, int character);
typedef void (*flush_func_t)(void* ctx);
typedef int (*print_func_t_w)(void* ctx, const wchar_t* data, size_t len);
typedef int (*print_func_t_a)(void* ctx, const char* data, size_t len);
typedef int (*print_convert_func_t_w)(void* ctx, const wchar_t* data, size_t len);
typedef int (*print_convert_func_t_a)(void* ctx, const char* data, size_t len);

// the various printf/sprintf etc. functions use an implementation template (_vprint_impl), this structure 
// effectively provides the different policies used.
typedef struct _vprint_ctx
{
	print_func_t_w	_print_w;
	print_func_t_a	_print_a;
	print_convert_func_t_w	_print_convert_w;
	print_convert_func_t_a	_print_convert_a;
	putchar_func_t  _putchar;
	flush_func_t    _flush;
	void*			_that;
} _vprint_ctx_t;

extern int _vprint_impl_w(_vprint_ctx_t* ctx, const wchar_t* __restrict format, va_list parameters);
extern int _vprint_impl_a(_vprint_ctx_t* ctx, const char* __restrict format, va_list parameters);
