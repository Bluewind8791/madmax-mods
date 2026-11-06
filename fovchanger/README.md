# FovChanger mod: change the field of view

## Status (2026-10-18)

- Goal: a wider (or narrower) camera field of view, with hotkeys, as an `.asi` mod that installs with Vortex.
- Source: port of the Cheat Engine table "Mad Max Field of View Changer" (`MadMaxFov.CT`, Carl Alldis, 2019, Nexus). The user installed the `.CT` with Vortex and it did nothing: a `.CT` needs Cheat Engine, which is not installed. The table's three patch sites still match the current Steam build (checked against `../re/MadMax_dump.exe`).
- Existing mods: searched the web on 2026-10-18. Nexus has "FOV And Camera Tweaks" (`nexusmods.com/madmax/mods/140`). Its page returned 403, so its install method is unknown.
- v1 (2026-10-18): first in-game test: `Patches: OK`, F1 visibly changes the FOV, no crash. Numpad +/- were not pressed (the user used the main-row keys), so the default keys changed to 187/189. Multiplier change in game not confirmed yet.
- 1.0.0 (2026-10-18): second test (21:29) with keys 187/189: the log shows F1 toggles and multiplier changes from 1.20 up to 1.95, and the user confirmed it works in game. Sniper zoom with the mod on was not reported separately.
- Release: `release/FovChanger-1.0.0.zip` (`fovchanger.asi`, `fovchanger.ini`, `FovChanger_readme.txt`) for Vortex. Installed with Vortex on 2026-10-18 (hardlinks, mod `FovChanger-1.0.0`); the log at 21:51 shows `Patches: OK`.

## Files

Paths are relative to this folder. `<game folder>` = `C:\Program Files (x86)\Steam\steamapps\common\Mad Max`.

| Path | Purpose |
|------|---------|
| `src/fovchanger.cpp` | The mod. Three code patches with two stubs, and a hotkey thread. |
| `src/build.bat` | Builds `src/build/fovchanger.asi`. Needs VS 2022 C++. No MinHook. |
| `fovchanger.ini` | Settings: `Multiplier` (start value, 0.5-2.0), `Step`, `ToggleKey`, `IncreaseKey`, `DecreaseKey` (virtual-key codes, 0 = none). |
| `<game folder>/fovchanger.log` | Log. |

## Install, uninstall, test

- Requires Ultimate ASI Loader as `<game folder>/dinput8.dll` (see `../README.md`).
- Install: copy `src/build/fovchanger.asi` and `fovchanger.ini` to the game folder. Uninstall: delete them and `fovchanger.log`.
- A healthy start writes:
  ```
  FovChanger loaded.
  Multiplier 1.20, step 0.05, keys: toggle 112, increase 187, decrease 189
  Patches: OK
  ```
- Hotkeys work only while the game window has focus. F1 toggles (`FOV on`/`FOV off`), the main-row `=`/`+` and `-` keys change the multiplier (`Multiplier X`). The CT used Numpad +/- (107/109); the user's first test pressed the main-row keys, so v1 changed the default to 187/189. Changes made with keys are not saved to the ini.
- Test: on foot and in the car, compare F1 on/off; Numpad +/-; sniper rifle zoom and binoculars still zoom.

## How it works (Steam build, image base 0x140000000)

| Address | Original | Patch |
|---------|----------|-------|
| `0x1422444e7` | `mov [rbx+290h],r11d` (7 bytes). A camera settings copy from `rbx+0x194..` to `rbx+0x290..`; r11d is the default FOV. | `jmp` to a stub that stores `r11d * multiplier` (keeps r11, xmm0 and flags). |
| `0x14222cee4` | `movss [rbx+40h],xmm0` (5 bytes). xmm0 = degrees × (1/180) × pi (constants `0x1412de334`, `0x1412311f8`). Followed by `jmp` to the function epilogue. | `jmp` to a stub that writes only values below 0.6 rad (zoom), so sniper zoom works and the normal FOV is not overwritten. |
| `0x14224ad42` | `movss [r12+40h],xmm0` (7 bytes). Another FOV write. | One 7-byte NOP. |

- Stubs and their data (multiplier, 0.6 limit) are in one page allocated below `0x140000000` (rel32 range).
- Each patch fits in one aligned 16-byte block. `WriteCode` writes it with `InterlockedCompareExchange128`, so toggling while the game runs never exposes a half-written instruction.
- Signatures: the replaced instruction plus the following bytes (12 bytes each). If any does not match within 10 minutes, the mod logs "not supported" and stays inactive.
- Risk: the protector has code checksum guards (see `../re/README.md`). It is unknown whether these three sites are guarded.

## Next steps

1. Optional: save multiplier changes made with the keys to `fovchanger.ini`.
