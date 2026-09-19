# AuraOS adversarial architecture review

## Executive verdict

**The current specifications are not implementation-ready.** They describe a plausible small OS surrounded by mutually inconsistent performance promises, incomplete isolation boundaries, and several compatibility claims that are technically false.

The viable core is:

> A small, protected-process C operating system with a serial console, read-only boot filesystem, native ELF applications, a software desktop, and carefully bounded device support.

The following are **not** yet credible commitments:

- Transparent execution of arbitrary APK, PE, or Linux ELF binaries.
- Universal service restart without client-visible failure.
- Safe storage flushing after arbitrary kernel corruption.
- Portable sub-500 ms reset or sub-5 ms application cold starts.
- Modern glass rendering at 60 FPS across all target hardware.
- A complete GUI application memory budget that excludes its pixel buffers.
- VirtIO-GPU acceleration across QEMU, VirtualBox, and VMware.
- One executable binary across four instruction-set architectures.

This is a **specification audit**, not a source-code or repository verification. “Approved,” “verified,” and consultant attribution are not evidence of an implemented or tested contract.

**Severity notation**

- **P0:** Resolve before freezing interfaces or writing dependent implementation.
- **P1:** Resolve before exposing the subsystem to untrusted applications or persistent data.
- **P2:** Resolve before making support or performance claims.

---

# 1. Critical Blind Spots & Architectural Holes

## 1.1 P0 — No authoritative architecture or specification precedence

The documents disagree on fundamental decisions:

| Area | Conflicting specifications |
|---|---|
| Identity | AuraOS, Aeris, Aero; `libaeris-ui`, `libaglass`, `libaura-ui` |
| Compatibility | Universal runner versus explicitly rejecting universal compatibility |
| Rendering | Glass everywhere versus opaque legacy profile |
| Filesystem ownership | Kernel VFS versus restartable user-space filesystem service |
| Recovery | Safe reset after corruption versus flushing transactions from the corrupted kernel |
| Memory model | “Flat memory” philosophy versus protected user address spaces |
| Support | Four architectures versus an almost exclusively x86 roadmap |
| Dependencies | Small auditable dependencies versus Mesa/LLVM/DBT/Android compatibility |
| ABI portability | Separate architecture builds versus “identical binary” everywhere |

**Consequence:** Different contributors can correctly implement incompatible designs.

**Required decision:** One normative architecture document, explicit supersession of obsolete claims, and a support matrix distinguishing **implemented**, **experimental**, and **not supported**.

“Flat memory” may mean a flat virtual-address model. It must not mean all applications share writable physical memory.

---

## 1.2 P0 — The security model does not exist

“Explicit capabilities” appears without defining:

- What a capability is.
- Which kernel objects have handles.
- Which rights each handle grants.
- How handles are inherited, transferred, duplicated, revoked, and closed.
- Who may map physical memory, program devices, or own interrupts.
- Who may kill processes, change priorities, inspect memory, or view telemetry.
- Whether capsule code is trusted.
- Whether applications can read other applications’ shared surfaces.
- Whether the compositor can access arbitrary process memory.
- Whether arbitrary native programs can open raw storage.

**Process isolation is not a security model.**

A process with unrestricted device access can bypass isolation through DMA. A process with unrestricted task-control syscalls can terminate the desktop without exploiting anything.

**Minimum:** A single-user system with per-process handle tables and explicit rights is sufficient. A full multi-user Unix permission model is not required initially.

Memory protection, safe user copying, handle validation, and privilege enforcement belong in **Phase 2**, not Phase 6.

---

## 1.3 P0 — The execution model is underspecified

Missing contracts include:

- Process versus thread ownership.
- Thread creation and termination.
- `spawn`, exit status, wait, and zombie reaping.
- Parent death and orphan adoption.
- Cancellation of blocked operations.
- User fault delivery and process termination.
- TLS.
- Floating-point/SIMD context.
- Stack growth or fixed stack limits.
- Monotonic timers and sleep.
- File descriptor inheritance.
- Application ABI and startup stack.
- Dynamic linking policy.
- Signals, if any.

The specifications use `SIGTERM`, `SIGKILL`, PTYs, Android runtimes, TCC, and SDL2 without specifying the process semantics they require.

**Minimal choice:** Start with `spawn/exit/wait`, one thread per process if necessary, cooperative application-close events, and privileged forced termination. Do not name Unix signals unless implementing their semantics.

---

## 1.4 P0 — No kernel concurrency or blocking rules

A preemptive kernel must define:

