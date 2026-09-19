The user is the lead OS architect for AuraOS, a freestanding C operating system supporting 4 architectures (likely x86_64, i686, aarch64, riscv64). They want a rigorous PRD for Component #14: "Hostile-Content Parsing, Document Bounds & Asset Limits".

The structure is strictly mandated:
1. Executive Summary & Scope Boundary
2. Core Requirements (Functional & Non-Functional)
3. Data Structures, C Interfaces & Wire ABI Contracts (concrete C code blocks)
4. Failure Modes, Bounds & Resource Limits
5. Acceptance Criteria & Test Gates (Evidence required to mark done)
6. Anti-Goals

Philosophy: Clean. Simple. Small. Fast. Direct. No overengineering. So the PRD itself should reflect that — no absurd enterprise bloat, no dependency injection nonsense, no plugin registries with 40 callbacks. Keep it tight, C-centric, freestanding (no libc, no malloc by default — or a fixed arena), deterministic bounds, constant-time-ish checks where relevant, integer overflow checks on all bounds math, no recursion for untrusted input (or bounded recursion depth), etc.

The component is about:
- Hostile-content parsing: parsing untrusted documents (images, fonts, archives, text, configs) that come from untrusted sources, with the assumption the content is adversarial.
- Document bounds: maximum document sizes, nesting depths, entity counts, string lengths.
- Asset limits: per-asset caps (textures, fonts, sounds), total budget accounting, allocation from a fixed arena/pool.

So this component is essentially a hardened parsing library + resource governor for AuraOS. It should provide:
- A bounded reader (`aura_reader`) API — cursor over a byte span with checked reads, little-endian decode, no pointer arithmetic overflow.
- A token/event or structure-size budget enforcement API — `aura_budget`.
- Limits constants: AURA_MAX_DOC_BYTES, AURA_MAX_DEPTH, AURA_MAX_ENTITIES, AURA_MAX_STR_BYTES, AURA_MAX_ASSET_BYTES, per-type.
- Error codes as an enum with stable ABI values.
- A "parse verdict" output: accept / reject with reason code and byte offset.
- Deterministic, allocation-free (arena-based) operation; no recursion; O(n) time; fixed stack usage.
- Wire ABI contract: struct layouts with explicit padding, fixed-width types, versioned header, endianness declaration (little-endian on wire), alignment, checksum optional.

Four architectures: x86_64, i686 (or x86), aarch64, riscv64. Should specify: same struct layout across arches due to fixed-width types and explicit padding; no pointer fields in wire ABI (offsets only); unaligned access forbidden — readers use memcpy-style loads.

Format considerations: The user wants "production-grade markdown only", "complete, rigorous PRD", with "concrete C code blocks". This should be a long, detailed document — the user explicitly asks for rigor and completeness, with six sections. Headers, tables where appropriate (bounds tables, error codes), C code blocks with real interfaces. It's a PRD, so it should include requirement IDs (FR-1, NFR-1), acceptance criteria mapping, test gates. Evidence requirements: e.g., fuzzing corpus run, measured stack depth, sanitizer runs, property tests.

But keep grounded: no overengineering. So no "AI-assisted heuristic sandbox", no "regex JIT", no "fully general markup parser" — the component provides primitives and limits; format-specific parsers are separate components. Anti-goals section handles that: it does NOT implement format codecs themselves, does not do content sniffing heuristics, does not do virus scanning, does not do networking, does not do compression bomb expansion beyond budget accounting, etc.

Let me define the concrete design.

**Kernel vs userspace**: In a small OS, this could be a kernel-adjacent library used by the asset loader / initrd parser. AuraOS freestanding C. I'll say: component lives as a static library `libaurabound` linked into both kernel and the asset service; no syscall surface required — it's a library, but exposes a versioned ABI struct for hand-off across module boundary. Actually the PRD asks for "Wire ABI Contracts" — so there should be a serialized descriptor (e.g., asset manifest entry) that crosses module/process boundaries. Keep that minimal: a fixed 32-byte asset descriptor with magic, version, kind, u32 offsets/sizes, u16 limits, checksum16? Keep it small.

Let me draft the pieces:

Constants (all in `aura/bounds.h`):
```c
#define AURA_ABI_VERSION 1u

/* Absolute document caps */
#define AURA_MAX_DOC_BYTES       (64u * 1024u * 1024u)   /* 64 MiB hard ceiling */
#define AURA_MAX_STR_BYTES       (1u * 1024 * 1024)       /* 1 MiB per string/entity */
#define AURA_MAX_DEPTH           64u
#define AURA_MAX_ELEMENTS        (1u << 20)               /* 1,048,576 */
#define AURA_MAX_ENTITIES        ...
```

Hmm — but "Clean. Simple. Small." Maybe a limits struct that callers can tighten but not loosen beyond compile-time ceilings.

Reader:
```c
typedef struct aura_reader {
    const u8 *base;
    usize     len;
    usize     pos;
    u32       err;   /* 0 = ok, else AURA_E_* */
} aura_reader;
```
Methods: `aura_reader_init`, `aura_r8`, `aura_r16le`, `aura_r32le`, `aura_r64le`, `aura_rbytes`, `aura_skip`, `aura_remaining`, `aura_err_at`. Each returns bool success, sets err on failure with offset. All integer math checked: `pos + n` overflow guard via `n > len - pos` pattern (since pos <= len invariant maintained).

