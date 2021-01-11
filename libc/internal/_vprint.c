#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifndef _JOS_KERNEL_BUILD
#define EOF (int)(-1)
#endif 
#include "include\_stdio.h"
#include "include\libc_internal.h"

#define _JOS_ASSERT(cond)

static int printdecimal(ctx_t* ctx, long long d, int un_signed)
{
	int written = 0;
	if (!d)
	{
		return ctx->_putchar(ctx->_that, (int)L'0');
	}

	if (d < 0)
	{
		if (!un_signed)
		{
			int res = ctx->_putchar(ctx->_that, (int)L'-');
			if (res == EOF)
				return EOF;
			++written;
		}
		d *= -1;
	}
	// simple and dumb but it works...
	long long pow_10 = 1;
	long long dd = d;

	// find highest power of 10 
	while (dd > 9)
	{
		dd /= 10;
		pow_10 *= 10;
	}

	// print digits from MSD to LSD
	while (true)
	{
		int res = ctx->_putchar(ctx->_that, (int)L'0' + (int)dd);
		if (res == EOF)
			return EOF;
		written+=res;
		d = d - (dd * pow_10);
		pow_10 /= 10;
		if (!pow_10)
			break;
		dd = d / pow_10;
	};
	return written;
}

static int printhex(ctx_t* ctx, int width, long long d)
{
	static const wchar_t* kHexDigits = L"0123456789abcdef";
	int written = 0;

	if (d < 0)
	{
		d *= -1;
		//don't write a sign
	}
	if (width)
	{
		// calculate leading zeros required
		if (!d)
		{
			// all zeros; just output them
			written += width;
			while (width--)
			{
				int res = ctx->_putchar(ctx->_that, (int)L'0');
				if (res == EOF)
					return EOF;
			}
			return written;
		}

#ifdef _JOS_KERNEL_BUILD
		unsigned long d_width;
		(d_width = 63 - __builtin_clzll(d), ++d_width);
#else
		unsigned long d_width;
		(_BitScanReverse64(&d_width, d), ++d_width);
#endif
		d_width = (d_width / 4) + (d_width & 3 ? 1 : 0);
		if (width >= (int)d_width)
		{
			width -= d_width;
		}
	}

	if (d <= 256)
	{
		// write leading 0's
		written += width;
		while (width--)
		{
			int res = ctx->_putchar(ctx->_that, L'0');
			if (res == EOF)
				return EOF;
		}

		if (d > 15)
		{
			int res = ctx->_putchar(ctx->_that, (int)kHexDigits[(d & 0xf0) >> 4]);
			if (res == EOF)
				return EOF;
			++written;
		}
		if (ctx->_putchar(ctx->_that, (int)kHexDigits[(d & 0xf)]) == EOF)
			return EOF;
		return written + 1;
	}

	// round padding down to nearest 2 since we always write pairs of digits below
	width &= ~1;
	written += width;
	while (width--)
	{
		if (ctx->_putchar(ctx->_that, L'0') == EOF)
			return EOF;
	}

	int high_idx = 0;
	long long dd = d;
	while (dd > 15)
	{
		dd >>= 4;
		++high_idx;
	}
	// convert to byte offset
	high_idx >>= 1;
	// read from MSD to LSD
	const char* chars = (const char*)(&d) + high_idx;
	do
	{
		//NOTE: this will always "pad" the output to an even number of nybbles
		size_t lo = *chars & 0x0f;
		size_t hi = (*chars & 0xf0) >> 4;
		int res = ctx->_putchar(ctx->_that, (int)kHexDigits[hi]);
		res = res == EOF || ctx->_putchar(ctx->_that, (int)kHexDigits[lo]);
		if (res == EOF)
			return EOF;
		written += 2;
		--high_idx;
		--chars;
	} while (high_idx >= 0);

	return written;
}

static int printbin(ctx_t* ctx, unsigned long long d)
{
	int written = 0;
	unsigned long long dd = d;
	unsigned long long bc = 0;
	while (dd)
	{
		dd >>= 1;
		++bc;
	}
	while (bc)
	{
		unsigned long dc = d & (1ull << (bc - 1));
		dc >>= (bc - 1);
		if (ctx->_putchar(ctx->_that, (int)L'0' + (int)dc) == EOF)
			return EOF;
		--bc;
		++written;
	}
	return written;
}

