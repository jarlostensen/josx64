
#include "kernel_detail.h"
#include <kernel/cpu.h>
#include <stdio.h>
#include <string.h>

#include <cpuid.h>

//NOTE: if these are clear in REAL mode it's a 286 but...that's not something we check for yet
#define CPU_FLAGS_8086      (0x7<<12)

// Intel IA dev guide, 10.4
#define IA32_APIC_BASE_MSR      0x1b


// max supported basic and extended
static unsigned int _max_basic_cpuid = 0;
static unsigned int _max_extended_cpuid = 0;
static char _vendor_string[13];
static bool _hypervisor_checked = false;
static bool _hypervisor = false;
static char _hypervisor_id[13];
static const char* kCpuChannel = "cpu";

unsigned int k_cpuid_max_extended(void)
{
    if(!_max_extended_cpuid)
    {
        _max_extended_cpuid = __get_cpuid_max(0x80000000, NULL);
    }
    return _max_extended_cpuid;
}

unsigned int k_cpuid_max_basic(void)
{
    if(!_max_basic_cpuid)
    {
        _max_basic_cpuid =  __get_cpuid_max(0, NULL);
    }
    return _max_basic_cpuid;
}

bool k_cpuid_hypervisor(void)
{
    if(!_hypervisor_checked)
    {
        uint32_t a,b,c,d;
        __cpuid(0x1, a,b,c,d);
        _hypervisor = (c & (1<<31))!=0;
        if(_hypervisor)
        {
             unsigned int regs[4];
            __cpuid(0x40000000, regs[0], regs[1], regs[2], regs[3]);
            _hypervisor_id[13] = 0;
            memcpy(_hypervisor_id, regs + 1, 3 * sizeof(regs[0]));
        }
        _hypervisor_checked = true;
    }
    return _hypervisor;
}

bool k_cpu_check(void)
{    
    _JOS_KTRACE_CHANNEL(kCpuChannel,"k_cpu_check\n");
    unsigned int flags = k_eflags();
    if((flags & CPU_FLAGS_8086) == CPU_FLAGS_8086)
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"error: unsupported CPU (8086)\n");
        return false;
    }

    _max_basic_cpuid = __get_cpuid_max(0, NULL);
    if(!_max_basic_cpuid)
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"error: CPUID not supported\n");
        return false;
    }

    unsigned int _eax = 0, _ebx, _ecx, _edx;
    int cpu_ok = 0;
    _max_extended_cpuid = __get_cpuid_max(0x80000000, NULL);
    if( _max_extended_cpuid>=0x80000001)
    {
        __get_cpuid(0x80000001,&_eax, &_ebx, &_ecx, &_edx);
        // supports 64 bit long mode?
        cpu_ok = (_edx & (1<<29)) == (1<<29);
    }
    
    if(!cpu_ok)
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"this CPU does not support 64 bit mode\n");        
        //TODO: we might make this a hard req at some point, but for now it's not critical
    }
    
    __get_cpuid(0, &_eax, &_ebx, &_ecx, &_edx);
    memcpy(_vendor_string + 0, &_ebx, sizeof(_ebx));
    memcpy(_vendor_string + 4, &_edx, sizeof(_edx));
    memcpy(_vendor_string + 8, &_ecx, sizeof(_ecx));
    _vendor_string[12] = 0;
    _JOS_KTRACE_CHANNEL(kCpuChannel,"vendor string \"%s\"\n", _vendor_string);

    if ( k_cpu_feature_present(kCpuFeature_TSC | kCpuFeature_MSR) )
        _JOS_KTRACE_CHANNEL(kCpuChannel,"TSC & MSR supported\n");
    if( k_cpu_feature_present(kCpuFeature_APIC))
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"APIC present\n");
    }

    int pae = 0;
    if(k_cpu_feature_present(kCpuFeature_LocalAPIC))
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"Local APIC present\n");

        uint32_t apic_lo, apic_hi;
        rdmsr(IA32_APIC_BASE_MSR, &apic_lo, &apic_hi);
        _JOS_KTRACE_CHANNEL(kCpuChannel,"APIC base phys address is 0x%lx\n",  (uint64_t)apic_lo | ((uint64_t)apic_hi << 32));
    }
    if(k_cpu_feature_present(kCpuFeature_PAE))
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"PAE supported\n");
        pae = 1;
    }
    if(k_cpu_feature_present(kCpuFeature_PSE_36))    
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"PSE-36 supported\n");
    }
    if(k_cpuid_hypervisor())
    {
        _JOS_KTRACE_CHANNEL(kCpuChannel,"running in hypervisor, vendor id is \"%s\"\n", _hypervisor_id);
    }

    if(k_cpuid_max_extended() >= 0x80000008)
    {
        __cpuid(0x80000008, _eax, _ebx, _ecx, _edx);
        _JOS_KTRACE_CHANNEL(kCpuChannel,"physical address size %d bits\n", _eax & 0xff);
        _JOS_KTRACE_CHANNEL(kCpuChannel,"linear address size %d bits\n", (_eax >> 8) & 0xff);
    }
    else
    {
        if(pae)
            _JOS_KTRACE_CHANNEL(kCpuChannel,"physical address size 36 bits\n");
        else
            _JOS_KTRACE_CHANNEL(kCpuChannel,"physical address size 32 bits\n");
        _JOS_KTRACE_CHANNEL(kCpuChannel,"linear address size 32 bits\n");
    }    

    return true;
}

bool k_cpu_feature_present(cpu_feature_t feature)
{
    uint32_t a,b,c,d;
    __cpuid(0x1, a,b,c,d);
    return (d & feature)==feature;
}