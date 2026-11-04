/* Shared fixtures for the ncalrpc tests: see rpcx.h. */
#include "rpcx.h"
#include "rpcx_fmt.h"

/* ------------------------------------------------------------------ unique names */
static volatile LONG g_epn;
/* Endpoints cannot be released once a protocol sequence is registered, so names are recycled from a pool of
 * RX_EP_POOL (a runtime may limit the number of live endpoints); rx_use_ep tolerates a repeated registration
 * of a name this process itself registered earlier. */
#define RX_EP_POOL 40
void rx_ep(WCHAR *buf, size_t cch) {
    LONG n = InterlockedIncrement(&g_epn);
    wsprintfW(buf, L"olet_%lu_%ld", GetCurrentProcessId(), (n - 1) % RX_EP_POOL + 1);
    (void)cch;
}
static volatile LONG g_epu;
void rx_ep_unique(WCHAR *buf, size_t cch) {      /* never recycled: for tests that call UseProtseq* themselves, and for child servers */
    LONG n = InterlockedIncrement(&g_epu);
    wsprintfW(buf, L"olet_%lu_u%ld", GetCurrentProcessId(), n);
    (void)cch;
}
static WCHAR g_reg[RX_EP_POOL * 2][64]; static int g_nreg;
void rx_epa(char *buf, size_t cb) { WCHAR w[64]; rx_ep_unique(w, 64); WideCharToMultiByte(CP_ACP, 0, w, -1, buf, (int)cb, NULL, NULL); }

/* ------------------------------------------------------------------ server routines for xif */
volatile LONG rx_calls, rx_rundowns, rx_ctx_closes;
typedef struct { int magic; int v; } rx_ctx;
#define RX_CTX_MAGIC 0x43545831

static void *__RPC_API rx_alloc(size_t n) { return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
volatile LONG g4_frees;
static void __RPC_API rx_free(void *p) { InterlockedIncrement(&g4_frees); HeapFree(GetProcessHeap(), 0, p); }
static void __RPC_API rx_ctx_rundown(void *c) { InterlockedIncrement(&rx_rundowns); rx_free(c); }

static void s_void(void) { InterlockedIncrement(&rx_calls); }
static int s_add(int a, int b) { InterlockedIncrement(&rx_calls); return a + b; }
static int s_wlen(const WCHAR *s) { InterlockedIncrement(&rx_calls); return lstrlenW(s); }
static int s_sum(int n, int *a) { int r = 0, i; InterlockedIncrement(&rx_calls); for (i = 0; i < n; i++) r += a[i]; return r; }
static void s_seq(int n, int *a) { int i; InterlockedIncrement(&rx_calls); for (i = 0; i < n; i++) a[i] = i * i; }
static void s_bump(rx_S *s) { InterlockedIncrement(&rx_calls); s->a += 1; s->b += 2; s->c *= 2.0; }
static void s_raise(int code) { InterlockedIncrement(&rx_calls); UIN("rpcrt4.dll", RpcRaiseException)((RPC_STATUS)code); }
volatile LONG rx_sleep_started, rx_sleep_done;
static void s_sleep(int ms) { InterlockedIncrement(&rx_calls); InterlockedIncrement(&rx_sleep_started); Sleep(ms); InterlockedIncrement(&rx_sleep_done); }
static void s_ctx_open(void **h, int v) {
    rx_ctx *c = rx_alloc(sizeof *c); InterlockedIncrement(&rx_calls);
    c->magic = RX_CTX_MAGIC; c->v = v; *h = c;
}
volatile LONG g4_ctxmode; volatile LONG g4_ctxres[8];
UDECL(RPC_STATUS, RpcSsContextLockExclusive, (RPC_BINDING_HANDLE, PVOID));
UDECL(RPC_STATUS, RpcSsContextLockShared, (RPC_BINDING_HANDLE, PVOID));
static int s_ctx_get(void *h) {
    rx_ctx *c = h; InterlockedIncrement(&rx_calls);
    if (g4_ctxmode) {
        LONG mode = g4_ctxmode; RPC_BINDING_HANDLE b = NULL; int bogus = 0; int i;
        for (i = 0; i < 8; i++) g4_ctxres[i] = 0x7777;
        g4_ctxmode = 0;
        if (mode == 2) { g4_ctxres[7] = U(RpcServerInqBindingHandle)(&b); }
        if (mode == 1 || mode == 2) {          /* serialized (default) context: downgrade, upgrade, downgrade */
            g4_ctxres[0] = UN(RpcSsContextLockShared)(b, h);
            g4_ctxres[1] = UN(RpcSsContextLockExclusive)(b, h);
            g4_ctxres[2] = UN(RpcSsContextLockShared)(b, h);
            g4_ctxres[3] = UN(RpcSsContextLockShared)(b, &bogus);      /* pointer that is not a context handle of this call */
            g4_ctxres[4] = UN(RpcSsContextLockExclusive)(b, &bogus);
        } else if (mode == 5) {                /* RpcUserFree(NULL, p): frees through the stub's free routine */
            void *p = rx_alloc(32); LONG b = g4_frees;
            U(RpcUserFree)(NULL, p);
            g4_ctxres[0] = g4_frees - b;
        } else if (mode == 3) {                /* after RpcSsDontSerializeContext: upgrade first */
            g4_ctxres[0] = UN(RpcSsContextLockExclusive)(b, h);
            g4_ctxres[1] = UN(RpcSsContextLockShared)(b, h);
            g4_ctxres[2] = UN(RpcSsContextLockExclusive)(b, h);
        }
    }
    return c->magic == RX_CTX_MAGIC ? c->v : -1;
}
static void s_ctx_close(void **h) { InterlockedIncrement(&rx_calls); InterlockedIncrement(&rx_ctx_closes); rx_free(*h); *h = NULL; }
static void s_info(int *pid, int *level, int *islocal, int *authn) {
    RPC_CALL_ATTRIBUTES_V2_W a; RPC_STATUS st;
    InterlockedIncrement(&rx_calls);
    memset(&a, 0, sizeof a); a.Version = 2; a.Flags = 0;
    st = t_is_wine() ? (RPC_STATUS)0xFFFF : U(RpcServerInqCallAttributesW)(NULL, &a);
    if (st) { *pid = -1; *level = (int)st; *islocal = -1; *authn = -1; return; }
    *pid = (int)(ULONG_PTR)a.ClientPID; *level = (int)a.AuthenticationLevel;
    *islocal = (int)a.IsClientLocal; *authn = (int)a.AuthenticationService;
}
static void s_echo(const WCHAR *s, int n, WCHAR *o) {
    int i = 0; InterlockedIncrement(&rx_calls);
    for (; i < n; i++) { o[i] = s[i]; if (!s[i]) break; }
    if (n > 0 && i == n) o[n - 1] = 0;
}
static void s_stop(void) { InterlockedIncrement(&rx_calls); U(RpcMgmtStopServerListening)(NULL); }
static int s_counter(void) { return rx_calls; }
static int s_ctx_peek(void *h, int *rd) { rx_ctx *c = h; InterlockedIncrement(&rx_calls); *rd = rx_rundowns; return c->magic == RX_CTX_MAGIC ? c->v : -1; }

static const SERVER_ROUTINE rx_routines[RX_NPROCS] = {
    (SERVER_ROUTINE)s_void, (SERVER_ROUTINE)s_add, (SERVER_ROUTINE)s_wlen, (SERVER_ROUTINE)s_sum,
    (SERVER_ROUTINE)s_seq, (SERVER_ROUTINE)s_bump, (SERVER_ROUTINE)s_raise, (SERVER_ROUTINE)s_sleep,
    (SERVER_ROUTINE)s_ctx_open, (SERVER_ROUTINE)s_ctx_get, (SERVER_ROUTINE)s_ctx_close, (SERVER_ROUTINE)s_info,
    (SERVER_ROUTINE)s_echo, (SERVER_ROUTINE)s_stop, (SERVER_ROUTINE)s_counter, (SERVER_ROUTINE)s_ctx_peek };
static const unsigned short rx_fmt_off[RX_NPROCS] = { 0, 26, 70, 108, 152, 190, 222, 254, 286, 324, 368, 406, 456, 500, 526, 558 };

/* ------------------------------------------------------------------ xif interface structures */
const RPC_SYNTAX_IDENTIFIER rx_ndr_syntax = { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} };
const UUID rx_xif_uuid = { 0x6f0e7a10, 0x0001, 0x4c3d, {0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa1} };
handle_t rx_ih;

