# N64Recomp Configuration Guide

**For LLMs and Developers | December 2025**

---

## Quick Reference

```
N64Recomp = Tool that translates MIPS binary → C code
Recomp Project = Game-specific implementation using N64Recomp
N64ModernRuntime = Runtime library for OS emulation + graphics
```

---

## 1. What is N64Recomp?

N64Recomp is a **static recompiler** that translates Nintendo 64 MIPS binary code into C code that runs on modern systems.

### Difference from Emulation

| Aspect | Emulation | N64Recomp |
|--------|-----------|-----------|
| **Method** | Runtime interpretation | Compile-time translation |
| **Performance** | CPU overhead | Native speed |
| **Modifiability** | Difficult | C code is editable |
| **Output** | None | Readable C functions |

### Core Principle

```
ROM (MIPS binary)
    ↓
N64Recomp tool + configuration
    ↓
C code (funcs_*.c files)
    ↓
Compiler (clang/gcc)
    ↓
Native executable + runtime
```

---

## 2. Project Structure

### Minimal Directory Layout

```
game-recomp/
├── game.toml              # Main configuration for N64Recomp
├── game.syms.toml         # Function definitions (addresses + sizes)
├── game.overlays.txt      # Overlay sections (can be empty)
├── game.us.z64            # ROM file (user provided)
│
├── RecompiledFuncs/       # OUTPUT: generated C code
│   ├── funcs.h            # Header with all declarations
│   ├── funcs_0.c          # Recompiled functions (split)
│   ├── funcs_1.c
│   └── ...
│
├── src/
│   └── game/
│       └── stubs.cpp      # Custom stub implementations
│
├── lib/
│   └── N64ModernRuntime/  # Runtime (submodule)
│
└── CMakeLists.txt         # Build configuration
```

---

## 3. Configuration Files

### 3.1 Main Configuration (game.toml)

```toml
[input]
# VRAM address where game starts (from ROM header or decomp)
entrypoint = 0x80046800

# Path to symbols file
symbols_file_path = "./game.syms.toml"

# ROM file (relative path)
rom_file_path = "game.us.z64"

# Output directory for generated C code
output_func_path = "RecompiledFuncs"

# Use absolute symbol names (recommended: true)
use_absolute_symbols = true

# Overlay configuration file (can be empty file)
relocatable_sections_path = "game.overlays.txt"

[patches]
# Functions that CANNOT be recompiled
# Get empty stub generated
stubs = [
    # MMIO/Hardware access
    "__osSpGetStatus",
    "__osSpSetStatus",
    "__osDpGetStatus",

    # Cache/COP0 instructions
    "osWritebackDCache",
    "osInvalICache",

    # Complex control flow
    "func_with_jump_table_problem",
]

# Functions to completely ignore (no output)
ignored = [
    "func_that_causes_crash",
]
```

### 3.2 Symbol Definitions (game.syms.toml)

This is the MOST IMPORTANT file - defines where all functions are located.

```toml
# Section definition
[[section]]
name = ".text"           # Section name (arbitrary but descriptive)
rom = 0x00001000         # Offset in ROM file (HEX)
vram = 0x80046800        # N64 VRAM address where code loads
size = 0x8BDB0           # Total size of section

# Function list within section
functions = [
    { name = "entrypoint", vram = 0x80046800, size = 0x50 },
    { name = "func_80046850", vram = 0x80046850, size = 0x90 },
    { name = "osSendMesg", vram = 0x800C57A0, size = 0x150 },
    # ... more functions
]

# Overlay section (dynamically loaded code)
[[section]]
name = ".overlay_menu"
rom = 0x000A95D0         # ROM position
vram = 0x801DAFA0        # VRAM load address
size = 0x19268

functions = [
    { name = "ovl_func_801DAFA0", vram = 0x801DAFA0, size = 0x18 },
    # ... overlay functions
]
```

### Critical Points for syms.toml:

