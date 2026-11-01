"""Find where the demand-paged region 0x41000000 lives in ROM.

Collects every `jal` into 0x41xxxxxx from the boot code and scores candidate ROM bases by how many targets hit
a function prologue (`addiu sp,sp,-N`) or the instruction right after a `jr ra` delay slot.
"""
import struct
import sys

rom = open(sys.argv[1], "rb").read()
BOOT = (0x1000, 0x4DC30)
VM = 0x41000000


def be32(o):
    return struct.unpack_from(">I", rom, o)[0]


targets = set()
for o in range(BOOT[0], BOOT[1], 4):
    w = be32(o)
    if w >> 26 == 3:  # jal
        t = 0x40000000 | ((w & 0x3FFFFFF) << 2)
        if VM <= t < VM + 0x400000:  # larger values are data words that merely look like jal
            targets.add(t - VM)
targets = sorted(targets)
print("jal targets into vm:", len(targets), "min", hex(targets[0]), "max", hex(targets[-1]))


def prologue(o):
    if o < 8 or o + 4 > len(rom):
        return False
    w = be32(o)
    return (w & 0xFFFF8000) == 0x27BD8000 or be32(o - 8) == 0x03E00008


best = []
for base in range(0x1000, len(rom) - targets[-1], 0x10):
    hits = 0
    for t in targets[:200]:
        if prologue(base + t):
            hits += 1
    if hits > 20:
        best.append((hits, base))
best.sort(reverse=True)
for hits, base in best[:10]:
    print(f"ROM base {base:07X}: {hits}/{min(200, len(targets))} sample targets hit a prologue")
