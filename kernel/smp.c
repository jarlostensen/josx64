
#include <jos.h>
#include <string.h>

#include <cpuid.h>
#include <x86_64.h>
#include <collections.h>
#include <smp.h>
#include <apic.h>
#include <arena_allocator.h>
#include <fixed_allocator.h>

// in efi_main.c
static CEfiMultiProcessorProtocol*  _mpp = 0;

static size_t   _bsp_id = 0;
//NOTE: we can't use any PER_CPU storage before smp_initialise setus up this 
static size_t   _num_processors = 0;
static size_t   _num_enabled_processors = 0;
static processor_information_t* _processors = 0;
static const char* kSmpChannel = "smp";
static arena_allocator_t* _smp_arena = NULL;

// ==================================================================================================

#define CPUID_FEATURE_FLAG_ENABLED(reg, index) (((reg) & (1<<(index))) == (1<<(index)))

static void collect_this_cpu_information(processor_information_t* info) {

    info->_max_basic_cpuid = __get_cpuid_max(0, NULL);
    info->_max_ext_cpuid = __get_cpuid_max(0x80000000, NULL);

    uint32_t eax = 0,ebx,ecx,edx;

    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    memcpy(info->_vendor_string + 0, &ebx, sizeof(ebx));
    memcpy(info->_vendor_string + 4, &edx, sizeof(edx));
    memcpy(info->_vendor_string + 8, &ecx, sizeof(ecx));
    info->_vendor_string[12] = 0;

    __get_cpuid_count(0x1, 0, &eax, &ebx, &ecx, &edx);
    info->_has_hypervisor = CPUID_FEATURE_FLAG_ENABLED(ecx, 31);
    if(info->_has_hypervisor) {
        unsigned int regs[4];
        __get_cpuid_count(0x40000000, 0, regs+0, regs+1, regs+2, regs+3);
        info->_hypervisor_id[12] = 0;
        memcpy(info->_hypervisor_id, regs + 1, 3 * sizeof(regs[0]));
    }
    
    info->_has_tsc = CPUID_FEATURE_FLAG_ENABLED(edx, 5);
    info->_has_msr = CPUID_FEATURE_FLAG_ENABLED(edx, 6);
    info->_xsave = CPUID_FEATURE_FLAG_ENABLED(ecx, 26);

    //NOTE: this should ALWAYS be true for x64
    info->_has_local_apic = CPUID_FEATURE_FLAG_ENABLED(edx, 9);
    if (info->_has_local_apic) {
        apic_collect_this_cpu_information(info);
        info->_local_apic_info._has_x2apic = CPUID_FEATURE_FLAG_ENABLED(ecx, 21);
    }
    
    if (info->_xsave) {
        __get_cpuid_count(0xd, 0, &eax, &ebx, &ecx, &edx);
        info->_xsave_info._xsave_bitmap = (uint64_t)eax | ((uint64_t)edx << 32);
        info->_xsave_info._xsave_area_size = ebx;

        // enable OSXSAVE in cr4 so that we can XSAVE/XRESTORE and FXSAVE/FXRESTORE
        x86_64_write_cr4(x86_64_read_cr4() | ((1 << 18) | (1<<9)));
        
    } else {
        info->_xsave_info._xsave_area_size = 0;
    }
    
    __get_cpuid_count(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    info->_intel_64_arch = CPUID_FEATURE_FLAG_ENABLED(edx, 29);
    info->_has_1GB_pages = CPUID_FEATURE_FLAG_ENABLED(edx, 26);
    
    //NOTE: if x2APIC is supported we can use 1b or b CPUID functions for topology information as well
    
    info->_is_good = true;
}

static void initialise_this_ap(void* arg) {

    processor_information_t* proc_info = (processor_information_t*)arg;
    collect_this_cpu_information(proc_info);

    // store the address of the ID field in the processor info block directory in gs:0 on this CPU
    // i.e. 
    //
    //  | cpu0 | cpu1 | ... | cpuN |
    //    gs:0   gs:0   ...   gs:0
    //    info0  info1  ...   infoN
    //    
    uint64_t proc_info_ptr = (uint64_t)&proc_info->_id;
    x86_64_wrmsr(_JOS_K_IA32_GS_BASE, (uint32_t)(proc_info_ptr) & 0xffffffff, (uint32_t)(proc_info_ptr >> 32));

    _JOS_KTRACE_CHANNEL(kSmpChannel, "initialised ap %d, gs @ 0x%llx -> %d", proc_info->_id, proc_info_ptr, per_cpu_this_cpu_id());
}

jo_status_t    smp_initialise(jos_allocator_t* allocator, CEfiBootServices *boot_services) {

    const size_t kSMP_PER_CPU_MEMORY_ARENA_SIZE = 1024*1024;

    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer,0,sizeof(handle_buffer));

    CEfiStatus efi_status = boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if ( efi_status==C_EFI_SUCCESS ) {
        //TODO: this works but it's not science; what makes one handle a better choice than another? 
        //      
        size_t num_handles = handle_buffer_size/sizeof(CEfiHandle);
        for(size_t n = 0; n < num_handles; ++n)
        {
            efi_status = boot_services->handle_protocol(handle_buffer[n], &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, (void**)&_mpp);
            if ( efi_status == C_EFI_SUCCESS )
            {
                break;
            }            
        }
        
        if ( efi_status == C_EFI_SUCCESS ) {                

            _JOS_KTRACE_CHANNEL(kSmpChannel, "using UEFI MP protocol");

            efi_status = _mpp->who_am_i(_mpp, &_bsp_id);
            if ( efi_status != C_EFI_SUCCESS ) {
                return _JO_STATUS_INTERNAL;
            }            

            efi_status = _mpp->get_number_of_processors(_mpp, &_num_processors, &_num_enabled_processors);
            if ( efi_status != C_EFI_SUCCESS ) {
                return _JO_STATUS_INTERNAL;
            }

            _JOS_KTRACE_CHANNEL(kSmpChannel, "BSP id is %d, %d processors present", _bsp_id, _num_processors);

            // now we have the information we need to register this module
            // we'll set aside one meg per core for misc
            _smp_arena = arena_allocator_create(allocator->alloc(allocator, kSMP_PER_CPU_MEMORY_ARENA_SIZE*_num_enabled_processors), 
                            kSMP_PER_CPU_MEMORY_ARENA_SIZE*_num_enabled_processors);

            _processors = (processor_information_t*)arena_allocator_alloc(_smp_arena, sizeof(processor_information_t) * _num_processors);
            memset(_processors, 0, sizeof(processor_information_t) * _num_processors);
            _processors[_bsp_id]._id = _bsp_id;
            initialise_this_ap((void*)&_processors[_bsp_id]);
            
            for(size_t p = 0; p < _num_processors; ++p) {

                // assigning the id this way ensures we don't depend on any hw particulars
                _processors[p]._id = p;
                if( p != _bsp_id ) {
                    efi_status = _mpp->get_processor_info(_mpp, p, &_processors[p]._uefi_info);
                    if ( efi_status == C_EFI_SUCCESS ) {
                        // execute the information collect function on this processor
                        //NOTE: infinite timeout here because the callback is quick, if that changes this has to be re-considered
                        efi_status = _mpp->startup_this_ap(_mpp, initialise_this_ap, p, NULL, 0, (void*)(_processors+p), NULL);
                        if ( efi_status != C_EFI_SUCCESS ) {
                            _processors[p]._is_good = false;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // uni processor
        _smp_arena = arena_allocator_create(allocator->alloc(allocator, kSMP_PER_CPU_MEMORY_ARENA_SIZE), kSMP_PER_CPU_MEMORY_ARENA_SIZE);
        _JOS_KTRACE_CHANNEL(kSmpChannel, "uni processor system, or no UEFI MP protocol handler available");
        _processors = (processor_information_t*)arena_allocator_alloc(_smp_arena, sizeof(processor_information_t));
        _processors->_id = 0;
        initialise_this_ap(_processors);        
        _processors->_is_good = true;
        _num_processors = 1;
        _num_enabled_processors = 1;
    }

    return _JO_STATUS_SUCCESS;        
}

size_t smp_get_processor_count() {
    _JOS_ASSERT(_num_processors);
    return _num_processors;
}

size_t smp_get_bsp_id() {
    return _bsp_id;
}

jo_status_t        smp_get_processor_information(processor_information_t* out_info, size_t processor_index) {
    _JOS_ASSERT(_num_processors);
    if ( processor_index >= _num_processors ) {
        return _JO_STATUS_OUT_OF_RANGE;
    }
    memcpy(out_info, _processors+processor_index, sizeof(processor_information_t));
    return _JO_STATUS_SUCCESS;
}

// ====================================================================================
// per CPU 

per_cpu_ptr_t       per_cpu_create_ptr(void) {
    _JOS_ASSERT(_num_processors);
    return (per_cpu_ptr_t)arena_allocator_alloc(_smp_arena, sizeof(uintptr_t)*_num_processors);
}

per_cpu_queue_t     per_cpu_create_queue(void) {
    _JOS_ASSERT(_num_processors);
    return (per_cpu_queue_t)arena_allocator_alloc(_smp_arena, sizeof(queue_t)*_num_processors);
}

per_cpu_qword_t     per_cpu_create_qword(void) {
    _JOS_ASSERT(_num_processors);
    return (per_cpu_qword_t)arena_allocator_alloc(_smp_arena, sizeof(uint64_t)*_num_processors);
}