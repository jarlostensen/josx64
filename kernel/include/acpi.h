#pragma once
#ifndef _JOS_ACPI_H
#define _JOS_ACPI_H

#include <jos.h>
#include <c-efi-system.h>

_JOS_API_FUNC jo_status_t    acpi_intitialise(CEfiSystemTable* st);

_JOS_API_FUNC bool           acpi_v2(void);

#endif // _JOS_ACPI_H