The user wants a rigorous PRD for AuraOS Component #15: Native TinyCC & MicroPython Toolchain Integration. AuraOS is a freestanding C OS supporting 4 architectures. The PRD must follow exactly 6 sections, production-grade markdown, with concrete C code blocks in section 3.

Let me think about what this component actually is: integrating TinyCC (a small C compiler) and MicroPython natively into AuraOS so users can compile and run C and Python on the OS itself. This is a "self-hosting" toolchain component — no cross-compilation from a host, everything native on the target.

Key design constraints:
- Freestanding C (no libc assumptions unless AuraOS provides its own minimal libc — typically these hobby/production OSes have a small POSIX-ish layer)
- 4 architectures: likely x86_64 (AMD64), i386 (IA-32), AArch64, and... could be RISC-V 64. Let me pick: amd64, i386, aarch64, riscv64. That's a sensible 4-arch spread. I'll state this clearly.
- Clean. Simple. Small. Fast. Direct. No overengineering.

So the PRD should be minimal: a toolchain subsystem that exposes:
1. TinyCC integrated as a library (libtcc) compiled freestanding against AuraOS's runtime, with backends for the 4 arches, generating native code into memory, executing it.
2. MicroPython ported as a component with a minimal port layer (mphal / machine module bound to AuraOS syscalls), bytecode VM running natively.
3. A wire ABI between the two? Hmm — "Wire ABI Contracts" — maybe:
   - The in-memory code object ABI (what TinyCC emits and how AuraOS loads/jumps to it)
   - The system call surface exposed to both runtimes (the "guest ABI")
   - The integration IPC/pipe ABI for the `tcc` and `micropython` userland binaries
   - Maybe a REPL protocol over the console device.

I should define concrete data structures:
- `aura_toolchain_api` dispatch table
- memory allocation hooks for TinyCC (tcc_set_malloc_hooks or similar)
- `aura_mp_port` structure (MicroPython port hooks)
- object module header: `aura_objhdr` with magic, arch id, flags, entry offset, section table
- syscall shim table shared by both runtimes: `aura_guest_abi` with versioned vtable
- bounds: max line length for REPL, max AST nodes? TinyCC handles its own. Max heap carveouts for MicroPython GC heap size.

Failure modes section:
- OOM during compile
- unsupported construct (e.g., long double, varargs edge cases in TinyCC)
- codegen bounds exceeded
- MicroPython GC exhaustion
- stack overflow in parser recursion
- illegal instruction from bad code — sandbox? Careful: "no overengineering" — AuraOS likely doesn't have a full MMU sandbox on all arches (i386 might). I should specify a pragmatic mitigation: compiled code runs at reduced privilege where the MMU exists; on arches without per-process isolation... Actually AuraOS presumably has processes. Keep it simple: compiled code is a process; the kernel's existing fault handling catches wild pointers.

Resource limits:
- TCC: max source file size, max symbol count, compile timeout (watchdog ticks)
- MicroPython: fixed GC heap (e.g., 512 KiB default, 4 MiB max), max recursion depth (MP_STACK_CHECK), import path fixed.

Acceptance criteria with test gates:
- Bootstrap test: compile a hello-world C on-target on each of 4 arches, run it, exit code 0.
- Self-compile (TCC compiling TCC)? That may be overengineering — but it's a classic gate. Maybe optional/stretch. Actually "no overengineering" — I'd make "tcc compiles a nontrivial test corpus" the gate and leave TCC-bootstrap as explicitly out of scope or a stretch. Hmm, TinyCC famously can self-compile. But requiring full self-hosting of the OS is beyond component scope. I'll put it in Anti-Goals or as a non-gate.
- MicroPython: run a test suite (microtests), REPL echo over console, timer interrupts during GC proving no reentrancy bugs.
- Binary size budgets: tcc binary < N KiB, micropython < M KiB, libtcc.a size, RAM at runtime.
- Compile time bound: hello.c compiled in < X ms on reference hardware.
- Determinism: same input → same output hash on all arches where applicable.

Anti-Goals:
- No C++ support, no full libc (only minimal aura-libc subset), no linker beyond static single-unit or simple multi-file, no dynamic loading of shared libs (or minimal?), no gdb/debugger, no optimization beyond what TCC offers (-O0 essentially), no pip/network package manager, no frozen modules beyond fixed set? Actually frozen modules are a size win — allow a fixed frozen set, but no arbitrary freezing at runtime. No threading in MicroPython (no GIL work), no native emitters for MicroPython beyond... MicroPython has native emitter per-arch; that's scope creep — exclude or restrict to bytecode VM only + @micropython.native explicitly out. No vi/emacs, obviously. No cross toolchain in this component.

Architecture notes: TinyCC has backends for x86-64, i386, arm (aarch64 support exists in tcc for arm64 — yes, tcc supports aarch64), riscv64 support exists in Mobius/landley forks and upstream merged riscv64 in 2021 (tinycc has riscv64 backend). Good — all 4 arches plausible.

MicroPython ports: unix port as reference; we do a "bare metal" style port with our own mp_port. Supported on all 4 arches since it's a C VM (interpreter), arch matters only for the VM's word size and optional native emitters (excluded).

