/* RpcSm* / RpcSs* client-allocator hooks, thread handles and enable counts (rpcrt4).
 * Decomp-verified model (rpcrt4 19041): a per-thread (TLS) ALLOCATION_CONTEXT holds {alloc, free} hooks (initially the
 * runtime's DefaultAllocate/DefaultFree), an enable count, and a context reference count (starts at 1).
 *   Set/SwapClientAllocFree create the context on demand (count 0) and only exchange the hook pair;
 *   Enable (count 0 -> 1) installs RpcSsAllocate/RpcSsFree as the hooks and Disable never restores the old ones;
 *   SetThreadHandle(other) drops the current context (freed outright when its reference count is 1) and adopts `other`;
 *   NdrRpcSmClientAllocate/Free (and RpcSmClientFree) simply call the current hooks, defaults when the thread has no context.
 * The status-returning RpcSm* wrappers always return RPC_S_OK. */
#include "gap7.h"

typedef void *(__RPC_API *a_fn)(size_t);
typedef void (__RPC_API *f_fn)(void *);

static volatile LONG g_a[4], g_f[4];
static void *volatile g_last_free[4];
static volatile size_t g_last_size[4];
static unsigned char g_arena[4][4096];
#define HOOKS(i) \
    static void *__RPC_API ha##i(size_t n) { InterlockedIncrement(&g_a[i]); g_last_size[i] = n; return g_arena[i]; } \
    static void __RPC_API hf##i(void *p) { InterlockedIncrement(&g_f[i]); g_last_free[i] = p; }
HOOKS(0) HOOKS(1) HOOKS(2) HOOKS(3)
static const a_fn HA[4] = { ha0, ha1, ha2, ha3 };
static const f_fn HF[4] = { hf0, hf1, hf2, hf3 };
static void reset_hooks(void) { int i; for (i = 0; i < 4; i++) { g_a[i] = g_f[i] = 0; g_last_free[i] = (void *)1; g_last_size[i] = 0; } }

typedef struct { void *r[16]; DWORD d[16]; } res;

/* ---------- RpcSmSetClientAllocFree + NdrRpcSmClientAllocate/Free dispatch ---------- */
static void body_dispatch(void *p)
{
    res *r = p; void *m; int i = 0;
    r->d[i++] = U(RpcSmSetClientAllocFree)(HA[0], HF[0]);
    m = U(NdrRpcSmClientAllocate)(24);
    r->d[i++] = m == g_arena[0]; r->d[i++] = g_a[0]; r->d[i++] = (DWORD)g_last_size[0];
    U(NdrRpcSmClientFree)(m);
    r->d[i++] = g_f[0]; r->d[i++] = g_last_free[0] == g_arena[0];
    r->d[i++] = U(RpcSmClientFree)(m);
    r->d[i++] = g_f[0];
    r->d[i++] = U(RpcSmClientFree)(NULL);
    r->d[i++] = g_f[0]; r->d[i++] = g_last_free[0] == NULL;
    m = U(NdrRpcSmClientAllocate)(0);
    r->d[i++] = g_a[0]; r->d[i++] = (DWORD)g_last_size[0];
}
TEST(gap7_stubmem_client_hooks_dispatch_allocate_and_free)
{
    res r; DWORD x;
    REQUIRE(U(RpcSmSetClientAllocFree)); REQUIRE(U(NdrRpcSmClientAllocate)); REQUIRE(U(NdrRpcSmClientFree)); REQUIRE(U(RpcSmClientFree));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    reset_hooks(); memset(&r, 0, sizeof r);
    x = g7_run(body_dispatch, &r);
    EQ(x, 0);
    EQ(r.d[0], 0);                         /* RpcSmSetClientAllocFree -> RPC_S_OK */
    EQ(r.d[1], 1); EQ(r.d[2], 1); EQ(r.d[3], 24);
    EQ(r.d[4], 1); EQ(r.d[5], 1);          /* NdrRpcSmClientFree -> hook free with the same pointer */
    EQ(r.d[6], 0); EQ(r.d[7], 2);          /* RpcSmClientFree -> RPC_S_OK, hook free again */
    EQ(r.d[8], 0); EQ(r.d[9], 3); EQ(r.d[10], 1);   /* even NULL reaches the hook */
    EQ(r.d[11], 2); EQ(r.d[12], 0);        /* zero-byte request is passed through unchanged */
}

