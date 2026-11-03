/* RpcServerUseProtseq{A,IfA,IfW,AllProtseqsIf} endpoint registration from the interface's protseq/endpoint table, and
 * NdrStubCall2 / NdrStubCall3 driven as the dispatch routine of a live ncalrpc interface.
 *
 * Decomp (rpcrt4 19041):
 *   RpcServerUseProtseqIfExW(Protseq, MaxCalls, IfSpec, SD, Policy):
 *      Policy->Length < 12 -> 0x6c6 (RPC_S_INVALID_BOUND); for each table entry whose protocol sequence matches
 *      (_wcsnicmp over wcslen(Protseq) characters: a prefix of the entry's name matches too) the endpoint is registered
 *      (I_RpcServerUseProtseqEp2A); the first failure is returned at once; >= 1 registration -> 0; none -> 0x6a7.
 *   RpcServerUseProtseqIfA / UseProtseqA: strings longer than 0xffff chars -> ERROR_INVALID_PARAMETER.
 *   RpcServerUseAllProtseqsIfEx: Policy->Length < 12 -> 0x6c6; empty table -> 0x6b7; every entry is tried (errors other
 *      than OUT_OF_MEMORY/0x53a/0x6b9 are ignored); >= 1 success -> 0; otherwise 0x6b7 (RPC_S_NO_PROTSEQS).
 *   NdrServerCall2(msg) == NdrStubCall2(NULL, NULL, msg, &phase); NdrStubCall3 forwards to NdrStubCall2 for NDR20. */
#include "gap7.h"
#include <rpcdcep.h>
#include "gap7_fmt.h"

#define S_INVALID_BOUND     0x6c6
#define S_PROTSEQ_UNSUP     0x6a7
#define S_NO_PROTSEQS       0x6b7

#define WINE_SKIP_IF() do { if (t_is_wine()) { t_skip("RpcServerUseProtseqIf*/AllProtseqsIf* are not implemented in wine"); return; } } while (0)

UDECL(RPC_STATUS, RpcCertGeneratePrincipalNameW, (void *, DWORD, RPC_WSTR *));
UDECL(RPC_STATUS, RpcCertGeneratePrincipalNameA, (void *, DWORD, RPC_CSTR *));

/* ------------------------------------------------------------------ the interface */
enum { P_ADD_CTL, P_ADD_S2, P_ADD_S3, P_WLEN_S2, P_WLEN_S3, P_RAISE_S2, P_RAISE_S3, P_COUNT };
static const unsigned char g_proc_fmt[2 + 44 * 3 + 38 * 2 + 32 * 2 + 2] = { 0, 0,
    ADD_PROC(0) ADD_PROC(1) ADD_PROC(2) WLEN_PROC(3) WLEN_PROC(4) RAISE_PROC(5) RAISE_PROC(6) 0, 0 };
static const unsigned short g_off[P_COUNT] = { 0, 44, 88, 132, 170, 208, 240 };
static const unsigned char g_type_fmt[] = { 0, 0, 0, 0, 0x11, 0x08, 0x25, 0x5c, 0 };

static volatile LONG g_calls, g_d2, g_d3, g_ret2, g_ret3, g_ph2, g_ph3, g_ts_bad;
static handle_t g_ih;
static void *__RPC_API s_alloc(size_t n) { return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
static void __RPC_API s_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }

static int s_add(int a, int b) { InterlockedIncrement(&g_calls); return a + b; }
static int s_wlen(const WCHAR *s) { InterlockedIncrement(&g_calls); return lstrlenW(s); }
static void s_raise(int code) { InterlockedIncrement(&g_calls); UIN(RT, RpcRaiseException)((RPC_STATUS)code); }
static const SERVER_ROUTINE g_routines[P_COUNT] = { (SERVER_ROUTINE)s_add, (SERVER_ROUTINE)s_add, (SERVER_ROUTINE)s_add,
    (SERVER_ROUTINE)s_wlen, (SERVER_ROUTINE)s_wlen, (SERVER_ROUTINE)s_raise, (SERVER_ROUTINE)s_raise };

