# AuraOS Test Gates & Evidence Ledger (`VAL-001`)

**Document ID:** `VAL-001` · **Status:** ACTIVE · **Owner:** QA Subagent

---

## 1. Automated Verification Gates

Every pull request or milestone commit must pass all gates before merge.

| Gate | Category | Test Command | Success Condition |
|---|---|---|---|
| **GATE-01** | ABI Static Assertions | `make check-abi` | All `_Static_assert` pass across i686/x86_64/aarch64/armv7 |
| **GATE-02** | Host Unit Tests | `make test-host` | PMM bitmap, Ring buffers, and VFS run in host Linux with ASan |
| **GATE-03** | Headless QEMU Boot | `tools/test_qemu_boot.sh` | QEMU boots to serial string `[AURA_BOOT_SUCCESS]` in <2.0s |
| **GATE-04** | User Fault Isolation | `tools/test_fault_inject.sh` | User process `#PF` and divide-by-zero terminate cleanly; kernel lives |
| **GATE-05** | Memory Leak Check | `tools/test_leak_stress.sh` | Spawn/kill 1000 processes; zero kernel heap growth |
| **GATE-06** | Audio Underrun Test | `tools/test_audio_stress.sh` | 10-minute continuous 48kHz audio run with 0 dropped frames |

---

## 2. Evidence Ledger

*(Entries are recorded upon successful validation runs with commit hash and logs)*

| Date | Commit Hash | Target Arch | Verified Deliverable | Status | Tester |
|---|---|---|---|---|---|
| 2026-09-18 | `d8861bc` | Spec/Docs | Complete PRD & Wiki architecture | PASS | Astra + GLM Pro |
