/* RpcSs* / RpcSm* per-thread "stub memory" allocator and the Ndr*Allocate hooks.
 * Semantics from the Windows 10 rpcrt4 decompilation: the allocation context lives in TLS; Enable pushes a
 * nesting count, Disable pops it and releases all RpcSsAllocate blocks when it reaches zero. RpcSsAllocate raises
 * ERROR_INVALID_PARAMETER (0x57) when the thread has no context at all and RPC_S_OUT_OF_MEMORY (0xE) when the
 * context exists but is not enabled or the request is >= 0x80000000. */
#include "epx.h"

#define XCODE_INVALID_PARAMETER 0x57
#define XCODE_OUT_OF_MEMORY     0x0E

typedef void *(WINAPI *alloc_pfn)(size_t);
typedef void (WINAPI *raisefn_pfn)(RPC_STATUS);

static struct {
    size_t size; void *res;
    RPC_STATUS st; RPC_STATUS st_in;
    void *a, *b;
} G;

static void do_ssalloc(void *p) { (void)p; G.res = U(RpcSsAllocate)(G.size); }
static void do_smalloc(void *p) { (void)p; G.res = U(RpcSmAllocate)(G.size, &G.st); }
static DWORD try_ssalloc(size_t n) { G.size = n; G.res = NULL; return epx_try(do_ssalloc, NULL); }

/* ---------- thread bodies (each runs on a fresh thread so TLS state never leaks between tests) ---------- */
static void t_noenable(void *p) {
    DWORD c = try_ssalloc(16);
    *(DWORD *)p = c;
}
TEST(epm_alloc_ss_alloc_without_context_raises_invalid_parameter) {
    DWORD code = 0xFFFFFFFF;
    REQUIRE(U(RpcSsAllocate));
    WINE_STUB();
    CHECK(epx_run(t_noenable, &code, 10000));
    EXPECT_EQ(code, XCODE_INVALID_PARAMETER);
    CHECK(G.res == NULL);
}

static void t_basic(void *p) {
    unsigned char *a, *b, *c; int i; DWORD *ok = p;
    *ok = 0;
    U(RpcSsEnableAllocate)();
    a = U(RpcSsAllocate)(1); b = U(RpcSsAllocate)(8); c = U(RpcSsAllocate)(100);
    if (!a || !b || !c) goto out;
    if ((ULONG_PTR)a & 7 || (ULONG_PTR)b & 7 || (ULONG_PTR)c & 7) { *ok = 2; goto out; }
    memset(a, 0xA1, 1); memset(b, 0xB2, 8); memset(c, 0xC3, 100);
    for (i = 0; i < 8; i++) if (b[i] != 0xB2) goto out;
    for (i = 0; i < 100; i++) if (c[i] != 0xC3) goto out;
    if (a == b || b == c || a == c) goto out;
    *ok = 1;
out:
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_enable_allocate_disable) {
    DWORD ok = 0;
    REQUIRE(U(RpcSsEnableAllocate) && U(RpcSsAllocate) && U(RpcSsDisableAllocate));
    WINE_STUB();
    CHECK(epx_run(t_basic, &ok, 10000));
    EXPECT_EQ(ok, 1);   /* 2 would mean a returned block was not 8-byte aligned */
}

static void t_sizes(void *p) {
    static const size_t sizes[] = {0, 1, 7, 8, 9, 15, 16, 17, 255, 256, 1000, 4095, 4096, 4097, 65535, 65536, 1048576};
    unsigned char *prev[32]; unsigned i, j; DWORD *bad = p;
    *bad = 0;
    U(RpcSsEnableAllocate)();
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        unsigned char *q;
        G.size = sizes[i]; G.res = NULL;
        if (epx_try(do_ssalloc, NULL)) { *bad |= 1; continue; }
        q = G.res;
        if (!q) { *bad |= 2; continue; }
        if ((ULONG_PTR)q & 7) *bad |= 4;
        if (sizes[i]) { q[0] = 0x5A; q[sizes[i] - 1] = 0x5B; }   /* both ends writable */
        for (j = 0; j < i; j++) if (prev[j] == q) *bad |= 8;
        prev[i] = q;
    }
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_alloc_size_ladder) {
    DWORD bad = 0xFF;
    REQUIRE(U(RpcSsAllocate));
    WINE_STUB();
    CHECK(epx_run(t_sizes, &bad, 15000));
    EXPECT_EQ(bad, 0);
}

