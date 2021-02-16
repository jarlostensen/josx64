

#include <jos.h>
#include <x86_64.h>
#include <i8253.h>

// http://www.brokenthorn.com/Resources/OSDev16.html

#define PIT_COMMAND 0x43
#define PIT_DATA_0  0x40    // channel/counter 0; IRQ0 generator
#define PIT_DATA_1  0x41    // channel/counter 1; (deprectated) memory refresh signal [not used]
#define PIT_DATA_2  0x42    // channel/counter 2; PC speaker

// PIT command word flags
#define PIT_COUNTER_0           0
#define PIT_COUNTER_1           (1u<<5)
#define PIT_COUNTER_2           (2u<<5)

#define PIT_MODE_SQUAREWAVE     0x06     // binary, square wave
#define PIT_RL_DATA             0x30     // LSB, then MSB
#define PIT_MODE_ONESHOT        0x01     // strictly one-shot in BCD counting mode


// wait for one 18 Hz period (~55ms)
void i8253_wait_55ms(void)
{
    // channel 2 enable (see for example a nice overview of the 8253 here https://www.cs.usfca.edu/~cruse/cs630f08/lesson15.ppt)
    // this enables GATE2 on the PIT (bit 0 = 1) which means that channel 2 will generate a signal, 
    // but also disables the speaker output (bit 1 = 0)
    x86_64_outb(0x61,(x86_64_inb(0x61) & ~0x02) | 0x01);

    x86_64_outb(PIT_COMMAND, PIT_COUNTER_2 | PIT_MODE_ONESHOT);
    // set the timer to be the max interval, i.e. 18.2 Hz
    x86_64_outb(PIT_DATA_2, 0xff);
    x86_64_outb(PIT_DATA_2, 0xff);
    
    uint8_t p61status = x86_64_inb(0x61);
    while((p61status & 0x20)==0) {
        p61status = x86_64_inb(0x61);
    }

    // disable GATE2 channel 2 timer
    x86_64_outb(0x61,(x86_64_inb(0x61) & ~0x02));

    // // dummy read, give time for the next edge rise
    // x86_64_inb(PIT_DATA_2);
    // char msb = x86_64_inb(PIT_DATA_2);
    // do {
    //     x86_64_inb(PIT_DATA_2);
    //     msb = x86_64_inb(PIT_DATA_2);
    // } while(msb);
}

void i8253_start_clock(uint16_t divisor) {
    x86_64_outb(PIT_COMMAND, PIT_COUNTER_0 | PIT_MODE_SQUAREWAVE | PIT_RL_DATA);
    // set frequency         
    x86_64_outb(PIT_DATA_0, divisor & 0xff);
    x86_64_outb(PIT_DATA_0, (divisor>>8) & 0xff);
}