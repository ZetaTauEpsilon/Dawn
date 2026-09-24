"""Offline execution tests for the exact 1AU repair DLL; never loads the DLL.

Requires pefile and unicorn. Source-level tests additionally exercise complete
Controller life/update/spawn paths and the complete shared packet serializer.
"""
from pathlib import Path
import argparse
import hashlib
import itertools
import json
import struct

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

HERE = Path(__file__).resolve().parent
RECIPE = json.loads((HERE / 'candidate-patch.json').read_text())
CONTROL = 0x176A440
CONTROL_PAGE = CONTROL & ~0xFFF
CONTROL_SIZE = 0x31000
STACK = 0x70000000
SNAPSHOT = 0x30000000


def patch(data):
    assert len(data) == RECIPE['size'], 'Unexpected DLL size'
    assert hashlib.sha256(data).hexdigest() == RECIPE['originalSha256'], 'Wrong input DLL'
    out = bytearray(data)
    previous_end = 0
    for item in RECIPE['ranges']:
        offset = item['offset']
        before, after = bytes.fromhex(item['before']), bytes.fromhex(item['after'])
        assert offset >= previous_end and len(before) == len(after)
        assert data[offset:offset + len(before)] == before, item['description']
        out[offset:offset + len(after)] = after
        previous_end = offset + len(after)
    assert hashlib.sha256(out).hexdigest() == RECIPE['patchedSha256']
    return bytes(out)


class Machine:
    def __init__(self, data, code_page):
        self.mu = Uc(UC_ARCH_X86, UC_MODE_64)
        image = pefile.PE(data=data, fast_load=True)
        self.mu.mem_map(code_page, 0x2000)
        self.mu.mem_write(code_page, image.get_data(code_page, 0x2000))
        self.mu.mem_map(CONTROL_PAGE, CONTROL_SIZE)
        self.mu.mem_map(STACK, 0x2000)
        self.mu.mem_map(SNAPSHOT, 0x30000)
        self.mu.mem_map(0x542000, 0x1000)
        self.mu.mem_write(0x542250, b'\xc3')
        self.mu.hook_add(UC_HOOK_CODE, self.hook)
        self.stops = set()
        self.stopped = None

    def hook(self, mu, address, size, _):
        if address in self.stops:
            self.stopped = address
            mu.emu_stop()
        elif address == 0x542250:
            # Model only the unchanged pure checkpoint selector used by the
            # original death path. The patched death path never calls it.
            section = mu.reg_read(UC_X86_REG_RCX) & 0xFF
            if section >= 12: result = (12, 0, 0x45920385)
            elif section >= 9: result = (9, 0, 0x2EA8FB98)
            elif section >= 8: result = (8, 5, 0x782CAF4C)
            elif section >= 6: result = (6, 5, 0x82328D63)
            elif section >= 2: result = (2, 7, 0x4B27745D)
            else: result = (1, 8, 0x9C58857A)
            mu.reg_write(UC_X86_REG_RAX, result[0] | result[1] << 8 | result[2] << 32)

    def run(self, start, stops):
        self.stops, self.stopped = set(stops), None
        self.mu.reg_write(UC_X86_REG_RSP, STACK + 0x1000)
        self.mu.reg_write(UC_X86_REG_EFLAGS, 2)
        self.mu.emu_start(start, 0, count=1000)
        assert self.stopped in self.stops, 'Execution escaped the verified code region'
        return self.stopped


def life_tests(original, repaired):
    machines = [Machine(data, 0x556000) for data in (original, repaired)]
    count = 0
    for dead, restricted, phase, finished, cinematic, stale in itertools.product(
            (False, True), (False, True), range(5), (False, True), (0, 6, 7), (0, 1, 2)):
        fixture = bytearray(CONTROL_SIZE)
        def put(offset, fmt, value):
            struct.pack_into('<' + fmt, fixture, CONTROL - CONTROL_PAGE + offset, value)
        put(0x18, 'I', 0xA0002001)
        put(0x1480, 'Q', 91)
        put(0x1488, 'I', 257)
        put(0x26781, 'B', finished)
        put(0x26782, 'B', restricted)
        put(0x26785, 'B', 4)
        put(0x2EF60, 'B', cinematic)
        put(0x2EF88, 'B', phase)
        outputs = []
        for machine in machines:
            mu = machine.mu
            mu.mem_write(CONTROL_PAGE, bytes(fixture))
            mu.mem_write(STACK, bytes(0x2000))
            for register, value in ((UC_X86_REG_RAX, 100000), (UC_X86_REG_RSI, 90 if stale == 1 else 91),
                    (UC_X86_REG_RBX, 256 if stale == 2 else 257), (UC_X86_REG_RDI, 0xA0002001),
                    (UC_X86_REG_R12, 0), (UC_X86_REG_R14, int(dead))):
                mu.reg_write(register, value)
            machine.run(0x5560D2, {0x55627E})
            outputs.append(bytes(mu.mem_read(CONTROL_PAGE, CONTROL_SIZE)))
        if dead:
            assert outputs[1] == fixture, 'Death changed mission progress or recovery'
            if restricted and phase == 0 and not finished and cinematic == 6 and stale == 0:
                assert outputs[0][CONTROL - CONTROL_PAGE + 0x2EF88] == 1, 'Original wipe not reproduced'
        else:
            assert outputs[0] == outputs[1], 'Living-player handling changed'
        count += 1
    return count


