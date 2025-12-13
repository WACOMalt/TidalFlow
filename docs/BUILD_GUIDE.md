# Build Guide

This guide explains how to build Wave Race 64 Recompiled on Windows (using WSL2) or Linux.

---

## Prerequisites

### Windows (WSL2)

1. **Install WSL2** with Ubuntu:
   ```powershell
   wsl --install -d Ubuntu
   ```

2. **Install build tools** (in WSL):
   ```bash
   sudo apt update
   sudo apt install build-essential cmake ninja-build clang lld
   sudo apt install libsdl2-dev libgtk-3-dev
   ```

### Linux (Native)

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build clang lld
sudo apt install libsdl2-dev libgtk-3-dev
```

---

## Getting the Code

```bash
# Clone with submodules
git clone --recursive https://github.com/chrisking1981/Wave-Race-64-recomp.git
cd Wave-Race-64-recomp
```

---

## ROM Setup

Place your legally obtained Wave Race 64 ROM in the project:

```bash
cp /path/to/waverace.us.z64 waverace-recomp/
```

**Required ROM:**
- Region: NTSC-U (USA)
- Format: Big-endian (.z64)
- SHA1: `C4E3D6B9E15C5CD1A7CE32C5B45E2EDA78F65D16`

---

## Building N64Recomp Tool

First, build the N64Recomp tool:

```bash
cd N64Recomp
cmake -B build -G Ninja
cmake --build build -j$(nproc)
cd ..
```

---

## Running N64Recomp

Generate the recompiled C code:

```bash
cd waverace-recomp
../N64Recomp/build/N64Recomp waverace.toml
```

This creates `RecompiledFuncs/` with generated C files.

**When to re-run N64Recomp:**
- After modifying `waverace.toml`
- After modifying `waverace.syms.toml`
- After adding new overlays or functions

---

## Building the Game

### Full Build

```bash
cd waverace-recomp
cmake -B build -G Ninja
cmake --build build -j$(nproc)
```

### Incremental Build

After code changes:
```bash
cmake --build build -j$(nproc)
```

### Clean Rebuild

If you have strange issues:
```bash
rm -rf build
cmake -B build -G Ninja
cmake --build build -j$(nproc)
```

---

## Running the Game

### Basic Run

```bash
./build/WaveRace64Recompiled
```

### With Debug Output

```bash
./build/WaveRace64Recompiled 2>&1 | head -500
```

### With Timeout (for testing)

```bash
timeout 30 ./build/WaveRace64Recompiled 2>&1 | grep -E '(STATE|FRAME|ERROR)'
```

### Software Rendering (WSL2)

If you have GPU issues in WSL2:
```bash
LIBGL_ALWAYS_SOFTWARE=1 ./build/WaveRace64Recompiled
```

---

## Windows Commands (PowerShell)

If running from Windows PowerShell, use the WSL wrapper:

```powershell
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp/waverace-recomp && cmake --build build -j 24"

# Run
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp/waverace-recomp && ./build/WaveRace64Recompiled"

# Run with timeout
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp/waverace-recomp && timeout 30 ./build/WaveRace64Recompiled 2>&1 | head -200"
```

---

## Common Build Errors

### "Function not defined"

Add the missing function to `waverace.syms.toml`:
```toml
{ name = "func_800XXXXX", vram = 0x800XXXXX }
```

### "Failed to determine size of jump table"

The VRAM address for a section is incorrect. Check the decompilation for the correct overlay base address.

### CMake errors

Try a clean rebuild:
```bash
rm -rf build
cmake -B build -G Ninja
cmake --build build -j$(nproc)
```

### Linker errors (undefined symbol)

Usually means a function is in `stubs` but shouldn't be, or is in `ignored` but has no implementation. Check `waverace.toml`.

---

## Project Configuration Files

### waverace.toml

Main configuration for N64Recomp:
- `stubs` - Functions that can't be translated (hardware access)
- `ignored` - Functions with custom implementations

### waverace.syms.toml

Symbol definitions:
- Function names and addresses
- Overlay sections with ROM/VRAM addresses
- Data symbols

### waverace_stubs.cpp

Custom C++ implementations for stubbed functions.

---

## Debugging Tips

### Add Debug Output

In `waverace_stubs.cpp`:
```cpp
printf("[DEBUG] Function called: state=%d, value=0x%08X\n", state, value);
fflush(stdout);
```

### Trace Function Calls

Add entry/exit logging:
```cpp
printf(">>> Entering func_XXXXXXXX\n");
// ... function code ...
printf("<<< Leaving func_XXXXXXXX\n");
```

### Check State Variables

Print key game state:
```cpp
printf("Game state: %d, Boot flag: %d\n",
       MEM_W(0, 0x800DAB24),
       MEM_W(0, 0x801CE63C));
```

---

## Next Steps

After building successfully:
1. Check [Lessons Learned](LESSONS_LEARNED.md) for debugging insights
2. Read session logs in `docs/sessions/` to understand the project history
3. Try to fix remaining issues or implement missing features

Happy building!
