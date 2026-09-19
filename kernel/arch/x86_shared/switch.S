/**
 * @file        kernel/arch/x86_shared/switch.S
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   ARCH_CONTEXT_SWITCH
 * @contract    ADR-001-normative-architecture
 * @description Low-level x86_64 cooperative context switch and new-task trampoline.
 *              switch_to() saves/restores callee-saved registers and swaps RSP
 *              between two task_structs. task_trampoline is the first-time entry
 *              point for freshly created tasks.
 *
 * @connects
 *              - Upstream:   kernel/core/sched.c (sched_yield, sched_create_task)
 *              - Downstream: task kernel stacks
 *              - Hardware:   CPU general-purpose registers (SysV AMD64 ABI)
 *
 * @flow        [AURA_FLOW: CONTEXT_SWITCH]
 *              1. Push callee-saved registers (rbx, rbp, r12-r15) onto current stack.
 *              2. Save current RSP into prev->rsp (offset 0 of task_struct).
 *              3. Load next->rsp into RSP.
 *              4. Pop callee-saved registers in reverse order.
 *              5. RET into the next task's saved instruction pointer.
 */

.section .text
.code64

/* ------------------------------------------------------------------ */
/*  switch_to(prev, next)                                             */
/*                                                                    */
/*  SysV AMD64 ABI:                                                   */
/*    %rdi = prev  (pointer to current task_struct)                   */
/*    %rsi = next  (pointer to next task_struct)                      */
/*                                                                    */
/*  task_struct layout: rsp is at offset 0.                           */
/* ------------------------------------------------------------------ */
.global switch_to
switch_to:
    /* Save callee-saved registers onto prev's stack */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Save current stack pointer into prev->rsp (offset 0) */
    movq  %rsp, (%rdi)

    /* Load next task's stack pointer */
    movq  (%rsi), %rsp

    /* Restore callee-saved registers from next's stack */
    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %rbp
    popq  %rbx

    /* Return — pops the saved instruction pointer from the new stack.
     * For a fresh task, this is the address of task_trampoline.
     * For a previously-scheduled task, this is somewhere inside sched_yield(). */
    ret

/* ------------------------------------------------------------------ */
/*  task_trampoline                                                    */
/*                                                                    */
/*  First-time entry point for newly created tasks.                   */
/*  After switch_to's ret lands here, the stack looks like:           */
/*                                                                    */
/*    RSP + 0  ->  entry function pointer (uint64_t)                 */
/*    RSP + 8  ->  (rest of stack)                                    */
/*                                                                    */
/*  We pop the entry pointer into %rdi (not a real argument — we     */
/*  just use %rdi as a scratch register) and call through it.        */
/*  If entry() ever returns, we park in a sched_yield() loop.        */
/* ------------------------------------------------------------------ */
.extern sched_yield
.global task_trampoline
task_trampoline:
    /* Pop the entry function pointer from the stack into %rdi */
    popq    %rdi

    /* RSP is now 16-byte aligned (sp was 16-aligned, we pushed 8 values
     * of 8 bytes each = 64, switch_to popped 7 (6 regs + ret) = 56,
     * we popped 1 more = 8, total 64, back to 16-aligned).
     * The call will push 8 bytes (return addr), giving callee standard
     * RSP % 16 == 8 alignment.  Safe for SysV ABI. */
    callq   *%rdi

    /* entry() returned — park this task in a yield loop forever */
.L_park:
    callq   sched_yield
    jmp     .L_park
