
#include "pe.h"
#include <string.h>

//TODO: 64 bit guard

bool peutil_bind(peutil_pe_context_t* ctx, const void* ptr, peutil_instance_type type) {

    memset(ctx,0,sizeof(peutil_pe_context_t));
    const char* data = (const char*)(ptr);
    ctx->_relocated = type == kPe_Relocated;
    ctx->_header = (const IMAGE_DOS_HEADER*)data;
    if(ctx->_header->e_magic == IMAGE_DOS_SIGNATURE)
    {
        ctx->_nt_header = (const IMAGE_NT_HEADERS64*)(data + ctx->_header->e_lfanew);
        ctx->_is_64_bit = (ctx->_nt_header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

        if(ctx->_is_64_bit)
        {
            ctx->_headers64 = (const IMAGE_NT_HEADERS64*)(data + ctx->_header->e_lfanew);
            ctx->_imageSections = (const IMAGE_SECTION_HEADER*)(ctx->_headers64 + 1);
            ctx->_numSections = ctx->_headers64->FileHeader.NumberOfSections;
            ctx->_is_dot_net = (ctx->_headers64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress != 0);
            return true;
        }
    }
    return false;
}

const void* peutil_rva_to_phys(peutil_pe_context_t* ctx, DWORD rva)
{
    const IMAGE_SECTION_HEADER* section = peutil_section_for_rva(ctx, rva);
    if(!section)
        return 0;

    if(!ctx->_relocated)
    {
        return (const void*)((const char*)(ctx->_header) + (rva - section->VirtualAddress + section->PointerToRawData));
    }
    // if we're binding to relocated data (i.e. an HINSTANCE)
    return (const void*)((const char*)(ctx->_header) + rva);
}

/// given an RVA, returns the section it is in (or nullptr)
const IMAGE_SECTION_HEADER* peutil_section_for_rva(peutil_pe_context_t* ctx, DWORD rva)
{
    for(WORD section = 0; section < ctx->_numSections; ++section)
    {
        if(rva >= ctx->_imageSections[section].VirtualAddress && rva < ctx->_imageSections[section].VirtualAddress + ctx->_imageSections[section].SizeOfRawData)
            return ctx->_imageSections + section;
    }
    return 0;
}

uintptr_t peutil_entry_point(peutil_pe_context_t* ctx) {
    return (uintptr_t)(peutil_rva_to_phys(ctx, ctx->_nt_header->OptionalHeader.AddressOfEntryPoint));
}
        
/// any image section
const void* peutil_image_section(peutil_pe_context_t *ctx, const size_t image_section_entry_id)
{
    const IMAGE_DATA_DIRECTORY* resdir = &ctx->_headers64->OptionalHeader.DataDirectory[image_section_entry_id];
    return peutil_rva_to_phys(ctx, resdir->VirtualAddress);
}

const IMAGE_RESOURCE_DIRECTORY* peutil_resource_directory(peutil_pe_context_t *ctx)
{
    return (const IMAGE_RESOURCE_DIRECTORY*)(peutil_image_section(ctx, IMAGE_DIRECTORY_ENTRY_RESOURCE));
}

const IMAGE_IMPORT_DESCRIPTOR* peutil_static_import_directory(peutil_pe_context_t * ctx)
{
    return (const IMAGE_IMPORT_DESCRIPTOR*)(peutil_image_section(ctx, IMAGE_DIRECTORY_ENTRY_IMPORT));
}

const IMAGE_DELAYLOAD_DESCRIPTOR* peutil_delayload_directory(peutil_pe_context_t * ctx)
{
    return (const IMAGE_DELAYLOAD_DESCRIPTOR*)(peutil_image_section(ctx, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT));
}
