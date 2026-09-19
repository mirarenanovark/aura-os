# AuraOS Versioning & Release Protocol

**Rule (Rafx, 2026-09-19): every stage completion = a version bump + a commit. Modules carry their own versions. No stage closes without both.**

## 1. Global version (repo root)
- `VERSION` file at repo root, semver `MAJOR.MINOR.PATCH-<stage>` — e.g. `0.1.0-uart`, next `0.2.0-fb`.
- **Stage = MINOR bump** (stage gate passed: builds, boots, stage checklist ticked). PATCH = fixes within the current stage. MAJOR = first bootable ISO (1.0.0), GUI compositor live, etc.
- Every bump gets a git **tag** `v<version>` AND a `docs/CHANGELOG.md` entry: `## v0.2.0-fb (2026-09-20) — stage: framebuffer` + bullets of what landed.

## 2. Module versions
Each top-level module dir carries `MODULE_VERSION` (plain text, semver + short codename):
- `kernel/MODULE_VERSION`, `drivers/MODULE_VERSION`, `gui/MODULE_VERSION`, `libs/MODULE_VERSION`, `userland/MODULE_VERSION`, `boot/MODULE_VERSION`, `tools/MODULE_VERSION`, `tests/MODULE_VERSION`.
- Bump the module version **in the same commit** that changes that module. One commit = one module + its version bump + changelog line under a `### module` sub-heading.
- `tools/version.py` (see §4) refuses a stage release if any modified module's version wasn't bumped.

## 3. Commit discipline
- Conventional commits, per-module scope: `feat(kernel): ...`, `fix(gui): ...`, `docs(gpu): ...`.
- A **stage closes** only when: build clean on all flavors → stage test (e.g. UART outputs banner) → commit `chore(release): v0.X.0-<stage>` updating `VERSION` + all touched `MODULE_VERSION`s + `CHANGELOG.md` → tag `v0.X.0-<stage>` → push with tags (`git push origin main --tags`).
- Never mix a release commit with feature changes. Feature commits happen first, release commit last.

## 4. Enforcement tooling
- `tools/version.py check` — CI-style gate (run before any push): parses `VERSION`, validates semver, compares `git diff --name-only HEAD~1` against module version bumps, verifies tag exists for current VERSION, verifies CHANGELOG has an entry for it. Exit 1 with a human message on any miss.
- `tools/version.py bump <module|stage> <part>` — does the edit + prints the exact release-commit command line to paste.
- Mira-dev runs `tools/version.py check` as the last step of every stage before reporting "stage complete".

## 5. Current state (initialize from)
- `VERSION` = `0.1.0-uart` (UART bring-up committed bcb376f = this stage's content).
- Tag `v0.1.0-uart` on bcb376f, CHANGELOG seeded with all prior work (specs, docs site, UART).
