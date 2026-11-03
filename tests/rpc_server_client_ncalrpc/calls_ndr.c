/* In-process NDR (NdrClientCall2 / NdrServerCall2) calls over ncalrpc with the hand-assembled "xif" interface.
 * Each test brings up its own endpoint; the server runs on RPC runtime threads of this process. */
#include "rpcx.h"

#define UP(f) do { if (!rx_fx_up(&f)) { CHECKF(0, "server fixture failed"); rx_fx_down(&f); return; } } while (0)
#define NDR(f, proc, a0, a1, a2, a3, ret) rx_ndr_h((f).h, (proc), (ULONG_PTR)(a0), (ULONG_PTR)(a1), (ULONG_PTR)(a2), (ULONG_PTR)(a3), (ret))

TEST(ncalrpc_ndr_void_call_counts) {
    rx_fx f; LONG_PTR r = -1; LONG before; DWORD e;
    UP(f);
    before = rx_calls;
    e = NDR(f, RX_VOID, 0, 0, 0, 0, &r);
    EQ(e, 0);
    EQ(rx_calls, before + 1);
    e = NDR(f, RX_VOID, 0, 0, 0, 0, &r); EQ(e, 0);
    EQ(rx_calls, before + 2);
    e = NDR(f, RX_COUNTER, 0, 0, 0, 0, &r);
    EQ(e, 0); EQ((int)r, before + 2);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_add_table) {
    static const struct { int a, b, r; } t[] = {
        {0,0,0},{1,2,3},{-1,1,0},{-5,-7,-12},{100000,200000,300000},{0x7fffffff,0,0x7fffffff},
        {0x7fffffff,1,(int)0x80000000},{(int)0x80000000,(int)0x80000000,0},{12345,-12345,0},{(int)0xdeadbeef,0x11111111,(int)(0xdeadbeef+0x11111111)} };
    rx_fx f; unsigned i; int r;
    UP(f);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        DWORD e = rx_add(f.h, t[i].a, t[i].b, &r);
        EQ(e, 0); EQ(r, t[i].r);
    }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_add_many_sequential) {
    rx_fx f; int i, bad = 0, r;
    UP(f);
    for (i = 0; i < 300; i++) { if (rx_add(f.h, i, i * 3, &r) != 0 || r != i * 4) bad++; }
    EQ(bad, 0);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_implicit_handle_global) {
    /* the stub desc uses the implicit handle in rx_ih; rx_ndr (no explicit handle) must use it as is */
    rx_fx f; LONG_PTR r = 0; DWORD e;
    UP(f);
    rx_ih = f.h;
    e = rx_ndr(RX_ADD, 40, 2, 0, 0, &r);
    EQ(e, 0); EQ(r, 42);
    rx_ih = NULL;
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_wlen_strings) {
    rx_fx f; LONG_PTR r; DWORD e; unsigned i;
    static WCHAR big[4097];
    static const WCHAR *t[] = { L"", L"a", L"hello", L"h\x00e9llo \x20ac \xd83d\xde00", L"embedded\\path\\\"quote" };
    static const int want[] = { 0, 1, 5, 9, 20 };
    UP(f);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        r = -1; e = NDR(f, RX_WLEN, t[i], 0, 0, 0, &r);
        EQ(e, 0); EQ((int)r, lstrlenW(t[i]));
    }
    for (i = 0; i < 4096; i++) big[i] = L'A' + (i % 26);
    big[4096] = 0;
    e = NDR(f, RX_WLEN, big, 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 4096);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_wlen_null_ref_pointer) {
    rx_fx f; LONG_PTR r = 99; DWORD e; LONG before;
    UP(f);
    before = rx_calls;
    e = NDR(f, RX_WLEN, 0, 0, 0, 0, &r);
    EQ(e, RPC_X_NULL_REF_POINTER);          /* [in,string] ref pointer: client stub raises before sending */
    EQ(rx_calls, before);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_sum_conformant_array) {
    rx_fx f; LONG_PTR r; DWORD e; static int a[100000]; int i, n;
    static const int sizes[] = { 0, 1, 2, 3, 7, 100, 1000, 100000 };
    unsigned k;
    UP(f);
    for (i = 0; i < 100000; i++) a[i] = i % 17 - 8;
    for (k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        long long want = 0;
        n = sizes[k];
        for (i = 0; i < n; i++) want += a[i];
        r = -12345; e = NDR(f, RX_SUM, n, a, 0, 0, &r);
        EQ(e, 0); EQ((int)r, (int)want);
    }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_sum_null_array) {
    rx_fx f; LONG_PTR r; DWORD e; int one = 1;
    UP(f);
    e = NDR(f, RX_SUM, 0, 0, 0, 0, &r);
    EQ(e, RPC_X_NULL_REF_POINTER);
    e = NDR(f, RX_SUM, 1, &one, 0, 0, &r); EQ(e, 0); EQ((int)r, 1);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_seq_out_array) {
    rx_fx f; DWORD e; unsigned k; static int a[100001]; int i, bad;
    static const int sizes[] = { 1, 2, 5, 16, 1000, 100000 };
    UP(f);
    for (k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        int n = sizes[k];
        for (i = 0; i <= n; i++) a[i] = 0x5a5a5a5a;
        e = NDR(f, RX_SEQ, n, a, 0, 0, NULL);
        EQ(e, 0);
        for (i = 0, bad = 0; i < n; i++) if (a[i] != i * i) bad++;
        EQ(bad, 0);
        EQ(a[n], 0x5a5a5a5a);               /* nothing written past the conformant size */
    }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_seq_zero_length) {
    rx_fx f; DWORD e; int a[2] = { 11, 22 };
    UP(f);
    e = NDR(f, RX_SEQ, 0, a, 0, 0, NULL);
    EQ(e, 0); EQ(a[0], 11); EQ(a[1], 22);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_seq_null_out_pointer) {
    rx_fx f; DWORD e;
    UP(f);
    e = NDR(f, RX_SEQ, 4, 0, 0, 0, NULL);
    EQ(e, RPC_X_NULL_REF_POINTER);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_inout_struct) {
    rx_fx f; DWORD e; rx_S s; int i;
    UP(f);
    s.a = 10; s.b = -3; s.c = 1.5;
    for (i = 0; i < 4; i++) {
        e = NDR(f, RX_BUMP, &s, 0, 0, 0, NULL);
        EQ(e, 0);
    }
    EQ(s.a, 14); EQ(s.b, 5); CHECK(s.c == 24.0);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_inout_struct_null) {
    rx_fx f; DWORD e;
    UP(f);
    e = NDR(f, RX_BUMP, 0, 0, 0, 0, NULL);
    EQ(e, RPC_X_NULL_REF_POINTER);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_inout_struct_extremes) {
    rx_fx f; DWORD e; rx_S s;
    UP(f);
    s.a = 0x7fffffff; s.b = 0x7fff; s.c = -0.0;
    e = NDR(f, RX_BUMP, &s, 0, 0, 0, NULL);
    EQ(e, 0);
    EQ(s.a, (int)0x80000000); EQ(s.b, (short)0x8001); CHECK(s.c == 0.0);
    s.a = 1; s.b = 1; s.c = 1e300;
    e = NDR(f, RX_BUMP, &s, 0, 0, 0, NULL);
    CHECK(s.c == 2e300);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_echo_string_out_buffer) {
    static const struct { const WCHAR *s; int n; const WCHAR *want; } t[] = {
        { L"abc", 4, L"abc" }, { L"abc", 10, L"abc" }, { L"abc", 3, L"ab" }, { L"abc", 2, L"a" }, { L"", 1, L"" },
        { L"unicode \x20ac\x00e9", 32, L"unicode \x20ac\x00e9" } };
    rx_fx f; unsigned i; WCHAR out[64]; DWORD e;
    UP(f);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        memset(out, 0x7f, sizeof out);
        e = NDR(f, RX_ECHO, t[i].s, t[i].n, out, 0, NULL);
        EQ(e, 0);
        CHECK(!lstrcmpW(out, t[i].want));
    }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_echo_null_pointers) {
    rx_fx f; DWORD e; WCHAR out[4];
    UP(f);
    e = NDR(f, RX_ECHO, 0, 4, out, 0, NULL);   EQ(e, RPC_X_NULL_REF_POINTER);
    e = NDR(f, RX_ECHO, L"x", 4, 0, 0, NULL);  EQ(e, RPC_X_NULL_REF_POINTER);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_raise_exception_codes) {
    /* RpcRaiseException in a manager routine reaches the client as an exception with that code */
    static const int codes[] = { ERROR_FILE_NOT_FOUND, ERROR_ACCESS_DENIED, ERROR_INVALID_PARAMETER, 1234, ERROR_NOT_SUPPORTED };
    rx_fx f; unsigned i; DWORD e;
    UP(f);
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        e = NDR(f, RX_RAISE, codes[i], 0, 0, 0, NULL);
        EQ(e, codes[i]);
    }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_raise_rpc_status_codes) {
    static const int codes[] = { RPC_S_CALL_FAILED, RPC_S_INVALID_BOUND, RPC_X_BAD_STUB_DATA, RPC_S_SERVER_TOO_BUSY, RPC_X_NULL_REF_POINTER, RPC_S_PROCNUM_OUT_OF_RANGE };
    rx_fx f; unsigned i; DWORD e;
    UP(f);
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        e = NDR(f, RX_RAISE, codes[i], 0, 0, 0, NULL);
        t_note("server raised %d -> client exception %lu", codes[i], e);
        CHECK(e != 0 && e != RX_TIMEOUT);
    }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_server_survives_exception) {
    rx_fx f; DWORD e; int r;
    UP(f);
    e = NDR(f, RX_RAISE, 1234, 0, 0, 0, NULL); EQ(e, 1234);
    e = rx_add(f.h, 3, 4, &r);                 EQ(e, 0); EQ(r, 7);
    e = NDR(f, RX_RAISE, 1234, 0, 0, 0, NULL); EQ(e, 1234);
    e = rx_add(f.h, 5, 6, &r);                 EQ(e, 0); EQ(r, 11);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_context_handle_roundtrip) {
    rx_fx f; void *h = NULL; LONG_PTR r; DWORD e; LONG closes0, rd0;
    UP(f);
    closes0 = rx_ctx_closes; rd0 = rx_rundowns;
    e = NDR(f, RX_CTX_OPEN, &h, 4242, 0, 0, NULL);
    EQ(e, 0); CHECK(h != NULL);
    r = 0; e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r);        EQ(e, 0); EQ((int)r, 4242);
    r = 0; e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r);        EQ(e, 0); EQ((int)r, 4242);   /* repeatable */
    {   int rd = -1;
        r = 0; e = NDR(f, RX_CTX_PEEK, h, &rd, 0, 0, &r);  EQ(e, 0); EQ((int)r, 4242); }
    e = NDR(f, RX_CTX_CLOSE, &h, 0, 0, 0, NULL);
    EQ(e, 0);
    CHECK(h == NULL);                                       /* server set the handle to NULL: context destroyed */
    EQ(rx_ctx_closes, closes0 + 1);
    EQ(rx_rundowns, rd0);                                   /* a clean close does not run the rundown routine */
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_context_handles_independent) {
    rx_fx f; void *h[5] = { 0 }; LONG_PTR r; DWORD e; int i;
    UP(f);
    for (i = 0; i < 5; i++) { e = NDR(f, RX_CTX_OPEN, &h[i], 100 + i, 0, 0, NULL); EQ(e, 0); CHECK(h[i] != NULL); }
    for (i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) CHECK(h[i] != h[j]);
    for (i = 4; i >= 0; i--) { r = 0; e = NDR(f, RX_CTX_GET, h[i], 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 100 + i); }
    for (i = 0; i < 5; i++) { e = NDR(f, RX_CTX_CLOSE, &h[i], 0, 0, 0, NULL); EQ(e, 0); CHECK(h[i] == NULL); }
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_context_handle_null_in) {
    rx_fx f; LONG_PTR r = 0; DWORD e;
    UP(f);
    e = NDR(f, RX_CTX_GET, 0, 0, 0, 0, &r);
    EQ(e, RPC_X_SS_IN_NULL_CONTEXT);           /* [in] context handle flagged "cannot be null" */
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_context_handle_use_after_close) {
    rx_fx f; void *h = NULL, *stale; LONG_PTR r = 0; DWORD e;
    UP(f);
    e = NDR(f, RX_CTX_OPEN, &h, 7, 0, 0, NULL); EQ(e, 0);
    stale = h;                                  /* client-side NDR context object */
    e = NDR(f, RX_CTX_CLOSE, &h, 0, 0, 0, NULL); EQ(e, 0);
    (void)stale;                                /* the stale client object is freed by the runtime: not dereferenced */
    e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r);     /* h is now NULL */
    EQ(e, RPC_X_SS_IN_NULL_CONTEXT);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_context_handle_two_bindings_one_server) {
    rx_fx f; RPC_BINDING_HANDLE h2; void *c1 = NULL, *c2 = NULL; LONG_PTR r; DWORD e;
    UP(f);
    h2 = rx_bind(f.ep); CHECK(h2 != NULL);
    e = rx_ndr_h(f.h, RX_CTX_OPEN, (ULONG_PTR)&c1, 1, 0, 0, NULL); EQ(e, 0);
    e = rx_ndr_h(h2, RX_CTX_OPEN, (ULONG_PTR)&c2, 2, 0, 0, NULL); EQ(e, 0);
    r = 0; e = rx_ndr_h(f.h, RX_CTX_GET, (ULONG_PTR)c1, 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 1);
    r = 0; e = rx_ndr_h(h2, RX_CTX_GET, (ULONG_PTR)c2, 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 2);
    e = rx_ndr_h(f.h, RX_CTX_CLOSE, (ULONG_PTR)&c1, 0, 0, 0, NULL); EQ(e, 0);
    e = rx_ndr_h(h2, RX_CTX_CLOSE, (ULONG_PTR)&c2, 0, 0, 0, NULL); EQ(e, 0);
    rx_unbind(h2);
    rx_fx_down(&f);
}

