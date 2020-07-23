#include <c-efi.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

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

    //unsigned long cr4 = _read_cr4();
    //Print(L"cr4: 0x%x\n\r", cr4);

    CEfiInputKey key;
    st->con_out->output_string(st->con_out, L"\n\rPress a key, any key...\n\r"); 
    CEfiUSize x;
    st->boot_services->wait_for_event(1, &st->con_in->wait_for_key, &x);

    st->con_out->output_string(st->con_out, L"\n\rGetting the memory map....\n\r");

    CEfiUSize map_size = 0;
    CEfiMemoryDescriptor *memory_map = 0;
    CEfiUSize map_key, descriptor_size;
    CEfiU32 descriptor_version;
    st->boot_services->get_memory_map(&map_size, memory_map, 0, &descriptor_size, 0);
    map_size += 2*descriptor_size;
    status = st->boot_services->allocate_pool(C_EFI_CONVENTIONAL_MEMORY, map_size, (void**)&memory_map);

    st->boot_services->get_memory_map(&map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);

    //ErrorCheck(result, EFI_SUCCESS);
    st->con_out->output_string(st->con_out, L"Exiting boot services!\n\r");
    status = st->boot_services->exit_boot_services(h, map_key);
    
    st->con_out->output_string(st->con_out, L"Goodbye, we're going to sleep now...\n\r");
    
    while(1)
    {
        __asm volatile ("pause");
    }
    __builtin_unreachable(); 
}
