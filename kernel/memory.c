#include "kernel_detail.h"
#include <arena_allocator.h>
#include <fixed_allocator.h>
#include "memory.h"
#include "interrupts.h"
#include <collections.h>
#include <multiboot.h>
#include <stdio.h>
#include <string.h>

static const char* kMemChannel = "mem";

// https://wiki.osdev.org/Paging
// NOTE: these are really just different views on the same 32(64)-bit data.

struct page_directory_struct 
{
    uint8_t     _present : 1;
    uint8_t     _read_write:1;
    uint8_t     _user:1;
    uint8_t     _write_through:1;
    uint8_t     _cache_disable:1;
    uint8_t     _accessed:1;
    uint8_t     _zero_1:1;
    uint8_t     _size:1;
    uint8_t     _zero_2:1;
    uint8_t     _system:3;
    uint32_t    _phys_address:20;
}  __attribute__((packed))
page_directory_t;

struct page_table_struct 
{    
    uint8_t     _present : 1;
    uint8_t     _read_write:1;
    uint8_t     _user:1;
    uint8_t     _write_through:1;
    uint8_t     _cached:1;
    uint8_t     _accessed:1;
    uint8_t     _dirty:1;
    uint8_t     _zero:1;
    uint8_t     _global:1;
    uint8_t     _system:3;
    uint32_t    _phys_address:20;
}  __attribute__((packed)) 
page_table_t;

// in link_high.ld
void _k_phys_end(void);
void _k_phys_start(void);
void _k_virt_end(void);
void _k_virt_start(void);

typedef struct page_frame_alloc_struct
{
    uintptr_t   _phys;
    uintptr_t   _virt;
} __attribute__((packed))
page_frame_alloc_t;

// in kernel_loader.asm; points to the physical memory just above the end of the kernel, used for page table allocation
void _k_page_frame_alloc_ptr(void);


static vmem_arena_t*   _vmem_arena = 0;
// pools for different sizes of small allocations: 8, 16, 32, 64 bytes
static vmem_fixed_t* _small_pools[4];
static size_t _num_small_pools = sizeof(_small_pools)/sizeof(_small_pools[0]);

// ====================================================================================

