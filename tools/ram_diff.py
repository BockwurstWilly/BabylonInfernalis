# Compares two RDRAM dumps of the emulator (reference) with two of the recompiled build, both taken while the game
# idles. Lists 32-bit words that are constant in the reference but change in our build and look like floats.
# Usage: python -I tools/ram_diff.py <emu_a> <emu_b> <our_a> <our_b> [start_hex end_hex]
import struct
import sys

emu_a, emu_b, our_a, our_b = (open(p, "rb").read() for p in sys.argv[1:5])
start = int(sys.argv[5], 16) if len(sys.argv) > 5 else 0
end = int(sys.argv[6], 16) if len(sys.argv) > 6 else min(len(emu_a), len(our_a))


def as_float(b, o):
    return struct.unpack(">f", b[o:o + 4])[0]


hits = []
for o in range(start, end, 4):
    ea, eb, oa, ob = emu_a[o:o + 4], emu_b[o:o + 4], our_a[o:o + 4], our_b[o:o + 4]
    if ea != eb or oa == ob:
        continue
    fa, fb = as_float(our_a, o), as_float(our_b, o)
    fe = as_float(emu_a, o)
    plausible = all(f == f and 1e-6 < abs(f) < 1e6 for f in (fa, fb))
    if plausible:
        hits.append((o, fe, fa, fb))

print(f"{len(hits)} float-like words constant in the reference but changing in our build")
prev = -100
for o, fe, fa, fb in hits:
    if o - prev > 0x40:
        print("--")
    prev = o
    print(f"{o:06X}  emu {fe:12.4f}   ours {fa:12.4f} -> {fb:12.4f}")
