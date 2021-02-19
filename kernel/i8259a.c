// 8259A Programmable Interrupt Controller

#include <jos.h>
#include <kernel.h>
#include <x86_64.h>
#include <i8259a.h>

// https://wiki.osdev.org/PIC
// http://www.brokenthorn.com/Resources/OSDevPic.html
#define PIC1		0x20        // master PIC
#define PIC2		0xA0		// slave PIC
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)
#define ICW1_ICW4	0x01		/* ICW4 */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04	/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08	/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C	/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

// IRQ enabled bitmask, 0 when no IRQs enabled
// IRQ 2 must always be enabled 
unsigned int _i8259a_irq_mask = _JOS_i8259a_IRQ_CLEAR_MASK;

void i8259a_enable_irq(int i)
{
    if(i < 8)
    {
        // unmask IRQ in PIC1
	    x86_64_outb(PIC1_DATA, x86_64_inb(PIC1_DATA) & ~(1<<i));
    }
    else
    {
        // unmask IRQ in PIC2
	    x86_64_outb(PIC2_DATA, x86_64_inb(PIC2_DATA) & ~(1<<(i-8)));
    } 

    // enable in irq mask
    _i8259a_irq_mask |= (1<<i);
}

void i8259a_disable_irq(int i)
{
    if(i < 8)
    {
        // mask IRQ in PIC1
        uint8_t masked = x86_64_inb(PIC1_DATA) | (1<<i);
	    x86_64_outb(PIC1_DATA, masked);
    }
    else
    {
        // nmask IRQ in PIC2
        uint8_t masked = x86_64_inb(PIC2_DATA) | (1<<(i-8));
	    x86_64_outb(PIC2_DATA, masked);
    } 

    // disable in irq mask
    _i8259a_irq_mask &= ~(1<<i);
}

void i8259a_initialise(void) {
        
    // initialise PIC1 and PIC2 in cascade mode

    x86_64_outb(PIC1_COMMAND , ICW1_INIT | ICW1_ICW4);
	x86_64_outb(PIC2_COMMAND , ICW1_INIT | ICW1_ICW4);
    // remap IRQs offsets to 0x20 and 0x28
	x86_64_outb(PIC1_DATA , _JOS_i8259a_IRQ_BASE_OFFSET);
	x86_64_outb(PIC2_DATA , _JOS_i8259a_IRQ_BASE_OFFSET+8);
    x86_64_io_wait();
	// cascade PIC1 & PIC2 over IR line 2
	x86_64_outb(PIC1_DATA , 0x04);  
	x86_64_outb(PIC2_DATA , 0x02);
    x86_64_io_wait();
	// enable x86 mode
	x86_64_outb(PIC1_DATA , 0x01);
	x86_64_outb(PIC2_DATA , 0x01);
    x86_64_io_wait();    

	//NOTE: disable all IRQs except for 2 for now, only enable when someone registers an IRQ handler.
    //NOTE: 2 MUST be enabled on PIC1 since PIC2 communicates with PIC1 through it
	x86_64_outb(PIC1_DATA, ~((uint8_t)_JOS_i8259a_IRQ_CLEAR_MASK));
	x86_64_outb(PIC2_DATA, 0xff);
    x86_64_io_wait();   

}