
#include <jos.h>
#include <kernel.h>
#include <x86_64.h>

#include <stdio.h>
#include <output_console.h>

// http://www.brokenthorn.com/Resources/OSDev16.html

#define PIT_COMMAND 0x43
#define PIT_DATA_0  0x40
#define PIT_DATA_2  0x42

// PIT command word flags
#define PIT_COUNTER_0           0
#define PIT_COUNTER_1           0x20
#define PIT_COUNTER_2           0x40

#define PIT_MODE_SQUAREWAVE     0x06     // binary, square wave
#define PIT_RL_DATA             0x30    // LSB, then MSB
#define PIT_MODE_ONESHOT        0x01

// wait for one 18 Hz period (~55ms)
static void _wait_one_55ms_interval(void)
{
    x86_64_outb(PIT_COMMAND, PIT_COUNTER_2 | PIT_MODE_ONESHOT);
    // set the timer to be the max interval, i.e. 18.2 Hz
    x86_64_outb(PIT_DATA_2, 0xff);
    x86_64_outb(PIT_DATA_2, 0xff);

    // channel 2 enable (see for example a nice overview of the 8254 here https://www.cs.usfca.edu/~cruse/cs630f08/lesson15.ppt)
    x86_64_outb(0x61,(x86_64_inb(0x61) & ~0x02) | 0x01);

    // dummy read, give time for the next edge rise
    x86_64_inb(PIT_DATA_2);
    char msb = x86_64_inb(PIT_DATA_2);
    do {
        x86_64_inb(PIT_DATA_2);
        msb = x86_64_inb(PIT_DATA_2);
    } while(msb);
}

//TODO: this needs to be moved into cpu.h/c and made per-core
static uint64_t _est_cpu_freq(void)
{
    //TODO: percpu
    static uint64_t _est_freq = 0;

    if(_est_freq)
        return _est_freq;
    
    // not sure what the right number should be?
    int tries = 3;
    //NOTE: this is assuming the 18.2 Hz interval is programmed by the one-shot-period timer 
    const uint64_t elapsed_ms = 55;
    uint64_t min_cpu_hz = ~(uint64_t)0;
    uint64_t max_cpu_hz = 0;
    do
    {
        uint64_t rdtsc_start = __rdtsc();
        _wait_one_55ms_interval();
        uint64_t rdtsc_end = __rdtsc();
        const uint64_t cpu_hz = 1000*(rdtsc_end - rdtsc_start)/elapsed_ms;
        if(cpu_hz < min_cpu_hz)
            min_cpu_hz = cpu_hz;
        if(cpu_hz > max_cpu_hz)
            max_cpu_hz = cpu_hz;
    } while(tries--);
    // estimated frequency is average of our measurements, seems fair...
    return _est_freq = (max_cpu_hz + min_cpu_hz)/2;
}

void clock_initialise(void) {
    uint64_t bsp_freq = _est_cpu_freq();
    wchar_t buf[128];
    const size_t bufcount = sizeof(buf)/sizeof(wchar_t);

    swprintf(buf,bufcount,L"clock: bsp freq estimated ~ %llu Hz\n", bsp_freq);
    output_console_output_string(buf);
}