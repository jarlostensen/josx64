#include "kernel_detail.h"
#include "../include/kernel/kernel.h"
#include "dt.h"
#include "interrupts.h"

#include <stdio.h>
#include <string.h>

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

// ==============================================================================
// IDT
// https://wiki.osdev.org/Interrupt_Descriptor_Table
struct idt_type_attr_struct
{
    /* 
    only care about either of these:
    0x5 : 80386 32 bit task gate
    0xe : 80386 32-bit interrupt gate
    0xf : 80386 32-bit trap gate
    */
    uint8_t gate_type : 4;
    // 0 for interrupt and trap gates
    uint8_t storage_segment : 1;
    // Descriptor Privilege Level
    uint8_t dpl : 2;
    // 0 for unused interrupts
    uint8_t present:1;
};
typedef struct idt_type_attr_struct idt_type_attr_t;

struct idt_entry_struct
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    union 
    {
        uint8_t byte;
        idt_type_attr_t fields;
    } type_attr;
    uint16_t offset_high;
} __attribute((packed));
typedef struct idt_entry_struct idt_entry_t;

struct idt32_descriptor_struct 
{
    uint16_t size;
    uint32_t address;
} __attribute((packed));
typedef struct idt32_descriptor_struct idt32_descriptor_t;

// ISR stub handlers (interrupts.asm)
extern void _k_isr0();
extern void _k_isr1();
extern void _k_isr2();
extern void _k_isr3();
extern void _k_isr4();
extern void _k_isr5();
extern void _k_isr6();
extern void _k_isr7();
extern void _k_isr8();
extern void _k_isr9();
extern void _k_isr10();
extern void _k_isr11();
extern void _k_isr12();
extern void _k_isr13();
extern void _k_isr14();
extern void _k_isr15();
extern void _k_isr16();
extern void _k_isr17();
extern void _k_isr18();
extern void _k_isr19();
extern void _k_isr20();
extern void _k_isr21();
extern void _k_isr22();
extern void _k_isr23();
extern void _k_isr24();
extern void _k_isr25();
extern void _k_isr26();
extern void _k_isr27();
extern void _k_isr28();
extern void _k_isr29();
extern void _k_isr30();
extern void _k_isr31();
extern void _k_isr32();

// IRQ stub handlers (interrupts.asm)
extern void _k_irq0();
extern void _k_irq1();
extern void _k_irq2();
extern void _k_irq3();
extern void _k_irq4();
extern void _k_irq5();
extern void _k_irq6();
extern void _k_irq7();
extern void _k_irq8();
extern void _k_irq9();
extern void _k_irq10();
extern void _k_irq11();
extern void _k_irq12();
extern void _k_irq13();
extern void _k_irq14();
extern void _k_irq15();
extern void _k_irq16();

// in arch/i386/interrupts.asm
extern void _k_load_idt(void);
extern void _k_store_idt(idt32_descriptor_t* desc);

static idt_entry_t _idt[256];
idt32_descriptor_t _idt_desc = {.size = sizeof(_idt), .address = (uint32_t)(&_idt)};
static isr_handler_func_t _isr_handlers[256];
static irq_handler_func_t _irq_handlers[32];

void _k_isr_handler(isr_stack_t isr_stack)
{
    if(_isr_handlers[isr_stack.handler_id])
    {
        _isr_handlers[isr_stack.handler_id](isr_stack.error_code, isr_stack.cs, isr_stack.eip);
        return;
    }
    // no handler
    printf("_isr_handler: unhandled interrupt 0x%x, next instruction @ 0x%x : 0x%x\n", isr_stack.handler_id, isr_stack.cs, isr_stack.eip);
}

void _k_irq_handler(int irq_num)
{
    //NOTE: see interrupts.asm IRQ stub for intro-extro code (including interrupt ack to PIC)
    if( _irq_handlers[irq_num] )
    {
        _irq_handlers[irq_num](irq_num+IRQ_BASE_OFFSET);
        return;
    }
    printf("_k_irq_handler: unhandled IRQ 0x%x\n", irq_num);    
}

static void idt_set_gate(uint8_t i, uint16_t sel, uint32_t offset, idt_type_attr_t* type_attr)
{
    idt_entry_t* entry = _idt + i;
    entry->offset_high = (uint16_t)((offset >> 16) & 0x0000ffff);
    entry->offset_low = offset & 0xffff;
    entry->selector = sel;
    entry->type_attr.fields = *type_attr;
}

isr_handler_func_t k_set_isr_handler(int i, isr_handler_func_t handler)
{
    isr_handler_func_t prev = _isr_handlers[i];
    _k_disable_interrupts();
    _isr_handlers[i] = handler;
    _k_enable_interrupts();
    //DEBUG: printf("k_set_isr_handler 0x%x, prev = 0x%x, new = 0x%x\n", i, prev, handler);
    return prev;
}

