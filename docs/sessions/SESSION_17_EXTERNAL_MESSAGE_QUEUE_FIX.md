# Session 17: External Message Queue Deadlock Fix

> **VOOR AI: Lees eerst `chris docs/prompt.md` voor volledige project context, N64Recomp configuratie, en build instructies!**

**Date:** December 2025
**Status:** FIXED - External message queue deadlock resolved

---

## Build Instructions

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# Als toml files wijzigen:
../N64Recomp/build/N64Recomp waverace.toml

# Build:
cmake --build build -j4

# Test:
timeout 20 ./build/WaveRace64Recompiled 2>&1 | head -100
```

---

## Summary

**ROOT CAUSE FOUND AND FIXED**: The scheduler was blocking forever waiting for SP/DP completion messages because of an external message queue deadlock in the N64ModernRuntime.

### The Problem

1. **sp_complete()** and **dp_complete()** are called from the `gfx_thread` (NOT a game thread)
2. When called from a non-game thread, messages go to `enqueue_external_message()` instead of directly to the queue
3. `dequeue_external_messages()` is only called at the **START** of `osRecvMesg()` and `osSendMesg()`
4. When a game thread blocks in `osRecvMesg()` waiting for a message:
   - It waits in `do_recv()` loop via `run_next_thread_and_wait()`
   - While waiting, external messages accumulate but are never drained
   - **DEADLOCK**: Thread waits for message that's stuck in external queue

### The Fix

Modified `lib/N64ModernRuntime/ultramodern/src/mesgqueue.cpp` to drain external messages after waking up from blocking:

```cpp
bool do_recv(RDRAM_ARG PTR(OSMesgQueue) mq_, PTR(OSMesg) msg_, bool block) {
    // ...
    } else {
        while (MQ_IS_EMPTY(mq)) {
            ultramodern::thread_queue_insert(PASS_RDRAM GET_MEMBER(OSMesgQueue, mq_, blocked_on_recv), ultramodern::this_thread());
            ultramodern::run_next_thread_and_wait(PASS_RDRAM1);
            // WAVE RACE FIX: After waking up, drain external messages before checking the queue again.
            // This fixes deadlock where gfx_thread sends SP/DP completion via external queue,
            // but game thread is blocked in osRecvMesg and never drains it.
            dequeue_external_messages(PASS_RDRAM1);  // <-- NEW LINE
        }
    }
    // ...
}
```

Same fix applied to `do_send()` for symmetry.

---

## Technical Details

### Message Flow Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    N64ModernRuntime Message Flow                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  gfx_thread (NOT game thread)          Game Threads (1, 3, 4, 5)        │
│       │                                       │                          │
│       │ sp_complete()                         │                          │
│       │ dp_complete()                         │                          │
│       │     │                                 │                          │
│       │     ▼                                 │                          │
│       │ osSendMesg()                          │ osRecvMesg()             │
│       │     │                                 │     │                    │
│       │     │ is_game_thread() = false        │     │                    │
│       │     ▼                                 │     ▼                    │
│       │ enqueue_external_message()     dequeue_external_messages()       │
│       │     │                                 │     ▲                    │
│       │     └──────────────────────────────┬──┴─────┘                    │
│       │                                    │                             │
│       │              external_messages queue                             │
│       │                                                                  │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key Files

| File | Purpose |
|------|---------|
| `lib/N64ModernRuntime/ultramodern/src/mesgqueue.cpp` | Message queue implementation, **MODIFIED** |
| `lib/N64ModernRuntime/ultramodern/src/events.cpp` | sp_complete(), dp_complete() definitions |
| `lib/N64ModernRuntime/ultramodern/src/threads.cpp` | run_next_thread_and_wait() |

### Before vs After

| Metric | Before Fix | After Fix |
|--------|------------|-----------|
| Frames before hang | 2 | 4+ (then crash in RT64) |
| SP/DP completion | Never reached | Working |
| GFX thread | Blocked | Processing DLs |

---

## Results

After the fix:

```
[DEBUG-GFX-THREAD] Processing DL #0, data_ptr=0x801388D0
[DEBUG-SP] sp_complete() #0
[DEBUG-GFX-THREAD] Calling send_dl #0...
[RT64] send_dl: processDisplayLists(data_ptr=0x1388D0)...
[RT64] send_dl: DONE
[DEBUG-GFX-THREAD] send_dl #0 returned
│ [DL-IMPL] func_80092CF0 FRAME #3
[DEBUG-GFX-THREAD] Processing DL #1, data_ptr=0x801388D0
...
```

The game now:
1. Processes multiple display lists
2. SP/DP completion messages are sent and received
3. Scheduler no longer blocks
4. Game progresses past frame 2

---

## Remaining Issue

The game now crashes in RT64's `processDisplayLists()` around frame 4. This is a **different issue** - likely related to display list content or segment addressing - not the message queue deadlock.

Next steps:
1. Investigate RT64 crash (probably display list parsing issue)
2. Check segment 8 addressing
3. Verify display list commands

---

## Files Modified

| File | Change |
|------|--------|
| `lib/N64ModernRuntime/ultramodern/src/mesgqueue.cpp` | Added `dequeue_external_messages()` call in `do_recv()` and `do_send()` blocking loops |

---

*Session 17 - External Message Queue Deadlock Fix*
