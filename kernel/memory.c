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
    kMemoryRegion_LoaderCode,
    kMemoryRegion_LoaderData,
    kMemoryRegion_BootServicesCode,
    kMemoryRegion_BootServicesData,
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
#define MAX_MEMORY_REGIONS 256
static memory_region_t _regions[MAX_MEMORY_REGIONS];
static size_t _num_regions = 0;

static void _add_memory_region(uintptr_t start, size_t size, memory_region_type_t type, CEfiU32 uefi_type) {
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
            case kMemoryRegion_LoaderCode:
            {
                type_name = "Loader Code";
            }
            break;
            case kMemoryRegion_LoaderData:
            {
                type_name = "Loader Data";
            }
            break;
            case kMemoryRegion_BootServicesCode:
            {
                type_name = "BootServices Code";
            }
            break;
            case kMemoryRegion_BootServicesData:
            {
                type_name = "BootServices Data";
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

/*
exit_bs: false when we're in the UEFI init phase. true when exiting boot services and we want to merge 
         regions used by the boot services with conventional RAM.
 
Layout will vary per system, but as an example from my 4Gig VirtualBox VM instance:

[memory] RAM: 0000000000000000 540 KB
[memory] BootServices Data: 0000000000087000 100 KB
[memory] RAM: 0000000000100000 3 GB
[memory] BootServices Data: 00000000dbfbe000 29 MB
[memory] Loader Code: 00000000ddd49000 940 KB
[memory] Loader Data: 00000000dde34000 4 KB
[memory] ACPI: 00000000dde35000 4 KB
[memory] BootServices Code: 00000000dde36000 364 KB
[memory] RAM: 00000000ddf1b000 400 KB
[memory] BootServices Data: 00000000ddf7f000 6 MB
[memory] BootServices Data: 00000000de61a000 1 MB
[memory] BootServices Data: 00000000de726000 6 MB
[memory] BootServices Code: 00000000dee1b000 1 MB
[memory] RESERVED: 00000000defef000 16 KB
[memory] ACPI: 00000000deff3000 32 KB
[memory] NVS: 00000000deffb000 16 KB
[memory] BootServices Data: 00000000defff000 2 MB
[memory] BootServices Data: 00000000df218000 128 KB
[memory] BootServices Code: 00000000df238000 108 KB
[memory] BootServices Data: 00000000df253000 2 MB
[memory] BootServices Code: 00000000df457000 80 KB
[memory] RAM: 0000000100000000 549 MB

*/
static jo_status_t _build_memory_map(bool exit_bs) {
    
    CEfiMemoryDescriptor* desc = _boot_service_memory_map;
    _boot_service_memory_map_entries = _boot_service_memory_map_size / _descriptor_size;    
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "%d memory descriptors found", _boot_service_memory_map_entries);
    
    _num_regions = 0;
    memory_region_t* prev = 0;
    for ( unsigned i = 0; i < _boot_service_memory_map_entries; ++i )
    {        
        if ( desc->number_of_pages==0 )
            continue;

        // pre-exit boot services we don't merge boot services code and data with conventional memory
        if ( !exit_bs 
            && 
            (desc->type == C_EFI_BOOT_SERVICES_CODE
             ||
             desc->type == C_EFI_BOOT_SERVICES_DATA)) {
                if ( desc->type == C_EFI_BOOT_SERVICES_DATA ) {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_BootServicesData, desc->type);
                }
                else {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_BootServicesCode, desc->type);
                }
             }
        else {
            switch(desc->type)
            {
                case C_EFI_RESERVED_MEMORY_TYPE:
                {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_Reserved, desc->type);
                }
                break;
                case C_EFI_UNUSABLE_MEMORY:
                {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_Unusable, desc->type);
                }
                break;
                case C_EFI_ACPI_RECLAIM_MEMORY:
                {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_ACPI, desc->type);
                }
                break;
                case C_EFI_ACPI_MEMORY_NVS:
                {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_NVS, desc->type);
                }
                break;
                case C_EFI_LOADER_CODE:
                {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_LoaderCode, desc->type);
                }
                break;
                case C_EFI_LOADER_DATA: 
                {
                    _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_LoaderData, desc->type);
                }
                break;
                // merge boot services blocks as well as conventional blocks
                case C_EFI_BOOT_SERVICES_CODE:
                case C_EFI_BOOT_SERVICES_DATA:       
                case C_EFI_CONVENTIONAL_MEMORY:
                {                
                    if (desc->attribute & C_EFI_MEMORY_WB) {
                        if (prev 
                            && 
                            (prev->_start + prev->_size) >= desc->physical_start) {
                                // merge by growing previous region to encompass this one
                                size_t delta = (prev->_start + prev->_size) - desc->physical_start;
                                prev->_size += (desc->number_of_pages * UEFI_POOL_PAGE_SIZE) - delta;
                        }
                        else {
                            _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_RAM, desc->type);
                        }
                    }
                    else {
                        // NOTE: strictly this is not "reserved" but it is memory that is not cacheable, so we don't treat it as usable for CPU I/O 
                        _add_memory_region(desc->physical_start, desc->number_of_pages * UEFI_POOL_PAGE_SIZE, kMemoryRegion_Reserved, desc->type);
                    }
                }
                break;
                default:;
            }
        }
        
        prev = _regions + _num_regions - 1;
        desc = (CEfiMemoryDescriptor*)((uintptr_t)desc + _descriptor_size);
    }
    
    return _JO_STATUS_SUCCESS;
}

