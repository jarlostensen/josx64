// ===================================================================================
// tasks and task switching
//
//  TL;DR
//      this implementation is currently COOPERATIVE, there is no pre-emption of tasks 
//      and tasks are expected to yield and play nice.
//  
//

#include <jos.h>
#include <collections.h>
#include <interrupts.h>
#include <smp.h>
#include <debugger.h>
#include <x86_64.h>
#include <tasks.h>
#include <internal/_tasks.h>
#include <linear_allocator.h>

#include <stdlib.h>
#include <string.h>

#include <output_console.h>

// one meg
#define TASK_STACK_SIZE     1024*1024
#define TASK_QUEUE_SIZE     16

#include <internal/_tasks.h>

static const char* kTaskChannel = "tasks";
static linear_allocator_t*  _tasks_allocator = 0;


static void cpu_context_initialise(cpu_task_context_t* cpu_ctx) {
    memset(cpu_ctx, 0, sizeof(cpu_task_context_t));
    for ( int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {        
        cpu_ctx->_ready_queues[pri]._tail = &cpu_ctx->_ready_queues[pri]._head;
        cpu_ctx->_ready_queues[pri]._head._next = 0;
    }
}

static void cpu_context_push_task(cpu_task_context_t* cpu_ctx, size_t pri, task_context_t* task) {

    if (cpu_ctx->_ready_queues[pri]._head._next == 0
        &&
        cpu_ctx->_ready_queues[pri]._tail != &cpu_ctx->_ready_queues[pri]._head) {
            while (true) {
                x86_64_pause_cpu();
            }
        }

    x86_64_cli();
    cpu_ctx->_ready_queues[pri]._tail->_next = task;
    cpu_ctx->_ready_queues[pri]._tail = task;
    task->_next = 0;
    x86_64_sti();
}

static task_context_t* cpu_context_try_pop_task(cpu_task_context_t* cpu_ctx, size_t pri) {

    task_context_t* task = 0;
    x86_64_cli();

    if( cpu_ctx->_ready_queues[pri]._head._next) {
        task = cpu_ctx->_ready_queues[pri]._head._next;
        cpu_ctx->_ready_queues[pri]._head._next = task->_next;
        if (cpu_ctx->_ready_queues[pri]._head._next == 0) {
            cpu_ctx->_ready_queues[pri]._tail = &cpu_ctx->_ready_queues[pri]._head;
        }
    }
    
    x86_64_sti();

    return task;
}

// in x86_64.asm
extern task_context_t* x86_64_task_switch(uintptr_t* curr_stack, uintptr_t* new_stack);
// handle to per-cpu context instances
static per_cpu_ptr_t _per_cpu_ctx;

_JOS_API_FUNC _tasks_debugger_task_iterator_t _tasks_debugger_task_iterator_begin(void) {
    return ((cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx))->_running_task;
}

_JOS_API_FUNC uint32_t _tasks_debugger_num_tasks(void) {
    //TODO: per priority, or something. For now just return 1 == running task on this CPU
    return 1;
}

// selects the next task to run *on this CPU*
static task_context_t*  _select_next_task_to_run(void) {

    // =====================================================
    //NOTE: 
    //      Interrupts are NOT disabled here because we're still 
    //      running co-operatively, and no CPUs can interfere with each other's task queues.
    //      IF that changes this has to be made re-entrant safe

    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    // pick the next highest priority task available
    for(int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {
        
        // anything on this queue? 
        task_context_t* task = cpu_context_try_pop_task(cpu_ctx, pri);
        if ( task ) {
            // _JOS_KTRACE_CHANNEL(kTaskChannel, "next task \"%s\" ready at pri level %d", task->_name, pri);

            // check if we need to swap out the currently running task
            if(cpu_ctx->_running_task!=0
                &&
                // ignore if it's the IDLE task
                cpu_ctx->_running_task->_pri!=kTaskPri_NumPris
                ) {
                
                //_JOS_KTRACE_CHANNEL(kTaskChannel, "switching out \"%s\"", cpu_ctx->_running_task->_name);

                // move currently running back to the end of the queue
                cpu_context_push_task(cpu_ctx, pri, cpu_ctx->_running_task);
            }
            
            // enable new running task
            cpu_ctx->_running_task = task;
            return cpu_ctx->_running_task;
        }
    }
    
    // if we get here the only task left to run is either idle (running_task=0) or just continue 
    return cpu_ctx->_running_task;
}

static void _task_wrapper(task_context_t* ctx);

static void _yield_to_next_task(void) {
    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    task_context_t* prev = cpu_ctx->_running_task;
    task_context_t* next_task = _select_next_task_to_run();
    if (next_task && next_task == prev) {
        // just keep the currently running task doing it's thing
        return;
    }

    if( next_task ) {
        //_JOS_KTRACE_CHANNEL(kTaskChannel, "switching from \"%s\" to \"%s\"", 
        //    prev ? prev->_name : "(0)", cpu_ctx->_running_task->_name);        
        //TESTING:
        if (prev && prev->_xsave_area) {
            __asm__ volatile ("xsave64  %0" : : "m" (prev->_xsave_area));
        }
        x86_64_task_switch(prev ? prev->_stack : 0, cpu_ctx->_running_task->_stack);
    } else {
        // else we're now idling
        if (prev != cpu_ctx->_cpu_idle) {
            cpu_ctx->_running_task = cpu_ctx->_cpu_idle;
            // _JOS_KTRACE_CHANNEL(kTaskChannel, "back to \"%s\" on cpu %d", cpu_ctx->_running_task->_name, per_cpu_this_cpu_id());
            x86_64_task_switch(prev->_stack, cpu_ctx->_running_task->_stack);
        }
    }
}

// the idle task is the task we fall back to when there is NO OTHER WORK TO DO on this CPU.
// it just keeps the wheels turning and waits for work to do
// NOTE: 
//  since we are not preemtive (yet), once we fall through to the idle task nothing else will 
//  happen because no other code can run. For that reason this task is only run at startup and 
//  as the last thing when all other tasks are done.
//
static jo_status_t _idle_task(void* ptr) {        
    //ZZZ: not so much "true" as wait for a kernel shutdown signal   
    while(true) {
        _yield_to_next_task();
        x86_64_pause_cpu();
    }
    _JOS_UNREACHABLE();
    return _JO_STATUS_SUCCESS;
}

static void _task_wrapper(task_context_t* ctx) {

    jo_status_t status _JOS_MAYBE_UNUSED = ctx->_func(ctx->_ptr);

    // post-amble: remove this task and switch to a new one
    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    _JOS_ASSERT(cpu_ctx->_running_task == ctx);
    // pick a fresh task, or idle
    // TODO: remove the task, i.e. destroy it properly (this is a LEAK)
    cpu_ctx->_running_task = 0;
    _yield_to_next_task();
}

#define TASK_STACK_CONTEXT_SIZE TASK_STACK_SIZE + sizeof(task_context_t)
static task_context_t* _create_task_context(task_func_t func, void* ptr, const char* name) {

    // allocate memory for the stack and the task context object
    /*
            ---------------------------
            | stack_top               |
            .                         .
            | rsp (interrupt_stack)   |
            ---------------------------
            .                         .
            .                         .
            ...........................
            | ctx + 1                 |
            ___________________________
            | ctx                     |
            ---------------------------
    */
    task_context_t* ctx = (task_context_t*)_tasks_allocator->_super.alloc((jos_allocator_t*)_tasks_allocator, TASK_STACK_CONTEXT_SIZE);
    _JOS_ASSERT(ctx);

    // set aside space for XSAVE if we use it
    processor_information_t* this_cpu_info = per_cpu_this_cpu_info();
    if (this_cpu_info->_xsave && this_cpu_info->_xsave_area_size) {
        ctx->_xsave_area = _tasks_allocator->_super.alloc((jos_allocator_t*)_tasks_allocator, this_cpu_info->_xsave_area_size);
        _JOS_ASSERT(ctx->_xsave_area);
    } else {
        ctx->_xsave_area = 0;
    }

    ctx->_func = func;
    ctx->_ptr = ptr ? ptr : (void*)ctx;    //< we can also pass in "self"...
    ctx->_name = name;
    
    // set up returnable stack for the task, rounded down to make it 10h byte aligned
    const uintptr_t stack_top = ((uintptr_t)(ctx+1) + TASK_STACK_SIZE) & ~0x0f;
    interrupt_stack_t* interrupt_frame = (interrupt_stack_t*)(stack_top - sizeof(interrupt_stack_t));

    memset(interrupt_frame, 0, sizeof(interrupt_stack_t));
    interrupt_frame->cs = x86_64_get_cs();
    interrupt_frame->ss = x86_64_get_ss();

    // The top of stack is 16 byte aligned but the call to _task_wrapper will 
    // push an 8 byte return value on to the stack, making it non-aligned again.
    // This adjustment makes sure that we're 16-byte aligned again when the task function is invoked
    interrupt_frame->rsp = stack_top - 8;
    interrupt_frame->rbp = interrupt_frame->rsp;
    
    // always enable IF for tasks!
    interrupt_frame->rflags = x86_64_get_rflags() | (1ull<<9);

    // argument to _task_wrapper (fastcall)
    interrupt_frame->rcx = (uintptr_t)ctx;
    interrupt_frame->rip = (uintptr_t)_task_wrapper;
    
    // switching stack area
    ctx->_stack[0] = (uint64_t)interrupt_frame;
    ctx->_stack[1] = interrupt_frame->ss;
    
    // this is as far up as a stack unwind can go, beyond this there is no sensible information
    ctx->_stack_top = interrupt_frame->rsp;

    return ctx;
}

// ------------------------------------------------------

_JOS_API_FUNC task_context_t* tasks_this_task(void) {
    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);    
    return cpu_ctx->_running_task;
}

task_handle_t   tasks_create(task_create_args_t* args) {

    _JOS_KTRACE_CHANNEL(kTaskChannel, "created task \"%s\" 0x%llx (0x%llx), pri %d", 
        args->name, args->func, args->ptr, args->pri);

    task_context_t* ctx = _create_task_context(args->func, args->ptr, args->name);
    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);    

    //ZZZ: this should probably be done in a separate "start" function?    
    cpu_context_push_task(cpu_ctx, args->pri, ctx);
    
    return (task_handle_t)ctx;
}

