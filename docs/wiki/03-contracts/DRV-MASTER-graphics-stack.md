# AuraOS Master Graphics Architecture & Resilient Fallback Specification

**Contract ID:** `DRV-MASTER` · **Status:** NORMATIVE ARCHITECTURE · **Audited against:** Clean. Simple. Small. Fast. Direct.

---

## 1. Executive Summary & Philosophy Compliance

AuraOS strictly rejects monolithic GPU fragility where a single GPU hang, bad shader, or missing hardware driver causes a black screen or kernel panic.

The **AuraGraphics Subsystem** operates on one uncompromising rule:
> **"The Screen Never Dies."** If high-performance 3D or native hardware acceleration encounters an error, the system dynamically and transparently degrades down the driver chain to simpler, guaranteed-stable tiers without interrupting userland applications.

---

## 2. The 4-Tier Resilient Fallback Engine ("Never-Black-Screen" Contract)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ LEVEL 3: Hardware 3D Acceleration (Mesa Gallium / Panfrost / Radv / Virgl) │
│ • Full hardware vertex/fragment shaders, SPIR-V, Vulkan 1.3, OpenGL 4.5    │
│ • State: Peak Performance Mode                                              │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ ⚠️ On GPU MMIO timeout / Fence stall
                                       ▼ (Fallback Time: < 16ms)
┌─────────────────────────────────────────────────────────────────────────────┐
│ LEVEL 2: Native 2D Hardware Acceleration (Direct BAR / 2D BitBLT Engine)    │
│ • Hardware window moves, rectangle clears, cursor overlays (Intel/ATI BAR)  │
│ • Zero CPU load for 2D blits. 3D apps fall back to CPU TinyGL rasterizer.   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ ⚠️ On display mode crash / Unknown GPU
                                       ▼ (Fallback Time: < 2ms)
┌─────────────────────────────────────────────────────────────────────────────┐
│ LEVEL 1: Universal Linear Framebuffer (UEFI GOP / VESA VBE 3.0)             │
│ • 100% universal across all x86_64, i686, aarch64, and armv7 hardware       │
│ • CPU Write-Combining (PAT / MTRR) enables fast software 2D blitting at 60Hz│
│ • Zero hardware driver dependency. Never fails.                             │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ ⚠️ On broken video clock / Headless
                                       ▼ (Fallback Time: Immediate)
┌─────────────────────────────────────────────────────────────────────────────┐
│ LEVEL 0: Emergency Serial Console & Text TTY (UART 16550 / PL011)           │
│ • Full terminal shell & kernel log stream at 115200 baud over COM1          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Fallback State Machine & Watchdog Protocol

The kernel GPU manager maintains an asynchronous watchdog timer (100ms interval):
1. **Fence Timeout:** If a hardware 3D ring-buffer command does not signal completion within **500ms**, the watchdog flags the GPU ring as hung.
2. **Subsystem Reset:** The kernel resets the GPU command streamer and drops to **Level 2 (2D Native BAR)**.
3. **Transparent Migration:** `aura-wm` automatically redirects its scanout buffer to the Level 1/2 linear framebuffer. Applications running on `libaura-ui` experience zero crashes.

```c
/* kernel/include/aura/gfx_fallback.h */

typedef enum {
    AURA_GFX_LEVEL_SERIAL = 0,    /* COM1 UART emergency console */
    AURA_GFX_LEVEL_FRAMEBUFFER = 1, /* Universal UEFI GOP / VESA VBE */
    AURA_GFX_LEVEL_2D_ACCEL = 2,  /* Native 2D BitBLT / BAR MMIO */
    AURA_GFX_LEVEL_3D_ACCEL = 3   /* Mesa Gallium / DRM 3D Pipeline */
} aura_gfx_tier_t;

struct aura_gfx_tier_ops {
    const char      *tier_name;
    aura_gfx_tier_t  tier_level;
    
    int  (*probe)(void);
    int  (*init)(void *device_ctx);
    void (*flip_page)(uint32_t buffer_handle);
    void (*blit_rect)(int x, int y, int w, int h, const void *pixels, uint32_t pitch);
    void (*reset_hardware)(void);
    void (*shutdown)(void);
};

struct aura_gfx_manager {
    aura_gfx_tier_t          current_tier;
    aura_gfx_tier_t          maximum_supported_tier;
    const struct aura_gfx_tier_ops *active_ops;
    
    uint32_t                 screen_width;
    uint32_t                 screen_height;
    uint32_t                 screen_pitch;
    uint32_t                 bits_per_pixel;
    void                    *linear_framebuffer_vaddr;
    
    uint32_t                 consecutive_fence_timeouts;
    uint64_t                 last_heartbeat_tick;
};
```

