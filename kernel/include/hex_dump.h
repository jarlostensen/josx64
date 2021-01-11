#pragma once

#ifndef _JOS_KERNEL_HEX_DUMP_H
#define _JOS_KERNEL_HEX_DUMP_H

#include <jos.h>

enum hex_dump_unit_size
{
	k8bitInt,
	k16bitInt,
	k32bitInt,
	k64bitInt,
};

void hex_dump_mem(void* mem, size_t bytes, enum hex_dump_unit_size unit_size);

#endif // _JOS_KERNEL_HEX_DUMP_H

