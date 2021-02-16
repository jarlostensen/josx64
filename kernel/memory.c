#include <c-efi.h>

#include <arena_allocator.h>
#include <fixed_allocator.h>
#include <collections.h>
#include <stdio.h>
#include <string.h>

#include <wchar.h>

#include "memory.h"

// in efi_main.c
extern CEfiBootServices  * g_boot_services;
extern CEfiSystemTable   * g_st;

static CEfiMemoryDescriptor *_boot_service_memory_map = 0;
static CEfiUSize            _boot_service_memory_map_size = 0;
static CEfiUSize            _boot_service_memory_map_entries = 0;
static CEfiUSize            _map_key = 0;

static size_t _total_initial_available_memory = 0;
static vmem_arena_t* _bootstrap_arena = 0;
static vector_t      _memory_map;

// only valid immediately after memory_pre_exit_bootservices_initialise()
static uint8_t*      _memory_bitmap = 0;

// 4Meg minimum
#define MINIMUM_MEMORY_AVAILABLE_PAGES  1024

jo_status_t memory_pre_exit_bootservices_initialise() 
{
    CEfiStatus status = C_EFI_SUCCESS;
    
    CEfiUSize map_key, descriptor_size;
    CEfiU32 descriptor_version;

    if ( !g_boot_services ) {
        return _JO_STATUS_PERMISSION_DENIED;
    }    

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
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    available *= 0x1000;
   
    // set up our main allocation arena to bootstrap everything else
    _bootstrap_arena = vmem_arena_create((void*)max_desc->physical_start, max_desc->number_of_pages * 0x1000);
    if ( !_bootstrap_arena ) {
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    // allocate a vector for our memory map
    //vector_create(&_memory_map, _boot_service_memory_map_entries, sizeof(CEfiMemoryDescriptor));
    // now build our own map
    // desc = _boot_service_memory_map;
    // for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    // {
    //     //TODO:
    // }

    return _JO_STATUS_SUCCESS;
}

CEfiUSize memory_boot_service_get_mapkey() {
    if ( !g_boot_services ) {
        return 0;
    }
    return _map_key;
}

jo_status_t memory_refresh_boot_service_memory_map() {

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
    unsigned mem_desc_entries = _boot_service_memory_map_size / descriptor_size;
    
    _boot_service_memory_map_size += 2*descriptor_size;    
    if ( C_EFI_ERROR(g_boot_services->allocate_pool(C_EFI_LOADER_DATA, _boot_service_memory_map_size, (void**)&_boot_service_memory_map))) {
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    g_boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, &_map_key, &descriptor_size, &descriptor_version);

    return _JO_STATUS_SUCCESS;
}

jo_status_t memory_post_exit_bootservices_initialise() {
    
    _memory_bitmap = vmem_arena_alloc(_bootstrap_arena, _boot_service_memory_map_entries);
    if ( !_memory_bitmap )
    {
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }
    memset(_memory_bitmap, 0, _boot_service_memory_map_entries);

    CEfiMemoryDescriptor* desc = _boot_service_memory_map;
    CEfiMemoryDescriptor* max_desc = 0;
    CEfiUSize max_conventional = 0;
    size_t available = 0;
    for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    {
        _memory_bitmap[i] = desc[i].type;
    }
    
    return _JO_STATUS_SUCCESS;
}

const uint8_t* memory_get_memory_bitmap(size_t *out_dim) {
    out_dim[0] = _boot_service_memory_map_entries;
    return _memory_bitmap;
}

// ==============================================================
// TODO: more pools and arenas?

void* malloc(size_t size) {
    return vmem_arena_alloc(_bootstrap_arena, size);
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