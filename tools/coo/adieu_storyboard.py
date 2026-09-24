"""Emit one record per second of the supplied Adieu reference.

Annotations describe the reviewed video, never elapsed-time gameplay gates.
Run after prepare_adieu_reference.py. Audio cue identity needs a separate audit.
"""
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/coo/adieu-research/video'
# Inclusive seconds in the original upload. Every sampled frame was reviewed in
# minute contact sheets; boundaries are accurate to the one-second sampling only.
BEATS = [
    (0, 4, 'uploader_intro', 'Uploader ident', 'editorial'),
    (5, 15, 'vision_bird', 'Vision: spirit bird, clouds and Traveler', 'cinematic'),
    (16, 30, 'vision_water', 'Vision: submerged bodies and triangular shadows', 'cinematic'),
    (31, 45, 'vision_destruction', 'Vision: destruction and abstract light', 'cinematic'),
    (46, 57, 'vision_shard', 'Vision: spirit, waterfall and Shard landscape', 'cinematic'),
    (58, 75, 'load_city', 'Black/loading interval', 'loading'),
    (76, 79, 'card_city', 'Last City title card', 'cinematic'),
    (80, 80, 'city_fade', 'Black transition', 'loading'),
    (81, 86, 'city_ground', 'Wounded Guardian on ground among city wreckage', 'authored'),
    (87, 92, 'city_rise', 'Guardian rises injured; red health and monochrome pulses', 'authored'),
    (93, 119, 'city_street', 'Slow third-person movement through damaged street', 'player'),
    (120, 139, 'city_rubble', 'Rubble and planter route; player looks around', 'player'),
    (140, 153, 'city_canal', 'Descent into canal beside burning wreck', 'player'),
    (154, 179, 'city_wreck', 'Travel under wreck; patrol searchlight overhead', 'player'),
    (180, 190, 'city_patrol', 'Under fuselage, looking toward patrol light', 'player'),
    (191, 201, 'ghost_approach', 'Climb rubble toward Ghost glow', 'player'),
    (202, 213, 'ghost_searching', 'Ghost calls out; subtitles while player approaches', 'authored'),
    (214, 218, 'ghost_collapse', 'Camera collapses; Ghost approaches', 'authored'),
    (219, 223, 'ghost_recognition', 'Ghost recognizes living Guardian; healing begins', 'authored'),
    (224, 224, 'ghost_heal_column', 'Visible healing column', 'authored'),
    (225, 226, 'ghost_stand', 'Guardian stands; Ghost close to camera', 'authored'),
    (227, 229, 'ghost_control', 'First-person view restored and health white', 'authored'),
    (230, 257, 'city_escape', 'EXODUS objective and Ghost held while leaving canal', 'player'),
    (258, 271, 'city_broadcast', 'Tunnel traversal with emergency broadcast subtitles', 'player'),
    (272, 276, 'city_exit', 'Greenery beyond tunnel; Ghost dialogue ends', 'player'),
    (277, 286, 'load_outskirts', 'Fade and loading interval', 'loading'),
    (287, 290, 'card_outskirts', 'City Outskirts title card', 'cinematic'),
    (291, 291, 'outskirts_fade', 'Black transition', 'loading'),
    (292, 304, 'outskirts_arrival', 'Mountain path; Ghost retracts; sidearm visible', 'player'),
    (305, 312, 'camp_approach', 'Approach camp and banner at summit', 'player'),
    (313, 325, 'camp_dead', 'Dead Guardians; Ghost raised; Arm Yourself objective', 'player'),
    (326, 332, 'camp_weapon', 'Weapon on orange tarp beside body; interaction pause', 'interaction'),
    (333, 338, 'camp_equip', 'Weapon awarded, removed from tarp, SMG equipped', 'authored'),
    (339, 343, 'camp_beasts_arrive', 'Player aims up slope; first War Beasts approach', 'combat'),
    (344, 371, 'camp_combat', 'War Beast fight, close melee, shots and reloads', 'combat'),
    (372, 378, 'camp_clear', 'Fighting ends and player turns toward exit', 'player'),
    (379, 388, 'camp_exit', 'Ghost raised and follow objective; snow route upward', 'player'),
    (389, 396, 'load_gap', 'Fade and loading interval', 'loading'),
    (397, 400, 'card_gap', 'Twilight Gap title card', 'cinematic'),
    (401, 401, 'gap_fade', 'Black transition', 'loading'),
    (402, 413, 'gap_arrival', 'Snowy pass; Ghost lowers; falcon dialogue subtitle', 'player'),
    (414, 423, 'gap_icicles', 'Narrow snowy route beneath icicles', 'player'),
    (424, 439, 'gap_city_view', 'Traveler and City vista beside exposed cliff', 'player'),
    (440, 449, 'gap_carrier', 'Cabal carrier flies above distant City; player watches', 'authored'),
    (450, 464, 'gap_falcon', 'Route turns; falcon takes flight; player follows bends', 'player'),
    (465, 483, 'bowl_approach', 'Ghost raised then lowered; golden tree clearing opens', 'player'),
    (484, 495, 'bowl_opening', 'First shots into War Beast pack on slope', 'combat'),
    (496, 537, 'bowl_combat', 'Sustained War Beast and Legionary combat', 'combat'),
    (538, 548, 'bowl_cleanup', 'Final shots, reload and weapon swap', 'combat'),
    (549, 555, 'canyon_approach', 'Walk to canyon arch with Ghost raised', 'player'),
    (556, 584, 'canyon_jumps', 'Descend golden canyon by jumping between ledges', 'player'),
    (585, 592, 'rescue_black', 'Abrupt black after canyon traversal, then loading', 'loading'),
    (593, 600, 'rescue_awake', 'Wake under forest sky; Hawthorne leans into view', 'cinematic'),
    (601, 608, 'rescue_stand', 'Hawthorne helps Guardian stand; group reacts', 'cinematic'),
    (609, 616, 'rescue_ghost', 'Ghost and Hawthorne exchange before departure', 'cinematic'),
    (617, 621, 'rescue_louis', 'Falcon flies down and lands on Hawthorne arm', 'cinematic'),
    (622, 634, 'rescue_introductions', 'Hawthorne and Louis introduced; Guardian nods', 'cinematic'),
    (635, 638, 'rescue_shotgun', 'Hawthorne tosses shotgun; Guardian catches it', 'cinematic'),
    (639, 652, 'rescue_depart', 'Departure conversation and evacuation camp panorama', 'cinematic'),
    (653, 664, 'rescue_fade', 'Fade and black interval', 'loading'),
    (665, 677, 'ghaul_intro', 'Red Legion emblem; Ghaul at Traveler cage window', 'cinematic'),
    (678, 694, 'ghaul_pledge', 'Ghaul approaches camera and addresses captive', 'cinematic'),
    (695, 718, 'ghaul_consul', 'Consul enters; discussion of victory', 'cinematic'),
    (719, 764, 'ghaul_light', 'Discussion of the Traveler and acquiring the Light', 'cinematic'),
    (765, 793, 'ghaul_ambition', 'Consul speech; Ghaul turns to captive guest', 'cinematic'),
    (794, 816, 'ghaul_speaker', 'Speaker restraint descends; reveal and exchange', 'cinematic'),
    (817, 850, 'load_farm', 'Emblem fade and loading interval', 'loading'),
    (851, 863, 'farm_flyin', 'Ships above Farm valley and Shard vista', 'cinematic'),
    (864, 880, 'farm_arrival', 'Guardian transmats; camera pans across Farm', 'cinematic'),
    (881, 888, 'farm_fade', 'Fade to playable social space', 'loading'),
    (889, 947, 'farm_tyra', 'Tyra vendor conversation and class item selection', 'epilogue'),
    (948, 953, 'farm_walk', 'Vendor exit and brief Farm movement', 'epilogue'),
    (954, 954, 'farm_edit', 'Black edit before next vendor', 'editorial'),
    (955, 1001, 'farm_hawthorne', 'Hawthorne vendor conversation and reward menu', 'epilogue'),
    (1002, 1005, 'uploader_outro', 'Uploader closing ident', 'editorial'),
]


