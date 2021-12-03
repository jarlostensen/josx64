#include <string.h>
#include <stdint.h>

// https://github.com/nadavrot/memset_benchmark/tree/main/src

void* memset(void* bufptr, int value, size_t size) {
	uint32_t v32 = (value * 0x01010101);
	size_t s = size >> 3;
	uint32_t* ptr32 = (uint32_t*)bufptr;
	while(s--)
	{
		*ptr32++ = v32;
		*ptr32++ = v32;
	}

	s = (size & 7)>>2;
	while(s--)
	{
		*ptr32++ = v32;
	}

	s = size & 3;
	unsigned char* cbuf = (unsigned char*)ptr32;
	while(s--)
	{
		*cbuf++ = (unsigned char)value;
	}

	return bufptr;
}
