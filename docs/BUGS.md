---
layout: default
title: Known Issues & Bugs Bulletin Board
---

# 📌 AuraOS Known Issues & Bugs Bulletin Board (`BUG-BOARD-001`)

**Document ID:** `BUG-BOARD-001`  
**Status:** ACTIVE · PUBLIC LEDGER  
**Purpose:** Track active bugs, architectural limitations, edge-case regressions, and resolved issues across the AuraOS codebase. Subagents and contributors can claim any open bug by filing a plan or fixing it alongside standard tasks.

---

## 1. Severity Definitions

* 🔴 **CRITICAL:** Causes triple-fault, system hang, data corruption, or memory leaks on standard execution paths.
* 🟡 **MAJOR:** Subsystem functions incorrectly under specific conditions (e.g. high RAM, fragmented allocations, rapid interrupt frequency).
* 🔵 **MINOR / PERFORMANCE:** Suboptimal efficiency, missing telemetry field, or cosmetic UI anomaly.
* 🟢 **ENHANCEMENT:** Planned architectural hardening or future-proofing.

---

## 2. Active Bug Reports

### 🔴 Critical & Major Bugs

| Bug ID | Severity | Subsystem | Description & Repro | Impact | Root Cause & Remediation | Status | Claimed By |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BUG-001` | 🟡 MAJOR | `kernel/arch/x86_64/vmm.c` | **Direct Physical Pointer Dereference in `vmm_map_page()`**<br>When allocating missing intermediate tables (`pdpt`, `pd`, `pt`), `pmm_alloc_frame()` returns a raw physical address that is directly cast to `uint64_t *`. | Works currently only because memory <1GB is identity mapped in `entry64.S`. When RAM >1GB or higher-half kernel is enabled, page tables crash on `#PF`. | Add Higher-Half Direct Map (HHDM) window or translation offset before dereferencing allocated frames. | `[OPEN]` | Unassigned |
| `BUG-002` | 🟡 MAJOR | `kernel/core/pmm.c` | **Hardcoded Initial PMM Bitmap Physical Base `0x20000`**<br>`main.c` initializes PMM bitmap at fixed address `0x20000`. | On systems with reserved ACPI/EBDA or BIOS tables between 64KB and 1MB, this may overwrite firmware data. | Dynamically position the bitmap immediately after linker symbol `_end` (end of kernel ELF binary). | `[OPEN]` | Unassigned |
| `BUG-003` | 🔵 MINOR | `kernel/core/pmm.c` | **$O(N)$ Linear Search in Frame Allocator**<br>`pmm_alloc_frame()` performs bit-by-bit testing in a loop. | Under heavy allocation churn or on systems with >4GB RAM, frame allocation slows down considerably. | Upgrade to 64-bit word scanning with `__builtin_ctzll` and rotating allocation watermark. | `[OPEN]` | Unassigned |
| `BUG-004` | 🔵 MINOR | `kernel/drivers/vga.c` | **VGA Scroll Nested Loop Inefficiency**<br>`vga_scroll()` copies 24 lines cell-by-cell (`24 * 80 = 1920` individual reads and writes). | Frame stutter when streaming voluminous serial or kernel boot messages to VGA console. | Implement `uint64_t` word-aligned block copy for 8 bytes (4 characters) per instruction. | `[OPEN]` | Unassigned |
| `BUG-005` | 🔵 MINOR | `kernel/arch/x86_shared/serial.c` | **Missing Non-Blocking Serial RX (`serial_getc`)**<br>Serial driver only implements TX output (`serial_putc`, `serial_puts`, `serial_printf`). | Cannot accept keyboard commands or shell inputs over COM1 serial console. | Implement `serial_has_data()` checking LSR bit 0, and non-blocking `serial_getc()`. | `[OPEN]` | Unassigned |

---

## 3. Resolved Bugs & Fixes History

| Bug ID | Severity | Subsystem | Description & Fix Applied | Resolution Date | Fix Commit | Verified By |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `FIX-001` | 🔴 CRITICAL | `kernel/arch/x86_shared/isr.S` | **Missing Dummy Error Code Alignment**<br>Exceptions without CPU error codes caused stack misalignment upon entering C interrupt handlers. | 2026-09-18 | `d8861bc` | Antigravity QA |
| `FIX-002` | 🔴 CRITICAL | `kernel/arch/x86_shared/idt.c` | **8259 PIC Vector Collision with Exceptions 0-15**<br>Default BIOS interrupts overlap with CPU traps (IRQ0 = Double Fault INT 8). Remapped PIC to 0x20/0x28. | 2026-09-19 | `473f848` | Antigravity QA |
| `FIX-003` | 🟡 MAJOR | `kernel/main.c` | **VirtualBox / Headless VM Black Screen**<br>System previously output diagnostics only to serial COM1, leaving VM graphical display black. Integrated VGA 80x25 text mode console and btop-style dashboard. | 2026-09-19 | `9154a32` | Antigravity QA |
| `FIX-004` | 🟡 MAJOR | `kernel/core/heap.c` | **Heap Fragmentation on Mixed Allocations**<br>Early prototype lacked free block coalescing. Added bidirectional boundary-tag coalescing on adjacent blocks in `kfree()`. | 2026-09-19 | `714392b` | Antigravity QA |

---

## 4. How to Report a New Bug (Subagent Guide)

When a subagent or contributor uncovers a defect:
1. Generate an ID formatted as `BUG-<NNN>`.
2. Add a new row to Section 2 detailing:
   - File path and line range
   - Minimal reproduction sequence
   - Severity level
   - Suspected root cause and proposed remediation
3. If you intend to fix it immediately, set the status to `[CLAIMED]` and put your subagent ID in **Claimed By**.
