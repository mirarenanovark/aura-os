# AuraOS Capsule Compatibility: Astra Consultation Verdict

**Consultant:** Astra (`gpt-6-astra`)  
**Topic:** AuraCapsule architecture, ARM-on-x86 DBT feasibility, Android ABI shims, and vintage Pentium 4 (128-256 MB) reality check.  
**Repository:** https://github.com/mirarenanovark/aura-os

---

## 1. Executive Verdict
> **"Use native ports first, narrowly scoped per-app compatibility capsules second, and ARM emulation only as an optional fallback; modern Subway Surfers on a 128–256 MB Pentium 4 is not a credible performance target."**

---

## 2. Best Architecture: Native-First, Version-Pinned Userland Compatibility

1. **Keep foreign ABIs 100% out of the kernel:**
   - The AuraOS kernel exposes general-purpose primitives (threads, shm, IPC, graphics scanout).
   - Foreign environments run entirely in userland isolated capsule processes with explicit capabilities.
2. **Separate CPU Execution from OS Compatibility:**
   - Same-ISA binaries (`x86` on `x86`, `ARM` on `ARM`) execute natively, needing only ABI and library shims.
   - Cross-ISA binaries (`ARM` on `x86`) need an instruction translator on top of the ABI shim.
3. **App-Specific Over Universal:**
   - Start with one legally obtained, exact version-pinned APK (e.g., an older lightweight ARMv7 build or x86 Android build).
   - Share small loader, ABI, and EGL/GLES bridge components between capsules.
4. **On-Demand Loading Saves Idle Overhead, Not Running Overhead:**
   - Capsules don't bloat idle RAM (0 MB when not in use), but a heavy app still requires whatever RAM it allocates at runtime.

---

## 3. What to Reject (Anti-Overengineering Rules)
- **Do not promise "any program, any OS, any architecture" with low overhead.** Universal emulation contradicts our core philosophy.
- **Do not write a from-scratch optimizing JIT / DBT.** Building a fast ARMv7->i686 JIT is a multi-year project. Use proven engines (QEMU TCG for correctness) or prefer native x86 builds / source ports.
- **Do not assume thread-stripping or arbitrary memory capping works universally:** If a game requires 80 MB of textures and Unity engine allocations, pinning it to 48 MB will crash it.
- **Box64 / FEX are for x86-on-ARM hosts**, not ARM-on-x86.

---

## 4. Realistic Hardware Profiles

| Hardware Target | Same-ISA (x86 Linux/Win32 or ARM APK on ARM) | Cross-ISA (ARM APK on Pentium 4 x86) |
|---|---|---|
| **Pentium 4 (128–256 MB RAM)** | Fast, playable with minimal C shim | **Unsupported for modern heavy APKs.** Target lightweight 2D / retro games or native C ports with downscaled assets. |
| **Modern x86_64 (4 GB+ RAM, GPU)** | Near-native speed | Feasible for lightweight ARM apps via translation + EGL bridge. |
| **AuraOS ARM (R36S / Pi 4)** | **100% Native Bare-Metal execution** of ARM APK native libs (`.so`). | N/A (Already native ARM). |

---

## 5. Philosophy Check & Simplification
- **The Capsule interface (`libaura-capsule`) fits "Clean, Simple, Small, Fast, Direct":**
  - An executable format detector (ELF, PE, APK).
  - A small versioned C capsule interface (init, window surface, input, audio, tick, teardown).
  - A curated list of supported and tested apps.
- A clean **"unsupported on this hardware profile"** error message is vastly better than an overengineered, buggy emulation layer that crashes.
