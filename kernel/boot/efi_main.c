#include <c-efi.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <jos.h>
#include <kernel/video.h>
#include <output_console.h>
#include <memory.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../stb/stb_image_resize.h"
#include "../font8x8/font8x8_basic.h"

//https://github.com/rust-lang/rust/issues/62785/
// TL;DR linker error we get when building with Clang on Windows 
int _fltused = 0;

CEfiSystemTable*    g_st = 0;
// used from various startup modules (pre-ExitBootServices, after it will be set to 0 again)
CEfiBootServices * g_boot_services = 0;

static CEfiChar16*   kLoaderHeading = L"| jOSx64 ----------------------------\n\r";
static wchar_t*      kTestString = L"Hello, this is a test string\nAnd this is another line....\n";

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
        output_console_set_colour(video_make_color(0xff,0,0));        
        output_console_output_string(L"***FATAL ERROR: Unable to exit boot services. Halting.\n");
        halt_cpu();
    }    
    g_boot_services = 0;    

    k_status k_stat = memory_post_exit_bootservices_initialise();
    if ( _JOS_K_FAILED(k_stat) ) {
        output_console_set_colour(video_make_color(0xff,0,0));        
        output_console_output_string(L"***FATAL ERROR: post memory exit failed. Halting.\n");
        halt_cpu();
    }
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

    video_clear_screen(0x6495ed);
    output_console_initialise();
    output_console_set_font((const uint8_t*)font8x8_basic, 8,8);
    output_console_set_colour(0xffffffff);
    output_console_set_bg_colour(0x11223344);
    output_console_output_string(kTestString);

#ifdef _JOS_KERNEL_BUILD
    output_console_output_string(L"\n\nkernel build\n");
#endif

    // after this point we can no longer use boot services (only runtime)
    
    exit_boot_services(h);

    size_t dim;
    const uint8_t* memory_bitmap = memory_get_memory_bitmap(&dim);

    const size_t new_w = dim * 8;
    const size_t new_h = 64;
    const size_t channels = 4;
    uint8_t* scaled_bitmap = (uint8_t*)malloc(new_w*new_h*channels);
    if ( scaled_bitmap )
    {
        stbir_resize_uint8(memory_bitmap, dim,1,dim, scaled_bitmap, new_w,new_h, new_w, channels);

        uint32_t palette[_C_EFI_MEMORY_TYPE_N];
        uint8_t dc = 0xff/_C_EFI_MEMORY_TYPE_N;
        for(size_t i = 0; i < _C_EFI_MEMORY_TYPE_N; ++i) {
            uint8_t c = dc*i;
            palette[i] = video_make_color(c,c,c);
        }
        video_scale_draw_indexed_bitmap( scaled_bitmap, palette, _C_EFI_MEMORY_TYPE_N, new_w,new_h, 10,500, new_w,new_h );
    }   
    output_console_set_colour(video_make_color(0xff,0,0));
    output_console_output_string(L"\nThe kernel has exited!");
    __builtin_unreachable(); 
}
