
#include <stdint.h>
#include "kernel_detail.h"
#include "interrupts.h"
#include <kernel/clock.h>
#include <kernel/cpu.h>

#include <stdio.h>


//NOTE: setting this to the same as what the Linux kernel (currently) uses...
#define HZ              200
#define CLOCK_FREQ      1193182
#define ONE_FP32        0x100000000
#define HALF_FP32       0x010000000

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

static bool _clock_ok = false;
uint32_t _k_clock_freq_frac = 0;
uint32_t _k_clock_freq_whole = 0;
volatile uint32_t _k_clock_frac = 0;
volatile uint32_t _k_clock_whole = 0;
volatile uint64_t _k_ms_elapsed;
volatile uint64_t _k_ticks_elapsed = 0;

const char* kClockChannel = "clock";

// kernel.asm
extern void _k_update_clock(void);

typedef struct {
    
    uint32_t    _hz;
    uint16_t    _divisor;
    uint64_t    _ms_per_tick_fp32;

} clock_pit_interval_t;

_JOS_INLINE_FUNC clock_pit_interval_t _make_pit_interval(uint32_t hz)
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

_JOS_INLINE_FUNC void _set_divisor(clock_pit_interval_t* info, uint16_t port)
{
    k_outb(port, info->_divisor & 0xff);
    k_outb(port, (info->_divisor>>8) & 0xff);
}

// See
// https://github.com/torvalds/linux/blob/master/arch/x86/kernel/tsc.c

static void clock_irq_handler(int i)
{
    (void)i;
    ++_k_ticks_elapsed;
    _k_update_clock();        
    //TODO: check if any outstanding timers are pending etc.
}

uint64_t k_clock_ms_since_boot(void)
{
    return _k_ms_elapsed;
}

uint64_t k_ticks_since_boot(void)
{
    return _k_ticks_elapsed;
}

uint64_t k_clock_get_ms_res(void)
{
    return (uint64_t)_k_clock_freq_whole<<32 | (uint64_t)_k_clock_freq_frac;
}

// wait for one 18 Hz period (~55ms)
static void _wait_one_55ms_interval(void)
{
    k_outb(PIT_COMMAND, PIT_COUNTER_2 | PIT_MODE_ONESHOT);
    // set the timer to be the max interval, i.e. 18.2 Hz
    k_outb(PIT_DATA_2, 0xff);
    k_outb(PIT_DATA_2, 0xff);

    // channel 2 enable (see for example a nice overview of the 8254 here https://www.cs.usfca.edu/~cruse/cs630f08/lesson15.ppt)
    k_outb(0x61,(k_inb(0x61) & ~0x02) | 0x01);

    // dummy read, give time for the next edge rise
    k_inb(PIT_DATA_2);
    char msb = k_inb(PIT_DATA_2);
    do {
        k_inb(PIT_DATA_2);
        msb = k_inb(PIT_DATA_2);
    } while(msb);
}

//TODO: this needs to be moved into cpu.h/c and made per-core
uint64_t _k_clock_est_cpu_freq(void)
{
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

uint64_t k_clock_ms_to_cycles(uint64_t ms)
{
    return  (_k_clock_est_cpu_freq() * ms + 500)/1000;
}

void k_clock_init(void)
{
    uint64_t cpu_hz = _k_clock_est_cpu_freq();
    _JOS_KTRACE_CHANNEL(kClockChannel, "CPU clocked at %d MHz\n", cpu_hz);
    if( cpu_hz <= HZ )
    {
        _JOS_KTRACE_CHANNEL(kClockChannel, "*** Clock speed too low for our kernel, no timers will be running!\n");
    }
    else
    {
        clock_pit_interval_t info = _make_pit_interval(HZ);

        // whole and fractional part for our interrupt handler
        _k_clock_freq_whole = (uint32_t)(info._ms_per_tick_fp32 >> 32);
        _k_clock_freq_frac = (uint32_t)(info._ms_per_tick_fp32 & 0x00000000ffffffff);
        _k_ms_elapsed = 0;

        if(!info._divisor)
        {
            _JOS_KTRACE_CHANNEL(kClockChannel, "non-sensical clock frequencies, is this running in an emulator?\n");
            return;
        }

        _JOS_KTRACE_CHANNEL(kClockChannel, "starting PIT for %d HZ with divider %d, resolution is %d.%d\n", HZ, (int)info._divisor, _k_clock_freq_whole, _k_clock_freq_frac);

        // run once to initialise counters
        _k_update_clock();
        
        // initialise PIT
        k_outb(PIT_COMMAND, PIT_COUNTER_0 | PIT_MODE_SQUAREWAVE | PIT_RL_DATA);
        // set frequency         
        _set_divisor(&info, PIT_DATA_0);
        
        // start the clock IRQ
        k_set_irq_handler(0,clock_irq_handler);
        k_enable_irq(0);

        _clock_ok = true;
    }
}

bool k_clock_ok(void)
{
    return _clock_ok;
}