- Where preemption is allowed.
- Whether kernel code can be preempted.
- Which operations can run in interrupt context.
- Which locks permit sleeping.
- Lock ordering.
- Whether page faults can occur while holding a lock.
- When process objects may be freed.
- How interrupt completion races with cancellation.
- Which shared data uses atomics.
- Whether SMP is supported or merely anticipated.

Even on one CPU, interrupts and preemption create races.

“Ring buffers,” “fixed arrays,” and “zero locks” do not establish correctness.

**Minimum:** Single-core, non-preemptible kernel outside explicit blocking points; short interrupt-disabled critical sections where justified; a documented lock order. Add SMP only after lifetime rules are sound.

---

## 1.5 P0 — No global resource exhaustion policy

The design contains many individually “small” components but no complete memory accounting.

Missing limits:

- Threads and processes.
- Per-process page tables and kernel stacks.
- Shared-memory bytes and surface dimensions.
- Handles, queued messages, and outstanding requests.
- Files and directory entries.
- Font caches and image decode buffers.
- Terminal scrollback.
- Compiler input/output sizes.
- Audio buffers.
- Capsule translation caches.
- Disk cache.
- Supervisor restart attempts.

A malicious application does not need an exploit if it can allocate surfaces until the compositor or kernel fails.

**Required:** Every externally triggerable allocation must have:

1. Overflow-safe size validation.
2. A resource owner.
3. A limit.
4. A failure result.
5. A destruction path.

Define OOM behavior before the desktop: fail the requesting operation, reclaim eligible caches, and preserve a kernel reserve. Do not silently assume swap exists.

---

## 1.6 P0 — Booting is confused with having a usable platform

The roadmap reaches an interactive compiler session without providing a complete input/output/storage path on most targets.

Missing or incomplete:

- Writable storage for source files.
- Block-device drivers.
- Partition handling.
- A writable filesystem.
- USB host controllers and HID.
- SD/eMMC support.
- Board-specific input.
- RTC and monotonic clock support.
- Reset and watchdog discovery.
- Firmware/device-tree parsing.
- Entropy.

A read-only initramfs is enough to boot and execute test programs. It is not enough to “write a C program” unless a bounded writable RAM filesystem or equivalent scratch storage exists.

**Minimal solution:** Initramfs plus a small RAM filesystem and serial input. Persistent writes come later.

---

## 1.7 P1 — Networking, audio, and terminal infrastructure are missing

The application suite assumes infrastructure that is not specified.

### Networking

Requires at least:

- A supported NIC and transport.
- Packet buffer ownership.
- ARP/NDP and IP policy.
- UDP/TCP or a deliberately smaller protocol set.
- DHCP or static configuration.
- DNS.
- Socket blocking semantics.
- Timeouts and cancellation.
- TLS, trusted roots, time validation, and entropy for an AI bridge.

### Audio

Requires:

- A supported physical or virtual device.
- Sample formats and rate negotiation.
- Buffer ownership.
- Underrun behavior.
- Mixing and clipping rules.
- Device-loss behavior.
- Playback position semantics.

### Terminal

Requires:

- PTY or equivalent duplex byte stream.
- EOF, hangup, and backpressure.
- Resize notifications.
- A defined escape-sequence subset.
- Bounded parser state.
- UTF-8 width and combining-character policy.

**Do not schedule applications before their foundational contracts.**

---

## 1.8 P1 — Device ownership and driver placement are undefined

The fault-containment specification assumes that drivers can be killed and restarted. The architecture describes a monolithic kernel with unspecified driver placement.

These are different failure domains:

1. Kernel-resident driver.
2. User-space service calling a kernel-resident driver.
3. User-space driver with MMIO/IRQ access.
4. User-space driver controlling DMA through an IOMMU.

Restarting an audio service does not recover a corrupted kernel audio driver. Moving a DMA driver into user space does not isolate its DMA on machines without an IOMMU.

**Required:** For each driver, document its execution domain, privileges, DMA authority, and supported recovery operation.

---

## 1.9 P1 — Desktop ownership, scheduling, and input trust are absent

Missing:

- Who owns display modes and scanout.
- Whether display modes may change.
- Pixel formats and pitch.
- Input focus and capture.
- Key-repeat ownership.
- Window close versus process kill.
- Clipboard ownership and size limits.
- Surface lifecycle during resize.
- Hidden/minimized window behavior.
- Vsync availability.
- Damage propagation through translucent windows.
- Frame pacing when no presentation interrupt exists.
- Display server authentication after restart.

