# Session 16: Scheduler Blocking Analysis

> **VOOR AI: Lees eerst `chris docs/prompt.md` voor volledige project context, N64Recomp configuratie, en build instructies!**

**Date:** December 2025
**Status:** ROOT CAUSE FOUND - Scheduler blocks on SP/DP completion

---

## Build Instructions

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# Als toml files wijzigen:
../N64Recomp/build/N64Recomp waverace.toml

# Build:
cmake --build build -j$(nproc)

# Test:
timeout 5 ./build/WaveRace64Recompiled 2>&1 | head -100
```

---

## Summary

De game draait maar 2 frames en dan hangt de game loop. Root cause gevonden: de scheduler thread wacht op RSP/RDP completion events die nooit komen.

---

## Root Cause Analysis

### Message Queue Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ Message Queue Flow in Wave Race 64                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  VI Thread              Scheduler (Thread 3)    Game (Thread 5)  │
│     │                        │                       │          │
│     │ ──VI msg (0x19)───>    │                       │          │
│     │                   [osRecvMesg 0x80154130]      │          │
│     │                        │                       │          │
│     │                   [Process task]               │          │
│     │                        │                       │          │
│     │                   [Wait for SP/DP]       [BLOCKS HERE]    │
│     │                        │  (never comes)        │          │
│     │                        v                       │          │
│     │                   [osSendMesg 0x80154118] ───> │          │
│     │                   (only happens ONCE!)         │          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### The Problem

1. **Game Thread** (Thread 5) calls `func_80092CF0` to build display list
2. **Game Thread** sends task to scheduler queue (0x80154130)
3. **Scheduler** (Thread 3) receives task, processes it
4. **Scheduler** waits for SP (RSP) and DP (RDP) completion events
5. **SP/DP events never come** because RT64 handles graphics differently
6. **Scheduler** never sends completion to game thread queue (0x80154118)
7. **Game Thread** blocks forever on `osRecvMesg(0x80154118)`

### Evidence

```
# Scheduler receives many VI messages but only sends ONE completion:
[DEBUG-RT] osRecvMesg_recomp(mq=0x80154130, msg=0x801521CC, flags=1)...  # Many times
[DEBUG-RT] osSendMesg_recomp(mq=0x80154118, msg=0x00000033, flags=0)     # Only ONCE!

# Game thread blocks waiting:
[DEBUG-RT] osRecvMesg_recomp(mq=0x80154118, msg=0x8015195C, flags=1)...  # Forever
```

---

## Potential Solutions

### Option 1: Bypass Scheduler (Easy)

Add a stub that auto-sends completion messages to the game thread after each frame:

```cpp
// In waverace_stubs.cpp, periodically send:
osSendMesg(rdram, 0x80154118, 0x33, OS_MESG_NOBLOCK);
```

### Option 2: Fix SP/DP Event Generation (Correct)

Find where sp_complete() and dp_complete() are called and ensure they trigger after RT64 processes each display list.

### Option 3: Stub the Scheduler Function

Find the scheduler loop function and stub it to immediately forward messages.

---

## Files to Investigate

1. `lib/N64ModernRuntime/ultramodern/src/events.cpp` - sp_complete(), dp_complete()
2. `lib/N64ModernRuntime/ultramodern/src/rsp.cpp` - RSP task handling
3. `lib/rt64/src/hle/rt64_workload_queue.cpp` - Where RT64 signals completion

---

## Current State

- **State 5 → 6**: Works (auto-advance)
- **func_80092CF0**: Called 2x then blocks
- **Display list**: Generated correctly (495 commands)
- **VI messages**: Working (60+ per second)
- **Game loop**: Blocked after 2 frames

---

## Next Steps

1. Implement one of the solutions above
2. Test if game progresses past 2 frames
3. Implement state 6 → 2 transition
4. Continue with ovl_i0 implementation

---

*Session 16 - Scheduler Blocking Analysis*
