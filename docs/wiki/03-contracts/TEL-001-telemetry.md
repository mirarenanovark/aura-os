# AuraOS Frozen Contract: Telemetry & Process Observatory (`TEL-001`)

**Contract ID:** `TEL-001` · **Status:** FROZEN · **Audited by:** Astra

---

## 1. Concrete C Wire Structures

```c
#ifndef _AURA_CONTRACT_TELEMETRY_H
#define _AURA_CONTRACT_TELEMETRY_H

#include <stdint.h>

#define AURA_TELEMETRY_ABI_VERSION 1
#define AURA_MAX_PROCS             256
#define AURA_MAX_CORES             16

#pragma pack(push, 8)

struct aura_proc_record {
    uint32_t pid;
    uint32_t generation;          /* Avoids PID reuse race */
    uint32_t parent_pid;
    uint32_t thread_count;
    
    uint8_t  state;               /* RUNNING, READY, SLEEPING, ZOMBIE */
    uint8_t  priority;            /* PRIO_IDLE ... PRIO_REALTIME */
    uint8_t  capsule_origin;      /* 0:Native, 1:Linux, 2:Win, 3:Android */
    uint8_t  _pad0;
    
    uint32_t cpu_affinity_mask;
    uint32_t open_fds;
    
    uint64_t cpu_ticks;           /* Scheduler time since boot */
    uint64_t mem_rss_bytes;       /* Resident physical pages */
    uint64_t mem_shm_bytes;       /* Shared-memory canvas pages */
    uint64_t disk_read_bytes;
    uint64_t disk_write_bytes;
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    
    char     name[32];            /* Null-terminated executable name */
};

struct aura_telemetry_snapshot {
    uint32_t abi_version;         /* Must match AURA_TELEMETRY_ABI_VERSION */
    uint32_t proc_count;          /* Number of valid records in procs[] */
    uint64_t snapshot_timestamp;  /* Monotonic tick counter */
    
    uint64_t total_ram_bytes;
    uint64_t free_ram_bytes;
    uint64_t kernel_ram_bytes;
    uint64_t cache_ram_bytes;
    
    uint32_t core_count;
    uint8_t  core_usage_percent[AURA_MAX_CORES];
    
    struct aura_proc_record procs[AURA_MAX_PROCS];
};

#pragma pack(pop)

#endif
```

---

## 2. Invocation Contract
```c
int sys_telemetry(struct aura_telemetry_snapshot *out_buf, uint32_t buf_size);
```
* `buf_size` must equal `sizeof(struct aura_telemetry_snapshot)`.
* Returns `0` on success, `-EINVAL` if buffer size is wrong, `-EFAULT` if memory unmapped.
* **Guarantee:** Fast non-blocking snapshot copy. Zero memory allocations during query.
