"""Package a full or explicitly mission-scoped Lua validation run without installing or launching Destiny."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
import verify
import verify_lua
import native_test_inputs

ROOT = verify.ROOT
SCRIPTS = ('one_au.lua', 'homecoming.lua', 'launchpad.lua', 'omega.lua', 'deadly_trial.lua', 'gateway.lua', 'beyond_infinity.lua', 'deep_storage.lua', 'hijacked.lua', 'strike_pact.lua', 'strike_bond.lua', 'mission_pact.lua', 'mission_bond.lua', 'eater_of_worlds.lua', 'mercury_freeroam.json',
           'eden_freeroam.json', 'fleet_freeroam.json', 'polaris_freeroam.json', 'planet_x_freeroam.json',
           'tangled_shore_freeroam.json', 'dreaming_city_freeroam.json', 'infinite_abyss.json')


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', type=Path, required=True)
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument('--mission', choices=('hijacked', 'deep_storage', 'mercury_freeroam', 'eater_of_worlds', 'eater_contest'), help='Require only the named mission suites and the Release DLL.')
    scope.add_argument('--configuration', choices=('Release',),
                       help='Require the complete validation job set for this configuration.')
    args = parser.parse_args()
    out = args.validation.resolve()
    require(out.is_relative_to(ROOT / 'build/coo'), 'Validation must be inside build/coo.')
    require(not (out / 'package.json').exists(), 'Preserve existing package evidence; choose a new validation run.')
    require(not (out / 'payload').exists(), 'Payload directory already exists; inspect the previous packaging attempt.')
    results = json.loads((out / 'results.json').read_text())
    require(not json.loads((out / 'failures.json').read_text()), 'Validation contains failures.')
    require(all(r.get('validation') != 'compiled-only' for r in results), 'Compile-only results cannot be packaged as validated tests.')
    mission_scopes = {
        'hijacked': ('hijacked-release', ('hijacked_tests', 'hijacked_catalog_tests', 'Dawn')),
        'deep_storage': ('deep-storage-release', ('deep_storage_tests', 'Dawn')),
        'mercury_freeroam': ('mercury-reentry-release', ('retained_authority_scope_tests', 'Dawn')),
        'eater_of_worlds': ('eater-reactor-release', ('eater_of_worlds_tests', 'eater_of_worlds_roster_tests',
                                                   'player_position_tests', 'other_mission_protocol_tests', 'Dawn')),
        'eater_contest': ('eater-contest-release', ('eater_of_worlds_tests', 'eater_of_worlds_roster_tests',
                         'player_position_tests', 'other_mission_protocol_tests', 'native_nightfall_power_tests',
                         'nightfall_rules_tests', 'mission_launch_lifecycle_tests', 'mission_launch_visual_tests', 'Dawn')),
    }
    if args.mission:
        scope, projects = mission_scopes[args.mission]
        expected = {(name, 'Release') for name in projects}
    elif args.configuration:
        scope = 'full-lua-release'
        expected = set(verify_lua.validation_jobs(configurations=(args.configuration,)))
    else:
        scope, expected = 'full-lua', set(verify_lua.validation_jobs())
    require(len(results) == len(expected) and {(r['project'], r['configuration']) for r in results} == expected,
            f'Validation results do not match the required {scope} scope.')
    sources = json.loads((out / 'source-manifest.json').read_text())
    require(sources == verify_lua.source_manifest(), 'Source changed after validation; validate the current source.')
    for result in results:
        folder = out / (result['project'] + '-local') / result['configuration']
        binary = folder / ('steam_api64.dll' if result['project'] == 'Dawn' else result['project'] + '.exe')
        require(verify.digest(binary) == result.get('sha256', result.get('binarySha256')), f'Binary changed: {binary}')
        if result['project'] == 'Dawn':
            require(verify.digest(folder / 'steam_api64.pdb') == result['pdbSha256'], 'PDB changed after validation.')
    inputs = {
        'steam_api64.dll': out / 'Dawn-local/Release/steam_api64.dll',
        'steam_api64.pdb': out / 'Dawn-local/Release/steam_api64.pdb',
        'Lua_LICENSE.txt': ROOT / 'Dawn/vendor/lua/LICENSE.txt',
    }
    inputs.update({'Dawn/scripts/' + name: ROOT / 'Dawn/scripts' / name for name in SCRIPTS})
    release = next(r for r in results if r['project'] == 'Dawn' and r['configuration'] == 'Release')
    expected_payload = {name: sources[source.relative_to(ROOT).as_posix()] for name, source in inputs.items() if not name.startswith('steam_api64.')}
    expected_payload.update({'steam_api64.dll': release['sha256'], 'steam_api64.pdb': release['pdbSha256']})
    payload = out / 'payload'
    for name, source in inputs.items():
        target = payload / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        require(verify.digest(target) == expected_payload[name], f'Payload copy failed: {name}')
    manifest = {
        'format': 1, 'createdUtc': datetime.now(timezone.utc).isoformat(),
        'buildsAndTests': len(results), 'compilerWarnings': 0, 'validationScope': scope,
        'validationJobs': sorted(f"{r['project']}|{r['configuration']}" for r in results),
        'sourceManifestSha256': verify.digest(out / 'source-manifest.json'),
        'unavailableHistoricalProofs': native_test_inputs.CAPTURE_ONLY if not args.mission else {},
        'files': expected_payload,
        'previousFiles': {name: verify.digest(ROOT / name) for name in inputs if (ROOT / name).is_file()},
    }
    with zipfile.ZipFile(out / 'source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in sources:
            data = (ROOT / name).read_bytes()
            require(hashlib.sha256(data).hexdigest() == sources[name], f'Source changed while archiving: {name}')
            archive.writestr(name, data)
    require(sources == verify_lua.source_manifest(), 'Source changed during packaging.')
    require(all(verify.digest(payload / n) == h for n, h in expected_payload.items()), 'Payload changed during packaging.')
    (out / 'package.json').write_text(json.dumps(manifest, indent=2))
    with zipfile.ZipFile(out / 'lua-missions.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in inputs:
            data = (payload / name).read_bytes()
            require(hashlib.sha256(data).hexdigest() == expected_payload[name], f'Payload changed while archiving: {name}')
            archive.writestr('payload/' + name, data)
        for name in ('package.json', 'results.json', 'failures.json', 'source-manifest.json', 'source.zip'):
            archive.write(out / name, name)
    receipt = {'archive': str(out / 'lua-missions.zip'), 'sha256': verify.digest(out / 'lua-missions.zip'),
               'bytes': (out / 'lua-missions.zip').stat().st_size, 'installed': False}
    (out / 'delivery.json').write_text(json.dumps(receipt, indent=2))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
