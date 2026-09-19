# AuraOS Code Tags & Subsystem Cross-Reference Guide

**Document ID:** `CODE-TAGS-001`  
**Purpose:** Quick navigation and architectural tracing connecting every line of code to its parent layer, contract, caller, and execution flow.  
**Rule Reference:** Governed by [`docs/RULEBOOK.md`](file:///f:/devlounge/AuraOS/docs/RULEBOOK.md)

---

## 1. Quick Grep Cheat-Sheet

To trace how components link together across the AuraOS tree, run these simple terminal searches:

```bash
# Find all files belonging to a specific stack layer:
grep -rn "\[AURA_LAYER: STAGE_1" .
grep -rn "\[AURA_LAYER: STAGE_2" .
grep -rn "\[AURA_LAYER: STAGE_3" .
grep -rn "\[AURA_LAYER: STAGE_4" .

# Find what connects to a specific subsystem:
grep -rn "\[AURA_CONNECTS: .*SYSMON" .
grep -rn "\[AURA_CONNECTS: .*PMM" .
grep -rn "\[AURA_CONNECTS: .*HEAP" .
grep -rn "\[AURA_CONNECTS: .*VGA" .

# Trace the complete boot and interrupt execution flows:
grep -rn "\[AURA_FLOW: BOOT_ENTRY\]" .
grep -rn "\[AURA_FLOW: KERNEL_INIT\]" .
grep -rn "\[AURA_FLOW: TIMER_TICK\]" .
grep -rn "\[AURA_FLOW: HEAP_GROW\]" .
```

---

## 2. Master Component Tag Index

| Component Tag | Layer | Primary Source Files | Primary Contracts | Upstream Connection | Downstream Connection |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `[AURA_COMPONENT: BOOT_MB2_HEADER]` | Stage 1 | `boot/multiboot2_header.S` | Multiboot2 Spec | GRUB / Limine | `boot/entry64.S` |
| `[AURA_COMPONENT: BOOT_ENTRY64]` | Stage 1 | `boot/entry64.S` | `PRD-07-boot-platform` | BIOS/GRUB | `kernel/main.c:kernel_main` |
| `[AURA_COMPONENT: LINKER_SCRIPT]` | Stage 1 | `kernel/linker.ld` | ADR-001 | GCC / LD | Physical memory map (1MB+) |
| `[AURA_COMPONENT: SERIAL_UART16550]`| Stage 1 | `kernel/arch/x86_shared/serial.c`, `serial.h` | `PRD-07-boot-platform` | `kernel_main`, `panic` | Hardware COM1 (Port 0x3F8) |
| `[AURA_COMPONENT: ARCH_GDT]` | Stage 1 | `kernel/arch/x86_shared/gdt.c`, `gdt.h` | `PRD-02-capabilities` | `kernel_main` | CPU GDTR & Ring 0/3 selectors |
| `[AURA_COMPONENT: ARCH_IDT]` | Stage 1 | `kernel/arch/x86_shared/idt.c`, `idt.h` | `PRD-07-boot-platform` | `kernel_main` | CPU IDTR & 8259 PIC (0x20/0x28) |
| `[AURA_COMPONENT: ARCH_ISR_STUBS]` | Stage 1 | `kernel/arch/x86_shared/isr.S` | `PRD-07-boot-platform` | CPU Interrupt Vector | `idt.c:isr_handler` / `irq_handler` |
| `[AURA_COMPONENT: ARCH_PIT_8254]` | Stage 1 | `kernel/arch/x86_shared/pit.c`, `pit.h` | `PRD-07-boot-platform` | `kernel_main`, IDT IRQ0 | `sysmon.c:sysmon_tick` |
| `[AURA_COMPONENT: BOOT_MULTIBOOT2]` | Stage 1 | `kernel/core/multiboot2.c`, `multiboot2.h`| `PRD-07-boot-platform` | `kernel_main` | PMM RAM map & VGA/GOP linear FB |
| `[AURA_COMPONENT: CORE_PANIC]` | Stage 2 | `kernel/core/panic.c`, `panic.h` | `PRD-06-verification` | Kernel Assertions | VGA + COM1 halt sequence |
| `[AURA_COMPONENT: PMM_BITMAP_ALLOCATOR]` | Stage 2 | `kernel/core/pmm.c`, `pmm.h` | `PRD-05-resources` | `kernel_main`, Heap, VMM | Physical RAM frames (4KB) |
| `[AURA_COMPONENT: KERNEL_HEAP_FREELIST]` | Stage 3 | `kernel/core/heap.c`, `heap.h` | `PRD-05-resources` | Kernel allocations | `pmm.c:pmm_alloc_contiguous` |
| `[AURA_COMPONENT: VMM_4LEVEL_PAGING]` | Stage 3 | `kernel/arch/x86_64/vmm.c`, `vmm.h` | `PRD-02-capabilities` | Kernel paging setup | `pmm.c:pmm_alloc_frame`, CR3 |
| `[AURA_COMPONENT: DRIVER_VGA_CONSOLE]` | Stage 4 | `kernel/drivers/vga.c`, `vga.h` | `PRD-07-boot-platform` | `kernel_main`, TUI, Panic | MMIO 0xB8000 (80x25 text buffer) |
| `[AURA_COMPONENT: DRIVER_TUI_ENGINE]` | Stage 4 | `kernel/drivers/tui.c`, `tui.h` | `PRD-11-desktop` | `dashboard.c` | `vga.c:VGA_MEMORY` |
| `[AURA_COMPONENT: TELEMETRY_SYSMON]` | Stage 4 | `kernel/core/sysmon.c`, `sysmon.h` | `TEL-001`, `PRD-05` | PIT callback (1000Hz) | `dashboard.c:draw_cpu_box` |
| `[AURA_COMPONENT: MONITOR_DASHBOARD]` | Stage 4 | `kernel/core/dashboard.c`, `dashboard.h`| `TEL-001` | `kernel_main` idle loop | TUI, Sysmon, PMM, Heap |
| `[AURA_COMPONENT: KERNEL_ORCHESTRATOR]`| All | `kernel/main.c` | Master Spec | `boot/entry64.S` | Dispatches all initializations |

---

## 3. End-to-End Tracing Call Graphs

### 3.1 Power-On Boot Flow (`[AURA_FLOW: BOOT_ENTRY]` -> `[AURA_FLOW: KERNEL_INIT]`)

```
[GRUB/Limine 32-bit Multiboot2]
      │
      ▼
boot/multiboot2_header.S   // [AURA_COMPONENT: BOOT_MB2_HEADER]
      │
      ▼
boot/entry64.S             // [AURA_COMPONENT: BOOT_ENTRY64]
      ├── [AURA_FLOW: BOOT_ENTRY] Step 1: Check CPUID & Long Mode (EDX bit 29)
      ├── [AURA_FLOW: BOOT_ENTRY] Step 2: Build 1GB 2MB-Huge Identity Paging (PML4, PDPT, PD)
      ├── [AURA_FLOW: BOOT_ENTRY] Step 3: Enable CR4.PAE, EFER.LME, CR0.PG
      ├── [AURA_FLOW: BOOT_ENTRY] Step 4: Load 64-bit GDT & Far Jump into Long Mode
      └── [AURA_FLOW: BOOT_ENTRY] Step 5: Setup 64-bit stack, pass MBI and jump to kernel_main()
            │
            ▼
kernel/main.c              // [AURA_COMPONENT: KERNEL_ORCHESTRATOR]
      ├── [AURA_FLOW: KERNEL_INIT] 1. vga_init()             ──> [AURA_COMPONENT: DRIVER_VGA_CONSOLE]
      ├── [AURA_FLOW: KERNEL_INIT] 2. serial_init(COM1)      ──> [AURA_COMPONENT: SERIAL_UART16550]
      ├── [AURA_FLOW: KERNEL_INIT] 3. multiboot2_parse()     ──> [AURA_COMPONENT: BOOT_MULTIBOOT2]
      ├── [AURA_FLOW: KERNEL_INIT] 4. gdt_init()             ──> [AURA_COMPONENT: ARCH_GDT]
      ├── [AURA_FLOW: KERNEL_INIT] 5. isr_install()          ──> [AURA_COMPONENT: ARCH_IDT] (PIC 0x20/0x28)
      ├── [AURA_FLOW: KERNEL_INIT] 6. pit_init()             ──> [AURA_COMPONENT: ARCH_PIT_8254] (1000Hz)
      ├── [AURA_FLOW: KERNEL_INIT] 7. pmm_init()             ──> [AURA_COMPONENT: PMM_BITMAP_ALLOCATOR]
      ├── [AURA_FLOW: KERNEL_INIT] 8. heap_init()            ──> [AURA_COMPONENT: KERNEL_HEAP_FREELIST]
      ├── [AURA_FLOW: KERNEL_INIT] 9. sysmon_init()          ──> [AURA_COMPONENT: TELEMETRY_SYSMON]
      ├── [AURA_FLOW: KERNEL_INIT] 10. Enable IRQs ("sti")
      ├── [AURA_FLOW: KERNEL_INIT] 11. Verify 50ms timer delivery
      └── [AURA_FLOW: KERNEL_INIT] 12. dashboard_render()    ──> [AURA_COMPONENT: MONITOR_DASHBOARD]
            │
            ▼
kernel/main.c (Idle Loop)
      └── while(1) { sysmon_set_idle(); __asm__("hlt"); }
```

### 3.2 1ms Timer & Telemetry Flow (`[AURA_FLOW: TIMER_TICK]`)

```
Hardware PIT Oscillator (Channel 0 @ 1.193182 MHz)
      │
      ▼ Fires IRQ0 every 1.000 ms
8259 PIC (Remapped to Vector 0x20)
      │
      ▼ CPU executes INT 32 gate
kernel/arch/x86_shared/isr.S : irq32   // [AURA_COMPONENT: ARCH_ISR_STUBS]
      │ Pushes dummy error code & int_no 32
      ▼
kernel/arch/x86_shared/idt.c : irq_handler()
      │ Looks up interrupt_handlers[32]
      ▼
kernel/arch/x86_shared/pit.c : pit_irq_handler()  // [AURA_COMPONENT: ARCH_PIT_8254]
      ├── pit_ticks++
      └── Invokes pit_callback()
            │
            ▼
kernel/core/sysmon.c : sysmon_tick()              // [AURA_COMPONENT: TELEMETRY_SYSMON]
      ├── Accumulates idle_ticks & rolling 1-second sample window (1000 ticks)
      ├── Calculates CPU load percentage
      └── Latches stats for dashboard_render()
            │
            ▼
kernel/arch/x86_shared/idt.c : outb(0x20, 0x20) (Sends EOI to PIC)
      │
      ▼
kernel/arch/x86_shared/isr.S (pop registers, iretq to resume interrupted code)
```

### 3.3 Dynamic Memory Allocation Flow (`[AURA_FLOW: HEAP_ALLOC]`)

```
Consumer code invokes kmalloc(size)
      │
      ▼
kernel/core/heap.c : kmalloc()        // [AURA_COMPONENT: KERNEL_HEAP_FREELIST]
      ├── Aligns size to 8-byte boundary
      ├── Searches free_list for first block >= (size + header)
      ├── If suitable free block found:
      │     └── Splitted if remainder >= 32 bytes, marks used, returns payload
      │
      └── If NO suitable block found:
            └── Invokes grow_heap(needed_size)
                  │
                  ▼
            kernel/core/pmm.c : pmm_alloc_contiguous(pages)  // [AURA_COMPONENT: PMM_BITMAP_ALLOCATOR]
                  ├── Finds contiguous run of 4KB frames in bitmap
                  ├── Marks bits used in bitmap, decrements free frame count
                  └── Returns physical address to heap.c
            Creates new block header, merges with adjacent free blocks, returns payload
```
