
// =========================================================================================

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../libc/internal/include/libc_internal.h"

extern int _JOS_LIBC_FUNC_NAME(swprintf)(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vswprintf)(wchar_t*__restrict buffer, size_t bufsz, const wchar_t* __restrict format, va_list vlist);
extern int _JOS_LIBC_FUNC_NAME(snprintf)(char* __restrict buffer, size_t sizeOfBuffer, const char* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vsnprintf)(char*__restrict buffer, size_t bufsz, const char* __restrict format, va_list vlist);

void test_a(char* buffer, const char* format, ...)
{
    va_list parameters;
	va_start(parameters, format);
    _JOS_LIBC_FUNC_NAME(vsnprintf)(buffer, 512, format, parameters);
    //vsnprintf(buffer, 512, format, parameters);
    va_end(parameters);
}

void test_w(wchar_t* buffer, const wchar_t* format, ...)
{
    va_list parameters;
	va_start(parameters, format);
    _JOS_LIBC_FUNC_NAME(vswprintf)(buffer, 512, format, parameters);
    //vswprintf(buffer, 512, format, parameters);
    va_end(parameters);
}

void trace(const char* __restrict channel, const char* __restrict format,...) {

    if(!format || !format[0])
        return;

    static unsigned long long _ticks = 0;

    //ZZZ:
    char buffer[256];
    va_list parameters;
    va_start(parameters, format);
	int written;
	if(channel)
		written = snprintf(buffer, sizeof(buffer), "[%lld:%s] ", _ticks++, channel);
	else
		written = snprintf(buffer, sizeof(buffer), "[%lld] ", _ticks++); 
    written += vsnprintf(buffer+written, sizeof(buffer)-written, format, parameters);
    va_end(parameters);

    buffer[written+0] = '\r';
    buffer[written+1] = '\n';
    buffer[written+2] = 0;

    printf(buffer);
}

int main(void)
{
    char buffer[512];
    wchar_t wbuffer[512];

    trace("lab","this is a message from the lab");
    trace("lab","and this is also a message from the lab");

    test_a(buffer, "%s %S", "this is a test", L"and this is wide");
    test_w(wbuffer, L"%s %S", L"this is w test ", "and this is narrow");

    _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S\n", 
                    1,
                    42,
                    80,
                    1,
                    0,
                    L"enabled"
                    );
		
	_JOS_LIBC_FUNC_NAME(swprintf)(wbuffer, sizeof(wbuffer)/sizeof(wchar_t), L"\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S\n", 
                    1,
                    42,
                    80,
                    1,
                    0,
                    "enabled"
                    );


	return 0;
}