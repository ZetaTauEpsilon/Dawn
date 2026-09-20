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
  `0x80B500BC`, package hash `0x9ACCB518`, dialogue bank `0x80C2AF61`, music bank
  `0x80B5090F`.
- Opening bubble 9 (Underwatch), slice set 72. The spawn set stays absent on
  purpose: the authored arrival owns the point inside the slice, exactly as the
  existing Towerfall prelaunch profile already declared.
- Launch: the Red War opening is two activities. Public activity 265 (hash
  `0xF07A75D3`) is the pre-rendered opening video: it has no scenario package,
  carries the same video presentation block as Gateway's nameless entries 289
  and 290, and its first chain link is the mission (266, link kind 2, the kind
  the client advances by itself; Gateway's briefing-to-mission link is kind 1
  and needs the adapter). The campaign panel publishes the 1AU-style opening
  override for 266, scoped to the mission and its chain source, queues the
  video, observes playback through the native video manager at step 39, and
  takes the exact loaded Homecoming scenario as the arrival receipt
  (`prologue.h`). If the client stays in orbit for fifteen seconds after the
  video instead, the adapter launches 266 itself. Without the installed video
  entry the mission launches directly. The Chosen (282) donor fallback that the
  archived Towerfall experiment used is untouched.
- Lua entry: `Dawn/scripts/homecoming.lua` (mission id `homecoming`, profile
  `homecoming.native.v1`); native controller: `src/state/activity/vanilla/homecoming`.
- Mission cinematic owners: the ship approach `0x964D8F24` (bubble 2, region 17),
  Amanda's pickup `0xE8B02346` (bubble 8, region 65), the outro `0x42E8F541`
  (bubble 1, region 9). Route 4 returns to the Underwatch (region 72) after the
  approach; route 5 lands on the command ship (region 64) after the pickup. The
  roster synthesizes the three type-6 owners the same way 1AU does and retires
  the whole roster during the outro.
- The plaza (bubble 6, region 48) is an authored *public* bubble inside a
  private mission. The client's slice-set switch into it waits for a public
  activity host that never connects, so `region_private.cpp` reports it private
  for the whole native run regardless of the `regionPrivate` setting.

## Sections

The executor runs nine graphs, one per section, and hands over with the
`finishSection` mechanic: Underwatch crash site, Underwatch armory, military
wing, plaza defence, plaza assaults, boulevard and bazaar, command ship pods and
decks, generator room, escape. Section bubbles are 9, 9, 4, 6, 6, 0, 8, 8, 8.

- Encounters are cohorts of authored sources (`kCohorts`). The native spawner
  still owns template selection, placement and actor creation; the graph only
  requests categories. Placed encounters are requested when their section (or
  the preceding gate) starts, not at the encounter trigger, so squads stand in
  cover before the player sees them: overlook, friendlies and corridor squads on
  entering the military wing, the hangar floor when the first gate opens, every
  bazaar squad when the bazaar comes into view, the pod bays on landing on the
  command ship, the airlock, matrix and engine-room squads on entering the
  generator section. Only authored arrivals still drop in: the breach and
  Centurion Scenes, the corridor rush, the drop pods, the plaza waves.
- Every source an authored Scene casts is Scene-owned (`SCENE_OWNED` in the
  generator): the frames firing at the Cabal ship, the breach, Shaxx, Cayde,
  the Centurion, the hero moment, the post-gun guards, every civilian pose, run
  and reaction performance, and the hangar fake fight with its frames. Those
  sources never appear in a loose cohort; the standing civilian (36) and the
  aiming Red Guard (113), which stood idle in the Cayde and armory doorways, are
  not requested at all.
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

Beats follow the retail walkthrough captions (YouTube `7_OVvrItocY`, gameplay
begins at 03:30; cross-checked with four other recordings). Seconds after the
Underwatch landing:

