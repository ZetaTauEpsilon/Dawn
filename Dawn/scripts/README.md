# Mission scripts

The mission runtime loads `one_au.lua`, `homecoming.lua`, `omega.lua`, `deadly_trial.lua`, `gateway.lua`, `beyond_infinity.lua`, `deep_storage.lua`, `hijacked.lua`, `strike_pact.lua`, `strike_bond.lua`, `mission_pact.lua`, and `mission_bond.lua`. Each returns one definition through the shared Lua builders. C++ validates it and the universal executor runs the compiled graph. Beyond Infinity is a reconstruction in progress; its full native playthrough is not yet accepted. See [the Beyond Infinity implementation checkpoint](../docs/BEYOND-INFINITY-IMPLEMENTATION.md).

Eater of Worlds is excluded from this build. Its source, script, tests, and reconstruction notes are preserved in [the separate archive](../../optional/eater-of-worlds/README.md).

Edit the script beside the installed DLL, validate it, and restart Destiny. Scripts load once per process; mission restarts and death keep the loaded definition. Logs in `Dawn/logs/dawn.log` identify the mission, `format=lua`, path, and source fingerprint. Missing or invalid files block executor selection.

Read [Lua mission authoring](../docs/LUA-MISSION-AUTHORING.md) for builders, native bindings, limits, and ownership. [Format reference](FORMAT.md) covers composition and Omega's additional native requirements.

From the workspace root, run:

```powershell
python tools/coo/verify_lua.py --out build/coo/validation-my-change
python tools/coo/package_lua.py --validation build/coo/validation-my-change
powershell -File tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/validation-my-change
```

The first command runs all 22 suites in Debug and Release and builds both DLL configurations. Packaging verifies the exact source and binary hashes. Installation requires Destiny to be closed and backs up the previous DLL, symbols, license, and scripts together. It does not change launch settings or start the game. Use the installer's `-ValidateOnly` switch for a read-only package check.

For a focused Omega script check, use the validator from your validation directory:

```powershell
& build/coo/validation-my-change/coo_script_tests-local/Release/coo_script_tests.exe --validate Dawn/scripts/omega.lua
```

Mission JSON and its loader have been removed. Settings, recovered package data, and validation receipts still use their existing formats. Historical rollback archives retain the files required by their older DLLs.

Hijacked adds the Nessus Entangled Mind pursuit and Well of Echoes conflux route. Select **Activity override → Hijacked opening** before launching the normal Chosen donor route. See [implementation and fidelity record](../docs/HIJACKED-IMPLEMENTATION.md); its fresh native playthrough remains unverified.
