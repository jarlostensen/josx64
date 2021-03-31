#pragma once
#ifndef _JOS_WCHAR_H
#define _JOS_WCHAR_H

#include <stdint.h>

#ifndef _JOS_WCHAR_DEFINED
    #define _JOS_WCHAR_DEFINED
    typedef uint16_t   wchar_t;
#endif

size_t wcslen( const wchar_t *str );

#endif // _JOS_WCHAR_H

