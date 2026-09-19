The user wants a complete, rigorous PRD for AuraOS Component #08: Read-Only Initramfs, Bounded RAMFS & Native Shell Slice. This is a freestanding C operating system project with 4 architectures (presumably x86_64, i686, aarch64, and riscv64). The style requirements: Clean. Simple. Small. Fast. Direct. No overengineering.

The PRD must follow the exact structure:
1. Executive Summary & Scope Boundary
2. Core Requirements (Functional & Non-Functional)
3. Data Structures, C Interfaces & Wire ABI Contracts (concrete C code blocks)
4. Failure Modes, Bounds & Resource Limits
5. Acceptance Criteria & Test Gates (Evidence required to mark done)
6. Anti-Goals (What this component strictly does NOT do to avoid bloat)

Let me think about what this component actually is:

**Component #08** of AuraOS covers:
1. **Read-Only Initramfs** — a packed cpio-like archive (probably a custom minimal format, or newc cpio) baked into the kernel image or loaded by the bootloader, mounted as the root filesystem, read-only.
2. **Bounded RAMFS** — a small in-memory writable filesystem with hard caps on memory usage (a memory budget). Used for /tmp, /dev, /etc overlays, etc.
3. **Native Shell Slice** — a minimal interactive shell running on the kernel console. Not a full POSIX shell — a "slice" meaning the minimal viable command set: ls, cat, cd, echo, help, clear, meminfo, maybe ps/kill if processes exist, mount info, hexdump, etc.

Key design decisions to make it clean and small:
- One VFS layer shared by both initramfs and ramfs? Or keep them separate? Given "no overengineering," a minimal VFS with a unified inode/file interface is justified because the shell needs to traverse both. But it must be tiny.
- Initramfs format: The PRD should decide between standard cpio newc format vs a custom format. Standard `cpio -H newc` is simple to parse (~110 byte header, ASCII hex fields), well-understood, and can be generated with standard host tools. That's the right call — no custom packing tool needed. Hardlinks/symlinks: support symlinks? Minimal: support regular files, directories, symlinks (maybe). To keep it small: regular files + directories only, symlinks rejected at parse time with a warning. Actually, a shell might want symlinks... but anti-bloat says: no symlinks in v1. Keep it grounded.
- RAMFS bounded: a global byte budget (e.g., 1 MiB default, tunable at boot via kernel cmdline `ramfs.max=`). Allocation from the kernel heap with per-file and global accounting. When budget is exceeded: ENOSPC. Synchronous write-behind? No — writes go directly to memory. No page cache layer — the file content IS the memory.
- Shell: line editor with basic editing (backspace, arrow keys maybe), command table, builtins only. Max line length 256 bytes. Max args 16. No pipes, no redirection, no variables, no scripting/globbing in v1? Hmm, "Native Shell Slice" — slice suggests minimal. Let me define: builtins: help, ls, cat, cd, pwd, echo, clear, meminfo, mounts/free? hexdump, stat, uname, uptime, reboot. Redirection maybe not. Keep it small.

Wire ABI contracts: This should include:
- The initramfs archive on-disk (in-image) format: cpio newc header struct, magic "070701"/"070702", ASCII hex fields, the TRAILER!!! entry.
- The kernel command line contract: `initrd=` is not needed if the bootloader multiboot2 module; but wire contract for how the initramfs is located: Multiboot2 module tag, or device tree /chosen/linux,initrd-start/end for aarch64/riscv64, or as a section in the kernel image. For 4 arches: multiboot2 (x86), Limine protocol? Let me define: x86_64/i686 use Multiboot2 module 0; aarch64 uses DTB /chosen with linux,initrd-start & linux,initrd-end; riscv64 same via DTB. Also a linker-section embedded fallback (`__initrd_start`/`__initrd_end`) so QEMU `-initrd` or direct kernel works. Actually simplest universal fallback: embed via objcopy into a section. I'll define both: primary = bootloader-passed, fallback = embedded section.
- The syscall or kernel-internal interface: Since AuraOS at component #08 may not have full userland syscalls yet, the shell runs as a kernel task. Interfaces are kernel-internal C APIs (kapi). But the PRD asks for "Wire ABI Contracts" — meaning on-media/in-memory formats that must remain stable. So: cpio newc subset, ramfs superblock layout, shell command table ABI (struct with name, handler, help string), VFS inode ops table.

Data structures:
- `struct vfs_node` with ops table, refcount, type, size, data pointer (for ramfs), children as simple linked list or a tiny hash? For small scale, sorted linked list or simple array. Clean and small: singly-linked list per directory with lookup; O(n) fine for tens of entries.
- `struct file` (open file instance) with offset, flags, ref to node.
- `struct ramfs_super` with budget, used, inode count caps.
- Initramfs parse: zero-copy — the archive stays in its loaded memory; file data pointers reference directly into the archive (read-only, no copy). That's a key "fast/direct" design point: initramfs nodes point into the archive buffer; no copying except path strings? Path strings could also point into archive but they're prefixed with "./" typically; copy path components into nodes for normalization. Minimal: copy names into a small per-node buffer (name limited to 64 bytes) or reference in place. To keep small: store pointer+length into archive for name too, but names in cpio are NUL-terminated so pointer works fine. But we need the mount point prefix handling. Keep: name is a char* into archive memory (stable, kernel keeps archive memory reserved). Good.

