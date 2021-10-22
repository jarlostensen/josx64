#include "acpi.h"
#include <string.h>
#include <kernel.h>

#include <stdio.h>
#include <output_console.h>


static const rsdp_descriptor20_t*   _rsdp_desc_20 = 0;
static const rsdp_descriptor_t*     _rsdp_desc_10 = 0;
static const _xsdt_header_t*        _xsdt = 0;

bool do_checksum(const uint8_t*ptr, size_t length) {
    uint8_t checksum = 0;
    while(length) {
        checksum += *ptr++;
        --length;
    }
    return checksum==0;
}

static char kRSDPSignature[8] = {'R','S','D',' ','P','T','R',' '};
#define C_EFI_ACPI_1_0_GUID         C_EFI_GUID(0xeb9d2d30, 0x2d88, 0x11d3, 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d)
#define C_EFI_ACPI_2_0_GUID         C_EFI_GUID(0x8868e871, 0xe4f1, 0x11d3, 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81)

#define MAKE_HEADER_TAG(a, b, c, d)\
    (a) | ((b) << 8) | ((c) << 16) | ((d) << 24)

// see https://wiki.osdev.org/XSDT
#define ACPI_APIC_TAG MAKE_HEADER_TAG('A','P','I','C')
#define ACPI_HPET_TAG MAKE_HEADER_TAG('H','P','E','T')
#define ACPI_BGRT_TAG MAKE_HEADER_TAG('B','G','R','T')

_JOS_API_FUNC jo_status_t    acpi_intitialise(CEfiSystemTable* st) {
    
    CEfiConfigurationTable* config_tables = (CEfiConfigurationTable*)st->configuration_table;
    hive_set(kernel_hive(), "acpi:config_table_entries", HIVE_VALUE_INT(st->number_of_table_entries), HIVE_VALUELIST_END);

    for(size_t n = 0; n < st->number_of_table_entries; ++n) {

        if ( memcmp(&C_EFI_ACPI_2_0_GUID, &config_tables[n].vendor_guid, sizeof(CEfiGuid))==0 ) {

            _rsdp_desc_20 = (const rsdp_descriptor20_t*)config_tables[n].vendor_table;
            // check RSD signature (we still need to, don't trust anyone)
            if ( memcmp(_rsdp_desc_20->_rsdp_descriptor._signature, kRSDPSignature, sizeof(kRSDPSignature))==0 ) {
                _xsdt = (const _xsdt_header_t*)_rsdp_desc_20->_xsdt_address;
                if ( do_checksum((const uint8_t*)&_xsdt->_std, _xsdt->_std._length) ) {
                    size_t total = (_xsdt->_std._length - sizeof(acpi_sdt_header_t)) / 8;
                    hive_set(kernel_hive(), "sdt:2.0", HIVE_VALUE_PTR(_rsdp_desc_20), HIVE_VALUE_INT(total), HIVE_VALUELIST_END);

                    const char* header_ptrs = (const char*)_xsdt + sizeof(acpi_sdt_header_t);
                    while(total) {
                        acpi_sdt_header_t* sdt = (acpi_sdt_header_t*)((uintptr_t)(*(uint64_t*) header_ptrs));

                        const uint32_t sig = *(const uint32_t*)sdt->_signature;
                        switch(sig) {
                            case ACPI_APIC_TAG:
                                hive_set(kernel_hive(), "APIC", HIVE_VALUELIST_END);
                            break;
                            case ACPI_HPET_TAG:
                                hive_set(kernel_hive(), "HPET", HIVE_VALUELIST_END);
                            break;
                            case ACPI_BGRT_TAG:
                                hive_set(kernel_hive(), "BGRT", HIVE_VALUELIST_END);
                            break;
                            default:;
                        }
                        header_ptrs += sizeof(uint64_t*);
                        --total;
                    }
                }
            }
        }
        else if ( memcmp(&C_EFI_ACPI_1_0_GUID, &config_tables[n].vendor_guid, sizeof(CEfiGuid))==0 ) {

            _rsdp_desc_10 = (const rsdp_descriptor_t*)config_tables[n].vendor_table;
            // check RSD signature (we still need to, don't trust anyone)
            if ( memcmp(_rsdp_desc_10->_signature, kRSDPSignature, sizeof(kRSDPSignature))==0 ) {
                hive_set(kernel_hive(), "sdt:1.0", HIVE_VALUE_PTR(_rsdp_desc_10), HIVE_VALUELIST_END);
           }
        }
    }
    
    return _JO_STATUS_SUCCESS;    
}
