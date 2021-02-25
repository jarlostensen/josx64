#include <c-efi.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <jos.h>
#include <kernel.h>
#include <video.h>
#include <output_console.h>
#include <memory.h>
#include <serial.h>
#include <trace.h>
#include <processors.h>
#include <hex_dump.h>
#include <interrupts.h>
#include <clock.h>
#include <debugger.h>
#include <keyboard.h>
#include <pe.h>
#include <x86_64.h>

#include <programs/scroller.h>
#include <font8x8/font8x8_basic.h>

//https://github.com/rust-lang/rust/issues/62785/
// TL;DR linker error we get when building with Clang on Windows 
int _fltused = 0;

CEfiSystemTable*    g_st = 0;
// used from various startup modules (pre-ExitBootServices, after it will be set to 0 again)
CEfiBootServices * g_boot_services = 0;

static CEfiChar16*   kLoaderHeading = L"| jOSx64 ----------------------------\n\r";

#define _EFI_PRINT(s)\
g_st->con_out->output_string(g_st->con_out, s)

// ==============================================================
void exit_boot_services(CEfiHandle h) {

    memory_refresh_boot_service_memory_map();
    CEfiStatus status = g_boot_services->exit_boot_services(h, memory_boot_service_get_mapkey());    
    if ( C_EFI_ERROR(status )) {
        output_console_set_colour(video_make_color(0xff,0,0));        
        output_console_output_string(L"***FATAL ERROR: Unable to exit boot services. Halting.\n");
        halt_cpu();
    }    
    g_boot_services = 0;    

    jo_status_t k_stat = memory_post_exit_bootservices_initialise();
    if ( _JO_FAILED(k_stat) ) {
        output_console_set_colour(video_make_color(0xff,0,0));        
        output_console_output_string(L"***FATAL ERROR: post memory exit failed. Halting.\n");
        halt_cpu();
    }
}

//TODO: kernel variables that could be dynamic like this should live somewhere else, a basic key-value store somewhere perhaps?
uint16_t kJosKernelCS;

CEfiLoadedImageProtocol * _lip = 0;
void image_protocol_info(CEfiHandle h, CEfiStatus (*_efi_main)(CEfiHandle, CEfiSystemTable *)) {

    _JOS_KTRACE_CHANNEL("image_protocol","opening image protocol...");
    CEfiStatus efi_status = g_boot_services->handle_protocol(h, &C_EFI_LOADED_IMAGE_PROTOCOL_GUID, (void**)&_lip);
    if ( efi_status==C_EFI_SUCCESS ) {

        peutil_pe_context_t pe_ctx;
        peutil_bind(&pe_ctx, (const void*)_lip->image_base, kPe_Relocated);
        
        wchar_t buf[256];
        const size_t bufcount = sizeof(buf)/sizeof(wchar_t);        
        swprintf(buf, bufcount, L"\nimage is %llu bytes, loaded at 0x%llx, efi_main @ 0x%llx, PE entry point @ 0x%llx\n", 
            _lip->image_size, _lip->image_base, _efi_main, peutil_entry_point(&pe_ctx));
        output_console_output_string(buf);
        hex_dump_mem((void*)_lip->image_base, 64, k8bitInt);
        output_console_line_break();
    }    

    if (!_lip) {
        _JOS_KTRACE_CHANNEL("image protocol", "not found");
    }    
}



void pre_exit_boot_services() {
    wchar_t buf[256];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

    uint64_t rflags = x86_64_get_rflags();
    kJosKernelCS = x86_64_get_cs();

    serial_initialise();
    _JOS_KTRACE_CHANNEL("efi_main","pre exit boot services");

    jo_status_t status = memory_pre_exit_bootservices_initialise();
    if ( _JO_FAILED(status) ) {
        swprintf(buf, bufcount, L"***FATAL ERROR: memory initialise returned 0x%x\n\r", status);
        _EFI_PRINT(buf);
        halt_cpu();
    }

    status = processors_initialise();
    if ( !_JO_SUCCEEDED(status) ) {
        swprintf(buf, bufcount, L"***FATAL ERROR: MP initialise returned 0x%x\n\r", status);
        _EFI_PRINT(buf);
        halt_cpu();
    }

    status = video_initialise(&(jos_allocator_t){
        ._alloc = malloc,
        ._free = free,
    });
    if ( _JO_FAILED(status)  ) {

        swprintf(buf, bufcount, L"***FATAL ERROR: video initialise returned 0x%x\n\r", status);
        _EFI_PRINT(buf);
        halt_cpu();
    }

    video_clear_screen(0x6495ed);
    output_console_initialise();
    output_console_set_font((const uint8_t*)font8x8_basic, 8,8);
    output_console_set_colour(0xffffffff);
    output_console_set_bg_colour(0x6495ed);

    _JOS_KTRACE_CHANNEL("efi_main","CS = 0x%x, RFLAGS = 0x%x\n", kJosKernelCS, rflags);
    _JOS_KTRACE_CHANNEL("efi_main","kernel starting");

}

