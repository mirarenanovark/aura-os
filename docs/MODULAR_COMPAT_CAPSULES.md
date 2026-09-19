# AuraOS Modular Compatibility Engine: "AuraCapsule" (or "AuraVisor")

A modular, on-demand runtime loading system that enables foreign binaries (Android APKs, Windows EXEs, Linux ELFs) to execute seamlessly on AuraOS while adhering strictly to:
**Clean. Simple. Small. Fast. Direct. No bloat. Proven.**

---

## 1. Core Architecture: The Capsule Model

Instead of baking heavy emulators or monolithic runtimes into the kernel or base OS, AuraOS introduces **Capsules (`.capsule` / `.aura-mod`)**: lightweight, dynamically loaded execution engines that attach to a target binary at launch.

```
┌─────────────────────────────────────────────────────────────┐
│                      AuraOS Desktop Shell                   │
│                                                             │
│   User double-clicks "SubwaySurfers.apk"                    │
│   (or launches via CLI: `aura-run SubwaySurfers.apk`)       │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
            ┌────────────────────────────────────┐
            │       Aura Capsule Resolver        │
            │   Inspects magic bytes / format    │
            └──────────────────┬─────────────────┘
                               │
            ┌──────────────────┴──────────────────┐
            ▼                                     ▼
  [Path 1: Universal Runner]            [Path 2: App-Specific Capsule]
  - General Android Capsule             - `subway-surfers.capsule`
  - Loads default translator            - Contains tailor-made patches,
  - Generic touch-to-key map              pre-configured input mappings,
  - Standard GL ES bridge                 fixed-res framebuffer, sound tweaks
            │                                     │
            └──────────────────┬──────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 AuraVisor Runtime Layer                     │
│                                                             │
│   [ISA Translation (if needed)]                             │
│   • ARM on x86: Fast JIT / Dynamic Binary Translator        │
│     (e.g., libndk_translation / FEX / Box86-style JIT)      │
│   • Same-ISA (ARM on ARM or x86 on x86): 100% Native Bare-Metal│
│                                                             │
│   [OS / Framework Emulation]                                │
│   • Android NDK / Bionic C runtime shim                     │
│   • Fake minimal Android surfaceview -> Window Server       │
│   • Audio HAL -> AuraOS Audio Mixer (PCM ring buffer)       │
│                                                             │
│   [Graphics Scanout]                                        │
│   • OpenGL ES / EGL commands pass directly to GPU / TinyGL  │
│   • Output blits into a native frosted glass window         │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. The Two Execution Pathways

### Pathway 1: The Universal Profile Loader (`aura-runner`)
* **How it works:**
  * User points AuraOS to any generic foreign file (`.apk`, `.exe`).
  * The generic loader reads a lightweight profile (`default.profile`) specifying:
    * Execution mode: Native direct vs. Dynamic Binary Translation (DBT).
    * Display resolution & aspect ratio.
    * Input bindings (e.g. keyboard arrow keys mapped to screen swipes).
* **Target Audience:** Quick launch of casual software without custom packaging.

### Pathway 2: App-Specific Modules (Tailored Soft-Ports)
* **How it works:**
  * An app developer or community member packages an APK/binary with an optimized C glue module.
  * Uses a clean template provided by AuraOS: `capsule_template.c`.
  * The module overrides specific bottlenecks:
    * Bypasses Google Play Services / telemetry completely (zero network waste).
    * Locks memory pools to stay strictly within a constrained RAM budget (e.g., 64MB-128MB on Pentium 4).
    * Maps game controls directly to gamepad or keyboard/mouse.
    * Can be distributed as a single self-contained bundle: `game.aura` containing the APK + the tailor-made runner.

---

## 3. Hardware Reality on Vintage vs. Modern CPUs

| Platform | Same-Arch (e.g. ARM APK on ARM, Win32 on x86) | Cross-Arch (ARM APK on x86 Pentium 4) |
|---|---|---|
| **CPU Execution** | **100% Native Speed** (Zero translation penalty) | **Dynamic Binary Translation (DBT)** |
| **Memory Cost** | <16MB runtime overhead | ~24MB-32MB runtime overhead |
| **Feasibility on P4 (128-256MB)** | Extreme speed (Win32 / x86 Linux) | Playable for 2D / lightweight 3D games with static recompilation / fast block JIT |

---

## 4. Valve's Paradigm (Proton / FEX / Waydroid model)
Valve proved with Proton and ARM translation projects (like FEX-Emu / Waydroid bridges) that:
1. You do **not** run an entire guest operating system (no heavyweight VM, no Android boot sequence, no background bloatware).
2. You only translate the **ABI boundary** (the system calls and library calls the app actually touches).
3. The app thinks it is on Android or Windows, but its draw calls go straight to the native GPU driver and its pixels render into the native AuraOS window.

---

## 5. Clean & Simple Repository Structure

All compatibility logic remains strictly decoupled from the core kernel:

```
~/mira_dev/OS/
├── kernel/core/            # 100% untouched pure C monolithic kernel
├── userland/
│   └── aura-run/           # Universal capsule loader CLI & GUI wrapper
└── capsules/               # Modular add-on templates & runtimes
    ├── templates/          # Simple C templates for community wrappers
    │   └── app_capsule.c   # Clean hook template: init, input, audio, render
    ├── android/            # Android APK loader + minimal Bionic/EGL shim
    ├── windows/            # Win32 PE loader + minimal User32/GDI shim
    └── profiles/           # INI configuration profiles for known apps
```
