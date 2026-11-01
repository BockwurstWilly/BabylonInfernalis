"""Compare our symbol names with N64Recomp's built-in reimplemented/ignored/renamed libultra lists.

Shows which of our libultra functions the runtime will replace, and which functions the game calls but the runtime
does not know (they are recompiled as normal code, so hardware access in them must work or be patched).
usage: recomp_lists.py <symbol_lists.cpp> <symbols.csv>
"""
import csv
import re
import sys

src = open(sys.argv[1], encoding="utf-8").read()
lists = {}
for m in re.finditer(r"N64Recomp::(\w+)\s*\{(.*?)\};", src, re.S):
    lists[m.group(1)] = set(re.findall(r'"([^"]+)"', m.group(2)))
for k, v in lists.items():
    print(f"{k}: {len(v)}")

ours = [r for r in csv.DictReader(open(sys.argv[2])) if r["kind"] == "func"]
lib = [r for r in ours if r["note"].startswith("n64sym") or r["name"].startswith(("os", "__os"))]
for k in ("reimplemented_funcs", "ignored_funcs", "renamed_funcs"):
    hit = sorted(r["name"] for r in ours if r["name"] in lists.get(k, ()))
    print(f"\n{k} ({len(hit)}): {' '.join(hit)}")
known = set().union(*lists.values())
print("\nlibultra names unknown to N64Recomp:", " ".join(sorted(r["name"] for r in lib if r["name"] not in known)))
