# Native camera reference: build 86657

The [camera feature](../THIRD-PERSON-CAMERA.md) implements following/front modes
and camera-only rendering. This reference retains the bindings needed to maintain
it and the separate, unimplemented drone investigation.

All addresses are RVAs from the loaded `destiny2.exe` base. Labels describe
recovered behavior, not exported symbols. The verified executable is build
`86657.20.08.23.1800.d2_rc`, SHA-256
`81964380664e7fcee3c620085a157fdeaf91fefacf7214907820f188bbeb4ced`.

## Implemented bindings

| Binding | RVA | Contract |
| --- | --- | --- |
| Camera selector | `0x128F290` | Director in RCX, mode in EDX, transition in XMM2, force reconstruction in R9B. |
| Gameplay director update | `0x12954C0` | Vtable `0x1C809D0`, slot `+0x08`; local player index at director `+0x340`. |
| Ordinary first-person request | `0x1295ABC` | Mode 4; dispatch at `0x1295B71`, return at `0x1295B74`. Only this caller is overridden. |
| Following-camera update | `0x1294500` | Vtable `0x1C803D0`, slot `+0x10`; owned camera at director `+0x10`. |
| Following output / placement | `0x1294A51` / `0x12D5A61` | Forward/up at output `+0x3C/+0x48`; native placement resolves the relative offset afterward. |
| Existing camera-frame producer | `0x12D3B90` | Owned by teleport lifecycle; shared for key polling and current player identity. |
| Render-view builder | `0x11D4670` | Accept world(1) -> HUD(7), or world(5) -> weapon(2-4) -> HUD(7); preserve every link. |
| Scene visibility inheritance | `0x58C280` | Observe scene, owner, and model-proxy identities after the native call. |
| Masked visibility setter | `0x1169110` | Native update of renderer mask `+0x54` and spatial entry `+0x34`; also used for restoration. |
| Prepared-view draw submission | `0x11D5A70` | Preserve all seven arguments; weapon-only omission uses false-empty result at `0x11D62AD`. |
| HUD packet producer | `0x132B890` | Run completely, then clear packet at owning frame `+0x36F18` for prepared view type 7. |
| HUD packet consumer | `0x116031C` | Explicitly accepts a null packet; this is not a null view pointer. |

Native world jobs require the first-person weapon prepared record even when it
has no drawing. The verifier checks that dependency, the false-empty submission
path, and the nullable HUD packet path. Runtime installation additionally checks
unique signatures, caller bytes, vtables, mode dispatch, and relevant layouts.

## Native modes and deferred drone work

The name table at `0x1FE1160` and constructor dispatch at `0x1290398` distinguish:

| Camera mode | Native name | Constructor | Use |
| --- | --- | --- | --- |
| 0 | following | `0x1281590` | Rear/front third-person feature |
| 1 | orbiting | `0x12D1610` | Orbit candidate; unimplemented |
| 2 | flying | `0x1281470` | Detached drone candidate; unimplemented |
| 4 | first person | `0x1281420` | Ordinary gameplay |

Director modes are a different enumeration: director 0 constructs gameplay at
`0x12815D0`; director 3 constructs debug at `0x1281190`. The director getter is
`0x1284950`, and director selection is `0x1289230`. Debug update `0x1291CB0` uses
vtable `0x1C807F8`; flying update `0x1293790` uses `0x1C80860 + 0x10`.

A drone could enter the native debug director/flying camera on the existing
game-thread lifecycle, retain the character, then restore native director
selection. Flying initializes its own view pose and updates camera-local position
at `camera + 0x60`; Dawn's existing Fly feature instead moves the player's body.
The debug mode settings at `0x1FE1138/0x1FE113C` are mutable, not enum constants.

Player movement/fire suppression, character visibility, death/loading/cinematic
handoff, correct director restoration, and streaming away from the character
remain untested. No drone controls or stationary-player behavior are promised.

## Reproduce the binding check

[The verifier](../../tools/re/verify_camera_modes.py) requires Python 3.11+ and
only the standard library. It checks the executable hash, bytes, native names,
constructors, dispatch tables, and vtable slots. Live reads use a self-contained
Windows process reader with query/read rights; no native game functions are called
or process memory written.

From the repository root, either inspect a running game or use a mapped capture:

```powershell
$gameRoot = 'C:\Path\To\Dawn'
$gamePid = (Get-Process -Name destiny2).Id
python tools/re/verify_camera_modes.py --pid $gamePid --exe "$gameRoot/destiny2.exe"

python tools/re/verify_camera_modes.py --snapshot "$gameRoot/.dawn/camera-investigation" --exe "$gameRoot/destiny2.exe" --output docs/research/evidence/camera-modes-86657.json
```

A snapshot contains `mapped-image.json` (base and section RVAs) plus `text.bin`,
`rdata.bin`, `data.bin`, and `pdata.bin`. Captures stay local; only the compact
[56/56 passing check report](evidence/camera-modes-86657.json) is tracked. This
evidence verifies native bindings, not visual gameplay or drone input isolation.
