---
layout: default
title: Stage 1 Boot Flow
---

# Stage 1 Boot Flow: Multiboot2 → Long Mode → kernel_main

AuraOS boots through four distinct phases before any C code runs. This page traces every step from firmware handoff to the first instruction of `kernel_main`.

---

## Boot Call Graph

```
BIOS / UEFI Firmware
       |
       v
GRUB 2  (reads /boot/auraos.elf, loads to 1MB)
       |
       | EAX = 0x36d76289 (multiboot2 magic)
       | EBX = physical addr of multiboot2 info struct
       | CPU is 32-bit protected mode
       v
boot/multiboot2_header.S   (.multiboot section at 1MB)
       |
       v
boot/entry64.S  (_start, .code32)
       |
       |-- 1. Save multiboot magic + info pointer to BSS
       |-- 2. Set up 16KB stack (stack_top)
       |-- 3. Verify CPUID support (flag bit 21 toggle)
       |-- 4. Verify extended CPUID (0x80000000 >= 0x80000001)
       |-- 5. Verify Long Mode (CPUID 0x80000001, EDX bit 29)
       |-- 6. Build identity page tables:
       |       PML4[0] -> PDPT -> PD (512 x 2MB huge pages = 1GB)
       |-- 7. Load CR3 with PML4 base
       |-- 8. Enable PAE (CR4 bit 5)
       |-- 9. Enable Long Mode (EFER MSR 0xC0000080, bit 8)
       |-- 10. Enable paging + protected mode (CR0: PG|PE)
       |-- 11. Load 64-bit GDT (boot_gdt64: null, code, data)
       |-- 12. Far jump: ljmp $0x08, $long_mode_start
       |
       v
boot/entry64.S  (long_mode_start, .code64)
       |
       |-- Reload DS/ES/FS/GS/SS with 0x10 (data segment)
       |-- Set RSP = stack_top
       |-- RDI = multiboot_info, RSI = multiboot_magic
       |
       v
kernel/main.c  (kernel_main)
       |
       |-- vga_init()                         VGA text console
       |-- serial_init(COM1, 115200)          COM1 UART 8N1
       |-- multiboot2_parse()                 RAM map + framebuffer
       |-- gdt_init()                         5 segments + TSS
       |-- isr_install()                      256 IDT gates, PIC remap
       |-- pit_init()                         1000 Hz heartbeat
       |-- pmm_init()                         64MB bitmap frame allocator
       |-- sti                                Enable interrupts
       |-- Wait 50 PIT ticks (~50ms)          Verify timer delivery
       |
       v
     idle loop:  for (;;) { hlt; }
```

---

## Multiboot2 Header

The header lives in `.multiboot` (placed at 1MB by `linker.ld`) so GRUB finds it by scanning the first 8KB of the loaded ELF:

```asm
.long 0xE85250D6        /* Multiboot2 magic */
.long 0                  /* Architecture: i386 32-bit */
.long header_length       /* Size of this header */
.long -(magic + arch + length)  /* Checksum: must sum to zero */
.short 0                 /* End tag */
.short 0
.long 8
```

Source: `boot/multiboot2_header.S`

---

## Phase 1: 32-bit Protected Mode Entry (`_start`)

GRUB drops us in 32-bit protected mode with paging disabled.

1. **`cli; cld`** — interrupts off, direction flag forward.
2. **Save registers** — `EAX` (magic `0x36d76289`) and `EBX` (mbi pointer) to BSS globals.
3. **Stack setup** — `ESP = stack_top` (16KB in `.bss`).

---

## Phase 2: CPU Capability Checks

Three checks gate entry to Long Mode:

| Check | Method | Failure |
|-------|--------|---------|
| CPUID available | Toggle EFLAGS bit 21 | Hang (`cli; hlt` loop) |
| Extended CPUID | `cpuid(0x80000000)` ≥ `0x80000001` | Hang |
| Long Mode | `cpuid(0x80000001)` EDX bit 29 | Hang |

If any check fails, the CPU enters `.no_long_mode` — an infinite `cli; hlt; jmp` loop. No error is printed (serial/VGA not yet initialized).

---

## Phase 3: Identity Paging Setup

