# 1AU integration candidate

Adds **Red War → 1AU** to Dawn. Homecoming remains hidden and all existing mission
indices are preserved. No other Red War missions are included.

The mission implementation comes from
[`1AU-UnEx` at `04d37b2`](https://github.com/isinternets/evil-ass-repo-of-doom-and-despair/tree/04d37b2),
adapted to Dawn's production code at `c18b399` (0.1.3). Work is isolated on
`codex/integrate-1au`; the production release is unchanged. The local development
game uses the separate 1AU candidate for playtesting.

## Runtime

- Native activity 281, `mission_ember`, activity tag `0x80B3C07D`, scenario `0x80B3C09E`.
- Opening bubble 8, slice set 64, spawn set `0x2EA8FB98`.
- Lua entry: `Dawn/scripts/one_au.lua`; native controller:
  `src/state/activity/vanilla/one_au`.
- Imports the encounter graphs, opening and ending cinematics, objectives,
  reactor and sun hazards, transport, checkpoint recovery and native receipts.
- Keeps 1AU's combatant/presentation encoders private and adapts existing shared
  receipts to the imported contracts. Cinematic sibling-host revision handling
  is restricted to 1AU's exact activity/package selection.
- Release packaging includes the mission entry automatically. The developer
  candidate installer and Lua packager include it as well.

## Verification

The full x64 Release DLL builds successfully without compiler or linker warnings.

The dedicated `Dawn/unit/one_au_tests.vcxproj` suite passes 510 checks covering
the launch identity, Lua entry, native authority widths, opening/ending sequences,
stale receipts, checkpoint handshake, reactor cycle, Ghost receipt parsing and
the bridge console's native owner authority guards.
`Dawn/unit/activity_host_lifecycle_tests.vcxproj` covers region transitions,
including the 1AU-only sibling-host exception and rejection of changed records.

All 13 existing Release regression suites passed:

- Mission prelaunch, launch arguments, launch lifecycle and launch visuals.
- Player position and activity sense parsing.
- Other-mission protocol and native roster/lifetime wire checks.
- New Light integration, A Deadly Trial, combat, combat runtime and universal services.

The visual test renders the actual launch panel and verifies that Red War lists
only 1AU and selects activity 281. Existing mission fixtures and game fonts were
read without modifying the game installation.

## Bridge scan fix

The September 18 local playtest reached the bridge console after the opening
and defenders. Logs stalled at `F6FFB59E/65/60` with no completed Ghost receipt.
Read-only inspection during the player's interaction showed:

- Source `80B3C98F` was enabled at generation 258. Its native controller
  `80C3D38C` was in mode 2, with duration 3 seconds and elapsed time 0.
- Ghost was in mode 5 and bound to that exact salted controller reference at
  interface offset `0x30`. All six native participant references remained empty.
- The controller owner's bit in the native authority table was clear.
  Native `E50740` checks this bit before adding Ghost; without it, registration
  fails and the scan never begins.

The existing `E4A590` scan tick now repairs only this bridge controller owner's
authority using the signature-checked native setter `403BD0`. It validates the
active 1AU bridge request, source definition/registry/type/slot, generation,
controller definition and playback, salted component/entity identities, and
live entity row twice before the handoff. A retired run, checkpoint handoff,
foreign source, stale entity or different client signature prevents the write.
It does not summon Ghost, insert participants, change elapsed time, or manufacture
completion. Native type-65 receipts still control bridge extension.

The repair logs `ev=one_au stage=bridge_authority ... result=local`; the normal
receipt path then logs `stage=ghost`. Tests reproduce the stalled native values
and verify the exact owner grant, unchanged timer/participants/neighboring bits,
and rejection of stale or foreign identities. After this fix, the 1AU suite and
A Deadly Trial (6,838 checks), other-mission protocol, and host lifecycle suites
pass. The Release DLL builds with zero warnings and errors.

The corrected scan and a full mission playthrough still require in-game
verification. Validate bridge extension, subsequent encounters, checkpoint
reloads, reactor/escape, both endings and a return to an existing mission before
promoting the candidate to production.
