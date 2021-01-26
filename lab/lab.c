
// =========================================================================================

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../libc/internal/include/libc_internal.h"
#include "../kernel/include/hex_dump.h"
#include "../kernel/include/pe.h"
#include "../kernel/include/collections.h"
#include "../libc/include/extensions/slices.h"
#include "../libc/include/extensions/pdb_index.h"

void slice_printf(char_array_slice_t* slice) {
    for (unsigned n = 0; n < slice->_length; ++n) {
        printf("%c", slice->_ptr[n]);
    }
}


void dump_index(pdb_index_node_t* root, int level) {

    // this is a beauty!
    printf("%*s", level<<1, "");
    
    if ( !slice_is_empty(&root->_prefix) ) {
        slice_printf(&root->_prefix);
    }
    else {
        printf("root");
    }
    if (!symbol_is_empty(&root->_symbol)) {
        printf("\trva = 0x%x, section = %d", root->_symbol._rva, root->_symbol._section);
    }
    if (!vector_is_empty(&root->_children)) {
        printf("\n");
        const unsigned child_count = vector_size(&root->_children);
        for (unsigned c = 0; c < child_count; ++c) {
            pdb_index_node_t* child = (pdb_index_node_t*)vector_at(&root->_children, c);
            dump_index(child, level+1);
        }
    }    
    else {
        printf("\n");
    }
}

void test_search(const char* str) {

    pdb_index_node_t root, *leaf;
    memset(&root, 0, sizeof(root));

    char_array_slice_t body;
    char_array_slice_create(&body, str, 0,0);
    char_array_slice_t prefix = pdb_index_next_token(&body);
    pdb_index_match_result m = pdb_index_match_search(&root, prefix, body, &leaf);

}


// ======================================================================================

extern int _JOS_LIBC_FUNC_NAME(swprintf)(wchar_t* __restrict buffer, size_t sizeOfBuffer, const wchar_t* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vswprintf)(wchar_t*__restrict buffer, size_t bufsz, const wchar_t* __restrict format, va_list vlist);
extern int _JOS_LIBC_FUNC_NAME(snprintf)(char* __restrict buffer, size_t sizeOfBuffer, const char* __restrict format, ...);
extern int _JOS_LIBC_FUNC_NAME(vsnprintf)(char*__restrict buffer, size_t bufsz, const char* __restrict format, va_list vlist);

void test_a(char* buffer, const char* format, ...)
{
    va_list parameters;
	va_start(parameters, format);
    _JOS_LIBC_FUNC_NAME(vsnprintf)(buffer, 512, format, parameters);
    //vsnprintf(buffer, 512, format, parameters);
    va_end(parameters);
}

void test_w(wchar_t* buffer, const wchar_t* format, ...)
{
    va_list parameters;
	va_start(parameters, format);
    _JOS_LIBC_FUNC_NAME(vswprintf)(buffer, 512, format, parameters);
    //vswprintf(buffer, 512, format, parameters);
    va_end(parameters);
}

void trace(const char* __restrict channel, const char* __restrict format,...) {

    if(!format || !format[0])
        return;

    static unsigned long long _ticks = 0;

    //ZZZ:
    char buffer[256];
    va_list parameters;
    va_start(parameters, format);
	int written;
	if(channel)
		written = snprintf(buffer, sizeof(buffer), "[%lld:%s] ", _ticks++, channel);
	else
		written = snprintf(buffer, sizeof(buffer), "[%lld] ", _ticks++); 
    written += vsnprintf(buffer+written, sizeof(buffer)-written, format, parameters);
    va_end(parameters);

    buffer[written+0] = '\r';
    buffer[written+1] = '\n';
    buffer[written+2] = 0;

    printf(buffer);
}

int main(void)
{
    char buffer[512];
    wchar_t wbuffer[512];

    trace("lab","this is a message from the lab");
    trace("lab","and this is also a message from the lab");

    test_a(buffer, "%s %S", "this is a test", L"and this is wide");
    test_w(wbuffer, L"%s %S", L"this is w test ", "and this is narrow");

    _JOS_LIBC_FUNC_NAME(snprintf)(buffer, sizeof(buffer), "\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S\n", 
                    1,
                    42,
                    80,
                    1,
                    0,
                    L"enabled"
                    );
		
	_JOS_LIBC_FUNC_NAME(swprintf)(wbuffer, sizeof(wbuffer)/sizeof(wchar_t), L"\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S\n", 
                    1,
                    42,
                    80,
                    1,
                    0,
                    "enabled"
                    );

    HMODULE this_module = GetModuleHandle(0);
    hex_dump_mem((void*)this_module, 64, k8bitInt);
    peutil_pe_context_t pe_ctx;
    peutil_bind(&pe_ctx, (const void*)this_module, kPe_Relocated);
    uintptr_t entry = peutil_entry_point(&pe_ctx);

    dump_index(pdb_index_load_from_pdb_yml(),0);

    char_array_slice_t slice = pdb_index_symbol_name_for_address(71904);
    slice = pdb_index_symbol_name_for_address(137950);
    
	return 0;
}