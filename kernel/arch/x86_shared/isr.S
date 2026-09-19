/**
 * @file        kernel/arch/x86_shared/isr.S
 * @layer       STAGE_1_HARDWARE_BOOT
 * @component   ARCH_ISR_STUBS
 * @contract    PRD-07-boot-platform
 * @description Low-level assembly trampolines for CPU exceptions (0..31) and
 *              hardware IRQs (32..47). Pushes dummy error codes where necessary,
 *              saves general-purpose registers, switches to C ABI, and returns via iretq.
 *
 * @connects
 *              - Upstream:   CPU interrupt vector invocation
 *              - Downstream: kernel/arch/x86_shared/idt.c (isr_handler, irq_handler)
 *              - Hardware:   CPU execution context & interrupt stack frame
 *
 * @flow        [AURA_FLOW: ISR_TRAMPOLINE]
 *              1. CPU triggers interrupt / exception.
 *              2. Stub pushes dummy error code (if absent) and interrupt number.
 *              3. Common stub pushes RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8-R15.
 *              4. Passes stack pointer as struct registers * argument to C handler.
 *              5. Restores registers and resumes execution via iretq.
 */

.section .text
.code64

.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    pushq $0                /* Dummy error code */
    pushq $\num              /* Interrupt number */
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr\num
isr\num:
    /* CPU pushes error code automatically */
    pushq $\num              /* Interrupt number */
    jmp isr_common_stub
.endm

.macro IRQ_STUB num, irq
.global irq\num
irq\num:
    pushq $0                /* Dummy error code */
    pushq $\irq             /* Interrupt number */
    jmp irq_common_stub
.endm

/* 32 CPU Exceptions */
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

/* 16 PIC IRQs (mapped to vectors 32..47) */
IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

.extern isr_handler
.extern irq_handler

isr_common_stub:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi         /* Argument 1: registers_t *regs */
    call isr_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp          /* Clean up int_no and err_code */
    iretq

irq_common_stub:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi         /* Argument 1: registers_t *regs */
    call irq_handler

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    addq $16, %rsp          /* Clean up int_no and err_code */
    iretq
