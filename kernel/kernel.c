
#include <jos.h>

#include <c-efi.h>

#include <kernel.h>
#include <memory.h>
#include <video.h>
#include <serial.h>
#include <linear_allocator.h>
#include <pagetables.h>
#include <x86_64.h>
#include <interrupts.h>
#include <debugger.h>
#include <clock.h>
#include <keyboard.h>
#include <tasks.h>
#include <smp.h>
#include <acpi.h>

#define _JOS_IMPLEMENT_HIVE
#include <hive.h>

//https://github.com/rust-lang/rust/issues/62785/
// TL;DR linker error we get when building with Clang on Windows 
//       so we just need to define it somewhere
int _fltused = 0;

// make sure this has an implementation somewhere
#define _JOS_IMPLEMENT_JSON
#include <extensions/json.h>

//ZZZ: this is a bit lame...
uint16_t kJosKernelCS;

static const char* kKernelChannel = "kernel";
static hive_t _hive;

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
static jos_allocator_t*  _kernel_allocator = 0;
static size_t _initial_memory = 0;

_JOS_API_FUNC void kernel_memory_available(size_t* on_boot, size_t* now) {
    *on_boot = _initial_memory;
    *now = _kernel_allocator->available(_kernel_allocator);
}

_JOS_API_FUNC jo_status_t kernel_uefi_init(CEfiSystemTable* system_services) {

    _JOS_KTRACE_CHANNEL(kKernelChannel, "uefi init");

    kJosKernelCS = x86_64_get_cs();

    pagetables_initialise();    

    //STRICTLY assumes this doesn't need any memory, video, or SMP functionality
    serial_initialise();

    //NOTE: we can initialise the kernel allocator here because we never exit back to the EFI firmware, otherwise 
    //      this memory would have to be freed at that point
    jo_status_t status = memory_uefi_init(system_services->boot_services);
    if ( !_JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: memory initialise returned 0x%x", status);
        return status;
    }

    // allocate almost all available RAM for the kernel (for now) 
    _initial_memory = memory_get_available();
    _kernel_allocator = memory_allocate_pool(kMemoryPoolType_Static, memory_get_available());
    
    status = smp_initialise((jos_allocator_t*)_kernel_allocator, system_services->boot_services);
    if ( !_JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel, "***FATAL ERROR: SMP initialise returned 0x%x", status);
        return status;
    }

    status = acpi_intitialise(system_services);

    // port it to use module register
    status = video_initialise((jos_allocator_t*)_kernel_allocator, system_services->boot_services);
    if ( _JO_FAILED(status)  ) {
        _JOS_KTRACE_CHANNEL(kKernelChannel,"***FATAL ERROR: video initialise returned 0x%x", status);
        return status;
    }
    
    // create our hive storage
    hive_create(&_hive, (jos_allocator_t*)_kernel_allocator);

    // =====================================================================

    _JOS_KTRACE_CHANNEL(kKernelChannel, "uefi init ok");
    return _JO_STATUS_SUCCESS;
}

_JOS_API_FUNC jo_status_t kernel_runtime_init(CEfiHandle h, CEfiSystemTable* system_services) {
    
    jo_status_t k_stat = memory_runtime_init(h, system_services->boot_services);
    if ( _JO_FAILED(k_stat) ) {
        return k_stat;
    }

    interrupts_initialise_early();
	debugger_initialise((jos_allocator_t*)_kernel_allocator);
    clock_initialise();
    keyboard_initialise();    
    tasks_initialise((jos_allocator_t*)_kernel_allocator);
    return _JO_STATUS_SUCCESS;
}

_JOS_NORETURN void  kernel_runtime_start(void) {
    tasks_start_idle();
    _JOS_UNREACHABLE();
}
