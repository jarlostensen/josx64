#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>

#ifndef _JOS_KERNEL_BUILD
#define EOF (int)(-1)
#endif 
//#include "include\_stdio.h"
#include "include\libc_internal.h"

#define _JOS_ASSERT(cond)

typedef int (*putchar_func_t)(void* ctx, int character);
typedef void (*flush_func_t)(void* ctx);
typedef int (*print_func_t_w)(void* ctx, const wchar_t* data, size_t len);
typedef int (*print_func_t_a)(void* ctx, const char* data, size_t len);

// the various printf/sprintf etc. functions use an implementation template (_vprint_impl), this structure 
// effectively provides the different policies used.
typedef struct _ctx
{
	print_func_t_w	_print_w;
	print_func_t_a	_print_a;
	putchar_func_t  _putchar;
	flush_func_t    _flush;
	void*			_that;
} ctx_t;

// WIDECHAR is defined: this will build the wide implementation
#define WIDECHAR
#include "_vprint.inc.c"

// implements narrow versions
#undef WIDECHAR
#include "_vprint.inc.c"

