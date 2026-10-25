"""Call stock-firmware routines in the emulator and log their peripheral writes.

Usage: stock_trace.py <workspace> 0xADDR[,arg0,...] ...   e.g. stock_trace.py . 0xC6FC 0x6DD8,64
PLL LOCKED reads as 1 so clock routines finish. Needs backup/.
"""
import sys, struct
sys.path.insert(0, sys.argv[1] + '/tools')
from emu import Firmware
from svd_map import load
from unicorn import UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
names = {a: n for a, (n, f) in load(sys.argv[1] + '/vendor/FM33LC0XX_DFP/SVD/FM33LC0XX.SVD').items()}
fw = Firmware(sys.argv[1] + '/backup/ut334a_stock_1.bin')
fw.uc.mem_write(0x1FFFF000, open(sys.argv[1] + '/backup/ut334a_info_1FFFFA00-1FFFFFFF.bin', 'rb').read().rjust(0x1000, b'\0'))
log = []
def wr(uc, acc, addr, size, val, ud):
    log.append(f'  W {names.get(addr, hex(addr)):16s} = 0x{val & 0xFFFFFFFF:08X}')
def rd(uc, acc, addr, size, val, ud):
    # status bits that firmware polls: PLL LOCKED, everything ready
    if addr == 0x40000218:
        v = struct.unpack('<I', uc.mem_read(addr, 4))[0] | 0x80
        uc.mem_write(addr, struct.pack('<I', v))
fw.uc.hook_add(UC_HOOK_MEM_WRITE, wr, begin=0x40000000, end=0x4001FFFF)
fw.uc.hook_add(UC_HOOK_MEM_READ, rd, begin=0x40000000, end=0x4001FFFF)
for spec in sys.argv[2:]:
    a, *args = [int(x, 0) for x in spec.split(',')]
    log.clear()
    try:
        fw.call(a, *args, max_insn=500000)
        st = 'returned'
    except Exception as e:
        st = f'FAILED {e}'
    print(f'call 0x{a:X}{tuple(args)}: {st}')
    print('\n'.join(log))
