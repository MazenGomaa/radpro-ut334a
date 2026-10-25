"""List BL calls in a code range with the constant value of r0/r1 (if known), plus GPIO DSET/DRST
writes resolved from literal bases. Usage: call_args.py image.bin start end [name=addr ...]"""
import struct, sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
img = open(sys.argv[1], 'rb').read()
start, end = int(sys.argv[2], 0), int(sys.argv[3], 0)
names = {int(a, 0): n for n, a in (x.split('=') for x in sys.argv[4:])}
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
k = {}
for i in md.disasm(img[start:end], start):
    o = [x.strip() for x in i.op_str.split(',')]
    mn = i.mnemonic
    try:
        if mn in ('movs', 'mov') and len(o) == 2:
            if o[1].startswith('#'): k[o[0]] = int(o[1][1:], 0)
            elif o[1] in k: k[o[0]] = k[o[1]]
            else: k.pop(o[0], None)
        elif mn == 'ldr' and '[pc' in i.op_str:
            h = struct.unpack_from('<H', img, i.address)[0]
            k[o[0]] = struct.unpack_from('<I', img, ((i.address + 4) & ~3) + (h & 0xFF) * 4)[0]
        elif mn in ('lsls', 'lsrs') and len(o) == 3 and o[1] in k:
            s = int(o[2][1:], 0); k[o[0]] = (k[o[1]] << s) & 0xFFFFFFFF if mn == 'lsls' else k[o[1]] >> s
        elif mn in ('adds', 'subs') and len(o) == 2 and o[0] in k and o[1].startswith('#'):
            k[o[0]] += int(o[1][1:], 0) * (1 if mn == 'adds' else -1)
        elif mn == 'bl':
            t = int(o[0][1:], 0)
            r0 = f'0x{k["r0"]:X}' if 'r0' in k else '?'
            r1 = f'0x{k["r1"]:X}' if 'r1' in k else '?'
            print(f'{i.address:05X}: {names.get(t, hex(t)):10} r0={r0} r1={r1}')
            for r in ('r0', 'r1', 'r2', 'r3'): k.pop(r, None)
        elif mn == 'str' and o[1].startswith('[r') and o[0] in k:
            b = o[1].strip('[]')
            off = int(o[2].strip('#]'), 0) if len(o) > 2 else 0
            if b in k and 0x40000C00 <= k[b] < 0x40000D00 and off in (0x14, 0x18):
                port = 'ABCD'[(k[b] - 0x40000C00) // 0x40]
                pins = [f'P{port}{n}' for n in range(16) if k[o[0]] >> n & 1]
                print(f'{i.address:05X}: {"SET" if off == 0x14 else "RESET":10} {",".join(pins)}')
        elif o and o[0] in k and not mn.startswith(('str', 'cmp', 'tst', 'b')):
            k.pop(o[0], None)
    except (ValueError, KeyError):
        k.pop(o[0], None) if o else None
