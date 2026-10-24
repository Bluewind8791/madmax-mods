// FovChanger: multiplies the game's field of view. Hotkeys turn it on/off and change the multiplier.
// Loaded as an .asi plugin by Ultimate ASI Loader. Delete fovchanger.asi from the game folder to uninstall.
//
// Port of the Cheat Engine table "Mad Max Field of View Changer" (MadMaxFov.CT, Carl Alldis, 2019).
// It patches the same three places (addresses are for the current Steam build of MadMax.exe):
// - 0x1422444e7 mov [rbx+290h],r11d: the camera copies its default FOV. A stub multiplies it first.
// - 0x14222cee4 movss [rbx+40h],xmm0: writes the camera FOV (degrees * pi/180) and overrides the multiplied
//   FOV. A stub only lets values below 0.6 rad through, so sniper zoom still works.
// - 0x14224ad42 movss [r12+40h],xmm0: another FOV write. Replaced with a NOP.
// Each patch fits in one aligned 16-byte block and is written with one cmpxchg16b, so the game thread never
// sees a half-written instruction.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <share.h>

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

// --- settings --------------------------------------------------------------

static const float kMinMultiplier = 0.5f;
static const float kMaxMultiplier = 2.0f;

static float g_step = 0.05f;
static int g_toggleKey = VK_F1;
static int g_increaseKey = VK_OEM_PLUS;
static int g_decreaseKey = VK_OEM_MINUS;
static float g_startMultiplier = 1.2f;

static float ClampMultiplier(float m)
{
    return m < kMinMultiplier ? kMinMultiplier : m > kMaxMultiplier ? kMaxMultiplier : m;
}

static float ReadFloat(const char *key, float fallback, const char *iniPath)
{
    char text[32];
    GetPrivateProfileStringA("FovChanger", key, "", text, sizeof(text), iniPath);
    return text[0] ? (float)atof(text) : fallback;
}

static void LoadSettings(const char *iniPath)
{
    g_startMultiplier = ClampMultiplier(ReadFloat("Multiplier", g_startMultiplier, iniPath));
    g_step = ReadFloat("Step", g_step, iniPath);
    g_toggleKey = GetPrivateProfileIntA("FovChanger", "ToggleKey", g_toggleKey, iniPath);
    g_increaseKey = GetPrivateProfileIntA("FovChanger", "IncreaseKey", g_increaseKey, iniPath);
    g_decreaseKey = GetPrivateProfileIntA("FovChanger", "DecreaseKey", g_decreaseKey, iniPath);
    Log("Multiplier %.2f, step %.2f, keys: toggle %d, increase %d, decrease %d", g_startMultiplier, g_step,
        g_toggleKey, g_increaseKey, g_decreaseKey);
}

// --- patches ---------------------------------------------------------------

struct Patch
{
    const char *name;
    uintptr_t address;
    size_t length;               // bytes replaced by the patch
    unsigned char original[12];  // signature: the replaced instruction and the next bytes
    unsigned char patched[8];
};

static Patch g_fovCopy = {"FOV copy", 0x1422444e7, 7,
                          {0x44, 0x89, 0x9b, 0x90, 0x02, 0x00, 0x00, 0x89, 0x83, 0xa4, 0x02, 0x00}};
static Patch g_fovWrite = {"FOV write", 0x14222cee4, 5,
                           {0xf3, 0x0f, 0x11, 0x43, 0x40, 0xe9, 0xd9, 0x00, 0x00, 0x00, 0x48, 0x89}};
static Patch g_fovWrite2 = {"FOV write 2", 0x14224ad42, 7,
                            {0xf3, 0x41, 0x0f, 0x11, 0x44, 0x24, 0x40, 0x39, 0x7e, 0x34, 0x0f, 0x84},
                            {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00}}; // one 7-byte NOP
static Patch *const g_patches[] = {&g_fovCopy, &g_fovWrite, &g_fovWrite2};

// Stub memory: data first, then code. Must be within +-2 GB of the patches for rel32 jumps.
struct StubData
{
    volatile float multiplier;
    float sniperLimit; // FOV writes below this (radians) are zoom and pass through
};
static StubData *g_data;

static bool Matches(const Patch &p)
{
    return memcmp((void *)p.address, p.original, sizeof(p.original)) == 0;
}

