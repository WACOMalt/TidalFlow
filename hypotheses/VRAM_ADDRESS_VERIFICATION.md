# VRAM Address Verification - Definitief Bewijs

**Date:** December 2025
**Status:** VERIFIED - Recomp configuratie is FOUT

---

## Executive Summary

De huidige `waverace.syms.toml` configuratie gebruikt **verkeerde** VRAM en ROM adressen voor de codeseg overlay. Dit is de **root cause** van alle overlay problemen.

---

## De Twee Configuraties

| Parameter | Recomp (FOUT) | Decomp (CORRECT) |
|-----------|---------------|------------------|
| ROM base  | 0xA95E8       | **0xA95D0**      |
| VRAM base | 0x801E0000    | **0x801DAFA0**   |
| ROM end   | (onbekend)    | 0xF6090          |
| Size      | 0x19168       | **0x5CAC0**      |

---

## Bewijs 1: Load_Codeseg Assembler

De functie `Load_Codeseg` (0x80098190) laadt de codeseg naar een specifiek VRAM adres:

```asm
; File: asm/nonmatchings/game/code_52990/Load_Codeseg.s
glabel Load_Codeseg
    lui   $a0, %hi(func_801DAFA0)    ; = 0x801E
    addiu $a0, $a0, %lo(func_801DAFA0)  ; = 0xAFA0 (signed: -0x5060)
    ; Resultaat: $a0 = 0x801DAFA0
```

De assembler code bewijst dat de overlay geladen wordt naar **VRAM 0x801DAFA0**.

---

## Bewijs 2: ROM Verificatie

### func_801ECAF4 (Display List Builder)

Decomp code:
```c
void func_801ECAF4(void) {
    D_801CE634 = D_800DAB24;  // Eerste instructie
    ...
}
```

Expected MIPS:
```
lui   $v0, 0x800E        ; D_800DAB24 upper bits
addiu $v0, $v0, -0x54DC  ; = 0x800DAB24
```

### ROM Verificatie

**Decomp berekening:** VRAM 0x801ECAF4 - 0x801DAFA0 + 0xA95D0 = **0xBB124**
```
000bb124: 3c02 800e 2442 ab24 ...
          ^^^^^^^^^^^^^^^^^^
          lui $v0, 0x800E; addiu $v0, -0x54DC
          = CORRECT! Dit IS func_801ECAF4
```

**Recomp berekening:** VRAM 0x801ECAF4 - 0x801E0000 + 0xA95E8 = **0xB60DC**
```
000b60dc: 3c0e 8023 3c0f 8023 ...
          = FOUT! Dit is een andere functie
```

---

## Bewijs 3: Decomp Symbol File

`symbol_addrs.txt`:
```
codeseg_ROM_START = 0xA95D0; //defined:true
codeseg_ROM_END = 0xF6090; //defined:true
```

`waverace64.us.yaml`:
```yaml
- name: codeseg # This segment is loaded into memory on func_80098190
  type: code
  start: 0xA95D0
  vram: 0x801DAFA0
```

---

## Impact van de Foute Configuratie

Met de foute configuratie:
1. **Functies worden op verkeerde ROM posities gezocht**
   - func_801ECAF4 wordt gelezen van 0xB60DC i.p.v. 0xBB124
   - Dit leest compleet andere machine code

2. **Jump tables kunnen niet gevonden worden**
   - Rodata staat op andere offsets dan verwacht

3. **BSS sectie conflicteert**
   - De recomp BSS sectie (0x801DAFB8) overlapt met codeseg VRAM (0x801DAFA0)

---

## De Correcte Configuratie

```toml
[[section]]
name = ".codeseg"
rom = 0x000A95D0
vram = 0x801DAFA0
size = 0x5CAC0   # 0xF6090 - 0xA95D0
```

---

## Waarom Was Recomp Configuratie Fout?

De recomp configuratie lijkt gegenereerd door een tool die:
1. Een andere methode gebruikte om VRAM te bepalen
2. De eerste functie (func_801DAFA0) miste
3. Startte bij func_801DAFB8 (18 bytes later in ROM)

De 0x801E0000 lijkt een "rounded" VRAM adres te zijn, maar dit is incorrect.

---

## Volgende Stappen

1. **Update waverace.syms.toml** met correcte configuratie
2. **Regenereer symbol list** met decomp's ovl_symbols.txt (218 functies)
3. **Rebuild** en test

---

## Functie Adres Mapping

Om de recomp functies te matchen met decomp, moeten alle VRAM adressen blijven zoals ze zijn in decomp. De functie namen in syms.toml moeten `func_801DAFA0`, `func_801ECAF4`, etc. zijn (NIET `ovl_func_801E...`).

---

*Dit document bewijst definitief dat de root cause een verkeerde VRAM base address is, niet missende functies of BSS issues.*
