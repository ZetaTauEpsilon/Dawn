# Dawn 0.1.3 — 1AU + Omega hotfix + no wipes

Includes the 0.1.3 Hotfix changes while keeping this release's 1AU mission:

- Omega's seven roster searches resolve the registry and object identity before copying their matching records, avoiding repeated large catalog copies during portal transitions.
- Omega's unintended 0/100 progress ring is removed. Its script authority, script state, objectives and progression remain enabled.
- Install-Dawn.cmd now detects an existing profile and uses the save-preserving update path automatically. Incomplete installations are rejected instead of silently starting over.
- Retains the 1AU entrance repairs: Ghost console ownership, automatic doors, safe duplicate bridge cleanup, partial roster handling, region transitions and the identified early pipe enemy removal.
- Retains 1AU's darkness-zone presentation and normal respawns without death wipes or the escape-timeout wipe.

Extract the complete ZIP, close Destiny 2, and run **Install-Dawn.cmd**. **Update-Dawn.cmd** also preserves existing saves and settings. Requires archived game build 86657.20.08.23.1800.d2_rc.

Validation: 1AU regression suite; Omega and other-mission protocol suites; identity lookup and descriptor checks; offline execution comparisons against the supplied hotfix; installer selection tests; package checksums. This combined DLL has **not** received an in-game playtest.

This is a reconstructed release based on the existing repaired 1AU DLL, with a matching patch recipe and source changes in repair-source/hotfix. The earlier entrance repair's complete C++ source port remains separate work. The upstream hotfix DLL itself has not replaced the 1AU DLL.
