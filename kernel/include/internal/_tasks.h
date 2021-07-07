#pragma once

#define MAX_TASK_NAME_LENGTH    32

// information about a single task
typedef struct _task_context {

    // current priority level
    task_priority_level_t _pri;

    // rsp, ss for task switches
    uintptr_t       _stack[2];
    // top-of-stack for this task, we can use this to terminate stack unwinds
    uintptr_t       _stack_top;

    // task entry point
    task_func_t     _func;
    // task handler argument
    void*           _ptr;
    // 'tis helpful 
    const char*     _name;
    
    // next task in priority queue
    struct _task_context*   _next;
    
    // points to the area used to save/restore FP state
    void*                   _xsave_area;

} _JOS_PACKED_ task_context_t;

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

typedef task_context_t* _tasks_debugger_task_iterator_t;
_JOS_API_FUNC _tasks_debugger_task_iterator_t _tasks_debugger_task_iterator_begin(void);
#define _tasks_debugger_task_iterator_end() ((_tasks_debugger_task_iterator_t)0)
#define _tasks_debugger_task_iterator_next(i) (i) = (i)->_next
#define _tasks_debugger_task_iterator(i) (i)
_JOS_API_FUNC uint32_t _tasks_debugger_num_tasks(void);
_JOS_API_FUNC task_context_t* tasks_this_task(void);
