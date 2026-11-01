# Prints a RAM range of two dumps side by side as hex words and floats, marking differences.
# Usage: python -I tools/ram_side.py <dump_a> <dump_b> <start hex> <end hex>
import struct
import sys

a = open(sys.argv[1], "rb").read()
b = open(sys.argv[2], "rb").read()
start, end = int(sys.argv[3], 16), int(sys.argv[4], 16)
for o in range(start, end, 4):
    wa, wb = a[o:o + 4], b[o:o + 4]
    fa, fb = struct.unpack(">f", wa)[0], struct.unpack(">f", wb)[0]
    mark = "   " if wa == wb else "<<<"
    print(f"{o:06X}  {wa.hex().upper()} {fa:12.4f}   {wb.hex().upper()} {fb:12.4f}  {mark}")