static inline void _flush_tlb_single(uintptr_t addr)
{
   asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

#define PF_PRESENT          0x1
#define PF_WRITE            0x2
#define PF_USER             0x4
#define PF_RESERVED_WRITE   0x8
#define PF_INSTR_FETCH      0x10

void _k_mem_page_fault_handler(uint32_t error_code, uint16_t cs, uint32_t eip)
{    
    unsigned long virt;
    _JOS_BOCHS_DBGBREAK();
    asm volatile ( "mov %%cr2, %0" : "=r"(virt) );
    printf("\npage fault @ 0x%x [0x%x:0x%x] error = 0x%x...", virt, cs,eip, error_code);
}

static const uintptr_t kPageDirVirAddress = 0xfffff000;

uintptr_t _k_mem_virt_to_pt_entry(uintptr_t virt)
{
    // we're using the magic "recursive page directory entry" @ 0xfffff000

    const uintptr_t* page_dir = (const uintptr_t*)kPageDirVirAddress;
    const uintptr_t page_table_index = virt >> 22;
    
    // we can use this up as a virtual address because of the "self" mapping
    if(page_dir[page_table_index] & 1)
    {
        const uintptr_t frame_index = (virt & 0x3fffff) / _JOS_KERNEL_PAGE_SIZE;
        const uintptr_t* page_table = (const uintptr_t*)(0xffc00000 | (page_table_index * _JOS_KERNEL_PAGE_SIZE));
        if(page_table[frame_index] & 1)
        {
            return page_table[frame_index];
        }
    }
    return 0;
}

uintptr_t k_mem_virt_to_phys(uintptr_t virt)
{
    return _k_mem_virt_to_pt_entry(virt) & ~0xfff;
}

// allocate a frame of physical memory from the space just above the end of the kernel
// this is  used to allocate page tables as they are needed for vmem mappings.
static uintptr_t _alloc_phys_kernel_frames(size_t frameCount)
{
    // will map to the above
    static page_frame_alloc_t* _page_frame_alloc_ptr = (page_frame_alloc_t*)&_k_page_frame_alloc_ptr;
    static size_t _avail_frames = 0;
    if(_avail_frames < frameCount)
        return 0;

    uintptr_t phys = _page_frame_alloc_ptr->_phys;
    _page_frame_alloc_ptr->_phys += _JOS_KERNEL_PAGE_SIZE * frameCount;
    _page_frame_alloc_ptr->_virt += _JOS_KERNEL_PAGE_SIZE * frameCount;
    _avail_frames -= frameCount;
    // always 4K aligned, low 12 bits 0
    return phys;
}

void _k_vmem_map(uintptr_t virt, uintptr_t phys)
{
    virt &= ~_JOS_KERNEL_PAGE_MOD;
    phys &= ~_JOS_KERNEL_PAGE_MOD;

    uintptr_t* page_dir = (uintptr_t*)0xfffff000;
    const uintptr_t page_table_index = virt >> 22;
    
    // we can use this up as a virtual address because of the "self" mapping
    if((page_dir[page_table_index] & 1)==0)
    {
        // page table doesn't exist, allocate it and mark it as present
        uintptr_t pt_frame = _alloc_phys_kernel_frames(1);
        page_dir[page_table_index] = pt_frame | 1;
        // flush TLB to make sure it gets reloaded when we next access
        _flush_tlb_single(pt_frame);
        // clear the table itself
        memset((void*)(0xffc00000 | (page_table_index<<12)), 0, _JOS_KERNEL_PAGE_SIZE);
    }

    const uintptr_t frame_index = (virt & 0x3fffff) / _JOS_KERNEL_PAGE_SIZE;
    uintptr_t* page_table = (uintptr_t*)(0xffc00000 | (page_table_index<<12));
    if((page_table[frame_index] & 1)==0)
    {
        page_table[frame_index] = phys | 1;
        _flush_tlb_single(phys);
    }
    else
    {
        // overwrite...
        _JOS_GDB_DBGBREAK();
    }
}

// helper for identity mapping
_JOS_MAYBE_UNUSED static void _identity_map_range(uintptr_t from, uintptr_t to)
{
    for(; from < to; from += _JOS_KERNEL_PAGE_SIZE)
    {
        _k_vmem_map(from,from);
    }
}

/////////////////////////////////////////////////////////////////////////////////////


// set to above the kernel and the initial page directory
static uintptr_t _phys_RAM_safe_start = 0;
static const uintptr_t kHighRAMStart = 0xfebfffff;
static const uintptr_t kVirtMapEnd = 0xffc00000;

// see memory map in memory.h
static const uintptr_t kVmemAllocationRangeStart = 0x7000000;
// 256 pages = 1Meg addressable range
static size_t _vmem_free_pages = 256;
static uintptr_t _vmem_free_ptr;

 void*   _k_vmem_reserve_range(size_t pages)
{
    if(_vmem_free_pages < pages)
    {
        _JOS_KTRACE_CHANNEL(kMemChannel, "no more virtual addresses; asked for %d, have %d\n", pages, _vmem_free_pages);
        _JOS_KERNEL_PANIC();
        return 0;
    }
    
    uintptr_t ptr = _vmem_free_ptr;
    _vmem_free_pages += pages;
    _vmem_free_ptr += (_JOS_KERNEL_PAGE_SIZE * pages);
    return (void*)ptr;
}

// map out one Gig or so starting from where the kernel code ends
static void _init_kernel_heap(size_t availableRAM)
{
    if(availableRAM<_JOS_KERNEL_HEAP_SIZE)
    {
        _JOS_KTRACE_CHANNEL(kMemChannel, "this kernel needs at least %d bytes of available RAM, we only found %d\n", _JOS_KERNEL_HEAP_SIZE, availableRAM);
        k_panic();
    }

    availableRAM = min(_JOS_KERNEL_HEAP_SIZE, availableRAM);
    size_t frames = availableRAM / _JOS_KERNEL_PAGE_SIZE;
    uintptr_t virt = (uintptr_t)&_k_virt_end;
    uintptr_t phys = _phys_RAM_safe_start;
    do
    {
        _k_vmem_map(virt, phys);
        availableRAM -= _JOS_KERNEL_PAGE_SIZE;
        virt += _JOS_KERNEL_PAGE_SIZE;
        phys += _JOS_KERNEL_PAGE_SIZE;
    }
    while(availableRAM > _JOS_KERNEL_PAGE_SIZE 
        && 
        virt < kVirtMapEnd 
        && 
        _phys_RAM_safe_start < kHighRAMStart);
    _JOS_KTRACE_CHANNEL(kMemChannel,"%d bytes in %d frames mapped from virt:[0x%x, 0x%x] -> phys:[0x%x, 0x%x]\n", 
                                                frames*_JOS_KERNEL_PAGE_SIZE, frames, 
                                                (uintptr_t)&_k_virt_end, _phys_RAM_safe_start, virt, phys);

    // go back to page under the limit
    virt -= _JOS_KERNEL_PAGE_SIZE;
    
    // create a master heap using the general purpose arena allocator, round down to nearest page
    size_t arena_size = (virt - (uintptr_t)&_k_virt_end) & (~_JOS_KERNEL_PAGE_MOD);
    _vmem_arena = vmem_arena_create((void*)((uintptr_t)&_k_virt_end), arena_size);
    _JOS_KTRACE_CHANNEL(kMemChannel,"%d KB in %d frames allocated for pools, starts at 0x%x, ends at 0x%x\n", 
                                    arena_size/1024, arena_size/_JOS_KERNEL_PAGE_SIZE, (uintptr_t)&_k_virt_end, (uintptr_t)virt);
    
    // now allocate some fixed-size pools for small-ish allocations out of the main heap
#define CREATE_SMALL_POOL(i, size, pow2)\
    _small_pools[i] = vmem_fixed_create(vmem_arena_alloc(_vmem_arena,size), size, pow2);\
    _JOS_KTRACE_CHANNEL(kMemChannel, "%d bytes in fixed %d element size pool\n", size, 1<<pow2)

    CREATE_SMALL_POOL(0, 512*8, 3);
    CREATE_SMALL_POOL(1, 512*16, 4);
    CREATE_SMALL_POOL(2, 512*32, 5);
    CREATE_SMALL_POOL(3, 512*64, 6);
}

void k_mem_init(struct multiboot_info *mboot)
{
    // end of kernel + 4Megs to skip past area used by page tables
    _phys_RAM_safe_start = (uintptr_t)&_k_phys_end + 0x400000;
    _vmem_free_ptr = kVmemAllocationRangeStart;

    _JOS_KTRACE_CHANNEL(kMemChannel, "%d pages of virtual memory range available for mapping @ 0x%x\n", _vmem_free_pages, _vmem_free_ptr);
    _JOS_KTRACE_CHANNEL(kMemChannel, "kernel page size is %d bytes\n", _JOS_KERNEL_PAGE_SIZE);

    if(mboot->flags & MULTIBOOT_INFO_MEMORY)
    {
        _JOS_KTRACE_CHANNEL(kMemChannel,"mem_lower = %d KB, mem_upper = %d KB\n", mboot->mem_lower, mboot->mem_upper);
        
        // look for available main RAM so that we can create a heap
        size_t avail = 0;
        for (multiboot_memory_map_t* mmap = (multiboot_memory_map_t *) mboot->mmap_addr; 
            (unsigned long) mmap <mboot->mmap_addr + mboot->mmap_length; 
            mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + mmap->size + sizeof (mmap->size)))
        {
            if(mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
            {                
                _JOS_KTRACE_CHANNEL(kMemChannel,"available 0x%x bytes @ 0x%x\n",
                            (unsigned) mmap->len,
                            (unsigned) mmap->addr);
                // just look for the region in which our phys address lives
                if( _phys_RAM_safe_start > mmap->addr && _phys_RAM_safe_start < mmap->addr+mmap->len)
                {
                    avail = (size_t)(mmap->addr+mmap->len) - (size_t)_phys_RAM_safe_start;                    
                    break;
                }
            }
        }

        // see for example Intel Multiprocessor specification, Table 3-1.
        // IO APIC
        _identity_map_range(_JOS_KERNEL_IO_APIC_VIRT_START, _JOS_KERNEL_IO_APIC_VIRT_END);
        // local APIC
        _identity_map_range(_JOS_KERNEL_LOCAL_APIC_VIRT_START, _JOS_KERNEL_LOCAL_APIC_VIRT_END);
        
        // set up our heap and map it to the virtual range just above the kernel
        _init_kernel_heap(avail);
    }
    else
    {
        _JOS_KTRACE("error: no available RAM or RAM information\n");
        _JOS_KERNEL_PANIC();
    }
}

void* k_mem_alloc(size_t size)
{
    if(!size)
        return 0;
    void* ptr = 0;
    
    if(size>64)
    {
        ptr = vmem_arena_alloc(_vmem_arena, size);         
    }
    else
    {
        size_t pow2 = 3;
        do
        {
            if(size <= (1u<<pow2))
            {
                vmem_fixed_t* pool = _small_pools[pow2-3];
                ptr = vmem_fixed_alloc(pool,size);
                break;
            }
            ++pow2;
        }
        while(pow2 < 12);        
    }

    if(!ptr)
    {
        _JOS_KTRACE_CHANNEL(kMemChannel, "Out-of-memory requesting %d bytes. Arena has %d bytes left\n", size, _vmem_arena->_size);
        _JOS_KERNEL_PANIC();
    }

    return ptr;
}

void k_mem_free(void* ptr)
{
    if(!ptr)
        return;
    for(size_t n = 0; n < _num_small_pools; ++n)
    {
        if(vmem_fixed_in_pool(_small_pools[n],ptr))
        {
            vmem_fixed_free(_small_pools[n], ptr);
            return;
        }
    }
    vmem_arena_free(_vmem_arena, ptr);
}