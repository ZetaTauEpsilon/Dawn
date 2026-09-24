# Dawn 0.1.5.2 waypoint recovery

This is a source reconstruction of the waypoint changes in the user-supplied
release, not a claim that the missing original patch or its source was recovered.
It applies only to New Light, Gateway, A Deadly Trial, Beyond Infinity, Deep
Storage, Tree of Probabilities, A Garden World, and Hijacked. Existing
Homecoming/Exodus changes remain in the working tree and candidate build.

## Reference and reproduction

The reference is `payload/steam_api64.dll` from `Dawnv0.1.5.2 (2).zip`:
SHA-256 `88C75AC8F411215A87A6E62758F9DCC34FB4BAB903EBDEC074C2E08164ACC7C8`.
Place that file at `build/waypoint-recovery-20260920/release.dll`.
The oracle rejects any other hash. It executes only offline, pure buffer
transformations under Unicorn; it never loads the reference DLL into Windows
or injects it into the game.

Build `tools/testing/waypoint_test_bridge.vcxproj` as Release/x64 with the local
MSVC toolchain, then run `python tools/testing/waypoint_differential_tests.py`.
Python dependencies are `pefile`, `capstone`, and `unicorn`.
`RecoverWaypoints.java` is the Ghidra postscript used to inspect explicit RVAs.
`waypoint_binary_index.py` supplies heuristic leads only; its matches are not
equivalence evidence.

## Recovered contracts

- Shared objective encoding: `2BD920`, `2C7CA0`. Three-slot revision selection,
  native locator sentinels, route selector, and paired type-60 references. The
  eight missions opt in; the old encoder remains the default for other missions.
- Shared readiness registry: `43CA10`, `466E60`, `44E390..44EE10`, `26DB00`.
  Publication follows the admitted directive descriptor and current owner;
  readiness changes rotate delivery without accepting older runs or generations.
- New Light: `5CCEB0`. Area-qualified named-point locators and the final indoor
  Hangar doorway target, including the objective preceding the outside route.
- Gateway: released marker table, including Forest entrance type 60/slot 448,
  portal/module targets, and Brother Vance. Uses shared readiness and encoding.
- A Deadly Trial: `2709A0`, `270C10`, `270FB0`, `2714D0`, `271690`, `2723F0`.
  Opening doorway/stair route, exact native point identity, stale-point retirement,
  stable repeat updates, and validated append-only restoration of both meshes.
- Beyond Infinity: `26CD30`, `26CE70`, `26D010`, `26D680` and controller state.
  Context/section/progress-qualified portals and reflection/return destinations;
  both owned Forest terminal observations and real route-volume observations.
- Deep Storage: `26C1B0`, `26C250`, `26C850`. Entrance/interior routes, both final
  plates, scan/conflux/doorway sequence, platforms, final box and lens targets.
- Tree of Probabilities/A Garden World: `26BA50`, `26BE70` and native worker
  checks. Retire only the matching distant root fallback after a valid nearby
  authored Forest marker exists. Campaign and strike descriptor identities stay
  distinct.
- Hijacked: `7A4CB0`, `5B3F20`, `272AE0`, `273260`, `5A7890`. All eight objective
  sources, area-qualified navigation, validated mesh restoration, and matching
  visible-banner acknowledgement to avoid redundant readiness republishing.

## Validation boundary

The differential suite passes **9,715** comparisons against the pinned release,
including complete transformed buffers, publication masks, repeat/idempotent
updates, invalid-owner/source guards, objective wire bytes, marker identities,
mesh-list preservation and resource validation. Another **38** local checks
cover owner/readiness isolation and visible-banner acknowledgement.

Debug mission suites passed for Gateway, A Deadly Trial, Beyond Infinity, Deep
Storage, New Light (Launchpad), Hijacked, and campaign/strike variants. The broad
A Garden World suite stops at its pre-existing `forest_endpoint_regressions`
assertion: that test requires all four columns to be nonnegative and an old exit
height, but the unchanged `strike_bond/forest_binding.h` intentionally gives
unused side exits column -1. Neither that Forest layout nor that test was changed
by this port. Both missions' recovered duplicate-marker predicates pass the
binary differential tests; this does not substitute for a complete gameplay run.

The production Release/x64 build is also compiled. Native engine callbacks and
all eight complete in-game playthroughs remain unverified. The staged DLL is a
test candidate, not a published release. No installation, push or release is
performed by this recovery workflow.
