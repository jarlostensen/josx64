#include <c-efi.h>

#include <arena_allocator.h>
#include <fixed_allocator.h>
#include <collections.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"

// in efi_main.c
extern CEfiBootServices  * g_boot_services;
extern CEfiSystemTable   * g_st;

static CEfiMemoryDescriptor *_boot_service_memory_map = 0;
static CEfiUSize     _map_key = 0;
static CEfiUSize     _map_size = 0;

static size_t _total_initial_available_memory = 0;
static vmem_arena_t* _main_arena = 0;
static vector_t      _memory_map;

// 4Meg minimum
#define MINIMUM_MEMORY_AVAILABLE_PAGES  1024

k_status memory_pre_exit_bootservices_initialise() 
{
    CEfiStatus status = C_EFI_SUCCESS;
    
    CEfiUSize map_key, descriptor_size;
    CEfiU32 descriptor_version;

    if ( !g_boot_services ) {
        return _JOS_K_STATUS_PERMISSION_DENIED;
    }    

    g_boot_services->get_memory_map(&_map_size, _boot_service_memory_map, 0, &descriptor_size, 0);
    unsigned mem_desc_entries = _map_size / descriptor_size;

    // make room for any expansion that might happen when we call "allocate_pool" 
    _map_size += 2*descriptor_size;    
    status = g_boot_services->allocate_pool(C_EFI_LOADER_DATA, _map_size, (void**)&_boot_service_memory_map);
    g_boot_services->get_memory_map(&_map_size, _boot_service_memory_map, &_map_key, &descriptor_size, &descriptor_version);

    // our allocation setup strategy is simply as follows:
    // - first find the largest block of conventional memory available, this will become our root arena allocator region.
    // - secondly look for special memory regions that we need to know about and map these into our own memory map structures (allocated in the previously found arena)

    CEfiMemoryDescriptor* desc = _boot_service_memory_map;
    CEfiMemoryDescriptor* max_desc = 0;
    CEfiUSize max_conventional = 0;
    size_t available = 0;
    for ( unsigned i = 0; i < mem_desc_entries; ++i )
    {        
        if ( (desc->attribute & C_EFI_CONVENTIONAL_MEMORY) == C_EFI_CONVENTIONAL_MEMORY ) {  
            if ( desc->number_of_pages > max_conventional ) {
                
                max_conventional = desc->number_of_pages;
                max_desc = desc;
            }            
            available += desc->number_of_pages;
        }
        else if ( (desc->attribute & (C_EFI_BOOT_SERVICES_CODE | C_EFI_BOOT_SERVICES_DATA)) == (C_EFI_BOOT_SERVICES_CODE | C_EFI_BOOT_SERVICES_DATA) ) {
            available += desc->number_of_pages;
        }
    }

    if ( available < MINIMUM_MEMORY_AVAILABLE_PAGES ) {
        return _JOS_K_STATUS_RESOURCE_EXHAUSTED;
    }

    available *= 0x1000;

    // set up our main allocation arena to bootstrap everything else
    _main_arena = vmem_arena_create((void*)max_desc->physical_start, max_desc->number_of_pages * 0x1000);

    // allocate a vector for our memory map
    vector_create(&_memory_map, mem_desc_entries, sizeof(CEfiMemoryDescriptor));

    // now build our own map
    desc = _boot_service_memory_map;
    for ( unsigned i = 0; i < mem_desc_entries; ++i )
    {
        //TODO:
    }

    return _JOS_K_STATUS_SUCCESS;
}

CEfiUSize memory_boot_service_get_mapkey() {
    if ( !g_boot_services ) {
        return 0;
    }
    return _map_key;
}

k_status memory_refresh_boot_service_memory_map() {

    if ( !g_boot_services ) {
        return _JOS_K_STATUS_PERMISSION_DENIED;
    }

    if ( _map_size ) {
        g_boot_services->free_pool(_boot_service_memory_map);
        _map_size = 0;
    }

    CEfiUSize descriptor_size;
    CEfiU32 descriptor_version;

    g_boot_services->get_memory_map(&_map_size, _boot_service_memory_map, 0, &descriptor_size, 0);
    unsigned mem_desc_entries = _map_size / descriptor_size;
    
    _map_size += 2*descriptor_size;    
    if ( C_EFI_ERROR(g_boot_services->allocate_pool(C_EFI_LOADER_DATA, _map_size, (void**)&_boot_service_memory_map))) {
        return _JOS_K_STATUS_RESOURCE_EXHAUSTED;
    }

    g_boot_services->get_memory_map(&_map_size, _boot_service_memory_map, &_map_key, &descriptor_size, &descriptor_version);

    return _JOS_K_STATUS_SUCCESS;
}

k_status memory_post_exit_bootservices_initialise() {
    return _JOS_K_STATUS_UNIMPLEMENTED;
}

// ==============================================================
// TODO: more pools and arenas?

void* malloc(size_t size) {
    return vmem_arena_alloc(_main_arena, size);
}

void free(void* block) {
    return vmem_arena_free(_main_arena, block);
}