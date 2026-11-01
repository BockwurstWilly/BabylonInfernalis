"""Find function-boundary problems in the recomp symbol file before N64Recomp trips over them.

- data islands: functions whose last instruction pair contains no jr/j/branch (cannot end like that);
- PC-relative tricks: bltzal/bgezal $zero (link without branching, used to read the own address);
- branches (not calls) whose target lies in another function but not at its start.
usage: scan_boundaries.py <rom> <syms.toml>
"""
import bisect
import re
import struct
import sys

rom = open(sys.argv[1], "rb").read()
text = open(sys.argv[2]).read()
secs = [(int(r, 16), int(v, 16)) for r, v in re.findall(r"rom = 0x([0-9A-F]+)\nvram = 0x([0-9A-F]+)", text)]
funcs = sorted((int(v, 16), int(s, 16), n) for n, v, s in
               re.findall(r'name = "(\w+)", vram = 0x([0-9A-F]+), size = 0x([0-9A-F]+)', text))
starts = [f[0] for f in funcs]


def w(va):
    r, v = max((s for s in secs if s[1] <= va), key=lambda s: s[1])
    return struct.unpack_from(">I", rom, r + va - v)[0]


def is_jump(x):
    op = x >> 26
    if op == 0 and (x & 0x3F) in (0x08, 0x09):  # jr / jalr
        return True
    if op in (2, 3) or op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17):
        return True
    if op == 1 and ((x >> 16) & 31) in (0, 1, 2, 3, 0x10, 0x11, 0x12, 0x13):
        return True
    return op == 0x11 and ((x >> 21) & 31) == 8  # bc1x


def branch_target(a, x):
    op = x >> 26
    if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op == 1 and ((x >> 16) & 31) in (0, 1, 2, 3)) or \
            (op == 0x11 and ((x >> 21) & 31) == 8):
        off = x & 0xFFFF
        return a + 4 + ((off - 0x10000 if off & 0x8000 else off) << 2)
    if op == 2:  # j (not jal)
        return (a & 0xF0000000) | ((x & 0x3FFFFFF) << 2)
    return None


for va, size, name in funcs:
    words = [w(a) for a in range(va, va + size, 4)]
    tail = words[-2:]
    if not any(is_jump(x) for x in tail):
        print(f"no-exit   {name} {va:08X} size {size:#x}: {' '.join(f'{x:08x}' for x in words[:4])}")
    for i, x in enumerate(words):
        a = va + 4 * i
        if x >> 26 == 1 and (x >> 21) & 31 == 0 and ((x >> 16) & 31) in (0x10, 0x11):
            print(f"pc-trick  {name} {a:08X}: {'bltzal' if ((x >> 16) & 31) == 0x10 else 'bgezal'} $zero")
        t = branch_target(a, x)
        if t is not None and not (va <= t < va + size):
            k = bisect.bisect_right(starts, t) - 1
            if k >= 0 and funcs[k][0] != t and t < funcs[k][0] + funcs[k][1]:
                print(f"mid-jump  {name} {a:08X} -> {t:08X} inside {funcs[k][2]}")
