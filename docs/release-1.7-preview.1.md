# Dawn 1.7 Preview 1

This preview adds Adieu (Exodus), expands Homecoming, and restores mission guidance across the current campaign build.

## Included changes

- **Adieu:** City escape, Ghost reunion, mountain traversal, falcon scenes, encounters, soundtrack cues, and the ending rescue sequence. Ghost cleanup now waits for the observed dialogue window and handles the native scene completing after its selector disappears, fixing the City-exit stall. The starting loadout contains Traveler's Chosen (Damaged), with weapon pickups available later in the mission.
- **Homecoming:** opening and continuation fixes, native scene playback, ship-barrier progression, proximity-triggered progression, and opening-fade handling across the transition into Adieu.
- **Mission guidance:** restored route markers and objective behavior for New Light, Gateway, A Deadly Trial, Beyond Infinity, Deep Storage, Tree of Probabilities, A Garden World, and Hijacked.

## Credits

Thanks to [rhys-magno](https://github.com/rhys-magno) for providing the baseplate and scene planning for Homecoming.

## Install or update

Download and extract **Dawn-1.7-preview.1.zip**. Close Destiny 2, then run **Update-Dawn.cmd** to preserve an existing Dawn save and settings. Use **Install-Dawn.cmd** for a fresh save. Keep the entire extracted package together.

Requires an existing Destiny 2 installation of build **86657.20.08.23.1800.d2_rc** with its complete packages folder. The game is not included.

## Validation and preview status

- Release x64 production build passed. Its captured build inputs match the released source.
- 658 Adieu checks and installed package-binding validation passed.
- The latest live run confirmed Ghost retirement and progression from the City into the outskirts.
- The waypoint port previously passed 9,715 offline comparisons and 38 additional lifecycle checks. The broad A Garden World suite still has a known assertion against an older Forest anchor layout; the recovered duplicate-marker checks pass separately.
- Full mission playthrough coverage and performance verification remain in progress. This is a prerelease for testing.

The player ZIP includes the installer/updater, runtime DLL, mission scripts, clean defaults, vendor rules, event presets, licenses, and file-hash manifest. Local saves, settings, caches, logs, source files, and debug symbols are excluded.

Runtime DLL SHA-256:
`E0CFDEE9A77AD8F9BFBF888B23A32A8C02E9836012B24A0A87BBB0626BE53208`
