The user wants a rigorous PRD for AuraOS Component #05: Resource Quotas, OOM Policy & Safe Telemetry ABI. AuraOS is a freestanding C OS with 4 architectures (likely x86_64, aarch64, riscv64, and maybe i686 or loongarch64). The PRD must follow the exact 6-section structure. It should be production-grade markdown, grounded in Clean/Simple/Small/Fast/Direct, no overengineering.

I need to write concrete C code blocks for data structures, interfaces, and wire ABI contracts. This is a kernel-level component: per-process/per-cgroup resource quotas (memory, CPU time slices?, file descriptors, threads), OOM policy (kill selection, per-process oom_score_adj-like, guaranteed reserves), and a safe telemetry ABI (fixed structs, versioned, copy_to_user semantics, no pointers in wire structs, seq counters).

Key design principles to keep it small:
- Fixed-size structs, no variable-length anything in ABI.
- Versioned ABI with a magic + version field.
- Per-process quota struct embedded in task control block or a small quota table indexed by pid (or by task id).
- OOM policy: deterministic victim selection — largest RSS above guarantee with adjustable bias, bounded work O(n) scan with a hard cap.
- Telemetry: one syscall (or one entry point) returning fixed snapshot structs; no strings, no pointers; seqlock-style generation counter for tear detection; per-CPU counters with relaxed atomics, aggregated on read.
- Memory reserves for kernel paths (like Linux min_free_kbytes but simpler).
- 4 arches: types must be uniform — use fixed-width types (uint64_t etc.), no arch-specific layout; alignment explicit.

Avoid overengineering: no hierarchical cgroups, no io controllers, no user-facing cgroupfs, no RB-tree, no per-resource pluggable policies, no rate limiting of telemetry, no compression. One policy, compile-time selectable victim selection constants maybe.

Let me plan the sections:

## 1. Executive Summary & Scope Boundary
- What it is: three tightly-coupled kernel subsystems sharing one accounting core.
- Scope in: quota enforcement for memory (pages), threads, FDs, maybe CPU ticks budget per scheduling epoch; OOM kill decisioning with deterministic victim selection and a kernel memory reserve; telemetry ABI — versioned, fixed-layout snapshot structs readable via one syscall + a userspace ring? No — keep it simple: read-only snapshot via one syscall, plus optional mmap'd single page? Mmap of a page adds complexity (page fault handler), maybe still fine. Actually simplest: one syscall `sys_quotactl`? Better: `sys_telem_read` with generation. Keep to two syscalls: `sys_quota` (get/set on self or child) and `sys_telemetry`. And OOM is internal, no syscall, but expose `oom_score_adj` via quota set op.
- Scope out: hierarchical groups, swap, PSI, IO accounting, persistent accounting, compression of telemetry, remote telemetry, etc.

## 2. Core Requirements
Functional:
- FR-1: Quota objects: memory pages (RSS), thread count, fd count, CPU tick budget per epoch.
- FR-2: Enforcement points: page fault / allocation path, thread creation, fd install, tick accounting.
- FR-3: Graceful error codes: QUA_ENOMEM? Actually return ENOMEM/EAGAIN style errno mapped.
- FR-4: OOM policy: two-phase — reclaim warning then kill; deterministic victim selection: max (rss_pages - guarantee) with bias from user adj; hard cap on scan work; kernel reserve tiers (steal-free reserve for atomic contexts).
- FR-5: Telemetry: aggregate counters (allocations, faults, kills, quota denials per class), watermark levels, per-epoch samples; one-shot consistent snapshot read.
- FR-6: ABI versioning and backward compat: version field, size-checked, unknown flags ignored (or rejected? For safety, reject unknown flags — simpler and stricter).

Non-functional:
- NFR-1: O(header) hot-path cost: quota check is a single atomic load + compare on fast path; no locks on per-CPU counters.
- NFR-2: Bounded memory overhead: quota block ≤ 64 bytes, telemetry block ≤ 4 KiB per CPU.
- NFR-3: Deterministic: same system state → same OOM victim (tie-break by pid ascending).
- NFR-4: No allocation in OOM path.
- NFR-5: 4-arch identical ABI (fixed-width types, explicit alignment, static_assert layout).
- NFR-6: WCET bound for OOM scan, e.g., O(nr_tasks) single pass, no sorting.
- NFR-7: Telemetry read is wait-free for readers, single writer discipline or seqcount.

## 3. Data Structures, C Interfaces & Wire ABI Contracts

C code blocks:

```c
/* kernel/inc/quota.h */
#define AURA_QUOTA_ABI_VERSION 1u

/* resource ids */
enum aura_res {
    AURA_RES_MEM = 0,   /* resident pages */
    AURA_RES_THREADS,
    AURA_RES_FDS,
    AURA_RES_CPU_TICKS, /* per scheduling epoch */
    AURA_RES_NR
};

struct aura_quota { /* 32 bytes */
    uint32_t abi_version;
    uint32_t flags;      /* AURA_QF_* */
    uint64_t limit[AURA_RES_NR];
    uint64_t usage[AURA_RES_NR];
    int32_t  oom_adj;    /* -1000..1000, higher = more likely killed */
    uint32_t _pad;
};
```

Wait — must be careful with sizes. AURA_RES_NR = 4, so limit[4] = 32 bytes, usage[4] = 32 bytes. Total: 4+4+32+32+4+4 = 80 bytes. Fine. Keep it as is, state 80 bytes, static_assert.