/* ---------- hooks are per thread ---------- */
static HANDLE g_ev1, g_ev2;
static DWORD WINAPI other_thread(void *p)
{
    res *r = p; void *m;
    WaitForSingleObject(g_ev1, 8000);
    m = U(NdrRpcSmClientAllocate)(40);          /* no context on this thread: defaults */
    r->d[0] = m != NULL; r->d[1] = g_a[0];
    if (m) { memset(m, 0xAB, 40); U(NdrRpcSmClientFree)(m); }
    r->d[2] = g_f[0];
    SetEvent(g_ev2);
    return 0;
}
static void body_perthread(void *p)
{
    res *r = p; HANDLE th = CreateThread(NULL, 0, other_thread, p, 0, NULL);
    U(RpcSmSetClientAllocFree)(HA[0], HF[0]);
    SetEvent(g_ev1);
    WaitForSingleObject(g_ev2, 8000);
    WaitForSingleObject(th, 8000);
    CloseHandle(th);
    r->d[3] = g_a[0];
}
TEST(gap7_stubmem_client_hooks_are_per_thread)
{
    res r;
    REQUIRE(U(RpcSmSetClientAllocFree)); REQUIRE(U(NdrRpcSmClientAllocate));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    reset_hooks(); memset(&r, 0, sizeof r);
    g_ev1 = CreateEventA(NULL, FALSE, FALSE, NULL); g_ev2 = CreateEventA(NULL, FALSE, FALSE, NULL);
    EQ(g7_run(body_perthread, &r), 0);
    EQ(r.d[0], 1);            /* default allocator works */
    EQ(r.d[1], 0);            /* other thread's hook not called */
    EQ(r.d[2], 0);
    EQ(r.d[3], 0);            /* and the hook thread made no allocation itself */
    CloseHandle(g_ev1); CloseHandle(g_ev2);
}

