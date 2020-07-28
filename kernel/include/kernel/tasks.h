#ifndef _JOS_KERNEL_TASKS_H
#define _JOS_KERNEL_TASKS_H

/*
    Task management and task state.    

    A task is state and a stack which is used to save and restore the register context and current eip.
    Switching between tasks uses the stack and the iret instruction for simplicity.
*/

enum 
{
	kPri_Idle = 0,
	kPri_Low,
	kPri_Medium,
	kPri_Highest,

	kPri_NumLevels
};

typedef void (*task_func_t)(void* obj);
typedef struct _task_context task_context_t;

// initialise task system and switch to the root task
_JOS_NORETURN void k_tasks_init(task_func_t root, void* obj);

typedef struct _task_create_info
{
    unsigned int _pri;
    task_func_t _func;
    void*       _obj;
} task_create_info_t;

// create a task, return the id.
// this sets up the initial stack and context for the task.
task_context_t* k_task_create(task_create_info_t* info);
void k_task_yield(void);
int k_task_priority(const task_context_t* task);

#endif // _JOS_KERNEL_TASKS_H