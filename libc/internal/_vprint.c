#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>

#ifndef _JOS_KERNEL_BUILD
#define EOF (int)(-1)
#endif 

#include "include\_vprint.h"

//ZZZ: this include file issue needs tidying!
#define _JOS_ASSERT(cond)

// WIDECHAR is defined: this will build the wide implementation
#define WIDECHAR
#include "_vprint.inc.c"

// implements narrow versions
#undef WIDECHAR
#include "_vprint.inc.c"