Wire ABI: The "wire ABI contract" should include:
1. `AURA_TCB` — toolchain binary interface: how generated code calls back into the runtime (the guest shim).
2. Object container format for cached compile artifacts: `AOC` (Aura Object Container) with magic "AOC1", arch field, so an .aoc built on i386 is rejected on riscv64 with EARCH.
3. Console/REPL framing protocol: newline-delimited, ESC sequences minimal.
4. Exit-status ABI: guest → runtime exit code propagation.

Let me now think about concrete C code blocks:

```c
/* kernel/include/aura/toolchain.h */

#define AURA_TOOLCHAIN_ABI_VERSION 1

typedef enum {
    AURA_ARCH_I386 = 1,
    AURA_ARCH_AMD64 = 2,
    AURA_ARCH_AARCH64 = 3,
    AURA_ARCH_RISCV64 = 4,
} aura_arch_t;
```

Dispatch table:

```c
typedef struct aura_toolchain_ops {
    uint32_t abi_version;
    aura_arch_t arch;
    aura_status_t (*compile_c)(const aura_compile_request_t *, aura_compile_result_t *);
    void (*destroy_result)(aura_compile_result_t *);
    ...
} aura_toolchain_ops_t;
```

Compile request/result:

```c
typedef struct {
    const char *source_path;     /* null-terminated, aura-fs path */
    const char *argv[16];        /* max 16 tcc args */
    uint32_t opt_flags;          /* AURA_TC_F_* */
    uint32_t timeout_ms;
} aura_compile_request_t;

typedef struct {
    void   *text;                /* executable mapping, RWX granted by vm_grant_exec */
    size_t  text_len;
    aura_arch_t arch;
    int     (*entry)(int, char **);
} aura_compile_result_t;
```

Object container header:

```c
typedef struct {
    char     magic[4];   /* "AOC1" */
    uint16_t arch;       /* aura_arch_t */
    uint16_t flags;
    uint32_t text_off, text_len;
    uint32_t sym_off, sym_len;
    uint32_t rel_off, rel_len;
    uint32_t crc32;
} aoc_header_t;  /* packed, 32 bytes */
```

Guest syscall shim (shared by TCC-generated binaries and MicroPython port):

```c
#define AURA_GUEST_ABI_VERSION 1

typedef struct {
    uint32_t version;
    long (*write)(int fd, const void *buf, size_t n);
    long (*read)(int fd, void *buf, size_t n);
    long (*open)(const char *path, int flags);
    long (*close)(int fd);
    long (*exit)(int code) __attribute__((noreturn));
    void *(*mmap_anon)(size_t n);
    long (*munmap)(void *p, size_t n);
    uint64_t (*clock_ms)(void);
} aura_guest_abi_t;

extern const aura_guest_abi_t aura_guest; /* populated by the loader before entry */
```

MicroPython port layer:

```c
/* comp15/micropython/mp_aura_port.c */
typedef struct {
    void (*uart_tx)(const char *s, size_t len);
    int  (*uart_rx)(char *buf, size_t len, int block_ms);
    uint64_t (*ticks_ms)(void);
    void (*fatal)(const char *msg) __attribute__((noreturn));
} mp_aura_hal_t;

/* fixed at link time; no runtime mutation */
extern const mp_aura_hal_t mp_aura_hal;
```

GC config:

```c
#define MP_AURA_HEAP_MIN  (128 * 1024)
#define MP_AURA_HEAP_DEFAULT (512 * 1024)
#define MP_AURA_HEAP_MAX (4 * 1024 * 1024)
```

Memory hooks for TCC:

```c
typedef struct {
    void *(*alloc)(size_t n, void *ctx);
    void *(*realloc)(void *p, size_t n, void *ctx);
    void  (*free)(void *p, void *ctx);
    void *ctx;
    size_t budget_bytes;      /* hard cap: compile fails at budget, never OOMs kernel */
    size_t used_bytes;
} aura_tc_allocator_t;
```

TCC integration: we vendor tcc source under `comp15/tcc/` with a pinned commit; patch surface limited to `N` lines (bound: patch diff ≤ 400 lines total across arches, tracked). That's a good rigor element: vendor + patch budget to avoid fork drift.

Userland commands:

```
tcc [-o out] file.c          → produces AOC or runs directly (-r)
upy [file.py | -i]           → runs script or drops to REPL
```

REPL wire protocol: line-based, max 256 bytes per line, `\n` terminated, Ctrl-C = KeyboardInterrupt via guest `raise`, etc.

Also the "wire" between compiler service and kernel: is the toolchain a kernel component or userland? Given "freestanding C OS", components are typically kernel + userland. The toolchain should be userland processes; the kernel provides only: exec mapping (RWX page grant), the guest ABI vector passed in a fixed register or AT-vector-like aux block. That's the true "wire ABI": how the runtime hands control to JIT'd code — entry state contract per arch: which registers hold argc/argv/abi pointer.

Entry-state contract per arch (this is a genuinely rigorous wire ABI):

- i386: eax = argc, ebx = argv, ecx = &aura_guest — hmm System V i386 uses stack