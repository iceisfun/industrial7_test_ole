/* Additional rpcrt4 server/client tests over ncalrpc: registration edge cases, option calls, in-flight
 * unregister, interface id enumeration.  Fixtures from rpcx.h / rawfx.h. */
#include "rawfx.h"

UDECL(RPC_STATUS, RpcBindingSetOption, (RPC_BINDING_HANDLE, unsigned long, ULONG_PTR));
UDECL(RPC_STATUS, RpcBindingInqOption, (RPC_BINDING_HANDLE, unsigned long, ULONG_PTR *));

/* Decomp (RPC_SERVER::UseRpcProtocolSequence): the address stays in the server's address list after the
 * server stops listening, so registering the same protseq/endpoint again still reports RPC_S_DUPLICATE_ENDPOINT. */
TEST(ncalrpc_extra_dup_endpoint_after_listen_stop) {
    WCHAR ep[64];
    REQUIRE(U(RpcServerUseProtseqEpW));
    if (t_is_wine()) { t_skip("wine accepts duplicate endpoints and hangs with many live endpoints"); return; }
    rx_server_reset(); rx_ep_unique(ep, 64);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)(rx_xif_s_ifspec, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    rx_server_down(rx_xif_s_ifspec, NULL);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL), RPC_S_DUPLICATE_ENDPOINT);
}
TEST(ncalrpc_extra_dup_endpoint_ansi_wide_same_name) {
    WCHAR ep[64]; char epa[64];
    REQUIRE(U(RpcServerUseProtseqEpA));
    if (t_is_wine()) { t_skip("wine accepts duplicate endpoints and hangs with many live endpoints"); return; }
    rx_server_reset(); rx_ep_unique(ep, 64);
    WideCharToMultiByte(CP_ACP, 0, ep, -1, epa, sizeof epa, NULL, NULL);
    EQ(U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", 10, (RPC_CSTR)epa, NULL), RPC_S_OK);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL), RPC_S_DUPLICATE_ENDPOINT);
    EQ(U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", 10, (RPC_CSTR)epa, NULL), RPC_S_DUPLICATE_ENDPOINT);
}
TEST(ncalrpc_extra_ansi_endpoint_is_callable) {
    WCHAR ep[64]; char epa[64]; RPC_BINDING_HANDLE h; DWORD e; LONG_PTR r = -1;
    REQUIRE(U(RpcServerUseProtseqEpA));
    { WCHAR warm[64]; rx_ep(warm, 64); EQ(rx_serve(rx_xif_s_ifspec, warm, NULL, NULL), RPC_S_OK); rx_server_down(rx_xif_s_ifspec, NULL); }  /* initialises the stub dispatch table */
    rx_server_reset(); rx_ep_unique(ep, 64);
    WideCharToMultiByte(CP_ACP, 0, ep, -1, epa, sizeof epa, NULL, NULL);
    EQ(U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", 10, (RPC_CSTR)epa, NULL), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)(rx_xif_s_ifspec, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(ep); CHECK(h != NULL);
    e = rx_ndr_h(h, RX_ADD, 30, 12, 0, 0, &r);
    EQ(e, 0); EQ((int)r, 42);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}

/* ---- in-flight calls vs RpcServerUnregisterIf(WaitForCallsToComplete=0) ---------------------------- */
typedef struct { RPC_BINDING_HANDLE h; int ms; DWORD exc; } xjob;
static DWORD WINAPI xsleeper(void *p) { xjob *j = p; j->exc = rx_ndr_h(j->h, RX_SLEEP, j->ms, 0, 0, 0, NULL); return j->exc; }
TEST(ncalrpc_extra_unregister_nowait_does_not_block_on_call) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; xjob j; HANDLE th; DWORD t0; LONG started0, done0; RPC_STATUS st;
    REQUIRE(U(RpcServerUnregisterIf));
    rx_ep(ep, 64); EQ(rx_serve(rx_xif_s_ifspec, ep, NULL, NULL), RPC_S_OK);
    h = rx_bind(ep);
    j.h = h; j.ms = 2000; j.exc = 0xdead; started0 = rx_sleep_started; done0 = rx_sleep_done;
    th = rx_thread(xsleeper, &j);
    for (t0 = GetTickCount(); rx_sleep_started == started0 && GetTickCount() - t0 < 5000; ) Sleep(5);
    st = U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 0);       /* WaitForCallsToComplete = FALSE */
    EQ(st, RPC_S_OK);
    EQ(rx_sleep_done, done0);                     /* the 2 s call is still running when UnregisterIf returned */
    CHECK(rx_join(th, 10000));
    EQ(j.exc, 0);                                 /* an already dispatched call is allowed to finish */
    EQ(rx_sleep_done, done0 + 1);
    rx_unbind(h); rx_server_down(NULL, NULL);
}

/* ---- manager types on unregister ------------------------------------------------------------------ */
TEST(ncalrpc_extra_unregister_wrong_mgrtype) {
    rx_raw r; UUID u, mt, other; static void *epv[1]; RPC_STATUS st;
    REQUIRE(U(RpcServerRegisterIf));
    rx_server_reset(); rx_raw_uuid(&u, 0xc0); rx_raw_uuid(&mt, 0xc1); rx_raw_uuid(&other, 0xc2); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, &mt, (RPC_MGR_EPV *)epv), RPC_S_OK);
    st = U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, &other, 1);
    t_note("UnregisterIf with a manager type that was not registered -> %lu", (unsigned long)st);
    CHECK(st == RPC_S_UNKNOWN_MGR_TYPE || st == RPC_S_UNKNOWN_IF);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, &mt, 1), RPC_S_OK);
}
TEST(ncalrpc_extra_register_same_mgrtype_twice) {
    rx_raw r; UUID u, mt; static void *e1[1], *e2[1]; RPC_STATUS st;
    REQUIRE(U(RpcServerRegisterIf));
    rx_server_reset(); rx_raw_uuid(&u, 0xc3); rx_raw_uuid(&mt, 0xc4); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, &mt, (RPC_MGR_EPV *)e1), RPC_S_OK);
    st = U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, &mt, (RPC_MGR_EPV *)e2);
    t_note("second registration of the same (interface, manager type) -> %lu", (unsigned long)st);
    CHECK(st == RPC_S_OK || st == RPC_S_ALREADY_REGISTERED);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, &mt, 1), RPC_S_OK);
}

