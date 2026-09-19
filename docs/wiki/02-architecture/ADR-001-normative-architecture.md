# ADR-001: Normative Architecture, Precedence & Gated Scope
**Status:** ACCEPTED · **Date:** 2026-09-18 · **Author:** miradev · **Auditor:** Astra

## Context
Previous iterations produced scattered design notes (`MASTER_SPEC.md`, `APPS_SPEC.md`, `FAULT_CONTAINMENT_SPEC.md`, `TASK_MANAGER_SPEC.md`). Astra's adversarial review identified overlapping names, unconstrained memory budgets, and unsupported compatibility claims.

## Decision

1. **Canonical Identity:**
   * Operating System: **AuraOS**
   * Core Kernel: Monolithic freestanding C with strict HAL separation.
   * UI Toolkit: **`libaura-ui`**
   * Audio Engine: **`AuraAudio`** (`aura-audiod` + `libaura-audio`)
   * Process Observatory: **`sysmon`** (GUI) + **`auratop`** (CLI)

2. **Gated Scope & Support Tiers:**
   * **Tier 1 (Guaranteed Core):** x86_64 and i686 builds, protected processes, capability handles, linear framebuffer compositor, TCC, MicroPython, terminal, sysmon.
   * **Tier 2 (Gated Extensions):** aarch64/armv7 ports, userland Capsule runners, Dual Kawase blur SIMD acceleration.
   * **Explicit Anti-Goal:** No universal "runs any APK on any CPU" claim without an approved app-specific capsule.

3. **Precedence Hierarchy:**
   `ADRs` (Architecture Decision Records) > `Frozen Contracts` (`docs/wiki/03-contracts/`) > `PRDs` (`docs/wiki/04-prds/`) > `Plans` (`docs/wiki/05-plans/`).
   Any unapproved divergence in implementation blocks CI merge.
