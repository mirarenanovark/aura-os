# AuraOS Implementation Plan 02: Capabilities, Paging & Process Lifecycle

**Plan ID:** `PLAN-02` · **Implements:** `PRD-02`, `PRD-03`, `PRD-04` · **Phase:** Phase 2

---

## 1. Goal
Physical page bitmap allocator, 4-level x86_64 paging (PML4), small kernel heap (`kmalloc`/`kfree`), process table with isolated address spaces, and basic static ELF64 executable loader.

## 2. Work Breakdown

| Task | File Path | Deliverable |
|---|---|---|
| **2.1** | `kernel/mm/pmm.c` | Physical Memory Manager: bitmap tracking free physical 4KB frames |
| **2.2** | `kernel/arch/x86_64/vmm.c` | Virtual Memory Manager: map, unmap, protect PML4 page entries |
| **2.3** | `kernel/mm/heap.c` | Kernel slab/buddy allocator for dynamic kernel structs |
| **2.4** | `kernel/core/handle.c` | Per-process Capability Object table (`aura_handle_t`) with rights masks |
| **2.5** | `kernel/sched/sched.c` | Round-robin scheduler driven by PIT/APIC timer tick |
| **2.6** | `kernel/exec/elf.c` | Static ELF64 parser: maps `PT_LOAD` segments to user page table |
| **2.7** | `kernel/arch/x86_64/usermode.S` | Context switch stub jumping to Ring-3 user space |

## 3. Verification Criteria
* Launch an isolated Ring-3 user process that outputs `Hello from Ring 3` via `SYS_FS_WRITE` and cleanly exits via `SYS_EXIT`.
* Attempting to access kernel address `0xffffffff80000000` from user space triggers a `#PF` page fault that terminates the process with zero kernel panic.