---

## 3. End-to-End Visual Pipeline Diagram

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
│ 3. KERNEL DISPLAY INTERFACE (`/dev/fb0` & `/dev/dri/card0`)                 │
│                                                                             │
│   [Linear Framebuffer Interface]               [5 DRM GEM IOCTLs]           │
│   • Write dirty pixels / page flip             • Alloc / Map / Submit / Sync│
└─────────────────────────────┬──────────────────────────────────────┬────────┘
                              │                                      │
                              ▼                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 4. HARDWARE DRIVERS (`kernel/drivers/gpu/`)                                 │
│                                                                             │
│   Level 3: VirtIO-GPU 3D (VM) / Panfrost (Mali ARM) / AMDGPU (Bare Metal)   │
│   Level 2: Intel GMA/i915 2D BAR / ATI Rage 2D Engine / VirtIO-GPU 2D       │
│   Level 1: UEFI GOP / VESA VBE 3.0 Linear Framebuffer (WC MTRR/PAT)         │
│   Level 0: UART 16550 / PL011 Serial TTY Console                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Userspace Mesa 3D Strategy & The 5-IOCTL Contract

Mesa is strictly an unprivileged userspace library. The kernel implements only **5 DRM GEM IOCTLs** on `/dev/dri/card0`:

| IOCTL Name | Function | Kernel Implementation in Freestanding C |
|---|---|---|
| `DRM_IOCTL_VERSION` | Probe driver identity & capabilities | Return static version struct (8 lines) |
| `DRM_IOCTL_GEM_CREATE` | Allocate contiguous video/DMA RAM | Call `pmm_alloc_contiguous_dma()` (15 lines) |
| `DRM_IOCTL_GEM_MMAP` | Map DMA buffer into application address space | Map PTEs via `vmm_map_user_page()` (18 lines) |
| `DRM_IOCTL_EXECBUFFER2`| Submit hardware command packets to GPU | Append to hardware command ring buffer (25 lines) |
| `DRM_IOCTL_GEM_CLOSE` | Release buffer allocation | Call `pmm_free_frames()` (8 lines) |

### Stripped Mesa Build Configuration (Meson)
```meson
meson setup build \
  -Dgallium-drivers=virgl,panfrost,iris \
  -Dvulkan-drivers=lavapipe \
  -Dllvm=disabled \
  -Dglx=disabled \
  -Degl=enabled \
  -Dgbm=enabled \
  -Dtools=[] \
  -Dplatforms=aura \
  -Dbuildtype=release \
  -Db_lto=true
```

---

## 5. Hardware Support Matrix & Resource Budgets

| Hardware Platform | Acceleration Level | 2D Driver | 3D Engine | Total RAM Overhead | Target Framerate |
|---|---|---|---|---|---|
| **Pentium III + ATI Rage / Intel 845** | Level 2 (2D Native) | Native C 2D Engine | TinyGL (Pure C) | **< 512 KB** | 60 FPS (1024x768) |
| **ThinkPad T60 / X200 (Intel GMA)** | Level 2 (2D Native) | Native i915 BAR Driver | TinyGL / Mesa Classic | **< 1.5 MB** | 60 FPS (1440x900) |
| **R36S / RK3326 Handheld (ARM Mali)** | Level 3 (Mesa 3D) | Panfrost DRM via Shim | Mesa Panfrost Gallium | **< 4.0 MB** | 60 FPS (640x480) |
| **Modern PC (AMDGPU / Intel Iris)** | Level 3 (Mesa 3D) | AMDGPU / Iris via Shim | Stripped Mesa (Radv/Iris) | **< 8.0 MB** | 60+ FPS (1080p/4K) |
| **Virtual Machines (QEMU/VMware)** | Level 3 (VirtIO 3D) | VirtIO-GPU 2D Driver | Mesa Virgl Passthrough | **< 6.0 MB** | 60 FPS (Host GPU) |
| **Unknown / Brand New Hardware** | Level 1 (Firmware FB)| UEFI GOP Linear FB (WC)| TinyGL Software Blit | **0 MB (Driverless)** | 60 FPS (2D Desktop) |

---

## 6. Summary of Architectural Guarantees

1. **Zero Black Screen Failure Mode:** The fallback engine guarantees that even an unexpected GPU lockup results in an immediate downgrade to Level 1 or 2 rather than a system crash.
2. **Zero Bloat in 2D Desktop:** Core applications (Notepad, Sysmon, Calc, Terminal) **never initialize or load Mesa**, preserving the sub-32MB idle RAM goal.
3. **100% Freestanding Core:** The kernel graphics subsystem remains 100% pure, self-contained freestanding C.
