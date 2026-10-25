"""Print an SVD register's fields, optionally decoding a value. Usage: svd_fields.py FM33.SVD REG [value]"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from svd_map import load
m = load(sys.argv[1])
want = sys.argv[2].upper()
val = int(sys.argv[3], 0) if len(sys.argv) > 3 else None
for a, (n, fields) in sorted(m.items()):
    if n.upper() == want:
        print(f'{n} @0x{a:08X}')
        for f, lsb, w in sorted(fields, key=lambda x: -x[1]):
            s = f'  [{lsb + w - 1}:{lsb}] {f}'
            if val is not None:
                s += f' = 0x{(val >> lsb) & ((1 << w) - 1):X}'
            print(s)
