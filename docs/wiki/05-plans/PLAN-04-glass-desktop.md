# AuraOS Implementation Plan 04: Glass Compositor & Desktop Shell

**Plan ID:** `PLAN-04` · **Implements:** `PRD-11-desktop-compositor.md` · **Phase:** Phase 4

---

## 1. Goal
Userspace window server `aura-wm`, framebuffer resolution, dirty rect blitting, rounded Aero Glass frames, and boot into interactive desktop shell with draggable windows.

## 2. Work Breakdown

| Task | File Path | Deliverable |
|---|---|---|
| **4.1** | `drivers/fb/fbdev.c` | Limine/UEFI framebuffer: pitch, bpp, depth, scanout pointer |
| **4.2** | `gui/aura_wm.c` | Userspace window manager process, z-ordering, focus tracking |
| **4.3** | `gui/compositor.c` | Damage-based dirty rect blitting to linear framebuffer |
| **4.4** | `gui/kawase_blur.c` | 4x Dual Kawase downsample + upsample with SSE2 / NEON paths |
| **4.5** | `libs/libaura-ui/` | Core widget toolkit: `window`, `button`, `label`, `input`, `panel` |
| **4.6** | `userland/shell/desktop.c` | Desktop shell: taskbar, start menu, wallpaper |
| **4.7** | `drivers/input/ps2.c` | PS/2 mouse & keyboard driver with IPC event routing |

## 3. Verification Criteria
* Launch desktop with 1024x768 framebuffer.
* Open two translucent glass terminals. Drag them: 60 FPS rendering with no ghosting artifacts.
* Kill the window server: `aura-init` restarts it in <50ms, existing windows reconnect.
