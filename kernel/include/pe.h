#pragma once

#include <stdint.h>
#include <stdbool.h>
#ifdef _JOS_KERNEL_BUILD
#include <joWinNtLt.h>
#else
#pragma warning(disable:5105)
#include <windows.h>
#endif
#include <jos.h>



typedef enum _peutil_instance_type
{
    // image is relocated (i.e. loaded by the OS PE loader), for example a HINSTANCE
    kPe_Relocated,
    // image is not relocated, i.e. you might have lodaded the image directly from disk with raw file reads
    kPe_Unrelocated
} peutil_instance_type;

typedef struct _peutil_pe_context {
    const IMAGE_DOS_HEADER* _header;
    const IMAGE_NT_HEADERS64* _nt_header;
    union _headers_tag {
        const IMAGE_NT_HEADERS32* _headers32;
        const IMAGE_NT_HEADERS64* _headers64;
    } _headers;
    const IMAGE_SECTION_HEADER* _imageSections;
    WORD _numSections;
    DWORD _text_va;
    DWORD _text_ptr;
    DWORD _text_end;
    bool _is_64_bit : 1;
    bool _relocated : 1;
    bool _is_dot_net : 1;

} peutil_pe_context_t;

bool peutil_bind(peutil_pe_context_t* ctx, const void* ptr, peutil_instance_type type);

/**
* \brief memory address of entry point for PE image (winCRTStartup, or DLLMain [but this can be 0!])
*/
uintptr_t peutil_entry_point(peutil_pe_context_t* ctx);

const void* peutil_image_section(peutil_pe_context_t *ctx, const size_t image_section_entry_id);

const IMAGE_RESOURCE_DIRECTORY* peutil_resource_directory(peutil_pe_context_t *ctx);

const IMAGE_IMPORT_DESCRIPTOR* peutil_static_import_directory(peutil_pe_context_t * ctx);

const IMAGE_DELAYLOAD_DESCRIPTOR* peutil_delayload_directory(peutil_pe_context_t * ctx);

const void* peutil_rva_to_phys(peutil_pe_context_t* ctx, DWORD rva);

const IMAGE_SECTION_HEADER* peutil_section_for_rva(peutil_pe_context_t* ctx, DWORD rva);

bool peutil_phys_is_executable(peutil_pe_context_t* ctx, uintptr_t phys, uintptr_t* out_rva);

const void* peutil_get_proc_name_address(peutil_pe_context_t* ctx, const char* proc_name);
void peutil_load_dll(peutil_pe_context_t* ctx, page_allocator_t* allocator);
