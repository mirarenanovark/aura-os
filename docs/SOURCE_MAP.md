---
layout: default
title: Code Map & Architecture
---

# AuraOS Source Map & System Architecture

**Standard:** Clean. Simple. Small. Fast. Direct. No AI Bloat.

---

## 1. The Grand System Stack (5 Stages)

```
+=============================================================================+
| STAGE 5: USERLAND APPLICATIONS (Unprivileged Ring 3)                        |
|                                                                             |
|   +-------------------+  +-------------------+  +-------------------+       |
|   |   AuraDesk WM     |  |   Sysmon (Task)   |  |   AuraAudio DAW   |       |
|   | (Glass Compositor)|  | (Flat Struct Obs) |  | (Lock-free SPSC)  |       |
|   +---------+---------+  +---------+---------+  +---------+---------+       |
|             |                      |                      |                 |
|             +----------------------+----------------------+                 |
|                                    |  POSIX-like Syscall ABI               |
+====================================+========================================+
| STAGE 4: KERNEL SERVICES & DRIVER MANAGEMENT (Privileged Ring 0)            |
|                                                                             |
|   +-------------------+  +-------------------+  +-------------------+       |
|   |   AuraDRM / KMS   |  |   IPC Fast-Ring   |  |   AuraInit / OOM  |       |
|   | (5 GEM IOCTLs)    |  | (Zero-Copy SHM)   |  | (Resilient Tree)  |       |
|   +-------------------+  +-------------------+  +-------------------+       |
+=============================================================================+
| STAGE 3: MEMORY VIRTUALIZATION & PREEMPTIVE SCHEDULER                       |
|                                                                             |
|   +------------------------------------------+  +-------------------------+ |
|   | VMM: 4-Level Paging (PML4->PDPT->PD->PT) |  | Round-Robin Scheduler   | |
|   |  x86_64 Long Mode / i686 2-Level Paging  |  | (PIT 1000Hz Heartbeat)  | |
|   +------------------------------------------+  +-------------------------+ |
+=============================================================================+
| STAGE 2: PHYSICAL MEMORY & FAULT TRAPS                                      |
|                                                                             |
|   +------------------------------------------+  +-------------------------+ |
|   | PMM: Bit-Level Frame Allocator (4KB)     |  | IDT: 256 Exception/IRQ  | |
|   | (alloc / free / contiguous DMA)          |  | (GDT Flat Kernel Model) | |
|   +------------------------------------------+  +-------------------------+ |
+=============================================================================+
| STAGE 1: HARDWARE ABSTRACTION & BOOTLOADER HANDOFF                          |
|                                                                             |
|   +------------------------------------------+  +-------------------------+ |
|   | Multiboot2 (GRUB / Limine)               |  | 16550 UART COM1 Serial  | |
|   | (Physical Memory Map & GOP Linear FB)    |  | (Kernel Debug Output)   | |
|   +------------------------------------------+  +-------------------------+ |
+=============================================================================+
```

---

## 2. Boot Flow Call Graph (Exactly What Happens on Power-On)

```
BIOS / UEFI Firmware
       |
       v
   GRUB 2 (multiboot2 /boot/auraos.elf)
       |
       | (Passes magic 0x36d76289 in EAX, MB2 info in EBX; 32-bit protected mode)
       v
boot/multiboot2_header.S ──> boot/entry64.S (_start)
                                 |
                                 |- Verify CPUID & Long Mode (0x80000001 bit 29)
                                 |- Set up initial 1GB Identity Paging (PML4, PDPT, PD 2MB huge pages)
                                 |- Enable PAE (CR4.PAE) & Long Mode (EFER.LME)
                                 |- Enable Paging (CR0.PG | CR0.PE)
                                 |- Load temporary 64-bit GDT & far jump to long mode
                                 |
                                 v
kernel/main.c (kernel_main)
       |
       |-> serial_init()   [kernel/arch/x86_shared/serial.c] : COM1 UART 115200 8N1
       |-> gdt_init()      [kernel/arch/x86_shared/gdt.c]    : 5 segments + TSS (0x08,0x10,0x18,0x20,0x28)
       |-> isr_install()   [kernel/arch/x86_shared/idt.c]    : PIC remap (0x20/0x28) + 256 gates (isr.S)
       |-> pit_init()      [kernel/arch/x86_shared/pit.c]    : PIT 1000Hz (mode 3, divisor 1193)
       |-> pmm_init()      [kernel/core/pmm.c]               : 64MB bitmap frame allocator
       |-> sti             (Enable CPU interrupts)
       '-> Main idle loop  (hlt wait for interrupts)
```

---

## 3. Memory Layout Map (Physical Address Space)

```
 0x00000000 +----------------------------------+
            |   Real Mode IVT / BIOS area      |  (do not touch)
 0x000003F8 +----------------------------------+
            |   COM1 UART MMIO registers       |
 0x00010000 +----------------------------------+
            |   PMM Bitmap (2KB per 64MB)      |  <- kernel/core/pmm.c
 0x00100000 +----------------------------------+
            |   KERNEL LOADED HERE (1MB)       |
            |   .multiboot (header, aligned)   |  <- boot/multiboot2_header.S
            |   .text   (executable code)      |  <- compiled kernel
            |   .rodata (constants, strings)   |
            |   .data   (initialized globals)  |
            |   .bss    (zeroed globals)       |
            +----------------------------------+
            |   Free RAM (PMM-managed frames)  |  <- pmm_alloc_frame()
            |   ...                            |
 0x04000000 +----------------------------------+
            |   64MB RAM limit (test config)   |
            +----------------------------------+
```

