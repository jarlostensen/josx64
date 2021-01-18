#pragma once

void trace_buf(const char* __restrict channel, const void* __restrict data, size_t length);
void trace(const char* __restrict channel, const char* __restrict format,...);
