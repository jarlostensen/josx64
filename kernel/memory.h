#ifndef _JOS_KERNEL_MEMORY_H
#define _JOS_KERNEL_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

/*
    jOS kernel virtual memory map

        |----------------------------------------|
        |   0xfffff000  |   page dir             |
        |----------------------------------------|
        |   0xffc00000  | max  kernel HEAP       |
        |----------------------------------------|
        |   0xfeefffff  |                        |
        |   0xfee00000  | APIC                   |
        |----------------------------------------|
        |   0xfecfffff  |                        |
        |   0xfec00000  | APIC                   |
        |----------------------------------------|        

                    unused

        |----------------------------------------|
        | _k_virt_end+1Gig | ~top of kernel HEAP |
        |                                        |
        |      ........................          |
        |                                        |
        | _k_virt_end   | ~start of kernel HEAP  | 
        |----------------------------------------|
        | _k_virt_end   | end of kernel code     |
        | 0xc0100000    | start of kernel code   |
        |----------------------------------------|

                    unused

        |----------------------------------------|
        | 0x7100000     |                        |
        | 0x7000000     | 1 Meg virtual memory   | < this is used to allocate vmem ranges for whatever needs
        |----------------------------------------|
        
                    unused

        |----------------------------------------|
        | 0x100000      |                        |
        |----------------------------------------|
        | 0xeffff       |                        |
        | 0xe0000       |  part of EBDA          |
        |----------------------------------------| 
        | 0             |  BIOS, low RAM         |
        |----------------------------------------|

*/

// 1/2 Gig suffices for now
#define _JOS_KERNEL_HEAP_SIZE   0x20000000
#define _JOS_KERNEL_PAGE_SIZE   0x1000
#define _JOS_KERNEL_PAGE_MOD    (_JOS_KERNEL_PAGE_SIZE-1)

// we identity map the default IO APIC memory address range
#define _JOS_KERNEL_IO_APIC_VIRT_START 0xfec00000
#define _JOS_KERNEL_IO_APIC_VIRT_END   0xfecfffff

// we identity map the default local APIC memory address range
#define _JOS_KERNEL_LOCAL_APIC_VIRT_START 0xfee00000
#define _JOS_KERNEL_LOCAL_APIC_VIRT_END   0xfeefffff

struct multiboot_info;
// sets up the kernel heap, map various regions required (like APIC), and initialise allocators.
// after this function returns you can call malloc/free etc.
void k_mem_init(struct multiboot_info *mboot);

// reserve an area of virtual memory (this does not back the memory with physical pages)
void*   _k_vmem_reserve_range(size_t pages);
// map a virtual address to physical
void _k_vmem_map(uintptr_t virt, uintptr_t phys);

#endif // _JOS_KERNEL_MEMORY_H