static void t_zero(void *p) {
    DWORD *ok = p; void *a, *b;
    *ok = 0;
    U(RpcSsEnableAllocate)();
    a = U(RpcSsAllocate)(0); b = U(RpcSsAllocate)(0);
    *ok = (a && b && a != b) ? 1 : 0;   /* size 0 is rounded up to 8 */
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_alloc_zero_bytes_returns_distinct_blocks) {
    DWORD ok = 0;
    REQUIRE(U(RpcSsAllocate));
    WINE_STUB();
    CHECK(epx_run(t_zero, &ok, 10000));
    EXPECT_EQ(ok, 1);
}

static void t_huge(void *p) {
    DWORD *r = p; DWORD c;
    U(RpcSsEnableAllocate)();
    c = try_ssalloc(0x80000000u);   r[0] = c;       /* >= 2 GiB is refused before any allocation is attempted */
    c = try_ssalloc(0xFFFFFFFFu);   r[1] = c;
    c = try_ssalloc((size_t)0x100000000ull); r[2] = c;
    c = try_ssalloc(64);            r[3] = c;       /* the context is still usable afterwards */
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_alloc_oversize_raises_out_of_memory) {
    DWORD r[4] = {1, 1, 1, 1};
    REQUIRE(U(RpcSsAllocate));
    WINE_STUB();
    CHECK(epx_run(t_huge, r, 10000));
    EXPECT_EQ(r[0], XCODE_OUT_OF_MEMORY);
    EXPECT_EQ(r[1], XCODE_OUT_OF_MEMORY);
    EXPECT_EQ(r[2], XCODE_OUT_OF_MEMORY);
    EXPECT_EQ(r[3], 0);
}

static void t_handle(void *p) {
    void **r = p;
    r[0] = U(RpcSsGetThreadHandle)();           /* no context yet */
    U(RpcSsEnableAllocate)();
    r[1] = U(RpcSsGetThreadHandle)();
    r[2] = U(RpcSsGetThreadHandle)();
    U(RpcSsDisableAllocate)();
    r[3] = U(RpcSsGetThreadHandle)();
}
TEST(epm_alloc_ss_thread_handle_tracks_enable_state) {
    void *r[4] = {(void *)1, 0, 0, (void *)1};
    REQUIRE(U(RpcSsGetThreadHandle));
    WINE_STUB();
    CHECK(epx_run(t_handle, r, 10000));
    CHECK(r[0] == NULL);
    CHECK(r[1] != NULL);
    CHECK(r[1] == r[2]);
    CHECK(r[3] == NULL);
}

static void t_nested(void *p) {
    void **r = p;
    U(RpcSsEnableAllocate)();
    U(RpcSsEnableAllocate)();
    U(RpcSsDisableAllocate)();
    r[0] = U(RpcSsGetThreadHandle)();     /* still enabled: count 2 -> 1 */
    G.size = 24; r[1] = (void *)(ULONG_PTR)epx_try(do_ssalloc, NULL);
    U(RpcSsDisableAllocate)();
    r[2] = U(RpcSsGetThreadHandle)();     /* count 0 */
    r[3] = (void *)(ULONG_PTR)try_ssalloc(24);   /* context exists but is disabled: out of memory */
}
TEST(epm_alloc_ss_enable_is_reference_counted) {
    void *r[4] = {0, (void *)9, (void *)1, 0};
    REQUIRE(U(RpcSsEnableAllocate));
    WINE_STUB();
    CHECK(epx_run(t_nested, r, 10000));
    CHECK(r[0] != NULL);
    EXPECT_EQ((ULONG_PTR)r[1], 0);
    CHECK(r[2] == NULL);
    EXPECT_EQ((ULONG_PTR)r[3], XCODE_OUT_OF_MEMORY);
}

static void t_disable_only(void *p) {
    U(RpcSsDisableAllocate)();
    U(RpcSsDisableAllocate)();
    *(DWORD *)p = 1;
}
TEST(epm_alloc_ss_disable_without_enable_is_harmless) {
    DWORD ok = 0;
    REQUIRE(U(RpcSsDisableAllocate));
    WINE_STUB();
    CHECK(epx_run(t_disable_only, &ok, 10000));
    EXPECT_EQ(ok, 1);
}

