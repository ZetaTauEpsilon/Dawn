"""Offline native-code regression for Homecoming's network authority handoff.

Executes the pinned engine ownership setter and named-state validation in
Unicorn with synthetic resources. No game process is opened or modified.
Only the obfuscated global mapping accessor is substituted with a fixture;
stop at A1FCE0, before physics/presentation mutations requiring a real world.
"""
import argparse
import hashlib
from pathlib import Path
import struct

from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_RIP
from unicorn.x86_const import UC_X86_REG_RSI, UC_X86_REG_R12, UC_X86_REG_R14

BASE = 0x140000000
FIXTURE = 0x700000000000


def scheduler_checks(data):
    """Run the real scheduler tail after its native component callback.

    186AE60 forwards to the registered callback (DF7FF0 for devices). Its zero
    result reaches 594CF0 with disable=true; nonzero retains the active phase.
    Stub only that bit-update call, not the scheduler's return-value branch.
    """
    checks = 0
    for result in (0, 1, 0xA5):
        for forced in (False, True):
            uc = Uc(UC_ARCH_X86, UC_MODE_64)
            for offset in (0x56D000, 0x594000):
                uc.mem_map(BASE+offset, 0x1000)
                uc.mem_write(BASE+offset, data[offset:offset+0x1000])
            uc.mem_map(FIXTURE, 0x10000)
            stack, row, context = (FIXTURE+n for n in (0xF000, 0x100, 0x200))
            uc.mem_write(row, struct.pack('<I', 0x1234))
            uc.mem_write(row+0x14, struct.pack('<HH', 2, 4))
            uc.mem_write(stack+0x58, bytes([8 if forced else 0]))
            for register, value in ((UC_X86_REG_RSP, stack), (UC_X86_REG_RSI, row),
                                    (UC_X86_REG_R12, context), (UC_X86_REG_R14, 0),
                                    (UC_X86_REG_RAX, result)):
                uc.reg_write(register, value)
            disables = []

            def code(uc, address, size, user):
                if address != BASE+0x594CF0:
                    return
                rsp = uc.reg_read(UC_X86_REG_RSP)
                disables.append((uc.reg_read(UC_X86_REG_RCX), uc.reg_read(UC_X86_REG_RDX),
                                 uc.reg_read(UC_X86_REG_R8), uc.mem_read(rsp+0x28, 1)[0]))
                uc.reg_write(UC_X86_REG_RIP, struct.unpack('<Q', uc.mem_read(rsp, 8))[0])
                uc.reg_write(UC_X86_REG_RSP, rsp+8)

            uc.hook_add(UC_HOOK_CODE, code)
            uc.emu_start(BASE+0x56D3CF, BASE+0x56D410, count=100)
            kept = bool(result or forced)
            assert disables == ([] if kept else [(0x1234, 2, 4, 1)])
            assert uc.mem_read(context+0x60, 1)[0] == int(kept)
            checks += 2
    return checks


