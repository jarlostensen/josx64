#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <c-efi.h>
#include <jos.h>
#include <linear_allocator.h>

// used by the kernel UEFI Init code 
_JOS_API_FUNC jo_status_t     memory_uefi_init(CEfiBootServices* boot_services, linear_allocator_t** main_allocator);
_JOS_API_FUNC CEfiUSize       memory_boot_service_get_mapkey(void);
_JOS_API_FUNC jo_status_t     memory_refresh_boot_service_memory_map(CEfiBootServices* boot_services);
_JOS_API_FUNC jo_status_t     memory_runtime_init(CEfiHandle h, CEfiBootServices* boot_services);
_JOS_API_FUNC size_t          memory_get_total(void);
_JOS_API_FUNC void            _memory_debugger_dump_map(void);

#endif // _JOS_KERNEL_MEMORY_H
