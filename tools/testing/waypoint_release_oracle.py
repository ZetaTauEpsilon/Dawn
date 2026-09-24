"""Offline oracle for the supplied 0.1.5.2 DLL. Never loads it into a process.

Only pure buffer transformations are emulated. Unsupported external calls fail.
The pinned hash prevents silently testing a different release.
"""
import argparse
import hashlib
import struct
from pathlib import Path
import capstone
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / 'build/waypoint-recovery-20260920/release.dll'
HASH = '88c75ac8f411215a87a6e62758f9dcc34fb4bab903ebdec074c2e08164acc7c8'
BASE = 0x180000000


class Oracle:
    def __init__(self):
        assert hashlib.sha256(RELEASE.read_bytes()).hexdigest() == HASH
        self.pe = pefile.PE(str(RELEASE))
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_64)
        self.cpu.mem_map(BASE, (self.pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
        for section in self.pe.sections:
            self.cpu.mem_write(BASE+section.VirtualAddress, section.get_data())
        self.cpu.mem_map(0x10000000, 0x1000000)
        self.cursor = 0x10000000
        self.stack = 0x10fff008
        self.stop = 0x10ffff00
        self.cpu.mem_write(self.stop, b'\xf4')
        self.cpu.hook_add(UC_HOOK_CODE, self._hook)

    def alloc(self, data):
        address = self.cursor
        self.cursor += (len(data)+15)&~15
        assert self.cursor < 0x10e00000
        self.cpu.mem_write(address, bytes(data))
        return address

    def _hook(self, cpu, address, size, user):
        rva = address-BASE
        if rva == 0x5e9560:  # stack-cookie check (no externally observable behavior)
            self._ret()
        if rva == 0x622f30:  # memcmp
            n = cpu.reg_read(UC_X86_REG_R8)
            a = bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RCX),n))
            b = bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RDX),n))
            cpu.reg_write(UC_X86_REG_RAX,((a>b)-(a<b)) & 0xffffffffffffffff)
            self._ret()
        # MSVC pure support routines; no Win32/game code is executed.
        if rva in (0x622af0, 0x623030):
            dst = cpu.reg_read(UC_X86_REG_RCX)
            src = cpu.reg_read(UC_X86_REG_RDX)
            count = cpu.reg_read(UC_X86_REG_R8)
            assert count < 0x100000
            data = bytes([src & 255])*count if rva == 0x622af0 else bytes(cpu.mem_read(src,count))
            cpu.mem_write(dst,data)
            cpu.reg_write(UC_X86_REG_RAX,dst)
            self._ret()

    def _ret(self):
        sp = self.cpu.reg_read(UC_X86_REG_RSP)
        address, = struct.unpack('<Q', self.cpu.mem_read(sp,8))
        self.cpu.reg_write(UC_X86_REG_RSP,sp+8)
        self.cpu.reg_write(UC_X86_REG_RIP,address)

    def call(self, rva, *args):
        cpu = self.cpu
        cpu.mem_write(self.stack, struct.pack('<Q',self.stop)+bytes(32))
        for reg in (UC_X86_REG_RAX,UC_X86_REG_RBX,UC_X86_REG_RSI,UC_X86_REG_RDI,UC_X86_REG_RBP,
                    UC_X86_REG_R12,UC_X86_REG_R13,UC_X86_REG_R14,UC_X86_REG_R15):
            cpu.reg_write(reg,0)
        for reg,value in zip((UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9),args):
            cpu.reg_write(reg,value)
        for n,value in enumerate(args[4:]):
            cpu.mem_write(self.stack+40+n*8,struct.pack('<Q',value&0xffffffffffffffff))
        cpu.reg_write(UC_X86_REG_RSP,self.stack)
        try:
            cpu.emu_start(BASE+rva,self.stop,count=3000000)
        except Exception as error:
            raise RuntimeError(f'Oracle stopped at {cpu.reg_read(UC_X86_REG_RIP):x}') from error
        assert cpu.reg_read(UC_X86_REG_RIP)==self.stop, 'instruction limit'
        return cpu.reg_read(UC_X86_REG_RAX)


def disasm(pe, rva):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    for e in pe.DIRECTORY_ENTRY_EXCEPTION:
        if e.struct.BeginAddress <= rva < e.struct.EndAddress:
            return list(md.disasm(pe.get_data(e.struct.BeginAddress,e.struct.EndAddress-e.struct.BeginAddress),BASE+e.struct.BeginAddress))
    raise ValueError(hex(rva))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--disasm',type=lambda x:int(x,16))
    parser.add_argument('--callers',type=lambda x:int(x,16))
    parser.add_argument('--refs',type=lambda x:int(x,16))
    args = parser.parse_args()
    pe = pefile.PE(str(RELEASE))
    if args.disasm is not None:
        for i in disasm(pe,args.disasm): print(f'{i.address-BASE:x}: {i.mnemonic} {i.op_str}')
    if args.callers is not None:
        needle = f'{BASE+args.callers:#x}'
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        for e in pe.DIRECTORY_ENTRY_EXCEPTION:
            start,end=e.struct.BeginAddress,e.struct.EndAddress
            for i in md.disasm(pe.get_data(start,end-start),BASE+start):
                if i.mnemonic in ('call','jmp') and i.op_str == needle:
                    print(f'{e.struct.BeginAddress:x}: {i.address-BASE:x} {i.mnemonic} {i.op_str}')
    if args.refs is not None:
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md.detail=True
        for e in pe.DIRECTORY_ENTRY_EXCEPTION:
            start,end=e.struct.BeginAddress,e.struct.EndAddress
            for i in md.disasm(pe.get_data(start,end-start),BASE+start):
                for op in i.operands:
                    if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                        if i.address+i.size+op.mem.disp == BASE+args.refs:
                            print(f'{start:x}: {i.address-BASE:x} {i.mnemonic} {i.op_str}')
