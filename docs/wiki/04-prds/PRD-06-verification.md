The user wants a rigorous PRD for AuraOS Component #06: Host Testing, Fuzzing, Emulator Fault-Injection & Conformance. It's a freestanding C OS project with 4 architectures (likely x86_64, aarch64, riscv64, and one more — maybe i686 or loongarch64; I'll pick x86_64, aarch64, riscv64, and... let me think. Common 4-arch freestanding OS: x86_64, aarch64, riscv64, and loongarch64 is trendy, but i686 is more common. I'll go with x86_64, aarch64, riscv64, loongarch64 — actually safest to pick x86_64, aarch64, riscv64, and i686? Hmm. Many hobby OSes do x86_64, aarch64, riscv64. A fourth could be loongarch64. I'll state the 4 arches explicitly as x86_64, aarch64, riscv64, loongarch64 — or maybe safer to keep it generic-ish but the prompt says "4 arches" so I should name them. I'll go: x86_64, aarch64, riscv64, loongarch64. Actually i686 is far more traditional for "4 arches" in teaching kernels. Hmm — for a modern host-tested OS, x86_64/aarch64/riscv64/loongarch64 is a modern set. I'll pick that and make it consistent throughout. Actually, let me reconsider: the choice matters little as long as it's consistent. I'll use x86_64, aarch64, riscv64, loongarch64 and abbreviate amd64/arm64/rv64/la64.

Key constraints:
- Freestanding C (no libc on target; host tests compile kernel code with a host harness)
- Clean. Simple. Small. Fast. Direct. No overengineering.
- Production-grade markdown.
- Six sections strictly in the given order.
- Section 3 must have concrete C code blocks: data structures, C interfaces, wire ABI contracts.

What is Component #06 about?
- Host testing: run kernel subsystem unit tests on the host OS (Linux/macOS) by compiling freestanding code with a host shim (host_ prefix). No device drivers under test; test pure logic (allocators, parsers, data structures, bitmap, buddy, slab, VFS logic, etc.).
- Fuzzing: libFuzzer/AFL++ style harnesses against parsers: ACPI table walker? Actually ACPI needs memory blob — fine, feed bytes. Fuzz targets: ELF loader, tar/initrd parser, ACPI MADT parse, DTB parser, string/number parsers, syscall arg validation, bitmap ops, buddy allocator with random ops, PE? No. Keep small.
- Emulator fault injection: QEMU machine checks, NMI injection, memory poisoning via KVM? Simplest: QEMU gdbstub to inject faults, or use QEMU's `-icount` determinism, plus a small "fault injector" that uses the QEMU monitor (QMP/HMP) to inject: device time-outs, disk errors (via `blkdebug`), memory unplug, NMI via `inject-nmi`, RNG with seed, etc. Also the kernel under test exposes a test control device (hypercall or MMIO "test port") so the guest can report pass/fail — conformance.
- Conformance: a conformance test suite run in the emulator per arch that validates boot, memory map handling, timer ticks, interrupt delivery, context switch, syscall ABI stability (wire ABI contracts), etc. Produces a machine-readable results table (TSV/JSON?) — keep simple: line-based text protocol over the debug console (e.g., "TAP" style). TAP (Test Anything Protocol) is simple and well-known: "1..N", "ok 1", "not ok 2". Clean and small. Good.

Design the component:

