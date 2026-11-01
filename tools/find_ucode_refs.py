"""Find pointers to the RSP microcode region in the boot segment and the 0x41000000 window.

Looks for 32-bit data words and lui/addiu (or lui/ori) pairs that form an address in the region, in any of the
0x40/0x80/0xA0 views. Also prints the RSP block boundaries by exact COP2 density per 8 bytes.
usage: find_ucode_refs.py <rom>
"""
import struct
import sys

rom = open(sys.argv[1], "rb").read()
SEGS = [("boot", 0x1000, 0x4DC30, 0x40000400), ("vm", 0x4DC30, 0x4DC30 + 0xBE620, 0x41000000)]
LO, HI = 0x40037200, 0x4003BA00


def be32(o):
    return struct.unpack_from(">I", rom, o)[0]


def norm(a):
    return (a & 0x1FFFFFFF) | 0x40000000 if a >> 28 in (4, 8, 0xA) and a < 0xC0000000 else a


def vaddr(o):
    for _, s, e, v in SEGS:
        if s <= o < e:
            return v + o - s
    return None


hits = []
for name, s, e, v in SEGS:
    for o in range(s, e, 4):
        w = be32(o)
        if LO <= norm(w) < HI:
            hits.append((norm(w), f"word  {vaddr(o):08X}"))
        if w >> 26 == 0x0F:  # lui
            rt = (w >> 16) & 31
            hi = (w & 0xFFFF) << 16
            for k in range(1, 12):
                w2 = be32(o + 4 * k)
                op = w2 >> 26
                if op in (0x09, 0x0D) and (w2 >> 21) & 31 == rt:  # addiu/ori rt, rt, imm
                    imm = w2 & 0xFFFF
                    a = (hi + (imm - 0x10000 if op == 0x09 and imm & 0x8000 else imm)) & 0xFFFFFFFF
                    if LO <= norm(a) < HI:
                        hits.append((norm(a), f"pair  {vaddr(o):08X}"))
                    break
for a, where in sorted(set(hits)):
    print(f"{a:08X} <- {where}")

print("\n# RSP blocks (runs of >= 16 words, gaps <= 8 words without vector ops)")


def is_rsp(w):
    op = w >> 26
    return (op == 0x12 and (w >> 25) & 1) or op in (0x32, 0x3A)


start = last = None
for o in range(0x37000, 0x3C000, 4):
    if is_rsp(be32(o)):
        if start is None:
            start = o
        last = o
    elif start is not None and o - last > 32:
        if last - start > 64:
            print(f"ROM {start:06X}-{last + 4:06X}  RAM {start - 0x1000 + 0x40000400:08X}-{last + 4 - 0x1000 + 0x40000400:08X}")
        start = None
