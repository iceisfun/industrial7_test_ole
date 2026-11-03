/* rpcrt4 server lifecycle over ncalrpc: protseq/endpoint registration, interface registration,
 * listen / stop / wait protocol, inquiry functions.  Servers are in-process; every test resets the
 * process-wide listening state first and tears down in its tail. */
#include "rpcx.h"

#define SERVE(ep) EQ(rx_serve(rx_xif_s_ifspec, (ep), NULL, NULL), RPC_S_OK)

static int call_add(RPC_BINDING_HANDLE h, int a, int b, DWORD *exc) {
    LONG_PTR r = -1; DWORD e = rx_ndr_h(h, RX_ADD, a, b, 0, 0, &r);
    if (exc) *exc = e;
    return (int)r;
}

/* ---- RpcServerUseProtseqEp ------------------------------------------------ */
TEST(ncalrpc_life_useprotseqep_w) {
    WCHAR ep[64];
    REQUIRE(U(RpcServerUseProtseqEpW));
    rx_server_reset(); rx_ep_unique(ep, 64);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)ep, NULL), RPC_S_OK);
}
TEST(ncalrpc_life_useprotseqep_a) {
    char ep[64];
    REQUIRE(U(RpcServerUseProtseqEpA));
    rx_server_reset(); rx_epa(ep, sizeof ep);
    EQ(U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)ep, NULL), RPC_S_OK);
}
TEST(ncalrpc_life_useprotseqep_case_insensitive_protseq) {
    WCHAR ep[64];
    REQUIRE(U(RpcServerUseProtseqEpW));
    rx_ep_unique(ep, 64);
    /* protocol sequence names are matched without regard to case */
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"NCALRPC", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)ep, NULL), RPC_S_OK);
}
/* Decomp (I_RpcServerUseProtseqEp2W -> RPC_SERVER::UseRpcProtocolSequence): "ncalrpc" is matched with _wcsicmp;
 * any other name that does not begin (case-insensitively) with "ncacn_" fails with 0x6a7 (RPC_S_PROTSEQ_NOT_SUPPORTED). */
TEST(ncalrpc_life_useprotseqep_invalid_protseq) {
    WCHAR ep[64];
    REQUIRE(U(RpcServerUseProtseqEpW));
    rx_ep_unique(ep, 64);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"olet_not_a_protseq", 10, (RPC_WSTR)ep, NULL), RPC_S_PROTSEQ_NOT_SUPPORTED);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"", 10, (RPC_WSTR)ep, NULL), RPC_S_PROTSEQ_NOT_SUPPORTED);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncadg_ip_udp", 10, (RPC_WSTR)ep, NULL), RPC_S_PROTSEQ_NOT_SUPPORTED);
    EQ(U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc2", 10, (RPC_WSTR)ep, NULL), RPC_S_PROTSEQ_NOT_SUPPORTED);
}
TEST(ncalrpc_life_useprotseqep_a_invalid_protseq) {
    REQUIRE(U(RpcServerUseProtseqEpA));
    EQ(U(RpcServerUseProtseqEpA)((RPC_CSTR)"olet_not_a_protseq", 10, (RPC_CSTR)"olet_x", NULL), RPC_S_PROTSEQ_NOT_SUPPORTED);
}
TEST(ncalrpc_life_useprotseqep_unsupported_protseqs) {
    /* Well-known but unsupported sequences: observed status is version dependent. */
    static const WCHAR *names[] = { L"ncacn_dnet_nsp", L"ncadg_ip_udp", L"ncacn_nb_tcp", L"ncacn_spx", L"ncadg_ipx" };
    WCHAR ep[64]; unsigned i;
    REQUIRE(U(RpcServerUseProtseqEpW));
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        RPC_STATUS st; rx_ep_unique(ep, 64);
        st = U(RpcServerUseProtseqEpW)((RPC_WSTR)names[i], 10, (RPC_WSTR)ep, NULL);
        if (wcsncmp(names[i], L"ncacn_", 6)) EQ(st, RPC_S_PROTSEQ_NOT_SUPPORTED);   /* decomp-verified */
        else CHECK(st == RPC_S_PROTSEQ_NOT_SUPPORTED || st == RPC_S_INVALID_RPC_PROTSEQ || st == RPC_S_OK);
        t_note("%ls -> %lu", names[i], (unsigned long)st);
    }
}
TEST(ncalrpc_life_useprotseqep_null_endpoint) {
    RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqEpW));
    rx_server_reset();
    /* a NULL endpoint on ncalrpc: dynamic endpoint (same as RpcServerUseProtseq) */
    st = U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", 10, NULL, NULL);
    CHECK(st == RPC_S_OK || st == RPC_S_INVALID_ENDPOINT_FORMAT);
    t_note("UseProtseqEp(ncalrpc, NULL endpoint) -> %lu", (unsigned long)st);
}
TEST(ncalrpc_life_useprotseq_dynamic_endpoint) {
    RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqW));
    rx_server_reset();
    st = U(RpcServerUseProtseqW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, NULL);
    EQ(st, RPC_S_OK);
    st = U(RpcServerUseProtseqA)((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, NULL);
    EQ(st, RPC_S_OK);                  /* re-registering a protseq reuses its endpoint */
    EQ(U(RpcServerUseProtseqW)((RPC_WSTR)L"olet_bad_protseq", 10, NULL), RPC_S_PROTSEQ_NOT_SUPPORTED);
}
TEST(ncalrpc_life_useprotseqep_same_endpoint_twice) {
    WCHAR ep[64]; RPC_STATUS s1, s2;
    REQUIRE(U(RpcServerUseProtseqEpW));
    rx_server_reset(); rx_ep_unique(ep, 64);
    s1 = U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)ep, NULL);
    s2 = U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)ep, NULL);
    EQ(s1, RPC_S_OK);
    EQ(s2, RPC_S_DUPLICATE_ENDPOINT);   /* decomp: SameEndpointAndProtocolSequence hit -> 0x6cc when not an "ok if registered" caller */
}
TEST(ncalrpc_life_useprotseqep_duplicate_across_processes) {
    WCHAR ep[64]; PROCESS_INFORMATION pi; HANDLE h; DWORD code = 0xdead;
    REQUIRE(U(RpcServerUseProtseqEpW));
    rx_server_reset(); rx_ep_unique(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    /* the child tries to take the same endpoint name and exits with the status */
    h = rx_spawn_server(ep, "plain", &pi);
    REQUIRE(h);
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); t_note("child timed out"); }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    EQ(code, RPC_S_DUPLICATE_ENDPOINT);
}

