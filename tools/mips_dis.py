"""Tiny MIPS disassembler via capstone for quick looks at raw ROM ranges.

usage: mips_dis.py <rom> <rom_off_hex> <count> [vaddr_hex]
"""
import struct
import sys

from capstone import CS_ARCH_MIPS, CS_MODE_BIG_ENDIAN, CS_MODE_MIPS64, Cs

rom = open(sys.argv[1], "rb").read()
off = int(sys.argv[2], 16)
cnt = int(sys.argv[3])
va = int(sys.argv[4], 16) if len(sys.argv) > 4 else off - 0x1000 + 0x80000400
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS64 + CS_MODE_BIG_ENDIAN)
for i in range(cnt):
    w = rom[off + i * 4: off + i * 4 + 4]
    insns = list(md.disasm(w, va + i * 4))
    txt = f"{insns[0].mnemonic} {insns[0].op_str}" if insns else f".word {struct.unpack('>I', w)[0]:08x}"
    print(f"{va + i * 4:08X}  {w.hex()}  {txt}")
