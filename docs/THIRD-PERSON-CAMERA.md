# Camera controls

Build 86657 supports normal, rear third-person, front third-person, and an
independent camera-only view. Open **Insert > Camera** for the same controls.

| Key | Action |
| --- | --- |
| F5 | Cycle Normal -> Third person -> Third person front -> Normal |
| F6 | Toggle rear third person; press again for Normal |
| F7 | Toggle front third person; press again for Normal |
| F8 | Toggle Remove HUD and player: hide the HUD and local character/weapon models |

F6 and F7 switch directly from either other mode. F8 works independently of the
selected mode and resets off every launch. Dawn's menu stays available. This
feature follows the character; detached drone movement is not implemented.

All four keys are rebindable in **Camera > Key bindings**. Click a key, then press
a new keyboard key. Escape cancels; Backspace unbinds. A key already used by
another camera action swaps the two bindings. The Dawn menu key is reserved.
**Reset key bindings** restores F5/F6/F7/F8 without changing the selected mode.
Use **Normal** in the menu to select the ordinary view directly.

The default mode is Normal. The selected mode is saved in `Dawn/camera.json`
beside the loaded DLL, using `third_person_enabled` and `front_view`. The same
file saves `cycle_key`, `rear_key`, `front_key`, and `camera_only_key` as Windows
virtual-key codes (zero is unbound). Missing fields use the defaults above.
Changes save immediately through a temporary file and atomic replacement. Disabled
third person always means Normal, even if an old front preference is present.
Camera hotkeys ignore repeats, keys held at startup, an open Dawn menu, and an
unfocused game. Rebinding consumes keys already held. Simultaneous mode presses
prefer rear, front, then cycle; visibility is independent. F9 has no default action.

## Implementation

- [Settings](../Dawn/src/client/camera/camera_settings.cpp) publish one atomic
  camera mode and a separate session-only visibility flag.
- [Third person](../Dawn/src/client/hooks/camera/third_person.cpp) overrides
  only the ordinary first-person request from the local gameplay director,
  selecting native following mode. Native construction, targets, transitions,
  and return values remain authoritative. Death, vehicle, cinematic, and other
  native requests pass through and release ownership.
- Front view rotates the owned following camera's output forward/up basis by
  180 degrees around world Z after the native update. Native offset placement
  then moves it in front of the character. Finite, unit, perpendicular vectors
  are required; actor orientation and input are unchanged.
- The existing teleport camera-frame hook polls the keys and supplies the local
  player handle. There is no second detour on that function. Camera settings,
  the [Camera page](../Dawn/src/client/ui/camera/camera_panel.cpp), hook
  installation, and shutdown are wired into the client lifecycle and project.

The selector and following-update hooks attach as one transaction after code,
caller, vtable, and layout checks. Unsupported bindings leave the controls
unavailable. Camera-only rendering validates its own five hooks; failure there leaves the ordinary
camera modes available. Addresses and evidence are in the
[native reference](research/camera-modes-86657.md).

## Camera-only rendering and restoration

[Camera-only rendering](../Dawn/src/client/hooks/camera/clean_view.cpp) preserves
the complete native world/weapon/HUD view chain. It suppresses drawing at three
boundaries:

1. Local model visibility is saved and cleared through the native masked setter
   at render-view construction. Full handles, weak generations, and the parent
   hierarchy establish ownership. Native visibility changes while hidden update
   the saved mask; restoration rechecks the current renderer identity.
2. Weapon prepared views of types 2-4 return the native empty-stage result at
   drawing submission. Their records remain available to world rendering jobs.
3. The HUD producer completes normally, then its gameplay view publishes the
   native null draw packet. Its consumer accepts null; UI updates, buffer
   rotation, and resources keep their native lifetimes.

Dawn's optional HUD overlays are also suppressed while camera-only is active;
their saved switches stay intact, and the menu remains available.

**Never unlink the weapon view.** A first-person world view unconditionally
submits its prepared weapon record. Removing that record caused the original
F9 null-read crash at RVA `0x11D5AA3`; suppressing its drawing avoids that failure.

The Homecoming correction reads the current controlled entity through
`teleport::read_controlled_entity` every local camera frame. Teleport's cached
physics owner can be empty when teleport is disabled or stale after a destination
change, so it cannot identify the current player for camera-only rendering.

Shutdown quiesces the camera consumers, detaches the camera-frame producer, then
removes the camera hooks before settings are destroyed. Hidden model masks must
be restored on a render frame before unloading; pending restoration retains the
module for a later shutdown attempt.

## Build and validation

From the repository root, using MSBuild 18, v145, and the Windows SDK required by
the projects:

```powershell
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Dawn/Dawn.vcxproj -p:Configuration=Release -p:Platform=x64 -p:PreferredToolArchitecture=x64 -m:4 -v:minimal -nologo
& $msbuild Dawn/unit/camera_toggle_tests.vcxproj -p:Configuration=Release -p:Platform=x64 -p:PreferredToolArchitecture=x64 -v:minimal -nologo
& ./build/unit/camera_toggle/Release/camera_toggle_tests.exe
```

The [unit suite](../Dawn/unit/camera_toggle_tests.cpp) runs 1,811 checks covering
key edges/focus, cycles and direct selection, legacy preferences, native-mode
exclusions, front-view ownership and geometry, invalid orientations, weapon draw
filtering, and all 256 visibility masks with native updates while hidden. It also
checks each rebindable action, duplicate-key swaps, capture cancellation and
clearing, held keys during rebind, legacy/malformed settings, persistence across
reload, default restoration, and preservation after a failed settings write. The
[read-only verifier](../tools/re/verify_camera_modes.py) checks 56 native bindings;
see the reference for snapshot and live-process commands. Neither test replaces
an in-game visual check.

Look for `ev=camera` in `Dawn/logs/dawn.log`: `stage=install`, `stage=mode`,
`stage=select`, and `stage=front` identify mode activation. Camera-only installation logs
`stage=camera_only_install result=ok default_key=F8 revision=preserve_views`; first-use
markers `camera_only_models`, `camera_only_weapon`, and `camera_only_hud` each
record `view_chain=preserved`.

Rear third person and the corrected F9 renderer were user-confirmed in gameplay.
Front-view behavior and the later Homecoming owner correction still need separate
gameplay confirmation on v0.1.5. Check mode cycling/direct keys, F8 on/off in each mode,
aiming and wall collision, weapon swaps, other actors remaining visible,
death/respawn, cinematic handoff, and destination changes including Homecoming.
Rebind each action, restart, and check that the chosen keys remain selected.
