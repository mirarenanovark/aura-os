# Stage 1 Boot Flow

## Overview

AuraOS boots through Multiboot2 (GRUB), transitions to 64-bit long mode in `entry64.S`, then calls `kernel_main` to initialize core subsystems.

## ASCII Call Graph

```
BIOS/UEFI Firmware
    │
    ▼
GRUB2 (multiboot2 /boot/auraos.elf)
    │ EAX = 0x36d76289 (magic)
    │ EBX = physical addr of MB2 info struct
    │ CPU is in 32-bit protected mode
    ▼
boot/entry64.S :: _start  [.code32]
    │
    ├─ cli, cld
    ├─ Save EAX/EBX to multiboot_magic / multiboot_info
    ├─ Set stack: esp = stack_top (16KB, .bss)
    │
    ├─ Verify CPUID support (toggle EFLAGS bit 21)
    ├─ Check extended CPUID 0x80000000 >= 0x80000001
    ├─ Check Long Mode bit (0x80000001 EDX bit 29)
    │   └─ If absent: cli; hlt; jmp  (hang)
    │
    ├─ Build 1GB identity paging:
    │   ├─ boot_pml4[0] → boot_pdpt  (Present|Writable)
    │   ├─ boot_pdpt[0] → boot_pd    (Present|Writable)
    │   └─ boot_pd[0..511] = 2MB huge pages (0x83)
    │       Covers 0x0000_0000 → 0x3FFF_FFFF
    │
    ├─ CR3 = boot_pml4
    ├─ CR4.PAE = 1
    ├─ EFER.LME = 1 (MSR 0xC0000080 bit 8)
    ├─ CR0.PG | CR0.PE = 1
    │
    ├─ Load boot GDT64 (null, code64, data64)
    ├─ ljmp 0x08, long_mode_start      [.code64]
    │
    ├─ Reload DS/ES/FS/GS/SS = 0x10
    ├─ RSP = stack_top
    ├─ EDI = multiboot_info  (arg1)
    ├─ ESI = multiboot_magic (arg2)
    │
    ▼
kernel_main(mbi_addr, magic)            [kernel/main.c]
    │
    ├─ vga_init()             VGA text console visible immediately
    ├─ serial_init(COM1)     COM1 UART 115200 8N1
    │
    ├─ multiboot2_parse(magic, mbi_addr, &boot_info)
    │   ├─ Walk MB2 tags
    │   ├─ Memory map tag (type 4) → total/available RAM
    │   └─ Framebuffer tag (type 8) → addr, WxH, bpp, pitch
    │
    ├─ gdt_init()             5 segments + TSS (kcode/kdata/ucode/udata/tss)
    ├─ isr_install()          PIC remap 0x20/0x28, 256 IDT gates
    ├─ pit_init()             PIT channel 0, 1000 Hz (divisor 1193, mode 3)
    ├─ pmm_init(ram_size)     Bitmap allocator over available RAM
    ├─ sti                    Enable CPU interrupts
    │
    ├─ Wait 50 PIT ticks (~50ms) to verify timer IRQ delivery
    │
    └─ while (1) { hlt; }    Idle loop — halt until next interrupt
```

## RAM Map Discovery

GRUB's Multiboot2 info structure (passed in EBX) contains a **Memory Map tag** (type 4).

Each entry:
| Field   | Meaning                        |
|---------|--------------------------------|
| base    | Physical start address         |
| length  | Region size in bytes           |
| type    | 1=available, 3=ACPI reclaimable, others=reserved |

`multiboot2_parse()` walks these tags and sums:
- `total_memory_bytes` — highest address across all usable regions
- `available_memory_bytes` — sum of type-1 regions

This feeds directly into `pmm_init()`, which builds a flat bitmap (1 bit per 4KB frame) starting at `0x20000`.

## Framebuffer Detection

Multiboot2 **Framebuffer tag** (type 8) provides:

| Field          | Description                           |
|----------------|---------------------------------------|
| `fb_addr`      | Physical base of linear framebuffer   |
| `fb_width`     | Horizontal pixels                     |
| `fb_height`    | Vertical pixels                       |
| `fb_bpp`       | Bits per pixel (typically 32 BGRA)    |
| `fb_pitch`     | Bytes per scanline                    |

When present, `boot_info.has_framebuffer = true`. Used by the graphics subsystem for direct pixel rendering.
