"""Run the Rad Pro UT334A build in Unicorn from reset and log peripheral writes.

Usage: fw_emu.py <workspace> <firmware.bin> <nm-output.txt> <max_ticks> <key_mask> [key_release_tick] [pulse_period_ticks] [screen.png]
  pulse_period_ticks: run COMPx_IRQHandler (a GM pulse) every N ticks.
  screen.png: decode the SPI1 bytes (CS PB3, DC PB8) as ST7789 commands and save the 240x320 frame
  (as the RAM holds it, and as the panel shows it with MADCTL/INVON applied).
  key_mask: GPIO DIN bits forced low, 16 bits per port from PA (e.g. 0x400 = PA10/OK held).
  EMU_USB=on[:off[:on...]]: USB powered (PA9 high) from tick on, unplugged at off, and so on. EMU_BAT=tick:raw: IN9 reads raw from that tick.
  EMU_UART_RX=tick:text: UART0 receives text from that tick ('|' = newline, several commands allowed);
  UART0 TX is printed at the end. After each tick the UART0 IRQ runs while TXBE_IE is set, then once for
  the next received byte (one byte per tick). After each newline it waits for the whole reply (CR LF).
  EMU_DUMP=sym[+offset],...: print 32-bit RAM values at the end (also as float).
  EMU_POKE=tick:sym[+offset]:mask:value,...: at that tick, set the masked bits of a 32-bit RAM word.
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
screen_png = sys.argv[8] if len(sys.argv) > 8 else None
# EMU_KEY2=start:end:mask presses keys again later; EMU_DEEPSLEEP=continue keeps running through DeepSleep
# (each WFI is still one SysTick), e.g. to power on again from the off state
import os
key2 = [int(x, 0) for x in os.environ['EMU_KEY2'].split(':')] if os.environ.get('EMU_KEY2') else None
# EMU_DEEPSLEEP=real: as continue, but SysTick does not run while PMU_CR.PMOD = 2 (as on the chip, where
# SysTick stops in DeepSleep); time still advances one tick per WFI so EMU_USB/EMU_KEY2 keep working
deepsleep_real = os.environ.get('EMU_DEEPSLEEP') == 'real'
deepsleep_continue = os.environ.get('EMU_DEEPSLEEP') == 'continue' or deepsleep_real
# EMU_USB=tick: USB powered (PA9 high) from that tick; EMU_BAT=tick:raw: battery ADC reads raw from that tick
# EMU_USB=on[:off[:on...]]: ticks at which USB power toggles, starting with plug-in
usb_edges = [int(x) for x in os.environ['EMU_USB'].split(':')] if os.environ.get('EMU_USB') else []
def usb_powered():
    return sum(1 for t in usb_edges if state['ticks'] >= t) % 2 == 1
bat_step = [int(x, 0) for x in os.environ['EMU_BAT'].split(':')] if os.environ.get('EMU_BAT') else None
uart_rx_at, _, uart_rx = os.environ.get('EMU_UART_RX', '').partition(':')
uart = {'rx': list(uart_rx.replace('|', chr(10)).encode()), 'rx_at': int(uart_rx_at or 0), 'rxbf': False, 'byte': 0,
        'tx': bytearray(), 'tx_ticks': [], 'await_from': None}
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
# FLS key state machine as measured on the bench: keys need the matching EREQ/PREQ; after an operation one
# KEY write of any value closes it; KEY = 0 in the protected state is a key error (lock until power-on reset)
fls = {'st': 'prot', 'k1': None, 'locks': 0, 'ops': 0}
def fls_lock(why):
    if fls['st'] != 'locked': fls['locks'] += 1; print(f"FLS KEY ERROR at tick {state['ticks']}: {why}")
    fls['st'] = 'locked'
def fls_key(u, v):
    if fls['st'] == 'locked': return
    if fls['st'] == 'done': fls['st'] = 'prot'; return
    epcr = struct.unpack('<I', u.mem_read(0x40001014, 4))[0]
    if fls['k1'] is None:
        if v == 0: fls_lock('KEY = 0 while protected') if fls['st'] == 'prot' else fls.update(st='prot')
        else: fls['k1'] = v
        return
    pair, fls['k1'] = (fls['k1'], v), None
    if pair == (0x96969696, 0xEAEAEAEA) and (epcr & 3) == 1: fls['st'] = 'erase'
    elif pair == (0xA5A5A5A5, 0xF1F1F1F1) and (epcr & 3) == 2: fls['st'] = 'program'
    else: fls_lock(f'key pair {pair[0]:08X}/{pair[1]:08X} with EPCR {epcr:X}')
def flash_wr(u, acc, addr, size, val, ud):
    if fls['st'] in ('erase', 'program'):
        fls['st'] = 'done'; fls['ops'] += 1
    elif fls['st'] != 'locked':
        fls_lock(f'flash write 0x{addr:05X} without keys')
    if (val & 0xFFFFFFFF) == 0x1234ABCD and (struct.unpack('<I', u.mem_read(0x40001014, 4))[0] & 1):
        state['erase'] = addr & ~0x1FF
spi = []   # (cs, dc, byte) for every SPI1_TXBUF write
spi_ticks = []
gpiob = {'do': 0}
def wr(u, acc, addr, size, val, ud):
    if addr == 0x40001018: fls_key(u, val & 0xFFFFFFFF)
    if addr == 0x40011C14:   # UART0_TXBUF
        uart['tx'].append(val & 0xFF); uart['tx_ticks'].append(state['ticks']); return
    if addr == 0x40018C14:
        spi.append(((gpiob['do'] >> 3) & 1, (gpiob['do'] >> 8) & 1, val & 0xFF)); spi_ticks.append(state['ticks']); return
    if addr == 0x40000C50: gpiob['do'] = val
    elif addr == 0x40000C54: gpiob['do'] |= val
    elif addr == 0x40000C58: gpiob['do'] &= ~val
    log.append((state['ticks'], names.get(addr, hex(addr)), val & 0xFFFFFFFF))
    if addr == 0x40000100: state['pmu_deep'] = (val & 3) == 2
    if addr == 0x40000100 and (val & 3) == 2:
        state['deepsleeps'] = state.get('deepsleeps', 0) + 1
        if not deepsleep_continue:
            state['stop'] = 'deepsleep'; u.emu_stop()
def rd(u, acc, addr, size, val, ud):
    if addr == 0x40001020:   # FLS_ISR: erase/program done
        if state.get('erase') is not None:
            u.mem_write(state['erase'], bytes([0xFF]) * 0x200); log.append((state['ticks'], f"ERASE page 0x{state['erase']:05X}", 0)); state['erase'] = None
        u.mem_write(addr, struct.pack('<I', 0x3))
    if addr == 0x40011C08:   # UART0_ISR: TXBE + TXSE always, RXBF while a byte waits
        u.mem_write(addr, struct.pack('<I', 0x3 | (0x100 if uart['rxbf'] else 0)))
    if addr == 0x40011C10:   # UART0_RXBUF
        u.mem_write(addr, struct.pack('<I', uart['byte'])); uart['rxbf'] = False
    if addr == 0x40018C10:   # SPI1_ISR: TXBE, not busy
        u.mem_write(addr, struct.pack('<I', 0x2))
    if addr == 0x4001AC00:   # ADC_ISR: EOC
        u.mem_write(addr, struct.pack('<I', 0x1))
        cher = struct.unpack('<I', u.mem_read(0x4001AC14, 4))[0]
        bat = bat_step[1] if bat_step and state['ticks'] >= bat_step[0] else 2640   # 2640: ~3.9 V battery
        u.mem_write(0x4001AC18, struct.pack('<I', bat if cher & (1 << 9) else 1500))
    if addr == 0x40000218:
        u.mem_write(addr, struct.pack('<I', struct.unpack('<I', u.mem_read(addr, 4))[0] | 0x80))
    din = {0x40000C1C: 0xFFFF & ~(1 << 9), 0x40000C5C: 0xFFFF, 0x40000C9C: 0xFFFF, 0x40000CDC: 0xFFFF}
    if addr in din:
        port = (addr - 0x40000C1C) // 0x40
        mask = key_mask if state['ticks'] < key_release else 0
        if key2 and key2[0] <= state['ticks'] < key2[1]: mask |= key2[2]
        value = din[addr] | (1 << 9 if port == 0 and usb_powered() else 0)
        u.mem_write(addr, struct.pack('<I', value & ~((mask >> (16 * port)) & 0xFFFF)))
def code(u, addr, size, ud):
    if u.mem_read(addr, 2) == b'\x30\xbf':   # WFI
        state['stop'] = 'wfi'; state['pc'] = addr + 2; u.emu_stop()
uc.hook_add(UC_HOOK_MEM_WRITE, wr, begin=0x40000000, end=0x4001FFFF)
uc.hook_add(UC_HOOK_MEM_READ, rd, begin=0x40000000, end=0x4001FFFF)
uc.hook_add(UC_HOOK_MEM_WRITE, flash_wr, begin=0x00000000, end=0x0003FFFF)
uc.hook_add(UC_HOOK_CODE, code)
uc.reg_write(UC_ARM_REG_SP, 0x20006000)
pc = syms['Reset_Handler']
# EMU_POKE=tick:sym[+offset]:mask:value,...: at that tick, set the masked bits of a 32-bit RAM word (e.g. a settings bitfield)
pokes = []
for pk in filter(None, os.environ.get('EMU_POKE', '').split(',')):
    t, target, m, v = pk.split(':'); sym, _, off = target.partition('+')
    pokes.append((int(t), syms[sym] + int(off or '0', 0), int(m, 0), int(v, 0)))
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
        if not (deepsleep_real and state.get('pmu_deep')):
            uc.emu_start(syms['SysTick_Handler'] | 1, RET, count=200000)
        else:
            state['ds_ticks'] = state.get('ds_ticks', 0) + 1
        if pulse_period and state['ticks'] % pulse_period == 0 and 'COMPx_IRQHandler' in syms:
            uc.reg_write(UC_ARM_REG_SP, 0x20007F00); uc.reg_write(UC_ARM_REG_LR, RET | 1)
            uc.emu_start(syms['COMPx_IRQHandler'] | 1, RET, count=200000)
        if 'UART0_IRQHandler' in syms:
            rx_done = False   # one received byte per tick; a pending transmission goes first
            for _ in range(256):
                ier = struct.unpack('<I', uc.mem_read(0x40011C04, 4))[0]
                if ier & 0x2:
                    pass
                elif uart['await_from'] is not None:
                    # A host waits for the whole reply line (CR LF) before sending the next command
                    if len(uart['tx']) > uart['await_from'] and uart['tx'].endswith(bytes([13, 10])):
                        uart['await_from'] = None
                    break
                elif (ier & 0x100) and uart['rx'] and state['ticks'] >= uart['rx_at'] and not rx_done:
                    uart['byte'] = uart['rx'].pop(0); uart['rxbf'] = True; rx_done = True
                    if uart['byte'] == 10:
                        uart['await_from'] = len(uart['tx'])
                else:
                    break
                uc.reg_write(UC_ARM_REG_SP, 0x20007F00); uc.reg_write(UC_ARM_REG_LR, RET | 1)
                uc.emu_start(syms['UART0_IRQHandler'] | 1, RET, count=200000)
        for r, v in zip(REGS, ctx): uc.reg_write(r, v)
        state['ticks'] += 1; pc = state['pc']
        for pk in pokes:
            if pk[0] == state['ticks']:
                old_v = struct.unpack('<I', uc.mem_read(pk[1], 4))[0]
                uc.mem_write(pk[1], struct.pack('<I', (old_v & ~pk[2]) | (pk[3] & pk[2])))
        if state['ticks'] >= max_ticks: print(f"stopped after {max_ticks} ticks, DeepSleep entries: {state.get('deepsleeps', 0)}, runs over 5 M instructions: {state.get('long_runs', 0)}, ticks without SysTick (DeepSleep): {state.get('ds_ticks', 0)}"); break
    elif state['stop'] is None and state.get('long_runs', 0) < 100:
        # 5 M instructions without a WFI (e.g. a full-screen image draw): carry on from here
        state['long_runs'] = state.get('long_runs', 0) + 1; pc = uc.reg_read(UC_ARM_REG_PC)
    else:
        print(f"null renderer calls skipped: {state.get('nullcalls', 0)}"); print(f"stop: {state['stop']} at tick {state['ticks']} (pc 0x{uc.reg_read(UC_ARM_REG_PC):X})"); break
prev = None
for t, n, v in log:
    if (n, v) != prev: print(f'{t:6d} W {n:16s} = 0x{v:08X}')
    prev = (n, v)

def st7789_frame(spi):
    """Minimal ST7789 model: CASET/RASET/RAMWR/MADCTL/INVON/INVOFF, RGB565, 240x320 RAM."""
    W, H = 240, 320
    fb = [[0] * W for _ in range(H)]
    cmd, args, madctl, inv = None, [], 0, False
    x0 = x1 = y0 = y1 = x = y = 0
    pix = []
    stats = {'bytes': 0, 'pixels': 0, 'out_of_window': 0, 'cs_high_bytes': 0}
    for cs, dc, b in spi:
        if cs:
            stats['cs_high_bytes'] += 1; continue
        stats['bytes'] += 1
        if dc == 0:
            cmd, args, pix = b, [], []
            if b == 0x21: inv = True
            elif b == 0x20: inv = False
            elif b == 0x2C: x, y = x0, y0
            continue
        if cmd == 0x2C:
            pix.append(b)
            if len(pix) == 2:
                c = (pix[0] << 8) | pix[1]; pix = []
                # MADCTL MV/MX/MY: logical (x, y) in the window -> RAM (col, row)
                col, row = (y, x) if madctl & 0x20 else (x, y)
                if madctl & 0x40: col = W - 1 - col
                if madctl & 0x80: row = H - 1 - row
                if 0 <= col < W and 0 <= row < H: fb[row][col] = c
                else: stats['out_of_window'] += 1
                stats['pixels'] += 1
                x += 1
                if x > x1: x = x0; y += 1
                if y > y1: y = y0
            continue
        args.append(b)
        if cmd == 0x2A and len(args) == 4: x0, x1 = (args[0] << 8) | args[1], (args[2] << 8) | args[3]
        if cmd == 0x2B and len(args) == 4: y0, y1 = (args[0] << 8) | args[1], (args[2] << 8) | args[3]
        if cmd == 0x36 and len(args) == 1: madctl = b
    return fb, inv, madctl, stats

print(f"FLS: {fls['ops']} erase/program operations, key-error locks: {fls['locks']}, final state {fls['st']}")
if os.environ.get('EMU_SPI_CMDS_FROM'):
    t0 = int(os.environ['EMU_SPI_CMDS_FROM'])
    cmds = [(t, b) for (cs, dc, b), t in zip(spi, spi_ticks) if t >= t0 and dc == 0]
    print('ST7789 commands from tick', t0, ':', ' '.join(f'{t}:{b:02X}' for t, b in cmds[:16]))
if screen_png:
    from PIL import Image
    fb, inv, madctl, stats = st7789_frame(spi)
    print(f"SPI: {stats}, MADCTL 0x{madctl:02X}, INVON {inv}")
    img = Image.new('RGB', (240, 320))
    for r, row in enumerate(fb):
        for c, v in enumerate(row):
            img.putpixel((c, r), (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
    img.save(screen_png)
    if inv:
        from PIL import ImageOps
        ImageOps.invert(img).save(screen_png.replace('.png', '_panel_inverted.png'))
    print(f"saved {screen_png}")
if uart['tx']:
    print(f"UART0 TX (ticks {uart['tx_ticks'][0]}..{uart['tx_ticks'][-1]}): {bytes(uart['tx'])!r}")
for name in filter(None, os.environ.get('EMU_DUMP', '').split(',')):   # EMU_DUMP=sym[+offset],...: 32-bit values
    sym, _, off = name.partition('+')
    addr = syms[sym] + int(off or '0', 0)
    raw = bytes(uc.mem_read(addr, 4)); print(f"{name} @0x{addr:08X} = 0x{struct.unpack('<I', raw)[0]:08X} ({struct.unpack('<f', raw)[0]:.4g} as float)")
