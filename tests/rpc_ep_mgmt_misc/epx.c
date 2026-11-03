#include "epx.h"
#include <setjmp.h>

static jmp_buf g_jb;
static volatile DWORD g_tid, g_code;
static PVOID g_veh;
DWORD epx_last_flags, epx_last_nparams;
unsigned char epx_fake[256];

static LONG CALLBACK epx_veh(PEXCEPTION_POINTERS ep) {
    DWORD c;
    if (g_tid != GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
    c = ep->ExceptionRecord->ExceptionCode;
    if (c == 0x406D1388 || c == 0x40010006 || c == 0x4001000A || c == DBG_PRINTEXCEPTION_C) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c;
    epx_last_flags = ep->ExceptionRecord->ExceptionFlags;
    epx_last_nparams = ep->ExceptionRecord->NumberParameters;
    g_tid = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

DWORD epx_try(epx_fn fn, void *arg) {
    if (!g_veh) g_veh = AddVectoredExceptionHandler(1, epx_veh);
    g_code = 0;
    if (setjmp(g_jb) == 0) {
        g_tid = GetCurrentThreadId();
        fn(arg);
        g_tid = 0;
        return 0;
    }
    g_tid = 0;
    return g_code;
}

struct run_ctx { epx_fn fn; void *arg; };
static DWORD WINAPI run_thunk(void *p) { struct run_ctx *c = p; c->fn(c->arg); return 0; }
BOOL epx_run(epx_fn fn, void *arg, DWORD ms) {
    struct run_ctx c; HANDLE h; BOOL ok;
    c.fn = fn; c.arg = arg;
    h = CreateThread(NULL, 0, run_thunk, &c, 0, NULL);
    if (!h) return FALSE;
    ok = WaitForSingleObject(h, ms) == WAIT_OBJECT_0;
    if (!ok) { TerminateThread(h, 1); WaitForSingleObject(h, 2000); }
    CloseHandle(h);
    return ok;
}

static volatile LONG g_epn;
void epx_unique_ep(WCHAR *buf, int cch) {
    LONG n = InterlockedIncrement(&g_epn);
    wsprintfW(buf, L"olet_epm_%lu_%ld", GetCurrentProcessId(), n);
    (void)cch;
}

RPC_BINDING_HANDLE epx_mkbind(void) {
    WCHAR ep[64]; RPC_WSTR sb = NULL; RPC_BINDING_HANDLE h = NULL;
    epx_unique_ep(ep, 64);
    if (U(RpcStringBindingComposeW)(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)ep, NULL, &sb) != RPC_S_OK) return NULL;
    if (U(RpcBindingFromStringBindingW)(sb, &h) != RPC_S_OK) h = NULL;
    U(RpcStringFreeW)(&sb);
    return h;
}