The entry code builds a 1GB identity mapping using 2MB huge pages so virtual addresses equal physical addresses after paging is enabled:

```
PML4[0]  ──>  boot_pdpt         (Present | Writable = 0x03)
PDPT[0]  ──>  boot_pd           (Present | Writable = 0x03)
PD[0..511]    2MB huge pages     (Present | Writable | PS = 0x83)
              VA 0x00000000  to  VA 0x3FFFFFFF  →  same physical
```

This covers 1GB. The kernel, stack, and page tables all live within this window.

---

## Phase 4: Long Mode Activation

The x86_64 mode switch requires setting bits in three control registers in order:

1. **CR3** ← `boot_pml4` (page table root)
2. **CR4.PAE** (bit 5) — required for 64-bit paging
3. **EFER.LME** (bit 8 via MSR `0xC0000080`) — enables Long Mode
4. **CR0.PG | CR0.PE** (bits 31, 0) — activates paging + protected mode

After CR0 write, the CPU is in compatibility mode (32-bit sub-mode of Long Mode). A far jump to the 64-bit code segment completes the switch:

```asm
lgdt (boot_gdt64_ptr)
ljmp $0x08, $long_mode_start
```

---

## Phase 5: 64-bit Entry (`long_mode_start`)

```asm
.code64
long_mode_start:
    movw $0x10, %ax        /* reload all segment regs with data selector */
    movw %ax, %ds / %es / %fs / %gs / %ss
    movq $stack_top, %rsp  /* 64-bit stack pointer */
    movl (multiboot_info), %edi   /* arg1: mbi physical address */
    movl (multiboot_magic), %esi  /* arg2: 0x36d76289 */
    call kernel_main
```

---

## RAM Map Discovery

`kernel_main` calls `multiboot2_parse()` (in `kernel/core/multiboot2.c`) which iterates the Multiboot2 info tag list:

| Tag Type | Field Set | Notes |
|----------|-----------|-------|
| `BASIC_MEMINFO` (type 4) | `total_memory_bytes` | `1MiB + upper_kiB` converted to bytes |
| `MMAP` (type 6) | `available_memory_bytes` | Sums all `MULTIBOOT_MEMORY_AVAILABLE` entries |
| `FRAMEBUFFER` (type 8) | `fb_addr`, `fb_width`, `fb_height`, `fb_pitch`, `fb_bpp`, `has_framebuffer` | GOP linear framebuffer for GUI |

Tags are walked 8-byte aligned. Unknown tags are skipped.

If parsing fails (magic mismatch), kernel_main falls back to a 64MB default for the PMM.

---

## Framebuffer Detection

The Multiboot2 framebuffer tag provides:

- **Base address** (`fb_addr`) — physical address of the linear framebuffer
- **Resolution** — `fb_width` × `fb_height` in pixels
- **Pitch** — bytes per scanline row
- **BPP** — bits per pixel (typically 32 for BGRA)

These values are stored in `boot_info` and available to later stages (AuraDRM/KMS, compositor). The VGA text console (`vga_init()`) is used independently for early boot output.

---

## Boot Memory Map

```
0x00000000 +--------------------------+  IVT / BIOS (do not touch)
           ...
0x00010000 +--------------------------+  PMM Bitmap (2KB)
0x00100000 +--------------------------+  Kernel loaded (1MB)
           |  .multiboot header       |  boot/multiboot2_header.S
           |  .text                   |  Compiled C + ASM
           |  .rodata                 |  String constants
           |  .data                   |  Initialized globals
           |  .bss                    |  Page tables + stack
           +--------------------------+
           |  Free frames (PMM)       |  pmm_alloc_frame()
           ...
0x04000000 +--------------------------+  64MB test config limit
```

---

## Source Files

| File | Role |
|------|------|
| `boot/multiboot2_header.S` | Multiboot2 header (magic, arch, end tag) |
| `boot/entry64.S` | 32→64 mode switch, paging, calls `kernel_main` |
| `kernel/core/multiboot2.c` | Tag iteration, RAM map + framebuffer extraction |
| `kernel/main.c` | Init orchestration, idle loop |
| `kernel/linker.ld` | `.multiboot` at 1MB, section ordering |
