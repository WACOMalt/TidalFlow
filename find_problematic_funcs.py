#!/usr/bin/env python3
"""
Find functions with problematic instructions that N64Recomp can't handle:
- CACHE instructions
- COP0 register access (MFC0/MTC0)
- Other unhandled ops
"""

import struct

def read_rom(rom_path: str) -> bytes:
    with open(rom_path, 'rb') as f:
        return f.read()

def get_func_boundaries(rom_data: bytes, code_start_rom: int, code_end_rom: int, vram_start: int):
    code_size = code_end_rom - code_start_rom
    vram_end = vram_start + code_size

    jal_targets = set()
    offset = code_start_rom
    while offset < code_end_rom - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        if (word >> 26) == 0x03:
            target = (word & 0x03FFFFFF) << 2
            target_vram = (vram_start & 0xF0000000) | target
            if vram_start <= target_vram < vram_end:
                jal_targets.add(target_vram)
        offset += 4

    jal_targets.add(vram_start)
    sorted_funcs = sorted(jal_targets)

    func_boundaries = {}
    for i, vram in enumerate(sorted_funcs):
        if i + 1 < len(sorted_funcs):
            end = sorted_funcs[i + 1]
        else:
            end = vram_end
        func_boundaries[vram] = end

    return func_boundaries

def find_func_for_addr(func_boundaries, addr):
    for func_start, func_end in func_boundaries.items():
        if func_start <= addr < func_end:
            return func_start
    return None

def find_problematic_funcs(rom_data: bytes, code_start_rom: int, code_end_rom: int, vram_start: int):
    func_boundaries = get_func_boundaries(rom_data, code_start_rom, code_end_rom, vram_start)

    problems = {
        'cache': set(),
        'cop0': set(),
        'other': set()
    }

    offset = code_start_rom
    while offset < code_end_rom - 4:
        word = struct.unpack('>I', rom_data[offset:offset+4])[0]
        current_vram = vram_start + (offset - code_start_rom)
        opcode = word >> 26

        func = find_func_for_addr(func_boundaries, current_vram)
        if func is None:
            offset += 4
            continue

        # CACHE instruction (opcode 0x2F)
        if opcode == 0x2F:
            problems['cache'].add(func)

        # COP0 instructions (opcode 0x10)
        elif opcode == 0x10:
            rs = (word >> 21) & 0x1F
            # MFC0 (rs=0) or MTC0 (rs=4) - reading/writing CPU control registers
            if rs == 0 or rs == 4:
                problems['cop0'].add(func)

        # BREAK instruction (opcode 0x00, funct 0x0D)
        elif opcode == 0x00:
            funct = word & 0x3F
            if funct == 0x0D:  # BREAK
                problems['other'].add(func)

        # SYSCALL instruction (opcode 0x00, funct 0x0C)
            if funct == 0x0C:  # SYSCALL
                problems['other'].add(func)

        offset += 4

    return problems

def main():
    rom_path = "rom/baserom.us.z64"
    CODE_START_ROM = 0x1000
    CODE_END_ROM = 0x8CDB0
    VRAM_START = 0x80046800

    rom_data = read_rom(rom_path)
    problems = find_problematic_funcs(rom_data, CODE_START_ROM, CODE_END_ROM, VRAM_START)

    all_stubs = problems['cache'] | problems['cop0'] | problems['other']

    print(f"Functions with CACHE: {len(problems['cache'])}")
    print(f"Functions with COP0: {len(problems['cop0'])}")
    print(f"Functions with other issues: {len(problems['other'])}")
    print(f"Total unique functions to stub: {len(all_stubs)}")

    print("\n# Add to stubs list in waverace.toml:")
    print("stubs = [")
    for func in sorted(all_stubs):
        print(f'    "func_{func:08X}",')
    print("]")

if __name__ == "__main__":
    main()
