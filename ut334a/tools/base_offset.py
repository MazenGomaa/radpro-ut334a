"""After each LDR-literal of a peripheral base, follow that register for a few instructions
and report loads/stores whose effective address lands in [lo, hi).
Usage: base_offset.py image.bin lo hi"""
import struct, sys, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
img = open(sys.argv[1], 'rb').read()
lo, hi = int(sys.argv[2], 0), int(sys.argv[3], 0)
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
for pc in range(0, len(img) - 1, 2):
    h = struct.unpack_from('<H', img, pc)[0]
    if h >> 11 != 0b01001:
        continue
    t = ((pc + 4) & ~3) + (h & 0xFF) * 4
    base = struct.unpack_from('<I', img, t)[0]
    if not (0x40000000 <= base < 0x40020000):
        continue
    reg = f'r{(h >> 8) & 7}'
    for i in list(md.disasm(img[pc + 2:pc + 40], pc + 2))[:12]:
        m = re.match(rf'\[{reg}(?:, #(0x[0-9a-f]+|\d+))?\]', i.op_str.split(', ', 1)[-1])
        if i.mnemonic.startswith(('str', 'ldr')) and m:
            ea = base + int(m.group(1) or '0', 0)
            if lo <= ea < hi:
                print(f'{i.address:08X}: {i.mnemonic} {i.op_str}   ; base 0x{base:08X} -> 0x{ea:08X}')
        if i.op_str.startswith(reg + ',') and not i.mnemonic.startswith(('str', 'cmp', 'tst')):
            break
