

#include "kernel_detail.h"
#include <collections.h>
#include <kernel/tasks.h>
#include "memory.h"
#include "cpu_core.h"
#include "hex_dump.h"

#include <stdio.h>
#ifdef _JOS_KERNEL_BUILD
#include <cpuid.h>
#include <kernel/cpu.h>
#endif

_JOS_MAYBE_UNUSED static const char* kCpuCoreChannel = "cpu_core";


// These things are from the ancient Intel MultiProcessor Specification
// multi processor floating pointer structure
// found in memory by scan_smpning (see _smp_init)
// NOTE: this structure is exactly 16 bytes in size
typedef struct smp_fp_struct
{
    uint32_t    _sig;
    uint32_t    _phys_address;
    uint8_t     _length;
    uint8_t     _specrev;
    uint8_t     _checksum;
    uint8_t     _feature_information;
    uint8_t     _feature_information_2:7;
    uint8_t     _imcr_present:1;
    uint8_t     _reserved[3];
} _JOS_PACKED smp_fp_t;

typedef struct smp_config_header_struct
{
    uint32_t    _sig;
    uint16_t    _bt_length;
    uint8_t     _specrev;
    uint8_t     _checksum;
    uint8_t     _oem_id[20];
    uint32_t    _oem_table_ptr;
    uint16_t    _oem_table_size;
    uint16_t    _oem_entry_count;
    uint32_t    _local_apic;
    uint16_t    _ext_length;
    uint8_t     _ext_checksum;
    uint8_t     _reserved;
} _JOS_PACKED smp_config_header_t;

typedef struct smp_processor_struct
{
    uint8_t     _type;
    uint8_t     _local_apic_id;
    uint8_t     _local_apic_version;
    uint8_t     _flags;
    uint32_t    _cpu_sig;
    uint32_t    _feature_flags;
    uint32_t    _reserved[2];
} _JOS_PACKED smp_processor_t;

typedef struct smp_bus_struct
{
    uint8_t     _type;
    uint8_t     _bus_id;
    uint8_t     _type_string[6];
} _JOS_PACKED smp_bus_t;

static const smp_fp_t *_smp_fp = 0;
static const smp_config_header_t * _smp_config = 0;

static unsigned char _calculate_st_checksum(void* table, size_t size)
{
    unsigned char chksum = 0;
    unsigned char* rp = (unsigned char*)table;
    for(unsigned n = 0; n < size; ++n)
    {
        chksum += *rp++;
    }
    return chksum;
}

// all system tables use the same checksum algorithm
static bool _validate_st_checksum(void* table, size_t size)
{
    return _calculate_st_checksum(table,size)==0 ? true : false;
}

// =============================== RSDP (Root System Description Pointer)
struct _rsdp_descriptor {
    // all versions
    // 20 bytes

    // "RSD PTR "
    char    _sig[8];
    uint8_t _checksum;
    char    _oem_id[6];
    uint8_t _revision;
    uint32_t _rsdt_address;

    // version 2+ only
    // 16 bytes
    uint32_t    _length;
    uint64_t    _xsdt_address;
    uint8_t     _ext_checksum;
    uint8_t     _reserved[3];

} _JOS_PACKED;
typedef struct _rsdp_descriptor rsdp_descriptor_t;

static bool _valid_rsdp_checksum(rsdp_descriptor_t* desc)
{
    return _validate_st_checksum(desc, 20);    
}

_JOS_MAYBE_UNUSED static bool _valid_rsdp_20_checksum(rsdp_descriptor_t* desc)
{
    return _validate_st_checksum(desc,20) && _validate_st_checksum(&desc->_length, 16);
}

// we use the 2.0 version pointer for both v1 and v2+, since the latter contains the former
rsdp_descriptor_t * _rsdp_desc = 0;

// The SDTs (System Descriptor Table) are found from the RSDP and all SDT entries share this header 
// the sice of this structure is 0x20 bytes
struct _sdt_header 
{
  char      _signature[4];
  uint32_t  _length;
  uint8_t   _revision;
  uint8_t   _checksum;
  char      _oem_id[6];
  char      _oem_table_id[8];
  uint32_t  _oem_revision;
  uint32_t  _creator_id;
  uint32_t  _creator_revision;
} _JOS_PACKED;
typedef struct _sdt_header sdt_header_t;

