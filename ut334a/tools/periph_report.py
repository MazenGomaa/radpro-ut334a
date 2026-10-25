"""Annotate every peripheral register access in an FM33 flash image.

For each LDR-literal of a peripheral/SCS address, follow the base register (through adds/subs/mov)
for a short window, emulating constants well enough to show written values and read-modify-write
bit operations. Register names come from the SVD.

Usage: periph_report.py image.bin FM33LC0XX.SVD [filter-substring ...]
Output: <func> <addr> R|W <REG> <value/op>
"""
import struct, sys, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
sys.path.insert(0, __import__('os').path.dirname(__file__))
from svd_map import load

img = open(sys.argv[1], 'rb').read()
regmap = load(sys.argv[2])
filters = sys.argv[3:]
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

SCS = {0xE000E010: 'SysTick_CTRL', 0xE000E014: 'SysTick_LOAD', 0xE000E018: 'SysTick_VAL',
       0xE000E100: 'NVIC_ISER', 0xE000E180: 'NVIC_ICER', 0xE000E200: 'NVIC_ISPR', 0xE000E280: 'NVIC_ICPR',
       0xE000ED04: 'SCB_ICSR', 0xE000ED0C: 'SCB_AIRCR', 0xE000ED10: 'SCB_SCR', 0xE000ED1C: 'SCB_SHPR2',
       0xE000ED20: 'SCB_SHPR3'}
for i in range(8):
    SCS[0xE000E400 + 4 * i] = f'NVIC_IPR{i}'

def name(a):
    if a in regmap:
        return regmap[a][0]
    if a in SCS:
        return SCS[a]
    return f'?{a:08X}'

def lit(pc):
    h = struct.unpack_from('<H', img, pc)[0]
    return struct.unpack_from('<I', img, ((pc + 4) & ~3) + (h & 0xFF) * 4)[0]

def func_start(a):
    for p in range(a & ~1, max(a - 0x800, 0), -2):
        h = struct.unpack_from('<H', img, p)[0]
        if h >> 8 in (0xB5, 0xB4):
            return p
    return None

def is_periph(v):
    return 0x40000000 <= v < 0x40020000 or 0xE000E000 <= v < 0xE000EF00

out = set()
for pc in range(0, len(img) - 1, 2):
    h = struct.unpack_from('<H', img, pc)[0]
    if h >> 11 != 0b01001 or not is_periph(lit(pc)):
        continue
    k = {f'r{(h >> 8) & 7}': lit(pc)}      # known constant registers
    base = dict(k)                         # registers holding a peripheral address
    loaded = {}                            # reg -> (periph reg name, op string) after ldr from periph
    for i in list(md.disasm(img[pc + 2:pc + 64], pc + 2))[:24]:
        o = [x.strip() for x in i.op_str.split(',')]
        mn = i.mnemonic
        m = re.match(r'(\w+), \[(\w+)(?:, #(0x[0-9a-f]+|\d+))?\]', i.op_str)
        if m and m.group(2) in base and mn.startswith(('str', 'ldr')):
            ea = base[m.group(2)] + int(m.group(3) or '0', 0)
            rt = m.group(1)
            if mn.startswith('str'):
                if rt in k:
                    val = f'= 0x{k[rt] & 0xFFFFFFFF:X}'
                elif rt in loaded and loaded[rt][0] == name(ea):
                    val = loaded[rt][1] or '(rmw)'
                else:
                    val = f'= {rt}'
                out.add((i.address, 'W', name(ea), val))
            else:
                out.add((i.address, 'R', name(ea), ''))
                loaded[rt] = (name(ea), '')
                k.pop(rt, None); base.pop(rt, None)
            continue
        if mn in ('b', 'bl', 'bx', 'blx', 'pop') or (mn.startswith('b') and mn not in ('bics',) and len(mn) <= 3):
            break
        try:
            if mn in ('movs', 'mov') and len(o) == 2:
                if o[1].startswith('#'):
                    k[o[0]] = int(o[1][1:], 0); base.pop(o[0], None); loaded.pop(o[0], None)
                elif o[1] in k:
                    k[o[0]] = k[o[1]]
                    if o[1] in base: base[o[0]] = base[o[1]]
                    if o[1] in loaded: loaded[o[0]] = loaded[o[1]]
                else:
                    k.pop(o[0], None); base.pop(o[0], None)
                    if o[1] in loaded: loaded[o[0]] = loaded[o[1]]
                continue
            if mn == 'ldr' and '[pc' in i.op_str:
                v = lit(i.address); k[o[0]] = v; loaded.pop(o[0], None)
                if is_periph(v): base[o[0]] = v
                else: base.pop(o[0], None)
                continue
            if mn in ('lsls', 'lsrs', 'asrs') and len(o) == 3 and o[1] in k and o[2].startswith('#'):
                s = int(o[2][1:], 0)
                k[o[0]] = (k[o[1]] << s) & 0xFFFFFFFF if mn == 'lsls' else k[o[1]] >> s
                base.pop(o[0], None); continue
            if mn in ('adds', 'subs') and len(o) == 2 and o[0] in k and o[1].startswith('#'):
                d = int(o[1][1:], 0) * (1 if mn == 'adds' else -1)
                k[o[0]] += d
                if o[0] in base: base[o[0]] += d
                continue
            if mn in ('orrs', 'bics', 'ands', 'eors') and len(o) == 2 and o[0] in loaded:
                arg = f'0x{k[o[1]]:X}' if o[1] in k else o[1]
                sym = {'orrs': '|=', 'bics': '&= ~', 'ands': '&=', 'eors': '^='}[mn]
                n, prev = loaded[o[0]]
                loaded[o[0]] = (n, (prev + ' ' if prev else '') + f'{sym} {arg}')
                continue
            if mn in ('lsls', 'lsrs') and len(o) == 3 and o[1] in loaded:
                n, prev = loaded[o[1]]
                loaded[o[0]] = (n, (prev + ' ' if prev else '') + f'{mn} {o[2]}'); continue
        except (KeyError, ValueError):
            pass
        if o and o[0] in k and not mn.startswith(('str', 'cmp', 'tst')):
            k.pop(o[0], None); base.pop(o[0], None); loaded.pop(o[0], None)

for a, rw, n, v in sorted(out):
    line = f'{(func_start(a) or 0):05X} {a:05X} {rw} {n:22} {v}'
    if not filters or any(f in line for f in filters):
        print(line)
