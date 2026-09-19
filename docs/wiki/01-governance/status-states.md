---
layout: default
title: Component Status Legend
---

# 🚦 Component Status States

**Rule (Rafx, 2026-09-19): every PRD, plan, and module carries exactly one of these five states. The state lives in the `Status` column of the PRD map (`00-index.md`) and in each doc's frontmatter `status:` field.**

| State | Meaning | Entry criteria | Evidence required |
|---|---|---|---|
| **`planned`** | We have a plan for this — it is scheduled work, nothing designed yet | PRD exists, linked from the map | PRD number in the map |
| **`outlined`** | Advisors have mapped out how it will be made — complete design exists | Design/consultation doc complete (Astra or advisor verdict recorded), interfaces decided | Design doc + advisor verdict in `docs/` or `05-plans/` |
| **`coded`** | It is being coded as of now — active implementation, not complete | First real code merged for the component | Commits touching the module dir |
| **`tested`** | It has been through the testing phase — passes its gates, not yet shipped | Stage checklist + test evidence recorded (`06-validation/`) | VAL entry / test log, gate run output |
| **`completed`** | It has been shipped — in a tagged release, works end-to-end | Release tag contains it (`vX.Y.Z-<stage>`) | Tag + CHANGELOG entry |

## Rules

1. States only move **forward**. `coded → outlined` means the design was wrong; that is a revert, recorded as such.
2. `tested` requires evidence — "looks done" is `coded`.
3. `completed` requires a **tag**. Untagged = never shipped.
4. When a component's state changes, update: `00-index.md` PRD map **and** the doc's `status:` frontmatter **in the same commit** (which must also bump that module's `MODULE_VERSION` if code changed).
5. Current state of the whole project = the highest state any module has reached, shown on the main page status chips.

## Current map (2026-09-19)

- UART/serial bring-up: **completed** (v0.1.0-uart)
- Boot infrastructure (Multiboot2, trampoline): **tested** — QEMU boot verified, v0.2.0-boot release pending
- GPU stack (DRV-001/002/003, MESA-001, DRV-MASTER): **outlined** — complete advisor design, no code yet
- All 17 PRDs: **outlined** (specified + advisor-audited)