def main():
    manifest = json.loads((OUT / 'evidence.json').read_text(encoding='utf-8'))
    cues_path = OUT.parent / 'music/cue-map.json'
    cues = json.loads(cues_path.read_text(encoding='utf-8')) if cues_path.is_file() else {}
    matches = cues.get('acousticCandidates', [])
    records = []
    for first, last, beat, visual, owner in BEATS:
        assert first == len(records), (first, len(records))
        for second in range(first, last + 1):
            frame = OUT / 'frames' / f'{second:04}.jpg'
            assert frame.is_file(), frame
            acoustic = [m for m in matches if m['referenceStart'] < second + 1
                        and m['referenceStart'] + m['windowSeconds'] > second]
            records.append(dict(second=second, time=f'{second // 60:02}:{second % 60:02}',
                beat=beat, visual=visual, timingOwner=owner, frame=f'frames/{second:04}.jpg',
                videoReview='one-second contact-sheet review',
                musicVerification='aligned acoustic candidate' if acoustic else 'unresolved; absence is not established',
                musicMedia='; '.join(sorted({f'{m["tag"]:08X}' for m in acoustic})),
                musicNativeOrdinals='; '.join(str(v) for v in sorted({v for m in acoustic for v in m['nativeOrdinals']})),
                musicBestScore=max((m['score'] for m in acoustic), default=''),
                audioReview='Auditory review pending; dialogue and SFX can mask music.'))
    assert len(records) == manifest['frameCount'] == 1006
    data = dict(reference=manifest['url'], sourceHashes=manifest['files'],
        precision='One-second sampling; verify individual frame boundaries before subsecond claims.',
        timingPolicy='Walkthrough timestamps are evidence locations, never progression delays.',
        speechCaution='Automatic transcription is unreliable in the opening; do not use it as a completion or timing receipt.',
        scope='Adieu and its cinematics through Farm arrival. Vendor epilogue is recorded separately.',
        seconds=records)
    (OUT / 'storyboard.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    with (OUT / 'storyboard.csv').open('w', newline='', encoding='utf-8-sig') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    manifest['visualReview'] = 'All 1006 one-second samples reviewed in 17 contact sheets; see storyboard.json.'
    manifest['audioReview'] = f'{len(matches)} aligned acoustic windows linked to packaged music; auditory review and cinematic audio remain pending. Automatic speech output is not verification.'
    (OUT / 'evidence.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Wrote {len(records)} consecutive seconds in {len(BEATS)} reviewed beats.')


if __name__ == '__main__':
    main()
