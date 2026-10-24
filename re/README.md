# Mad Max game internals (shared notes for native mods)

Moved from the removed CarAutoLoot mod on 2026-10-21. Steam build of MadMax.exe, image base 0x140000000. Mod-specific internals are in each mod's README.

## Protection

- On disk, the code is encrypted: sections `.xpdata`/`.xtext`, no `.text`. Static analysis needs a decrypted image of the running game (`MadMax_dump.exe` in this folder, not included in the repository).
- Code is ready about 300 ms after process start. Wait for signature bytes before hooking.
- Anti-disassembly: junk bytes after `jmp`/`ret`, and constants split as `mov r, A; lea r, [r + B]`. Linear disassembly shows garbage after unconditional jumps.
- The protector inserts code checksum guards (example right after `0x141d9b038`). Up to 112 MinHook inline hooks ran stable, but keep hooks few.
- `RtlCaptureStackBackTrace` fails from hook stubs. Use stack scanning (scan the stack for return addresses in the code range).
- MSVC RTTI is present, so class names can be read from vtables (read the RTTI complete object locator at vtable - 8).

## Addresses

| What | Address / chain |
|------|-----------------|
| Player `CCharacter` | `[[[0x1417f5338] + 0x37a8] + 0x20]` (script object = this + 8) |
| Player world position | `player + 0x208`: float x, height, z |
| Player inventory | `[player + 0x438]` |
| Player scrap resource id | u16 at `inventory + 0x40` (`0xffff` = none) |
| Vehicle of a character | `0x141c82910(CCharacter*)` returns the vehicle or null |
| `SetEconomyResourceAmount` | `0x141d9ad90(u16 id, float amount, bool, int)` |
| Per-frame, game thread | `0x141e4b900`, `0x141fc8640` (~65 calls/s) |
| Main loop (never returns, do not use for a tick) | `0x141b79830` |
| Script native tables | in `.data` near `0x17ad000` (RVA) |

## Economy resource table

- Header `[0x1417f6428]` = {u32 static count (1520), u32 used count, entry pointer}. Entries from the static count up to the used count are a dynamic pool for streamed objects (car debris and others). The used count also shrinks, and the ids get reused for other objects. Allocation (`0x141d8dd20`) looks up the u64 key at entry `+0` first.
- Entry, 24 bytes: u32 hash, u32, u8 type, pad, float amount at `+0xC`, u32 timestamp at `+0x10`, u32.
- Positions `[[0x1417f6420]]`: float[3] per entry. Note the double dereference.
- Types `[[0x1417f6418]]`, 36 bytes each: {u32 name hash (Jenkins lookup3, seed 0), u32 category, float[7]}. The last float is 1.0 for regular pickups.
- Category 3 = scrap. Type 68 = `player_scrap` (hash `0xb31e209f`). Type 38 (cap 10000) and 69 (hash = "bool") are not world piles. World piles are types 15-37 with max ≤ 100. Seen collected: 15, 18, 24 (loot drop), 27, 28 (car debris), 29, 31, 34, 37.
- Type fields (`typeinfo.py` f0-f6): f0 regrowth (non-zero sets the entry timestamp), f2 = f3 = max, f6 = probability. `SetEconomyResourceAmount` clamps to f3. Probability: 1 for piles, 0.1-0.35 for loot drops (types 21-25), 0 for counters (38, 68 `player_scrap` with max 1e6, 69, 72).
- Other categories (names unknown): 0 = large stores (fuel/water?, 1000), 1 = 250/500/1000 stations, 4 = collectibles (amount 1).

## Script natives

- Each table row is {name, flags, fn, hash}, but **the fn in row k belongs to the name in row k+1**. Example: the row "IsCharacter" holds GetPlayer's fn `0x1420a5e30`. `../tools/re/natives.py` output is NOT corrected for this.
- flags `0x0000RRAA........`: RR = return count, AA = argument count.
- Values are 16-byte XVM values. `0x142fb7dd0(ctx, &value, typeHash)` converts a value to an object.

## Pickup flow (on foot)

