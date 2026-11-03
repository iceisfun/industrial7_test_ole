/* Shared helpers for area gap_07 (thin-coverage public exports of rpcrt4 / sechost).
 * Every runtime call goes through U()/UIN() (LoadLibrary + GetProcAddress); kernel32 is used only as scaffolding. */
#ifndef GAP7_H
#define GAP7_H
#include "t.h"
#include <rpc.h>
#include <rpcndr.h>
#include <setjmp.h>
#include <ntsecapi.h>
#include <wchar.h>

#define NELEM(a) ((unsigned)(sizeof(a) / sizeof((a)[0])))
#define SH "sechost.dll"
#define RT "rpcrt4.dll"

/* single-evaluation equality (EXPECT_EQ evaluates its arguments twice, wrong for calls into the runtime) */
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); \
    t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)

/* ---- run a function on a fresh thread, capturing a raised exception (RpcRaiseException) ---------------------
 * Returns 0 when fn returned normally, the exception code when one was raised, G7_TIMEOUT when it did not finish.
 * A fresh thread guarantees clean TLS (the stub-memory allocation context lives in TLS). */
#define G7_TIMEOUT 0xFFFFFFF0u
typedef void (*g7_fn)(void *);
static jmp_buf g7_jb;
static volatile DWORD g7_tid, g7_code;
static LONG CALLBACK g7_veh(PEXCEPTION_POINTERS ep)
{
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (GetCurrentThreadId() != g7_tid) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x406D1388 || c == 0x4001000A || c == 0x40010005 || c == 0x80000003 || c == 0x4001000B || c == 0xE06D7363)
        return EXCEPTION_CONTINUE_SEARCH;
    g7_code = c; g7_tid = 0;
    longjmp(g7_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
typedef struct { g7_fn fn; void *arg; DWORD code; } g7_job;
static DWORD WINAPI g7_thread(void *p)
{
    g7_job *j = p;
    g7_code = 0;
    g7_tid = GetCurrentThreadId();
    if (!setjmp(g7_jb)) { j->fn(j->arg); g7_tid = 0; }
    j->code = g7_code;
    return 0;
}
static DWORD g7_run(g7_fn fn, void *arg)
{
    static int veh;
    g7_job j; HANDLE th;
    if (!veh) { veh = 1; AddVectoredExceptionHandler(1, g7_veh); }
    j.fn = fn; j.arg = arg; j.code = 0;
    th = CreateThread(NULL, 0, g7_thread, &j, 0, NULL);
    if (!th) return G7_TIMEOUT;
    if (WaitForSingleObject(th, 15000) != WAIT_OBJECT_0) { TerminateThread(th, 1); CloseHandle(th); return G7_TIMEOUT; }
    CloseHandle(th);
    return j.code;
}
/* same, on the calling thread (for tests whose state must live on the main thread) */
static DWORD g7_try_here(g7_fn fn, void *arg)
{
    static int veh;
    if (!veh) { veh = 1; AddVectoredExceptionHandler(1, g7_veh); }
    g7_code = 0; g7_tid = GetCurrentThreadId();
    if (!setjmp(g7_jb)) { fn(arg); g7_tid = 0; }
    return g7_code;
}

static void g7_narrow(const WCHAR *w, char *a, size_t cap)
{
    size_t i = 0;
    for (; w[i] && i + 1 < cap; i++) a[i] = (char)w[i];
    a[i] = 0;
}
static void g7_widen(const char *a, WCHAR *w, size_t cap)
{
    size_t i = 0;
    for (; a[i] && i + 1 < cap; i++) w[i] = (unsigned char)a[i];
    w[i] = 0;
}
#endif
