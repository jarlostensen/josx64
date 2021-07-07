#include <c-efi.h>

#define _JOS_IMPLEMENT_ALLOCATORS
#define _JOS_IMPLEMENT_CONTAINERS

#include <arena_allocator.h>
#include <fixed_allocator.h>
#include <linear_allocator.h>
#include <collections.h>

#include <stdio.h>
#include <string.h>
#include <serial.h>
#include <wchar.h>
#include <memory.h>

static CEfiMemoryDescriptor *_boot_service_memory_map = 0;
static CEfiUSize            _boot_service_memory_map_size = 0;
static CEfiUSize            _boot_service_memory_map_entries = 0;
static CEfiUSize            _map_key = 0;
static CEfiUSize            _descriptor_size = 0;
static CEfiU32              _descriptor_version = 0;   
static linear_allocator_t*  _main_allocator = 0;

static const char* kMemoryChannel = "memory";

// 1GB minimum (NOTE: the UEFI pool allocator always works on 4K pages)
#define UEFI_POOL_PAGE_SIZE 0x1000
#define MINIMUM_MEMORY_AVAILABLE_PAGES (4*1024*1024) / UEFI_POOL_PAGE_SIZE

_JOS_API_FUNC void _memory_debugger_dump_map(void) {
    CEfiMemoryDescriptor* desc = _boot_service_memory_map;    
    _boot_service_memory_map_entries = _boot_service_memory_map_size / _descriptor_size;
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "%d memory descriptors found", _boot_service_memory_map_entries);
    
    for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    {        
        if ( desc[i].type != C_EFI_RESERVED_MEMORY_TYPE 
            &&  
            desc[i].type != C_EFI_UNUSABLE_MEMORY) {
                
                switch(desc[i].type)
                {
                    case C_EFI_CONVENTIONAL_MEMORY:
                    {
                        _JOS_KTRACE_CHANNEL(kMemoryChannel, "CONV: %d pages", desc[i].number_of_pages);
                    }
                    break;
                    case C_EFI_BOOT_SERVICES_CODE:
                    case C_EFI_BOOT_SERVICES_DATA:
                    {
                        _JOS_KTRACE_CHANNEL(kMemoryChannel, "BOOTSERVICES: %d pages", desc[i].number_of_pages);
                    }
                    default:;
                }         
            }
    }
}

static jo_status_t _refresh_memory_map(void) {
    
    CEfiMemoryDescriptor* desc = _boot_service_memory_map;
    CEfiMemoryDescriptor* max_desc = 0;
    CEfiUSize max_conventional = 0;
    _boot_service_memory_map_entries = _boot_service_memory_map_size / _descriptor_size;
    size_t available = 0;
    
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "%d memory descriptors found", _boot_service_memory_map_entries);
    
    for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    {        
        if ( desc[i].type != C_EFI_RESERVED_MEMORY_TYPE 
            &&  
            desc[i].type != C_EFI_UNUSABLE_MEMORY) {
                
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
    
    if ( available < MINIMUM_MEMORY_AVAILABLE_PAGES ) {
        _JOS_KTRACE_CHANNEL(kMemoryChannel, "**** FATAL ERROR: not enough RAM");
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }
    
    available *= UEFI_POOL_PAGE_SIZE;
    // set up our main allocator to bootstrap everything else
    // TODO: this only uses the largest block of memory, ultimately we need to create a collection of 
    // allocators for each block
    _main_allocator = linear_allocator_create((void*)max_desc->physical_start, max_desc->number_of_pages * UEFI_POOL_PAGE_SIZE);    

    return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC jo_status_t memory_uefi_init(CEfiBootServices* boot_services, linear_allocator_t** main_allocator) {
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init");
    memory_refresh_boot_service_memory_map(boot_services);
    _refresh_memory_map();
    *main_allocator = _main_allocator;    
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init succeeded");
    return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC size_t      memory_get_total(void) {
    return (size_t)_main_allocator->_end - (size_t)_main_allocator->_begin;
}

_JOS_API_FUNC CEfiUSize memory_boot_service_get_mapkey(void) {
    return _map_key;
}

_JOS_API_FUNC jo_status_t memory_refresh_boot_service_memory_map(CEfiBootServices* boot_services) {

    if ( _boot_service_memory_map_size ) {
        boot_services->free_pool(_boot_service_memory_map);
        _boot_service_memory_map_size = 0;
    }
    
    boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, 0, &_descriptor_size, 0);    
    _boot_service_memory_map_size += 2*_descriptor_size;    
    if ( C_EFI_ERROR(boot_services->allocate_pool(C_EFI_LOADER_DATA, _boot_service_memory_map_size, (void**)&_boot_service_memory_map))) {
        return _JO_STATUS_RESOURCE_EXHAUSTED;
    }

    boot_services->get_memory_map(&_boot_service_memory_map_size, _boot_service_memory_map, &_map_key, &_descriptor_size, &_descriptor_version);
    return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC jo_status_t memory_runtime_init(CEfiHandle h, CEfiBootServices* boot_services) {
        
    memory_refresh_boot_service_memory_map(boot_services);
    CEfiStatus status = boot_services->exit_boot_services(h, memory_boot_service_get_mapkey());    
    if ( C_EFI_ERROR(status )) {
        return _JO_STATUS_UNAVAILABLE;        
    }
    return _JO_STATUS_SUCCESS;
}
