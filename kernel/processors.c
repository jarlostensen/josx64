
#include <jos.h>
#include <string.h>

#include <cpuid.h>
#include <x86_64.h>
#include <collections.h>
#include <processors.h>
#include <apic.h>


// in efi_main.c
extern CEfiBootServices * g_boot_services;
static CEfiMultiProcessorProtocol*  _mpp = 0;

static size_t   _bsp_id = 0;
static size_t   _num_processors = 1;
static size_t   _num_enabled_processors = 1;

static processor_information_t* _processors = 0;

// used as a placeholder event for 
static CEfiEvent    _ap_event = 0;

// ===================================================================================================
// TODO: move this into a separate acpi module, should be "internal"?

extern CEfiSystemTable*    g_st;
static char kRSDPSignature[8] = {'R','S','D','P',' ','P','T','R'};

// https://wiki.osdev.org/RSDP
typedef struct _rsdp_descriptor {
    char        _signature[8];
    uint8_t     _checksum;
    char        _oem_id[6];
    uint8_t     _revision;
    uint32_t    _rsdt_address;
} _JOS_PACKED_ rsdp_descriptor_t;

typedef struct _rsdp_descriptor20 {

    rsdp_descriptor_t   _rsdp_descriptor;
    uint32_t            _length;
    uint64_t            _xsdt_address;
    uint8_t             _checksum;
    uint8_t             _reserved[3];

} _JOS_PACKED_ rsdp_descriptor20_t;

// see for example: https://wiki.osdev.org/XSDT 
typedef struct _acpi_sdt_header {

    char        _signature[4];
    uint32_t    _length;
    uint8_t     _revision;
    uint8_t     _checksum;
    char        _oem_id[6];
    char        _oem_table_id[8];
    uint32_t    _oem_revision;
    uint32_t    _creator_id;
    uint32_t    _creator_revision;

} _JOS_PACKED_ acpi_sdt_header_t;

typedef struct _xsdt_header {

    acpi_sdt_header_t   _std;
    uint64_t*           _table_ptr;

} _JOS_PACKED_ _xsdt_header_t;

const rsdp_descriptor20_t*  _rsdp_desc_20 = 0;
const _xsdt_header_t*       _xsdt = 0;

bool do_checksum(const uint8_t*ptr, size_t length) {
    uint8_t checksum = 0;
    while(length) {
        checksum += *ptr++;
        --length;
    }
    return checksum==0;
}

#define C_EFI_ACPI_1_0_GUID         C_EFI_GUID(0xeb9d2d30, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d)
#define C_EFI_ACPI_2_0_GUID         C_EFI_GUID(0x8868e871, 0xe4f1, 0x11d3, 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81)

jos_status_t    intitialise_acpi() {
    
    // we require ACPI 2.0 
    CEfiConfigurationTable* config_tables = (CEfiConfigurationTable*)g_st->configuration_table;
    for(size_t n=0; n < g_st->number_of_table_entries; ++n) {

        if ( memcmp(&C_EFI_ACPI_2_0_GUID, &config_tables[n].vendor_guid, sizeof(CEfiGuid))==0 ) {

            _rsdp_desc_20 = (const rsdp_descriptor20_t*)config_tables[n].vendor_table;
            // check RSDP signature (we still need to, don't trust anyone)
            if ( memcmp(_rsdp_desc_20->_rsdp_descriptor._signature, kRSDPSignature, sizeof(kRSDPSignature))==0 ) {
                break;
            }
        }
    }

    if ( _rsdp_desc_20 ) {
        _xsdt = (const _xsdt_header_t*)_rsdp_desc_20->_xsdt_address;
        if ( do_checksum((const uint8_t*)&_xsdt->_std, _xsdt->_std._length) ) {
            return _JOS_K_STATUS_SUCCESS;
        }
        _xsdt = 0;
    }

    return _JOS_K_STATUS_NOT_FOUND;
}

// ==================================================================================================


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
    
    info->_has_tsc = (edx & 0x10);
    info->_has_msr = (edx & 0x20);

    //NOTE: this should ALWAYS be true for x64
    info->_has_local_apic = (edx & (1<<9)) == (1<<9);
    if (info->_has_local_apic) {
        apic_collect_this_cpu_information(info);
        info->_local_apic_info._has_x2apic = (ecx & (1<<21)) == (1<<21);
    }

    //NOTE: if x2APIC is supported we can use 1b or b CPUID functions for topology information as well
    
    info->_is_good = true;
}

// wrapper for UEFI callback protocol
static void collect_ap_information(void* arg) {

    collect_this_cpu_information((processor_information_t*)arg);
}

