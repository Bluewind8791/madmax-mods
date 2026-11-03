Ultimate ASI Loader 9.7.4 (x64) for Mad Max (Steam)

Loads .asi plugin mods (InfiniteAmmo, NoFallDamage, FovChanger,
CleanupCrewAnywhere and others) when the game starts.
Without it, .asi files in the game folder do nothing.

Unchanged dinput8.dll from ThirteenAG/Ultimate-ASI-Loader v9.7.4
(Ultimate-ASI-Loader-NoPDB_x64.zip). https://github.com/ThirteenAG/Ultimate-ASI-Loader
License: MIT, see UltimateASILoader_LICENSE.txt.

Install
- Vortex: add this archive and enable it. Install it once, together with the .asi mods.
- Manual: copy dinput8.dll into the game folder (next to MadMax.exe).

Check
- Start the game once. Each .asi mod writes its own <mod>.log in the game folder.
  No log means the loader did not run.

Conflict
- Another mod that also ships dinput8.dll (for example MadMaxCrashFix) cannot be used
  with this archive as is. Keep the other mod's dinput8.dll and copy this loader into
  the game folder by hand as winmm.dll or version.dll instead.

Uninstall
- Disable it in Vortex, or delete dinput8.dll. All .asi mods stop loading.
