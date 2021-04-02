#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <c-efi.h>
#include <jos.h>
#include <linear_allocator.h>

// used by the kernel UEFI Init code 
jo_status_t     memory_uefi_init(linear_allocator_t** main_allocator);
CEfiUSize       memory_boot_service_get_mapkey(void);
jo_status_t     memory_refresh_boot_service_memory_map(void);
jo_status_t     memory_runtime_init(void);

size_t          memory_get_total(void);
const uint8_t*  memory_get_memory_bitmap(size_t *dim);
void*           memory_alloc(size_t size);

#endif // _JOS_KERNEL_MEMORY_H