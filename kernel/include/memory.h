#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <jos.h>

k_status memory_initialise();
CEfiUSize memory_boot_service_get_mapkey();
k_status memory_refresh_boot_service_memory_map();

#endif // _JOS_KERNEL_MEMORY_H