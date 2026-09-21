"""Recover Homecoming (mission_towerfall) native bindings from the installed packages.

Read-only: the installed game, packages and the current build_data cache are only read.
The output feeds tools/vanilla/generate_homecoming.py, which writes the compiled catalog.

Run from a workspace whose root holds destiny2_unpacked.bin, packages/ and Dawn/cache, or point
DAWN_PACKAGE_TOOLS at a tools/coo directory whose parent workspace does.
"""
import hashlib
import json
import os
import re
import struct
import sys
from pathlib import Path

_tools = os.environ.get('DAWN_PACKAGE_TOOLS')
if _tools:
    sys.path.insert(0, _tools)
import package_read as packages  # noqa: E402
from extract_gateway_bindings import array, strings, u32, i64  # noqa: E402
from extract_deadly_trial_bindings import walk  # noqa: E402
import towerfall_walk_emulation as emulation  # noqa: E402


def sources(groups):
    """Type-1 squad sources: categories, per-variant candidate entries and the authored spawn rule.

    Unlike the Gateway helper this tolerates members whose candidate does not resolve to an
    actor definition (Homecoming's scene-only invisible target has no exact actor class); such
    entries are kept with ``unresolved`` set so the generator can exclude the source.
    """
    out = []
    for group in groups:
        for slot in group['slots']:
            if slot['slotTypes'] != 1 or slot['descriptorTags'] == 0xFFFFFFFF:
                continue
            tag = slot['descriptorTags']
            _, b = packages.read(tag)
            base = slot['descriptorOffsets'] + 0x68
            assert u32(b, base) == tag and u32(b, base + 4) == 0x80807EB9, (hex(tag), hex(base))
            cats = []
            unresolved = 0
            for c in array(b, base + 64, 104, 0x80808356):
                selections = []
                for i in range(6):
                    entries = []
                    for e in array(b, c + 8 + 16 * i, 24, 0x80808358):
                        at = e + i64(b, e)
                        entry = {'nameHash': u32(b, e + 8), 'weight': u32(b, e + 12), 'offset': at}
                        if 4 <= at <= len(b) - 4 and u32(b, at - 4) == 0x808099D8:
                            entry['entity'] = u32(b, at)
                        else:
                            entry['entity'] = None
                            unresolved += 1
                        entries.append(entry)
                    selections.append(entries)
                cats.append({'category': u32(b, c), 'selections': selections})
            rule_registry, rule_packed = struct.unpack_from('<II', b, base + 56)
            out.append({'registry': group['registry'], 'slot': slot['slotIndices'], 'tag': tag,
                        'offset': slot['descriptorOffsets'], 'name': slot['name'], 'categories': cats,
                        'ruleRegistry': rule_registry, 'ruleType': rule_packed & 255, 'ruleSlot': rule_packed >> 16,
                        'unresolved': unresolved})
    return out

OUT = packages.ROOT / 'build/coo/towerfall-research'
SCENARIO = 0x80B500BC
PACKAGE = 'mission_towerfall'
BANK = 0x80C2AF61
DIRECTIVES = 0x80B508FE
TAG_RANGE = (0x80B50000, 0x80B51800)
# Version-53 RosterGroupRecord layout (tools/coo/cache_layout.cpp). Verified below against the
# live cache by locating every walked (registry, object) pair at a consistent record stride.
RECORD_SIZE = 30730
FIELDS = {'slotTypes': (10, 1280), 'slotFlags': (1290, 1280), 'slotIndices': (2570, 2560),
          'descriptorTags': (5130, 5120), 'descriptorOffsets': (10250, 5120),
          'componentClasses': (15370, 5120), 'senseSchemas': (20490, 5120), 'authSchemas': (25610, 5120)}


