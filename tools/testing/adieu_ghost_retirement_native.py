"""Offline check of Adieu's deferred-object retirement, using captured native code.

Emulates the actual generation gate and retirement routine in isolated memory.
Entity lookup/generation and the final engine delete call use explicit fixtures;
the native comparisons, ownership check and weak-reference clearing run as-is.
Does not open or modify a game process.
"""
import argparse
import hashlib
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSI,
    UC_X86_REG_R14, UC_X86_REG_RSP, UC_X86_REG_RIP,
)

BASE = 0x140000000
MEM = 0x700000000000
ENTITY = 0x6DFAA201


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    gate = (args.capture / 'object-apply.bin').read_bytes()[0x94:0xA7]
    retire = (args.capture / 'object-retire.bin').read_bytes()[:0xA9]
    assert hashlib.sha256(gate).hexdigest() == 'f385625164a632bcc02ed06515786c61babba98031ebb5aff660c3e16e07a0ea'
    assert hashlib.sha256(retire).hexdigest() == 'fd8cd4dafb1639a79d6ecc162b6c9e8e2664fca5e88000f8ece52e38781aa569'
    # incoming generation, previous source generation, entity generation,
    # local authority, valid entity, expected delete, expected weak clearing.
    cases = (
        (2, 2, 2, True, True, False, False),  # captured defect: inactive, unchanged generation
        (3, 2, 2, True, True, True, True),    # corrected authority
        (3, 3, 2, True, False, False, False), # repeated retired publication
        (1, 2, 2, True, True, False, False),  # stale generation
        (3, 2, 3, True, True, False, False),  # never delete a newer entity
        (3, 2, 2, False, True, False, True), # native authority protects remote entity
        (3, 2, 2, True, False, False, False),# absent/expired weak reference
    )
    for incoming, previous, entity_generation, owned, valid, deleted, cleared in cases:
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        pages = set()

        def map_page(address):
            page = address & ~4095
            if page not in pages:
                uc.mem_map(page, 4096)
                pages.add(page)

        def put(address, fmt, *values):
            raw = struct.pack('<' + fmt, *values)
            for page in range(address & ~4095, (address + len(raw) + 4095) & ~4095, 4096):
                map_page(page)
            uc.mem_write(address, raw)

        def get(address, fmt):
            return struct.unpack('<' + fmt, uc.mem_read(address, struct.calcsize('<' + fmt)))[0]

        for rva, code in ((0x9F1A84, gate), (0x9EFAE0, retire)):
            map_page(BASE + rva)
            uc.mem_write(BASE + rva, code)
        for rva in (0x352310, 0x9F0BA0, 0x56A8F0):
            map_page(BASE + rva)
            uc.mem_write(BASE + rva, b'\xC3')
        source, incoming_state, stack, entities = MEM, MEM + 0x1000, MEM + 0x9000, MEM + 0x10000
        put(source + 0x180, 'I', previous)
        weak_pair = (ENTITY << 32) | 0xB7A395DB
        put(source + 0x440, 'Q', weak_pair)
        put(incoming_state, 'I', incoming)
        put(incoming_state + 8, 'B', 0)  # inactive in both failing and corrected cases
        put(BASE + 0x1F93428, 'QI', entities, 0xE0)
        put(entities + (ENTITY & 0x1FFF) * 0xE0 + 4, 'I', 0x271020)
        put(BASE + 0x26BE0E0 + 4 * ((ENTITY & 0x1FFF) >> 5), 'I', (1 << (ENTITY & 31)) if owned else 0)
        map_page(stack)
        map_page(stack - 0x1000)
        calls = []

        def hook(uc, address, size, user):
            if address == BASE + 0x9F1A97:
                uc.emu_stop()
            elif address == BASE + 0x352310:
                assert uc.reg_read(UC_X86_REG_RCX) == source + 0x440
                output = uc.reg_read(UC_X86_REG_RDX)
                put(output, 'I', ENTITY if valid else 0xFFFFFFFF)
                uc.reg_write(UC_X86_REG_RAX, output)
            elif address == BASE + 0x9F0BA0:
                assert uc.reg_read(UC_X86_REG_RCX) == ENTITY
                uc.reg_write(UC_X86_REG_RAX, entity_generation)
            elif address == BASE + 0x56A8F0:
                calls.append(uc.reg_read(UC_X86_REG_RCX))

        uc.hook_add(UC_HOOK_CODE, hook)
        uc.reg_write(UC_X86_REG_R14, source)
        uc.reg_write(UC_X86_REG_RSI, incoming_state)
        uc.reg_write(UC_X86_REG_RSP, stack)
        uc.emu_start(BASE + 0x9F1A84, BASE + 0x9F1A97, count=1000)
        assert uc.reg_read(UC_X86_REG_RIP) == BASE + 0x9F1A97
        assert calls == ([ENTITY] if deleted else []), (incoming, previous, calls)
        assert get(source + 0x440, 'Q') == (0xFFFFFFFFFFFFFFFF if cleared else weak_pair)
    print(f'PASS: {len(cases)} native Ghost retirement cases; same generation reproduces the leak, next generation deletes the owned entity')


if __name__ == '__main__':
    main()
