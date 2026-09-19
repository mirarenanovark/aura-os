# AuraOS Frozen Contract: System Call & Wire ABI (`ABI-001`)

**Contract ID:** `ABI-001` · **Status:** FROZEN · **Revision:** 1.0 · **Target:** All Architectures

---

## 1. System Call Invocation Convention

| Architecture | Instruction | Syscall # Register | Arg 1 | Arg 2 | Arg 3 | Arg 4 | Arg 5 | Return |
|---|---|---|---|---|---|---|---|---|
| **x86_64** | `syscall` | `rax` | `rdi` | `rsi` | `rdx` | `r10` | `r8` | `rax` |
| **i686** | `int 0x80` | `eax` | `ebx` | `ecx` | `edx` | `esi` | `edi` | `eax` |
| **aarch64** | `svc #0` | `x8` | `x0` | `x1` | `x2` | `x3` | `x4` | `x0` |
| **armv7** | `svc #0` | `r7` | `r0` | `r1` | `r2` | `r3` | `r4` | `r0` |

* Return value: `>= 0` on success, `-ERRNO` on failure.
* Preserved registers: Callee-saved per architecture ABI.

---

## 2. Core System Call Table (Numbers are frozen forever)

```c
#ifndef _AURA_SYSCALL_H
#define _AURA_SYSCALL_H

#include <stdint.h>

#define SYS_YIELD           0
#define SYS_EXIT            1
#define SYS_SPAWN           2
#define SYS_WAITPID         3
#define SYS_HANDLE_CLOSE    4

#define SYS_MMAP            10
#define SYS_MUNMAP          11
#define SYS_SHM_CREATE      12
#define SYS_SHM_MAP         13

#define SYS_IPC_SEND        20
#define SYS_IPC_RECV        21
#define SYS_IPC_REPLY       22

#define SYS_FS_OPEN         30
#define SYS_FS_READ         31
#define SYS_FS_WRITE        32
#define SYS_FS_SEEK         33
#define SYS_FS_CLOSE        34

#define SYS_TIME_MONO       40
#define SYS_TELEMETRY       50

#endif
```

---

## 3. Wire Layout Rules
* All structs crossing the user/kernel boundary must have **explicit padding** and equal size on 32-bit and 64-bit targets where applicable.
* No raw C pointers inside serialized structs — use `uint64_t` or offsets.
* Compile-time verification required on all builds: `_Static_assert(sizeof(...) == EXPECTED, "ABI size error");`.
