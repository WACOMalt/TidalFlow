# Session 9: Segment 8 Fix Attempt

**Datum:** December 2025
**Status:** IN PROGRESS - Crash still occurring, but DL format now correct

---

## Summary

In deze sessie hebben we:
1. De exacte oorzaak van de RT64 crash gevonden (segment 8 niet geïnitialiseerd)
2. Een fix geïmplementeerd die gSPSegment(8, base) injecteert in de master display list
3. Het correcte F3D G_MOVEWORD formaat uitgevonden en geïmplementeerd
4. Nog steeds een crash, maar de display list ziet er nu correct uit

---

## Key Discoveries

### 1. Segment 8, niet Segment 6!

De vorige sessie dacht dat segment 6 het probleem was, maar na analyse van de display list dump:

```
[  0] 06000000 08066180  (G_DL) -> segmented addr 0x08066180
```

Het segmented address `0x08066180` betekent:
- High byte `0x08` = segment nummer 8 (niet 6!)
- Lower 3 bytes `0x066180` = offset binnen segment

### 2. Display List Structuur

Wave Race heeft een twee-level DL structuur:
1. **Master DL** at `0x801388D0` - Dit is wat RT64 direct verwerkt
2. **Overlay DL** at `0x8011F940` - Gegenereerd door de overlay code

Het probleem was dat:
- De master DL leeg was (alleen NOOPs + G_ENDDL)
- Segment 8 nergens gezet werd
- De overlay DL segment 8 adressen gebruikte maar nooit aangeroepen werd

### 3. DMA en Segment Base Calculation

Van de DMA logs:
```
rdram=0x802310A0, phys=0x100FE320, size=0x678E0  <- Main assets
```

De overlay DL gebruikt offset `0x66180` in segment 8.
Als segment 8 wijst naar de juiste base, dan:
- `base + 0x66180 = 0x802310A0` (asset locatie)
- Dus `base = 0x802310A0 - 0x66180 = 0x801CAF20`

### 4. F3D G_MOVEWORD Formaat

Het correcte formaat voor `gSPSegment(8, base)` in F3D:
```
w0 = (0xBC << 24) | (offset << 8) | (index)
w0 = (0xBC << 24) | (0x20 << 8) | 0x06
w0 = 0xBC002006

w1 = base_address = 0x801CAF20
```

RT64 extraheert het segment nummer als: `p0(10, 4) = (w0 >> 10) & 0xF`
- `0xBC002006 >> 10 = 0x002F0008`
- `0x002F0008 & 0xF = 8` ✓

---

## Implementation

### Modified File: `lib/N64ModernRuntime/librecomp/src/sp.cpp`

```cpp
// fix_display_list() now injects gSPSegment command at start of master DL

uint32_t segment_8_base = 0x801CAF20;  // Calculated: 0x802310A0 - 0x66180

uint8_t* bytes = (uint8_t*)dl;
// w0 = 0xBC002006 in big-endian
bytes[0] = 0xBC;  // G_MOVEWORD opcode
bytes[1] = 0x00;  // offset high byte
bytes[2] = 0x20;  // offset low byte (segment 8 * 4 = 32)
bytes[3] = 0x06;  // G_MW_SEGMENT index
bytes[4] = (segment_8_base >> 24) & 0xFF;
bytes[5] = (segment_8_base >> 16) & 0xFF;
bytes[6] = (segment_8_base >> 8) & 0xFF;
bytes[7] = segment_8_base & 0xFF;

// Then add G_ENDDL
bytes[8]  = 0xB8;  // G_ENDDL opcode
...
```

### Current Display List Output

```
[SEQ-0] DL dump (first 12 cmds at 0x801388D0):
  [+0x00] BC002006 801CAF20  <- gSPSegment(8, 0x801CAF20) ✓
  [+0x08] B8000000 00000000  <- G_ENDDL ✓
```

Dit ziet er correct uit! Maar RT64 crasht nog steeds tijdens `processDisplayLists()`.

---

## Remaining Issues

### 1. Crash Still Occurring

Ondanks de correcte DL formaat, crasht RT64 nog steeds. Mogelijke oorzaken:
- Microcode detectie probleem
- RT64 interne state issue
- Iets anders in de GBI processing

### 2. Timing Issue with Overlay DL

De overlay DL wordt pas gevuld NADAT de eerste task is gesubmit. Daarom:
- Frame 1: Master DL heeft alleen segment setup + ENDDL (geen G_DL call)
- Frame 2+: Zou segment setup + G_DL(overlay) + ENDDL moeten hebben

Maar we komen niet eens voorbij frame 1 vanwege de crash.

---

## Test Commands

```bash
# Build
cd waverace-recomp
wsl bash -c "cmake --build build -j 24"

# Run with debug
wsl bash -c "timeout 15 ./build/WaveRace64Recompiled 2>&1 | grep -E '(DL-FIX|DL dump)'"
```

---

## Next Steps

### Priority 1: Debug RT64 Crash
- Add more debug output to RT64's processDisplayLists
- Check microcode detection
- Verify GBI handler for G_MOVEWORD is being called

### Priority 2: Verify Segment Base Address
- The calculation assumes offset 0x66180 maps to 0x802310A0
- May need to verify this with actual asset data analysis

### Priority 3: Implement G_DL Call for Frame 2+
- Once frame 1 works, add logic to call overlay DL on subsequent frames

---

## Files Modified This Session

| File | Change |
|------|--------|
| lib/N64ModernRuntime/librecomp/src/sp.cpp | Inject gSPSegment(8) command with correct F3D format |
| chris docs/prompt.md | Added AI debug output tips |
| chris docs/hypotheses/SESSION_9_SEGMENT_FIX.md | This documentation |

---

## Key Learnings

1. **Segment numbers are in the HIGH byte** of segmented addresses (0x08XXXXXX = segment 8)
2. **F3D G_MOVEWORD format** is `(opcode << 24) | (offset << 8) | (index)`, not `(opcode << 24) | (index << 16) | (offset)`
3. **RT64 extraction** for segment: `(w0 >> 10) & 0xF`
4. **Two-level DL structure** means we need to both set up segments AND call sub-DLs
5. **Timing is critical** - overlay DL is written after task submission

---

*Session 9 - Segment 8 Fix Investigation*
