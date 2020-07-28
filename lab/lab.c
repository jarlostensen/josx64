
// =========================================================================================

#include <stdint.h>
#include "../libc/internal/include/libc_internal.h"

extern int _JOS_LIBC_FUNC_NAME(swprintf_s)(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(snwprintf_s) (wchar_t* buffer, size_t buffercount, size_t n, const wchar_t* format, ...);


int main(void)
{
	wchar_t wbuffer[1024];
	//_JOS_LIBC_FUNC_NAME(swprintf_s)(wbuffer,sizeof(wbuffer)/sizeof(wchar_t),L"Hello wide %s, %3.2f this is 0x%x == %d, and \"%c\"", L"world", 3.1459f, 0xabcdef, 42, L'J');

	wbuffer[0] = 0;
	int written = _JOS_LIBC_FUNC_NAME(snwprintf_s)(wbuffer,sizeof(wbuffer)/sizeof(wchar_t), 16,L"Hello wide %s, %3.2f this is 0x%x == %d, and \"%c\"", L"world", 3.1459f, 0xabcdef, 42, L'J');
	
	return 0;
}