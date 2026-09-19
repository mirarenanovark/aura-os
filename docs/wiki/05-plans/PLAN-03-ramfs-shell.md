# AuraOS Implementation Plan 03: Initramfs, RAMFS & Interactive Shell

**Plan ID:** `PLAN-03` · **Implements:** `PRD-08-filesystem-shell.md` · **Phase:** Phase 3

---

## 1. Goal
Mount a read-only boot initramfs (`tar` or `cpio`), create an in-memory writable RAM filesystem (`ramfs`), implement basic VFS file descriptors, and boot into an interactive userland shell (`aura-sh`) over serial console.

## 2. Work Breakdown

| Task | File Path | Deliverable |
|---|---|---|
| **3.1** | `kernel/fs/vfs.c` | Virtual File System root, inode table, file descriptor translation |
| **3.2** | `kernel/fs/initramfs.c` | Read-only tar/cpio parser mounted at `/boot` |
| **3.3** | `kernel/fs/ramfs.c` | Bounded dynamic RAM filesystem mounted at `/tmp` and `/home` |
| **3.4** | `userland/libc/` | Minimal freestanding libc subset (`printf`, `memcpy`, `malloc`, `syscall`) |
| **3.5** | `userland/sh/aura_sh.c` | CLI Shell: `ls`, `cat`, `echo`, `touch`, `ps`, `kill`, `exec` |

## 3. Verification Criteria
* Boot QEMU without disk image (initramfs only).
* In shell: `touch /tmp/test.txt && echo "hello" > /tmp/test.txt && cat /tmp/test.txt` outputs `hello`.
* Launch a secondary ELF binary from `/boot/bin/hello` and verify clean exit.