1. **VRAM must be EXACT** - Wrong address = wrong jump tables
2. **Size must be correct** - Too small = code gets cut off
3. **ROM offset** - Start of code in ROM file
4. **Functions must NOT overlap**

---

## 4. stubs vs ignored

**This is THE most important lesson for this project!**

| Configuration | What it does | When to use |
|---------------|--------------|-------------|
| `stubs = ["func"]` | N64Recomp generates EMPTY stub | Hardware/MMIO/COP0 functions that can't be translated |
| `ignored = ["func"]` | N64Recomp generates NOTHING | Functions for which YOU write custom implementation in `stubs.cpp` |

### Common Mistake

```toml
# WRONG - function in BOTH lists
stubs = ["func_X"]
ignored = ["func_X"]  # WRONG! Choose ONE!
```

### Correct Usage

```toml
# Functions that N64Recomp CAN'T translate (MMIO, cache, etc.)
# N64Recomp generates empty stub
stubs = [
    "__osSpGetStatus",  # RSP register access
    "osWritebackDCache", # Cache instructions
]

# Functions for which WE write custom code in stubs.cpp
# N64Recomp generates NOTHING - we provide the implementation
ignored = [
    "func_800C7020",  # Custom timer fix
    "func_i0_802C5800",  # Custom overlay stub
]
```

---

## 5. Functions That Must Be Stubbed

Some functions CANNOT be translated to C:

### 5.1 Hardware/MMIO Access

```toml
stubs = [
    # SP (Signal Processor) registers
    "__osSpGetStatus",
    "__osSpSetStatus",
    "__osSpSetPc",
    "__osSpDeviceBusy",

    # DP (Display Processor) registers
    "__osDpGetStatus",
    "__osDpSetStatus",

    # SI (Serial Interface)
    "__osSiGetStatus",
    "__osSiRawStartDma",

    # PI (Parallel Interface)
    "__osPiGetStatus",
    "__osPiRawStartDma",
]
```

### 5.2 Cache/COP0 Instructions

```toml
stubs = [
    "osWritebackDCache",
    "osWritebackDCacheAll",
    "osInvalDCache",
    "osInvalICache",
    "__osSetCompare",
    "__osGetCause",
    "__osSetSR",
]
```

---

## 6. How N64Recomp Generates Code

### Input MIPS Assembly

```asm
func_80046850:
    lui     $v1, 0x8015       # Load upper immediate
    addiu   $v1, $v1, 0x194C  # Add offset -> $v1 = 0x8015194C
    lw      $v0, 0x0($v1)     # Load word from address
    jr      $ra               # Return
```

### Output C Code

```c
RECOMP_FUNC void func_80046850(uint8_t* rdram, recomp_context* ctx) {
    // 0x80046850: lui $v1, 0x8015
    ctx->r3 = S32(0x8015 << 16);
    // 0x80046854: addiu $v1, $v1, 0x194C
    ctx->r3 = ADD32(ctx->r3, 0x194C);
    // 0x80046858: lw $v0, 0x0($v1)
    ctx->r2 = MEM_W(ctx->r3, 0x0);
    // Return
}
```

### Important Macros

| Macro | Meaning |
|-------|---------|
| `ctx->r0` - `ctx->r31` | MIPS registers |
| `ctx->f0` - `ctx->f31` | Floating point registers |
| `MEM_W(addr, offset)` | 32-bit memory read |
| `MEM_H(addr, offset)` | 16-bit memory read |
| `MEM_B(addr, offset)` | 8-bit memory read |
| `S32()`, `U32()` | Signed/Unsigned 32-bit |

---

## 7. Critical: Sign Extension for N64 Pointers

**VERY IMPORTANT for custom stubs!**

N64 pointers (e.g., `0x8011F940`) must be sign-extended to 64-bit (`0xFFFFFFFF8011F940`) for the MEM_W macro to work correctly.

### The Problem

```c
// WRONG - not sign-extended
ctx->r2 = gfx_pointer;  // Results in 0x000000008011F940
```

