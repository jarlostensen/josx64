#ifndef _JOS_KERNEL_PROCESSORS_H_
#define _JOS_KERNEL_PROCESSORS_H_

#include <jos.h>
#include <stdint.h>
#include <c-efi-protocol-multi-processor.h>

typedef struct _local_apic_information {

    uint64_t        _base_address;
    uint32_t        _id;
    uint8_t         _version;
    bool            _enabled;
    bool            _has_x2apic;

} local_apic_information_t;

typedef struct _processor_information {

    CEfiProcessorInformation        _uefi_info;
    uint32_t                        _max_basic_cpuid;
    uint32_t                        _max_ext_cpuid;
    char                            _vendor_string[13];
    char                            _hypervisor_id[13];
    local_apic_information_t        _local_apic_info;
    
    bool                            _has_hypervisor : 1;
    bool                            _has_tsc : 1;
    bool                            _has_msr : 1;
    bool                            _has_local_apic : 1;
    bool                            _is_good : 1;
    bool                            _intel_64_arch : 1;
    bool                            _has_1GB_pages : 1;
    
} processor_information_t;

jo_status_t        processors_initialise();
size_t              processors_get_processor_count();
// gs points to an array of pointers where we stuff relevant information per cpu
#define _JOS_K_PER_CPU_IDX_PROCESSOR_INFO       0
#define _JOS_K_PER_CPU_IDX_TASK_INFO            1

const uintptr_t*     processors_get_per_cpu_ptr(size_t index);

jos_status_t        processors_initialise(void);
size_t              processors_get_processor_count(void);
size_t              processors_get_bsp_id();
jo_status_t        processors_get_processor_information(processor_information_t* out_info, size_t processor_index);
bool                processors_has_acpi_20();
jos_status_t        processors_get_this_processor_info(processor_information_t* out_info);

typedef void (*ap_worker_function_t)(void*);
jo_status_t        processors_startup_aps(ap_worker_function_t ap_worker_function, void* per_ap_data, size_t per_ap_data_stride);

#endif //_JOS_KERNEL_PROCESSORS_H_
