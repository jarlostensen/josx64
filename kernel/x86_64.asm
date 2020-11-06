; A collection of x86_64 assembly

[bits 64]

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
; see for example https://0xax.gitbooks.io/linux-insides/content/Interrupts/linux-interrupts-1.html
;

; push all gen. registers for interrupts.h::isr_tstack_t
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

; ===================================================================================
; general stuff

global x86_64_rflags:function
x86_64_rflags:
    pushfq
    pop rax
    ret



