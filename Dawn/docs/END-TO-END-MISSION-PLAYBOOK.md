# Dawn mission reconstruction: end-to-end implementation playbook

Rewritten for the Dawn checkout inspected on 20 September 2026. This document
adapts the supplied Sunrise playbook to Dawn's actual mission loader, executor,
native authority, observations and delivery tools.

The target is a playable reconstruction of the selected mission: launch, traversal,
encounters, authored scenes, interactions, devices, objectives, ending and recovery.
Dialogue and music belong in the coverage plan when they are part of the requested
experience.

**Architecture baseline:** Dawn evaluates Lua at load time to produce a validated,
immutable mission definition. The Lua VM then closes. C++ executes the definition,
publishes native authority and consumes qualified observations during gameplay.
There are no gameplay Lua callbacks or live script reload in this model.

**Ownership varies by mission.** Gateway and A Deadly Trial author progression in
Lua. Omega's Lua graphs still work within native phase contracts. Homecoming's Lua
is a composition entry point; its nine section graphs and story mechanics are
currently implemented in C++. Inspect the selected mission before deciding where
to make a change.

Source examples below describe the inspected checkout, including ongoing
Homecoming edits. They establish implementation behavior, not fresh in-game
acceptance of those edits. Recheck source and installed hashes for later work.

## 1. What complete means

A complete mission has a defined owner and an observable completion condition for
every required beat. Cover:

1. Activity selection, prelaunch identity, player spawn, world arrival and control.
2. Every progression trigger, including arming, early crossings and repeat visits.
3. Population requests, native admission, combat readiness, required deaths and
   optional survivors.
4. Scene cast, source reservation, actor bindings, event inputs, performance
   milestones, dialogue ownership and retirement.
5. Doors, blockers, consoles, projectiles, ships, effects, capture plates and other
   objects, including their starting states and native feedback.
6. Actual interaction start/completion and the conditions that enable an interaction.
7. Objectives, markers, music, region transitions and generated routes where present.
8. Cinematic start, normal end, skip, mission completion and the intended destination.
9. Death, wipe, restart, backtracking, streaming changes and late receipts.
10. Fresh in-game verification of presentation, combat, collision and traversal.

Preserve authored engine behavior where it works. Supply the missing mission
decisions, bindings or narrowly justified native support. Mark unsupported or
unverified behavior explicitly; a successful build does not make it complete.

## 2. Evidence and the meaning of success

Keep these evidence classes separate:

- **Package evidence:** asset identities, scene parameters, source definitions,
  placements, device branches and authored geometry.
- **Source-confirmed behavior:** what the current Dawn loader, graph, service,
  encoder or observer actually does.
- **Native observation:** a qualified receipt or read-only capture from a known
  process/build and the correct live owner.
- **Reference observation:** what the supplied walkthrough shows or plays.
- **Reconstruction choice:** a deliberate decision where original behavior is not
  established, such as encounter grouping or an estimated pacing delay.
- **Acceptance evidence:** a recorded result from the exact installed candidate,
  including what the player saw, heard and traversed.

Dawn's executor distinguishes `Wait::requested`, `Wait::nativeReady`,
`Wait::completed` and `Wait::observed`. They are not interchangeable:

- `requested` means the service accepted publication of the command.
- `nativeReady` requires the corresponding readiness milestone.
- `completed` requires a separate completion receipt; the executor rejects it
  before readiness.
- `observed` waits for an observation or event-relative condition, authenticated
  and interpreted by the selected adapter.

A capability fixes its operation and receipt policy in the trusted profile. Its
name alone does not establish its semantics. An action using `requested` can be
followed by a separate observation step for the actual result, as Homecoming does
for several doors and scenes.

Publication does not prove actor creation, scene playback, a moved door, a dead
enemy or a completed mission. Even native consumption may precede visible motion
or audio completion. Follow the evidence through to the effect that gates the route.

Source: [executor](../src/state/activity/coo/executor.h),
[native service bridge](../src/state/activity/coo/native_services.h).

## 3. Start with the current implementation and an inventory

Read the entire selected mission, including its Lua, profile or entry contract,
catalogs, controller, runtime, authority writers, observers, reset paths and tests.
Do not stop at the Lua file: a composition-only script contains very little of the
actual mission logic.

Use these project references as orientation, then verify their claims in source:

