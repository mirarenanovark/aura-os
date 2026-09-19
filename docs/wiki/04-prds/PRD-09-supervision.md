The user wants a complete, rigorous PRD for "AuraOS Component #09: Service Discovery, aura-init Supervisor & Recovery". AuraOS is a fictional/real minimal OS written in freestanding C targeting 4 architectures (likely x86_64, aarch64, riscv64, and maybe i686 or loongarch64 — I should pick sensible ones: x86_64, aarch64, riscv64, armv7? "4 arches" — common for hobby/serious minimal OSes: x86_64, aarch64, riscv64, i686. Let me pick x86_64, aarch64, riscv64, loongarch64 — hmm. Safer: x86_64, aarch64, riscv64, i686. I'll state them explicitly.)

The PRD must follow exactly the 6 sections. It must be production-grade markdown, concrete C code blocks in section 3. Grounded in "Clean. Simple. Small. Fast. Direct. No overengineering."

I'm the "lead OS architect". So tone should be authoritative, rigorous, precise. It should define:

- aura-init: PID 1 equivalent, process supervisor, service discovery, dependency resolution, restart policies, crash recovery.
- Service discovery: services register with aura-init, clients query.
- Since it's an OS with a microkernel-ish design? AuraOS — I should be consistent. Component #09 implies prior components (kernel, memory manager, scheduler, IPC, VFS, etc.). I'll reference plausible prior components: #01 Kernel core, #02 memory, #03 scheduling, #04 IPC (message ports), #05 VFS, #06 drivers framework... I can reference "AuraOS IPC (Component #04)" etc. Keep it plausible but not overly speculative; light references are fine.

Design decisions that keep it small:
- Services defined by declarative service descriptors in a fixed binary or simple table format (SVD — Service Descriptor). Flat table, no dynamic graphs beyond simple "after" dependencies. Dependency resolution: topological ordering with cycle detection at load time, simple Kahn's algorithm or DFS.
- Service states: finite state machine: UNREGISTERED → WAITING (deps unmet) → STARTING → RUNNING → STOPPING → STOPPED/FAILED. Keep it tiny.
- Restart policy: NONE, ON_FAILURE with backoff (bounded exponential), ALWAYS? Keep it to three values with fixed caps.
- Discovery: query by name via IPC port lookup. aura-init owns a well-known port (AURA_INIT_PORT = 1 or a name registry). Services register their port at startup. Discovery returns port handle + protocol version.
- Health: heartbeat? Might be overengineering. Simpler: aura-init tracks liveness via process exit notifications from kernel (child exit signals), plus optional readiness message (SERVICE_READY) sent by service on its own. Two-phase start: spawn, wait for READY msg with timeout; if timeout → fail.
- Recovery: restart with backoff, max retries, then mark FAILED and enter cooldown; critical services → panic or degrade? Critical flag: if critical service fails permanently, aura-init halts the system (kernel halt) — that's honest for PID 1. Or "reboot". Keep: CRITICAL services cause orderly shutdown/reboot after N failed attempts.
- Bounds: MAX_SERVICES 256 (fixed static table, no malloc — freestanding C, static allocation). Name length max 31 chars + NUL. MAX_DEPS 8 per service. Restart backoff 100ms → 6.4s max, MAX_RESTARTS 5 in window, cooldown 30s. Ready timeout 5s. Stop timeout 3s.
- Watchdog? Maybe simple: no hardware watchdog in this component; that's component for platform. Anti-goal.
- Wire ABI: fixed-width little-endian, versioned. Messages: struct aura_init_msg with 32-bit magic 'AINI', opcode, u32 seq/cookie, and fixed body sizes. Opcodes: SVC_REGISTER, SVC_UNREGISTER, SVC_READY, DISCOVER_REQ, DISCOVER_RSP, LIST, STOP/START admin op? Keep minimal: REGISTER, READY, UNREGISTER, DISCOVER, SUBSCRIBE (event on state change)? Subscribe might be bloat but useful for init scripts... keep DISCOVER + LIST + WAIT? Hmm. "No overengineering": REGISTER, READY, UNREGISTER, DISCOVER, EVENT (broadcast state changes to subscribers)? I'll include a minimal event subscription because recovery visibility matters for dependent services (they need to know a dependency died). Actually dependency restart: when a dependency restarts, dependents that requested restart-on-dep-death? Keep simple: when a service restarts, its port may change; clients re-discover. Provide EVENT notifications so clients can invalidate cached ports. That's justified and small: one subscription bit.

Let me define message sizes fixed: header 16 bytes: magic u32, opcode u16, version u16, cookie u32, length u32 (body length), then body up to 64 bytes? Fixed-size messages simplify: total frame 64 or 96 bytes. Use fixed struct with union of bodies. That's very much "no overengineering": single fixed-size message struct.

- C interfaces: 
  - libaura-init client-side: aura_discover(name, &handle, timeout_ms), aura_service_ready(status), aura_register_self(...), aura_event_poll(...).
  - Supervisor internal: struct aura_service table, svc_state machine, dependency resolver.
- Persistence across reboots: none (state rebuilt each boot). Anti-goal: persistent service state.
- Ordering: boot sequence from a Service Manifest compiled into the initramfs as a binary table (aura.svd) generated by a host tool (mkmanifest). Parsing is trivial: validate magic, count, fixed array.

Testing/acceptance: unit tests for resolver cycles, fuzz the manifest parser with bounded inputs, integration tests on all 4 arches (qemu), fault injection (service crash loop), timing bounds (start-to-ready measurement), memory: static footprint < N KiB, cyclomatic complexity gates? Provide evidence: logs, test artifacts, coverage numbers for the parser.

Non-functional: WCET-ish bounds, deterministic O(n) or O(n+e) boot resolution, static memory budget (e.g., 96 KiB table), no heap. Code size budget: aura-init .text ≤ 24 KiB per arch; library ≤ 8 KiB. Latency: discover round-trip ≤ 200 µs on emulated targets? Careful with claims; say "≤ 500 µs on reference QEMU x86_64 @ 2 GHz" as gate.

Architecture notes: freestanding C11, no libc; uses Component #04 IPC (ports), Component #03 processes (spawn/wait), Component #05 VFS for reading manifest. Word-size independence: fixed-width ints (stdint.h via freestanding headers — stdint.h is freestanding-ok). Alignment: use __attribute__((packed)) with static asserts on sizes; little-endian on wire for all 4 arches (note: all four targets are LE; if a BE arch ever added, must swap — put in risks).

Structures:

```c
/* aura/init/abi.h */
#define AURA_INIT_ABI_VERSION   1
#define AURA_INIT_MAGIC         0x494E4941u  /* "AINI" */
#define AURA_INIT_PORT          1
#define AURA_SVC_NAME_MAX       31
#define AURA_SVC_DEP_MAX        8
#define AURA_SVC_MAX            256
#define AURA_INIT_MSG_SIZE      64

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t abi_version;
    uint8_t  opcode;
    uint8_t  flags;
    uint32_t cookie;
    uint16_t body_len;
    uint16_t reserved;            /* must be 0 */
    /* body follows in fixed 48-byte tail union */
} aura_init_hdr_t;
```

Hmm, better to have one struct containing header + union body, total 64 bytes.

Opcodes:
- 1 REG_REQ (service → init): body { name[32], port u32, proto u16, critical flag, restart policy } → REG_RSP { svc_id u16, status }
Wait — registration by manifest: the manifest declares services; REG is the runtime handshake where the spawned process registers and announces its port. Then READY completes startup. So flow: init spawns process with svc_id in env/aux or argv? Freestanding: pass svc_id via spawn argument (aux block) or the child inherits a pre-bound port. Simplest: init spawns child; child sends REG {name, port} to init port; init matches name against manifest, replies svc_id + granted restart policy etc.; child then does work, sends READY {status}. If service has no dynamic port, port can be 0 (e.g., drivers). Discovery only possible once port known.

Simplify further: since manifest already knows names, REG just carries the port. Good: body { name[32] not needed if we pass svc_id in spawn args } — but passing via args requires argv support. Many minimal OSes pass a small "init block" pointer via auxv-like mechanism. To avoid coupling, keep name in REG message; matching by name is simple and robust. Keep name.

- 2 REG_RSP { status, svc_id, effective policy }
- 3 READY { status u32 (0 ok), flags }
- 4 DISCOVER_REQ { name[32], abi_min u16 } → 5 DISCOVER_RSP { status, svc_id, port, proto_version, state, gen (generation counter, bumped each restart to invalidate stale ports) }
- 6 UNREG { svc_id, reason } (or implicit on process exit)
- 7 SUBSCRIBE { mask } → EVENTS pushed: 8 EVENT { svc_id, old_state, new_state, gen }
- 9 SHUTDOWN? Admin: STOP_REQ { svc_id } → handled by initdev? Might be needed for orderly shutdown sequencing (reverse topological order). Include opcode STOP and STOP_RSP? Keep: ADMIN_STOP_REQ/RSP and that's it. Maybe also LIST? LIST can be served via a sysfs-like VFS file /sys/services? That'd be component #05. Anti-goal: expose state via debugfs instead of new opcodes. Good — keep LIST out of ABI, provided by a debug file. Actually a "initctl" tool may want list; but keep minimal: LIST omitted, debugfs provides it. Hmm, but debugfs read of binary table — fine, cat /sys/services as text. Keep as note.

Generation counter (gen): each (re)start increments; DISCOVER returns gen; client caches {port, gen}; EVENT with new gen triggers re-discovery. Simple, bounded, solves the "stale port after restart" problem without per-client tracking by init.

Manifest format (aura.svd): header { magic "ASVD", version, count, reserved }, then count × svc_entry { name[32], path[64]? binary path or module id, flags (critical, restart policy), ndeps u8, dep indices u16[8], ready_timeout_ms u16, stop ordering derived by reverse edges }. Path length: 96? Let's set AURA_SVC_PATH_MAX 95. Entry size: 32+96+2(flags)+1(ndeps)+1(pad)+16(deps)+2(timeout)+2(pad) = 152 bytes. 256 entries ≈ 38.9 KiB + header. Fine under memory budget.

Table entry runtime struct: state, port u32, gen u16, restarts u8, backoff_shift u8, pid/handle, cookie, deadline timestamps, subscribers bitmask? Subscribers: allow up to 32 subscribers (each a port) — bitmask u32