
#include <wchar.h>
#include "include/libc_internal.h"

size_t _JOS_LIBC_FUNC_NAME(wcslen)( const wchar_t *str )
{
    size_t n = 0;
	while(*str!=0) 
	{
		++n;
		++str;
	}
	return n;
}