static void *AllocNear(uintptr_t target, size_t size)
{
    for (uintptr_t a = 0x140000000 - 0x10000; a > target - 0x7ff00000; a -= 0x10000)
        if (void *p = VirtualAlloc((void *)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return p;
    return nullptr;
}

static void PutRel32(unsigned char *at, uintptr_t nextInstruction, uintptr_t target)
{
    int32_t rel = (int32_t)(target - nextInstruction);
    memcpy(at, &rel, 4);
}

static bool BuildStubs()
{
    unsigned char *mem = (unsigned char *)AllocNear(g_fovCopy.address, 0x1000);
    if (!mem) return false;
    g_data = (StubData *)mem;
    g_data->multiplier = g_startMultiplier;
    g_data->sniperLimit = 0.6f;

    // FOV copy: [rbx+290h] = r11d * multiplier. Keeps r11, xmm0 and flags unchanged.
    static const unsigned char copyStub[] = {
        0x48, 0x8d, 0x64, 0x24, 0xf0,                   // lea rsp,[rsp-10h]
        0xf3, 0x0f, 0x7f, 0x04, 0x24,                   // movdqu [rsp],xmm0
        0x66, 0x41, 0x0f, 0x6e, 0xc3,                   // movd xmm0,r11d
        0xf3, 0x0f, 0x59, 0x05, 0, 0, 0, 0,             // mulss xmm0,[multiplier]
        0xf3, 0x0f, 0x11, 0x83, 0x90, 0x02, 0x00, 0x00, // movss [rbx+290h],xmm0
        0xf3, 0x0f, 0x6f, 0x04, 0x24,                   // movdqu xmm0,[rsp]
        0x48, 0x8d, 0x64, 0x24, 0x10,                   // lea rsp,[rsp+10h]
        0xe9, 0, 0, 0, 0};                              // jmp back
    unsigned char *copy = mem + 0x10;
    memcpy(copy, copyStub, sizeof(copyStub));
    PutRel32(copy + 19, (uintptr_t)copy + 23, (uintptr_t)&g_data->multiplier);
    PutRel32(copy + 42, (uintptr_t)copy + 46, g_fovCopy.address + g_fovCopy.length);

    // FOV write: only write zoom values. The original code jumps to the function epilogue next, so flags are dead.
    static const unsigned char writeStub[] = {
        0x0f, 0x2f, 0x05, 0, 0, 0, 0, // comiss xmm0,[sniperLimit]
        0x73, 0x05,                   // jae skip
        0xf3, 0x0f, 0x11, 0x43, 0x40, // movss [rbx+40h],xmm0
        0xe9, 0, 0, 0, 0};            // skip: jmp back
    unsigned char *write = mem + 0x50;
    memcpy(write, writeStub, sizeof(writeStub));
    PutRel32(write + 3, (uintptr_t)write + 7, (uintptr_t)&g_data->sniperLimit);
    PutRel32(write + 15, (uintptr_t)write + 19, g_fovWrite.address + g_fovWrite.length);
    FlushInstructionCache(GetCurrentProcess(), mem, 0x1000);

    // jmp stub; the FOV copy patch pads with one 2-byte NOP (never executed, the jmp does not return there).
    g_fovCopy.patched[0] = 0xe9;
    PutRel32(g_fovCopy.patched + 1, g_fovCopy.address + 5, (uintptr_t)copy);
    g_fovCopy.patched[5] = 0x66;
    g_fovCopy.patched[6] = 0x90;
    g_fovWrite.patched[0] = 0xe9;
    PutRel32(g_fovWrite.patched + 1, g_fovWrite.address + 5, (uintptr_t)write);
    return true;
}

// Writes bytes inside one aligned 16-byte block atomically.
static bool WriteCode(uintptr_t address, const unsigned char *bytes, size_t length)
{
    uintptr_t block = address & ~(uintptr_t)15;
    if (address + length > block + 16) return false;
    DWORD oldProtect;
    if (!VirtualProtect((void *)block, 16, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    __declspec(align(16)) LONG64 expected[2];
    LONG64 desired[2];
    do
    {
        memcpy(expected, (void *)block, 16);
        memcpy(desired, expected, 16);
        memcpy((char *)desired + (address - block), bytes, length);
    } while (!InterlockedCompareExchange128((LONG64 *)block, desired[1], desired[0], expected));
    VirtualProtect((void *)block, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void *)block, 16);
    return true;
}

static bool SetEnabled(bool enabled)
{
    bool ok = true;
    for (Patch *p : g_patches)
        ok &= WriteCode(p->address, enabled ? p->patched : p->original, p->length);
    return ok;
}

// --- hotkeys ---------------------------------------------------------------

static bool GameHasFocus()
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static bool Pressed(int key, bool &wasDown)
{
    bool down = key && GameHasFocus() && (GetAsyncKeyState(key) & 0x8000);
    bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

static void HotkeyLoop()
{
    bool enabled = true, toggleDown = false, increaseDown = false, decreaseDown = false;
    for (;;)
    {
        Sleep(30);
        if (Pressed(g_toggleKey, toggleDown))
        {
            enabled = !enabled;
            bool ok = SetEnabled(enabled);
            Log("FOV %s%s", enabled ? "on" : "off", ok ? "" : " (patch write failed)");
        }
        int change = Pressed(g_increaseKey, increaseDown) - Pressed(g_decreaseKey, decreaseDown);
        if (change)
        {
            g_data->multiplier = ClampMultiplier(g_data->multiplier + change * g_step);
            Log("Multiplier %.2f", g_data->multiplier);
        }
    }
}

static char g_iniPath[MAX_PATH];

static DWORD WINAPI InitThread(LPVOID)
{
    LoadSettings(g_iniPath);
    // The protector decrypts code after start-up. Wait until the expected code appears.
    for (int i = 0; i < 6000; i++)
    {
        bool ready = false;
        __try
        {
            ready = Matches(g_fovCopy) && Matches(g_fovWrite) && Matches(g_fovWrite2);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        if (ready)
        {
            if (!BuildStubs())
            {
                Log("Could not allocate stub memory near the game code; FovChanger is inactive.");
                return 0;
            }
            Log("Patches: %s", SetEnabled(true) ? "OK" : "write failed");
            HotkeyLoop();
        }
        Sleep(100);
    }
    Log("Game code did not match. This game version is not supported; FovChanger is inactive.");
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
        size_t room = MAX_PATH - (name - path);
        strcpy_s(name, room, "fovchanger.ini");
        strcpy_s(g_iniPath, path);
        strcpy_s(name, room, "fovchanger.log");
        g_log = _fsopen(path, "w", _SH_DENYNO);
        Log("FovChanger loaded.");
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
