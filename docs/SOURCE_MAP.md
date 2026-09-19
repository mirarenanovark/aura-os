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
       |-> vga_init()         [kernel/drivers/vga.c]           : 80x25 Text console (0xB8000)
       |-> serial_init()      [kernel/arch/x86_shared/serial.c]: COM1 UART 115200 8N1
       |-> multiboot2_parse() [kernel/core/multiboot2.c]       : Parse RAM map & linear GOP framebuffer
       |-> gdt_init()         [kernel/arch/x86_shared/gdt.c]   : 5 segments + TSS (0x08,0x10,0x18,0x20,0x28)
       |-> isr_install()      [kernel/arch/x86_shared/idt.c]   : PIC remap (0x20/0x28) + 256 gates (isr.S)
       |-> pit_init()         [kernel/arch/x86_shared/pit.c]   : PIT 1000Hz (mode 3, divisor 1193)
       |-> pmm_init()         [kernel/core/pmm.c]              : Dynamic bitmap frame allocator
       |-> heap_init()        [kernel/core/heap.c]             : Free-list kernel heap
       |-> sysmon_init()      [kernel/core/sysmon.c]           : Telemetry engine hooked to PIT
       |-> sti                (Enable CPU interrupts)
       |-> Verify Timer       (Awaits 50 ticks ~ 50ms)
       |-> dashboard_render() [kernel/core/dashboard.c]        : Renders btop-style monitoring UI
       '-> Main idle loop     (hlt wait for interrupts, sysmon_set_idle)
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

