/* Cross-process ncalrpc: the server (or the client) lives in a child process of this exe (CHILD "rpcsrv" in rpcx.c),
 * plus the async-handle API surface.  Every child is waited for <= 10 s and killed on timeout. */
#include "rawfx.h"

typedef struct { WCHAR ep[64]; HANDLE proc; PROCESS_INFORMATION pi; RPC_BINDING_HANDLE h; int up; } child_srv;

static int cs_start(child_srv *c, const char *mode) {
    memset(c, 0, sizeof *c);
    rx_ep_unique(c->ep, 64);
    c->proc = rx_spawn_server(c->ep, mode, &c->pi);
    if (!c->proc) return 0;
    c->up = rx_wait_server_ready(c->ep, 10000);
    c->h = rx_bind(c->ep);
    return c->up && c->h;
}
/* ask the child to stop through the app-level p_stop procedure and wait for it; returns exit code or 0xdead0001 */
static DWORD cs_stop(child_srv *c) {
    DWORD code = 0xdead0001;
    if (!c->proc) return code;
    if (c->up && c->h) rx_ndr_h(c->h, RX_STOP, 0, 0, 0, 0, NULL);
    if (WaitForSingleObject(c->proc, 10000) != WAIT_OBJECT_0) { TerminateProcess(c->proc, 1); WaitForSingleObject(c->proc, 5000); }
    else GetExitCodeProcess(c->proc, &code);
    CloseHandle(c->pi.hThread); CloseHandle(c->proc); c->proc = NULL;
    rx_unbind(c->h); c->h = NULL;
    return code;
}
static void cs_kill(child_srv *c) {
    if (!c->proc) return;
    TerminateProcess(c->proc, 1); WaitForSingleObject(c->proc, 5000);
    CloseHandle(c->pi.hThread); CloseHandle(c->proc); c->proc = NULL;
    rx_unbind(c->h); c->h = NULL;
}
#define CUP(c, mode) do { if (!cs_start(&c, mode)) { CHECKF(0, "child server did not come up"); cs_kill(&c); return; } } while (0)

