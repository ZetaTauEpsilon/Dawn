# Adieu presentation repair — 20 September 2026

This candidate corrects the movie order, native clock, falcon startup and mountain
exit in the playable Adieu reconstruction. Delivery is a separate DLL. The user
explicitly requested no installation and no game shutdown; neither is part of
this repair's build or delivery.

## Evidence and corrections

The supplied transcript places the vision before the City, two fades during
traversal, then the fall, Hawthorne's rescue and Ghaul/Speaker. The reference video
was sampled at one-second intervals in 1,006 frames. The retained storyboard is
`build/coo/adieu-research/video/storyboard.json`; the rescue starts at approximately
09:53 and Ghaul at 11:05 in that specific walkthrough. These are reference
locations, not timers used to advance the mission.

The old implementation incorrectly equated the three type-6 cinematic owners
with the three section cards. Native package references distinguish them:

- Region 25 / `DBB4B7DE`: entity `80FC0F93`, resource `815AC0B1`, sequence name
  `EB73EDEB` — opening vision.
- Region 26 / `251FFE4E`: entity `80FC0FA8`, resource `80FC0FA7`, sequence name
  `7019D8DA` — Hawthorne rescue, with an in-engine cast and performances.
- Region 27 / `DC3FB621`: entity `80FC134E`, resource `815AC444`, sequence name
  `CDE0A3CB` — Ghaul/Speaker.

The scenario's `transition_card_01/02/03.sequence.tft` references are separate
assets (`80C10460/62/64`). They do not identify the three type-6 movies. The
candidate uses the existing native fades and direct authored gameplay arrivals
between sections; those separate native title-card sequences are not yet driven.

After the vision, routes 4, 5 and 6 lead to the City, Outskirts and Twilight Gap.
The final endpoint now requests route 2 (rescue), its native completion requests
route 3 (Ghaul), and only Ghaul's accepted end completes the activity. Skip requests
stop playback and still require the native end receipt. Timeouts never mark the
mission complete. Farm arrival after the ending is not implemented by this repair.

Adieu's `gameplayClockTicks` was never assigned and remained zero, although the
packet encoder published it as the native activity clock. It now uses the shared
monotonic clock at 673,200 ticks per second. This is a confirmed clock defect;
its exact contribution to each reported animation/audio symptom still needs
in-game confirmation.

Falcon objects and selectors were previously requested together, sometimes with
the flight input in the first publication. Object casts now require native ready
receipts before selector publication. Flight inputs wait for the qualified
selector startup and then publish with a new revision. All mountain falcons are
staged on region arrival. Previous section scenes and objects retire at departure.
The new `scene_started` log records each accepted startup once.

The captured playthrough admitted and killed all twelve camp sources, but only
sources 171 and 172 in the mountain bowl. Sources 161–170 had no admissions in
that capture. Loose combat requests now carry the explicit authored spawn rule
and cumulative loose-request mode; scene reservations retain their separate
ownership. Native spawn success for these ten sources needs a fresh playtest.
Crossing the authored bowl exit `tv_goto_haw` also permits continued escape without
requiring every enemy to die. It does not mark survivors dead or fabricate combat
receipts. The final `tv_goto_arrival` volume then starts the rescue.

Gameplay music stays selected across both section transitions. Silence is selected
only at the final cliff, when native cinematic audio takes ownership. The existing
music inventory has 21 aligned acoustic candidate windows; aliased Wwise states
and masked audio prevent claiming exact retail cue timing from those matches.
The user-supplied transcript is the narrative reference, and the user's explicit
instruction establishes continuous gameplay music as the intended behavior.

## Validation and delivery limits

The Adieu unit executable passes 520 checks, including cast readiness, early
flight-input retention, monotonic clock progression, continuous music selection,
direct section transits, physical escape with survivors, cinematic ordering,
normal completion, skip handling, stale/foreign receipts and timeout rejection.
The generated catalog matches the retained package evidence.

This is a build candidate, not a completed visual/audio acceptance run. No new
game process was launched and no live state was modified to verify it. The
installed `adieu.lua` remains compatible; this repair changes no script contract.
DLL identity and build outcome are recorded alongside the delivered file.

## Live Ghost retirement diagnosis — 20 September 2026, 21:03 UTC

