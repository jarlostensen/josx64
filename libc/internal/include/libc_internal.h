#pragma once
#ifndef _JOS_LIBC_INTERNAL_H
#define _JOS_LIBC_INTERNAL_H

#ifdef _JOS_KERNEL_BUILD

#define _JOS_LIBC_FUNC_NAME(std_name) std_name

#else

// so that we can build and link names like printf in the lab without conflicting with the *real* libc
#define _JOS_LIBC_FUNC_NAME(std_name) _jos_##std_name

#endif

#endif // _JOS_LIBC_INTERNAL_H
