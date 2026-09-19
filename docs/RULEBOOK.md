# AuraOS Documentation, Cataloging & Code Tagging Rulebook (`DOC-RULES-001`)

**Document ID:** `DOC-RULES-001`  
**Owner:** Core Engineering & Architecture Team  
**Status:** NORMATIVE · MANDATORY FOR ALL CODE AND DOC UPDATES  
**Applies to:** Humans, AI Agents, and Subagents contributing to AuraOS  

---

## 1. Core Principles: The Five Documentation Laws

All documentation and inline code annotations in AuraOS must obey the system philosophy:
> **Clean. Simple. Small. Fast. Direct. No Bloat.**

1. **Single Source of Truth:** Never duplicate specifications or API signatures across multiple docs. Hyperlink directly using canonical relative or GitHub Markdown links (`[Label](path/to/file.md)`).
2. **Every File Must Declare Its Connections:** No orphaned files. Every source file must state its **Layer**, **Component**, **Contract/PRD**, **Callers/Callees**, and **Execution Flow**.
3. **Synchronized Atomic Commits:** Code changes, inline tag updates, and GitHub Pages doc updates must be committed together. Never modify code without updating affected documentation.
4. **State Precision:** Always distinguish between `planned`, `outlined`, `coded`, `tested`, and `completed`. Never claim a feature is implemented without specifying the commit or automated test gate verifying it.
5. **Human and Agent Readability:** Inline tags must use standardized bracketed tokens (`[AURA_LAYER]`, `[AURA_CONNECTS]`, etc.) that are easily grep-searchable by humans and autonomous agents alike.

---

## 2. Code Tagging Standard (`[AURA_*]`)

Every source code file (`.c`, `.h`, `.S`, `.ld`) must contain a standard header block and granular flow tags within critical functions.

### 2.1 Standard File Header Block

Place this comment at the top of every source file:

```c
/**
 * @file        <relative_path_to_file>
 * @layer       STAGE_<1..5> (<Layer Name>)
 * @component   <AURA_COMPONENT_NAME>
 * @contract    <PRD-NN-* or ADR-NNN or SPEC reference>
 * @description <Concise, direct 1-3 sentence summary of purpose>
 *
 * @connects    <Upstream callers / Downstream dependencies / Hardware interfaces>
 *              - Upstream:   <who invokes or includes this>
 *              - Downstream: <what this invokes or allocates from>
 *              - Hardware:   <ports, MMIO, CPU registers, or bus interfaces touched>
 *
 * @flow        <Execution / Data flow sequence>
 *              1. Step 1 ...
 *              2. Step 2 ...
 */
```

### 2.2 Standard Tag Token Reference

Use these exact search tokens throughout code and documentation:

| Tag Token | Purpose | Example |
| :--- | :--- | :--- |
| `[AURA_LAYER: STAGE_N]` | Identifies which of the 5 Grand System Stack stages owns this file/routine. | `// [AURA_LAYER: STAGE_1_HARDWARE_BOOT]` |
| `[AURA_COMPONENT: NAME]` | Unique subsystem identifier. | `// [AURA_COMPONENT: PMM_BITMAP_ALLOCATOR]` |
| `[AURA_CONTRACT: ID]` | Formal contract or PRD governing the function/structure. | `// [AURA_CONTRACT: PRD-07-boot-platform]` |
| `[AURA_CONNECTS: TARGET]` | Explicit connection to another module or hardware port. | `// [AURA_CONNECTS: PIT_CHANNEL0 -> SYSMON_TICK]` |
| `[AURA_FLOW: NAME]` | Marks a discrete step in a system-wide execution flow. | `// [AURA_FLOW: BOOT_HANDOFF] Step 3: Enable Paging` |
| `[AURA_SAFETY: LEVEL]` | Concurrency, memory bounds, or reentrancy constraint. | `// [AURA_SAFETY: CLI_REQUIRED] Non-reentrant` |

### 2.3 Layer Taxonomy

AuraOS defines exactly 5 normative architectural stages:
* **`STAGE_1_HARDWARE_BOOT`**: Bootloader handoff, CPU identification, GDT, IDT/ISR, serial COM1, PIT timer, Multiboot2 parsing.
* **`STAGE_2_PHYSICAL_FAULT`**: Physical memory manager (PMM frame bitmap), CPU fault handling, panic dispatch.
* **`STAGE_3_VIRTUAL_SCHEDULER`**: Virtual memory manager (VMM 4-level paging / huge pages), kernel dynamic heap (`zalloc`/free-list), preemptive scheduler.
* **`STAGE_4_SERVICES_DRIVERS`**: Driver abstraction, VGA/TUI console, VirtIO-GPU, IPC rings, system monitor telemetry (`sysmon`).
* **`STAGE_5_USERLAND_APPS`**: Libaura-ui, window server, interactive terminal, applications (Sysmon, DAW, Coding Studio).

