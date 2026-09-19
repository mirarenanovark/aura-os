The user wants a complete PRD for "AuraOS Component #02: Capability Objects, Address-Space Protection & Device Authority". This is a freestanding C OS project supporting 4 architectures (likely x86_64, aarch64, riscv64, and one more — maybe i686 or loongarch64). The philosophy is "Clean. Simple. Small. Fast. Direct."

I need to write production-grade markdown with:
1) Executive Summary & Scope
2) Core Requirements
3) C Interfaces & ABI Contracts (code blocks)
4) Failure Modes & Limits
5) Acceptance Criteria & Test Gates
6) Anti-Goals

Let me think about what this component entails:

**Capability Objects**: A capability system where kernel objects (memory regions, endpoints, IRQs, device access, address spaces) are referenced via unforgeable handles. Classic seL4/L4-style capability model. Capabilities have rights (Read/Write/Execute/Grant/Derive), reference counts, and can be minted/derived/restricted but never escalated.

**Address-Space Protection**: VSpace management — mapping frames into address spaces, ASIDs, TLB management, page table manipulation across 4 architectures.

**Device Authority**: Control over MMIO regions, DMA, IRQ lines — granting a task authority over a device via untyped memory retyping or device-capability grants. Device isolation: devices assigned to user-space drivers via device caps + IRQ caps + MMIO caps.

Since this is freestanding C (no libc), I should use fixed-width types (uint64_t, etc.), explicit calling conventions, no dependencies on runtime. The ABI contracts should be precise: struct layouts, alignment, error codes as enums, static asserts on sizes.