static const RPC_CLIENT_INTERFACE rx_ci = {
    sizeof(RPC_CLIENT_INTERFACE),
    { {0x6f0e7a10, 0x0001, 0x4c3d, {0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa1}}, {1, 0} },
    { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} },
    0, 0, 0, 0, 0, 0 };
static const MIDL_STUB_DESC rx_cstub = {
    (void *)&rx_ci, rx_alloc, rx_free, { &rx_ih }, 0, 0, 0, 0, &rx_type_fmt[2], 1, 0x50002, 0, 0x50200ca, 0, 0, 0, 1, 0, 0, 0 };
RPC_IF_HANDLE rx_xif_c_ifspec = (RPC_IF_HANDLE)&rx_ci;

static const NDR_RUNDOWN rx_rundown_tab[] = { rx_ctx_rundown };
static RPC_DISPATCH_FUNCTION rx_dtab[RX_NPROCS + 1];
static RPC_DISPATCH_TABLE rx_dt = { RX_NPROCS, rx_dtab, 0 };
static const MIDL_SERVER_INFO rx_sinfo;
static const RPC_SERVER_INTERFACE rx_si = {
    sizeof(RPC_SERVER_INTERFACE),
    { {0x6f0e7a10, 0x0001, 0x4c3d, {0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa1}}, {1, 0} },
    { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} },
    &rx_dt, 0, 0, 0, (void *)&rx_sinfo, 0 };
static const MIDL_STUB_DESC rx_sstub = {
    (void *)&rx_si, rx_alloc, rx_free, { 0 }, rx_rundown_tab, 0, 0, 0, &rx_type_fmt[2], 1, 0x50002, 0, 0x50200ca, 0, 0, 0, 1, 0, 0, 0 };
