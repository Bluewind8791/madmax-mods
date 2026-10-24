// NoFallDamage: the player takes no damage from falls.
// Loaded as an .asi plugin by Ultimate ASI Loader. Delete nofalldamage.asi from the game folder to uninstall.
//
// How it works (addresses are for the current Steam build of MadMax.exe):
// - When a character lands, CCharacter::OnContact (0x141ceb980) builds a DamageInfo with damage type "Fall"
//   and calls CCharacter::ApplyDamage (vtable +0xb8, 0x141ce5bc0).
// - This mod hooks ApplyDamage. For the player and damage type "Fall", it returns 0 without calling the game.
//   All other damage (and fall damage of other characters) goes through unchanged.
// - After a very high fall the landing also sends a damage event (90000 damage) to the character's event
//   handler (0x141cf3e70), which calls ApplyDamage from 0x141cf53dc. The mod blocks damage from that call
//   site for the player only within a short time after a blocked fall.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <share.h>
#include <intrin.h>
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
    unsigned char bytes[16];
};
static const Signature kApplyDamage = {
    0x141ce5bc0, {0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x48, 0x89, 0xe5, 0x48, 0x81, 0xec, 0x80, 0x00, 0x00}};

struct DamageInfo
{
    void *vtable;
    float damage;
    float unknown0c;
    uint32_t type; // Jenkins lookup3 hashlittle(name, 0) of the damage type name
    int32_t unknown14;
    uint8_t unknown18;
    uint32_t unknown1c;
};

typedef float(__fastcall *ApplyDamage_t)(void *character, DamageInfo *info);
static ApplyDamage_t g_originalApplyDamage;

static const uint32_t kFallDamage = 0x61e9694d; // "Fall"

// The ApplyDamage call in the character event handler: mov rax,[rbx]; lea rdx,[rbp+58h]; mov rcx,rbx;
// call [rax+0B8h]. Its return address identifies damage events.
static const Signature kEventDamageCall = {
    0x141cf53d2, {0x48, 0x8b, 0x03, 0x48, 0x8d, 0x55, 0x58, 0x48, 0x89, 0xd9, 0xff, 0x90, 0xb8, 0x00, 0x00, 0x00}};
static const uintptr_t kEventDamageReturn = 0x141cf53e2;
static const ULONGLONG kFallDeathWindowMs = 2000; // damage events this soon after a blocked fall are blocked
static bool g_eventDamageMatches;
static volatile ULONGLONG g_lastFallBlockTick;

static const char *DamageTypeName(uint32_t type)
{
    switch (type)
    {
    case 0x1ccaf74a: return "Melee";
    case 0x91571cf0: return "Bullet";
    case 0x8e29c151: return "Explosive";
    case 0xb0a4a0c0: return "Impact";
    case kFallDamage: return "Fall";
    case 0x82658f08: return "Lightning";
    case 0x638dd466: return "Harpoon";
    }
    return "?";
}

static bool Matches(const Signature &s)
{
    return memcmp((void *)s.address, s.bytes, sizeof(s.bytes)) == 0;
}

static void *GetPlayer()
{
    uintptr_t mgr = *(uintptr_t *)kGameManager;
    if (!mgr) return nullptr;
    uintptr_t holder = *(uintptr_t *)(mgr + 0x37a8);
    return holder ? *(void **)(holder + 0x20) : nullptr;
}

static volatile LONG g_loggedCalls;

