#include <c-efi.h>

#define _JOS_IMPLEMENT_ALLOCATORS

#include <arena_allocator.h>
#include <fixed_allocator.h>
#include <collections.h>
#include <stdio.h>
#include <string.h>
#include <serial.h>
#include <wchar.h>
#include <linear_allocator.h>
#include <memory.h>

// in efi_main.c
extern CEfiBootServices  * g_boot_services;
extern CEfiSystemTable   * g_st;

static CEfiMemoryDescriptor *_boot_service_memory_map = 0;
static CEfiUSize            _boot_service_memory_map_size = 0;
static CEfiUSize            _boot_service_memory_map_entries = 0;
static CEfiUSize            _map_key = 0;
static linear_allocator_t*  _main_allocator = 0;

static const char* kMemoryChannel = "memory";

// 4Meg minimum
#define MINIMUM_MEMORY_AVAILABLE_PAGES  1024

jo_status_t memory_uefi_init(linear_allocator_t** main_allocator) 
{
    CEfiStatus status = C_EFI_SUCCESS;
    
    CEfiUSize descriptor_size;
    CEfiU32 descriptor_version;

    if ( !g_boot_services ) {
        return _JO_STATUS_PERMISSION_DENIED;
    }

    _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init");

    g_boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, 0, &descriptor_size, 0);

    // make room for any expansion that might happen when we call "allocate_pool" 
    _boot_service_memory_map_size += 2*descriptor_size;    
    status = g_boot_services->allocate_pool(C_EFI_LOADER_DATA, _boot_service_memory_map_size, (void**)&_boot_service_memory_map);
    g_boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, &_map_key, &descriptor_size, &descriptor_version);

    // our allocation setup strategy is simply as follows:
    // - first find the largest block of conventional memory available, this will become our root arena allocator region.
    // - secondly look for special memory regions that we need to know about and map these into our own memory map structures (allocated in the previously found arena)

    //wchar_t wbuffer[128];

    CEfiMemoryDescriptor* desc = _boot_service_memory_map;
    CEfiMemoryDescriptor* max_desc = 0;
    CEfiUSize max_conventional = 0;
    _boot_service_memory_map_entries = _boot_service_memory_map_size / descriptor_size;
    size_t available = 0;

    _JOS_KTRACE_CHANNEL(kMemoryChannel, "%d memory descriptors found", _boot_service_memory_map_entries);

    for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    {        
        if ( desc[i].type != C_EFI_RESERVED_MEMORY_TYPE 
            &&  
            desc[i].type != C_EFI_UNUSABLE_MEMORY) {

            //swprintf(wbuffer, 128, L"region @ 0x%x, %d pages, type %d\n\r", desc->physical_start, desc->number_of_pages, desc->type);
            //g_st->con_out->output_string(g_st->con_out, wbuffer);

            switch(desc[i].type)
            {
                case C_EFI_CONVENTIONAL_MEMORY:
                {
                    if ( desc[i].number_of_pages > max_conventional ) {
                
                        max_conventional = desc[i].number_of_pages;
                        max_desc = desc+i;
                    }
                    available += desc[i].number_of_pages;
                }
                break;
                case C_EFI_BOOT_SERVICES_CODE:
                case C_EFI_BOOT_SERVICES_DATA:
                {
                    available += desc[i].number_of_pages;
                }
                default:;
            }         
        }        
    }

    //swprintf(wbuffer, 128, L"largest conventional region @ 0x%x is %d pages\n\r", max_desc->physical_start, max_desc->number_of_pages);
    //g_st->con_out->output_string(g_st->con_out, wbuffer);

    if ( available < MINIMUM_MEMORY_AVAILABLE_PAGES ) {
        _JOS_KTRACE_CHANNEL(kMemoryChannel, "**** FATAL ERROR: not enough RAM");
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    available *= 0x1000;
    // set up our main allocator to bootstrap everything else
    // TODO: this only uses the largest block of memory, ultimately we need to create a collection of 
    // allocators for each block
    *main_allocator =  _main_allocator = linear_allocator_create((void*)max_desc->physical_start, max_desc->number_of_pages * 0x1000);
    
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init succeeded");

    return _JO_STATUS_SUCCESS;
}

size_t      memory_get_total(void) {
    return (size_t)_main_allocator->_end - (size_t)_main_allocator->_begin;
}

CEfiUSize memory_boot_service_get_mapkey(void) {
    if ( !g_boot_services ) {
        return 0;
    }
    return _map_key;
}

jo_status_t memory_refresh_boot_service_memory_map(void) {

    if ( !g_boot_services ) {
        return _JO_STATUS_PERMISSION_DENIED;
    }

    if ( _boot_service_memory_map_size ) {
        g_boot_services->free_pool(_boot_service_memory_map);
        _boot_service_memory_map_size = 0;
    }

    CEfiUSize descriptor_size;
    CEfiU32 descriptor_version;

    g_boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, 0, &descriptor_size, 0);
    
    _boot_service_memory_map_size += 2*descriptor_size;    
    if ( C_EFI_ERROR(g_boot_services->allocate_pool(C_EFI_LOADER_DATA, _boot_service_memory_map_size, (void**)&_boot_service_memory_map))) {
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    g_boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, &_map_key, &descriptor_size, &descriptor_version);

    return _JO_STATUS_SUCCESS;
}

jo_status_t memory_runtime_init(void) {
        
    //TODO:
    
    return _JO_STATUS_SUCCESS;
}

// ==============================================================
// TODO: more pools and arenas?
#if 0
void* malloc(size_t size) {
    void* ptr = vmem_arena_alloc(_bootstrap_arena, size);
    _JOS_ASSERT(ptr);
    return ptr;
}

void free(void* block) {
    return vmem_arena_free(_bootstrap_arena, block);
}

void *calloc(size_t nmemb, size_t size)
{
    if(!nmemb || !size)
        return 0;
    return vmem_arena_alloc(_bootstrap_arena, nmemb * size);
}

void *realloc(void *ptr, size_t size)
{
    if(!ptr)
        return vmem_arena_alloc(_bootstrap_arena, size);
    
    if(!size)
    {
        vmem_arena_free(_bootstrap_arena, ptr);
        return ptr;
    }

    //TODO: built in realloc, using knowledge of size of allocation
    void* new_ptr = vmem_arena_alloc(_bootstrap_arena, size);
    if(new_ptr)
        vmem_arena_free(_bootstrap_arena, ptr);
    return new_ptr;
}
#endif
