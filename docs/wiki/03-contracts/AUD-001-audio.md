# AuraOS Frozen Contract: AuraAudio Realtime Audio Protocol (`AUD-001`)

**Contract ID:** `AUD-001` · **Status:** FROZEN · **Owner:** aura-audiod

---

## 1. Lock-Free SPSC Shared-Memory Ring Buffer

```c
#ifndef _AURA_CONTRACT_AUDIO_H
#define _AURA_CONTRACT_AUDIO_H

#include <stdint.h>

#define AURA_AUDIO_ABI_VERSION 1
#define AURA_MAX_CHANNELS 2

/* 64-byte cache-line aligned sequence counters */
struct aura_audio_ring {
    /* Consumer cursor - written ONLY by the consumer */
    uint64_t read_pos  __attribute__((aligned(64)));
    /* Producer cursor - written ONLY by the producer */
    uint64_t write_pos __attribute__((aligned(64)));
    
    uint32_t channel_count;
    uint32_t sample_rate_hz;
    uint32_t capacity_frames;     /* Power of 2 */
    uint32_t format;              /* 0: F32, 1: S16LE */
    
    /* Float samples interleaved [L,R,L,R...] */
    float    samples[];
};

/* Realtime transport state (DAW friendly, shared read-only to clients) */
struct aura_audio_transport {
    uint64_t sample_frame_counter;      /* Absolute position in song */
    uint32_t is_playing;                /* 0: Stopped, 1: Playing */
    uint32_t bpm_x100;                  /* Tempo */
    uint32_t bar;               
    uint32_t beat;              
    uint32_t tick_within_beat;  
};

#endif
```

## 2. Realtime Restrictions (Violation = Bug)
Inside the audio processing callback, code **MUST NOT**:
* Allocate or free memory (`malloc`/`free`)
* Acquire blocking mutexes/locks
* Perform file, network, or console I/O
* Call into the kernel (syscalls)
* Wait on condition variables

## 3. Underrun Policy
* If a client misses its deadline, `aura-audiod` outputs silence for that period.
* Underruns are counted in a diagnostic atomic; they never block the master mix loop.
