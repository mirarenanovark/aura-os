# AuraOS Boot Infrastructure & Source Map

**Standard:** Clean. Simple. Small. Fast. Direct. No AI Bloat.

---

## 1. Boot Flow Call Graph

```
BIOS / UEFI Firmware
       │
       ▼
   GRUB 2 (multiboot2 /boot/auraos.elf)
       │
       ▼ (Passes magic 0x36d76289 in EAX, MB2 info in EBX; 32-bit protected mode)
boot/multiboot2_header.S ──> boot/entry64.S (_start)
                                  │
                                  ├─ Verify CPUID & Long Mode (0x80000001 bit 29)
                                  ├─ Set up initial 1GB Identity Paging (PML4, PDPT, PD 2MB huge pages)
                                  ├─ Enable PAE (CR4.PAE) & Long Mode (EFER.LME)
                                  ├─ Enable Paging (CR0.PG | CR0.PE)
                                  ├─ Load temporary 64-bit GDT & far jump to long mode
                                  │
                                  ▼
kernel/main.c (kernel_main)
       │
       ├─► serial_init()      [kernel/arch/x86_shared/serial.c] : COM1 UART 115200 8N1
       ├─► gdt_init()         [kernel/arch/x86_shared/gdt.c]    : 5 segments + TSS (0x08, 0x10, 0x18, 0x20, 0x28)
       ├─► isr_install()      [kernel/arch/x86_shared/idt.c]    : PIC remap (0x20/0x28) + 256 IDT gates (isr.S)
       ├─► pit_init()         [kernel/arch/x86_shared/pit.c]    : PIT 1000Hz (mode 3, divisor 1193, IRQ0 handler)
       ├─► pmm_init()         [kernel/core/pmm.c]               : 64MB bitmap frame allocator
       ├─► sti                (Enable CPU interrupts)
       └─► Main idle loop     (hlt wait for interrupts)
```

---

## 2. Component Map

| File | Purpose |
|------|---------|
| `boot/multiboot2_header.S` | 8-byte aligned Multiboot2 header (magic `0xE85250D6`, arch 0, end tag) in `.multiboot`. |
| `boot/entry64.S` | 32-bit kernel entry `_start`, checks Long Mode support, sets up 1GB boot identity paging, jumps to 64-bit long mode, and calls `kernel_main`. |
| `kernel/include/aura/gdt.h` | GDT descriptor structures, segment selector constants (`0x08`, `0x10`, `0x18`, `0x20`, `0x28`), and 64-bit TSS definition. |
| `kernel/arch/x86_shared/gdt.c` | Installs 5-entry GDT + TSS and reloads segment registers via inline assembly. |
| `kernel/include/aura/idt.h` | IDT gate entry structures, interrupt frame registers structure, and registration API. |
| `kernel/arch/x86_shared/isr.S` | Low-level assembly stubs for 32 CPU exceptions (with dummy error code insertion) and 16 PIC IRQs. Saves register state and dispatches to C handlers. |
| `kernel/arch/x86_shared/idt.c` | Remaps 8259 PIC (Master: 0x20, Slave: 0x28), sets up 256 IDT gates, and loads IDTR via `lidt`. |
| `kernel/include/aura/pit.h` | 1000Hz PIT driver definitions and tick count / callback API. |
| `kernel/arch/x86_shared/pit.c` | Programs 8254 PIT channel 0 to mode 3 square wave with divisor 1193, handles IRQ0 ticks. |
| `kernel/main.c` | Kernel main initialization routine orchestrating serial, GDT, IDT, PIT, PMM, and testing timer interrupt delivery. |
| `kernel/linker.ld` | Linker script positioning `.multiboot` section at 1MB physical address, followed by `.text`, `.rodata`, `.data`, `.bss`. |
| `boot/iso/boot/grub/grub.cfg` | GRUB2 configuration file loading `/boot/auraos.elf` with zero timeout. |
| `Makefile` | Builds ELF kernel, creates bootable GRUB ISO via `grub-mkrescue`, runs QEMU test, and runs host PMM unit tests. |
