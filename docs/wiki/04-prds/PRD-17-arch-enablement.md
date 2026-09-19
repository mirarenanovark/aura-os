The user wants a rigorous PRD for AuraOS Component #17: Per-Architecture Enablement for i686, x86_64, aarch64, armv7. This is a freestanding C OS project. I need to write production-grade markdown with exactly 6 sections:

1. Executive Summary & Scope Boundary
2. Core Requirements (Functional & Non-Functional)
3. Data Structures, C Interfaces & Wire ABI Contracts (concrete C code blocks)
4. Failure Modes, Bounds & Resource Limits
5. Acceptance Criteria & Test Gates
6. Anti-Goals

The philosophy: Clean. Simple. Small. Fast. Direct. No overengineering.

Let me think about what "Per-Architecture Enablement" means in an OS context. This component is the layer that makes the same kernel source build and boot on 4 architectures. It covers:

- Build system integration: per-arch toolchain flags, -march settings, code models, ABI selection (e.g., i686 uses regparm? Probably keep stack-based ABI for simplicity — actually for freestanding kernels, i686 often uses -mregparm=3 or -mabi=... Let's keep it simple: -msoft-float, no SSE in kernel, -ffreestanding, -mno-red-zone for x86_64, -mgeneral-regs-only for aarch64, etc.)
- Arch detection macros: AURA_ARCH_X86_64 etc., compile-time selection via #if
- Boot protocol contracts: Multiboot2 (i686, x86_64), and for ARM — typically custom/booti (Android boot image) or U-Boot booti for aarch64, and U-Boot legacy/bootm for armv7. Given "Clean. Simple. Small." — Multiboot2 for x86 both, U-Boot booti (raw Image + DTB) for aarch64, and zImage + DTB via U-Boot bootz for armv7. Or keep it simpler: U-Boot booti for aarch64, bootz for armv7.
- Early boot: entry points per arch, setting up stacks, BSS clear, relocation to higher half / address layout per arch.
- Context switch, trap frames, interrupt controllers (APIC on x86, GICv2/GICv3 on aarch64, GICv2 or GICv400 on armv7... but wait — interrupt controllers might be a separate component. Component #17 is "Per-Architecture Enablement" — the enablement layer, i.e., the arch abstraction seam, not the full drivers. So it defines the arch_ops interface that other components consume, and delivers the minimal per-arch implementation to bring each arch to a common "kernel skeleton" level: boot to C entry, console output (UART), timer tick, traps, context save/restore, barriers/cache ops interfaces, memory model hooks (MMU page table formats might be Component for VM — but the arch side of page tables probably lives here or in a paging component; I should draw a boundary: this component provides the arch seam and bring-up, not policy).

Let me define the scope boundary carefully:
- IN SCOPE: build integration (flags, linker scripts per arch), arch identification macros, boot protocol adapters (multiboot2 info parsing struct, U-Boot DTB pointer passing), per-arch entry assembly to C landing, per-arch trap frame definitions and entry/exit, context switch primitives, atomic/barrier intrinsics mapping, cache maintenance interface, per-arch early console (existing component dependency — e.g., Component #? console driver is separate; this component supplies the arch-side serial register base plumbing? Hmm. To avoid overlap: "Per-architecture enablement" = the seam + bring-up glue. Drivers like UART drivers belong elsewhere; but for bring-to-life each arch needs a minimal boot console. I'll say: this component delivers the boot-time architectural prerequisites and delegates device drivers to their components, EXCEPT it owns the minimal "bring-up console" path via arch_console_putc dispatch which calls into the serial component. That keeps it clean.)

Also: FPU/SIMD state policy — kernel saves lazily or eagerly? Keep simple: eager save of full register bank on context switch for arm with FP, and on x86 use xsave area of fixed max size. But maybe FP enablement is in scope as "arch enablement" — yes: enabling FPU/SIMD per arch (CR4.OSFXSR, CPACR for ARM), and defining the save area layout. That's classic arch enablement.

Wire ABI contracts: The "wire ABI" here means the layout contracts that cross the user/kernel boundary or between boot protocol and kernel, and the in-memory data contracts that must be stable: trap frame layout, register save area, boot info block passed to kernel C entry, auxv-like arch flags for userspace? Since AuraOS — probably has syscalls. The per-arch ABI contract includes syscall numbers identical across arches but per-arch invocation convention (int 0x80/syscall for x86, svc for ARM). This component owns that contract definition.

Let me draft concrete C code:

```c
/* kernel/arch/arch.h */
typedef enum {
    AURA_ARCH_I686 = 1,
    AURA_ARCH_X86_64 = 2,
    AURA_ARCH_AARCH64 = 3,
    AURA_ARCH_ARMV7 = 4,
} aura_arch_id_t;

#if defined(__i386__)
  #define AURA_ARCH AURA_ARCH_I686
  ...
#elif defined(__x86_64__)
...
```

arch_ops structure (small, direct):

```c
typedef struct arch_ops {
    void (*early_init)(void);
    void (*idle)(void);
    void (*halt)(void) __attribute__((noreturn));
    void (*context_switch)(arch_ctx_t *from, const arch_ctx_t *to);
    void (*fpu_enable)(void);
    ...
} arch_ops_t;
```

Hmm, but "no overengineering" — maybe avoid a full ops table and use static inline + weak symbols? A compile-time-selected single implementation via #include "arch/<name>/..." is simpler. But an ops table of ~10 function pointers is fine and idiomatic for OS. Actually cleanest for a 4-arch kernel: each arch provides `arch_impl.c` with the same symbol names, selected at link time. No function-pointer table at all — direct calls, faster, smaller. That aligns with "Fast. Direct. No overengineering." I'll spec: interface = header declarations; implementation = per-arch translation unit selected at link time; no dispatch tables, no runtime detection. Add a compile-time static_assert that exactly one arch is defined.

Boot ABI contract: register/state at kernel C entry:

```c
typedef struct aura_bootinfo {
    u32 magic;             /* AURA_BOOT_MAGIC 'AUB1' */
    u32 arch;              /* aura_arch_id_t */
    u32 arch_flags;        /* bit 0: pae, bit 1: pat, ... */
    u64 cmdline;           /* physical pointer or 0 */
    u32 cmdline_len;
    u64 memmap_phys;
    u32 memmap_entries;
    u64 fb_phys; u32 fb_w, fb_h, fb_pitch, fb_bpp;
    u64 dtb_phys; u32 dtb_size;   /* ARM only; 0 on x86 */
    u32 reserved0;
} aura_bootinfo_t;  /* packed? aligned 8 */
_Static_assert(sizeof(aura_bootinfo_t) == 64, "bootinfo is a wire ABI");
```

Keep it fixed 64 bytes, version field. Good — a wire ABI contract with _Static_assert size check.

Trap frame per arch — these are wire ABI (user/kernel boundary layout, also ptrace-ish later). Provide x86_64 and aarch64 examples concretely, describe i686 and armv7 (or give all four — user wants concrete code; I'll give all four but keep tight). Actually four full trap frames is a lot of lines but it's the meat of the component. Let me write them compactly.

x86_64 trap frame:

```c
typedef struct __attribute__((aligned(16))) trapframe_x86_64 {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, err;
    u64 rip, cs, rflags, rsp, ss;
} trapframe_x86_64_t;
```

i686:

```c
typedef struct trapframe_i686 {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    u32 int_no, err;
    u32 eip, cs, eflags, esp, ss;
} trapframe_i686_t;
```

aarch64 (EL0 → kernel via sync/irq):

```c
typedef struct trapframe_aarch64 {
    u64 x[29];      /* x0..x28 */
    u64 fp, lr;
    u64 spsr_el1, elr_el1, sp_el0;
    u64 esr_el1, far_el1;
} trapframe_aarch64_t;
```

armv7:

```c
typedef struct __attribute__((aligned(8))) trapframe_armv7 {
    u32 r[13];   /* r0..r12 */
    u32 sp, lr, pc, cpsr;
    u32 dfsr, ifsr, far, adfsr, aifsr;
} trapframe_armv7_t;
```

Syscall ABI contract table:

| arch | entry | nr reg | args | ret | clobber |
- x86_64: `syscall`/`sysret`, rax=nr, rdi rsi rdx r10 r8 r9, ret rax, rcx/r11 clobbered
- i686: `int 0x80`, eax=nr, ebx ecx edx esi edi ebp, ret eax
- aarch64: `svc #0`, x8=nr, x0..x5, ret x0
- armv7: `svc #0`, r7=nr, r0..r5, ret r0

Context switch struct per arch: callee-saved only.

```c
typedef struct arch_ctx_x86_64 { u64 rbx, rbp, r12, r13, r14, r15, rsp; } ...
aarch64: x19..x28, fp, lr, sp + fpu region
```

FPU save area: define layout + alignment, xsave area 576/832? Keep: x86_64: 832-byte XSAVE area aligned 64 (covers AVX2); i686: 512-byte FXSAVE aligned 16; aarch64: 512 bytes (32×128-bit V regs + FPCR/ FPSR 8 bytes) aligned 16; armv7: 264 bytes? VFPv4: 32 × 64-bit = 256 + FPSCR 4 → 260, round to 264 aligned 8. Keep fixed sizes with static asserts.

Build flags table per arch:

Common: -ffreestanding -fno-stack-protector -fno-pic? (x86_64 kernel may want -mcmodel=kernel with -mno-red-zone; aarch64 -mgeneral-regs-only -mstrict-align? strict-align optional; -mcpu for armv7 = armv7-a+... ). Also -mno