def allocator_checks(data):
    """Reproduce the captured null TLS+50 fault in the real 98D70 wrapper."""
    for present in (False, True):
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        uc.mem_map(BASE+0x98000, 0x1000)
        uc.mem_write(BASE+0x98000, data[0x98000:0x99000])
        uc.mem_map(BASE+0x20BB000, 0x1000)
        uc.mem_map(BASE+0x3CC7000, 0x1000)
        uc.mem_map(FIXTURE, 0x10000)
        def put(address, value):
            uc.mem_write(address, struct.pack('<Q', value))
        tls, service, vtable, tls_stub, method, stop, stack = (FIXTURE+n for n in (0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0xF008))
        put(BASE+0x3CC7890, tls_stub)  # Pinned TlsGetValue IAT.
        put(tls+0x50, service if present else 0)
        put(service, vtable)
        put(vtable+0x18, method)
        put(stack, stop)
        uc.reg_write(UC_X86_REG_RSP, stack)
        uc.reg_write(UC_X86_REG_RDX, FIXTURE+0x800)
        calls = []
        def code(uc, address, size, user):
            if address not in (tls_stub, method):
                return
            if address == method:
                calls.append((uc.reg_read(UC_X86_REG_RCX), uc.reg_read(UC_X86_REG_RDX)))
            rsp = uc.reg_read(UC_X86_REG_RSP)
            uc.reg_write(UC_X86_REG_RAX, tls if address == tls_stub else 0x1234)
            uc.reg_write(UC_X86_REG_RIP, struct.unpack('<Q', uc.mem_read(rsp, 8))[0])
            uc.reg_write(UC_X86_REG_RSP, rsp+8)
        uc.hook_add(UC_HOOK_CODE, code)
        try:
            uc.emu_start(BASE+0x98D70, stop, count=100)
        except UcError:
            assert not present and uc.reg_read(UC_X86_REG_RIP) == BASE+0x98D8C
            assert uc.reg_read(UC_X86_REG_RCX) == 0 and not calls
        else:
            assert present and calls == [(service, FIXTURE+0x800)]
            assert uc.reg_read(UC_X86_REG_RAX) == 0x1234
    return 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    args = parser.parse_args()
    data = args.image.read_bytes()
    assert hashlib.sha256(data).hexdigest() == '63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e'
    checks = allocator_checks(data) + scheduler_checks(data)
    for entity in (0x4EFAA046, 0x2CFAA045):
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        uc.mem_map(FIXTURE, 0x400000)
        mapped = set()

        def page(address):
            start = address & ~4095
            if start in mapped:
                return
            offset = start - BASE
            assert 0 <= offset <= len(data) - 4096
            uc.mem_map(start, 4096)
            uc.mem_write(start, data[offset:offset+4096])
            mapped.add(start)

        def unmapped(uc, access, address, size, value, user):
            for address in range(address & ~4095, (address+size+4095) & ~4095, 4096):
                page(address)
            return True

        def put(address, fmt, *values):
            if BASE <= address < BASE+len(data):
                page(address)
            uc.mem_write(address, struct.pack('<'+fmt, *values))

        def get(address, fmt):
            return struct.unpack('<'+fmt, uc.mem_read(address, struct.calcsize('<'+fmt)))[0]

        directory, registry, metadata = FIXTURE+0x10000, FIXTURE+0x11000, FIXTURE+0x12000
        mapping, pool, entities = FIXTURE+0x20000, FIXTURE+0x100000, FIXTURE+0x200000
        record_handle, provider_handle, bundle, tag = 2, 3, 4, 5
        record, provider_allocation, definition = pool+0x20000, pool+0x30000, pool+0x50000
        provider = provider_allocation-0x100
        child = provider+0x60
        put(BASE+0x2439C70, 'Q', directory)
        put(directory, 'QQI', registry, FIXTURE+0x13000, 0x40)
        put(directory+0x1C, 'H', 1)
        put(FIXTURE+0x13010, 'Q', FIXTURE+0x14000)
        put(FIXTURE+0x14000, 'I', 1)
        put(registry+8, 'QQ', pool, metadata)
        put(registry+0x30, 'Ii', 0x10000, -1)
        put(metadata+0x2C, 'I', 0)
        put(mapping+bundle*4, 'I', record_handle)
        put(record+0xC0, 'III', record_handle, 6, bundle)
        put(record+0x120, 'IHH', entity, 8, 17)
        put(record+0x98, 'B', 0xFF)
        put(provider_allocation+8, 'QI', 0x100, bundle)
        put(provider, 'IIQ', tag, 0x80808870, 0x128)
        put(provider+0x24, 'I', provider_handle)
        put(provider+0x2C, 'I', entity)
        put(provider+0x38, 'q', 0x18)
        put(child, 'IIQq', tag, 0x80808868, 0x1D0, -0x70)
        put(child+0x20, 'i', 1)
        put(definition+0x128+0x48, 'Qq', 1, 0x80)
        put(definition+0x128+0x80+0x70, 'I', 0x697C33EC)
        put(definition+0x1D0+0x18, 'Qq', 4, 0x40)
        for i, value in enumerate((0xBD855C4C, 0xBD855C4F, 0xBD855C4E, 0xBD855C49)):
            put(definition+0x1D0+0x40+0x30+i*8, 'I', value)
        put(BASE+0x1F93428, 'QI', entities, 0xE0)
        authority_word = BASE+0x26BE0E0+4*((entity & 0x1FFF) >> 5)
        authority_bit = 1 << (entity & 31)
        put(authority_word, 'I', authority_bit)  # Exactly the captured inconsistency.
        put(FIXTURE, 'II', 0x697C33EC, 0xBD855C4C)
        stop, stack = FIXTURE+0x1000, FIXTURE+0x8000
        reached = []

        def code(uc, address, size, user):
            if address == BASE+0x3F6EE0:
                # Supply the world-owned bundle map, not a permission result.
                rsp = uc.reg_read(UC_X86_REG_RSP)
                uc.reg_write(UC_X86_REG_RAX, mapping)
                uc.reg_write(UC_X86_REG_RIP, get(rsp, 'Q'))
                uc.reg_write(UC_X86_REG_RSP, rsp+8)
            if address == BASE+0xA1FCE0:
                reached.append((uc.reg_read(UC_X86_REG_RCX), uc.reg_read(UC_X86_REG_RDX)))
                uc.emu_stop()

        uc.hook_add(UC_HOOK_MEM_UNMAPPED, unmapped)
        uc.hook_add(UC_HOOK_CODE, code)

        def call(rva, rcx, rdx=0, r8=0):
            put(stack, 'Q', stop)
            uc.reg_write(UC_X86_REG_RSP, stack)
            uc.reg_write(UC_X86_REG_RCX, rcx)
            uc.reg_write(UC_X86_REG_RDX, rdx)
            uc.reg_write(UC_X86_REG_R8, r8)
            uc.emu_start(BASE+rva, stop, count=10000)
            assert reached or uc.reg_read(UC_X86_REG_RIP) == stop, 'instruction budget exhausted'
            return uc.reg_read(UC_X86_REG_RAX)

        assert call(0x3F7A20, FIXTURE+8, bundle) == FIXTURE+8
        assert get(FIXTURE+8, 'I') == record_handle
        assert call(0x3F3C00, bundle) & 255 == 0
        assert call(0xA1FBB0, provider, FIXTURE, FIXTURE+4) & 255 == 0 and not reached
        assert get(authority_word, 'I') == authority_bit
        checks += 5
        call(0x4DAD00, record, 1)
        assert get(record+0x124, 'H') == 9
        assert get(record+0x126, 'H') == 0
        assert get(record+0x98, 'B') == 0xBE
        assert get(authority_word, 'I') == authority_bit
        assert call(0x3F3C00, bundle) & 255 == 1
        checks += 5
        call(0xA1FBB0, provider, FIXTURE, FIXTURE+4)
        assert reached == [(child, 0)], reached
        assert get(child+0x20, 'i') == 1  # Stopped before native state mutation.
        checks += 2
    print(f'PASS: {checks} offline native authority/allocator/scheduler checks; null-service crash reproduced at 98D8C; gameplay passability still requires testing')


if __name__ == '__main__':
    main()