int _vprint_impl(ctx_t* ctx, const wchar_t* __restrict format, va_list parameters)
{
	int written = 0;

	while (*format != L'\0') {
		const size_t maxrem = INT_MAX - written;
		if (!maxrem) {
			// TODO: Set errno to EOVERFLOW.
			return -1;
		}
		if (format[0] != L'%' || format[1] == L'%')
		{
			if (format[0] == L'%')
			{
				format++;
			}
			size_t amount = 1;
			while (format[amount] && format[amount] != L'%')
			{
				amount++;
			}
			if (maxrem < amount)
			{
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			int result = ctx->_wprint(ctx->_that, format, amount);
			if (result == EOF)
			{
				return EOF;
			}
			format += (size_t)amount;
			written += result;
		}

		if (format[0])
		{
			const wchar_t* format_begun_at = format++;

			//TODO: parsing, but ignoring
			char flag = 0;
			if (format[0] == L'+' || format[0] == L'-' || format[0] == L' ' || format[0] == L'0' || format[0] == L'#')
			{
				flag = *format++;
			}
			(void)flag;

			// width modifier
			int width = 0;
			int width_digits = 0;
			int pow10 = 1;
			while (format[width_digits] >= L'0' && format[width_digits] <= L'9')
			{
				++width_digits;
				pow10 *= 10;
			}
			if (width_digits)
			{
				pow10 /= 10;
				while (format[0] >= L'0' && format[0] <= L'9')
				{
					width += (*format - L'0') * pow10;
					pow10 /= 10;
					++format;
				}
			}
			//TODO: parsing, but ignoring
			int precision = 0;
			if (format[0] == L'.')
			{
				++format;
				pow10 = 1;
				while (format[0] >= L'0' && format[0] <= L'9')
				{
					precision += (*format - L'0') * pow10;
					pow10 *= 10;
					++format;
				}
			}

			//TODO: parsed, but only l is handled
			char length[2] = { 0,0 };
			if (format[0] == L'h' || format[0] == L'l' || format[0] == L'L' || format[0] == L'z' || format[0] == L'j' || format[0] == L't')
			{
				length[0] = *format++;
				if (format[0] == L'h' || format[0] == L'l')
					length[1] = *format++;
			}

			wchar_t type = *format++;

			switch (type)
			{
			case L'c':
			{
				wchar_t c = (wchar_t)va_arg(parameters, int);
				int result = ctx->_wprint(ctx->_that, &c, 1);
				if (result == EOF)
					return EOF;
				written++;
			}
			break;
			case L's':
			{
				const char* str = va_arg(parameters, const char*);
				//NOTE: this is *not* standard, supporting a width modifier for %s
				size_t len = width ? (size_t)width : strlen(str);
				if (maxrem < len)
				{
					// TODO: Set errno to EOVERFLOW.
					return EOF;
				}
				if (len)
				{
					int result = ctx->_print(ctx->_that, str, len);
					if (result == EOF)
					{
						// TODO: Set errno to EOVERFLOW.
						return EOF;
					}
					written += result;
				}
			}
			break;
			case L'S':
			{
				const wchar_t* str = va_arg(parameters, const wchar_t*);
				//NOTE: this is *not* standard, supporting a width modifier for %s
				size_t len = width ? (size_t)width : wcslen(str);
				if (maxrem < len)
				{
					// TODO: Set errno to EOVERFLOW.
					return EOF;
				}
				if (len)
				{
					int result = ctx->_wprint(ctx->_that, str, len);
					if (result == EOF)
					{
						// TODO: Set errno to EOVERFLOW.
						return EOF;
					}
					written += result;
				}
			}
			break;
			case L'd':
			case L'i':
			{
				int res;
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						long long d = va_arg(parameters, long long);
						res = printdecimal(ctx, d, 0);
					}
					else
					{
						long d = va_arg(parameters, long);
						res = printdecimal(ctx, d, 0);
					}
				}
				else
				{
					int d = va_arg(parameters, int);
					res = printdecimal(ctx, d, 0);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			case L'u':
			{
				int res;
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						res = printdecimal(ctx, d, 1);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						res = printdecimal(ctx, d, 1);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					res = printdecimal(ctx, d, 1);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			case L'x':
			{
				int res;
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						res = printhex(ctx, width, d);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						res = printhex(ctx, width, d);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					res = printhex(ctx, width, d);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			// -----------------------------------------
			// NOT STANDARD
			case L'b':
			{
				int res;
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						res = printbin(ctx, d);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						res = printbin(ctx, d);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					res = printbin(ctx, d);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			// -----------------------------------------
			case L'f':
			{
				const float f = (float)va_arg(parameters, double);
				int integral_part = (int)f;
				float fractional_part = f - (float)integral_part;
				int res = printdecimal(ctx, (long long)integral_part, 0);
				if (res == EOF)
					return EOF;
				written += res;
				if (fractional_part != 0.0f && precision)
				{
					static wchar_t c = L'.';
					res = ctx->_wprint(ctx->_that, &c, 1);
					if (res == EOF)
						return EOF;
					++written;
					int prec = precision;
					do
					{
						fractional_part *= 10.0f;
						res = printdecimal(ctx, (long long)fractional_part, 0);
						if (res == EOF)
							return EOF;
						written += res;
						fractional_part -= (float)((int)fractional_part);
						//TODO: we need to use machine rounding mode for precision						
					} while (fractional_part != 0.0f && --prec);
				}
			}
			break;
			default:
			{
				format = format_begun_at;
				size_t len = wcslen(format);
				if (maxrem < len) {
					// TODO: Set errno to EOVERFLOW.
					return EOF;
				}
				int res = ctx->_wprint(ctx->_that, format, len);
				if (res == EOF)
					return EOF;
				written += res;
				format += len;
			}
			break;
			}
		}
	}

	if (ctx->_flush)
		ctx->_flush(ctx->_that);

	return written;
}

// ====================================================================================================================

typedef struct buffer_ctx_struct
{
	// current writing position
	wchar_t* _wp;
	// points to position of the last valid nibble in the buffer (including 0 terminator)
	const wchar_t* _end;
} buffer_t;

static size_t buffer_characters_left(buffer_t* buffer)
{
	return ((size_t)buffer->_end - (size_t)buffer->_wp) / sizeof(wchar_t);
}

static int buffer_putchar(void* ctx_, int c)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	const size_t rem_chars = buffer_characters_left(ctx);
	if (rem_chars == 1)
	{
		return EOF;
	}
	*ctx->_wp++ = (wchar_t)c;
	return 1;
}

static int buffer_wprint(void* ctx_, const wchar_t* data, size_t length)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	const size_t rem_chars = buffer_characters_left(ctx);
	if (rem_chars < length + 1)
	{
		if (rem_chars == 1)
		{
			return EOF;
		}
		length = rem_chars - 1;
	}
	int written = 0;
	for (unsigned i = 0; i < length; ++i)
	{
		wchar_t wc = data[i];
		switch (wc)
		{
		case L'\t':
		{
			if ((rem_chars-length) < 5)
			{
				// no more space
				return EOF;
			}
			// expand tabs to four spaces because It Is The Law
			static const wchar_t kTab[4] = {L' ',L' ',L' ',L' '};
			memcpy(ctx->_wp, kTab, sizeof(kTab));
			ctx->_wp += 4;
			written+=4;
		}
		break;
		//TODO: more of these, if we can be bothered...
#define _JOS_ESCAPED_CHAR(ec,c)\
		case ec:\
			if((rem_chars-length) < 2)\
			{\
				return EOF;\
			}\
			*ctx->_wp++ = c;\
			++written;\
			break
		_JOS_ESCAPED_CHAR(L'\"','"');
		default:
			*ctx->_wp++ = wc;
			++written;
			break;
		}
	}
	return written;
}

static int buffer_print(void* ctx_, const char* data, size_t length)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	const size_t rem_chars = buffer_characters_left(ctx);
	if (rem_chars < length + 1)
	{
		if (rem_chars == 1)
		{
			return EOF;
		}
		length = rem_chars - 1;
	}
	for (unsigned i = 0; i < length; ++i)
	{
		wchar_t wc = (wchar_t)data[i];
		*ctx->_wp++ = wc;
	}
	return (int)length;
}

static int buffer_putchar_count(void* ctx_, int c)
{
	(void)ctx_;
	(void)c;
	return 1;
}

static int buffer_wprint_count(void* ctx_, const wchar_t* data, size_t length)
{
	(void)ctx_;
	(void)data;
	return (int)length;
}

static int buffer_print_count(void* ctx_, const char* data, size_t length)
{
	(void)ctx_;
	(void)data;
	return (int)length;
}

int _JOS_LIBC_FUNC_NAME(swprintf) (wchar_t* buffer, size_t bufsz, const wchar_t* format, ...)
{
	if (!buffer || !format || !format[0])
		return 0;

	va_list parameters;
	va_start(parameters, format);
	int written = _vprint_impl(&(ctx_t) {
		._print = (bufsz ? buffer_print : buffer_print_count),
			._wprint = (bufsz ? buffer_wprint : buffer_wprint_count),
			._putchar = (bufsz ? buffer_putchar : buffer_putchar_count),
			._that = (void*)&(buffer_t) { ._wp = buffer, ._end = buffer + bufsz }
	},
		format, parameters);
	buffer[written] = 0;
	va_end(parameters);
	return written;
}

int _JOS_LIBC_FUNC_NAME(vswprintf)(wchar_t* __restrict buffer, size_t bufsz, const wchar_t* __restrict format, va_list parameters)
{
	if (!buffer || !format || !format[0])
		return 0;

	int written = _vprint_impl(&(ctx_t) {
		._print = (bufsz ? buffer_print : buffer_print_count),
			._wprint = (bufsz ? buffer_wprint : buffer_wprint_count),
			._putchar = (bufsz ? buffer_putchar : buffer_putchar_count),
			._that = (void*)&(buffer_t) { ._wp = buffer, ._end = buffer + bufsz }
	},
		format, parameters);
	buffer[written] = 0;
	return written;
}

