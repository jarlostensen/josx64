#pragma once

#ifndef _JOS_KERNEL_smp_H_
#define _JOS_KERNEL_smp_H_

#include <jos.h>
#include <stdint.h>
#include <collections.h>
#include <x86_64.h>
#include <c-efi-protocol-multi-processor.h>

typedef struct _local_apic_information {

    uint64_t        _base_address;
    uint32_t        _id;
    uint8_t         _version;
    bool            _enabled;
    bool            _has_x2apic;

} local_apic_information_t;

typedef struct _xsave_information {
    // eax:edx from cpuid 0xd:0
    uint64_t                        _xsave_bitmap;
    uint32_t                        _xsave_area_size;
} xsave_information_t;


typedef struct _processor_information {

    // this is *our* ID, not dependent on any hw ID
    size_t                          _id;    
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
    bool                            _xsave : 1;

    xsave_information_t             _xsave_info;
    
} processor_information_t;

// ==================================================================================================
// per cpu structures are just arrays of N items, where N is the number of processors in the system.
// the unique ID of each CPU is stored in gs:0 and used to index these structures

#define _JOS_K_IA32_FS_BASE             0xc0000100
#define _JOS_K_IA32_GS_BASE             0xc0000101
#define _JOS_K_IA32_KERNEL_GS_BASE      0xc0000102

typedef queue_t*        per_cpu_queue_t;
typedef uintptr_t*      per_cpu_ptr_t;
typedef uint64_t*       per_cpu_qword_t;

per_cpu_ptr_t       per_cpu_create_ptr(void);
per_cpu_queue_t     per_cpu_create_queue(void);
per_cpu_qword_t     per_cpu_create_qword(void);

_JOS_INLINE_FUNC    size_t per_cpu_this_cpu_id(void) {
    uint64_t val;
    x86_64_read_gs(0,&val);
    return val;
}

_JOS_INLINE_FUNC    processor_information_t* per_cpu_this_cpu_info(void) {
    uint32_t lo, hi;
    x86_64_rdmsr(_JOS_K_IA32_GS_BASE, &lo, &hi);
    return (processor_information_t*)((uintptr_t)lo | ((uintptr_t)hi << 32));
}

#define _JOS_PER_CPU_THIS_QUEUE(queue)\
(queue)[per_cpu_this_cpu_id()]

#define _JOS_PER_CPU_THIS_PTR(ptr)\
(ptr)[per_cpu_this_cpu_id()]

#define _JOS_PER_CPU_THIS_QWORD(qword)\
(qword)[per_cpu_this_cpu_id()]

#define _JOS_PER_CPU_PTR(ptr, cpu)\
(ptr)[(cpu)]

// ===============================================================================================

//NOTE: called on the BSP *only*
jo_status_t     smp_initialise(jos_allocator_t* allocator);

size_t          smp_get_processor_count(void);
size_t          smp_get_bsp_id();
jo_status_t     smp_get_processor_information(processor_information_t* out_info, size_t processor_index);
bool            smp_has_acpi_20();
_JOS_INLINE_FUNC jo_status_t     smp_get_this_processor_info(processor_information_t* out_info) {
    return smp_get_processor_information(out_info, per_cpu_this_cpu_id());
}

#endif //_JOS_KERNEL_smp_H_
