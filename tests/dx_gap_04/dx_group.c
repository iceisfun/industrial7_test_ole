/* dx_gap_04: interface groups that are activated (RpcServerInterfaceGroupActivate registers the endpoints and the
 * interfaces with the RPC endpoint mapper) plus RpcServerUseAllProtseqs[Ex] in a child process (opens every
 * configured protocol sequence). Gated on the first successful activation: ACCESS_DENIED -> skip. The group is always
 * deactivated and closed at the end of each test; nothing persists after the process exits. */
#include "g4_group.h"

#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)
#define G4_ALREADY_LISTENING 1713
#define G4_NOT_LISTENING 1715
#define G4_NO_BINDINGS 1718
#define G4_TOO_BUSY 1723

typedef struct { rx_raw r; G4_IFT_W ift; G4_EPT_W ept; WCHAR ep[64]; void *grp; RPC_BINDING_HANDLE cli; } gfx;
static void gfx_init(gfx *g, unsigned n) {
    UUID u;
    memset(g, 0, sizeof *g);
    rx_raw_uuid(&u, n); rx_raw_init(&g->r, &u, 1, 0);
    rx_ep_unique(g->ep, 64);
    g->ift.IfSpec = (RPC_IF_HANDLE)&g->r.si; g->ift.MaxCalls = RPC_C_LISTEN_MAX_CALLS_DEFAULT; g->ift.MaxRpcSize = 0xFFFFFFFF;
    g->ept.ProtSeq = L"ncalrpc"; g->ept.Endpoint = g->ep; g->ept.Backlog = RPC_C_PROTSEQ_MAX_REQS_DEFAULT;
}
static int gfx_create_activate(gfx *g, int *denied) {
    RPC_STATUS st;
    *denied = 0;
    st = G4_CreateW(&g->ift, 1, &g->ept, 1, INFINITE, NULL, NULL, &g->grp);
    if (st != RPC_S_OK) { CHECKF(0, "Create -> %lu", (unsigned long)st); return 0; }
    st = G4_Activate(g->grp);
    if (st == ERROR_ACCESS_DENIED) { *denied = 1; G4_Close(g->grp); g->grp = NULL; return 0; }
    if (st != RPC_S_OK) { CHECKF(0, "Activate -> %lu", (unsigned long)st); G4_Close(g->grp); g->grp = NULL; return 0; }
    return 1;
}
static void gfx_down(gfx *g) {
    if (g->cli) { rx_unbind(g->cli); g->cli = NULL; }
    if (g->grp) { G4_Deactivate(g->grp, TRUE); G4_Close(g->grp); g->grp = NULL; }
}
#define GUP(g, n) do { int den_; gfx_init(&g, n); if (!gfx_create_activate(&g, &den_)) { if (den_) t_skip("not permitted to register with the endpoint mapper"); gfx_down(&g); return; } } while (0)