//TODO: MERGE all memory boot loader regions obtained from _build_memory_map and create a linked list of usable-RAM pools 

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

_JOS_API_FUNC jo_status_t memory_uefi_init(CEfiBootServices* boot_services) {
    _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init");    
    _get_boot_service_memory_map(boot_services);
    jo_status_t status = _build_memory_map(false);

    if (_JO_SUCCEEDED(status)) {
        size_t total_bytes = 0;
        for (size_t r = 0; r < _num_regions; ++r) {                
            //TODO: use linked regions instead
            if (_regions[r]._type == kMemoryRegion_RAM) {
                total_bytes += _regions[r]._size;
            }
        }
        _JOS_ASSERT(total_bytes >= JOSX_MINIMUM_STARTUP_HEAP_SIZE);
        CEfiPhysicalAddress phys;
        _JOS_ASSERT(!C_EFI_ERROR(boot_services->allocate_pages(C_EFI_ALLOCATE_ANY_PAGES, C_EFI_LOADER_DATA, 
                            JOSX_MINIMUM_STARTUP_HEAP_SIZE/UEFI_POOL_PAGE_SIZE, 
                            &phys)));
        // we create one linear allocator for the initial memory requirements of the kernel
        // which will create allocators inside the memory managed by this allocator                            
        _main_allocator = linear_allocator_create((void*)phys, JOSX_MINIMUM_STARTUP_HEAP_SIZE);
        _JOS_KTRACE_CHANNEL(kMemoryChannel, "uefi init succeeded");
    }

    return status;
}

_JOS_API_FUNC size_t      memory_get_total(void) {
    return (size_t)_main_allocator->_end - (size_t)_main_allocator->_begin;
}

_JOS_API_FUNC size_t memory_get_available(void) {
    return linear_allocator_available(_main_allocator);
}

_JOS_API_FUNC jo_status_t memory_runtime_init(CEfiHandle h, CEfiBootServices* boot_services) {
        
    // from UEFI Spec 2.6:
    // An image that calls ExitBootServices() (i.e., a UEFI OS Loader) first calls
    // EFI_BOOT_SERVICES.GetMemoryMap() to obtain the current memory map. Following the
    // ExitBootServices() call, the image implicitly owns all unused memory in the map. This
    // includes memory types EfiLoaderCode, EfiLoaderData, EfiBootServicesCode,
    // EfiBootServicesData, and EfiConventionalMemory. A UEFI OS Loader and OS
    // must preserve the memory marked as EfiRuntimeServicesCode and
    // EfiRuntimeServicesData.
    //
    _get_boot_service_memory_map(boot_services);
    _build_memory_map(true);
    CEfiStatus status = boot_services->exit_boot_services(h, _map_key);    
    if ( C_EFI_ERROR(status )) {
        return _JO_STATUS_UNAVAILABLE;
    }
    
    size_t max_region = 0;
    for (size_t r = 0; r < _num_regions; ++r) {                
        //TODO: use linked regions instead
        if (_regions[r]._type == kMemoryRegion_RAM) {
            if (_regions[r]._size > max_region) {
                max_region = _regions[r]._size;
            }
        }
    }

    //TODO: create *another* allocator that the kernel will switch to at this point?

    return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC size_t  memory_pool_overhead(memory_pool_type_t type) {
    // overhead is size of allocator structure + max alignment
    switch(type) {
        case kMemoryPoolType_Dynamic:
            return sizeof(arena_allocator_t) + kAllocAlign_8-1;
        case kMemoryPoolType_Static:
            return sizeof(linear_allocator_t) + kAllocAlign_8-1;
        default:;
    }
    return 0;
}

_JOS_API_FUNC generic_allocator_t*  memory_allocate_pool(memory_pool_type_t type, size_t size) {
    
    const size_t overhead = memory_pool_overhead(type); 
    size = size ? size + overhead : memory_get_available() - kAllocAlign_8;
    switch (type) {
        case kMemoryPoolType_Dynamic:
        {
            // standard arena allocator
            _JOS_ASSERT(size<=memory_get_available());
            void* pool = _main_allocator->_super.alloc((generic_allocator_t*)_main_allocator, size);
            return (generic_allocator_t*)arena_allocator_create(pool, size);
        }
        break;
        case kMemoryPoolType_Static:
        {
            // basic linear allocator
            _JOS_ASSERT(size<=memory_get_available());
            void* pool = _main_allocator->_super.alloc((generic_allocator_t*)_main_allocator, size);
            return (generic_allocator_t*)linear_allocator_create(pool, size);
        }
        break;
        default:;
    }

    return 0;
}