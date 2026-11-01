"""List places in boot + 0x41000000 window that load a given 16-bit immediate with addiu/ori rX, zero, imm.

usage: find_insn.py <rom> <imm_hex> [<imm_hex> ...]
"""
import struct
import sys

rom = open(sys.argv[1], "rb").read()
SEGS = [(0x1000, 0x4DC30, 0x40000400), (0x4DC30, 0x4DC30 + 0xBE620, 0x41000000)]
imms = {int(x, 16) for x in sys.argv[2:]}
for s, e, v in SEGS:
    for o in range(s, e, 4):
        w = struct.unpack_from(">I", rom, o)[0]
        if w >> 26 in (0x09, 0x0D) and (w >> 21) & 31 == 0 and (w & 0xFFFF) in imms:
            print(f"{v + o - s:08X}  rt=${(w >> 16) & 31}  imm={w & 0xFFFF:#x}")
