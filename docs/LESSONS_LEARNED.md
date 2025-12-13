# Lessons Learned - Wave Race 64 Recompilation

> **For AI/LLM: This document contains important lessons from previous sessions. Read this BEFORE you start debugging!**

---

## Lesson 1: ALWAYS Compare Flow with the Decomp (Session 20)

### The Problem

We had a "hack" in `waverace_stubs.cpp` that manually set the state from 6 to 2 after 180 frames:

```cpp
// WRONG - This was our hack:
static int state6_frames = 0;
if (game_state == 6) {
    state6_frames++;
    if (state6_frames == 180) {
        write_u32(rdram, ADDR_GAME_STATE, 2);  // Only sets state
        write_u32(rdram, ADDR_BOOT_FLAG, 1);   // And boot flag
    }
}
```

### What the Decomp Shows

The real game does this **completely differently**:

```c
// CORRECT - In decomp (func_1B1FB0_802C5BA4):
// After 14 frames, func_801EB180() is called which:
D_800DAB24 = 2;       // State
D_801CE630 = 0;
D_801CE638 = 0;
D_801CE63C = 1;       // Boot flag
D_800DAB1C = 0;
D_800D461C = 3;
gPlayers = 1;
gRiders = 2;
gGameModes = 0;
D_800D49B0 = 0x14;
D_800D8174 = 5;
D_801CE728 = 3;
// ... and 10+ other variables!
```

### The Lesson

**ALWAYS compare the decomp flow with our recomp flow before writing a hack!**

1. Search in decomp where state transitions happen
2. See which functions are called
3. See which variables are set
4. Let the real code run instead of a hack

---

## Lesson 2: `ignored` vs `stubs` in waverace.toml (Session 15+)

### The Difference

| Config | What N64Recomp does | When to use |
|--------|---------------------|-------------|
| `stubs = ["func"]` | Generates EMPTY stub function | Hardware/MMIO/COP0 that can't be translated |
| `ignored = ["func"]` | Generates NOTHING | Functions where YOU write custom code |

### Common Mistake

```toml
# WRONG - Function is in ignored but we have no custom implementation!
ignored = [
    "func_i0_802C5800",  # <- N64Recomp generates nothing
]

# And in waverace_stubs.cpp:
extern "C" void func_i0_802C5800(...) {
    ctx->r2 = ctx->r4;  # <- Does nothing!
}
```

### The Lesson

1. **If you use `ignored`, you MUST write a working custom implementation**
2. **If you don't have a custom impl, REMOVE the function from ignored**
3. **Let N64Recomp generate the real code if possible**

---

## Lesson 3: Overlay System - All Overlays Share the Same VRAM (Session 12-20)

### How N64 Overlays Work

```
ROM Layout:
├── 0x1B1FB0 - 0x1B3EC0: ovl_ings (boot)
├── 0x1B3EC0 - 0x1B55A0: ovl_i0 (menu)
├── 0x1B55A0 - 0x1B9440: ovl_i1 (title)
└── ...

But they ALL load to VRAM 0x802C5800!
```

### What This Means

- The game loads different overlays to the same address
- `func_802C5800` in state 5/6 is DIFFERENT code than `func_802C5800` in state 2
- N64Recomp must define each overlay separately with unique names

### In waverace.syms.toml

```toml
# Boot overlay (state 5/6)
[[section]]
name = ".ovl_segment_1B1FB0"
rom = 0x001B1FB0
vram = 0x802C5800
functions = [
    { name = "ovl_func_802C5800", vram = 0x802C5800 },  # Note: ovl_ prefix
    { name = "ovl_func_802C5BA4", vram = 0x802C5BA4 },
]

# Menu overlay (state 2/3/4)
[[section]]
name = ".ovl_802C_ovl_i0"
rom = 0x001B3EC0
vram = 0x802C5800
functions = [
    { name = "func_i0_802C5800", vram = 0x802C5800 },  # Note: func_i0_ prefix
]
```

### The Lesson

**Use unique function names for each overlay, even if they have the same VRAM address!**

---

## Lesson 4: Debug Output is Your Eyes (All Sessions)

### The Problem

As AI, we have no visual output. We can't SEE what the game does.

### The Solution

**ALWAYS add extensive debug output:**

```cpp
// GOOD:
printf(">>> [STATE %d] CALLING func_X\n", state);
printf("    Input: r4=0x%08X, r5=0x%08X\n", ctx->r4, ctx->r5);
printf("    Variables: D_800DAB24=%d, D_801CE63C=%d\n",
       read_u32(rdram, 0x800DAB24), read_u32(rdram, 0x801CE63C));
fflush(stdout);

// After the call:
printf("<<< RETURNED from func_X\n");
printf("    Output: r2=0x%08X\n", ctx->r2);
```

### The Lesson

1. Print BEFORE and AFTER each important function call
2. Print state variables (D_800DAB24, D_801CE63C, etc.)
3. Print register values (ctx->r4, ctx->r2, etc.)
4. Use `fflush(stdout)` to see output immediately
5. Use markers (>>>, <<<, !!!, ***) for easy grepping

