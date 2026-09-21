# Dawn 1.7.2

Homecoming stutter fix.

- Disabled redundant opening-area scene/spawner diagnostic polling that ran on repeated objective updates and dialogue scans.
- Preserved mission progression, enemy spawning, scenes, dialogue, and doors.

## Install or update

Download and extract **Dawn-1.7.2.zip**. Close Destiny 2, then run **Update-Dawn.cmd** to keep your existing save and settings. **Install-Dawn.cmd** starts a fresh save. Keep the complete extracted package together.

Requires Destiny 2 build **86657.20.08.23.1800.d2_rc** and its complete packages folder. The game is not included.

The player ZIP excludes performance-capture tools, Python scripts, debug symbols, logs, caches, and personal saves.

## Validation

- Homecoming Debug and Release: 155,840 checks passed in each configuration.
- Release x64 DLL built successfully; diagnostic polling is compiled out of normal builds.
- No new post-fix gameplay FPS benchmark is claimed.
