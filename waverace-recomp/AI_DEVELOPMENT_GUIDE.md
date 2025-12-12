# Wave Race 64 Recompilation - AI/LLM Development Guide

This document provides comprehensive instructions for AI language models to understand, build, and run this Wave Race 64 native PC recompilation project.

## Project Overview

This is a static recompilation of Wave Race 64 (N64) to native PC using:
- **N64Recomp**: Static recompiler that converts MIPS binaries to C code
- **N64ModernRuntime**: Runtime library providing N64 OS emulation (ultramodern), graphics (RT64), and audio
- **RT64**: Modern graphics renderer using parallel-rdp for accurate N64 graphics

## Prerequisites

### Required Software
- **WSL (Windows Subsystem for Linux)** with Ubuntu 24.04 or similar
- **CMake** (3.20+)
- **GCC/G++** (13.x recommended)
- **SDL2 development libraries**: `sudo apt install libsdl2-dev`
- **GTK3 development libraries**: `sudo apt install libgtk-3-dev`
- **Freetype**: `sudo apt install libfreetype-dev`
- **X11 libraries**: `sudo apt install libx11-dev libxrandr-dev`

### Required Files
- **ROM File**: `waverace.us.z64` (Wave Race 64 USA ROM, 8MB)
  - Must be placed in the project root directory
  - SHA1: (verify with original ROM)

## Directory Structure

```
waverace-recomp/
├── assets/                    # Font files and UI assets
├── build/                     # CMake build directory (generated)
├── lib/
│   ├── N64ModernRuntime/      # Runtime libraries
│   │   ├── librecomp/         # Recompiler runtime (OS function stubs)
│   │   │   └── src/
│   │   │       ├── sp.cpp     # RSP task handling
│   │   │       └── ultra_translation.cpp  # OS function wrappers
│   │   └── ultramodern/       # N64 OS emulation
│   │       └── src/
│   │           ├── events.cpp      # VI/RSP/DP event handling
│   │           ├── mesgqueue.cpp   # Message queue implementation
│   │           └── threads.cpp     # Thread management
│   └── rt64/                  # RT64 graphics renderer
├── RecompiledFuncs/           # Generated C code from recompiler
│   ├── funcs_0.c through funcs_15.c
│   └── lookup.cpp
├── RecompiledPatches/         # Game-specific patches
├── waverace.us.z64            # ROM file (not included, user must provide)
├── waverace.syms.toml         # Symbol definitions
└── waverace.toml              # Recompiler configuration
```

## Building

**IMPORTANT**: This project uses **GCC** with **Unix Makefiles** in WSL - NOT clang/Ninja as suggested in BUILDING.md (that file is a template from MarioKart64Recomp and was not updated).

### Build Configuration (verified working)
- **Compiler**: GCC 13 (`/usr/bin/c++` and `/usr/bin/cc`)
- **Generator**: Unix Makefiles (default cmake generator)
- **Platform**: WSL (Windows Subsystem for Linux) with Ubuntu

