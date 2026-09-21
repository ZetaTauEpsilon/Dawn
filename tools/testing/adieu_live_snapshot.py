"""Read-only Adieu native entity/source snapshot. Never writes process memory."""
import argparse
import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 're'))
from verify_camera_modes import LiveImage, EXPECTED_EXE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--component-start', type=lambda s: int(s, 0))
    parser.add_argument('--component-bytes', type=lambda s: int(s, 0), default=0x100000)
    args = parser.parse_args()
    exe = Path('C:/Destiny 2 Development/destiny2.exe')
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == EXPECTED_EXE_SHA256
    live = LiveImage(args.pid, exe)
    args.output.mkdir(parents=True, exist_ok=True)

    def read(address, size):
        if address < 65536 or not 0 < size <= 1048576:
            raise ValueError('read bounds')
        buf = c.create_string_buffer(size)
        got = c.c_size_t()
        if not live.kernel.ReadProcessMemory(live.handle, address, buf, size, c.byref(got)) or got.value != size:
            raise ValueError('unreadable')
        return buf.raw

    def val(address, fmt):
        return struct.unpack('<' + fmt, read(address, struct.calcsize('<' + fmt)))[0]

    try:
        directory = val(live.base + 0x2439C70, 'Q')
        registry = val(directory, 'Q')

        def resolve(handle, kind=0, offset=0):
            if handle == 0xffffffff:
                raise ValueError('absent')
            shifted = (c.c_int32(handle).value >> 13) & 0xffffffff
            index = ((shifted | 0xffc0000) >> 18) & (shifted & 0xffff)
            row = read(registry + index * 0x40, 0x38)
            stride, mask = struct.unpack_from('<ii', row, 0x30)
            if not 0 < stride <= 1048576:
                raise ValueError('stride')
            element = struct.unpack_from('<Q', row, 8)[0] + (handle & 0x1fff) * stride
            return ((element - (val(element + 8, 'Q') & (mask & 0xffffffffffffffff))) & 0xffffffffffffffff) + offset

        def weak(serial, handle):
            try:
                stride = val(directory + 0x10, 'i')
                if not 0 < stride <= 0x1000:
                    return False
                index = (((c.c_int32(handle).value >> 31) & 0x3c00) | 0x3ff) & (handle >> 13) & 0xffff
                metadata = val(registry + index * stride + 0x10, 'Q')
                head, elements = struct.unpack('<QQ', read(metadata, 16))
                offset, width = struct.unpack('<II', read(metadata + 0x1c, 8))
                return ((handle & 0x1fff) < val(head + 0x1c, 'H') and 0 < width <= 1048576
                        and val(elements + offset + (handle & 0x1fff) * width, 'I') == serial)
            except ValueError:
                return False

        def components(bundle, entity):
            seen = set()
            emitted = set()
            while bundle != 0xffffffff and len(seen) < 64:
                if bundle in seen:
                    raise ValueError('bundle cycle')
                seen.add(bundle)
                shifted = (c.c_int32(bundle).value >> 13) & 0xffffffff
                index = ((shifted | 0xffc0000) >> 18) & (shifted & 0xffff)
                allocation = val(registry + index * 0x40 + 8, 'Q') + (bundle & 0x1fff) * val(registry + index * 0x40 + 0x30, 'i')
                address = resolve(bundle)
                if not val(address, 'I') & 2:
                    metadata = resolve(val(address + 4, 'I'))
                    count = val(metadata + 0x68, 'Q')
                    if count > 1024:
                        raise ValueError('component count')
                    rows = metadata + 0x80 + val(metadata + 0x70, 'q')
                    for j in range(count):
                        offset = val(rows + j * 24 + 0x14, 'i')
                        if not 0 <= offset <= 0x400000:
                            raise ValueError('component offset')
                        component = address + offset
                        if component not in emitted and val(component + 0x2c, 'I') == entity and resolve(val(component + 0x24, 'I')) == component:
                            emitted.add(component)
                            yield component
                bundle = val(allocation + 0x18, 'I')

        entities = val(live.base + 0x1F93428, 'Q')
        stride = val(live.base + 0x1F93430, 'I')
        assert 0x80 <= stride <= 0x10000
        results = []
        seen = set()
        active = 0
        targets = {(4, 118), (4, 119), (1, 90), (1, 91), (43, 101), (43, 102), (5, 133)}
        if args.component_start:
            assert 0 < args.component_bytes <= 0x1000000
            for page in range(args.component_start, args.component_start + args.component_bytes, 0x1000):
                try:
                    data = read(page, 0x1000)
                except ValueError:
                    continue
                for offset in range(0, len(data) - 16, 8):
                    header = struct.unpack_from('<IIq', data, offset)
                    if header[1] not in (0x80809928, 0x8080948F, 0x80806266):
                        continue
                    address = page + offset
                    try:
                        scope = struct.unpack('<IHH', read(resolve(*header) + 0x30, 8))
                        if scope[0] != 0x8577EEB1 or scope[1:] not in targets:
                            continue
                        result = {'address': hex(address), 'scope': [hex(scope[0]), *scope[1:]],
                                  'definition': [hex(v) for v in header]}
                        if scope[1] == 4:
                            serial, handle = struct.unpack('<II', read(address + 0x440, 8))
                            result.update(generation=val(address + 0x180, 'I'), active=val(address + 0x188, 'B'),
                                          committed_generation=val(address + 0x2f0, 'I'), entity_handle=hex(handle),
                                          entity_serial=hex(serial), entity_valid=weak(serial, handle))
                            if result['entity_valid']:
                                entity_row = entities + (handle & 0x1fff) * stride
                                result['entity_flags'] = hex(val(entity_row + 4, 'I'))
                                result['entity_locally_owned'] = bool(val(live.base + 0x26BE0E0 + 4 * ((handle & 0x1fff) >> 5), 'I') & (1 << (handle & 31)))
                                result['entity_components'] = [{'address': hex(a), 'definition': [hex(v) for v in struct.unpack('<IIq', read(a, 16))]}
                                                              for a in components(val(entity_row + 0x4c, 'I'), handle)]
                        elif scope[1] == 43:
                            serial, handle = struct.unpack('<II', read(address + 0x2e8, 8))
                            result.update(generation=val(address + 0x254, 'I'), complete=val(address + 0x258, 'B'),
                                          selector=hex(handle), selector_valid=weak(serial, handle))
                        else:
                            result.update(generation=val(address + 0x1fc, 'I'), sense_generation=val(address + 0x244, 'I'))
                        raw = read(address, 0x480 if scope[1] == 4 else 0x310)
                        result['stable_header'] = read(address, 16) == raw[:16]
                        (args.output / f'sensor-{scope[1]}-{scope[2]}.bin').write_bytes(raw)
                        results.append(result)
                    except (ValueError, AssertionError):
                        continue
        for i in range(8192):
            row = entities + i * stride
            try:
                entity = val(row + 0xc, 'I')
                if entity == 0xffffffff or entity & 0x1fff != i or val(row + 4, 'I') & 5:
                    continue
                active += 1
                for address in components(val(row + 0x4c, 'I'), entity):
                    if address in seen:
                        continue
                    seen.add(address)
                    header = struct.unpack('<IIq', read(address, 16))
                    if header[1] not in (0x80809928, 0x8080948F, 0x80806266, 0x8080626D):
                        continue
                    definition = resolve(*header)
                    scope = struct.unpack('<IHH', read(definition + 0x30, 8))
                    if scope[0] != 0x8577EEB1 or scope[1:] not in targets:
                        continue
                    result = {'address': hex(address), 'scope': [hex(scope[0]), *scope[1:]],
                              'definition': [hex(v) for v in header], 'owner_entity': hex(entity)}
                    if scope[1] == 4:
                        serial, handle = struct.unpack('<II', read(address + 0x440, 8))
                        result.update(generation=val(address + 0x180, 'I'), active=val(address + 0x188, 'B'),
                                      committed_generation=val(address + 0x2f0, 'I'),
                                      entity_handle=hex(handle), entity_serial=hex(serial), entity_valid=weak(serial, handle))
                        if result['entity_valid']:
                            result['entity_row'] = hex(entities + (handle & 0x1fff) * stride)
                            result['entity_flags'] = hex(val(entities + (handle & 0x1fff) * stride + 4, 'I'))
                            result['entity_components'] = [
                                {'address': hex(a), 'definition': [hex(v) for v in struct.unpack('<IIq', read(a, 16))]}
                                for a in components(val(entities + (handle & 0x1fff) * stride + 0x4c, 'I'), handle)]
                        size = 0x480
                    elif scope[1] == 43:
                        serial, handle = struct.unpack('<II', read(address + 0x2e8, 8))
                        result.update(generation=val(address + 0x254, 'I'), complete=val(address + 0x258, 'B'),
                                      selector=hex(handle), selector_valid=weak(serial, handle))
                        size = 0x310
                    else:
                        size = 0x280
                    raw = read(address, size)
                    result['stable_header'] = read(address, 16) == raw[:16]
                    (args.output / f'sensor-{scope[1]}-{scope[2]}.bin').write_bytes(raw)
                    results.append(result)
            except (ValueError, AssertionError):
                continue
        output = {'pid': args.pid, 'utc': datetime.now(timezone.utc).isoformat(), 'base': hex(live.base),
                  'active_entities_scanned': active, 'components_scanned': len(seen), 'sensors': results}
        (args.output / 'snapshot.json').write_text(json.dumps(output, indent=2))
        summary = dict(output)
        summary['sensors'] = [{k: v for k, v in row.items() if k != 'entity_components'} for row in results]
        print(json.dumps(summary, indent=2))
    finally:
        live.close()


if __name__ == '__main__':
    main()