TEST(ncalrpc_xproc_basic_calls) {
    child_srv c; int r; DWORD e;
    CUP(c, "plain");
    e = rx_add(c.h, 19, 23, &r); EQ(e, 0); EQ(r, 42);
    e = rx_add(c.h, -1, -2, &r); EQ(e, 0); EQ(r, -3);
    EQ(cs_stop(&c), 0);              /* RpcServerListen returned in the child after the remote p_stop */
}
TEST(ncalrpc_xproc_marshalling) {
    child_srv c; LONG_PTR r; DWORD e; rx_S s = { 1, 2, 4.0 }; static int a[50000]; int i; WCHAR out[32];
    CUP(c, "plain");
    e = rx_ndr_h(c.h, RX_WLEN, (ULONG_PTR)L"cross process", 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 13);
    for (i = 0; i < 50000; i++) a[i] = i & 0xff;
    e = rx_ndr_h(c.h, RX_SUM, 50000, (ULONG_PTR)a, 0, 0, &r); EQ(e, 0);
    { long long w = 0; for (i = 0; i < 50000; i++) w += a[i]; EQ((int)r, (int)w); }
    memset(a, 0, sizeof a);
    e = rx_ndr_h(c.h, RX_SEQ, 50000, (ULONG_PTR)a, 0, 0, NULL); EQ(e, 0); EQ(a[49999], 49999 * 49999); EQ(a[7], 49);
    e = rx_ndr_h(c.h, RX_BUMP, (ULONG_PTR)&s, 0, 0, 0, NULL); EQ(e, 0); EQ(s.a, 2); EQ(s.b, 4); CHECK(s.c == 8.0);
    e = rx_ndr_h(c.h, RX_ECHO, (ULONG_PTR)L"echo me", 32, (ULONG_PTR)out, 0, NULL); EQ(e, 0); CHECK(!lstrcmpW(out, L"echo me"));
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_server_pid_not_client_pid) {
    /* the child's counter lives in the child, not here */
    child_srv c; LONG_PTR r = 0; LONG mine;
    CUP(c, "plain");
    mine = rx_calls;
    rx_ndr_h(c.h, RX_VOID, 0, 0, 0, 0, NULL);
    rx_ndr_h(c.h, RX_VOID, 0, 0, 0, 0, NULL);
    EQ(rx_calls, mine);                                    /* local counter untouched */
    rx_ndr_h(c.h, RX_COUNTER, 0, 0, 0, 0, &r);
    CHECK((int)r >= 3);                                    /* ready-probe call + 2 above */
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_call_attributes_seen_by_child) {
    child_srv c; int v[4] = { -7, -7, -7, -7 }; DWORD e;
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    CUP(c, "plain");
    e = rx_ndr_h(c.h, RX_INFO, (ULONG_PTR)&v[0], (ULONG_PTR)&v[1], (ULONG_PTR)&v[2], (ULONG_PTR)&v[3], NULL);
    EQ(e, 0);
    EQ(v[0], GetCurrentProcessId());                       /* ClientPID as the child's RPC runtime sees it */
    EQ(v[2], 1);                                           /* RpcCallClientLocal */
    t_note("child sees authn level %d, authn service %d for an unauthenticated call", v[1], v[3]);
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_authenticated_call_attributes) {
    child_srv c; int v[4] = { -7, -7, -7, -7 }; DWORD e; RPC_BINDING_HANDLE h;
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    CUP(c, "winnt");
    h = rx_bind(c.ep);
    EQ(U(RpcBindingSetAuthInfoW)(h, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE), RPC_S_OK);
    e = rx_ndr_h(h, RX_INFO, (ULONG_PTR)&v[0], (ULONG_PTR)&v[1], (ULONG_PTR)&v[2], (ULONG_PTR)&v[3], NULL);
    EQ(e, 0);
    EQ(v[0], GetCurrentProcessId()); EQ(v[1], RPC_C_AUTHN_LEVEL_PKT_PRIVACY); EQ(v[3], RPC_C_AUTHN_WINNT); EQ(v[2], 1);
    rx_unbind(h);
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_security_callback_denies) {
    child_srv c; int r = 0; DWORD e;
    CUP(c, "deny");              /* child: callback returns RPC_S_ACCESS_DENIED; the readiness probe is rejected too */
    (void)r;
    e = rx_add(c.h, 1, 2, &r);
    EQ(e, RPC_S_ACCESS_DENIED);
    cs_kill(&c);
}
TEST(ncalrpc_xproc_remote_stop_via_rpc) {
    child_srv c; DWORD code;
    CUP(c, "plain");
    code = cs_stop(&c);
    EQ(code, 0);
}
TEST(ncalrpc_xproc_child_raw_interface_dispatch_errors) {
    child_srv c; rx_raw r; UUID u; ULONG n; RPC_STATUS st; char in[64], out[64]; int i;
    CUP(c, "plain");
    rx_raw_uuid(&u, 1); rx_raw_init(&r, &u, 1, 0);            /* uuid #1 is the child's raw interface */
    for (i = 0; i < 64; i++) in[i] = (char)i;
    st = rx_raw_call(c.h, &r, 0, in, 64, out, 64, &n);      EQ(st, RPC_S_OK); EQ(n, 64); CHECK(!memcmp(in, out, 64));
    if (!t_is_wine()) {   /* wine leaks the in-flight call on this error, which would block the child's shutdown */
        st = rx_raw_call(c.h, &r, 13, NULL, 0, NULL, 0, &n);    EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);   /* table has 13 entries */
        st = rx_raw_call(c.h, &r, 500, NULL, 0, NULL, 0, &n);   EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);
    }
    st = rx_raw_call(c.h, &r, 0, in, 8, out, 8, &n);        EQ(st, RPC_S_OK);                    /* binding survives the errors */
    rx_raw_init(&r, &u, 2, 0);
    st = rx_raw_call(c.h, &r, 0, in, 8, out, 8, &n);        EQ(st, RPC_S_UNKNOWN_IF);            /* wrong major version */
    rx_raw_uuid(&u, 3); rx_raw_init(&r, &u, 1, 0);
    st = rx_raw_call(c.h, &r, 0, in, 8, out, 8, &n);        EQ(st, RPC_S_UNKNOWN_IF);            /* unregistered uuid */
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_raw_fault_from_child) {
    child_srv c; rx_raw r; UUID u; ULONG n; RPC_STATUS st; int code = ERROR_ACCESS_DENIED;
    CUP(c, "plain");
    rx_raw_uuid(&u, 1); rx_raw_init(&r, &u, 1, 0);
    st = rx_raw_call(c.h, &r, 1, &code, 4, NULL, 0, &n); EQ(st, ERROR_ACCESS_DENIED);
    code = 1234;
    st = rx_raw_call(c.h, &r, 1, &code, 4, NULL, 0, &n); EQ(st, 1234);
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_large_raw_echo) {
    child_srv c; rx_raw r; UUID u; ULONG n = 0; RPC_STATUS st; unsigned char *in, *out; ULONG i, sz = 4 << 20;
    CUP(c, "plain");
    rx_raw_uuid(&u, 1); rx_raw_init(&r, &u, 1, 0);
    in = HeapAlloc(GetProcessHeap(), 0, sz); out = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz);
    for (i = 0; i < sz; i++) in[i] = (unsigned char)(i * 31 + (i >> 8));
    st = rx_raw_call(c.h, &r, 0, in, sz, out, sz, &n);
    EQ(st, RPC_S_OK); EQ(n, sz);
    CHECK(!memcmp(in, out, sz));
    HeapFree(GetProcessHeap(), 0, in); HeapFree(GetProcessHeap(), 0, out);
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_server_killed_mid_call) {
    child_srv c; typedef struct { RPC_BINDING_HANDLE h; DWORD e; } job; job j; HANDLE th; DWORD t0;
    extern DWORD WINAPI ncalrpc_xproc_sleeper(void *);
    CUP(c, "plain");
    j.h = c.h; j.e = 0xdead;
    th = rx_thread(ncalrpc_xproc_sleeper, &j);
    Sleep(500);                                   /* the call is blocked in the child's Sleep(8000) */
    TerminateProcess(c.proc, 3);
    CHECK(rx_join(th, 10000));
    CHECK(j.e == RPC_S_CALL_FAILED || j.e == RPC_S_CALL_FAILED_DNE || j.e == RPC_S_SERVER_UNAVAILABLE);
    t_note("call interrupted by server process death -> %lu", j.e);
    cs_kill(&c);
}
DWORD WINAPI ncalrpc_xproc_sleeper(void *p) {
    struct { RPC_BINDING_HANDLE h; DWORD e; } *j = p;
    j->e = rx_ndr_h(j->h, RX_SLEEP, 8000, 0, 0, 0, NULL);
    return 0;
}
TEST(ncalrpc_xproc_server_gone_then_unavailable) {
    child_srv c; int r; DWORD e;
    CUP(c, "plain");
    e = rx_add(c.h, 1, 1, &r); EQ(e, 0);
    cs_kill(&c);
    {   RPC_BINDING_HANDLE h = rx_bind(c.ep);
        e = rx_add(h, 1, 1, &r);
        EQ(e, RPC_S_SERVER_UNAVAILABLE);
        rx_unbind(h); }
}
TEST(ncalrpc_xproc_two_servers_independent) {
    child_srv a, b; int r; DWORD e;
    CUP(a, "plain");
    if (!cs_start(&b, "plain")) { CHECKF(0, "second child"); cs_kill(&b); cs_kill(&a); return; }
    e = rx_add(a.h, 1, 2, &r); EQ(e, 0); EQ(r, 3);
    e = rx_add(b.h, 10, 20, &r); EQ(e, 0); EQ(r, 30);
    EQ(cs_stop(&a), 0);
    e = rx_add(b.h, 5, 5, &r); EQ(e, 0); EQ(r, 10);          /* b unaffected by a's shutdown */
    EQ(cs_stop(&b), 0);
}

