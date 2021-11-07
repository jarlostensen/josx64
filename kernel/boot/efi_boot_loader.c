#include <c-efi.h>
#include <jos.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <stddef.h>

#include <kernel.h>
#include <clock.h>
#include <video.h>
#include <tasks.h>
#include <output_console.h>
#include <programs/scroller.h>
#include <font8x8/font8x8_basic.h>
#include <hex_dump.h>
#include <smp.h>
#include <debugger.h>
#include <keyboard.h>
#include <acpi.h>
#include <fixed_allocator.h>

static CEfiSystemTable*    _st = 0;
static CEfiBootServices * _boot_services = 0;
static CEfiLoadedImageProtocol * _lip = 0;
static peutil_pe_context_t _pe_ctx;

#define _EFI_PRINT(s)\
_st->con_out->output_string(_st->con_out, s)

// ==============================================================

static void uefi_panic(const wchar_t* panic) {
    _EFI_PRINT((CEfiChar16*)panic);
    halt_cpu();
}

static void exit_boot_services(CEfiHandle h) {

     // everything needed to run anything; interrupts, clocks, keyboard...etc...
    if (_JO_FAILED(kernel_runtime_init(h, _st))) {
        output_console_set_colour(video_make_color(0xff,0,0));        
        output_console_output_string(L"***FATAL ERROR: post memory exit failed. Halting.\n");
        uefi_panic(L"***FATAL ERROR: post memory exit failed. Halting.\n");
    }
}

void uefi_init(CEfiHandle h) {

    CEfiStatus efi_status = _boot_services->handle_protocol(h, &C_EFI_LOADED_IMAGE_PROTOCOL_GUID, (void**)&_lip);
    if ( C_EFI_ERROR(efi_status) ) {
        uefi_panic(L"unable to locate image protocol!");
    }
    // we use our PE image to look up code and provide helpful information to the debugger 
    peutil_bind(&_pe_ctx, (const void*)_lip->image_base, kPe_Relocated); 

    // initialise memory manager, serial comms, video, and any modules that require access 
    // to uefi boot services
    // here we also request a bit of memory for use by this efi application
    jo_status_t status = kernel_uefi_init(_st);
    if ( _JO_FAILED(status) ) {
        uefi_panic(L"failed to initialise kernel!");
    }
    
    video_clear_screen(0x6495ed);
    output_console_initialise();
    output_console_set_font((const uint8_t*)font8x8_basic, 8,8);
    output_console_set_colour(0xffffffff);
    output_console_set_bg_colour(0x6495ed);

    _JOS_KTRACE_CHANNEL("efi_main", "kernel starting");
}

static jo_status_t main_task(void* ptr);
static void start_debugger() {
    // start debugger
    uint32_t col = output_console_get_colour();
    output_console_set_colour(0xff2222);
    output_console_output_string(L"\n\nwaiting for debugger...");

    debugger_wait_for_connection(&_pe_ctx, (uint64_t)_lip->image_base);
    debugger_set_breakpoint((uintptr_t)main_task);

    output_console_output_string(L"connected\n\n");
    output_console_set_colour(col);
}

static bool _read_input(void) {
    wchar_t buf[128];
    if ( keyboard_has_key() ) {
        uint32_t key = keyboard_get_last_key();
        if ( KEYBOARD_VK_PRESSED(key) ) {
            short c = (short)KEYBOARD_VK_CHAR(key);
            
                switch(c) {
                case KEYBOARD_VK_ESC:
                    output_console_output_string(L"\ngot ESC\n");
                    return true;
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
                case KEYBOARD_VK_BACKSPACE:
                    output_console_output_string(L"bs");
                    break;
                case KEYBOARD_VK_CR:
                    output_console_line_break();
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
                default: {
                    swprintf(buf, 128, L"0x%x ", key);
                    output_console_output_string(buf);                
                }
                break;
            }                
        }
    }
    return false;
}

static jo_status_t scroller_task(void* ptr) {
    (void)ptr;
    _JOS_KTRACE_CHANNEL("scroller_task", "starting");

    scroller_initialise(&(rect_t){
        .top = 250,
        .left = 32,
        .bottom = 400,
        .right = 632
    });

    uint64_t t0 = clock_ms_since_boot();     
    while (true) {
        uint64_t t1 = clock_ms_since_boot();
        if (t1 - t0 >= 33) {
            t0 = t1;
            scroller_render_field();            
        }
        tasks_yield();
    }
}