static const MIDL_SERVER_INFO rx_sinfo = { &rx_sstub, rx_routines, &rx_proc_fmt[2], rx_fmt_off, 0, 0, 0, 0 };
RPC_IF_HANDLE rx_xif_s_ifspec = (RPC_IF_HANDLE)&rx_si;

static void rx_init_dtab(void) {
    static int done;
    if (!done) {
        int i;
        for (i = 0; i < RX_NPROCS; i++) rx_dtab[i] = (RPC_DISPATCH_FUNCTION)U(NdrServerCall2);
        done = 1;
    }
}

/* ------------------------------------------------------------------ NDR client calls on a helper thread */
static volatile DWORD g_uef_code;
static LONG WINAPI rx_uef(EXCEPTION_POINTERS *ep) {
    g_uef_code = ep->ExceptionRecord->ExceptionCode;
    ExitThread(0xE0C0DE01);
    return EXCEPTION_EXECUTE_HANDLER;
}
typedef struct { unsigned proc; ULONG_PTR a[4]; LONG_PTR ret; RPC_BINDING_HANDLE h; DWORD exc; } ndr_job;
static DWORD WINAPI ndr_thread(void *p) {
    ndr_job *j = p;
    CLIENT_CALL_RETURN r;
    if (j->h) rx_ih = j->h;
    r = U(NdrClientCall2)((PMIDL_STUB_DESC)&rx_cstub, (PFORMAT_STRING)&rx_proc_fmt[2 + rx_fmt_off[j->proc]],
                          j->a[0], j->a[1], j->a[2], j->a[3]);
    j->ret = (LONG_PTR)r.Simple;
    return 0;
}
static CRITICAL_SECTION g_ndr_cs; static int g_ndr_cs_init;
static DWORD ndr_once(RPC_BINDING_HANDLE h, unsigned proc, ULONG_PTR a0, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, LONG_PTR *ret) {
    ndr_job j; HANDLE th; DWORD w, ec = 0, res;
    if (!g_ndr_cs_init) { InitializeCriticalSection(&g_ndr_cs); g_ndr_cs_init = 1; SetUnhandledExceptionFilter(rx_uef); }
    memset(&j, 0, sizeof j); j.proc = proc; j.a[0] = a0; j.a[1] = a1; j.a[2] = a2; j.a[3] = a3; j.h = h;
    g_uef_code = 0;
    th = CreateThread(NULL, 0, ndr_thread, &j, 0, NULL);
    if (!th) return RX_TIMEOUT;
    w = WaitForSingleObject(th, 15000);
    if (w != WAIT_OBJECT_0) { TerminateThread(th, 1); CloseHandle(th); return RX_TIMEOUT; }
    GetExitCodeThread(th, &ec); CloseHandle(th);
    if (ec == 0xE0C0DE01) res = g_uef_code ? g_uef_code : 0xFFFFFFF1u; else res = 0;
    if (ret) *ret = j.ret;
    return res;
}
/* An emulator that does not transparently re-establish a cached connection to a restarted server reports
 * RPC_S_CALL_FAILED_DNE for the first call on a recycled endpoint name; retry there (no-op on Windows). */
DWORD rx_ndr_h(RPC_BINDING_HANDLE h, unsigned proc, ULONG_PTR a0, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, LONG_PTR *ret) {
    DWORD e = ndr_once(h, proc, a0, a1, a2, a3, ret);
    for (int k = 0; k < 3 && t_is_wine() && e == RPC_S_CALL_FAILED_DNE; k++) e = ndr_once(h, proc, a0, a1, a2, a3, ret);
    return e;
}
DWORD rx_ndr(unsigned proc, ULONG_PTR a0, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, LONG_PTR *ret) {
    return rx_ndr_h(NULL, proc, a0, a1, a2, a3, ret);
}

/* ------------------------------------------------------------------ raw interfaces */
volatile LONG rx_raw_calls, rx_cb_calls, rx_cb_result;
volatile void *rx_cb_last_if, *rx_cb_last_ctx;
volatile LONG rx_cb_attr_st, rx_cb_attr_pid, rx_cb_attr_level, rx_cb_attr_local;

RPC_STATUS RPC_ENTRY rx_sec_cb(RPC_IF_HANDLE ifh, void *ctx) {
    InterlockedIncrement(&rx_cb_calls);
    rx_cb_last_if = ifh; rx_cb_last_ctx = ctx;
    rx_cb_attr_st = 0xFFFF;
    if (!t_is_wine() && ctx) {
        RPC_CALL_ATTRIBUTES_V2_W a; memset(&a, 0, sizeof a); a.Version = 2;
        rx_cb_attr_st = U(RpcServerInqCallAttributesW)(ctx, &a);
        rx_cb_attr_pid = (LONG)(ULONG_PTR)a.ClientPID; rx_cb_attr_level = (LONG)a.AuthenticationLevel; rx_cb_attr_local = (LONG)a.IsClientLocal;
    }
    return (RPC_STATUS)rx_cb_result;
}

