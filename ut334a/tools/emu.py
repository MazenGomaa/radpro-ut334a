"""Call functions of the stock firmware in an emulator (Unicorn, Cortex-M0 Thumb).

Peripheral space is mapped as plain zeroed RAM, so this is only meant for pure code
(soft-float helpers, conversion maths), never for anything that waits on hardware.

    from emu import Firmware
    fw = Firmware('backup/ut334a_stock_1.bin')
    r0, r1 = fw.call(0x1264, 0x3F800000)        # returns (r0, r1, flags)
"""
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UcError
from unicorn.arm_const import *

RET_MAGIC = 0x0003FFF0   # a BKPT-free spot at the end of flash used as the return address


class Firmware:
    def __init__(self, path):
        self.img = open(path, 'rb').read()
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.uc.mem_map(0x00000000, 0x40000)
        self.uc.mem_write(0, self.img)
        self.uc.mem_write(RET_MAGIC, b'\x00\xbf\x00\xbf')   # nop; nop
        self.uc.mem_map(0x1FFFF000, 0x1000)
        self.uc.mem_map(0x20000000, 0x8000)
        self.uc.mem_map(0x40000000, 0x20000)
        self.uc.mem_map(0xE000E000, 0x1000)
        # SysTick CTRL always reports COUNTFLAG, so delay_ms() loops finish immediately
        from unicorn import UC_HOOK_MEM_READ
        self.uc.hook_add(UC_HOOK_MEM_READ, lambda uc, acc, addr, size, val, ud:
                         uc.mem_write(0xE000E010, struct.pack('<I', 0x10001)),
                         begin=0xE000E010, end=0xE000E013)

    def w32(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xFFFFFFFF))

    def r32(self, a):
        return struct.unpack('<I', self.uc.mem_read(a, 4))[0]

    def wf(self, a, f):
        self.uc.mem_write(a, struct.pack('<f', f))

    def rf(self, a):
        return struct.unpack('<f', self.uc.mem_read(a, 4))[0]

    def call(self, addr, *args, max_insn=200000):
        regs = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
        for r, v in zip(regs, args):
            self.uc.reg_write(r, v & 0xFFFFFFFF)
        self.uc.reg_write(UC_ARM_REG_SP, 0x20005F00)
        self.uc.reg_write(UC_ARM_REG_LR, RET_MAGIC | 1)
        self.uc.emu_start(addr | 1, RET_MAGIC, count=max_insn)
        pc = self.uc.reg_read(UC_ARM_REG_PC)
        if pc != RET_MAGIC:
            raise RuntimeError(f'did not return (pc=0x{pc:X})')
        return (self.uc.reg_read(UC_ARM_REG_R0), self.uc.reg_read(UC_ARM_REG_R1),
                self.uc.reg_read(UC_ARM_REG_CPSR))


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xFFFFFFFF))[0]


def d2u(d):
    lo, hi = struct.unpack('<II', struct.pack('<d', d))
    return lo, hi


def u2d(lo, hi):
    return struct.unpack('<d', struct.pack('<II', lo & 0xFFFFFFFF, hi & 0xFFFFFFFF))[0]