def cache_groups(walked):
    cache = (packages.ROOT / 'Dawn/cache/build_data.bin').read_bytes()
    version = u32(cache, 8)
    wanted = {}
    for region in walked['regions']:
        for obj in region['objects']:
            wanted.setdefault((obj['registry'], obj['tag']), []).append((region['bubble'], region['state'], obj['array']))
    found = {}
    for (registry, tag) in wanted:
        needle = struct.pack('<II', registry, tag)
        at = cache.find(needle)
        hits = []
        while at != -1:
            hits.append(at)
            at = cache.find(needle, at + 1)
        found[(registry, tag)] = hits
    offsets = sorted(o for hits in found.values() for o in hits)
    assert offsets, 'no roster group records located'
    base = offsets[0]
    # Every record must sit at the same stride from the first; anything else is a false hit.
    aligned = {o for o in offsets if (o - base) % RECORD_SIZE == 0}
    groups = []
    for (registry, tag), hits in found.items():
        rows = [o for o in hits if o in aligned]
        if not rows:
            continue
        assert len(rows) == 1, (hex(registry), hex(tag), rows)
        start = rows[0]
        data = cache[start:start + RECORD_SIZE]
        count = struct.unpack_from('<H', data, 8)[0]
        assert count <= 1280, (hex(registry), count)
        slots = []
        for j in range(count):
            slot = {}
            for name, (offset, size) in FIELDS.items():
                stride = size // 1280
                slot[name] = int.from_bytes(data[offset + j * stride:offset + (j + 1) * stride], 'little')
            slot['name'] = ''
            if slot['descriptorTags'] != 0xFFFFFFFF:
                _, blob = packages.read(slot['descriptorTags'])
                off = slot['descriptorOffsets']
                assert off + 56 <= len(blob), (hex(registry), j)
                assert u32(blob, off + 48) == registry and struct.unpack_from('<H', blob, off + 54)[0] == slot['slotIndices'], (hex(registry), j)
                name_at = off + 0x50 + i64(blob, off + 0x50)
                name = blob[name_at:].split(b'\0', 1)[0].decode(errors='replace') if 0 <= name_at < len(blob) else ''
                if not re.fullmatch(r'[\w /.\-\[\]]{3,160}', name):
                    names = re.findall(rb'[a-z][a-z_0-9]{10,}', blob)
                    name = names[-1].decode() if names else ''
                slot['name'] = name
            slots.append(slot)
        memberships = wanted[(registry, tag)]
        groups.append({'registry': registry, 'objectTag': tag, 'cacheIndex': (start - base) // RECORD_SIZE,
                       'cacheOffset': start, 'topLevel': all(a == 0 for _, _, a in memberships),
                       'memberships': memberships, 'slots': slots})
    groups.sort(key=lambda g: g['cacheIndex'])
    return version, base, groups


def dialogue():
    _, b = packages.read(BANK)
    roots = {u32(b, o): o + 8 + i64(b, o + 8) for o in array(b, 24, 16, 0x80808D19)}
    starts = sorted(roots.values()) + [len(b)]
    rows = []
    containers = {}
    for index, o in enumerate(array(b, 8, 8, 0x80808D18)):
        selector = u32(b, o)
        start = roots[selector]
        end = starts[starts.index(start) + 1]
        texts = []
        for at in range(start, end - 7, 4):
            container, key = struct.unpack_from('<II', b, at)
            if not BANK - 0x2000 <= container <= BANK + 0x2000:
                continue
            if container not in containers:
                try:
                    containers[container] = strings(container)
                except (AssertionError, ValueError, FileNotFoundError, struct.error):
                    containers[container] = {}
            text = containers[container].get(key)
            if text and text not in texts:
                texts.append(text)
        rows.append({'row': index, 'selector': selector,
                     'durationMs': round(struct.unpack_from('<f', b, o + 4)[0] * 1000), 'texts': texts})
    return rows


