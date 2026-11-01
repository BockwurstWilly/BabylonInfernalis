"""Post-process RSPRecomp output: drop writes to $zero ("0 = ...;").

RSPRecomp emits them for data words that decode as instructions with rd = $zero (e.g. 0x00000680 after a jump in
the Factor 5 audio microcode); they are unreachable but do not compile.
usage: fix_rsp_output.py <file.cpp>
"""
import re
import sys

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
fixed, n = re.subn(r"^(\s*)0 = [^;\n]*;", r"\1// (write to $zero removed by tools/fix_rsp_output.py)", src, flags=re.M)
open(path, "w", encoding="utf-8", newline="\n").write(fixed)
print(f"{path}: removed {n} write(s) to $zero")