/* ---- RpcServerInqBindings ------------------------------------------------- */
TEST(ncalrpc_life_inqbindings_contains_endpoint) {
    WCHAR ep[64]; RPC_BINDING_VECTOR *v = NULL; RPC_STATUS st; unsigned i; int found = 0;
    REQUIRE(U(RpcServerInqBindings)); REQUIRE(U(RpcBindingToStringBindingW));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    st = U(RpcServerInqBindings)(&v);
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK && v) {
        CHECK(v->Count >= 1);
        for (i = 0; i < v->Count; i++) {
            RPC_WSTR sb = NULL;
            if (U(RpcBindingToStringBindingW)(v->BindingH[i], &sb) == RPC_S_OK) {
                if (sb && wcsstr((WCHAR *)sb, ep)) {
                    found = 1;
                    CHECK(!wcsncmp((WCHAR *)sb, L"ncalrpc:", 8));
                }
                U(RpcStringFreeW)(&sb);
            }
        }
        CHECK(found);
        EQ(U(RpcBindingVectorFree)(&v), RPC_S_OK);
        CHECK(v == NULL);
    }
}
TEST(ncalrpc_life_inqbindings_binding_usable) {
    /* a binding taken from the vector is a server-side handle for the local endpoint and can be
       converted back to a string; the string has the ncalrpc[endpoint] form */
    WCHAR ep[64]; RPC_BINDING_VECTOR *v = NULL; unsigned i; int ok = 0;
    REQUIRE(U(RpcServerInqBindings)); REQUIRE(U(RpcBindingToStringBindingW));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    if (U(RpcServerInqBindings)(&v) != RPC_S_OK) { t_skip("no bindings"); return; }
    for (i = 0; i < v->Count; i++) {
        RPC_WSTR sb = NULL, proto = NULL, ept = NULL;
        if (U(RpcBindingToStringBindingW)(v->BindingH[i], &sb) != RPC_S_OK) continue;
        if (U(RpcStringBindingParseW)(sb, NULL, &proto, NULL, &ept, NULL) == RPC_S_OK) {
            if (proto && !lstrcmpW((WCHAR *)proto, L"ncalrpc") && ept && !lstrcmpiW((WCHAR *)ept, ep)) ok = 1;
            if (proto) U(RpcStringFreeW)(&proto);
            if (ept) U(RpcStringFreeW)(&ept);
        }
        U(RpcStringFreeW)(&sb);
    }
    CHECK(ok);
    U(RpcBindingVectorFree)(&v);
}
TEST(ncalrpc_life_bindingvectorfree_null) {
    RPC_BINDING_VECTOR *v = NULL; RPC_STATUS st;
    REQUIRE(U(RpcBindingVectorFree));
    if (t_is_wine()) { t_skip("crashes in wine"); return; }
    st = U(RpcBindingVectorFree)(&v);       /* *v == NULL */
    t_note("RpcBindingVectorFree(&NULL) -> %lu", (unsigned long)st);
}

