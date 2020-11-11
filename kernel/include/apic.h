#ifndef _JOS_KERNEL_APIC_H
#define _JOS_KERNEL_APIC_H

#include <processors.h>

// Intel IA dev guide 10-6 VOL 3A
#define _JOS_K_LOCAL_APIC_REGISTER_ID       0x20
#define _JOS_K_LOCAL_APIC_REGISTER_VERSION  0x30
#define _JOS_K_LOCAL_APIC_REGISTER_SPIV     0xf0
#define _JOS_K_IA32_APIC_BASE_MSR           0x1b

// invoked internally in processors.c to collect information for each cpu in the system  
void apic_collect_this_cpu_information(processor_information_t* info);

#endif // _JOS_KERNEL_APIC_H