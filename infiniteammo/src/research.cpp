// InfiniteAmmo research build: logs every write to up to 4 watched 2-byte values (inventory ammo counts).
// Install as <game folder>\infiniteammo_research.asi. Write up to 4 hex addresses, one per line, into
// <game folder>\infiniteammo_watch.txt (tools/inv.py prints the entry addresses; the count is at entry + 4).
// F8 writes a marker line to the log.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <share.h>
#include <tlhelp32.h>

// --- logging ---------------------------------------------------------------

static FILE *g_log;
static CRITICAL_SECTION g_logLock;

static void Log(const char *fmt, ...)
{
    if (!g_log) return;
    EnterCriticalSection(&g_logLock);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_logLock);
}

static bool SafeRead(const void *src, void *dst, size_t size)
{
    __try
    {
        memcpy(dst, src, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static const uintptr_t kCodeLo = 0x141b38000, kCodeHi = 0x145d54000;

// Formats up to `max` exe code addresses found on the stack (a rough call chain).
static void StackScan(uintptr_t rsp, char *out, size_t outSize, int max)
{
    uintptr_t slots[256];
    int found = 0;
    size_t n = 0;
    out[0] = 0;
    if (!SafeRead((void *)rsp, slots, sizeof(slots))) return;
    for (int i = 0; i < 256 && found < max; i++)
        if (slots[i] >= kCodeLo && slots[i] < kCodeHi)
        {
            n += sprintf_s(out + n, outSize - n, " %llx", (unsigned long long)slots[i]);
            found++;
        }
}

// --- hardware write watch ----------------------------------------------------------------

static const int kSlots = 4;
static char g_watchPath[MAX_PATH];
static volatile uintptr_t g_watch[kSlots];
static volatile LONG g_watchHits;
static volatile LONG g_markerCount;

static LONG CALLBACK OnException(EXCEPTION_POINTERS *info)
{
    DWORD64 dr6 = info->ContextRecord->Dr6;
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(dr6 & 0xF)) return EXCEPTION_CONTINUE_SEARCH;
    info->ContextRecord->Dr6 = 0;
    for (int i = 0; i < kSlots; i++)
    {
        if (!(dr6 & (1ull << i)) || InterlockedIncrement(&g_watchHits) > 500) continue;
        uint16_t value = 0;
        SafeRead((void *)g_watch[i], &value, 2);
        char stack[512];
        StackScan(info->ContextRecord->Rsp, stack, sizeof(stack), 16);
        const CONTEXT *c = info->ContextRecord;
        Log("WRITE slot %d %p value=%u rip=%llx tid=%lu rax=%llx rcx=%llx rdx=%llx r8=%llx r9=%llx stack:%s", i, (void *)g_watch[i], value, c->Rip,
            GetCurrentThreadId(), c->Rax, c->Rcx, c->Rdx, c->R8, c->R9, stack);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void ApplyWatchToAllThreads()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = {sizeof(te)};
    int count = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
    {
        if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId()) continue;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!thread) continue;
        SuspendThread(thread);
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(thread, &ctx))
        {
            DWORD64 *dr[kSlots] = {&ctx.Dr0, &ctx.Dr1, &ctx.Dr2, &ctx.Dr3};
            for (int i = 0; i < kSlots; i++)
            {
                *dr[i] = g_watch[i];
                // Ln enable, RWn = 01 (write), LENn = 01 (2 bytes)
                ctx.Dr7 &= ~((3ull << (i * 2)) | (0xFull << (16 + i * 4)));
                if (g_watch[i]) ctx.Dr7 |= (1ull << (i * 2)) | (1ull << (16 + i * 4)) | (1ull << (18 + i * 4));
            }
            if (SetThreadContext(thread, &ctx)) count++;
        }
        ResumeThread(thread);
        CloseHandle(thread);
    }
    CloseHandle(snap);
    Log("Watch %p %p %p %p applied to %d threads", (void *)g_watch[0], (void *)g_watch[1], (void *)g_watch[2], (void *)g_watch[3], count);
}

// Re-reads the watch file every second. Re-applies to all threads every 10 s to cover new threads.
static void PollWatchFile()
{
    static DWORD lastApply;
    uintptr_t next[kSlots] = {};
    FILE *f = _fsopen(g_watchPath, "r", _SH_DENYNO);
    if (f)
    {
        char line[64];
        for (int i = 0; i < kSlots && fgets(line, sizeof(line), f); i++) next[i] = (uintptr_t)strtoull(line, nullptr, 16) & ~(uintptr_t)1;
        fclose(f);
    }
    bool changed = false, any = false;
    for (int i = 0; i < kSlots; i++)
    {
        changed = changed || next[i] != g_watch[i];
        any = any || next[i];
    }
    if (changed || (any && GetTickCount() - lastApply > 10000))
    {
        if (changed) g_watchHits = 0;
        for (int i = 0; i < kSlots; i++) g_watch[i] = next[i];
        lastApply = GetTickCount();
        ApplyWatchToAllThreads();
    }
}

static DWORD WINAPI MainThread(LPVOID)
{
    Log("InfiniteAmmo research build loaded. Watch file: %s", g_watchPath);
    AddVectoredExceptionHandler(1, OnException);
    bool wasDown = false;
    DWORD lastPoll = 0;
    for (;;)
    {
        if (GetTickCount() - lastPoll > 1000)
        {
            lastPoll = GetTickCount();
            PollWatchFile();
        }
        bool down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (down && !wasDown) Log("===== MARKER %ld (F8) =====", InterlockedIncrement(&g_markerCount));
        wasDown = down;
        Sleep(20);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        InitializeCriticalSection(&g_logLock);
        char path[MAX_PATH];
        GetModuleFileNameA(module, path, MAX_PATH);
        char *name = strrchr(path, '\\') + 1;
        strcpy_s(g_watchPath, path);
        strcpy_s(g_watchPath + (name - path), MAX_PATH - (name - path), "infiniteammo_watch.txt");
        strcpy_s(name, MAX_PATH - (name - path), "infiniteammo_research.log");
        g_log = _fsopen(path, "w", _SH_DENYNO);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
