# AuraOS Audio System Specification (AuraAudio)
**Architecture, Protocol, Effects Graph & DAW Compatibility**

**Document:** `docs/wiki/04-prds/PRD-12-audio.md` · **Status:** NORMATIVE · **Owner:** miradev

---

## 1. Core Principles (Astra Audio Contract)

1. **Zero DSP in the Kernel:** The kernel provides only a DMA ring-buffer driver and periodic IRQ notification. All mixing, effects, routing, and format conversion run strictly in userland.
2. **Zero Allocation in the Realtime Path:** No `malloc`/`free`, no blocking locks, no IPC system calls, no disk/network I/O in the audio process callback.
3. **Lock-Free SPSC Shared Memory Rings:** Communication between client apps (DAW, player, game) and the audio mixer uses single-producer, single-consumer lock-free ring buffers in shared memory.
4. **Baseline Format:** 32-bit float stereo @ 48,000 Hz internal graph format. Bit-perfect pass-through when format matches device.
5. **Ultra-Low Latency:** Negotiable buffer sizes:
   * **DAW/Pro Mode:** 64–128 samples (~1.3ms–2.6ms buffer latency) for live synthesizer and MIDI monitoring.
   * **Standard Desktop:** 512 samples (~10.6ms buffer latency) for power savings and rock-solid glitch-free playback.

---

## 2. Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Audio Applications                       │
│                                                                 │
│  [Aura DAW / Tracker]      [Media Player]       [Game / Capsule]│
│           │                       │                     │       │
│           ▼                       ▼                     ▼       │
│  [libaura-audio (Client Library: Zero-Copy Shm Ring Buffers)]   │
└───────────────────────────────────┬─────────────────────────────┘
                                    │ Lock-free Shm Rings (SPSC)
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│              AuraAudio Server (`aura-audiod`)                   │
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │               Routing Matrix & Channel Mixer            │   │
│   └────────────────────────────┬────────────────────────────┘   │
│                                ▼                                │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │       PulseEffects-Style Real-time DSP Graph (C99)      │   │
│   │                                                         │   │
│   │   [10-Band EQ] ──► [Compressor/Limiter] ──► [Bass Boost]│   │
│   │        │                                         │      │   │
│   │   [Stereo Widener] ──► [Convolver/Reverb] ───────┘      │   │
│   └────────────────────────────┬────────────────────────────┘   │
│                                ▼                                │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │   Hardware Backend (HDA Intel / AC97 / VirtIO-Sound)    │   │
│   └─────────────────────────────────────────────────────────┘   │
└───────────────────────────────────┬─────────────────────────────┘
                                    │ Direct Kernel DMA Ring Buffer
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│           Kernel Audio Driver (`kernel/drivers/audio/`)         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Real-Time Effects Suite (PulseEffects-Style, Built in Pure C)

AuraAudio includes an optional, highly optimized modular DSP chain that can be toggled per-app or globally with near-zero CPU cost:

1. **10-Band Parametric Equalizer (`eq10`):**
   * Biquad IIR filter bank with SSE/NEON vectorization (<0.5% CPU).
   * Presets: *Aero Clarity*, *Warm Vintage*, *Vocal Boost*, *Acoustic*, *Flat*.
2. **Dynamic Range Compressor & Peak Limiter (`limiter`):**
   * Soft-knee compression preventing audio clipping/distortion.
   * Fast attack (5ms), program-dependent release (50-250ms).
3. **Bass Enhancer / Sub-Bass Synthesizer (`bassboost`):**
   * Harmonic saturation algorithm for small laptop/handheld speakers (adds perceived bass without cone distortion).
4. **Stereo Enhancer / Spatial Widener (`widener`):**
   * Mid/Side matrix phase adjustment for an expansive, airy soundstage.
5. **Lightweight Convolution Reverb (`reverb`):**
   * Fast partitioned convolution for room acoustic modeling.
6. **Tube Preamp Saturation (`tube`):**
   * Warm harmonic distortion modeled after classic analog circuits.

---

## 4. DAW & Pro-Audio Compatibility Contract

* **Bit-Exact Engine:** IEEE 754 float32 internal representation with 64-bit float summation nodes to eliminate rounding noise.
* **Master Clock & Transport Sync:** Shared-memory transport structure with integer sample-frame counter, BPM, bar/beat location, and play/pause state.
* **Native Plugin Format (`.aura-fx`):**
  * Tiny C shared library with a frozen 4-function interface:
  ```c
  struct aura_fx_descriptor {
      const char *name;
      uint32_t version;
      void* (*init)(uint32_t sample_rate, uint32_t block_size);
      void  (*process)(void *instance, const float *in_l, const float *in_r, float *out_l, float *out_r, uint32_t frames);
      void  (*set_param)(void *instance, uint32_t param_id, float value);
      void  (*destroy)(void *instance);
  };
  ```
* **VST2 / LV2 / CLAP Bridge:** Simple userland wrapper linking standard C/C++ audio plugins directly into the AuraAudio graph.

---

## 5. Failure & Latency Recovery

* **Glitch/Underrun Handling:** If a client app hangs or misses its realtime deadline:
  * The server zero-fills (silence) the client's slice without popping or clicking.
  * Increments an underrun diagnostic counter.
  * Does NOT block the main mix loop or other applications.
* **Automatic Device Recovery:** If an external USB audio interface is disconnected, AuraAudio seamlessly redirects streams to onboard audio/VirtIO-Sound in <10ms.
