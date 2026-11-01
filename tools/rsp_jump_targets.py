"""Find indirect jump targets of an RSP microcode for RSPRecomp's extra_indirect_branch_targets.

Looks at every jr (except jr $ra-style returns are kept too, since RSP code uses jr for dispatch) and searches the
microcode's DMEM data for runs of >= 4 halfwords that are valid instruction addresses inside the text
(IMEM 0x1000-0x1FFF, word aligned; the RSP ignores bit 12 differences, so 0x0xxx and 0x1xxx are both accepted).
usage: rsp_jump_targets.py <text.bin> <text_imem_addr_hex> <text_size_hex> <data.bin>
"""
import struct
import sys

text = open(sys.argv[1], "rb").read()
base = int(sys.argv[2], 16) & 0x1FFF
size = int(sys.argv[3], 16)
data = open(sys.argv[4], "rb").read()

jrs = []
for i in range(0, size, 4):
    w = struct.unpack_from(">I", text, i)[0]
    if w >> 26 == 0 and (w & 0x3F) == 0x08:
        jrs.append((base + i, (w >> 21) & 31))
print("jr instructions:", " ".join(f"{a:04X}(r{r})" for a, r in jrs))


def valid(h):
    a = (h & 0x0FFF) | 0x1000
    return h & 3 == 0 and base <= a < base + size and h & 0xE000 == 0


runs = []
i = 0
while i < len(data) - 1:
    j = i
    while j < len(data) - 1 and valid(struct.unpack_from(">H", data, j)[0]):
        j += 2
    if j - i >= 8:
        runs.append((i, j))
        i = j
    else:
        i += 2
targets = set()
for s, e in runs:
    vals = [struct.unpack_from(">H", data, k)[0] for k in range(s, e, 2)]
    print(f"DMEM table 0x{s:03X}-0x{e:03X}: " + " ".join(f"{v:04X}" for v in vals))
    targets.update((v & 0x0FFF) | 0x1000 for v in vals)
print("\nextra_indirect_branch_targets = [" + ", ".join(f"0x{t:04X}" for t in sorted(targets)) + "]")
