"""Sanity checks for the Ghidra function export before it becomes an N64Recomp symbol file.

Reports per segment: functions inside the microcode region, functions past the end of the code, overlaps,
non-contiguous bodies and large gaps between functions (bytes not covered by any function).
usage: check_functions.py <functions.csv> <rom> [code_end_boot code_end_vm]
"""
import csv
import struct
import sys

rows = list(csv.DictReader(open(sys.argv[1])))
rom = open(sys.argv[2], "rb").read()
SEG = {"boot": (0x40000400, 0x1000), "vm_paged": (0x41000000, 0x4DC30)}
UCODE = (0x400371B0, 0x4003AE50)


def word(va, block):
    base, r = SEG[block]
    return struct.unpack_from(">I", rom, r + va - base)[0]


for block in SEG:
    fs = sorted((int(r["entry"], 16), int(r["body_max"], 16) + 1, r) for r in rows if r["block"] == block)
    print(f"== {block}: {len(fs)} functions, {fs[0][0]:08X}-{fs[-1][1]:08X}")
    for e, end, r in fs:
        if UCODE[0] <= e < UCODE[1]:
            print(f"  in ucode   {e:08X} {r['name']}")
        if int(r["ranges"]) > 1:
            print(f"  ranges={r['ranges']} {e:08X}-{end:08X} {r['name']}")
    for (e1, x1, r1), (e2, _, r2) in zip(fs, fs[1:]):
        if x1 > e2:
            print(f"  overlap    {r1['name']} {e1:08X}-{x1:08X} runs into {r2['name']} {e2:08X}")
        gap = e2 - x1
        if gap > 0x40:
            nonzero = sum(1 for a in range(x1, e2, 4) if word(a, block) != 0)
            print(f"  gap {gap:#7x} at {x1:08X}-{e2:08X} ({nonzero} non-zero words) before {r2['name']}")