CEfiStatus efi_main(CEfiHandle h, CEfiSystemTable *st)
{    
    CEfiStatus efi_status;
    g_st = st;
    g_boot_services = st->boot_services;

    pre_exit_boot_services();
    
    image_protocol_info(h, efi_main);

    wchar_t buf[256];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);
    size_t bsp_id = processors_get_bsp_id();
    swprintf(buf, 256, L"%d processors detected, bsp is processor %d\n", processors_get_processor_count(), bsp_id);    
    output_console_output_string(buf);    
    
    if ( processors_has_acpi_20() ) {
        output_console_output_string(L"ACPI 2.0 configuration enabled\n");
    }

    const processor_information_t* proc_info = (const processor_information_t*)processors_get_per_cpu_ptr(_JOS_K_PER_CPU_IDX_PROCESSOR_INFO);
    if(proc_info) {        
        swprintf(buf, 256, L"\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S, Intel 64 arch %S\n", 
                    proc_info->_uefi_info.processor_id,
                    proc_info->_uefi_info.status_flag,
                    proc_info->_uefi_info.extended_information.location.package,
                    proc_info->_uefi_info.extended_information.location.core,
                    proc_info->_uefi_info.extended_information.location.thread,
                    proc_info->_has_tsc ? "enabled":"disabled",
                    proc_info->_intel_64_arch ? "supported" : "not supported"
                    );
            output_console_output_string(buf);
    }

    #if 0
    for ( size_t p = processors_get_processor_count(); p>0; --p ) {
        processor_information_t info;
        jo_status_t status = processors_get_processor_information(&info, p-1);
        if ( _JO_SUCCEEDED(status) ) {

            swprintf(buf, 256, L"\tid %d, status 0x%x, package %d, core %d, thread %d, TSC is %S, Intel 64 arch %S\n", 
                    info._uefi_info.processor_id,
                    info._uefi_info.status_flag,
                    info._uefi_info.extended_information.location.package,
                    info._uefi_info.extended_information.location.core,
                    info._uefi_info.extended_information.location.thread,
                    info._has_tsc ? "enabled":"disabled",
                    info._intel_64_arch ? "supported" : "not supported"
                    );
            output_console_output_string(buf);

            if ( (p-1) == bsp_id ) {
                swprintf(buf, 256, L"\tBSP vendor is \"%S\",%S hypervisor detected ", 
                    info._vendor_string,
                    info._has_hypervisor?L" ":L" no");
                output_console_output_string(buf);
                if ( info._has_hypervisor ) {
                    swprintf(buf, 256, L"\"%s\"\n", info._hypervisor_id);
                    output_console_output_string(buf);
                }
                else {
                    output_console_output_string(L"\n");
                }
            }
            
            if ( info._has_local_apic ) {
                swprintf(buf, 256, L"\t\tlocal APIC id %d (0x%x) is %S, %S, x2APIC %S supported\n", 
                    info._local_apic_info._id, info._local_apic_info._id >> 24, 
                    info._local_apic_info._enabled ? "enabled":"disabled",
                    info._local_apic_info._version? "integrated":"discrete 8248DX",
                    info._local_apic_info._has_x2apic ? "is":"not"
                    );
                output_console_output_string(buf);
            }
            output_console_output_string(L"\n");
        }
        else
        {
            swprintf(buf, 256, L"processors_get_processor_information returned %x\n", status);
            output_console_output_string(buf);
        }
    }
    #endif
    

#ifdef _JOS_KERNEL_BUILD
    output_console_output_string(L"\n\nkernel build\n");
