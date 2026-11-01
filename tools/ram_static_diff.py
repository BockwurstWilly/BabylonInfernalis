# Finds words that stay constant within each build (two dumps each) but differ between the emulator and our build.
# Static data like level geometry should match exactly, so differences here point at corrupted or wrongly loaded data.
# Usage: python -I tools/ram_static_diff.py <emu_a> <emu_b> <our_a> <our_b> [block_hex]
import sys

emu_a, emu_b, our_a, our_b = (open(p, "rb").read() for p in sys.argv[1:5])
block = int(sys.argv[5], 16) if len(sys.argv) > 5 else 0x10000
size = min(len(emu_a), len(our_a))

print("block     static  differ  first diff")
for base in range(0, size, block):
    static = differ = 0
    first = None
    for o in range(base, base + block, 4):
        ea, oa = emu_a[o:o + 4], our_a[o:o + 4]
        if ea != emu_b[o:o + 4] or oa != our_b[o:o + 4]:
            continue
        static += 1
        if ea != oa:
            differ += 1
            if first is None:
                first = o
    if differ:
        print(f"{base:06X}  {static:7d} {differ:7d}  {first:06X}")
