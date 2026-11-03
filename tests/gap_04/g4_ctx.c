/* gap_04: server/client context-handle helpers: RpcSsContextLockShared/Exclusive, RpcSsDontSerializeContext,
 * RpcSmDestroyClientContext, RpcUserFree. */
#include "rawfx.h"

extern volatile LONG g4_ctxmode; extern volatile LONG g4_ctxres[8];
extern volatile LONG g4_frees;
UDECL(RPC_STATUS, RpcSsContextLockExclusive, (RPC_BINDING_HANDLE, PVOID));
UDECL(RPC_STATUS, RpcSsContextLockShared, (RPC_BINDING_HANDLE, PVOID));
UDECL(void, RpcSsDontSerializeContext, (void));

#define UP(f) do { if (!rx_fx_up(&f)) { CHECKF(0, "server fixture failed"); rx_fx_down(&f); return; } } while (0)
#define NDR(f, proc, a0, a1, a2, a3, ret) rx_ndr_h((f).h, (proc), (ULONG_PTR)(a0), (ULONG_PTR)(a1), (ULONG_PTR)(a2), (ULONG_PTR)(a3), (ret))
#define ERR_INVALID_HANDLE 6
#define NOWINE(why) do { if (t_is_wine()) { t_skip("%s", why); return; } } while (0)

/* ---- locks outside a call ---------------------------------------------------------------------- */
TEST(gap4_ctxlock_outside_call_shared) {
    NOWINE("RpcSsContextLock* is an unimplemented stub in wine");
    int x = 0;
    REQUIRE(UN(RpcSsContextLockShared));
    /* no call on this thread, so the pointer cannot name a context handle: ERROR_INVALID_HANDLE */
    EQ(UN(RpcSsContextLockShared)(NULL, &x), ERR_INVALID_HANDLE);
    EQ(UN(RpcSsContextLockShared)(NULL, NULL), ERR_INVALID_HANDLE);
}
TEST(gap4_ctxlock_outside_call_exclusive) {
    NOWINE("RpcSsContextLock* is an unimplemented stub in wine");
    int x = 0;
    REQUIRE(UN(RpcSsContextLockExclusive));
    EQ(UN(RpcSsContextLockExclusive)(NULL, &x), ERR_INVALID_HANDLE);
    EQ(UN(RpcSsContextLockExclusive)(NULL, NULL), ERR_INVALID_HANDLE);
}
TEST(gap4_ctxlock_client_binding_handle) {
    NOWINE("RpcSsContextLock* is an unimplemented stub in wine");
    RPC_BINDING_HANDLE h; WCHAR ep[64]; int x = 0;
    REQUIRE(UN(RpcSsContextLockShared)); REQUIRE(UN(RpcSsContextLockExclusive));
    rx_ep_unique(ep, 64);
    h = rx_bind(ep); CHECK(h != NULL);
    if (!h) return;
    /* only a server call handle can own context handles */
    EQ(UN(RpcSsContextLockShared)(h, &x), ERR_INVALID_HANDLE);
    EQ(UN(RpcSsContextLockExclusive)(h, &x), ERR_INVALID_HANDLE);
    rx_unbind(h);
}

