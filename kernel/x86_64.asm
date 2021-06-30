[bits 64]
[default rel] ;< needed to ensure RIP relative addressing is used

section .text

;=====================================================================================
; interrupt handling
;
; stack frame when the CPU has called an interupt handler:
; +---------------+
; |               |
; |      SS       | 40
; |      RSP      | 32
; |     RFLAGS    | 24
; |      CS       | 16
; |      RIP      | 8
; |   Error code  | 0 <- rsp
; |               |
; +---------------+
;
; see for example https://0xax.gitbooks.io/linux-insides/content/Interrupts/linux-interrupts-1.html and 
; the IA-32 dev manual Volume 2 6-12
;

; push all gen. registers for interrupts.c::isr_tstack_t
%macro PUSHAQ 0
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rbp
    push    rsi
    push    rdi
%endmacro

; pop all gen. registers as pushed by PUSHAQ
%macro POPAQ 0
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
%endmacro

%macro ISR_CLEAN_STACK 0
    POPAQ
    ; drop error code and handler id
    add rsp, 16
%endmacro 

%define STACK_REL(idx) [rsp+8*idx]

%define DBG_BREAK int 3

; --------------------------------------------------------------------------
; task handlers

%define TASK_CONTEXT_T_RSP 160
%define TASK_CONTEXT_T_SS  168

global x86_64_task_switch
; task_context_t* int x86_64_task_switch(uintptr_t* curr_stack, uintptr_t* new_stack)
; curr/next_stack are 2 element arrays containing rsp, ss
x86_64_task_switch:

    ; prev may be 0 when idle tasks are started
    test    rcx, rcx
    jz      ._task_switch_next

    ; save current context on this stack
    ; cs:_task_switch_resume for our return to this task

    pushfq
    pop     rax
    ; make sure IF=1 always so that tasks never resume with it off
    or      rax, (1<<9)
    push    rax
    xor     rax, rax
    mov     ax, cs
    push    ax
    ; we'll continue here when we next switch back to this task
    lea     rax,[._task_switch_resume]
    push    rax
    push    0
    push    0
    PUSHAQ

    cli
    ; save current task's stack
    mov     [rcx+0], rsp
    xor     rax, rax
    mov     ax, ss
    mov     [rcx+8], rax

._task_switch_next:
    
    cli
    ; switch to new task's stack
    mov     rsp, [rdx+0]
    mov     rax, [rdx+8]
    mov     ss, ax          ;< this isn't really needed, since we never change ss but kept for good measure    
    sti

    ; restore registers for the task we're switching to and jump to it
    ISR_CLEAN_STACK
    iretq

._task_switch_resume:
    ; when we next switch back to "curr"'s context we'll appear here
    ; return "curr", i.e. the currently running task
    mov rax, rcx
    ret

; --------------------------------------------------------------------------
; interrupt handlers

; nop-handler stub used for testing
isr_null_handler:
    add rsp, 16
    iretq

; ISRs
; in interrupts.c
extern interrupts_isr_handler
isr_handler_stub:

    PUSHAQ
    cld 
    ; rcx = rsp for fastcall argument 0; ptr to isr_stack_t
    mov rcx, rsp
    call interrupts_isr_handler
    
    ISR_CLEAN_STACK
    iretq

; an isr/fault/trap that doesn't provide an error code
%macro ISR_HANDLER 1
global interrupts_isr_handler_%1
interrupts_isr_handler_%1:
    cli
    ; empty error code
    push qword 0
    ; isr id
    push qword %1    
    jmp isr_handler_stub    
%endmacro

; some faults (but not all) provide an error code
%macro ISR_HANDLER_ERROR_CODE 1
global interrupts_isr_handler_%1
interrupts_isr_handler_%1:
    cli     
    ; error code is already pushed
    ; isr id
    push qword %1    
    jmp isr_handler_stub
%endmacro

; intel IA dev guide Vol 3 6.3

