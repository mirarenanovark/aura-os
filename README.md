# Project Aeris — Ultra-Lightweight Operating System

**Project Aeris** is an ultra-lightweight, high-performance desktop operating system built entirely in freestanding C. It features a beautiful Aero Glass/Frost compositor (Windows 7 Aero meets Apple Frost), VirtIO-GPU hardware acceleration, OpenGL & Vulkan support, and a complete built-in developer ecosystem (TinyCC, MicroPython, SDL2, AI Coding Studio).

## Key Features
- **Dual Architecture:** Independent 32-bit (i686) and 64-bit (x86_64) kernel builds
- **Aero Glass / Frost GUI:** Dual-Kawase real-time blur compositor, 9-slice theming, fully moddable themes
- **VirtIO-GPU Acceleration:** Native 2D scanout + 3D Virgl/Vulkan passthrough (VMware / VirtualBox / QEMU)
- **Extreme Efficiency:** Boots with <32MB RAM idle target; sub-second app launches
- **Onboard Development:** TinyCC compiler, MicroPython runtime, SDL2 game/graphics backend
- **AI Coding Studio:** Integrated AI-assisted IDE for live in-OS development
- **Complete Core Suite:** Notepad, Word Processor, Spreadsheet, Task Manager, System Monitor, Terminal

## Architecture

```
~/mira_dev/OS/
├── boot/          Bootloader configs (Limine / Multiboot2)
├── kernel/        Freestanding C monolithic kernel (arch/i686 + arch/x86_64)
├── drivers/       VirtIO-GPU, PS/2, PCI, AC97/HDA, IDE/AHCI
├── gui/           Window Server, Kawase blur compositor, theme engine
├── libs/          libc, libaeris-ui, SDL2 backend, MicroPython, TCC
├── userland/      Terminal, Notepad, Write, Calc, TaskMgr, BTop, Studio
├── tools/         Build scripts, image generators, VM runners
├── build/         Build outputs
└── docs/          Architecture records & specs
```

## Documentation
- [Master Specification](docs/MASTER_SPEC.md) — Kernel, GPU, graphics stack
- [Application Suite](docs/APPS_SPEC.md) — Core apps & GUI library contracts

## Status
📋 **Planning & Architecture Phase** — Astra consultation pending; kernel bootstrap next.

---
*Built by Mira (miradev) for Rafx.*
