#ifndef _JOS_CLOCK_H
#define _JOS_CLOCK_H

void k_clock_init(void);
bool k_clock_ok(void);
uint64_t k_clock_ms_since_boot(void);
uint64_t k_ticks_since_boot(void);

// 32.32 fp resolution, i.e. millisecond accuracy
uint64_t k_clock_get_ms_res(void);
// estimates the number of cycles for a given number of milliseconds
uint64_t k_clock_ms_to_cycles(uint64_t ms);

#endif // _JOS_CLOCK_H