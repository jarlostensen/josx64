#pragma once
#include <jos.h>

typedef enum _task_priority_level {

    kTaskPri_Highest = 0,
    kTaskPri_Normal,
    kTaskPri_Lowest,

    kTaskPri_NumPris,

} task_priority_level_t;

typedef void*   task_handle_t;
typedef jo_status_t (*task_func_t)(void* ptr);

typedef struct _task_create_args {

    task_func_t             func;
    void*                   ptr;
    task_priority_level_t   pri;
    const char*             name;

} task_create_args_t;

void            tasks_initialise(heap_allocator_t* allocator);
// start the idle task on this CPU.
// this function never returns
void            tasks_start_idle(void);
task_handle_t   tasks_create(task_create_args_t* args);
void            tasks_yield(void);