/* ---- context handle rundown when the client process dies ------------------ */
CHILD(rpcctxclient) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; void *ctx = NULL; DWORD e;
    MultiByteToWideChar(CP_ACP, 0, argc > 1 ? argv[1] : "x", -1, ep, 64);
    h = rx_bind(ep);
    if (!h) return 11;
    e = rx_ndr_h(h, RX_CTX_OPEN, (ULONG_PTR)&ctx, 99, 0, 0, NULL);
    if (e || !ctx) return 12;
    ExitProcess(0);                               /* exit with the context still open: no close, no RpcBindingFree */
    return 0;
}
TEST(ncalrpc_xproc_context_rundown_on_client_exit) {
    rx_fx f; char epa[64]; LONG rd0; DWORD code, t0;
    if (!rx_fx_up(&f)) { CHECKF(0, "server"); rx_fx_down(&f); return; }
    WideCharToMultiByte(CP_ACP, 0, f.ep, -1, epa, sizeof epa, NULL, NULL);
    rd0 = rx_rundowns;
    code = t_run_child("rpcctxclient", epa, 15000);
    EQ(code, 0);
    for (t0 = GetTickCount(); rx_rundowns == rd0 && GetTickCount() - t0 < 10000; ) Sleep(20);
    EQ(rx_rundowns, rd0 + 1);                     /* the runtime ran the rundown routine once the client died */
    rx_fx_down(&f);
}
TEST(ncalrpc_xproc_context_rundown_after_binding_free) {
    /* closing the (only) binding while a context handle is open: the association is torn down and the runtime
       eventually runs the rundown routine.  Timing is runtime-defined, so this is bounded and reported. */
    rx_fx f; void *ctx = NULL; DWORD e, t0; LONG rd0;
    if (!rx_fx_up(&f)) { CHECKF(0, "server"); rx_fx_down(&f); return; }
    rd0 = rx_rundowns;
    e = rx_ndr_h(f.h, RX_CTX_OPEN, (ULONG_PTR)&ctx, 5, 0, 0, NULL); EQ(e, 0); CHECK(ctx != NULL);
    rx_unbind(f.h); f.h = NULL;
    for (t0 = GetTickCount(); rx_rundowns == rd0 && GetTickCount() - t0 < 5000; ) Sleep(20);
    t_note("rundown after RpcBindingFree with open context within 5 s: %s", rx_rundowns > rd0 ? "yes" : "no");
    rx_fx_down(&f);
}

