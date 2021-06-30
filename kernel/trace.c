
#include <jos.h>
#include <serial.h>
#include <trace.h>
#include <debugger.h>
#include <stdio.h>
#include <string.h>

void trace_buf(const char* __restrict channel, const void* __restrict data, size_t length) {

    if(!data || !length)
        return;
    char buffer[1024];
	int written;
	if(channel)
		written = snprintf(buffer, sizeof(buffer), "[%s] ", channel);
	else
		written = snprintf(buffer, sizeof(buffer), "[.] ");

    length = length < (sizeof(buffer)-written-2) ? length:(sizeof(buffer)-written-2);
    memcpy(buffer+written, data, length);
    
    if (!debugger_is_connected()) {    
        buffer[written+0] = '\r';
        buffer[written+1] = '\n';
        buffer[written+2] = 0;
        serial_write(kCom1, buffer, written+length+3);
    }
    else {
        debugger_send_packet(kDebuggerPacket_Trace, buffer, written+length);
    }
}

void trace(const char* __restrict channel, const char* __restrict format,...) {

    if(!format || !format[0])
        return;
    //ZZZ:
    char buffer[1024];
    va_list parameters;
    va_start(parameters, format);
	int written;
	if(channel)
		written = snprintf(buffer, sizeof(buffer), "[%s] ", channel);
	else
		written = snprintf(buffer, sizeof(buffer), "[.] ");
    written += vsnprintf(buffer+written, sizeof(buffer)-written, format, parameters);
    va_end(parameters);
    
    if (!debugger_is_connected()) {        
        buffer[written+0] = '\r';
        buffer[written+1] = '\n';
        buffer[written+2] = 0;
        serial_write(kCom1, buffer, written+3);
    }
    else {
        debugger_send_packet(kDebuggerPacket_Trace, buffer, written);
    }
}
