# Scheduler — Cooperative Round-Robin Context Switch

## Overview

AuraOS has a **cooperative kernel task scheduler** that lets multiple
kernel-level tasks share the CPU by voluntarily yielding control.

Key properties:

| Property | Value |
|---|---|
| Type | Cooperative (tasks must call `sched_yield()`) |
| Algorithm | Round-robin scan for next `TASK_READY` task |
| Max tasks | 8 (static table, no dynamic allocation) |
| Stack per task | 4 KiB kernel stack, 16-byte aligned |
| Context switch | Callee-saved registers only (rbx, rbp, r12-r15) |

Task 0 is always the **boot task** (the kernel's initial idle loop).
Additional tasks are created at init time with `sched_create_task()`.

---

## Context Switch Flow

```
  sched_yield()                    switch_to(prev, next)
  ─────────────                    ──────────────────────
       │
       ├─ Find next READY task
       ├─ Set prev → READY, next → RUNNING
       ├─ prev->ticks++
       └─ Call switch_to(&prev, &next)
                │
                ▼
  ┌─────────────────────────┐
  │  switch_to (switch.S)   │
  │                         │
  │  push rbx, rbp, r12-r15│  ◄── save prev's callee regs
  │  mov rsp → prev->rsp   │  ◄── snapshot stack pointer
  │                         │
  │  mov next->rsp → rsp   │  ◄── restore next's stack
  │  pop r15-r12, rbp, rbx │  ◄── restore next's callee regs
  │  ret                    │  ◄── jump to saved instruction pointer
  └─────────────────────────┘
                │
                ▼
  If first time: ret → task_trampoline → entry()
  If resuming:   ret → somewhere inside sched_yield()
```

---

## Stack Frame Layout

When `switch_to()` is called, the stack looks like this
(higher addresses at the top, stack grows downward):

```
  Address                   Contents
  ───────                   ────────
  RSP + 0x30   ┌────────┐
               │  rbx   │   ← pushed first
  RSP + 0x28   ├────────┤
               │  rbp   │
  RSP + 0x20   ├────────┤
               │  r12   │
  RSP + 0x18   ├────────┤
               │  r13   │
  RSP + 0x10   ├────────┤
               │  r14   │
  RSP + 0x08   ├────────┤
               │  r15   │   ← RSP points here after switch_to's push phase
  RSP + 0x00   └────────┘
```

For a **freshly created task**, the initial stack frame is hand-crafted
inside `sched_create_task()`:

```
  kstack + 4096 (top, 16-byte aligned)
    ┌─────────────────────────────────┐
    │  entry function pointer         │  ← trampoline pops this into %rdi, calls it
    ├─────────────────────────────────┤  (sp0 - 16)
    │  task_trampoline address        │  ← ret from switch_to lands here
    ├─────────────────────────────────┤  (sp0 - 24)
    │  rbx = 0                        │
    ├─────────────────────────────────┤
    │  rbp = 0                        │
    ├─────────────────────────────────┤
    │  r12 = 0                        │
    ├─────────────────────────────────┤
    │  r13 = 0                        │
    ├─────────────────────────────────┤
    │  r14 = 0                        │
    ├─────────────────────────────────┤
    │  r15 = 0                        │  ← RSP initially points here
    └─────────────────────────────────┘
```

---

## Round-Robin Yield Algorithm

`sched_yield()` uses a simple linear scan starting from `cur_task + 1`:

```
  prev = cur_task
  for i in 1..TASK_MAX:
      candidate = (prev + i) % TASK_MAX
      if tasks[candidate].state == TASK_READY:
          next = candidate
          break
  if no next found: return (nothing to yield to)
  prev.state = READY
  next.state = RUNNING
  prev.ticks++
  switch_to(&tasks[prev], &tasks[next])
```

This gives every READY task a fair turn. Blocked tasks are skipped.

---

## task_trampoline

New tasks don't start directly at their entry function. Instead, they
enter through `task_trampoline` (in `switch.S`), which:

1. Pops the `entry` function pointer from the stack into `%rdi`.
2. Calls `entry()`.
3. If `entry()` returns, parks in an infinite `sched_yield()` loop.

This ensures a task that accidentally returns from its entry function
doesn't crash the kernel — it just becomes a permanent yielder.

---

## API Reference

```c
void sched_init(void);
```

Initialize the scheduler. Must be called once at boot before any
`sched_create_task()` or `sched_yield()` calls. Sets up task 0 (idle).

```c
int sched_create_task(const char *name, void (*entry)(void));
```

Create a new cooperative task. Returns the task PID (0..TASK_MAX-1)
on success, -1 on failure (table full, NULL args). The task starts in
`TASK_READY` state.

```c
void sched_yield(void);
```

Voluntarily yield the CPU to the next READY task. The current task's
callee-saved registers and instruction pointer are saved; the next
task's are restored.

```c
struct task_struct *sched_current(void);
```

Returns a pointer to the currently-running `task_struct`.

---

## Source Files

| File | Purpose |
|---|---|
| `kernel/include/aura/sched.h` | `task_struct` definition, states, API prototypes |
| `kernel/core/sched.c` | Scheduler logic: init, create, yield, round-robin |
| `kernel/arch/x86_shared/switch.S` | Low-level `switch_to()` and `task_trampoline` |

---

## Testing

The scheduler is a freestanding kernel component and cannot be directly
host-tested. Verify via:

1. **Build:** `make clean && make all` must succeed with zero warnings.
2. **QEMU boot:** `timeout 8 qemu-system-x86_64 -cdrom build/auraos.iso -serial stdio -display none -no-reboot` — check for `[OK] Scheduler initialized` on serial.
