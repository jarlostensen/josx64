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

typedef struct _task_context {

    // a little below the top of the stack for this task (holds registers+iretq stack frame)
    uintptr_t       _rsp;

    // entry point
    task_func_t     _func;
    // task handler argument
    void*           _ptr;

} _JOS_PACKED_ task_context_t;

static const char* kTaskChannel = "tasks";

// ===================================================================================
//TODO: all of this needs to be per-CPU and concurrency safe, ultimately

static queue_t              _ready_tasks[kTaskPri_NumPris];
static queue_t              _waiting_tasks[kTaskPri_NumPris];
static task_context_t*      _running_tasks[kTaskPri_NumPris];
static task_context_t*      _running_task = 0;

// in x86_64.asm
extern task_context_t* x86_64_task_switch(interrupt_stack_t* curr_stack, interrupt_stack_t* new_stack);

//NOTE: interrupts need to be disabled when this runs
static task_context_t*  _select_next_task_to_run(void) {
    
    // pick the next highest priority task available
    for(int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {
        if ( !queue_is_empty(&_ready_tasks[pri]) ) {
            if(_running_tasks[pri]!=0) {
                // move currently running task to the back of the queue
                queue_push_ptr(&_ready_tasks[pri], (void*)(&_running_tasks[pri]));
            }
            // make the head of the queue the running task
            _running_tasks[pri] = (task_context_t*)queue_front_ptr(_ready_tasks + pri);
            queue_pop(&_ready_tasks[pri]);
            return _running_tasks[pri];
        }
    }
    _JOS_ASSERT(false);
    return 0;
}

static void _yield_to_next_task(void) {
    task_context_t* prev = _running_task;
    _running_task = _select_next_task_to_run();
    if( _running_task ) {
        _JOS_ASSERT(_running_task->_rsp);
        x86_64_task_switch(prev ? (interrupt_stack_t*)prev->_rsp : 0, (interrupt_stack_t*)_running_task->_rsp);
    }
}

//NOTE: when we switch to proper SMP this will be per CPU
static jo_status_t _idle_task(void* ptr) {

    task_context_t* idle_task = (task_context_t*)ptr;
    //ZZZ: not so much "true" as wait for a kernel shutdown signal
    while(true) {
        _yield_to_next_task();
        x86_64_pause_cpu();
    }
    _JOS_UNREACHABLE();
    return _JO_STATUS_SUCCESS;
}

static void _task_wrapper(task_context_t* ctx) {

    _JOS_KTRACE_CHANNEL(kTaskChannel, "task_wrapper(0x%llx [0x%llx])", ctx->_func, ctx->_ptr);
    // pre-amble
    
    // invoke
    jo_status_t status = ctx->_func(ctx->_ptr);

    // post-amble
    //TODO: remove this task from the task list, or mark as "done" for removal later
}

static task_context_t* _create_task_context(task_func_t func, void* ptr) {

    // allocate memory for the stack and the task context object
    const size_t stack_size = TASK_STACK_SIZE + sizeof(task_context_t);
    task_context_t* ctx = (task_context_t*)malloc(stack_size);
    _JOS_ASSERT(ctx);
    ctx->_func = func;
    ctx->_ptr = ptr ? ptr : (void*)ctx;    //< we can also pass in "self"...

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

task_handle_t   task_create(task_create_args_t* args) {
    task_context_t* ctx = _create_task_context(args->func, args->ptr);
    queue_push_ptr(&_ready_tasks[args->pri], (void*)ctx);

    _JOS_KTRACE_CHANNEL(kTaskChannel, "created task 0x%llx (0x%llx), pri %d", args->func, args->ptr, args->pri);

    return (task_handle_t)ctx;
}

void task_yield(void) {
    //ZZZ: this is not pre-emptive safe yet!
    _yield_to_next_task();
}

void task_initialise(void) {

    memset(_running_tasks, 0, sizeof(_running_tasks));
    for(int pri = (int)kTaskPri_Highest; pri < (int)kTaskPri_NumPris; ++pri) {

        queue_create(&_ready_tasks[pri], TASK_QUEUE_SIZE, sizeof(task_context_t*));
        queue_create(&_waiting_tasks[pri], TASK_QUEUE_SIZE, sizeof(task_context_t*));
    }

    _JOS_KTRACE_CHANNEL(kTaskChannel,"initialised");
}

void task_start_idle(void) {
    _JOS_ASSERT(_running_task==0);
    // add idle task to this CPU
    task_create(&(task_create_args_t){
        .func = _idle_task,
        .pri = kTaskPri_Lowest
    });
    // and start
    _yield_to_next_task();
    _JOS_UNREACHABLE();
}