static void __stdcall disp2(PRPC_MESSAGE m)
{
    ULONG ph = 0x7777; LONG r = U(NdrStubCall2)(NULL, NULL, m, &ph);
    g_ret2 = r; g_ph2 = (LONG)ph; InterlockedIncrement(&g_d2);
}
static void __stdcall disp3(PRPC_MESSAGE m)
{
    ULONG ph = 0x7777; LONG r;
    if (m->TransferSyntax && m->TransferSyntax->SyntaxGUID.Data1 != 0x8a885d04) InterlockedIncrement(&g_ts_bad);
    r = U(NdrStubCall3)(NULL, NULL, m, &ph);
    g_ret3 = r; g_ph3 = (LONG)ph; InterlockedIncrement(&g_d3);
}
static RPC_DISPATCH_FUNCTION g_dtab[P_COUNT];
static RPC_DISPATCH_TABLE g_dt = { P_COUNT, g_dtab, 0 };
static const MIDL_SERVER_INFO g_sinfo;
static RPC_SERVER_INTERFACE g_si = {
    sizeof(RPC_SERVER_INTERFACE),
    { {0x6f0e7a10, 0x0007, 0x4c3d, {0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb7}}, {1, 0} },
    { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} },
    &g_dt, 0, 0, 0, (void *)&g_sinfo, 0 };
static const RPC_CLIENT_INTERFACE g_ci = {
    sizeof(RPC_CLIENT_INTERFACE),
    { {0x6f0e7a10, 0x0007, 0x4c3d, {0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb7}}, {1, 0} },
    { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} }, 0, 0, 0, 0, 0, 0 };
static const MIDL_STUB_DESC g_cstub = {
    (void *)&g_ci, s_alloc, s_free, { &g_ih }, 0, 0, 0, 0, &g_type_fmt[2], 1, 0x50002, 0, 0x50200ca, 0, 0, 0, 1, 0, 0, 0 };
static const MIDL_STUB_DESC g_sstub = {
    (void *)&g_si, s_alloc, s_free, { 0 }, 0, 0, 0, 0, &g_type_fmt[2], 1, 0x50002, 0, 0x50200ca, 0, 0, 0, 1, 0, 0, 0 };
static const MIDL_SERVER_INFO g_sinfo = { &g_sstub, g_routines, &g_proc_fmt[2], g_off, 0, 0, 0, 0 };

static void init_dtab(void)
{
    g_dtab[P_ADD_CTL] = (RPC_DISPATCH_FUNCTION)U(NdrServerCall2);
    g_dtab[P_ADD_S2] = g_dtab[P_WLEN_S2] = g_dtab[P_RAISE_S2] = disp2;
    g_dtab[P_ADD_S3] = g_dtab[P_WLEN_S3] = g_dtab[P_RAISE_S3] = disp3;
}

/* ------------------------------------------------------------------ naming, serving, binding */
static volatile LONG g_epn;
static void new_ep(char *a, WCHAR *w)
{
    LONG n = InterlockedIncrement(&g_epn);
    wsprintfA(a, "olet_gap7_%lu_%ld", GetCurrentProcessId(), n);
    if (w) g7_widen(a, w, 64);
}
static RPC_BINDING_HANDLE bind_to(const char *ep)
{
    RPC_CSTR sb = NULL; RPC_BINDING_HANDLE h = NULL;
    if (UIN(RT, RpcStringBindingComposeA)(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)ep, NULL, &sb) != RPC_S_OK) return NULL;
    if (UIN(RT, RpcBindingFromStringBindingA)(sb, &h) != RPC_S_OK) h = NULL;
    UIN(RT, RpcStringFreeA)(&sb);
    return h;
}
static DWORD WINAPI wait_thread(void *p) { (void)p; return U(RpcMgmtWaitServerListen)(); }
static DWORD WINAPI unreg_thread(void *p) { (void)p; return U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&g_si, NULL, 1); }
static void bounded(LPTHREAD_START_ROUTINE fn)
{
    HANDLE th = CreateThread(NULL, 0, fn, NULL, 0, NULL);
    if (!th) return;
    if (WaitForSingleObject(th, 10000) != WAIT_OBJECT_0) t_note("server shutdown step did not finish within 10 s");
    CloseHandle(th);
}
static void srv_reset(void)
{
    if (U(RpcMgmtIsServerListening)(NULL) == RPC_S_OK) { U(RpcMgmtStopServerListening)(NULL); bounded(wait_thread); }
}
static void srv_down(void)
{
    U(RpcMgmtStopServerListening)(NULL);
    bounded(wait_thread);
    bounded(unreg_thread);
}
static RPC_PROTSEQ_ENDPOINT g_pe[4];
static char g_pe_proto[4][32], g_pe_ep[4][96];
static void table_set(unsigned i, const char *proto, const char *ep)
{
    lstrcpyA(g_pe_proto[i], proto); lstrcpyA(g_pe_ep[i], ep);
    g_pe[i].RpcProtocolSequence = (unsigned char *)g_pe_proto[i]; g_pe[i].Endpoint = (unsigned char *)g_pe_ep[i];
}
static void table_apply(unsigned n) { g_si.RpcProtseqEndpointCount = n; g_si.RpcProtseqEndpoint = n ? g_pe : NULL; }
static RPC_STATUS register_and_listen(void)
{
    RPC_STATUS st;
    init_dtab();
    st = U(RpcServerRegisterIf)((RPC_IF_HANDLE)&g_si, NULL, NULL);
    if (st != RPC_S_OK) return st;
    return U(RpcServerListen)(1, 20, TRUE);
}

