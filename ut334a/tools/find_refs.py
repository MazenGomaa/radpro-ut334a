"""List literal-pool words in a flash image that fall in an address range, and the
Thumb LDR (literal) instructions that load them. Usage: find_refs.py image.bin lo hi"""
import struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

img = open(sys.argv[1], 'rb').read()
lo, hi = int(sys.argv[2], 0), int(sys.argv[3], 0)

pool = {}
for off in range(0, len(img) - 3, 4):
    w = struct.unpack_from('<I', img, off)[0]
    if lo <= w < hi:
        pool[off] = w

# Thumb-1 LDR Rt,[PC,#imm8*4]: 01001 ttt iiiiiiii
refs = []
for pc in range(0, len(img) - 1, 2):
    h = struct.unpack_from('<H', img, pc)[0]
    if h >> 11 == 0b01001:
        tgt = ((pc + 4) & ~3) + (h & 0xFF) * 4
        if tgt in pool:
            refs.append((pc, (h >> 8) & 7, tgt, pool[tgt]))

for pc, rt, tgt, w in refs:
    print(f"{pc:08X}: ldr r{rt}, =0x{w:08X}   (pool @{tgt:08X})")