def objectives():
    cls, data = packages.read(DIRECTIVES)
    assert cls == 0x80804F72, hex(cls)
    out = []
    for index, at in enumerate(array(data, 8, 40, 0x80804F74)):
        texts = []
        # Four typed hashes followed by flags: 36 bytes, not 32. The second
        # generator variant contains the exhaust-turbine counter label.
        for a in array(data, at + 16, 36, 0x80804F76):
            variant = []
            for j in (0, 8, 16, 24):
                container, key = u32(data, a + j), u32(data, a + j + 4)
                try:
                    variant.append(strings(container).get(key, '') if container != 0x811C9DC5 else '')
                except (AssertionError, ValueError, FileNotFoundError, struct.error):
                    variant.append('')
            texts.append(variant)
        out.append({'row': index, 'event': u32(data, at), 'texts': texts})
    return out


def volumes(owned):
    """Type-60 trigger volumes plus type-47/48 authored points found in the mission's package tags."""
    out = []
    points = []
    for tag in range(*TAG_RANGE):
        try:
            cls, data = packages.read(tag)
        except (AssertionError, ValueError, FileNotFoundError, struct.error):
            continue
        for at in range(12, len(data) - 0x40, 4):
            type_ = struct.unpack_from('<H', data, at + 4)[0]
            if type_ not in (47, 48, 60) or u32(data, at) not in owned:
                continue
            base = at - 12
            name_at = base + i64(data, base)
            if not 0 <= name_at < len(data):
                continue
            name = data[name_at:].split(b'\0', 1)[0].decode(errors='replace')
            if not re.fullmatch(r'[a-z_0-9.\[\]]+', name):
                continue
            slot = struct.unpack_from('<H', data, at + 6)[0]
            if type_ != 60:
                points.append({'tag': tag, 'offset': base, 'registry': u32(data, at), 'type': type_,
                               'slot': slot, 'name': name})
                continue
            if at + 0x114 > len(data):
                continue
            try:
                vertices = [struct.unpack_from('<3f', data, o) for o in array(data, base + 0xD0, 16, 0x80800094)]
            except (AssertionError, struct.error):
                continue
            out.append({'tag': tag, 'offset': base, 'registry': u32(data, at), 'slot': slot, 'name': name,
                        'min': struct.unpack_from('<3f', data, base + 0xB0),
                        'max': struct.unpack_from('<3f', data, base + 0xC0), 'vertices': vertices})
    return out, points


def scenes(groups):
    out = []
    for g in groups:
        for s in g['slots']:
            if s['slotTypes'] != 43 or s['descriptorTags'] == 0xFFFFFFFF:
                continue
            _, b = packages.read(s['descriptorTags'])
            offset = s['descriptorOffsets']
            key = (g['registry'], 43, s['slotIndices'])
            assert struct.unpack_from('<IHH', b, offset + 0x30) == key, (hex(g['registry']), s['slotIndices'])
            cast = []
            for ref in array(b, offset + 0x68, 8, 0x80806268):
                target = ref + i64(b, ref)
                kind = u32(b, target - 4)
                assert kind in (0x80806262, 0x80806264), hex(kind)
                registry, type_, slot = struct.unpack_from('<IHH', b, target + 8)
                cast.append({'kind': kind, 'registry': registry, 'type': type_, 'slot': slot,
                             'nameWords': [u32(b, target), u32(b, target + 4)]})
            selector = u32(b, offset + 0x60)
            out.append({'registry': g['registry'], 'slot': s['slotIndices'], 'name': s['name'],
                        'tag': s['descriptorTags'], 'offset': offset, 'selector': selector, 'cast': cast})
    return out


