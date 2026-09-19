---
layout: default
title: Subagent Development Task Queue
---

# 🤖 AuraOS Subagent Task Queue & Claim Board (`TASK-QUEUE-001`)

**Document ID:** `TASK-QUEUE-001`  
**Owner:** Lead Architect / Antigravity Orchestrator  
**Status:** ACTIVE · NORMATIVE FOR ALL SUBAGENT ASSIGNMENTS  
**Rule Reference:** Must follow [`docs/RULEBOOK.md`](RULEBOOK.md) for code tagging, contracts, and evidence submission.

---

## 1. Protocol for Subagents: How to Claim & Execute a Task

Whenever an autonomous subagent is spawned to work on AuraOS:

1. **Check the Queue:** Inspect the task tables below. Find the highest-priority task marked `[READY]`.
2. **Atomic Claim:** Edit this file (`docs/TASK_QUEUE.md`) to change the status to `[CLAIMED]`, insert your **Agent ID / Role**, and write the current timestamp.
3. **Read Contracts First:** Before writing code, view the normative documents listed in the task's **Contracts** column.
4. **Implement with Code Tags:** Every modified or created file must use the `@file` header and bracketed tokens (`[AURA_LAYER]`, `[AURA_COMPONENT]`, `[AURA_FLOW]`, `[AURA_CONNECTS]`).
5. **Verify Gate:** Run the designated verification command (e.g. `make test`, QEMU boot test).
6. **Mark Complete:** Update this board to `[COMPLETED]`, link the commit hash, and log the evidence in [`docs/wiki/06-validation/VAL-001-test-gates.md`](wiki/06-validation/VAL-001-test-gates.md).

### Status Legend
* `[READY]` — Available for immediate claiming. Dependencies are satisfied.
* `[BLOCKED]` — Waiting on prior tasks or hardware dependencies.
* `[CLAIMED]` — Actively being implemented by an assigned subagent.
* `[REVIEW]` — Code complete, pending verification / gate approval.
* `[COMPLETED]` — Tested, merged, evidence recorded.

---

## 2. Active Development Task Queue

### Sprint 2: Memory Subsystem Modernization & Page Faults (High Priority)

| Task ID | Component | Description & Target | Contracts | Dependencies | Status | Assigned Agent |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TASK-MEM-00` | zRAM Compressor | In-RAM WKdm/RLE compressed page pool (`zram.c`, `zram.h`), telemetry hooks, and TUI display gauge. | `PRD-05-resources`, `auraos_memory_blueprint` | None | `[COMPLETED]` | Orchestrator (v0.2.2) |
| `TASK-MEM-01` | PMM Fast-Path | Replace linear bit scanning with 64-bit word scanning (`__builtin_ctzll` / `tzcnt`) + allocation hint pointer in `pmm.c`. | `PRD-05-resources`, `kernel_optimization_report` | None | `[READY]` | Unassigned |
| `TASK-MEM-02` | Dynamic Bitmap | Relocate PMM bitmap dynamically to physical memory immediately after kernel `_end` instead of fixed `0x20000`. | `PRD-07-boot`, `ADR-001` | None | `[READY]` | Unassigned |
| `TASK-MEM-03` | VMM Higher-Half | Implement Higher-Half Direct Map (HHDM) window (`0xFFFF800000000000`) and 2MB huge page folding in `vmm.c`. | `PRD-02-capabilities`, `auraos_memory_blueprint` | `TASK-MEM-01` | `[READY]` | Unassigned |
| `TASK-MEM-04` | Page Fault ISR | Implement `#PF` (Vector 14) exception handler reading CR2, diagnosing violation code (present, write, user), and panic/recovery. | `PRD-02-capabilities`, `PRD-06-verification` | None | `[COMPLETED]` | Torve (`trove/mimo-v2.5`) + Mira |
| `TASK-MEM-05` | Slab Allocator | Implement Darwin-inspired segregated power-of-2 slab zone allocator (`zalloc` 16B..2048B) replacing raw free-list overhead. | `PRD-05-resources`, `auraos_memory_blueprint` | `TASK-MEM-01` | `[READY]` | Unassigned |

---

### Sprint 3: Preemptive Scheduler, Tasks & Ring-3 (Core Architecture)

