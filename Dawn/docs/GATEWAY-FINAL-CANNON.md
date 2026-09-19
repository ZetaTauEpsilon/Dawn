# Gateway final-cannon force gate

## Captured defect (2026-09-19)

A live run waiting for the remaining enemy in traversal source 209 confirmed
that type23/2's position channel was zero while its separate placed launcher
was present. The launcher is `80F46DB6/80803DBC/+13E8` at the final ledge,
not either of the type4 cannon sources (3/4). VFX and launch force therefore
had different activation paths. The live inspection was read-only.

## Fix

`gateway_cannon` gates native per-region force accumulation at `D3B550` using
the current Gateway run's `frame.finalCannon`, the same end-clear join used
by the VFX authority. The component identity and resolving self handle must
match. Other launchers and activities retain native behavior.

Overlap admission/removal and the native tick remain active. The caller clears
each target's force, velocity and apply flags before accumulation, so a closed
gate does not leave a cached impulse. No shared asset or retained player state
is edited. The hook uses byte validation and the existing quiesce/drain lifecycle.

## Validation

The Gateway traversal regression retains one admitted source-209 enemy and
checks that time, proximity and stale deaths release neither force nor VFX.
Both release after its qualified death. Identity tests reject other cannons,
wrong classes/offsets, recycled handles and foreign runs; a new run relocks.

Run `python tools/coo/verify_gateway_cannon_native.py <destiny2_unpacked.bin>`
to verify the pinned native reset/apply sequence and call boundary. Run the
`gateway_opening_tests` project in both Debug and Release.

A new in-game playthrough is still required to confirm the installed candidate's
launch behavior. The runtime logs `ev=gateway stage=final_cannon_install` on
successful installation, and `stage=final_cannon released=0/1` when processing
overlaps before/after the final encounter clears (client logging level: info).