TEST(dx_gap4_group_activate_call_deactivate) {
    gfx g; ULONG n; char out[8]; RPC_STATUS st;
    G4_NEED_GROUP();
    GUP(g, 0xE5);
    g.cli = rx_bind(g.ep); CHECK(g.cli != NULL);
    st = rx_raw_call(g.cli, &g.r, 0, "abcdefgh", 8, out, 8, &n);
    EQ(st, RPC_S_OK); EQ(n, 8); CHECK(!memcmp(out, "abcdefgh", 8));
    EQ(G4_Deactivate(g.grp, TRUE), RPC_S_OK);
    st = rx_raw_call(g.cli, &g.r, 0, "abcdefgh", 8, out, 8, &n);
    CHECKF(st != RPC_S_OK, "call after Deactivate succeeded");
    t_note("call after Deactivate -> %lu", (unsigned long)st);
    EQ(G4_Close(g.grp), RPC_S_OK); g.grp = NULL;
    gfx_down(&g);
}
TEST(dx_gap4_group_inqbindings_follows_activation) {
    gfx g; RPC_BINDING_VECTOR *v = NULL; unsigned i; int found = 0; WCHAR needle[96];
    G4_NEED_GROUP();
    GUP(g, 0xE6);
    EQ(G4_InqBindings(g.grp, &v), RPC_S_OK);
    CHECK(v != NULL);
    wsprintfW(needle, L"[%s]", g.ep);
    for (i = 0; v && i < v->Count; i++) {
        RPC_WSTR s = NULL;
        if (U(RpcBindingToStringBindingW)(v->BindingH[i], &s) == RPC_S_OK && s) { if (!_wcsnicmp((WCHAR *)s, L"ncalrpc", 7) && wcsstr((WCHAR *)s, needle)) found = 1; U(RpcStringFreeW)(&s); }
    }
    CHECK(found);
    if (v) U(RpcBindingVectorFree)(&v);
    EQ(G4_Deactivate(g.grp, TRUE), RPC_S_OK);
    v = (RPC_BINDING_VECTOR *)(ULONG_PTR)0x1234;
    EQ(G4_InqBindings(g.grp, &v), G4_NO_BINDINGS);
    CHECK(v == (RPC_BINDING_VECTOR *)(ULONG_PTR)0x1234);
    gfx_down(&g);
}
TEST(dx_gap4_group_inqbindings_not_in_global_list) {
    /* the process-wide RpcServerInqBindings does not report the group's endpoints (they belong to the group) */
    gfx g; RPC_BINDING_VECTOR *v = NULL; unsigned i; WCHAR needle[96]; int found = 0; RPC_STATUS st;
    G4_NEED_GROUP();
    GUP(g, 0xE7);
    st = U(RpcServerInqBindings)(&v);
    wsprintfW(needle, L"[%s]", g.ep);
    for (i = 0; st == RPC_S_OK && v && i < v->Count; i++) {
        RPC_WSTR s = NULL;
        if (U(RpcBindingToStringBindingW)(v->BindingH[i], &s) == RPC_S_OK && s) { if (wcsstr((WCHAR *)s, needle)) found = 1; U(RpcStringFreeW)(&s); }
    }
    if (v) U(RpcBindingVectorFree)(&v);
    t_note("RpcServerInqBindings -> %lu, group endpoint visible globally: %d", (unsigned long)st, found);
    gfx_down(&g);
}
TEST(dx_gap4_group_activate_twice) {
    gfx g;
    G4_NEED_GROUP();
    GUP(g, 0xE8);
    EQ(G4_Activate(g.grp), G4_ALREADY_LISTENING);
    gfx_down(&g);
}
TEST(dx_gap4_group_deactivate_twice) {
    gfx g;
    G4_NEED_GROUP();
    GUP(g, 0xE9);
    EQ(G4_Deactivate(g.grp, TRUE), RPC_S_OK);
    EQ(G4_Deactivate(g.grp, TRUE), G4_NOT_LISTENING);
    EQ(G4_Deactivate(g.grp, FALSE), G4_NOT_LISTENING);
    gfx_down(&g);
}
TEST(dx_gap4_group_reactivate) {
    gfx g; ULONG n; char out[4]; RPC_STATUS st;
    G4_NEED_GROUP();
    GUP(g, 0xEA);
    EQ(G4_Deactivate(g.grp, TRUE), RPC_S_OK);
    EQ(G4_Activate(g.grp), RPC_S_OK);                    /* a deactivated group can be activated again */
    g.cli = rx_bind(g.ep);
    st = rx_raw_call(g.cli, &g.r, 0, "xyz", 3, out, 4, &n);
    EQ(st, RPC_S_OK); EQ(n, 3);
    gfx_down(&g);
}
TEST(dx_gap4_group_close_active_group_deactivates) {
    gfx g; ULONG n; char out[4]; RPC_STATUS st;
    G4_NEED_GROUP();
    GUP(g, 0xEB);
    g.cli = rx_bind(g.ep);
    EQ(rx_raw_call(g.cli, &g.r, 0, "ab", 2, out, 4, &n), RPC_S_OK);
    EQ(G4_Close(g.grp), RPC_S_OK); g.grp = NULL;         /* "performs a forced deactivation before closing" */
    st = rx_raw_call(g.cli, &g.r, 0, "ab", 2, out, 4, &n);
    CHECKF(st != RPC_S_OK, "call after Close succeeded");
    gfx_down(&g);
}
TEST(dx_gap4_group_dynamic_endpoint) {
    gfx g; RPC_BINDING_VECTOR *v = NULL; RPC_WSTR s = NULL; RPC_BINDING_HANDLE cli = NULL; ULONG n; char out[4]; int den;
    G4_NEED_GROUP();
    gfx_init(&g, 0xEC); g.ept.Endpoint = NULL;
    if (!gfx_create_activate(&g, &den)) { if (den) t_skip("not permitted to register with the endpoint mapper"); gfx_down(&g); return; }
    EQ(G4_InqBindings(g.grp, &v), RPC_S_OK);
    if (v && v->Count >= 1 && U(RpcBindingToStringBindingW)(v->BindingH[0], &s) == RPC_S_OK && s) {
        WCHAR *lb = wcschr((WCHAR *)s, L'['); t_note("dynamic group binding: %ls", s);
        CHECK(!_wcsnicmp((WCHAR *)s, L"ncalrpc", 7));
        CHECK(lb && lb[1] != L']');                       /* an endpoint name was assigned */
        EQ(U(RpcBindingFromStringBindingW)(s, &cli), RPC_S_OK);
        if (cli) EQ(rx_raw_call(cli, &g.r, 0, "q", 1, out, 4, &n), RPC_S_OK);
        if (cli) rx_unbind(cli);
        U(RpcStringFreeW)(&s);
    } else CHECKF(0, "no binding for the dynamic endpoint");
    if (v) U(RpcBindingVectorFree)(&v);
    gfx_down(&g);
}
TEST(dx_gap4_group_interface_not_served_on_plain_endpoint) {
    gfx g; WCHAR ep2[64]; RPC_BINDING_HANDLE c2; ULONG n; char out[4]; RPC_STATUS st;
    G4_NEED_GROUP();
    GUP(g, 0xED);
    rx_ep_unique(ep2, 64);
    rx_server_reset();
    EQ(rx_use_ep(ep2), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    c2 = rx_bind(ep2);
    st = rx_raw_call(c2, &g.r, 0, "ab", 2, out, 4, &n);   /* "Interfaces in an interface group can only be called over endpoints of the same group" */
    CHECKF(st != RPC_S_OK, "group interface answered on a non-group endpoint");
    t_note("call over a non-group endpoint -> %lu", (unsigned long)st);
    rx_unbind(c2);
    rx_server_down(NULL, NULL);
    gfx_down(&g);
}
typedef struct { gfx *g; RPC_STATUS st; } slowjob;
static DWORD WINAPI slow_call(void *p) {
    slowjob *j = p; int ms = 2500; ULONG n;
    j->st = rx_raw_call(j->g->cli, &j->g->r, 2, &ms, 4, NULL, 0, &n);
    return 0;
}
TEST(dx_gap4_group_deactivate_not_forced_with_active_call) {
    gfx g; slowjob j; HANDLE th; LONG c0; DWORD t0;
    G4_NEED_GROUP();
    GUP(g, 0xEE);
    g.cli = rx_bind(g.ep); j.g = &g; j.st = 0xFFFF;
    c0 = rx_raw_calls;
    th = CreateThread(NULL, 0, slow_call, &j, 0, NULL);
    t0 = GetTickCount();
    while (rx_raw_calls == c0 && GetTickCount() - t0 < 5000) Sleep(10);
    if (rx_raw_calls == c0) { t_note("slow call did not start"); }
    else {
        RPC_STATUS st = G4_Deactivate(g.grp, FALSE);
        EQ(st, G4_TOO_BUSY);                              /* outstanding client activity and no force: rolled back */
        t_note("Deactivate(FALSE) during a call -> %lu", (unsigned long)st);
        { ULONG n; char out[4]; int ms = 0; RPC_STATUS s2 = rx_raw_call(g.cli, &g.r, 4, NULL, 0, out, 4, &n); (void)ms; t_note("concurrent nop call after the refused deactivate -> %lu", (unsigned long)s2); }
    }
    if (th) { WaitForSingleObject(th, 8000); CloseHandle(th); }
    gfx_down(&g);
}

/* ---- RpcServerUseAllProtseqs / Ex in a child process ---------------------------------------------------------------- */
static void run_allp(const char *mode) {
    WCHAR ep[64]; PROCESS_INFORMATION pi; HANDLE proc; DWORD code = 0xdead0001;
    REQUIRE(U(RpcServerUseAllProtseqs)); REQUIRE(U(RpcServerUseAllProtseqsEx));
    if (t_is_wine()) { t_skip("RpcServerUseAllProtseqs* unimplemented in wine"); return; }
    rx_ep_unique(ep, 64);
    proc = rx_spawn_server(ep, mode, &pi);
    if (!proc) { CHECKF(0, "spawn failed"); return; }
    if (WaitForSingleObject(proc, 20000) != WAIT_OBJECT_0) { TerminateProcess(proc, 1); WaitForSingleObject(proc, 5000); }
    else GetExitCodeProcess(proc, &code);
    CloseHandle(pi.hThread); CloseHandle(proc);
    CHECKF((code & 0xff00) == 0x100, "child exit 0x%lx (0x7101 no bindings, 0x7102 no ncalrpc binding, 0x7103 policy modified, small values = RPC status)", code);
    if ((code & 0xff00) == 0x100) t_note("%s registered protocols: tcp=%lu np=%lu", mode, code & 1, (code >> 1) & 1);
}
TEST(dx_gap4_allprotseqs_registers_ncalrpc_in_child) { run_allp("allp"); }
TEST(dx_gap4_allprotseqsex_registers_and_restores_policy) { run_allp("allpex"); }
