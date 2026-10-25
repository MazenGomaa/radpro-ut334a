"""Find loads/stores whose effective address falls in [lo, hi), following a register loaded
from a literal pool through adds/subs #imm and movs/mov copies (within a short window).
Usage: periph_access.py image.bin lo hi"""
import struct, sys, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

img = open(sys.argv[1], 'rb').read()
lo, hi = int(sys.argv[2], 0), int(sys.argv[3], 0)
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
seen = set()
for pc in range(0, len(img) - 1, 2):
    h = struct.unpack_from('<H', img, pc)[0]
    if h >> 11 != 0b01001:
        continue
    t = ((pc + 4) & ~3) + (h & 0xFF) * 4
    base = struct.unpack_from('<I', img, t)[0]
    if not (0x40000000 <= base < 0x40020000):
        continue
    regs = {f'r{(h >> 8) & 7}': base}
    for i in list(md.disasm(img[pc + 2:pc + 48], pc + 2))[:16]:
        ops = [o.strip() for o in i.op_str.split(',')]
        if i.mnemonic in ('b', 'bl', 'bx', 'pop', 'blx') or i.mnemonic.startswith('b') and len(i.mnemonic) <= 3 and i.mnemonic not in ('bic', 'bics'):
            break
        m = re.match(r'(\w+), \[(\w+)(?:, #(0x[0-9a-f]+|\d+))?\]', i.op_str)
        if m and m.group(2) in regs and i.mnemonic.startswith(('str', 'ldr')):
            ea = regs[m.group(2)] + int(m.group(3) or '0', 0)
            if lo <= ea < hi and i.address not in seen:
                seen.add(i.address)
                print(f'{i.address:08X}: {i.mnemonic:5} {i.op_str:24} -> 0x{ea:08X}')
            if i.mnemonic.startswith('ldr'):
                regs.pop(m.group(1), None)
            continue
        if i.mnemonic in ('adds', 'subs') and len(ops) == 2 and ops[0] in regs and ops[1].startswith('#'):
            regs[ops[0]] += int(ops[1][1:], 0) * (1 if i.mnemonic == 'adds' else -1)
            continue
        if i.mnemonic in ('mov', 'movs') and len(ops) == 2 and ops[1] in regs:
            regs[ops[0]] = regs[ops[1]]
            continue
        if ops and ops[0] in regs and not i.mnemonic.startswith(('str', 'cmp', 'tst')):
            regs.pop(ops[0])
        if not regs:
            break