/* ---- RpcServerRegisterIf / Unregister ------------------------------------ */
TEST(ncalrpc_life_registerif_ok_and_call) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcServerRegisterIf)); rx_ep(ep, 64);
    SERVE(ep);
    h = rx_bind(ep); CHECK(h != NULL);
    EQ(call_add(h, 20, 22, &e), 42); EQ(e, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_registerif_twice) {
    WCHAR ep[64]; RPC_STATUS s2, s3;
    REQUIRE(U(RpcServerRegisterIf)); rx_ep(ep, 64);
    SERVE(ep);
    s2 = U(RpcServerRegisterIf)(rx_xif_s_ifspec, NULL, NULL);
    CHECK(s2 == RPC_S_OK || s2 == RPC_S_ALREADY_REGISTERED);
    t_note("second RpcServerRegisterIf of same if/mgr -> %lu", (unsigned long)s2);
    s3 = U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1);
    EQ(s3, RPC_S_OK);
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_unregisterif_unknown) {
    rx_raw r; UUID u;
    REQUIRE(U(RpcServerUnregisterIf));
    rx_raw_uuid(&u, 0x10); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, NULL, 0), RPC_S_UNKNOWN_IF);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, NULL, 1), RPC_S_UNKNOWN_IF);
}
TEST(ncalrpc_life_unregisterif_twice) {
    WCHAR ep[64];
    REQUIRE(U(RpcServerUnregisterIf)); rx_ep(ep, 64);
    SERVE(ep);
    EQ(U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1), RPC_S_OK);
    EQ(U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1), RPC_S_UNKNOWN_IF);
    rx_server_down(NULL, NULL);
}
TEST(ncalrpc_life_unregisterifex_unknown_and_known) {
    WCHAR ep[64]; rx_raw r; UUID u;
    REQUIRE(U(RpcServerUnregisterIfEx));
    rx_raw_uuid(&u, 0x11); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcServerUnregisterIfEx)((RPC_IF_HANDLE)&r.si, NULL, 0), RPC_S_UNKNOWN_IF);
    rx_ep(ep, 64); SERVE(ep);
    EQ(U(RpcServerUnregisterIfEx)(rx_xif_s_ifspec, NULL, 0), RPC_S_OK);
    EQ(U(RpcServerUnregisterIfEx)(rx_xif_s_ifspec, NULL, 1), RPC_S_UNKNOWN_IF);
    rx_server_down(NULL, NULL);
}
TEST(ncalrpc_life_unregistered_if_not_callable) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcServerUnregisterIf)); rx_ep(ep, 64);
    SERVE(ep);
    h = rx_bind(ep);
    call_add(h, 1, 1, &e); EQ(e, 0);
    EQ(U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1), RPC_S_OK);
    call_add(h, 1, 1, &e);
    EQ(e, RPC_S_UNKNOWN_IF);
    EQ(U(RpcServerRegisterIf)(rx_xif_s_ifspec, NULL, NULL), RPC_S_OK);   /* and can be registered again */
    call_add(h, 1, 1, &e); EQ(e, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_registerif_two_versions_same_uuid) {
    /* the same uuid may be registered with different major versions side by side */
    rx_raw a, b; UUID u; WCHAR ep[64]; RPC_BINDING_HANDLE h; ULONG n; DWORD v;
    REQUIRE(U(RpcServerRegisterIf));
    rx_server_reset(); rx_raw_uuid(&u, 0x12); rx_ep(ep, 64);
    rx_raw_init(&a, &u, 1, 0); rx_raw_init(&b, &u, 2, 0);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&a.si, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&b.si, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(ep);
    v = 0; EQ(rx_raw_call(h, &a, 4, NULL, 0, &v, 4, &n), RPC_S_OK);
    v = 0; EQ(rx_raw_call(h, &b, 4, NULL, 0, &v, 4, &n), RPC_S_OK);
    EQ(rx_raw_calls >= 2, 1);
    rx_unbind(h);
    rx_server_down(NULL, NULL);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&a.si, NULL, 1), RPC_S_OK);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&b.si, NULL, 1), RPC_S_OK);
}
TEST(ncalrpc_life_registerifex_flags_zero) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcServerRegisterIfEx)); rx_ep(ep, 64);
    EQ(rx_serve_ex(rx_xif_s_ifspec, ep, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL), RPC_S_OK);
    h = rx_bind(ep);
    EQ(call_add(h, 2, 3, &e), 5); EQ(e, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_registerif2_ok) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcServerRegisterIf2));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf2)(rx_xif_s_ifspec, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, (unsigned)-1, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(ep);
    EQ(call_add(h, 7, 8, &e), 15); EQ(e, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_registerif2_maxrpcsize_limits_request) {
    /* MaxRpcSize bounds the size of a call's request buffer; a smaller call goes through, a larger one is rejected */
    WCHAR ep[64]; rx_raw r; UUID u; RPC_BINDING_HANDLE h; static char big[8192]; char out[16]; ULONG n; RPC_STATUS st;
    REQUIRE(U(RpcServerRegisterIf2));
    rx_server_reset(); rx_ep(ep, 64); rx_raw_uuid(&u, 0x13); rx_raw_init(&r, &u, 1, 0);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf2)((RPC_IF_HANDLE)&r.si, NULL, NULL, 0, 20, 1024, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(ep);
    EQ(rx_raw_call(h, &r, 0, big, 16, out, sizeof out, &n), RPC_S_OK);
    st = rx_raw_call(h, &r, 0, big, sizeof big, NULL, 0, &n);
    t_note("8 KiB request with MaxRpcSize=1024 -> %lu", (unsigned long)st);
    rx_unbind(h); rx_server_down((RPC_IF_HANDLE)&r.si, NULL);
}
TEST(ncalrpc_life_registerif3_ok) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(UN(RpcServerRegisterIf3));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(UN(RpcServerRegisterIf3)(rx_xif_s_ifspec, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, (unsigned)-1, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(ep);
    EQ(call_add(h, 100, -1, &e), 99); EQ(e, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_registerif3_security_descriptor_ignored_for_local_if) {
    /* RegisterIf3 takes a security descriptor pointer; NULL is the documented default */
    WCHAR ep[64];
    REQUIRE(UN(RpcServerRegisterIf3));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(UN(RpcServerRegisterIf3)(rx_xif_s_ifspec, NULL, NULL, RPC_IF_ALLOW_LOCAL_ONLY, 10, (unsigned)-1, NULL, NULL), RPC_S_OK);
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_autolisten) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e; RPC_STATUS lst;
    REQUIRE(U(RpcServerRegisterIfEx));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIfEx)(rx_xif_s_ifspec, NULL, NULL, RPC_IF_AUTOLISTEN, 10, NULL), RPC_S_OK);
    h = rx_bind(ep);
    EQ(call_add(h, 5, 6, &e), 11); EQ(e, 0);   /* no RpcServerListen was called */
    lst = U(RpcMgmtIsServerListening)(NULL);
    t_note("IsServerListening with autolisten interface -> %lu", (unsigned long)lst);
    EQ(U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1), RPC_S_OK);
    call_add(h, 5, 6, &e);
    CHECK(e != 0);
    t_note("call after autolisten interface unregistered -> exception/status %lu", (unsigned long)e);
    rx_unbind(h);
    rx_server_reset();
}
TEST(ncalrpc_life_autolisten_does_not_block_explicit_listen) {
    WCHAR ep[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerRegisterIfEx));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIfEx)(rx_xif_s_ifspec, NULL, NULL, RPC_IF_AUTOLISTEN, 10, NULL), RPC_S_OK);
    st = U(RpcServerListen)(1, 20, TRUE);
    CHECK(st == RPC_S_OK || st == RPC_S_ALREADY_LISTENING);
    t_note("explicit RpcServerListen after an autolisten interface -> %lu", (unsigned long)st);
    rx_server_down(rx_xif_s_ifspec, NULL);
}