/* ---------- swap sequences ---------- */
static void body_swap(void *p)
{
    res *r = p; void *oa = 0, *of = 0;
    r->d[0] = U(RpcSmSwapClientAllocFree)(HA[0], HF[0], (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[0] = oa; r->r[1] = of;                                /* defaults */
    r->d[1] = U(RpcSmSwapClientAllocFree)(HA[1], HF[1], (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[2] = oa; r->r[3] = of;                                /* pair 0 */
    U(RpcSsSetClientAllocFree)((RPC_CLIENT_ALLOC *)HA[2], (RPC_CLIENT_FREE *)HF[2]);
    r->d[2] = U(RpcSmSwapClientAllocFree)(HA[3], HF[3], (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[4] = oa; r->r[5] = of;                                /* pair 2 (Ss and Sm share the context) */
    U(RpcSsSwapClientAllocFree)((RPC_CLIENT_ALLOC *)HA[0], (RPC_CLIENT_FREE *)HF[0], (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[6] = oa; r->r[7] = of;                                /* pair 3 */
    U(RpcSsSwapClientAllocFree)(NULL, NULL, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[8] = oa; r->r[9] = of;                                /* pair 0 */
    U(RpcSsSwapClientAllocFree)((RPC_CLIENT_ALLOC *)HA[1], (RPC_CLIENT_FREE *)HF[1], (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[10] = oa; r->r[11] = of;                              /* NULL pair */
}
static void body_defaults(void *p)
{
    res *r = p; void *oa = 0, *of = 0;
    U(RpcSmSwapClientAllocFree)(HA[0], HF[0], (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[0] = oa; r->r[1] = of;
}
TEST(gap7_stubmem_swap_chains_previous_pairs_across_ss_and_sm)
{
    res r, d;
    REQUIRE(U(RpcSmSwapClientAllocFree)); REQUIRE(U(RpcSsSwapClientAllocFree)); REQUIRE(U(RpcSsSetClientAllocFree));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    memset(&r, 0, sizeof r); memset(&d, 0, sizeof d);
    EQ(g7_run(body_swap, &r), 0);
    EQ(g7_run(body_defaults, &d), 0);
    EQ(r.d[0], 0); EQ(r.d[1], 0); EQ(r.d[2], 0);
    CHECK(r.r[0] != NULL && r.r[1] != NULL);
    CHECK(r.r[0] == d.r[0] && r.r[1] == d.r[1]);                  /* process-wide default pair, same on every thread */
    CHECK(r.r[2] == (void *)HA[0] && r.r[3] == (void *)HF[0]);
    CHECK(r.r[4] == (void *)HA[2] && r.r[5] == (void *)HF[2]);
    CHECK(r.r[6] == (void *)HA[3] && r.r[7] == (void *)HF[3]);
    CHECK(r.r[8] == (void *)HA[0] && r.r[9] == (void *)HF[0]);
    CHECK(r.r[10] == NULL && r.r[11] == NULL);
}

/* ---------- enable installs RpcSs hooks that survive Disable ---------- */
static void body_enable_hooks(void *p)
{
    res *r = p; void *oa = 0, *of = 0; DWORD code;
    U(RpcSmSetClientAllocFree)(HA[0], HF[0]);
    r->d[0] = U(RpcSmEnableAllocate)();
    U(RpcSsSwapClientAllocFree)(NULL, NULL, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[0] = oa; r->r[1] = of;                                    /* RpcSsAllocate / RpcSsFree */
    U(RpcSsSwapClientAllocFree)((RPC_CLIENT_ALLOC *)oa, (RPC_CLIENT_FREE *)of, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    {   /* the RpcSs hooks really allocate from the enabled context */
        void *m = U(NdrRpcSmClientAllocate)(64);
        r->d[1] = m != NULL;
        if (m) { memset(m, 0x5c, 64); U(NdrRpcSmClientFree)(m); }
    }
    r->d[2] = U(RpcSmDisableAllocate)();
    U(RpcSsSwapClientAllocFree)(NULL, NULL, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[2] = oa; r->r[3] = of;                                    /* still the RpcSs hooks: Disable does not restore */
    r->d[3] = g_a[0];
}
TEST(gap7_stubmem_enable_replaces_client_hooks_and_disable_leaves_them)
{
    res r;
    REQUIRE(U(RpcSmEnableAllocate)); REQUIRE(U(RpcSmDisableAllocate)); REQUIRE(U(RpcSsAllocate)); REQUIRE(U(RpcSsFree));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    reset_hooks(); memset(&r, 0, sizeof r);
    EQ(g7_run(body_enable_hooks, &r), 0);
    EQ(r.d[0], 0); EQ(r.d[1], 1); EQ(r.d[2], 0);
    CHECK(r.r[0] != NULL && r.r[1] != NULL);
    CHECK(r.r[0] != (void *)HA[0] && r.r[1] != (void *)HF[0]);     /* the user pair was replaced by Enable */
    CHECK(r.r[2] == r.r[0] && r.r[3] == r.r[1]);                    /* Disable leaves whatever Enable installed */
    t_note("Enable installs %s RpcSsAllocate and %s RpcSsFree exports", r.r[0] == (void *)U(RpcSsAllocate) ? "the" : "not the", r.r[1] == (void *)U(RpcSsFree) ? "the" : "not the");
    EQ(r.d[3], 0);                                                 /* the earlier user hook was never called */
}
static void body_alloc_after_disable(void *p)
{
    void *m;
    U(RpcSmEnableAllocate)();
    U(RpcSmDisableAllocate)();
    m = U(NdrRpcSmClientAllocate)(16);      /* hooks are RpcSsAllocate, but the context is no longer enabled */
    *(void **)p = m;
}
TEST(gap7_stubmem_client_allocate_after_enable_disable_cycle_raises_out_of_memory)
{
    void *m = (void *)1; DWORD x;
    REQUIRE(U(RpcSmEnableAllocate)); REQUIRE(U(NdrRpcSmClientAllocate));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    x = g7_run(body_alloc_after_disable, &m);
    EQ(x, RPC_S_OUT_OF_MEMORY);
}

/* ---------- mixed Ss/Sm enable counting ---------- */
static void body_mixed(void *p)
{
    res *r = p;
    r->d[0] = U(RpcSsGetThreadHandle)() == NULL;
    U(RpcSmEnableAllocate)(); U(RpcSsEnableAllocate)(); U(RpcSmEnableAllocate)();
    r->d[1] = U(RpcSsGetThreadHandle)() != NULL;
    U(RpcSsDisableAllocate)();
    U(RpcSmDisableAllocate)();
    r->d[2] = U(RpcSsGetThreadHandle)() != NULL;     /* 3 enables - 2 disables */
    U(RpcSsDisableAllocate)();
    r->d[3] = U(RpcSsGetThreadHandle)() == NULL;
    r->d[4] = U(RpcSmDisableAllocate)();             /* extra disable on a disabled context: harmless */
    r->d[5] = U(RpcSsGetThreadHandle)() == NULL;
    U(RpcSsEnableAllocate)();
    r->d[6] = U(RpcSsGetThreadHandle)() != NULL;     /* usable again */
    U(RpcSsDisableAllocate)();
}
TEST(gap7_stubmem_sm_and_ss_enable_counts_share_one_counter)
{
    res r;
    REQUIRE(U(RpcSmEnableAllocate)); REQUIRE(U(RpcSsEnableAllocate)); REQUIRE(U(RpcSsGetThreadHandle));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    memset(&r, 0, sizeof r);
    EQ(g7_run(body_mixed, &r), 0);
    EQ(r.d[0], 1); EQ(r.d[1], 1); EQ(r.d[2], 1); EQ(r.d[3], 1); EQ(r.d[4], 0); EQ(r.d[5], 1); EQ(r.d[6], 1);
}

/* ---------- SetThreadHandle(NULL) discards a solely-owned context ---------- */
static void body_drop_hooks(void *p)
{
    res *r = p; void *oa = 0, *of = 0, *d_a = 0, *d_f = 0;
    U(RpcSmSwapClientAllocFree)(HA[0], HF[0], (RPC_CLIENT_ALLOC **)&d_a, (RPC_CLIENT_FREE **)&d_f);
    r->d[0] = U(RpcSmSetThreadHandle)(NULL);
    U(RpcSsSwapClientAllocFree)(NULL, NULL, (RPC_CLIENT_ALLOC **)&oa, (RPC_CLIENT_FREE **)&of);
    r->r[0] = oa; r->r[1] = of; r->r[2] = d_a; r->r[3] = d_f;          /* back to the defaults: user hooks were discarded */
}
TEST(gap7_stubmem_set_thread_handle_null_discards_sole_context_and_hooks)
{
    res r;
    REQUIRE(U(RpcSmSetThreadHandle)); REQUIRE(U(RpcSmSwapClientAllocFree));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    memset(&r, 0, sizeof r);
    EQ(g7_run(body_drop_hooks, &r), 0);
    EQ(r.d[0], 0);
    CHECK(r.r[0] == r.r[2] && r.r[1] == r.r[3] && r.r[0] != NULL);
}
static void body_drop_enabled(void *p)
{
    res *r = p; DWORD c;
    U(RpcSsEnableAllocate)();
    r->d[0] = U(RpcSsGetThreadHandle)() != NULL;
    U(RpcSsSetThreadHandle)(NULL);                                      /* context released although enabled */
    r->d[1] = U(RpcSsGetThreadHandle)() == NULL;
    r->r[0] = NULL;
    r->r[0] = U(RpcSsAllocate)(8);                                      /* no context at all now: raises ERROR_INVALID_PARAMETER */
}
TEST(gap7_stubmem_set_thread_handle_null_on_enabled_thread_removes_context)
{
    res r; DWORD x;
    REQUIRE(U(RpcSsSetThreadHandle)); REQUIRE(U(RpcSsEnableAllocate)); REQUIRE(U(RpcSsAllocate));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    memset(&r, 0, sizeof r); r.r[0] = (void *)1;
    x = g7_run(body_drop_enabled, &r);
    EQ(x, ERROR_INVALID_PARAMETER);
    EQ(r.d[0], 1); EQ(r.d[1], 1);
}

/* ---------- shared context: enable counts are shared, lifetime follows the count ---------- */
static HANDLE sh_ev[4]; static void *sh_ctx; static res sh_res;
static DWORD WINAPI sh_b(void *p)
{
    WaitForSingleObject(sh_ev[0], 8000);
    U(RpcSsSetThreadHandle)(sh_ctx);
    U(RpcSsEnableAllocate)();                                          /* count 1 -> 2 on the shared context */
    sh_res.d[0] = U(RpcSsGetThreadHandle)() == sh_ctx;
    SetEvent(sh_ev[1]);
    WaitForSingleObject(sh_ev[2], 8000);
    /* A has disabled once: count is 1, B can still allocate */
    {
        void *m = NULL; DWORD c;
        sh_res.d[2] = U(RpcSsGetThreadHandle)() == sh_ctx;
        m = U(RpcSsAllocate)(32); sh_res.d[3] = m != NULL;
    }
    U(RpcSsDisableAllocate)();                                         /* count 0 */
    sh_res.d[4] = U(RpcSsGetThreadHandle)() == NULL;
    SetEvent(sh_ev[3]);
    return 0;
}
static void body_shared(void *p)
{
    HANDLE th = CreateThread(NULL, 0, sh_b, NULL, 0, NULL);
    U(RpcSsEnableAllocate)();
    sh_ctx = U(RpcSsGetThreadHandle)();
    SetEvent(sh_ev[0]);
    WaitForSingleObject(sh_ev[1], 8000);
    U(RpcSsDisableAllocate)();                                         /* 2 -> 1 */
    sh_res.d[1] = U(RpcSsGetThreadHandle)() == sh_ctx;
    SetEvent(sh_ev[2]);
    WaitForSingleObject(sh_ev[3], 8000);
    sh_res.d[5] = U(RpcSsGetThreadHandle)() == NULL;                   /* count reached 0 through B's Disable */
    WaitForSingleObject(th, 8000); CloseHandle(th);
}
TEST(gap7_stubmem_shared_context_enable_count_is_shared_between_threads)
{
    int i;
    REQUIRE(U(RpcSsSetThreadHandle)); REQUIRE(U(RpcSsEnableAllocate)); REQUIRE(U(RpcSsAllocate));
    if (t_is_wine()) { t_skip("wine lacks the TLS allocation-context hooks"); return; }
    memset(&sh_res, 0, sizeof sh_res); sh_ctx = NULL;
    for (i = 0; i < 4; i++) sh_ev[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
    EQ(g7_run(body_shared, NULL), 0);
    CHECK(sh_ctx != NULL);
    for (i = 0; i < 6; i++) CHECKF(sh_res.d[i] == 1, "step %d", i);
    for (i = 0; i < 4; i++) CloseHandle(sh_ev[i]);
}

/* ---------- default allocator round trip ---------- */
static void body_default_roundtrip(void *p)
{
    res *r = p; unsigned i, bad = 0;
    for (i = 0; i < 2000; i++) {
        size_t n = (i * 37) % 3000 + 1; unsigned char *m = U(NdrRpcSmClientAllocate)(n);
        if (!m) { bad++; continue; }
        memset(m, (int)i, n);
        if (m[0] != (unsigned char)i || m[n - 1] != (unsigned char)i) bad++;
        if (i & 1) U(NdrRpcSmClientFree)(m); else U(RpcSmClientFree)(m);
    }
    r->d[0] = bad;
}
TEST(gap7_stubmem_default_client_allocator_roundtrip)
{
    res r;
    REQUIRE(U(NdrRpcSmClientAllocate)); REQUIRE(U(NdrRpcSmClientFree)); REQUIRE(U(RpcSmClientFree));
    if (t_is_wine()) { t_skip("wine lacks NdrRpcSmClientAllocate"); return; }
    memset(&r, 0xff, sizeof r);
    EQ(g7_run(body_default_roundtrip, &r), 0);
    EQ(r.d[0], 0);
}
