// interrupt handling

#include <interrupts.h>
#include <kernel.h>
#include <string.h>
#include <x86_64.h>
#include <i8259a.h>

#include <stdio.h>
#include <output_console.h>

// in efi_main.c for now, loaded with the kernel's CS selector on boot
extern uint16_t kJosKernelCS;

typedef struct _isr_stack
{    
    // bottom of stack (rsp)

    uint64_t        rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t        r15, r14, r13, r12, r11, r10, r9, r8;

    uint64_t        handler_id;
    uint64_t        error_code; //< will be pushed as 0 by our stub if not done by the CPU    

    uint64_t        rip;    
    uint64_t        cs;
    uint64_t        rflags;
    //NOTE: CPU always pushes these 64-bit mode (not just for CPL changes) 
    uint64_t        rsp;
    uint64_t        ss;     // <- top of stack (rsp + 184)

} _JOS_PACKED_ isr_stack_t;

enum {
    kInterruptGate  = 0xe,
    kTrapGate       = 0xf,
    kCallGate       = 0xc,
    kTaskGate       = 0x5,
};

static isr_handler_func_t _isr_handlers[256];
static irq_handler_func_t _irq_handlers[32];
// we use this to control interrupts at a "soft" level; if this flag is true we forward interrupts to ISR handlers
// if false we don't, and we may filter some IRQs as well
static bool _interrupts_enabled = true;


// Intel IA dev guide Vol 3 6.14.1 64-Bit Mode IDT
typedef struct _idt_64_entry {

    uint16_t        offset_lo;
    uint16_t        segment_sel;
    uint16_t        ist:3,          // interrupt stack table
                    zero:5,     
                    type:5,         // interrupt, trap, call, or task gate
                    dpl:2,          // priority level
                    p:1;            // segment present
    uint16_t        offset_mid;
    uint32_t        offset_hi;
    uint32_t        reserved0;

} _JOS_PACKED_ idt_entry_t;

typedef struct _idt_desc {

    uint16_t        size;
    uint64_t        address;

} _JOS_PACKED_ idt_desc_t;

// our IDT and IDT Descriptors
static idt_entry_t  _idt[256];
static idt_desc_t   _idt_desc = { .size = sizeof(_idt), .address = (uint64_t)&_idt};

// interrupt handler nesting level
static atomic_int _nesting_level = -1;
#define INC_NESTING_LEVEL()\
    atomic_fetch_add(&_nesting_level,1)

#define DEC_NESTING_LEVEL()\
    atomic_fetch_sub(&_nesting_level,1)

// handler stubs (from x84_64.asm)
#define EXTERN_ISR_HANDLER(N)\
    extern void interrupts_isr_handler_##N(void)

EXTERN_ISR_HANDLER(0);
EXTERN_ISR_HANDLER(1);
EXTERN_ISR_HANDLER(2);
EXTERN_ISR_HANDLER(3);
EXTERN_ISR_HANDLER(4);
EXTERN_ISR_HANDLER(5);
EXTERN_ISR_HANDLER(6);
EXTERN_ISR_HANDLER(7);
EXTERN_ISR_HANDLER(8);
EXTERN_ISR_HANDLER(9);
EXTERN_ISR_HANDLER(10);
EXTERN_ISR_HANDLER(11);
EXTERN_ISR_HANDLER(12);
EXTERN_ISR_HANDLER(13);
EXTERN_ISR_HANDLER(14);
EXTERN_ISR_HANDLER(15);
EXTERN_ISR_HANDLER(16);
EXTERN_ISR_HANDLER(17);
EXTERN_ISR_HANDLER(18);
EXTERN_ISR_HANDLER(19);
EXTERN_ISR_HANDLER(20);
EXTERN_ISR_HANDLER(21);
EXTERN_ISR_HANDLER(22);
EXTERN_ISR_HANDLER(23);
EXTERN_ISR_HANDLER(24);
EXTERN_ISR_HANDLER(25);
EXTERN_ISR_HANDLER(26);
EXTERN_ISR_HANDLER(27);
EXTERN_ISR_HANDLER(28);
EXTERN_ISR_HANDLER(29);
EXTERN_ISR_HANDLER(30);
EXTERN_ISR_HANDLER(31);

#define EXTERN_IRQ_HANDLER(N)\
    extern void interrupts_irq_handler_##N(void)

EXTERN_IRQ_HANDLER(0);
EXTERN_IRQ_HANDLER(1);
EXTERN_IRQ_HANDLER(2);
EXTERN_IRQ_HANDLER(3);
EXTERN_IRQ_HANDLER(4);
EXTERN_IRQ_HANDLER(5);
EXTERN_IRQ_HANDLER(6);
EXTERN_IRQ_HANDLER(7);
EXTERN_IRQ_HANDLER(8);
EXTERN_IRQ_HANDLER(9);
EXTERN_IRQ_HANDLER(10);
EXTERN_IRQ_HANDLER(11);
EXTERN_IRQ_HANDLER(12);
EXTERN_IRQ_HANDLER(13);
EXTERN_IRQ_HANDLER(14);
EXTERN_IRQ_HANDLER(15);
EXTERN_IRQ_HANDLER(16);
EXTERN_IRQ_HANDLER(17);
EXTERN_IRQ_HANDLER(18);
EXTERN_IRQ_HANDLER(19);

