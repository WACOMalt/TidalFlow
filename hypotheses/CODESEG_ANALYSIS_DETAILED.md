# Codeseg Address Analysis - Detailed Investigation

**Date:** December 2025
**Analyst:** Claude Code (Opus 4.5)
**Status:** INVESTIGATION COMPLETE - Complex situation discovered

---

## Executive Summary

The situation is more complex than initially thought. There appears to be an **inconsistency between the decomp's symbol file and its ROM mapping**.

---

## Key Findings

### 1. Documentation vs Reality Mismatch

The `PROJECT_STATUS.md` claims the VRAM fix is "FIXED", but the actual `waverace.syms.toml` still contains the OLD values:

| Source | ROM | VRAM |
|--------|-----|------|
| PROJECT_STATUS.md says "Correct" | 0xA95D0 | 0x801DAFA0 |
| Actual waverace.syms.toml | **0xA95E8** | **0x801E0000** |

**The fix was documented but NOT applied.**

---

### 2. Decomp ROM Mapping (waverace64.us.yaml)

```yaml
- name: codeseg
  start: 0xA95D0      # ROM offset
  vram: 0x801DAFA0    # VRAM address
```

### 3. Decomp Symbol File (ovl_symbols.txt)

First function: `func_801DAFA0 = 0x801dafa0`
Display function: `func_801ECAF4 = 0x801ecaf4`

---

## Verification: First Function (func_801DAFA0)

### ROM Bytes at 0xA95D0
```
AFA50004 3C014040 44812000 8C8E0000 03E00008 E5C40050
```

### MIPS Disassembly
```
sw    a1, 4(sp)       # Save arg1
lui   at, 0x4040      # Load 3.0f upper bits
mtc1  at, f4          # Move to FPU
lw    t6, 0(a0)       # Load *arg0
jr    ra              # Return
swc1  f4, 0x50(t6)    # Store 3.0f to (*arg0)+0x50
```

### Decompiled C (from decomp)
```c
void func_801DAFA0(void** arg0, s32 arg1) {
    *(f32*) (((u8*) (*arg0)) + 0x50) = 3.0f;
}
```

**MATCH CONFIRMED: ROM 0xA95D0 = func_801DAFA0**

---

## The Confusing Part: func_801ECAF4

### What the decomp says:
- Symbol: `func_801ECAF4 = 0x801ecaf4`
- If ROM base is 0xA95D0 and VRAM base is 0x801DAFA0:
  - func_801ECAF4 ROM = 0xA95D0 + (0x801ECAF4 - 0x801DAFA0) = **0xBB124**

### What's at ROM 0xBB124:
```
3c02 800e 2442 ab24 8c4e 0000 3c01 801d
```
This is `lui $v0, 0x800E` - **NOT** the expected function start!

### What the current recomp config says:
- ROM base: 0xA95E8, VRAM base: 0x801E0000
- func_801ECAF4 ROM = 0xA95E8 + (0x801ECAF4 - 0x801E0000) = **0xB60DC**

### What's at ROM 0xB60DC:
```
3c0e 8023 3c0f 8023 3c18 8023 3c19 8023
```
This is `lui $t6, 0x8023` - **CORRECT** function start!

---

## The Inconsistency

| Calculation Method | ROM Offset | Contains |
|-------------------|------------|----------|
| Decomp mapping (0x801DAFA0 base) | 0xBB124 | Wrong code |
| Recomp mapping (0x801E0000 base) | 0xB60DC | **Correct code** |

The current recomp config, despite having "wrong" base addresses, actually produces the correct ROM offset for func_801ECAF4!

---

## What This Means

### Possibility 1: Decomp ovl_symbols.txt is offset by 0x5048
The decomp symbol addresses might be 0x5048 bytes too high. The correct address for the display function might be `0x801E7AAC`, not `0x801ECAF4`.

### Possibility 2: The codeseg is loaded at runtime to a different address
The game might load the codeseg to VRAM 0x801E0000, not 0x801DAFA0. The decomp's waverace64.us.yaml might be describing the *ROM layout*, not the *runtime VRAM*.

### Possibility 3: Two different segments
There might be two overlapping segments, and we're conflating them.

---

## Evidence Supporting Possibility 2

From decomp yaml comment:
```yaml
- name: codeseg # This segment is loaded into memory on func_80098190
```

The codeseg is **dynamically loaded** by `func_80098190`. The load address might differ from the yaml's declared vram.

Let me check `func_80098190` to see where it loads the segment...

---

## Current Recomp Sections

```
.text:          rom=0x1000,   vram=0x80046800  (main code)
.overlay_801E:  rom=0xA95E8,  vram=0x801E0000  (codeseg - our focus)
.bss_801D:      rom=0xA45A0,  vram=0x801DAFB8  (BSS data)
.overlay_802C:  rom=0x1AC7B0, vram=0x802C0000  (second overlay)
```

Note: `.bss_801D` starts at VRAM 0x801DAFB8, which is very close to where decomp says codeseg should start (0x801DAFA0). There might be an overlap issue.

---

## Rodata/Jump Table Problem

### Current section doesn't cover rodata:
- Section ends at: VRAM 0x801F9268 (0x801E0000 + 0x19268)
- Rodata is at: VRAM 0x80225F10 (calculated from decomp)

This is **outside** the current section bounds, which is why N64Recomp can't find jump tables!

---

## Recommended Investigation Steps

1. **Check func_80098190** - How does it load the codeseg? What VRAM does it use?

2. **Verify more functions** - Check 5-10 more functions to see if the 0x5048 offset is consistent

3. **Check the .bss_801D section** - Is there overlap with codeseg?

4. **Extend section size** - Try extending the section to include rodata

---

## Test Proposals

### Test A: Keep current base, extend size
```toml
[[section]]
name = ".overlay_801E"
rom = 0x000A95E8
vram = 0x801E0000
size = 0x30000  # Extended to include rodata
```

### Test B: Use decomp base addresses
```toml
[[section]]
name = ".codeseg"
rom = 0x000A95D0
vram = 0x801DAFA0
size = 0x60000  # Much larger to include everything
```

### Test C: Hybrid - decomp base with adjusted function addresses
Recalculate all function VRAM addresses by subtracting 0x5048.

---

## Files Involved

### Decomp
- `Wave-Race-64/waverace64.us.yaml` - ROM segment mapping
- `Wave-Race-64/ovl_symbols.txt` - Function addresses (possibly wrong?)
- `Wave-Race-64/src/codeseg/A95D0.c` - Decompiled source

### Recomp
- `waverace-recomp/waverace.syms.toml` - Current (possibly correct?) config
- `waverace-recomp/waverace.toml` - Stubs configuration

---

## Conclusion

The situation is NOT as simple as "wrong VRAM address". The current recomp config might actually be MORE correct than the decomp's yaml suggests, because the function bytes are found at the expected ROM offsets.

The main issue is that the **section size is too small** to include the rodata (jump tables).

**Recommended first test:** Extend section size to 0x60000 and see if N64Recomp can now analyze jump tables.

---

*This analysis reveals the complexity of N64 recompilation when dealing with dynamically loaded segments.*
