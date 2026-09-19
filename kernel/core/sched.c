/**
 * @file        kernel/core/sched.c
 * @layer       STAGE_3_VIRTUAL_SCHEDULER
 * @component   COOPERATIVE_SCHEDULER
 * @contract    ADR-001-normative-architecture
 * @description Cooperative round-robin scheduler.
 *              Manages a static table of TASK_MAX tasks. Task 0 is the boot
 *              task; additional tasks are created with sched_create_task().
 *              sched_yield() round-robins to the next READY task via the
 *              low-level switch_to() in switch.S.
 *
 * @connects
 *              - Upstream:   kernel/main.c (sched_init, sched_yield)
 *              - Downstream: kernel/arch/x86_shared/switch.S (switch_to)
 *
 * @flow        [AURA_FLOW: SCHED_YIELD]
 *              1. Caller invokes sched_yield().
 *              2. Round-robin scan from cur+1 for next TASK_READY task.
 *              3. Mark current task READY, next task RUNNING.
 *              4. Call switch_to(&tasks[cur], &tasks[next]) to swap stacks.
 */

#include <aura/sched.h>
#include <aura/serial.h>
#include <aura/vga.h>

/* [AURA_COMPONENT: COOPERATIVE_SCHEDULER] */
/* Forward declarations — implemented in kernel/arch/x86_shared/switch.S */
extern void switch_to(struct task_struct *prev, struct task_struct *next);
extern void task_trampoline(void);

/* Static task table and current index */
static struct task_struct tasks[TASK_MAX];
static int cur_task = 0;
static int task_count = 0;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void sched_init(void) {
    /* [AURA_FLOW: SCHED_INIT] Task 0 = boot task, uses the existing stack */
    for (int i = 0; i < TASK_MAX; i++) {
        tasks[i].state = TASK_BLOCKED;
        tasks[i].rsp   = 0;
        tasks[i].pid   = (uint32_t)i;
        tasks[i].ticks = 0;
        tasks[i].name[0] = '\0';
    }

    tasks[0].state = TASK_RUNNING;
    tasks[0].pid   = 0;
    tasks[0].ticks = 0;
    /* Copy name manually (freestanding — no string.h) */
    const char *boot_name = "idle";
    for (int j = 0; boot_name[j]; j++) {
        tasks[0].name[j] = boot_name[j];
        tasks[0].name[j + 1] = '\0';
    }

    cur_task   = 0;
    task_count = 1;

    serial_printf(COM1, "[OK] Scheduler initialized (task 0: idle, cooperative RR)\n");
    vga_printf("[OK] Scheduler initialized\n");
}

int sched_create_task(const char *name, void (*entry)(void)) {
    if (!name || !entry) return -1;
    if (task_count >= TASK_MAX) return -1;

    int idx = task_count;
    struct task_struct *t = &tasks[idx];

    /* Fill in the task control block */
    t->pid   = (uint32_t)idx;
    t->state = TASK_READY;
    t->ticks = 0;

    /* Copy name */
    for (int j = 0; j < 31 && name[j]; j++) {
        t->name[j] = name[j];
        t->name[j + 1] = '\0';
    }

    /*
     * Build the initial kernel stack for this task.
     * Stack grows DOWN on x86_64.  We place the initial frame at the TOP
     * of the task's kstack[] buffer.
     *
     * The trick: switch_to()'s `ret` pops a return address from the stack.
     * We place the address of `task_trampoline` (in switch.S) there.
     * The next slot is the `entry` function pointer, which trampoline
     * pops into %rdi and calls.
     *
     * Stack layout (highest address at top, grows down):
     *
     *   kstack + KSTACK_SIZE
     *     [ 8 bytes: task_trampoline ]  <-- "return address" for switch_to's ret
     *     [ 8 bytes: entry pointer   ]  <-- trampoline pops this and calls it
     *     [ 8 bytes: rbx = 0 ]          <-- callee-saved (restored by switch_to)
     *     [ 8 bytes: rbp = 0 ]
     *     [ 8 bytes: r12 = 0 ]
     *     [ 8 bytes: r13 = 0 ]
     *     [ 8 bytes: r14 = 0 ]
     *     [ 8 bytes: r15 = 0 ]          <-- RSP points here when switch_to loads
     *
     * When switch_to() runs for the first time with this task as 'next':
     *   1. mov (%rsi), %rsp   — loads our crafted RSP
     *   2. pop r15..rbx       — all zeros
     *   3. ret                — pops task_trampoline, jumps to it
     *   4. task_trampoline    — pops entry, calls it, parks on sched_yield
     */

    /* Start from top of kstack, align down to 16 bytes */
    uint64_t *sp = (uint64_t *)(t->kstack + KSTACK_SIZE);
    sp = (uint64_t *)((uintptr_t)sp & ~0xFULL);

    /*
     * Stack order (grows down, popped in reverse):
     * Highest address (sp0 - 8):  entry arg (trampoline will pop this into %rdi)
     * Next slot       (sp0 - 16): task_trampoline address (switch_to's `ret` lands here!)
     * Rest (sp0 - 24..64):        callee-saved registers (rbx..r15)
     */
    *(--sp) = (uint64_t)(uintptr_t)entry;            /* entry arg for trampoline */
    *(--sp) = (uint64_t)(uintptr_t)task_trampoline;  /* return address for switch_to */

    /* Callee-saved: switch_to will pop these on first restore */
    *(--sp) = 0;  /* rbx */
    *(--sp) = 0;  /* rbp */
    *(--sp) = 0;  /* r12 */
    *(--sp) = 0;  /* r13 */
    *(--sp) = 0;  /* r14 */
    *(--sp) = 0;  /* r15 */

    t->rsp = (uint64_t)(uintptr_t)sp;

    task_count++;

    serial_printf(COM1, "[OK] Task '%s' created (pid=%u, stack @ %p)\n",
                  t->name, t->pid, (void *)sp);
    vga_printf("[OK] Task '%s' created (pid=%u)\n", t->name, t->pid);

    return idx;
}

void sched_yield(void) {
    /* [AURA_FLOW: SCHED_YIELD] */
    int prev = cur_task;

    /* Round-robin: find next READY task */
    int next = -1;
    for (int i = 1; i <= TASK_MAX; i++) {
        int candidate = (prev + i) % TASK_MAX;
        if (tasks[candidate].state == TASK_READY) {
            next = candidate;
            break;
        }
    }

    /* If no READY task found, return (nothing to switch to) */
    if (next < 0) return;

    /* Update states */
    if (tasks[prev].state == TASK_RUNNING) {
        tasks[prev].state = TASK_READY;
    }
    tasks[next].state = TASK_RUNNING;
    tasks[prev].ticks++;
    cur_task = next;

    /* [AURA_CONNECTS: COOPERATIVE_SCHEDULER -> ARCH_CONTEXT_SWITCH] */
    switch_to(&tasks[prev], &tasks[next]);
}

struct task_struct *sched_current(void) {
    return &tasks[cur_task];
}
