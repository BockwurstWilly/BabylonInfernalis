"""List the *_recomp functions (and macros) the generated code calls but N64Recomp does not define, with callers.

These must come from the runtime (N64ModernRuntime / ultramodern) or from Indy64-specific patches.
usage: runtime_needs.py <RecompiledFuncs dir> <recomp.h>
"""
import collections
import glob
import os
import re
import sys

d = sys.argv[1]
header = open(sys.argv[2]).read()
defined = set(re.findall(r"RECOMP_FUNC void (\w+)\(", "".join(open(f).read() for f in glob.glob(os.path.join(d, "*.c")))))
declared = set(re.findall(r"void (\w+)\(uint8_t\* rdram, recomp_context\* ctx\);", open(os.path.join(d, "funcs.h")).read()))
needs = collections.defaultdict(set)
for f in glob.glob(os.path.join(d, "*.c")):
    cur = None
    for line in open(f):
        m = re.match(r"RECOMP_FUNC void (\w+)\(", line)
        if m:
            cur = m.group(1)
            continue
        for call in re.findall(r"\b(\w+_recomp)\(rdram, ctx\)", line):
            if call not in defined and call not in declared:
                needs[call].add(cur)
        for macro in re.findall(r"\b(MEM_\w+)\(", line):
            if f"define {macro}(" not in header:
                needs[macro].add(cur)
for name in sorted(needs):
    callers = sorted(needs[name])
    print(f"{name:28} <- {', '.join(callers[:6])}{' ...' if len(callers) > 6 else ''}")
