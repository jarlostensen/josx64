#pragma once 

#include <jos.h>

// set up page tables, mark 0th page as inaccessible, etc.
_JOS_API_FUNC void  pagetables_initialise(void);
_JOS_API_FUNC bool  pagetables_four_level_paging_enabled(void);
// traverse the page table entries for at and store each in the first four slots of entries
_JOS_API_FUNC void   pagetables_traverse_tables(void* at, uintptr_t * entries, size_t num_entries);