/* ------------------------------------------------------------------ client side (on a thread, exceptions captured) */
static volatile DWORD g_uef_code;
static LONG WINAPI uef(EXCEPTION_POINTERS *ep) { g_uef_code = ep->ExceptionRecord->ExceptionCode; ExitThread(0xE0C0DE01); return EXCEPTION_EXECUTE_HANDLER; }
typedef struct { unsigned proc; ULONG_PTR a0, a1; LONG_PTR ret; } cjob;
static DWORD WINAPI call_thread(void *p)
{
    cjob *j = p; CLIENT_CALL_RETURN r;
    r = U(NdrClientCall2)((PMIDL_STUB_DESC)&g_cstub, (PFORMAT_STRING)&g_proc_fmt[2 + g_off[j->proc]], j->a0, j->a1);
    j->ret = (LONG_PTR)r.Simple;
    return 0;
}
/* 0 or the exception code raised to the caller */
static DWORD ndr_call(RPC_BINDING_HANDLE h, unsigned proc, ULONG_PTR a0, ULONG_PTR a1, LONG_PTR *ret)
{
    static int once; cjob j; HANDLE th; DWORD w, ec = 0;
    if (!once) { once = 1; SetUnhandledExceptionFilter(uef); }
    memset(&j, 0, sizeof j); j.proc = proc; j.a0 = a0; j.a1 = a1;
    g_ih = h; g_uef_code = 0;
    th = CreateThread(NULL, 0, call_thread, &j, 0, NULL);
    if (!th) return G7_TIMEOUT;
    w = WaitForSingleObject(th, 15000);
    if (w != WAIT_OBJECT_0) { TerminateThread(th, 1); CloseHandle(th); return G7_TIMEOUT; }
    GetExitCodeThread(th, &ec); CloseHandle(th);
    if (ret) *ret = j.ret;
    return ec == 0xE0C0DE01 ? (g_uef_code ? g_uef_code : 0xFFFFFFF1u) : 0;
}
typedef struct { char ep[64]; RPC_BINDING_HANDLE h; } fx;
static int fx_up(fx *f)
{
    RPC_STATUS st;
    memset(f, 0, sizeof *f);
    srv_reset();
    new_ep(f->ep, NULL);
    table_set(0, "ncalrpc", f->ep); table_apply(1);
    if (t_is_wine()) st = U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", 10, (RPC_CSTR)f->ep, NULL);
    else st = U(RpcServerUseProtseqIfA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL);
    if (st != RPC_S_OK) { CHECKF(0, "protseq registration failed %lu", (unsigned long)st); return 0; }
    st = register_and_listen();
    if (st != RPC_S_OK) { CHECKF(0, "register/listen failed %lu", (unsigned long)st); srv_down(); return 0; }
    f->h = bind_to(f->ep);
    if (!f->h) { CHECKF(0, "binding failed"); srv_down(); return 0; }
    return 1;
}
static void fx_down(fx *f) { if (f->h) UIN(RT, RpcBindingFree)(&f->h); srv_down(); }

