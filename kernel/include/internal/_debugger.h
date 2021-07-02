// packet definitions for the debugger/kernel protocol.


typedef struct _debugger_packet_rw_target_memory {
	
	uint64_t    _address;
	uint32_t    _length;
	
} _JOS_PACKED debugger_packet_rw_target_memory_t;

#define INTEL_AMD_MAX_INSTRUCTION_LENGTH 15
typedef struct _debugger_packet_bp {
	interrupt_stack_t   _stack;
	// MAX 15 bytes for Intel/AMD instructions
	uint8_t             _instruction[INTEL_AMD_MAX_INSTRUCTION_LENGTH];
	uint64_t            _cr0;
	uint64_t            _cr2;
	uint64_t            _cr3;
	uint64_t            _cr4;
	
	// NOTE: an array of _stack_size of 8 byte pointers follows this packet if a call stack is present
	uint16_t _call_stack_size;
	
} _JOS_PACKED debugger_packet_bp_t;

typedef struct _debugger_packet_page_info {
	
	uintptr_t   _address;    
	
} _JOS_PACKED debugger_packet_page_info_t;

typedef struct _debugger_packet_page_info_resp {
	
	uintptr_t   _address;
	//NOTE: assumes 4 level paging!
	uintptr_t   _entries[4];
	
} _JOS_PACKED debugger_packet_page_info_resp_t;

typedef struct _debugger_task_info_header {
	
	uint32_t    _num_tasks;
	uint32_t    _task_context_size;
	
} _JOS_PACKED debugger_task_info_header_t;

typedef struct _debugger_task_info {
	// truncated name, 0 terminated
	char                _name[MAX_TASK_NAME_LENGTH+1];
	uint64_t            _entry_pt;
	interrupt_stack_t    _stack;
} _JOS_PACKED debugger_task_info_t;

typedef struct _debugger_packet_rdmsr {
	uint32_t _msr;
} _JOS_PACKED debugger_packet_rdmsr_t;

typedef struct _debugger_packet_rdmsr_resp {
	uint32_t    _msr;
	uint32_t    _lo;
	uint32_t    _hi;
} _JOS_PACKED debugger_packet_rdmsr_resp_t;

#define _BREAKPOINT_STATUS_ENABLED  0
#define _BREAKPOINT_STATUS_DISABLED 1
#define _BREAKPOINT_STATUS_CLEARED  2
typedef struct _debugger_packet_breakpoint_info {
	uint64_t    _at;
	uint8_t     _edc;
} _JOS_PACKED debugger_packet_breakpoint_info_t;

