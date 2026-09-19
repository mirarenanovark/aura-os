# AuraOS Development Plan — Sprint Batch 1 (v0.3.x Series)

**Owner:** Mira (orchestrator) · **Coder:** Torve (`trove/mimo-v2.5`) subagents
**Date created:** 2026-09-19 · **Status:** ACTIVE
**Rule:** One milestone per task, docs updated in the same change. Every task ends with:
`make clean && make all && make test` green + headless QEMU boot check + `python3 tools/version.py check`.

## Goal

Advance AuraOS through 5 milestones in parallel lanes (disjoint file sets — no two agents touch the
same file), then version-gate, document, build ISO, publish to the portal, and tag.

## Milestones

| Milestone | Task | Files (exclusive) | Status |
|:---|:---|:---|:---|
| M1 | `TASK-MEM-02` Dynamic bitmap placement | `kernel/core/pmm.c`, `kernel/include/aura/pmm.h`, `kernel/main.c`, `docs/wiki/04-internals/PMM.md` | QUEUED |
| M2 | `TASK-MEM-05` Slab allocator (`zalloc`) | `kernel/core/slab.c`, `kernel/include/aura/slab.h`, `tests/test_slab.c`, `Makefile` (test block only), `docs/wiki/04-internals/SLAB.md` | QUEUED |
| M3 | `TASK-DRV-02` Serial RX polling | `kernel/arch/x86_shared/serial.c`, `kernel/include/aura/serial.h`, `kernel/core/menu.c` (input only), `docs/wiki/04-internals/SERIAL-CONSOLE.md` | QUEUED |
| M4 | `TASK-SCHED-01` Context switch | `kernel/core/sched.c`, `kernel/include/aura/sched.h`, `kernel/arch/x86_shared/switch.S`, `kernel/main.c` (init call only), `docs/wiki/04-internals/SCHEDULER.md`, `docs/TASK_QUEUE.md` | QUEUED |
| M5 | Integration & release | all of the above, `VERSION`, `kernel/MODULE_VERSION`, `drivers/MODULE_VERSION`, `docs/CHANGELOG.md`, portal | ORCHESTRATOR |

## Dependency rules

- M4 (sched) must not call slab/pmm internals beyond the public headers.
- M1 lands bitmap relocation first; M2 may depend on the relocated bitmap only via `pmm_init()` signature — coordinate through the orchestrator if the signature changes.
- No subagent commits. Orchestrator commits per milestone after gates pass.

## Verification gate (every milestone)

1. `make clean && make all` — zero warnings
2. `make test` — all unit tests pass
3. `timeout 10 qemu-system-x86_64 -cdrom build/auraos.iso -serial stdio -display none -no-reboot` — boots clean, no exceptions
4. Docs page exists for the new module (simplest-form style per `PANIC-HANDLER.md`)
5. `docs/CHANGELOG.md` entry + `docs/TASK_QUEUE.md` status flip
