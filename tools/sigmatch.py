"""Match n64sym signatures (sig_v1, third_party/n64sym/builtin_signatures.sig) against the Indy64 code segments.

Same rule as n64sym's CSignatureFile::TestSymbol: relocated words are stripped (hi16/lo16: low 16 bits zeroed,
targ26: low 26 bits zeroed), then crcA = CRC32 of the first min(size, 8) bytes, crcB = CRC32 of all bytes.
Every word-aligned offset of both segments is a candidate. Matches also name their relocation targets
(jal targets and hi16/lo16 pairs) unless the target name is a section-local placeholder (obj_text_0010 etc.).

usage: sigmatch.py <rom> <sigfile> <out.csv>
"""
import collections
import re
import struct
import sys
import zlib

rom = open(sys.argv[1], "rb").read()
SEGS = [("boot", 0x1000, 0x4DC30, 0x40000400), ("vm", 0x4DC30, 0x4DC30 + 0xBE620, 0x41000000)]
TYPES = {".hi16": 0xFFFF0000, ".lo16": 0xFFFF0000, ".targ26": 0xFC000000}
LOCAL = re.compile(r"_(text|data|rodata|bss|sdata|sbss)_[0-9A-F]{4}$")


def load_sigs(path):
    syms = []
    cur = None
    for line in open(path, encoding="ascii", errors="replace"):
        p = line.split()
        if not p or p[0].startswith("#"):
            continue
        if p[0] in TYPES:
            for off in p[2:]:
                cur["relocs"].append((int(off, 16), p[0], p[1]))
        else:
            cur = {"name": p[0], "size": int(p[1], 16), "crcA": int(p[2], 16), "crcB": int(p[3], 16), "relocs": []}
            syms.append(cur)
    for s in syms:
        s["relocs"].sort()
        s["masks"] = {o: TYPES[t] for o, t, _ in s["relocs"]}
    return syms


def stripped(buf, start, size, masks):
    out = bytearray(buf[start:start + size])
    for o, m in masks.items():
        if o + 4 <= size:
            w = struct.unpack_from(">I", out, o)[0] & m
            struct.pack_into(">I", out, o, w)
    return bytes(out)


def main():
    syms = load_sigs(sys.argv[2])
    print("signatures:", len(syms))
    # group by the strip pattern of the first 8 bytes so crcA is computed once per pattern and offset
    groups = collections.defaultdict(list)
    for s in syms:
        n = min(s["size"], 8)
        key = (n, tuple(sorted((o, m) for o, m in s["masks"].items() if o < n)))
        groups[key].append(s)
    print("crcA patterns:", len(groups))

    found = {}  # vaddr -> list of (name, size)
    derived = collections.defaultdict(set)  # vaddr -> names from relocations
    for seg, s0, s1, va in SEGS:
        data = rom[s0:s1]
        for (n, pat), members in groups.items():
            want = collections.defaultdict(list)
            for s in members:
                want[s["crcA"]].append(s)
            masks = dict(pat)
            for off in range(0, len(data) - n + 1, 4):
                c = zlib.crc32(stripped(data, off, n, masks) if masks else data[off:off + n])
                if c not in want:
                    continue
                for s in want[c]:
                    if off + s["size"] > len(data):
                        continue
                    if zlib.crc32(stripped(data, off, s["size"], s["masks"])) != s["crcB"]:
                        continue
                    found.setdefault(va + off, []).append((s["name"], s["size"]))
                    hi = {}
                    for ro, t, name in s["relocs"]:
                        w = struct.unpack_from(">I", data, off + ro)[0]
                        if t == ".targ26":
                            tgt = (va & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                        elif t == ".hi16":
                            hi[name] = (w & 0xFFFF) << 16
                            continue
                        elif name in hi:
                            tgt = (hi[name] + struct.unpack(">h", struct.pack(">H", w & 0xFFFF))[0]) & 0xFFFFFFFF
                        else:
                            continue
                        if not LOCAL.search(name):
                            derived[tgt].add(name)
        print(seg, "done, matches so far:", len(found))

    with open(sys.argv[3], "w") as f:
        f.write("address,name,size,source,alternatives\n")
        for a in sorted(found):
            cands = sorted(set(found[a]), key=lambda x: -x[1])
            name, size = cands[0]
            alts = " ".join(sorted({c[0] for c in cands} - {name}))
            f.write(f"0x{a:08X},{name},0x{size:X},sig,{alts}\n")
        for a in sorted(derived):
            if a in found:
                continue
            names = sorted(derived[a])
            f.write(f"0x{a:08X},{names[0]},,reloc,{' '.join(names[1:])}\n")
    print("functions matched:", len(found), " reloc-derived names:", len([a for a in derived if a not in found]))


main()
