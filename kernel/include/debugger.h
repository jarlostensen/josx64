#ifndef _JOS_KERNEL_DEBUGGER_H
#define _JOS_KERNEL_DEBUGGER_H

#include <jos.h>
#include <pe.h>

// packet ids used to decode data sent to or recieved from the debugger
typedef enum _debugger_packet_id {

    kDebuggerPacket_Continue,
    kDebuggerPacket_Trace,
    kDebuggerPacket_KernelConnectionInfo,
    kDebuggerPacket_Breakpoint,
    kDebuggerPacket_ReadTargetMemory,
    kDebuggerPacket_WriteTargetMemory,
    kDebuggerPacket_GPF,
    kDebuggerPacket_Get_TaskList,
    kDebuggerPacket_SingleStep,
    kDebuggerPacket_TraversePageTable,
    kDebuggerPacket_Assert,
    kDebuggerPacket_RDMSR,
    kDebuggerPacket_UD,
    kDebuggerPacket_UpdateBreakpoints,
    
    // response packets have a high bit set so that they can be filtered in the debugger
    kDebuggerPacket_Response_Mask = 0x800,
    kDebuggerPacket_ReadTargetMemory_Resp = (kDebuggerPacket_ReadTargetMemory + kDebuggerPacket_Response_Mask),
    kDebuggerPacket_Get_TaskList_Resp = (kDebuggerPacket_Get_TaskList + kDebuggerPacket_Response_Mask),
    kDebuggerPacket_TraversePageTable_Resp = (kDebuggerPacket_TraversePageTable + kDebuggerPacket_Response_Mask),
    kDebuggerPacket_RDMSR_Resp = (kDebuggerPacket_RDMSR + kDebuggerPacket_Response_Mask),
    
} debugger_packet_id_t;

// to be called after early initialisation of interrupts
_JOS_API_FUNC void debugger_initialise(jos_allocator_t* allocator);
// outputs disassembly to the console 
_JOS_API_FUNC void debugger_disasm(void* at, size_t bytes, wchar_t* output_buffer, size_t output_buffer_length);

typedef struct _debugger_serial_packet {
    uint32_t        _id;
    uint32_t        _length;
} JOS_PACKED;
typedef struct _debugger_serial_packet debugger_serial_packet_t;

_JOS_INLINE_FUNC void debugger_packet_create(debugger_serial_packet_t* packet, debugger_packet_id_t id, unsigned int length) {
    packet->_id = (uint32_t)id;
    packet->_length = (uint32_t)length;
}

_JOS_INLINE_FUNC debugger_packet_id_t debugger_packet_id(debugger_serial_packet_t* packet) {
    return (debugger_packet_id_t)packet->_id;
}

_JOS_INLINE_FUNC unsigned int debugger_packet_length(debugger_serial_packet_t* packet) {
    return (unsigned int)packet->_length;
}

// waits for an establishes a connection with the remote debugger
// subsequently the "debugger_is_connected" function will return true
_JOS_API_FUNC void debugger_wait_for_connection(peutil_pe_context_t* pe_ctx, uint64_t image_base);

// triggers an assert message to the debugger (if connected)
_JOS_API_FUNC void debugger_trigger_assert(const char* cond, const char* file, int line);

// true if we're connected to a debugger...obviously
_JOS_API_FUNC bool debugger_is_connected(void);

_JOS_API_FUNC void debugger_send_packet(debugger_packet_id_t id, void* data, uint32_t length);
_JOS_API_FUNC void debugger_read_packet_header(debugger_serial_packet_t* packet);
_JOS_API_FUNC void debugger_read_packet_body(debugger_serial_packet_t* packet, void* buffer, uint32_t buffer_size);

_JOS_API_FUNC void debugger_set_breakpoint(uintptr_t at);
_JOS_API_FUNC void debugger_ext_break(void);

#endif // _JOS_KERNEL_DEBUGGER_H
