# InfiniteAmmo mod: ammo never runs out

## Status (2026-10-17)

- v1 works for the shotgun and the sniper rifle. Confirmed in game on 2026-10-17: 5+ shotgun shots and several sniper shots, counts did not go down (log: `Kept 1 shotgun_shell`, `Kept 3 signature_sniper_bullet`; the sniper rifle loads 3 rounds at once).
- The Thunderpoon and shivs are not tested: the user's save has not unlocked them yet.
- Requirement from `../MOD_TODO.md`: infinite ammo for every ammo type, switchable in settings. Implemented as one on/off switch per ammo type in `infiniteammo.ini`.
- Meaning of "infinite": the inventory count never goes down while it is at least 1. Weapons still reload. With 0 rounds the game behaves normally.
- Existing mods: searched Nexus Mods on 2026-10-17. No `.asi`/standalone infinite-ammo mod for Mad Max was found. Trainers exist (for example CheatHappens), but they are separate programs, not mods.
- Open questions:
  - Do the Thunderpoon and shivs go through the same function? Test when the save unlocks them. Install the research build, watch their counts, and check that `infiniteammo.log` writes `Kept`.
  - Which of `thunderstick` / `thunderstick_cap` is the Thunderpoon ammo? Both are under the `Thunderpoon` switch.
  - Unknown inventory item `0xba9679ea` (name not found yet).

## Files

Paths are relative to this folder (`E:\mods\Mad Max\infiniteammo`). `<game folder>` = `C:\Program Files (x86)\Steam\steamapps\common\Mad Max`.

| Path | Purpose |
|------|---------|
| `src/infiniteammo.cpp` | The mod. Hooks `TakeInventoryItems`. |
| `infiniteammo.ini` | Default settings, shipped next to the `.asi`. |
| `release/InfiniteAmmo-1.0.0.zip` | Release archive for Vortex/Nexus: `infiniteammo.asi`, `infiniteammo.ini`, `InfiniteAmmo_readme.txt` at the root. |
| `release/InfiniteAmmo_readme.txt` | Readme shipped in the archive. |
| `release/InfiniteAmmo_install.bat`, `release/InfiniteAmmo_uninstall.bat` | Manual install/uninstall without Vortex (double-click). Not in the archive. |
| `src/research.cpp` | Research build: logs every write to up to 4 watched 2-byte values (hardware breakpoints), F8 writes a marker. |
| `src/build.bat` | `build.bat` = mod (`src/build/infiniteammo.asi`), `build.bat research` = `src/build/infiniteammo_research.asi`. Needs VS 2022 C++. |
| `tools/inv.py` | Prints the player's inventory entries from the running game, with names. `save`/`diff` show what changed. `set <item> <count>` writes a count (test only). |
| `tools/hashnames.json` | Cache for `inv.py`: Jenkins hash of every string in `../re/MadMax_dump.exe`. Delete it after a new dump. |

Run the tools with the Python venv that has `pefile` and `capstone` (see `../re/README.md`, "Analysis tools"). `inv.py` itself needs no packages.

## Install, uninstall, test

- Requires Ultimate ASI Loader as `<game folder>/dinput8.dll` (see `../README.md`).
- Install: copy `src/build/infiniteammo.asi` and `infiniteammo.ini` to the game folder. Uninstall: delete both (and `infiniteammo.log`).
- Without Vortex: double-click `release/InfiniteAmmo_install.bat`. It finds the game folder (Steam registry, then the default path, then asks), extracts `infiniteammo.asi` and `infiniteammo.ini` from the newest `InfiniteAmmo-*.zip` next to it with Windows `tar.exe`, and keeps an existing `infiniteammo.ini`. `release/InfiniteAmmo_uninstall.bat` deletes `infiniteammo.asi`, `infiniteammo.ini` and `infiniteammo.log`. Both stop if the game is running or if `vortex.deployment.json` lists `infiniteammo.asi`. Optional argument: the game folder.
- A healthy start writes this to `<game folder>/infiniteammo.log`:
  ```
  InfiniteAmmo loaded. Settings: ...\infiniteammo.ini
  Shotgun (shotgun_shell): infinite
  ...
  Ammo hook: MH_OK
  ```
  The first 20 kept loads write `Kept N <item> (have M)`.
