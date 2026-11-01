# Lists float-like words that are constant within each build but differ between the emulator and our build.
# Usage: python -I tools/ram_float_diff.py <emu_a> <emu_b> <our_a> <our_b> <start_hex> <end_hex>
import struct
import sys

emu_a, emu_b, our_a, our_b = (open(p, "rb").read() for p in sys.argv[1:5])
start, end = int(sys.argv[5], 16), int(sys.argv[6], 16)


def as_float(b, o):
    return struct.unpack(">f", b[o:o + 4])[0]


prev = -100
for o in range(start, end, 4):
    ea, oa = emu_a[o:o + 4], our_a[o:o + 4]
    if ea != emu_b[o:o + 4] or oa != our_b[o:o + 4] or ea == oa:
        continue
    fe, fo = as_float(emu_a, o), as_float(our_a, o)
    if not all(f == f and (f == 0 or 1e-4 < abs(f) < 1e5) for f in (fe, fo)):
        continue
    if o - prev > 0x40:
        print("--")
    prev = o
    print(f"{o:06X}  emu {fe:12.4f}   ours {fo:12.4f}")