void tasks_yield(void) {
    //ZZZ: this is not pre-emptive safe yet!
    _yield_to_next_task();
    x86_64_pause_cpu();
}

void tasks_initialise(jos_allocator_t * allocator) {

    //NOTE: called on the BSP *only*
    _JOS_ASSERT(smp_get_bsp_id() == per_cpu_this_cpu_id());
    _per_cpu_ctx = per_cpu_create_ptr();

    // fixed pool of memory for the per-cpu IDLE tasks, this is all we allocate up front    
    const size_t idle_task_pool_size = sizeof(linear_allocator_t) + smp_get_processor_count() * (sizeof(cpu_task_context_t) + TASK_STACK_CONTEXT_SIZE);
    void* allocator_arena = allocator->alloc(allocator, idle_task_pool_size);
    _JOS_ASSERT(allocator_arena);
    _tasks_allocator = linear_allocator_create(allocator_arena, idle_task_pool_size);
    
    for(size_t cpu = 0; cpu < smp_get_processor_count(); ++cpu ) {
        
        cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)linear_allocator_alloc(_tasks_allocator, sizeof(cpu_task_context_t));
        cpu_context_initialise(cpu_ctx);
        _JOS_KTRACE_CHANNEL(kTaskChannel, "initialising for ap %d (ctx 0x%llx)", cpu, cpu_ctx);
        _JOS_PER_CPU_PTR(_per_cpu_ctx, cpu) = (uintptr_t)cpu_ctx;        
    }
}

//NOTE: called on each AP (+BSP)
void tasks_start_idle(void) {
    
    cpu_task_context_t* ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    ctx->_running_task = 0;
    ctx->_cpu_idle = _create_task_context(_idle_task, 0, "cpu_idle");
    //NOTE: idle priority is special, and lower than anything else
    ctx->_cpu_idle->_pri = kTaskPri_NumPris;
    // start the first task
    _yield_to_next_task();
    _JOS_UNREACHABLE();
}

