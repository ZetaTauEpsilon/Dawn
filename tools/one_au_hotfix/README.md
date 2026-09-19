# Reproducible 1AU hotfix integration

This folder preserves the historical binary package recipe. Current source builds
compile the hotfix, no-wipe behavior and entrance repairs directly into the DLL;
see [the source integration notes](../../Dawn/docs/ONE-AU-ENTRANCE-SOURCE-PORT.md).
Do not apply this recipe to a newly compiled DLL.

Input: the exact `0.1.3-1au-no-wipe` package and user-supplied `0.1.3-Hotfix.zip`.
The latter's manifest calls itself `0.1.3-omega-fix`. All shared non-DLL payloads
are byte-identical, and it has no one_au.lua. Its DLL must not replace 1AU's DLL.

The published hotfix tag is the unmodified production source, and the matching
source ZIP named in its notes is absent from its release assets. This integration
implements and tests the fixes in the available 1AU source. It verifies the HUD
serialization byte-for-byte against offline execution of the supplied hotfix.
The documented upstream live playtest does not validate this combined build.

## Exact candidate changes

`candidate-patch.json` gates the entire input DLL by SHA-256. Three local search
regions call its existing registry+object-tag lookup, followed by the unchanged
descriptor validator. These cover reveal, boss, crown and four combat groups.
The existing lookup rejects duplicate full identities. No ordinal hints or
catalog-wide record copying remain in these seven searches.

Two type-18 serialization regions call a small helper assembled from
`activity-script.asm`. For archiveOmega it writes the hotfix's constructed time
state and clears the generic progress flag, retaining activityScriptState.
Every other mission keeps its previous output and buffer failure behavior.
The helper has assembler-generated Windows x64 unwind data. One new RX section
contains it and a sorted exception directory preserving every existing entry.
Original sections, imports, relocations, and previous repair bytes remain intact
outside the five declared code ranges and required PE header fields.

`source-port.patch` records the cumulative no-wipe and hotfix C++
changes against integration commit 1f00c834d3a95f57072114bfe8a2740f3c253338.
That historical patch does not contain the subsequent entrance C++ source port.
The package also retains the original entrance and no-wipe repair recipes.

## Reproduce and verify

Requires Python with pefile and unicorn for offline tests. Rebuild the supplied
assembler object with the MSVC 14.51 x64 ML64 assembler:

    ml64 /nologo /c /Fo activity-script.obj activity-script.asm

COFF timestamps can change the object hash; do not silently update the reviewed
recipe. The included object and recipe reproduce the exact released DLL.

    python test_candidate.py INPUT/payload/steam_api64.dll 0.1.3-Hotfix.zip
    python build_candidate.py INPUT 0.1.3-Hotfix.zip NEW_OUTPUT_DIRECTORY

The builder refuses an existing output, validates both manifests, preserves all
45 1AU payload files, and verifies the ZIP. It never installs into the game.

`Test-Setup.ps1 -SetupPath PACKAGE/Setup-Dawn.ps1` checks fresh/profile/update,
incomplete installation, restoration and WhatIf routing in disposable fixtures.
The underlying installer/updater are unchanged apart from upstream line endings.

For gameplay acceptance, cold-start and replay Omega's portal transition, then
1AU's entrance and a restricted-zone death. Confirm Omega's objectives work,
the ring stays absent, doors/console work, and 1AU respawns without a wipe.