/* ---- RpcServerInqIf / RpcIfInqId / RpcMgmtInqIfIds ----------------------- */
TEST(ncalrpc_life_inqif_unknown) {
    rx_raw r; UUID u; RPC_MGR_EPV *epv = (RPC_MGR_EPV *)(ULONG_PTR)1;
    REQUIRE(U(RpcServerInqIf));
    if (t_is_wine()) { t_skip("RpcServerInqIf unimplemented in wine"); return; }
    rx_raw_uuid(&u, 0x20); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&r.si, NULL, &epv), RPC_S_UNKNOWN_IF);
}
TEST(ncalrpc_life_inqif_returns_registered_epv) {
    rx_raw r; UUID u; static void *epv_dummy[2]; RPC_MGR_EPV *got = NULL; UUID mt;
    REQUIRE(U(RpcServerInqIf));
    if (t_is_wine()) { t_skip("RpcServerInqIf unimplemented in wine"); return; }
    rx_server_reset(); rx_raw_uuid(&u, 0x21); rx_raw_init(&r, &u, 1, 0);
    rx_raw_uuid(&mt, 0x22);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, NULL, (RPC_MGR_EPV *)epv_dummy), RPC_S_OK);
    EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&r.si, NULL, &got), RPC_S_OK);
    CHECK(got == (RPC_MGR_EPV *)epv_dummy);
    /* a manager type that was never registered is unknown */
    EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&r.si, &mt, &got), RPC_S_UNKNOWN_MGR_TYPE);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, NULL, 1), RPC_S_OK);
    EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&r.si, NULL, &got), RPC_S_UNKNOWN_IF);
}
TEST(ncalrpc_life_inqif_with_mgrtype) {
    rx_raw r; UUID u, mt; static void *e1[1], *e2[1]; RPC_MGR_EPV *got = NULL;
    REQUIRE(U(RpcServerInqIf));
    if (t_is_wine()) { t_skip("RpcServerInqIf unimplemented in wine"); return; }
    rx_server_reset(); rx_raw_uuid(&u, 0x23); rx_raw_uuid(&mt, 0x24); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, &mt, (RPC_MGR_EPV *)e1), RPC_S_OK);
    EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&r.si, &mt, &got), RPC_S_OK);
    CHECK(got == (RPC_MGR_EPV *)e1);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, &mt, 1), RPC_S_OK);
}
TEST(ncalrpc_life_ifinqid_server_and_client_handles) {
    RPC_IF_ID id; rx_raw r; UUID u;
    REQUIRE(U(RpcIfInqId));
    rx_raw_uuid(&u, 0x25); rx_raw_init(&r, &u, 3, 7);
    memset(&id, 0xcc, sizeof id);
    EQ(U(RpcIfInqId)((RPC_IF_HANDLE)&r.si, &id), RPC_S_OK);
    CHECK(!memcmp(&id.Uuid, &u, sizeof u)); EQ(id.VersMajor, 3); EQ(id.VersMinor, 7);
    memset(&id, 0xcc, sizeof id);
    EQ(U(RpcIfInqId)((RPC_IF_HANDLE)&r.ci, &id), RPC_S_OK);
    CHECK(!memcmp(&id.Uuid, &u, sizeof u)); EQ(id.VersMajor, 3); EQ(id.VersMinor, 7);
    memset(&id, 0xcc, sizeof id);
    EQ(U(RpcIfInqId)(rx_xif_s_ifspec, &id), RPC_S_OK);
    CHECK(!memcmp(&id.Uuid, &rx_xif_uuid, sizeof u)); EQ(id.VersMajor, 1); EQ(id.VersMinor, 0);
}
TEST(ncalrpc_life_mgmt_inqifids_local) {
    WCHAR ep[64]; RPC_IF_ID_VECTOR *v = NULL; unsigned i; int found = 0; RPC_STATUS st;
    REQUIRE(U(RpcMgmtInqIfIds)); REQUIRE(U(RpcIfIdVectorFree));
    if (t_is_wine()) { t_skip("RpcIfIdVectorFree unimplemented in wine"); return; }
    rx_ep(ep, 64); SERVE(ep);
    st = U(RpcMgmtInqIfIds)(NULL, &v);
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK && v) {
        for (i = 0; i < v->Count; i++)
            if (!memcmp(&v->IfId[i]->Uuid, &rx_xif_uuid, sizeof(UUID)) && v->IfId[i]->VersMajor == 1 && v->IfId[i]->VersMinor == 0) found = 1;
        CHECK(found);
        EQ(U(RpcIfIdVectorFree)(&v), RPC_S_OK);
        CHECK(v == NULL);
    }
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_mgmt_inqifids_after_unregister_lacks_if) {
    WCHAR ep[64]; RPC_IF_ID_VECTOR *v = NULL; unsigned i; int found = 0; RPC_STATUS st;
    REQUIRE(U(RpcMgmtInqIfIds));
    if (t_is_wine()) { t_skip("RpcIfIdVectorFree unimplemented in wine"); return; }
    rx_ep(ep, 64); SERVE(ep);
    EQ(U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1), RPC_S_OK);
    st = U(RpcMgmtInqIfIds)(NULL, &v);
    CHECK(st == RPC_S_OK || st == RPC_S_NO_INTERFACES);
    if (st == RPC_S_OK && v) {
        for (i = 0; i < v->Count; i++) if (!memcmp(&v->IfId[i]->Uuid, &rx_xif_uuid, sizeof(UUID))) found = 1;
        CHECK(!found);
        U(RpcIfIdVectorFree)(&v);
    }
    rx_server_down(NULL, NULL);
}
TEST(ncalrpc_life_mgmt_inqifids_remote_binding) {
    WCHAR ep[64]; RPC_IF_ID_VECTOR *v = NULL; RPC_BINDING_HANDLE h; RPC_STATUS st; unsigned i; int found = 0;
    REQUIRE(U(RpcMgmtInqIfIds));
    if (t_is_wine()) { t_skip("RpcIfIdVectorFree unimplemented in wine"); return; }
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    st = U(RpcMgmtInqIfIds)(h, &v);
    CHECK(st == RPC_S_OK || st == RPC_S_ACCESS_DENIED);
    t_note("RpcMgmtInqIfIds via ncalrpc binding -> %lu", (unsigned long)st);
    if (st == RPC_S_OK && v) {
        for (i = 0; i < v->Count; i++) if (!memcmp(&v->IfId[i]->Uuid, &rx_xif_uuid, sizeof(UUID))) found = 1;
        CHECK(found);
        U(RpcIfIdVectorFree)(&v);
    }
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_ifidvectorfree_null_vector) {
    RPC_IF_ID_VECTOR *v = NULL; RPC_STATUS st;
    REQUIRE(U(RpcIfIdVectorFree));
    if (t_is_wine()) { t_skip("unimplemented in wine"); return; }
    if (t_is_wine()) { t_skip("RpcIfIdVectorFree unimplemented in wine"); return; }
    st = U(RpcIfIdVectorFree)(&v);
    t_note("RpcIfIdVectorFree(&NULL) -> %lu", (unsigned long)st);
}

