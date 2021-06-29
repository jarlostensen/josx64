
#include <pagetables.h>
#include <jos.h>
#include <x86_64.h>
#include <string.h>

// this is an excellent article to use as a reference https://blog.llandsmeer.com/tech/2019/07/21/uefi-x64-userland.html

static const char* kPageTablesChannel = "pagetables";

struct _mapping_table {
    uint64_t entries[512];
} _JOS_PACKED;
typedef struct _mapping_table mapping_table_t;

#define PML4_IDX(logical)   (((logical) >> 39) & 0x1ff)
#define PDPT_IDX(logical)   (((logical) >> 30) & 0x1ff)
#define PD_IDX(logical)     (((logical) >> 21) & 0x1ff)
#define PT_IDX(logical)     (((logical) >> 12) & 0x1ff)

#define PHYS_1GB(logical)   (((logical) >> 30) & 0x1ff)
#define PHYS_2MB(logical)   (((logical) >> 21) & 0x1ff)

#define PAGE_ADDR_MASK          0x000ffffffffff000
#define PAGE_FLAGS_MASK         0xfff
#define PAGE_BIT_P_PRESENT      (1<<0)
#define PAGE_BIT_RW_WRITABLE    (1<<1)
#define PAGE_BIT_US_USER        (1<<2)
#define PAGE_XD_NX              (1<<63)
//NOTE: if in level 3 (pdptt) this creates a 1GB page, if in level 2 (pd) a 2MB page
// see Intel Dev Guide 4.5
#define PAGE_HUGE               (1<<7)

// https://wiki.osdev.org/CPU_Registers_x86-64#IA32_EFER
#define MSR_IA32_EFER   0xc0000080

static mapping_table_t* _pml4 = 0;
static bool _4_level_paging = false;

_JOS_API_FUNC bool pagetables_four_level_paging_enabled(void) {
    return _4_level_paging;
}

_JOS_API_FUNC void    pagetables_initialise(void) {

    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "initialising");
    //NOTE: this assumes default identity mapping as initialised by the UEFI boot loader

    _pml4 = (mapping_table_t*)x86_64_get_pml4();
    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "pml4 @ 0x%x", _pml4);

    uint64_t cr0 = x86_64_read_cr0();
    uint64_t cr4 = x86_64_read_cr4();
    uint32_t efer_lo, efer_hi;
    x86_64_rdmsr(MSR_IA32_EFER, &efer_lo, &efer_hi);

    _4_level_paging = ( (cr0 & (1<<31)) && (cr4 & (1<<5)) ) && (efer_lo & (1<<8));
}

_JOS_API_FUNC void    pagetables_traverse_tables(void* at, uintptr_t * entries, size_t num_entries) {
    _JOS_ASSERT(entries &&_4_level_paging && num_entries>=4);
    
    memset(entries, 0, 4*sizeof(uintptr_t));

    uintptr_t address =(uintptr_t)at;
    mapping_table_t* cr3 = (mapping_table_t*)x86_64_get_pml4();
    uintptr_t pml4e = cr3->entries[PML4_IDX(address)];
    entries[0] = pml4e;
    if ( pml4e & PAGE_BIT_P_PRESENT ) {
        mapping_table_t*  pml4 = (mapping_table_t*)(pml4e & PAGE_ADDR_MASK);         
        uintptr_t pdpte = pml4->entries[PDPT_IDX(address)];
        entries[1] = pdpte;

        if ( (pdpte & PAGE_HUGE)==0 ) {
            if ( pdpte & PAGE_BIT_P_PRESENT ) {
                mapping_table_t* pdpt = (mapping_table_t*)(pdpte & PAGE_ADDR_MASK);
                uintptr_t pde = pdpt->entries[PD_IDX(address)];
                entries[2] = pde;

                if ( (pde & PAGE_HUGE)==0 ) {
                    // 4KB pages
                    mapping_table_t* pd = (mapping_table_t*)(pde & PAGE_ADDR_MASK);
                    uintptr_t pte = pd->entries[PT_IDX(address)];
                    entries[3] = pte;
                }
                // else 2MB pages    
            }
        }
        // else 1GB pages
    }
}

