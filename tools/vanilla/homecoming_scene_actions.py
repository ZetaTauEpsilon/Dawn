"""Extract/verify exact Homecoming action watches. Read-only package access.

Without --check, emit the C++ header to stdout. With --check, require the
checked-in table to match every relevant action in every catalogued graph.
"""
import argparse
import importlib
import json
from pathlib import Path
import struct
import sys


def extract(reader_dir):
    sys.path.insert(0, str(reader_dir))
    reader = importlib.import_module('package_read')
    array = importlib.import_module('extract_gateway_bindings').array
    bindings = json.loads((reader_dir.parents[1] / 'build/coo/towerfall-research/towerfall-bindings.json').read_text())
    u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
    i64 = lambda b, o: struct.unpack_from('<q', b, o)[0]
    rows = {r['selector']: r['row'] for r in bindings['dialogue']}
    graphs = {}
    sources = set()
    bindings_to_graphs = set()
    for scene in bindings['scenes']:
        if scene['selector'] == 0xFFFFFFFF:
            continue
        _, resource = reader.read(scene['selector'])
        for off in range(0xB0, min(len(resource)-3, 0xE0), 4):
            tag = u32(resource, off)
            if tag in graphs:
                if graphs[tag][1]:
                    sources.add(scene['tag'])
                    bindings_to_graphs.add((scene['tag'], tag, graphs[tag][0]))
                continue
            if not 0x80A00000 <= tag < 0x81000000:
                continue
            try:
                _, data = reader.read(tag)
            except (ValueError, FileNotFoundError, KeyError):
                continue
            if len(data) < 0xA0 or u32(data, 0x94) != 0x80806384:
                continue
            actions = []
            for entry in array(data, 0xC8, 0x30):
                node = entry + 0x20 + i64(data, entry + 0x20)
                kind = u32(data, node + 4)
                definition = i64(data, node + 8)
                if kind == 0x808062F6 and u32(data, definition+0x7C) == 0x80C2AF61:
                    selector = u32(data, definition+0x78)
                    if selector in rows:
                        actions.append((node-0x90, definition, 'speech', rows[selector]))
                elif kind == 0x80806297:
                    parameter, flag = struct.unpack_from('<II', data, definition+0x58)
                    if parameter < 16 and flag in (0x1526DC06, 0xF9521E4E):
                        actions.append((node-0x90, definition, 'combat' if flag == 0x1526DC06 else 'damage', parameter))
            graphs[tag] = (i64(data, 0x98), actions)
            if actions:
                sources.add(scene['tag'])
                bindings_to_graphs.add((scene['tag'], tag, graphs[tag][0]))
    return graphs, sources, bindings_to_graphs


def header(graphs, sources, bindings_to_graphs):
    lines = ['#pragma once', '// Package-derived by tools/vanilla/homecoming_scene_actions.py; do not hand-edit.',
             '// Read only these actions, not every entry of every active selector each frame.',
             '#include <cstdint>', '#include <span>',
             'namespace dawn::state::activity::vanilla::homecoming {',
             'enum class SceneActionKind : std::uint8_t { speech,combat,damage };',
             'struct SceneAction {std::uint32_t node,definition;SceneActionKind kind;std::uint8_t value;};',
             'struct SceneActionPlan {std::uint32_t graph,root;std::span<const SceneAction> actions;};']
    for tag, (root, actions) in sorted(graphs.items()):
        if not actions:
            continue
        lines.append(f'inline constexpr SceneAction kActions{tag:08X}[]{{')
        for node, definition, kind, value in actions:
            lines.append(f'    {{0x{node:X},0x{definition:X},SceneActionKind::{kind},{value}}},')
        lines.append('};')
    lines.append('inline constexpr SceneActionPlan kSceneActionPlans[]{')
    for tag, (root, actions) in sorted(graphs.items()):
        if actions:
            lines.append(f'    {{0x{tag:08X},0x{root:X},kActions{tag:08X}}},')
    lines.extend(['};', 'constexpr const SceneActionPlan* scene_action_plan(std::uint32_t graph,std::uint32_t root) noexcept {',
                  '    for(const auto& plan:kSceneActionPlans) if(plan.graph==graph && plan.root==root) return &plan;',
                  '    return nullptr;', '}'])
    lines.append('inline constexpr std::uint32_t kSceneActionDefinitions[]{')
    lines.extend(f'    0x{tag:08X},' for tag in sorted(sources))
    lines.extend(['};', 'struct SceneActionSource {std::uint32_t definition,graph,root;};',
                  'inline constexpr SceneActionSource kSceneActionSources[]{'])
    lines.extend(f'    {{0x{definition:08X},0x{graph:08X},0x{root:X}}},'
                 for definition, graph, root in sorted(bindings_to_graphs))
    lines.extend(['};', '}', ''])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir', type=Path, required=True)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    graphs, sources, bindings_to_graphs = extract(args.reader_dir.resolve())
    content = header(graphs, sources, bindings_to_graphs)
    if args.check:
        path = Path(__file__).resolve().parents[2] / 'Dawn/src/state/activity/vanilla/homecoming/scene_actions.h'
        assert path.read_text() == content, 'Action watch table differs from installed package inventory'
        print(f'PASS: exact action inventory across {len(graphs)} graphs; {sum(len(v[1]) for v in graphs.values())} watched actions')
    else:
        print(content, end='')


if __name__ == '__main__':
    main()