void k_set_irq_handler(int i, irq_handler_func_t handler)
{
    _JOS_ASSERT(_irq_handlers[i]==0);
    _JOS_ASSERT(i >= 0 && i < 31);
    //DEBUG:printf("k_set_irq_handler 0x%x, 0x%x\n", i, handler);
    _irq_handlers[i] = handler;    
}

void k_enable_irq(int i)
{
    if(i < 8)
    {
        // unmask IRQ in PIC1
	    k_outb(PIC1_DATA, k_inb(PIC1_DATA) & ~(1<<i));
    }
    else
    {
        // unmask IRQ in PIC2
	    k_outb(PIC2_DATA, k_inb(PIC2_DATA) & ~(1<<(i-8)));
    } 
}

void k_disable_irq(int i)
{
    if(i < 8)
    {
        // mask IRQ in PIC1
	    k_outb(PIC1_DATA, k_inb(PIC1_DATA) | (1<<i));
    }
    else
    {
        // nmask IRQ in PIC2
	    k_outb(PIC2_DATA, k_inb(PIC2_DATA) | (1<<(i-8)));
    } 
}

bool k_irq_enabled(int i)
{
    if(i < 8)
    {
        return (k_inb(PIC1_DATA) & (1<<i)) == 0;
    }
    else
    {
        return (k_inb(PIC2_DATA) & (1<<(i-8))) == 0;
    } 
}

void _k_init_isrs()
{
    _JOS_KTRACE("_k_init_isrs\n");
    memset(_idt, 0, sizeof(_idt));
    memset(_isr_handlers, 0, sizeof(_isr_handlers));
    memset(_irq_handlers, 0, sizeof(_irq_handlers));

    // http://www.brokenthorn.com/Resources/OSDevPic.html
    // start initialising PIC1 and PIC2 
    k_outb(PIC1_COMMAND , ICW1_INIT | ICW1_ICW4);
	k_outb(PIC2_COMMAND , ICW1_INIT | ICW1_ICW4);
    // remap IRQs offsets to 0x20 and 0x28
	k_outb(PIC1_DATA , IRQ_BASE_OFFSET);
	k_outb(PIC2_DATA , IRQ_BASE_OFFSET+8);
    k_io_wait();
	// cascade PIC1 & PIC2
	k_outb(PIC1_DATA , 0x00);  
	k_outb(PIC2_DATA , 0x00);  
    k_io_wait();
	// enable x86 mode
	k_outb(PIC1_DATA , 0x01);
	k_outb(PIC2_DATA , 0x01);	
    k_io_wait();    
	// disable all IRQs for now, only enable when someone registers an IRQ handler.
	k_outb(PIC1_DATA, 0xff);
	k_outb(PIC2_DATA, 0xff);
    k_io_wait();    

#define K_ISR_SET(i)\
    idt_set_gate(i,K_CODE_SELECTOR,(uint32_t)_k_isr##i,&(idt_type_attr_t){.gate_type = 0xe, .dpl = 0, .present = 1})

    K_ISR_SET(0);
    K_ISR_SET(1);
    K_ISR_SET(2);
    K_ISR_SET(3);
    K_ISR_SET(4);
    K_ISR_SET(5);
    K_ISR_SET(6);
    K_ISR_SET(7);
    K_ISR_SET(8);
    K_ISR_SET(9);
    K_ISR_SET(10);
    K_ISR_SET(11);
    K_ISR_SET(12);
    K_ISR_SET(13);
    K_ISR_SET(14);
    K_ISR_SET(15);
    K_ISR_SET(16);
    K_ISR_SET(17);
    K_ISR_SET(18);
    K_ISR_SET(19);
    K_ISR_SET(20);
    K_ISR_SET(21);
    K_ISR_SET(22);
    K_ISR_SET(23);
    K_ISR_SET(24);
    K_ISR_SET(25);
    K_ISR_SET(26);
    K_ISR_SET(27);
    K_ISR_SET(28);
    K_ISR_SET(29);
    K_ISR_SET(30);
    K_ISR_SET(31);

#define K_IRQ_SET(i)\
    idt_set_gate(IRQ_BASE_OFFSET+i,K_CODE_SELECTOR,(uint32_t)_k_irq##i,&(idt_type_attr_t){.gate_type = 0xe, .dpl = 0, .present = 1})

    K_IRQ_SET(0);
    K_IRQ_SET(1);
    K_IRQ_SET(2);
    K_IRQ_SET(3);
    K_IRQ_SET(4);
    K_IRQ_SET(5);
    K_IRQ_SET(6);
    K_IRQ_SET(7);
    K_IRQ_SET(8);
    K_IRQ_SET(9);
    K_IRQ_SET(10);
    K_IRQ_SET(11);
    K_IRQ_SET(12);
    K_IRQ_SET(13);
    K_IRQ_SET(14);
    K_IRQ_SET(15);
}

void _k_load_isrs()
{
    _JOS_KTRACE("_k_load_isrs\n");
    //TODO: some error checking?
    // make it so!
    _k_load_idt(); 
}

