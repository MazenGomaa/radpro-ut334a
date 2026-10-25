"""Run the Rad Pro UT334A build in Unicorn from reset and log peripheral writes.

Usage: fw_emu.py <workspace> <firmware.bin> <nm-output.txt> <max_ticks> <key_mask> [key_release_tick] [pulse_period_ticks]
  pulse_period_ticks: run COMPx_IRQHandler (a GM pulse) every N ticks.
  key_mask: GPIO DIN bits forced low, 16 bits per port from PA (e.g. 0x400 = PA10/OK held).
SysTick is simulated: every WFI runs SysTick_Handler once. The flash controller is modelled minimally
(page erase on the 0x1234ABCD request, ERD/PRD always set); SPI1 is always ready, the ADC reads
2640 on IN9 and 1500 on VREF (a ~3.9 V battery). Calls to address 0 (unset callbacks) are
treated as empty functions. Stops at the first DeepSleep entry (PMU_CR.PMOD = 2) or after max_ticks.
Needs backup/ (info area for the trim values), so it only runs in the private workspace.
"""
import sys, struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ, UC_HOOK_CODE
from unicorn.arm_const import *
root, binpath, elf_syms = sys.argv[1], sys.argv[2], sys.argv[3]
max_ticks = int(sys.argv[4]); key_mask = int(sys.argv[5], 0)  # GPIO DIN bits forced low (keys pressed)
key_release = int(sys.argv[6]) if len(sys.argv) > 6 else 1 << 62
pulse_period = int(sys.argv[7]) if len(sys.argv) > 7 else 0
sys.path.insert(0, root + '/tools')
from svd_map import load
names = {a: nm for a, (nm, f) in load(root + '/vendor/FM33LC0XX_DFP/SVD/FM33LC0XX.SVD').items()}
syms = {}
for l in open(elf_syms):
    p = l.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
img = open(binpath, 'rb').read()
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0, 0x40000); uc.mem_write(0, img)
uc.mem_map(0x1FFFF000, 0x1000)
uc.mem_write(0x1FFFF000, open(root + '/backup/ut334a_info_1FFFFA00-1FFFFFFF.bin', 'rb').read().rjust(0x1000, b'\0'))
uc.mem_map(0x20000000, 0x8000); uc.mem_map(0x40000000, 0x20000); uc.mem_map(0xE000E000, 0x1000)
RET = 0x3FFF0; uc.mem_write(RET, b'\x00\xbf\x00\xbf')
log, state = [], {'stop': None, 'ticks': 0}
def flash_wr(u, acc, addr, size, val, ud):
    if (val & 0xFFFFFFFF) == 0x1234ABCD and (struct.unpack('<I', u.mem_read(0x40001014, 4))[0] & 1):
        state['erase'] = addr & ~0x1FF
def wr(u, acc, addr, size, val, ud):
    log.append((state['ticks'], names.get(addr, hex(addr)), val & 0xFFFFFFFF))
    if addr == 0x40000100 and (val & 3) == 2:
        state['stop'] = 'deepsleep'; u.emu_stop()
def rd(u, acc, addr, size, val, ud):
    if addr == 0x40001020:   # FLS_ISR: erase/program done
        if state.get('erase') is not None:
            u.mem_write(state['erase'], bytes([0xFF]) * 0x200); log.append((state['ticks'], f"ERASE page 0x{state['erase']:05X}", 0)); state['erase'] = None
        u.mem_write(addr, struct.pack('<I', 0x3))
    if addr == 0x40018C10:   # SPI1_ISR: TXBE, not busy
        u.mem_write(addr, struct.pack('<I', 0x2))
    if addr == 0x4001AC00:   # ADC_ISR: EOC
        u.mem_write(addr, struct.pack('<I', 0x1))
        cher = struct.unpack('<I', u.mem_read(0x4001AC14, 4))[0]
        u.mem_write(0x4001AC18, struct.pack('<I', 2640 if cher & (1 << 9) else 1500))   # ~3.9 V battery
    if addr == 0x40000218:
        u.mem_write(addr, struct.pack('<I', struct.unpack('<I', u.mem_read(addr, 4))[0] | 0x80))
    din = {0x40000C1C: 0xFFFF & ~(1 << 9), 0x40000C5C: 0xFFFF, 0x40000C9C: 0xFFFF, 0x40000CDC: 0xFFFF}
    if addr in din:
        port = (addr - 0x40000C1C) // 0x40
        mask = key_mask if state['ticks'] < key_release else 0
        u.mem_write(addr, struct.pack('<I', din[addr] & ~((mask >> (16 * port)) & 0xFFFF)))
def code(u, addr, size, ud):
    if u.mem_read(addr, 2) == b'\x30\xbf':   # WFI
        state['stop'] = 'wfi'; state['pc'] = addr + 2; u.emu_stop()
uc.hook_add(UC_HOOK_MEM_WRITE, wr, begin=0x40000000, end=0x4001FFFF)
uc.hook_add(UC_HOOK_MEM_READ, rd, begin=0x40000000, end=0x4001FFFF)
uc.hook_add(UC_HOOK_MEM_WRITE, flash_wr, begin=0x00000000, end=0x0003FFFF)
uc.hook_add(UC_HOOK_CODE, code)
uc.reg_write(UC_ARM_REG_SP, 0x20006000)
pc = syms['Reset_Handler']
REGS = [UC_ARM_REG_R0 + i for i in range(13)] + [UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_CPSR]
while True:
    state['stop'] = None
    try:
        uc.emu_start(pc | 1, RET, count=5_000_000)
    except Exception as e:
        if uc.reg_read(UC_ARM_REG_PC) == 0:   # display stub: NULL renderer callback -> treat as empty function
            pc = uc.reg_read(UC_ARM_REG_LR) & ~1; state['nullcalls'] = state.get('nullcalls', 0) + 1; continue
        print(f'error {e} lr 0x{uc.reg_read(UC_ARM_REG_LR):X} at pc 0x{uc.reg_read(UC_ARM_REG_PC):X}, tick {state["ticks"]}, last pc {pc:X}'); break
    if state['stop'] == 'wfi':
        ctx = [uc.reg_read(r) for r in REGS]
        uc.reg_write(UC_ARM_REG_SP, 0x20007F00); uc.reg_write(UC_ARM_REG_LR, RET | 1)
        uc.emu_start(syms['SysTick_Handler'] | 1, RET, count=200000)
        if pulse_period and state['ticks'] % pulse_period == 0 and 'COMPx_IRQHandler' in syms:
            uc.reg_write(UC_ARM_REG_SP, 0x20007F00); uc.reg_write(UC_ARM_REG_LR, RET | 1)
            uc.emu_start(syms['COMPx_IRQHandler'] | 1, RET, count=200000)
        for r, v in zip(REGS, ctx): uc.reg_write(r, v)
        state['ticks'] += 1; pc = state['pc']
        if state['ticks'] >= max_ticks: print(f'stopped after {max_ticks} ticks'); break
    else:
        print(f"null renderer calls skipped: {state.get('nullcalls', 0)}"); print(f"stop: {state['stop']} at tick {state['ticks']} (pc 0x{uc.reg_read(UC_ARM_REG_PC):X})"); break
prev = None
for t, n, v in log:
    if (n, v) != prev: print(f'{t:6d} W {n:16s} = 0x{v:08X}')
    prev = (n, v)
