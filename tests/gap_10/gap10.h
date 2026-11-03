/* Shared helpers for the gap_10 area (non-public / internal exports). Calls go through U()/UIN()/UN() only. */
#ifndef GAP10_H
#define GAP10_H
#include "t.h"
#include <rpc.h>
#include <rpcndr.h>
#include <objbase.h>
#include <setjmp.h>

typedef struct { const char *name; const char *fwd; unsigned ord; } pe;

/* Wine does not export most of these, and aborts on some it stubs: call tests skip there (no-op on Windows). */
#define NOT_WINE(why) do { if (t_is_wine()) { t_skip("wine: " why); return; } } while (0)
/* presence: a hard failure on Windows, only a note on an emulator */
#define PRESENT(cond, ...) do { if (t_is_wine()) { if (!(cond)) t_note(__VA_ARGS__); } else CHECKF(cond, __VA_ARGS__); } while (0)

/* ---- exception capture (VEH + longjmp; no SEH keywords in mingw) ---- */
static jmp_buf g_jb;
static volatile LONG g_armed;
static volatile DWORD g_code;
static LONG CALLBACK g_veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_armed) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x406D1388 || c == 0x4001000A || c == 0x40010005 || c == 0x80000003 || c == 0x4001000B) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c; g_armed = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void g_init(void) { static int done; if (!done) { done = 1; AddVectoredExceptionHandler(1, g_veh); } }
#define GTRY  do { g_init(); g_code = 0; if (!setjmp(g_jb)) { g_armed = 1;
#define GEND  g_armed = 0; } } while (0)

/* typed function pointer from a typedef-less signature: FN(int, Name, (void *)) declares Name_ and skips the test if missing */
#define FN(ret, name, args) ret (WINAPI *name##_) args = (void *)t_proc(#name); REQUIRE(name##_)
#define FNIN(dll, ret, name, args) ret (WINAPI *name##_) args = (void *)t_proc_in(dll, #name); REQUIRE(name##_)

static const GUID gNULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
#define HR_E_INVALIDARG ((HRESULT)0x80070057)
#endif
