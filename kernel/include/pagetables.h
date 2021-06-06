#pragma once 

// set up page tables, mark 0th page as inaccessible, etc.
void    pagetables_initialise(void);
// mark page 0 as not-present
void    pagetables_enable_nullptr_gpf(void);