static void raw_reply(RPC_MESSAGE *m, const void *p, ULONG n) {
    m->BufferLength = n;
    if (U(I_RpcGetBuffer)(m) != RPC_S_OK) { U(RpcRaiseException)(RPC_S_OUT_OF_MEMORY); }
    if (n) memcpy(m->Buffer, p, n);
}
static void RPC_ENTRY raw_echo(RPC_MESSAGE *m) { ULONG n = m->BufferLength; void *tmp = rx_alloc(n); memcpy(tmp, m->Buffer, n); InterlockedIncrement(&rx_raw_calls); raw_reply(m, tmp, n); rx_free(tmp); }
static void RPC_ENTRY raw_raise(RPC_MESSAGE *m) { int code = *(int *)m->Buffer; InterlockedIncrement(&rx_raw_calls); U(RpcRaiseException)((RPC_STATUS)code); }
static void RPC_ENTRY raw_sleep(RPC_MESSAGE *m) { int ms = *(int *)m->Buffer; InterlockedIncrement(&rx_raw_calls); Sleep(ms); raw_reply(m, NULL, 0); }
static void RPC_ENTRY raw_whoami(RPC_MESSAGE *m) {
    RPC_CALL_ATTRIBUTES_V2_W a; DWORD r[11]; RPC_STATUS st;
    InterlockedIncrement(&rx_raw_calls);
    memset(&a, 0, sizeof a); a.Version = 2;
    st = t_is_wine() ? (RPC_STATUS)0xFFFF : U(RpcServerInqCallAttributesW)(NULL, &a);
    r[0] = (DWORD)(ULONG_PTR)a.ClientPID; r[1] = a.AuthenticationLevel; r[2] = a.IsClientLocal; r[3] = a.AuthenticationService;
    r[4] = a.OpNum; r[5] = st; r[6] = a.ProtocolSequence; r[7] = a.CallStatus; r[8] = a.CallType; r[9] = a.NullSession; r[10] = a.KernelMode;
    raw_reply(m, r, sizeof r);
}
static void RPC_ENTRY raw_nop(RPC_MESSAGE *m) { InterlockedIncrement(&rx_raw_calls); raw_reply(m, NULL, 0); }
static void RPC_ENTRY raw_inqobject(RPC_MESSAGE *m) {
    struct { UUID u; DWORD st; } r; InterlockedIncrement(&rx_raw_calls);
    memset(&r, 0xAA, sizeof r);
    r.st = U(RpcBindingInqObject)(m->Handle, &r.u);
    raw_reply(m, &r, sizeof r);
}
static void RPC_ENTRY raw_stop(RPC_MESSAGE *m) { DWORD st; InterlockedIncrement(&rx_raw_calls); st = U(RpcMgmtStopServerListening)(NULL); raw_reply(m, &st, 4); }
static void RPC_ENTRY raw_waitcancel(RPC_MESSAGE *m) {
    DWORD st = 0, t0 = GetTickCount(); InterlockedIncrement(&rx_raw_calls);
    if (t_is_wine()) { st = 0xFFFF; raw_reply(m, &st, 4); return; }
    while (GetTickCount() - t0 < 5000) { st = U(RpcServerTestCancel)(NULL); if (st != RPC_S_OK) break; Sleep(10); }
    raw_reply(m, &st, 4);
}
static void RPC_ENTRY raw_inqmsg(RPC_MESSAGE *m) {
    DWORD r[4]; InterlockedIncrement(&rx_raw_calls);
    r[0] = m->DataRepresentation; r[1] = m->BufferLength; r[2] = m->ProcNum; r[3] = m->RpcFlags;
    raw_reply(m, r, sizeof r);
}
static void RPC_ENTRY raw_impersonate(RPC_MESSAGE *m) {
    DWORD r[4]; HANDLE tok = NULL; InterlockedIncrement(&rx_raw_calls);
    r[0] = U(RpcImpersonateClient)(NULL);
    r[1] = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
    r[2] = U(RpcRevertToSelf)();
    tok = NULL; r[3] = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
    raw_reply(m, r, sizeof r);
}
static void RPC_ENTRY raw_authclient(RPC_MESSAGE *m) {
    DWORD r[4]; RPC_AUTHZ_HANDLE pc = NULL; RPC_WSTR sn = NULL; unsigned long lvl = 0xdead, svc = 0xdead, az = 0xdead;
    InterlockedIncrement(&rx_raw_calls);
    r[0] = U(RpcBindingInqAuthClientW)(NULL, &pc, &sn, &lvl, &svc, &az);
    r[1] = lvl; r[2] = svc; r[3] = az;
    if (r[0] == RPC_S_OK && sn) U(RpcStringFreeW)(&sn);
    raw_reply(m, r, sizeof r);
}

