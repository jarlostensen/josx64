
#include <pagetables.h>
#include <jos.h>
#include <x86_64.h>
#include <string.h>

// this is an excellent article to use as a reference https://blog.llandsmeer.com/tech/2019/07/21/uefi-x64-userland.html

static const char* kPageTablesChannel = "pagetables";

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
#define PAGE_XD_NX              0x8000000000000000
//NOTE: if in level 3 (pdptt) this creates a 1GB page, if in level 2 (pd) a 2MB page
// see Intel Dev Guide Vol 3 4.5
#define PAGE_HUGE               (1<<7)

// by default pages are present, read/writable, and no-execute
#define PAGE_CREATE_FLAGS       (uintptr_t)(PAGE_FLAGS_MASK | PAGE_BIT_RW_WRITABLE | PAGE_XD_NX)

// https://wiki.osdev.org/CPU_Registers_x86-64#IA32_EFER
#define MSR_IA32_EFER   0xc0000080

typedef struct _page_table {
    uint64_t entries[512];
} _JOS_PACKED  page_table_t;

// the kernel root page table
static page_table_t* _pml4 = 0;

static page_table_t* _allocate_table(generic_allocator_t* allocator) {
    void* base;
    void* table;
    aligned_alloc(allocator, sizeof(page_table_t), kAllocAlign_4k, &base, &table);
    //NOTE: we're not keeping the base, we never intend to free this
    return (page_table_t*)table;
}

static void _map(page_table_t* pml4, uintptr_t phys_start, uintptr_t page_flags) {
    
    uintptr_t offs = PML4_IDX(phys_start);
    pml4->entries[offs] = phys_start | page_flags;
}

_JOS_API_FUNC void    pagetables_uefi_initialise(void) {

    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "initialising");
    //NOTE: this assumes default identity mapping as initialised by the UEFI boot loader

    _pml4 = (page_table_t*)x86_64_get_pml4();
    _JOS_KTRACE_CHANNEL(kPageTablesChannel, "pml4 @ 0x%x", _pml4);

    uint64_t cr0 = x86_64_read_cr0();
    uint64_t cr4 = x86_64_read_cr4();
    uint32_t efer_lo, efer_hi;
    x86_64_rdmsr(MSR_IA32_EFER, &efer_lo, &efer_hi);

    // these should always be true for our 64 bit OS
    bool _4_level_paging = ( (cr0 & (1<<31)) && (cr4 & (1<<5)) ) && (efer_lo & (1<<8));
    // execute disable supported
    bool _nxe = (efer_lo & (1<<11)) == (1<<11);
    _JOS_ASSERT(_4_level_paging && _nxe);
}

_JOS_API_FUNC void pagetables_runtime_init(generic_allocator_t* allocator) {
    
    _pml4 = _allocate_table(allocator);
    // make sure the entries are valid, albeit not-present
    memset(_pml4, 0, sizeof(page_table_t));
    // map pages: 
    //  our default allocation is simply one level in pml4 to cover 
    //  up to 512GB of RAM (who will ever need more than...etc)
    _map(_pml4, 0, PAGE_CREATE_FLAGS);


}

// see for example Intel Dev Guide Vol 3 4.2
_JOS_API_FUNC void    pagetables_traverse_tables(void* at, uintptr_t * entries, size_t num_entries) {
    _JOS_ASSERT(num_entries == 4);
    memset(entries, 0, 4*sizeof(uintptr_t));

    uintptr_t address =(uintptr_t)at;
    page_table_t* cr3 = (page_table_t*)x86_64_get_pml4();
    uintptr_t pml4e = cr3->entries[PML4_IDX(address)];
    entries[0] = pml4e;
    if ( pml4e & PAGE_BIT_P_PRESENT ) {
        page_table_t*  pml4 = (page_table_t*)(pml4e & PAGE_ADDR_MASK);         
        uintptr_t pdpte = pml4->entries[PDPT_IDX(address)];
        entries[1] = pdpte;

        if ( (pdpte & PAGE_HUGE)==0 ) {
            if ( pdpte & PAGE_BIT_P_PRESENT ) {
                page_table_t* pdpt = (page_table_t*)(pdpte & PAGE_ADDR_MASK);
                uintptr_t pde = pdpt->entries[PD_IDX(address)];
                entries[2] = pde;

                if ( (pde & PAGE_HUGE)==0 ) {
                    // 4KB pages
                    page_table_t* pd = (page_table_t*)(pde & PAGE_ADDR_MASK);
                    uintptr_t pte = pd->entries[PT_IDX(address)];
                    entries[3] = pte;
                }
                // else 2MB pages    
            }
        }
        // else 1GB pages
    }
}

static uintptr_t _prot_flags_to_page_flags(uintptr_t page_flags, int prot_flags) {
    
    if ( (prot_flags & PAGE_NOACCESS) ) {
        return 0;
    }

    //ZZZ: uintptr_t page_flags = 0x8000000000000001; // default is present, read only, kernel, no execute

    if ( (prot_flags & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) 
        && 
        (prot_flags & PAGE_READONLY)==0
        ) {
        page_flags |= 0x02; // writable
    }

    if ( prot_flags & (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE) ) {
            page_flags &= ~0x8000000000000000;  // executable
        }

    if ( prot_flags & PAGE_NOCACHE) {
        page_flags |= 0x10; // disable cache
    }

    if ( prot_flags & PAGE_WRITECOMBINE ) {
        page_flags |= 0x8;  // write through
    }
    
    return page_flags;
}

//NOTE: flags from jos.h 
_JOS_API_FUNC int pagetables_protect_page(void* at, int prot_flags) {

    //TODO: sanity check flags

    uintptr_t address =(uintptr_t)at;
    page_table_t* cr3 = (page_table_t*)x86_64_get_pml4();
    uintptr_t pml4e = cr3->entries[PML4_IDX(address)];
    if ( pml4e & PAGE_BIT_P_PRESENT ) {
        page_table_t*  pml4 = (page_table_t*)(pml4e & PAGE_ADDR_MASK);         
        uintptr_t pdpte = pml4->entries[PDPT_IDX(address)];
        
        if ( (pdpte & PAGE_HUGE)==0 ) {
            if ( pdpte & PAGE_BIT_P_PRESENT ) {
                page_table_t* pdpt = (page_table_t*)(pdpte & PAGE_ADDR_MASK);
                uintptr_t pde = pdpt->entries[PD_IDX(address)];
                
                if ( (pde & PAGE_HUGE)==0 ) {
                    // 4KB pages
                    page_table_t* pd = (page_table_t*)(pde & PAGE_ADDR_MASK);
                    uintptr_t pte = pd->entries[PT_IDX(address)];

                    //ZZZ: basically this, but more secure & correct
                    int curr_flags = (int)(pte & 0xff);
                    pte = _prot_flags_to_page_flags(pte, prot_flags);
                    pd->entries[PT_IDX(address)] = pte;
                    //NOTE: this flushes the TLB for this processor only
                    x86_64_flush_tlb_for_address(address);

                    return curr_flags;
                }
                // else 2MB pages    
                // TODO: SET FLAGS pde
            }
        }
        // else 1GB pages
        // TODO: SET FLAGS pdpte
    }
    //ZZZ:
    return 0;
}

