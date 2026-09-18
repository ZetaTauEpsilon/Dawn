# 1AU integration candidate

Adds **Red War → 1AU** to Dawn. Homecoming remains hidden and all existing mission
indices are preserved. No other Red War missions are included.

The mission implementation comes from
[`1AU-UnEx` at `04d37b2`](https://github.com/isinternets/evil-ass-repo-of-doom-and-despair/tree/04d37b2),
adapted to Dawn's production code at `c18b399` (0.1.3). Work is isolated on
`codex/integrate-1au`; the production branch and installed game have not been changed.

## Runtime

- Native activity 281, `mission_ember`, scenario `0x80B3C07D`.
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

The dedicated `Dawn/unit/one_au_tests.vcxproj` suite passes 468 checks covering
the launch identity, Lua entry, native authority widths, opening/ending sequences,
stale receipts, checkpoint handshake, reactor cycle and Ghost receipt parsing.
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

A live in-game playthrough has **not** been performed. Validate the opening,
all encounters and objectives, deaths/checkpoint reloads, reactor/escape sequence,
both ending cinematics, and returning to an existing mission on a separate test
installation before promoting this candidate to production.
