"""Emulate Dawn's roster walk (client/content/scenarios/scenario_roster_groups.cpp) for one placed object.

Reproduces collect_descriptors -> follow_handle -> visit_slot_descriptors -> record_slot -> fill_slots so
that groups the live cache did not admit (Homecoming's plaza registry and cinematic owners) can be
catalogued with exactly the record the client walk would produce once they are admitted.
"""
import os
import struct
import sys

_tools = os.environ.get('DAWN_PACKAGE_TOOLS')
if _tools:
    sys.path.insert(0, _tools)
import package_read as packages  # noqa: E402
from extract_gateway_bindings import array, u32, i64  # noqa: E402

PLACED = 0x80809C36
INDIRECT = 0x80809468
REDIRECT = 0x80809B14
ABSENT = 0xFFFFFFFF


def is_class(v):
    return (v >> 16) == 0x8080


def is_schema(v):
    return v == ABSENT or is_class(v)


def descriptors(blob, own_tag, registry):
    out = []
    if len(blob) < 128:
        return out
    for base in range(0, len(blob) - 128 + 1, 4):
        if u32(blob, base) != own_tag or u32(blob, base + 8) != 0x70 or u32(blob, base + 48) != registry:
            continue
        component, sense, auth = u32(blob, base + 4), u32(blob, base + 68), u32(blob, base + 72)
        slot_type, slot_index = struct.unpack_from('<HH', blob, base + 52)
        if not is_class(component) or not is_schema(sense) or not is_schema(auth):
            continue
        out.append({'sourceTag': own_tag, 'sourceOffset': base, 'component': component, 'sense': sense,
                    'auth': auth, 'type': slot_type, 'index': slot_index})
    return out


def follow(handle, registry):
    tag = handle
    for _ in range(8):
        try:
            cls, blob = packages.read(tag)
        except Exception:
            return []
        if cls == PLACED:
            return descriptors(blob, tag, registry)
        if cls == REDIRECT:
            tag = u32(blob, 12)
            continue
        if cls == INDIRECT:
            handles = array(blob, 16, 4)
            if not handles:
                return []
            tag = u32(blob, handles[0])
            continue
        return []
    return []


def walk_object(object_tag):
    cls, blob = packages.read(object_tag)
    assert cls == 0x80809462, hex(cls)
    registry = u32(blob, 12)
    declared = [(u32(blob, o), u32(blob, o + 4)) for o in array(blob, 32, 8)]
    found = []
    overflow = False
    for bubble in array(blob, 56, 24):
        bubble_index = struct.unpack_from('<i', blob, bubble)[0]
        for h in array(blob, bubble + 8, 4):
            for d in follow(u32(blob, h), registry):
                d['bubble'] = bubble_index
                if d['type'] == 0 or d['type'] > 72 or d['index'] >= 1280:
                    overflow = True
                    continue
                prior = next((f for f in found if f['index'] == d['index']), None)
                if prior:
                    if any(prior[k] != d[k] for k in ('type', 'sourceTag', 'sourceOffset', 'component', 'sense', 'auth')):
                        overflow = True
                    continue
                found.append(d)
    found.sort(key=lambda d: d['index'])
    slots = []
    for d in found:
        assert d['index'] < len(declared) and declared[d['index']][0] == d['type'], (hex(object_tag), d)
        flags = (2 if d['auth'] != ABSENT else 0) | (1 if d['sense'] != ABSENT else 0)
        slots.append({'slotTypes': d['type'], 'slotFlags': flags, 'slotIndices': d['index'],
                      'descriptorTags': d['sourceTag'], 'descriptorOffsets': d['sourceOffset'],
                      'componentClasses': d['component'], 'senseSchemas': d['sense'], 'authSchemas': d['auth'],
                      'nameHash': declared[d['index']][1]})
    explicit = 0
    for bubble in array(blob, 56, 24):
        b = struct.unpack_from('<i', blob, bubble)[0]
        if 0 <= b < 64:
            explicit |= 1 << b
    return {'registry': registry, 'objectTag': object_tag, 'declared': len(declared), 'slots': slots,
            'overflow': overflow, 'explicitSliceMask': explicit}
