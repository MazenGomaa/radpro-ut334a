"""Disassemble Thumb code from a flash image. Usage: dis.py image.bin start end"""
import struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

img = open(sys.argv[1], 'rb').read()
start, end = int(sys.argv[2], 0), int(sys.argv[3], 0)
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
for i in md.disasm(img[start:end], start):
    extra = ''
    if i.mnemonic == 'ldr' and '[pc' in i.op_str:
        h = struct.unpack_from('<H', img, i.address)[0]
        if h >> 11 == 0b01001:
            t = ((i.address + 4) & ~3) + (h & 0xFF) * 4
            extra = f'   ; =0x{struct.unpack_from("<I", img, t)[0]:08X}'
    print(f'{i.address:08X}: {i.mnemonic:6} {i.op_str}{extra}')
