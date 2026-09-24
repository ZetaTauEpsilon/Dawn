# 1AU darkness presentation without wipes

This folder preserves the historical binary package recipe. Current source builds
compile these changes and the entrance repairs directly into the DLL; see
[the source integration notes](../../Dawn/docs/ONE-AU-ENTRANCE-SOURCE-PORT.md).
Do not apply this recipe to a newly compiled DLL.

This exact-candidate repair builds `0.1.3-1au-no-wipe` from the already repaired
`0.1.3-1au-fix` package. It preserves all preceding console, bridge, door, roster,
region and pipe-enemy changes. It changes only three branch instructions in the
DLL; it does not add code, alter function boundaries, or change unwind metadata.

- A dead player no longer starts 1AU checkpoint recovery.
- 1AU always publishes the existing three-second individual respawn delay,
  including while the darkness-zone presentation is active.
- The escape timer continues displaying but expiry does not reset the encounter.
- Darkness presentation, combat damage, hazards and mission completion remain.
- Other missions retain their existing respawn policy.

The matching source changes and regression tests are in `source-port.patch` and
the 1AU source checkout. The entrance repair was still a binary repair when this
package was assembled; the later source integration is documented above.

## Build a package

Run with PowerShell 5.1 or newer:

```powershell
./tools/one_au_no_wipe/New-1AUNoWipeCandidate.ps1 `
  -CandidateRoot 'C:\path\Dawn-0.1.3-1au-fix' `
  -OutputDirectory 'C:\path\Dawn-0.1.3-1au-no-wipe'
```

The builder rejects other DLLs, existing outputs and nested output paths. It
verifies every input and output payload hash before producing the ZIP. Install
through `Update-Dawn.cmd` while the game is closed to preserve the player profile
and retain a rollback backup. The root project GPL license applies.

## Verification

`test_candidate.py <original-repaired-DLL>` executes both original and modified
machine-code paths offline using Unicorn. It requires Python, pefile and unicorn.
It checks 360 player-life cases, 24 escape-timer cases and 16 respawn-policy cases.
The whole-DLL comparison permits changes only within the three reviewed ranges.

The 1AU source suite passes 1,136 checks, including repeated deaths in every
section, preservation of progress, unchanged darkness authority and escape expiry.
The shared protocol suite checks the actual packet writer for both 1AU and
unrelated restricted activities, including fields following the respawn delay.
These checks do not replace the in-game acceptance checks in `TESTING.md`.
