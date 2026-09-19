The user wants a rigorous PRD for AuraOS Component #04: Kernel Concurrency, Object Lifetimes, IPC & Shared Memory. This is a freestanding C kernel component for a small operating system with 4 architectures (presumably x86_64, aarch64, riscv64, and one more — maybe i686 or loongarch64; I'll pick x86_64, aarch64, riscv64, i686 as common for hobby/serious small OSes).

The PRD must be production-grade markdown, strictly following the 6 sections. It needs concrete C code blocks for data structures, interfaces, and wire ABI contracts. Grounded in: Clean. Simple. Small. Fast. Direct. No overengineering.

Let me think about what a minimal-but-rigorous kernel concurrency/lifetime/IPC/shm design looks like for a small multiserver or hybrid kernel OS.

Design philosophy: microkernel-lite. Kernel provides:
- Synchronization primitives (spinlocks, mutexes with wait queues, rwlock maybe not — keep small; semaphores maybe). Actually keep: spinlock (irqsave variant), sleeping mutex, condition variable or wait queue primitive. That's enough. No rwlock, no RCU — anti-goals.
- Reference counting with atomic ops and a generic "object header" (kobj) so kernel objects share lifetime machinery.
- Wait queues / sleep-wake.
- IPC: synchronous message passing (fixed-size copy registers, L4-style) OR channel-based. For simplicity and determinism: synchronous fixed-size message IPC with optional reply phase, plus asynchronous notifications (signals/notifs). L4 heritage: message registers (MRs) and buffer registers. Keep it simple: fixed 8-word message + optional extended payload via caller-provided buffer copied by kernel.
- Shared memory: capability-based mapping of memory objects (frames) into address spaces with specified permissions; refcounted frame objects; grant/loan model. Keep simple: frames created from physical memory allocator, mapped via pager with perms; shared memory = same frame mapped into multiple address spaces.

Object lifetimes: kernel objects (threads, address spaces, channels/endpoints, frames) have a common header: refcount, type tag, destructor fn pointer, lock maybe. Last reference drops -> destructor runs. Plus a "dead" / zombify state so handle lookup can be race-free: lookup under object table lock, take reference, release lock.

Handle table per-process: capability table with rights bits. Handles are process-local integers. Table is 2-level or flat array with generation counters to avoid ABA. Keep simple: flat array with gen in upper bits — handle = (gen << 20) | index. 

IPC design: Endpoint-based like seL4 or channel like Zircon. Zircon channels are simple: two endpoints, message queue, async. L4 sync IPC is fast. The PRD title says "Fast. Direct." — synchronous IPC with small fixed message is fastest and simplest. But async notification needed for interrupts delivery. Let me define:

- `endpoint` object: message queue of fixed-size messages (8 words) + waiting threads? If synchronous: sender blocks until receiver calls recv. That's L4-style "rendezvous" — small and fast, no buffering. Add `notification` object (word-sized bitmask, async) for interrupt events.

Actually, hybrid: rendezvous endpoints for RPC (call/reply), notifications for async events. Plus shared memory for bulk data. This trio covers everything: control plane via sync IPC, events via notifications, data plane via shm. That's the classic minimal set. 

Replies: reply capability created by kernel on recv of a call, single-use. Keep: `ipc_call` includes a reply wait inline (call = send + recv on ephemeral reply channel). Simplest: `k_ipc_call(ep, msg, reply)` blocks for the reply. `k_ipc_reply(...)` — need to identify who to reply to. Options: reply token in received message header. Kernel gives receiver a one-shot reply handle. Keep it: recv returns a `reply_token` valid for exactly one reply. Implement as embedding a pointer to the waiting sender's thread with a single ref. Must be careful: sender cancelled/timed out -> token invalid; kernel marks. Simple: reply token = index into per-ep "pending replies"? Or the receiving thread's TCB holds `reply_waiter` pointer. One reply per blocked thread since a thread can only be in one call. So token is opaque: `void*`? No — must be an integer for ABI. `k_reply_handle_t` = 64-bit opaque. Internally points to the waiting thread's object id + a sequence. To be rigorous but simple: reply handle is an encoded (thread_id, seq). Kernel validates seq against sender thread's current reply_seq. 

Timeouts: optional, keep with simple absolute deadline or tick count; maybe support 0 = wait forever, else ticks. Keep.

Object table: per-process handle table with entries {kobj*, rights, gen}. System call numbers. 

Wire ABI: syscall numbers, struct layouts for messages, shared memory grant structs, all packed with fixed-width types. 4 arches: x86_64, aarch64, riscv64, i686 — wire ABI must be arch-independent: fixed-size fields, no pointer fields in wire structs (handles instead), explicit padding, little-endian canonical (state big-endian byte swap requirement? Keep: little-endian on all four target archs — true for x86, i686, and LE modes of ARM/RISC-V. State that all supported arches run LE.)

Syscall convention: vDSO-like entry — each arch has a documented register mapping. Provide table per arch: syscall number register, arg regs, return regs. That's the "wire ABI contract" for syscall layer.

Also include kernel-internal C interfaces (not ABI): spinlock API, mutex API, refcount API, wait queue, kobj.

