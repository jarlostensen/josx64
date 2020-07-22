#include <efi.h>
#include <efilib.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>


static inline uint32_t read_cr4(void)
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

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;
    (void)SystemTable;
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    /* Store the system table for future use in other functions */
    ST = SystemTable;

#ifdef __clang_major__
    Status = ST->ConOut->OutputString(ST->ConOut, L"JØS EFI Loader Test (CLANG BUILD)\n\r");
#else
    Status = ST->ConOut->OutputString(ST->ConOut, L"JØS EFI Loader Test\n\r");
#endif
    if (EFI_ERROR(Status))
        return Status;

    while(read_cr4())
    {
        __asm volatile ("pause");
    }
    __builtin_unreachable();    
}
