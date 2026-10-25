"""Rewrite Ghidra's DAT_xxxxxxxx literal-pool references in decompiled.c to what they hold:
RAM pointers -> g_2000XXXX, peripheral addresses -> SVD register name, plausible floats -> value,
flash addresses -> L_XXXX. Usage: resolve_dat.py image.bin decompiled.c SVD out.c [names.txt]
names.txt (optional): lines "0x20000E34 g_pulse_count" to give RAM variables real names."""
import re, struct, sys, os
sys.path.append(os.path.dirname(__file__))
from svd_map import load

img = open(sys.argv[1], 'rb').read()
src = open(sys.argv[2], encoding='utf-8').read()
regs = load(sys.argv[3])
names = {}
if len(sys.argv) > 5:
    for line in open(sys.argv[5], encoding='utf-8'):
        if line.strip() and not line.startswith('#'):
            a, n = line.split()[:2]
            names[int(a, 0)] = n


def describe(v):
    if v in names:
        return names[v]
    if 0x20000000 <= v < 0x20006000:
        return f'g_{v:08X}'
    if v in regs:
        return regs[v][0]
    if 0x40000000 <= v < 0x40020000:
        base = max((a for a in regs if a <= v), default=None)
        return f'{regs[base][0]}+0x{v - base:X}' if base else f'P_{v:08X}'
    if 0 < v < 0x40000:
        return f'L_{v:05X}'
    f = struct.unpack('<f', struct.pack('<I', v))[0]
    if 1e-6 < abs(f) < 1e9 and (v & 0xFFF) != 0 or (v & 0x7FFFF) == 0 and 0x30000000 < v < 0x50000000:
        return f'{f:g}f/*0x{v:08X}*/'
    return f'0x{v:X}'


def sub(m):
    a = int(m.group(1), 16)
    if a + 4 > len(img):
        return m.group(0)
    return describe(struct.unpack_from('<I', img, a)[0])


out = re.sub(r'\bDAT_([0-9a-f]{8})\b', sub, src)
open(sys.argv[4], 'w', encoding='utf-8').write(out)
