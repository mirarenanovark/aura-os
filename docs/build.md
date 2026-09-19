---
layout: default
title: Build & Test
---

# Build, Run & Test Guide

Everything required to compile the kernel, run it in QEMU, and execute the host test suite.

---

## Toolchain Requirements

```
+------------------------------------------------------------------+
|  HOST BUILD ENVIRONMENT                                          |
|                                                                  |
|   gcc (x86_64 + i686 multilib)     -> freestanding C compile     |
|   GNU ld                           -> kernel linking             |
|   grub-mkrescue + xorriso          -> bootable ISO assembly      |
|   qemu-system-x86_64               -> runtime verification       |
|   make                             -> build orchestration        |
+------------------------------------------------------------------+
```

Install on Ubuntu/Debian:

```bash
sudo apt install build-essential nasm grub-pc-bin xorriso \
                 qemu-system-x86 gcc-multilib
```

---

## Build Flow Diagram

```
   kernel/*.c  kernel/arch/**/*.c  boot/*.S
        |
        |  gcc -ffreestanding -mcmodel=kernel -fno-pie
        v
   +----------+     +-------------+     +---------------+
   |  .o files| --> | ld -T       | --> | auraos.elf    |
   | (obj)    |     | linker.ld   |     | (kernel ELF)  |
   +----------+     +-------------+     +-------+-------+
                                                |
                                grub-mkrescue   |
                                        v       v
                                 +---------------------+
                                 |  build/auraos.iso   |
                                 +----------+----------+
                                            |
                                 qemu-system-x86_64
                                            v
                                 +---------------------+
                                 | Serial COM1 output  |
                                 | [OK] boot banner    |
                                 +---------------------+
```

---

## Commands

### Build the kernel + ISO
```bash
make all        # produces build/auraos.elf and build/auraos.iso
```

### Boot in QEMU (serial console mode)
```bash
make run        # equivalent to:
# qemu-system-x86_64 -cdrom build/auraos.iso -nographic -no-reboot
```

### Run host unit tests
```bash
make test       # compiles tests/test_pmm.c against kernel/core/pmm.c
```

### Clean build artifacts
```bash
make clean
```

---

## Expected Verified Boot Output

```
[OK] Multiboot2 magic verified
[OK] GDT loaded (kcode=0x08 kdata=0x10 ucode=0x18 udata=0x20 tss=0x28)
[OK] IDT loaded (256 gates, PIC remapped 0x20/0x28)
[OK] PIT channel 0 configured (divisor 1193)
[OK] PMM ready: 65536 KB free / 65536 KB total
[OK] Timer alive: 100 ticks in ~100ms
```

---

## Test Verification Matrix

| Test Target | Scope | Method | Status |
|---|---|---|---|
| `test_pmm` | Frame alloc/free/contiguous | Host unit test (simulated 64MB) | ✅ PASS |
| Serial COM1 | 16550 UART 115200 8N1 | QEMU `-nographic` | ✅ PASS |
| GDT/TSS | Segment loading | Boot banner `[OK]` | ✅ PASS |
| IDT + PIC | 256 gates, remap 0x20/0x28 | Boot banner `[OK]` | ✅ PASS |
| PIT Timer | 1000Hz interrupt cadence | 100 ticks / ~100ms | ✅ PASS |
| VMM (4-level paging) | PML4/PDPT/PD/PT walk | Compile-verified (runtime test pending Sprint 2) | 🔄 |

---

## Compiler Flags Explained

| Flag | Why we use it |
|---|---|
| `-ffreestanding` | No libc. The kernel provides its own runtime. |
| `-nostdlib -nostartfiles` | No crt0, no hosted startup stubs. |
| `-mcmodel=kernel` | Code addresses above 2GB (higher-half kernel). |
| `-fno-pie` | Kernel is loaded at a fixed 1MB physical address. |
| `-fno-stack-protector` | No `__stack_chk_fail` dependency (we add our own later). |
| `-mno-red-zone` | Interrupts can clobber the 128-byte red zone; mandatory in kernels. |
| `-Wall -Wextra` | Zero-warning policy per the Five Laws. |