// helpers for the two variants of the SDT; RSDT and XSDT.

_JOS_MAYBE_UNUSED static bool _valid_sdt_header_checksum(sdt_header_t* header)
{
    return _validate_st_checksum(header, sizeof(sdt_header_t));
}

_JOS_MAYBE_UNUSED static uint32_t* _sdt_pointer_table(sdt_header_t* header)
{
    return (uint32_t*)(header+1);
}

_JOS_MAYBE_UNUSED static size_t _rsdt_pointer_table_size(sdt_header_t* header)
{
    return (header->_length - sizeof(sdt_header_t))/sizeof(uint32_t);
}

_JOS_MAYBE_UNUSED static size_t _xsdt_pointer_table_size(sdt_header_t* header)
{
    return (header->_length - sizeof(sdt_header_t))/sizeof(uint64_t);
}

_JOS_MAYBE_UNUSED static void _dump_sdt_headers(sdt_header_t* header)
{    
    size_t num_entries = _rsdt_pointer_table_size(header);    
    _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "%d headers\n", num_entries);
    while(num_entries--)
    {
        bool valid_checksum = _calculate_st_checksum(header,sizeof(sdt_header_t));
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "%s sdt header @ 0x%x\n\t_signature: %4s\n\t_length = %d\n\t_oem_id = %6s\n\t_oem_table_id = %8s\n\t_creator_id = %4s\n", 
        valid_checksum ? "valid" : "invalid", header, header->_signature, header->_length, header->_oem_id, header->_oem_table_id, &header->_creator_id);

        header =(sdt_header_t*)(((char*)header + header->_length));
    }
}

// ==================================================================

struct _cpu_core_context
{	
	// tasks ready to run, in order of priority
	queue_t _ready[kPri_NumLevels];
	// tasks waiting for something, in order of priority
	queue_t _waiting[kPri_NumLevels];
	// currently running task (only ever one per core, obviously)
	task_context_t* _running;
    // the context of the idle task for this core
    task_context_t* _idle;
	// x2APIC ID
	uint32_t 		_apic_id;
	uint8_t			_local_apic_id;
	int				_boot:1;
    //TODO: these things are really all "perf counters"
	size_t			_task_switches;
};

// we keep a link to the boot processor 
static cpu_core_context_t* _boot = 0;

// vector of cpu_core_context_t's 
static vector_t _cores;

static cpu_core_context_t* _cpu_core_context_create(void)
{
	cpu_core_context_t* ctx = (cpu_core_context_t*)malloc(sizeof(cpu_core_context_t));
    for(size_t n = 0; n < kPri_NumLevels; ++n)
    {
        queue_create(ctx->_ready + n, 8, sizeof(task_context_t*));
		queue_create(ctx->_waiting+ n, 8, sizeof(task_context_t*));
	}
	ctx->_running = ctx->_idle = 0;

	//TODO: this will be executed by the setup code on each CPU
// #ifdef _JOS_KERNEL_BUILD
// 	unsigned _eax, _ebx, _ecx, _edx;
// 	__cpuid(0xb, _eax, _ebx, _ecx, _edx);
// 	// 32 bit x2APIC for *this* CPU
// 	ctx->_apic_id = _edx;
// 	_JOS_KTRACE_CHANNEL(kCpuCoreChannel,"context for core 0x%x\n", ctx->_id);
// #endif
	return ctx;
}