- [Lua mission authoring](LUA-MISSION-AUTHORING.md).
- [Universal mission services](UNIVERSAL-MISSION-SERVICES.md).
- [New mission reconstruction guide](NEW-MISSION-RECONSTRUCTION-GUIDE.md).
- [Mission implementation template](MISSION-IMPLEMENTATION-TEMPLATE.md).
- [Mission scripts](../scripts/README.md).
- [Mission compiler](../src/state/activity/coo/mission_script.cpp) and
  [Lua front end](../src/state/activity/coo/script_lua.cpp).

Older documents contain historical suite counts, migration status and installation
paths. The current source and matching build receipts determine what is supported.

Record the branch/commit, relevant working changes, game executable version,
installed DLL/script hashes, selected launch settings and existing acceptance
evidence. Preserve unrelated work. Keep research in a new directory under
`build/coo/<mission>-research/`, using paths appropriate to this checkout.

Build machine-readable inventories for:

- Activity index, investment/activity/scenario assets, launch descriptor, bubble,
  region/slice and spawn set. These are distinct identifiers.
- Authored registries and each relevant asset's registry, definition, type and slot.
- Player triggers and occupancy volumes, including their geometry and visit scope.
- Population sources, category/member counts, spawn rules, placements and tactical
  task groups; distinguish loose populations from scene-owned cast.
- Scenes, full ordered cast arrays, actor cells, child graphs, event inputs and
  available start/performance/completion observations.
- Type-23 devices and position/power/lock branches; type-4 objects and their native
  controllers, interaction/destruction contracts and lifetime.
- Type-5 sequences, type-6 cinematics, Ghost-link sensors, portals and generators.
- Dialogue selectors, scene-owned lines, objectives, marker targets and music cues.

Join assets by actual identity and ownership. Names and proximity provide leads;
they do not establish that two records refer to the same object. Keep package
facts separate from reconstructed counts and grouping policies.

Inspect extraction/generation tools before running them. Some regenerate checked-in
catalogs by default. Use a real `--check` mode only where the tool supports it, and
keep another mission's output paths out of new research.

## 4. Model the route and choose the correct owner

Create a beat record for every stage before implementing it. Each record needs:

- A stable name, region and visit/attempt scope.
- Entry evidence and prerequisites.
- Requested populations and the exact required-clear cohort.
- Scenes, dialogue, devices, objects, objectives and music.
- The native receipt or observation that permits progression.
- Optional branches and what may remain alive or active at departure.
- Retirement, cancellation, backtracking and checkpoint behavior.
- Evidence sources, reconstruction choices and remaining acceptance checks.

Assign one owner to each source activation, scene, door transition, dialogue row
and progression decision. Several observers may report facts; they must not become
competing mission controllers.

For a new mission, prefer Lua for story decisions among registered capabilities,
trusted C++ bindings for native identities and supported operations, and shared
services for reusable runtime behavior. A new capability still needs actual native
support; inventing a Lua name cannot create an engine event.

Preserve the selected mission's existing architecture unless migration is part of
the task. In particular:

- [Gateway Lua](../scripts/gateway.lua) owns its dependency graphs and selected cues;
  its [bindings](../src/state/activity/gateway/bindings.h) supply native capabilities.
- [Homecoming Lua](../scripts/homecoming.lua) starts `mission.native` and waits for
  `mission.finished`. Its [entry contract](../src/state/activity/vanilla/homecoming/entry.h)
  registers `homecoming.native.v1` with the `otherMissions` schema.
- Homecoming's [mission graphs](../src/state/activity/vanilla/homecoming/mission.cpp)
  cover Underwatch, Armory, Military, Plaza, Plaza assaults, Boulevard, Command ship,
  Shield generator and Escape. Their orchestration is currently native C++.

Do not copy Omega's additional phase constraints or another mission's authority
schema simply because that mission is a convenient example.

## 5. Dawn's execution and observation pattern

Use the existing bounded executor and mission services:

1. Load and validate the document against the selected trusted profile. Pin the
   immutable definition for its entire use.
2. Establish the mission run and lifecycle lease. Connect authenticated selection,
   seed and world-arrival conditions through the adapter's existing launch path.
3. Publish a step's commands when its dependencies are satisfied. Independent
   branches may run together; Dawn does not require one outstanding request globally.
4. Let services request the native action through the correct authority schema.
5. Authenticate incoming evidence against the mission, asset, generation and live
   owner, including entity/controller/selector identity where applicable.
