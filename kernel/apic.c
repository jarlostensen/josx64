
#include <apic.h>
#include <x86_64.h>

static uint32_t read_local_apic_register(processor_information_t* info, local_apic_register_t reg) {
    uint64_t register_address = info->_local_apic_info._base_address | (uint64_t)reg;
    const uint32_t* reg_ptr = (const uint32_t*)register_address;
    return *reg_ptr;
}

static void write_local_apic_register(processor_information_t* info, local_apic_register_t reg, uint32_t val) {
    uint64_t register_address = info->_local_apic_info._base_address | (uint64_t)reg;
    uint32_t* reg_ptr = (uint32_t*)register_address;
    reg_ptr[0] = val;
}

// disable the local APIC and make sure it's in a known "0" state before we proceeed
static void reset_local_apic(processor_information_t* info) {

    uint32_t val;

// IA dev guide vol 3a, figure 10-8
#define _LAPIC_MASK_LVT (1<<16)

    // disable (mask) timer and external (lint) interrupt vectors.    
    //ZZZ: Linux kernel does not mask CMCI register in it's clear_apic function, why not?

    val = read_local_apic_register(info, kLApic_Reg_LvtTimer);
    write_local_apic_register(info, kLApic_Reg_LvtTimer, val | _LAPIC_MASK_LVT);

    val = read_local_apic_register(info, kLApic_Reg_LvtLint0);
    write_local_apic_register(info, kLApic_Reg_LvtLint0, val | _LAPIC_MASK_LVT);

    val = read_local_apic_register(info, kLApic_Reg_LvtLint1);
    write_local_apic_register(info, kLApic_Reg_LvtLint1, val | _LAPIC_MASK_LVT);

    int max_num_lvts = (info->_local_apic_info._version >> 15) & 0xff;
}

void apic_collect_this_cpu_information(processor_information_t* info) {

     // yes, but is it enabled?
    uint32_t spiv = read_local_apic_register(info, kLApic_Reg_Spiv);
    // IA dev guide Vol 3a, figure 10-24
    info->_local_apic_info._enabled = (spiv & (1<<8)) == (1<<8);
    uint32_t apic_lo, apic_hi;
    x86_64_rdmsr(_JOS_K_IA32_APIC_BASE_MSR, &apic_lo, &apic_hi);
    info->_local_apic_info._base_address = (((uint64_t)apic_hi << 32) | (uint64_t)apic_lo) & 0xfffff000;
    info->_local_apic_info._id = read_local_apic_register(info, kLApic_Reg_Id);
    //NOTE: contains max LVT as well as version
    info->_local_apic_info._version = read_local_apic_register(info, kLApic_Reg_Version);
}
