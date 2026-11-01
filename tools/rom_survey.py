"""First survey of the Indy64 ROM: header, IPL3, code region, notable strings, entropy map."""
import hashlib
import math
import re
import struct
import sys
import zlib

rom = open(sys.argv[1], "rb").read()


def be32(off):
    return struct.unpack_from(">I", rom, off)[0]


print("size", hex(len(rom)))
print("clock", hex(be32(4)), "entry", hex(be32(8)), "release", hex(be32(0xC)))
print("crc1/2", hex(be32(0x10)), hex(be32(0x14)))
print("title", rom[0x20:0x34].decode("ascii", "replace").strip(), "id", rom[0x3B:0x3F])
ipl3 = rom[0x40:0x1000]
print("ipl3 crc32", hex(zlib.crc32(ipl3)), "md5", hashlib.md5(ipl3).hexdigest())

# Strings that hint at libultra version, microcodes, compression, debug info
interesting = re.compile(rb"(RSP|SP |Gfx|F3D|ucode|uCode|MusyX|musyx|Factor|LucasArts|Indy|Version|version|PRINTF|assert|\.c\b|\.cpp\b|Error|ERROR|libultra|Nintendo|Copyright|\(C\)|Yay0|Yaz0|MIO0|LZ|zlib|deflate)")
strings = []
for m in re.finditer(rb"[\x20-\x7e]{6,}", rom):
    if interesting.search(m.group()):
        strings.append((m.start(), m.group().decode("ascii")))
print("\n# interesting strings:", len(strings))
for off, s in strings[:400]:
    print(f"{off:08X} {s[:120]}")

# Compression magic
print("\n# compression magic")
for magic in (b"Yay0", b"Yaz0", b"MIO0", b"\x1f\x8b\x08"):
    hits = [m.start() for m in re.finditer(re.escape(magic), rom)]
    print(magic, len(hits), [hex(h) for h in hits[:10]])

# Entropy per 64 KiB block (high ~8 = compressed/encrypted, low = code/tables/padding)
print("\n# entropy per 256 KiB")
blk = 0x40000
row = []
for i in range(0, len(rom), blk):
    b = rom[i:i + blk]
    cnt = [0] * 256
    for x in b:
        cnt[x] += 1
    e = -sum(c / len(b) * math.log2(c / len(b)) for c in cnt if c)
    row.append(f"{i:07X}:{e:.2f}")
for i in range(0, len(row), 8):
    print("  ".join(row[i:i + 8]))

# Heuristic: find where MIPS code ends, by counting typical opcodes (jr ra, addiu sp) per 4 KiB
print("\n# code density (jr ra + addiu sp,sp) per 64 KiB")
JR_RA = 0x03E00008
dens = []
for i in range(0x1000, len(rom), 0x10000):
    n = 0
    for j in range(i, min(i + 0x10000, len(rom)), 4):
        w = be32(j)
        if w == JR_RA or (w & 0xFFFF0000) == 0x27BD0000:
            n += 1
    dens.append((i, n))
print(" ".join(f"{i:07X}:{n}" for i, n in dens if n > 20))
