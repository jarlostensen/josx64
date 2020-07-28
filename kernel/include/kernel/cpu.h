#ifndef _JOS_CPU_H
#define _JOS_CPU_H

#include <stdbool.h>

typedef enum cpu_feature_enum
{
    kCpuFeature_x87 = 0x1,
    kCpuFeature_Vme = 0x2,
    kCpuFeature_DebuggingExtensions = 0x4,
    kCpuFeature_PageSizeExtension = 0x8,
    kCpuFeature_TSC = 0x10,
    kCpuFeature_MSR = 0x20,
    kCpuFeature_PAE = 0x40,
    kCpuFeature_APIC = 0x100,
    kCpuFeature_SEP = 0x400,
    kCpuFeature_ACPI = 0x200000,    
    kCpuFeature_LocalAPIC = 1<<9,
    kCpuFeature_PSE_36 = 1<<17,

} cpu_feature_t;

bool k_cpu_check(void);

bool k_cpu_feature_present(cpu_feature_t feature);

// returns the highest extended function number supported by this CPU
unsigned int k_cpuid_max_extended(void);
// returns the highest basic function number supported by this CPU
unsigned int k_cpuid_max_basic(void);
// true if running in a hypervisor
bool k_cpuid_hypervisor(void);

// read MSR
inline void rdmsr(uint32_t msr, uint32_t* lo, uint32_t* hi)
{
    asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}
// write MSR
inline void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
   asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

#endif // _JOS_CPU_H