static jo_status_t main_task(void* ptr) {
    
    _JOS_KTRACE_CHANNEL("main_task", "starting");
    output_console_output_string(L"any key or ESC...\n");
    
    tasks_create(&(task_create_args_t) {
        .func = scroller_task,
        .pri = kTaskPri_Normal,
        .name = "scroller_task"
    });
    
    do {
        
        tasks_yield();

    } while(!_read_input());

    output_console_line_break();
    output_console_set_colour(video_make_color(0xff,0,0));
    wchar_t buf[128];
    swprintf(buf,128,L"\nmain task is done @ %dms\n", clock_ms_since_boot());
    output_console_output_string(buf);
    
    _JOS_KTRACE_CHANNEL("main_task", "terminating %llu ms after boot", clock_ms_since_boot());    
    
    return _JO_STATUS_SUCCESS;
}

CEfiStatus efi_main(CEfiHandle h, CEfiSystemTable *st)
{    
    _st = st;
    _boot_services = st->boot_services;
    _EFI_PRINT(L"jox64\n");
    
    uefi_init(h);
    if ( _JO_FAILED(hive_get(kernel_hive(), "kernel:booted", 0)) ) {
        uefi_panic(L"KERNEL IS NOT PROPERLY BOOTED");
    }

    wchar_t buf[256];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);        
    swprintf(buf, bufcount, L"\nimage is %llu bytes, loaded at 0x%llx, efi_main @ 0x%llx, PE entry point @ 0x%llx\n", 
        _lip->image_size, _lip->image_base, efi_main, peutil_entry_point(&_pe_ctx));
    output_console_output_string(buf);
    _JOS_KTRACE_CHANNEL("image_protocol", "image is %llu bytes, loaded at 0x%llx, efi_main @ 0x%llx, PE entry point @ 0x%llx", 
        _lip->image_size, _lip->image_base, efi_main, peutil_entry_point(&_pe_ctx));
    hex_dump_mem((void*)_lip->image_base, 64, k8bitInt);
    output_console_line_break();

    size_t bsp_id = smp_get_bsp_id();
    swprintf(buf, 256, L"%d processors detected, bsp is processor %d\n", smp_get_processor_count(), bsp_id);    
    output_console_output_string(buf);

    char info_buffer[sizeof(fixed_allocator_t) + 8*sizeof(uintptr_t)];
    fixed_allocator_t* info_allocator = fixed_allocator_create(info_buffer, sizeof(info_buffer), 3);
    vector_t info;
    vector_create(&info, 16, sizeof(hive_value_t), (heap_allocator_t*)info_allocator);

    if ( _JO_SUCCEEDED(hive_get(kernel_hive(), "acpi:config_table_entries", &info)) ) {
        swprintf(buf, bufcount, L"system configuration tables contain %d entries\n", (int)vector_at(&info, 0));
        output_console_output_string(buf);
    }
    vector_reset(&info);

    if ( _JO_SUCCEEDED(hive_get(kernel_hive(), "acpi:2.0", &info) ) ){
        output_console_output_string(L"ACPI 2.0 configuration found\n");
    }
    if ( _JO_SUCCEEDED(hive_get(kernel_hive(), "acpi:1.0", &info) ) ){
        output_console_output_string(L"ACPI 1.0 configuration found\n");
    }
    
    if ( smp_get_processor_count()==1 ) {
        processor_information_t info;    
        if ( _JO_SUCCEEDED(smp_get_this_processor_info(&info) ) ) {
            swprintf(buf, 256, L"BSP id %d, TSC is %S, Intel 64 arch %S\n", 
                        info._id,
                        info._has_tsc ? "enabled":"disabled",
                        info._intel_64_arch ? "supported" : "not supported"
                        );
                output_console_output_string(buf);
        }
    }
    else {
        for ( size_t p = smp_get_processor_count(); p>0; --p ) {
            processor_information_t info;    
            jo_status_t status = smp_get_processor_information(&info, p-1);
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
                        info._local_apic_info._id >> 24, info._local_apic_info._id, 
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
                swprintf(buf, 256, L"smp_get_processor_information returned %x\n", status);
                output_console_output_string(buf);
            }
        }
    }
    
#ifdef _JOS_KERNEL_BUILD
    output_console_output_string(L"\n\nkernel build\n");
#endif
            
    exit_boot_services(h);
    output_console_output_string(L"\n\nkernel started\n");
    start_debugger(); 
    
    tasks_create(&(task_create_args_t){
         .func = main_task,
            .pri = kTaskPri_Normal,
            .name = "main_task"
    });

    kernel_runtime_start();

    return C_EFI_SUCCESS;
}
 