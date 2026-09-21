"""Read-only snapshots of Homecoming's two exact post-scan barrier entities."""
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
    args = parser.parse_args()
    exe = Path('C:/Destiny 2 Development/destiny2.exe')
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == EXPECTED_EXE_SHA256
    live = LiveImage(args.pid, exe)

    def read(address, size):
        if address < 65536 or not 0 < size <= 1048576:
            raise ValueError('read bounds')
        buf, got = c.create_string_buffer(size), c.c_size_t()
        if not live.kernel.ReadProcessMemory(live.handle, address, buf, size, c.byref(got)) or got.value != size:
            raise ValueError('unreadable')
        return buf.raw

    def val(address, fmt):
        return struct.unpack('<'+fmt, read(address, struct.calcsize('<'+fmt)))[0]

    try:
        registry = val(val(live.base+0x2439C70, 'Q'), 'Q')

        def allocation(handle):
            if handle == 0xffffffff:
                raise ValueError('invalid handle')
            shifted = (c.c_int32(handle).value >> 13) & 0xffffffff
            index = ((shifted | 0xffc0000) >> 18) & (shifted & 0xffff)
            row = registry+index*0x40
            stride = val(row+0x30, 'i')
            if not 0 < stride <= 1048576:
                raise ValueError('stride')
            return val(row+8, 'Q')+(handle & 0x1fff)*stride, val(row+0x34, 'i')

        def resolve(handle):
            element, mask = allocation(handle)
            return (element-(val(element+8, 'Q') & (mask & 0xffffffffffffffff))) & 0xffffffffffffffff

        def components(bundle, entity):
            seen, emitted = set(), set()
            while bundle != 0xffffffff and len(seen) < 64:
                if bundle in seen:
                    raise ValueError('bundle cycle')
                seen.add(bundle)
                alloc, _ = allocation(bundle)
                address = resolve(bundle)
                if not val(address, 'I') & 2:
                    metadata = resolve(val(address+4, 'I'))
                    count = val(metadata+0x68, 'Q')
                    if count > 1024:
                        raise ValueError('component count')
                    rows = metadata+0x80+val(metadata+0x70, 'q')
                    for j in range(count):
                        offset = val(rows+j*24+0x14, 'i')
                        if not 0 <= offset <= 0x400000:
                            raise ValueError('component offset')
                        part = address+offset
                        if part not in emitted and val(part+0x2c, 'I') == entity and resolve(val(part+0x24, 'I')) == part:
                            emitted.add(part)
                            yield part
                bundle = val(alloc+0x18, 'I')

        table, stride = val(live.base+0x1F93428, 'Q'), val(live.base+0x1F93430, 'I')
        assert stride == 0xE0
        results = []
        for slot in range(8192):
            row = table+slot*stride
            identity = read(row, 0x98)
            entity = struct.unpack_from('<I', identity, 0xc)[0]
            definition, record, guid = struct.unpack_from('<IIQ', identity, 0x88)
            if entity == 0xffffffff or entity & 0x1fff != slot or definition != 0x80C3B4B7:
                continue
            if (record, guid) not in ((25, 0xD8FFC2F1F8979C65), (26, 0x1EB7FCA5F866CD3A)):
                continue
            bundle = val(row+0x4c, 'I')
            result = {'slot': record+31, 'entity': hex(entity), 'bundle': hex(bundle),
                      'owned': bool(val(live.base+0x26BE0E0+4*((entity & 0x1fff) >> 5), 'I') & (1 << (entity & 31))), 'components': []}
            for address in components(bundle, entity):
                tag, kind, offset = struct.unpack('<IIQ', read(address, 16))
                if tag not in (0x80F2769A, 0x80B9F367, 0x80C22C67, 0x80F27699):
                    continue
                item = {'address': hex(address), 'tag': hex(tag), 'kind': hex(kind), 'offset': hex(offset)}
                if tag == 0x80C22C67 and kind == 0x80803910:
                    item.update(position=val(address+0x370, 'f'), target=val(address+0x37c, 'f'),
                                revision=val(address+0x960, 'i'), revisions=list(struct.unpack('<6i', read(address+0x950, 24))))
                elif tag == 0x80B9F367 and kind == 0x80808870:
                    item['state'] = val(address+0x80, 'i')
                elif tag == 0x80F2769A and kind == 0x808084E9:
                    item.update(duration=val(address+0xA24, 'f'), remaining=val(address+0xA28, 'Q'),
                                phase=val(address+0xA30, 'B'), output=list(struct.unpack('<4f', read(address+0xAF0, 16))),
                                writers=val(address+0xB00, 'i'))
                result['components'].append(item)
            result['stable_identity'] = read(row, 0x98) == identity
            results.append(result)
        output = {'capturedAt': datetime.now(timezone.utc).isoformat(), 'pid': args.pid, 'readOnly': True,
                  'base': hex(live.base), 'barriers': results}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(output, indent=2))
        print(json.dumps(output, indent=2))
    finally:
        live.close()


if __name__ == '__main__':
    main()
