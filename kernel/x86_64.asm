; A collection of x86_64 assembly

[bits 64]

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

%define STACK_REL(idx) [rsp+8*idx]

; --------------------------------------------------------------------------

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
    
    POPAQ
    ; drop error code and handler id
    add rsp, 16

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

PIC1_COMMAND equ 0x20
PIC2_COMMAND equ 0x0a

; handler; forwards call to the registered handler via argument 0
extern interrupts_irq_handler

;ZZZ: this is not correct for 64 bit
irq_handler_stub:

    PUSHAQ

    ; handler(irq number)
    mov rcx, STACK_REL(15)
    call interrupts_irq_handler

    POPAQ
    
    ; send EOI to the right PIC based on the IRQ number
    pop rax
    cmp al, 8
    jl .irq_handler_stub_1

    ; EOI to PIC2
    mov al, 0x20
    out PIC2_COMMAND, al
    ; always send EOI to master (PIC1)

.irq_handler_stub_1:
    ; EOI to PIC1
    mov al, 0x20
    out PIC1_COMMAND, al

.irq_handler_stub_2:
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