6. Update the service ledger or retained observation, then enqueue the milestone
   for the active executor token: `run`, `incarnation`, `step`, `command`.
7. Join the exact receipts declared by the graph. Retire owned work on cancellation,
   failure or a deliberately defined phase exit.

`Services::publish` and `cancel` must not call back into the executor. The owner
serializes execution; native hooks pass copied, validated evidence through the
appropriate queue/lock boundary. A receipt for an inactive step is rejected, so
retain valid early facts in the service or adapter when the design needs them later.

Lua builders provide `graph`, `step`, `sequence`, `parallel`, `command`, `condition`,
`any_of` and `all_of`. They construct data. Conditions combine supported
observations; they cannot perform actions or turn elapsed time into a kill receipt.
The compiler resolves dependencies and rejects invalid/cyclic definitions.

The embedded server owns mission decisions and authority publication. The client
engine performs native behavior and reports evidence. This is an in-process
architecture with mission-specific adapters, not a claim that every mechanism has
already been replaced by a universal server service.

Use existing diagnostics to distinguish definition, publication, queue-overflow
and native failures. Identify the missing prerequisite before retrying. A retry
policy belongs to the relevant service and must preserve identity, boundedness and
deduplication; the executor does not provide a universal retry-to-success loop.

Source: [executor](../src/state/activity/coo/executor.h),
[mission runtime](../src/state/activity/coo/mission_runtime.h),
[stall diagnostics](../src/state/activity/coo/stall_diagnostics.h).

## 6. Population requests, readiness and encounter clearance

Define separately when a source is preloaded, requested, admitted, ready for its
intended behavior, required dead and retired. A single "spawned" flag cannot express
all of these states.

For each encounter:

- Recover category counts, members per category, placements and spawn rules. Compute
  expected actors from the actual request policy, not the number of catalog rows.
- Request populations at the intended encounter boundary, allowing authored arrival
  presentation and streaming. Avoid activating every source at mission arrival.
- Supply the correct tactical group and readiness policy. Combat readiness can
  require creation, a typed health owner, AI ownership and the authored assignment.
  An intentionally idle reveal is a distinct supported policy.
- Keep scene-owned sources under their scene. Do not independently place a hero,
  civilian or enemy that the scene already owns.
- Require admission of the expected cohort and real deaths for a clear gate. Initial
  zero, an unknown population, a timeout, a retired source or an empty sample does
  not establish that the required enemies died.
- Keep optional survivors and background fights out of a required-clear cohort.
  Explicitly define any survivor-safe phase exit.
- Handle early authenticated deaths, duplicate reports, delayed readiness and stale
  generations without inventing completion.

The shared [population service](../src/state/activity/coo/population_service.h)
tracks admitted identities and deaths. Native teardown and ledger reset are
separate operations. Streaming rebind/restart requires a qualified owner change;
it must not erase real deaths or let old actors satisfy a new encounter.

Homecoming examples in the current source:

- The first Military door excludes the allied-frame background fight from its
  clear cohort.
- The corridor uses source 13's request override and source 18; source 17's authored
  point is documented outside the corridor walls.
- Clear observations require a 3,000 ms settled clear in this adapter. That delay
  is Homecoming policy, not a universal executor rule.
- Command-ship populations are requested before the console scan completes.

When visible enemies are gone but the gate stays closed, inspect expected/admitted
counts, exact salted actor/source owners, death receipts, anchors and the native
readiness state. Report unknown capacity as unknown. Raising actor limits or
loosening a clear predicate does not diagnose the missing actor.

Source: [Homecoming cohorts](../src/state/activity/vanilla/homecoming/mission.h),
[controller](../src/state/activity/vanilla/homecoming/controller.cpp).

## 7. Authored scenes, cast and dialogue

Dawn already executes native scene graphs. Reconstruct each scene's inputs and
ownership before introducing another scene system.

For every scene:

1. Recover its parameter array and preserve the exact dynamic cast order, including
   doors, targets and other non-actor references.
2. Identify scene-owned population sources, named actor cells, retained actors and
   static points. Reserve/request/bind them through the supported native path.
3. Verify the chosen authority writer and its capacity. The shared cast writer
   currently accepts at most 15 references; mission-specific arrays can be smaller.
4. Activate the correct scene generation and publish only verified event keys in
   their required order.
5. Distinguish native start, input-revision application, entry cue, child performance
   completion, conversation completion and parent-scene completion.
