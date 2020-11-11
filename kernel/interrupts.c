// interrupt handling

#include <interrupts.h>
#include <string.h>

// in efi_main.c for now, loaded with the kernel's CS selector on boot
extern uint16_t kJosKernelCS;

typedef struct _isr_stack
{    
    // bottom of stack (rsp)

    uint64_t        rdi, rsi, rbp, rbx, rdx, rcx, rax;
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
EXTERN_ISR_HANDLER(32);

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

void interrupts_isr_handler(isr_stack_t *stack) {
    //TODO:
    (void)stack;
}

void interrupts_initialise(void) {
    
// load IDT with the initial handlers

#define SET_ISR_HANDLER(N)\
    idt_init(_idt+N, interrupts_isr_handler_##N)

    SET_ISR_HANDLER(0);
    SET_ISR_HANDLER(1);
    SET_ISR_HANDLER(2);
    SET_ISR_HANDLER(3);
    // etc...

    //TODO: load_idt(_idt_desc);
}

