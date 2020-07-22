#include <efi.h>
#include <efilib.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

//https://github.com/rust-lang/rust/issues/62785/
// TL;DR linker error we get when building with Clang on Windows 
int _fltused = 0;

static CHAR16*   kLoaderHeading = L"| jOSx64 ----------------------------\n\r";

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

static EFI_STATUS _wait_for_key(EFI_INPUT_KEY* key)
{
    ST->ConOut->OutputString(ST->ConOut, L"\n\rPress a key, any key...\n\r"); 
    EFI_STATUS status = ST->ConIn->Reset(ST->ConIn, FALSE);
    if (EFI_ERROR(status))
        return status; 
    while ((status = ST->ConIn->ReadKeyStroke(ST->ConIn, key)) == EFI_NOT_READY) ;
    return status;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{    
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    ST = SystemTable;

    ST->ConOut->ClearScreen(ST->ConOut);
    ST->ConOut->OutputString(ST->ConOut, kLoaderHeading);

#ifdef _JOS_KERNEL_BUILD
    ST->ConOut->OutputString(ST->ConOut, L"kernel build\n\r\n\r");
#endif

    //unsigned long cr4 = _read_cr4();
    //Print(L"cr4: 0x%x\n\r", cr4);

    Status = _wait_for_key(&Key);

    ST->ConOut->OutputString(ST->ConOut, L"\n\rGetting the memory map....\n\r");
    UINTN mapSize = 0, mapKey, descriptorSize;
    EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
    UINT32 descriptorVersion;
    EFI_STATUS result = ST->BootServices->GetMemoryMap(&mapSize, memoryMap, NULL, &descriptorSize, NULL);
    mapSize += 2 * descriptorSize;
    result = ST->BootServices->AllocatePool(AllocateAnyPages, mapSize, (void**)&memoryMap);
    //ErrorCheck(result, EFI_SUCCESS);
    result = ST->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

    //ErrorCheck(result, EFI_SUCCESS);
    ST->ConOut->OutputString(ST->ConOut, L"Exiting boot services!\n\r");
    result = ST->BootServices->ExitBootServices(ImageHandle, mapKey);
    //ErrorCheck(result, EFI_SUCCESS);
    ST->ConOut->OutputString(ST->ConOut, L"Goodbye, we're going to sleep now...\n\r");
    
    while(1)
    {
        __asm volatile ("pause");
    }
    __builtin_unreachable(); 
}