/* ---- locks inside a call ------------------------------------------------------------------------ */
TEST(gap4_ctxlock_in_call_serialized) {
    NOWINE("RpcSsContextLock* is an unimplemented stub in wine");
    rx_fx f; void *h = NULL; LONG_PTR r = 0; DWORD e; int i;
    REQUIRE(UN(RpcSsContextLockShared)); REQUIRE(UN(RpcSsContextLockExclusive));
    UP(f);
    e = NDR(f, RX_CTX_OPEN, &h, 31337, 0, 0, NULL); EQ(e, 0); CHECK(h != NULL);
    g4_ctxmode = 1;
    e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 31337);
    EQ(g4_ctxmode, 0);                       /* the manager routine ran the lock sequence */
    EQ(g4_ctxres[0], 0);                     /* serialized -> shared (downgrade) */
    EQ(g4_ctxres[1], 0);                     /* shared -> exclusive (upgrade) */
    EQ(g4_ctxres[2], 0);                     /* exclusive -> shared again */
    EQ(g4_ctxres[3], ERR_INVALID_HANDLE);    /* a pointer that is not this call's context handle */
    EQ(g4_ctxres[4], ERR_INVALID_HANDLE);
    /* the context is still usable by later calls */
    for (i = 0; i < 3; i++) { r = 0; e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 31337); }
    e = NDR(f, RX_CTX_CLOSE, &h, 0, 0, 0, NULL); EQ(e, 0); CHECK(h == NULL);
    rx_fx_down(&f);
}
TEST(gap4_ctxlock_in_call_with_call_binding_handle) {
    NOWINE("RpcSsContextLock* is an unimplemented stub in wine");
    rx_fx f; void *h = NULL; LONG_PTR r = 0; DWORD e;
    REQUIRE(UN(RpcSsContextLockShared)); REQUIRE(U(RpcServerInqBindingHandle));
    UP(f);
    e = NDR(f, RX_CTX_OPEN, &h, 4, 0, 0, NULL); EQ(e, 0);
    g4_ctxmode = 2;
    e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r); EQ(e, 0); EQ((int)r, 4);
    EQ(g4_ctxres[7], RPC_S_OK);              /* RpcServerInqBindingHandle inside the call */
    EQ(g4_ctxres[0], 0); EQ(g4_ctxres[1], 0); EQ(g4_ctxres[2], 0);   /* an explicit call handle behaves like NULL */
    EQ(g4_ctxres[3], ERR_INVALID_HANDLE); EQ(g4_ctxres[4], ERR_INVALID_HANDLE);
    e = NDR(f, RX_CTX_CLOSE, &h, 0, 0, 0, NULL); EQ(e, 0);
    rx_fx_down(&f);
}
TEST(gap4_ctxlock_in_call_repeated_cycles) {
    NOWINE("RpcSsContextLock* is an unimplemented stub in wine");
    rx_fx f; void *h = NULL; LONG_PTR r = 0; DWORD e; int i, bad = 0;
    REQUIRE(UN(RpcSsContextLockShared));
    UP(f);
    e = NDR(f, RX_CTX_OPEN, &h, 9, 0, 0, NULL); EQ(e, 0);
    for (i = 0; i < 20; i++) {
        g4_ctxmode = 1;
        e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r);
        if (e != 0 || (int)r != 9 || g4_ctxres[0] || g4_ctxres[1] || g4_ctxres[2]) bad++;
    }
    EQ(bad, 0);
    e = NDR(f, RX_CTX_CLOSE, &h, 0, 0, 0, NULL); EQ(e, 0);
    rx_fx_down(&f);
}

/* ---- RpcSsDontSerializeContext ------------------------------------------------------------------- */
TEST(gap4_dontserializecontext_returns) {
    /* process-wide and irreversible, so exercised in a child server only */
    REQUIRE(UN(RpcSsDontSerializeContext));
}
static int run_dontser_child(DWORD *exit_code) {
    WCHAR ep[64]; PROCESS_INFORMATION pi; HANDLE proc; RPC_BINDING_HANDLE h; void *ctx = NULL; LONG_PTR r = 0; DWORD e;
    int ok = 0;
    rx_ep_unique(ep, 64);
    proc = rx_spawn_server(ep, "dontser", &pi);
    if (!proc) return 0;
    h = rx_bind(ep);
    if (h && rx_wait_server_ready(ep, 10000)) {
        e = rx_ndr_h(h, RX_CTX_OPEN, (ULONG_PTR)&ctx, 77, 0, 0, NULL);
        if (e == 0 && ctx) {
            e = rx_ndr_h(h, RX_CTX_GET, (ULONG_PTR)ctx, 0, 0, 0, &r);
            ok = (e == 0 && (int)r == 77);
            rx_ndr_h(h, RX_CTX_CLOSE, (ULONG_PTR)&ctx, 0, 0, 0, NULL);
        }
        rx_ndr_h(h, RX_STOP, 0, 0, 0, 0, NULL);
    }
    if (WaitForSingleObject(proc, 10000) != WAIT_OBJECT_0) { TerminateProcess(proc, 1); WaitForSingleObject(proc, 5000); *exit_code = 0xdead0001; ok = 0; }
    else GetExitCodeProcess(proc, exit_code);
    CloseHandle(pi.hThread); CloseHandle(proc);
    rx_unbind(h);
    return ok;
}
TEST(gap4_dontserialize_then_lock_upgrade) {
    DWORD code = 0xdead0000;
    REQUIRE(UN(RpcSsDontSerializeContext));
    if (t_is_wine()) { t_skip("context locks unimplemented in wine"); return; }
    CHECK(run_dontser_child(&code));
    /* child exit code: lock results {exclusive, shared, exclusive} of one call after RpcSsDontSerializeContext,
     * each one byte; every lock must have returned RPC_S_OK */
    CHECKF(code == 0, "child exit 0x%lx (lock results excl=%lu shared=%lu excl=%lu)", code, code & 0xff, (code >> 8) & 0xff, (code >> 16) & 0xff);
}

