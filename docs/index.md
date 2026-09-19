---
layout: default
title: AuraOS Developer Wiki & System Architecture
---

# AuraOS Architecture & Developer Wiki

<div style="background: rgba(41, 128, 185, 0.1); border-left: 4px solid #2980b9; padding: 12px 18px; margin-bottom: 25px; border-radius: 4px;">
  <strong>Core Philosophy:</strong> <em>Clean. Simple. Small. Fast. Direct. No AI bloat. No overengineering. Proven. Works.</em><br>
  <strong>Repository:</strong> <a href="https://github.com/mirarenanovark/aura-os">github.com/mirarenanovark/aura-os</a> | <strong>Status:</strong> Specifications & Frozen Contracts Complete
</div>

---

## 🏛️ System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          AuraOS Desktop Workspace                           │
│                                                                             │
│  [Aura Terminal]    [Sysmon Observatory]    [AI Coding Studio]   [Aura DAW] │
│         │                    │                      │                │      │
│         └────────────────────┼──────────────────────┴────────────────┘      │
│                              ▼                                              │
│               ┌───────────────────────────────┐                             │
│               │  libaura-ui (Zero-Overhead)   │                             │
│               └──────────────┬────────────────┘                             │
│                              │ Lock-free Shm Surfaces / SPSC Rings          │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │           Aura Userland Services (Supervised by aura-init)          │   │
│   │                                                                     │   │
│   │   [aura-wm] Glass Compositor         [aura-audiod] Audio Mixer      │   │
│   │   • Dual Kawase 4x Pyramid Blur      • Lock-free SPSC Float32 Rings │   │
│   │   • 9-Slice Aero/Frost Theming       • 10-Band EQ & Dynamic Limiter │   │
│   │   • Dirty-Rect Blitting              • <2.6ms Low-Latency DAW Mode  │   │
│   └──────────────────────────┬──────────────────────────────────────────┘   │
│                              │ Direct Kernel Ring Buffers & MMIO            │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │             AuraOS Freestanding C Monolithic Kernel                 │   │
│   │                                                                     │   │
│   │   [Memory Manager]   [Preemptive Sched]   [VFS / RamFS]   [IPC Engine]│ │
│   │   • Bitmap PMM       • Round-Robin        • Bounded ramfs • Lock-free │ │
│   │   • 4-Level Paging   • Capability Handles • Initramfs     • Syscalls  │ │
│   └──────────────────────────┬──────────────────────────────────────────┘   │
│                              │ Hardware Abstraction Layer (HAL)             │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │       Hardware & Emulators (x86_64, i686, aarch64, armv7)           │   │
│   │       VirtIO-GPU · GOP Linear Framebuffer · AC97/HDA · Serial COM1  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 📑 Complete Developer Wiki Directory

### 1. Governance, Philosophy & ADRs
* 📖 **[Master Wiki Index (`00-index.md`)](wiki/00-index.md)** — Entry point, reading order, and rules for developers and subagents.
* 📜 **[The Five Laws (`01-governance/philosophy.md`)](wiki/01-governance/philosophy.md)** — The strict engineering philosophy and conflict resolution rules.
* 🏛️ **[ADR-001: Normative Architecture (`02-architecture/ADR-001.md`)](wiki/02-architecture/ADR-001-normative-architecture.md)** — Canonical names, tier definitions, and system scope.

### 2. Frozen ABI & Syscall Contracts (`03-contracts/`)
* 🔒 **[ABI-001: Syscall & Wire Conventions](wiki/03-contracts/ABI-001-syscalls.md)** — Register layout, frozen syscall table, and alignment laws.
* 🔒 **[TEL-001: Safe Telemetry Wire ABI](wiki/03-contracts/TEL-001-telemetry.md)** — Zero-overhead task manager contract, PID generation counters, struct layouts.
* 🔒 **[WIN-001: Window Server & Surface Protocol](wiki/03-contracts/WIN-001-window-server.md)** — Double-buffered shared memory canvas and damage tracking.
* 🔒 **[AUD-001: AuraAudio Realtime Protocol](wiki/03-contracts/AUD-001-audio.md)** — Lock-free SPSC float32 rings and zero-allocation realtime rules.

