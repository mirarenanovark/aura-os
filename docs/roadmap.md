---
layout: default
title: Roadmap & Milestone Tracker
---

# 🗺️ Engineering Roadmap & Milestone Tracker

**Status:** Phase 1 (Boot & Core Platform) is **COMPLETE & VERIFIED** in QEMU / VirtualBox.  
**Current Phase:** Phase 2 (Virtual Memory, Process Scheduler & Capabilities) is **IN PROGRESS**.  
**Governing Rule:** [`RULEBOOK.md`](RULEBOOK.md) · **Tasks Board:** [`TASK_QUEUE.md`](TASK_QUEUE.md) · **Bugs Board:** [`BUGS.md`](BUGS.md)

---

## 1. Phase Completion Diagram (Live Status)

```
                         AuraOS Engineering Journey
                         ==========================

   [PHASE 1: BOOT & CORE]     [PHASE 2: VMM & SCHED]     [PHASE 3: VFS & SHELL]
   MB2 + GDT + PMM + Heap     HHDM + Context + Ring-3    Initramfs + TCC + CLI
        [##########]               [####......]               [..........]
         100% DONE                  40% DONE                   0% PLANNED
             |                          |                          |
             v                          v                          v
   +-------------------------------------------------------------------+
   |  Dual Console Diagnostics verified via QEMU & VirtualBox:        |
   |  [OK] VGA text mode (0xB8000) & COM1 UART (115200 8N1)            |
   |  [OK] Multiboot2 parsed (RAM map + linear GOP framebuffer)        |
   |  [OK] GDT 5-segment + 64-bit TSS loaded                           |
   |  [OK] IDT 256 gates + PIC remapped (0x20/0x28) + 32 ISR stubs     |
   |  [OK] PIT 8254 channel 0 configured (1000Hz heartbeat)            |
   |  [OK] PMM bitmap frame allocator active (4KB page tracking)       |
   |  [OK] Kernel dynamic heap active (kmalloc/kfree free-list)        |
   |  [OK] Telemetry engine (sysmon) sampling CPU load %               |
   |  [OK] Interactive btop-style monitoring dashboard rendered        |
   +-------------------------------------------------------------------+

   [PHASE 4: GLASS UI]        [PHASE 5: MEDIA & APPS]    [PHASE 6: HARDENING]
   Dual Kawase + Compositor   AuraAudio + VirtIO-GPU     Sysmon GUI + Releases
        [..........]               [..........]               [..........]
         0% PLANNED                 0% PLANNED                 0% PLANNED
```

---

## 2. Sprint-by-Sprint Milestone Breakdown

### Sprint 1: Boot Foundation & Low-Level Architecture ✅ **100% COMPLETED**
- ✅ **Multiboot2 Specification Conformance:** 8-byte aligned header (`0xE85250D6`), architecture 0 (i386 32-bit), tag stream parsing (`boot/multiboot2_header.S`, `kernel/core/multiboot2.c`).
- ✅ **64-Bit Long Mode Trampoline:** CPUID instruction check, long mode bit validation, 1GB 2MB-huge page identity paging, CR4.PAE, EFER.LME, CR0.PG transition (`boot/entry64.S`).
- ✅ **Physical Memory Linker Script:** Kernel positioned at 1MB physical boundary with 4KB segment alignments (`kernel/linker.ld`).
- ✅ **Segmentation & Privilege Boundaries:** Flat 5-segment GDT (`0x08`, `0x10`, `0x18`, `0x20`) plus 16-byte 64-bit TSS descriptor (`0x28`) (`kernel/arch/x86_shared/gdt.c`).
- ✅ **Interrupts & PIC Remapping:** 8259 dual-PIC remapped to vectors 0x20/0x28; 256-gate IDT with 32 CPU exception stubs and 16 hardware IRQs (`kernel/arch/x86_shared/idt.c`, `isr.S`).
- ✅ **1000Hz Timer Heartbeat:** 8254 PIT channel 0 programmed to Mode 3 square-wave with divisor 1193, IRQ0 verified via 50ms startup delay loop (`kernel/arch/x86_shared/pit.c`).
- ✅ **Dual-Output Diagnostic Console:** Synchronous polled COM1 UART 16550 driver (`kernel/arch/x86_shared/serial.c`) paired with 80x25 VGA text mode driver (`kernel/drivers/vga.c`).
- ✅ **Automated Build & Boot Matrix:** `Makefile` with ELF kernel and bootable GRUB ISO generation via `grub-mkrescue`, headless QEMU boot test.

---

