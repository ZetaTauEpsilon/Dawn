"""Read-only package validation for the Homecoming ship-barrier handoff.

This proves authored identities and curve/state bindings, not in-game passability.
"""
import argparse
import importlib
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir', type=Path, required=True)
    parser.add_argument('--image', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir.resolve()))
    read = importlib.import_module('package_read').read
    array = importlib.import_module('extract_gateway_bindings').array
    checks = 0

    def expect(data, offset, fmt, expected, why):
        nonlocal checks
        actual = struct.unpack_from('<' + fmt, data, offset)
        assert actual == expected, (why, hex(offset), actual, expected)
        checks += 1

    _, graph = read(0x80F2769A)
    expect(graph, 0x90, 'IIq', (0x80F2769A, 0x808084E9, 0xD20), 'behavior root')
    expect(graph, 0x90+0xB0, 'Qq', (7, 0x628), 'action table')
    expect(graph, 0x90+0xC0, 'Qq', (1, 0x9F8), 'output table')
    expect(graph, 0x90+0x800, 'q', (0x200,), 'action 5 relative pointer')
    expect(graph, 0x90+0xA00, 'IIq', (0x80F2769A, 0x808093C4, 0x1788), 'opening curve')
    expect(graph, 0x90+0xAD0, 'IIq', (0x80F2769A, 0x808084DF, 0x1900), 'presentation output')
    expect(graph, 0x1788+0x24, 'ff', (3.5, 3.5), 'authored opening duration')
    # Native expression constants describe the final 1 + (-1) = 0 sample.
    expect(graph, 0x1850, 'ffff', (0., 0., -1., 1.), 'opening curve constants')
    expect(graph, 0x1738+0x30, 'I', (0x35,), 'opening named-state action flags')
    expect(graph, 0x1738+0x40, 'II', (0x697C33EC, 0xBD855C4C), 'opening named-state action')
    expect(graph, 0x1928, 'I', (0xBBC7B08A,), 'curve output variable')
    expect(graph, 0x1970, 'I', (0x6D408B83,), 'device position condition input')

    _, provider = read(0x80B9F367)
    expect(provider, 0x80, 'IIq', (0x80B9F367, 0x80808870, 0x128), 'real named-state provider')
    expect(provider, 0xB0, 'Qq', (1, 0x18), 'runtime named-state array')
    expect(provider, 0xE0, 'IIq', (0x80B9F367, 0x80808868, 0x1D0), 'runtime state child')
    expect(provider, 0xF0, 'q', (-0x70,), 'child resolves back to provider')
    expect(provider, 0x1E0, 'I', (0x697C33EC,), 'state name')
    states = [struct.unpack_from('<I', provider, o)[0] for o in array(provider, 0x1E8, 8)]
    assert states == [0xBD855C4C, 0xBD855C4F, 0xBD855C4E, 0xBD855C49], states
    checks += 1
    _, physics = read(0x80F27699)
    expect(physics, 0x7A0, 'IIII', (0x5266EA90, 0xBD855C4F, 0x51E7A18D, 0x1A8FEB14), 'physics enabled in blocking state')
    _, data = read(0x80B9F35D)
    outputs = array(data, 0x8B0, 12)
    assert len(outputs) == 2
    expect(data, outputs[0], 'III', (0x811C9DC5, 0x6D408B83, 0), 'position output maps input zero')
    expect(data, outputs[1], 'III', (0x811C9DC5, 0xBBC7B08A, 1), 'presentation output maps curve input one')
    if args.image:
        image = args.image.read_bytes()
        assert image[0xA1FBB0:0xA1FBB0+16] == bytes.fromhex('40 53 48 83 EC 20 8B 02 49 8B D8 48 8D 54 24 38')
        assert image[0x576420:0x576420+16] == bytes.fromhex('48 89 5C 24 08 48 89 74 24 10 55 57 41 56 48 8D')
        checks += 2
        # Pin the complete narrow ownership setter, its bookkeeping callees,
        # bundle lookup and the permission gate used by the named-state setter.
        for rva, size, expected in (
            (0x4DAD00, 110, 0x29C57CCCC97ACD14), (0x403BD0, 47, 0x5F813B6AEE56C054),
            (0x3FBB30, 8, 0xBCBA18632C231B55), (0x3F7A20, 75, 0xE0452FD7E4EF6403),
            (0x3F3C00, 133, 0xABFE9B8F63B5D9FE),
        ):
            actual = 14695981039346656037
            for byte in image[rva:rva+size]:
                actual = ((actual ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
            assert actual == expected, ('network authority binding', hex(rva), hex(actual))
            checks += 1
    print(f'PASS: {checks} Homecoming barrier package/native binding checks; gameplay acceptance pending')


if __name__ == '__main__':
    main()
