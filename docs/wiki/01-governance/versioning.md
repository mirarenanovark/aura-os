---
layout: default
title: Versioning Protocol
---

# 📏 Versioning Protocol

**Rule (Rafx, 2026-09-19): every stage completion = a version bump + a commit + a tag. Modules carry their own versions. No stage closes without all three.**

Normative spec: [`docs/VERSIONING.md`](https://github.com/mirarenanovark/aura-os/blob/main/docs/VERSIONING.md) in the repo.

## The three layers

| Layer | File | Scheme | When it bumps |
|---|---|---|---|
| Repo release | `VERSION` | `MAJOR.MINOR.PATCH-<stage>` (e.g. `0.1.0-uart`) | Stage gate passed → MINOR. In-stage fix → PATCH. First bootable ISO → 1.0.0 |
| Module | `<module>/MODULE_VERSION` | semver (`0.1.0`) | Same commit that changes that module |
| Release record | `docs/CHANGELOG.md` | newest-first, grouped `### module` | Every release commit |

Modules: `boot`, `kernel`, `drivers`, `gui`, `libs`, `userland`, `tools`, `tests`.

## Stage-close sequence

1. Feature commits land first — conventional commits, per-module scope (`feat(kernel): …`), each touching exactly one module + its `MODULE_VERSION` bump.
2. One release commit `chore(release): vX.Y.Z-<stage>` — touches **only** `VERSION`, `docs/CHANGELOG.md`, and `MODULE_VERSION` files.
3. Annotated tag `vX.Y.Z-<stage>` on the release commit.
4. `git push origin main --tags`.
5. Gate must pass before push: `python3 tools/version.py check` → `VERSION GATE OK`.

## The gate

`tools/version.py check` verifies: semver shapes, tag exists for `VERSION`, changelog entry exists for `VERSION`, every module has a valid `MODULE_VERSION`, and any feature commit touching a module bumped its version. Pure bookkeeping/release commits are exempt from the per-module check.

## Current release

**v0.1.0-uart** (2026-09-19) — UART bring-up: freestanding 16550 driver + `serial_printf`, clean x86_64/i686 dual compile. Prior work (17 PRDs, GPU stack specs, docs site) is changelogged as groundwork under the same release.

Next stage: **v0.2.0-boot** — Multiboot2/long-mode boot infrastructure (in progress on `main`).
