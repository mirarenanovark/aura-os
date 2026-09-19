The user wants a rigorous PRD for AuraOS Component #11: Display Ownership, Glass Compositor & libaura-ui Toolkit. This is a freestanding C operating system with 4 architectures (likely x86_64, aarch64, riscv64, and one more — maybe i686 or loongarch64; I'll pick x86_64, aarch64, riscv64, loongarch64 as a reasonable modern set, though I could also mention i686. Let me just say the 4 arches without being overly specific, or pick x86_64, aarch64, riscv64, i686 — actually a modern OS would more likely be x86_64, aarch64, riscv64, loongarch64. I'll define AURA_ARCH values).

Design philosophy: Clean. Simple. Small. Fast. Direct. No overengineering.

Key design decisions for this component:

1. **Display Ownership**: A kernel or early-userland component that owns the framebuffer / scanout device, arbitrates access. In AuraOS, likely a userland display server or a kernel-owned display manager that hands framebuffer to a single compositor process. Given "no overengineering" — the simplest model: kernel enumerates display hardware (via arch drivers), claims a framebuffer, and grants exclusive ownership to exactly ONE process: the glass compositor. Mode setting done by a small KMS-lite driver set.

2. **Glass Compositor**: A single user-space process ("glassd") that owns all scanout, does damage tracking, window compositing with per-pixel alpha, receives input events from input component (probably Component #10 or similar), and exposes a wire protocol over IPC (shared memory ring buffers) to clients.

3. **libaura-ui**: A client-side toolkit — immediate-ish or retained lightweight widgets, fonts (bitmap or simple vector), drawing into client-side buffers that are then swapped/presented to the compositor via buffer attachment. No CSS, no HTML, no JavaScript, no GPU shaders required (CPU compositing first, GPU optional fast path later — actually, anti-goal: no GPU accel in v1).

Wire ABI: fixed-size 64-byte messages? Keep it simple: a request/response protocol over a ring buffer with shared-memory buffers for pixels. Version negotiation with a single protocol version. Little-endian canonical. Fixed pixel format: XRGB8888 (opaque) and ARGB8888 (alpha) for windows.

Let me structure the ABI:

- Client connects via IPC port "aura.display" (or via namespace).
- Handshake: client sends HELLO with version, compositor replies with display info (bounds, dpi, scale).
- Client creates a window (CREATE_WINDOW), gets a window id, mmaps a buffer pool (double/triple buffer via buffer slots).
- Client attaches buffer slot + damage rect to present.
- Compositor sends input events (pointer, keyboard, close requests), configuration events (resize).
- Compositing: damage-union based, per-window occlusion via simple rect list, alpha blend with SIMD-friendly inner loops; dirty-region-only repaint.

Buffer management: server allocates a buffer pool (gralloc-lite): N slots per window, each slot a fixed-size frame. Slot count bounded (e.g., 2–4). Simple, direct.

Damage tracking: client supplies damage rects (bounded, e.g., max 16 rects or a single bounding box — simpler: single bounding box per frame plus optional up to 16 rects; actually simpler still: client supplies one bounding rect; compositor unions per window). Keep it to a bounding box — Clean. Simple. But allow up to 16 rects? Let me decide: client sends `damage_count` up to AURA_UI_MAX_DAMAGE (8), plus compositor always unions into frame damage. Hmm — for simplicity and speed, a single bounding box per present is enough for v1, and compositor maintains frame-level damage. I'll go with up to 16 rects because tiled redraw in toolkits is common, but honestly one bounding box is the "no overengineering" answer... Middle ground: client may send 0 (full damage) or up to 16 rects. It's cheap and prevents pathological overdraw. I'll allow 16 but keep code paths single-loop.

Data structures:

Kernel side:
- `aura_display` — framebuffer info: phys addr, width, height, stride, format.
- `aura_disp_owner` token.

Compositor side:
- window table: fixed max windows (e.g., 128).
- per-window: surface slots, geometry, z-order (simple list, no z-tree), visibility flags, opaque hint.

libaura-ui:
- widget model: retained widget tree with invalidation; events delivered top-down; hit testing by rect; drawing via a `aura_canvas` (pixel ops: fill, blit, glyph blit8, line, rect, rounded-rect optional — maybe just rect).
- Font: 8x16 bitmap font baked in for default; extensible via bitmap font file format (PSF-like). Simple.
- Theming: fixed struct of colors, no dynamic themes.

Non-functional: latency budget (present-to-scanout ≤ 1 frame + 2ms typical), CPU budget, memory bounds (compositor RSS ≤ 4 MiB + framebuffers), predictable: no unbounded allocations, no malloc after init except bounded pools.

Failure modes: client death → buffer reclamation, window removal within one frame; compositor crash → system restart policy (supervisor restarts, screen shows boot logo); mode set failure → fallback mode; OOM on buffer alloc → deny window creation with error code; protocol violation → disconnect client, keep compositor alive.

Bounds:
- Max displays: 4
- Max windows: 128 (per compositor)
- Max damage rects: 16
- Max buffer slots per window: 4 (min 2)
- Window min size 1x1, max = display size * 2? Max = display bounds.
- Max clients: 256
- Input queue depth: 256 events per client.

Acceptance criteria: measurable test gates — latency benchmark, throughput (fill rate MB/s), memory ceiling, fault injection (kill -9 client 10k times), fuzz the wire protocol (malformed messages), determinism, no heap growth over soak test.

Anti-goals: no GPU acceleration v1, no network transparency, no remote desktop, no shaders, no CSS/theming DSL, no text shaping (bidi/complex scripts), no animations framework beyond optional per-frame callback, no accessibility tree (leave a stub? no — anti-goal), no multi-monitor spanning/advanced layouts beyond simple per-display stacking, no gamma/color management, no HDR, no variable refresh negotiation, no screenshot API? maybe simple one. No dlopen plugins, no scripting.

Pixel format: canonical AURA_PF_XRGB8888 and AURA_PF_ARGB8888. Scanout format assumed XRGB8888; compositor converts if hardware differs? Simplest: mode setter guarantees XRGB8888 scanout; if impossible on some arch, compositor converts in software at present time (cost accounted).

Alignment: messages fixed 48 bytes? Let's define header 16 bytes + body up to 48 → fixed 64-byte messages? Fixed-size messages simplify parsing massively. I'll do: all messages are exactly 32 bytes, with variable data (damage rects) following in the same packet — hmm, variable length complicates. Alternative: damage rects sent as separate 32-byte messages before ATTACH, or pack up to... Let me make message size 32 bytes: [u32 type][u32 len][u64 cookie] + 16 bytes payload? Damage rects are 4×u16 = 8 bytes each; 2 per message → too fiddly.

Cleaner: ATTACH carries a single bounding-box damage (u16 x,y,w,h = 8 bytes) and optional flag AURA_WF_FULL_DAMAGE. If client wants fine damage it sends an optional DAMAGE message per rect before attach (each 32-byte msg: type DAMAGE, window_id, rect). Compositor coalesces. Max 16 DAMAGE messages between presents; extras must still be accepted but coalesced into bounding box (bounded work). Good — bounded, simple.

Message frame: 32 bytes fixed:
```c
typedef struct {
    uint32_t type;      /* aura_msg_type */
    uint32_t size;      /* payload bytes, 0..16 */
    uint32_t window_id; /* or 0 */
    uint32_t seq;
    uint8_t  payload[16];
} aura_msg;  /* 32 bytes, static_assert */
```
Hmm, seq adds complexity — needed for cookie matching on replies. Keep seq as request id echoed in events? Simpler: cookie field. Actually with a request/reply over ring buffer, order is FIFO; replies correlate in order for synchronous ops. Events are one-way. So no seq needed if all synchronous requests produce exactly one reply in order. But CREATE_WINDOW is sync (needs window id back). ATTACH is async (no reply). Round-trips: create window, create buffer pool, destroy — all sync, in-order replies. That's fine; drop seq. Keep 32-byte: type, size, window_id, payload[20]? Let me lay out:

```c
typedef struct {
    uint16_t type;
    uint16_t window_id;
    uint16_t payload_size;  /* 0 or 16 */
    uint16_t flags;
    uint8_t  payload[16];
} aura_netmsg; /* 24 bytes */
```
Hmm, let me pick a clean 32-byte layout:

```c
typedef struct {
    uint32_t type;        /* aura_ui_msg */
    uint32_t window;      /* window id, 0 for connection-level */
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint32_t arg4;
    uint32_t arg5;
} aura_ui_msg; /* exactly 32 bytes */
```
arg0..arg5 = 24 bytes payload. CREATE_WINDOW: arg0=x, arg1=y, arg2=w, arg3=h, arg4=format/flags, arg5=surface count. CREATE_SURFACE_REPLY: window in `window`, args width height stride slot_count shm_handle? shm handles: pass pool as IPC cap + offsets. Since AuraOS IPC (assume a message + capability model like seL4-lite), buffer pool is a shared memory object granted by compositor; offsets computed as slot_index * slot_size.

Present/attach: ATTACH window, arg0=slot, arg1=damage x, arg2=damage y, arg3=damage w, arg4=damage h, arg5=flags (async). 

Events: RESIZE (arg0..3 new bounds), CLOSE, POINTER (arg0=x, arg1=y, arg2=buttons, arg3=flags), KEY (arg0=keycode, arg1=mods, arg2=down), FOCUS, ENTER/LEAVE, FRAME (ack of attach → client can reuse slot).

That's clean. Ring buffer: 4 KiB aligned pages, head/tail in shared header, power-of-two 128–4096 messages, single-producer/single-consumer each direction. Standard.

Frame loop: compositor driven by vsync IRQ (or timer fallback 60 Hz tick), dirty region compositing, direct scanout optimization when single fullscreen opaque window with zero damage → skip composition (fast path), otherwise compose into back buffer and pageflip/memcpy.

Input: consumed from input component ring; routed by focus/topmost hit test; keyboard to focused window; pointer to window under cursor with implicit grab on button down.

Z-order: single global stacking list per display; ops: raise, lower? Keep: raise on click (optional policy flag), explicit RAISE request.