6. Release combat AI, projectiles, collision or progression only on the milestone
   required by that beat.
7. Define actor retention and retirement across child-scene handoffs and region changes.

A stopped child is cancellation, not proof of natural performance completion. A
parent scene may keep an idle branch alive after its required conversation ends.
Choose the appropriate observation rather than waiting forever for that idle branch.

Homecoming's current graph gives concrete examples:

- Cayde uses seven authored participants, including both doors and the aiming
  target. The graph waits for `performanceFinished` before its collision handoff.
- Shaxx's performance gates the weapon racks.
- Ikora's projectile/event follows the observed `entryCue`; a guessed delay does
  not supply that cue.
- Zavala's arrival, combat, Ward, death and revival are separate scene transitions
  with explicit actor and interaction ownership.

These are source contracts under active development. Verify native playback and
visible choreography against the installed candidate before calling them accepted.

Give each dialogue row one owner: the native scene or Dawn's dialogue service.
Do not queue scene-owned lines a second time. Distinguish queue acceptance, native
submission, the speech window and whole-scene completion. A log that says a line
was submitted does not prove it was audible.

Timers need a documented origin. The shared event timeline can anchor a cue to a
qualified native event without restarting on duplicates. Homecoming's current
`eventAfter` observation instead measures from command activation. Inspect the
adapter; an operation name does not guarantee a particular clock origin. When
speech completion uses an authored duration after submission, label that explicitly.

Keep traversal presentation responsive while dialogue is queued. Latch valid
crossings so a fast player does not need to walk backward to replay a trigger.

Sources: [scene orchestration](../src/state/activity/coo/scene_orchestration.h),
[cast authority](../src/state/activity/coo/native_scene_cast_authority.h),
[event timeline](../src/state/activity/coo/event_timeline.h),
[Homecoming playback contract](../src/state/activity/vanilla/homecoming/scene_playback.h).

## 8. Devices, blockers and physical traversal

Track these facts separately: desired value, publication, consumed revision,
measured pose, visual motion, named destruction state, collision variant and actual
player passage.

Recover position/power/lock semantics and branch values from the authored device.
Do not assume position 1 always opens it or that every mechanism is binary.
Publish revisions through the existing writer and wait for the evidence required
by the route. A consumed revision alone does not prove motion finished.

Homecoming records the native measured pose separately from the target. Its device
handler synchronizes revision counters, rejects older feedback and acknowledges
the target only when the current revision's measured position matches it. Several
route gates use a separate `posed(...)` observation. Do not claim every device
request has that gate: inspect each graph edge.

For an invisible barrier, identify the precise placement/component responsible.
Use the authored object retirement or named-state transition that matches that
obstruction. The current Homecoming bazaar adapter waits for acknowledged opening
and a graph release, then requests the verified door physics component's authored
`state=destroyed` through the native setter. Removing nearby named blockers alone
does not establish that the door's collision slab changed.

After any such correction, test ordinary movement through the opening, including
the floor and frame. Neither a successful setter call nor a visually open door
proves passage.

Sources: [object service](../src/state/activity/coo/object_service.h),
[Homecoming device feedback](../src/state/activity/vanilla/homecoming/controller.cpp),
[bazaar door adapter](../src/state/activity/vanilla/homecoming/door_native.h).

## 9. Ghost scans, holds and capture plates

Define enablement, actor/object binding, accepted start, progress, interruption,
completion and retirement as distinct interaction states.

Homecoming's console flow currently:

1. Arms the exact type-65 Ghost link during the command-ship section.
2. Accepts finite positive progress only from the expected scan generation and
   current mission owner.
3. Requires a previously observed start, followed by an inactive report with
   progress at least 1 in that same expected generation.
4. Disables the link and publishes the console/door changes through the graph.

Its console-door action currently uses requested completion; do not describe that
edge as a separately observed door-pose gate. Add/verify an explicit pose condition
where a route dependency needs one.

Ghost disappearing, proximity or elapsed time cannot substitute for the accepted
completion report. If admission fails, inspect the exact console entity, authority,
sensor and interaction owner before changing progression.

Native holds also need their real use receipt. Homecoming's armory additionally
waits for the actual weapon-grant acknowledgement; accepted use alone is insufficient.
Zavala's revival uses its separately armed, qualified native interaction.

For capture plates and other scans, use the selected adapter's native timer and
playback contracts. Record charge duration, contest rules, step-off behavior,
completed-charge retention and visual pose independently. Missing enemy samples
do not mean a contested area is empty. Keep plate wave ownership separate from
charge ownership unless the authored design explicitly connects them.

