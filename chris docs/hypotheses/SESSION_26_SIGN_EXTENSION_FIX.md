# Session 26: Sign Extension Fix for Gfx* Returns

> **Lees eerst `chris docs/prompt.md` voor project context en build instructies!**

**Datum:** December 2025
**Focus:** Fix crash in func_i0_802C5800 - Sign extension for N64 pointers

---

## Samenvatting

De crash in state 2 frame 4 is opgelost! Het probleem was dat gestubde functies hun Gfx* return values niet correct sign-extended naar 64-bit.

---

## Root Cause

### Het Probleem

N64 pointers (bijv. `0x8011F940`) moeten sign-extended worden naar 64-bit (`0xFFFFFFFF8011F940`) voor de MEM_W macro om correct te werken.

De MEM_W macro in recomp.h:
```c
#define MEM_W(offset, reg) \
    (*(int32_t*)(rdram + ((((reg) + (offset))) - 0xFFFFFFFF80000000)))
```

### Wat er fout ging

Onze stubs deden:
```c
ctx->r2 = gfx_in;  // gfx_in is uint32_t
```

Dit resulteerde in `ctx->r2 = 0x000000008011F940` (niet sign-extended).

De MEM_W berekening werd dan:
```
0x8011F940 + 4 - 0xFFFFFFFF80000000 = 0x10011F944  // FOUT! Buiten RDRAM
```

### De Fix

```c
// CORRECT: Sign extend de return value
ctx->r2 = (gpr)(int32_t)gfx_in;  // -> 0xFFFFFFFF8011F940
```

---

## Gewijzigde Bestanden

| Bestand | Wijziging |
|---------|-----------|
| `waverace.toml` | Added `func_8008FB74` and `func_8009328C` to ignored list |
| `waverace_stubs.cpp` | Added stubs with proper sign extension |

### Nieuwe Stubs

```cpp
// func_8008FB74 - Main render DL builder
extern "C" void func_8008FB74(uint8_t* rdram, recomp_context* ctx) {
    uint32_t gfx_in = (uint32_t)ctx->r4;
    ctx->r2 = (gpr)(int32_t)gfx_in;  // SIGN EXTENDED!
}

// func_8009328C - Main gameplay DL chain
extern "C" void func_8009328C(uint8_t* rdram, recomp_context* ctx) {
    uint32_t gfx_in = (uint32_t)ctx->r4;
    ctx->r2 = (gpr)(int32_t)gfx_in;  // SIGN EXTENDED!
}
```

---

## Resultaat

**VOOR fix:**
- Frame 1: OK
- Frame 2: CRASH in func_i0_802C5800

**NA fix:**
- 549+ frames stabiel, geen crashes!
- Workaround (skip after frame 4) verwijderd
- Echte overlay code draait

---

## State Flow Update

```
STATE 5  -->  STATE 6  -->  STATE 2 (ovl_i0)
(Boot 1)      (Logo)        (Menu intro)
WORKING       WORKING       WORKING! (was crashing)
```

---

## Belangrijke Les

**Voor alle custom stubs die N64 pointers (0x80xxxxxx) returnen:**

```cpp
// FOUT - niet sign-extended
ctx->r2 = some_pointer;  // 0x000000008XXXXXXX

// CORRECT - sign-extended
ctx->r2 = (gpr)(int32_t)some_pointer;  // 0xFFFFFFFF8XXXXXXX
```

Dit is nodig omdat:
1. MIPS N64 is 32-bit, maar registers zijn logisch 64-bit met sign extension
2. MEM_W macro verwacht sign-extended adressen voor correcte RDRAM offset berekening

---

## Build & Test Commands

```bash
# Build
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && cmake --build build -j 24"

# Test
wsl bash -c "cd /mnt/c/Users/User/Documents/recompilations/wave-race-64-recomp-claude-code-opus45/waverace-recomp && LIBGL_ALWAYS_SOFTWARE=1 timeout 30 ./build/WaveRace64Recompiled 2>&1 | grep -E '(STATE 2|FRAME)'"
```

---

*Session 26 - Sign Extension Fix - ovl_i0 crash resolved!*
