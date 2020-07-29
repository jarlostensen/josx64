#include <c-efi.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

//#include <internal/include/_stdio.h>

//https://github.com/rust-lang/rust/issues/62785/
// TL;DR linker error we get when building with Clang on Windows 
int _fltused = 0;

static CEfiChar16*   kLoaderHeading = L"| jOSx64 ----------------------------\n\r";

static uint32_t _read_cr4(void)
{
     uint32_t val;
#ifdef __x86_64__
     __asm volatile ( 
         "mov %%cr4, %%rax\n\t"
         "mov %%eax, %0\n\t"
         : "=rm" (val) );
#elif defined(__i386__)
    //TODO:
     __asm volatile ( "mov %%cr4, %0" : "=rm" (val) );
#endif
     return val;
}

CEfiStatus efi_main(CEfiHandle h, CEfiSystemTable *st)
{    
    CEfiStatus status;
    
    st->con_out->clear_screen(st->con_out);
    st->con_out->output_string(st->con_out, kLoaderHeading);

#ifdef _JOS_KERNEL_BUILD
    st->con_out->output_string(st->con_out, L"kernel build\n\r\n\r");
#endif

    //CEfiInputKey key;
    //st->con_out->output_string(st->con_out, L"\n\rPress a key, any key...\n\r"); 
    //CEfiUSize x;
    //st->boot_services->wait_for_event(1, &st->con_in->wait_for_key, &x);

    CEfiUSize map_size = 0;
    CEfiMemoryDescriptor *memory_map = 0;
    CEfiUSize map_key, descriptor_size;
    CEfiU32 descriptor_version;
    st->boot_services->get_memory_map(&map_size, memory_map, 0, &descriptor_size, 0);
    unsigned mem_desc_entries = map_size / descriptor_size;

    // make room for any expansion that might happen when we call "allocate_pool" 
    map_size += 2*descriptor_size;    
    status = st->boot_services->allocate_pool(C_EFI_LOADER_DATA, map_size, (void**)&memory_map);
    st->boot_services->get_memory_map(&map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);

#define _EFI_PRINT(s)\
st->con_out->output_string(st->con_out, s)

    wchar_t buf[256];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);
    swprintf(buf, bufcount, L"There are %d entries in the memory map\n\r", mem_desc_entries);
    _EFI_PRINT(buf);
    // traverse memory map and dump it    
    CEfiMemoryDescriptor* desc = memory_map;
    for ( unsigned i = 0; i < mem_desc_entries; ++i )
    {        
        _EFI_PRINT(buf);
        switch(desc->type)
        {
            case C_EFI_LOADER_CODE:
            {
                _EFI_PRINT(L"Loader code\n\r");
            }
            break;
            case C_EFI_LOADER_DATA:
            {
                _EFI_PRINT(L"Loader data\n\r");
            }
            break;
            case C_EFI_BOOT_SERVICES_CODE:
            {
                _EFI_PRINT(L"Boot services code\n\r");
            }
            break;
            case C_EFI_BOOT_SERVICES_DATA:
            {
                _EFI_PRINT(L"Boot services data\n\r");
            }
            break;
            case C_EFI_RUNTIME_SERVICES_CODE:
            {
                _EFI_PRINT(L"Runtime services code\n\r");
            }
            break;
            case C_EFI_RUNTIME_SERVICES_DATA:
            {
                _EFI_PRINT(L"Runtime services data\n\r");
            }
            break;
            case C_EFI_CONVENTIONAL_MEMORY:
            {
                _EFI_PRINT(L"Conventional memory\n\r");
            }
            break;
            default:
            {
                _EFI_PRINT(L"unhandled\n\r");
            }
            break;            
        }
        
        swprintf(buf, bufcount, L"\ttype 0x%x, starts at 0x%llx, %d pages\n\r", desc->type, desc->physical_start, desc->number_of_pages);
        desc = (CEfiMemoryDescriptor*)((char*)desc + descriptor_size);

        if ( i && i%8==0 )
        {
            CEfiInputKey key;
            st->con_out->output_string(st->con_out, L"\n\rpress any key...\n\r"); 
            CEfiUSize x;
            st->boot_services->wait_for_event(1, &st->con_in->wait_for_key, &x);
        }
    }

    _EFI_PRINT(L"\n\rexiting boot services\n\r");

    // after this point we can no longer use boot services (only runtime)
    status = st->boot_services->exit_boot_services(h, map_key);
    
    _EFI_PRINT(L"Goodbye, we're going to sleep now...\n\r");
    
    while(1)
    {
        __asm volatile ("pause");
    }
    __builtin_unreachable(); 
}
