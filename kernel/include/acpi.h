#pragma once
#ifndef _JOS_ACPI_H
#define _JOS_ACPI_H

#include <jos.h>
#include <c-efi-system.h>

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

_JOS_API_FUNC jo_status_t    acpi_intitialise(CEfiSystemTable* st);

_JOS_API_FUNC bool           acpi_v2(void);

#endif // _JOS_ACPI_H