"""Recover Adieu's authored inventory without changing the installed game.

Run with --game-root pointing at build 86657, including its unpacked research
image. Outputs belong to this checkout, independently of the package location.
"""
import argparse
import hashlib
import json
import os
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', required=True, type=Path)
    parser.add_argument('--out', type=Path, default=Path(__file__).resolve().parents[2] / 'build/coo/adieu-research')
    args = parser.parse_args()
    os.environ['DAWN_GAME_ROOT'] = str(args.game_root.resolve())
    import package_read as packages
    from extract_deadly_trial_bindings import walk
    from extract_gateway_bindings import array, u32, i64
    import towerfall_walk_emulation as emulation
    import extract_towerfall_bindings as decoder

    activity, scenario = 0x80B5E014, 0x80B5E01F
    cls, activity_data = packages.read(activity)
    assert cls == 0x80808AAE and u32(activity_data, 64) == scenario
    walked = walk(scenario)
    assert walked['packageHash'] == u32(activity_data, 8) == 0xD8A28814
    _, investment = packages.read(0x81327CF0)
    rows = array(investment, 8, 16, 0x808076FC)
    entry = rows[288]
    record = entry + 8 + i64(investment, entry + 8)
    name_at = record + 0x68 + i64(investment, record + 0x68)
    assert investment[name_at:].split(b'\0', 1)[0] == b'mission_journey'
    assert u32(investment, entry) == 0xB913ED3F
    memberships = {}
    for region in walked['regions']:
        for obj in region['objects']:
            memberships.setdefault((obj['registry'], obj['tag']), []).append([region['bubble'], region['state'], obj['array']])
    groups = []
    for (registry, tag), membership in memberships.items():
        group = emulation.walk_object(tag)
        assert group['registry'] == registry and not group['overflow'], hex(registry)
        for slot in group['slots']:
            slot['name'] = decoder.slot_name(slot)
        group.update(objectTag=tag, topLevel=all(m[2] == 0 for m in membership), memberships=membership, cacheIndex=None)
        groups.append(group)
    decoder.TAG_RANGE = (0x80B5E000, 0x80B60000)
    decoder.BANK, decoder.DIRECTIVES = 0x80C2AB74, 0x80B5E023
    volumes, points = decoder.volumes({g['registry'] for g in groups})
    result = dict(walked, package='mission_journey', activityIndex=288, investmentHash=0xB913ED3F,
                  activityTag=activity, launchTag=u32(activity_data, 68), groups=groups,
                  volumes=volumes, points=points, scenes=decoder.scenes(groups), sources=decoder.sources(groups),
                  triggers=decoder.trigger_links(groups, volumes))
    result.update(dialogueBank=decoder.BANK, directiveTable=decoder.DIRECTIVES,
                  dialogue=decoder.dialogue(), objectives=decoder.objectives(),
                  objectiveSlots=decoder.objective_slots(groups))
    # Keep movie-state roster variants separate: vision, Hawthorne rescue and
    # Ghaul/Speaker share bubble 3. The scenario's transition_card_01/02/03
    # sequences are independent of these type-6 owners.
    movies = []
    for group in groups:
        for slot in group['slots']:
            if slot['slotTypes'] != 6:
                continue
            _, blob = packages.read(slot['descriptorTags'])
            memberships = group['memberships']
            assert len(memberships) == 1 and memberships[0][0] == 3
            movies.append(dict(registry=group['registry'], object=group['objectTag'],
                definition=slot['descriptorTags'], offset=slot['descriptorOffsets'],
                sequenceIdentity=struct.unpack_from('<Q', blob, slot['descriptorOffsets'] + 0x58)[0],
                bubble=3, region=24 + memberships[0][1]))
    result['movies'] = movies
    # The inline music component contains an ordered name/ref table and an
    # equally sized array of Wwise state commands. Preserve ordinal identity.
    _, music = packages.read(0x80B5E027)
    names = array(music, 0x718, 12, 0x80804F66)
    commands = array(music, 0x6B0, 8, 0x80804E93)
    assert len(names) == len(commands) == 7
    result['music'] = []
    for index, (name, command) in enumerate(zip(names, commands)):
        at = command + i64(music, command)
        assert u32(music, at - 4) == 0x80804E95
        result['music'].append(dict(ordinal=index, nameHash=u32(music, name),
            bank=u32(music, at), stateGroup=u32(music, at + 4), state=u32(music, at + 8),
            referenceMatch='unverified; package identity alone does not establish audible timing'))
    # Scene graph inputs, speech and child action identities. The selector
    # template points to exactly one graph resource; do not scan nearby hashes.
    for scene in result['scenes']:
        cls, entity = packages.read(scene['selector'])
        assert cls == 0x80809C0F
        graphs = []
        for resource in array(entity, 0x10, 12):
            tag = u32(entity, resource)
            cls, graph = packages.read(tag)
            if cls == 0x80809C36 and len(graph) > 0xA0 and u32(graph, 0x94) == 0x80806384:
                assert u32(graph, 0x90) == tag
                graphs.append((tag, graph))
        assert len(graphs) == 1, (scene['name'], [hex(t) for t, _ in graphs])
        tag, graph = graphs[0]
        events, actions = [], []
        for event in array(graph, 0xE8, 0xC0):
            assert u32(graph, event + 4) == 0x8080638A
            definition = i64(graph, event + 8)
            assert u32(graph, definition + 4) == 0x8080637D
            events.append(u32(graph, definition + 0x10))
        for index, entry in enumerate(array(graph, 0xC8, 0x30)):
            prototype = entry + 0x20 + i64(graph, entry + 0x20)
            definition = i64(graph, prototype + 8)
            action = dict(index=index, prototype=prototype - 0x90,
                kind=u32(graph, prototype + 4), definition=definition)
            if action['kind'] not in (0x8080631F, 0x80806323, 0x80806330, 0x8080632E):
                action['startInput'] = u32(graph, definition + 0x48)
                action['successorInputs'] = [u32(graph, p) for p in array(graph, definition + 0x20, 4)]
            if action['kind'] == 0x80806285:
                action['delayRangeSeconds'] = struct.unpack_from('<ff', graph, definition + 0x58)
            if action['kind'] == 0x808062E4:
                inputs = array(graph, 0xF8, 16)
                emitted = []
                for p in array(graph, definition + 0x68, 4):
                    index = u32(graph, p)
                    assert index < len(inputs)
                    target = i64(graph, inputs[index] + 8) - 0x50
                    assert u32(graph, target + 4) == 0x8080638A
                    event_def = i64(graph, target + 8)
                    emitted.append(u32(graph, event_def + 0x10))
                action['emittedEvents'] = emitted
            if action['kind'] == 0x808062F6:
                selector, bank = struct.unpack_from('<II', graph, definition + 0x78)
                action.update(dialogueSelector=selector, dialogueBank=bank)
                action['dialogueRows'] = [r['row'] for r in result['dialogue']
                    if bank == decoder.BANK and r['selector'] == selector]
            actions.append(action)
        scene.update(graph=tag, graphRoot=i64(graph, 0x98), graphSha256=hashlib.sha256(graph).hexdigest(),
                     events=events, actions=actions)
    spawn_points = []
    # Installed the_journey map plus its two activity packages.
    for package_id in (0x03F4, 0x01AF, 0x022B):
        path, data, table, count, _ = packages.package(package_id)
        for index in range(count):
            if u32(data, table + 16 * index) != 0x80809162:
                continue
            tag = 0x80800000 + (package_id << 13) + index
            _, blob = packages.read(tag)
            for entry in array(blob, 8, 48):
                spawn_points.append(dict(tag=tag, offset=entry, package=path.name,
                    setHash=u32(blob, entry + 32),
                    rotation=struct.unpack_from('<4f', blob, entry),
                    position=struct.unpack_from('<3f', blob, entry + 16)))
    result['spawnPoints'] = spawn_points
    result['provenance'] = {'method': 'Package descriptors and emulated native roster walk; no live acceptance',
                            'gameRoot': str(args.game_root.resolve()),
                            'scenarioSha256': hashlib.sha256(packages.read(scenario)[1]).hexdigest()}
    args.out.mkdir(parents=True, exist_ok=True)
    output = args.out / 'adieu-bindings.json'
    output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'output': str(output), 'groups': len(groups), 'volumes': len(volumes),
                      'sources': len(result['sources']), 'scenes': len(result['scenes'])}))


if __name__ == '__main__':
    main()