The MEM_W macro then calculates:
```
0x8011F940 + 4 - 0xFFFFFFFF80000000 = 0x10011F944  // WRONG! Outside RDRAM
```

### The Fix

```c
// CORRECT - sign-extended
ctx->r2 = (gpr)(int32_t)gfx_pointer;  // Results in 0xFFFFFFFF8011F940
```

Now MEM_W calculates correctly:
```
0xFFFFFFFF8011F940 + 4 - 0xFFFFFFFF80000000 = 0x11F944  // Correct RDRAM offset
```

---

## 8. Common Problems

### 8.1 "Failed to determine size of jump table"

**Cause:** VRAM address in syms.toml is wrong

**Solution:**
```toml
# WRONG:
vram = 0x801E0000

# CORRECT (check decomp/disassembly):
vram = 0x801DAFA0
```

### 8.2 "Unknown function at 0x801XXXXX"

**Cause:** Function not defined in syms.toml

**Solution:** Add the function:
```toml
{ name = "func_801XXXXX", vram = 0x801XXXXX, size = 0xYY },
```

### 8.3 Game Hangs After 2 Frames

**Cause:** Message queue deadlock (external message queue not drained)

**Solution:** See Runtime section below

---

## 9. Runtime Architecture

### N64ModernRuntime Components

```
N64ModernRuntime/
├── librecomp/          # Core recompilation support
│   ├── recomp.cpp      # Function dispatch
│   ├── overlays.cpp    # Overlay loading
│   └── sp.cpp          # osSpTaskLoad/StartGo
│
├── ultramodern/        # N64 OS emulation
│   ├── events.cpp      # VI/SP/DP events
│   ├── threads.cpp     # Thread scheduling
│   ├── mesgqueue.cpp   # osRecvMesg/osSendMesg
│   └── timer.cpp       # osGetTime etc
│
└── RT64/               # Graphics rendering
```

### External Message Queue Issue

The N64ModernRuntime has a threading architecture with game threads and non-game threads. When a non-game thread (like `gfx_thread`) sends a message via `osSendMesg()`, it goes to an external queue instead of directly to the N64 message queue.

**Deadlock Scenario:**
1. Game thread calls `osRecvMesg()` with `OS_MESG_BLOCK`
2. Queue is empty → thread waits
3. `gfx_thread` sends SP/DP completion via external queue
4. But `dequeue_external_messages()` only called at START of `osRecvMesg()`
5. **DEADLOCK**: Thread waits for message stuck in external queue

**The Fix** (in `mesgqueue.cpp`):
```cpp
while (MQ_IS_EMPTY(mq)) {
    ultramodern::thread_queue_insert(...);
    ultramodern::run_next_thread_and_wait(PASS_RDRAM1);
    // FIX: After waking up, drain external messages
    dequeue_external_messages(PASS_RDRAM1);  // ESSENTIAL
}
```

---

## 10. Tips for LLMs

### When Analyzing Config

1. **Check VRAM addresses** - Compare with decomp/disassembly
2. **Function sizes** - Must not overlap
3. **ROM offsets** - Calculate: `ROM = VRAM - VRAM_base + ROM_base`

### When Debugging

1. **Look at generated C code** - Is control flow logical?
2. **Check function calls** - Calling correct functions?
3. **Memory access** - Correct addresses?

### When Adding Functions

```toml
# Template for new function
{ name = "func_XXXXXXXX", vram = 0xXXXXXXXX, size = 0xYY },

# Calculate size: next_function_vram - this_function_vram
```

---

## 11. Quick Command Reference

```bash
# Recompile MIPS to C
./N64Recomp game.toml

# Build project
cmake --build build/ -j$(nproc)

# Clean rebuild
rm -rf build/ RecompiledFuncs/
./N64Recomp game.toml
cmake -B build -G Ninja && cmake --build build
```

---

*This document is intended as a practical reference for LLMs and developers working with N64Recomp projects.*
