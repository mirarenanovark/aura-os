# AuraOS: Architecture & Master Specification
**Target Directory:** `~/mira_dev/OS/`

---

## 1. System Vision & Architecture
* **Multi-Architecture Support:** Independent builds sharing 90%+ pure C codebase via clean HAL:
  * **x86 32-bit (`i686`):** Vintage Pentium III / Celeron / low-end hardware (<128MB RAM).
  * **x86 64-bit (`x86_64`):** Modern x86-64 hardware, VMware, VirtualBox, QEMU.
  * **ARM 64-bit (`aarch64`):** Raspberry Pi 4/5, Apple Silicon VMs (QEMU/UTM), modern ARM SBCs.
  * **ARM 32-bit (`armv7`):** Retro handhelds (R36S/RK3326), older Raspberry Pi / embedded boards.
* **Core Philosophy:** Minimalist, deterministic freestanding C monolithic kernel + high-throughput IPC + ultra-fast boot.
* **Target Platforms:** VMware Workstation, VirtualBox, QEMU, ARM SBCs, and bare-metal x86/ARM hardware.

---

## 2. Display, Compositor & Aero Glass Aesthetic
* **Theme Specification:** Windows 7 Aero Glass meets modern Apple Frost & Cloud.
* **Compositor Engine:**
  * **Real-time Frosted Glass Blur:** Dual Kawase Downsampling Blur (4x pyramid reduction, 2-pass 5-tap kernel, bilinear upsampling) with SIMD acceleration (NEON on ARM, SSE2/AVX2 on x86).
  * **Aero Details:** 1px specular top highlight, Fresnel depth gradient, 2% noise grain dither, rounded translucent window frames.
  * **9-Slice Theming:** Modular INI theme engine with replaceable bitmap assets for full moddability.
* **GPU & Acceleration:**
  * **VirtIO-GPU (2D & 3D):** Native scanout & resource transfer to host GPU on VirtualBox/VMware/QEMU (both x86 and ARM).
  * **Vulkan & OpenGL:** Direct support via Lavapipe (CPU Vulkan 1.3), TinyGL (lightweight OpenGL 1.1), and Virgl 3D command streams.

---

## 3. Developer & Application Ecosystem
* **Built-in Coding Stack:**
  * **Tiny C Compiler (TCC):** Instant, in-OS compilation of C source code directly to native binaries (x86 & ARM backends).
  * **MicroPython / PyScript:** Ultra-fast embedded Python interpreter consuming <512KB RAM.
  * **SDL2 Port:** Custom SDL2 backend targeting the Aura Window Server shared-memory canvas and input events.
  * **Built-in AI-Assisted IDE / Coding Harness:** Minimal graphical code editor with integrated syntax highlighting and an AI coding helper bridge.
  * **Terminal Emulator:** High-performance hardware/software terminal with ANSI color support and custom bitmap/TrueType fonts.

---

## 4. Directory Structure
```
~/mira_dev/OS/
├── boot/          # Limine (x86_64/aarch64), Multiboot2 (i686), U-Boot (ARM)
├── kernel/        # Core kernel
│   ├── arch/      # i686, x86_64, aarch64, armv7
│   ├── core/      # mm, sched, vfs, sys, ipc (100% shared C)
├── drivers/       # VirtIO-GPU, PL011/16550 UART, PS/2 mouse/keyboard, PCI/MMIO
├── gui/           # Window Server, Dual Kawase blur, 9-slice compositor, theme engine
├── libs/          # Custom libc subset, libaura-ui, SDL2 backend, MicroPython, TCC runtime
├── userland/      # Terminal, Desktop Shell, Sysmon, Notepad, Write, Calc, AI IDE
├── tools/         # Cross-compilation scripts, disk image generator, QEMU/VM runners
├── build/         # Build outputs (.iso, .img, binaries)
└── docs/          # Architecture specs, roadmap, and milestone contracts
```
