# AuraOS Engineering Wiki — Master Index

**Status:** NORMATIVE · **Owner:** miradev · **Revision:** 1

This is the single source of truth for AuraOS engineering. Both human developers and
subagents **must** read this index plus the relevant contracts before writing or
modifying anything.

---

## Reading Order (mandatory for all contributors)

1. [`01-governance/philosophy.md`](01-governance/philosophy.md) — the five laws, applies to every line.
2. [`../RULEBOOK.md`](../RULEBOOK.md) — mandatory documentation, cataloging, and tagging rulebook (`DOC-RULES-001`).
3. [`../CODE_TAGS.md`](../CODE_TAGS.md) — cross-reference index and grep tracing cheat sheet (`CODE-TAGS-001`).
4. [`02-architecture/ADR-001-normative-architecture.md`](02-architecture/ADR-001-normative-architecture.md) — what AuraOS **is**.
5. [`03-contracts/`](03-contracts/) — the frozen interfaces (ABI, telemetry, audio, window).
6. [`04-prds/`](04-prds/) — one PRD per component.
7. [`05-plans/`](05-plans/) — implementation plans per PRD.
8. [`06-validation/`](06-validation/) — test gates and evidence ledger.

---

## Directory Conventions

| Directory | Purpose |
|---|---|
| `01-governance/` | Philosophy, decision process, contribution rules |
| `02-architecture/` | ADRs (Architecture Decision Records), system diagrams |
| `03-contracts/` | Frozen wire/kernel ABI specifications |
| `04-prds/` | Product Requirements per component (numbered `PRD-NN-*`) |
| `05-plans/` | Detailed implementation plans per PRD |
| `06-validation/` | Test matrices, fault-injection checklists, evidence |
| `07-platforms/` | Per-arch enablement (i686, x86_64, aarch64, armv7) |
| `08-measurements/` | Performance measurement contracts and results |
| `99-archive/` | Superseded documents (never referenced as authority) |

**Precedence:** ADRs govern contracts → contracts govern PRDs → PRDs govern plans.
Conflicts block affected work until resolved by an approved ADR.

---

## Support Tiers (from Astra audit — normative)

* **Tier 1 (guaranteed):** kernel, native ELF processes, `libaura-ui` desktop, TCC, MicroPython, terminal, sysmon.
* **Tier 2 (experimental, per-app gated):** Capsule runners (Linux/Windows/Android ABIs), NEON/SIMD blur paths.
* **Not supported:** "runs anything" promises; unsupported hardware claims without evidence in `08-measurements/`.

---

## Rules for Subagents

1. Read this index + the architecture ADR + every contract your component touches **before** editing code.
2. Report contradictions; propose contract changes via a new ADR. Never invent semantics silently.
3. Submit implementation + tests + docs together. Never self-approve; validation evidence goes in `06-validation/`.
4. Distinguish **specified** vs **implemented** vs **tested**. Attach commit IDs and commands to every claim.
5. One source of truth per contract; link, never copy.

---

## Component PRD Map (order = build order)

| # | PRD | Component | Status |
|---|---|---|---|
| 01 | [PRD-01-governance](04-prds/PRD-01-governance-support-matrix.md) | Governance, threat model, support matrix | outlined |
| 02 | [PRD-02-capabilities](04-prds/PRD-02-capabilities-protection.md) | Capability objects, protection, device authority | outlined |
| 03 | [PRD-03-process](04-prds/PRD-03-process-lifecycle.md) | Process lifecycle, scheduler, ELF ABI | outlined |
| 04 | [PRD-04-concurrency](04-prds/PRD-04-concurrency-ipc.md) | Concurrency, IPC, object lifetimes | outlined |
| 05 | [PRD-05-resources](04-prds/PRD-05-resources-telemetry.md) | Resource quotas, OOM, PMM & telemetry | tested |
| 06 | [PRD-06-verification](04-prds/PRD-06-verification.md) | Test harness, fuzzing, fault injection | coded |
| 07 | [PRD-07-boot](04-prds/PRD-07-boot-platform.md) | Boot, serial console, clocks, VGA console | tested |
| 08 | [PRD-08-ramfs](04-prds/PRD-08-filesystem-shell.md) | initramfs, RAM filesystem, shell | planned |
| 09 | [PRD-09-supervision](04-prds/PRD-09-supervision.md) | Service supervision & recovery | planned |
| 10 | [PRD-10-storage](04-prds/PRD-10-storage.md) | Block devices, persistent storage | planned |
| 11 | [PRD-11-desktop](04-prds/PRD-11-desktop-compositor.md) | Display, compositor, GUI toolkit | outlined |
| 12 | [PRD-12-audio](04-prds/PRD-12-audio.md) | Audio stack, effects, DAW support | planned |
| 13 | [PRD-13-terminal](04-prds/PRD-13-terminal.md) | Terminal streams & parser | planned |
| 14 | [PRD-14-parsers](04-prds/PRD-14-hostile-content.md) | Hostile-content parsing & limits | planned |
| 15 | [PRD-15-toolchain](04-prds/PRD-15-toolchain.md) | TCC/MicroPython dev workflows | planned |
| 16 | [PRD-16-network](04-prds/PRD-16-network.md) | Networking, TLS, AI client | planned |
| 17 | [PRD-17-arches](04-prds/PRD-17-arch-enablement.md) | i686/aarch64/armv7 enablement | planned |

**Status states:** `planned` → `outlined` → `coded` → `tested` → `completed` — definitions and evidence rules in [Status States](01-governance/status-states.md).
