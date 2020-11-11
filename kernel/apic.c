
#include <apic.h>
#include <x86_64.h>

static uint32_t read_local_apic_register(processor_information_t* info, uint16_t reg) {
    uint64_t register_address = info->_local_apic_info._base_address | (uint64_t)reg;
    const uint32_t* reg_ptr = (const uint32_t*)register_address;
    return *reg_ptr;
}

void apic_collect_this_cpu_information(processor_information_t* info) {

     // yes, but is it enabled?
    uint32_t spiv = read_local_apic_register(info, _JOS_K_LOCAL_APIC_REGISTER_SPIV);
    // IA dev guide Vol 3a, figure 10-24
    info->_local_apic_info._enabled = (spiv & (1<<8)) == (1<<8);
    uint32_t apic_lo, apic_hi;
    x86_64_rdmsr(_JOS_K_IA32_APIC_BASE_MSR, &apic_lo, &apic_hi);
    info->_local_apic_info._base_address = (((uint64_t)apic_hi << 32) | (uint64_t)apic_lo) & 0xfffff000;
    info->_local_apic_info._id = read_local_apic_register(info, _JOS_K_LOCAL_APIC_REGISTER_ID);
    info->_local_apic_info._version = read_local_apic_register(info, _JOS_K_LOCAL_APIC_REGISTER_VERSION);    

}