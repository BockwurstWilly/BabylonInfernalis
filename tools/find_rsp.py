"""Locate RSP microcode candidates in the ROM by the density of COP2 vector instructions.

RSP vector ops are encoded as COP2 (opcode 0x12) with bit 25 set; lwc2/swc2 (0x32/0x3A) are RSP vector loads/stores.
CPU code on the VR4300 never uses these, so runs of them mark RSP text.
"""
import struct
import sys

rom = open(sys.argv[1], "rb").read()
end = int(sys.argv[2], 16) if len(sys.argv) > 2 else len(rom)
WIN = 0x100


def is_rsp(w):
    op = w >> 26
    return (op == 0x12 and (w >> 25) & 1) or op in (0x32, 0x3A)


hits = []
for off in range(0, end, WIN):
    words = struct.unpack_from(f">{WIN // 4}I", rom, off)
    n = sum(1 for w in words if is_rsp(w))
    if n >= WIN // 4 // 4:
        hits.append(off)

# merge adjacent windows into regions
regions = []
for h in hits:
    if regions and h - regions[-1][1] <= WIN * 2:
        regions[-1][1] = h + WIN
    else:
        regions.append([h, h + WIN])
for a, b in regions:
    print(f"ROM {a:07X}-{b:07X} size {b - a:#7x}  (RAM {a - 0x1000 + 0x80000400:08X} if in boot segment)")
