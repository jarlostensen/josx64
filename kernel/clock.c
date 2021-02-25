
#include <jos.h>
#include <kernel.h>
#include <x86_64.h>
#include <interrupts.h>
#include <i8253.h>

#include <stdio.h>
#include <output_console.h>


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
// will be ~1us, for less-than-accurate timekeeping
static uint64_t _micro_epsilon = 0;

const char* kClockChannel = "clock";

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
        i8253_wait_55ms();
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
    // read register C to ack the interrupt   
    x86_64_outb(0x70, 0x0c);
    x86_64_inb(0x71);
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

    _JOS_KTRACE(kClockChannel, "initialising");

    //NOTE from i8253.c (Linux arch\x86):
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

    uint64_t bsp_freq = _est_cpu_freq();
    wchar_t buf[128];
    
    _pit_interval = _make_pit_interval(HZ);
    
    swprintf(buf,128,L"PIT initialised to %dHz\n", HZ);
    output_console_output_string(buf);

    i8253_start_clock(_pit_interval._divisor);
    interrupts_set_irq_handler(0, _irq_0_handler);
    
    interrupts_set_irq_handler(0x8, _irq_8_handler);
    _enable_rtc_timer();

    output_console_output_string(L"waiting for about 10 MS...\n");

    uint64_t ms_start = clock_ms_since_boot();
    uint64_t ms_now = clock_ms_since_boot();
    uint64_t tsc_start = __rdtsc();
    while(ms_now-ms_start<=10) {
        ms_now = clock_ms_since_boot();
        x86_64_io_wait();
    }
    uint64_t delta = __rdtsc() - tsc_start;
    uint64_t cpu_hz = 100*delta;
    _micro_epsilon = delta/10000;

    swprintf(buf,128,L"clock: bsp freq estimated ~ %llu MHz, 1ue = %llu, %llu 1KHz ticks measured\n", bsp_freq/1000000, _micro_epsilon, _1khz_counter);
    output_console_output_string(buf);

    _JOS_KTRACE(kClockChannel, "initialised");
}