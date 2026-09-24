"""Read-only package checks for Adieu's rooftop and Ghost reunion repairs."""
import argparse
import os
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    args = parser.parse_args()
    os.environ['DAWN_GAME_ROOT'] = str(args.game_root.resolve())
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'coo'))
    import package_read as packages
    from extract_gateway_bindings import array, u32, i64, strings

    def graph(tag):
        kind, data = packages.read(tag)
        assert kind == 0x80809C36 and u32(data, 0x94) == 0x80806384
        return data

    def event_inputs(data):
        result = {}
        for event in array(data, 0xE8, 0xC0):
            definition = i64(data, event + 8)
            result[u32(data, definition + 0x10)] = [u32(data, p) for p in array(data, definition + 0x30, 4)]
        return result

    rooftop = graph(0x80FE0318)
    events = event_inputs(rooftop)
    assert events[0xDD6D986F] == [62] and events[0x5A9CAA14] == [68]
    for node, definition, signal in ((0x1EA0, 0xAFE8, 62), (0x60D0, 0xCCB8, 68)):
        assert struct.unpack_from('<IIq', rooftop, 0x90 + node) == (0x80FE0318, 0x80806278, definition)
        assert u32(rooftop, definition + 0x68) == signal

    finding = graph(0x80FE032A)
    assert struct.unpack_from('<IIq', finding, 0x90 + 0x18A0) == (0x80FE032A, 0x808062FE, 0x5558)
    assert u32(finding, 0x5558 + 0x58) == 0x80B3AAE0
    # Animation completion is independent of the final reunion voice action.
    assert struct.unpack_from('<IIq', finding, 0x90 + 0x2060) == (0x80FE032A, 0x808062F6, 0x5878)
    assert struct.unpack_from('<II', finding, 0x5878 + 0x78) == (0x0DD577ED, 0x80C2AB74)
    _, dialogue = packages.read(0x80C2AB74)
    final_voice = array(dialogue, 8, 8)[16]
    assert u32(dialogue, final_voice) == 0x0DD577ED
    assert round(struct.unpack_from('<f', dialogue, final_voice + 4)[0] * 1000) == 18186
    assert event_inputs(finding)[0xAFCD176B] == [39]
    assert u32(finding, 0x6008 + 0x68) == 39  # final parent wait, after the child
    for node, definition in ((0x1720, 0x54B8), (0x3460, 0x5F68), (0x37C0, 0x6098)):
        assert struct.unpack_from('<IIq', finding, 0x90 + node) == (0x80FE032A, 0x808062E4, definition)

    _, sequence = packages.read(0x80B5E04E)
    assert struct.unpack_from('<IHH', sequence, 0x658 + 0x30) == (0x8577EEB1, 5, 133)
    assert u32(sequence, 0x658 + 0x58) == 0x80C0E726
    assert not array(sequence, 0x658 + 0x60, 8)  # no parameter rows to override
    _, ghost = packages.read(0x80B5E288)
    assert struct.unpack_from('<IHH', ghost, 0x4C8 + 0x30) == (0x8577EEB1, 4, 118)
    for tag in (0x80B5E288, 0x80B5E28B):
        _, source = packages.read(tag)
        assert source[0x4C8 + 0x94] == 1  # deferred; retirement requires a newer generation
    for tag, slot in ((0x80B5E13E, 17), (0x80B5E147, 20)):
        _, effect = packages.read(tag)
        assert struct.unpack_from('<IHH', effect, 0xAC8 + 0x30) == (0x8577EEB1, 26, slot)
        assert u32(effect, 0xAC8 + 0x48) == 0x8080954B
    for tag, slot in ((0x80B5E09A, 246), (0x80B5E0A0, 248)):
        _, collection = packages.read(tag)
        assert struct.unpack_from('<IHH', collection, 0x388 + 0x30) == (0x8577EEB1, 34, slot)
        assert u32(collection, 0x388 + 0x48) == 0x8080956A
    # Pin the user's equipped variant, captured from the active character.
    # Other definitions share the exact same damaged-sidearm display name.
    _, items = packages.read(0x81327CCB)
    row = array(items, 8, 24)[2509]
    assert (u32(items, row), u32(items, row + 16)) == (53159281, 0x81319EAD)
    _, sidearm = packages.read(0x81319EAD)
    assert sidearm[184] == 0 and sidearm[187] == 1  # kinetic, instanced
    equipment = 16 + i64(sidearm, 16)
    assert sidearm[equipment + 24] == 7  # native kinetic equipment slot
    _, displays = packages.read(0x81613CF1)
    row = array(displays, 8, 24)[2509]
    assert (u32(displays, row), u32(displays, row + 16)) == (53159281, 0x8132973F)
    _, display = packages.read(0x8132973F)
    assert u32(display, 0x88) == 0xBEF711D7
    assert strings(0x81330B22)[0xBEF711D7] == "Traveler's Chosen (Damaged)"
    print('PASS: Adieu rooftop, Ghost reunion, healing/effect bindings and damaged starting sidearm')


if __name__ == '__main__':
    main()