/* ---- RpcSmDestroyClientContext ------------------------------------------------------------------- */
TEST(gap4_smdestroyclientcontext_basic) {
    rx_fx f; void *h = NULL; DWORD e; LONG rd0; RPC_STATUS st; DWORD t0;
    REQUIRE(U(RpcSmDestroyClientContext));
    UP(f);
    rd0 = rx_rundowns;
    e = NDR(f, RX_CTX_OPEN, &h, 1234, 0, 0, NULL); EQ(e, 0); CHECK(h != NULL);
    st = U(RpcSmDestroyClientContext)(&h);
    EQ(st, RPC_S_OK);                         /* always RPC_S_OK */
    CHECK(h == NULL);                         /* the client context is freed and the variable cleared */
    /* the server is not told: its copy is run down once the connection goes away */
    t0 = GetTickCount();
    while (rx_rundowns == rd0 && GetTickCount() - t0 < 5000) Sleep(50);
    t_note("rundowns after RpcSmDestroyClientContext: %ld (before %ld)", (long)rx_rundowns, (long)rd0);
    /* the binding handle used by the fixture remains usable */
    { int r = 0; e = rx_add(f.h, 2, 3, &r); t_note("call on the fixture binding afterwards: exception %lu result %d", e, r); }
    rx_fx_down(&f);
}
TEST(gap4_smdestroyclientcontext_two_contexts) {
    rx_fx f; void *a = NULL, *b = NULL; DWORD e; RPC_STATUS st;
    REQUIRE(U(RpcSmDestroyClientContext));
    UP(f);
    e = NDR(f, RX_CTX_OPEN, &a, 1, 0, 0, NULL); EQ(e, 0);
    e = NDR(f, RX_CTX_OPEN, &b, 2, 0, 0, NULL); EQ(e, 0);
    CHECK(a != NULL && b != NULL && a != b);
    st = U(RpcSmDestroyClientContext)(&a); EQ(st, RPC_S_OK); CHECK(a == NULL);
    st = U(RpcSmDestroyClientContext)(&b); EQ(st, RPC_S_OK); CHECK(b == NULL);
    rx_fx_down(&f);
}

/* ---- RpcUserFree -------------------------------------------------------------------------------------- */
TEST(gap4_userfree_frees_via_stub_free_routine) {
    rx_fx f; void *h = NULL; LONG_PTR r = 0; DWORD e;
    REQUIRE(U(RpcUserFree));
    NOWINE("RpcUserFree is an unimplemented stub in wine");
    UP(f);
    e = NDR(f, RX_CTX_OPEN, &h, 5, 0, 0, NULL); EQ(e, 0);
    g4_ctxmode = 5;
    e = NDR(f, RX_CTX_GET, h, 0, 0, 0, &r); EQ(e, 0);
    EQ(g4_ctxres[0], 1);        /* a heap block outside the RPC buffer is released through the stub's free routine exactly once */
    e = NDR(f, RX_CTX_CLOSE, &h, 0, 0, 0, NULL); EQ(e, 0);
    rx_fx_down(&f);
}
