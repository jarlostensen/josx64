
////////////////////////////////////////////////////////////////
// NOTE: this file is included by _vprint.c
// it contains code that is part macro unrolled and part hand rolled
// to generate wchar and char versions of various stdio string routines
// 
// Here be dragons.
//

#undef CHAR
#undef DUAL_CHAR
#undef __TEXT
#undef WA_NAME
#undef DUAL_WA_NAME
#undef STRLEN
#undef DUAL_STRLEN
#undef A_NAME
#undef W_NAME
#undef WA

#ifdef WIDECHAR
#define WA w
#define CHAR wchar_t
#define DUAL_CHAR char
#define STRLEN wcslen
#define DUAL_STRLEN strlen
#define __TEXT(s) L##s
#define WA_NAME(name) name##_w
#define DUAL_WA_NAME(name) name##_a
#define W_NAME(name) WA_NAME(name)
#define A_NAME(name) DUAL_WA_NAME(name)
#else
#define WA a
#undef CHAR
#define CHAR char
#define DUAL_CHAR wchar_t
#define STRLEN strlen
#define DUAL_STRLEN wcslen
#define __TEXT(s) s
#define WA_NAME(name) name##_a
#define DUAL_WA_NAME(name) name##_w
#define A_NAME(name) WA_NAME(name)
#define W_NAME(name) DUAL_WA_NAME(name)
#endif
#define TEXT(s) __TEXT(s)