Design decisions to make it "Clean. Simple. Small. Fast. Direct":
- Single capability table per CSpace (radix tree or flat two-level array)
- 64-bit capability identifiers? Or maybe CNode addressing like seL4. Keep it simple: a CSpace is a two-level radix tree; a CPtr is a 64-bit index (guard + slot). Actually simpler: flat 32-bit slot index within a CNode, CSpaces can be chained.
- Capability types: Null, CNode, VSpace (address space), Frame (typed memory), Untyped, Endpoint (maybe out of scope for this component — IPC is component #03 presumably), IRQ, Device (MMIO region), TCB (maybe component #01?). Given this is Component #02, TCBs might exist (component #01 could be "Boot & Physical Memory" or "Tasks & Scheduling"). Let me define scope: this component covers caps, VSpaces, device authority. TCB/thread caps may be referenced but owned by component #01 (Task Objects) or #03. I'll note interfaces assume TCB handles from Component #01.

Four architectures: x86_64, aarch64, riscv64, loongarch64 — a sensible modern choice. Or x86_64, aarch64, riscv64, i686. I'll pick x86_64, aarch64, riscv64, loongarch64 and state it. Actually, to be safe, I could say "the four target architectures (x86-64, AArch64, RISC-V 64, LoongArch64)". That's a reasonable modern set. Alternatively i686 for 32-bit coverage. Hmm — capability bits and word size matter. If all four are 64-bit, ABI is uniform (64-bit caps, 64-bit addresses). I'll go with all 64-bit: x86-64, AArch64, RISC-V RV64, LoongArch64. That keeps it clean: single ABI word size.

Let me structure the PRD:

# PRD: AuraOS Component #02 — Capability Objects, Address-Space Protection & Device Authority

Front matter: doc ID, status, owners, version, dependencies.

## 1) Executive Summary & Scope
- Purpose: unforgeable authority, memory isolation, device delegation.
- In scope: capability kernel (CSpace, CNode, mint/derive/revoke), VSpace objects (root page tables, mapping frames, ASIDs, TLB shootdown), device authority (MMIO frames, IRQ caps, DMA domains via IOMMU where present), bootstrap of initial caps.
- Out of scope: IPC endpoints/message queues (Component #03), scheduling/TCB lifecycle (Component #01), drivers themselves, userspace runtime.
- Design pillars aligned with philosophy.

## 2) Core Requirements
Functional requirements numbered FR-1...:
- FR-1: Capability model — unforgeable 64-bit handles, per-task CSpace, rights masks.
- FR-2: Capability derivation — Mint (rights-restricting), Copy, Move, Revoke, Delete. No escalation; monotonic restriction lattice.
- FR-3: CSpace structure — two-level radix, slot states (Null/Free/Reserved), amortized O(1) lookup.
- FR-4: Address spaces — per-task VSpace roots, map/unmap frames at page granularity with arch-specific large-page hints, ASID allocation, TLB invalidation/shootdown protocol.
- FR-5: Untyped memory retyping — boot untypeds, retype to frames/cnodes, revoke cascades.
- FR-6: Device authority — Device caps for MMIO regions (must be uncached/device-nx mappings), IRQ caps with ack semantics, PCI BDF enumeration caps maybe minimal, IOMMU (SMMU/VT-d/IOMMU) map/unmap for DMA.
- FR-7: Revocation & recycling — revoke descends derivation tree, unmaps children, returns memory.
- FR-8: Determinism limits — worst-case bounds on operations.
- FR-9: Cross-arch uniform ABI.
- NFRs: WCET bounds, memory budget (kernel .text size target), zero dynamic allocation, no recursion (bounded loops) — important for verification-friendly design.

Also security requirements: no ambient authority, complete mediation (every kernel entry checks a cap), rights checked once at invocation time, TOCTOU avoidance.

## 3) C Interfaces & ABI Contracts
Code blocks:

- Word types & constants:
```c
typedef uint64_t au_word_t;   /* 64-bit on all four targets */
typedef uint64_t au_cptr_t;   /* capability address: CSpace-relative */
```
- Error codes enum au_status: SUCCESS, INVAL, INVALID_CAP, ILLEGAL_DERIVATION, NOT_AUTHORIZED? Keep small: OK, E_INVAL, E_BAD_CAP, E_BAD_RIGHTS, E_NO_ASID, E_ALREADY_MAPPED, E_NOT_MAPPED, E_OVERFLOW, E_DENIED... Keep clean and minimal.

- Rights bitmask:
```c
#define AU_R (1u<<0) ...
```
Rights: R, W, X, Grant (G), Duplicate? In L4: Read/Write/Execute/Grant. For frames: R/W/X/Device-cache. Keep: AU_CAP_R, AU_CAP_W, AU_CAP_X, AU_CAP_G (grant through endpoint — but endpoints are component #03; still reserve the bit).

- Capability types enum: AU_CAP_NULL, AU_CAP_CNODE, AU_CAP_VSPACE, AU_CAP_FRAME, AU_CAP_UNTYPED, AU_CAP_IRQ, AU_CAP_DEVICE, AU_CAP_IOMMU? Maybe AU_CAP_ASID_POOL? Keep small: Null, CNode, VSpace, Frame, Untyped, IRQ, Device. IOMMU mappings can be operations on Device caps. Good — smaller.

- Cap slot internal representation (kernel-private, but document for ABI stability of the user-visible portion? Actually the internal cap node is not ABI; ABI is the syscall numbers, cptr encoding, and struct layouts passed in registers). I'll present:
  - au_cptr_t encoding: bits [63:32] CNode depth-2 index? Simpler: 32-bit slot index, upper 32 reserved zero. Two-level: cptr = (level1 << 16) | level0? Let me define: CSpace = array of 256 CNodes; each CNode = 256 slots; cptr bits [15:8] select CNode, bits [7:0] slot. That gives 65536 caps per task — enough, small, fast (two loads). But maybe more headroom: 2-level radix of fan-out 2^10 each = 1M caps; lookup = 2 dependent loads. Keep clean: fan-out 256 (8 bits) × 2 levels = 64 Ki slots; cptr is just a 16-bit index, upper bits zero. Direct and simple. Slots 0..255 in root... Actually simpler: root CNode is level 0; slot values point to either a cap or a level-1 CNode. Hmm, that complicates. Cleanest: fixed two-level table, cptr = L1index<<8 | L0index. I'll spec that.

  - Invocation ABI: syscall numbers, register mapping per arch (x86-64 SysV: rdi/rsi/rdx/rcx/r8/r9; aarch64 x0-x7; riscv a0-a7; loongarch a0-a7). Registers for: syscall nr, cspace root (implicit in current task), cptr, opcode, arg0..arg3, and return status in x0/a0/rax. Document precisely.

- API functions (user-facing wrappers around syscalls):
  - au_cap_mint(dst, src, rights) → status
  - au_cap_copy, au_cap_move, au_cap_revoke, au_cap_delete
  - au_cnode_create? Creating a CNode from untyped: au_untyped_retype(...).
  - au_frame_map(vspace_cptr, frame_cptr, vaddr, rights, attrs), au_frame_unmap
  - au_vspace_new? From untyped retype.
  - au_irq_bind? IRQ caps: au_irq_attach(dev_cptr, irqn...) — kernel allocates IRQ cap only from a Device cap or the boot master IRQ untyped. au_irq_ack.
  - au_dev_map(vspace_cptr, dev_cptr, vaddr) — map MMIO with device attributes.
  - au_iommu_map(dev_cptr, frame_cptr, iova, rights) — optional; must exist on all 4