/* request: DWORD version, DWORD flags.  reply: DWORD status, level, namelen(bytes, incl NUL), then client principal name chars */
static void RPC_ENTRY raw_attrs(RPC_MESSAGE *m) {
    struct { DWORD st, level, svc, namelen; WCHAR name[256]; } r; DWORD *q = (DWORD *)m->Buffer;
    RPC_CALL_ATTRIBUTES_V2_W a; WCHAR sbuf[256], cbuf[256];
    InterlockedIncrement(&rx_raw_calls);
    memset(&r, 0, sizeof r); memset(&a, 0, sizeof a); memset(sbuf, 0, sizeof sbuf); memset(cbuf, 0, sizeof cbuf);
    a.Version = q[0]; a.Flags = q[1];
    a.ServerPrincipalName = sbuf; a.ServerPrincipalNameBufferLength = sizeof sbuf;
    a.ClientPrincipalName = cbuf; a.ClientPrincipalNameBufferLength = sizeof cbuf;
    r.st = t_is_wine() ? 0xFFFF : U(RpcServerInqCallAttributesW)(NULL, &a);
    r.level = a.AuthenticationLevel; r.svc = a.AuthenticationService;
    r.namelen = a.ClientPrincipalNameBufferLength;
    memcpy(r.name, cbuf, sizeof cbuf);
    raw_reply(m, &r, sizeof r);
}
/* reply: DWORD status, then the string binding of RpcBindingServerFromClient(NULL) as WCHARs */
static void RPC_ENTRY raw_serverfromclient(RPC_MESSAGE *m) {
    struct { DWORD st; WCHAR s[200]; } r; RPC_BINDING_HANDLE b = NULL; RPC_WSTR str = NULL;
    InterlockedIncrement(&rx_raw_calls);
    memset(&r, 0, sizeof r);
    r.st = U(RpcBindingServerFromClient)(NULL, &b);
    if (r.st == RPC_S_OK && U(RpcBindingToStringBindingW)(b, &str) == RPC_S_OK) { lstrcpynW(r.s, (WCHAR *)str, 199); U(RpcStringFreeW)(&str); }
    if (b) U(RpcBindingFree)(&b);
    raw_reply(m, &r, sizeof r);
}

/* ---- gap_04 raw procedures ---- */
UDECL(RPC_STATUS, RpcImpersonateClientContainer, (RPC_BINDING_HANDLE));
UDECL(RPC_STATUS, RpcRevertContainerImpersonation, (void));
UDECL(RPC_STATUS, RpcRevertToSelfEx, (RPC_BINDING_HANDLE));
/* 13 inqbind: reply 6 x DWORD: st1, handle==call handle, st2, h1==h2, ToStringBinding status, protseq is ncalrpc */
static void RPC_ENTRY raw_inqbind(RPC_MESSAGE *m) {
    DWORD r[6]; RPC_BINDING_HANDLE h1 = (RPC_BINDING_HANDLE)(ULONG_PTR)0x1234, h2 = (RPC_BINDING_HANDLE)(ULONG_PTR)0x5678; RPC_WSTR s = NULL;
    InterlockedIncrement(&rx_raw_calls);
    memset(r, 0, sizeof r);
    r[0] = U(RpcServerInqBindingHandle)(&h1); r[1] = h1 == m->Handle;
    r[2] = U(RpcServerInqBindingHandle)(&h2); r[3] = h1 == h2;
    r[4] = U(RpcBindingToStringBindingW)(h1, &s);
    if (r[4] == RPC_S_OK && s) { r[5] = !_wcsnicmp((WCHAR *)s, L"ncalrpc", 7); U(RpcStringFreeW)(&s); }
    raw_reply(m, r, sizeof r);
}
/* 14 attrsA: request {version, flags, clientBufChars, serverBufChars}.
 * reply: {st, clientLen, serverLen, level, svc, local, pid, canary_ok, char cname[160], char sname[160]} */
typedef struct { DWORD st, clen, slen, level, svc, local, pid, canary_ok; char cname[160]; char sname[160]; } g4_attrsA_reply;
static void RPC_ENTRY raw_attrsA(RPC_MESSAGE *m) {
    g4_attrsA_reply r; DWORD *q = (DWORD *)m->Buffer; union { RPC_CALL_ATTRIBUTES_V2_A a; BYTE v3pad[0x100]; } ua; char cb[160], sb[160]; DWORD i, ok = 1;
#define a ua.a
    /* Win10 accepts Version 3 (a 0x78-byte structure, larger than the SDK V2 one): give it room so it cannot write past our buffer */
    InterlockedIncrement(&rx_raw_calls);
    memset(&r, 0, sizeof r); memset(&ua, 0, sizeof ua); memset(cb, 0x7f, sizeof cb); memset(sb, 0x7f, sizeof sb);
    a.Version = q[0]; a.Flags = q[1];
    a.ClientPrincipalName = (unsigned short *)cb; a.ClientPrincipalNameBufferLength = q[2];
    a.ServerPrincipalName = (unsigned short *)sb; a.ServerPrincipalNameBufferLength = q[3];
    r.st = t_is_wine() ? 0xFFFF : U(RpcServerInqCallAttributesA)(NULL, &a);
    r.clen = a.ClientPrincipalNameBufferLength; r.slen = a.ServerPrincipalNameBufferLength;
    r.level = a.AuthenticationLevel; r.svc = a.AuthenticationService; r.local = a.IsClientLocal; r.pid = (DWORD)(ULONG_PTR)a.ClientPID;
    for (i = q[2]; i < 160; i++) if (cb[i] != 0x7f) ok = 0;      /* nothing written past the caller's buffer length */
    for (i = q[3]; i < 160; i++) if (sb[i] != 0x7f) ok = 0;
    r.canary_ok = ok;
    memcpy(r.cname, cb, 160); memcpy(r.sname, sb, 160);
    raw_reply(m, &r, sizeof r);
#undef a
}
/* 15 container: reply 9 x DWORD:
 *  0 ImpersonateClientContainer(NULL)  1 RevertContainerImpersonation()  2 RevertToSelfEx(NULL) with nothing impersonated
 *  3 ImpersonateClient(NULL)  4 OpenThreadToken ok  5 RevertToSelfEx(call handle)  6 OpenThreadToken ok afterwards
 *  7 ImpersonateClientContainer(call handle)  8 RevertContainerImpersonation() after that */