Read-only inspection of running PID 59212 confirmed the installed build reached
`healed=1 ghost_retired=1`. Both Ghost scenes had invalidated their selectors,
and actor sources 90/91 had accepted generation 2. The surviving visible Ghost
belonged to deferred object source `8577EEB1/4/118` (`80B5E288`): requested and
committed generation 2, active flag 0, valid weak entity `6DFAA201/B7A395DB`,
flags `271020`, and local native authority. Source 119 had no live entity.
Snapshots are retained in `build/coo/adieu-live-20260920-ghost-confirmed`.

The loaded engine's `9F19F0` deferred-source branch calls retirement routine
`9EFAE0` only if the incoming generation exceeds the source's previous generation.
Disabling an already-created deferred object at the same generation commits the
inactive state but leaves the entity alive. This explains why the controller's
retirement-intent log did not establish actual deletion. Both Ghost definitions
have the deferred flag at definition +94 set to 1.

Adieu now publishes one additional generation for retired Ghost objects 118/119,
with active=false. The value is stable on retransmission, remains inside the
run's existing 256-generation reservation, and does not change other missions'
object service or Adieu's other objects. Late creation/playback receipts remain
rejected. No process memory was written and the game was not stopped.

The user's selected kinetic in character `9EAA300100100102` was read through a
read-only database connection: item definition **53159281**, investment row 2509,
native tag `81319EAD`, display `8132973F`. It shares the name **Traveler's Chosen
(Damaged)** with the previously chosen definition 2362471601. The starting
loadout now uses the user's exact selected variant; once-per-run reset and later
weapon pickups retain their existing behavior.

Validation: 622 Adieu checks passed; installed package bindings and catalog
checks passed; Release x64 production compilation passed without linking a DLL.
`tools/testing/adieu_ghost_retirement_native.py` executes the captured generation
gate and retirement routine in offline Unicorn memory. Seven cases reproduce
same-generation retention and check next-generation deletion, repeated/stale
requests, newer entities, native ownership and expired references. Lookup and
final delete calls use explicit fixtures; this is not an in-game acceptance run.
The source changes have not been installed in the running game.

## Preserve Ghost's reunion dialogue — 20 September 2026

The subsequent playtest confirmed deletion works, but retiring the scene at
revive-animation completion cuts off Ghost's continuing speech. Healing and
player control still resume at that animation boundary. Object/source retirement
and scene shutdown now wait for the separate final dialogue action to finish.

The installed reunion graph `80FE032A` contains speech action `2060/5878`,
selector `0DD577ED`, bank `80C2AB74`, row 16: the speech ending with “We have to
get out of here.” Its authored duration is 18,186 ms. The existing qualified
selector observer now records finished speech actions separately from started
speech and child-animation completion. Cleanup requires final-row completion
plus the full authored voice window and a 250 ms tail from its first observation.
Repeated receipts do not extend this deadline; stale selectors cannot finish it.
The extra generation required for actual deferred-object deletion is retained.

Crossing the City exit early waits for the same boundary before unloading the
section. Queued follow-up dialogue also waits while the reunion is unfinished.
No timeout substitutes for an unobserved final speech action, and a first sample
that already reports completion conservatively reserves the full voice window.

Validation: 640 Adieu checks and installed package/catalog checks passed. Cases
cover animation completion during speech, missing final speech, early action
completion, duration without completion, stale receipts, retransmission, and
early City departure. This is a source-only update at the user's request; no DLL
was linked or installed and the running Homecoming session was left alone.
The project-wide compile encountered a temporarily missing Homecoming header
while the user was editing that mission, then was stopped to avoid racing that
work. The Adieu unit build completed; full-project compilation remains pending.

## Correct the dialogue gate after a captured City stall — 20 September 2026

Read-only inspection of PID 53392 found the controller healed, fault-free and
still in escape stage 2, with the City exit already visited. Its reunion selector
had ended natively and invalidated its weak reference. Matching installed debug
symbols located the controller without guessing its layout. Scene 102 retained
speech/finished-speech masks `CA00` (rows 9, 11, 14 and 15), output mask `18`
(healing and `B01280FE` departure), and natural child-performance completion.
Row 16 never started, and `reunionVoiceUntil_` stayed zero. The previous patch's
mandatory row-16 check was therefore impossible to satisfy. The captured evidence
is in `build/coo/adieu-stall-20260920-2044`, including native sensor snapshots,
debug-symbol field offsets and the read-only controller snapshot.