static float __fastcall HookApplyDamage(void *character, DamageInfo *info)
{
    bool isPlayer = false;
    __try
    {
        isPlayer = character == GetPlayer();
        if (isPlayer && info->type == kFallDamage)
        {
            g_lastFallBlockTick = GetTickCount64();
            Log("Blocked fall damage %.1f (character+0x17a0 = %.3f)", info->damage,
                *(float *)((char *)character + 0x17a0));
            return 0.0f;
        }
        if (isPlayer && g_eventDamageMatches && (uintptr_t)_ReturnAddress() == kEventDamageReturn &&
            g_lastFallBlockTick && GetTickCount64() - g_lastFallBlockTick < kFallDeathWindowMs)
        {
            Log("Blocked fall death damage %.1f (type %08x)", info->damage, info->type);
            return 0.0f;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("Hook failed with exception %08lx", GetExceptionCode());
    }
    float dealt = g_originalApplyDamage(character, info);
    // The first player damage calls show which damage types the game uses.
    if (isPlayer && InterlockedIncrement(&g_loggedCalls) <= 50)
        Log("Player damage: type %s (%08x), base %.1f, dealt %.1f", DamageTypeName(info->type), info->type,
            info->damage, dealt);
    return dealt;
}

#ifdef RESEARCH
// Research build: logs every health decrease of the player with its callers, to find deaths that do not go
// through ApplyDamage. SetHealth(entity, newHealth, &damageType): health float at +0x184.
static const Signature kSetHealth = {
    0x141d6a0d0, {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x70, 0x80, 0xb9, 0xea, 0x00, 0x00, 0x00}};

typedef void(__fastcall *SetHealth_t)(void *entity, float health, const uint32_t *type);
static SetHealth_t g_originalSetHealth;
static volatile LONG g_loggedSetHealth;

static void __fastcall HookSetHealth(void *entity, float health, const uint32_t *type)
{
    __try
    {
        float old = *(float *)((char *)entity + 0x184);
        if (entity == GetPlayer() && health < old && InterlockedIncrement(&g_loggedSetHealth) <= 200)
        {
            char callers[512];
            int len = 0;
            uintptr_t *stack = (uintptr_t *)_AddressOfReturnAddress();
            for (int i = 0, found = 0; i < 128 && found < 10; i++)
                if (stack[i] >= 0x141b38000 && stack[i] < 0x1432c4000)
                {
                    len += sprintf_s(callers + len, sizeof(callers) - len, " %llx", (unsigned long long)stack[i]);
                    found++;
                }
            Log("SetHealth player %.1f -> %.1f, type %s (%08x), callers:%s", old, health, DamageTypeName(*type),
                *type, callers);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("SetHealth hook failed with exception %08lx", GetExceptionCode());
    }
    g_originalSetHealth(entity, health, type);
}
#endif

static DWORD WINAPI InitThread(LPVOID)
{
    // The protector decrypts code after start-up. Wait until the expected code appears.
    for (int i = 0; i < 6000; i++)
    {
        __try
        {
            if (Matches(kApplyDamage))
            {
                g_eventDamageMatches = Matches(kEventDamageCall);
                if (!g_eventDamageMatches) Log("Event damage call does not match; high-fall deaths are not blocked.");
                MH_Initialize();
                MH_STATUS s = MH_CreateHook((void *)kApplyDamage.address, (void *)HookApplyDamage,
                                            (void **)&g_originalApplyDamage);
                if (s == MH_OK) s = MH_EnableHook((void *)kApplyDamage.address);
                Log("Damage hook: %s", MH_StatusToString(s));
#ifdef RESEARCH
                if (Matches(kSetHealth))
                {
                    s = MH_CreateHook((void *)kSetHealth.address, (void *)HookSetHealth,
                                      (void **)&g_originalSetHealth);
                    if (s == MH_OK) s = MH_EnableHook((void *)kSetHealth.address);
                    Log("SetHealth hook (research): %s", MH_StatusToString(s));
                }
                else
                    Log("SetHealth signature does not match");
#endif
                return 0;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        Sleep(100);
    }
    Log("Game code did not match. This game version is not supported; NoFallDamage is inactive.");
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
        strcpy_s(name, MAX_PATH - (name - path), "nofalldamage.log");
        g_log = _fsopen(path, "w", _SH_DENYNO);
        Log("NoFallDamage loaded.");
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
