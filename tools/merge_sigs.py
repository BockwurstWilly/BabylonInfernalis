"""Merge libultra signature matches (sigmatch.py output) into symbols/symbols.csv.

Rules:
- function matches: size >= 0x14 and a unique name, plus the known pairs of identical code resolved by link order;
- data names derived from relocations: lowest address per name (higher hits are struct fields), names that only
  come from tiny ambiguous signatures are dropped;
- manual entries in symbols.csv win unless listed in OVERRIDE (my earlier guesses that the signatures disproved).

usage: merge_sigs.py <libultra_matches.csv> <symbols.csv>
"""
import csv
import sys

MIN_SIZE = 0x14
# identical code, told apart by position in the libultra link order
RESOLVE = {
    0x400406E0: "__osPiCreateAccessQueue", 0x40040730: "__osPiGetAccess", 0x40040774: "__osPiRelAccess",
    0x40040F40: "__osSiCreateAccessQueue", 0x40040F90: "__osSiGetAccess", 0x40040FD4: "__osSiRelAccess",
    0x4003E1AC: "__ull_rem", 0x4003E250: "__ll_rem",
    0x4003C0F0: "__osContGetInitData", 0x4003FA50: "__osPfsGetInitData",
    0x4003E560: "osMotorStop", 0x4003E700: "osMotorStart",
}
UNSURE = {0x4003E560, 0x4003E700}
DROP_DATA = {"n_alSynRemovePlayer", "__osProfFlushMQ"}
OVERRIDE = {0x40036AF0}  # was guessed as osInitialize; the real one is at 0x4003D080

matches = list(csv.DictReader(open(sys.argv[1])))
rows = list(csv.DictReader(open(sys.argv[2])))
fields = ["address", "name", "kind", "size", "note"]
by_addr = {int(r["address"], 16): r for r in rows}
for a in OVERRIDE:
    by_addr.pop(a, None)

funcs = [m for m in matches if m["source"] == "sig" and int(m["size"], 16) >= MIN_SIZE]
count = {}
for m in funcs:
    count[m["name"]] = count.get(m["name"], 0) + 1
added = 0
for m in funcs:
    a = int(m["address"], 16)
    name = RESOLVE.get(a) or (m["name"] if count[m["name"]] == 1 else None)
    if not name or a in by_addr:
        if a in by_addr and not by_addr[a].get("size"):
            by_addr[a]["size"] = m["size"]
        continue
    note = "n64sym signature" + (", order of the identical pair unverified" if a in UNSURE else "")
    by_addr[a] = {"address": f"0x{a:08X}", "name": name, "kind": "func", "size": m["size"], "note": note}
    added += 1

lowest = {}
for m in matches:
    if m["source"] != "reloc" or m["name"] in DROP_DATA:
        continue
    a = int(m["address"], 16)
    if m["alternatives"]:
        continue  # several libultra objects disagree about this address
    if m["name"] not in lowest or a < lowest[m["name"]]:
        lowest[m["name"]] = a
for name, a in lowest.items():
    if a in by_addr:
        continue
    by_addr[a] = {"address": f"0x{a:08X}", "name": name, "kind": "data", "size": "", "note": "from libultra relocations"}
    added += 1

with open(sys.argv[2], "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=fields)
    w.writeheader()
    for a in sorted(by_addr):
        r = by_addr[a]
        w.writerow({k: r.get(k, "") for k in fields})
print("added", added, "total", len(by_addr))
