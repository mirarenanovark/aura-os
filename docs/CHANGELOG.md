# AuraOS Changelog

All releases, newest first. Module scopes: `boot` `kernel` `drivers` `gui` `libs` `userland` `tools` `tests`. See the [Versioning Protocol](wiki/01-governance/versioning.html) for the rules.

## v0.1.0-uart (2026-09-19) — stage: UART bring-up
First versioned stage. Everything before this point = architecture & docs groundwork.
### kernel
- freestanding 16550 UART driver + serial_printf, clean x86_64/i686 dual compile (bcb376f)
### drivers
- DRV-001/002/003 GPU driver architecture specs, hardware support matrix, master graphics stack spec w/ 4-tier never-black-screen fallback
### docs
- MESA-001 userspace 3D strategy, roadmap milestone cards, docs site (glass navbar, dark frosted), PHILOSOPHY.md, Astra design audits
