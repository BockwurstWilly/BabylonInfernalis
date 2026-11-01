# Decodes every F5 MTX command (0105C03F <addr>) of a display-list dump with the RDRAM dump taken at the same list.
# Usage: python -I tools/dl_matrices.py <dl_n.txt> <ram_n.bin>
import struct
import sys

ram = open(sys.argv[2], "rb").read()


def fixed_matrix(addr):
    a = addr & 0x7FFFFF
    ints = struct.unpack(">16h", ram[a:a + 32])
    fracs = struct.unpack(">16H", ram[a + 32:a + 64])
    return [[ints[r * 4 + c] + fracs[r * 4 + c] / 65536.0 for c in range(4)] for r in range(4)]


n = 0
for line in open(sys.argv[1]):
    parts = line.split()
    if len(parts) >= 3 and parts[1] == "0105C03F":
        m = fixed_matrix(int(parts[2], 16))
        rows = " | ".join(" ".join(f"{v:9.3f}" for v in row) for row in m)
        print(f"{n:3d} {parts[2]}  {rows}")
        n += 1
