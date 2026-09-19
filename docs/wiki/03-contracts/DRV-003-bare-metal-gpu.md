# AuraOS Bare-Metal GPU Drivers & Hardware Acceleration Contract

**Contract ID:** `DRV-003` · **Status:** NORMATIVE ARCHITECTURE · **Audited against:** Clean. Simple. Small. Fast. Direct.

---

## 1. The Bare-Metal Hardware Reality

When AuraOS boots on a real physical machine (not a VM), it encounters one of four GPU hardware classes:

1. **Vintage Legacy GPUs (1998–2006):** ATI Rage 128, NVIDIA RIVA TNT/GeForce 2-4, Intel Extreme Graphics 845G/855GM, S3 Trio/Virge.
2. **Classic Intel Integrated (2006–2015):** Intel GMA 950/3100, Intel HD Graphics 2000–5000 (Sandy Bridge, Ivy Bridge, Haswell).
3. **Modern PC GPUs (2016+):** AMD Radeon (GCN / RDNA), Intel Iris Xe / Arc, NVIDIA GTX/RTX.
4. **ARM Mobile / Embedded GPUs:** ARM Mali-G31 / Bifrost (Rockchip RK3326 / R36S, Allwinner, Raspberry Pi VideoCore).

---

## 2. The Complete Bare-Metal GPU Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       AuraOS Userland Window Server                         │
│                                                                             │
│   [libaura-ui 2D Desktop]      [TinyGL 1.1]       [Stripped Mesa 3D]        │
│   (Notepad, Sysmon, Shell)     (Quake, 2.5D)      (Vulkan & OpenGL Games)   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    AuraOS Display Server Scanout Buffer                     │
│                        (Double-Buffered Page-Flip)                          │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
═══════════════════════════════════════╪═══════════════════════════════════════
  KERNEL SPACE                         │ Hardware Mode-Setting & DMA Execution
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│               AuraOS Bare-Metal Kernel Display Driver Subsystem             │
│                                                                             │
│  Class A: Universal Firmware Framebuffer (100% Boot Guarantee)              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ • UEFI GOP (Modern PCs & aarch64) & VESA VBE 2.0/3.0 (BIOS/i686)      │  │
│  │ • Zero driver needed. Firmware provides linear base address (MMIO).   │  │
│  │ • Guarantees immediate 60 FPS 2D desktop on ATI Rage, NVIDIA, Intel.  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                      │                                      │
│  Class B: Native Bare-Metal Drivers (Pure Freestanding C, < 500 lines each) │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 1. Intel Legacy (i810 / i830 / i845 / i915 / GMA 950):                │  │
│  │    • Direct PCI MMIO BAR programming                                  │  │
│  │    • Hardware 2D BitBLT accelerator & hardware cursor                 │  │
│  │    • Hardware double-buffer page-flipping (tear-free)                 │  │
│  │ 2. ATI / AMD Legacy (Mach64 / Rage 128 / Radeon 7000-9000):           │  │
│  │    • Direct 2D GUI engine acceleration registers (ENGINE_CNTL)        │  │
│  │    • Instant hardware line, rectangle fill, and screen-to-screen blit │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                      │                                      │
│  Class C: Modern Hardware Acceleration (via aura-kshim Micro-DRM)           │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 1. Panfrost / Lima: Native ARM Mali-G31 GPU (R36S / RK3326)           │  │
│  │ 2. AMDGPU (Radeon GCN / RDNA): Modern open AMD driver                 │  │
│  │ 3. Intel Iris / Xe: Modern Intel integrated graphics                  │  │
│  │ 4. Nouveau: Basic open NVIDIA kernel mode-setting                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. How Bare-Metal Acceleration Works in AuraOS

### Tier A: The Universal Baseline (UEFI GOP & VESA VBE)
* **How it works:** When AuraOS boots on a machine with no native driver (e.g. a brand new NVIDIA RTX 4090 or a 1998 laptop with an obscure Trident card), the firmware sets the native resolution (1080p, 4K, or 1024x768) and gives AuraOS a direct 32-bit physical linear framebuffer address (`0xE0000000`).
* **Performance:** Writing to video memory uses **Write-Combining (WC) CPU caching** via x86 MTRRs/PAT registers. This allows software 2D blits to run at hundreds of megabytes per second — fast enough for a rock-solid 60 FPS desktop even without a GPU driver.

### Tier B: Native Legacy 2D Accelerators (Pure C, <500 lines)
On vintage chips (Pentium III/4 era), GPU 2D acceleration was simple and clean:
* **Intel Extreme / GMA:** We program the 2D command streamer ring buffer (`RING_BUFFER_TAIL`). The chip copies windows and clears screens in hardware with zero CPU load.
* **ATI Rage 128 / Radeon:** We write to the `SCALE_3D_CNTL` and `DEFAULT_OFFSET` registers for instant hardware window dragging.

### Tier C: Modern Bare-Metal GPUs (`aura-kshim` Micro-DRM)
For modern GPUs requiring complex PLL clock programming, DisplayPort link training, and memory management:
* We compile the isolated Linux DRM KMS mode-setting files (`amdgpu_kms.c`, `intel_display.c`, `panfrost_drm.c`) against our tiny `<1,200-line` C header shim (`aura_linux_shim.h`).
* **Zero Linux kernel in AuraOS.** The shim translates the DRM calls directly into native AuraOS PCI and memory allocation routines.

---

## 4. Hardware Support Matrix

| Hardware Target | Primary 2D Driver | 3D Engine | Maximum RAM Overhead |
|---|---|---|---|
| **Pentium III + ATI Rage / Intel 845** | Native C 2D Engine / VESA VBE | TinyGL (Pure C) | **< 512 KB** |
| **ThinkPad T60 / X200 (Intel GMA)** | Native i915 2D BAR Driver | TinyGL / Mesa Classic | **< 1.5 MB** |
| **R36S / RK3326 Handheld (ARM Mali)** | Panfrost DRM via `aura-kshim` | Mesa Panfrost Gallium | **< 4.0 MB** |
| **Modern AMD/Intel PC (Radeon / Iris)** | AMDGPU / Iris via `aura-kshim` | Stripped Mesa (Radv/Iris) | **< 8.0 MB** |
| **Unknown / Brand New GPU** | UEFI GOP Linear Framebuffer | TinyGL Software Rasterizer | **0 MB (Zero driver)** |