static void t_reenable(void *p) {
    DWORD *r = p; void *h1, *h2;
    U(RpcSsEnableAllocate)(); h1 = U(RpcSsGetThreadHandle)(); U(RpcSsDisableAllocate)();
    U(RpcSsEnableAllocate)(); h2 = U(RpcSsGetThreadHandle)();
    r[0] = h1 != NULL; r[1] = h2 != NULL;
    r[2] = try_ssalloc(40);
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_enable_disable_cycles) {
    DWORD r[3] = {0, 0, 9};
    REQUIRE(U(RpcSsEnableAllocate));
    WINE_STUB();
    CHECK(epx_run(t_reenable, r, 10000));
    EXPECT_EQ(r[0], 1); EXPECT_EQ(r[1], 1); EXPECT_EQ(r[2], 0);
}

static void t_many(void *p) {
    enum { N = 3000 };
    unsigned char **v; int i, j; DWORD *bad = p;
    *bad = 0;
    v = HeapAlloc(GetProcessHeap(), 0, N * sizeof *v);
    if (!v) { *bad = 99; return; }
    U(RpcSsEnableAllocate)();
    for (i = 0; i < N; i++) {
        v[i] = U(RpcSsAllocate)(24 + (i % 5));
        if (!v[i]) { *bad |= 1; break; }
        memset(v[i], i & 0xFF, 24);
    }
    for (i = 0; i < N && !*bad; i++) for (j = 0; j < 24; j++) if (v[i][j] != (i & 0xFF)) { *bad |= 2; break; }
    U(RpcSsDisableAllocate)();
    HeapFree(GetProcessHeap(), 0, v);
}
TEST(epm_alloc_ss_many_small_blocks_do_not_overlap) {
    DWORD bad = 0xFF;
    REQUIRE(U(RpcSsAllocate));
    WINE_STUB();
    CHECK(epx_run(t_many, &bad, 20000));
    EXPECT_EQ(bad, 0);
}

static void t_free(void *p) {
    DWORD *r = p; unsigned char *a;
    U(RpcSsEnableAllocate)();
    a = U(RpcSsAllocate)(32); memset(a, 0x77, 32);
    U(RpcSsFree)(a);               /* blocks are only released by Disable: the memory is still readable */
    r[0] = a[0] == 0x77 && a[31] == 0x77;
    U(RpcSsDisableAllocate)();
    r[1] = 1;
}
TEST(epm_alloc_ss_free_keeps_block_until_disable) {
    DWORD r[2] = {0, 0};
    REQUIRE(U(RpcSsFree));
    WINE_STUB();
    CHECK(epx_run(t_free, r, 10000));
    EXPECT_EQ(r[0], 1); EXPECT_EQ(r[1], 1);
}

/* ---- client alloc/free hooks ---- */
static void *WINAPI fake_a1(size_t n) { return NULL; }
static void WINAPI fake_f1(void *p) {}
static void *WINAPI fake_a2(size_t n) { return NULL; }
static void WINAPI fake_f2(void *p) {}

static void t_swap(void *p) {
    void **r = p; void *o1a = 0, *o1f = 0, *o2a = 0, *o2f = 0, *o3a = 0, *o3f = 0;
    U(RpcSsSwapClientAllocFree)((RPC_CLIENT_ALLOC *)fake_a1, (RPC_CLIENT_FREE *)fake_f1, (RPC_CLIENT_ALLOC **)&o1a, (RPC_CLIENT_FREE **)&o1f);
    U(RpcSsSwapClientAllocFree)((RPC_CLIENT_ALLOC *)fake_a2, (RPC_CLIENT_FREE *)fake_f2, (RPC_CLIENT_ALLOC **)&o2a, (RPC_CLIENT_FREE **)&o2f);
    U(RpcSsSwapClientAllocFree)((RPC_CLIENT_ALLOC *)o1a, (RPC_CLIENT_FREE *)o1f, (RPC_CLIENT_ALLOC **)&o3a, (RPC_CLIENT_FREE **)&o3f);
    r[0] = o1a; r[1] = o1f; r[2] = o2a; r[3] = o2f; r[4] = o3a; r[5] = o3f;
    U(RpcSsEnableAllocate)(); U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_swap_client_alloc_free_returns_previous_pair) {
    void *r[6] = {0};
    REQUIRE(U(RpcSsSwapClientAllocFree));
    WINE_STUB();
    CHECK(epx_run(t_swap, r, 10000));
    CHECK(r[0] != NULL && r[1] != NULL);        /* a fresh context starts with the runtime's default hooks */
    CHECK(r[2] == (void *)fake_a1);
    CHECK(r[3] == (void *)fake_f1);
    CHECK(r[4] == (void *)fake_a2);
    CHECK(r[5] == (void *)fake_f2);
}