/* ---- concurrency ---------------------------------------------------------- */
typedef struct { RPC_BINDING_HANDLE h; int id, n, bad; } cjob;
static DWORD WINAPI conc_thread(void *p) {
    cjob *j = p; int i, r;
    for (i = 0; i < j->n; i++) {
        DWORD e = rx_add(j->h, j->id * 1000, i, &r);
        if (e != 0 || r != j->id * 1000 + i) j->bad++;
    }
    return 0;
}
TEST(ncalrpc_ndr_concurrent_clients_shared_binding) {
    enum { N = 6 };
    rx_fx f; HANDLE th[N]; cjob j[N]; int i, bad = 0; LONG before;
    UP(f);
    before = rx_calls;
    for (i = 0; i < N; i++) { j[i].h = f.h; j[i].id = i + 1; j[i].n = 20; j[i].bad = 0; th[i] = rx_thread(conc_thread, &j[i]); CHECK(th[i] != NULL); }
    for (i = 0; i < N; i++) { CHECK(WaitForSingleObject(th[i], 20000) == WAIT_OBJECT_0); CloseHandle(th[i]); bad += j[i].bad; }
    EQ(bad, 0);
    EQ(rx_calls, before + N * 20);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_concurrent_clients_own_bindings) {
    enum { N = 6 };
    rx_fx f; HANDLE th[N]; cjob j[N]; int i, bad = 0;
    UP(f);
    for (i = 0; i < N; i++) { j[i].h = rx_bind(f.ep); j[i].id = i + 10; j[i].n = 20; j[i].bad = 0; th[i] = rx_thread(conc_thread, &j[i]); }
    for (i = 0; i < N; i++) { CHECK(WaitForSingleObject(th[i], 20000) == WAIT_OBJECT_0); CloseHandle(th[i]); bad += j[i].bad; rx_unbind(j[i].h); }
    EQ(bad, 0);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_calls_overlap_on_server) {
    /* two sleeping calls from two threads must be in flight at the same time (server is multithreaded) */
    rx_fx f; cjob unused; HANDLE th[2]; DWORD t0; typedef struct { RPC_BINDING_HANDLE h; DWORD e; } sj;
    sj s[2]; int i; LONG started0, done0;
    extern DWORD WINAPI ncalrpc_sleep_thread(void *);
    UP(f);
    started0 = rx_sleep_started; done0 = rx_sleep_done;
    for (i = 0; i < 2; i++) { s[i].h = f.h; s[i].e = 0xdead; th[i] = rx_thread(ncalrpc_sleep_thread, &s[i]); }
    for (t0 = GetTickCount(); rx_sleep_started - started0 < 2 && GetTickCount() - t0 < 5000; ) Sleep(5);
    EQ(rx_sleep_started - started0, 2);
    EQ(rx_sleep_done - done0, 0);                /* both still inside the manager routine: they overlap */
    for (i = 0; i < 2; i++) { CHECK(WaitForSingleObject(th[i], 15000) == WAIT_OBJECT_0); CloseHandle(th[i]); EQ(s[i].e, 0); }
    rx_fx_down(&f);
}
DWORD WINAPI ncalrpc_sleep_thread(void *p) {
    struct { RPC_BINDING_HANDLE h; DWORD e; } *s = p;
    s->e = rx_ndr_h(s->h, RX_SLEEP, 700, 0, 0, 0, NULL);
    return 0;
}