// scan to find the Multi Processor Floating Pointer struct. 
// this is the "old school" approach.
static void _scan_mp(void)
{
    int scan_smp(uintptr_t start, const uintptr_t length)
    {
        static const uintptr_t kMpTag = (('_'<<24) | ('P'<<16) | ('M'<<8) | '_');
        const uintptr_t* end = (const uintptr_t*)(start + length);
        uintptr_t* rp = (uintptr_t*)start;
        while(rp<=end)
        {
            if(*rp == kMpTag)
            {                
                smp_fp_t* smp_fp = (smp_fp_t*)rp;
                if(smp_fp->_length==1 && (smp_fp->_specrev == 0x1 || smp_fp->_specrev == 0x4))
                {
                    _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"MP structure @ 0x%x, version 1.%d\n", rp, smp_fp->_specrev);
                    _smp_fp = smp_fp;
                    return 1;
                }
                // this appears to be something we just have to deal with, both VMWare and Bochs contain at least one of these invalid entries.
            }
            rp = (uintptr_t*)((uint8_t*)rp + 16);
        }
        return 0;
    }

    // look for SMP signature in candidate RAM regions
    if(scan_smp(0,0x400) || scan_smp(639 * 0x400, 0x400) || scan_smp(0xf0000,0x10000))
    {                
        if(!_smp_fp->_feature_information)
        {
            static const uint32_t kPmpTag = (('P'<<24) | ('M'<<16) | ('C'<<8) | 'P');
            // MP configuration table is present
            _smp_config = (const smp_config_header_t*)_smp_fp->_phys_address;
            if(_smp_config->_sig==kPmpTag)
            {
                _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"MP configuration table @ 0x%x with %d entries\n", _smp_config,_smp_config->_oem_entry_count);
                _JOS_KTRACE_CHANNEL_BUF(kCpuCoreChannel,_smp_config->_oem_id, sizeof(_smp_config->_oem_id)); 

                // the OEM entries follow the configuration header
                // do a pass over these to count them first
                size_t processor_count = 0;
                //size_t bus_count = 0;
                //size_t io_apic = 0;
                                                
                const uint8_t* oem_entry_ptr = (const uint8_t*)(_smp_config+1);
                for(uint16_t n =0; n < _smp_config->_oem_entry_count; n++ )
                {
                    switch (oem_entry_ptr[0])
                    {
                        case 0:
                        {
                            // processor
                            const smp_processor_t* smp_proc = (const smp_processor_t*)oem_entry_ptr;
                            oem_entry_ptr += sizeof(smp_processor_t);
                            if(smp_proc->_flags & 1)
                            {
                                ++processor_count;                                
                                // add this core, the information will be updated with more later (in task.c, for example)
                                cpu_core_context_t* context = _cpu_core_context_create();
                                context->_boot = (smp_proc->_flags & 2) ? true:false;
                                context->_local_apic_id = smp_proc->_local_apic_id;
                                vector_push_back_ptr(&_cores, context);
                                if(context->_boot)
                                {
                                    _boot = context;
                                }
                            }
                            else
                            {
                                _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"processor %d is marked as unusable\n", processor_count);                                
                            }
                        }
                        break;
                        case 1:
                        {
                            // bus
                            const smp_bus_t* smp_bus = (const smp_bus_t*)oem_entry_ptr;
                            unsigned char bus_id[6];
                            memcpy(bus_id, smp_bus->_type_string, 6);
                            // clean the bus ID string and 0-terminate it.
                            for(size_t n = 0; n < 6; ++n)
                            {
                                // filter out non-printable characters too
                                if( bus_id[n] == ' ' || ((int)bus_id[n]<32 || ((int)bus_id[n])>127))
                                {
                                    bus_id[n] = 0;
                                    break;
                                }
                            }                            
                            _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"bus %d, %s\n", smp_bus->_bus_id, bus_id);
                            oem_entry_ptr += sizeof(smp_bus_t);
                        }
                        break;
                        case 2:
                        {
                            // I/O apic
                            //_JOS_KTRACE_CHANNEL(kCpuCoreChannel,"I/O apic\n");
                            oem_entry_ptr += 8;
                        }
                        break;
                        case 3:
                        {
                            // I/O int assignment
                            //_JOS_KTRACE_CHANNEL(kCpuCoreChannel,"I/O int assignment\n");
                            oem_entry_ptr += 8;
                        }
                        break;
                        case 4:
                        {
                            // local int assignment
                           // _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"local int assignment\n");
                            oem_entry_ptr += 8;
                        }
                        break;
                        default:;
                    }
                }
            }                        
        }
        else
        {
            _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"TODO: default configurations\n");
        }
    }
    else
    {    
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"no valid _MP_ tag found, processor appears to be single core\n");    
    }
}

