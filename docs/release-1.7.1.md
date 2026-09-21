# Dawn 1.7.1

Homecoming and Adieu added, with performance improvements and bug fixes.

## Missions

- **Homecoming:** added the Red War opening mission, including its opening scenes, Tower progression, encounters, ship-barrier sequence, and transition into Adieu.
- **Adieu:** added the City escape, Ghost reunion, mountain traversal, falcon sequences, encounters, soundtrack cues, and Hawthorne rescue ending. The mission starts with Traveler's Chosen (Damaged), with weapon pickups available along the way.

## Performance and bug fixes

- Includes the existing stutter and runtime performance improvements.
- Fixed Adieu's City-exit stall and Ghost cleanup timing so his dialogue can finish before he disappears.
- Fixed Homecoming opening, native scene playback, and progression issues.
- Restored campaign objective and route-marker behavior across New Light, Gateway, A Deadly Trial, Beyond Infinity, Deep Storage, Tree of Probabilities, A Garden World, and Hijacked.
- Ported the New Light lighting hotfix: the Breach switch starts the authored room-lighting sequence and sound once, while preserving Ghost's return and subsequent shutter progression.

## Credits

Thanks to [rhys-magno](https://github.com/rhys-magno) for providing the **Homecoming template and scenes**.

## Install or update

Download and extract **Dawn-1.7.1.zip**. Close Destiny 2, then run **Update-Dawn.cmd** to keep your existing Dawn save and settings. **Install-Dawn.cmd** starts a fresh save. Keep the entire extracted package together.

Requires an existing Destiny 2 installation of build **86657.20.08.23.1800.d2_rc** and its complete packages folder. The game is not included.

The package includes the runtime DLL, mission scripts, defaults, vendor rules, event presets, installer, updater, uninstaller, licenses, and payload hashes. Personal saves, local settings, logs, caches, and debug symbols are excluded.

## Validation

- Release x64 build passed.
- New Light integration: 111,355 checks passed.
- Shared-hook and 1AU regression suite: 1,391 checks passed.
- Adieu previously passed 658 checks, with live confirmation of Ghost retirement and City-to-outskirts progression.
- Live verification of the new lighting fix and full mission playthrough coverage remain pending; no new FPS benchmark is claimed.