#endif

    if(processors_get_processor_count()>1)
    {
        size_t* ids = (size_t*)malloc(sizeof(size_t)*processors_get_processor_count());
        for(size_t n = 0; n < processors_get_processor_count(); ++n )
        {
            ids[n] = n;
        }
        
        //ZZZ:
        // jo_status_t status = processors_startup_aps(ap_idle_func, (void*)ids, sizeof(size_t));
        // if ( _JO_FAILED(status )) {
        //     output_console_set_colour(video_make_color(0xff,0,0));
        //     swprintf(buf, bufcount, L"\tstartup aps failed with 0x%x\n", status);
        //     output_console_output_string(buf);
        // }
    }

    // after this point we can no longer use boot services (only runtime)
    
    _JOS_KTRACE_CHANNEL("efi_main","exiting boot services...");
    exit_boot_services(h);
    _JOS_KTRACE_CHANNEL("efi_main","boot services exited");

    _JOS_KTRACE_CHANNEL("efi_main","kernel setup");
    //ZZZ:
    interrupts_initialise_early();
    debugger_initialise();
    clock_initialise();
    keyboard_initialise();

    scroller_initialise(&(rect_t){
        .top = 250,
        .left = 8,
        .bottom = 400,
        .right = 600
    });

    uint64_t elapsed = __rdtsc();
    x86_64_io_wait();
    elapsed = __rdtsc() - elapsed;

    _JOS_KTRACE_CHANNEL("efi_main", "port 0x80 wait took ~ %d cycles\n", elapsed);
    
    keyboard_state_t kbd_state;
    keyboard_get_state(&kbd_state);
    _JOS_KTRACE_CHANNEL("efi_main", "keyboard controller id is 0x%x, scan code set 0x%x\n", keyboard_get_id(), kbd_state.set);
    
    output_console_output_string(L"any key or ESC...\n");

    video_present();

    uint64_t t0 = clock_ms_since_boot();
    bool done = false;
    while(!done) {
        if ( keyboard_has_key() ) {
            uint32_t key = keyboard_get_last_key();
            if ( KEYBOARD_VK_PRESSED(key) ) {
                short c = (short)KEYBOARD_VK_CHAR(key);
                swprintf(buf, bufcount, L"0x%x ", c);
                output_console_output_string(buf);

                video_present();

                switch(c) {
                    case KEYBOARD_VK_ESC:
                        output_console_output_string(L"\ngot ESC\n");
                        done = true;
                        break;
                    case KEYBOARD_VK_RIGHT:
                        output_console_output_string(L" -> ");
                        break;
                    case KEYBOARD_VK_LEFT:
                        output_console_output_string(L" <- ");
                        break;
                    case KEYBOARD_VK_UP:
                        output_console_output_string(L" ^ ");
                        break;
                    case KEYBOARD_VK_DOWN:
                        output_console_output_string(L" v ");
                        break;
                    case KEYBOARD_VK_F1:
                        output_console_output_string(L" F1 ");
                        break;
                    case KEYBOARD_VK_F2:
                        output_console_output_string(L" F2 ");
                        break;
                    case KEYBOARD_VK_F3:
                        output_console_output_string(L" F3 ");
                        break;
                    case KEYBOARD_VK_F4:
                        output_console_output_string(L" F4 ");
                        break;
                    case KEYBOARD_VK_F5:
                        output_console_output_string(L" F5 ");
                        break;
                    case KEYBOARD_VK_F6:
                        output_console_output_string(L" F6 ");
                        break;
                    case KEYBOARD_VK_F7:
                        output_console_output_string(L" F7 ");
                        break;
                    case KEYBOARD_VK_F8:
                        output_console_output_string(L" F8 ");
                        break;
                    case KEYBOARD_VK_F9:
                        output_console_output_string(L" F9 ");
                        break;
                    case KEYBOARD_VK_F10:
                        output_console_output_string(L" F10 ");
                        break;
                    case KEYBOARD_VK_F11:
                        output_console_output_string(L" F11 ");
                        break;
                    case KEYBOARD_VK_F12:
                        output_console_output_string(L" F12 ");
                        break;
                    default:
                    break;
                }                
            }
        }

        uint64_t t1 = clock_ms_since_boot();
        if( t1 - t0 > 33 ) {
            t0 = t1;
            scroller_render_field();
            video_present();
        }
    }
    output_console_line_break();


    // size_t dim;
    // const uint8_t* memory_bitmap = memory_get_memory_bitmap(&dim);
    // const size_t new_w = dim * 8;
    // const size_t new_h = 64;
    // const size_t channels = 4;
    // uint8_t* scaled_bitmap = (uint8_t*)malloc(new_w*new_h*channels);
    // if ( scaled_bitmap )
    // {
    //     stbir_resize_uint8(memory_bitmap, dim,1,dim, scaled_bitmap, new_w,new_h, new_w, channels);

    //     uint32_t palette[_C_EFI_MEMORY_TYPE_N];
    //     uint8_t dc = 0xff/_C_EFI_MEMORY_TYPE_N;
    //     for(size_t i = 0; i < _C_EFI_MEMORY_TYPE_N; ++i) {
    //         uint8_t c = dc*i;
    //         palette[i] = video_make_color(c,c,c);
    //     }
    //     video_scale_draw_indexed_bitmap( scaled_bitmap, palette, _C_EFI_MEMORY_TYPE_N, new_w,new_h, 200,500, new_w,new_h );
    // } 

    //TEST:
    asm volatile (
        "nop\r\n"
        "movq $0x1234567812345678, %r11\r\n"
        "int $0x3\r\n"
        "nop\r\n"   
        "movq $0x1234567812345678, %r12\r\n"
        );

    output_console_set_colour(video_make_color(0xff,0,0));

    swprintf(buf,128,L"\nThe kernel has exited @ %dms\n", clock_ms_since_boot());
    output_console_output_string(buf);
    _JOS_KTRACE_CHANNEL("efi_main", "exiting %llu ms after boot", clock_ms_since_boot());

    video_present();
    
    halt_cpu();
}
 