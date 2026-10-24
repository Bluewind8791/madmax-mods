// InfiniteAmmo: inventory ammo never goes down. Each ammo type can be switched off in infiniteammo.ini.
// Loaded as an .asi plugin by Ultimate ASI Loader. Delete infiniteammo.asi from the game folder to uninstall.
//
// How it works (addresses are for the current Steam build of MadMax.exe):
// - Ammo is an inventory item: {u32 item hash, u16 count} entries in the character's inventory.
// - When a weapon loads rounds, the game calls TakeInventoryItems(inventory, &hash, amount), which lowers
//   the count and returns how many it took. This mod hooks that function. For the player's enabled ammo types,
//   it returns the requested amount without lowering the count, as long as the player has at least one round.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <share.h>
#include "MinHook.h"

// --- logging ---------------------------------------------------------------

static FILE *g_log;

static void Log(const char *fmt, ...)
{
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);
}

// --- game addresses ------------------------------------------------------------

static const uintptr_t kGameManager = 0x1417f5338; // [mgr + 0x37a8] + 0x20 = player CCharacter

struct Signature
{
    uintptr_t address;
    unsigned char bytes[8];
};
// Virtualized function; the game calls it through the protector thunk 0x141e006b0.
static const Signature kTakeInventoryItems = {0x141fffbf0, {0x51, 0x48, 0x8d, 0x0d, 0xdd, 0x11, 0x36, 0x00}};

typedef int(__fastcall *TakeInventoryItems_t)(void *inventory, const uint32_t *itemHash, int amount);
static TakeInventoryItems_t g_originalTake;

// Item hashes = Jenkins lookup3 hashlittle(name, 0).
struct AmmoType
{
    const char *setting; // key in infiniteammo.ini
    uint32_t hash;
    const char *item;
    bool enabled;
};
static AmmoType g_ammo[] = {
    {"Shotgun", 0xa83a99c8, "shotgun_shell", true},
    {"Sniper", 0x33f90c70, "signature_sniper_bullet", true},
    {"Thunderpoon", 0x101e5fcd, "thunderstick", true},
    {"Thunderpoon", 0xde7296a9, "thunderstick_cap", true},
    {"Shivs", 0x97f610e7, "shiv", true},
};
static const int kAmmoCount = sizeof(g_ammo) / sizeof(g_ammo[0]);

static bool Matches(const Signature &s)
{
    return memcmp((void *)s.address, s.bytes, sizeof(s.bytes)) == 0;
}

static void *GetPlayerInventory()
{
    uintptr_t mgr = *(uintptr_t *)kGameManager;
    if (!mgr) return nullptr;
    uintptr_t holder = *(uintptr_t *)(mgr + 0x37a8);
    if (!holder) return nullptr;
    uintptr_t player = *(uintptr_t *)(holder + 0x20);
    return player ? *(void **)(player + 0x438) : nullptr;
}

struct InventoryEntry
{
    uint32_t hash;
    uint16_t count;
    uint8_t unknown6;
    uint8_t unknown7;
};
static_assert(sizeof(InventoryEntry) == 8, "inventory entry size");

static int CountInInventory(void *inventory, uint32_t hash)
{
    InventoryEntry *begin = *(InventoryEntry **)inventory;
    InventoryEntry *end = *(InventoryEntry **)((char *)inventory + 8);
    for (InventoryEntry *e = begin; e < end; e++)
        if (e->hash == hash) return e->count;
    return 0;
}

static volatile LONG g_loggedCalls;

static int __fastcall HookTake(void *inventory, const uint32_t *itemHash, int amount)
{
    __try
    {
        if (amount > 0 && inventory == GetPlayerInventory())
            for (int i = 0; i < kAmmoCount; i++)
            {
                if (g_ammo[i].hash != *itemHash || !g_ammo[i].enabled) continue;
                int count = CountInInventory(inventory, *itemHash);
                if (count <= 0) break;
                if (InterlockedIncrement(&g_loggedCalls) <= 20) Log("Kept %d %s (have %d)", amount, g_ammo[i].item, count);
                return amount;
            }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("Hook failed with exception %08lx", GetExceptionCode());
    }
    return g_originalTake(inventory, itemHash, amount);
}

static void LoadSettings(const char *iniPath)
{
    for (int i = 0; i < kAmmoCount; i++)
    {
        g_ammo[i].enabled = GetPrivateProfileIntA("InfiniteAmmo", g_ammo[i].setting, 1, iniPath) != 0;
        Log("%s (%s): %s", g_ammo[i].setting, g_ammo[i].item, g_ammo[i].enabled ? "infinite" : "normal");
    }
}

static char g_iniPath[MAX_PATH];

static DWORD WINAPI InitThread(LPVOID)
{
    LoadSettings(g_iniPath);
    // The protector decrypts code after start-up. Wait until the expected code appears.
    for (int i = 0; i < 6000; i++)
    {
        __try
        {
            if (Matches(kTakeInventoryItems))
            {
                MH_Initialize();
                MH_STATUS s = MH_CreateHook((void *)kTakeInventoryItems.address, (void *)HookTake, (void **)&g_originalTake);
                if (s == MH_OK) s = MH_EnableHook((void *)kTakeInventoryItems.address);
                Log("Ammo hook: %s", MH_StatusToString(s));
                return 0;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        Sleep(100);
    }
    Log("Game code did not match. This game version is not supported; InfiniteAmmo is inactive.");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        char path[MAX_PATH];
        GetModuleFileNameA(module, path, MAX_PATH);
        char *name = strrchr(path, '\\') + 1;
        strcpy_s(g_iniPath, path);
        strcpy_s(g_iniPath + (name - path), MAX_PATH - (name - path), "infiniteammo.ini");
        strcpy_s(name, MAX_PATH - (name - path), "infiniteammo.log");
        g_log = _fsopen(path, "w", _SH_DENYNO);
        Log("InfiniteAmmo loaded. Settings: %s", g_iniPath);
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
