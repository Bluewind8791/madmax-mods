# NoFallDamage mod: no health loss from falling

## Status (2026-10-18)

- Goal: when Max lands after a fall, he loses no health and does not die.
- Existing mods: searched the web (Nexus Mods) on 2026-10-18. No fall damage mod for Mad Max was found.
- v1 (2026-10-18): blocked `Fall` damage only. In game, normal falls were blocked, but Max still died from very high falls (game over).
- v2 (2026-10-18): also blocks the 90000 "fall death" damage event (see below). Research build test at 18:17: a high fall logged `Blocked fall damage 471.9` and `Blocked fall death damage 90000.0`, and no health loss followed. Smaller falls (15.5, 110.0, 309.8) were blocked too. The user confirmed on screen: Max survived, and enemy/car damage works as normal.
- Release: `release/NoFallDamage-1.0.0.zip` (`nofalldamage.asi` + `NoFallDamage_readme.txt`) for Vortex.
- How it works: hooks `CCharacter::ApplyDamage` (`0x141ce5bc0`). For the player it returns 0 and skips the original when:
  - the damage type is `Fall`, or
  - the call comes from the character event handler's damage call (return address `0x141cf53e2`) within 2 s after a blocked fall.
- No settings file: uninstall = delete the `.asi`.

## Files

Paths are relative to this folder (`E:\mods\Mad Max\nofalldamage`). `<game folder>` = `C:\Program Files (x86)\Steam\steamapps\common\Mad Max`.

| Path | Purpose |
|------|---------|
| `src/nofalldamage.cpp` | The mod. One MinHook hook on `ApplyDamage`. With `RESEARCH` defined it also hooks `SetHealth` and logs every player health decrease with its callers. |
| `src/build.bat` | Builds `src/build/nofalldamage.asi`; `build.bat research` builds `src/build/nofalldamage_research.asi`. Needs VS 2022 C++. Uses MinHook from `../third_party/minhook/`. |
| `release/NoFallDamage_install.bat`, `release/NoFallDamage_uninstall.bat` | Manual install/uninstall without Vortex (double-click). Not in the archive. |
| `<game folder>/nofalldamage.log` | Log. |

## Install, uninstall, test

- Requires Ultimate ASI Loader as `<game folder>/dinput8.dll` (see `../README.md`).
- Install: copy `src/build/nofalldamage.asi` to the game folder. Uninstall: delete it (and `nofalldamage.log`).
- Without Vortex: double-click `release/NoFallDamage_install.bat`. It finds the game folder (Steam registry, then the default path, then asks) and extracts `nofalldamage.asi` from the newest `NoFallDamage-*.zip` next to it with Windows `tar.exe`. `release/NoFallDamage_uninstall.bat` deletes `nofalldamage.asi` and `nofalldamage.log`. Both stop if the game is running or if `vortex.deployment.json` lists `nofalldamage.asi`. Optional argument: the game folder.
- A healthy start writes:
  ```
  NoFallDamage loaded.
  Damage hook: MH_OK
  ```
- Each blocked fall writes `Blocked fall damage X (character+0x17a0 = Y)`. A blocked fall death writes `Blocked fall death damage 90000.0 (type c9f61d0d)`. The first 50 other player damage calls write `Player damage: type T (hash), base B, dealt D`.
- If the event damage call site does not match, the log says `Event damage call does not match; high-fall deaths are not blocked.` and only `Fall` damage is blocked.
- Test: jump from about 15 m, then from a very high place (a tower or cliff, where v1 still died). Check that other damage (enemies, cars hitting Max) still works.

## Game internals (Steam build, image base 0x140000000, from `../re/MadMax_dump.exe`)

### Damage types

- The game has damage type names `Melee`, `Bullet`, `Explosive`, `Impact`, `Fall`, `Lightning`, `Harpoon` (strings near RVA `0x1259b68`).
- Static initializers store the Jenkins lookup3 hash (`hashlittle(name, 0)`) of each name in a global:

| Type | Hash | Global |
|------|------|--------|
| `Melee` | `0x1ccaf74a` | `0x1417f6298` |
| `Bullet` | `0x91571cf0` | `0x1417f629c` |
| `Explosive` | `0x8e29c151` | `0x1417f62cc` |
| `Impact` | `0xb0a4a0c0` | `0x1417f6260` |
| `Fall` | `0x61e9694d` | `0x1417f6264` (written by `0x14324b320`) |
| `Lightning` | `0x82658f08` | `0x1417f62d0` |
| `Harpoon` | `0x638dd466` | ? |
| unknown | `0xc9f61d0d` | `0x1417f6284`. Generic type: script `InflictDamage`/`InstantDeath`, damage events, and the type `SetHealth` receives for every hit. |
| unknown | `0xf63a5ca1` | `0x1417f6290`. Damage over time (fire?), 20-50 per tick. |

- Code that reads the `Fall` global (all found with a RIP-relative scan):
  - `0x141cebec9` in `CCharacter::OnContact` (below). This is the only place that builds a `Fall` damage info.
  - `0x141ce6a20`, `0x141d4df9d`, `0x141d4f5c0`/`0x141d4f660`, `0x141d6c570`: per-type resistance tables (set, get, load from data). Not on the damage path.