def trigger_links(groups, vols):
    """Type-31 player triggers reference one type-60 volume of the same registry."""
    by_key = {(v['registry'], v['slot']): v for v in vols}
    out = []
    for g in groups:
        for s in g['slots']:
            if s['slotTypes'] != 31 or s['descriptorTags'] == 0xFFFFFFFF:
                continue
            _, b = packages.read(s['descriptorTags'])
            off = s['descriptorOffsets']
            linked = None
            for at in range(off, min(off + 0x400, len(b) - 8), 4):
                registry = u32(b, at)
                type_, slot = struct.unpack_from('<HH', b, at + 4)
                if registry == g['registry'] and type_ == 60 and (registry, slot) in by_key:
                    linked = slot
                    break
            if linked is None:
                lowered = s['name'].lower()
                for (registry, slot), v in by_key.items():
                    if registry == g['registry'] and v['name'] == lowered:
                        linked = slot
                        break
            out.append({'registry': g['registry'], 'slot': s['slotIndices'], 'name': s['name'],
                        'tag': s['descriptorTags'], 'offset': off, 'volume': linked})
    return out


# Registries the installed cache walk classifies as not relevant (neither a scenario root, an
# explicit-slice third-registry local nor a player group). The client roster walk is emulated for
# them so their records match what Dawn admits once the mission's required-registry hook exists.
EMULATED = [(0x28A6B21F, 0x80B50746, 6, [[6, 0, 1]]), (0xF8F959CD, 0x80B50616, 6, [[6, 0, 1]])]
# Cinematic owners: bubble/state-1 type-6 slots synthesized by the roster like 1AU's movies().
MOVIES = [(0x964D8F24, 0x80B5012D, 2, 17), (0xE8B02346, 0x80B508FC, 8, 65), (0x42E8F541, 0x80B50126, 1, 9)]


def slot_name(slot):
    if slot['descriptorTags'] == 0xFFFFFFFF:
        return ''
    _, blob = packages.read(slot['descriptorTags'])
    off = slot['descriptorOffsets']
    name_at = off + 0x50 + i64(blob, off + 0x50)
    name = blob[name_at:].split(b'\0', 1)[0].decode(errors='replace') if 0 <= name_at < len(blob) else ''
    if not re.fullmatch(r'[\w /.\-\[\]]{3,160}', name):
        names = re.findall(rb'[a-z][a-z_0-9]{10,}', blob)
        name = names[-1].decode() if names else ''
    return name


def emulated_groups():
    out = []
    for registry, tag, bubble, memberships in EMULATED:
        walked = emulation.walk_object(tag)
        assert walked['registry'] == registry and not walked['overflow'], hex(registry)
        assert walked['explicitSliceMask'] == (1 << bubble), hex(registry)
        slots = []
        for s in walked['slots']:
            slot = {k: s[k] for k in ('slotTypes', 'slotFlags', 'slotIndices', 'descriptorTags', 'descriptorOffsets',
                                       'componentClasses', 'senseSchemas', 'authSchemas')}
            slot['name'] = slot_name(slot)
            slots.append(slot)
        out.append({'registry': registry, 'objectTag': tag, 'cacheIndex': None, 'cacheOffset': None, 'topLevel': False,
                    'memberships': memberships, 'slots': slots, 'emulated': True})
    return out


def movies():
    out = []
    for registry, tag, bubble, region in MOVIES:
        walked = emulation.walk_object(tag)
        assert walked['registry'] == registry and len(walked['slots']) == 1, hex(registry)
        s = walked['slots'][0]
        assert (s['slotTypes'], s['slotIndices'], s['descriptorOffsets'], s['componentClasses'], s['authSchemas']) == (6, 0, 0x2E8, 0x80804F06, 0x80804F08), hex(registry)
        cls, blob = packages.read(s['descriptorTags'])
        selector = u32(blob, s['descriptorOffsets'] + 0x60)
        out.append({'registry': registry, 'object': tag, 'definition': s['descriptorTags'], 'selector': selector,
                    'region': region, 'bubble': bubble})
    return out


def declared_slots(tag):
    _, blob = packages.read(tag)
    return [(u32(blob, o), u32(blob, o + 4)) for o in array(blob, 32, 8)]


def fnv1(text):
    value = 0x811C9DC5
    for byte in text.encode():
        value = (value * 0x01000193) & 0xFFFFFFFF
        value ^= byte
    return value


