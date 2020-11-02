#include <c-efi.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <jos.h>
#include <kernel/video.h>
#include <memory.h>

#include "../font8x8/font8x8_basic.h"

//https://github.com/rust-lang/rust/issues/62785/
// TL;DR linker error we get when building with Clang on Windows 
int _fltused = 0;

CEfiSystemTable*    g_st = 0;
// used from various startup modules (pre-ExitBootServices, after it will be set to 0 again)
CEfiBootServices * g_boot_services = 0;

static CEfiChar16*   kLoaderHeading = L"| jOSx64 ----------------------------\n\r";
static wchar_t*      kTestString = L"Hello, this is a test string\n\r";

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

#define _EFI_PRINT(s)\
g_st->con_out->output_string(g_st->con_out, s)

void halt_cpu() {
    while(1)
    {
        __asm volatile ("pause");
    } 
    __builtin_unreachable();
}

void exit_boot_services(CEfiHandle h) {

    memory_refresh_boot_service_memory_map();
    CEfiStatus status = g_boot_services->exit_boot_services(h, memory_boot_service_get_mapkey());
    if ( C_EFI_ERROR(status )) {
        _EFI_PRINT(L"***FATAL ERROR: Unable to exit boot services. Halting.\n\r");
        halt_cpu();
    }    
    g_boot_services = 0;    
}

CEfiStatus efi_main(CEfiHandle h, CEfiSystemTable *st)
{    
    CEfiStatus status;

    wchar_t buf[256];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

    g_st = st;
    g_boot_services = st->boot_services;

    k_status k_stat = memory_pre_exit_bootservices_initialise();
    if ( _JOS_K_FAILED(k_stat) ) {
        swprintf(buf, bufcount, L"***FATAL ERROR: memory initialise returned 0x%x\n\r", k_stat);
        _EFI_PRINT(buf);
        halt_cpu();
    }

    status = video_initialise();
    if ( status!=C_EFI_SUCCESS ) {

        swprintf(buf, bufcount, L"***FATAL ERROR: video initialise returned 0x%x\n\r", status);
        _EFI_PRINT(buf);
        halt_cpu();
    }
    else
    {   
        video_clear_screen(0x6495ed);
        st->con_out->output_string(st->con_out, kLoaderHeading);
    }

    video_draw_text_segment(&(draw_text_segment_args_t){
        .colour = 0xffffffff,
        .bg_colour = 0x11223344,
        .font_ptr = font8x8_basic,
        .left = 100,
        .top = 100,
        .seg_len = wcslen(kTestString),        
    },
    kTestString);

#ifdef _JOS_KERNEL_BUILD
    st->con_out->output_string(st->con_out, L"kernel build\n\r\n\r");
#endif

    // after this point we can no longer use boot services (only runtime)
    
    _EFI_PRINT(L"Goodbye, we're going to sleep now...\n\r");
    exit_boot_services(h);

    k_stat = memory_post_exit_bootservices_initialise();
    
    __builtin_unreachable(); 
}