Kernel-side:

```c
struct quota_block {
    uint64_t limit[AURA_RES_NR];       /* 0 = unlimited */
    _Atomic uint64_t usage[AURA_RES_NR];
    int32_t  oom_adj;
    uint32_t magic;
};
```

Hmm, for hot-path check: single atomic load + compare. But per-process usage shared across threads — atomic increments on every page fault could be contended. Simple approach: it's fine, atomics on one cache line; alternative per-CPU offsets is overengineering. Keep atomic add. Note contention is accepted; cacheline-align the struct.

API:

```c
void   quota_init(struct quota_block *q);
int    quota_charge(struct quota_block *q, enum aura_res r, uint64_t n);
void   quota_uncharge(struct quota_block *q, enum aura_res r, uint64_t n);
bool   quota_would_fit(const struct quota_block *q, enum aura_res r, uint64_t n);
int    quota_set_limit(struct quota_block *q, enum aura_res r, uint64_t n);
```

quota_charge returns 0 or -AURA_EQ_QUOTA. Lowering a limit below current usage: allowed; enforcement occurs on next charge (never kills for shrink).

OOM:

```c
enum oom_action { OOM_NONE, OOM_RECLAIM_RETRY, OOM_KILL };
struct oom_ctx { uint64_t free_pages; uint64_t reserve_pages; };
struct task; /* opaque */

enum oom_action oom_evaluate(const struct oom_ctx *ctx);
struct task *oom_pick_victim(void); /* deterministic, single pass, no alloc */
void oom_kill(struct task *t);      /* async signal, not synchronous teardown */
```

Scoring: score = (rss - guarantee)... but do we have guarantee? Simplify: score = rss_pages + oom_adj * AURA_OOM_ADJ_WEIGHT, where negative adj protects. Actually make it explicit: victim = argmax(rss_pages) + (oom_adj >= 0 ? + : -)... Let me define:

```
score = rss_pages * AURA_OOM_RSS_SCALE + (int64_t)oom_adj * AURA_OOM_ADJ_SCALE
```
with AURA_OOM_RSS_SCALE = 1, AURA_OOM_ADJ_SCALE = 64 pages equivalent per adj step? Keep constants named with rationale: one adj point = 64 pages (256 KiB) of RSS equivalence. Also: kernel-critical tasks (pid < some threshold? no — tasks with AURA_QF_CRITICAL flag) are skipped unless nothing else.

Telemetry:

```c
enum aura_telem_class {
    TELEM_PAGEFAULT, TELEM_QUOTA_DENY_MEM, TELEM_QUOTA_DENY_THREAD,
    TELEM_QUOTA_DENY_FD, TELEM_QUOTA_DENY_CPU, TELEM_OOM_KILL,
    TELEM_OOM_RECLAIM, TELEM_NR_EVENTS
};
struct telem_percpu {
    _Atomic uint64_t events[TELEM_NR_EVENTS];   /* 8 * 8 = 64 B */
    _Atomic uint64_t ticks_used, ticks_denied;  /* 128 B total */
};
_Static_assert(sizeof(struct telem_percpu) == 128, "cache-line pair");
```

Wire snapshot:

```c
#define AURA_TELEM_ABI_VERSION 1u
#define AURA_TELEM_MAGIC 0x54454C31 /* "TEL1" */

struct aura_telem_snapshot {
    uint32_t magic;
    uint32_t abi_version;
    uint32_t struct_size;   /* sizeof(self), for forward compat */
    uint32_t flags;         /* must be 0 on call; nonzero bits rejected */
    uint64_t gen;           /* seqcount generation at capture */
    uint64_t events[TELEM_NR_EVENTS]; /* summed across CPUs */
    uint64_t mem_total_pages, mem_free_pages, mem_reserved_pages;
    uint64_t nr_tasks, nr_threads;
    uint64_t oom_kills;
    uint32_t cpu_count;
    uint32_t uptime_ms_lo? 
};
```

uptime: keep `uint64_t uptime_ticks`. 

Syscalls:

```c
long sys_quota(unsigned int op, enum aura_res res, uint64_t value);
  /* op: GET_QUOTA(0), SET_LIMIT(1), SET_OOM_ADJ(2) — applies to current task or child? */
```

Simplest: a task may read its own quota; only privileged may set. For OS with no users yet maybe privilege = kernel capability. Provide `AURA_QOP_GET=0, AURA_QOP_SET_LIMIT, AURA_QOP_SET_ADJ, AURA_QOP_NR`. Target task: current task only, plus privileged caller may pass a pid. To avoid pid lookup complexity, keep: current task only in v1. Parent sets child quota at spawn via spawn attrs — cross-component contract with #04 Process/Spawn. Good: mention integration contract.

```c
long sys_telemetry(void *buf, uint32_t len);
```

Returns bytes copied; validates len >= struct_size, buf aligned to 8. Retries internally on seqcount tear up to 3 times; returns -EAGAIN after.

Wire ABI guarantees: no pointers, no floats, fixed-width, little discussion of endianness: AuraOS is little-endian on all four arches (x86_64, aarch64, riscv64, loongarch64)? If arches include big-endian, say ABI is LE by convention. I'll pick x86_64, aarch64, riscv64, loongarch64 — all LE. State: all four targets are little-endian; ABI is LE; no endian swizzling.

Static asserts on offsets for the wire struct.

## 4. Failure