# Scans an RDRAM dump for F5 fixed-point MVP matrices (columns 2 and 3 equal in the first three rows, as the game's
# projection produces) and prints them. Used to find the emulator's matrix pool, whose address differs from ours.
# Usage: python -I tools/find_matrices.py <ram.bin> [start_hex end_hex]
import struct
import sys

ram = open(sys.argv[1], "rb").read()
start = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0
end = int(sys.argv[3], 16) if len(sys.argv) > 3 else len(ram)

for a in range(start, end - 64, 8):
    ints = struct.unpack(">16h", ram[a:a + 32])
    fracs = struct.unpack(">16H", ram[a + 32:a + 64])
    m = [[ints[r * 4 + c] + fracs[r * 4 + c] / 65536.0 for c in range(4)] for r in range(4)]
    if not all(m[r][2] == m[r][3] for r in range(3)):
        continue
    if not (0.5 < m[3][3] - m[3][2] < 1.5) or all(m[r][2] == 0 for r in range(3)):
        continue
    rows = " | ".join(" ".join(f"{v:9.3f}" for v in row) for row in m)
    print(f"{a:06X}  {rows}")