- VFS operations: vfs_lookup, vfs_open, vfs_read, vfs_opendir/readdir, vfs_mount maybe minimal.

Bounds:
- MAX_INITRAMFS_SIZE: 32 MiB
- RAMFS_BUDGET_DEFAULT: 1 MiB, min 64 KiB, max 16 MiB
- MAX_FILES_RAMFS: 512
- MAX_PATH 256 (PATH_MAX 256), NAME_MAX 64
- SHELL_MAX_LINE 256, SHELL_MAX_ARGS 16, SHELL_MAX_ARG 128
- open file table: 64 files globally
- Each ramfs file max size: 256 KiB
- Directory entries per dir: unlimited but bounded by file count cap.

Failure modes:
- Corrupt archive: header magic mismatch → parse abort, kernel continues with empty root + ramfs root, panic policy decision: warn and continue with fallback minimal root (shell still works). Actually a rootless kernel with a shell is the point — the shell is the recovery path. Good design: if initramfs is missing/corrupt, mount ramfs at `/`, create `/dev`, `/tmp`, and shell runs. That's "direct."
- Budget exceeded: ENOSPC, no OOM surprises.
- Integer overflow on cpio hex fields: validate with strtoul-like custom parser bounded to 8 hex digits.
- Alignment: cpio newc headers are 4-byte aligned; validate name length padding.

Non-functional:
- Deterministic memory: total RAMFS memory bounded by budget + fixed overhead table.
- No dynamic allocation in shell parser (stack buffers).
- Startup: initramfs mount < 5 ms on reference target (QEMU) for a 1 MiB archive.
- Code size budget: initramfs+ramfs+vfs+shell ≤ 24 KiB of .text across arches? Give a number like ≤ 16 KiB per-arch text, total measured via nm. Let's say: combined .text ≤ 20 KiB on x86_64 -O2.
- No locks beyond a single spinlock/mutex per ramfs mount (single CPU at this stage? components earlier presumably have SMP or not — keep it simple: one global ramfs lock, IRQ-safe).
- Freestanding C: -ffreestanding, no libc, no stdio; all printf via existing kernel console (Component for kprintf).

Acceptance criteria with evidence:
- Test list: unit tests via ktest harness, integration boot tests on all 4 arches in QEMU, screenshots/logs of boot, `ls -R` golden output diff, budget-exhaustion test writes file until ENOSPC and verifies used-byte accounting returns to zero after unlink, power-loss? Not applicable since no persistence. Determinism: same archive → same inode numbering? Provide stable inode numbers derived from archive order (1..N).
- Fuzz test: corrupt byte sweep on archive headers (e.g., 1000 random corruptions) → no crash, parse fails cleanly.
- Metrics: memory accounting test, boot time measure, code size gate via `nm --size-sort`.

Anti-goals:
- No write support to initramfs (never).
- No page cache, no dcache, no dentry trees, no RCU, no namespace replication.
- No symlinks, hardlinks, xattrs, ACLs, POSIX perms enforcement (store mode bits, don't enforce), no timestamps beyond maybe mtime stored unused.
- No tmpfs shrinking/reclamation of clean pages to swap (no swap).
- No pipes/redirection/globbing/env vars/scripting in shell; not POSIX-compatible; no dynamic loading of external commands (v1 builtins only; extension point: command table entry).
- No gzip/xz compressed initramfs (uncompressed cpio only; compression belongs to bootloader/loader or later component).
- No directories writable in initramfs — writes require copy to ramfs... actually "overlay" is an anti-goal. Reads of /etc from initramfs; writes only in ramfs mounts.
- No seeking beyond read for special files? Keep: seek supported (offset clamped).
- No mmap support (v1).
- No async I/O, io_uring-anything.
- No mount propagation/remount/loop mounts of other fs types; exactly two fs implementations, exactly one ramfs instance? Allow a second ramfs instance for /tmp? Simplicity: single ramfs mounted at /ram? Hmm.

Mount layout decision: Keep it dead simple:
- `/` = initramfs (read-only)
- `/tmp` = ramfs (bounded, writable) — mounted over the initramfs `/tmp` directory if present, or auto-created.
Mount point crossing in lookup: VFS supports mounting a ramfs on a directory node. Minimal: a single mount table of, say, 4 entries. That's justified, tiny.

Actually even simpler: root is initramfs; ramfs is mounted at `/tmp` and `/var` (both if needed). Mount table MAX_MOUNTS 4. Fine.

Alternatively root = ramfs and initramfs mounted at /boot? No — classic: root = initramfs, /tmp = ramfs. I'll go with that.

Inode numbering: initramfs inodes numbered sequentially at parse (1-based). ramfs inodes numbered from a separate counter (e.g., starting at 0x40000000) or