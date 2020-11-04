
#include <jos.h>
#include <string.h>

#include <cpuid.h>
#include <x86_64.h>
#include <collections.h>
#include <processors.h>


// Intel IA dev guide 10-6 VOL 3A
#define LOCAL_APIC_REGISTER_ID      0x20
#define LOCAL_APIC_REGISTER_VERSION 0x30


// in efi_main.c
extern CEfiBootServices * g_boot_services;
static CEfiMultiProcessorProtocol*  _mpp = 0;

static size_t   _bsp_id = 0;
static size_t   _num_processors = 1;
static size_t   _num_enabled_processors = 1;

static processor_information_t* _processors = 0;

static uint32_t read_local_apic_register(processor_information_t* info, uint16_t reg) {
    uint64_t register_address = info->_local_apic_info._base_address | (uint64_t)reg;
    const uint32_t* reg_ptr = (const uint32_t*)register_address;
    return *reg_ptr;
}

static void collect_this_cpu_information(processor_information_t* info) {

    memset(info,0,sizeof(processor_information_t));

    info->_max_basic_cpuid = __get_cpuid_max(0, NULL);
    info->_max_ext_cpuid = __get_cpuid_max(0x80000000, NULL);

    uint32_t eax = 0,ebx,ecx,edx;

    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    memcpy(info->_vendor_string + 0, &ebx, sizeof(ebx));
    memcpy(info->_vendor_string + 4, &edx, sizeof(edx));
    memcpy(info->_vendor_string + 8, &ecx, sizeof(ecx));
    info->_vendor_string[12] = 0;

    __get_cpuid(0x1, &eax, &ebx, &ecx, &edx);
    info->_has_hypervisor = (ecx & (1<<31))!=0;
    if(info->_has_hypervisor) {
        unsigned int regs[4];
        __cpuid(0x40000000, regs[0], regs[1], regs[2], regs[3]);
        info->_hypervisor_id[12] = 0;
        memcpy(info->_hypervisor_id, regs + 1, 3 * sizeof(regs[0]));
    }

    info->_has_local_apic = (edx & (1<<9)) == (1<<9);
    if (info->_has_local_apic) {
        uint32_t apic_lo, apic_hi;
        x86_64_rdmsr(IA32_APIC_BASE_MSR, &apic_lo, &apic_hi);
        info->_local_apic_info._base_address = (((uint64_t)apic_hi << 32) | (uint64_t)apic_lo) & 0xfffff000;
        info->_local_apic_info._id = read_local_apic_register(info, LOCAL_APIC_REGISTER_ID);
        info->_local_apic_info._version = read_local_apic_register(info, LOCAL_APIC_REGISTER_VERSION);
    }
}

// called on each AP
static void collect_ap_information(void* arg) {

    processor_information_t* info = (processor_information_t*)arg;
    collect_this_cpu_information(info);
    info->_is_good = true;
}

k_status    processors_initialise() {

    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer,0,sizeof(handle_buffer));

    CEfiStatus status = g_boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if ( status==C_EFI_SUCCESS ) {

        //TODO: this works but it's not science; what makes one handle a better choice than another? 
        //      we should combine these two tests (handler and resolution) and pick the base from that larger set
        size_t num_handles = handle_buffer_size/sizeof(CEfiHandle);
        for(size_t n = 0; n < num_handles; ++n)
        {
            status = g_boot_services->handle_protocol(handle_buffer[n], &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, (void**)&_mpp);
            if ( status == C_EFI_SUCCESS )
            {
                break;
            }            
        }
        
        if ( status == C_EFI_SUCCESS ) {                
            status = _mpp->who_am_i(_mpp, &_bsp_id);
            if ( status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_INTERNAL;
            }

            status = _mpp->get_number_of_processors(_mpp, &_num_processors, &_num_enabled_processors);
            if ( status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_INTERNAL;
            }

            _processors = (processor_information_t*)malloc(sizeof(processor_information_t) * _num_processors);
            memset(_processors, 0, sizeof(processor_information_t) * _num_processors);
            // collect information about the BSP
            collect_ap_information(&_processors[_bsp_id]);

            for(size_t p = 0; p < _num_processors; ++p) {
                if( p != _bsp_id ) {
                    status = _mpp->get_processor_info(_mpp, p, &_processors[p]._uefi_info);
                    if ( status == C_EFI_SUCCESS ) {
                        // execute the information collect function on this processor
                        //NOTE: infinite timeout....
                        status = _mpp->startup_this_ap(_mpp, collect_ap_information, p, NULL, 0, (void*)(_processors+p), NULL);
                        if ( status != C_EFI_SUCCESS ) {
                            _processors[p]._is_good = false;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // uni processor
        _processors = (processor_information_t*)malloc(sizeof(processor_information_t));
        collect_this_cpu_information(_processors);
        _processors->_is_good = true;
    }

    return _JOS_K_STATUS_SUCCESS;        
}

size_t processors_get_processor_count() {
    return _num_processors;
}

size_t processors_get_bsp_id() {
    return _bsp_id;
}

k_status        processor_get_processor_information(processor_information_t* out_info, size_t processor_index)
{
    if ( processor_index >= _num_processors ) {
        return _JOS_K_STATUS_OUT_OF_RANGE;
    }

    memcpy(out_info, _processors+processor_index, sizeof(processor_information_t));
    return _JOS_K_STATUS_SUCCESS;
}