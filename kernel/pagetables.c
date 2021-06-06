
#include <pagetables.h>
#include <jos.h>
#include <x86_64.h>

// this is an excellent article to use as a reference https://blog.llandsmeer.com/tech/2019/07/21/uefi-x64-userland.html

static const char* kPageTablesChannel = "pagetables";

struct _mapping_table {
    uint64_t entries[512];
} _JOS_PACKED;
typedef struct _mapping_table mapping_table_t;

#define PML4_IDX(logical)   ((logical) >> 39) & 0x1ff
#define PDP_IDX(logical)    ((logical) >> 30) & 0x1ff
#define PD_IDX(logical)     ((logical) >> 21) & 0x1ff
#define PT_IDX(logcial)     ((logical) >> 12) & 0x1ff
#define FRAME_IDX           ((logical) & 0x7ff

#define PAGE_ADDR_MASK      0x000ffffffffff000
#define PAGE_FLAGS_MASK     0xfff
#define PAGE_BIT_P_PRESENT (1<<0)
#define PAGE_BIT_RW_WRITABLE (1<<1)
#define PAGE_BIT_US_USER (1<<2)
#define PAGE_XD_NX (1<<63)

static mapping_table_t* _pml4 = 0;

void    pagetables_initialise(void) {

    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "initialising");
    //NOTE: this assumes default identity mapping as initialised by the UEFI boot loader

    _pml4 = (mapping_table_t*)x86_64_get_pml4();
    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "pml4 @ 0x%x", _pml4);
}

uint64_t    pagetables_entry_for_virt(uint64_t virt) {

}

void    pagetables_enable_nullptr_gpf(void) {
    // make sure we pagefault on nullptr access:
    mapping_table_t* pdp = (mapping_table_t*)(_pml4->entries[0] & PAGE_ADDR_MASK);
    mapping_table_t* pd = (mapping_table_t*)(pdp->entries[0] & PAGE_ADDR_MASK);
    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "pd[0] is 0x%x", pdp->entries[0]);
    void* frame = (void*)(pd->entries[0] & PAGE_ADDR_MASK);
    uint16_t frame_flags = (uint64_t)(pd->entries[0] & PAGE_FLAGS_MASK);
    // mark 0th frame as not present, not writable
    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "marking frame %x as non-writable, non accessible", frame);
    pd->entries[0] = 0;
    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "marked");
}

