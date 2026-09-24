# 1AU entrance repairs in source

The entrance repairs are compiled into the normal Release x64 DLL. No post-link
binary patch is required for these repairs. The historical candidate recipes in
`tools/one_au_no_wipe` and `tools/one_au_hotfix` describe earlier packaged binaries;
they must not be applied to this newly compiled DLL.

## Runtime implementation

- `vanilla/one_au/bridge_native.h` grants the current bridge console owner local
  authority after validating the sensor, authored link, salted references,
  compaction-corrected controller, revision and live entity row. Native playback
  mode and timer values do not gate the repair and are never changed by it.
- `vanilla/one_au/entrance_native.h` implements the two exact door identities and
  the six bridge placement records. Door repairs grant authority; native power,
  locks, proximity and animation still control opening. Cleanup requires a
  distinct owned copy with the same placement table, record and full identifier.
  It rejects stale handles and retiring/uninitialized rows, requires a valid
  component bundle on the entity being retired, excludes the current callback entity, rechecks identities and
  ownership, and retires at most one duplicate per invocation.
- `bootflow/one_au_entrance.h` supplies checked native reads and signature-gated
  engine calls for game build 86657. It is included by the compiled
  `omega_vex_lattice_probe.h` DF7FF0 device callback, inside its existing CallGate scope.
  The original callback still runs once with the original arguments and retains
  its return value. The repair runs before the first device snapshot and native tick,
  matching the tested binary candidate's callback boundary. It does not depend
  on a sleeping door invoking its own callback. The older
  `omega_reveal_native.cpp` path is not a DLL build input and is not used.
- The binary recipe's operand at file offset 1685044 redirects the DF7FF0
  callback's first snapshot call (candidate RVA 19C233) to entrance RVA 28FD80.
  Operand 2476668 redirects the E4A590 campaign observer call (RVA 25D67B) to
  console-owner RVA 28FB00. These are separate repairs and callback boundaries.
  The Ghost callback retains console ownership only.
- The reference assembly checks the bundle only on the retirement candidate.
  An owned matching placement or automatic door may still be loading its bundle.
  A callback owner of `FFFFFFFF` does not block repairs of valid unrelated entities.
  The native readability fallback walks all regions covering a table, accepting
  adjacent readable regions while rejecting guard, inaccessible and uncommitted pages.
  `entrance_probe` logs show inactive requests, failed preflight checks, and bounded
  scan counts; `entrance_repair` logs report actual changes.
- The runtime exposes a small request using named Frame fields, including run,
  generation and section. Landing/bridge/processing frames enable cleanup;
  bridge/processing frames enable doors. Darkness Zone restriction does not
  suppress the observer. Finished, faulted and inactive missions do.
- The other-mission sense parser accepts independently optional roster keys,
  masks, states and bubble identities. Only complete identified bodies publish
  acknowledgement entries; malformed and over-capacity bodies still fail.
- Membership routing retains both region legs for `mission_ember`, feeding the
  existing outgoing membership encoder. Its held-region routing policy is
  unchanged.
- The pipe cohort contains source 59 only. Source 58 is absent from both spawn
  and clearance selection. Mercury remains `{40,51,52,53,54,55}`.

## Validation

Release x64 projects under `Dawn/unit`:

- `one_au_tests`: 1,352 checks, including synthetic native rows, every native row
  index, frame gating, ownership races, invalid-bundle crash regression, real
  native-registry compaction resolution, actual pipe spawn/admission/death and
  clearance, and the existing no-wipe cases. It invokes the actual DF7FF0
  replacement with a synthetic image/table and a stub original callback, checking
  observer-before-native order, a non-lattice device, sentinel callback ownership,
  readable region boundaries, arguments, return value, and quiescence.
- `activity_sense_update_parser_tests`: all partial-field combinations, unknown
  bubble identity, malformed counts, capacity and truncation, plus existing
  Towerfall and native Omega captures.
- `retained_authority_scope_tests`: 25,968 checks including 1AU route mapping,
  sparse merge, snapshots and encoded outgoing region legs.
- `activity_host_lifecycle_tests`: membership transaction commit with both legs.
- `native_roster_lifetime_tests`: 699 checks.
- `native_roster_lifetime_wire_tests`: 3,389 checks.
- `omega_native_readable_tests`: 2,026 checks, including spans across different
  readable regions and spans containing inaccessible, guarded or decommitted pages.
- `omega_hook_bundle_tests`: isolated native-hook transaction and original
  publication checks against `destiny2_unpacked.bin`; no native game code runs.

Build the DLL through `Dawn/Dawn.vcxproj` with Configuration=Release,
Platform=x64. Output: `build/x64/Release/steam_api64.dll` and matching PDB.
The final build record belongs in `build/entrance-source-port/verification.json`.

The first compiled port (`B2776CEE...`) failed the user's entrance check: the
bridge was already extended before the console; the scan and doors were not tested.
Revision 1 also attached entrance cleanup to E4A590 instead of DF7FF0 by tracing
the console-owner patch operand instead of the entrance operand. Revision 2
corrects the callback and the three admission differences described above. Its owned
placement and cross-region regression tests failed before the correction and pass
afterward. The exact cause of the user's run is unconfirmed because file logging
was disabled. Revision 2 still requires a gameplay test. Check a cold process start and
repeated 1AU loads: missing early pipe enemy, normal Ghost scan, bridge hidden
before deployment, no duplicate bridge collision, both automatic doors, and
continued processing-room progression with Darkness Zone presentation and no
wipe. A playtest of the earlier binary patch does not validate this build.
