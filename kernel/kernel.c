
#include <jos.h>
#include <kernel.h>
#include <memory.h>
#include <video.h>
#include <serial.h>
#include <linear_allocator.h>
#include <x86_64.h>
#include <interrupts.h>
#include <debugger.h>
#include <clock.h>
#include <keyboard.h>
#include <tasks.h>
#include <smp.h>

static const char* kKernelChannel = "kernel";

_JOS_NORETURN void halt_cpu() {
    serial_write_str(kCom1, "\n\nkernel halting\n");
    while(1)
    {
        __asm__ volatile ("pause");
    } 
   _JOS_UNREACHABLE();
}

// the main allocator used by the kernel to initialise modules and provide memory for memory pools
// NOTE: this memory is never freed, each module is expected to deal with the details of memory resource 
// management themselves
static linear_allocator_t*  _kernel_allocator = 0;
static void* _kernel_alloc(size_t size) {
    void* ptr = linear_allocator_alloc(_kernel_allocator, size);
    _JOS_ASSERT(ptr);
    return ptr;
}
static void _kernel_free(void* ptr) {
    _JOS_KTRACE_CHANNEL(kKernelChannel, "WARNING: kernel free called on 0x%x", ptr);
}

static jos_allocator_t  _kernel_allocator_interface = { _kernel_alloc, _kernel_free };

jo_status_t kernel_uefi_init(kernel_uefi_init_args_t* args) {

    _JOS_KTRACE_CHANNEL(kKernelChannel, "uefi init");

    //STRICTLY assumes this doesn't need any memory, video, or SMP functionality
    serial_initialise();

    linear_allocator_t* main_allocator;
    jo_status_t status = memory_uefi_init(&main_allocator);
    if ( !_JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: memory initialise returned 0x%x", status);
        return status;
    }

    status = smp_initialise(&_kernel_allocator_interface);
    if ( !_JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: SMP initialise returned 0x%x", status);
        return status;
    }

    //TODO: video needs an allocator for the backbuffer (at least)
    // port it to use module register
    status = video_initialise(&_kernel_allocator_interface);
    if ( _JO_FAILED(status)  ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel,"***FATAL ERROR: video initialise returned 0x%x", status);
        return status;
    }

    // provide memory to the caller, i.e. the outer UEFI application, if it requires it
    if (args->application_memory_size_required) {
        *args->application_allocated_memory = _kernel_alloc(args->application_memory_size_required);
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
    tasks_initialise(&(jos_allocator_t){
        .alloc = _kernel_alloc,
        .free = _kernel_free
    });
    return _JO_STATUS_SUCCESS;
}

_JOS_NORETURN void  kernel_runtime_start(void) {
    tasks_start_idle();
    _JOS_UNREACHABLE();
}
