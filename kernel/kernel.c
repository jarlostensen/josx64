
#include <jos.h>
#include <kernel.h>
#include <memory.h>
#include <video.h>
#include <serial.h>
#include <module.h>
#include <linear_allocator.h>
#include <x86_64.h>
#include <interrupts.h>
#include <debugger.h>
#include <clock.h>
#include <keyboard.h>
#include <tasks.h>

static const char* kKernelChannel = "kernel";

_JOS_NORETURN void halt_cpu() {
    serial_write_str(kCom1, "\n\nkernel halting\n");
    while(1)
    {
        __asm__ volatile ("pause");
    } 
   _JOS_UNREACHABLE();
}

jo_status_t kernel_uefi_init(void) {

    _JOS_KTRACE_CHANNEL(kKernelChannel, "uefi init");

    //STRICTLY assumes this doesn't need any memory, video, or SMP functionality
    serial_initialise();

    linear_allocator_t* main_allocator;
    jo_status_t status = memory_uefi_init(&main_allocator);
    if ( !_JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: memory initialise returned 0x%x", status);
        return status;
    }

    status = module_initialise(main_allocator);
    if ( !_JO_SUCCEEDED(status)) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: module initialise returned 0x%x", status);
        return status;
    }

    status = smp_initialise();
    if ( !_JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: SMP initialise returned 0x%x", status);
        return status;
    }

    //TODO: video needs an allocator for the backbuffer (at least)
    // port it to use module register
    status = video_initialise();
    if ( _JO_FAILED(status)  ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel,"***FATAL ERROR: video initialise returned 0x%x", status);
        return status;
    }

    // =====================================================================

    _JOS_KTRACE_CHANNEL(kKernelChannel, "uefi init ok");
    return _JO_STATUS_SUCCESS;
}

jo_status_t kernel_runtime_init(void) {
    interrupts_initialise_early();
    debugger_initialise();
    clock_initialise();
    keyboard_initialise();    
    tasks_initialise();
}

_JOS_NORETURN void  kernel_runtime_start(void) {
    tasks_start_idle();
    _JOS_UNREACHABLE();
}