"""Estimate which display-list command bytes the CPU code builds.

Display-list commands are 64-bit words whose first byte is the opcode; game code typically builds them with
`lui rX, 0xCCnn` (CC = opcode) followed by a store. This counts `lui` immediates whose register is stored with
sw within the next few instructions, grouped by the top byte, for both code segments.
usage: dl_opcodes.py <rom> <syms.toml> [opcode_hex ...]   (with opcodes: list the functions that build them)
"""
import collections
import re
import struct
import sys

rom = open(sys.argv[1], "rb").read()
text = open(sys.argv[2]).read()
secs = [(int(r, 16), int(v, 16)) for r, v in re.findall(r"rom = 0x([0-9A-F]+)\nvram = 0x([0-9A-F]+)", text)]
funcs = [(int(v, 16), int(s, 16)) for _, v, s in re.findall(r'name = "(\w+)", vram = 0x([0-9A-F]+), size = 0x([0-9A-F]+)', text)]


def w(va):
    r, v = max((s for s in secs if s[1] <= va), key=lambda s: s[1])
    return struct.unpack_from(">I", rom, r + va - v)[0]


count = collections.Counter()
where = collections.defaultdict(set)
for va, size in funcs:
    words = [w(a) for a in range(va, va + size, 4)]
    for i, x in enumerate(words):
        if x >> 26 != 0x0F:
            continue
        rt = (x >> 16) & 31
        imm = x & 0xFFFF
        for y in words[i + 1:i + 6]:
            op = y >> 26
            if op in (0x09, 0x0D) and (y >> 16) & 31 == rt and (y >> 21) & 31 != rt:
                break  # register overwritten by an unrelated value
            if op == 0x0D and (y >> 21) & 31 == rt:  # ori rt, rt, lo: still a command word
                continue
            if op == 0x2B and (y >> 16) & 31 == rt:  # sw rt, ...
                count[imm >> 8] += 1
                where[imm >> 8].add(va)
                break
if len(sys.argv) > 3:
    for o in sys.argv[3:]:
        op = int(o, 16)
        print(f"0x{op:02X}: " + " ".join(f"{a:08X}" for a in sorted(where[op])))
    sys.exit()
for op, n in sorted(count.items(), key=lambda t: -t[1]):
    if n >= 3:
        print(f"0x{op:02X}: {n:5} stores in {len(where[op]):4} functions")