; divide by zero
ISR_HANDLER 0
; debug
ISR_HANDLER 1
; NMI
ISR_HANDLER 2
; breakpoint
ISR_HANDLER 3
; overflow
ISR_HANDLER 4
; bound range exceeded
ISR_HANDLER 5
; invalid opcode
ISR_HANDLER 6
; device not available
ISR_HANDLER 7
; double fault
ISR_HANDLER_ERROR_CODE 8
; "coprocessor segment overrun"
ISR_HANDLER 9
; invalid TSS
ISR_HANDLER_ERROR_CODE 10
; segment not present
ISR_HANDLER_ERROR_CODE 11
; stack segment fault
ISR_HANDLER_ERROR_CODE 12
; general protection fault
ISR_HANDLER_ERROR_CODE 13
; page fault
ISR_HANDLER_ERROR_CODE 14
; this one is reserved...
ISR_HANDLER 15
; x87 FP exception
ISR_HANDLER 16
; alignment check fault
ISR_HANDLER_ERROR_CODE 17
; machine check
ISR_HANDLER 18
; SIMD fp exception
ISR_HANDLER 19
; virtualization exception
ISR_HANDLER 20
; reserved
ISR_HANDLER 21
ISR_HANDLER 22
ISR_HANDLER 23
ISR_HANDLER 24
ISR_HANDLER 25
ISR_HANDLER 26
ISR_HANDLER 27
ISR_HANDLER 28
ISR_HANDLER 29
; security exception
ISR_HANDLER_ERROR_CODE 30
; "fpu error interrupt"
ISR_HANDLER 31

; =====================================================================================
; IRQs

PIC1_COMMAND            equ 0x20
PIC2_COMMAND            equ 0xa0
PIC_NON_SPECIFIC_EOI    equ 0x20

global i8259a_send_eoi
; void i8259a_send_eoi(int irq)
i8259a_send_eoi:
    push rax
    cmp cl, 8
    jl .i8259a_send_eoi_1
    ; EOI to PIC2
    mov al, PIC_NON_SPECIFIC_EOI
    out PIC2_COMMAND, al
    ; just a delay
    out 80h, al
    ; +always send EOI to master (PIC1)
.i8259a_send_eoi_1:
    ; EOI to PIC1
    mov al, PIC_NON_SPECIFIC_EOI
    out PIC1_COMMAND, al
    pop rax
    ret

; handler; forwards call to the registered handler via argument 0
extern interrupts_irq_handler
irq_handler_stub:

    PUSHAQ

    ; interrupts_irq_handler(irq number)
    mov rcx, STACK_REL(15)
    call interrupts_irq_handler

    POPAQ
    
    add rsp,8
    iretq

%macro IRQ_HANDLER 1
global interrupts_irq_handler_%1
interrupts_irq_handler_%1:        
    ; needed for EOI check later
    push qword %1
    jmp irq_handler_stub
%endmacro

IRQ_HANDLER 0 
IRQ_HANDLER 1
IRQ_HANDLER 2
IRQ_HANDLER 3
IRQ_HANDLER 4
IRQ_HANDLER 5
IRQ_HANDLER 6
IRQ_HANDLER 7
IRQ_HANDLER 8
IRQ_HANDLER 9
IRQ_HANDLER 10 
IRQ_HANDLER 11
IRQ_HANDLER 12
IRQ_HANDLER 13
IRQ_HANDLER 14
IRQ_HANDLER 15
IRQ_HANDLER 16
IRQ_HANDLER 17
IRQ_HANDLER 18
IRQ_HANDLER 19

; ===================================================================================
; general stuff

global x86_64_get_rflags
x86_64_get_rflags:
    pushfq
    pop rax
    ret

global x86_64_get_cs
x86_64_get_cs:
    xor     rax, rax
    mov     ax, cs
    ret

global x86_64_get_ss
x86_64_get_ss:
    xor     rax, rax
    mov     ax, ss
    ret

global x86_64_get_pml4
x86_64_get_pml4:
    mov     rax, cr3
    ret

global x86_64_get_rsp
x86_64_get_rsp:
    mov     rax, rsp
    ret

; apparently one does not simply disable chkstk insertion on Clang, so this is the second best thing
; https://metricpanda.com/rival-fortress-update-45-dealing-with-__chkstk-__chkstk_ms-when-cross-compiling-for-windows/
global __chkstk
__chkstk:
    ret
