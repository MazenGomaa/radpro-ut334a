"""Which functions reference a given address (via literal pool)?  Uses Ghidra's functions.txt for names.
Usage: xref.py image.bin functions.txt addr [addr ...]"""
import struct, sys, bisect
img = open(sys.argv[1], 'rb').read()
funcs = []
for line in open(sys.argv[2], encoding='utf-8'):
    p = line.split()
    funcs.append((int(p[0], 16), p[1]))
funcs.sort()
starts = [f[0] for f in funcs]
def fname(a):
    i = bisect.bisect_right(starts, a) - 1
    return f'{funcs[i][1]}@{funcs[i][0]:X}' if i >= 0 else '?'
for t in (int(x, 0) for x in sys.argv[3:]):
    pools = [o for o in range(0, len(img) - 3, 4) if struct.unpack_from('<I', img, o)[0] == t]
    users = set()
    for pc in range(0, len(img) - 1, 2):
        h = struct.unpack_from('<H', img, pc)[0]
        if h >> 11 == 0b01001 and ((pc + 4) & ~3) + (h & 0xFF) * 4 in pools:
            users.add(fname(pc))
    print(f'0x{t:08X}: {", ".join(sorted(users)) or "-"}')