### Sprint 2: Memory Subsystem & Kernel Telemetry ✅ **100% COMPLETED**
- ✅ **Physical Memory Manager (PMM):** Bit-level frame allocator managing 4KB pages across physical RAM (`kernel/core/pmm.c`).
- ✅ **Contiguous Physical DMA Allocator:** Contiguous page frame reservation (`pmm_alloc_contiguous`) for kernel heaps, framebuffers, and DMA descriptors.
- ✅ **Dynamic Kernel Heap:** Free-list allocator with 8-byte alignment, boundary-tag block headers, dynamic block splitting (`MIN_SPLIT = 32`), adjacent free coalescing, and automatic PMM expansion (`kernel/core/heap.c`).
- ✅ **Virtual Memory Manager (VMM 4-Level):** Complete PML4 $\rightarrow$ PDPT $\rightarrow$ PD $\rightarrow$ PT walker with demand page allocation, unmap, virtual-to-physical translation, and `invlpg` invalidation (`kernel/arch/x86_64/vmm.c`).
- ✅ **System Telemetry Monitor (`sysmon`):** PIT 1000Hz callback hook measuring idle CPU ticks vs active execution across a 1-second rolling window (`kernel/core/sysmon.c`).
- ✅ **btop-Style System Dashboard:** Coordinate-based text user interface (`kernel/drivers/tui.c`) rendering live CPU load bar, RAM and Heap utilization gauges, and process table on VGA (`kernel/core/dashboard.c`).
- ✅ **Host Unit Test Harness:** Standalone host test suites for PMM (`tests/test_pmm.c`) and Heap (`tests/test_heap.c`) passing in CI/make.
- ✅ **Normative Architecture & Audit:** Darwin/iOS memory analysis report, memory architecture blueprint, and kernel optimization audit report compiled in `docs/devdocs/`.

---

### Sprint 3: Memory Modernization & Process Scheduler 🔄 **IN PROGRESS (40%)**
- ✅ 4-level paging walker baseline (`vmm.c`)
- ⬜ **Fast-Path PMM:** 64-bit word scanning with hardware bit-search (`__builtin_ctzll` / `tzcnt`) + allocation watermark hint (`TASK-MEM-01`).
- ⬜ **Dynamic Bitmap Relocation:** Relocate PMM bitmap immediately after kernel `_end` symbol (`TASK-MEM-02`).
- ⬜ **Higher-Half Direct Map (HHDM):** 64-bit higher-half kernel mapping (`0xFFFF800000000000`) and 2MB huge page folding (`TASK-MEM-03`).
- ⬜ **Page Fault Exception Handler (`#PF` Vector 14):** Read CR2 register, diagnose violation flags, and handle demand paging / panic (`TASK-MEM-04`).
- ⬜ **Darwin-Inspired Slab Allocator (`zalloc`):** Segregated power-of-2 zone cache (16B..2048B) with zero metadata overhead (`TASK-MEM-05`).
- ⬜ **Thread Control Block & Context Switch:** `task_struct`, kernel/user stacks, and assembly context switch `switch_to(prev, next)` (`TASK-SCHED-01`).
- ⬜ **Preemptive Round-Robin Scheduler:** PIT-driven preemption quantum cycling ready threads (`TASK-SCHED-02`).
- ⬜ **Ring-3 User Mode Transition:** Privilege drop trampoline via `iretq` into unprivileged user mode (`TASK-SCHED-03`).
- ⬜ **Syscall ABI Entry:** `syscall` / `sysret` fast MSR dispatch conforming to `ABI-001-syscalls` (`TASK-SCHED-04`).

---

### Sprint 4: VFS, Ramdisk & Interactive Shell ⏳ **UPCOMING**
- ⬜ **Read-Only Boot Initramfs:** Ustar tar archive reader loaded as Multiboot2 module (`TASK-SHELL-01`).
- ⬜ **Virtual File System (VFS):** Mount table, inode abstraction, standard POSIX-like open/read/write/close operations.
- ⬜ **Interactive CLI Shell (`aura-sh`):** Polled keyboard input over serial COM1 and VGA console with line editing and history.
- ⬜ **In-OS TinyCC (TCC) Port:** Self-hosting C compiler compiling native binaries inside AuraOS.
- ⬜ **Lock-Free IPC Fast-Rings:** Single-producer single-consumer circular ring buffer in shared memory (`TASK-IPC-01`).

---

### Sprint 5: Linear Framebuffer, Compositor & Aero Glass UI ⏳ **PLANNED**
- ⬜ **Linear Framebuffer Driver:** Native UEFI GOP / Multiboot2 32bpp linear scanout engine (`TASK-GFX-01`).
- ⬜ **PSF2 Bitmap Font Engine:** 8x16 console glyph rasterizer over linear graphics buffer (`TASK-GFX-02`).
- ⬜ **Userspace Window Server (`aura-wm`):** Shared memory double-buffering with dirty rectangle clipping.
- ⬜ **Dual Kawase Pyramid Blur Engine:** SIMD-accelerated (SSE2/AVX2) 4x downsample/upsample blur shader.
- ⬜ **`libaura-ui` Toolkit:** Zero-overhead widget framework: frosted glass frames, buttons, textboxes, and sliders.

---

### Sprint 6: Native Applications & Release Hardening ⏳ **PLANNED**
- ⬜ **Ubuntu-Style `sysmon` GUI:** Graphical process observatory with real-time SVG-like wave graphs.
- ⬜ **AuraAudio DAW Engine:** Lock-free SPSC pro-audio mixer with sub-2.6ms roundtrip latency.
- ⬜ **MicroPython Runtime:** Lightweight embedded Python scripting engine (<512KB footprint).
- ⬜ **Multi-Architecture Enablement:** i686 32-bit legacy mirror, ARMv7, and AArch64 bring-up.
- ⬜ **Release ISO Matrix:** Bootable hybrid release media for physical hardware, VirtualBox, and QEMU.
