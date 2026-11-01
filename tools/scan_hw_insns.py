"""Find instructions N64Recomp cannot translate, per function of the recomp symbol file.

Flags: cache, COP0 moves of registers other than Status (12), TLB ops and eret, plus sync/syscall/break.
Prints one line per function with the offending addresses, then a summary.
usage: scan_hw_insns.py <rom> <syms.toml>
"""
import collections
import re
import struct
import sys

rom = open(sys.argv[1], "rb").read()
text = open(sys.argv[2]).read()
sections = re.findall(r"rom = 0x([0-9A-F]+)\nvram = 0x([0-9A-F]+)", text)
funcs = re.findall(r'name = "(\w+)", vram = 0x([0-9A-F]+), size = 0x([0-9A-F]+)', text)
secs = [(int(r, 16), int(v, 16)) for r, v in sections]


def rom_of(va):
    r, v = max((s for s in secs if s[1] <= va), key=lambda s: s[1])
    return r + va - v


def classify(w):
    op = w >> 26
    if op == 0x2F:
        return "cache"
    if op == 0x10:
        rs = (w >> 21) & 31
        rd = (w >> 11) & 31
        if rs in (0, 4) and rd != 12:
            return f"{'mfc0' if rs == 0 else 'mtc0'} r{rd}"
        if rs == 0x10:
            return {1: "tlbr", 2: "tlbwi", 6: "tlbwr", 8: "tlbp", 0x18: "eret"}.get(w & 0x3F, "cop0?")
    if op == 0 and (w & 0x3F) in (0x0C, 0x0D, 0x0F):
        return {0x0C: "syscall", 0x0D: "break", 0x0F: "sync"}[w & 0x3F]
    return None


summary = collections.Counter()
for name, va, size in funcs:
    va, size = int(va, 16), int(size, 16)
    hits = []
    for a in range(va, va + size, 4):
        k = classify(struct.unpack_from(">I", rom, rom_of(a))[0])
        if k:
            hits.append(f"{a:08X}:{k}")
            summary[k.split()[0]] += 1
    if hits:
        print(f"{name:32} {len(hits):3}  {' '.join(hits[:8])}{' ...' if len(hits) > 8 else ''}")
print("\nsummary:", dict(summary))