Shared services and server intake cover much of this infrastructure, but some
mechanics remain mission-specific. Reuse proven boundaries and verify current
callers rather than assuming that an older migration report describes every active path.

Source: [Homecoming controller](../src/state/activity/vanilla/homecoming/controller.cpp),
[console binding](../src/state/activity/vanilla/homecoming/console_scan.h).

## 10. Objects, destructibles and objectives

Establish the exact source and current object owner before accepting an interaction
or destruction report. Object preparation, creation, entity/controller binding,
initial device state and readiness are separate milestones.

Use the shared destructible contract where applicable: bind the qualified owner,
apply its immune/vulnerable state and accept confirmed destruction of that owner.
Linked device values and source retirement belong to verified bindings. Absence
after streaming or deliberate retirement is not automatically destruction; accept
absence only if the selected native contract proves that meaning.

Homecoming's turbines use a different concrete signal: the controller watches
their generation-matched type-23 feedback for authored position 0.4. Their setup
and shutdown use fractional device values. The three turbine milestones drive
shutdown and the escape gate; this implementation does not count arbitrary object
disappearance as a turbine kill.

If a visible object retains an earlier creation generation during a later source
command, establish the exact retained-owner contract. Do not add a general rule
that accepts stale generations or repeatedly recreate an already bound object.

Set and replace objectives/markers through the mission's supported presentation
service and native writer. Clear them at the correct lifecycle boundary. The
objective text reports mission state; it does not itself prove the underlying
destruction, scan or traversal completed.

Sources: [object/destructible service](../src/state/activity/coo/object_service.h),
[objective service](../src/state/activity/coo/objective_service.h),
[Homecoming generator graph](../src/state/activity/vanilla/homecoming/mission.cpp).

## 11. Region transitions, cinematics and ending

Preserve the mission owner across valid region changes. A new bubble or packed
region is not automatically a new mission attempt. Recover the launch and transit
contract before treating missing story behavior as a scripting failure.

For portals and generated routes, verify both the departure and receiving ends:
contact, authority, transport, landing, playable geometry and the exit connection.
A gate flag cannot generate missing geometry. Repeated visits require current-visit
observations; a first-pass latch must not complete the return trip.

Before departure, define which work stops and which presentation persists. Cancel
or seal obsolete graph branches, queued work and receipts before their assets
unload. Executor cancellation invokes the adapter's retirement implementation;
it does not guarantee native cleanup if that implementation does nothing.

Homecoming's native cinematic sequence owns intro, Amanda's pickup and outro. It
tracks preparation, offer, accepted playback, stopping, return-to-gameplay landing
and terminal completion. Receipts are scoped to the mission owner, exact movie
source and valid phase. Timeout is failure, not simulated success.

In its current Escape graph, `Operation::complete` begins the outro handoff; it does
not immediately finish the mission. The controller publishes completion only when
the cinematic state reaches `complete`. Verify ordinary termination and the skip
path, plus rejection of termination before accepted start. Do not port Sunrise's
named cinematic callbacks into this native contract.

Audit late-event behavior against the actual graph. Homecoming explicitly stops its
escape scenes before the outro and its controller gates updates on cinematic phase;
its `cancel` callback is currently empty. Further retirement behavior must be
established in the graph/runtime, not inferred from the shared executor API.

The shared lifecycle normally publishes mission-complete state 6. It also has timed
completion and orbit policies; the selected adapter determines which applies.
Mission completion, an orbit return and automatic next-mission launch are separate
requirements.

Sources: [Homecoming cinematics](../src/state/activity/vanilla/homecoming/cinematics.h),
[controller](../src/state/activity/vanilla/homecoming/controller.cpp),
[lifecycle service](../src/state/activity/coo/lifecycle_service.h).

## 12. Reload, checkpoint and retry design

Dawn loads the mission script once per process. Editing it on disk does not alter
an active mission; death or a mission restart also does not reload that document.
Verify the next process's load log and source fingerprint after deploying changes.

Separate these recovery cases:

- Repeated trigger or duplicate native receipt within one attempt.
- Streaming out and back under a changed salted object/source owner.
- Section or module transition within the same mission.
- Death/wipe with restoration at an authored checkpoint.
- Full activity restart using a fresh lifecycle generation.
- New process loading a new DLL/script pair.