/* ======================= NdrStubCall2 / NdrStubCall3 as dispatch routines ======================= */
TEST(gap7_stubcall_server_call2_control_add)
{
    fx f; unsigned i; static const int v[][2] = { {0,0}, {1,2}, {-5,3}, {0x7fffffff,1}, {100000,-100000} };
    REQUIRE(U(NdrServerCall2)); REQUIRE(U(NdrClientCall2)); REQUIRE(U(RpcServerListen));
    if (!fx_up(&f)) return;
    for (i = 0; i < NELEM(v); i++) {
        LONG_PTR r = -1; DWORD x = ndr_call(f.h, P_ADD_CTL, v[i][0], v[i][1], &r);
        EQ(x, 0); EQ((int)r, v[i][0] + v[i][1]);
    }
    fx_down(&f);
}
TEST(gap7_stubcall2_as_dispatch_routine_add)
{
    fx f; unsigned i; static const int v[][2] = { {0,0}, {1,2}, {-5,3}, {0x7fffffff,1}, {123456,654321} };
    REQUIRE(U(NdrStubCall2)); REQUIRE(U(NdrClientCall2)); REQUIRE(U(RpcServerListen));
    if (!fx_up(&f)) return;
    g_d2 = 0; g_calls = 0;
    for (i = 0; i < NELEM(v); i++) {
        LONG_PTR r = -1; DWORD x = ndr_call(f.h, P_ADD_S2, v[i][0], v[i][1], &r);
        EQ(x, 0); EQ((int)r, v[i][0] + v[i][1]);
    }
    EQ(g_d2, NELEM(v)); EQ(g_calls, NELEM(v));            /* the wrapper ran once per call and reached the server routine once */
    CHECKF(g_ret2 >= 0, "NdrStubCall2 returned 0x%lx", (unsigned long)g_ret2);
    t_note("NdrStubCall2 return %ld, *pdwStubPhase %ld after a completed call", (long)g_ret2, (long)g_ph2);
    fx_down(&f);
}
TEST(gap7_stubcall3_as_dispatch_routine_add_ndr20)
{
    fx f; unsigned i; static const int v[][2] = { {0,0}, {7,8}, {-1,-1}, {0x40000000,0x3fffffff} };
    REQUIRE(U(NdrStubCall3)); REQUIRE(U(NdrClientCall2)); REQUIRE(U(RpcServerListen));
    if (t_is_wine()) { t_skip("NdrStubCall3 is not exported by wine"); return; }
    if (!fx_up(&f)) return;
    g_d3 = 0; g_ts_bad = 0; g_calls = 0;
    for (i = 0; i < NELEM(v); i++) {
        LONG_PTR r = -1; DWORD x = ndr_call(f.h, P_ADD_S3, v[i][0], v[i][1], &r);
        EQ(x, 0); EQ((int)r, v[i][0] + v[i][1]);
    }
    EQ(g_d3, NELEM(v)); EQ(g_calls, NELEM(v));
    EQ(g_ts_bad, 0);                                       /* the runtime hands NDR20 (8a885d04-...) or NULL to the dispatcher */
    CHECKF(g_ret3 >= 0, "NdrStubCall3 returned 0x%lx", (unsigned long)g_ret3);
    fx_down(&f);
}
TEST(gap7_stubcall_variants_agree_on_string_calls)
{
    fx f; unsigned i, k; static WCHAR big[8193];
    static const WCHAR *t[] = { L"", L"a", L"hello world", L"h\x00e9llo \x20ac \xd83d\xde00", L"embedded\\path\\\"quote" };
    REQUIRE(U(NdrStubCall2)); REQUIRE(U(NdrClientCall2)); REQUIRE(U(RpcServerListen));
    if (!fx_up(&f)) return;
    for (i = 0; i < 8192; i++) big[i] = L'A' + (i % 26);
    big[8192] = 0;
    for (k = 0; k < 2; k++) {
        unsigned proc = k ? P_WLEN_S3 : P_WLEN_S2;
        if (k && (t_is_wine() || !U(NdrStubCall3))) break;
        for (i = 0; i < NELEM(t); i++) {
            LONG_PTR r = -1; DWORD x = ndr_call(f.h, proc, (ULONG_PTR)t[i], 0, &r);
            EQ(x, 0); EQ((int)r, lstrlenW(t[i]));
        }
        { LONG_PTR r = -1; DWORD x = ndr_call(f.h, proc, (ULONG_PTR)big, 0, &r); EQ(x, 0); EQ((int)r, 8192); }
        { LONG_PTR r = -1; DWORD x = ndr_call(f.h, proc, 0, 0, &r); EQ(x, RPC_X_NULL_REF_POINTER); }   /* client stub rejects a NULL [in,string] */
    }
    fx_down(&f);
}
TEST(gap7_stubcall_server_fault_reaches_client_identically_for_all_dispatchers)
{
    static const int codes[] = { 1234, 1723, 0x6c6, 5 };
    fx f; unsigned i;
    REQUIRE(U(NdrStubCall2)); REQUIRE(U(NdrClientCall2)); REQUIRE(U(RpcServerListen));
    if (!fx_up(&f)) return;
    for (i = 0; i < NELEM(codes); i++) {
        LONG_PTR r;
        DWORD s2 = ndr_call(f.h, P_RAISE_S2, codes[i], 0, &r), s3 = s2;
        if (!t_is_wine() && U(NdrStubCall3)) s3 = ndr_call(f.h, P_RAISE_S3, codes[i], 0, &r);
        CHECKF(s2 != 0, "code %d: fault must reach the caller", codes[i]);
        CHECKF(s2 == s3, "code %d: StubCall2 0x%lx vs StubCall3 0x%lx", codes[i], (unsigned long)s2, (unsigned long)s3);
        if (codes[i] == 1234) CHECKF(s2 == 1234, "code 1234: client saw 0x%lx", (unsigned long)s2);   /* a plain code passes through unchanged */
        else t_note("server raised %d -> client exception %lu", codes[i], (unsigned long)s2);
    }
    fx_down(&f);
}
static volatile LONG g_conc_bad;
static DWORD WINAPI conc_thread(void *p)
{
    unsigned id = (unsigned)(ULONG_PTR)p, i;
    for (i = 0; i < 40; i++) {
        LONG_PTR r = -1; unsigned proc = (i + id) % 2 ? P_ADD_S2 : P_ADD_CTL;
        if (ndr_call(g_ih, proc, id * 1000 + i, i, &r) != 0 || (int)r != (int)(id * 1000 + 2 * i)) InterlockedIncrement(&g_conc_bad);
    }
    return 0;
}
TEST(gap7_stubcall2_concurrent_clients)
{
    fx f; HANDLE th[4]; unsigned i;
    REQUIRE(U(NdrStubCall2)); REQUIRE(U(NdrClientCall2)); REQUIRE(U(RpcServerListen));
    if (!fx_up(&f)) return;
    g_conc_bad = 0; g_calls = 0;
    /* ndr_call sets the shared implicit handle before each call, from several threads: use the same value */
    g_ih = f.h;
    for (i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, conc_thread, (void *)(ULONG_PTR)(i + 1), 0, NULL);
    for (i = 0; i < 4; i++) if (th[i]) { if (WaitForSingleObject(th[i], 30000) != WAIT_OBJECT_0) CHECKF(0, "worker %u timed out", i); CloseHandle(th[i]); }
    EQ(g_conc_bad, 0); EQ(g_calls, 160);
    fx_down(&f);
}

