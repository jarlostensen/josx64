
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../../libc/libc_internal.h"
#ifdef _JOS_KERNEL_BUILD
#include "kernel_detail.h"
#include "serial.h"
#include <kernel/clock.h>
#else
// just a dummy for testing
static __int64 k_clock_ms_since_boot(void)
{
	static __int64 tick = 0;
	return tick++;
}
static __int64 k_ticks_since_boot(void)
{
	return k_clock_ms_since_boot();
}
#endif

void _k_trace_buf(const char* __restrict channel, const void* data, size_t length)
{
    if(!data || !length)
        return;
    char buffer[512];
	int written;
	if(channel)
		written = _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "[%lld:%s] ", k_ticks_since_boot(), channel);
	else
		written = _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "[%lld] ", k_ticks_since_boot());
    length = length < (sizeof(buffer)-written-2) ? length:(sizeof(buffer)-written-2);
    memcpy(buffer+written, data, length);
    buffer[written+length] ='\n';
    buffer[written+length+1] =0;
#ifdef _JOS_KERNEL_BUILD
    k_serial_write(kCom1, buffer, written+length+1);
#else
	printf(buffer);
#endif
}

void _k_trace(const char* __restrict channel, const char* __restrict format,...)
{
    if(!format || !format[0])
        return;
    char buffer[256];
    va_list parameters;
    va_start(parameters, format);
	int written;
	if(channel)
		written = _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "[%lld:%s] ", k_ticks_since_boot(), channel);
	else
		written = _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "[%lld] ", k_ticks_since_boot());
    written += _JOS_LIBC_FUNC_NAME(vsnprintf)(buffer+written, sizeof(buffer)-written, format, parameters);
    va_end(parameters);
#ifdef _JOS_KERNEL_BUILD
    k_serial_write(kCom1, buffer, written);
#else
	printf(buffer);
#endif
}