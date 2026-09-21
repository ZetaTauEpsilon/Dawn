"""Read task-local content tags with the installed package reader's layout.

Key material is borrowed from the pinned mapped image, never printed or saved.
"""
from pathlib import Path
from functools import lru_cache
import os
import ctypes as C
import re
import struct as S
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

ROOT = Path(os.environ.get('DAWN_GAME_ROOT', Path(__file__).resolve().parents[2])).resolve()
OUT = ROOT / 'build/coo/native-tags'
u32 = lambda b, o: S.unpack_from('<I', b, o)[0]
u64 = lambda b, o: S.unpack_from('<Q', b, o)[0]

def borrowed_material():
    image = (ROOT / 'destiny2_unpacked.bin').read_bytes()
    hits = list(re.finditer(rb'\x0f\x10\x05....\x48\x8d\x64\x24\xf8\x48\x89\x2c\x24\x48\x8d\x2d....\xe9', image, re.S))
    assert len(hits) == 1, 'Package key-table signature is not unique'
    pos = hits[0].start()
    table = pos + 7 + S.unpack_from('<i', image, pos + 3)[0]
    token_hits = list(re.finditer(rb'\x0f\x10\x44\x24.\x41\xb8\x14\x00\x00\x00\x48\x8d\x15....\x0f\x10\x4c\x24.\x48\x8d\x8c\x24....', image, re.S))
    assert len(token_hits) == 1, 'Content-id token signature is not unique'
    token_load = token_hits[0].start()
    token = token_load + 18 + S.unpack_from('<i', image, token_load+14)[0]
    primary = bytes((image[token+i]+image[table+16+i]) & 255 for i in range(16))
    return primary, image[table:table+16], image[table+32:table+44]

_primary, _alternate, _nonce_base = borrowed_material()
_codec = C.CDLL(str(ROOT / 'bin/x64/oo2core_3_win64.dll'))
_decode = _codec.OodleLZ_Decompress
_decode.restype = C.c_int64
_decode.argtypes = [C.c_void_p,C.c_int64,C.c_void_p,C.c_int64,C.c_int,C.c_int,C.c_int64,C.c_void_p,C.c_int64,C.c_void_p,C.c_void_p,C.c_void_p,C.c_int64,C.c_int]

@lru_cache(maxsize=12)
def package(pid):
    candidates = list((ROOT/'packages').glob(f'*_{pid:04x}_*.pkg'))
    assert candidates, f'Missing package {pid:04X}'
    latest = max(candidates, key=lambda p: int(p.stem.rsplit('_', 1)[1]))
    blob = latest.read_bytes()
    assert S.unpack_from('<H', blob, 0)[0] == 38
    entries = u32(blob, 0xB4)
    table = u32(blob, 0x110) + 96
    return latest, blob, table, entries, table+entries*16+32

@lru_cache(maxsize=32)
def block(pid, index):
    latest, blob, table, count, block_table = package(pid)
    off,size,patch,flags = S.unpack_from('<IIHH', blob, block_table+48*index)
    auth_tag = blob[block_table+48*index+32:block_table+48*index+48]
    path = latest.with_name(latest.stem.rsplit('_', 1)[0]+f'_{patch}.pkg')
    with path.open('rb') as f:
        f.seek(off)
        raw = f.read(size)
    if flags & 2:
        nonce = bytearray(_nonce_base)
        nonce[0] ^= pid >> 8
        nonce[1] = 0xF9
        nonce[11] ^= pid & 255
        raw = AESGCM(_alternate if flags & 4 else _primary).decrypt(bytes(nonce), raw+auth_tag, None)
    if flags & 1:
        output = C.create_string_buffer(0x40000+64)
        source = C.create_string_buffer(raw)
        for size in range(0x40000, 0, -0x4000):
            result = _decode(source,len(raw),output,size,0,0,0,None,0,None,None,None,0,3)
            if result == size:
                return output.raw[:size]
        raise ValueError('Oodle decode failed')
    return raw

@lru_cache(maxsize=256)
def read(tag):
    if not 0x80800000 <= tag <= 0xFFFFFFFF:
        raise ValueError("Tag is outside the installed handle range")
    pid = (tag-0x80800000) >> 13
    index = tag & 0x1FFF
    latest, blob, table, count, _ = package(pid)
    assert index < count
    cls,_,placement = S.unpack_from('<IIQ', blob, table+16*index)
    block_index = placement & 0x3FFF
    offset = ((placement >> 14) & 0x3FFF) << 4
    size = placement >> 28
    output = bytearray()
    while len(output) < size:
        data = block(pid, block_index)
        if offset >= len(data):
            raise ValueError("Entry block offset is outside decoded data")
        output.extend(data[offset:offset+size-len(output)])
        block_index += 1
        offset = 0
    return cls, bytes(output)

def dump(tag):
    cls, data = read(tag)
    directory = OUT/'tags'
    directory.mkdir(parents=True, exist_ok=True)
    path = directory/f'{tag:08X}.{cls:08X}.bin'
    path.write_bytes(data)
    print(f'{tag:08X} class={cls:08X} bytes={len(data)}')
    return data

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tags', nargs='+', type=lambda value: int(value, 16))
    parser.add_argument('--out', type=Path, default=OUT)
    args = parser.parse_args()
    OUT = args.out
    for tag in args.tags:
        dump(tag)