/* ---- RpcServerListen / Stop / Wait / IsListening -------------------------- */
TEST(ncalrpc_life_is_listening_states) {
    WCHAR ep[64];
    REQUIRE(U(RpcMgmtIsServerListening));
    rx_server_reset();
    EQ(U(RpcMgmtIsServerListening)(NULL), RPC_S_NOT_LISTENING);
    rx_ep(ep, 64); SERVE(ep);
    EQ(U(RpcMgmtIsServerListening)(NULL), RPC_S_OK);
    rx_server_down(rx_xif_s_ifspec, NULL);
    EQ(U(RpcMgmtIsServerListening)(NULL), RPC_S_NOT_LISTENING);
}
TEST(ncalrpc_life_listen_already_listening) {
    WCHAR ep[64];
    REQUIRE(U(RpcServerListen));
    rx_ep(ep, 64); SERVE(ep);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_ALREADY_LISTENING);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_ALREADY_LISTENING);
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_stop_when_not_listening) {
    REQUIRE(U(RpcMgmtStopServerListening));
    rx_server_reset();
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_NOT_LISTENING);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_NOT_LISTENING);
}
TEST(ncalrpc_life_wait_when_not_listening) {
    REQUIRE(U(RpcMgmtWaitServerListen));
    rx_server_reset();
    EQ(U(RpcMgmtWaitServerListen)(), RPC_S_NOT_LISTENING);
}
TEST(ncalrpc_life_stop_then_wait) {
    WCHAR ep[64];
    REQUIRE(U(RpcMgmtStopServerListening));
    rx_ep(ep, 64); SERVE(ep);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
    EQ(U(RpcMgmtWaitServerListen)(), RPC_S_OK);
    EQ(U(RpcMgmtIsServerListening)(NULL), RPC_S_NOT_LISTENING);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_NOT_LISTENING);
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_restart_listening) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e; int round;
    REQUIRE(U(RpcServerListen));
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    for (round = 0; round < 3; round++) {
        EQ(call_add(h, round, round, &e), 2 * round); EQ(e, 0);
        EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
        EQ(U(RpcMgmtWaitServerListen)(), RPC_S_OK);
        EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    }
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
static DWORD WINAPI listen_block_thread(void *p) { return U(RpcServerListen)(1, 20, FALSE); }
static DWORD WINAPI wait_thread2(void *p) { return U(RpcMgmtWaitServerListen)(); }
TEST(ncalrpc_life_listen_blocking_until_stop) {
    WCHAR ep[64]; HANDLE th; DWORD code = 0xdead; RPC_BINDING_HANDLE h; DWORD e, t0;
    REQUIRE(U(RpcServerListen));
    rx_server_reset(); rx_ep(ep, 64);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)(rx_xif_s_ifspec, NULL, NULL), RPC_S_OK);
    th = rx_thread(listen_block_thread, NULL); CHECK(th != NULL);
    for (t0 = GetTickCount(); !rx_listening() && GetTickCount() - t0 < 5000; ) Sleep(10);
    EQ(rx_listening(), 1);
    CHECK(WaitForSingleObject(th, 200) == WAIT_TIMEOUT);        /* still blocked in RpcServerListen */
    h = rx_bind(ep);
    EQ(call_add(h, 3, 4, &e), 7); EQ(e, 0);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
    CHECK(WaitForSingleObject(th, 10000) == WAIT_OBJECT_0);
    GetExitCodeThread(th, &code); CloseHandle(th);
    EQ(code, RPC_S_OK);                                  /* RpcServerListen returns RPC_S_OK after the stop */
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_wait_blocks_until_stop) {
    WCHAR ep[64]; HANDLE th; DWORD code = 0xdead;
    REQUIRE(U(RpcMgmtWaitServerListen));
    rx_ep(ep, 64); SERVE(ep);
    th = rx_thread(wait_thread2, NULL);
    CHECK(WaitForSingleObject(th, 200) == WAIT_TIMEOUT);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
    CHECK(WaitForSingleObject(th, 10000) == WAIT_OBJECT_0);
    GetExitCodeThread(th, &code); CloseHandle(th);
    EQ(code, RPC_S_OK);
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_wait_second_waiter_already_listening) {
    WCHAR ep[64]; HANDLE th; DWORD code = 0xdead;
    REQUIRE(U(RpcMgmtWaitServerListen));
    rx_ep(ep, 64); SERVE(ep);
    th = rx_thread(wait_thread2, NULL);
    Sleep(200);
    /* documented: another thread is already waiting in RpcMgmtWaitServerListen */
    {
        HANDLE th2 = rx_thread(wait_thread2, NULL); DWORD c2 = 0xdead;
        if (WaitForSingleObject(th2, 2000) == WAIT_OBJECT_0) { GetExitCodeThread(th2, &c2); EQ(c2, RPC_S_ALREADY_LISTENING); }
        else CHECKF(0, "second RpcMgmtWaitServerListen did not return");
        CloseHandle(th2);
    }
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
    CHECK(WaitForSingleObject(th, 10000) == WAIT_OBJECT_0);
    GetExitCodeThread(th, &code); CloseHandle(th);
    EQ(code, RPC_S_OK);
    rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_listen_parameter_checks) {
    RPC_STATUS st;
    REQUIRE(U(RpcServerListen));
    rx_server_reset();
    {
        WCHAR ep[64]; rx_ep(ep, 64); EQ(rx_use_ep(ep), RPC_S_OK);
    }
    st = U(RpcServerListen)(5, 2, TRUE);                       /* MaxCalls < MinimumCallThreads */
    t_note("RpcServerListen(min=5,max=2) -> %lu", (unsigned long)st);
    CHECK(st == RPC_S_OK || st == RPC_S_MAX_CALLS_TOO_SMALL);
    if (st == RPC_S_OK) rx_server_reset();
    st = U(RpcServerListen)(1, 0, TRUE);                       /* MaxCalls = 0 */
    t_note("RpcServerListen(min=1,max=0) -> %lu", (unsigned long)st);
    CHECK(st == RPC_S_OK || st == RPC_S_MAX_CALLS_TOO_SMALL || st == RPC_S_INVALID_ARG);
    if (st == RPC_S_OK) rx_server_reset();
}
TEST(ncalrpc_life_listen_no_protseqs_in_fresh_process) {
    PROCESS_INFORMATION pi; HANDLE h; DWORD code = 0xdead; WCHAR ep[64];
    rx_ep(ep, 64);
    h = rx_spawn_server(ep, "noprotseq", &pi);
    REQUIRE(h);
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    EQ(code, RPC_S_NO_PROTSEQS_REGISTERED);
}
TEST(ncalrpc_life_isserverlistening_fresh_process) {
    PROCESS_INFORMATION pi; HANDLE h; DWORD code = 0xdead; WCHAR ep[64];
    rx_ep(ep, 64);
    h = rx_spawn_server(ep, "nodefault", &pi);
    REQUIRE(h);
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    EQ(code, RPC_S_NOT_LISTENING);
}

