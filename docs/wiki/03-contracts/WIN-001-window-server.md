# AuraOS Frozen Contract: Window Server & Surface Protocol (`WIN-001`)

**Contract ID:** `WIN-001` · **Status:** FROZEN · **Target:** `aura-wm` & `libaura-ui`

---

## 1. Zero-Copy Shared-Memory Surface Protocol

Every top-level window owns an exclusive double-buffered shared-memory canvas:

```c
#ifndef _AURA_CONTRACT_WINDOW_H
#define _AURA_CONTRACT_WINDOW_H

#include <stdint.h>

#define AURA_WINDOW_TITLE_MAX 64

enum aura_window_flags {
    AURA_WIN_DECORATED   = (1 << 0),
    AURA_WIN_TRANSLUCENT = (1 << 1),  /* Requests Dual Kawase Glass blur */
    AURA_WIN_RESIZABLE   = (1 << 2),
    AURA_WIN_POPUP       = (1 << 3),
};

struct aura_rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

/* Message passed via sys_ipc_send() to Window Server */
struct aura_wm_msg {
    uint32_t type;         /* 1: CREATE, 2: DESTROY, 3: DAMAGE, 4: RESIZE */
    uint32_t window_id;
    uint32_t shm_handle;   /* Backbuffer memory object */
    struct aura_rect rect;
    uint32_t flags;
    char title[AURA_WINDOW_TITLE_MAX];
};

#endif
```

---

## 2. Damage Tracking & Compositing Law
1. An application renders into its `shm_handle` backbuffer.
2. The application posts a damage notification with dirty rects: `(x, y, w, h)`.
3. The Compositor blits only the damaged regions to the screen.
4. **Glass Blur Rule:** If a window has `AURA_WIN_TRANSLUCENT`, the compositor:
   * Expands the damage region by the blur radius (16px).
   * Downsamples the background layer (4x reduction).
   * Runs the 2-pass Dual Kawase blur.
   * Blends specular highlights and 9-slice frame borders.
