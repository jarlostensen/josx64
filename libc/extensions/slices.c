
#ifdef _JOS_KERNEL_BUILD
#include <extensions/slices.h>
#else
#include "..\include\extensions\slices.h"
#endif

const char_array_slice_t kEmptySlice = { ._ptr = 0, ._length = 0 };

_JOS_API_FUNC bool char_array_slice_match_str(char_array_slice_t* slice, const char* str) {
    
    if(slice_is_empty(slice) || str_is_empty(str) ) {
        return false;
    }

    if(strlen(str)!=slice->_length)
        return false;

    unsigned n = 0;
    while(n < slice->_length && slice->_ptr[n]==str[n])
    {
        ++n;
    }

    return n == slice->_length;
}

_JOS_API_FUNC bool char_array_slice_equals(char_array_slice_t prefix, char_array_slice_t candidate) {
    
    if (slice_is_empty(&prefix) || slice_is_empty(&candidate)) {
        return false;
    }

    if (prefix._length != candidate._length) {
        return false;
    }

    for (unsigned n = 0; n < prefix._length; ++n) {
        if ( prefix._ptr[n]!=candidate._ptr[n])
            return false;
    }
    return true;
}