static void RPC_ENTRY raw_container(RPC_MESSAGE *m) {
    DWORD r[9]; HANDLE tok = NULL;
    InterlockedIncrement(&rx_raw_calls);
    memset(r, 0, sizeof r);
    r[0] = UN(RpcImpersonateClientContainer)(NULL);
    r[1] = UN(RpcRevertContainerImpersonation)();
    r[2] = UN(RpcRevertToSelfEx)(NULL);
    r[3] = U(RpcImpersonateClient)(NULL);
    r[4] = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
    r[5] = UN(RpcRevertToSelfEx)(m->Handle);
    tok = NULL; r[6] = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
    r[7] = UN(RpcImpersonateClientContainer)(m->Handle);
    r[8] = UN(RpcRevertContainerImpersonation)();
    U(RpcRevertToSelf)();
    raw_reply(m, r, sizeof r);
}
/* 16 revertex: ImpersonateClient(NULL); RevertToSelfEx(NULL) ; reply {imp, tokenwhile, revert, tokenafter, revert2} */
static void RPC_ENTRY raw_revertex(RPC_MESSAGE *m) {
    DWORD r[5]; HANDLE tok = NULL;
    InterlockedIncrement(&rx_raw_calls);
    memset(r, 0, sizeof r);
    r[0] = U(RpcImpersonateClient)(NULL);
    r[1] = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
    r[2] = UN(RpcRevertToSelfEx)(NULL);
    tok = NULL; r[3] = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
    r[4] = UN(RpcRevertToSelfEx)(NULL);
    raw_reply(m, r, sizeof r);
}

void rx_raw_uuid(UUID *u, unsigned n) {
    memset(u, 0, sizeof *u);
    u->Data1 = 0x7a3c0000u + n; u->Data2 = 0x4c1d; u->Data3 = 0x4e7b;
    u->Data4[0] = 0x80; u->Data4[1] = 0x01; u->Data4[7] = (unsigned char)n;
}
void rx_raw_init(rx_raw *r, const UUID *id, unsigned short major, unsigned short minor) {
    memset(r, 0, sizeof *r);
    r->uuid = *id;
    r->fn[0] = raw_echo; r->fn[1] = raw_raise; r->fn[2] = raw_sleep; r->fn[3] = raw_whoami; r->fn[4] = raw_nop;
    r->fn[5] = raw_inqobject; r->fn[6] = raw_stop; r->fn[7] = raw_waitcancel; r->fn[8] = raw_inqmsg;
    r->fn[9] = raw_impersonate; r->fn[10] = raw_authclient; r->fn[11] = raw_attrs; r->fn[12] = raw_serverfromclient;
    r->fn[13] = raw_inqbind; r->fn[14] = raw_attrsA; r->fn[15] = raw_container; r->fn[16] = raw_revertex;
    r->dt.DispatchTableCount = 17; r->dt.DispatchTable = r->fn;
    r->si.Length = sizeof r->si;
    r->si.InterfaceId.SyntaxGUID = *id; r->si.InterfaceId.SyntaxVersion.MajorVersion = major; r->si.InterfaceId.SyntaxVersion.MinorVersion = minor;
    r->si.TransferSyntax = rx_ndr_syntax;
    r->si.DispatchTable = &r->dt;
    r->ci.Length = sizeof r->ci;
    r->ci.InterfaceId = r->si.InterfaceId;
    r->ci.TransferSyntax = rx_ndr_syntax;
}

