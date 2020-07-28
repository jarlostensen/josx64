
#include "kernel_detail.h"
#include <stdio.h>

// in link.ld
void _k_phys_end(void);
void _k_phys_start(void);
// in kernel_loader.asm
void _k_frame_alloc_ptr(void);

static uintptr_t _free_ptr = 0;
static size_t _allocated = 0;

void k_alloc_init()
{
    uintptr_t *frame_alloc_ptr = (uintptr_t*)&_k_frame_alloc_ptr;
    //NOTE: virtual address is stored in second entry in kernel_loader.asm
    _free_ptr = frame_alloc_ptr[1];
    _JOS_KTRACE("k_alloc_init. Kernel is %d bytes, heap starts at 0x%x\n", (uintptr_t)&_k_phys_end - (uintptr_t)&_k_phys_start, _free_ptr);
}

void* k_alloc(size_t bytes, alignment_t alignment)
{
    switch(alignment)
    {
        case kAlign16:
        {
            _allocated += _free_ptr & 0x1;
            _free_ptr = (_free_ptr + 1) & ~1;
        }
        break;
        case kAlign32:
        {
            _allocated += _free_ptr & 0x3;
            _free_ptr = (_free_ptr + 3) & ~3;
        }
        break;
        case kAlign64:
        {
            _allocated += _free_ptr & 0x7;
            _free_ptr = (_free_ptr + 7) & ~7;
        }
        break;
        case kAlign128:
        {
            _allocated += _free_ptr & 0xf;
            _free_ptr = (_free_ptr + 0xf) & ~0xf;
        }
        break;
        case kAlign256:
        {
            _allocated += _free_ptr & 0x1f;
            _free_ptr = (_free_ptr + 0x1f) & ~0x1f;
        }
        break;
        case kAlign512:
        {
            _allocated += _free_ptr & 0x1ff;
            _free_ptr = (_free_ptr + 0x1ff) & ~0x1ff;
        }
        break;
        case kAlign4k:
        {
            _allocated += _free_ptr & 0xfff;
            _free_ptr = (_free_ptr + 0xfff) & ~0xfff;
        }
        break;
        default:;
    }
    uint32_t ptr = _free_ptr;
    _free_ptr += bytes;
    _allocated += bytes;
    return (void*)ptr;
}