### CCharacter vtable `0x141251308`

- `[2]` `0x141ceb980` = `OnContact(CCharacter*, Contact*)`: physics contact handler.
- `[23]` (`+0xb8`) `0x141ce5bc0` = `ApplyDamage(CCharacter*, DamageInfo*)`, returns the damage dealt (float, `xmm0`).

### Fall damage (in `OnContact`, `0x141cebe89`)

```
speed = -((contact[0x50] - contact[0x44]) * contact[0x4])   // velocity change along the contact normal
if speed > 15.0 (0x141239d2c):
    info = { vtable 0x14123bd08, damage = (speed - 15.0) * 100.0 (0x141233260), 0.0, type = [0x1417f6264] (Fall), -1, 0, 0 }
    character->vtable[0xb8](&info)                            // ApplyDamage, result ignored
```

- 15 m/s is a free fall of about 11.5 m.
- The same function also applies `Impact` damage (type `[0x1417f6260]`) when another object hits the character. The mod must not touch that.

### ApplyDamage `0x141ce5bc0`

- Signature: `55 56 57 41 54 41 55 48 89 e5 48 81 ec 80 00 00 00 48 c7 45 a0 fe ff ff ff` (normal prologue, safe for MinHook).
- `DamageInfo`: `+0x00` vtable, `+0x08` float damage, `+0x0c` float, `+0x10` u32 type hash, `+0x14` int (-1 for falls), `+0x18` u8, `+0x1c` u32.
- Flow: damage = `info->vtable[1]()` × per-type multipliers (`character + 0x450`); copies the info to `character + 0x31f0`; calls `0x141d6a260` (health reduction); then posts a `DamageReceivedEvent`-like event through `0x1430105a0`.
- Hook: `if (character == player && info->type == [0x1417f6264]) return 0.0f;` else call the original. Read the hash from the global at run time, or compare with `0x61e9694d`.

### Fall death: damage event 90000

- Found with the research build (2026-10-18 18:05): after a high fall, in the same frame as the blocked `Fall` damage, `ApplyDamage` received damage 90000 with type `0xc9f61d0d` and Max died.
- Caller: the character event handler `0x141cf3e70`. It compares the event name with name slots of the object (`rsi + 0x968`, ..., `rsi + 0x680`) through `0x141c82d30`/`0x142981d90`. The damage event case parses the event's value string with an imported function (`call [0x1432c2b10]`, returns a double, likely `atof`), builds a `DamageInfo` (type `[0x1417f6284]`, `+0x14` = -1) and calls `vtable[0xb8]` (`ApplyDamage`) at `0x141cf53dc`; return address `0x141cf53e2`.
- The event name is unknown (the name slots are filled at run time). The landing animation or state likely sends it. The 90000 event came with fall damage 542.4 and 471.9, but not with 489.9, so the trigger is not the `Fall` damage amount.
- The same handler may send damage events in other situations. The mod blocks this call site only within 2 s after a blocked `Fall` damage.

### SetHealth `0x141d6a0d0`

- `SetHealth(entity, float newHealth, const u32* type)`. Signature `48 89 5c 24 08 57 48 83 ec 70 80 b9 ea 00 00 00`. 22 callers.
- Health float at `entity + 0x184` (previous value at `+0x188`), dead flag `+0xe8`, invulnerability flags `+0xe9`/`+0xea`. At health ≤ 0 it sets the dead flag and calls vfuncs `0xd0`/`0xd8`. Player max health 1500.
- `ApplyDamage` reaches it through `0x141d6a260` (return address `0x141d6a4c1`).
- Script native `InstantDeath` (`0x1420cae00`) calls `SetHealth(char, 0.0, &[0x1417f6284])` directly. Script native `InflictDamage` is `0x1420cad20`. The graph node `InflictDamage` (`0x141ea5f90`) calls `ApplyDamage` (return address `0x141ea613b`); with its kill flag it sets damage = health + 1. In game it dealt 500-damage hits (unrelated to falls).

### CFallDamageCondition (state machine condition)

- Vtable `0x141240f00`. `Evaluate` (`0x141c70090`) returns true when float `character + 0x17a0` > 1.5 (`0x141239738`).
- The only static writer of `character + 0x17a0` is the `CCharacter` constructor (`0x141ce9124`, writes a register value). The runtime writer is unknown (maybe virtualized code).
- Its role is unknown. In game the value was 0.000 at every blocked fall, including the falls that sent the 90000 damage event.
- `CFallState` (vtable `0x14123e628`) keeps fall data in `[character + 0x17a8]` (`+0x580` accumulates time while falling).

### Player

- Player `CCharacter` chain: see `../re/README.md` (`[[[0x1417f5338] + 0x37a8] + 0x20]`).

## Next steps

1. Remove `nofalldamage_research.asi` from the game folder and install the release zip with Vortex.
2. Optional: find the name of the damage event (log the event name in `0x141cf3e70`) to replace the 2 s time window with an exact match.