static RPC_STATUS raw_once(RPC_BINDING_HANDLE h, const rx_raw *r, unsigned proc, const void *in, ULONG inlen,
                           void *out, ULONG outcap, ULONG *outlen, ULONG rpcflags) {
    RPC_MESSAGE m; RPC_STATUS st;
    memset(&m, 0, sizeof m);
    m.Handle = h; m.DataRepresentation = 0x10; m.ProcNum = proc; m.BufferLength = inlen;
    m.RpcInterfaceInformation = (void *)&r->ci; m.TransferSyntax = (PRPC_SYNTAX_IDENTIFIER)&r->ci.TransferSyntax;
    m.RpcFlags = rpcflags;
    if (outlen) *outlen = 0;
    st = U(I_RpcGetBuffer)(&m);
    if (st != RPC_S_OK) return st;
    if (inlen) memcpy(m.Buffer, in, inlen);
    st = U(I_RpcSendReceive)(&m);
    if (st != RPC_S_OK) return st;      /* buffer ownership after a failed send is runtime-defined: do not free */
    if (outlen) *outlen = m.BufferLength;
    if (out && outcap) memcpy(out, m.Buffer, m.BufferLength < outcap ? m.BufferLength : outcap);
    U(I_RpcFreeBuffer)(&m);
    return RPC_S_OK;
}
RPC_STATUS rx_raw_call_obj(RPC_BINDING_HANDLE h, const rx_raw *r, unsigned proc, const void *in, ULONG inlen,
                           void *out, ULONG outcap, ULONG *outlen, ULONG rpcflags) {
    RPC_STATUS st = raw_once(h, r, proc, in, inlen, out, outcap, outlen, rpcflags);
    for (int k = 0; k < 3 && t_is_wine() && st == RPC_S_CALL_FAILED_DNE; k++) st = raw_once(h, r, proc, in, inlen, out, outcap, outlen, rpcflags);
    return st;
}
RPC_STATUS rx_raw_call(RPC_BINDING_HANDLE h, const rx_raw *r, unsigned proc, const void *in, ULONG inlen,
                       void *out, ULONG outcap, ULONG *outlen) {
    return rx_raw_call_obj(h, r, proc, in, inlen, out, outcap, outlen, 0);
}

/* ------------------------------------------------------------------ server / binding control */
RPC_STATUS rx_use_ep(const WCHAR *ep) {
    int i; RPC_STATUS st;
    st = U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)ep, NULL);
    for (i = 0; i < g_nreg; i++) if (!lstrcmpW(g_reg[i], ep)) { if (st == RPC_S_DUPLICATE_ENDPOINT) st = RPC_S_OK; return st; }
    if (st == RPC_S_OK && g_nreg < RX_EP_POOL * 2) lstrcpynW(g_reg[g_nreg++], ep, 64);
    return st;
}
RPC_BINDING_HANDLE rx_bind_obj(const WCHAR *ep, const UUID *obj) {
    RPC_WSTR sb = NULL, ostr = NULL; RPC_BINDING_HANDLE h = NULL;
    if (obj && U(UuidToStringW)(obj, &ostr) != RPC_S_OK) return NULL;
    if (U(RpcStringBindingComposeW)(ostr, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)ep, NULL, &sb) != RPC_S_OK) return NULL;
    if (ostr) U(RpcStringFreeW)(&ostr);
    if (U(RpcBindingFromStringBindingW)(sb, &h) != RPC_S_OK) h = NULL;
    U(RpcStringFreeW)(&sb);
    return h;
}
RPC_BINDING_HANDLE rx_bind(const WCHAR *ep) { return rx_bind_obj(ep, NULL); }
void rx_unbind(RPC_BINDING_HANDLE h) { if (h) U(RpcBindingFree)(&h); }

int rx_listening(void) { return U(RpcMgmtIsServerListening)(NULL) == RPC_S_OK; }

static DWORD WINAPI wait_thread(void *p) { (void)p; return U(RpcMgmtWaitServerListen)(); }
HANDLE rx_thread(LPTHREAD_START_ROUTINE fn, void *arg) { return CreateThread(NULL, 0, fn, arg, 0, NULL); }
BOOL rx_join(HANDLE th, DWORD ms) {
    BOOL ok;
    if (!th) return FALSE;
    ok = WaitForSingleObject(th, ms) == WAIT_OBJECT_0;
    CloseHandle(th);
    return ok;
}
void rx_server_reset(void) {
    if (rx_listening()) {
        HANDLE th;
        U(RpcMgmtStopServerListening)(NULL);
        th = rx_thread(wait_thread, NULL);
        rx_join(th, 10000);
    }
}
typedef struct { RPC_IF_HANDLE ifs; UUID *mgr; } unreg_job;
static DWORD WINAPI unreg_thread(void *p) { unreg_job *j = p; return U(RpcServerUnregisterIf)(j->ifs, j->mgr, 1); }
void rx_server_down(RPC_IF_HANDLE ifs, UUID *mgrtype) {
    HANDLE th;
    static unreg_job jobs[64]; static LONG nj;
    U(RpcMgmtStopServerListening)(NULL);
    th = rx_thread(wait_thread, NULL);
    rx_join(th, 10000);
    if (ifs) {
        unreg_job *j = &jobs[InterlockedIncrement(&nj) & 63];
        j->ifs = ifs; j->mgr = mgrtype;
        th = rx_thread(unreg_thread, j);
        if (!rx_join(th, 10000)) {                  /* bounded: a call that never completes must not hang the run */
            t_note("RpcServerUnregisterIf(wait) did not return within 10 s");
            U(RpcServerUnregisterIf)(ifs, mgrtype, 0);
        }
    }
}
RPC_STATUS rx_serve_ex(RPC_IF_HANDLE ifs, const WCHAR *ep, UUID *mgrtype, void *mgrepv, unsigned flags,
                       unsigned maxcalls, RPC_IF_CALLBACK_FN *cb) {
    RPC_STATUS st;
    rx_init_dtab();
    rx_server_reset();
    st = rx_use_ep(ep); if (st != RPC_S_OK) return st;
    st = U(RpcServerRegisterIfEx)(ifs, mgrtype, mgrepv, flags, maxcalls, cb); if (st != RPC_S_OK) return st;
    if (flags & RPC_IF_AUTOLISTEN) return RPC_S_OK;
    return U(RpcServerListen)(1, 20, TRUE);
}
RPC_STATUS rx_serve(RPC_IF_HANDLE ifs, const WCHAR *ep, UUID *mgrtype, void *mgrepv) {
    return rx_serve_ex(ifs, ep, mgrtype, mgrepv, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL);
}

