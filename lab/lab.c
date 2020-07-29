
// =========================================================================================

#include <stdint.h>
#include <varargs.h>
#include "../libc/internal/include/libc_internal.h"

extern int _JOS_LIBC_FUNC_NAME(swprintf)(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vswprintf)(wchar_t *__restrict buffer, size_t bufsz, const wchar_t * __restrict format, va_list vlist);
extern size_t _JOS_LIBC_FUNC_NAME(wcslen)( const wchar_t *str);

int main(void)
{
	wchar_t wbuffer[1024];
	int written = _JOS_LIBC_FUNC_NAME(swprintf)(wbuffer,sizeof(wbuffer)/sizeof(wchar_t),L"Hello wide %s, %3.2f this is 0x%x == %d, and \"%c\"", L"world", 3.1459f, 0xabcdef, 42, L'J');
	wbuffer[0] = 0;
	written = _JOS_LIBC_FUNC_NAME(swprintf)(wbuffer,0,L"Hello wide %s, %3.2f this is 0x%x == %d, and \"%c\"", L"world", 3.1459f, 0xabcdef, 42, L'J');

	written = _JOS_LIBC_FUNC_NAME(wcslen)(0);
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"");
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"1");
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"22");
	written = _JOS_LIBC_FUNC_NAME(wcslen)(L"123456789");

	return 0;
}