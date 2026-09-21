"""Read-only validation of Homecoming's native Scene offsets against installed packages.

Usage: python tools/vanilla/validate_homecoming_scene_bindings.py --reader-dir <tools/coo>
Uses the existing authenticated package reader; does not modify the game or emit keys.
This checks package identities, not runtime animation/interaction success.
"""
import argparse
import importlib
from pathlib import Path
import re
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir.resolve()))
    reader = importlib.import_module('package_read')
    array = importlib.import_module('extract_gateway_bindings').array
    root = Path(__file__).resolve().parents[2]
    source = (root / 'Dawn/src/state/activity/vanilla/homecoming/scene_playback.h').read_text()
    rows = re.findall(r'\{asset\((\w+),43,(\d+)\),((?:0x[0-9A-Fa-f]+,){4}0x[0-9A-Fa-f]+)\}', source)
    assert len(rows) == 5, 'Expected four arrival performances and a distinct Zavala revival binding'
    checks = 0
    for registry, slot, values in rows:
        tag, definition, node, node_definition, kind = [int(v, 16) for v in values.split(',')]
        _, data = reader.read(tag)
        assert struct.unpack_from('<IIq', data, 0x90) == (tag, 0x80806384, definition), (registry, slot, 'root')
        assert struct.unpack_from('<IIq', data, 0x90 + node) == (tag, kind, node_definition), (registry, slot, 'action')
        checks += 2
        print(f'PASS: {registry}/43/{slot} graph={tag:08X} natural-performance action')
    _, data = reader.read(0x80C3DD7D)
    assert struct.unpack_from('<IIq', data, 0x13D0) == (0x80C3DD7D, 0x80806358, 0xD230), 'Ikora entry signal reference'
    assert struct.unpack_from('<I', data, 0x1430)[0] == 0, 'Ikora entry counter initially clear'
    checks += 2
    # The Ikora actor performer, not a top-level dialogue action, already
    # schedules all of row 72 after her Nova Bomb lines.
    _, ikora = reader.read(0x80B3A848)
    for offset, selector in ((0x2844,0x8504A78E),(0x28AC,0x7F025F85),
                             (0x2914,0x7900179C),(0x297C,0x72FDCF93),(0x29E4,0x06F8A841)):
        assert struct.unpack_from('<III',ikora,offset)==(selector,255,0x80C2AF61)
        checks += 1
    # Veteran selections are real bank conditions, resolved through the
    # expression pool to dense evaluated flag indices (not durable profile edits).
    _, bank = reader.read(0x80C2AF61)
    _, pool = reader.read(0x81319324)
    _, flags = reader.read(0x81319321)
    expressions = {struct.unpack_from('<I',pool,o)[0]:o for o in array(pool,8,24,0x80807C4F)}
    flag_rows = array(flags,8,8,0x80807D4F)
    for offset, condition, dense, key in ((0x35C8,0x63533000,2024,0x2D4AA2EA),
            (0x2B28,0xD12EC6F2,2027,0x0D991A39),(0x4C98,0xEAB8395B,2028,0x0244DDE2)):
        assert struct.unpack_from('<II',bank,offset)==(0x80808D3A,condition)
        ops=array(pool,expressions[condition]+8,8,0x80807D31)
        assert len(ops)==1 and struct.unpack_from('<II',pool,ops[0])==(1,dense)
        assert struct.unpack_from('<I',flags,flag_rows[dense])[0]==key
        checks += 3
    # The previously used Shaxx child is a door performer bound to parameter 1,
    # not a speech/actor completion. Keep it explicitly out of progression gates.
    assert not any(registry == 'kUnderwatch' and slot == '14' for registry, slot, _ in rows)
    _, shaxx = reader.read(0x80BEB7EF)
    assert struct.unpack_from('<II', shaxx, 0x6770) == (0x80B3A3F7, 1), 'Shaxx child 26 remains exit-door parameter 1'
    checks += 1
    def words(data, offset):
        return [struct.unpack_from('<I', data, o)[0] for o in array(data, offset, 4)]

    def events(data):
        result = {}
        for entry in array(data, 0xE8, 0xC0):
            definition = struct.unpack_from('<q', data, entry + 8)[0]
            key = struct.unpack_from('<I', data, definition + 0x10)[0]
            result[key] = words(data, definition + 0x30)
        return result

    # Early Shaxx acquisition is driven by init, independently of his entry.
    # Init -> input 30 -> join action 0 -> input 4 -> acquire parameter 0.
    assert events(shaxx)[0x385838EC] == [30]
    assert struct.unpack_from('<I', shaxx, 0x56A8 + 0x88)[0] == 30
    assert words(shaxx, 0x56A8 + 0x40) == [4]
    assert struct.unpack_from('<I', shaxx, 0x5788 + 0x48)[0] == 4
    assert struct.unpack_from('<I', shaxx, 0x5788 + 0x70)[0] == 0
    # Entry releases wait 14. Only that branch starts the half-second delay
    # and then greeting 21; merely instantiating the selector does not talk.
    assert events(shaxx)[0x6F51AC66] == [32, 35, 37]
    assert struct.unpack_from('<I', shaxx, 0x5F18 + 0x68)[0] == 32
    assert words(shaxx, 0x5F18 + 0x20) == [17, 18, 19]
    assert struct.unpack_from('<I', shaxx, 0x5CE8 + 0x48)[0] == 17
    assert words(shaxx, 0x5CE8 + 0x20) == [14]
    assert struct.unpack_from('<I', shaxx, 0x5D68 + 0x48)[0] == 14
    checks += 11
    # The stairwell fight observes child 26 START, never child 25's staged
    # closed idle or the persistent performer's eventual completion.
    door = re.search(r'kShaxxDoorOpening\{\s*asset\(kUnderwatch,43,14\),([^}]+)\}', source)
    assert door, 'Shaxx door-entry binding exists separately from completion bindings'
    graph, root, node, definition, child, child_root = [int(v,16) for v in door[1].split(',')]
    assert struct.unpack_from('<IIq', shaxx, 0x90) == (graph,0x80806384,root)
    assert struct.unpack_from('<IIq', shaxx, 0x90+node) == (graph,0x808062FE,definition)
    assert struct.unpack_from('<I', shaxx, definition+0x48)[0] == 18
    assert struct.unpack_from('<II', shaxx, definition+0x58) == (0x80B3A3F7,1)
    _, door_resource = reader.read(0x80B3A3F7)
    assert struct.unpack_from('<I', door_resource, 0xCC)[0] == child
    _, door_child = reader.read(child)
    assert struct.unpack_from('<IIq', door_child, 0x90) == (child,0x808084E9,child_root)
    assert struct.unpack_from('<IIq', door_child, 0x650) == (child,0x808031D2,0x11F8)
    assert struct.unpack_from('<ff', door_child, 0x11F8+0x1C) == (0.,0.)
    checks += 8
    # Scene 111 parameter 0 is the friendly frame, not another Cabal.
    # The first event removes the opening hold but leaves F9521E4E on
    # Cabal parameter 1. The second event removes that remaining flag.
    # Its parallel death/re-protection branch belongs to friendly parameter 0.
    _, postgun = reader.read(0x80C3DF11)
    assert struct.unpack_from('<IIq', postgun, 0x90) == (0x80C3DF11, 0x80806384, 0x5528)
    assert events(postgun)[0x38857CF3] == [31, 33]
    assert struct.unpack_from('<I', postgun, 0x5CA8 + 0x78)[0] == 31
    assert words(postgun, 0x5CA8 + 0x20) == [8]
    assert struct.unpack_from('<I', postgun, 0x5D68 + 0x48)[0] == 8
    assert words(postgun, 0x5D68 + 0x20) == [9, 10, 11, 12]
    assert struct.unpack_from('<I', postgun, 0x5F88 + 0x48)[0] == 10
    assert struct.unpack_from('<I', postgun, 0x6188 + 0x68)[0] == 33
    assert words(postgun, 0x6188 + 0x20) == [18]
    assert struct.unpack_from('<I', postgun, 0x6318 + 0x48)[0] == 18
    for node, definition, parameter in ((0x1990, 0x5F88, 0), (0x2370, 0x6318, 1)):
        assert struct.unpack_from('<IIq', postgun, 0x90 + node) == (0x80C3DF11, 0x80806297, definition)
        assert struct.unpack_from('<II', postgun, definition + 0x58) == (parameter, 0x1526DC06)
        checks += 2
    checks += 10
    assert events(postgun)[0x18EF2ABC] == [32, 34]
    assert words(postgun, 0x6398 + 0x20) == [22]
    assert struct.unpack_from('<I', postgun, 0x6428 + 0x48)[0] == 22
    assert words(postgun, 0x6428 + 0x20) == [23, 24]
    assert struct.unpack_from('<I', postgun, 0x6938 + 0x48)[0] == 24
    assert struct.unpack_from('<IIq', postgun, 0x90 + 0x3760) == (0x80C3DF11, 0x80806297, 0x6938)
    assert struct.unpack_from('<II', postgun, 0x6938 + 0x58) == (1, 0xF9521E4E)
    assert struct.unpack_from('<II', postgun, 0x6108 + 0x58) == (1, 0xF9521E4E)
    assert struct.unpack_from('<II', postgun, 0x6660 + 0x60) == (1, 3)
    assert struct.unpack_from('<II', postgun, 0x67A8 + 0x58) == (0, 0x1526DC06)
    checks += 10
    holds = re.findall(r'\{asset\((\w+),43,(\d+)\),((?:0x[0-9A-Fa-f]+,){3}0x[0-9A-Fa-f]+),(\d+)\}', source)
    assert {(registry, int(slot)) for registry, slot, _, _ in holds} == {
        ('kUnderwatch',111),('kMilitary',70),('kMilitary',72),('kMilitary',74),('kMilitary',77)}
    for registry, slot, values, signal in holds:
        tag, definition, node, node_definition = [int(v, 16) for v in values.split(',')]
        _, graph = reader.read(tag)
        assert struct.unpack_from('<IIq', graph, 0x90) == (tag, 0x80806384, definition)
        assert struct.unpack_from('<IIq', graph, 0x90 + node) == (tag, 0x80806278, node_definition)
        assert struct.unpack_from('<I', graph, node_definition + 0x68)[0] == int(signal)
        assert int(signal) in events(graph)[0x18EF2ABC]
        checks += 4
        print(f'PASS: {registry}/43/{slot} native combat hold and second-stage release')
    _, cover = reader.read(0x80C3DD7F)
    assert events(cover)[0x18EF2ABC] == [43,44,45,46]
    assert words(cover, 0x9048 + 0x20) == [37]
    assert struct.unpack_from('<I', cover, 0x90D8 + 0x48)[0] == 37
    assert words(cover, 0x90D8 + 0x20) == [38]
    assert struct.unpack_from('<I', cover, 0x9208 + 0x48)[0] == 38
    assert struct.unpack_from('<I', cover, 0x9208 + 0x58)[0] == 1
    _, corridor = reader.read(0x80C3DD81)
    assert events(corridor)[0x18EF2ABC] == [23,24]
    assert words(corridor, 0x4838 + 0x20) == [14,15]
    assert struct.unpack_from('<IIq', corridor, 0x90 + 0x2650) == (0x80C3DD81, 0x80806297, 0x4CF0)
    assert struct.unpack_from('<I', corridor, 0x4CF0 + 0x48)[0] == 15
    assert struct.unpack_from('<II', corridor, 0x4CF0 + 0x58) == (1, 0xF9521E4E)
    checks += 11
    _, zavala = reader.read(0x80C3DEBD)
    assert struct.unpack_from('<II', zavala, 0xBEB0 + 0x58) == (0, 0x1526DC06), 'Zavala actor flag-release handoff'
    assert struct.unpack_from('<I', zavala, 0xBEB0 + 0x48)[0] == 63, 'Handoff consumes animation completion input 63'
    assert struct.unpack_from('<II', zavala, 0xDC40 + 0x58) == (0, 0x80B8263B), 'Arrival animation is bound to Zavala'
    checks += 3
    assert struct.unpack_from('<IIq', zavala, 0x90 + 0x68D0) == (0x80C3DEBD, 0x80806307, 0xD7D8), 'Revival successor idle action'
    assert struct.unpack_from('<II', zavala, 0xD720 + 0x58) == (0, 0x80F1FCB6), 'Revival animation is bound to Zavala'
    assert struct.unpack_from('<I', zavala, 0xD720 + 0x78)[0] == 0xFFFFFFFF, 'Revival animation has no cancellation input'
    assert struct.unpack_from('<II', zavala, 0xD7D8 + 0x58) == (0, 0x80BFA697), 'Revival successor is the authored Zavala idle'
    assert struct.unpack_from('<I', zavala, 0xD7D8 + 0x48)[0] == 56, 'Revival idle consumes input 56'
    checks += 5
    # Both regular Wards already own a shield on the performer's native
    # timeline. The mission's old object18 enable added an independent copy
    # at entry, before the authored casting beat. No host timer is needed.
    _, ward_graph = reader.read(0x80BEB7A1)
    assert struct.unpack_from('<II', ward_graph, 0x2D78 + 0x58) == (0x80B3A2D8, 2)
    _, ward_resource = reader.read(0x80B3A2D8)
    assert struct.unpack_from('<I', ward_resource, 0xCC)[0] == 0x80B3A858
    _, ward = reader.read(0x80B3A858)
    assert struct.unpack_from('<IIq', ward, 0x90) == (0x80B3A858, 0x808084E9, 0xEC0)
    assert struct.unpack_from('<IIq', ward, 0x630) == (0x80B3A858, 0x808031D2, 0x13D8)
    assert struct.unpack_from('<ff', ward, 0x13F4) == (3., 3.)
    assert struct.unpack_from('<I', ward, 0x14C4)[0] == 0x80B3A2D3
    assert struct.unpack_from('<IIq', ward, 0xA20) == (0x80B3A858, 0x80808881, 0x16C0)
    assert struct.unpack_from('<ff', ward, 0x16DC) == struct.unpack('<ff', struct.pack('<ff', 5.3, 5.3))
    assert struct.unpack_from('<I', ward, 0x1734)[0] == 0x80B826F4
    _, standalone = reader.read(0x80B50F39)
    assert struct.unpack_from('<IHH', standalone, 0x4F8) == (0x28A6B21F, 4, 18)
    assert struct.unpack_from('<I', standalone, 0x580)[0] == 0x80B826F1
    # These are two shield resource variants with shared components, not a
    # missing visual that requires spawning object18 from mission logic.
    _, native_shield = reader.read(0x80B826F4)
    _, duplicate_shield = reader.read(0x80B826F1)
    assert struct.unpack_from('<I', native_shield, 0xC0)[0] == 0x80BEB6B8
    assert struct.unpack_from('<I', duplicate_shield, 0xC0)[0] == 0x80BEB6B8
    checks += 13
    print('PASS: both regular Wards retain the native casting/shield timeline; no early standalone duplicate')
    # The generator objective's second 36-byte value, not its description-only
    # first value, supplies the label and counter flag from the retail HUD.
    _, directives = reader.read(0x80B508FE)
    entries = array(directives, 8, 40, 0x80804F74)
    assert struct.unpack_from('<I', directives, entries[11])[0] == 0xF0D48F30
    variants = array(directives, entries[11] + 16, 36, 0x80804F76)
    assert len(variants) == 2
    assert struct.unpack_from('<II', directives, variants[1] + 16) == (0x80B9EBCD, 0x44FBFF68)
    assert struct.unpack_from('<I', directives, variants[0] + 32)[0] == 0
    assert struct.unpack_from('<I', directives, variants[1] + 32)[0] == 1
    checks += 5
    # obj_deck extends through matrix, shield-generator and escape areas. These
    # providers justify assigning the same authored task to sources 36-55.
    import json
    bindings = json.loads((args.reader_dir.parents[1] / 'build/coo/towerfall-research/towerfall-bindings.json').read_text())
    group = next(g for g in bindings['groups'] if g['registry'] == 0x2D322467)
    task = next(s for s in group['slots'] if s['slotTypes'] == 3 and s['slotIndices'] == 1)
    _, tasks = reader.read(task['descriptorTags'])
    rows = array(tasks, task['descriptorOffsets'] + 0x88, 40)
    assert len(rows) == 24
    for row, slots in ((10, (261,263)), (12, (268,269,270)), (14, (273,)), (18, (277,278))):
        refs = [struct.unpack_from('<IHH', tasks, o + 32) for o in array(tasks, rows[row] + 16, 40)]
        assert refs == [(0x2D322467,45,s) for s in slots], (row,refs)
        checks += 1
    checks += 1
    print(f'PASS: {checks} installed-package checks; no runtime playback claim')


if __name__ == '__main__':
    main()
