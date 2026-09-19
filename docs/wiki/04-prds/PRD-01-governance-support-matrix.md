The user wants a rigorous PRD for AuraOS Component #01: Normative Architecture, Threat Model & Support Matrix. This is a freestanding C OS project targeting 4 architectures. The PRD must be production-grade markdown with 6 specific sections.

Let me think about what a Component #01 for an OS would look like. It's the foundational "normative architecture" document — this component would define the normative rules (coding standards, ABI contracts, threat model, supported hardware matrix) that all other components must conform to. As an actual *component* (code + docs), it would include:

- The normative architecture rules (Rust-free, freestanding C, no dynamic allocation in early boot, etc.)
- Threat model definitions (attacker classes, assets, trust boundaries)
- Support matrix (x86_64, aarch64, riscv64, and one more — let me pick... 4 arches: x86_64, aarch64, riscv64, i686? Or loongarch64? For "Clean. Simple. Small. Fast. Direct. No overengineering" — x86_64, aarch64, riscv64, i686 is a reasonable tiered matrix. Actually i686 adds 32-bit complexity; maybe x86_64, aarch64, riscv64, loongarch64. Hmm. Let me go with x86_64 (Tier 1), aarch64 (Tier 1), riscv64 (Tier 2), i686 (Tier 2). That's classic and defensible.)

Since it's a PRD, the component itself must ship *artifacts*: a machine-checkable support matrix (C header with feature macros), a threat model register (C enum + assertion table), ABI contract definitions (canonical C types, ABI version macros), and validation tooling/tests that gate CI. So Section 3 needs concrete C code blocks.

The component should be concrete enough to have:
- `aura/arc.h` — architecture contract macros (canonical integer widths, ILP32 vs LP64, endianness, page sizes, ABI version)
- `aura/threat.h` — threat model identifiers (enum of threat classes), trust boundary IDs, and a compile-time register table
- `aura/support.h` — support matrix: tier levels, per-arch target triples, feature flags
- Wire ABI contracts: fixed-width types, byte order (little-endian canonical on the wire? AuraOS: define wire ABI as little-endian, packed structs forbidden in favor of explicit serialization? For "no overengineering", define packed little-endian structs with static_asserts on sizes).

Wait — but components in a lean OS shouldn't have runtime threat-model code. The way to make this a *component* rather than just a doc: it's the normative contract that all other components `#include` and are validated against. It provides:

1. Compile-time contracts (`static_assert` gates) that fail the build if an arch/toolchain violates normative assumptions.
2. A machine-readable support matrix consumed by CI to select targets and tier-gate features.
3. A threat register with stable IDs referenced by other components' design docs and test evidence tags.
4. Wire ABI primitives: `aura_u16_le`, load/store helpers, packed record macros with size asserts.

That's small, direct, and genuinely useful — not overengineered.

Constraints from persona: "Clean. Simple. Small. Fast. Direct. No overengineering."

Let me define normative decisions:

**Languages & toolchain:**
- Freestanding C (C11) for kernel & core; hosted C11 for userland bootstrap; no C++, no Rust in core components.
- Single toolchain family per arch: GCC ≥ 13 or Clang ≥ 17 (one required, both allowed? For simplicity: "GCC ≥ 13 is normative; Clang ≥ 17 conforming alternative").
- `-ffreestanding -nostdlib -fno-builtin -fno-stack-protector?` Actually stack protector can be used... For freestanding, no. Keep flags list short and normative.

**Four arches:**
- Tier 1: x86-64 (AMD64 System V kernel ABI, LP64), AArch64 (AAPCS64, LP64)
- Tier 2: RISC-V 64 (RV64GC? For simplicity: rv64imac? Keep rv64gc with Zicsr/Zifencei implied by G... Actually RV64GC = IMAFD + Zicsr/Zifencei historically. Use rv64gc), i686 (IA-32 System V, ILP32)

Page sizes: x86-64: 4 KiB (normative; 2 MiB large pages optional), AArch64: 16 KiB? Or 4 KiB/64 KiB configurable. Normative: AArch64 base 4 KiB? Many modern kernels choose 16 KiB (Apple) or 64 KiB (ARM recommendation). For a portable kernel, choose 4 KiB as cross-arch normative base with per-arch "native" page sizes exposed. RISC-V: 4 KiB. i686: 4 KiB. AArch64: 64 KiB recommended... but let's define: normative base page per arch: x86_64=4K, aarch64=16K? Hmm. Simpler: define `AURA_PAGE_LOG2` per arch, with 4 KiB on x86-64/riscv64/i686 and 16 KiB on aarch64? Or 4 KiB everywhere for uniformity? AArch64 with 4 KiB has 48-bit VA in 2 levels (0,1,2) — fine. Simplicity argues for 4 KiB everywhere; note 64 KiB allowed as build-time variant on aarch64 but non-normative. Actually "no overengineering": normative base page = 4 KiB on all four arches; aarch64 may use 16/64 KiB only if a future component PRD amends this. Good — that's a clean, small decision.

Endianness: all four are little-endian in supported configurations (riscv64 normative little-endian). So kernel-internal and wire ABI both little-endian; big-endian RISC-V out of scope.

Canonical types: fixed-width from `<stdint.h>` unavailable in freestanding? Freestanding environments still provide `<stdint.h>` per C standard (freestanding includes stdint.h among the required headers: float.h, iso646.h, limits.h, stdalign.h, stdarg.h, stdatomic.h, stdbool.h, stddef.h, stdint.h, stdnoreturn.h). Yes — C11 freestanding requires those. So use stdint.h. Good.

**Kernel ABI:** syscall convention per arch: x86-64: syscall instruction, rdi/rsi/rdx/r10/r8/r9, nr in rax, return rax; aarch64: svc #0, x0–x5 args, x8 nr; riscv64: ecall, a0–a5, a7 nr; i686: int 0x80 or sysenter — pick `int $0x80`? Modern: sysenter is messy for return; use int 0x80 for simplicity. Fine.

**Threat model:** Define attacker classes: A1 local unprivileged process, A2 remote network peer, A3 malicious unsigned user-supplied payload/data (untrusted input parsers), A4 physical attacker with console/reset (limited), A5 malicious device/DMA (out of scope for now? or in-scope-with-IOMMU-later), A6 supply-chain/toolchain attacker (partially in scope: reproducible-ish builds, pinned toolchains, hashes).

Assets: kernel memory integrity, process isolation, syscall interface correctness, boot chain integrity, IPC confidentiality/integrity, persistent storage integrity.

Trust boundaries: TB0 firmware→bootloader handoff, TB1 bootloader→kernel (normative boot protocol per arch: stivalle2? For simplicity define AuraBoot protocol—hmm that may be a separate component; here we normatively *reference* the handoff contract IDs). Keep boundaries enumerated with IDs TB0–TB5.

Non-goals of the threat model: no side-channel resistance guarantees (explicit non-goal until component on that), no Meltdown/Spectre class mitigations promised at component #01 (defer), no secure boot/Microsoft-signing, no multi-tenant cloud hardening.

**Support matrix:** machine-checkable via `aura/support.h` macros + a `support_matrix.csv`-like table? Keep in C header + Markdown table both; CI reads header. Tiers:
- Tier 1: build + full test suite + release blocking (x86-64, AArch64)
- Tier 2: build + core test suite, non-blocking (RISC-V 64, i686)
- Tier 3: community/experimental — out of scope.

QEMU platforms normative for CI: q35 (x86-64), virt (aarch64), virt (riscv64), pc-i440fx? For i686 use q35 too or i440fx — pick `q35` with `-m` sizes.

Toolchain triples:
- x86_64-elf (kernel) / x86_64-aura (userland)? Simplify: kernel freestanding triplets: x86_64-elf, aarch64-elf, riscv64-elf, i686-elf. Userland: same arch with Aura target later. Keep kernel-only for Component #01 scope; userland ABI referenced.

**ABI versioning:** `AURA_ABI_VERSION` single integer; wire structures carry `abi_ver` field? For internal IPC records: 16-byte fixed header with magic `AUR1`, version, length, CRC? CRC adds complexity; maybe simple 16-bit ones-complement? Keep: magic + version + length + reserved; integrity is transport's job. "No overengineering" — yes, drop CRC, note anti-goal.

Wire ABI rules:
- All multi-byte wire fields little-endian.
- No implicit padding: every structure packed explicitly via `aura_wire` macro + `_Static_assert(sizeof == N)`.
- No pointers, no size_t, no long, no enum on the wire; only fixed-width ints and fixed arrays.
- Strings: u16 length + UTF-8 bytes, no NUL reliance... or u8 length + bytes. u16 allows longer names. Keep u16.
- Alignment 1 on wire; native structs must be decoded/encoded via explicit load/store helpers (endianness-safe).

C code blocks needed in Section 3:

1. `aura/abi.h` — ABI version, wire types, load/store helpers, packed macros, header layout.
2. `aura/arch.h` — per-arch contract macros: page size, pointer width, word size, stack alignment, syscall numbers layout, boot handoff register contract maybe just IDs.
3. `aura/threat.h` — threat register enum + boundary enum + a static registry struct with a single instance + compile-time uniqueness check? Keep: enums + `#define AURA_THREAT_REGISTERED ...` count assert.
4. `aura/support.h` — tier macros, `#error` if arch unsupported, feature gates `AURA_FEATURE_*` per tier.
5. Maybe a self-test: `arc_selftest.c` with `_Static_assert`s and a runtime check `arc_selftest()` returning 0. Small.

Also acceptance/test gates: CI matrix (4 targets × 2 toolchains? Keep GCC normative, Clang optional non-blocking), `abi-check` script that dumps struct sizes, doc lint for threat IDs referenced,