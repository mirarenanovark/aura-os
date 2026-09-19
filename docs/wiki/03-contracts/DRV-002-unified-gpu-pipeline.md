# AuraOS Unified GPU & Graphics Pipeline Specification

**Contract ID:** `DRV-002` · **Status:** NORMATIVE ARCHITECTURE · **Audited against:** Clean. Simple. Small. Fast. Direct.

---

## 1. The Complete End-to-End Pipeline Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. APPLICATION LAYER                                                        │
│                                                                             │
│   [2D Desktop & Core Apps]        [Retro / 2.5D Games]     [Modern 3D Games]│
│   (Notepad, Sysmon, Terminal)     (Quake, Emulators)       (Blender, Vulkan)│
│               │                            │                       │        │
│               ▼                            ▼                       ▼        │
│        [libaura-ui 2D]                [TinyGL 1.1]         [Stripped Mesa]  │
│        (Software Blit)                (Pure C, <256KB)     (virgl/panfrost) │
└───────────────┬────────────────────────────┬───────────────────────┬────────┘
                │                            │                       │
                │ Shared-Memory Backbuffer   │ Software Pixels       │ GEM DMA
                ▼                            ▼                       │ Buffers
┌────────────────────────────────────────────────────────────┐       │
│ 2. WINDOW SERVER & COMPOSITOR (`aura-wm`)                  │       │
│                                                            │       │
│   • Multi-Window Z-Ordering & Focus                        │       │
│   • Dual Kawase 4x Pyramid Glass Blur (SSE2/NEON)          │       │
│   • 9-Slice Aero/Frost Frame Borders & Specular Highlight  │       │
│   • Bounded Dirty-Rect Blitter                             │       │
└─────────────────────────────┬──────────────────────────────┘       │
                              │ Unified Presentation Framebuffer     │
                              ▼                                      │
┌────────────────────────────────────────────────────────────────────┴────────┐
│ 3. USERSYSTEM / KERNEL DISPLAY INTERFACE (`/dev/fb0` & `/dev/dri/card0`)    │
│                                                                             │
│   [Simple Framebuffer Interface]               [5 DRM GEM IOCTLs]           │
│   • Write dirty pixels / page flip             • Alloc / Map / Submit / Sync│
└─────────────────────────────┬──────────────────────────────────────┬────────┘
                              │                                      │
                              ▼                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 4. KERNEL HARDWARE DRIVERS (`kernel/drivers/gpu/`)                          │
│                                                                             │
│   Tier 1: VirtIO-GPU Driver (Primary VM Target)                             │
│   • 2D: VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D + RESOURCE_FLUSH                │
│   • 3D: Virgl command stream passthrough to host physical GPU               │
│                                                                             │
│   Tier 2: Universal Linear Framebuffer (Universal Bare-Metal Fallback)      │
│   • UEFI GOP (x86_64 / aarch64) & VESA VBE 2.0/3.0 (i686)                   │
│   • Direct write to physical video RAM linear base address                  │
│                                                                             │
│   Tier 3: Micro-DRM Hardware Drivers (via aura-kshim — <1,200 lines C)      │
│   • Intel i915 / GMA (Vintage ThinkPads, Core 2, Pentium 4)                 │
│   • Panfrost / Lima (ARM Mali on R36S / RK3326)                             │
│   • AMDGPU / Radeon (Bare-metal PCs)                                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Walkthrough: How a Frame Travels from App to Screen

### Path A: 2D Desktop Apps (Notepad, Sysmon, Write) — *Zero GPU Overhead*
1. **Rendering:** App renders text and buttons into its own shared-memory backbuffer (`shm_handle`) using `libaura-ui`. **Mesa is NEVER loaded.**
2. **Damage:** App sends `aura_wm_msg` to `aura-wm` with dirty rectangle `(x, y, w, h)`.
3. **Compositing:** `aura-wm` runs Dual Kawase 4x blur behind translucent glass frames and blits the dirty rects to the final screen canvas.
4. **Scanout:** `aura-wm` flips the final buffer to the display driver. Total RAM cost: **< 1.2 MB**.

---

### Path B: Retro 3D Games & Handhelds (Quake, DevilutionX) — *TinyGL Path*
1. **Rendering:** Game links against **TinyGL** (pure freestanding C, <256KB RAM).
2. **Rasterization:** TinyGL rasterizes triangles directly into a shared-memory canvas on the CPU.
3. **Scanout:** Blits directly into an AuraOS window.
4. **Performance:** 60+ FPS on vintage Pentium III and ARM handhelds with zero GPU driver dependencies.

---

### Path C: Modern 3D Games & Blender — *Stripped Mesa Path*
1. **Rendering:** App calls standard OpenGL 4.5 or Vulkan.
2. **Mesa Translation:** Stripped **Mesa (`virgl` / `panfrost`)** compiles the shaders and builds a hardware command buffer.
3. **Kernel Hand-off:** Mesa calls the 5 simple DRM IOCTLs on `/dev/dri/card0` to allocate DMA memory and submit the command stream.
4. **Hardware Execution:**
   * In a VM (QEMU/VMware): The command stream passes directly to your host's NVIDIA/AMD/Apple Silicon GPU.
   * On Bare Metal: The GPU executes the commands natively at full hardware speed.

---

## 3. The 3 Golden Rules of the AuraOS GPU Pipeline

1. **2D Never Pays for 3D:** You never waste GPU memory or run heavy shader compilers just to draw a window or a text box.
2. **Kernel Stays Minimal:** The kernel contains zero OpenGL, zero Vulkan, and zero shader code. It only handles memory allocation and command submission.
3. **100% Boot Guarantee:** If no 3D accelerator exists, AuraOS boots immediately on UEFI GOP / VESA linear framebuffer without missing a beat.
