; Noxiom OS - 64-bit Kernel Entry Point
; _start is first in the binary (enforced by linker.ld).
; Also contains: ISR/IRQ stubs, gdt_flush, idt_load.

[BITS 64]

extern kmain
extern isr_handler
extern irq_handler

global _start
global gdt_flush
global idt_load

; ─── Kernel Entry ──────────────────────────────────────────────────────────────

_start:
    mov rsp, stack_top
    xor rbp, rbp
    call kmain
.halt:
    cli
    hlt
    jmp .halt

; ─── ISR Macros ────────────────────────────────────────────────────────────────

; Exceptions that do NOT push an error code: we push a dummy 0
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        push qword 0
        push qword %1
        jmp isr_common_stub
%endmacro

; Exceptions that DO push an error code (CPU does it automatically)
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        push qword %1
        jmp isr_common_stub
%endmacro

; IRQ stubs (IRQs 0-15 mapped to vectors 32-47 after PIC remapping)
%macro IRQ 2
    global irq%1
    irq%1:
        push qword 0
        push qword %2
        jmp irq_common_stub
%endmacro

; ─── CPU Exception Stubs (0–31) ────────────────────────────────────────────────

ISR_NOERRCODE 0     ; #DE Divide-by-Zero
ISR_NOERRCODE 1     ; #DB Debug
ISR_NOERRCODE 2     ;     NMI
ISR_NOERRCODE 3     ; #BP Breakpoint
ISR_NOERRCODE 4     ; #OF Overflow
ISR_NOERRCODE 5     ; #BR Bound Range Exceeded
ISR_NOERRCODE 6     ; #UD Invalid Opcode
ISR_NOERRCODE 7     ; #NM Device Not Available
ISR_ERRCODE   8     ; #DF Double Fault
ISR_NOERRCODE 9     ;     Coprocessor Segment Overrun
ISR_ERRCODE   10    ; #TS Invalid TSS
ISR_ERRCODE   11    ; #NP Segment Not Present
ISR_ERRCODE   12    ; #SS Stack-Segment Fault
ISR_ERRCODE   13    ; #GP General Protection Fault
ISR_ERRCODE   14    ; #PF Page Fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16    ; #MF x87 FP Exception
ISR_ERRCODE   17    ; #AC Alignment Check
ISR_NOERRCODE 18    ; #MC Machine Check
ISR_NOERRCODE 19    ; #XM SIMD FP Exception
ISR_NOERRCODE 20    ; #VE Virtualization
ISR_ERRCODE   21    ; #CP Control Protection
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30    ; #SX Security Exception
ISR_NOERRCODE 31

; ─── IRQ Stubs (0–15) ──────────────────────────────────────────────────────────

IRQ  0, 32  ; PIT timer
IRQ  1, 33  ; PS/2 keyboard
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40  ; RTC
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ─── Common ISR Stub ───────────────────────────────────────────────────────────
; Stack at entry (bottom to top):
;   [err_code / 0] [int_no] <-- pushed by macro
;   [rip] [cs] [rflags] [rsp] [ss] <-- pushed by CPU

isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp            ; pass pointer to registers_t as first argument
    call isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16             ; discard int_no + err_code
    iretq

; ─── Common IRQ Stub ───────────────────────────────────────────────────────────

irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

; ─── GDT Flush ─────────────────────────────────────────────────────────────────
; void gdt_flush(uint64_t gdt_ptr_addr);
; Loads the GDTR, reloads all segment registers.

gdt_flush:
    lgdt [rdi]
    mov ax, 0x10            ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rax                 ; grab return address
    push qword 0x08         ; kernel code segment selector
    push rax
    retfq                   ; far return: reloads CS

; ─── IDT Load ──────────────────────────────────────────────────────────────────
; void idt_load(uint64_t idt_ptr_addr);

idt_load:
    lidt [rdi]
    sti
    ret

; ─── Kernel Stack ──────────────────────────────────────────────────────────────

section .bss
align 16
stack_bottom:
    resb 65536              ; 64KB kernel stack
stack_top:
