// CleanupCrewAnywhere: the stronghold Cleanup Crew collects scrap from defeated vehicles anywhere on the map.
// Loaded as an .asi plugin by Ultimate ASI Loader. Delete cleanupcrewanywhere.asi from the game folder to uninstall.
//
// How it works (addresses are for the current Steam build of MadMax.exe):
// - A destroyed vehicle runs graphs/open_world/reward_cleanup_crew.gsrc. The graph asks the GetProjectStatus node
//   (0x141ea4bc0) for project "cleanup_crew" without naming a stronghold, and collects the vehicle's scrap only
//   when the answer is "built".
// - Without a stronghold, GetProjectStatus uses the stronghold whose territory contains the player. So the crew
//   only works inside the territory of a stronghold where it was built.
// - This mod hooks GetProjectStatus. For project "cleanup_crew" without a stronghold, it answers "built" when the
//   Cleanup Crew is built in any stronghold. All other calls go to the game unchanged.
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

struct Signature
{
    uintptr_t address;
    const unsigned char *bytes;
    size_t size;
};

// GetProjectStatus node input handler, whole body up to the output pin call. It also checks the calls to the
// graph helpers below and the stronghold manager global.
static const unsigned char kGetProjectStatusBytes[] = {
    0x48, 0x89, 0x5c, 0x24, 0x10, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x48, 0x83, 0xec, 0x30, 0x48, 0x89,
    0xd7, 0x48, 0x89, 0xcb, 0x41, 0x81, 0xf8, 0x4a, 0xa7, 0x4b, 0xc9, 0x0f, 0x85, 0x29, 0x01, 0x00, 0x00, 0x41,
    0xb8, 0x0f, 0xa2, 0xff, 0xe6, 0x45, 0x8d, 0x80, 0x17, 0x7e, 0x3d, 0x2f, 0xe8, 0xdb, 0x6c, 0x16, 0x01, 0x45,
    0x31, 0xf6, 0x48, 0x89, 0xc6, 0x48, 0x85, 0xc0, 0x74, 0x37, 0x48, 0x89, 0xd9, 0xe8, 0xb8, 0x5c, 0x16, 0x01,
    0x48, 0x89, 0xf1, 0x44, 0x89, 0xf5, 0x49, 0x89, 0xc4, 0xe8, 0x5a, 0x13, 0x16, 0x01, 0x84, 0xc0, 0x74, 0x0d,
    0x48, 0x89, 0xf2, 0x4c, 0x89, 0xe1, 0xe8, 0x6b, 0x11, 0x16, 0x01, 0x8b, 0x28, 0x48, 0x8b, 0x0d, 0xaa, 0x13,
    0x95, 0xff, 0x89, 0xea, 0xe8, 0xbb, 0x00, 0xf0, 0xff, 0xeb, 0x22, 0x48, 0x8b, 0x0d, 0xfa, 0x06, 0x95, 0xff,
    0x48, 0x8d, 0x54, 0x24, 0x20, 0xe8, 0x38, 0xc6, 0xde, 0xff, 0x48, 0x8b, 0x0d, 0x89, 0x13, 0x95, 0xff, 0x48,
    0x8d, 0x54, 0x24, 0x20, 0xe8, 0x07, 0x2f, 0xf0, 0xff, 0x48, 0x89, 0xc6, 0x48, 0x85, 0xc0, 0x0f, 0x84, 0x96,
    0x00, 0x00, 0x00, 0x41, 0xb8, 0x0f, 0x05, 0x4f, 0xc3, 0x48, 0x89, 0xfa, 0x48, 0x89, 0xd9, 0x45, 0x8d, 0x80,
    0x59, 0x49, 0xb6, 0x1d, 0xe8, 0x53, 0x6c, 0x16, 0x01, 0x48, 0x89, 0xc5, 0x48, 0x85, 0xc0, 0x74, 0x76, 0x48,
    0x89, 0xd9, 0x4c, 0x89, 0x6c, 0x24, 0x60, 0xe8, 0x2e, 0x5c, 0x16, 0x01, 0x48, 0x89, 0xe9, 0x45, 0x89, 0xf4,
    0x49, 0x89, 0xc5, 0xe8, 0xd0, 0x12, 0x16, 0x01, 0x84, 0xc0, 0x74, 0x0e, 0x48, 0x89, 0xea, 0x4c, 0x89, 0xe9,
    0xe8, 0xe1, 0x10, 0x16, 0x01, 0x44, 0x8b, 0x20, 0x41, 0x81, 0xfc, 0xef, 0xbe, 0xad, 0xde, 0x48, 0x8d, 0x54,
    0x24, 0x70, 0x48, 0x89, 0xf1, 0x45, 0x0f, 0x44, 0xe6, 0x44, 0x89, 0x64, 0x24, 0x70, 0xe8, 0x51, 0x14, 0xfc,
    0xff, 0x4c, 0x8b, 0x6c, 0x24, 0x60, 0x48, 0x85, 0xc0, 0x74, 0x22, 0x0f, 0xb6, 0x40, 0x46, 0x84, 0xc0, 0x74,
    0x1a, 0x3c, 0x01, 0x75, 0x08, 0x41, 0xb8, 0xca, 0x50, 0x73, 0xfd, 0xeb, 0x14, 0x3c, 0x02, 0x75, 0x1b, 0x41,
    0xb8, 0x03, 0x40, 0x63, 0xd3, 0xeb, 0x08, 0x03, 0xb1, 0x41, 0xb8, 0xd0, 0xff, 0x56, 0x2d, 0x48, 0x89, 0xfa,
    0x48, 0x89, 0xd9, 0xe8, 0x24, 0x66, 0x16, 0x01
};
// CStronghold::FindProject(stronghold, &projectId): projects are 0x48 bytes in [stronghold+0x1c0, +0x1c8).
static const unsigned char kFindProjectBytes[] = {
    0x4c, 0x8b, 0x81, 0xc0, 0x01, 0x00, 0x00, 0x48, 0x8b, 0x81, 0xc8, 0x01, 0x00, 0x00, 0x8b, 0x12, 0x49, 0x39,
    0xc0, 0x74, 0x0e, 0x41, 0x39, 0x10, 0x74, 0x0c, 0x49, 0x83, 0xc0, 0x48, 0x49, 0x39, 0xc0, 0x75, 0xf2, 0x31,
    0xc0, 0xc3, 0x4c, 0x89, 0xc0, 0xc3
};
// CStrongholdManager::FindByTerritory(manager, territoryId): count at +0x7c, weak handles at +0x18 (16 bytes).
static const unsigned char kFindByTerritoryBytes[] = {
    0x48, 0x89, 0xe0, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x89, 0xe5, 0x48, 0x81, 0xec,
    0x80, 0x00, 0x00, 0x00, 0x48, 0xc7, 0x45, 0xa0, 0xfe, 0xff, 0xff, 0xff, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89,
    0x70, 0x10, 0x48, 0x89, 0x78, 0x18, 0x41, 0x89, 0xd7, 0x48, 0x89, 0xcf, 0x31, 0xf6, 0x41, 0x89, 0xf4, 0x39,
    0x71, 0x7c, 0x0f, 0x8e, 0x3a, 0x02, 0x00, 0x00, 0x4c, 0x8d, 0x71, 0x68, 0x4c, 0x8d, 0x69, 0x20, 0x44, 0x3b,
    0x67, 0x7c, 0x0f, 0x8d, 0x0c, 0x02, 0x00, 0x00, 0x49, 0x63, 0xc4, 0x48, 0x01, 0xc0, 0x48, 0x8d, 0x5c, 0xc7,
    0x18, 0x48, 0x8d, 0x55, 0xa8, 0x48, 0x89, 0xd9
};
static const Signature kSignatures[] = {
    {0x141ea4bc0, kGetProjectStatusBytes, sizeof(kGetProjectStatusBytes)},
    {0x141e66120, kFindProjectBytes, sizeof(kFindProjectBytes)},
    {0x141da4cf0, kFindByTerritoryBytes, sizeof(kFindByTerritoryBytes)},
};

static const uintptr_t kStrongholdManager = 0x1417f5fd8;

// Graph script helpers, called the same way as GetProjectStatus calls them.
typedef void *(__fastcall *GetVariable_t)(void *node, void *context, uint32_t name);
typedef void *(__fastcall *GetMemory_t)(void *node);
typedef bool(__fastcall *IsVariableSet_t)(void *variable);
typedef uint32_t *(__fastcall *GetValue_t)(void *memory, void *variable);
typedef void(__fastcall *FireOutput_t)(void *node, void *context, uint32_t pin);
static const GetVariable_t GetVariable = (GetVariable_t)0x14300b8d0;
static const GetMemory_t GetMemory = (GetMemory_t)0x14300a8c0;
static const IsVariableSet_t IsVariableSet = (IsVariableSet_t)0x143005f70;
static const GetValue_t GetValue = (GetValue_t)0x143005d90;
static const FireOutput_t FireOutput = (FireOutput_t)0x14300b330;

// Jenkins lookup3 hashlittle(name, 0)
static const uint32_t kPinIn = 0xc94ba74a;         // "in"
static const uint32_t kPinBuilt = 0xd3634003;      // "built"
static const uint32_t kVarStronghold = 0x163d2026; // stronghold variable of GetProjectStatus
static const uint32_t kVarProjectId = 0xe1054e68;  // "project_id"
static const uint32_t kCleanupCrew = 0x29e5a2c0;   // "cleanup_crew"
static const uint8_t kStatusBuilt = 2;             // project status byte at +0x46 (1 = completed, 2 = built)

typedef uint32_t(__fastcall *GetProjectStatus_t)(void *node, void *context, uint32_t pin);
static GetProjectStatus_t g_originalGetProjectStatus;

static bool Matches(const Signature &s)
{
    return memcmp((void *)s.address, s.bytes, s.size) == 0;
}

static bool CleanupCrewBuiltAnywhere()
{
    uintptr_t manager = *(uintptr_t *)kStrongholdManager;
    if (!manager) return false;
    int count = *(int *)(manager + 0x7c);
    for (int i = 0; i < count; i++)
    {
        // Weak handle {object, control block}; the object is alive while the use count at +8 is not 0.
        uintptr_t stronghold = *(uintptr_t *)(manager + 0x18 + i * 16);
        uintptr_t control = *(uintptr_t *)(manager + 0x18 + i * 16 + 8);
        if (!stronghold || !control || *(volatile LONG *)(control + 8) == 0) continue;
        uintptr_t end = *(uintptr_t *)(stronghold + 0x1c8);
        for (uintptr_t project = *(uintptr_t *)(stronghold + 0x1c0); project < end; project += 0x48)
            if (*(uint32_t *)project == kCleanupCrew && *(uint8_t *)(project + 0x46) == kStatusBuilt) return true;
    }
    return false;
}

static volatile LONG g_loggedCalls;

static uint32_t __fastcall HookGetProjectStatus(void *node, void *context, uint32_t pin)
{
    if (pin == kPinIn)
    {
        __try
        {
            void *projectVariable = GetVariable(node, context, kVarProjectId);
            if (projectVariable && !GetVariable(node, context, kVarStronghold))
            {
                void *memory = GetMemory(node);
                uint32_t project = IsVariableSet(projectVariable) ? *GetValue(memory, projectVariable) : 0;
                if (project == kCleanupCrew && CleanupCrewBuiltAnywhere())
                {
                    if (InterlockedIncrement(&g_loggedCalls) <= 20) Log("Cleanup Crew status: built (any stronghold)");
                    FireOutput(node, context, kPinBuilt);
                    return 1;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Log("Hook failed with exception %08lx", GetExceptionCode());
        }
    }
    return g_originalGetProjectStatus(node, context, pin);
}

static DWORD WINAPI InitThread(LPVOID)
{
    // The protector decrypts code after start-up. Wait until the expected code appears.
    for (int i = 0; i < 6000; i++)
    {
        __try
        {
            bool all = true;
            for (const Signature &s : kSignatures) all = all && Matches(s);
            if (all)
            {
                MH_Initialize();
                void *target = (void *)kSignatures[0].address;
                MH_STATUS s = MH_CreateHook(target, (void *)HookGetProjectStatus, (void **)&g_originalGetProjectStatus);
                if (s == MH_OK) s = MH_EnableHook(target);
                Log("GetProjectStatus hook: %s", MH_StatusToString(s));
                return 0;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        Sleep(100);
    }
    Log("Game code did not match. This game version is not supported; CleanupCrewAnywhere is inactive.");
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
        strcpy_s(name, MAX_PATH - (name - path), "cleanupcrewanywhere.log");
        g_log = _fsopen(path, "w", _SH_DENYNO);
        Log("CleanupCrewAnywhere loaded.");
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