static void t_set_then_alloc(void *p) {
    DWORD *r = p; void *oa = 0, *of = 0;
    U(RpcSsSetClientAllocFree)((RPC_CLIENT_ALLOC *)fake_a1, (RPC_CLIENT_FREE *)fake_f1);
    r[0] = (DWORD)(ULONG_PTR)U(RpcSsGetThreadHandle)();           /* created but not enabled -> NULL */
    r[1] = try_ssalloc(16);                                          /* not enabled -> out of memory */
    U(RpcSsSwapClientAllocFree)(NULL, NULL, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r[2] = oa == (void *)fake_a1 && of == (void *)fake_f1;
}
TEST(epm_alloc_ss_set_client_hooks_creates_disabled_context) {
    DWORD r[3] = {9, 9, 0};
    REQUIRE(U(RpcSsSetClientAllocFree));
    WINE_STUB();
    CHECK(epx_run(t_set_then_alloc, r, 10000));
    EXPECT_EQ(r[0], 0);
    EXPECT_EQ(r[1], XCODE_OUT_OF_MEMORY);
    EXPECT_EQ(r[2], 1);
}

static void t_enable_installs(void *p) {
    void **r = p; void *oa = 0, *of = 0;
    U(RpcSsEnableAllocate)();
    U(RpcSsSwapClientAllocFree)(NULL, NULL, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r[0] = oa; r[1] = of;
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_enable_installs_rpcss_hooks) {
    void *r[2] = {0};
    REQUIRE(U(RpcSsSwapClientAllocFree) && U(RpcSsAllocate) && U(RpcSsFree));
    WINE_STUB();
    CHECK(epx_run(t_enable_installs, r, 10000));
    CHECK(r[0] == (void *)U(RpcSsAllocate));
    CHECK(r[1] == (void *)U(RpcSsFree));
}

/* ---- thread handles shared between threads ---- */
static HANDLE g_ev_a, g_ev_b;
static void *g_shared; static DWORD g_res[4];
static DWORD WINAPI th_b(void *p) {
    void *h;
    WaitForSingleObject(g_ev_a, 8000);
    g_res[0] = U(RpcSsGetThreadHandle)() == NULL;    /* B has no context of its own */
    U(RpcSsSetThreadHandle)(g_shared);                /* adopt A's context */
    h = U(RpcSsGetThreadHandle)();
    g_res[1] = (h == g_shared);
    g_res[2] = try_ssalloc(48) == 0 && G.res != NULL;
    U(RpcSsSetThreadHandle)(NULL);                    /* drop it */
    g_res[3] = U(RpcSsGetThreadHandle)() == NULL;
    SetEvent(g_ev_b);
    return 0;
}
static void t_share_a(void *p) {
    HANDLE th;
    U(RpcSsEnableAllocate)();
    g_shared = U(RpcSsGetThreadHandle)();
    th = CreateThread(NULL, 0, th_b, NULL, 0, NULL);
    SetEvent(g_ev_a);
    WaitForSingleObject(g_ev_b, 8000);
    WaitForSingleObject(th, 8000);
    CloseHandle(th);
    *(void **)p = U(RpcSsGetThreadHandle)();           /* A is unaffected by B's release */
    U(RpcSsDisableAllocate)();
}
TEST(epm_alloc_ss_set_thread_handle_adopts_context) {
    void *a_after = NULL;
    REQUIRE(U(RpcSsSetThreadHandle) && U(RpcSsGetThreadHandle));
    WINE_STUB();
    g_ev_a = CreateEventA(NULL, FALSE, FALSE, NULL); g_ev_b = CreateEventA(NULL, FALSE, FALSE, NULL);
    memset(g_res, 0, sizeof g_res); g_shared = NULL;
    CHECK(epx_run(t_share_a, &a_after, 20000));
    CHECK(g_shared != NULL);
    EXPECT_EQ(g_res[0], 1); EXPECT_EQ(g_res[1], 1); EXPECT_EQ(g_res[2], 1); EXPECT_EQ(g_res[3], 1);
    CHECK(a_after == g_shared);
    CloseHandle(g_ev_a); CloseHandle(g_ev_b);
}

/* ---------------- RpcSm* : status-returning wrappers ---------------- */
static void t_sm(void *p) {
    DWORD *r = p; RPC_STATUS st; void *h; void *mem;
    memset(r, 0xFF, 12 * sizeof(DWORD));
    r[0] = U(RpcSmEnableAllocate)();
    st = 0xDEAD; mem = U(RpcSmAllocate)(64, &st);
    r[1] = st; r[2] = mem != NULL;
    st = 0xDEAD; h = U(RpcSmGetThreadHandle)(&st);
    r[3] = st; r[4] = h != NULL;
    r[5] = U(RpcSmFree)(mem);
    r[6] = U(RpcSmDisableAllocate)();
    st = 0xDEAD; h = U(RpcSmGetThreadHandle)(&st);
    r[7] = st; r[8] = h == NULL;
    r[9] = U(RpcSmSetThreadHandle)(NULL);
}
TEST(epm_alloc_sm_wrappers_return_success_status) {
    DWORD r[12];
    REQUIRE(U(RpcSmEnableAllocate) && U(RpcSmAllocate) && U(RpcSmGetThreadHandle) && U(RpcSmFree) && U(RpcSmDisableAllocate) && U(RpcSmSetThreadHandle));
    WINE_STUB();
    CHECK(epx_run(t_sm, r, 10000));
    EXPECT_EQ(r[0], 0); EXPECT_EQ(r[1], 0); EXPECT_EQ(r[2], 1); EXPECT_EQ(r[3], 0); EXPECT_EQ(r[4], 1);
    EXPECT_EQ(r[5], 0); EXPECT_EQ(r[6], 0); EXPECT_EQ(r[7], 0); EXPECT_EQ(r[8], 1); EXPECT_EQ(r[9], 0);
}

static void t_sm_noenable(void *p) {
    RPC_STATUS st = 0xBEEF;
    G.size = 16; G.res = NULL; G.st = 0xBEEF;
    *(DWORD *)p = epx_try(do_smalloc, NULL);
}
TEST(epm_alloc_sm_alloc_without_context_raises_and_leaves_status) {
    DWORD code = 0;
    REQUIRE(U(RpcSmAllocate));
    WINE_STUB();
    CHECK(epx_run(t_sm_noenable, &code, 10000));
    EXPECT_EQ(code, XCODE_INVALID_PARAMETER);
    EXPECT_EQ(G.st, 0xBEEF);       /* *pStatus is written only after a successful allocation */
}

static void t_sm_hooks(void *p) {
    DWORD *r = p; void *oa = 0, *of = 0;
    r[0] = U(RpcSmSetClientAllocFree)((RPC_CLIENT_ALLOC *)fake_a1, (RPC_CLIENT_FREE *)fake_f1);
    r[1] = U(RpcSmSwapClientAllocFree)((RPC_CLIENT_ALLOC *)fake_a2, (RPC_CLIENT_FREE *)fake_f2, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r[2] = oa == (void *)fake_a1 && of == (void *)fake_f1;
}
TEST(epm_alloc_sm_client_hooks_return_success_and_swap) {
    DWORD r[3] = {9, 9, 0};
    REQUIRE(U(RpcSmSetClientAllocFree) && U(RpcSmSwapClientAllocFree));
    WINE_STUB();
    CHECK(epx_run(t_sm_hooks, r, 10000));
    EXPECT_EQ(r[0], 0); EXPECT_EQ(r[1], 0); EXPECT_EQ(r[2], 1);
}

TEST(epm_alloc_sm_free_and_client_free_accept_null) {
    REQUIRE(U(RpcSmFree) && U(RpcSmClientFree));
    WINE_STUB();
    EXPECT_EQ(U(RpcSmFree)(NULL), 0);
    EXPECT_EQ(U(RpcSmClientFree)(NULL), 0);
}

/* ---------------- Ndr* hooks ---------------- */
static void t_ndr_enable(void *p) {
    void **r = p; MIDL_STUB_MESSAGE m; void *q;
    memset(&m, 0, sizeof m);
    U(NdrRpcSsEnableAllocate)(&m);
    r[0] = (void *)m.pfnAllocate; r[1] = (void *)m.pfnFree;
    r[2] = U(RpcSsGetThreadHandle)();
    q = m.pfnAllocate(40);
    r[3] = q;
    U(NdrRpcSsDisableAllocate)(&m);
    r[4] = (void *)m.pfnAllocate; r[5] = (void *)m.pfnFree;
    r[6] = U(RpcSsGetThreadHandle)();
}
TEST(epm_alloc_ndr_ss_enable_disable_swap_stub_hooks) {
    void *r[7] = {0};
    REQUIRE(U(NdrRpcSsEnableAllocate) && U(NdrRpcSsDisableAllocate) && U(RpcSsAllocate) && U(RpcSsFree) && U(NdrRpcSsDefaultAllocate) && U(NdrRpcSsDefaultFree));
    WINE_STUB();
    CHECK(epx_run(t_ndr_enable, r, 10000));
    CHECK(r[0] == (void *)U(RpcSsAllocate));
    CHECK(r[1] == (void *)U(RpcSsFree));
    CHECK(r[2] != NULL);
    CHECK(r[3] != NULL);
    CHECK(r[4] == (void *)U(NdrRpcSsDefaultAllocate));
    CHECK(r[5] == (void *)U(NdrRpcSsDefaultFree));
    CHECK(r[6] == NULL);
}

TEST(epm_alloc_ndr_ss_default_allocate_free_roundtrip) {
    unsigned char *p; int i;
    REQUIRE(U(NdrRpcSsDefaultAllocate) && U(NdrRpcSsDefaultFree));
    WINE_STUB();
    p = U(NdrRpcSsDefaultAllocate)(200);
    CHECK(p != NULL);
    if (p) { for (i = 0; i < 200; i++) p[i] = (unsigned char)i; for (i = 0; i < 200; i++) if (p[i] != (unsigned char)i) { CHECK(0); break; } U(NdrRpcSsDefaultFree)(p); }
    p = U(NdrRpcSsDefaultAllocate)(0);
    CHECK(p != NULL);
    if (p) U(NdrRpcSsDefaultFree)(p);
}

static void t_sm_client(void *p) {
    void **r = p; MIDL_STUB_MESSAGE m; void *q;
    memset(&m, 0, sizeof m);
    U(NdrRpcSmSetClientToOsf)(&m);
    r[0] = (void *)m.pfnAllocate; r[1] = (void *)m.pfnFree;
    q = U(NdrRpcSmClientAllocate)(64);       /* no context on this thread: default allocator */
    r[2] = q;
    if (q) U(NdrRpcSmClientFree)(q);
}
TEST(epm_alloc_ndr_sm_client_allocate_without_context) {
    void *r[3] = {0};
    REQUIRE(U(NdrRpcSmSetClientToOsf) && U(NdrRpcSmClientAllocate) && U(NdrRpcSmClientFree));
    WINE_STUB();
    CHECK(epx_run(t_sm_client, r, 10000));
    CHECK(r[0] == (void *)U(NdrRpcSmClientAllocate));
    CHECK(r[1] == (void *)U(NdrRpcSmClientFree));
    CHECK(r[2] != NULL);
}

/* ---------------- I_RpcAllocate / I_RpcFree ---------------- */
TEST(epm_alloc_i_rpc_allocate_free) {
    unsigned char *p; unsigned n;
    REQUIRE(U(I_RpcAllocate) && U(I_RpcFree));
    for (n = 0; n < 5; n++) {
        static const unsigned sz[] = {0, 1, 100, 4096, 1 << 20};
        p = U(I_RpcAllocate)(sz[n]);
        CHECK(p != NULL);                                  /* HeapAlloc semantics: size 0 still yields a pointer */
        if (p && sz[n]) { p[0] = 1; p[sz[n] - 1] = 2; }
        if (p) U(I_RpcFree)(p);
    }
}

/* ---------------- context-handle helpers ---------------- */
TEST(epm_alloc_ss_get_context_binding_rejects_foreign_block) {
    RPC_BINDING_HANDLE b = (RPC_BINDING_HANDLE)(ULONG_PTR)0x1234;
    REQUIRE(U(RpcSsGetContextBinding));
    WINE_STUB();
    EXPECT_EQ(U(RpcSsGetContextBinding)(epx_fake, &b), RPC_S_INVALID_ARG);
    EXPECT_EQ((ULONG_PTR)b, 0x1234);       /* *Binding untouched on failure */
}