---

## Lesson 5: External Message Queue Deadlock (Session 17)

### The Problem

Game hung after 2 frames. `osRecvMesg()` waited for a message that never came.

### The Cause

- `gfx_thread` sends SP/DP completion via `enqueue_external_message()`
- But `dequeue_external_messages()` was only called at the START of `osRecvMesg()`
- If thread was already in blocking wait, new messages weren't processed

### The Fix

In `lib/N64ModernRuntime/ultramodern/src/mesgqueue.cpp`:

```cpp
while (MQ_IS_EMPTY(mq)) {
    ultramodern::thread_queue_insert(...);
    ultramodern::run_next_thread_and_wait(PASS_RDRAM1);
    // FIX: After waking up, drain external messages
    dequeue_external_messages(PASS_RDRAM1);  // <-- ESSENTIAL
}
```

### The Lesson

**For threading/message queue issues:**
1. Check if messages are correctly sent
2. Check if messages are correctly received
3. Check the order of operations
4. Watch out for external vs internal queues

---

## Lesson 6: Segment 8 Runtime Fix (Session 12)

### The Problem

Display list crashed on segment 8 addresses (0x08XXXXXX).

### The Cause

Game set segment 8 to 0x80316800, but data was at 0x801CAF20.

### The Fix

In `lib/rt64/src/gbi/rt64_gbi_f3d.cpp`:

```cpp
case G_MW_SEGMENT: {
    uint8_t seg_num = (*dl)->p0(10, 4);
    uint32_t seg_val = (*dl)->w1;

    // WAVE RACE FIX: Override segment 8 with correct value
    if (seg_num == 8 && seg_val == 0x80316800) {
        seg_val = 0x801CAF20;
    }

    state->rsp->setSegment(seg_num, seg_val);
    break;
}
```

### The Lesson

**For segment address crashes:**
1. Log which segment is set and with which value
2. Compare with where the data actually is
3. Override if necessary

---

## Lesson 7: Sign Extension for N64 Pointers (Session 26)

### The Problem

Game crashed when calling stubs that return Gfx* pointers.

### The Cause

N64 pointers (e.g., `0x8011F940`) must be sign-extended to 64-bit (`0xFFFFFFFF8011F940`) for the MEM_W macro to work correctly.

```c
// WRONG - not sign-extended
ctx->r2 = gfx_in;  // gfx_in is uint32_t
// Results in ctx->r2 = 0x000000008011F940

// MEM_W calculation becomes:
// 0x8011F940 + 4 - 0xFFFFFFFF80000000 = 0x10011F944  // WRONG! Outside RDRAM
```

### The Fix

```c
// CORRECT: Sign extend the return value
ctx->r2 = (gpr)(int32_t)gfx_in;  // -> 0xFFFFFFFF8011F940
```

### The Lesson

**For all custom stubs returning N64 pointers (0x80xxxxxx):**

```cpp
// WRONG - not sign-extended
ctx->r2 = some_pointer;  // 0x000000008XXXXXXX

// CORRECT - sign-extended
ctx->r2 = (gpr)(int32_t)some_pointer;  // 0xFFFFFFFF8XXXXXXX
```

This is needed because:
1. MIPS N64 is 32-bit, but registers are logically 64-bit with sign extension
2. MEM_W macro expects sign-extended addresses for correct RDRAM offset calculation

---

## Lesson 8: Use the Decomp! (All Sessions)

### What the Decomp is For

The decompilation contains:

| Directory | Contents |
|-----------|----------|
| `src/overlays/` | Decompiled overlay code |
| `src/game/` | Game logic (state machine, etc.) |
| `src/ovl_table.c` | **CRITICAL!** Overlay table with ROM/VRAM addresses |
| `asm/nonmatchings/` | Assembly where decomp doesn't match |
| `include/` | Headers with structures |

### How to Use It

1. **Understand state machine:** `src/game/code_4C750.c` → `func_80092CF0`
2. **Overlay info:** `src/ovl_table.c` → ROM/VRAM addresses
3. **Function code:** `src/overlays/ovl_i0/ovl_1B3EC0.c`
4. **Assembly as backup:** `asm/nonmatchings/...`

### The Lesson

**The decomp is your best friend. ALWAYS use it before guessing!**

---

## Summary: Debugging Workflow

```
1. IDENTIFY PROBLEM
   └── Analyze console output
   └── Where does it crash/hang?

2. CONSULT DECOMP
   └── Find the relevant function/state in decomp
   └── Understand what the real code does
   └── Compare with our implementation

3. FIND ROOT CAUSE
   └── What's the difference?
   └── Are we missing variables?
   └── Are we calling the wrong function?
   └── Is an overlay not loaded?

4. IMPLEMENT FIX
   └── Preference: Let real N64Recomp code run
   └── Alternative: Custom implementation that does EVERYTHING the decomp does
   └── NOT: Quick hack that only fixes symptoms

5. TEST AND DOCUMENT
   └── Build and test
   └── Update SESSION_*.md
   └── Update LESSONS_LEARNED.md if needed
```

---

*This document is updated with new lessons from each session.*