Depth guard:
```c
typedef struct aura_depth { u32 cur, max; } ...
```
Actually simpler: `aura_enter(aura_depth*)` / `aura_leave`. Or just track locally in parsers; but a shared guard prevents the classic mistake. Provide `aura_depth_enter(aura_depth *d)` returning bool.

Budget:
```c
typedef struct aura_budget { usize cap, used; } aura_budget;
bool aura_budget_charge(aura_budget*, usize n);
```

Arena for parse scratch (allocation-free guarantee): caller provides scratch buffer; component never allocates. `aura_arena` with bump allocation, or the parser component provides `aura_scratch`. Keep: parsers must be given all memory up front; component itself performs zero dynamic allocation.

Verdict struct:
```c
typedef struct aura_verdict {
    u32 code;      /* AURA_OK or AURA_E_* */
    u32 offset;    /* byte offset where the failure was detected */
} aura_verdict;
```

Error enum — stable values:
```c
enum {
    AURA_OK            = 0,
    AURA_E_EMPTY       = 1,
    AURA_E_TRUNCATED   = 2,
    AURA_E_TOO_BIG     = 3,
    AURA_E_DEPTH       = 4,
    AURA_E_COUNT       = 5,   /* element/entity count exceeded */
    AURA_E_MALFORMED   = 6,
    AURA_E_MAGIC       = 7,
    AURA_E_VERSION     = 8,
    AURA_E_OVERFLOW    = 9,   /* integer overflow in size arithmetic */
    AURA_E_BUDGET      = 10,
    AURA_E_ALIGNMENT   = 11,
    AURA_E_CHECKSUM    = 12,
    AURA_E_IO          = 13,  /* source couldn't supply bytes */
    AURA_E_INTERNAL    = 14   /* parser bug; must never happen; assert in debug */
};
```

Asset limits & descriptor wire ABI (32 bytes, little-endian):
```c
typedef struct __attribute__((packed)) aura_asset_desc { ... }
```
Fields: magic u32 = 'AUBD' (0x44554241?), version u16, kind u16 (enum: text, bitmap, font, audio, archive, blob), payload_off u32, payload_len u32, crc32 u32, declared_len u32? Hmm. Also decompressed size cap for compressed assets. Let's design 32 bytes:

```
offset size field
0      4    magic        0x42445541 "AUBD" LE ('A'=0x41... magic bytes "AUB1"?)
4      2    abi_version
6      2    kind
8      4    payload_off
12     4    payload_len
16     4    unpacked_len (0 if not compressed)
20     4    crc32_payload
24     2    flags
26     2    reserved0 (must be 0)
28     4    crc32_header (over bytes 0..27)
```
Total 32. Rule: payload_off + payload_len must not overflow and must be within the enclosing blob. CRC checked in constant-ish time? CRC32 is fine; no timing secrecy needed for integrity (not auth). State: CRC is integrity against corruption, NOT authentication — hostile content can forge it; that's fine because parser still treats content as hostile. Important note for rigor: all downstream parsing assumes hostility regardless of checksum pass.

Asset kind table with per-kind caps:
- AURA_ASSET_TEXT: 8 MiB, 4096 lines? Hmm lines can be arbitrary. Keep caps: 8 MiB.
- AURA_ASSET_BITMAP: 64 MiB raw; dimensions capped: max 16384x16384; width*height*bpp overflow-checked; bpp allowed set {1,2,4,8,16,24,32}.
- AURA_ASSET_FONT: glyph count ≤ 65536; points/curve budget.
- AURA_ASSET_AUDIO: PCM cap 64 MiB; sample rate ≤ 192 kHz; channels ≤ 8.
- AURA_ASSET_ARCHIVE: entries ≤ 65536; path length ≤ 255; nesting ≤ 8? Actually archives inside archives: depth cap 4.
- AURA_ASSET_BLOB: generic, 64 MiB.

Global asset budget: e.g., 256 MiB total live unpacked asset bytes per subsystem; enforced by aura_budget.

Non-functional:
- No dynamic allocation, ever (freestanding).
- Bounded stack: parser frames must not exceed 512 B per level; recursion prohibited (or depth-bounded to 64 with static assert); stack usage total ≤ 8 KiB in kernel context.
- Deterministic: same input → same verdict/offset; no time/address-dependent behavior.
- Time bounds: linear in input size; reject within O(1) on oversize header declarations; worst-case parsing ≤ k×n with k small; mandatory progress: every loop iteration consumes ≥1 byte or bumps a monotonic counter.
- No writes outside caller-provided buffers; outputs fully defined on failure.
- Reentrancy: pure functions of inputs; no globals, no errno, no hidden state. Thread-safety: all state in caller-owned structs.
- Compile-time: -Wall -Wextra -Werror, no UB, compile on all 4 arches; static asserts for struct sizes.
- Unaligned-safe: use memcpy-style load; on strict-alignment arches (riscv64 without Zicclsm? Actually riscv64 allows misaligned in many impls but not guaranteed; aarch64 allows unaligned for normal loads; i686 allows; but keep portable) — require alignment-safe reads everywhere.
- Endianness: wire = little-endian; host may be LE on all 