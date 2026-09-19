# AuraOS GPU Driver Architecture & Lightweight Linux DRM/KMS Porting Strategy

**Topic:** Primary GPU Driver Stack & Micro-DRM/KMS Linux Driver Adaptation  
**Status:** NORMATIVE ARCHITECTURE · **Audited against:** Clean. Simple. Small. Fast. Direct.

---

## 1. What GPU Drivers We Are Using (The 3-Tier Graphics Stack)

To run everywhere from vintage Pentium III chips to modern QEMU/VMware VMs and bare metal without a massive 15-million-line Linux kernel dependency:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          AuraOS Window Server (aura-wm)                     │
│                                                                             │
│   [Software Dual Kawase Blur]      [TinyGL / OpenGL]      [Lavapipe Vulkan] │
└────────────────────────────────────┬────────────────────────────────────────┘
                                     │ Linear Framebuffer / KMS Scanout
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      AuraOS Display Driver Subsystem                        │
│                                                                             │
│   Tier 1: Primary VM Target       Tier 2: Universal Fallback                │
│   ┌───────────────────────────┐   ┌──────────────────────────────────────┐  │
│   │ VirtIO-GPU Driver         │   │ UEFI GOP / VESA VBE Linear FB        │  │
│   │ • PCI 0x1AF4:0x1050       │   │ • 100% universal across all x86/ARM  │  │
│   │ • 2D Scanout & Flush      │   │ • Direct MMIO linear address write   │  │
│   │ • 3D Virgl passthrough    │   │ • Works on real GPUs out of the box  │  │
│   └───────────────────────────┘   └──────────────────────────────────────┘  │
│                                                                             │
│   Tier 3: Lightweight Hardware Acceleration (Micro-DRM Porting Layer)       │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │ aura-kshim (Minimal Linux DRM/KMS C Shim — <1500 lines of C)         │  │
│   │ ├── Intel i915 / GMA (Pentium / Core 2 / Vintage laptops)            │  │
│   │ ├── AMDGPU / Radeon (GCN, RDNA, legacy ATI Rage)                     │  │
│   │ └── Panfrost / Lima (Mali GPU on ARM R36S / RK3326)                  │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Is It Possible to Port Linux-Based Drivers in a Lightweight Manner?

**YES — and the proven, lightweight way to do this is the "Micro-DRM Shim" (as done by FreeBSD, OpenBSD, and SerenityOS).**

### Why You CANNOT Copy the Whole Linux Graphics Stack:
* A modern Linux graphics driver (`amdgpu` or `i915`) pulls in the full Linux kernel tree: `kthread`, `dma-buf`, `mutex`, `workqueue`, `acpi`, `sysfs`, and `debugfs` (>500,000 lines of glue).
* Porting that directly would completely destroy the **"Clean. Simple. Small. Fast."** philosophy.

---

### The Clean AuraOS Solution: `aura-kshim` (Micro-DRM)
Instead of porting Linux, we write a **clean ~1,200-line C header wrapper (`aura_linux_shim.h`)** that translates only the ~25 primitive hardware operations Linux drivers actually use:

| Linux Kernel Primitive | How `aura-kshim` Implements It in AuraOS | Lines of C |
|---|---|---|
| `pci_read_config_dword()` | Direct AuraOS `pci_read32(bus, dev, func, reg)` | 8 lines |
| `ioremap_wc()` (MMIO mapping) | AuraOS `vmm_map_mmio(phys, size, PAGE_WRITE_COMBINING)` | 15 lines |
| `dma_alloc_coherent()` | AuraOS `pmm_alloc_contiguous_dma(pages)` | 20 lines |
| `mutex_lock()` / `spin_lock()` | AuraOS lightweight spinlocks | 12 lines |
| `drm_mode_set_crtc()` | Direct CRTC register programming | Native C |

---

## 3. Why This Approach Wins:

1. **Hardware 2D/3D Mode Setting Without Bloat:**
   * An Intel GMA or AMDGPU driver ported via `aura-kshim` compiles down to **under 80 KB of freestanding C**.
   * It initializes the GPU clocks, sets native 1080p/4K resolution, and configures hardware page-flipping for tear-free 60 FPS.
2. **Zero Linux Kernel Dependencies:**
   * Runs natively inside AuraOS.
   * Compiles with standard GCC, Clang, or in-OS TinyCC (TCC).
3. **ARM Handheld Support (R36S / RK3326):**
   * The Linux `panfrost` / `lima` DRM driver for ARM Mali GPUs can be dropped directly into `drivers/gpu/arm/` using the exact same shim.

---

## 4. Phase 1 to Phase 5 Graphics Rollout Plan

1. **Phase 1 & 2 (Now):** UEFI GOP / VBE Linear Framebuffer (100% universal on VMs and real PCs).
2. **Phase 4:** VirtIO-GPU 2D Driver (`TRANSFER_TO_HOST_2D` + `RESOURCE_FLUSH`) for zero-overhead QEMU/VMware display.
3. **Phase 5+:** `aura-kshim` for Intel i915 (vintage ThinkPads/Pentium 4 laptops) and Mali DRM (R36S ARM).