The executor incarnation and lifecycle generation protect against stale receipts.
The lifecycle preserves its generation high-water mark across reset, including reuse
of a run ID. These are runtime ownership protections, not a disk-persistence or
checkpoint serialization system.

Define checkpoint state deliberately: spawn/region, completed beats, surviving
populations, device/destructible state, retained cast, interaction arming, objectives,
dialogue and replay policy. Reconstruct fresh native identities after reset; do not
restore raw pointers or let old receipts satisfy new commands.

Homecoming has scoped source-restart behavior for an unfinished source returning
under a different native owner. That does not by itself establish same-room wipe
restoration or complete checkpoint fidelity. Keep those acceptance claims separate.

Do not carry over Sunrise's `on_load` persistence recipe. There is no corresponding
gameplay Lua callback here. Recovery belongs to Dawn's native adapter/services and
any explicitly implemented checkpoint mechanism.

## 13. When native changes are justified

First classify the missing behavior: a story dependency, missing binding, authority
field, native observation, ownership rule, delivery/thread requirement or genuinely
unsupported engine interaction.

Change the narrowest responsible layer:

- Rearrange supported mission decisions in the existing story owner.
- Add verified native identities and policies to the selected mission's bindings.
- Extend a shared service when the same mechanic is reusable.
- Add or change a native adapter only when evidence establishes the missing engine
  contract. Check existing hook registrations and callers before adding another one.

A native correction needs:

1. Evidence of the exact failing contract or missing capability.
2. Scope guards for the supported executable, mission, asset and current live owner.
3. Correct function signatures, instruction/layout validation, thread context and
   lifecycle behavior.
4. Engine-owned animation, interaction, physics or playback after the repair.
5. Rejection of malformed, stale, foreign or ambiguous identities.
6. Meaningful regressions and a fresh native acceptance check.

Repeated reflected rows can alias one valid component; distinct competing owners
are a different condition. Retained-object and missing-cast exceptions need their
own evidence and must not become blanket stale-owner acceptance.

Unsupported optional adapters should disable safely and report the missing
capability. If the route requires that capability, leave the mission blocked or
failed with an explanation rather than claiming success.

A temporary live intervention is diagnostic evidence. Carry the accepted correction
into source, test it and verify it after a clean restart. Do not turn a manually
set observation flag into permanent mission logic.

## 14. Resource limits that shape the design

Current shared source limits are:

- Executor: 32 steps per graph, 8 commands per step and 128 queued events.
- Mission composition: up to 8 modules and 32 observation mappings.
- Compiled Lua document: up to 64 graphs and 8 authored phase entries.
- Conditions: up to 64 declarations, with bounded 64-node expressions and depth 8.
- Lua evaluation: 1 MiB source, 8 MiB allocator budget, 1,000,000-instruction budget
  and a 30,000-node conversion bound, with additional nesting/table checks.
- Shared scene-cast authority: up to 15 cast references and 32 event inputs.

Profile, wire-format and mission-local storage limits also apply. For example,
Homecoming's nine native sections use their own fixed graph storage and each obeys
the executor's per-graph bound; they are not nine entries in Lua's eight-phase list.
Its controller has smaller local scene/event and per-source population capacities.

The bundled Lua compiler still has ordinary language limits. Split definitions
into deliberate graphs/modules and use supported builders; do not copy Sunrise's
131072-byte source limit, 512 state variables or 32 runtime timers into Dawn guidance.
Minifying a document does not solve an executor or native capacity limit.

Sources: [executor](../src/state/activity/coo/executor.h),
[mission compiler](../src/state/activity/coo/mission_script.cpp),
[Lua evaluator](../src/state/activity/coo/script_lua.cpp),
[Homecoming storage](../src/state/activity/vanilla/homecoming/mission.h).

## 15. Verification ladder

Scale validation to the changed behavior and run the required delivery scope:

1. **Inventory and binding checks:** identities, geometry, scene cast/events,
   population counts, device values and authority schema agree with evidence.
2. **Definition checks:** evaluate the full Lua document against its actual profile;
   validate native section graphs, dependencies, observations and capacity limits.
3. **Controller/service tests:** normal route, early/duplicate/late receipts, wrong
   owners, missing readiness, real deaths, optional survivors, interrupted scans,
   exact timer boundaries, phase cancellation and reset.
