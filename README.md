# Mad Max mods

Native mods for Mad Max (2015, Steam version only).

| Mod | What it does |
|---|---|
| [InfiniteAmmo](infiniteammo/) | Ammo never runs out. Per-type switches in `infiniteammo.ini`. |
| [NoFallDamage](nofalldamage/) | The player takes no fall damage and does not die from high falls. |
| [FovChanger](fovchanger/) | Camera field of view multiplier with hotkeys. |
| [CleanupCrewAnywhere](cleanupcrewanywhere/) | The stronghold Cleanup Crew collects scrap from defeated vehicles on the whole map. |

## Install

1. Install [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) (x64) as `dinput8.dll` in the game folder.
2. Download a mod zip from [Releases](../../releases) and extract it into the game folder (next to `MadMax.exe`). The zip also installs with Vortex.

To uninstall, delete the mod's `.asi` (and `.ini`) from the game folder.

## Build

Requires Visual Studio 2022 with C++. Run `<mod>/src/build.bat`. The output is `<mod>/src/build/<mod>.asi`.

## License

MIT (see `LICENSE`), for the code in this repository except:

- `third_party/minhook/`: MinHook, BSD 2-Clause (`third_party/minhook/LICENSE.txt`).
- `tools/Gibbed.MadMax/`: Gibbed.MadMax and its libraries, zlib and MIT licenses, modified (see `MODIFIED.md`).