/* ======================= RpcServerUseProtseqIf{A,W} / UseAllProtseqsIf ======================= */
/* run one add call through a fresh binding to ep: 0 on success */
static int endpoint_live(const char *ep)
{
    RPC_BINDING_HANDLE h = bind_to(ep); LONG_PTR r = 0; DWORD x;
    if (!h) return 0;
    x = ndr_call(h, P_ADD_CTL, 40, 2, &r);
    UIN(RT, RpcBindingFree)(&h);
    return x == 0 && (int)r == 42;
}
TEST(gap7_useprotseqifa_registers_endpoint_from_table)
{
    char ep[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqIfA)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
    st = U(RpcServerUseProtseqIfA)((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_IF_HANDLE)&g_si, NULL);
    EQ(st, RPC_S_OK);
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(endpoint_live(ep));
    srv_down();
}
TEST(gap7_useprotseqifw_registers_endpoint_from_table)
{
    char ep[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
    st = U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_IF_HANDLE)&g_si, NULL);
    EQ(st, RPC_S_OK);
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(endpoint_live(ep));
    srv_down();
}
TEST(gap7_useprotseqifw_protseq_match_is_case_insensitive)
{
    static const WCHAR *names[] = { L"NCALRPC", L"NcAlRpC" };
    unsigned i;
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    for (i = 0; i < NELEM(names); i++) {
        char ep[64];
        srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
        EQ(U(RpcServerUseProtseqIfW)((RPC_WSTR)names[i], 10, (RPC_IF_HANDLE)&g_si, NULL), RPC_S_OK);
        EQ(register_and_listen(), RPC_S_OK);
        CHECK(endpoint_live(ep));
        srv_down();
    }
}
TEST(gap7_useprotseqifw_protseq_compared_by_prefix_of_its_own_length)
{
    /* decomp: _wcsnicmp(Protseq, tableProtseq, wcslen(Protseq)): a prefix, and the empty string, match the entry */
    static const WCHAR *names[] = { L"ncal", L"n", L"" };
    unsigned i;
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    for (i = 0; i < NELEM(names); i++) {
        char ep[64]; RPC_STATUS st;
        srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
        st = U(RpcServerUseProtseqIfW)((RPC_WSTR)names[i], 10, (RPC_IF_HANDLE)&g_si, NULL);
        CHECKF(st == RPC_S_OK, "protseq \"%ls\" -> %lu", names[i], (unsigned long)st);
        if (st == RPC_S_OK) { EQ(register_and_listen(), RPC_S_OK); CHECK(endpoint_live(ep)); srv_down(); }
    }
}
TEST(gap7_useprotseqif_no_matching_entry_is_protseq_not_supported)
{
    char ep[64];
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(RpcServerUseProtseqIfA)); WINE_SKIP_IF();
    srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
    EQ(U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncacn_np", 10, (RPC_IF_HANDLE)&g_si, NULL), S_PROTSEQ_UNSUP);
    EQ(U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncalrpcx", 10, (RPC_IF_HANDLE)&g_si, NULL), S_PROTSEQ_UNSUP);   /* longer than the entry: no match */
    EQ(U(RpcServerUseProtseqIfA)((RPC_CSTR)"ncacn_ip_tcp", 10, (RPC_IF_HANDLE)&g_si, NULL), S_PROTSEQ_UNSUP);
    EQ(U(RpcServerUseProtseqIfA)((RPC_CSTR)"olet_bogus", 10, (RPC_IF_HANDLE)&g_si, NULL), S_PROTSEQ_UNSUP);
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(!endpoint_live(ep));                            /* nothing was registered for the table entry */
    srv_down();
}
TEST(gap7_useprotseqif_empty_table_is_protseq_not_supported)
{
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(RpcServerUseProtseqIfA)); WINE_SKIP_IF();
    table_apply(0);
    EQ(U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL), S_PROTSEQ_UNSUP);
    EQ(U(RpcServerUseProtseqIfA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL), S_PROTSEQ_UNSUP);
}
TEST(gap7_useprotseqifw_registers_every_matching_entry_and_skips_others)
{
    char ea[64], eb[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    srv_reset(); new_ep(ea, NULL); new_ep(eb, NULL);
    table_set(0, "ncalrpc", ea); table_set(1, "ncacn_np", "\\pipe\\olet_gap7_skipme"); table_set(2, "ncalrpc", eb); table_apply(3);
    st = U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL);
    EQ(st, RPC_S_OK);
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(endpoint_live(ea)); CHECK(endpoint_live(eb));
    srv_down();
}
TEST(gap7_useprotseqifw_first_failing_entry_aborts_with_its_status)
{
    char ea[64], eb[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqIfW)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    srv_reset(); new_ep(ea, NULL); new_ep(eb, NULL);
    table_set(0, "ncalrpc", ea); table_set(1, "ncalrpc", ea); table_set(2, "ncalrpc", eb); table_apply(3);
    st = U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL);
    EQ(st, RPC_S_DUPLICATE_ENDPOINT);                     /* second entry repeats the first endpoint */
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(endpoint_live(ea));                             /* the first registration stays in effect */
    CHECK(!endpoint_live(eb));                            /* the loop stopped before the third entry */
    srv_down();
}
TEST(gap7_useprotseqif_ex_rejects_short_policy_length)
{
    RPC_POLICY pol; char ep[64]; static const unsigned lens[] = { 0, 4, 8, 11 };
    unsigned i;
    REQUIRE(U(RpcServerUseProtseqIfExW)); REQUIRE(U(RpcServerUseProtseqIfExA)); REQUIRE(U(RpcServerUseAllProtseqsIfEx)); WINE_SKIP_IF();
    srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
    for (i = 0; i < NELEM(lens); i++) {
        memset(&pol, 0, sizeof pol); pol.Length = lens[i];
        EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL, &pol), S_INVALID_BOUND);
        EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL, &pol), S_INVALID_BOUND);
        EQ(U(RpcServerUseAllProtseqsIfEx)(10, (RPC_IF_HANDLE)&g_si, NULL, &pol), S_INVALID_BOUND);
    }
    /* with an empty table the length is still validated first */
    table_apply(0); pol.Length = 8;
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL, &pol), S_INVALID_BOUND);
    pol.Length = sizeof pol;
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL, &pol), S_PROTSEQ_UNSUP);
    EQ(U(RpcServerUseAllProtseqsIfEx)(10, (RPC_IF_HANDLE)&g_si, NULL, &pol), S_NO_PROTSEQS);
}
TEST(gap7_useprotseqif_ex_accepts_policy_longer_than_minimum)
{
    struct { RPC_POLICY p; DWORD extra[4]; } pol; char ep[64];
    REQUIRE(U(RpcServerUseProtseqIfExW)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    srv_reset(); new_ep(ep, NULL); table_set(0, "ncalrpc", ep); table_apply(1);
    memset(&pol, 0, sizeof pol); pol.p.Length = sizeof pol;
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&g_si, NULL, &pol.p), RPC_S_OK);
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(endpoint_live(ep));
    srv_down();
}
TEST(gap7_useprotseq_a_family_rejects_strings_over_65535_characters)
{
    static char big[0x10000 + 8];
    RPC_POLICY pol;
    REQUIRE(U(RpcServerUseProtseqA)); REQUIRE(U(RpcServerUseProtseqIfA)); WINE_SKIP_IF();
    memset(big, 'n', 0x10000); big[0x10000] = 0;
    memset(&pol, 0, sizeof pol); pol.Length = sizeof pol;
    table_apply(0);
    EQ(U(RpcServerUseProtseqA)((RPC_CSTR)big, 10, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RpcServerUseProtseqIfA)((RPC_CSTR)big, 10, (RPC_IF_HANDLE)&g_si, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)big, 10, (RPC_IF_HANDLE)&g_si, NULL, &pol), ERROR_INVALID_PARAMETER);
    big[0xffff] = 0;       /* 65535 characters pass the length check and fail later as an unknown protocol sequence */
    {
        RPC_STATUS st = U(RpcServerUseProtseqIfA)((RPC_CSTR)big, 10, (RPC_IF_HANDLE)&g_si, NULL);
        EQ(st, S_PROTSEQ_UNSUP);
    }
}
TEST(gap7_useallprotseqsif_registers_supported_entries_and_ignores_unsupported)
{
    char ea[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerUseAllProtseqsIf)); REQUIRE(U(NdrClientCall2)); WINE_SKIP_IF();
    srv_reset(); new_ep(ea, NULL);
    table_set(0, "olet_bogus_protseq", "x1"); table_set(1, "ncalrpc", ea); table_apply(2);
    st = U(RpcServerUseAllProtseqsIf)(10, (RPC_IF_HANDLE)&g_si, NULL);
    EQ(st, RPC_S_OK);                                     /* one entry succeeded: the unsupported one does not matter */
    EQ(register_and_listen(), RPC_S_OK);
    CHECK(endpoint_live(ea));
    srv_down();
}
TEST(gap7_useallprotseqsif_no_usable_entry_is_no_protseqs)
{
    REQUIRE(U(RpcServerUseAllProtseqsIf)); WINE_SKIP_IF();
    table_set(0, "olet_bogus_protseq", "x1"); table_set(1, "olet_other_bogus", "x2"); table_apply(2);
    EQ(U(RpcServerUseAllProtseqsIf)(10, (RPC_IF_HANDLE)&g_si, NULL), S_NO_PROTSEQS);
    table_apply(0);
    EQ(U(RpcServerUseAllProtseqsIf)(10, (RPC_IF_HANDLE)&g_si, NULL), S_NO_PROTSEQS);
}

