/**
 * @file        kernel/include/aura/sched.h
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   COOPERATIVE_SCHEDULER
 * @contract    ADR-001-normative-architecture
 * @description Cooperative round-robin task scheduler interface.
 *              Defines task_struct, task states, and scheduling API for
 *              kernel-level cooperative multitasking via context switch.
 *
 * @connects
 *              - Upstream:   kernel/main.c (sched_init), kernel core subsystems
 *              - Downstream: kernel/core/sched.c, kernel/arch/x86_shared/switch.S
 *
 * @flow        [AURA_FLOW: SCHED_YIELD]
 *              1. sched_init() sets up task 0 as the boot task (RUNNING).
 *              2. sched_create_task() allocates a task with a private kernel stack.
 *              3. sched_yield() picks the next READY task via round-robin.
 *              4. switch_to() saves/restores callee-saved registers and swaps RSP.
 */

#ifndef AURA_SCHED_H
#define AURA_SCHED_H

#include <stdint.h>

/* Task states */
#define TASK_READY    0
#define TASK_RUNNING  1
#define TASK_BLOCKED  2

/* Maximum concurrent kernel tasks */
#define TASK_MAX      8

/* Per-task kernel stack size */
#define KSTACK_SIZE   4096

/*
 * Task Control Block.
 *
 * rsp MUST be the first member — switch_to() in switch.S uses offset 0
 * to save/restore the stack pointer.
 */
struct task_struct {
    uint64_t rsp;                   /* Saved stack pointer (offset 0) */
    uint32_t pid;                   /* Process ID (0 = boot task)     */
    char     name[32];              /* Human-readable task name       */
    uint32_t state;                 /* TASK_READY / TASK_RUNNING / TASK_BLOCKED */
    uint64_t ticks;                 /* Total scheduling ticks on this task */
    uint8_t  kstack[KSTACK_SIZE] __attribute__((aligned(16)));
};

/* Initialize the scheduler. Creates task 0 (boot task). Call once at boot. */
void sched_init(void);

/*
 * Create a new cooperative task.
 *   name  — human-readable label (copied, max 31 chars)
 *   entry — function the task starts executing
 * Returns task PID (>= 0) on success, -1 on failure.
 */
int sched_create_task(const char *name, void (*entry)(void));

/*
 * Cooperatively yield the CPU to the next READY task.
 * Saves the current task's context and restores the next task's.
 */
void sched_yield(void);

/* Return a pointer to the currently-running task_struct. */
struct task_struct *sched_current(void);

#endif /* AURA_SCHED_H */
