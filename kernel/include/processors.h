#ifndef _JOS_KERNEL_PROCESSORS_H_
#define _JOS_KERNEL_PROCESSORS_H_

#include <jos.h>
#include <stdint.h>
#include <c-efi-protocol-multi-processor.h>

typedef struct _local_apic_information {

    uint64_t        _base_address;
    uint32_t        _id;
    uint8_t         _version;

} local_apic_information_t;

typedef struct _processor_information {

    CEfiProcessorInformation        _uefi_info;
    uint32_t                        _max_basic_cpuid;
    uint32_t                        _max_ext_cpuid;
    char                            _vendor_string[13];
    char                            _hypervisor_id[13];
    local_apic_information_t        _local_apic_info;
    
    bool                            _has_hypervisor;
    bool                            _has_local_apic;
    bool                            _is_good;
    
} processor_information_t;

k_status    processors_initialise();
size_t      processors_get_processor_count();
size_t      processors_get_bsp_id();
k_status    processor_get_processor_information(processor_information_t* out_info, size_t processor_index);

#endif //_JOS_KERNEL_PROCESSORS_H_