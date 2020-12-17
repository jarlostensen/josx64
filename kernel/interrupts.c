// interrupt handling

#include <interrupts.h>
#include <kernel.h>
#include <string.h>
#include <x86_64.h>

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

} _JOS_PACKED_ isr_stacx86_64_t;

enum {
    kInterruptGate  = 0xe,
    kTrapGate       = 0xf,
    kCallGate       = 0xc,
    kTaskGate       = 0x5,
};

// ==============================================================================
// PIC
// https://wiki.osdev.org/PIC
#define PIC1		0x20        // master PIC
#define PIC2		0xA0		// slave PIC
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04	/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08	/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C	/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */
#define IRQ_BASE_OFFSET 0x20    // the offset we apply to the IRQs to map them outside of the lower reserved ISR range


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

void interrupts_isr_handler(isr_stacx86_64_t *stack) {

    //ZZZ:
    wchar_t buf[128];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

    // past ss and rsp
    uint64_t rsp = ((uint64_t*)stack->rsp)[0];
    uint64_t ss = ((uint64_t*)stack->rsp)[8];
    uint64_t arg = ((uint64_t*)stack->rsp)[16];

    swprintf(buf,bufcount,L"int 0x%x, error code 0x%x: *rip : 0x%llx[0] = 0x%x, rsp = 0x%llx, arg = 0x%llx\n", 
        stack->handler_id, 
        stack->error_code,
        stack->rip,
        ((uint8_t*)stack->rip)[0],
        ss,rsp,
        arg);
    output_console_output_string(buf);

    if ( stack->handler_id == 0xd ) {
        output_console_output_string(L"\nGPF, halting...");
        halt_cpu();   
    }
}

void interrupts_check_return_stack(uint64_t* rsp) {
    wchar_t wbuf[64];
    swprintf(wbuf, sizeof(wbuf)/sizeof(wchar_t), L"->rip = 0x%llx\n", rsp[0]);
    output_console_output_string(wbuf);
}

void interrupts_irq_handler(int irqId) {

}

static void init_legacy_pic(void) {    
    // http://www.brokenthorn.com/Resources/OSDevPic.html
    // start initialising PIC1 and PIC2 
    x86_64_outb(PIC1_COMMAND , ICW1_INIT | ICW1_ICW4);
	x86_64_outb(PIC2_COMMAND , ICW1_INIT | ICW1_ICW4);
    // remap IRQs offsets to 0x20 and 0x28
	x86_64_outb(PIC1_DATA , IRQ_BASE_OFFSET);
	x86_64_outb(PIC2_DATA , IRQ_BASE_OFFSET+8);
    x86_64_io_wait();
	// cascade PIC1 & PIC2
	x86_64_outb(PIC1_DATA , 0x00);  
	x86_64_outb(PIC2_DATA , 0x00);  
    x86_64_io_wait();
	// enable x86 mode
	x86_64_outb(PIC1_DATA , 0x01);
	x86_64_outb(PIC2_DATA , 0x01);	
    x86_64_io_wait();    
	// disable all IRQs for now, only enable when someone registers an IRQ handler.
	x86_64_outb(PIC1_DATA, 0xff);
	x86_64_outb(PIC2_DATA, 0xff);
    x86_64_io_wait();   
}

void interrupts_initialise_early(void) {

    init_legacy_pic();

    // load IDT with the initial handlers

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
    idt_init(_idt+IRQ_BASE_OFFSET+N, interrupts_irq_handler_##N)

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

    //TEST:
    asm volatile (
        "nop\r\n"
        "int $0x3\r\n"
        "nop\r\n"        
        );

    output_console_output_string(L"interrupts initialised\n");
}