/* ---- RpcMgmtInqIfIds with several interfaces ------------------------------------------------------ */
TEST(ncalrpc_extra_inqifids_lists_all_registered) {
    rx_raw a, b; UUID ua, ub; WCHAR ep[64]; RPC_IF_ID_VECTOR *v = NULL; unsigned i; int fa = 0, fb = 0; RPC_STATUS st;
    REQUIRE(U(RpcMgmtInqIfIds)); REQUIRE(U(RpcIfIdVectorFree));
    if (t_is_wine()) { t_skip("RpcIfIdVectorFree unimplemented in wine"); return; }
    rx_server_reset(); rx_ep(ep, 64);
    rx_raw_uuid(&ua, 0xc5); rx_raw_uuid(&ub, 0xc6); rx_raw_init(&a, &ua, 2, 3); rx_raw_init(&b, &ub, 4, 5);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&a.si, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&b.si, NULL, NULL), RPC_S_OK);
    st = U(RpcMgmtInqIfIds)(NULL, &v);
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK && v) {
        for (i = 0; i < v->Count; i++) {
            RPC_IF_ID *id = v->IfId[i];
            if (!memcmp(&id->Uuid, &ua, sizeof ua)) { fa++; EQ(id->VersMajor, 2); EQ(id->VersMinor, 3); }
            if (!memcmp(&id->Uuid, &ub, sizeof ub)) { fb++; EQ(id->VersMajor, 4); EQ(id->VersMinor, 5); }
        }
        EQ(fa, 1); EQ(fb, 1);
        EQ(U(RpcIfIdVectorFree)(&v), RPC_S_OK);
    }
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&a.si, NULL, 1), RPC_S_OK);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&b.si, NULL, 1), RPC_S_OK);
}

/* ---- RegisterIfEx flag combinations -------------------------------------------------------------- */
TEST(ncalrpc_extra_registerifex_flag_combo_accepted) {
    rawfx f; ULONG n;
    RUPX(f, 0xc7, 1, 0, RPC_IF_ALLOW_LOCAL_ONLY | RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH | RPC_IF_SEC_NO_CACHE, rx_sec_cb);
    rx_cb_result = RPC_S_OK;
    EQ(rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n), RPC_S_OK);
    raw_down(&f);
}
TEST(ncalrpc_extra_registerifex_max_calls_one_serves_sequential_calls) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; int i; DWORD e; LONG_PTR r;
    REQUIRE(U(RpcServerRegisterIfEx));
    rx_ep(ep, 64);
    EQ(rx_serve_ex(rx_xif_s_ifspec, ep, NULL, NULL, 0, 1, NULL), RPC_S_OK);
    h = rx_bind(ep);
    for (i = 0; i < 20; i++) { r = -1; e = rx_ndr_h(h, RX_ADD, i, 1, 0, 0, &r); EQ(e, 0); EQ((int)r, i + 1); }
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}