/* ======================= RpcServerUseProtseqA with a dynamic endpoint ======================= */
TEST(gap7_useprotseqa_dynamic_ncalrpc_endpoint_appears_in_server_bindings)
{
    RPC_BINDING_VECTOR *v = NULL; RPC_STATUS st; unsigned i, found = 0;
    REQUIRE(U(RpcServerUseProtseqA)); REQUIRE(U(RpcServerInqBindings)); REQUIRE(U(RpcBindingToStringBindingA));
    REQUIRE(U(RpcStringBindingParseA)); REQUIRE(U(RpcBindingVectorFree));
    srv_reset();
    st = U(RpcServerUseProtseqA)((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, NULL);
    EQ(st, RPC_S_OK);
    st = U(RpcServerInqBindings)(&v);
    EQ(st, RPC_S_OK);
    for (i = 0; st == RPC_S_OK && v && i < v->Count; i++) {
        RPC_CSTR sb = NULL, proto = NULL, ep = NULL;
        if (U(RpcBindingToStringBindingA)(v->BindingH[i], &sb) != RPC_S_OK) continue;
        if (U(RpcStringBindingParseA)(sb, NULL, &proto, NULL, &ep, NULL) == RPC_S_OK) {
            if (proto && !lstrcmpiA((char *)proto, "ncalrpc") && ep && *ep) found++;
            UIN(RT, RpcStringFreeA)(&proto); UIN(RT, RpcStringFreeA)(&ep);
        }
        UIN(RT, RpcStringFreeA)(&sb);
    }
    CHECKF(found >= 1, "no ncalrpc binding with an endpoint among %lu bindings", v ? (unsigned long)v->Count : 0ul);
    if (v) U(RpcBindingVectorFree)(&v);
}

/* ======================= RpcCertGeneratePrincipalName{A,W} ======================= */
TEST(gap7_certgenerateprincipalname_null_context_fails_consistently)
{
    RPC_STATUS w, a; RPC_WSTR ws = (RPC_WSTR)(ULONG_PTR)0x77; RPC_CSTR as = (RPC_CSTR)(ULONG_PTR)0x77; static const DWORD flags[] = { 0, 1, 0xffffffffu };
    unsigned i;
    REQUIRE(UN(RpcCertGeneratePrincipalNameW)); REQUIRE(UN(RpcCertGeneratePrincipalNameA));
    if (t_is_wine()) { t_skip("wine returns a different status for a NULL certificate context"); return; }
    for (i = 0; i < NELEM(flags); i++) {
        ws = (RPC_WSTR)(ULONG_PTR)0x77; as = (RPC_CSTR)(ULONG_PTR)0x77;
        w = UN(RpcCertGeneratePrincipalNameW)(NULL, flags[i], &ws);
        a = UN(RpcCertGeneratePrincipalNameA)(NULL, flags[i], &as);
        CHECKF(w != 0, "W succeeded for a NULL certificate context (flags 0x%lx)", (unsigned long)flags[i]);
        CHECKF(a == w, "A status %lu vs W status %lu", (unsigned long)a, (unsigned long)w);
        CHECK(as == (RPC_CSTR)(ULONG_PTR)0x77);          /* the A wrapper writes its output only on success */
        t_note("RpcCertGeneratePrincipalNameW(NULL, 0x%lx) -> %lu (1764 = RPC_S_CANNOT_SUPPORT when the API set is absent)", (unsigned long)flags[i], (unsigned long)w);
    }
}
