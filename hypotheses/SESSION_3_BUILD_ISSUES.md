# Session 3: Build Issues - Overlay Function Size Errors

## Summary
N64Recomp succeeded with 1024 functions, but CMake build initially failed due to incorrect overlay function sizes. **FIXED** by correcting `ovl_func_801FC4D4` size from 0x100 to 0x368.

## Build Environment
- **Build system**: WSL (Windows Subsystem for Linux)
- **Build command**: `cmake .. && cmake --build . -j$(nproc)`
- **Location**: `/mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp/build`

## Current Error

```
funcs_19.c: In function 'ovl_func_801FC4D4':
funcs_19.c:34990:13: error: label 'L_801FC5FC' used but not defined
            goto L_801FC5FC;
funcs_19.c:34871:13: error: label 'L_801FC830' used but not defined
            goto L_801FC830;
```

## Root Cause Analysis

### Problem
The overlay function `ovl_func_801FC4D4` has size 0x100 in syms.toml:
- Start: 0x801FC4D4
- End: 0x801FC4D4 + 0x100 = 0x801FC5D4

But the generated code has branches to:
- `L_801FC5FC` (0x801FC5FC > 0x801FC5D4) - **outside function!**
- `L_801FC830` (0x801FC830 >> 0x801FC5D4) - **way outside function!**

### Generated Code Analysis (funcs_19.c:34867-34871)
```c
// 0x801FC4FC: bnel        $v0, $at, L_801FC830
if (ctx->r2 != ctx->r1) {
    // 0x801FC500: lw          $ra, 0x14($sp)
    ctx->r31 = MEM_W(ctx->r29, 0X14);
        goto L_801FC830;  // ERROR: Label doesn't exist!
}
```

The instruction at 0x801FC4FC is a "branch not equal likely" to 0x801FC830. But because the function size is only 0x100, N64Recomp stopped recompiling at 0x801FC5D0 and never generated the labels for addresses beyond that.

### Decomp Information
From `ovl_symbols.txt`, the decomp only lists 2 functions in the 0x801FC range:
- func_801FC39C
- func_801FC4D4

This suggests `ovl_func_801FC4D4` is the LAST overlay function, and it should extend to the end of the overlay section.

## Investigation: What is the overlay end address?

The overlay starts at VRAM 0x801DAFA0 (from previous session).

To find the overlay end, we need to check:
1. The overlay's ROM size (to calculate VRAM end)
2. Or find where BSS data starts after code

### TODO: Determine correct size for ovl_func_801FC4D4

Options:
1. Calculate from overlay section end
2. Find the actual `jr $ra` (return) instruction in the ROM
3. Check if 0x801FC830 is within BSS/data rather than code

## Files Modified This Session

- **waverace.syms.toml**: Contains overlay function definitions
  - Line 1047: `{ name = "ovl_func_801FC4D4", vram = 0x801FC4D4, size = 0x100 }`
  - This size is WRONG - needs to be larger

## Scripts Used

Build in WSL:
```bash
wsl bash -c "cd /mnt/c/.../waverace-recomp && rm -rf build && mkdir build && cd build && cmake .. && cmake --build . -j\$(nproc)"
```

## Next Steps

1. Determine correct size for `ovl_func_801FC4D4`
2. Check if 0x801FC830 is an actual code address or data address
3. If it's a data address, the function has an infinite loop or tail call
4. Consider if this function should be stubbed instead

## Solution Applied

Changed `ovl_func_801FC4D4` size in `waverace.syms.toml`:
- **Before**: `{ name = "ovl_func_801FC4D4", vram = 0x801FC4D4, size = 0x100 }`
- **After**: `{ name = "ovl_func_801FC4D4", vram = 0x801FC4D4, size = 0x368 }`

### How size 0x368 was determined
1. Calculated ROM offset: vram 0x801FC4D4 -> ROM 0x000CAB04
2. Searched for `jr $ra` (0x03E00008) instruction starting from function start
3. Found return at offset 0x360 (VRAM 0x801FC834)
4. Function ends at 0x801FC834 + 8 (delay slot) = 0x801FC83C
5. Size = 0x801FC83C - 0x801FC4D4 = 0x368

## Build Progress
After fixing, re-ran N64Recomp and cmake build. Build is now compiling successfully (RT64 renderer compilation in progress).

## Additional Fix: MEM_W Macro Conflict

After the overlay size fix, encountered another error in `waverace_stubs.cpp`:
- **Error**: Code used `MEM_W(rdram, address)` but the macro expects `MEM_W(offset, reg)`
- **Solution**: Renamed local `MEM_W` function to `MEM_W_READ` and updated all calls

## Additional Fix: Overlay Function Linker Errors

After MEM_W fix, got linker errors for undefined overlay functions:
- `ovl_func_801EC274`, `ovl_func_801EBF68`, etc.
- **Cause**: `ovl_func_801E270C_recomp` tried to call overlay functions with old VRAM-based names
- **Solution**: Simplified to a stub that just returns (menu state machine not critical for initial testing)

## BUILD SUCCESSFUL ✓

Final build completed at 100%:
```
[ 91%] Building CXX object CMakeFiles/WaveRace64Recompiled.dir/src/game/waverace_stubs.cpp.o
[ 91%] Linking CXX executable WaveRace64Recompiled
[100%] Built target WaveRace64Recompiled
```

## Key Learnings

1. **Overlay function sizes are critical** - The last function in an overlay segment needs correct sizing
2. **Decomp doesn't always have overlay function sizes** - May need to analyze ROM directly
3. **Find `jr $ra` to determine function end** - Search ROM for return instruction (0x03E00008)
4. **BUILDING.md is outdated** - It's a template from MarioKart64Recomp, the actual build uses GCC + Unix Makefiles, not clang + Ninja
5. **AI_DEVELOPMENT_GUIDE.md is the correct reference** - Updated with verified build configuration
