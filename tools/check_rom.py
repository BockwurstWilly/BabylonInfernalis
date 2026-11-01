"""Checks that rom/indy.z64 is the supported ROM: Indiana Jones and the Infernal Machine (USA), big-endian (.z64).

A byte-swapped (.v64) or little-endian (.n64) dump is converted to .z64 in place (the original is kept as .bak).
usage: check_rom.py [path]   (default: rom/indy.z64)
"""
import hashlib
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHA1 = "93300d63412d6c7432109b5ad2e4a8b9348e9538"


def to_z64(data):
    magic = data[:4]
    if magic == b"\x80\x37\x12\x40":
        return data
    if magic == b"\x37\x80\x40\x12":  # .v64: 16-bit byte swap
        return b"".join(data[i + 1:i + 2] + data[i:i + 1] for i in range(0, len(data), 2))
    if magic == b"\x40\x12\x37\x80":  # .n64: 32-bit byte swap
        return b"".join(data[i:i + 4][::-1] for i in range(0, len(data), 4))
    return None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "rom", "indy.z64")
    if not os.path.exists(path):
        sys.exit(f"ROM not found: {path}\nCopy your own dump of the USA version there (see BUILDING.md).")
    data = open(path, "rb").read()
    z64 = to_z64(data)
    if z64 is None:
        sys.exit("This file is not an N64 ROM (unknown header).")
    if hashlib.sha1(z64).hexdigest() != SHA1:
        sys.exit(f"Wrong ROM: SHA-1 {hashlib.sha1(z64).hexdigest()}, expected {SHA1} "
                 "(Indiana Jones and the Infernal Machine, USA).")
    if z64 != data:
        shutil.copyfile(path, path + ".bak")
        open(path, "wb").write(z64)
        print("converted the ROM to big-endian (.z64), original kept as .bak")
    print("ROM OK: Indiana Jones and the Infernal Machine (USA)")


main()