# Directive navigation points: Sunrise SDK symbols resolved to type-47 declared slots. Named
# points are matched by FNV-1 slot-name hash; SLOT_xxxx symbols carry the slot index directly.
NAVIGATION = [
    ('ap_ikora', 0x9027B6A1, 0x80B5167E, None), ('ap_weapon', 0x026087A0, 0x80B5168C, 9),
    ('ap_goto_military', 0x026087A0, 0x80B5168C, None), ('ap_goto_plaza', 0xD3847A1F, 0x80B50B91, 8),
    ('ap_plaza', 0xBB7B62E0, 0x80B51058, None), ('plaza_exit', 0xBB7B62E0, 0x80B51058, 14),
    ('speaker', 0xA73F44A8, 0x80B5097F, 10), ('board', 0xA73F44A8, 0x80B5097F, 4),
    ('ap_locate', 0x002D225E, 0x80B51397, None), ('ap_destroy_battleship', 0x002D225E, 0x80B51397, None),
    ('generator', 0x002D225E, 0x80B51397, 12), ('escape', 0x002D225E, 0x80B51397, 16),
]


def navigation():
    out = []
    for name, registry, tag, slot in NAVIGATION:
        declared = declared_slots(tag)
        if slot is None:
            matches = [i for i, (t, h) in enumerate(declared) if t == 47 and h == fnv1(name)]
            assert len(matches) == 1, (name, matches)
            slot = matches[0]
        assert declared[slot][0] == 47, (name, slot, declared[slot])
        out.append({'name': name, 'registry': registry, 'object': tag, 'slot': slot, 'nameHash': declared[slot][1]})
    return out


def objective_slots(groups):
    out = []
    for g in groups:
        for s in g['slots']:
            if s['slotTypes'] == 3:
                out.append({'registry': g['registry'], 'slot': s['slotIndices'], 'name': s['name'],
                            'tag': s['descriptorTags'], 'offset': s['descriptorOffsets']})
    return out


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    walked = walk(SCENARIO)
    assert walked['packageHash'] == 0x9ACCB518, hex(walked['packageHash'])
    version, base, groups = cache_groups(walked)
    groups.extend(emulated_groups())
    owned = {g['registry'] for g in groups}
    result = {
        'package': PACKAGE, 'scenario': SCENARIO, 'packageHash': walked['packageHash'],
        'cacheVersion': version, 'rosterSectionBase': base,
        'regions': walked['regions'], 'groups': groups,
        'sources': sources(groups), 'scenes': scenes(groups), 'objectiveSlots': objective_slots(groups),
        'dialogueBank': BANK, 'dialogue': dialogue(), 'directiveTable': DIRECTIVES, 'objectives': objectives(),
        'movies': movies(), 'navigation': navigation(),
    }
    result['volumes'], result['points'] = volumes(owned)
    result['triggers'] = trigger_links(groups, result['volumes'])
    result['provenance'] = {
        'method': 'Installed package walk, live build_data cache roster records and package descriptors; no live observations',
        'cache_sha256': hashlib.sha256((packages.ROOT / 'Dawn/cache/build_data.bin').read_bytes()).hexdigest(),
    }
    text = json.dumps(result, indent=1)
    (OUT / 'towerfall-bindings.json').write_text(text + '\n', encoding='utf-8')
    print('cache version', version, 'groups', len(groups), 'sources', len(result['sources']), 'scenes', len(result['scenes']),
          'volumes', len(result['volumes']), 'points', len(result['points']), 'triggers', len(result['triggers']),
          'dialogue rows', len(result['dialogue']), 'objectives', len(result['objectives']))
    unlinked = [t['name'] for t in result['triggers'] if t['volume'] is None]
    print('unlinked triggers:', unlinked)
    print('sha256', hashlib.sha256(text.encode('utf-8')).hexdigest())


if __name__ == '__main__':
    main()
