#include <jos.h>
#include <collections.h>
#include <interrupts.h>
#include <x86_64.h>
#include <tasks.h>

#include <stdlib.h>
#include <string.h>

// one meg
#define TASK_STACK_SIZE     1024*1024
#define TASK_QUEUE_SIZE     16

typedef struct _task_switch_stack
{    
    // bottom of stack (rsp)

    uint64_t        rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t        r15, r14, r13, r12, r11, r10, r9, r8;

    uint64_t        handler_id;
    uint64_t        error_code; //< will be pushed as 0 by our stub if not done by the CPU    

    uint64_t        rip;    
    uint64_t        cs;
    uint64_t        rflags;
    //NOTE: CPU always pushes these 64-bit mode (not just for CPL changes) 
    uint64_t        rsp;
    uint64_t        ss;     // <- top of stack (rsp + 184)

} _JOS_PACKED_ task_switch_stack_t;

typedef struct _task_context {

    // a little below the top of the stack for this task (holds registers+iretq stack frame)
    uintptr_t       _rsp;

    // entry point
    task_func_t     _func;
    // task handler argument
    void*           _ptr;

} _JOS_PACKED_ task_context_t;


// ===================================================================================
//TODO: all of this needs to be per-CPU and concurrency safe, ultimately

static queue_t              _ready_tasks[kTaskPri_NumPris];
static queue_t              _waiting_tasks[kTaskPri_NumPris];
static task_context_t*      _running_tasks[kTaskPri_NumPris];
static task_context_t*      _running_task = 0;

extern task_context_t* x86_64_task_yield(task_context_t* curr, task_context_t* new);

//NOTE: interrupts need to be disabled when this runs
static task_context_t*  _select_next_task_to_run(void) {
    
    // pick the next highest priority task available
    for(int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {
        if ( !queue_is_empty(_ready_tasks + pri) ) {
            if(_running_tasks[pri]!=0) {
                // move currently running task to the back of the queue
                queue_push_ptr(_ready_tasks + pri, (void*)(_running_tasks+pri));
            }
            // make the head of the queue the running task
            _running_tasks[pri] = (task_context_t*)queue_front_ptr(_ready_tasks + pri);
            queue_pop(_ready_tasks + pri);
            return _running_tasks[pri];
        }
    }
    return 0;
}

static void _yield_to_next_task(void) {
    task_context_t* prev = _running_task;
    _running_task = _select_next_task_to_run();
    if( _running_task ) {
        x86_64_task_yield(prev, _running_task);
    }
}

//NOTE: when we switch to proper SMP this will be per CPU
static jos_status_t _idle_task(void* ptr) {

    task_context_t* idle_task = (task_context_t*)ptr;
    //ZZZ: not so much "true" as wait for a kernel shutdown signal
    while(true) {
        _yield_to_next_task();
        x86_64_pause_cpu();
    }
    _JOS_UNREACHABLE();
    return _JOS_K_STATUS_SUCCESS;
}

static void _task_wrapper(task_context_t* ctx) {

    // pre-amble
    
    // invoke
    jos_status_t status = ctx->_func(ctx->_ptr);

    // post-amble
    //TODO: remove this task from the task list, or mark as "done" for removal later
}

static task_context_t* _create_task_context(task_func_t func, void* ptr) {

    // allocate memory for the stack and the task context object
    const size_t stack_size = TASK_STACK_SIZE + sizeof(task_context_t);
    task_context_t* ctx = (task_context_t*)malloc(stack_size);
    ctx->_func = func;
    ctx->_ptr = ptr ? ptr : (void*)ctx;    //< we can also pass in "self"...

    // set up returnable stack for the task, rounded down to make it 10h byte aligned
    const size_t stack_top = ((size_t)ctx + TASK_STACK_SIZE) & 0x0f;
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

    return ctx;
}

// ------------------------------------------------------

task_handle_t   task_create(task_create_args_t* args) {
    task_context_t* ctx = _create_task_context(args->func, args->ptr);
    queue_push_ptr(_ready_tasks + args->pri, (void*)ctx);
    return (task_handle_t)ctx;
}

void task_yield(void) {
    //ZZZ: this is not pre-emptive safe yet!
    _yield_to_next_task();    
}

void task_initialise(void) {

    memset(_running_tasks, 0, sizeof(_running_tasks));
    for(int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {

        queue_create(_ready_tasks + pri, TASK_QUEUE_SIZE, sizeof(task_context_t));
        queue_create(_waiting_tasks + pri, TASK_QUEUE_SIZE, sizeof(task_context_t));        
    }

    //ZZZ: testing
    task_handle_t idle = task_create(&(task_create_args_t)
    {   .func = _idle_task, 
        .pri = kTaskPri_Lowest});
    
    // this will start the idle task and never return
    task_yield();
    __builtin_unreachable();
}
