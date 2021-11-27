#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <output_console.h>
#include "../internal/include/_vprint.h"

#ifndef _JOS_KERNEL_BUILD
#include <intrin.h>
#endif

// ================================================================================================================

typedef struct _printf_ctx
{
#define _PRINTF_CTX_LINE_LENGTH 256
	char    _line[_PRINTF_CTX_LINE_LENGTH];
	size_t  _wp;
} printf_ctx_t;

static void console_flush(void* ctx)
{
	printf_ctx_t* printf_ctx = (printf_ctx_t*)ctx;
	if (printf_ctx->_wp)
	{
		output_console_output_string_a(printf_ctx->_line);
		//ZZZ:output_console_flush(&_stdout);
		printf_ctx->_wp = 0;
	}
}

static int console_print(void* ctx, const char* data, size_t length_) {
	printf_ctx_t* printf_ctx = (printf_ctx_t*)ctx;
	size_t length = length_;
	while (length)
	{
		//NOTE: this can never be 0, we always flush when the buffer is full
		size_t sub_length = min(_PRINTF_CTX_LINE_LENGTH - printf_ctx->_wp, length);
		memcpy(printf_ctx->_line + printf_ctx->_wp, data, sub_length);
		length -= sub_length;
		printf_ctx->_wp += sub_length;
		if (printf_ctx->_wp == _PRINTF_CTX_LINE_LENGTH)
		{
			console_flush(ctx);
		}
	}

	return (int)length_;
}

static int console_putchar(void* ctx, int c) {
	printf_ctx_t* printf_ctx = (printf_ctx_t*)ctx;
	//NOTE: this can never be 0, we always flush when the buffer is full
	memcpy(printf_ctx->_line + printf_ctx->_wp, &c, sizeof(c));
	++printf_ctx->_wp;
	if (printf_ctx->_wp == _PRINTF_CTX_LINE_LENGTH)
	{
		console_flush(ctx);
	}
	return 1;
}

extern int _JOS_LIBC_FUNC_NAME(printf)(const char* __restrict format, ...)
{
	va_list parameters;
	va_start(parameters, format);
	int count = _vprint_impl_a(&(_vprint_ctx_t) {
		._print_a = console_print,
			._putchar = console_putchar,
			._flush = console_flush,
			._that = (void*) & (printf_ctx_t) { ._wp = 0 }
	},
		format, parameters);
	va_end(parameters);
	return count;
}

int _JOS_LIBC_FUNC_NAME(puts)(const char* string) {
	return _JOS_LIBC_FUNC_NAME(printf)("%s\n", string);
}

// ================================================================================================================

