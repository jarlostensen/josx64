#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <c-efi.h>
#include <jos.h>

typedef enum _memory_pool_type {

    // pool supports arbitrarily sized allocations and frees
    kMemoryPoolType_Dynamic,
    // pool supports arbitraritly sized allocations only
    kMemoryPoolType_Static

} memory_pool_type_t;

_JOS_API_FUNC jo_status_t     memory_uefi_init(CEfiBootServices* boot_services);
_JOS_API_FUNC jo_status_t     memory_runtime_init(CEfiHandle h, CEfiBootServices* boot_services);
// total memory is total usable system RAM 
_JOS_API_FUNC size_t          memory_get_total(void);
// available memory is memory left for creating new pools
_JOS_API_FUNC size_t          memory_get_available(void);
// allocate a memory pool and allocator of a particular type
_JOS_API_FUNC jos_allocator_t*  memory_allocate_pool(memory_pool_type_t type, size_t size);

_JOS_API_FUNC void            _memory_debugger_dump_map(void);

#endif // _JOS_KERNEL_MEMORY_H