/* ---- RpcBindingSetOption / RpcBindingInqOption ---------------------------------------------------- */
#define OPT_COM_TIMEOUT 0
TEST(ncalrpc_extra_binding_option_roundtrip) {
    rawfx f; ULONG_PTR v = 0; RPC_STATUS st;
    REQUIRE(UN(RpcBindingSetOption)); REQUIRE(UN(RpcBindingInqOption));
    if (t_is_wine()) { t_skip("RpcBindingInqOption unimplemented in wine"); return; }
    RUP(f, 0xc8, 1, 0);
    st = UN(RpcBindingSetOption)(f.h, 12 /* RPC_C_OPT_CALL_TIMEOUT */, 30000);
    t_note("RpcBindingSetOption(CALL_TIMEOUT, 30000) -> %lu", (unsigned long)st);
    if (st == RPC_S_OK) {
        st = UN(RpcBindingInqOption)(f.h, 12, &v);
        EQ(st, RPC_S_OK); EQ(v, 30000);
    }
    raw_down(&f);
}
TEST(ncalrpc_extra_binding_option_null_binding) {
    ULONG_PTR v = 0;
    REQUIRE(UN(RpcBindingSetOption)); REQUIRE(UN(RpcBindingInqOption));
    if (t_is_wine()) { t_skip("wine dereferences NULL binding"); return; }
    EQ(UN(RpcBindingSetOption)(NULL, 12, 1000), RPC_S_INVALID_BINDING);
    EQ(UN(RpcBindingInqOption)(NULL, 12, &v), RPC_S_INVALID_BINDING);
}
TEST(ncalrpc_extra_call_timeout_option_effect) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e, t0, el; RPC_STATUS st;
    REQUIRE(UN(RpcBindingSetOption)); REQUIRE(U(RpcServerRegisterIf));
    rx_ep(ep, 64); EQ(rx_serve(rx_xif_s_ifspec, ep, NULL, NULL), RPC_S_OK);
    h = rx_bind(ep);
    st = UN(RpcBindingSetOption)(h, 12, 700);
    if (st != RPC_S_OK) { t_note("CALL_TIMEOUT unsupported: %lu", (unsigned long)st); rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL); return; }
    t0 = GetTickCount();
    e = rx_ndr_h(h, RX_SLEEP, 2500, 0, 0, 0, NULL);
    el = GetTickCount() - t0;
    t_note("call with 700 ms timeout against a 2500 ms server routine -> %lu after %lu ms", (unsigned long)e, (unsigned long)el);
    /* a honoured timeout fails the call early; an ignored one lets it run to completion. Either way the outcome
       must be consistent with the elapsed time. */
    CHECK(e == 0 ? el >= 2400 : el < 2400);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}

/* ---- misc server-call behaviours ----------------------------------------------------------------- */
TEST(ncalrpc_extra_manager_exception_does_not_leak_into_next_call_on_same_binding) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e; LONG_PTR r; int i;
    REQUIRE(U(RpcServerRegisterIf));
    rx_ep(ep, 64); EQ(rx_serve(rx_xif_s_ifspec, ep, NULL, NULL), RPC_S_OK);
    h = rx_bind(ep);
    for (i = 0; i < 5; i++) {
        e = rx_ndr_h(h, RX_RAISE, ERROR_ACCESS_DENIED, 0, 0, 0, NULL); EQ(e, ERROR_ACCESS_DENIED);
        r = -1; e = rx_ndr_h(h, RX_ADD, i, i, 0, 0, &r); EQ(e, 0); EQ((int)r, 2 * i);
    }
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_extra_two_bindings_to_same_endpoint_see_same_server_state) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h1, h2; DWORD e; LONG_PTR c0 = 0, c1 = 0, c2 = 0;
    REQUIRE(U(RpcServerRegisterIf));
    rx_ep(ep, 64); EQ(rx_serve(rx_xif_s_ifspec, ep, NULL, NULL), RPC_S_OK);
    h1 = rx_bind(ep); h2 = rx_bind(ep);
    CHECK(h1 != NULL && h2 != NULL && h1 != h2);
    e = rx_ndr_h(h1, RX_COUNTER, 0, 0, 0, 0, &c0); EQ(e, 0);
    e = rx_ndr_h(h1, RX_VOID, 0, 0, 0, 0, NULL); EQ(e, 0);
    e = rx_ndr_h(h1, RX_COUNTER, 0, 0, 0, 0, &c1); EQ(e, 0);
    e = rx_ndr_h(h2, RX_COUNTER, 0, 0, 0, 0, &c2); EQ(e, 0);
    EQ((int)c1, (int)c0 + 1);                     /* p_counter does not count itself; p_void does */
    EQ((int)c2, (int)c1);
    rx_unbind(h1); rx_unbind(h2); rx_server_down(rx_xif_s_ifspec, NULL);
}
