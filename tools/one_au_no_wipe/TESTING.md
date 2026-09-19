# Dawn 0.1.3-1au-no-wipe

Experimental 1AU update for Destiny build `86657.20.08.23.1800.d2_rc`.
Includes the previous 1AU entrance repairs and removes automatic 1AU wipes.

## Changes

- Keep the darkness-zone display.
- Allow the normal three-second individual respawn delay in all 1AU sections.
- Do not restart encounters when the player dies.
- Do not restart the escape section when the escape timer expires.
- Preserve the previous Ghost console, duplicate bridge, door and early-enemy fixes.

This does not grant invulnerability. Combat and environmental hazards can still
kill the player. Mission progress should survive those deaths. Other missions'
wipe and respawn policies are unchanged.

## Install and verify

1. Fully close Destiny 2 and extract the complete ZIP.
2. Run `Update-Dawn.cmd` and select the folder containing `destiny2.exe`.
3. Launch the game and start a fresh 1AU run.
4. Confirm the Ghost console, unfolding bridge and exit door still work.
5. Enter a darkness zone and confirm its presentation still appears.
6. Die deliberately. Verify individual respawn becomes available after about
   three seconds without a wipe countdown or checkpoint/encounter reset.
7. Repeat after killing some enemies; verify cleared enemies and objectives
   remain cleared. Check that the respawn location allows continued progression.
8. Reach the escape section, let the timer expire and verify no forced reset.
9. Return to orbit, reload 1AU and repeat a death. Test another mission as a control.

The installed `steam_api64.dll` in both the game root and `bin/x64` must have SHA-256:

`ea5de3644dc5e7436393f8aa81297ad14f7cb970939975de37891ed8feb48c71`

Automated source, packet, machine-code and package checks passed. The new
death/respawn behavior and a complete mission playthrough remain unverified in game.
Keep the updater's rollback backup until these checks pass.