static void _scan_rsdp(void)
{
    void scan_rsdp(uintptr_t start, uintptr_t length)
    {
        // "RSD PTR "
        static const uintptr_t kTagLo = (( ' '<<24) | ('D'<<16) | ('S'<<8) | 'R');
        static const uintptr_t kTagHi = ((' '<<24) | ('R'<<16) | ('T'<<8) | 'P');
        const uintptr_t* end = (const uintptr_t*)(start + length);
        uintptr_t* rp = (uintptr_t*)start;
        while(rp<=end)
        {
            if(rp[0] == kTagLo && rp[1] == kTagHi)
            {   
                rsdp_descriptor_t* desc = (rsdp_descriptor_t*)rp;
                if((desc->_revision && _valid_rsdp_20_checksum(desc)) || _valid_rsdp_checksum(desc))
                {
                    _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"valid RSDP structure @ 0x%x, ACPI v%d\n", rp, desc->_revision==0?1:2);
                    _JOS_KTRACE_CHANNEL_BUF(kCpuCoreChannel, desc->_sig, sizeof(desc->_sig));
                    _rsdp_desc = desc;
                    return;
                }
                //zzz:
            }
            rp = (uintptr_t*)((uint8_t*)rp + 16);
        }
    }

    // scan for RSDP structure
    uint16_t* ebd_ptr = (uint16_t*)(0x040e);
    // this isn't always valid (VMWare Workstation has this set to 0)
    if(ebd_ptr[0])
    {
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "EBDA base address 0x%x\n", ((uintptr_t)(*ebd_ptr)) << 4);
        scan_rsdp((((uintptr_t)(*ebd_ptr)) << 4), 1024);
    }
    if(!_rsdp_desc)
    {
        scan_rsdp(0xe0000, 0xfffff-0xe0000);
    }
}

// scan to find the Root System Descriptor pointer, the newer ACPI, approach
static void _smp_init(void)
{    
    // first we scan this
    _scan_rsdp();
    //TODO: if(!_rsdp_desc)
    {
        _scan_mp();
    }

    _JOS_ASSERT(_rsdp_desc);
    // //TESTING:
    if(_rsdp_desc->_revision)
    {
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"XDT header @ 0x%x\n", _rsdp_desc->_xsdt_address);

        void* xsdt_vmem = _k_vmem_reserve_range(1);
        _k_vmem_map((uintptr_t)xsdt_vmem, _rsdp_desc->_xsdt_address);

        sdt_header_t* sdt = (sdt_header_t*)xsdt_vmem;
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"%d entries in table 0x%x\n", _xsdt_pointer_table_size(sdt), _sdt_pointer_table(sdt));
        // uint64_t* ptr_entries = (uint64_t*)_sdt_pointer_table(sdt);
        // for(size_t n = _xsdt_pointer_table_size(sdt); n>0; --n)
        // {
        //     sdt_header_t* h = (sdt_header_t*)((uint32_t)(*ptr_entries++));
        //     _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"xsdt header 0x%x\n", h);            
        //     _JOS_KTRACE_CHANNEL_BUF(kCpuCoreChannel, h->_signature, sizeof(h->_signature));
        // }
    }
    else
    {
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"RSDT @ phys 0x%x\n", _rsdp_desc->_rsdt_address);
        // map it somewhere so that we can access it
        void* rsdt_vmem = _k_vmem_reserve_range(1);
        _k_vmem_map((uintptr_t)rsdt_vmem, _rsdp_desc->_rsdt_address);
        _dump_sdt_headers((sdt_header_t*)rsdt_vmem);

        if (_validate_st_checksum(rsdt_vmem, ((sdt_header_t*)rsdt_vmem)->_length) )
        {
            sdt_header_t* sdt = (sdt_header_t*)rsdt_vmem;
            _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"%d entries in table 0x%x\n", _rsdt_pointer_table_size(sdt), _sdt_pointer_table(sdt));
            _JOS_KTRACE_CHANNEL_BUF(kCpuCoreChannel, sdt->_signature, sizeof(sdt->_signature));
        }

        // uint32_t* ptr_entries = _sdt_pointer_table(sdt);
        // for(size_t n = _rsdt_pointer_table_size(sdt); n>0; --n)
        // {
        //     sdt_header_t* h = (sdt_header_t*)(*ptr_entries++);
        //     _JOS_KTRACE_CHANNEL(kCpuCoreChannel,"rsdt header 0x%x\n", h);
        //     _JOS_KTRACE_CHANNEL_BUF(kCpuCoreChannel, h->_signature, sizeof(h->_signature));
        // }
    }
}