A transparent window depends on the pixels below it. Dirty rectangles cannot be handled independently as if every window were opaque.

---

## 1.10 P1 — No hostile-input model for first-party applications

The small desktop includes a substantial parser attack surface:

- Fonts.
- Images.
- UTF-8 and terminal escape sequences.
- ELF, PE, ZIP/APK, RTF, CSV, Markdown.
- Themes and INI files.
- Formula expressions.
- Capsule profiles and executable glue.

Specific missing policies:

- Maximum decoded image dimensions.
- ZIP decompression limits and path traversal prevention.
- Formula depth, cyclic references, and recalculation limits.
- RTF nesting and embedded-object restrictions.
- Safe handling of malicious fonts.
- Terminal clipboard/control extensions.
- File-save atomicity and unsaved-document recovery.

“Small library” does not imply safe on attacker-controlled data.

The AI bridge also needs a trust boundary: no automatic shell execution, build execution, source upload, or credential access based solely on model output.

---

## 1.11 P2 — Performance claims have no measurement contract

“Cold start,” “RAM,” “zero-copy,” and “first paint” are undefined.

Questions the specifications must answer:

- Does RAM include executable pages, stacks, page tables, shared libraries, surfaces, and server-side allocations?
- Is cold start measured from rotating disk, RAM filesystem, or a warm page cache?
- Is launch timing measured until window creation, rendering completion, or visible scanout?
- What resolution, font set, CPU, and memory speed?
- What dataset produces the spreadsheet result?
- Are dropped frames allowed during storage I/O or memory pressure?

“Zero UI frame drops” is not a defensible cross-platform contract.

A spreadsheet displaying 100,000 rows can virtualize the **view**. That does not make the cell data, strings, formulas, dependency graph, and undo history fit in 2 MB.

---

## 1.12 P0 — No verification strategy for dangerous boundaries

Booting successfully is weak evidence.

Missing release criteria include:

- Malformed loader and syscall inputs.
- OOM at every allocation site.
- Kill during blocked I/O.
- IPC saturation.
- Handle reuse.
- Service crash during a request.
- Shared-memory mutation during parsing.
- Integer overflow.
- Power interruption during filesystem writes.
- Device reset during DMA.
- ARM weak-memory-ordering tests.

**Required:** Host-side tests for parsers and allocators, sanitizers where applicable, fuzzing of externally supplied structures, and emulator fault injection. These are not optional “hardening” after the application suite.

---

# 2. Contract & Interface Flaws

## 2.1 `sys_telemetry`: the proposed ABI is unsafe and internally inconsistent

### A. No buffer size

```c
sys_telemetry(buf, flags)
```

Cannot safely evolve without a caller-provided length.

A newer kernel copying a larger structure into an older application’s buffer corrupts user memory.

**Fix:** Supply buffer length; return bytes written or required capacity, with explicitly documented short-buffer behavior.

### B. C structure layout is not a portable wire ABI

Fixed-width fields do not eliminate padding and alignment differences.

The three `uint8_t` fields before `uint64_t cpu_ticks` are particularly problematic across ABI alignment rules.

**Fix:**

- Explicit padding.
- Defined field offsets and record sizes.
- Compile-time assertions on all architectures.
- No pointers, native enums, `long`, `size_t`, or bitfields in shared ABI records.
- Define endianness for serialized formats.

Do not solve this indiscriminately with `__attribute__((packed))`; unaligned 64-bit access is a portability and performance hazard.

### C. A large snapshot cannot be “one atomic copy”

A structure containing hundreds of process records cannot be copied atomically.

On 32-bit targets, unsynchronized 64-bit counter reads may tear. Process objects may disappear while the snapshot is traversed. Global coherence requires a synchronization policy.

**Fix:** Promise a **bounded, timestamped, approximately contemporaneous sample**, not a machine-wide atomic instant. Use short protected reads and stable object lifetimes.

Do not disable interrupts for the entire user-buffer copy.

### D. No process identity beyond reusable PID

A process can exit and another can reuse its PID between refreshes. CPU deltas may then be attributed to the wrong process.

Task-manager actions can target the wrong replacement process.

**Fix:** Include a generation/instance identifier; process-control operations validate the pair or use a process handle.

### E. `cpu_affinity` supports only eight CPUs

`uint8_t` is not a modern CPU mask. `AURA_PROC_MAX_CORES` is also undefined.

**Fix:** Remove affinity from the initial ABI or choose an explicit CPU cap and a correctly sized mask. Affinity belongs to threads unless process-wide behavior is explicitly