| File | Layer | Tag Component | Purpose & Contract |
|------|-------|---------------|---------------------|
| `boot/multiboot2_header.S` | Stage 1 | `[AURA_COMPONENT: BOOT_MB2_HEADER]` | 8-byte aligned Multiboot2 header (`0xE85250D6`), architecture 0 (i386 32-bit), ends tag. |
| `boot/entry64.S` | Stage 1 | `[AURA_COMPONENT: BOOT_ENTRY64]` | Protected-mode entry `_start`, CPUID long-mode check, 1GB huge identity paging, jump to 64-bit `kernel_main`. |
| `kernel/linker.ld` | Stage 1 | `[AURA_COMPONENT: LINKER_SCRIPT]` | Kernel layout positioning `.multiboot` at physical 1MB, `.text`, `.rodata`, `.data`, `.bss`. |
| `kernel/include/aura/serial.h` | Stage 1 | `[AURA_COMPONENT: SERIAL_UART16550]` | Polled COM1 16550 UART driver interface at port 0x3F8, `serial_printf`. |
| `kernel/arch/x86_shared/serial.c` | Stage 1 | `[AURA_COMPONENT: SERIAL_UART16550]` | 16550 UART driver: FIFO 14-byte trigger, 8N1 latching via DLAB, synchronous write. |
| `kernel/include/aura/gdt.h` | Stage 1 | `[AURA_COMPONENT: ARCH_GDT]` | GDT descriptor structures, selector constants (0x08, 0x10, 0x18, 0x20, 0x28), 64-bit TSS definition. |
| `kernel/arch/x86_shared/gdt.c` | Stage 1 | `[AURA_COMPONENT: ARCH_GDT]` | Installs 5-entry GDT + 16-byte TSS descriptor and reloads segment registers. |
| `kernel/include/aura/idt.h` | Stage 1 | `[AURA_COMPONENT: ARCH_IDT]` | IDT gate entry structures, interrupt frame `registers_t`, handler registration API. |
| `kernel/arch/x86_shared/idt.c` | Stage 1 | `[AURA_COMPONENT: ARCH_IDT]` | Remaps 8259 PIC (Master: 0x20, Slave: 0x28), populates 256 gates, loads IDTR via `lidt`. |
| `kernel/arch/x86_shared/isr.S` | Stage 1 | `[AURA_COMPONENT: ARCH_ISR_STUBS]` | Low-level x86_64 ISR stubs for 32 exceptions + 16 PIC IRQs. Pushes register frame, calls C dispatcher. |
| `kernel/include/aura/pit.h` | Stage 1 | `[AURA_COMPONENT: ARCH_PIT_8254]` | 1000Hz PIT definitions, tick count accumulator, callback hook. |
| `kernel/arch/x86_shared/pit.c` | Stage 1 | `[AURA_COMPONENT: ARCH_PIT_8254]` | Programs 8254 PIT channel 0 to mode 3 (divisor 1193), dispatches IRQ0 to `pit_callback`. |
| `kernel/include/aura/multiboot2.h` | Stage 1 | `[AURA_COMPONENT: BOOT_MULTIBOOT2]` | Multiboot2 tag definitions, memory map structures, GOP framebuffer info struct. |
| `kernel/core/multiboot2.c` | Stage 1 | `[AURA_COMPONENT: BOOT_MULTIBOOT2]` | Tag-stream parser extracting basic meminfo, physical memory map, and linear GOP framebuffer. |
| `kernel/include/aura/panic.h` | Stage 2 | `[AURA_COMPONENT: CORE_PANIC]` | Kernel panic macro capturing `__FILE__` and `__LINE__`. |
| `kernel/core/panic.c` | Stage 2 | `[AURA_COMPONENT: CORE_PANIC]` | Fatal panic handler: logs file/line to VGA and COM1, disables interrupts, halts CPU. |
| `kernel/include/aura/pmm.h` | Stage 2 | `[AURA_COMPONENT: PMM_BITMAP_ALLOCATOR]` | Physical frame allocator contract: single frame, contiguous DMA frames, memory query API. |
| `kernel/core/pmm.c` | Stage 2 | `[AURA_COMPONENT: PMM_BITMAP_ALLOCATOR]` | 4KB physical frame bitmap manager (2KB covers 64MB RAM). Contiguous allocator for heap. |
| `kernel/include/aura/vmm.h` | Stage 3 | `[AURA_COMPONENT: VMM_4LEVEL_PAGING]` | 4-level paging contract: PML4/PDPT/PD/PT walker, PTE flags (PRESENT, WRITABLE, USER, NX). |
| `kernel/arch/x86_64/vmm.c` | Stage 3 | `[AURA_COMPONENT: VMM_4LEVEL_PAGING]` | Auto-allocates page table levels on demand via PMM; page mapping, unmapping, CR3 reload, invlpg. |
| `kernel/include/aura/heap.h` | Stage 3 | `[AURA_COMPONENT: KERNEL_HEAP_FREELIST]` | Kernel dynamic heap allocator contract (`kmalloc`, `kfree`, `kcalloc`, `krealloc`). |
| `kernel/core/heap.c` | Stage 3 | `[AURA_COMPONENT: KERNEL_HEAP_FREELIST]` | First-fit free-list allocator with block splitting, coalescing, and automatic expansion via `pmm_alloc_contiguous`. |
| `kernel/include/aura/vga.h` | Stage 4 | `[AURA_COMPONENT: DRIVER_VGA_CONSOLE]` | 80x25 text mode driver at MMIO 0xB8000, 16-color attributes, formatted printing. |
| `kernel/drivers/vga.c` | Stage 4 | `[AURA_COMPONENT: DRIVER_VGA_CONSOLE]` | VGA text console: scrolling, line wrapping, cursor tracking, color formatting. |
| `kernel/include/aura/tui.h` | Stage 4 | `[AURA_COMPONENT: DRIVER_TUI_ENGINE]` | Text user interface primitives: coordinate printing, ASCII single/double boxes, progress bars. |
| `kernel/drivers/tui.c` | Stage 4 | `[AURA_COMPONENT: DRIVER_TUI_ENGINE]` | Direct VGA frame drawing engine: `tui_draw_box`, `tui_draw_bar`, string and integer formatting. |
| `kernel/include/aura/sysmon.h` | Stage 4 | `[AURA_COMPONENT: TELEMETRY_SYSMON]` | System telemetry monitor contract: CPU load %, memory statistics, rolling window metrics. |
| `kernel/core/sysmon.c` | Stage 4 | `[AURA_COMPONENT: TELEMETRY_SYSMON]` | PIT 1000Hz callback hook: tracks idle ticks vs busy ticks across 1-second rolling window. |
| `kernel/include/aura/dashboard.h`| Stage 4 | `[AURA_COMPONENT: MONITOR_DASHBOARD]` | System status dashboard contract (`dashboard_init`, `dashboard_render`). |
| `kernel/core/dashboard.c` | Stage 4 | `[AURA_COMPONENT: MONITOR_DASHBOARD]` | btop-style monitoring dashboard: CPU usage box, RAM & Heap gauges, simulated process table. |
| `kernel/main.c` | All | `[AURA_COMPONENT: KERNEL_ORCHESTRATOR]` | Kernel entry point coordinating VGA, Serial, Multiboot2, GDT, IDT, PIT, PMM, Heap, Sysmon, and Dashboard. |
| `boot/iso/boot/grub/grub.cfg` | Stage 1 | - | GRUB2 configuration loading `/boot/auraos.elf` with timeout 0. |
| `Makefile` | Build | - | Builds ELF kernel, bootable ISO via `grub-mkrescue`, runs QEMU, runs host tests. |
| `tests/test_pmm.c` | Test | `GATE-02` | Host unit test validating PMM alloc/free/reuse and contiguous allocation. |
| `tests/test_heap.c` | Test | `GATE-02` | Host unit test validating heap `kmalloc`/`kfree`, boundary splits, coalescing, stress test. |

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
