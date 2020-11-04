
#include <jos.h>
#include <string.h>
#include <c-efi-protocol-multi-processor.h>

// in efi_main.c
extern CEfiBootServices * g_boot_services;
extern CEfiSystemTable  * g_st;

static CEfiMultiProcessorProtocol*  _mpp = 0;

static size_t   _bsp_id = 0;
static size_t   _num_enabled_processors = 1;

k_status    processors_initialise() {

    CEfiHandle handle_buffer[3];
    CEfiUSize handle_buffer_size = sizeof(handle_buffer);
    memset(handle_buffer,0,sizeof(handle_buffer));

    CEfiStatus status = g_boot_services->locate_handle(C_EFI_BY_PROTOCOL, &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, 0, &handle_buffer_size, handle_buffer);
    if ( status==C_EFI_SUCCESS ) {

        //TODO: this works but it's not science; what makes one handle a better choice than another? 
        //      we should combine these two tests (handler and resolution) and pick the base from that larger set
        size_t num_handles = handle_buffer_size/sizeof(CEfiHandle);
        for(size_t n = 0; n < num_handles; ++n)
        {
            status = g_boot_services->handle_protocol(handle_buffer[n], &C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID, (void**)&_mpp);
            if ( status == C_EFI_SUCCESS )
            {
                break;
            }            
        }
        
        if ( status == C_EFI_SUCCESS ) {                
            status = _mpp->who_am_i(_mpp, &_bsp_id);
            if ( status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_INTERNAL;
            }

            CEfiUSize count;
            status = _mpp->get_number_of_processors(_mpp, &count, &_num_enabled_processors);
            if ( status != C_EFI_SUCCESS ) {
                return _JOS_K_STATUS_INTERNAL;
            }
        }
    }

    //TODO: else uniprocessor

    return _JOS_K_STATUS_SUCCESS;        
}

size_t processors_get_processor_count() {
    return _num_enabled_processors;
}