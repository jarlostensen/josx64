#include <string.h>
#include <stdint.h>

// https://github.com/nadavrot/memset_benchmark/tree/main/src

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {

	uint32_t* dst = (uint32_t*) dstptr;
	size_t rem = size & 3;
	size >>= 2;
	uint32_t* src = (uint32_t*)srcptr;
	for(size_t n = 0; n < size; ++n)
		*dst++ = *src++;
	unsigned char* cdst = (unsigned char*) dst;
	const unsigned char* csrc = (const unsigned char*) src;
	while(rem--)
		*cdst++ = *csrc++;
	return dstptr;
}