---

### 3. Product Requirements Documents (`04-prds/`)

| ID | Title | Key Technical Scope |
|---|---|---|
| **[PRD-01](wiki/04-prds/PRD-01-governance-support-matrix.md)** | Governance & Support Matrix | Tier 1 (Core) vs Tier 2 (Extensions) |
| **[PRD-02](wiki/04-prds/PRD-02-capabilities-protection.md)** | Capabilities & Protection | Handle tables, address space isolation |
| **[PRD-03](wiki/04-prds/PRD-03-process-lifecycle.md)** | Process Lifecycle & Scheduler | ELF loader, scheduling, thread state |
| **[PRD-04](wiki/04-prds/PRD-04-concurrency-ipc.md)** | Concurrency & IPC | Lock-free rings, message passing |
| **[PRD-05](wiki/04-prds/PRD-05-resources-telemetry.md)** | Resource Quotas & Telemetry | OOM handling, safe wire ABI |
| **[PRD-06](wiki/04-prds/PRD-06-verification.md)** | Verification & Testing | Fuzzing, fault-injection gates |
| **[PRD-07](wiki/04-prds/PRD-07-boot-platform.md)** | Boot & Firmware Handoff | Limine / Multiboot2, serial console |
| **[PRD-08](wiki/04-prds/PRD-08-filesystem-shell.md)** | Initramfs & Native Shell | Read-only boot image, RAMFS |
| **[PRD-09](wiki/04-prds/PRD-09-supervision.md)** | Service Supervision | `aura-init` crash recovery & backoff |
| **[PRD-10](wiki/04-prds/PRD-10-storage.md)** | Block Devices & Storage | Filesystem ownership, durable flushes |
| **[PRD-11](wiki/04-prds/PRD-11-desktop-compositor.md)** | Desktop & Compositor | Dual Kawase blur, `libaura-ui` |
| **[PRD-12](wiki/04-prds/PRD-12-audio.md)** | AuraAudio System | Low-latency mixing, DSP effects graph |
| **[PRD-13](wiki/04-prds/PRD-13-terminal.md)** | Terminal & Console Runtime | ANSI escape parser, UTF-8 policy |
| **[PRD-14](wiki/04-prds/PRD-14-hostile-content.md)** | Hostile-Content Security | Document bounds, image/font safety |
| **[PRD-15](wiki/04-prds/PRD-15-toolchain.md)** | Toolchain & Self-Hosting | TinyCC & MicroPython integration |
| **[PRD-16](wiki/04-prds/PRD-16-network.md)** | Networking & TLS | Minimal stack, secure entropy, AI client |
| **[PRD-17](wiki/04-prds/PRD-17-arch-enablement.md)** | Multi-Architecture Enablement | x86_64, i686, aarch64, armv7 HAL |

---

### 4. Implementation Plans & Verification (`05-plans/` & `06-validation/`)
* 🛠️ **[PLAN-01: Bootloader Handoff & Serial Bringup](wiki/05-plans/PLAN-01-boot-serial.md)** — Limine x86_64, higher-half kernel, UART16550 serial console.
* 🛠️ **[PLAN-02: Memory Paging & Processes](wiki/05-plans/PLAN-02-memory-processes.md)** — Bitmap PMM, 4-level PML4 paging, Ring-3 user mode.
* 🛠️ **[PLAN-03: Initramfs & Native Shell](wiki/05-plans/PLAN-03-ramfs-shell.md)** — VFS, read-only initramfs, writable ramfs, CLI shell.
* 🛠️ **[PLAN-04: Glass Compositor & Desktop](wiki/05-plans/PLAN-04-glass-desktop.md)** — `aura-wm`, Dual Kawase blur, `libaura-ui`, taskbar.
* ✅ **[VAL-001: Test Gates & Evidence Ledger](wiki/06-validation/VAL-001-test-gates.md)** — Automated CI checks, host sanitizer runs, and test records.