static int WA_NAME(printdecimal)(_vprint_ctx_t* ctx, long long d, int un_signed)
{
	int written = 0;
	if (!d)
	{
		return ctx->_putchar(ctx->_that, (int)TEXT('0'));
	}

	if (d < 0)
	{
		if (!un_signed)
		{
			int res = ctx->_putchar(ctx->_that, (int)TEXT('-'));
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
		int res = ctx->_putchar(ctx->_that, (int)TEXT('0') + (int)dd);
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

static int WA_NAME(printhex)(_vprint_ctx_t* ctx, int width, long long d)
{
	static const CHAR* kHexDigits = TEXT("0123456789abcdef");
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
				int res = ctx->_putchar(ctx->_that, (int)TEXT('0'));
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
			int res = ctx->_putchar(ctx->_that, TEXT('0'));
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
		if (ctx->_putchar(ctx->_that, TEXT('0')) == EOF)
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

static int WA_NAME(printbin)(_vprint_ctx_t* ctx, unsigned long long d)
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
		if (ctx->_putchar(ctx->_that, (int)TEXT('0' + (int)dc)) == EOF)
			return EOF;
		--bc;
		++written;
	}
	return written;
}

int WA_NAME(_vprint_impl)(_vprint_ctx_t* ctx, const CHAR* __restrict format, va_list parameters)
{
	int written = 0;

	while (*format != TEXT('\0')) {
		const size_t maxrem = INT_MAX - written;
		if (!maxrem) {
			// TODO: Set errno to EOVERFLOW.
			return -1;
		}
		if (format[0] != TEXT('%') || format[1] == TEXT('%'))
		{
			if (format[0] == TEXT('%'))
			{
				format++;
			}
			size_t amount = 1;
			while (format[amount] && format[amount] != TEXT('%'))
			{
				amount++;
			}
			if (maxrem < amount)
			{
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			int result = ctx->WA_NAME(_print)(ctx->_that, format, amount);
			if (result == EOF)
			{
				return EOF;
			}
			format += (size_t)amount;
			written += result;
		}

		if (format[0])
		{
			const CHAR* format_begun_at = format++;

			//TODO: parsing, but ignoring
			char flag = 0;
			if (format[0] == TEXT('+') || format[0] == TEXT('-') || format[0] == TEXT(' ') || format[0] == TEXT('0') || format[0] == TEXT('#'))
			{
				flag = *format++;
			}
			(void)flag;

			// width modifier
			int width = 0;
			int width_digits = 0;
			int pow10 = 1;
			while (format[width_digits] >= TEXT('0') && format[width_digits] <= TEXT('9'))
			{
				++width_digits;
				pow10 *= 10;
			}
			if (width_digits)
			{
				pow10 /= 10;
				while (format[0] >= TEXT('0') && format[0] <= TEXT('9'))
				{
					width += (*format - TEXT('0')) * pow10;
					pow10 /= 10;
					++format;
				}
			}
			//TODO: parsing, but ignoring
			int precision = 0;
			if (format[0] == TEXT('.'))
			{
				++format;
				pow10 = 1;
				while (format[0] >= TEXT('0') && format[0] <= TEXT('9'))
				{
					precision += (*format - TEXT('0')) * pow10;
					pow10 *= 10;
					++format;
				}
			}

			//TODO: parsed, but only l is handled
			char length[2] = { 0,0 };
			if (format[0] == TEXT('h') || format[0] == TEXT('l') || format[0] == TEXT('L') || format[0] == TEXT('z') || format[0] == TEXT('j') || format[0] == TEXT('t'))
			{
				length[0] = *format++;
				if (format[0] == TEXT('h') || format[0] == TEXT('l'))
					length[1] = *format++;
			}

			CHAR type = *format++;

			switch (type)
			{
			case TEXT('c'):
			{
				CHAR c = (CHAR)va_arg(parameters, int);
				int result = ctx->WA_NAME(_print)(ctx->_that, &c, 1);
				if (result == EOF)
					return EOF;
				written++;
			}
			break;
			case TEXT('s'):
			{
				const CHAR* str = va_arg(parameters, const CHAR*);
				//NOTE: this is *not* standard, supporting a width modifier for %s
				size_t len = width ? (size_t)width : STRLEN(str);
				if (maxrem < len)
				{
					// TODO: Set errno to EOVERFLOW.
					return EOF;
				}
				if (len)
				{
					int result = ctx->WA_NAME(_print)(ctx->_that, str, len);
					if (result == EOF)
					{
						// TODO: Set errno to EOVERFLOW.
						return EOF;
					}
					written += result;
				}
			}
			break;
			case TEXT('S'):
			{
				const DUAL_CHAR* str = va_arg(parameters, const DUAL_CHAR*);
				//NOTE: this is *not* standard, supporting a width modifier for %s
				size_t len = width ? (size_t)width : DUAL_STRLEN(str);
				if (maxrem < len)
				{
					// TODO: Set errno to EOVERFLOW.
					return EOF;
				}
				if (len)
				{
					int result = ctx->DUAL_WA_NAME(_print_convert)(ctx->_that, str, len);
					if (result == EOF)
					{
						// TODO: Set errno to EOVERFLOW.
						return EOF;
					}
					written += result;
				}
			}
			break;
			case TEXT('d'):
			case TEXT('i'):
			{
				int res;
				if (length[0] == TEXT('l'))
				{
					if (length[1] == TEXT('l'))
					{
						long long d = va_arg(parameters, long long);
						res = WA_NAME(printdecimal)(ctx, d, 0);
					}
					else
					{
						long d = va_arg(parameters, long);
						res = WA_NAME(printdecimal)(ctx, d, 0);
					}
				}
				else
				{
					int d = va_arg(parameters, int);
					res = WA_NAME(printdecimal)(ctx, d, 0);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			case TEXT('u'):
			{
				int res;
				if (length[0] == TEXT('l'))
				{
					if (length[1] == TEXT('l'))
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						res = WA_NAME(printdecimal)(ctx, d, 1);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						res = WA_NAME(printdecimal)(ctx, d, 1);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					res = WA_NAME(printdecimal)(ctx, d, 1);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			case TEXT('x'):
			{
				int res;
				if (length[0] == TEXT('l'))
				{
					if (length[1] == TEXT('l'))
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						res = WA_NAME(printhex)(ctx, width, d);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						res = WA_NAME(printhex)(ctx, width, d);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					res = WA_NAME(printhex)(ctx, width, d);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			// -----------------------------------------
			// NOT STANDARD
			case TEXT('b'):
			{
				int res;
				if (length[0] == TEXT('l'))
				{
					if (length[1] == TEXT('l'))
					{
						unsigned long long d = va_arg(parameters, unsigned long long);
						res = WA_NAME(printbin)(ctx, d);
					}
					else
					{
						unsigned long d = va_arg(parameters, unsigned long);
						res = WA_NAME(printbin)(ctx, d);
					}
				}
				else
				{
					unsigned int d = va_arg(parameters, unsigned int);
					res = WA_NAME(printbin)(ctx, d);
				}
				if (res == EOF)
					return EOF;
				written += res;
			}
			break;
			// -----------------------------------------
			case TEXT('f'):
			{
				const float f = (float)va_arg(parameters, double);
				int integral_part = (int)f;
				float fractional_part = f - (float)integral_part;
				int res = WA_NAME(printdecimal)(ctx, (long long)integral_part, 0);
				if (res == EOF)
					return EOF;
				written += res;
				if (fractional_part != 0.0f && precision)
				{
					static CHAR c = TEXT('.');
					res = ctx->WA_NAME(_print)(ctx->_that, &c, 1);
					if (res == EOF)
						return EOF;
					++written;
					int prec = precision;
					do
					{
						fractional_part *= 10.0f;
						res = WA_NAME(printdecimal)(ctx, (long long)fractional_part, 0);
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
				size_t len = STRLEN(format);
				if (maxrem < len) {
					// TODO: Set errno to EOVERFLOW.
					return EOF;
				}
				int res = ctx->WA_NAME(_print)(ctx->_that, format, len);
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

#ifndef _BUFFER_HELPERS_DEFINED

typedef struct _buffer_a
{
	char* _wp;
	const char* _end;
} buffer_t_a;

typedef struct _buffer_w
{
	wchar_t* _wp;
	const wchar_t* _end;
} buffer_t_w;

//NOTE: this is clunky but there isn't a nice way to wrap this in macros (and macros are rubbish for debugging anyway)

static size_t buffer_characters_left_a(buffer_t_a* buffer)
{
	return ((size_t)buffer->_end - (size_t)buffer->_wp) / sizeof(char);
}
static size_t buffer_characters_left_w(buffer_t_w* buffer)
{
	return ((size_t)buffer->_end - (size_t)buffer->_wp) / sizeof(wchar_t);
}

static int buffer_putchar_a(void* ctx_, int c)
{
	buffer_t_a* ctx = (buffer_t_a*)ctx_;
	const size_t rem_chars = buffer_characters_left_a(ctx);
	if (rem_chars == 1)
	{
		return EOF;
	}
	*ctx->_wp++ = (char)c;
	return 1;
}
static int buffer_putchar_w(void* ctx_, int c)
{
	buffer_t_w* ctx = (buffer_t_w*)ctx_;
	const size_t rem_chars = buffer_characters_left_w(ctx);
	if (rem_chars == 1)
	{
		return EOF;
	}
	*ctx->_wp++ = (wchar_t)c;
	return 1;
}

static int buffer_print_a(void* ctx_, const char* data, size_t length)
{
	buffer_t_a* ctx = (buffer_t_a*)ctx_;
	const size_t rem_chars = buffer_characters_left_a(ctx);
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
		char wc = data[i];
		switch (wc)
		{
		case '\t':
		{
			if ((rem_chars-length) < 5)
			{
				// no more space
				return EOF;
			}
			// expand tabs to four spaces because It Is The Law
			static const char kTab[4] = {' ',' ',' ',' '};
			memcpy(ctx->_wp, kTab, sizeof(kTab));
			ctx->_wp += 4;
			written+=4;
		}
		break;
		//TODO: more of these, if we can be bothered...
#define _JOS_ESCAPED_CHAR_A(ec,c)\
		case ec:\
			if((rem_chars-length) < 2)\
			{\
				return EOF;\
			}\
			*ctx->_wp++ = c;\
			++written;\
			break
		_JOS_ESCAPED_CHAR_A('\"','"');
		default:
			*ctx->_wp++ = wc;
			++written;
			break;
		}
	}
	return written;
}
//NOTE: the ackward naming is so that we can use the DUAL_WA_NAME macro below...
static int buffer_print_a_to_w(void* ctx_, const char* data, size_t length)
{
	buffer_t_w* ctx = (buffer_t_w*)ctx_;
	const size_t rem_chars = buffer_characters_left_w(ctx);
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
		char wc = data[i];
		switch (wc)
		{
		case '\t':
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
#define _JOS_ESCAPED_CHAR_A_TO_W(ec,c)\
		case ec:\
			if((rem_chars-length) < 2)\
			{\
				return EOF;\
			}\
			*ctx->_wp++ = c;\
			++written;\
			break
        _JOS_ESCAPED_CHAR_A_TO_W('\"',L'"');
		default:
			*ctx->_wp++ = wc;
			++written;
			break;
		}
	}
	return written;
}
static int buffer_print_w(void* ctx_, const wchar_t* data, size_t length)
{
	buffer_t_w* ctx = (buffer_t_w*)ctx_;
	const size_t rem_chars = buffer_characters_left_w(ctx);
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
#define _JOS_ESCAPED_CHAR_W(ec,c)\
		case ec:\
			if((rem_chars-length) < 2)\
			{\
				return EOF;\
			}\
			*ctx->_wp++ = c;\
			++written;\
			break
		_JOS_ESCAPED_CHAR_W(L'\"',L'"');
		default:
			*ctx->_wp++ = wc;
			++written;
			break;
		}
	}
	return written;
}
static int buffer_print_w_to_a(void* ctx_, const wchar_t* data, size_t length)
{
	buffer_t_a* ctx = (buffer_t_a*)ctx_;
	const size_t rem_chars = buffer_characters_left_a(ctx);
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
		case '\t':
		{
			if ((rem_chars-length) < 5)
			{
				// no more space
				return EOF;
			}
			// expand tabs to four spaces because It Is The Law
			static const char kTab[4] = {' ',' ',' ',' '};
			memcpy(ctx->_wp, kTab, sizeof(kTab));
			ctx->_wp += 4;
			written+=4;
		}
		break;
		//TODO: more of these, if we can be bothered...
#define _JOS_ESCAPED_CHAR_W_TO_A(ec,c)\
		case ec:\
			if((rem_chars-length) < 2)\
			{\
				return EOF;\
			}\
			*ctx->_wp++ = c;\
			++written;\
			break
		_JOS_ESCAPED_CHAR_W_TO_A(L'\"','"');
		default:
			*ctx->_wp++ = wc;
			++written;
			break;
		}
	}
	return written;
}

static int buffer_print_count_a(void* ctx_, const char* data, size_t length)
{
	(void)ctx_;
	(void)data;
	return (int)length;
}
static int buffer_print_count_w(void* ctx_, const wchar_t* data, size_t length)
{
	(void)ctx_;
	(void)data;
	return (int)length;
}

static int buffer_putchar_count(void* ctx_, int c)
{
	(void)ctx_;
	(void)c;
	return 1;
}

#define _BUFFER_HELPERS_DEFINED
#endif

#undef SXPRINTF
#undef VSXPRINTF
#undef BUFFER_PRINT_CONVERT

#ifdef WIDECHAR
#define SXPRINTF	_JOS_LIBC_FUNC_NAME(swprintf)
#define VSXPRINTF	_JOS_LIBC_FUNC_NAME(vswprintf)
#define BUFFER_PRINT_CONVERT buffer_print_a_to_w
#else
#define SXPRINTF	_JOS_LIBC_FUNC_NAME(snprintf)
#define VSXPRINTF	_JOS_LIBC_FUNC_NAME(vsnprintf)
#define BUFFER_PRINT_CONVERT buffer_print_w_to_a
#endif

int VSXPRINTF(CHAR* __restrict buffer, size_t bufsz, const CHAR* __restrict format, va_list parameters)
{
	if (!buffer || !format || !format[0])
		return 0;

	int written = WA_NAME(_vprint_impl)(&(_vprint_ctx_t) {
		.WA_NAME(_print) = (bufsz ? WA_NAME(buffer_print) : WA_NAME(buffer_print_count)),
		.DUAL_WA_NAME(_print) = (bufsz ? DUAL_WA_NAME(buffer_print) : DUAL_WA_NAME(buffer_print_count)),
		.DUAL_WA_NAME(_print_convert) = (bufsz ? BUFFER_PRINT_CONVERT : DUAL_WA_NAME(buffer_print_count)),
		._putchar = (bufsz ? WA_NAME(buffer_putchar) : buffer_putchar_count),
		._that = (void*)&(WA_NAME(buffer_t)) { ._wp = buffer, ._end = buffer + bufsz }
	},
		format, parameters);
	buffer[written] = 0;
	return written;	
}

int SXPRINTF(CHAR* buffer, size_t bufsz, const CHAR* format, ...)
{
	if (!buffer || !format || !format[0])
		return 0;

	va_list parameters;
	va_start(parameters, format);
	int written = VSXPRINTF(buffer,bufsz,format,parameters);
	va_end(parameters);

	return written;
}
