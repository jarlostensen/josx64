#ifndef _JOS_KERNEL_ASSERT_H
#define _JOS_KERNEL_ASSERT_H

#include <joBase.h>

#ifdef _JO_BARE_METAL_BUILD
#undef assert

#ifdef NDEBUG
    #define assert(expression) ((void)0)
#else

_JO_INLINE_FUNC void _jo_assert(char const* _Message, char const* _File, unsigned _Line) {
    //TODO: 
    (void)_Message;
    (void)_File;
    (void)_Line;
}

#define assert(expression) (void)(  \
    (!!(expression)) ||             \
        (_jo_assert(#expression, __FILE__, __LINE__), 0) \
    )

#endif
#endif

#endif // _JOS_KERNEL_ASSERT_H
