
// =========================================================================================

#include <stdint.h>
#include <stdio.h>
#include <varargs.h>
#include "../libc/internal/include/libc_internal.h"

extern int _JOS_LIBC_FUNC_NAME(swprintf)(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vswprintf)(wchar_t *__restrict buffer, size_t bufsz, const wchar_t * __restrict format, va_list vlist);
extern size_t _JOS_LIBC_FUNC_NAME(wcslen)( const wchar_t *str);

int main(void)
{
	wchar_t wbuffer[1024];
	uint32_t type = 1;
	uint64_t phys = 0xaabbccddeeff;
	uint64_t pages = 9;
	int written = _JOS_LIBC_FUNC_NAME(swprintf)(wbuffer, sizeof(wbuffer)/sizeof(wchar_t), L"\t%llu Kbytes\n\r", phys);
	written = _JOS_LIBC_FUNC_NAME(swprintf)(wbuffer, sizeof(wbuffer)/sizeof(wchar_t), L"\ttype 0x%x, starts at 0x%llx, %d pages, %llu Kbytes\n\r", type, phys, pages, (pages*0x1000)/0x400);

	written = _JOS_LIBC_FUNC_NAME(wcslen)(0);
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"");
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"1");
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"22");
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"123456789");

	return 0;
}