Components named:
- `host/` directory: `host/tests/` unit tests, `host/fuzz/` fuzz harnesses, `host/shim/` host shim layer (host_mem, host_io, host_assert, host_printf).
- `tools/runvm` — tiny QEMU wrapper script? PRD shouldn't mandate language; say POSIX sh.
- `test/` on-target conformance: `conformance.c` with TAP output over debug serial.
- Fault injection: kernel-side `fault_inj.h` — a compile-time + runtime fault injection framework (fail malloc, fail page alloc, panic hook counts). Keep it small: `FAULT_INJECT(point)` macros with a fault table, deterministic PRNG (xorshift64* or splitmix64) seeded per test.
- Wire ABI: conformance TAP line format over UART 8N1, plus a "result register" mailbox for CI to read via QEMU exit code. QEMU can be exited with a specific code via the `-semihosting` exit or isa-debug-exit device (x86) — for 4 arches, use semihosting `SYS_EXIT` (AArch64/RISC-V semihosting is standard; x86 semihosting isn't standard...). Simpler: use QEMU's `-device isa-debug-exit` only on x86; for others, use semihosting. Alternatively: harness reads TAP from serial and decides pass/fail; QEMU killed by harness on `TAP-END` sentinel line + timeout. That is arch-neutral and simple. Use the sentinel + watchdog timeout approach; mention isa-debug-exit/semihosting exit as an optional fast path but not required. Keep: serial TAP + harness kills VM on sentinel.

Host testing approach: kernel source directories compiled for host with `-ffreestanding -fsanitize=address,undefined` and a shim providing memcpy, memset, etc. Tests written in a tiny framework: `AURY_TEST(name)` macros, no dynamic allocation, fixed static buffers.

Fuzzing: libFuzzer (clang) with `AUR_FUZZ_ENTRY(fn, buf, len)` per target; corpus seeds in repo; each target ≤200 LOC; OSS-Fuzz-style but not required. Deterministic: no clocks, no getpid. Provide `-runs=1000000` CI quick mode and nightly long mode.

Fault injection: 
- Kernel-side: `aur_fault_state` with counters and masks; `AUR_FAULT(point)` returns true when the counter for that point matches schedule; schedule is a fixed table from the harness or PRNG seed. Provide `fault_points.def` X-macro list.
- Host-side emulator faults: blkdebug for disk I/O errors; `inject-nmi`; `-icount shift=auto,align=off,sleep=off` for determinism; memory hot(un)plug events; `wakeup`; `system_reset`. The `runvm` wrapper accepts a fault spec file: simple key=value lines (QMP-lite). Actually simpler: fault spec = numbered list of "delay_ms, monitor command" pairs. Keep to a tiny grammar.

Conformance suite structure: numbered test groups G01..: boot & memory, timers, interrupts, scheduling, IPC, VFS, syscall ABI. Each test uses `TEST_ASSERT` macros; TAP output. Wire ABI: define `struct aur_abi_v1` syscall table? Conformance includes checking syscall numbers and struct layouts via static asserts on target and runtime reflection dump: a "ABI map" printed as `ABI <name>=<offset>` lines the host harness diffs against a golden file. That's a nice concrete wire contract: `ABIDUMP` mode prints `key=value` lines; harness compares with `abi/x86_64.golden` etc. Good and concrete.

Resource limits: max test binary size, max runtime per test, max fuzz iterations in CI, corpus size, timeout values (e.g., 120s boot gate, 60s per unit suite, 10min fuzz smoke, 60min nightly), max RAM for VM 256MiB, max lines of code per harness (e.g., 300), no external deps except clang/gcc, qemu.

Acceptance criteria: quantitative gates: all host tests pass with ASan/UBSan, zero leaks; fuzzers 1M execs without crash; conformance 100% pass on 4 arches × QEMU version pinned; fault-injection matrix N cases with expected outcomes (panic→clean shutdown or containment); ABI golden files match; CI time budget < X minutes.

Anti-goals: no bespoke test "framework" with reflection/plugins; no full system emulation alternative (no writing our own emulator); no formal verification; no random syscall fuzzing on target beyond seeded PRNG (actually on-target conformance could include seeded syscall fuzz — keep minimal: seeded PRNG syscall smoke allowed? Keep as anti-goal? The scope includes fuzzing on host; on-target fault injection covers error paths. I'll include a seeded syscall-shuffle smoke test in conformance — small.) Anti-goals: no code coverage gates above line coverage 85%? Keep: coverage measured, no hard gate beyond listed; no GPU/device-model fuzzing; no fuzzing of third-party decompressors beyond boundary checks; no support for non-QEMU emulators (no VMware/VirtualBox); no Windows host support (Linux + macOS only); no parallel test sharding framework; no dynamic test discovery.

Freestanding C constraints: `-ffreestanding`, no libc includes except `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<stdalign.h>`? In freestanding, those headers are provided by compiler. Host shim provides `memcpy/memset/memmove/memcmp` since freestanding code may call them (GCC may emit calls). Mention `-fno-builtin`? Actually keep: kernel provides its own; on host they map to libc via shim, but for sanitizer fidelity use interceptors. Keep simple: shim defines them calling through to host libc.

Structs to define:

1. Host unit test framework (`host/aur_test.h`):
```c
#define AUR_TEST_MAX 256
typedef struct {
    const char *name;
    void (*fn)(struct aur_test_ctx *t);
} aur_test;
typedef struct aur_test_ctx {
    unsigned check_fail;
    unsigned check_total;
    const char *cur;
    char msg[128];
} aur_test_ctx;
```
Macros: `AUR_TEST_REG(name, fn)` via section or array. On host, use constructor attribute or an X-macro `tests.def`. Freestanding-style: X-macro list `HOST_TESTS.def` — deterministic, no link-time magic. Good: "no constructor sections; X-macro only" — direct and simple.

`AUR_CHECK(t, cond)` / `AUR_CHECK_EQ`.

2. Fault injection (`kernel/aur_fault.h`):
```c
enum { AUR_FP_MAX = 32 };
typedef struct {
    uint64_t hits[AUR_FP_MAX];   /* times each point fired */
    uint32_t armed[AUR_FP_MAX];  /* remaining forced hits */
    uint32_t prng;               /* xorshift32 seed for probabilistic points */
    uint8_t  rate[AUR_FP_MAX];   /* 0..255 fire probability denominator */
    uint8_t  enabled;
} aur_fault_state;
int aur_fault_fire(int point);  /* returns 1 to inject */
```
Points enumerated in `fault_points.def` X-macro: `X(ALLOC_FAIL)`, `X(PAGE_FAIL)`, `X(IRQ_LOSS)`, `X(TIMER_SKEW)`, `X(MMIO_TIMEOUT)`, `X(CHECKSUM_BAD)`, ...

`AUR_FAULT(p)` macro → `aur_fault_fire(AUR_FP