int rx_fx_up(rx_fx *f) {
    memset(f, 0, sizeof *f);
    rx_ep(f->ep, 64);
    if (rx_serve(rx_xif_s_ifspec, f->ep, NULL, NULL) != RPC_S_OK) return 0;
    f->h = rx_bind(f->ep);
    return f->h != NULL;
}
void rx_fx_down(rx_fx *f) { rx_unbind(f->h); f->h = NULL; rx_server_down(rx_xif_s_ifspec, NULL); }
DWORD rx_add(RPC_BINDING_HANDLE h, int a, int b, int *r) {
    LONG_PTR v = 0; DWORD e = rx_ndr_h(h, RX_ADD, (ULONG_PTR)(LONG_PTR)a, (ULONG_PTR)(LONG_PTR)b, 0, 0, &v);
    if (r) *r = (int)v;
    return e;
}

/* ------------------------------------------------------------------ child-process server */
HANDLE rx_spawn_server(const WCHAR *ep, const char *mode, PROCESS_INFORMATION *pi) {
    char args[256], epa[128];
    WideCharToMultiByte(CP_ACP, 0, ep, -1, epa, sizeof epa, NULL, NULL);
    snprintf(args, sizeof args, "%s %s", epa, mode ? mode : "plain");
    return t_spawn_child("gap4srv", args, 0, NULL, NULL, NULL, FALSE, NULL, pi);
}
int rx_wait_server_ready(const WCHAR *ep, DWORD ms) {
    DWORD t0 = GetTickCount(); RPC_BINDING_HANDLE h = rx_bind(ep);
    int ok = 0;
    while (h && GetTickCount() - t0 < ms) {
        if (rx_ndr_h(h, RX_VOID, 0, 0, 0, 0, NULL) == 0) { ok = 1; break; }
        Sleep(50);
    }
    rx_unbind(h);
    return ok;
}

/* argv: rpcsrv <endpoint> <mode>; mode: plain | winnt (RegisterAuthInfo) | deny (security callback denies) | raw1 */
static rx_raw g_child_raw;
CHILD(gap4srv) {
    char *epa = argc > 1 ? argv[1] : "olet_x"; const char *mode = argc > 2 ? argv[2] : "plain";
    WCHAR ep[128]; RPC_STATUS st; unsigned flags = 0; RPC_IF_CALLBACK_FN *cb = NULL; UUID u;
    MultiByteToWideChar(CP_ACP, 0, epa, -1, ep, 128);
    rx_init_dtab();
    if (!strcmp(mode, "noprotseq")) {            /* fresh process: Listen with nothing registered */
        st = U(RpcServerListen)(1, 20, TRUE);
        return (int)st;
    }
    if (!strcmp(mode, "nodefault")) {            /* report RpcMgmtIsServerListening of a fresh process */
        return (int)U(RpcMgmtIsServerListening)(NULL);
    }
    st = rx_use_ep(ep); if (st) return (int)st;   /* exit code = RPC status of UseProtseqEp */
    if (!strcmp(mode, "dontser")) {              /* RpcSsDontSerializeContext, then lock calls inside a context call */
        typedef void (WINAPI *dns_fn)(void);
        dns_fn dns = (dns_fn)t_proc("RpcSsDontSerializeContext");
        if (!dns) return 0x7010;
        dns();
        g4_ctxmode = 3;
        st = U(RpcServerRegisterIfEx)(rx_xif_s_ifspec, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL);
        if (st) return 0x7004;
        st = U(RpcServerListen)(1, 20, FALSE);
        if (st) return 0x7006;
        return (int)((g4_ctxres[0] & 0xff) | ((g4_ctxres[1] & 0xff) << 8) | ((g4_ctxres[2] & 0xff) << 16));
    }
    if (!strcmp(mode, "winnt")) { st = U(RpcServerRegisterAuthInfoW)(NULL, RPC_C_AUTHN_WINNT, NULL, NULL); if (st) return 0x7003; }
    if (!strcmp(mode, "deny")) { rx_cb_result = RPC_S_ACCESS_DENIED; cb = rx_sec_cb; flags = RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH; }
    st = U(RpcServerRegisterIfEx)(rx_xif_s_ifspec, NULL, NULL, flags, RPC_C_LISTEN_MAX_CALLS_DEFAULT, cb);
    if (st) return 0x7004;
    rx_raw_uuid(&u, 1); rx_raw_init(&g_child_raw, &u, 1, 0);
    st = U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&g_child_raw.si, NULL, NULL, flags, RPC_C_LISTEN_MAX_CALLS_DEFAULT, cb);
    if (st) return 0x7005;
    st = U(RpcServerListen)(1, 20, FALSE);       /* returns after RpcMgmtStopServerListening */
    return st ? 0x7006 : 0;
}
