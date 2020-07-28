#include <stdint.h>
#include "kernel_detail.h"
#include "memory.h"
#include "interrupts.h"
#include <kernel/tasks.h>
#include <kernel/atomic.h>
#include <collections.h>
#include "cpu_core.h"
#include <stdio.h>

// a task is described by a context object which contains information about the entry point, stack,
// priority, etc.
struct _task_context 
{
    unsigned int    _id;
    // priority
    unsigned int    _pri;
    // dedicated task stack top
    void*           _stack_top;
    // current esp
    void*           _esp;
    // current ebp
    void*           _ebp;
    // size of stack
    size_t          _stack_size;
    // the entry point for the task (NOTE: this is invoked by a task proxy to control shutdown behaviour etc.)
    task_func_t     _task_func;
    // optional argument for the task function
    void*           _obj;
} _JOS_PACKED;

static const size_t kTaskDefaultStackSize = 4096;
static const size_t kTaskStackAlignment = 16;
// in tasks.asm
extern void _k_task_switch_point(void);
extern void _k_task_yield(task_context_t* curr_ctx, task_context_t* new_ctx);

// unique, always increasing
static size_t _task_id = 0;

// helper to build task context stack
static void _push_32(task_context_t* ctx, uint32_t val)
{
    ctx->_esp = (void*)((uintptr_t)ctx->_esp - 4);
    ((uint32_t*)ctx->_esp)[0] = val;    
}

void _task_handler(task_context_t* ctx)
{
    //TODO: task start preamble
    //printf("_task_handler: ctx = 0x%x, ctx->_obj = 0x%x\n", ctx, ctx->_obj);

    // execute task body
    ctx->_task_func(ctx->_obj);

    //TODO: remove this task from task list
    // wait for this task never to be called again    
    while(true)
    {
        k_pause();
    }
}

static void _schedule_next_task(cpu_core_context_t* core_ctx)
{
	task_context_t* curr_ctx = _k_cpu_core_running_task(core_ctx);
	if( _k_cpu_core_context_schedule(core_ctx) )
	{
		// switch to new task
        //TODO: _k_cpu_core_track_switches(core_ctx);
		//TODO: ++core_ctx->_task_switches;

		_k_disable_interrupts();
		isr_stack_t* stack =  (isr_stack_t*)_k_cpu_core_running_task(core_ctx)->_esp;
        
		// ALLWAYS make sure IF is set when we iret into the new task
		stack->eflags |= (1<<9);
		_k_task_yield(curr_ctx, _k_cpu_core_running_task(core_ctx));
	}
}

void _idle_task(void* obj)
{	
	cpu_core_context_t* core_ctx = (cpu_core_context_t*)obj;
	while(true)
	{
		_schedule_next_task(core_ctx);
		k_pause();
	}
}

void k_task_yield(void)
{
	//TODO: get the actual core context
	_schedule_next_task(_k_cpu_core_this());
}

int k_task_priority(const task_context_t* task)
{
    return task->_pri;
}

__attribute__((__noreturn__)) void k_tasks_init(task_func_t root, void* obj)
{
	task_context_t* idle_ctx = k_task_create(&(task_create_info_t){ ._pri = kPri_Idle, ._func = _idle_task });
    // load up the idle task for each CPU
    void core_create_idle_task(cpu_core_context_t* ctx)    
    {        
        _k_cpu_core_add_task(ctx, idle_ctx);
    }
    _k_cpu_core_for_each(core_create_idle_task);

	//printf("idle_ctx = 0x%x, idle_ctx->_obj = 0x%x\n", idle_ctx, idle_ctx->_obj);
	task_context_t* root_ctx = k_task_create(&(task_create_info_t){ ._pri = kPri_Highest, ._func = root, ._obj = obj });
    _k_cpu_core_add_task(_k_cpu_core_this(), root_ctx);
    
	// now switch to the idle task to start things
    asm volatile("mov %0, %%esp\n\t"
                 "mov %1, %%ebp\n\t"
                 // see https://stackoverflow.com/a/3475763/2030688 for the syntax used here
                 "jmp %P2"
                : : "r" (idle_ctx->_esp), "r" (idle_ctx->_ebp), "i" (_k_task_switch_point));
    __builtin_unreachable();
}

task_context_t* k_task_create(task_create_info_t* info)
{   
    const size_t aligned_stack_size = (sizeof(isr_stack_t) + kTaskDefaultStackSize + kTaskStackAlignment-1); 
    task_context_t* ctx = (task_context_t*)k_mem_alloc(sizeof(task_context_t)+aligned_stack_size);

    ctx->_stack_size = kTaskDefaultStackSize;
    ctx->_obj = info->_obj;
    ctx->_task_func = info->_func;
    ctx->_pri = info->_pri;
    ctx->_id = _task_id++;
    // aligned stack top    
    ctx->_esp = ctx->_stack_top = (void*)( ((uintptr_t)(ctx+1) + aligned_stack_size) & ~(kTaskStackAlignment-1));

    //DEBUG: printf("creating task context at 0x%x, stack size is %d bytes, top @ 0x%x\n", ctx, aligned_stack_size, ctx->_esp);

    // build the "stack" contents;
    // top: | ctx ptr                            |
    //      | dummy (return address)             |
    //      | isr_stack_struct fields, up until: |  
    //      | eip/cs/flags                       |
    //    
	_push_32(ctx, (uint32_t)ctx);
    _push_32(ctx, (uint32_t)0x0badc0de);
    
    ctx->_esp = (void*)((uintptr_t)ctx->_esp - sizeof(isr_stack_t));
    isr_stack_t* stack =  (isr_stack_t*)(ctx->_esp);
    stack->eip = (uintptr_t)_task_handler;

    stack->eflags = k_eflags();
    // always enable IF
    stack->eflags |= (1<<9);
    
    stack->cs = _JOS_KERNEL_CS_SELECTOR;
    stack->ds = _JOS_KERNEL_DS_SELECTOR;
    stack->error_code = 0;
    stack->handler_id = 0;
    stack->edi = 0;
    stack->esi = 0;
    stack->ebx = 0;
    stack->edx = 0;
    stack->ecx = 0;
    stack->eax = 0;
    ctx->_ebp = ctx->_esp; 
    stack->ebp = stack->esp = (uintptr_t)ctx->_esp;

    return ctx;
}