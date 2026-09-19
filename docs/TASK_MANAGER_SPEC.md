# AuraOS Process Observatory & System Monitor (`sysmon`) — Full Specification

**Goal:** Give the user complete, real-time visibility into everything running on the machine — like Windows Task Manager + Process Explorer + btop fused into one ultra-light Ubuntu-style app — while consuming less than 1.2 MB of RAM.

---

## 1. Data Source: The Kernel Telemetry Contract

Sysmon never parses text files. The kernel exports one flat, versioned C struct array via a single syscall:

```c
/* sys_telemetry.h — the entire observation surface, one syscall: sys_telemetry(buf, flags) */
#define AURA_PROC_MAX 256

typedef enum { PRIO_IDLE, PRIO_LOW, PRIO_NORMAL, PRIO_HIGH, PRIO_REALTIME } aura_prio;
typedef enum { STATE_RUNNING, STATE_READY, STATE_SLEEPING, STATE_BLOCKED, STATE_ZOMBIE, STATE_STOPPED } aura_state;

struct aura_proc_info {
    uint32_t pid;               /* process id */
    uint32_t parent_pid;        /* tree structure */
    uint32_t thread_count;
    char     name[32];          /* short binary name */
    char     user[16];          /* owner */
    uint8_t  state;             /* aura_state */
    uint8_t  priority;          /* aura_prio */
    uint8_t  cpu_affinity;      /* core mask */
    /* All usage counters are delta-computed by sysmon, not the kernel: */
    uint64_t cpu_ticks;         /* scheduler accounting since boot */
    uint64_t mem_rss_bytes;     /* resident pages */
    uint64_t mem_shared_bytes;  /* shared libs / shm surfaces */
    uint64_t disk_read_bytes;   /* VFS counters */
    uint64_t disk_write_bytes;
    uint64_t net_rx_bytes;      /* socket counters */
    uint64_t net_tx_bytes;
    uint32_t open_fds;          /* file descriptors */
    uint32_t gpu_percent_x100;  /* GPU scheduler time, when VirtIO-GPU active */
    uint64_t wall_ticks_started;/* boot time when process began */
};

struct aura_sys_telemetry {
    uint32_t version;                    /* ABI contract, never breaks */
    uint32_t proc_count;
    uint64_t total_ram, free_ram, cached_ram, kernel_ram;
    uint64_t swap_total, swap_used;
    uint32_t cpu_count;
    uint32_t cpu_freq_mhz[AURA_PROC_MAX_CORES];
    uint8_t  cpu_load_percent[AURA_PROC_MAX_CORES];
    uint64_t disk_rw_bytes[4];           /* per-block-device */
    uint64_t net_rx_tx[4][2];            /* per-interface */
    struct aura_proc_info procs[AURA_PROC_MAX];
};
```

* **Kernel cost:** counters increment in existing scheduler/VFS code paths — **zero dedicated monitoring threads, zero locks** (single atomic copy per syscall).
* **`sys_sysinfo_snap()`** for the machine-wide header; one call per refresh tick.

---

## 2. UI Design: Ubuntu-Style, Frosted Glass, Four Tabs

### Tab 1 — Processes
* **Tree view** (parent → child) with capsule-origins icon badges: `[N] Native`, `[L] Linux`, `[W] Windows capsule`, `[A] Android capsule`.
* Columns: `Name · CPU% · RAM · Disk · Net · PID · State · Priority` — sortable, sticky filter box (`/` to focus, type-to-filter).
* Selection → bottom detail pane (like Process Explorer):
  * Per-thread list, CPU affinity checkboxes, open file handles, open sockets, shared-memory surfaces, command line, started-at, caps/permissions.
* Right-click actions: `End Task (SIGTERM)` · `Force Kill (SIGKILL)` · `Pause / Resume` · `Priority` · `Affinity` · `Details Pane`.

### Tab 2 — Resources
* Smooth scrolling wave graphs (libaura-ui canvas, 120-sample ring buffer):
  * **CPU:** per-core colored strands + combined overlay.
  * **Memory:** stacked ribbon — `Apps / Kernel / Cache / Free` — plus swap strip.
  * **Disk & Network:** throughput sparklines per device.
* All graphs share one dirty-rect redraw; zero text layout cost per tick.

### Tab 3 — Storage
* Mounted volumes + RAM disks: capacity bars, FS type, read/write activity LED dots.

### Tab 4 — Startup & Services
* Autostart apps with launch-impact rating (measured cold-boot cost), enable/disable toggles.
* Service tree with `Running / Stopped / Restarting` state chips and one-click `Restart` (talks to `aura-init` supervisor).

---

## 3. Engineering Budget (Philosophy: Small. Fast. Direct.)
| Metric | Budget |
|---|---|
| RAM (total, including history buffers) | **< 1.2 MB** |
| Launch → first paint | **< 5 ms** |
| Refresh tick cost | **< 0.1 ms** (memcpy of one flat struct + deltas) |
| Allocations after startup | **0** (fixed ring buffers, static column widths) |
| Redraw model | dirty-rect only, 500ms/1000ms selectable tick |
| Binary size target | **< 120 KB** |

* History = `uint16_t` ring buffers (4 KB per graph), not heap vectors.
* Numbers rendered via fixed-point integer formatting — no floating point in the UI path.
* Identical binary runs on i686, x86_64, aarch64, armv7 — pure `libaura-ui` + `sys_telemetry`.

---

## 4. Companion: `top`-style CLI (`auratop`)
Same `sys_telemetry` syscall, ~15 KB binary, ANSI-color TUI for servers/serial consoles: sortable process table, per-core bars, memory ribbon — the btop role, sharing 100% of the kernel contract with sysmon.