| Beat | Retail | Graph |
|---|---|---|
| Ghost "Let's get moving" (1) | +7 | landing + 7 s |
| breach, "Watch out! / Cabal!" and cue 6 | +15 / +17 | wall-explode trigger, cues 4, 5 (+1.5 s), 6 (+4 s) |
| Cayde's golden gun | +33 | approach cleared, player near the door |
| Shaxx at the armory | +53 | player near Shaxx |
| armory music | +68 | `tv_music_shaxx_door` |
| "Look at the size of that thing" (34) | +128 | hangar window trigger |
| hangar music, Red Legion conversation (37) | +159 / +162 | `tv_music_cabal_ship_reveal`, cue 3 s later |
| first assault, "we are better" (52), "don't let them past the gate" (51) | +245 / +267 | Zavala's Cabal cleared + 5 s, 51 twenty seconds after 52 |
| "we hold here" (55), barrage "missiles, stay inside my shield" (57) | +319 / +329 | clear + 5 s, then +10 s |
| last wave "more Red Legion" (54), shuttles away (59) | +360 / +365 | after the revival, clear + 5 s |
| Ikora's Nova, Zavala's pickup order (72) | +399 / +409 | start trigger, blast + 10 s |
| "Holliday is inbound" (75), "someone told me you need a ride" (76) | +437 / +455 | bazaar entry, Hawk present |
| deck music, "kick them where it hurts" (77) | +502 / +507 | landing, +3 s |
| hologram (78), route (79), Cayde's status (82) | +532 / +543 / +565 | landing + 25 s, scan, scan + 20 s |
| "straight ahead" (85), sabotage music, "destroy the turbines" (86) | +666 / +670 / +696 | generator door, chamber trigger |
| "shields are down" (91), "headed topside" (92) | +726 / +731 | shutdown, +5 s |

The native clip durations from the bank gate the next cue, so lines never
overlap; the audio bank owns the Vanguard broadcast (cue 10) and the Scene-owned
Cayde and Shaxx lines.

## Music

Bank `0x80B5090F` has twenty authored sections; their FNV-1 names were recovered
from the descriptor: 0 `underwatch_ruins`, 1 `first_cabal`, 3 `after_cayde`,
4 `shaxx_door`, 6 `first_pod`, 12 `leaving_plaza`, 16 `battleship_deck`,
17 `battleship_sabotage`, 18 `run`, 19 `ending`. The graphs select them at the
authored music volumes and mission beats through the `music` mechanic. Sections 7
and 8 are used positionally for the hangar reveal and the plaza approach volumes
(`tv_music_cabal_ship_reveal`, `tv_music_plaza_cabal_ship`); their names did not
resolve and are marked inferred in `mission.h`. The remaining unnamed sections
are not selected.

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
  format version moved from 65 to 66 so stale caches rebuild. The plaza areas
  registry `0xF8F959CD` has no cache record and publishes nothing, so it stays
  out of the wire roster (admitting it failed every roster snapshot and killed
  the host).

## Integration points

Every touchpoint mirrors 1AU: wire snapshot frame, authority body dispatch,
roster admission (`homecoming_roster.h`, groups resolved by key), membership
legs and transit, keepalive publication, sense/cinematic routing, opening fade
mask, position sampling, object/enemy/dialogue receipts, membership commit
guards, and the launch panel. The opening video chain reuses Gateway's native
video poll and the direct-launch override and publishes no in-world owner of
its own. The compact Tower Watch cue manifest is skipped while the native
module is prepared.

## Verification

- `Dawn/unit/homecoming_tests.vcxproj`: launch identity, catalog consistency
  (cache-record invariant, Scene casts, loose cohorts free of Scene-owned
  sources), the nine section graphs, the shipped Lua entry, every authority body
  width in three frame states, the approach/pickup/outro cinematic sequence, the
  opening video chain, the 94-row dialogue service, the music
  selection, the door authority grants and stale receipts.
- Existing suites updated for the listed mission: `one_au_tests`,
  `mission_launch_visual_tests`, `player_position_tests`.
- In-game: the first playtest confirmed the ship-approach movie, the Underwatch,
  the Cayde and Shaxx scenes, the armory and the military wing up to the plaza
  entry. The plaza public-bubble hang, the idle doorway civilians, the drop-in
  spawns, the missing opening cinematic and the cue and music timing found in
  that test are addressed above and need another pass. The second playtest
  chained the Tower approach movie (`cine_110_twr`) as the prologue, which is
  not the Red War opening; the video activity above replaces it and has not
  been played yet.
