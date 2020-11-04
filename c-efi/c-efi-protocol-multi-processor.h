#pragma once

/**
 * UEFI Protocol - Graphics Output Protocol
 * added to c-efi by jarl ostensen
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <c-efi-base.h>
#include <c-efi-system.h>

#define C_EFI_MULTI_PROCESSOR_PROTOCOL_GUID C_EFI_GUID(0x3fdda605, 0xa76e, 0x4f46, 0xad, 0x29, 0x12, 0xf4, 0x53, 0x1b, 0x3d, 0x08)

typedef struct _CEfiCpuPhysicalLocation {

    CEfiU32    package;
    CEfiU32    core;
    CEfiU32    thread;

} CEfiCpuPhysicalLocation;

typedef struct _CEfiCpuPhysicalLocation2 {

    CEfiU32     package;
    CEfiU32     module;
    CEfiU32     tile;
    CEfiU32     die;
    CEfiU32     core;
    CEfiU32     thread;

} CEfiCpuPhysicalLocation2;

typedef struct _CEfiExtendedProcessorInformation {

    CEfiCpuPhysicalLocation2        location;

} CEfiExtendedProcessorInformation;

typedef struct _CEfiProcessorInformation {

    CEfiU64                             processor_id;
    CEfiU32                             status_flag;
    CEfiCpuPhysicalLocation             location;
    CEfiExtendedProcessorInformation    extended_information;

} CEfiProcessorInformation;

typedef void (*CEfiApProcedure)(void* obj);

//https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/MpService.h
typedef struct _CEfiMultiProcessorProtocol {

    // This service retrieves the number of logical processor in the platform
    // and the number of those logical processors that are enabled on this boot.
    // This service may only be called from the BSP.
    CEfiStatus (CEFICALL *get_number_of_processors) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiUSize* count, CEfiUSize* enabled_count
    );
    // Gets detailed MP-related information on the requested processor at the
    // instant this call is made. This service may only be called from the BSP.
    CEfiStatus (CEFICALL *get_processor_info) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiUSize processor_number,
        CEfiProcessorInformation* information
    );
    // This service executes a caller provided function on all enabled APs. APs can
    // run either simultaneously or one at a time in sequence. This service supports
    // both blocking and non-blocking requests. The non-blocking requests use EFI
    // events so the BSP can detect when the APs have finished. This service may only
    // be called from the BSP.
    CEfiStatus (CEFICALL *startup_all_aps) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiApProcedure procedure,
        CEfiUSize processor_number,
        CEfiEvent wait_event,
        CEfiUSize timeout,
        void* proc_arg,
        bool *optional_finished
    );
    // This service lets the caller get one enabled AP to execute a caller-provided
    // function. The caller can request the BSP to either wait for the completion
    // of the AP or just proceed with the next task by using the EFI event mechanism.
    // See EFI_MP_SERVICES_PROTOCOL.StartupAllAPs() for more details on non-blocking
    // execution support.  This service may only be called from the BSP.
    CEfiStatus (CEFICALL *startup_this_ap) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiApProcedure procedure,
        CEfiUSize processor_number,
        CEfiEvent wait_event,
        CEfiUSize timeout,
        void* proc_arg,
        bool *optional_finished
    );
    // This service switches the requested AP to be the BSP from that point onward.
    // This service changes the BSP for all purposes.   This call can only be performed
    // by the current BSP.
    // This service switches the requested AP to be the BSP from that point onward.
    // This service changes the BSP for all purposes. The new BSP can take over the
    // execution of the old BSP and continue seamlessly from where the old one left
    // off. This service may not be supported after the UEFI Event EFI_EVENT_GROUP_READY_TO_BOOT
    // is signaled.
    CEfiStatus (CEFICALL *switch_bsp) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiUSize processor_number,
        bool enable_old_bsp
    );
    // This service lets the caller enable or disable an AP from this point onward.
    // This service may only be called from the BSP.
    CEfiStatus (CEFICALL *enable_disable_ap) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiUSize processor_number,
        bool enable_ap,
        CEfiU32 * optional_healt_flag
    );
    // This return the handle number for the calling processor.  This service may be
    // called from the BSP and APs.
    CEfiStatus (CEFICALL *who_am_i) (
        struct _CEfiMultiProcessorProtocol* this_,
        CEfiUSize * id
    );

} CEfiMultiProcessorProtocol;

#ifdef __cplusplus
}
#endif