---

## 3. Documentation Directory Hierarchy & Cataloging

All documentation in AuraOS lives under `docs/` and is published directly via GitHub Pages (Jekyll + Kramdown):

```
docs/
├── _config.yml              # Jekyll configuration (GFM markdown, Rouge syntax)
├── _layouts/                # HTML layout templates
│   └── default.html         # Main glass navigation header & sidebar
├── assets/                  # CSS stylesheets, fonts, and diagrams
│   └── css/style.css        # Aero Glass UI stylesheet
├── index.md                 # Landing page & executive system dashboard
├── SOURCE_MAP.md            # Grand system architecture & call graph map
├── CODE_TAGS.md             # Code tag glossary & grep tracing cross-reference
├── RULEBOOK.md              # THIS DOCUMENT: Documentation & cataloging rules
├── build.md                 # Toolchain, build, test, and ISO generation guide
├── roadmap.md               # 6-phase engineering milestones & progress
├── CHANGELOG.md             # Version release notes & audit history
├── devdocs/                 # Deep architectural research & audit reports
│   ├── ios_memory_architecture_report.md
│   └── report/
│       ├── auraos_memory_architecture_blueprint.md
│       └── kernel_optimization_report.md
└── wiki/                    # Normative engineering wiki
    ├── 00-index.md          # Master wiki index & reading order
    ├── 01-governance/       # Philosophy, versioning, status states
    ├── 02-architecture/     # Architecture Decision Records (ADRs)
    ├── 03-contracts/        # Frozen ABI & wire contracts (Syscalls, Audio, etc.)
    ├── 04-prds/             # Product Requirements Documents (PRD-01 to PRD-17)
    ├── 05-plans/            # Execution & implementation plans (PLAN-01 to ...)
    └── 06-validation/       # Test gates (VAL-001) & evidence ledger
```

---

## 4. How to Update & Catalog Documents for Future Updates

Whenever any subsystem is added or modified, follow this 6-step checklist:

### Step 1: Check Precedence & Contracts
1. Determine if the change introduces a new interface or modifies an existing one.
2. If modifying a contract (`docs/wiki/03-contracts/`), an ADR in `docs/wiki/02-architecture/` must be proposed first.
3. Verify that the change aligns with [`docs/wiki/01-governance/philosophy.md`](file:///f:/devlounge/AuraOS/docs/wiki/01-governance/philosophy.md).

### Step 2: Implement Code with Inline Tags
1. Add/update the standard `@file` header block in all touched files.
2. Insert `// [AURA_FLOW: ...]` and `// [AURA_CONNECTS: ...]` tags at all connection points.
3. Ensure no compiler warnings or lint errors are introduced.

### Step 3: Update `docs/SOURCE_MAP.md` & `docs/CODE_TAGS.md`
1. Update the **Grand System Stack** diagram if a new component is introduced.
2. Update the **Component File Map** table with the file path, size/lines, purpose, and connecting tags.
3. Update `docs/CODE_TAGS.md` so that grepping for the component tag displays complete upstream and downstream linkage.

### Step 4: Update PRD & Plan Statuses
1. In `docs/wiki/00-index.md`, update the status of affected PRDs according to [`docs/wiki/01-governance/status-states.md`](file:///f:/devlounge/AuraOS/docs/wiki/01-governance/status-states.md) (`planned` → `outlined` → `coded` → `tested` → `completed`).
2. If a new milestone phase is reached, update `docs/index.md` status badges and the roadmap checklist in `docs/roadmap.md`.

### Step 5: Update Sidebar & Navigation
1. If a new major document or contract was created, add it to `docs/_layouts/default.html` under the appropriate sidebar section (`Getting Started`, `Frozen Contracts`, `Component PRDs`, `Plans & Validation`, or `Architecture Reports`).
2. Run Jekyll locally or verify relative URLs to ensure zero 404 dead links.

### Step 6: Log Validation Evidence
1. Run the relevant test gate (e.g. `make test-pmm`, `make test-heap`, QEMU boot check).
2. Append a new row to the **Evidence Ledger** in [`docs/wiki/06-validation/VAL-001-test-gates.md`](file:///f:/devlounge/AuraOS/docs/wiki/06-validation/VAL-001-test-gates.md) recording:
   - Date
   - Git Commit Hash
   - Target Architecture
   - Verified Deliverable
   - Result (`PASS` / `FAIL`)
   - Tester / Agent ID

---

## 5. Verification & Audit Enforcement

Any pull request or commit that fails to include:
* Standard header tags on new source files
* Updates to `docs/SOURCE_MAP.md`
* Verification evidence in `docs/wiki/06-validation/VAL-001-test-gates.md`

...will be rejected at **GATE-01 / GATE-02** code review.
