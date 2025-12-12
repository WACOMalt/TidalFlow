# Session 15: ovl_i0 Overlay Stubs & Documentation Update

**Date:** December 2025
**Status:** COMPLETE - Build succeeds, game runs

---

## Build Instructions (BELANGRIJK!)

Dit project wordt gebouwd via **WSL** (Windows Subsystem for Linux):

```bash
# Navigeer naar project
cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp

# Als je waverace.toml of waverace.syms.toml wijzigt, eerst N64Recomp draaien:
../N64Recomp/build/N64Recomp waverace.toml

# Build
cmake --build build -j$(nproc)

# Test
timeout 20 ./build/WaveRace64Recompiled 2>&1 | head -100
```

---

## Summary

Added ovl_i0 overlay stubs for state 2 progression and updated documentation with critical N64Recomp configuration information.

---

## Key Lesson: stubs vs ignored

**Dit was de belangrijkste les van deze sessie!**

| Configuratie | Wat N64Recomp doet | Wanneer gebruiken |
|--------------|-------------------|-------------------|
| `stubs = ["func"]` | Genereert LEGE stub functie | Hardware/MMIO functies die niet vertaald kunnen worden |
| `ignored = ["func"]` | Genereert NIETS | Functies waar JIJ custom code voor schrijft in waverace_stubs.cpp |

**FOUT:** Functie in `stubs` zetten EN custom implementatie in waverace_stubs.cpp → "multiple definition" error!

**CORRECT:**
- Als je custom implementatie wilt → functie in `ignored` zetten
- Als N64Recomp lege stub moet genereren → functie in `stubs` zetten

---

## Changes Made

### 1. waverace.toml - ovl_i0 functies naar ignored

```toml
# Functies verplaatst van stubs naar ignored
ignored = [
    "func_800C7020",
    "func_80046BF4",
    # ovl_i0 overlay functions - custom stubs in waverace_stubs.cpp
    "func_i0_802C5800",
    "func_i0_802C5A7C",
    "func_i0_802C6044",
    "func_i0_802C63AC",
    "func_i0_802C6878",
    "func_i0_802C6944",
    "func_i0_802C6A1C",
    "func_i0_802C6AE4",
]
```

### 2. waverace_stubs.cpp - ovl_i0 stubs toegevoegd

```cpp
extern "C" void func_i0_802C5800(uint8_t* rdram, recomp_context* ctx) {
    static int call_count = 0;
    call_count++;
    if (call_count <= 5 || call_count % 60 == 0) {
        printf("[OVL_I0] func_i0_802C5800 STUB call #%d\n", call_count);
    }
    ctx->r2 = ctx->r4;  // Return DL pointer unchanged
}

// + stubs voor func_i0_802C5A7C t/m func_i0_802C6AE4
```

### 3. prompt.md - Documentatie geüpdatet

Toegevoegd:
- Verwijzing naar `chris docs/N64RECOMP_GUIDE.md`
- Decomp locatie: `C:\Users\User\Documents\decompilations\wave-race-64-n64-claude-code-opus-45\Wave-Race-64\`
- WSL build instructies
- N64Recomp configuratie sectie (stubs vs ignored)
- Static variabele warning

---

## Files Modified

| File | Change |
|------|--------|
| waverace.toml | Moved ovl_i0 functions from stubs to ignored |
| waverace_stubs.cpp | Added ovl_i0 function stubs, removed static var definitions |
| chris docs/prompt.md | Added N64RECOMP_GUIDE reference, decomp path, build instructions, stubs/ignored explanation |

---

## Current Status

- Game compiles and runs successfully
- State 5 → 6 auto-advance works (Nintendo logo displays)
- State 6 → 2 auto-advance after 180 frames
- ovl_i0 stubs called but don't do anything yet

---

## Next Steps

1. Implement actual ovl_i0 display list building (memory card screen)
2. Add state 2 → 3 transition
3. Implement ovl_i1 for title screen (state 7)

---

## Reference: Overlay Table from Decomp

De decomp bevat `ovl_table.c` met alle overlay informatie:

| Overlay | ROM Start | ROM End | VRAM | State |
|---------|-----------|---------|------|-------|
| ovl_i0 | 0x1B3EC0 | 0x1B55A0 | 0x802C5800 | 2 |
| ovl_i1 | 0x1B55A0 | 0x1B80A0 | 0x802C5800 | 7 |
| ... | ... | ... | ... | ... |

---

*Session 15 - ovl_i0 Overlay Stubs & Documentation Update*