Cleanup now requires the already-qualified native departure cue `B01280FE`,
successful healing, and the protected audio windows for whichever reunion rows
14–16 actually started. It no longer requires optional row 16 or a final node
sample after native selector destruction. The longest applicable voice deadline
wins, retransmissions cannot extend it, and a missing departure cue still cannot
be replaced by elapsed time. The City exit remains protected until cleanup.

644 Adieu checks passed, including the captured `CA00` case with row 16 absent,
the exact audio-window boundary, and a missed final speech callback. Installed
package validation also passed. No full-project build, DLL installation, process
restart or live-memory write was performed for this correction.

The active root DLL reported hash
`A24CDC56270F91281016CF839833033D53B171F48EA29176338F0F81964B6572`.
The separate `bin/x64` DLL had a different hash,
`BF599BE16493D6B6B50B67A8CC43228CBDA8639BE5C9C7D2A56B0BA33598395C`;
that copy was not the loaded module in this captured run. Neither was replaced.

## Install and release the City-stall correction — 20 September 2026, 21:10 EDT

After authorization, the complete Release x64 production build succeeded with
all 2,282 captured build-input hashes unchanged. The current Homecoming source
was retained. The AMD64 DLL's 17 exports match the previously installed module.
The game was already closed; both DLL/PDB pairs were backed up separately and
replaced with hash verification. Normal launch started PID 60008, whose new
build-identity log confirms the installed candidate SHA-256:
`8B9F809E40281FCDB60C33019ACF9C7FDF38F109F150F910D659AF1ECE6D68FB`.

Build log, validation and installation receipts are in
`build/coo/adieu-dialogue-stall-20260920-205841-049`. The backup is under
`C:/Destiny 2 Development/.dawn/backup/adieu-dialogue-stall-20260920-205841-049`.
The standard player release is
`build/releases/Dawn-adieu-dialogue-fix-20260920.zip`, with a separate symbols ZIP
and checksum file. All 47 runtime payload entries and both ZIP archives passed
integrity checks. The 644 Adieu checks and package-binding validation reported
above apply to this source; a complete in-game mission playtest is still pending.

## Observe completion after the selector destroys itself — 20 September 2026

The next live run (PID 60008, DLL `8B9F809E...`) exposed a second observation
gap. Read-only capture in `build/coo/adieu-stall-20260920-2115` found the protected
voice deadline correctly populated and expired, but scene 102 retained output
mask `0C`, speech `CA00`, and finished-speech `4A00`: the final departure action
and final speech completion were both missed. The native Scene component had
`complete=1`, generation 1 and an invalidated selector. The observer previously
returned immediately when that selector no longer resolved, ignoring the durable
completion flag it had already read.

The observer now accepts a stable completed component after selector destruction,
with matching definition, scope and generation, using the previously observed
selector handle/serial from the current run. Completion cannot establish playback,
accept a different generation/run/selector, or restart a stopped scene. Cleanup
requires healing, an observed reunion voice deadline that has elapsed, and either
the departure cue or this qualified scene completion. Elapsed time alone still
cannot finish the scene. A new `scene_completed` diagnostic records this path.

658 Adieu checks passed, including the captured `0C/CA00/4A00` case, completion
before the voice deadline, completion first observed afterward, early City-exit
entry, stale identity rejection and late receipts after retirement. Installed
package binding validation passed. Production build and installation are pending
at this point in the repair history.

The production build subsequently passed with all captured inputs unchanged.
At 21:24 EDT the game was already closed. Both DLL/PDB pairs were backed up under
`.dawn/backup/adieu-scene-completion-20260920-211813-380`, installed and verified.
Normal launch started PID 31400; its loaded root module and build-identity log
confirm DLL SHA-256
`E0CFDEE9A77AD8F9BFBF888B23A32A8C02E9836012B24A0A87BBB0626BE53208`.
Receipts are in `build/coo/adieu-scene-completion-20260920-211813-380`.
The replacement release is
`build/releases/Dawn-adieu-scene-completion-20260920.zip`, with all 47 payload
entries verified and matching symbols provided separately. The failing live
session was inspected read-only; no live-memory patch was used. In-game replay
of the corrected build is still pending.