/* ---- remote management interface ------------------------------------------ */
TEST(ncalrpc_xproc_mgmt_is_listening_remote) {
    child_srv c; RPC_STATUS st;
    CUP(c, "plain");
    st = U(RpcMgmtIsServerListening)(c.h);
    CHECK(st == RPC_S_OK || st == RPC_S_ACCESS_DENIED);
    t_note("RpcMgmtIsServerListening(remote ncalrpc binding) -> %lu", (unsigned long)st);
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_mgmt_inq_ifids_remote) {
    child_srv c; RPC_IF_ID_VECTOR *v = NULL; RPC_STATUS st; unsigned i; int found = 0;
    if (t_is_wine()) { t_skip("RpcIfIdVectorFree unimplemented in wine"); return; }
    CUP(c, "plain");
    st = U(RpcMgmtInqIfIds)(c.h, &v);
    t_note("RpcMgmtInqIfIds(remote) -> %lu", (unsigned long)st);
    if (st == RPC_S_OK && v) {
        for (i = 0; i < v->Count; i++) if (!memcmp(&v->IfId[i]->Uuid, &rx_xif_uuid, sizeof(UUID))) found = 1;
        EQ(found, 1);
        U(RpcIfIdVectorFree)(&v);
    }
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_mgmt_inq_stats_remote) {
    child_srv c; RPC_STATS_VECTOR *s = NULL; RPC_STATUS st;
    CUP(c, "plain");
    st = U(RpcMgmtInqStats)(c.h, &s);
    t_note("RpcMgmtInqStats(remote) -> %lu", (unsigned long)st);
    if (st == RPC_S_OK && s) { CHECK(s->Count >= 4); U(RpcMgmtStatsVectorFree)(&s); }
    EQ(cs_stop(&c), 0);
}
TEST(ncalrpc_xproc_mgmt_stop_remote_default_policy) {
    child_srv c; RPC_STATUS st; DWORD code = 0xdead0001;
    CUP(c, "plain");
    st = U(RpcMgmtStopServerListening)(c.h);
    t_note("RpcMgmtStopServerListening(remote ncalrpc binding, default authorization) -> %lu", (unsigned long)st);
    if (st == RPC_S_OK) {
        if (WaitForSingleObject(c.proc, 10000) == WAIT_OBJECT_0) GetExitCodeProcess(c.proc, &code);
        EQ(code, 0);
        CloseHandle(c.pi.hThread); CloseHandle(c.proc); c.proc = NULL; rx_unbind(c.h); c.h = NULL;
    } else cs_stop(&c);
}

