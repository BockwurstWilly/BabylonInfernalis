"""Extract the RSP microcode blobs from the boot segment into analysis/ucode and print their hashes.

usage: extract_ucode.py <rom> <outdir>
"""
import hashlib
import os
import sys

rom = open(sys.argv[1], "rb").read()
out = sys.argv[2]
os.makedirs(out, exist_ok=True)


def rom_off(ram):
    return ram - 0x40000400 + 0x1000


BLOBS = [
    ("rspboot_text", 0x40001700, 0xD0),
    ("f5_gfx_ucode_text", 0x400371B0, 0x40039A30 - 0x400371B0),
    ("f5_gfx_ucode_data", 0x40047610, 0x40047700 - 0x40047610),
    ("f5_audio_ucode_text", 0x40039A30, 0x1000),
    ("f5_audio_ucode_data", 0x40047700, 0x800),
    ("rsp_unknown_tail", 0x4003AA30, 0x4003AE50 - 0x4003AA30),
]
for name, ram, size in BLOBS:
    data = rom[rom_off(ram):rom_off(ram) + size]
    open(os.path.join(out, name + ".bin"), "wb").write(data)
    print(f"{name:22} RAM {ram:08X} ROM {rom_off(ram):06X} size {size:#06x} sha1 {hashlib.sha1(data).hexdigest()}")