- Scrap piles have a `CInteractionContext` with a `CInteractionComponentLinkToResource` and `CInteractionScriptCondition`s (`can_pickup.xvmc`). These conditions are evaluated every frame against `CInteractionUserProxy` while the player is near.
- On pickup, a graph script node (code near `0x141ebc825`) calls `SetEconomyResourceAmount` for the pile and then for `player_scrap`. Logged on 2026-10-18: `(pile, 0, notify 1, source 0)` and `(player_scrap, total, notify 1, source 0)`. Loot drops ("전리품 수집") take the same path.
- `SetEconomyResourceAmount(id, amount, notify, source)`: with notify, category ≠ 4 and amount > 0 it calls `0x141d9aa50` (`PostResourceEvent(id, useType, |delta| in xmm2, source)`). That posts a `ResourceEvent` (vtable `0x14125c4f8`, type id `0x5fafb8f0`) to the event manager `[0x1417f4ef8]` via `0x1430105a0`. Fields from event `+0x78`: type hash, category, useType, amount, region, territory, source, is_player, total, abs amount. Scripts receive it as `resourceEvent`. It is the only event posted through `0x1430105a0` during an on-foot pickup, so it most likely drives the "+N scrap" HUD message (hidden in vehicles). Other posting thunks were not checked. Then an event disables the context: `CInteractionContext` handler at vtable `@0xd0[1]` = `0x141fe1a50`, which compares event hashes at `+0x2a0` (enable) and `+0x2a8` (disable) and calls vfunc `0x130` = SetEnabled(bool).
- Loot bags (`CLootBag`) use events (`CLootBag@0xd0[1]` = `0x141f51620`).
- `CCharacter` give-item native path `0x1420ca090` → `0x141ccd230` is for items and weapons. Scrap does not use it.

## Dead ends (do not repeat)

- Pure `dropzone` script mod: there is no XVM assembler (`XvmModule.Serialize` throws). The script API has no "objects in radius" query (`collision.CastSphere` returns only a bool).
- The HUD scrap value (int, 950→954) is a per-frame copy written by the HUD code. The real value is the economy entry float.
- `0x141b79830` as the frame hook: it is the main loop, so the hook ran once.
- The research build's first hook used wrong signature bytes (`0x40 0x57` instead of `0x57`). Always take signature bytes from a dump.

## Analysis tools (`../tools/re/`)

Setup: `python -m venv venv`, then `venv/Scripts/pip install pefile capstone`. The default `D:\Program Files\Python\python.exe` has no venv or pip; use `%USERPROFILE%\AppData\Local\Programs\Python\Python310\python.exe`. Run scripts with `-I`.

| Script | Use |
|--------|-----|
| `exestr.py <exe> <out.txt>` | Strings with offsets. |
| `strings.py <file>` | Unique strings in a file. |
| `dis.py <dump> <va> [n]` | Disassemble at a VA and resolve RIP-relative strings. |
| `tbl.py <dump> <rva> <n>` | Print qwords and strings (pointer tables). |
| `natives.py <dump> <out>` | List script native tables (remember the row shift). |
| `xref.py <dump> names...` | Find code (`lea`) and absolute pointers that reference strings. |
| `funcof.py <dump> <natives.txt> va...` | Map addresses to functions via `.pdata`. |
| `classinfo.py <dump> classes...` | RTTI class: vtables, class-unique methods, constructors. |
| `gentable2.py <dump> <out.inc> <classinfo outputs...>` | Generate a hook target table (C include) for a research build. |
| `scan.py first\|next <value>` | Value scanner (int32 and float) on the running game. |
| `peek.py` | Print the player and scrap entry from the running game. |
| `snap.py save\|diff` | Snapshot the economy table and diff it (find which entry a pickup changed). |
| `typeinfo.py` | Print economy types (category and values). |
| `typenames.py <wordlists...>` | Brute-force type names by Jenkins hash. |
| `near.py [radius]` | Print economy entries near the player (default 40 m) and whether `IsScrapPile` accepts them. |

Game data tools (Gibbed) are in `../tools/Gibbed.MadMax/` (build: `dotnet build -c Release` in each tool's project folder; changes: `MODIFIED.md`).

## Economy resource objects (found 2026-10-19, not used by any mod yet)

- A dynamic economy entry (id >= the static count) belongs to a `CEconomyResource` game object (vtable `0x14125c698`, u16 id at `+0xd0`). Its destructor `0x141d94a90` frees the entry by swapping in the last entry (and its position), looks up the moved entry's object by key (`0x142310240` -> `0x141cbda30`) to update its id, decrements the used count and sets its own id to `0xffff`.
- A loot drop is a set of dynamic entries at the same position (for example type 24 scrap + type 54). An on-foot pickup destroys these objects, which also removes the floating loot UI. Setting only the amount to 0 leaves the object and its UI.