| Task ID | Component | Description & Target | Contracts | Dependencies | Status | Assigned Agent |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TASK-SCHED-01` | Context Switch | Create `task_struct`, kernel/user stack frames, and assembly context switch routine (`switch_to(prev, next)`). | `PRD-03-process`, `ABI-001-syscalls` | `TASK-MEM-04` | `[READY]` | Unassigned |
| `TASK-SCHED-02` | Preemption | Hook `pit_callback` in `pit.c` to scheduler preemptive tick to cycle ready queue every 1ms or 10ms quantum. | `PRD-03-process`, `TEL-001` | `TASK-SCHED-01` | `[BLOCKED]` | `TASK-SCHED-01` |
| `TASK-SCHED-03` | Ring-3 Drop | Implement user-mode trampoline using `iretq` with user code (`0x18`) and user data (`0x20`) selectors. | `PRD-02-capabilities`, `ABI-001` | `TASK-SCHED-01` | `[BLOCKED]` | `TASK-SCHED-01` |
| `TASK-SCHED-04` | Syscall Entry | Implement `syscall` / `sysret` MSR (STAR, LSTAR, SFMASK) fast dispatch handling syscall table. | `ABI-001-syscalls` | `TASK-SCHED-03` | `[BLOCKED]` | `TASK-SCHED-03` |

---

### Sprint 4: Telemetry, IPC & Console Services

| Task ID | Component | Description & Target | Contracts | Dependencies | Status | Assigned Agent |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TASK-TEL-01` | Sys Telemetry | Implement `sys_telemetry` snapshot struct population in `sysmon.c` exporting live PID CPU ticks and memory deltas. | `TEL-001`, `TASK_MANAGER_SPEC` | `TASK-SCHED-01` | `[READY]` | Unassigned |
| `TASK-IPC-01` | Lock-Free SPSC | Implement single-producer single-consumer circular ring buffer in shared memory for zero-copy IPC. | `PRD-04-concurrency`, `AUD-001` | `TASK-MEM-01` | `[READY]` | Unassigned |
| `TASK-DRV-01` | VGA Fast Scroll | Optimize `vga_scroll` in `vga.c` with 64-bit word block moves (`uint64_t *`) instead of single-character loops. | `PRD-07-boot`, `kernel_optimization_report` | None | `[READY]` | Unassigned |
| `TASK-DRV-02` | Serial RX Polling| Add non-blocking `serial_getc` checking Line Status Register (LSR) bit 0 (Data Ready) to allow keyboard over COM1. | `PRD-07-boot` | None | `[READY]` | Unassigned |

---

### Sprint 5: Linear Framebuffer, Graphics & Userland Shell

| Task ID | Component | Description & Target | Contracts | Dependencies | Status | Assigned Agent |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `TASK-GFX-01` | GOP Scanout | Initialize 32bpp linear graphics framebuffer from Multiboot2 info tag and render test color gradients & FPS counter. | `DRV-MASTER`, `PRD-11-desktop` | `TASK-MEM-03` | `[BLOCKED]` | `TASK-MEM-03` |
| `TASK-GFX-02` | PSF Font Engine | Implement 8x16 bitmap PSF2 console font renderer over linear framebuffer to replace VGA 80x25 text mode. | `PRD-11-desktop`, `PRD-13-terminal` | `TASK-GFX-01` | `[BLOCKED]` | `TASK-GFX-01` |
| `TASK-SHELL-01` | Initramfs Tar | Parse read-only ustar ramdisk archive loaded as Multiboot2 module and execute first user binary `/bin/init`. | `PRD-08-filesystem-shell` | `TASK-SCHED-03` | `[BLOCKED]` | `TASK-SCHED-03` |

---

## 3. Completed Tasks Ledger

| Task ID | Component | Delivered Output | Completion Date | Git Commit | Verified By |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `TASK-BOOT-01` | Boot & Assembly | Multiboot2 header + 32-to-64 bit Long Mode transition in `entry64.S` | 2026-09-18 | `d8861bc` | Antigravity QA |
| `TASK-BOOT-02` | GDT & IDT | 5-segment GDT, 64-bit TSS, 256-gate IDT, 8259 PIC remapping (0x20/0x28) | 2026-09-19 | `473f848` | Antigravity QA |
| `TASK-BOOT-03` | PIT 8254 Timer | 1000Hz Channel 0 heartbeat timer & 50ms startup interrupt verification | 2026-09-19 | `473f848` | Antigravity QA |
| `TASK-MEM-00` | PMM Baseline | 4KB bit-level physical frame allocator managing up to detected RAM | 2026-09-19 | `473f848` | Antigravity QA |
| `TASK-MEM-00B`| Dynamic Heap | Free-list kernel dynamic memory allocator (`kmalloc`/`kfree`) + tests | 2026-09-19 | `714392b` | Antigravity QA |
| `TASK-UI-01` | VGA & Dashboard | 80x25 text driver + coordinate TUI engine + btop-style monitoring UI | 2026-09-19 | `9154a32` | Antigravity QA |
| `TASK-DOC-01` | Rulebook & Tags | Mandatory `RULEBOOK.md`, `CODE_TAGS.md`, and inline tags on all 36 files | 2026-09-19 | `current` | Antigravity QA |
