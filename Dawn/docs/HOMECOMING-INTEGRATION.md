# Homecoming integration candidate

Adds **Red War → Homecoming** to Dawn beside 1AU. Existing mission indices are
preserved; the Homecoming row that was hidden in the launch panel is now listed.

The mission logic is translated from the Sunrise-runtime Lua reconstruction in
[isinternets/Dawn pull request 8](https://github.com/isinternets/Dawn/pull/8)
(`mission_towerfall.lua`, "Homecoming v40") into Dawn's native mission
architecture, following the constraints recorded in the HomecomingAudit report
set (v20–v44). Nothing from the Sunrise script is loaded at runtime; Dawn cannot
execute that API. The Sunrise declarations became a compile-time C++ catalog and
nine immutable section graphs.

## Runtime

- Native activity 266, `mission_towerfall`, activity tag `0x80B500AC`, scenario
  `0x80B500BC`, package hash `0x9ACCB518`, dialogue bank `0x80C2AF61`.
- Opening bubble 9 (Underwatch), slice set 72. The spawn set stays absent on
  purpose: the authored arrival owns the point inside the slice, exactly as the
  existing Towerfall prelaunch profile already declared.
- Launch: the campaign panel requests activity 266 directly through the same
  `publish_direct` opening path as 1AU. The Chosen (282) donor fallback that the
  archived Towerfall experiment used is untouched.
- Lua entry: `Dawn/scripts/homecoming.lua` (mission id `homecoming`, profile
  `homecoming.native.v1`); native controller: `src/state/activity/vanilla/homecoming`.
- Cinematic owners: intro `0x964D8F24` (bubble 2, region 17), Amanda's pickup
  `0xE8B02346` (bubble 8, region 65), outro `0x42E8F541` (bubble 1, region 9).
  Route 4 returns to the Underwatch (region 72) after the intro; route 5 lands on
  the command ship (region 64) after the pickup. The roster synthesizes the three
  type-6 owners the same way 1AU does and retires the whole roster during the outro.

## Sections

The executor runs nine graphs, one per section, and hands over with the
`finishSection` mechanic: Underwatch crash site, Underwatch armory, military
wing, plaza defence, plaza assaults, boulevard and bazaar, command ship pods and
decks, generator room, escape. Section bubbles are 9, 9, 4, 6, 6, 0, 8, 8, 8.

- Encounters are cohorts of authored sources (`kCohorts`). The native spawner
  still owns template selection, placement and actor creation; the graph only
  requests categories. Sources with two, three or four authored categories use
  the per-category request encoding in `native_combatant_authority.h`. The
  relocated corridor source (military 13) requests 1/3/2 and the plaza waves
  request three or four of one category, matching the Sunrise declarations.
- Clears settle for three seconds before they count (audit constraint), and
  every cohort clear is derived from admission and death receipts, never from
  timers.
- Scenes publish the authored Scene with a reference array of its type-1 sources
  and type-4 objects only. Type-48 markers stay out of the array (the native
  consumer at `4E83B0` faults on them). Entry events (`0x6F51AC66`, the breach
  entry, Ikora's move and blast, Zavala's arrival, ward and death events) are
  appended as scene inputs at least 500 ms after activation.
- Zavala's death and revival use the type-2 named member (plaza 2/7): the member
  is retired on a fresh revision after the death scene and bound again after the
  native accepted-use receipt from the revive interactable (plaza 4/24).
- Objectives publish the sixteen authored directive events with native
  navigation-point markers (`kNavigation`); no world-coordinate markers.

## Timing

Dialogue cues are queued with explicit delays measured from the retail
walkthrough captions (YouTube `7_OVvrItocY`, cross-checked with four other
recordings): gameplay starts about 03:30 into the video, the first Ghost line
seven seconds after landing, Cayde's scene near 04:03, Shaxx at 04:23, the
hangar reveal at 05:38, the plaza lines from 07:35, the boulevard lines around
09:35–11:05 and the command-ship lines from 12:00 to 15:41 before the outro at
16:04. The graphs encode those offsets as `eventAfter` steps and per-cue delays;
the native clip durations from the bank gate the next cue, so lines never overlap.

## Native receipts and repairs

- Dialogue: the shared `DialogueService` now supports up to 128 rows (the bank
  has 94 authored cues); the dispatch probe accepts Homecoming's bank.
- Cabal console (ship 65/166): the Ghost sensor tick grants local authority on
  the console controller's owner (audit V30/V31) exactly like 1AU's bridge scan,
  and the scan completion is read from the type-65 receipt.
- Hangar gates, pod doors, ship enter/exit doors, contact doors and the console
  entity receive local authority once per live placement from the existing
  device tick (`homecoming_entrance.h`); no positions, locks or power are written.
- Bazaar door (boulevard 23/9): after the published opening is acknowledged the
  graph releases the door, and a game-thread poll sets the physics component's
  authored `state` to `destroyed` through the native named-state setter
  (`A1FBB0`), once, on the verified placement (audit V43).
- Generator turbines: the three generator devices report destruction when their
  measured pose reaches 0.4 (performer `80C23AA6`), which drives the shutdown
  and escape steps.
- Plaza registry `0x28A6B21F`: the ordinary roster walk classifies it as not
  relevant (second registry array, explicit slice). `registries.h` admits it to
  the scenario cache like the lost-sector and open-world catalogs; the cache
  format version moved from 65 to 66 so stale caches rebuild.

## Integration points

Every touchpoint mirrors 1AU: wire snapshot frame, authority body dispatch,
roster admission (`homecoming_roster.h`, groups resolved by key), membership
legs and transit, keepalive publication, sense/cinematic routing, opening fade
mask, position sampling, object/enemy/dialogue receipts, membership commit
guards, and the launch panel. The compact Tower Watch cue manifest is skipped
while the native module is prepared.

## Verification

- `Dawn/unit/homecoming_tests.vcxproj`: launch identity, catalog consistency,
  the nine section graphs, the shipped Lua entry, every authority body width in
  three frame states, the intro/pickup/outro cinematic sequence, the 94-row
  dialogue service, the door authority grants and stale receipts.
- Existing suites updated for the listed mission: `one_au_tests`,
  `mission_launch_visual_tests`, `player_position_tests`.
- The mission has not been played end to end in this session; the section
  graphs, timings and native repairs follow the audit and the retail recording
  and need an in-game pass.
