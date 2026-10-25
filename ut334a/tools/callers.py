"""Find Thumb BL calls to a target address. Usage: callers.py image.bin target"""
import struct, sys
img = open(sys.argv[1], 'rb').read()
target = int(sys.argv[2], 0)
for pc in range(0, len(img) - 3, 2):
    h1, h2 = struct.unpack_from('<HH', img, pc)
    if h1 >> 11 == 0b11110 and h2 >> 14 == 0b11 and (h2 >> 12) & 1:
        s = (h1 >> 10) & 1
        j1, j2 = (h2 >> 13) & 1, (h2 >> 11) & 1
        i1, i2 = 1 - (j1 ^ s), 1 - (j2 ^ s)
        off = (s << 24) | (i1 << 23) | (i2 << 22) | ((h1 & 0x3FF) << 12) | ((h2 & 0x7FF) << 1)
        if s: off -= 1 << 25
        if pc + 4 + off == target:
            print(f'{pc:08X}')
