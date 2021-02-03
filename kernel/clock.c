
#include <jos.h>
#include <kernel.h>
#include <x86_64.h>
#include <interrupts.h>

#include <stdio.h>
#include <output_console.h>

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

//NOTE: setting this to the same as what the Linux kernel (currently) uses...
#define HZ              200
#define CLOCK_FREQ      1193182
#define ONE_FP32        0x100000000
#define HALF_FP32       0x010000000

typedef struct {
    
    uint32_t    _hz;
    uint16_t    _divisor;
    uint64_t    _ms_per_tick_fp32;

} clock_pit_interval_t;

static uint64_t _clock_ms_elapsed = 0;
static uint64_t _clock_ticks_elapsed = 0;
static clock_pit_interval_t _pit_interval;

const char* kClockChannel = "clock";

// wait for one 18 Hz period (~55ms)
static void _wait_one_55ms_interval(void)
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

static void _enable_rtc_timer(void) {
    x86_64_cli();
    x86_64_outb(0x70, 0x8b);
    x86_64_io_wait();
    uint8_t prev = x86_64_inb(0x71);
    x86_64_outb(0x70, 0x8b);
    x86_64_outb(0x71, prev | 0x40);
    x86_64_sti();
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

static uint64_t _1khz_counter = 0;
static void _irq_8_handler(int irq_id) {
    (void)irq_id;
    ++_1khz_counter;    
}

static  clock_pit_interval_t _make_pit_interval(uint32_t hz)
{
    clock_pit_interval_t    info;
    info._hz = hz;
    
    double ddiv = (double)CLOCK_FREQ/(double)hz;
    // round up 
    ddiv = (ddiv+0.5);
    // divisor for PIT
    info._divisor = (uint16_t)(ddiv);    
    // scale to 32.32 fixed point
    ddiv *= (double)ONE_FP32;
    info._ms_per_tick_fp32 = (uint64_t)(((ddiv * 1000.0)/(double)CLOCK_FREQ));

    return info;
}

static void _set_divisor(clock_pit_interval_t* info, uint16_t port)
{
    x86_64_outb(port, info->_divisor & 0xff);
    x86_64_outb(port, (info->_divisor>>8) & 0xff);
}

uint64_t clock_ms_since_boot(void) {
    return _clock_ms_elapsed>>32;
}

static void _irq_0_handler(int i)
{
    (void)i;
    ++_clock_ticks_elapsed;    
    _clock_ms_elapsed += _pit_interval._ms_per_tick_fp32;
}

void clock_initialise(void) {

    //ZZZ: this hangs and the note in i8253.c (Linux arch\x86) suggests it may be disabled:
    /*
        * Modern chipsets can disable the PIT clock which makes it unusable. It
        * would be possible to enable the clock but the registers are chipset
        * specific and not discoverable. Avoid the whack a mole game.
        *
        * These platforms have discoverable TSC/CPU frequencies but this also
        * requires to know the local APIC timer frequency as it normally is
        * calibrated against the PIT interrupt.
    */
   // HOWEVER: Qemu reports APIC disabled, so why should the PIT also be disabled..?

    uint64_t bsp_freq = 1; //_est_cpu_freq();
    wchar_t buf[128];
    
    _pit_interval = _make_pit_interval(HZ);
    
    swprintf(buf,128,L"PIT initialised to %dHz\n", HZ);
    output_console_output_string(buf);

    x86_64_outb(PIT_COMMAND, PIT_COUNTER_0 | PIT_MODE_SQUAREWAVE | PIT_RL_DATA);
    // set frequency         
    _set_divisor(&_pit_interval, PIT_DATA_0);

    interrupts_set_irq_handler(0, _irq_0_handler);
    interrupts_PIC_enable_irq(0);

    interrupts_set_irq_handler(0x8, _irq_8_handler);
    interrupts_PIC_enable_irq(0x8);
    _enable_rtc_timer();

    output_console_output_string(L"waiting for about 10 MS...\n");

    uint64_t ms_start = clock_ms_since_boot();
    uint64_t ms_now = clock_ms_since_boot();
    uint64_t tsc_start = __rdtsc();
    while(ms_now-ms_start<10) {
        ms_now = clock_ms_since_boot();
    }
    uint64_t cpu_hz = 100*(__rdtsc() - tsc_start);

    swprintf(buf,128,L"clock: bsp freq estimated ~ %llu MHz\n", cpu_hz/1000000);
    output_console_output_string(buf);
}