### Step 1: Configure with CMake (in WSL)

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp
rm -rf build
mkdir build
cd build
cmake ..
```

### Step 2: Build

```bash
cmake --build . -j$(nproc)
```

This creates `WaveRace64Recompiled` executable in the build directory.

**Note**: The build takes several minutes due to RT64 (graphics renderer) compilation. Be patient!

## Running

**Important**: Run from the project root directory (not build directory) so assets are found:

```bash
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp
./build/WaveRace64Recompiled 2>&1 | tee output.log
```

**IMPORTANT**: After launching, you must click **"Start game"** in the UI to actually start the game! The application will show a menu/launcher first - the game code only runs after you click Start game.

**Auto-start for debugging**: Set `AUTO_START_GAME = true` in `src/ui/ui_launcher.cpp` to automatically start the game after 500ms (useful for debugging iterations).

### Expected Output (Current State)
The game loop is now running and receiving VI messages (0x19). Frame limiter passes and sends ready signal (0x29). RSP task submission is still being debugged.

## Architecture Overview

### N64 OS Emulation (ultramodern)

The N64 uses cooperative multithreading with message queues. Key concepts:

1. **Message Queues**: Communication between threads and hardware events
   - `osCreateMesgQueue()`: Create a queue
   - `osSendMesg()`: Send message (non-blocking or blocking)
   - `osRecvMesg()`: Receive message (non-blocking or blocking)

2. **Events/Interrupts** (translated to messages):
   - `0x19` - VI (Video Interface) retrace - sent every frame (~60Hz)
   - `0x17` - SP (Signal Processor/RSP) task complete
   - `0x16` - DP (Display Processor/RDP) task complete
   - `0x18` - AI (Audio Interface) interrupt

3. **Threads**:
   - Main thread: Game logic
   - VI thread: Handles vertical retrace timing
   - RSP thread: Processes graphics/audio tasks

### Graphics Pipeline

1. Game builds display list (GBI commands)
2. `osSpTaskLoad()` loads RSP task
3. `osSpTaskStartGo()` starts RSP execution
4. RSP processes display list, sends to RDP
5. RDP renders to framebuffer
6. VI displays framebuffer at next retrace

### Key Files for Debugging

| File | Purpose |
|------|---------|
| `lib/N64ModernRuntime/ultramodern/src/events.cpp` | VI/RSP/DP event handling, framebuffer management |
| `lib/N64ModernRuntime/librecomp/src/sp.cpp` | RSP task submission |
| `lib/N64ModernRuntime/ultramodern/src/mesgqueue.cpp` | Message queue implementation |
| `RecompiledFuncs/funcs_0.c` | Contains game_thread_entry (main game loop at 0x80047530) |

## Current Issues Being Debugged

### FIXED: VI Message Queue Not Set
- **SOLVED**: Added `game_vi_swap_called` flag in events.cpp to prevent `set_dummy_vi()` from overwriting game framebuffers
- Game now properly initializes framebuffers (0x803B5000, 0x8038F800)
- `osViSetEvent()` is now called, VI messages (0x19) are being sent
- Game loop runs and receives messages correctly

### Current Issue: Render Thread Blocking on PI DMA Queue

**Symptom**: Render thread (func_80046DA0) starts but blocks before reaching main loop.

**Investigation Process**:
1. Added debug prints to render thread initialization
2. Found it blocks on `osRecvMesg(queue=0x801540B8)`
3. Traced call chain: func_80046DA0 → func_80097EC8 → func_800CA370 → func_800D0560
4. **Root Cause**: `func_800D0560` is EMPTY in recompiled code (line 7092-7095 in funcs_14.c)
5. This function is likely `osEPiStartDma` or similar PI DMA function not identified in symbols

**Solution**: Need to identify func_800D0560 in `waverace.syms.toml`:
- Compare with dino-recomp symbols where `osEPiStartDma = 0x80080F70 (size 0xD4)`
- Wave Race has func_800D0560 with size 0x150
- Add to symbols: `{ name = "osEPiStartDma", vram = 0x800D0560, size = 0x150 }`

### How to Debug Empty/Stub Functions

When game blocks unexpectedly:
1. Add debug prints to trace execution flow
2. Find where `osRecvMesg` blocks
3. Check which queue it's waiting on
4. Find who should send to that queue
5. If a function is empty, it's likely an unidentified libultra function
6. Compare with reference projects to identify it

## Debug Output Format

The codebase has extensive debug prints. Key patterns:

```
[DEBUG-VI] ...          - VI/Video related
[DEBUG-RT] ...          - Runtime OS functions
[GAMELOOP] ...          - Main game loop
[DEBUG] >>> func_...    - Function entry points
```

## Reference Projects

**IMPORTANT**: Use these reference projects to understand how similar issues were solved:

```
C:\Users\User\Documents\recompilations\wave-race-64-recomp-claude-code-opus45\recompiled_games_use_these_for_info_and_reference_dont_makeup_ideas_analyse_these\
├── dino-recomp/           # Dinosaur Planet - EXCELLENT reference for PI/DMA functions
│   └── lib/dino-recomp-decomp-bridge/dino.syms.toml  # Contains all PI function mappings
├── mk64recomp/            # Mario Kart 64
└── zelda64recomp-reference/  # Zelda: Ocarina of Time / Majora's Mask
```

Key things to check in references:
- How osViSetEvent is called
- How osCreateViManager is handled
- VI thread initialization
- **PI DMA functions**: osEPiStartDma, osEPiRawStartDma, osPiStartDma

## Common Commands

```bash
# Full rebuild
cd /mnt/c/Users/.../waverace-recomp
rm -rf build && mkdir build && cd build && cmake .. && cmake --build . -j$(nproc)

# Quick rebuild (after code changes)
cmake --build build -j$(nproc)

# Run with full debug output
./build/WaveRace64Recompiled 2>&1 | tee debug.log

# Search for function in recompiled code
grep -n "func_80047530" RecompiledFuncs/*.c

# Find OS function usage
grep -rn "osViSetEvent" lib/
```

## Symbol File Format (waverace.syms.toml)

```toml
[[func]]
name = "osViSetEvent"
vram = 0x800XXXXX

[[func]]
name = "osSpTaskLoad"
vram = 0x800C615C
```

## Adding Debug Prints

To add debug prints to recompiled functions in `RecompiledFuncs/funcs_*.c`:

```c
RECOMP_FUNC void func_80047530(uint8_t* rdram, recomp_context* ctx) {
    // Add at function start:
    fprintf(stderr, "[DEBUG] >>> func_80047530 called\n");

    // ... existing code ...
}
```

Note: `#include <stdio.h>` is already present in these files.

## Contact / Issues

This is an active development project. Issues should be documented with:
1. Full debug output
2. Steps to reproduce
3. Expected vs actual behavior
