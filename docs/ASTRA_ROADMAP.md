# AuraOS — Master Engineering Roadmap & Architectural Audit
**Consultant:** Astra (`gpt-6-astra`)  
**Target Repository:** https://github.com/mirarenanovark/aura-os  
**Status:** Approved & Verified

---

## 1. Executive Verdict & Core Takeaways
> **Astra Verdict:** *"Feasible as a small C operating system with a software-rendered desktop, provided the legacy and modern graphics profiles differ substantially. Prioritize a reliable kernel, framebuffer desktop, protected user processes, and TCC; defer accelerated 3D and heavyweight compatibility layers."*

### Key Strategic Rules from Astra:
1. **Two Build Profiles (Legacy vs. Modern):**
   * **Legacy Profile (Pentium III / low RAM):** Clean opaque / lightly translucent 9-slice windows, scalar blitting, cached reflections. (Pentium III supports SSE but NOT SSE2).
   * **Modern Profile (x86_64 / VM):** Dual Kawase live pyramid blur + SIMD (SSE2/AVX2) acceleration.
2. **Boot Protocol Normalization:** 
   * Use native **Limine** for `x86_64` and standard **Multiboot2** for `i686`. Both normalize into one unified `boot_info` C structure before kernel init.
3. **90%+ Shared Source HAL Structure:**
   * `kernel/arch/x86/common/`: Shared CPUID, I/O ports, PIC/APIC descriptors.
   * `kernel/arch/x86/i686/` & `kernel/arch/x86/x86_64/`: Pure architecture-specific entry, paging, exception handling, and context switches.
   * `kernel/core/`: 100% architecture-independent memory manager, scheduler, VFS, and IPC.

---

## 2. Six-Phase Engineering Roadmap

```
Phase 1: Boot & Diagnostics  ──►  Phase 2: Memory & Syscalls  ──►  Phase 3: Shell, VFS & TCC
                                                                            │
Phase 6: Release & Apps      ◄──  Phase 5: VirtIO-GPU & Python ◄──  Phase 4: Glass Compositor
```

### Phase 1: Reproducible Boot & Architecture Handoff
* **Deliverables:**
  * Pinned cross-compilation toolchain, linker scripts, and dual `i686` + `x86_64` ISO build pipeline.
  * Limine (`x86_64`) + Multiboot2 (`i686`) handoff normalized into a bounded `struct boot_info`.
  * Early serial output (`COM1`), panic reporter, basic VGA/GOP framebuffer diagnostics, GDT, IDT, and exception handlers.
* **Verification:** Both 32-bit and 64-bit ISOs boot in QEMU/VirtualBox to a clean diagnostic screen and serial log with zero faults.

### Phase 2: Memory, Interrupts & Process Foundation
* **Deliverables:**
  * Physical Memory Manager (Bitmap / Buddy allocator) + Architecture-specific Paging (2-level on i686, 4-level on x86_64).
  * Small, deterministic kernel heap (`kmalloc`/`kfree`).
  * Programmable Interval Timer (PIT) / APIC timer + round-robin preemptive scheduler (single-core initially).
  * Syscall ABI (via `int 0x80` or `syscall`), user address spaces, user stacks, and basic ELF loader.
* **Verification:** Run memory stress tests and spawn/kill isolated userland processes under 32MB RAM. Faulting user programs terminate safely without crashing the kernel.

### Phase 3: Minimal Usable Userland, Storage & TCC
* **Deliverables:**
  * Virtual File System (VFS) + boot-module read-only initramfs + standard file descriptors (`stdin`/`stdout`/`stderr`).
  * Minimal standard C library (`libc` subset) for user processes.
  * Simple interactive CLI Shell (`aura-sh`) with process and file inspection commands.
  * PCI bus enumeration + VirtIO transport layer + PS/2 keyboard/mouse input driver.
  * Port **TinyCC (TCC)** to compile small C programs to ELF binaries live in the shell.
* **Verification:** Boot to shell, write a C program, compile it with in-OS TCC, and run it as an isolated user process.

### Phase 4: Software Desktop & Aero Glass UI Library
* **Deliverables:**
  * User-space Window Server & Compositor mapped to the linear GOP/VBE display framebuffer.
  * `libaura-ui`: Zero-overhead C widget toolkit (windows, buttons, inputs, labels, scrollbars).
  * Shared-memory (`shm_id`) IPC for window canvas updates with dirty-rect clipping.
  * 9-slice procedural theme engine (Windows 7 Aero + Frost skin packs).
  * Optional Dual Kawase pyramid blur pass for modern x86_64 profile.
  * Core desktop shell (taskbar, start menu, desktop background, terminal emulator).
* **Verification:** Interactive 60 FPS window dragging, resizing, overlapping translucent windows, and clean focus management with sub-second launch times.

### Phase 5: VirtIO-GPU Acceleration, MicroPython & Developer Studio
* **Deliverables:**
  * VirtIO-GPU 2D driver (`VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D` + `RESOURCE_FLUSH`) for zero-overhead VM display scanout.
  * Port **MicroPython** (<512KB RAM runtime) with `libaura-ui` Python bindings.
  * **AI Coding Studio (`studio`):** Lightweight graphical IDE combining code editing, TCC compiler, MicroPython runtime, and interactive terminal pane.
  * Custom SDL2 backend driver for games and multimedia.
* **Verification:** Run graphical Python scripts, compile C GUI apps from within the Studio IDE, and verify smooth VirtIO-GPU frame flushes in QEMU/VirtualBox.

### Phase 6: Application Suite, Hardening & Release Qualification
* **Deliverables:**
  * Complete Core Suite:
    * `notepad`: Fast text editor.
    * `write`: Rich-text word processor.
    * `calc`: High-performance spreadsheet.
    * `sysmon`: Ubuntu-style frosted task manager (CPU/RAM/Disk live graphs).
    * `btop`: Graphical system dashboard.
  * Memory hardening, write-xor-execute protections, and allocation failure handling.
  * Dual-flavor reproducible release ISOs and VMDK images.
* **Verification:** Clean cold-boot stress tests on constrained RAM (32MB–64MB) and prolonged multi-window desktop workloads.