Non-functional: max syscall latency targets, zero-allocation fast paths, lock ordering rules, determinism, no dynamic allocation in IPC hot path, O(1) operations, memory budget for kernel structures (fixed static pools or slab), limits (max handles, max message size, max frames per mapping...), validation.

Failure modes: invalid handle, wrong rights, stale generation, dead object, timeout, queue full (if any queue), EPERM etc. Enumerate error codes as `kerr_t` negative errno style (like Linux -errno). Keep small list: KE_OK, KE_INVAL, KE_NOENT (stale handle), KE_PERM, KE_NOMEM, KE_AGAIN, KE_TIMEDOUT, KE_BADSTATE, KE_TOOBIG, KE_BUSY, KE_NOTIMP? keep minimal.

Bounds & resource limits: table of compile-time constants: K_MAX_HANDLES (e.g., 1024 per process default, configurable), K_IPC_WORDS (8), K_MAX_WAITQ? etc. Message payload max inline 8 words (64 bytes); extended payload up to 4 KiB copied (or maybe better: extended payload via shared frame only? "Direct. No overengineering." — inline 8 words + optional one frame grant per message (descriptor passing). That's capability-passing: you can transfer a handle to the receiver. Frame grants = the data plane. So no bulk copy in kernel at all: messages carry words + up to N handle grants (e.g., 2). Bulk data = map shared frames. This is clean and fast. Yes: message = 8 words + up to 2 handle grants transferred. Kernel validates rights for transfer (must have GRANT right on handle).

Shared memory model: `frame` objects = physical memory objects created via `k_frame_create(size log2, flags)` — physically contiguous? Keep simple: frames are power-of-two sized, physically contiguous (from PMM). Mapping: `k_vmap(as, frame_handle, vaddr, len, perms, flags)` where vaddr optional (let kernel pick). Page-granular only. COW? No — anti-goal. Lazy alloc? Flag FRAME_ZERO, FRAME_CONTIG optional? Keep: frame_create(order, flags) — zeroed by default (security), phys-contig default. Fast path note: zeroing cost accepted; alternative anti-goal? Provide FRAME_NOREZERO? Keep simple: always zero, note perf cost, allow future. Hmm "Fast" — provide flag FRAME_DIRTY to skip zeroing (kernel-internal only). Actually keep flag K_FRAME_NOZERO allowed only for handle with right? Simpler: always zero on create; document. Also mapping to user requires rights: FRAME rights = READ/WRITE/MAP/GRANT.

Memory objects shared: map same frame handle in two processes -> shared memory. Simple. Also `as` (address space) object handles for cross-process mapping: a process holding an AS handle with MAP right can map frames into it (that's the pager pattern).

Concurrency model details:
- Kernel preemption model: SMP kernels with runqueues, per-CPU. Spinlocks irqsave. Sleeping locks only where blocking allowed. One big lock? No — small locks with documented order. Lock ordering: per-object locks < handle-table lock < scheduler run lock < pmap locks? Define hierarchy levels.
- Memory ordering: provide `k_atomic_*` with acquire/release semantics; define arch mapping to instructions (lock cmpxchg / ldaxr-stlxr / lr-sc / lock cmpxchg for i686). Provide `k_barrier()` full barrier.
- Wait queues: linked list of waiters embedded in TCB (intrusive list) — no allocation on wait.
- Priority inheritance? Anti-goal? It matters for real-time but adds complexity. Small RTOSes often do basic PI on mutexes. Given "no overengineering," make PI an anti-goal; mutexes use FIFO wait queues; document risk. Hmm, but "rigorous" might expect it. I'll put simple priority-boost? No — keep FIFO, list PI as anti-goal with rationale (no user priorities in v1; scheduler is RR). AuraOS scheduler presumably simple. State assumption.

Interrupt handling & IPC link: interrupt notification object: `k_irq_attach(notif, irq)` driver binds an IRQ to notification; on IRQ, kernel posts bit 0 of notification and wakes waiters. That's the standard mechanism, include since it ties IPC to drivers. It's in scope? Component is concurrency + IPC + shm; IRQ delivery via notifications fits "IPC". Keep small section.

Data structures: 
```c
typedef struct k_kobj { k_u8 type; k_u8 flags; k_atomic_ref refs; k_vtordtor... } 
```
Keep: `struct k_obj { const struct k_obj_type *type; k_atomic_u32 refs; }` with type holding name + destroy fn. 

Endpoint:
```c
struct k_ep {
  struct k_obj obj;       /* type K_OBJ_EP */
  k_spinlock_t lock;
  struct k_wq waiters;    /* receivers queued (and senders?) */
  ...
};
```
Rendezvous model: queue contains both senders and receivers waiting; when match occurs, direct transfer. If a receiver waits, sender transfers and either continues (send) or blocks for reply (call). Implement: two wait queues or one list with tags. Keep two lists: `rx_wait`, `tx_wait`? But a call blocks on reply after rendezvous — the receiver holds reply token. Reply handled by waking that thread.

Message struct (wire):
```c
struct k_msg {
  k_u64 w[8];              /* payload words (LE) */
  k_handle grants[2];      /* capability grants, transferred */
  k_u8 n_words;            /* actual words 1..8 */
  k_u8 n_grants;           /* 0..2 */
  k_u8 _pad[6];
};
```
Hmm, keep fixed size,