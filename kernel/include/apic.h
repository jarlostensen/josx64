#ifndef _JOS_KERNEL_APIC_H
#define _JOS_KERNEL_APIC_H

#include <processors.h>

// Intel IA dev guide 10-6 VOL 3A

typedef enum _local_apic_register {
    
    kLApic_Reg_Id               = 0x20,
    kLApic_Reg_Version          = 0x30,

    kLApic_Reg_Eoi              = 0xb0,
    kLApic_Reg_Spiv             = 0xf0,
    kLApic_Reg_ErrorStatus      = 0x280,
    kLApic_Reg_LvtCmci          = 0x2f0,
    kLApic_Reg_LvtTimer         = 0x320,
    kLApic_Reg_LvtLint0         = 0x350,
    kLApic_Reg_LvtLint1         = 0x360,
    kLApic_Reg_LvtError         = 0x370,
    kLApic_Reg_TimerInitCount   = 0x380,
    kLApic_Reg_TimerCurrCount   = 0x390,
    kLApic_Reg_TimerDivConfig   = 0x3e0,

} local_apic_register_t;

#define _JOS_K_IA32_APIC_BASE_MSR           0x1b

// invoked internally in processors.c to collect information for each cpu in the system  
void apic_collect_this_cpu_information(processor_information_t* info);

#endif // _JOS_KERNEL_APIC_H