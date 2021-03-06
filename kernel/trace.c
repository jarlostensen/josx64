
#include <jos.h>
#include <serial.h>
#include <trace.h>
#include <stdio.h>
#include <string.h>

static long long _ticks = 0;

void trace_buf(const char* __restrict channel, const void* __restrict data, size_t length) {

    if(!data || !length)
        return;
    char buffer[512];
	int written;
	if(channel)
		written = snprintf(buffer, sizeof(buffer), "[%lld:%s] ", _ticks++, channel);
	else
		written = snprintf(buffer, sizeof(buffer), "[%lld] ", _ticks++);

    length = length < (sizeof(buffer)-written-2) ? length:(sizeof(buffer)-written-2);
    memcpy(buffer+written, data, length);
    
    buffer[written+0] = '\r';
    buffer[written+1] = '\n';
    buffer[written+2] = 0;

    serial_write(kCom1, buffer, written+length+3);
}

void trace(const char* __restrict channel, const char* __restrict format,...) {

    if(!format || !format[0])
        return;
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

    serial_write(kCom1, buffer, written+3);
}
