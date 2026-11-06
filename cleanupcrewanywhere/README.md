# CleanupCrewAnywhere mod: Cleanup Crew works on the whole map

## Status (2026-10-21)

- Goal: the stronghold Cleanup Crew project collects scrap from defeated vehicles anywhere, not only inside the territory of the stronghold where it was built.
- Game text (`text/master_gui_eng.stringlookup`): "Scrap is automatically collected from all defeated enemy vehicles within the stronghold's territory". The crew does not pick up scrap piles or loot drops. Scrap Crew is a different project (scrap while the game is off, needs online).
- 1.0.0 (2026-10-21): confirmed in game. The Cleanup Crew is built in two strongholds only (`enc1010`, `enc1020`). Vehicles destroyed in Pink Eye's territory gave scrap and the "Cleanup Crew Added %i scrap" message. The log showed `GetProjectStatus hook: MH_OK` and `Cleanup Crew status: built (any stronghold)`.
- Release: `release/CleanupCrewAnywhere-1.0.0.zip` (`cleanupcrewanywhere.asi` + `CleanupCrewAnywhere_readme.txt`) for Vortex. Installed with Vortex on 2026-10-21 (hardlink, same md5 as the tested build).
- Replaces CarAutoLoot for the user (removed from the game folder on 2026-10-20, workspace folder deleted on 2026-10-21).
- No settings file: uninstall = delete the `.asi`.

## Files

Paths are relative to this folder. `<game folder>` = `C:\Program Files (x86)\Steam\steamapps\common\Mad Max`.

| Path | Purpose |
|------|---------|
| `src/cleanupcrewanywhere.cpp` | The mod. One MinHook hook on the `GetProjectStatus` graph node. |
| `src/build.bat` | Builds `src/build/cleanupcrewanywhere.asi`. Needs VS 2022 C++. Uses MinHook from `../third_party/minhook/`. |
| `tools/` | Analysis scripts (below). Run them with the capstone venv Python and `-I`. |
| `<game folder>/cleanupcrewanywhere.log` | Log. |

## Install, test

- Requires Ultimate ASI Loader as `<game folder>/dinput8.dll` (see `../README.md`).
- A healthy start writes:
  ```
  CleanupCrewAnywhere loaded.
  GetProjectStatus hook: MH_OK
  ```
- The first 20 overridden calls write `Cleanup Crew status: built (any stronghold)`. A call also writes this line inside the territory of a stronghold that has the crew (the answer is the same there).
- Test: build the Cleanup Crew in one stronghold, go to another territory, destroy an enemy vehicle. About 2 s later the game shows "Cleanup Crew Added N scrap".

## How the Cleanup Crew works (Steam build, image base 0x140000000, from `../re/MadMax_dump.exe`)

- Vehicle `.blo` files (249 of them) have a `CGraphScriptGameObject` named `CleanupCrew` with script `graphs/open_world/reward_cleanup_crew.gsrc` and external variables: `BE99C103` = 1 (compared with TRUE), `Modifier` (`9FF8C74A`) = 1.0, `7189124C` = the vehicle object. No territory data.
- The graph: Start, CompareVariable (`BE99C103` == TRUE), `GetProjectStatus(project_id = "cleanup_crew")` with the stronghold variable not connected. On `built`: Timer 2 s, `GetEconomyResourceId(parent = vehicle, Type 3)`, `GetEconomyResource` amount, `MathOperator` amount * Modifier, ExternalGraph `give_player_scrap.gsrc`, `SetEconomyResource` 0, GUIMessage, SendGlobalEvent.
- `GetProjectStatus` node input handler `0x141ea4bc0(node, context, pin)` (registered by name at `0x141e7146f`):
  - Pin `in` (`0xc94ba74a`). Variable `0x163d2026` (stronghold). When it is set, it reads a territory id and calls `0x141da4cf0`. When it is not set, it gets the **player position** (`0x141c91280` on `[0x1417f5338]`) and calls `0x141da7b60(manager, &pos)`. That function gets the territory ids at the position (`[0x1417f5490]+0x240808`, `0x1422af510`) and calls `0x141da4cf0` for each.
  - Variable `project_id` (`0xe1054e68`), read with `0x14300b8d0` (get variable), `0x14300a8c0` (node memory), `0x143005f70` (is set), `0x143005d90` (value pointer). `0xdeadbeef` means unset.
  - `0x141e66120(stronghold, &projectId)` finds the project. The status byte at project `+0x46`: 1 fires `completed` (`0xfd7350ca`), 2 fires `built` (`0xd3634003`), else `incomplete` (`0x2d56ffd0`). Outputs are fired with `0x14300b330(node, context, pin)`.
- Stronghold manager `[0x1417f5fd8]`: count at `+0x7c`, weak handles {object, control block} at `+0x18 + i*16` (use count at control `+8`; `0x141d92060` locks a handle), cached object ids at `+0x68 + i*4`. `0x141da4cf0(manager, territoryId)` returns the stronghold whose `+0x128` equals the territory id.
- Stronghold object (vtable `0x141270ae8`): territory id at `+0x128` (hash of `enc1000`, `enc1010`, `enc1020`, `enc2010`, `enc2020`), projects in `[+0x1c0, +0x1c8)`, 0x48 bytes each, project id hash at `+0`, status at `+0x46`.
- Runtime check on 2026-10-20 (`tools/sh.py`): 5 strongholds, all loaded wherever the player is. `enc1000` has one project (probably Gastown). `cleanup_crew` (`0x29e5a2c0`) status: `enc1010` 2, `enc1020` 2, `enc2010` 0, `enc2020` 0.

## The mod

- Hook `0x141ea4bc0`. For pin `in`, stronghold variable not set and `project_id` = `cleanup_crew`: if any stronghold has `cleanup_crew` status 2, fire `built` and return 1 without calling the game. Otherwise call the original.
- Signatures (all checked before hooking): `0x141ea4bc0` (0x14c bytes, the whole handler up to the output call, so the helper calls and the manager global are checked too), `0x141e66120` (0x2a bytes, project list layout), `0x141da4cf0` (0x62 bytes, manager layout). On 2026-10-20 the bytes in the dump matched the running game (`tools/sigcmp.py`).

## Analysis tools (`tools/`)

- `extract_adf.py <exe> <out dir>`: writes the ADF type libraries embedded in the exe (41 in the dump). The GraphScript types (`GSGraph`, `GSNode`, `GSDataSet`, `GSData`) are in the one at file offset `0x148be70`. Gibbed ConvertAdf cannot read that library (it supports only structure and array types), so `.gsrc` files need `adf.py`/`gs.py`.
- `hashdb.py <out.pkl> <files...|@list>`: builds a {hashlittle: string} dictionary from printable strings in the files (the dump plus all `.gsrc` files gave 1.25 million). `adf.py` and `gs.py` load `hashdb.pkl` from their own folder.
- `adf.py <file> [typelib dir]`: generic ADF v4 instance dump.
- `gs.py <file.gsrc>`: prints a GraphScript graph: nodes, their data and pin links, with values resolved from the graph data block.
- `sh.py`: reads the stronghold manager from the running game and lists each stronghold's territory and project statuses (read-only).
- `sigcmp.py <va:len>...`: compares code bytes of the dump with the running game and prints them as a C array.
- Extract game files with `../tools/Gibbed.MadMax/out/Gibbed.MadMax.Unpack.exe -o -f "<regex>" <archive>.tab <out dir>`. The graph is in `archives_win64/game37`, the vehicle `.blo` files in several archives; `ConvertProperty` reads `.blo`.
