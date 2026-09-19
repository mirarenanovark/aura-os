# AuraOS Implementation Plan 01: Bootloader Handoff & Serial Bringup

**Plan ID:** `PLAN-01` · **Implements:** `PRD-07-boot-platform.md` · **Phase:** Phase 1

---

## 1. Goal
Boot an `x86_64` ISO in QEMU via Limine bootloader, print diagnostic banners to COM1 serial port, set up GDT/IDT exception traps, and output memory map reservations.

## 2. Work Breakdown

| Task | File Path | Deliverable | Est. Time |
|---|---|---|---|
| **1.1** | `boot/limine.cfg` | Bootloader configuration for 1024x768 framebuffer | 15 min |
| **1.2** | `kernel/arch/x86_64/linker.ld` | Higher-half 64-bit kernel linker script (`-2GB` offset) | 20 min |
| **1.3** | `kernel/arch/x86_64/entry.S` | Assembly entry point conforming to Limine base protocol | 25 min |
| **1.4** | `kernel/drivers/serial/uart16550.c` | COM1 (port 0x3F8) serial driver (poll-driven write) | 20 min |
| **1.5** | `kernel/core/kmain.c` | Kernel C entry, banner print, memory map parsing | 30 min |
| **1.6** | `kernel/arch/x86_64/gdt.c` | Flat 64-bit GDT + TSS setup | 25 min |
| **1.7** | `kernel/arch/x86_64/idt.c` | IDT with 32 CPU exception handlers (page fault, GPF) | 40 min |
| **1.8** | `tools/build_iso.sh` | Bash script building bootable QEMU ISO via `xorriso` | 20 min |

## 3. Verification Criteria
* Run: `qemu-system-x86_64 -cdrom build/aura-x86_64.iso -serial stdio -display none`
* Terminal outputs: `[AURA] Kernel loaded at 0xffffffff80000000 ... OK`
* Inject divide-by-zero: outputs `[CPU EXCEPTION 0x00] Division by zero at RIP=...` and halts without triple-fault.
