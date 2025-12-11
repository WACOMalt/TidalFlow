#!/usr/bin/env python3
"""
Extract functions from Wave Race 64's 0x801E overlay segment.
Based on ROM analysis findings:
- ROM: 0x0A95E8 - 0x0C2750
- VRAM: 0x801E0000 - 0x801F9168
- Size: 0x19168 (102,760 bytes)
"""

import struct
from pathlib import Path

# Overlay parameters found via hex analysis
OVERLAY_ROM_START = 0x0A95E8
OVERLAY_ROM_END = 0x0C2750
OVERLAY_VRAM_START = 0x801E0000
OVERLAY_SIZE = OVERLAY_ROM_END - OVERLAY_ROM_START  # 0x19168

def read_rom(rom_path: str) -> bytes:
    with open(rom_path, 'rb') as f:
        return f.read()

def is_function_prologue(word: int) -> bool:
    """Check if instruction is a function prologue (ADDIU SP, SP, -N)"""
    # ADDIU SP, SP, imm where imm is negative (stack allocation)
    if (word & 0xFFFF0000) == 0x27BD0000:
        imm = word & 0xFFFF
        if imm & 0x8000:  # Negative immediate
            return True
    return False

def find_jal_targets(rom_data: bytes, rom_start: int, rom_end: int, vram_start: int) -> set:
    """Find all JAL targets within the overlay"""
    targets = set()
    vram_end = vram_start + (rom_end - rom_start)

    # Scan main code for JAL to this overlay
    # Main code: ROM 0x1000 - 0x8CDB0
    for offset in range(0x1000, 0x8CDB0, 4):
        if offset + 4 > len(rom_data):
            break
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if (word >> 26) == 0x03:  # JAL opcode
            target = (word & 0x03FFFFFF) << 2
            target_vram = 0x80000000 | target
            if vram_start <= target_vram < vram_end:
                targets.add(target_vram)

    # Also scan the overlay itself for internal JALs
    for offset in range(rom_start, rom_end - 4, 4):
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if (word >> 26) == 0x03:  # JAL opcode
            target = (word & 0x03FFFFFF) << 2
            target_vram = 0x80000000 | target
            if vram_start <= target_vram < vram_end:
                targets.add(target_vram)

    return targets

def find_function_prologues(rom_data: bytes, rom_start: int, rom_end: int, vram_start: int) -> set:
    """Find function prologues in the overlay"""
    prologues = set()

    for offset in range(rom_start, rom_end - 4, 4):
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if is_function_prologue(word):
            vram = vram_start + (offset - rom_start)
            prologues.add(vram)

    return prologues

def extract_functions(rom_path: str):
    """Extract function boundaries from the overlay"""
    print(f"Reading ROM: {rom_path}")
    rom_data = read_rom(rom_path)

    print(f"\nOverlay parameters:")
    print(f"  ROM:  0x{OVERLAY_ROM_START:06X} - 0x{OVERLAY_ROM_END:06X}")
    print(f"  VRAM: 0x{OVERLAY_VRAM_START:08X} - 0x{OVERLAY_VRAM_START + OVERLAY_SIZE:08X}")
    print(f"  Size: 0x{OVERLAY_SIZE:X} ({OVERLAY_SIZE} bytes)")

    # Find JAL targets
    jal_targets = find_jal_targets(rom_data, OVERLAY_ROM_START, OVERLAY_ROM_END, OVERLAY_VRAM_START)
    print(f"\nFound {len(jal_targets)} JAL targets")

    # Find function prologues
    prologues = find_function_prologues(rom_data, OVERLAY_ROM_START, OVERLAY_ROM_END, OVERLAY_VRAM_START)
    print(f"Found {len(prologues)} function prologues")

    # Combine: prioritize JAL targets, add prologues that aren't near existing
    all_funcs = jal_targets.copy()
    for vram in prologues:
        if not any(abs(vram - t) < 8 for t in all_funcs):
            all_funcs.add(vram)

    # Always include overlay start
    all_funcs.add(OVERLAY_VRAM_START)

    # Sort and calculate sizes
    sorted_funcs = sorted(all_funcs)
    vram_end = OVERLAY_VRAM_START + OVERLAY_SIZE

    functions = []
    for i, vram in enumerate(sorted_funcs):
        if i + 1 < len(sorted_funcs):
            size = sorted_funcs[i + 1] - vram
        else:
            size = vram_end - vram

        # Align size to 4 bytes
        size = (size + 3) & ~3

        if size > 0:
            functions.append({
                'name': f'ovl_func_{vram:08X}',
                'vram': vram,
                'size': size
            })

    print(f"\nTotal functions: {len(functions)}")

    # Print first 20
    print("\nFirst 20 functions:")
    for f in functions[:20]:
        print(f"  {f['name']}: 0x{f['vram']:08X} (size 0x{f['size']:X})")

    return functions

def generate_toml_section(functions: list) -> str:
    """Generate TOML section for syms.toml"""
    lines = []
    lines.append('[[section]]')
    lines.append('name = ".overlay_801E"')
    lines.append(f'rom = 0x{OVERLAY_ROM_START:08X}')
    lines.append(f'vram = 0x{OVERLAY_VRAM_START:08X}')
    lines.append(f'size = 0x{OVERLAY_SIZE:X}')
    lines.append('')
    lines.append('functions = [')

    for f in functions:
        lines.append(f'    {{ name = "{f["name"]}", vram = 0x{f["vram"]:08X}, size = 0x{f["size"]:X} }},')

    lines.append(']')
    return '\n'.join(lines)

def main():
    rom_path = "waverace-recomp/waverace.us.z64"

    if not Path(rom_path).exists():
        print(f"ROM not found: {rom_path}")
        return

    functions = extract_functions(rom_path)

    # Generate TOML
    toml_section = generate_toml_section(functions)

    # Save to file
    output_path = "overlay_801E_section.toml"
    with open(output_path, 'w') as f:
        f.write(toml_section)
    print(f"\n\nTOML section saved to: {output_path}")
    print("Add this to the END of waverace.syms.toml")

    # Also print it
    print("\n" + "="*60)
    print("TOML SECTION TO ADD:")
    print("="*60)
    print(toml_section)

if __name__ == "__main__":
    main()
