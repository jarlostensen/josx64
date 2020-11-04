#ifndef _JOS_KERNEL_PROCESSORS_H_
#define _JOS_KERNEL_PROCESSORS_H_

#include <jos.h>
#include <stdint.h>

k_status    processors_initialise();
size_t      processors_get_processor_count();

#endif //_JOS_KERNEL_PROCESSORS_H_