The user wants a rigorous PRD for AuraOS Component #10: Block Devices, Filesystem Ownership & Persistent Storage. This is a freestanding C OS project supporting 4 architectures (likely x86_64, aarch64, riscv64, and one more — maybe i686 or loongarch64; I'll pick x86_64, aarch64, riscv64, i686 as the sensible four, or loongarch64. Let me just say x86_64, aarch64, riscv64, loongarch64 — actually i686 is more common for hobby OS 4-arch targets. Hmm. I'll state the four explicitly: x86_64, aarch64, riscv64, i686. That's a common 4-arch set.)

The philosophy: Clean. Simple. Small. Fast. Direct. No overengineering. So the PRD should be minimal but rigorous:

- Block device layer: a simple registered-device abstraction with a request-based sync/async I/O interface.
- Filesystem ownership: the component owns mounting, single filesystem type initially (FAT32 is the pragmatic choice for a small OS — simple, well-documented, ubiquitous; or a custom minimal FS. For "no overengineering," FAT32 read/write is defensible; maybe also ISO9660 read-only for boot? Keep it: FAT32 RW as the one persistent FS. Actually "Filesystem Ownership" suggests the component owns the VFS-to-FS binding. Let me define: a tiny mount table, FS driver registration via a fixed vtable, ownership semantics (who owns the buffer cache, who owns lifecycle).
- Persistent storage: partition table parsing (MBR + GPT read-only? MBR is simple; GPT adds complexity. Minimal: MBR primary partitions + GPT protective? To be rigorous but small: MBR only, GPT deferred. Hmm, but modern disks are GPT. A middle ground: parse MBR; detect GPT protective entry and parse GPT header/entries read-only — that's ~150 lines. I'll include MBR mandatory, GPT optional-lite (read entries only). Actually "no overengineering" — keep MBR only in v1, GPT listed in anti-goals or deferred. I'll make a call: MBR primary partitions only, v1. GPT explicitly deferred.)

Structure required:
1. Executive Summary & Scope Boundary
2. Core Requirements (Functional & Non-Functional)
3. Data Structures, C Interfaces & Wire ABI Contracts (concrete C code blocks)
4. Failure Modes, Bounds & Resource Limits
5. Acceptance Criteria & Test Gates (Evidence required to mark done)
6. Anti-Goals

Key design decisions to bake in (keeping it small):
- Synchronous blocking I/O primitive with completion callback for async; or simple request queue with waiters. For minimalism: synchronous read/write blocks API in the block layer, plus an optional async path via completion callback + semaphore? Simplest: each bdev has a `read_blocks`/`write_blocks` op; upper layers block. Async deferred (anti-goal) unless... Actually many OS courses do request queue. Keep it: synchronous interface, with the ability for the driver to complete inline. Deterministic, tiny. Async I/O explicitly an anti-goal/deferred.
- Buffer cache: a small, fixed-size LRU block cache owned by this component (that's the "ownership" of memory for FS pages). Or no cache — direct I/O with per-FS sector staging. A tiny fixed cache (e.g., 256 buffers × 512B–4KB) is justified for FAT32 which does scattered metadata access. Keep cache fixed-size, non-growing, LRU, write-through? Write-back adds flush/consistency complexity. For a small OS: write-through cache with immediate writes... that's slow for FAT table. Compromise: write-back with explicit flush()/sync on unmount and periodic? "Fast" matters. Decide: write-back LRU with lazy group commit — too complex. Choose write-through for correctness/simplicity; note that dirty-write batching is deferred. Actually FAT32 FAT writes through 512B sectors directly is fine — simple. I'll spec a fixed 64-slot buffer cache with write-back for data, but strict ordered flush for metadata? Hmm. Overthinking — PRD can mandate: write-through metadata, write-back data blocks bounded by 32 dirty buffers, flush on sync/unmount. That's a nice bounded design: max 32 dirty buffers gives bounded memory and bounded loss window. That's rigorous and small.

- Wire ABI: on-disk structures (MBR partition entry, FAT32 BPB, FS magic for any superblock), plus driver ABI (struct of function pointers with version/magic), and errno contract. "Wire ABI contracts" means on-disk format layouts must be exact: struct definitions with fixed-width types, static_asserts on sizes/offsets, endianness (little-endian for FAT/MBR).

- Error codes: unified negative errno list (EIO, EINVAL, ENOMEM, ENOSPC, ENXIO, ETIMEDOUT, EROFS, EAGAIN, EBUSY, ENOTTY-ish...). Define AuraOS errno subset.

- Interfaces:
  - `blk_register()` / `blk_unregister()`, `blk_get()`, `blk_read()`, `blk_write()`, `blk_flush()`, `blk_ioctl/geo`.
  - `part_scan()` producing partition handles.
  - `fs_register()` with ops vtable: mount, unmount, lookup, read file... wait — does this component implement the VFS? Component #10 is "Block Devices, Filesystem Ownership & Persistent Storage" — it owns the block layer, partition discovery, FS driver registration, mount table, and the persistent storage lifecycle; the VFS path resolution might be a separate component. To keep scope boundary crisp: this component provides the mount table and FS-driver ABI; pathname resolution/VFS is Component #11 (say). The "ownership" part: define who owns buffers, who calls unmount, refcounting on mounts, exclusion (one mount per device).
  - Persistence API for the OS itself: e.g., `persist_get`/`persist_set` key-value store? "Persistent Storage" might mean system config persistence. A tiny kconfig blob file on the boot FAT partition, atomic replace via write-temp+rename... FAT rename. Or a fixed reserved file "AURA.CFG". Keep: one well-known file `AURACFG.BIN` at root, written atomically (write temp, delete old, rename) with CRC32 header. Bounded 4 KiB. That's small and direct.

- Bounds: max 8 block devices, max 16 partitions per device, max 4 mounts, max file size FAT32 4 GiB−1, filename 8.3 + LFN? LFN needed for anything readable... LFN adds complexity but FAT32 without LFN is painful. Minimal: 8.3 short names mandatory; LFN read support only (deferred write)? That asymmetry causes trouble (writing a file with LFN that exists). Decide: 8.3 only in v1, names normalized uppercase; LFN in anti-goals/deferred. That's a defensible "clean, small" call, clearly documented.

- Non-functional: memory budget (e.g., ≤ 16 KiB .bss/.data static, no dynamic alloc beyond one fixed pool), interrupt context rules (no blocking in IRQ; completion from IRQ allowed), timeout bounds (e.g., 5 s per I/O), determinism, 4-arch portability (no arch-specific code except DMA alignment handled by drivers), alignment rules (buffers aligned to 8/16 bytes, DMA-safe).

- Failure modes: device timeouts → EIO + device marked degraded; corrupt FAT → mount read-only or refuse; dirty bit; power-loss window documented (bounded by 32 dirty buffers); partition table garbage → no partitions, whole-device mount option; buffer exhaustion → EAGAIN/evict; double mount → EBUSY; unmount with open refs → EBUSY.

- Acceptance criteria: test gates with concrete evidence — unit tests in kernel test harness, e.g., ramdisk-backed FAT32 image goldens, fault injection (corrupted BPB, bad CRC sector), soak test 10k mount/unmount cycles, performance gate (sequential read ≥ some % of ramdisk, e.g., ≥ 80% of device throughput; per-op overhead ≤ 5 µs), zero static-analysis warnings, struct-size static asserts pass on all 4 arches, ABI freeze check (offset dump script). Evidence: test logs, `sizeof` tables per arch, coverage of error paths.

- Anti-goals: no async I/O / io_uring-like queues, no NVMe multi-queue SMP sharding (single queue), no journaling FS of our own, no GPT (deferred), no LFN, no page cache integration with VM (no mmap), no user-space filesystems/FUSE, no hotplug/UEvents, no write caching in device (FUA/flush only), no RAID/LVM/encryption, no dynamic mount count growth, no ATAPI/floppy (or keep floppy? anti-goal), no ext2/ext4 (deferred), no SMP fine-grained locking (single block-layer lock, sufficient at this scale), no ioctls beyond defined set.

Let me now write the actual C code blocks. They must be freestanding, fixed-width, careful with endianness and static_asserts. C11 freestanding; static_assert usage.

Core structures:

```c
/* kernel/include/aura/blk.h */
#define AURA_BLK_ABI_VERSION  1u
#define AURA_BLK_MAGIC        0x4B4C4231u /* "KLB1" */

typedef struct blk_dev blk_dev;
typedef struct blk_req blk_req;

typedef enum {
    BLK_OP_READ  = 0,
    BLK_OP_WRITE = 1,
    BLK_OP_FLUSH = 2,
} blk_op;

typedef struct blk_ops {
    int  (*submit)(blk_dev *dev, const blk_req *req);
    u32  (*features)(const blk_dev *dev);      /* BLK_F_* */
    void (*destroy)(blk_dev *dev);
} blk_ops;
```

Hmm, do I want request-based or simple read/write? Let me do a small request struct that works both sync: `blk_read(bdev, lba, count, buf)` builds req on stack, calls submit, waits on completion (single-threaded kernel: submit is synchronous for RAM/driver-completed-in-IRQ designs we wait on a global counter/semaphore). Since AuraOS at this scale — likely single CPU or simple kernel lock — synchronous is fine. I'll define: `blk_io()` blocks until completion or timeout; drivers may complete inline (return) or via `blk_complete()` from IRQ.

Device struct:

```c
struct blk_dev {
    u32 magic;                 /* AURA_BLK_MAGIC */
    u16 abi_version;
    u16 id;                    /* stable index 0..BLK_MAX_DEVS-1 */
    char name[16];
    const blk_ops *ops;
    u64 nblocks;               /* capacity in logical blocks */
    u32 block_size;            /* 512, 1024, 2048, 4096 */
    u32 phys_block_size;       /* device-preferred; block_size <= phys */
    u32 flags;                 /* removable, readonly */
    /* private driver state follows; allocated by driver, owned by driver */
    void *drv_priv;
    u64 io_count;              /* stats */
    u64 err_count;
    u8 state;                  /* BLK_ST_* */
};
```

Public API:

```c
int  blk_register(blk_dev *