/* ---- async handle API ----------------------------------------------------- */
TEST(ncalrpc_async_initialize_handle_sizes) {
    RPC_ASYNC_STATE a; char big[512]; RPC_STATUS st;
    REQUIRE(U(RpcAsyncInitializeHandle));
    st = U(RpcAsyncInitializeHandle)((PRPC_ASYNC_STATE)big, sizeof big);       EQ(st, ERROR_INVALID_PARAMETER);
    st = U(RpcAsyncInitializeHandle)(&a, sizeof a - 1);                        EQ(st, ERROR_INVALID_PARAMETER);
    st = U(RpcAsyncInitializeHandle)(&a, 0);                                   EQ(st, ERROR_INVALID_PARAMETER);
    st = U(RpcAsyncInitializeHandle)(&a, sizeof a);                            EQ(st, RPC_S_OK);
}
TEST(ncalrpc_async_initialize_handle_fields) {
    RPC_ASYNC_STATE a; void *unset; int i;
    REQUIRE(U(RpcAsyncInitializeHandle));
    memset(&a, 0xcc, sizeof a); memset(&unset, 0xcc, sizeof unset);
    EQ(U(RpcAsyncInitializeHandle)(&a, sizeof a), RPC_S_OK);
    EQ(a.Size, sizeof a);
    EQ(a.Signature, 0x43595341);                   /* 'ASYC' */
    EQ(a.Lock, 0); EQ(a.Flags, 0);
    CHECK(a.StubInfo == NULL); CHECK(a.RuntimeInfo == NULL);
    CHECK(a.UserInfo == unset);                    /* not touched */
    EQ(a.Event, 0xcccccccc); EQ(a.NotificationType, 0xcccccccc);
    for (i = 0; i < 4; i++) EQ(a.Reserved[i], 0);
}
TEST(ncalrpc_async_initialize_handle_reinit_idempotent) {
    RPC_ASYNC_STATE a;
    REQUIRE(U(RpcAsyncInitializeHandle));
    EQ(U(RpcAsyncInitializeHandle)(&a, sizeof a), RPC_S_OK);
    a.UserInfo = (void *)(ULONG_PTR)0x1234; a.NotificationType = RpcNotificationTypeEvent;
    EQ(U(RpcAsyncInitializeHandle)(&a, sizeof a), RPC_S_OK);
    EQ(a.Signature, 0x43595341); EQ(a.Size, sizeof a);
}
TEST(ncalrpc_async_get_call_status_no_call) {
    RPC_ASYNC_STATE a; RPC_STATUS st;
    REQUIRE(U(RpcAsyncGetCallStatus));
    EQ(U(RpcAsyncInitializeHandle)(&a, sizeof a), RPC_S_OK);
    st = U(RpcAsyncGetCallStatus)(&a);
    EQ(st, RPC_S_INVALID_BINDING);                 /* an initialized handle with no call attached */
    memset(&a, 0, sizeof a);
    st = U(RpcAsyncGetCallStatus)(&a);
    EQ(st, RPC_S_INVALID_BINDING);
}
TEST(ncalrpc_async_complete_cancel_abort_no_call) {
    RPC_ASYNC_STATE a; RPC_STATUS st; int reply = 0;
    REQUIRE(U(RpcAsyncCompleteCall)); REQUIRE(U(RpcAsyncCancelCall)); REQUIRE(U(RpcAsyncAbortCall));
    if (t_is_wine()) { t_skip("wine does not validate async handles without a call"); return; }
    EQ(U(RpcAsyncInitializeHandle)(&a, sizeof a), RPC_S_OK);
    st = U(RpcAsyncCompleteCall)(&a, &reply);
    t_note("RpcAsyncCompleteCall on a handle without call -> %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    st = U(RpcAsyncCancelCall)(&a, FALSE);
    t_note("RpcAsyncCancelCall on a handle without call -> %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    st = U(RpcAsyncAbortCall)(&a, 1234);
    t_note("RpcAsyncAbortCall on a handle without call -> %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
}
TEST(ncalrpc_async_notification_fields_roundtrip) {
    RPC_ASYNC_STATE a; HANDLE ev;
    REQUIRE(U(RpcAsyncInitializeHandle));
    EQ(U(RpcAsyncInitializeHandle)(&a, sizeof a), RPC_S_OK);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    a.NotificationType = RpcNotificationTypeEvent; a.u.hEvent = ev; a.UserInfo = (void *)(ULONG_PTR)0xabc;
    CHECK(a.u.hEvent == ev); EQ(a.NotificationType, RpcNotificationTypeEvent);
    EQ(a.Size, sizeof a); EQ(a.Signature, 0x43595341);      /* the user may fill the notification part without disturbing the header */
    CloseHandle(ev);
}
