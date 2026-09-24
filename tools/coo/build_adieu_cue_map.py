"""Join native music switches, packaged media and repeatable acoustic candidates.

Requires the wwiser XML dump and match_adieu_music.py output. No listening or
live game acceptance is asserted. Consecutive aligned windows reduce accidental
matches to loading silence; ambiguous native switches remain ambiguous.
"""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('build/coo/adieu-research'))
    args = parser.parse_args()
    root = args.root
    evidence = json.loads((root / 'adieu-bindings.json').read_text(encoding='utf-8'))
    acoustic = json.loads((root / 'music/acoustic-matches.json').read_text(encoding='utf-8'))
    xml = root / 'music/80BF5FD7.bnk.xml'
    tree = ET.parse(xml).getroot()
    objects = {}
    for obj in tree.iter('obj'):
        identity = obj.find('./fld[@na="ulID"]')
        if identity is not None:
            objects[int(identity.attrib['va'])] = obj
    switch = objects[799680117]
    assert switch.attrib['na'] == 'CAkMusicSwitchCntr'
    groups = [int(f.attrib['va']) for f in switch.findall('.//obj[@na="AkGameSync"]/fld[@na="ulGroup"]')]
    assert groups == [0x4A467C68, 1433286457]
    media = {s['mediaId']: s['tag'] for s in acoustic['stems']}

    def reachable(identity, visited=None):
        visited = set() if visited is None else visited
        if identity in visited:
            return set()
        visited.add(identity)
        obj = objects[identity]
        result = set()
        for field in obj.iter('fld'):
            name, value = field.attrib.get('na'), field.attrib.get('va')
            if name == 'sourceID':
                assert int(value) in media
                result.add(media[int(value)])
            elif name in ('ulChildID', 'segmentID', 'SegmentID') and int(value) in objects:
                result |= reachable(int(value), visited)
        return result

    cues = []
    for state in evidence['music']:
        nodes = []
        for node in switch.findall('.//obj[@na="AkDecisionTree"]//obj[@na="Node"]'):
            key = node.find('./fld[@na="key"]')
            if key is not None and int(key.attrib['va']) == state['state']:
                nodes.extend(int(f.attrib['va']) for f in node.findall('.//fld[@na="audioNodeId"]'))
        assert len(nodes) == 1
        tags = sorted(set().union(*(reachable(n) for n in nodes)))
        assert tags
        cues.append(dict(state, audioNodes=nodes, mediaTags=tags))
    assert cues[1]['mediaTags'] == cues[2]['mediaTags'] == cues[3]['mediaTags']
    assert cues[4]['mediaTags'] == cues[5]['mediaTags']
    candidates = []
    for match in acoustic['matches']:
        if match['score'] < .50 or match.get('sourceRmsDbfs', -100) < -60:
            continue
        # Adieu bank only. Cinematic and Farm soundtracks require separate banks.
        if not 81 <= match['referenceStart'] <= 585:
            continue
        aligned = [m for m in acoustic['matches'] if m['tag'] == match['tag'] and m['score'] >= .5
                   and m.get('sourceRmsDbfs', -100) >= -60
                   and abs(abs(m['sourceStart'] - match['sourceStart']) - 8) < .01
                   and abs((m['referenceStart'] - m['sourceStart']) - (match['referenceStart'] - match['sourceStart'])) <= .25]
        if not aligned:
            continue
        candidates.append(dict(match, nativeOrdinals=[c['ordinal'] for c in cues if match['tag'] in c['mediaTags']],
                               evidence='Consecutive aligned acoustic windows; not auditory verification'))
    report = dict(method='Native ordinal -> Wwise decision tree -> child media -> energy-filtered aligned acoustic windows',
        limitation='Aliases prevent a unique switch assignment. Silence and unmatched windows do not prove absence of music. Cinematic audio and SFX still need an auditory pass.',
        sourceXmlSha256=hashlib.sha256(xml.read_bytes()).hexdigest(), cues=cues, acousticCandidates=candidates)
    (root / 'music/cue-map.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{len(cues)} native music states; {len(candidates)} aligned acoustic windows.')


if __name__ == '__main__':
    main()
