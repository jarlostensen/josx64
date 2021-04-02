
#include <jos.h>
#include <module.h>
#include <memory.h>

typedef struct _module_allocation_info {

    const char*     _ss_name;
    size_t          _max_memory;    

} module_allocation_info_t;

static module_allocation_info_t  _modules[8];
static size_t _modules_registered = 0;
static linear_allocator_t* _module_allocator = 0;

jo_status_t module_initialise(linear_allocator_t* module_allocator) {
    _module_allocator = module_allocator;
    return _JO_STATUS_SUCCESS;
}

module_handle_t module_register(module_registration_info_t* ss_info) {
    _JOS_ASSERT(_modules_registered<8);
    _modules[_modules_registered]._ss_name = ss_info->name;
    _modules[_modules_registered]._max_memory = ss_info->memory_required;
    void* memory = linear_allocator_alloc(_module_allocator, ss_info->memory_required);
    if ( !memory ) {
        return module_null_handle;
    }    
    ++_modules_registered;
    // let the module initialise itself with the memory we've allocated
    ss_info->initialise((module_handle_t)_modules_registered, 
                        memory, 
                        ss_info->memory_required);
    return (module_handle_t)(_modules + (_modules_registered-1));
}