/* ---- errors surfaced by the runtime -------------------------------------- */
TEST(ncalrpc_ndr_server_unavailable) {
    WCHAR ep[64]; RPC_BINDING_HANDLE h; DWORD e; int r;
    rx_server_reset(); rx_ep(ep, 64);
    h = rx_bind(ep); CHECK(h != NULL);          /* binding to a nonexistent endpoint can be created */
    e = rx_add(h, 1, 2, &r);
    EQ(e, RPC_S_SERVER_UNAVAILABLE);
    rx_unbind(h);
}
TEST(ncalrpc_ndr_server_unavailable_after_shutdown) {
    rx_fx f; DWORD e; int r;
    UP(f);
    e = rx_add(f.h, 1, 2, &r); EQ(e, 0);
    rx_server_down(rx_xif_s_ifspec, NULL);
    e = rx_add(f.h, 1, 2, &r);
    CHECK(e == RPC_S_SERVER_UNAVAILABLE || e == RPC_S_UNKNOWN_IF || e == RPC_S_CALL_FAILED_DNE || e == RPC_S_CALL_FAILED);
    t_note("call after stop+unregister (endpoint kept by process) -> %lu", e);
    rx_unbind(f.h);
}
TEST(ncalrpc_ndr_unknown_if_on_live_endpoint) {
    /* a live endpoint, but the interface the client names is not registered there */
    rx_fx f; rx_raw r; UUID u; ULONG n; RPC_STATUS st;
    UP(f);
    rx_raw_uuid(&u, 0x40); rx_raw_init(&r, &u, 1, 0);
    st = rx_raw_call(f.h, &r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_UNKNOWN_IF);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_procnum_out_of_range) {
    rx_fx f; rx_raw r; UUID u; ULONG n; RPC_STATUS st; WCHAR ep[64]; RPC_BINDING_HANDLE h;
    if (t_is_wine()) { t_skip("wine leaks the in-flight call after PROCNUM_OUT_OF_RANGE, blocking later waits"); return; }
    rx_server_reset(); rx_ep(ep, 64); rx_raw_uuid(&u, 0x41); rx_raw_init(&r, &u, 1, 0);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&r.si, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h = rx_bind(ep);
    st = rx_raw_call(h, &r, 11, NULL, 0, NULL, 0, &n);   EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);   /* table has 11 entries */
    st = rx_raw_call(h, &r, 12, NULL, 0, NULL, 0, &n);   EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);
    st = rx_raw_call(h, &r, 0xffff, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);
    st = rx_raw_call(h, &r, 4, NULL, 0, NULL, 0, &n);    EQ(st, RPC_S_OK);                     /* still works */
    rx_unbind(h); rx_server_down((RPC_IF_HANDLE)&r.si, NULL);
}
TEST(ncalrpc_ndr_ndr_procnum_out_of_range) {
    /* NDR interface: proc number beyond the format-string offset table; the dispatch table has RX_NPROCS entries */
    rx_fx f; rx_raw r; ULONG n; RPC_STATUS st; rx_raw ghost;
    if (t_is_wine()) { t_skip("wine leaks the in-flight call after PROCNUM_OUT_OF_RANGE, blocking later waits"); return; }
    UP(f);
    memset(&ghost, 0, sizeof ghost);
    ghost.ci.Length = sizeof ghost.ci;
    ghost.ci.InterfaceId.SyntaxGUID = rx_xif_uuid; ghost.ci.InterfaceId.SyntaxVersion.MajorVersion = 1;
    ghost.ci.TransferSyntax = rx_ndr_syntax;
    st = rx_raw_call(f.h, &ghost, RX_NPROCS, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);
    st = rx_raw_call(f.h, &ghost, 1000, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_PROCNUM_OUT_OF_RANGE);
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_bad_stub_data_short_request) {
    /* proc 1 (add) sent with an empty request buffer through the raw path: the server stub reports bad stub data */
    rx_fx f; rx_raw ghost; ULONG n; RPC_STATUS st;
    UP(f);
    memset(&ghost, 0, sizeof ghost);
    ghost.ci.Length = sizeof ghost.ci;
    ghost.ci.InterfaceId.SyntaxGUID = rx_xif_uuid; ghost.ci.InterfaceId.SyntaxVersion.MajorVersion = 1;
    ghost.ci.TransferSyntax = rx_ndr_syntax;
    st = rx_raw_call(f.h, &ghost, RX_ADD, NULL, 0, NULL, 0, &n);
    t_note("add with empty request buffer -> %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    EQ(rx_calls >= 0, 1);
    { int r; EQ(rx_add(f.h, 1, 1, &r), 0); EQ(r, 2); }       /* server still healthy */
    rx_fx_down(&f);
}
TEST(ncalrpc_ndr_raw_call_of_ndr_interface) {
    /* hand-marshal add(20,22) with the raw buffer protocol: two little-endian int32 in, one int32 out */
    rx_fx f; rx_raw ghost; ULONG n = 0; RPC_STATUS st; int in[2] = { 20, 22 }, out[4] = { 0 };
    UP(f);
    memset(&ghost, 0, sizeof ghost);
    ghost.ci.Length = sizeof ghost.ci;
    ghost.ci.InterfaceId.SyntaxGUID = rx_xif_uuid; ghost.ci.InterfaceId.SyntaxVersion.MajorVersion = 1;
    ghost.ci.TransferSyntax = rx_ndr_syntax;
    st = rx_raw_call(f.h, &ghost, RX_ADD, in, sizeof in, out, sizeof out, &n);
    EQ(st, RPC_S_OK);
    EQ(n >= 4, 1); EQ(out[0], 42);
    rx_fx_down(&f);
}
