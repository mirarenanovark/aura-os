# Project Aeris: Operating System Architecture & Master Specification
**Target Directory:** `~/mira_dev/OS/`

---

## 1. System Vision & Architecture
* **Dual Architecture:** Independent 32-bit (`i686` / `x86`) and 64-bit (`x86_64`) build flavors.
  * **32-bit Flavor:** Maximum compatibility for vintage Pentium III / Celeron / low-end hardware (<128MB RAM).
  * **64-bit Flavor:** Modern long-mode capability, 64-bit registers, and higher memory ceilings.
* **Core Philosophy:** Minimalist, deterministic freestanding C monolithic kernel + high-throughput IPC + ultra-fast boot.
* **Target Platforms:** VMware Workstation, VirtualBox, QEMU, and real bare-metal x86 hardware.

---

## 2. Display, Compositor & Aero Glass Aesthetic
* **Theme Specification:** Windows 7 Aero Glass meets modern Apple Vision/macOS Frost & Cloud.
* **Compositor Engine:**
  * **Real-time Frosted Glass Blur:** Hardware-accelerated / SIMD-optimized **Dual Kawase Downsampling Blur** (4x pyramid reduction, 2-pass 5-tap kernel, bilinear upsampling).
  * **Aero Details:** 1px specular top highlight, Fresnel depth gradient, 2% noise grain dither, rounded translucent window frames.
  * **9-Slice Theming:** Modular XML/INI theme engine with replaceable bitmap assets for full moddability.
* **GPU & Acceleration:**
  * **VirtIO-GPU (2D & 3D):** Native scanout & resource transfer to host GPU on VirtualBox/VMware/QEMU.
  * **Vulkan & OpenGL:** Direct support via Lavapipe (CPU Vulkan 1.3), TinyGL (lightweight OpenGL 1.1), and Virgl 3D command streams.

---

## 3. Developer & Application Ecosystem
* **Built-in Coding Stack:**
  * **Tiny C Compiler (TCC):** Instant, in-OS compilation of C source code directly to binaries.
  * **MicroPython / PyScript (Lightweight Python):** Ultra-fast embedded Python interpreter consuming <512KB RAM.
  * **SDL2 Port:** Custom SDL2 backend targeting the Aeris Window Server shared-memory canvas and input events (enables retro games, emulators, and media players).
  * **Built-in AI-Assisted IDE / Coding Harness:** Minimal graphical code editor with integrated syntax highlighting and an AI coding helper bridge that can write, test, and execute code live inside the OS.
  * **Terminal Emulator:** High-performance hardware/software terminal with ANSI color support and custom bitmap/TrueType fonts.

---

## 4. Directory Structure
```
~/mira_dev/OS/
├── boot/          # Limine & Multiboot2 bootloader configs & stage2 stubs
├── kernel/        # Core kernel (arch/i686, arch/x86_64, mm, sched, vfs, sys)
├── drivers/       # VirtIO-GPU, PS/2 mouse/keyboard, PCI, AC97/HDA audio, IDE/AHCI
├── gui/           # Window Server, Dual Kawase blur, 9-slice compositor, theme engine
├── libs/          # Custom libc/musl, SDL2 backend, MicroPython, TCC runtime
├── userland/      # Terminal, Desktop Shell, File Manager, AI IDE, Media Player
├── tools/         # Cross-compilation scripts, disk image generator, QEMU/VM runners
├── build/         # Build outputs (.iso, .vmdk, binaries)
└── docs/          # Architecture specs, roadmap, and milestone contracts
```
