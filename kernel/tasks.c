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
#include <fixed_allocator.h>

#include <stdlib.h>
#include <string.h>

#include <output_console.h>

// one meg
#define TASK_STACK_SIZE     1024*1024
#define TASK_QUEUE_SIZE     16

typedef struct _task_context {

    // current priority level
    task_priority_level_t _pri;

    // a little below the top of the stack for this task (holds registers+iretq stack frame)
    uintptr_t       _rsp;
    // entry point
    task_func_t     _func;
    // task handler argument
    void*           _ptr;
    // 'tis helpful 
    const char*     _name;

    //WIP: task queue link
    struct _task_context*   _next;

} _JOS_PACKED_ task_context_t;

static const char* kTaskChannel = "tasks";

// ===================================================================================

// each CPU has one of these, accessed via gs:0
typedef struct _cpu_task_context {

    struct _ready_queue {
        task_context_t      _head;
        task_context_t*     _tail;    
    } _ready_queues[kTaskPri_NumPris];
    
    // each CPU can only have one running task at one time, and this is the one
    task_context_t*      _running_task;

    // the idle task for this cpu which we fall back to when there is nothing else to do
    task_context_t*      _cpu_idle;

} cpu_task_context_t;

static void cpu_context_initialise(cpu_task_context_t* cpu_ctx) {
    memset(cpu_ctx, 0, sizeof(cpu_task_context_t));
    for ( int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {        
        cpu_ctx->_ready_queues[pri]._tail = &cpu_ctx->_ready_queues[pri]._head;
    }
}

static void cpu_context_push_task(cpu_task_context_t* cpu_ctx, size_t pri, task_context_t* task) {

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
    }
    
    x86_64_sti();

    return task;
}

// in x86_64.asm
extern task_context_t* x86_64_task_switch(interrupt_stack_t* curr_stack, interrupt_stack_t* new_stack);
// handle to per-cpu context instances
static per_cpu_ptr_t _per_cpu_ctx;

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
            _JOS_KTRACE_CHANNEL(kTaskChannel, "next task \"%s\" ready at pri level %d", task->_name, pri);
            // check if we need to swap out the currently running task
            if(cpu_ctx->_running_task!=0
                &&
                // ignore if it's the IDLE task
                cpu_ctx->_running_task->_pri!=kTaskPri_NumPris
                ) {
                
                _JOS_KTRACE_CHANNEL(kTaskChannel, "switching out \"%s\"", cpu_ctx->_running_task->_name);
                // move currently running back to the end of the queue
                cpu_context_push_task(cpu_ctx, pri, cpu_ctx->_running_task);
            }
            
            // enable new running task
            cpu_ctx->_running_task = task;
            return cpu_ctx->_running_task;
        }
    }

    // if we get here the only task left to run is idle
    return 0;
}

static void _yield_to_next_task(void) {
    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    task_context_t* prev = cpu_ctx->_running_task;
    task_context_t* next_task = _select_next_task_to_run();
    if( next_task ) {
        _JOS_KTRACE_CHANNEL(kTaskChannel, "switching from \"%s\" to \"%s\"", 
            prev ? prev->_name : "(0)", cpu_ctx->_running_task->_name);
        x86_64_task_switch(prev ? (interrupt_stack_t*)prev->_rsp : 0, (interrupt_stack_t*)cpu_ctx->_running_task->_rsp);
    }
    // else we're now idling
    _JOS_KTRACE_CHANNEL(kTaskChannel, "back to idling on cpu %d", per_cpu_this_cpu_id());
    cpu_ctx->_running_task = cpu_ctx->_cpu_idle;
    x86_64_task_switch(0, (interrupt_stack_t*)cpu_ctx->_cpu_idle->_rsp);
}

// the idle task is the task we fall back to when there is NO OTHER WORK TO DO on this CPU.
// it just keeps the wheels turning and waits for work to do
// NOTE: 
//  since we are not preemtive (yet), once we fall through to the idle task nothing else will 
//  happen because no other code can run. For that reason this task is only run at startup and 
//  as the last thing when all other tasks are done.
//
static jo_status_t _idle_task(void* ptr) {    
    task_context_t* idle_task = (task_context_t*)ptr;
    _JOS_KTRACE_CHANNEL(kTaskChannel, "idle starting");
    //ZZZ: not so much "true" as wait for a kernel shutdown signal
    while(true) {
        _yield_to_next_task();
        x86_64_pause_cpu();
    }
    _JOS_UNREACHABLE();
    return _JO_STATUS_SUCCESS;
}

