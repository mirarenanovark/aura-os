---
layout: default
title: System Architecture & Developer Wiki
---

<div class="hero-box">
  <span class="philosophy-chip">Philosophy Law</span>
  <h2 style="margin-top: 0; border: none; padding: 0; font-size: 1.4rem;">Clean. Simple. Small. Fast. Direct.</h2>
  <p style="color: #cbd5e1; font-size: 0.95rem;">
    AuraOS is an ultra-lightweight, from-scratch freestanding C operating system targeting x86_64, i686, aarch64, and armv7 with an Aero Glass & Frost compositing engine, pro-audio DAW latency, and zero runtime bloat.
  </p>
  <div style="margin-top: 14px; font-size: 0.85rem; color: #94a3b8;">
    <strong>Repository:</strong> <a href="https://github.com/mirarenanovark/aura-os" target="_blank">mirarenanovark/aura-os</a> &nbsp;•&nbsp; 
    <strong>Auditor:</strong> Astra (gpt-6-astra) &nbsp;•&nbsp; 
    <strong>Status:</strong> Specifications & Frozen Contracts Complete
  </div>
</div>

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

## 📑 Developer Wiki Quick Jump

### 1. Governance & System Decisions
* 📖 [Wiki Master Index (`00-index.md`)](wiki/00-index.html) — Mandatory reading order, ownership rules, and contribution gates.
* 📜 [The Five Laws (`01-governance/philosophy.md`)](wiki/01-governance/philosophy.html) — Core laws and conflict resolution rules.
* 🏛️ [ADR-001: Architecture Precedence (`02-architecture/ADR-001.md`)](wiki/02-architecture/ADR-001-normative-architecture.html) — Canonical system boundaries and tier definitions.

### 2. Frozen ABI & Syscall Contracts
* 🔒 [ABI-001: Syscalls & Wire Conventions](wiki/03-contracts/ABI-001-syscalls.html) — Register layout and frozen syscall table.
* 🔒 [TEL-001: Safe Telemetry Wire ABI](wiki/03-contracts/TEL-001-telemetry.html) — Zero-overhead task manager contract, PID generation counters.
* 🔒 [WIN-001: Window Server Protocol](wiki/03-contracts/WIN-001-window-server.html) — Double-buffered shared memory canvas and damage tracking.
* 🔒 [AUD-001: AuraAudio Protocol](wiki/03-contracts/AUD-001-audio.html) — Lock-free SPSC float32 rings and realtime bounds.

---

## 📦 Component Requirements (PRDs)

| ID | Title | Key Architectural Scope |
|---|---|---|
| **[PRD-01](wiki/04-prds/PRD-01-governance-support-matrix.html)** | Governance & Support Matrix | Tier 1 (Core) vs Tier 2 (Extensions) |
| **[PRD-02](wiki/04-prds/PRD-02-capabilities-protection.html)** | Capabilities & Protection | Handle tables, address space isolation |
| **[PRD-03](wiki/04-prds/PRD-03-process-lifecycle.html)** | Process Lifecycle & Scheduler | ELF loader, scheduling, thread state |
| **[PRD-04](wiki/04-prds/PRD-04-concurrency-ipc.html)** | Concurrency & IPC | Lock-free rings, message passing |
| **[PRD-05](wiki/04-prds/PRD-05-resources-telemetry.html)** | Resource Quotas & Telemetry | OOM handling, safe wire ABI |
| **[PRD-06](wiki/04-prds/PRD-06-verification.html)** | Verification & Testing | Fuzzing, fault-injection gates |
| **[PRD-07](wiki/04-prds/PRD-07-boot-platform.html)** | Boot & Firmware Handoff | Limine / Multiboot2, serial console |
| **[PRD-08](wiki/04-prds/PRD-08-filesystem-shell.html)** | Initramfs & Native Shell | Read-only boot image, RAMFS |
| **[PRD-09](wiki/04-prds/PRD-09-supervision.html)** | Service Supervision | `aura-init` crash recovery & backoff |
| **[PRD-10](wiki/04-prds/PRD-10-storage.html)** | Block Devices & Storage | Filesystem ownership, durable flushes |
| **[PRD-11](wiki/04-prds/PRD-11-desktop-compositor.html)** | Desktop & Compositor | Dual Kawase blur, `libaura-ui` |
| **[PRD-12](wiki/04-prds/PRD-12-audio.html)** | AuraAudio System | Low-latency mixing, DSP effects graph |
| **[PRD-13](wiki/04-prds/PRD-13-terminal.html)** | Terminal & Console Runtime | ANSI escape parser, UTF-8 policy |
| **[PRD-14](wiki/04-prds/PRD-14-hostile-content.html)** | Hostile-Content Security | Document bounds, image/font safety |
| **[PRD-15](wiki/04-prds/PRD-15-toolchain.html)** | Toolchain & Self-Hosting | TinyCC & MicroPython integration |
| **[PRD-16](wiki/04-prds/PRD-16-network.html)** | Networking & TLS | Minimal stack, secure entropy, AI client |
| **[PRD-17](wiki/04-prds/PRD-17-arch-enablement.html)** | Multi-Architecture Enablement | x86_64, i686, aarch64, armv7 HAL |

---

## 🛠️ Implementation Plans & Verification

* 🛠️ **[PLAN-01: Bootloader Handoff & Serial Bringup](wiki/05-plans/PLAN-01-boot-serial.html)** — Limine x86_64, higher-half kernel, UART16550 serial console.
* 🛠️ **[PLAN-02: Memory Paging & Processes](wiki/05-plans/PLAN-02-memory-processes.html)** — Bitmap PMM, 4-level PML4 paging, Ring-3 user mode.
* 🛠️ **[PLAN-03: Initramfs & Native Shell](wiki/05-plans/PLAN-03-ramfs-shell.html)** — VFS, read-only initramfs, writable ramfs, CLI shell.
* 🛠️ **[PLAN-04: Glass Compositor & Desktop](wiki/05-plans/PLAN-04-glass-desktop.html)** — `aura-wm`, Dual Kawase blur, `libaura-ui`, taskbar.
* ✅ **[VAL-001: Test Gates & Evidence Ledger](wiki/06-validation/VAL-001-test-gates.html)** — Automated CI checks, host sanitizer runs, and test records.