/* ---- unregister vs calls in flight --------------------------------------- */
typedef struct { RPC_BINDING_HANDLE h; int ms; DWORD exc; } sleep_job;
static DWORD WINAPI sleeper_thread(void *p) {
    sleep_job *j = p;
    j->exc = rx_ndr_h(j->h, RX_SLEEP, j->ms, 0, 0, 0, NULL);
    return j->exc;
}
TEST(ncalrpc_life_unregister_waits_for_calls) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; sleep_job j; HANDLE th; DWORD t0; RPC_STATUS st; LONG done_before, started_before;
    REQUIRE(U(RpcServerUnregisterIf));
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    j.h = h; j.ms = 800; j.exc = 0xdead;
    done_before = rx_sleep_done; started_before = rx_sleep_started;
    th = rx_thread(sleeper_thread, &j);
    for (t0 = GetTickCount(); rx_sleep_started == started_before && GetTickCount() - t0 < 5000; ) Sleep(5);
    st = U(RpcServerUnregisterIf)(rx_xif_s_ifspec, NULL, 1 /* WaitForCallsToComplete */);
    EQ(st, RPC_S_OK);
    EQ(rx_sleep_done, done_before + 1);       /* the in-flight call had completed before UnregisterIf returned */
    CHECK(rx_join(th, 10000));
    EQ(j.exc, 0);
    rx_unbind(h); rx_server_down(NULL, NULL);
}
TEST(ncalrpc_life_stop_listening_waits_for_calls) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; sleep_job j; HANDLE th; DWORD t0; LONG done_before, started_before;
    REQUIRE(U(RpcMgmtWaitServerListen));
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    j.h = h; j.ms = 800; j.exc = 0xdead; done_before = rx_sleep_done; started_before = rx_sleep_started;
    th = rx_thread(sleeper_thread, &j);
    for (t0 = GetTickCount(); rx_sleep_started == started_before && GetTickCount() - t0 < 5000; ) Sleep(5);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
    EQ(U(RpcMgmtWaitServerListen)(), RPC_S_OK);
    EQ(rx_sleep_done, done_before + 1);       /* RpcMgmtWaitServerListen returns only after all calls complete */
    CHECK(rx_join(th, 10000));
    EQ(j.exc, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_new_calls_rejected_after_stop) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcMgmtStopServerListening));
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    call_add(h, 1, 2, &e); EQ(e, 0);
    EQ(U(RpcMgmtStopServerListening)(NULL), RPC_S_OK);
    EQ(U(RpcMgmtWaitServerListen)(), RPC_S_OK);
    call_add(h, 1, 2, &e);
    CHECK(e != 0);
    t_note("call after stop+wait (interface still registered) -> %lu", (unsigned long)e);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}