jos_status_t    processors_initialise() {

    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer,0,sizeof(handle_buffer));

    CEfiStatus efi_status = g_boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if ( efi_status==C_EFI_SUCCESS ) {

        //TODO: this works but it's not science; what makes one handle a better choice than another? 
        //      
        size_t num_handles = handle_buffer_size/sizeof(CEfiHandle);
        for(size_t n = 0; n < num_handles; ++n)
        {
            efi_status = g_boot_services->handle_protocol(handle_buffer[n], &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, (void**)&_mpp);
            if ( efi_status == C_EFI_SUCCESS )
            {
                break;
            }            
        }
        
        if ( efi_status == C_EFI_SUCCESS ) {                
            efi_status = _mpp->who_am_i(_mpp, &_bsp_id);
            if ( efi_status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_INTERNAL;
            }

            efi_status = _mpp->get_number_of_processors(_mpp, &_num_processors, &_num_enabled_processors);
            if ( efi_status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_INTERNAL;
            }

            _processors = (processor_information_t*)malloc(sizeof(processor_information_t) * _num_processors);
            memset(_processors, 0, sizeof(processor_information_t) * _num_processors);
            // collect information about the BSP
            collect_ap_information(&_processors[_bsp_id]);

            for(size_t p = 0; p < _num_processors; ++p) {
                if( p != _bsp_id ) {
                    // note that this only works for processors that support x2APIC (i.e. 1f and b CPUID leafs)
                    efi_status = _mpp->get_processor_info(_mpp, p, &_processors[p]._uefi_info);
                    if ( efi_status == C_EFI_SUCCESS ) {
                        // execute the information collect function on this processor
                        //NOTE: infinite timeout here because the callback is quick, if that changes this has to be re-considered
                        efi_status = _mpp->startup_this_ap(_mpp, collect_ap_information, p, NULL, 0, (void*)(_processors+p), NULL);
                        if ( efi_status != C_EFI_SUCCESS ) {
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

    // ACPI 
    jos_status_t status = intitialise_acpi();
    if ( !_JOS_K_SUCCEEDED(status) ) {
        return status;
    }

    return _JOS_K_STATUS_SUCCESS;        
}

size_t processors_get_processor_count() {
    return _num_processors;
}

size_t processors_get_bsp_id() {
    return _bsp_id;
}

jos_status_t        processors_get_processor_information(processor_information_t* out_info, size_t processor_index)
{
    if ( processor_index >= _num_processors ) {
        return _JOS_K_STATUS_OUT_OF_RANGE;
    }

    memcpy(out_info, _processors+processor_index, sizeof(processor_information_t));
    return _JOS_K_STATUS_SUCCESS;
}

bool processors_has_acpi_20() {
    return _xsdt != 0;
}

jos_status_t        processors_startup_aps(ap_worker_function_t ap_worker_function, void* per_ap_data_, size_t per_ap_data_stride) {

    if(!g_boot_services) {
        return _JOS_K_STATUS_PERMISSION_DENIED;
    }

    if (!ap_worker_function || (per_ap_data_ && !per_ap_data_stride) || (!per_ap_data_ && per_ap_data_stride)) {
        return _JOS_K_STATUS_FAILED_PRECONDITION;
    }

    if ( _num_enabled_processors==1 ) {
        return _JOS_K_STATUS_RESOURCE_EXHAUSTED;
    }

    if ( _ap_event ) {
        // can't do this twice, there's only one try
        return _JOS_K_STATUS_PERMISSION_DENIED;
    }

    CEfiStatus efi_status;
    // efi_status = g_boot_services->create_event(C_EFI_EVT_RUNTIME,  C_EFI_TPL_APPLICATION, NULL, NULL, &_ap_event);
    // if ( efi_status != C_EFI_SUCCESS ) {
    //     return _JOS_K_STATUS_INTERNAL;
    // }

    CEfiUSize this_id;
    _mpp->who_am_i(_mpp, &this_id);
    if ( this_id!=_bsp_id ) {
        return _JOS_K_STATUS_PERMISSION_DENIED;
    }

    uint8_t* per_ap_data = (uint8_t*)per_ap_data_;
    for(size_t p = 0; p < _num_processors; ++p) {
        if( p != _bsp_id ) {

            // execute the information collect function on this processor
            efi_status = _mpp->startup_this_ap(_mpp, ap_worker_function, p, NULL, 100, (void*)(per_ap_data), NULL);
            if ( efi_status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_CANCELLED;
            }
        }

        per_ap_data += per_ap_data_stride;
    }

    return _JOS_K_STATUS_SUCCESS;
}