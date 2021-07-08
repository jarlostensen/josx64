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


typedef enum memory_region_type {
    kMemoryRegion_RAM,
    kMemoryRegion_Reserved,
    kMemoryRegion_ACPI,
    kMemoryRegion_NVS,
    kMemoryRegion_Unusable,
    kMemoryRegion_Unknown,
} memory_region_type_t;

typedef struct _memory_region {

    uintptr_t   _start;
    size_t      _size;    
    memory_region_type_t _type;
    CEfiU32 _uefi_type;

} memory_region_t;

// TODO: allocate dynamically from each region and link them as a list?
#define MAX_MEMORY_REGIONS 64
static memory_region_t _regions[MAX_MEMORY_REGIONS];
static size_t _num_regions = 0;

static void add_memory_region(uintptr_t start, size_t size, memory_region_type_t type, CEfiU32 uefi_type) {
    _JOS_ASSERT(_num_regions < MAX_MEMORY_REGIONS);
    _regions[_num_regions]._start = start;
    _regions[_num_regions]._size = size;
    _regions[_num_regions]._type = type;
    _regions[_num_regions]._uefi_type = uefi_type;
    ++_num_regions;    
}

_JOS_API_FUNC void _memory_debugger_dump_map(void) {
    
    for ( unsigned i = 0; i < _num_regions; ++i )
    {   
        CEfiISize size = _regions[i]._size;
        const char* unit;
        if ((size >> 40) > 0) {
            size >>= 40;
            unit = "TB";
        } else if ((size >> 30) > 0) {
            size >>= 30;
            unit = "GB";
        } else if ((size >> 20) > 0) {
            size >>= 20;
            unit = "MB";
        } else {
            size >>= 10;
            unit = "KB";
        }
        const char* type_name;
        char buffer[32];
        switch (_regions[i]._type) {
            case kMemoryRegion_RAM:
            {
                type_name = "RAM";
            }
            break;
            case kMemoryRegion_Reserved:
            {
                type_name = "RESERVED";
            }
            break;
            case kMemoryRegion_Unusable:
            {
                type_name = "UNUSABLE";
            }
            break;
            case kMemoryRegion_ACPI:
            {
                type_name = "ACPI";
            }
            break;
            case kMemoryRegion_NVS:
            {
                type_name = "NVS";
            }
            break;
            default:
            {
                snprintf(buffer, sizeof(buffer), "UNK 0x%x", _regions[i]._uefi_type);
                type_name = buffer;
            }
            break;
        }        

        _JOS_KTRACE_CHANNEL(kMemoryChannel, "%s: %016llx %d %s",  type_name, _regions[i]._start, size, unit);
    }
}

static jo_status_t _build_memory_map(void) {
    
    CEfiMemoryDescriptor* desc = _boot_service_memory_map;
    _boot_service_memory_map_entries = _boot_service_memory_map_size / _descriptor_size;    
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "%d memory descriptors found", _boot_service_memory_map_entries);
    
    for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    {        
        if ( desc[i].number_of_pages==0 )
            continue;

        switch(desc[i].type)
        {
            case C_EFI_RESERVED_MEMORY_TYPE:
            {
                add_memory_region(desc[i].physical_start, desc[i].number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_Reserved, desc[i].type);
            }
            break;
            case C_EFI_UNUSABLE_MEMORY:
            {
                add_memory_region(desc[i].physical_start, desc[i].number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_Unusable, desc[i].type);
            }
            break;
            case C_EFI_ACPI_RECLAIM_MEMORY:
            {
                add_memory_region(desc[i].physical_start, desc[i].number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_ACPI, desc[i].type);
            }
            break;
            case C_EFI_ACPI_MEMORY_NVS:
            {
                add_memory_region(desc[i].physical_start, desc[i].number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_NVS, desc[i].type);
            }
            break;
            case C_EFI_BOOT_SERVICES_CODE:
            case C_EFI_BOOT_SERVICES_DATA:
            case C_EFI_CONVENTIONAL_MEMORY:
            {
                if (desc[i].attribute & C_EFI_MEMORY_WB) {
                    add_memory_region(desc[i].physical_start, desc[i].number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_RAM, desc[i].type);
                }
                else {
                    add_memory_region(desc[i].physical_start, desc[i].number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_Reserved, desc[i].type);
                }                
            }
            break;
            default:;
        }
        
        //NOTE: regions *may* overlap and we should check for it but so far none have been seen in the wild...
    }
    
    return _JO_STATUS_SUCCESS;
}

static jo_status_t _get_boot_service_memory_map(CEfiBootServices* boot_services) {
    
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

_JOS_API_FUNC jo_status_t memory_uefi_init(CEfiBootServices* boot_services, linear_allocator_t** main_allocator) {
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init");
    _get_boot_service_memory_map(boot_services);
    jo_status_t status = _build_memory_map();
    if (_JO_SUCCEEDED(status)) {
        size_t max_r = 0;
        size_t max_region = 0;
        for (size_t r = 0; r < _num_regions; ++r) {
            if (_regions[r]._type == kMemoryRegion_RAM) {
                if (_regions[r]._size > max_region) {
                    max_region = _regions[r]._size;
                    max_r = r;
                }
            }
        }
        _main_allocator = linear_allocator_create((void*)_regions[max_r]._start, _regions[max_r]._size);
        *main_allocator = _main_allocator;
        _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init succeeded");
    }
    return status;
}

_JOS_API_FUNC size_t      memory_get_total(void) {
    return (size_t)_main_allocator->_end - (size_t)_main_allocator->_begin;
}

_JOS_API_FUNC jo_status_t memory_runtime_init(CEfiHandle h, CEfiBootServices* boot_services) {
        
    _get_boot_service_memory_map(boot_services);
    CEfiStatus status = boot_services->exit_boot_services(h, _map_key);    
    if ( C_EFI_ERROR(status )) {
        return _JO_STATUS_UNAVAILABLE;        
    }
    return _JO_STATUS_SUCCESS;
}
