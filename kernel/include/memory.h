#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <c-efi.h>
#include <jos.h>
#include <stdint.h>

jo_status_t memory_pre_exit_bootservices_initialise();
CEfiUSize memory_boot_service_get_mapkey();
jo_status_t memory_refresh_boot_service_memory_map();
jo_status_t memory_post_exit_bootservices_initialise();

const uint8_t* memory_get_memory_bitmap(size_t *dim);

#endif // _JOS_KERNEL_MEMORY_H