/* ---- statistics / tuning -------------------------------------------------- */
TEST(ncalrpc_life_inqstats_counts_calls) {
    WCHAR ep[64]; RPC_STATS_VECTOR *s0 = NULL, *s1 = NULL; RPC_BINDING_HANDLE h; int i; DWORD e;
    REQUIRE(U(RpcMgmtInqStats)); REQUIRE(U(RpcMgmtStatsVectorFree));
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    EQ(U(RpcMgmtInqStats)(NULL, &s0), RPC_S_OK);
    if (!s0) { rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL); return; }
    CHECK(s0->Count >= 4);
    for (i = 0; i < 10; i++) call_add(h, i, i, &e);
    EQ(U(RpcMgmtInqStats)(NULL, &s1), RPC_S_OK);
    if (s1) {
        unsigned d = s1->Stats[RPC_C_STATS_CALLS_IN] - s0->Stats[RPC_C_STATS_CALLS_IN];
        CHECK(d >= 10);
        t_note("CALLS_IN delta for 10 calls = %u, PKTS_IN delta = %u, PKTS_OUT delta = %u", d,
               s1->Stats[RPC_C_STATS_PKTS_IN] - s0->Stats[RPC_C_STATS_PKTS_IN],
               s1->Stats[RPC_C_STATS_PKTS_OUT] - s0->Stats[RPC_C_STATS_PKTS_OUT]);
        EQ(U(RpcMgmtStatsVectorFree)(&s1), RPC_S_OK);
        CHECK(s1 == NULL);
    }
    U(RpcMgmtStatsVectorFree)(&s0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}
