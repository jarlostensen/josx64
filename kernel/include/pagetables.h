#pragma once 

#include <jos.h>

// set up page tables, mark 0th page as inaccessible, etc.
_JOS_API_FUNC void  pagetables_uefi_initialise(void);
_JOS_API_FUNC void pagetables_runtime_init(generic_allocator_t* allocator);
// traverse the page table entries for at and store each in the first four slots of entries
_JOS_API_FUNC void   pagetables_traverse_tables(void* at, uintptr_t * entries, size_t num_entries);
//WIP:
_JOS_API_FUNC int pagetables_protect_page(void* at, int prot_flags);
