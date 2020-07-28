#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
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
		ctx->_putchar(ctx->_that, (int)L'0');
		return 1;
	}

	if (d < 0)
	{
		if (!un_signed)
		{
			ctx->_putchar(ctx->_that, (int)L'-');
			++written;
		}
		d *= -1;
	}
	// simple and dumb but it works...
	int pow_10 = 1;
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
		ctx->_putchar(ctx->_that, (int)L'0' + (int)dd);
		++written;
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
		if(!d)
		{
			// all zeros; just output them
			written += width;
			while (width--)
			{
				ctx->_putchar(ctx->_that, (int)L'0');
			}
			return written;
		}

#ifdef _JOS_KERNEL_BUILD
		unsigned long d_width;
		(d_width = __builtin_clzll(d), ++d_width);
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
			ctx->_putchar(ctx->_that, L'0');
		}
		
		if (d > 15)
		{
			ctx->_putchar(ctx->_that, (int)kHexDigits[(d & 0xf0) >> 4]);
			++written;
		}
		ctx->_putchar(ctx->_that, (int)kHexDigits[(d & 0xf)]);
		return written+1;
	}

	// round padding down to nearest 2 since we always write pairs of digits below
	width &= ~1;
	written += width;
	while (width--)
	{
		ctx->_putchar(ctx->_that, L'0');
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
		ctx->_putchar(ctx->_that, (int)kHexDigits[hi]);
		ctx->_putchar(ctx->_that, (int)kHexDigits[lo]);
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
		ctx->_putchar(ctx->_that, (int)L'0' + (int)dc);
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
			if (!ctx->_wprint(ctx->_that, format, amount))
			{
				return -1;
			}
			format += amount;
			written += (int)amount;
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
			if(width_digits)
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
				ctx->_wprint(ctx->_that, &c, 1);
				written++;
			}
			break;
			case L's':
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
					if ( ctx->_wprint(ctx->_that, str, len) == EOF )
					{
						// TODO: Set errno to EOVERFLOW.
						return EOF;
					}
					written += len;
				}
			}
			break;
			case L'd':
			case L'i':
			{
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						long long d = va_arg(parameters, long long);
						written += printdecimal(ctx, d, 0);
					}
					else
					{
						long d = va_arg(parameters, long);
						written += printdecimal(ctx, d, 0);
					}
				}
				else
				{
					int d = va_arg(parameters, int);
					written += printdecimal(ctx, d, 0);
				}
			}
			break;
			case L'u':
			{
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						written += printdecimal(ctx, d, 1);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						written += printdecimal(ctx, d, 1);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					written += printdecimal(ctx, d, 1);
				}
			}
			break;
			case L'x':
			{
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						written += printhex(ctx, width, d);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						written += printhex(ctx, width, d);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					written += printhex(ctx, width, d);
				}
			}
			break;
			// -----------------------------------------
			// NOT STANDARD
			case L'b':
			{
				if (length[0] == L'l')
				{
					if (length[1] == L'l')
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						written += printbin(ctx, d);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						written += printbin(ctx, d);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					written += printbin(ctx, d);
				}
			}
			break;
			// -----------------------------------------
			case L'f':
			{
				const float f = (float)va_arg(parameters, double);
				int integral_part = (int)f;
				float fractional_part = f - (float)integral_part;
				written += printdecimal(ctx, (long long)integral_part, 0);
				if(fractional_part!=0.0f && precision)
				{
					static wchar_t c = L'.';
					ctx->_wprint(ctx->_that, &c, 1);

					int prec = precision;
					do
					{
						fractional_part *= 10.0f;
						written += printdecimal(ctx, (long long)fractional_part, 0);
						fractional_part -= (float)((int)fractional_part);
						
						//TODO: we need to use machine rounding mode for precision						
					}
					while(fractional_part!=0.0f && --prec);
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
				ctx->_wprint(ctx->_that, format, len);
				written += len;
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
	const wchar_t*	_end;
	// points to the last permitted position in the buffer (including 0 terminator), may be < _end
	const wchar_t*	_n_end;
} buffer_t;

static size_t buffer_characters_left(buffer_t* buffer)
{
	return ((size_t)buffer->_n_end - (size_t)buffer->_wp)/sizeof(wchar_t);
}

static bool buffer_has_n_semtantics(buffer_t* buffer)
{
	return buffer->_n_end != buffer->_end;
}

static int buffer_putchar(void* ctx_, int c)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	const size_t rem_chars = buffer_characters_left(ctx);
	if ( rem_chars==1 )
	{
		if(!buffer_has_n_semtantics(ctx))
			return EOF;
		return 1;
	}
	*ctx->_wp++ = (wchar_t)c;
	return 1;
}

