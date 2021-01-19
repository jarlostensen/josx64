#pragma once

#include <stdint.h>
#include <stdbool.h>
#ifdef _JOS_KERNEL_BUILD
#include "winnt_lt.h"
#else
#include <windows.h>
#endif

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
    union {
        const IMAGE_NT_HEADERS32* _headers32;
        const IMAGE_NT_HEADERS64* _headers64;
    };
    const IMAGE_SECTION_HEADER* _imageSections;
    WORD _numSections;
    bool _is_64_bit : 1;
    bool _relocated : 1;
    bool _is_dot_net : 1;

} peutil_pe_context_t;

bool peutil_bind(peutil_pe_context_t* ctx, const void* ptr, peutil_instance_type type);

/**
* \brief memory address of entry point for PE image (winCRTStartup, or DLLMain [but this can be 0!])
*/
uintptr_t peutil_entry_point(peutil_pe_context_t* ctx);

/**
* true if 64 bit PE image
*/
bool peutil_is_64_bit(peutil_pe_context_t* ctx);

const void* peutil_image_section(peutil_pe_context_t *ctx, const size_t image_section_entry_id);

const IMAGE_RESOURCE_DIRECTORY* peutil_resource_directory(peutil_pe_context_t *ctx);

const IMAGE_IMPORT_DESCRIPTOR* peutil_static_import_directory(peutil_pe_context_t * ctx);

const IMAGE_DELAYLOAD_DESCRIPTOR* peutil_delayload_directory(peutil_pe_context_t * ctx);

const void* peutil_rva_to_phys(peutil_pe_context_t* ctx, DWORD rva);

const IMAGE_SECTION_HEADER* peutil_section_for_rva(peutil_pe_context_t* ctx, DWORD rva);
