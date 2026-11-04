/* gap_04: RpcServerInterfaceGroupCreate{A,W}, Close, InqBindings, Deactivate -- everything that does NOT activate a
 * group (activation registers with the endpoint mapper: see dx_gap_04).
 *
 * RPC_SERVER::CreateInterfaceGroup (Win10) returns ERROR_INVALID_PARAMETER (87) when: NumIfs == 0, NumEndpoints == 0,
 * Interfaces/Endpoints/IfGroup NULL, IdleCallback NULL with IdlePeriod != INFINITE, any interface template Version != 0,
 * any endpoint template Version & ~0x8000 != 0, an endpoint ProtSeq of "ncadg_ip_udp" or "ncacn_http", or an endpoint
 * with Version 0x8000 and a NULL or empty Endpoint. On failure *IfGroup is not written. */
#include "g4_group.h"

static void RPC_ENTRY g_idle(void *g, void *c, ULONG i) { (void)g; (void)c; (void)i; }

typedef struct { rx_raw r; G4_IFT_W ift; G4_EPT_W ept; WCHAR ep[64]; } gfx;
static void gfx_init(gfx *g, unsigned n) {
    UUID u;
    memset(g, 0, sizeof *g);
    rx_raw_uuid(&u, n); rx_raw_init(&g->r, &u, 1, 0);
    rx_ep_unique(g->ep, 64);
    g->ift.IfSpec = (RPC_IF_HANDLE)&g->r.si; g->ift.MaxCalls = RPC_C_LISTEN_MAX_CALLS_DEFAULT; g->ift.MaxRpcSize = 0xFFFFFFFF;
    g->ept.ProtSeq = L"ncalrpc"; g->ept.Endpoint = g->ep; g->ept.Backlog = RPC_C_PROTSEQ_MAX_REQS_DEFAULT;
}
#define SENT ((void *)(ULONG_PTR)0x5a5a5a5a)