---

## 4. Component File Map

| File | Purpose |
|------|---------|
| `boot/multiboot2_header.S` | 8-byte aligned Multiboot2 header (magic `0xE85250D6`, arch 0, end tag) in `.multiboot`. |
| `boot/entry64.S` | 32-bit kernel entry `_start`, checks Long Mode support, sets up 1GB boot identity paging, jumps to 64-bit long mode, calls `kernel_main`. |
| `kernel/include/aura/gdt.h` | GDT descriptor structures, segment selector constants, 64-bit TSS definition. |
| `kernel/arch/x86_shared/gdt.c` | Installs 5-entry GDT + TSS and reloads segment registers via inline assembly. |
| `kernel/include/aura/idt.h` | IDT gate entry structures, interrupt frame registers, registration API. |
| `kernel/arch/x86_shared/isr.S` | Low-level assembly stubs for 32 CPU exceptions (with dummy error codes) and 16 PIC IRQs. Saves register state, dispatches to C. |
| `kernel/arch/x86_shared/idt.c` | Remaps 8259 PIC (Master: 0x20, Slave: 0x28), sets up 256 IDT gates, loads IDTR via `lidt`. |
| `kernel/include/aura/pit.h` | 1000Hz PIT driver definitions, tick count and callback API. |
| `kernel/arch/x86_shared/pit.c` | Programs 8254 PIT channel 0 to mode 3 square wave with divisor 1193, handles IRQ0 ticks. |
| `kernel/include/aura/pmm.h` | Physical frame allocator contract: alloc, free, contiguous DMA, stats. |
| `kernel/core/pmm.c` | Bitmap frame allocator: 1 bit per 4KB frame; 64MB managed in 2KB of bitmap. |
| `kernel/include/aura/vmm.h` | 4-level paging contract: PTE flags (PRESENT, WRITABLE, USER, PAT-WC, NX), map/unmap/translate API. |
| `kernel/arch/x86_64/vmm.c` | Page table walker: auto-allocates PDPT/PD/PT levels on demand via PMM; CR3 switch + invlpg. |
| `kernel/main.c` | Kernel main initialization orchestrating serial, GDT, IDT, PIT, PMM, and timer interrupt verification. |
| `kernel/linker.ld` | Positions `.multiboot` at 1MB physical, then `.text`, `.rodata`, `.data`, `.bss`. |
| `boot/iso/boot/grub/grub.cfg` | GRUB2 config loading `/boot/auraos.elf`, zero timeout. |
| `Makefile` | Builds ELF kernel, creates bootable GRUB ISO, runs QEMU, runs host PMM unit tests. |
| `tests/test_pmm.c` | Host unit test simulating 64MB RAM; validates alloc/free/reuse/contiguous allocation. |

---

## 5. Data Flow: Timer Interrupt Tick (Every 1 Millisecond)

```
   PIT Hardware Oscillator (1.193182 MHz / 1193 divisor)
                     |
                     | IRQ0 fires every 1ms
                     v
   +-------------------------------------------+
   | 8259 PIC (remapped to vector 0x20)        |
   +---------------------+---------------------+
                         | pushes interrupt frame
                         v
   +-------------------------------------------+
   | isr.S: irq_stub_32                        |
   |  - pusha (save GP registers)              |
   +---------------------+---------------------+
                         | calls C
                         v
   +-------------------------------------------+
   | idt.c: irq_handler(regs)                  |
   |  - dispatch to registered callback        |
   +---------------------+---------------------+
                         |
                         v
   +-------------------------------------------+
   | pit.c: pit_callback()                     |
   |  - ticks++                                |
   |  - (future) scheduler_preempt()           |
   +---------------------+---------------------+
                         |
                         | EOI (End of Interrupt) to PIC
                         v
   +-------------------------------------------+
   | isr.S: popa / iretq (resume execution)    |
   +-------------------------------------------+
```

---

## 7. Detailed Wiki Pages

| Topic | Document |
|-------|----------|
| Stage 1 Boot Flow | [wiki/04-internals/BOOT-FLOW.md](wiki/04-internals/BOOT-FLOW.md) |
| Panic Handler | [wiki/04-internals/PANIC-HANDLER.md](wiki/04-internals/PANIC-HANDLER.md) |

---

## 6. Educational Walkthrough: Why Each Layer Exists

### Why a PMM (Physical Memory Manager)?
Before any `malloc()` can exist, the OS must know which physical RAM frames are free. The PMM uses a flat bitmap where each bit covers 4KB of RAM — a 64MB machine needs only 2KB of bitmap. This is the foundation everything else allocates from.

### Why a VMM (Virtual Memory Manager)?
Every process gets its own isolated address space via page tables. The VMM maps virtual addresses to physical frames with permission bits (read/write, user/supervisor, write-combine for framebuffers, no-execute for data). On x86_64 this is a 4-level walk: **PML4 → PDPT → PD → PT**.

### Why remap the PIC?
By default, x86 hardware IRQs overlap with CPU exceptions (IRQ0 = INT 8 = Double Fault!). Reprogramming the 8259 PIC moves hardware interrupts to vectors 0x20–0x2F, keeping exceptions 0x00–0x1F clean.

### Why the PIT at 1000Hz?
A fixed 1ms heartbeat drives preemption (fair CPU sharing), animation timing, and audio deadline scheduling. One square-wave timer, divisor 1193, zero complexity.
