#pragma once

#include <jos.h>
#include <linear_allocator.h>

typedef void* module_handle_t;

#define module_null_handle 0

// specify requirements and initialisation function during startup
typedef struct _module_registration_info {
    const   char*   name;
    size_t          memory_required;
    jo_status_t (*initialise)(module_handle_t assigned_handle, void* allocated_memory, size_t allocated_memory_size);
} module_registration_info_t;

// initialise the module subsystem with an allocator that will be used to 
// provide memory when requested by each starting module
jo_status_t module_initialise(linear_allocator_t* module_allocator);

// register a module and provide it's memory and other resources
module_handle_t module_register(module_registration_info_t* ss_info);
// there is no deregister...