static int buffer_wprint(void* ctx_, const wchar_t* data, size_t length)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	const size_t rem_chars = buffer_characters_left(ctx);
	if ( rem_chars < length+1 )
	{
		if(!buffer_has_n_semtantics(ctx))
			return EOF;
		if ( rem_chars==1 )
		{
			return 0;
		}
		length = rem_chars-1;
	}
	
	memcpy(ctx->_wp, data, length*sizeof(wchar_t));
	ctx->_wp+=length;
	
	return (int)length;
}

int _JOS_LIBC_FUNC_NAME(swprintf_s)(wchar_t* __restrict buffer, size_t buffercount, const wchar_t* __restrict format, ...)
{
	if (!buffer || !format || !format[0])
	{
		return 0;
	}

	const wchar_t* end = buffer + buffercount;
	va_list parameters;
	va_start(parameters, format);
	int written = _vprint_impl(&(ctx_t) {
		._wprint = buffer_wprint,
			._putchar = buffer_putchar,
			._that = (void*) & (buffer_t) { ._wp = buffer, ._end = end, ._n_end = end }
	},
		format, parameters);
	buffer[written] = 0;
	va_end(parameters);
	return written;
}

static int buffer_n_putchar(void* ctx_, int c)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	if (ctx->_wp != ctx->_end)
	{
		*ctx->_wp++ = (wchar_t)c;
		return 1;
	}
	return EOF;
}

static int buffer_n_print(void* ctx_, const wchar_t* data, size_t length)
{
	buffer_t* ctx = (buffer_t*)ctx_;
	const wchar_t* chars = (const wchar_t*)data;
	size_t rem_chars = (size_t)ctx->_end - (size_t)ctx->_wp;
	if(!rem_chars)
		return EOF;
	length = length < rem_chars ? length : rem_chars;	
	memcpy(ctx->_wp, chars, length*sizeof(wchar_t));
	ctx->_wp += length;
	return (int)length;
}

int _JOS_LIBC_FUNC_NAME(snwprintf_s) (wchar_t* buffer, size_t buffercount, size_t n, const wchar_t* format, ...)
{
	if (!buffer || !n || !format || !format[0])
		return 0;

	va_list parameters;
	va_start(parameters, format);
	int written = _vprint_impl(&(ctx_t) {
			._wprint = buffer_wprint,
			._putchar = buffer_putchar,
			._that = (void*) & (buffer_t) { ._wp = buffer, ._end = buffer + buffercount, ._n_end = buffer + n }
	},
		format, parameters);
	written = written < (int)n ? written : (int)n - 1;
	buffer[written] = 0;
	va_end(parameters);
	return written;
}
#if 0
int _JOS_LIBC_FUNC_NAME(vsnprintf)(char* buffer, size_t n, const char* format, va_list parameters)
{
	if (!buffer || !n || !format || !format[0])
		return 0;

	int written = _vprint_impl(&(ctx_t) {
		._print = buffer_n_print,
			._putchar = buffer_n_putchar,
			._that = (void*) & (buffer_t) { ._wp = buffer, ._end = buffer + (n - 1) }
	},
		format, parameters);
	buffer[written < (int)n ? written : (int)n - 1] = 0;
	return written;
}
#endif