TEST(gap4_group_create_close_w) {
    gfx g; void *grp = SENT;
    G4_NEED_GROUP();
    gfx_init(&g, 0xD1);
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    CHECK(grp != NULL && grp != SENT);
    EQ(G4_Close(grp), RPC_S_OK);
}
TEST(gap4_group_create_close_a) {
    gfx g; G4_IFT_A ia; G4_EPT_A ea; char epa[64]; void *grp = SENT;
    G4_NEED_GROUP();
    gfx_init(&g, 0xD2);
    memset(&ia, 0, sizeof ia); memset(&ea, 0, sizeof ea);
    rx_epa(epa, sizeof epa);
    ia.IfSpec = (RPC_IF_HANDLE)&g.r.si; ia.MaxCalls = RPC_C_LISTEN_MAX_CALLS_DEFAULT; ia.MaxRpcSize = 0xFFFFFFFF;
    ea.ProtSeq = "ncalrpc"; ea.Endpoint = epa; ea.Backlog = RPC_C_PROTSEQ_MAX_REQS_DEFAULT;
    EQ(G4_CreateA(&ia, 1, &ea, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    CHECK(grp != NULL && grp != SENT);
    EQ(G4_Close(grp), RPC_S_OK);
}
TEST(gap4_group_create_distinct_groups) {
    gfx a, b; void *ga = NULL, *gb = NULL;
    G4_NEED_GROUP();
    gfx_init(&a, 0xD3); gfx_init(&b, 0xD4);
    EQ(G4_CreateW(&a.ift, 1, &a.ept, 1, INFINITE, NULL, NULL, &ga), RPC_S_OK);
    EQ(G4_CreateW(&b.ift, 1, &b.ept, 1, INFINITE, NULL, NULL, &gb), RPC_S_OK);
    CHECK(ga && gb && ga != gb);
    EQ(G4_Close(gb), RPC_S_OK); EQ(G4_Close(ga), RPC_S_OK);
}
TEST(gap4_group_create_same_templates_twice) {
    gfx a; void *g1 = NULL, *g2 = NULL;
    G4_NEED_GROUP();
    gfx_init(&a, 0xD5);
    EQ(G4_CreateW(&a.ift, 1, &a.ept, 1, INFINITE, NULL, NULL, &g1), RPC_S_OK);
    EQ(G4_CreateW(&a.ift, 1, &a.ept, 1, INFINITE, NULL, NULL, &g2), RPC_S_OK);    /* creation does not register anything yet */
    CHECK(g1 != g2);
    EQ(G4_Close(g1), RPC_S_OK); EQ(G4_Close(g2), RPC_S_OK);
}
TEST(gap4_group_create_with_idle_callback) {
    gfx g; void *grp = NULL; static const ULONG periods[] = { 0, 1, 30, 3600, 0xFFFFFFFE };
    unsigned k;
    G4_NEED_GROUP();
    gfx_init(&g, 0xD6);
    for (k = 0; k < sizeof periods / sizeof *periods; k++) {
        grp = NULL;
        EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, periods[k], g_idle, (void *)0x1234, &grp), RPC_S_OK);
        CHECK(grp != NULL);
        if (grp) EQ(G4_Close(grp), RPC_S_OK);
    }
    /* INFINITE with a callback is also fine */
    grp = NULL;
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, g_idle, NULL, &grp), RPC_S_OK);
    if (grp) G4_Close(grp);
}
TEST(gap4_group_create_idle_period_requires_callback) {
    gfx g; void *grp = SENT; static const ULONG periods[] = { 0, 1, 1000, 0xFFFFFFFE };
    unsigned k;
    G4_NEED_GROUP();
    gfx_init(&g, 0xD7);
    for (k = 0; k < sizeof periods / sizeof *periods; k++) {
        grp = SENT;
        EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, periods[k], NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
        CHECK(grp == SENT);
    }
}
TEST(gap4_group_create_null_and_zero_arguments) {
    gfx g; void *grp = SENT;
    G4_NEED_GROUP();
    gfx_init(&g, 0xD8);
    EQ(G4_CreateW(&g.ift, 0, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);   /* NumIfs == 0 */
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 0, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);   /* NumEndpoints == 0 */
    EQ(G4_CreateW(NULL, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    EQ(G4_CreateW(&g.ift, 1, NULL, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);   /* IfGroup NULL */
    CHECK(grp == SENT);
}
TEST(gap4_group_create_null_and_zero_arguments_a) {
    gfx g; G4_IFT_A ia; G4_EPT_A ea; void *grp = SENT;
    G4_NEED_GROUP();
    gfx_init(&g, 0xD9);
    memset(&ia, 0, sizeof ia); memset(&ea, 0, sizeof ea);
    ia.IfSpec = (RPC_IF_HANDLE)&g.r.si; ea.ProtSeq = "ncalrpc"; ea.Endpoint = "olet_gap4_x";
    EQ(G4_CreateA(&ia, 0, &ea, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    EQ(G4_CreateA(&ia, 1, &ea, 0, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    /* Win10: the A entry point copies the IfGroup / endpoint arrays before it validates them, so a NULL array with a non-zero
     * count raises an access violation instead of ERROR_INVALID_PARAMETER (the W entry point does return the error). */
    EQ(G4_CreateA(&ia, 1, &ea, 1, INFINITE, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(G4_CreateA(&ia, 1, &ea, 1, 0, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    CHECK(grp == SENT);
}
TEST(gap4_group_create_template_versions) {
    gfx g; void *grp; static const ULONG ifv[] = { 1, 2, 0x8000, 0xFFFFFFFF };
    static const ULONG epbad[] = { 1, 2, 0x4000, 0x8001, 0x10000, 0xFFFFFFFF };
    unsigned k;
    G4_NEED_GROUP();
    gfx_init(&g, 0xDA);
    for (k = 0; k < sizeof ifv / sizeof *ifv; k++) {
        grp = SENT; g.ift.Version = ifv[k];
        EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
        CHECK(grp == SENT);
    }
    g.ift.Version = 0;
    for (k = 0; k < sizeof epbad / sizeof *epbad; k++) {
        grp = SENT; g.ept.Version = epbad[k];
        EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
        CHECK(grp == SENT);
    }
    g.ept.Version = 0x8000;                              /* the one defined flag, with a named endpoint */
    grp = NULL;
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    if (grp) G4_Close(grp);
}
TEST(gap4_group_create_flagged_endpoint_needs_name) {
    gfx g; void *grp = SENT;
    G4_NEED_GROUP();
    gfx_init(&g, 0xDB);
    g.ept.Version = 0x8000;
    g.ept.Endpoint = NULL;
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    g.ept.Endpoint = L"";
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    CHECK(grp == SENT);
    g.ept.Version = 0;                                   /* without the flag, a dynamic endpoint is fine */
    g.ept.Endpoint = NULL;
    grp = NULL;
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    if (grp) G4_Close(grp);
}
TEST(gap4_group_create_unsupported_protseqs) {
    gfx g; void *grp = SENT; static const WCHAR *const bad[] = { L"ncadg_ip_udp", L"ncacn_http" };
    unsigned k;
    G4_NEED_GROUP();
    gfx_init(&g, 0xDC);
    for (k = 0; k < 2; k++) {
        g.ept.ProtSeq = bad[k];
        EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    }
    CHECK(grp == SENT);
}
TEST(gap4_group_create_supported_protseqs_not_activated) {
    gfx g; void *grp; static const WCHAR *const ok[] = { L"ncalrpc", L"ncacn_np", L"ncacn_ip_tcp" };
    unsigned k;
    G4_NEED_GROUP();
    gfx_init(&g, 0xDD);
    for (k = 0; k < 3; k++) {
        grp = NULL; g.ept.ProtSeq = ok[k]; g.ept.Endpoint = k == 1 ? L"\\pipe\\olet_gap4_unused" : (k == 2 ? NULL : g.ep);
        EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);     /* creation only validates the templates */
        if (grp) G4_Close(grp);
    }
    /* an unknown protocol sequence is only detected on activation */
    grp = NULL; g.ept.ProtSeq = L"bogus_protseq"; g.ept.Endpoint = NULL;
    { RPC_STATUS st = G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp); t_note("Create with unknown protseq -> %lu", (unsigned long)st); if (st == 0 && grp) G4_Close(grp); }
}
TEST(gap4_group_create_multiple_interfaces_and_endpoints) {
    gfx a, b; G4_IFT_W ifs[2]; G4_EPT_W eps[3]; void *grp = NULL;
    G4_NEED_GROUP();
    gfx_init(&a, 0xDE); gfx_init(&b, 0xDF);
    ifs[0] = a.ift; ifs[1] = b.ift;
    eps[0] = a.ept; eps[1] = b.ept; eps[2] = a.ept; eps[2].Endpoint = NULL;
    EQ(G4_CreateW(ifs, 2, eps, 3, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    CHECK(grp != NULL);
    if (grp) EQ(G4_Close(grp), RPC_S_OK);
}
TEST(gap4_group_create_second_endpoint_invalid_detected) {
    gfx a; G4_EPT_W eps[2]; void *grp = SENT;
    G4_NEED_GROUP();
    gfx_init(&a, 0xE0);
    eps[0] = a.ept; eps[1] = a.ept; eps[1].ProtSeq = L"ncacn_http";
    EQ(G4_CreateW(&a.ift, 1, eps, 2, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);   /* every element is validated */
    eps[1].ProtSeq = L"ncalrpc"; eps[1].Version = 0x8000; eps[1].Endpoint = NULL;
    EQ(G4_CreateW(&a.ift, 1, eps, 2, INFINITE, NULL, NULL, &grp), ERROR_INVALID_PARAMETER);
    CHECK(grp == SENT);
}
TEST(gap4_group_create_annotation_and_flags) {
    gfx g; void *grp = NULL; G4_IFT_W t;
    G4_NEED_GROUP();
    gfx_init(&g, 0xE1);
    t = g.ift; t.Annotation = L"olet gap4 annotation"; t.Flags = RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH | RPC_IF_AUTOLISTEN; t.MaxCalls = 5; t.MaxRpcSize = 4096;
    EQ(G4_CreateW(&t, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    if (grp) G4_Close(grp);
}

/* ---- never-activated groups ----------------------------------------------------------------------------- */
TEST(gap4_group_inqbindings_before_activation) {
    gfx g; void *grp = NULL; RPC_BINDING_VECTOR *v = (RPC_BINDING_VECTOR *)SENT;
    G4_NEED_GROUP();
    gfx_init(&g, 0xE2);
    EQ(G4_CreateW(&g.ift, 1, &g.ept, 1, INFINITE, NULL, NULL, &grp), RPC_S_OK);
    EQ(G4_InqBindings(grp, &v), RPC_S_NO_BINDINGS);              /* no endpoints exist until activation */
    CHECK(v == (RPC_BINDING_VECTOR *)SENT);                      /* output untouched on failure */
    EQ(G4_Close(grp), RPC_S_OK);
}
TEST(gap4_group_deactivate_null_group) {
    G4_NEED_GROUP();
    EQ(G4_Deactivate(NULL, FALSE), ERROR_INVALID_PARAMETER);
    EQ(G4_Deactivate(NULL, TRUE), ERROR_INVALID_PARAMETER);
}