static void _task_wrapper(task_context_t* ctx) {

    _JOS_KTRACE_CHANNEL(kTaskChannel, "task_wrapper starting \"%s\"(0x%llx [0x%llx])", 
            ctx->_name, ctx->_func, ctx->_ptr);
    // pre-amble
    
    // invoke
    jo_status_t status = ctx->_func(ctx->_ptr);
    if ( _JO_SUCCEEDED(status) ) {
        _JOS_KTRACE_CHANNEL(kTaskChannel, "task \"%s\" terminated normally", ctx->_name);
    }
    else {
        _JOS_KTRACE_CHANNEL(kTaskChannel, "task \"%s\" terminated with error 0x%x", ctx->_name, status);
    }

    // post-amble: remove this task and switch to a new one
    cpu_task_context_t* cpu_ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    _JOS_ASSERT(cpu_ctx->_running_task == ctx);
    // pick a fresh task, or idle
    cpu_ctx->_running_task = 0;
    _yield_to_next_task();
}

static task_context_t* _create_task_context(task_func_t func, void* ptr, const char* name) {

    // allocate memory for the stack and the task context object
    const size_t stack_size = TASK_STACK_SIZE + sizeof(task_context_t);
    task_context_t* ctx = (task_context_t*)malloc(stack_size);
    _JOS_ASSERT(ctx);
    ctx->_func = func;
    ctx->_ptr = ptr ? ptr : (void*)ctx;    //< we can also pass in "self"...
    ctx->_name = name;
    
    // set up returnable stack for the task, rounded down to make it 10h byte aligned
    const size_t stack_top = ((size_t)ctx + TASK_STACK_SIZE) & ~0x0f;
    interrupt_stack_t* interrupt_frame = (interrupt_stack_t*)(stack_top - sizeof(interrupt_stack_t));

    memset(interrupt_frame, 0, sizeof(interrupt_stack_t));
    interrupt_frame->cs = x86_64_get_cs();
    interrupt_frame->ss = x86_64_get_ss();

    interrupt_frame->rsp = (uint64_t)interrupt_frame;
    interrupt_frame->rbp = interrupt_frame->rsp;
    // always enable IF for tasks!
    interrupt_frame->rflags = x86_64_get_rflags() | (1ull<<9);

    // argument to _task_wrapper (fastcall)
    interrupt_frame->rcx = (uintptr_t)ctx;
    interrupt_frame->rip = (uintptr_t)_task_wrapper;
    
    ctx->_rsp = interrupt_frame->rsp;

    return ctx;
}

// ------------------------------------------------------

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
}

void tasks_initialise(void) {

    //NOTE: called on the BSP *only*
    _JOS_ASSERT(smp_get_bsp_id() == per_cpu_this_cpu_id());
    _per_cpu_ctx = per_cpu_create_ptr();

    for(size_t cpu = 0; cpu < smp_get_processor_count(); ++cpu ) {

        _JOS_KTRACE_CHANNEL(kTaskChannel, "initialising for ap %d", cpu);

        cpu_task_context_t* ctx = (cpu_task_context_t*)malloc(sizeof(cpu_task_context_t));        
        cpu_context_initialise(ctx);
        
        _JOS_PER_CPU_PTR(_per_cpu_ctx,cpu) = (uintptr_t)ctx;
    }
}

//NOTE: called on each AP (+BSP)
void tasks_start_idle(void) {
    
    cpu_task_context_t* ctx = (cpu_task_context_t*)_JOS_PER_CPU_THIS_PTR(_per_cpu_ctx);
    // add idle task to this CPU
    ctx->_running_task = ctx->_cpu_idle = _create_task_context(_idle_task, 0, "cpu_idle");
    //NOTE: idle priority is special, and lower than anything else
    ctx->_cpu_idle->_pri = kTaskPri_NumPris;
    // we call this directly to kick things off and the first thing it will do 
    // is to find another (higher priorrity) task to switch to
    _idle_task(0);
    _JOS_UNREACHABLE();
}

