# Codeseg VRAM/ROM Address Fix Analysis

**Date:** December 2025
**Status:** ANALYSIS - Not yet applied

---

## The Problem

The overlay/codeseg section in `waverace.syms.toml` has **INCORRECT** VRAM and ROM addresses, causing N64Recomp to fail on many functions (jump table issues).

---

## Current (WRONG) Configuration

In `waverace-recomp/waverace.syms.toml`:

```toml
[[section]]
name = ".overlay_801E"
rom = 0x000A95E8    # WRONG - 0x18 bytes too late
vram = 0x801E0000   # WRONG - 0x5060 bytes too high
size = 0x19268

functions = [
    { name = "ovl_func_801E0000", vram = 0x801E0000, size = 0x6C },
    # ... 153 functions with wrong addresses
]
```

### Why It's Wrong

The addresses don't match the decomp project (`Wave-Race-64/waverace64.us.yaml`):

```yaml
- name: codeseg
  type: code
  start: 0xA95D0      # ROM offset
  vram: 0x801DAFA0    # VRAM address
```

---

## Correct Configuration (from Decomp)

From `Wave-Race-64/ovl_symbols.txt`, the codeseg should be:

```toml
[[section]]
name = ".codeseg"
rom = 0x000A95D0     # CORRECT - matches decomp
vram = 0x801DAFA0    # CORRECT - matches decomp
size = 0x21634

functions = [
    { name = "ovl_func_801DAFA0", vram = 0x801DAFA0, size = 0x18 },
    { name = "ovl_func_801DAFB8", vram = 0x801DAFB8, size = 0x6C },
    # ... 215 functions with correct addresses
]
```

---

## Comparison

| Parameter | Current (WRONG) | Correct (Decomp) | Difference |
|-----------|-----------------|------------------|------------|
| ROM offset | 0xA95E8 | 0xA95D0 | -0x18 |
| VRAM base | 0x801E0000 | 0x801DAFA0 | -0x5060 |
| Function count | 153 | 215 | +62 missing |
| First function | ovl_func_801E0000 | ovl_func_801DAFA0 | Different |

---

## Impact of Wrong Addresses

1. **Jump tables calculated incorrectly** - N64Recomp fails with "Failed to determine size of jump table"
2. **Missing functions** - 62 functions between 0x801DAFA0 and 0x801E0000 are not included
3. **Key display function affected** - `func_801ECAF4` (display list builder) has wrong VRAM

---

## Key Function: Display List Builder

The main display function `func_80092CF0` calls `ovl_func_801ECAF4`:

**With wrong VRAM (0x801E0000 base):**
- Function would be at: 0x801ECAF4 (same, but N64Recomp calculates jump tables wrong)

**With correct VRAM (0x801DAFA0 base):**
- Function is correctly at: 0x801ECAF4
- Jump tables in rodata align correctly

---

## Why Previous Config Was Wrong

Looking at the current config:
- First function: `ovl_func_801E0000` at VRAM 0x801E0000
- But decomp shows first function is `func_801DAFA0` at VRAM 0x801DAFA0
- The 0x5060 byte gap (0x801E0000 - 0x801DAFA0) was missing entirely!

---

## What Needs to Change

### 1. Update waverace.syms.toml

Replace the `[[section]]` for `.overlay_801E` with the correct `.codeseg` section:

- Change `rom` from `0x000A95E8` to `0x000A95D0`
- Change `vram` from `0x801E0000` to `0x801DAFA0`
- Replace all 153 functions with the correct 215 functions from decomp

### 2. Update waverace.toml stubs

Some functions that were stubbed due to recompilation errors may now work with correct addresses:
- `ovl_func_801E2530`
- `ovl_func_801E229C`
- `ovl_func_801E2B8C`
- `ovl_func_801E2C14`
- `ovl_func_801E3250`
- `ovl_func_801E4C08`

### 3. Update waverace.overlays.txt

May need to change `.overlay_801E` to `.codeseg` if used.

---

## Source Files

### Decomp Reference
```
C:\Users\User\Documents\decompilations\wave-race-64-n64-claude-code-opus-45\Wave-Race-64\
├── waverace64.us.yaml      # ROM segment mapping (correct addresses)
└── ovl_symbols.txt         # All 215+ codeseg function addresses
```

### Recomp Project
```
C:\Users\User\Documents\recompilations\wave-race-64-recomp-claude-code-opus45\waverace-recomp\
├── waverace.syms.toml      # NEEDS FIXING
├── waverace.toml           # Stubs may need updating
└── waverace.overlays.txt   # May need updating
```

---

## Generated Correct Symbols

A new symbols file has been generated at:
```
waverace-recomp/codeseg_symbols_new.toml
```

This contains the correct 215 functions from the decomp with proper VRAM addresses.

---

## Next Steps (When Ready to Apply)

1. Backup current `waverace.syms.toml`
2. Replace overlay section with content from `codeseg_symbols_new.toml`
3. Update `waverace.overlays.txt` to use `.codeseg`
4. Remove now-unnecessary stubs from `waverace.toml`
5. Re-run N64Recomp
6. Rebuild and test

---

## Expected Result

With correct addresses:
- Jump tables should resolve correctly
- All 215 codeseg functions should recompile
- `ovl_func_801ECAF4` (display list builder) should work
- Nintendo logo and real graphics should appear

---

*This analysis is based on comparing the decomp project with the recomp configuration.*