def escape_tests(original, repaired):
    machines = [Machine(data, 0x54D000) for data in (original, repaired)]
    count = 0
    for start, now in itertools.product((0, 100, 50000), (0, 99, 100, 59999, 60000, 60001, 110000, 2**32)):
        elapsed = max(now - start, 0)
        for version, machine in enumerate(machines):
            mu = machine.mu
            fixture = bytearray(CONTROL_SIZE)
            base = CONTROL - CONTROL_PAGE
            struct.pack_into('<Q', fixture, base + 0x2EFB8, start)
            fixture[base + 0x26782] = 1  # Darkness presentation is retained.
            mu.mem_write(CONTROL_PAGE, bytes(fixture))
            mu.reg_write(UC_X86_REG_RBX, CONTROL)
            mu.reg_write(UC_X86_REG_R11, now)
            mu.reg_write(UC_X86_REG_R12, 0)
            stop = machine.run(0x54D354, {0x54D37C, 0x54D4A2})
            expected = 0x54D37C if version == 0 and elapsed >= 60000 else 0x54D4A2
            assert stop == expected, 'Escape timeout selected the wrong path'
            struct.pack_into('<Q', fixture, base + 0x2EFC0, elapsed)
            assert bytes(mu.mem_read(CONTROL_PAGE, CONTROL_SIZE)) == fixture, 'Timer changed unrelated state'
        count += 1
    return count


def respawn_tests(original, repaired):
    machines = [Machine(data, 0x272000) for data in (original, repaired)]
    count = 0
    for enabled, restricted, native_restricted, encoded in itertools.product((False, True), repeat=4):
        for version, machine in enumerate(machines):
            mu = machine.mu
            mu.mem_write(SNAPSHOT, bytes(0x30000))
            mu.mem_write(SNAPSHOT + 0x178C0, bytes([enabled, 0, restricted]))
            mu.mem_write(SNAPSHOT + 0x27500, bytes([native_restricted]))
            for reg, value in ((UC_X86_REG_RBP, SNAPSHOT), (UC_X86_REG_RBX, SNAPSHOT + 0x27500),
                    (UC_X86_REG_RCX, int(enabled)), (UC_X86_REG_RAX, int(encoded))):
                mu.reg_write(reg, value)
            stop = machine.run(0x272D0E, {0x272D37, 0x272D3C, 0x272D8C})
            if not enabled and not native_restricted:
                assert stop == 0x272D3C, 'Unrelated participation acquired a delay'
            elif not encoded:
                assert stop == 0x272D8C, 'Failed packet write was ignored'
            else:
                assert stop == 0x272D37 and mu.reg_read(UC_X86_REG_R8) & 0xFF == 16
                expected = 0x4200 if enabled and (version == 1 or not restricted) else 0x4F80
                assert mu.reg_read(UC_X86_REG_RDX) == expected, 'Wrong native respawn delay'
        count += 1
    return count


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('dll', type=Path)
    args = parser.parse_args()
    original = args.dll.read_bytes()
    repaired = patch(original)
    pe = pefile.PE(data=original, fast_load=True)
    for item in RECIPE['ranges']:
        assert pe.get_offset_from_rva(item['rva']) == item['offset']
    allowed = {offset for item in RECIPE['ranges']
        for offset in range(item['offset'], item['offset'] + len(bytes.fromhex(item['before'])))}
    assert all(a == b or index in allowed for index, (a, b) in enumerate(zip(original, repaired)))
    result = dict(playerLifeCases=life_tests(original, repaired), escapeTimerCases=escape_tests(original, repaired),
        respawnPolicyCases=respawn_tests(original, repaired), onlyThreeReviewedRangesChanged=True,
        originalSha256=RECIPE['originalSha256'], patchedSha256=RECIPE['patchedSha256'],
        inGamePlaytest=False)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
