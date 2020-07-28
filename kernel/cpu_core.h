#ifndef _JOS_CPU_CORE_H
#define _JOS_CPU_CORE_H

#include <collections.h>
#include <kernel/tasks.h>

// we associate one of these with each core and use it for scheduling etc.
typedef struct _cpu_core_context cpu_core_context_t;

// schedule the next task on this CPU according to some implementation
// TODO: make this data driven and dynamic, ideally, so that we can play with schedulers in real time
bool _k_cpu_core_context_schedule(cpu_core_context_t* ctx);
void _k_cpu_core_for_each(void (*func)(cpu_core_context_t* ctx));
cpu_core_context_t* _k_cpu_core_this(void);
void _k_cpu_core_add_task(cpu_core_context_t* ctx, task_context_t* task);
void k_cpu_core_init(void);
task_context_t* _k_cpu_core_running_task(cpu_core_context_t* ctx);

#endif // _JOS_CPU_CORE_H