4. **Integration and protocol tests:** adapter routing, native authority encoding,
   source lifetime, objective/dialogue publication and independent wire fixtures.
   Keep encoder and expected-data derivation independent where possible.
5. **Affected regressions:** preserve existing missions when changing shared services,
   receipt hooks, launch routes or schemas. Use the independent universal-services
   fixture for genuinely shared changes.
6. **Build and freeze:** build the required configurations with the configured warning
   policy; record sources, scripts, binaries, symbols, executed tests and omissions.
7. **Native evidence:** verify that guards identify the real current owners and that
   requests reach the intended engine path.
8. **Fresh in-game route:** verify control, encounters, scene choreography, audible
   dialogue, door collision, scans, objectives, transitions, cinematics and completion.

Homecoming has [mission tests](../unit/homecoming_tests.cpp); shared execution has
[executor tests](../unit/coo_executor_tests.cpp) and
[universal-service tests](../unit/coo_universal_services_tests.cpp).
Inspect what each fixture actually asserts before claiming full-route coverage.

The current validation entry point is
[verify_lua.py](../../tools/coo/verify_lua.py). It supports selected projects and
configurations as well as the configured full run. Read its current job list and
capture requirements; do not quote historical suite totals. Compile-only checks or
tests lacking required native evidence are not executed acceptance passes.

For a documentation-only rewrite, review technical claims and links; rebuilding the
game is unnecessary. For an implementation candidate, freeze documentation before
the final validation because the source manifest includes relevant docs.

Mocks and replays prove their modeled logic. They do not prove native playback,
audible timing, rendered appearance, damage behavior or physical traversal.

## 16. Packaging, installation and rollback

Treat validation, packaging, installation and play acceptance as separate results.
Use the tools appropriate to the actual target checkout and installation.

The mission-candidate workflow uses [verify_lua.py](../../tools/coo/verify_lua.py),
[package_lua.py](../../tools/coo/package_lua.py) and
[install_candidate.ps1](../../tools/coo/install_candidate.ps1). Packaging checks the
declared test scope, source manifest and binary hashes. The candidate installer
resolves its target from the repository root; confirm that this is the intended
game installation before using it. A source checkout elsewhere is not implicitly
the game directory.

**Current checkout discrepancy:** the inspected packager includes `launchpad.lua`,
while the candidate installer's explicit payload list omits it. The installer also
contains fixed validation totals. Reconcile the exact payload/job contract or use
the appropriate supported release workflow before presenting this chain as ready
to install. Do not bypass manifest checks or relabel a partial run as full validation.

Player release packaging and installation have a separate workflow under
[tools/install](../../tools/install/New-DawnRelease.ps1) and the
[release installer documentation](../../tools/install/release/README.md).
Check both load locations when the chosen installer targets the game root and
`bin/x64`; verify which DLL and adjacent runtime scripts the new process loads.

For an authorized deployment:

- Finish the concrete candidate and required checks before the installation step.
- Close Destiny before replacing a loaded DLL. Do not treat this playbook as a
  request to terminate or launch a user's process.
- Preserve the exact pre-install files and a coherent previous validated
  DLL/script set; these can differ if working scripts were edited separately.
- Include matching PDBs and required runtime/license content in the recorded set.
- Verify baseline hashes, backup hashes, copied payload hashes and unchanged settings.
- Restart for the new process to load the matching scripts, then verify its load
  path/fingerprint and keep the installation receipt with the candidate evidence.
- Preserve superseded evidence rather than renaming an older run as the new result.

An installation receipt proves what was installed. Only the fresh route check
establishes which mission behavior was accepted in game.

## 17. Deliverables for a mission reconstruction

Finish with a coherent, reviewable set:

- The mission Lua document and any changed native story graph, bindings, adapters
  or shared services required by this mission's architecture.
- A readable route/beat coverage document with evidence and uncertainty labels.
- Native inventories and reproducible extraction notes, including reconstruction
  choices for populations and timing.
- Meaningful focused tests, affected regression results and exact build scope.
- A frozen candidate manifest, matching DLL/scripts/symbols and delivery artifact
  when packaging is in scope.
- Installation and rollback records when deployment is in scope.
- A concise fresh-run acceptance route, including required restart/checkpoint/skip
  cases and unresolved fidelity items.

Start with one playable section that proves launch, native action, authentic
completion and the next objective. Extend that same ownership model across the
mission. The final record should make it possible to tell what the source requests,
what the native runtime confirmed and what the player actually experienced.