- Vortex: drop `release/InfiniteAmmo-1.0.0.zip` on the Mods tab and enable it. Vortex's Mad Max default mod type deploys to the game folder root (hardlinks). Delete manually copied mod files first, or they conflict with the deployment.
- Rebuild the archive after every change to the `.asi`, `.ini` or readme (PowerShell `Compress-Archive` with the three files).
- The research build and the mod can be installed together (different file names and logs).
- Research build: copy `src/build/infiniteammo_research.asi` to the game folder. It writes `<game folder>/infiniteammo_research.log`.
- To watch ammo counts: run `tools/inv.py ../re/MadMax_dump.exe`, then write the entry address + 4 of each ammo entry (hex, one per line, at most 4) into `<game folder>/infiniteammo_watch.txt`. The build applies the watch within 1 s. Each write logs the value, `rip` and a stack scan.
- Remove `infiniteammo_research.asi` and `infiniteammo_watch.txt` after use.

## Game internals (Steam build, image base 0x140000000)

### Ammo storage

- Ammo is an inventory item. The player inventory is `[player + 0x438]` (player chain: see `../re/README.md`).
- Inventory `+0x00`/`+0x08`: begin/end of an array of 8-byte entries {u32 item hash, u16 count, u8, u8}.
- Item hash = Jenkins lookup3 `hashlittle(name, 0)` (`0x142fbd230` → `0x142fbc960`).
- Item definitions: `[[0x1417f5f98] + 0x10]` = {records*, u32 count}, 0x70 bytes per record, u32 hash at `+0`. If byte `+0x17` of the record is 1 or 2, the count is the entry's u16; otherwise the game counts separate item objects in the list at inventory `+0x20`.
- Player inventory items seen on 2026-10-17 (all present even at count 0): `thunderstick` `0x101e5fcd`, `shotgun_shell` `0xa83a99c8`, `thunderstick_cap` `0xde7296a9`, `shiv` `0x97f610e7`, `signature_sniper_bullet` `0x33f90c70`, unknown `0xba9679ea`.
- `player_ammo_shotgun`, `player_ammo_sniper`, `player_ammo_thundercap`, `player_shivs` are not item names. They only appear in a stats/telemetry writer (`0x1420fc680`) and a similar list (`0x1420d6060`).

### Taking ammo (found 2026-10-17 with the research build)

- Firing the shotgun lowered the `shotgun_shell` count by 1 per shot. The only writer was `rip=0x142087d46`, inside virtualized code (a VM handler `mov word ptr [rax], dx`). Never hook VM handlers; they are shared by all virtualized code.
- The native caller is the weapon load code in function `0x141f3787e` (at `0x141f379b8`):
  `inv = GetInventory(character)` (`0x141c59cb0`), `hash = WeaponAmmoHash(weapon)` (`0x141dc65a0`), `n = 0x142495cd0(weapon)` (probably rounds to load; thunk), `taken = TakeInventoryItems(inv, hash, n)` via thunk `0x141e006b0`, then `0x1428bc760(weapon, taken)` (thunk).
- `TakeInventoryItems` = `0x141fffbf0` (VM entry `push rcx; lea rcx, [rip+...]; jmp ...`, signature `51 48 8d 0d dd 11 36 00`). The thunk `0x141e006b0` computes it: `[0x14228eb6b] - checksum(bytes 0x14222c7c7..0x14222c7d4)`, checksum `eax = rol((eax + b) << 7, 3)`.
- Calls to the thunk: `0x141cf4b24`, `0x141cf4bbc`, `0x141cf4c63`, `0x141e24cc6`, `0x141eec796`, `0x141f35252`, `0x141f379df`, `0x1420c996a`, `0x1420c9d6a`. One direct `jmp` to `0x141fffbf0` at `0x141eec668`.

### Weapon

- Weapon magazine (rounds loaded): int at `CWeapon + 0x370` (`0x141dc6430`).
- Ammo item hash of a weapon: u32 at `[CWeapon + 0x3e0] + 0x30c` (`0x141dc65a0`).

### Script natives (row shift already applied)

| Native | Function |
|--------|----------|
| `GetWeaponAmmo(weapon)` | `0x1420c9290`, returns the magazine count |
| `GetInventoryAmmo(character, weapon)` | `0x1420c95c0`, counts the weapon's ammo item in the inventory (`0x1420002a0(inventory, &hash)`) |
| `AddInventoryAmmo(character, name, amount)` | `0x1420c9ae0`, calls the protected thunk `0x1423fa2b0(inventory, &hash, amount)` |

`0x1423fa2b0` is a protector thunk (checksum guard) that computes its real target at run time. Do not hook or patch it.

### Conflicts

- CarAutoLoot (deleted on 2026-10-21) hooked the per-frame functions `0x141e4b900` and `0x141fc8640`, so this mod does not use them. They are free now.
