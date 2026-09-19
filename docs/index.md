---
layout: default
title: AuraOS Developer Wiki & System Architecture
---

# AuraOS Documentation & Architecture

Welcome to the official **AuraOS** engineering documentation and developer wiki.

* **GitHub Repository:** [mirarenanovark/aura-os](https://github.com/mirarenanovark/aura-os)
* **Core Philosophy:** *Clean. Simple. Small. Fast. Direct. No AI bloat. No overengineering. Proven. Works.*

---

## Developer Wiki Directory

Access the normative architecture, contracts, and component specifications below:

* **[Wiki Master Index (00-index.md)](wiki/00-index.md)** — Entry point, reading order, and rules for human developers and subagents.
* **[Philosophy & Governance (01-governance/philosophy.md)](wiki/01-governance/philosophy.md)** — The 5 engineering laws and conflict resolution rules.
* **[ADR-001: Normative Architecture (02-architecture/ADR-001-normative-architecture.md)](wiki/02-architecture/ADR-001-normative-architecture.md)** — Authoritative system boundaries and tier definitions.

---

## Component PRDs (Product Requirements Documents)

| PRD | Title | Key Specification |
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

## Audits & Strategic References

* **[Astra Adversarial Architecture Audit](ASTRA_FULL_DESIGN_AUDIT.md)** — Red-team review of design holes, ABI traps, and mitigations.
* **[AuraAudio PRD](wiki/04-prds/PRD-12-audio.md)** — PulseEffects-style real-time DSP, pro-audio DAW latency contract.
* **[Task Manager Specification](TASK_MANAGER_SPEC.md)** — Ubuntu-style frosted glass process observatory (`sysmon`).