void _k_cpu_core_for_each(void (*func)(cpu_core_context_t* ctx))
{
	for(size_t n = 0; n < vector_size(&_cores); ++n)
	{
		func((cpu_core_context_t*)vector_at_ptr(&_cores, n));
	}
}

task_context_t* _k_cpu_core_running_task(cpu_core_context_t* ctx)
{
    _JOS_ASSERT(ctx);
    return ctx->_running;
}

void _k_cpu_core_add_task(cpu_core_context_t* ctx, task_context_t* task)
{
    _JOS_ASSERT(ctx);
    _JOS_ASSERT(task);
    //_JOS_ASSERT(task->_pri<kPri_NumLevels);

    //TODO: if ctx != current we need to lock 

    if(k_task_priority(task)==kPri_Idle)    
    {
        // disallow switching idle tasks, for now
        //_JOS_ASSERT(ctx->_idle == 0);
        ctx->_idle = task;
    }
    else
    {        
        queue_push_ptr(ctx->_ready + k_task_priority(task), task);
    }
}

cpu_core_context_t* _k_cpu_core_this(void)
{
	//ZZZ:
	return _boot;
}

bool _k_cpu_core_context_schedule(cpu_core_context_t* ctx)
{
	task_context_t* curr = ctx->_running;
	queue_t* queue = 0;
	if( !queue_is_empty(ctx->_ready+kPri_Highest) )
	{		
		queue = ctx->_ready + kPri_Highest;
	}
	else if( !queue_is_empty(ctx->_ready+kPri_Medium) )
	{
		queue = ctx->_ready + kPri_Medium;
	}
	else if( !queue_is_empty(ctx->_ready+kPri_Low) )
	{
		queue = ctx->_ready + kPri_Low;
	}
	if(queue)
	{
		// switch to the front most task
		ctx->_running = (task_context_t*)queue_front_ptr(queue);
		queue_pop(queue);
	}
    else 
    {
        // just switch to idle
        _JOS_ASSERT(ctx->_idle);
        ctx->_running = ctx->_idle;
    }
	// ok to schedule ctx->_running now
	return( curr != ctx->_running );
}

void k_cpu_core_init(void)
{
    //ZZZ: should be per CPU
    if ( !k_cpu_check() )
    {
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "unsupported CPU\n");
        _JOS_KERNEL_PANIC();
    }
    vector_create(&_cores, 64, sizeof(void*));
    if(k_cpu_feature_present(kCpuFeature_APIC))
    {
        _smp_init();
        if(!_boot)
        {
            _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "fatal: no boot processor identified\n");
            _JOS_KERNEL_PANIC();
        }
    }
    else
    {
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "non-APIC CPU, yet supported, this is unknown territory\n");
        // we're obviously running on *something*
        cpu_core_context_t* context = _cpu_core_context_create();
        context->_boot = true;
        context->_local_apic_id = 0;
        vector_push_back_ptr(&_cores, context);
    }

    void dump_core_info(cpu_core_context_t* ctx)
    {
        _JOS_KTRACE_CHANNEL(kCpuCoreChannel, "core: 0x%x, %s\n", ctx->_local_apic_id, ctx->_boot?"boot":"application");
    }
    _k_cpu_core_for_each(dump_core_info);
    
}