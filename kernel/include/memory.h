#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <jos.h>

k_status memory_pre_exit_bootservices_initialise();
CEfiUSize memory_boot_service_get_mapkey();
k_status memory_refresh_boot_service_memory_map();
k_status memory_post_exit_bootservices_initialise();

#endif // _JOS_KERNEL_MEMORY_H