// returns 64 bit RIP of interrupt handler from entry
static uint64_t idt_get_rip(idt_entry_t* entry) {
    return (uint64_t)entry->offset_lo | (uint64_t)(entry->offset_mid << 16) | ((uint64_t)(entry->offset_hi) << 32);
}
// set 64 bit RIP of interrupt handler in entry
static void idt_set_rip(idt_entry_t* entry, uint64_t rip) {
    entry->offset_lo = (uint16_t)(rip & 0xffff);
    entry->offset_mid = (uint16_t)((rip>>16) & 0xffff);
    entry->offset_hi = (uint32_t)(rip >> 32);
}

static void idt_init(idt_entry_t* entry, void* handler) {

    memset(entry, 0, sizeof(idt_entry_t));
    entry->segment_sel = kJosKernelCS;
    entry->type = kInterruptGate;
    entry->p = 1;
    idt_set_rip(entry, (uint64_t)handler);
}

void interrupts_isr_set_handler(int i, isr_handler_func_t handler) {
    //TODO: assert on handler already being set
    x86_64_cli();
    _isr_handlers[i] = handler;
    x86_64_sti();
}

void interrupts_isr_handler(isr_stack_t *stack) {

    if ( _interrupts_enabled )
    {        
        isr_handler_func_t handler = _isr_handlers[stack->handler_id];
        if( handler ) {

            // provide a read only context for the handler
            isr_context_t ctx;
            memcpy(&ctx.rdi, &stack->rdi, 15*sizeof(uint64_t));
            ctx.handler_code = stack->error_code;
            ctx.rip = stack->rip;
            ctx.cs = stack->cs;
            ctx.rflags = stack->rflags; 

            x86_64_sti();       
            handler(&ctx);
            x86_64_cli();
        }
        else {
            // =============================================================
            //ZZZ:
            wchar_t buf[128];
            const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

            swprintf(buf,bufcount,L"UNHANDLED: int 0x%x, error code 0x%x: rip : 0x%llx\n", 
                stack->handler_id, 
                stack->error_code,
                stack->rip);
            output_console_output_string(buf);
        }
    }

    // and hard handling, like this one
    if ( stack->handler_id == 0xd ) {
        output_console_output_string(L"\nGPF, halting...");
        halt_cpu();   
    }
}

void interrupts_set_isr_handler(int i, isr_handler_func_t handler) {
    //TODO: assert on handler already being set
    x86_64_cli();
    _isr_handlers[i] = handler;
    x86_64_sti();
}

void interrupts_irq_handler(int irq) {
    
    if ( i8259a_irqs_muted() )
        // no IRQ handlers enabled    
        return;

    irq_handler_func_t handler = _irq_handlers[irq];
    if ( handler
        &&
        i8259a_irq_enabled(irq)) {
            x86_64_sti();
            handler(irq);
            x86_64_cli();
    }
}

void interrupts_set_irq_handler(int irqId, irq_handler_func_t handler) {
    //TODO: check if this IRQ is enabled or not, it shouldn't be (for now we only allow one handler ever)
    x86_64_cli();
    _irq_handlers[irqId] = handler;
    x86_64_sti();
    // enable it right away
    i8259a_enable_irq(irqId);
}

void interrupts_initialise_early(void) {

    // initialise PICs
    i8259a_initialise();

    // load IDT with reserved ISRs

#define SET_ISR_HANDLER(N)\
    idt_init(_idt+N, interrupts_isr_handler_##N)

    SET_ISR_HANDLER(0);
    SET_ISR_HANDLER(1);
    SET_ISR_HANDLER(2);
    SET_ISR_HANDLER(3);
    SET_ISR_HANDLER(4);
    SET_ISR_HANDLER(5);
    SET_ISR_HANDLER(6);
    SET_ISR_HANDLER(7);
    SET_ISR_HANDLER(8);
    SET_ISR_HANDLER(9);
    SET_ISR_HANDLER(10);
    SET_ISR_HANDLER(11);
    SET_ISR_HANDLER(12);
    SET_ISR_HANDLER(13);
    SET_ISR_HANDLER(14);
    SET_ISR_HANDLER(15);
    SET_ISR_HANDLER(16);
    SET_ISR_HANDLER(17);
    SET_ISR_HANDLER(18);
    SET_ISR_HANDLER(19);
    SET_ISR_HANDLER(20);
    SET_ISR_HANDLER(21);
    SET_ISR_HANDLER(22);
    SET_ISR_HANDLER(23);
    SET_ISR_HANDLER(24);
    SET_ISR_HANDLER(25);
    SET_ISR_HANDLER(26);
    SET_ISR_HANDLER(27);
    SET_ISR_HANDLER(28);
    SET_ISR_HANDLER(29);
    SET_ISR_HANDLER(30);
    SET_ISR_HANDLER(31);

#define SET_IRQ_HANDLER(N)\
    idt_init(_idt+_JOS_i8259a_IRQ_BASE_OFFSET+N, interrupts_irq_handler_##N)

    SET_IRQ_HANDLER(0);
    SET_IRQ_HANDLER(1);
    SET_IRQ_HANDLER(2);
    SET_IRQ_HANDLER(3);
    SET_IRQ_HANDLER(4);
    SET_IRQ_HANDLER(5);
    SET_IRQ_HANDLER(6);
    SET_IRQ_HANDLER(7);
    SET_IRQ_HANDLER(8);
    SET_IRQ_HANDLER(9);
    SET_IRQ_HANDLER(10);
    SET_IRQ_HANDLER(11);
    SET_IRQ_HANDLER(12);
    SET_IRQ_HANDLER(13);
    SET_IRQ_HANDLER(14);
    SET_IRQ_HANDLER(15);
    
    x86_64_load_idt(&_idt_desc);

    //ZZZ: trace
    output_console_output_string(L"interrupts initialised\n");
}