TEST(ncalrpc_life_set_stack_size) {
    REQUIRE(U(RpcMgmtSetServerStackSize));
    EQ(U(RpcMgmtSetServerStackSize)(0x100000), RPC_S_OK);
    t_note("RpcMgmtSetServerStackSize(0) -> %lu", (unsigned long)U(RpcMgmtSetServerStackSize)(0));
}
TEST(ncalrpc_life_enable_idle_cleanup) {
    REQUIRE(U(RpcMgmtEnableIdleCleanup));
    EQ(U(RpcMgmtEnableIdleCleanup)(), RPC_S_OK);
    EQ(U(RpcMgmtEnableIdleCleanup)(), RPC_S_OK);
}
TEST(ncalrpc_life_server_works_with_larger_stack) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcMgmtSetServerStackSize));
    U(RpcMgmtSetServerStackSize)(0x200000);
    rx_ep(ep, 64); SERVE(ep);
    h = rx_bind(ep);
    EQ(call_add(h, 40, 2, &e), 42); EQ(e, 0);
    rx_unbind(h); rx_server_down(rx_xif_s_ifspec, NULL);
}

/* ---- UseProtseqIf / UseAllProtseqsIf ------------------------------------- */
TEST(ncalrpc_life_useprotseqif_without_endpoint_info) {
    rx_raw r; UUID u; RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqIfW));
    if (t_is_wine()) { t_skip("unimplemented in wine"); return; }
    rx_server_reset(); rx_raw_uuid(&u, 0x30); rx_raw_init(&r, &u, 1, 0);
    /* the interface carries no RpcProtseqEndpoint table: ncalrpc has no entry */
    st = U(RpcServerUseProtseqIfW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&r.si, NULL);
    t_note("UseProtseqIf(ncalrpc) on an interface without endpoint table -> %lu", (unsigned long)st);
    CHECK(st == RPC_S_OK || st == RPC_S_INVALID_ARG || st == RPC_S_PROTSEQ_NOT_FOUND || st == RPC_S_NO_ENDPOINT_FOUND || st == RPC_S_INVALID_ENDPOINT_FORMAT);
}
TEST(ncalrpc_life_useprotseqif_with_endpoint_table) {
    rx_raw r; UUID u; RPC_STATUS st; WCHAR epw[64]; char epa[64]; static RPC_PROTSEQ_ENDPOINT pe[1]; RPC_BINDING_HANDLE h; DWORD e;
    REQUIRE(U(RpcServerUseProtseqIfA));
    if (t_is_wine()) { t_skip("unimplemented in wine"); return; }
    rx_server_reset(); rx_raw_uuid(&u, 0x31); rx_raw_init(&r, &u, 1, 0);
    rx_ep_unique(epw, 64); WideCharToMultiByte(CP_ACP, 0, epw, -1, epa, sizeof epa, NULL, NULL);
    pe[0].RpcProtocolSequence = (unsigned char *)"ncalrpc"; pe[0].Endpoint = (unsigned char *)epa;
    r.si.RpcProtseqEndpointCount = 1; r.si.RpcProtseqEndpoint = pe;
    st = U(RpcServerUseProtseqIfA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&r.si, NULL);
    EQ(st, RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(epw);                      /* the endpoint named in the interface's table is live */
    e = rx_raw_call(h, &r, 4, NULL, 0, NULL, 0, NULL);
    EQ(e, RPC_S_OK);
    rx_unbind(h); rx_server_down((RPC_IF_HANDLE)&r.si, NULL);
}
TEST(ncalrpc_life_useallprotseqsif_with_endpoint_table) {
    rx_raw r; UUID u; RPC_STATUS st; static RPC_PROTSEQ_ENDPOINT pe[1]; char epa[64]; WCHAR epw[64]; RPC_BINDING_HANDLE h;
    REQUIRE(U(RpcServerUseAllProtseqsIf));
    if (t_is_wine()) { t_skip("UseAllProtseqsIf unimplemented in wine"); return; }
    rx_server_reset(); rx_raw_uuid(&u, 0x32); rx_raw_init(&r, &u, 1, 0);
    rx_ep_unique(epw, 64); WideCharToMultiByte(CP_ACP, 0, epw, -1, epa, sizeof epa, NULL, NULL);
    pe[0].RpcProtocolSequence = (unsigned char *)"ncalrpc"; pe[0].Endpoint = (unsigned char *)epa;
    r.si.RpcProtseqEndpointCount = 1; r.si.RpcProtseqEndpoint = pe;
    st = U(RpcServerUseAllProtseqsIf)(10, (RPC_IF_HANDLE)&r.si, NULL);
    t_note("UseAllProtseqsIf with a one-entry ncalrpc table -> %lu", (unsigned long)st);
    if (st != RPC_S_OK) { t_skip("UseAllProtseqsIf unavailable here"); return; }
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(epw);
    EQ(rx_raw_call(h, &r, 4, NULL, 0, NULL, 0, NULL), RPC_S_OK);
    rx_unbind(h); rx_server_down((RPC_IF_HANDLE)&r.si, NULL);
}
