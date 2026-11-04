/* Hand-driven calls (I_RpcGetBuffer / I_RpcSendReceive / I_RpcFreeBuffer) against RPC_DISPATCH_FUNCTION
 * tables: buffer protocol, interface version matching, object uuid / manager type routing, fault propagation,
 * binding-handle bookkeeping.  See rpcx.h for the raw procedure list. */
#include "rpcx.h"

#include "rawfx.h"

TEST(ncalrpc_raw_echo_sizes) {
    static const ULONG sizes[] = { 0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 255, 256, 1000, 4095, 4096, 4097, 65535, 65536, 65537, 1 << 20 };
    rawfx f; unsigned k; unsigned char *in, *out; ULONG n, i;
    RUP(f, 0x50, 1, 0);
    in = HeapAlloc(GetProcessHeap(), 0, (1 << 20) + 8); out = HeapAlloc(GetProcessHeap(), 0, (1 << 20) + 8);
    for (i = 0; i < (1 << 20) + 8; i++) in[i] = (unsigned char)(i * 7 + 3);
    for (k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        RPC_STATUS st;
        memset(out, 0, 64);
        st = rx_raw_call(f.h, &f.r, 0, in, sizes[k], out, sizes[k], &n);
        EQ(st, RPC_S_OK);
        if (st == RPC_S_OK) { EQ(n, sizes[k]); CHECKF(!memcmp(in, out, sizes[k]), "payload mismatch for size %lu", sizes[k]); }
    }
    HeapFree(GetProcessHeap(), 0, in); HeapFree(GetProcessHeap(), 0, out);
    raw_down(&f);
}
TEST(ncalrpc_raw_server_sees_request_length) {
    rawfx f; DWORD r[4]; ULONG n; RPC_STATUS st; static const ULONG lens[] = { 0, 1, 12, 100, 5000 }; unsigned k; char in[5000];
    RUP(f, 0x51, 1, 0);
    memset(in, 1, sizeof in);
    for (k = 0; k < sizeof lens / sizeof *lens; k++) {
        memset(r, 0xff, sizeof r);
        st = rx_raw_call(f.h, &f.r, 8, in, lens[k], r, sizeof r, &n);
        EQ(st, RPC_S_OK);
        EQ(n, 16);
        EQ(r[0], 0x10);          /* NDR little-endian ASCII IEEE float data representation */
        EQ(r[1], lens[k]);       /* BufferLength seen by the dispatch routine */
        EQ(r[2], 8);             /* ProcNum */
        t_note("server-side RPC_MESSAGE.RpcFlags for a plain call = 0x%lx", r[3]);
    }
    raw_down(&f);
}
TEST(ncalrpc_raw_reply_length_zero_and_nonzero) {
    rawfx f; ULONG n = 99; RPC_STATUS st; DWORD r[11];
    RUP(f, 0x52, 1, 0);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK); EQ(n, 0);
    st = rx_raw_call(f.h, &f.r, 3, NULL, 0, r, sizeof r, &n); EQ(st, RPC_S_OK); EQ(n, 44);
    raw_down(&f);
}
TEST(ncalrpc_raw_procnum_dispatch_distinct) {
    rawfx f; ULONG n; DWORD r[11]; RPC_STATUS st;
    RUP(f, 0x53, 1, 0);
    st = rx_raw_call(f.h, &f.r, 3, NULL, 0, r, sizeof r, &n);
    if (!t_is_wine()) { EQ(st, RPC_S_OK); EQ(r[4], 3); }          /* OpNum seen by RpcServerInqCallAttributes */
    else t_skip("RpcServerInqCallAttributes unimplemented in wine");
    raw_down(&f);
}
TEST(ncalrpc_raw_fault_propagates_status) {
    static const int codes[] = { ERROR_FILE_NOT_FOUND, ERROR_ACCESS_DENIED, ERROR_INVALID_PARAMETER, 1234, ERROR_NOT_SUPPORTED, RPC_S_CALL_FAILED };
    rawfx f; unsigned k; RPC_STATUS st; ULONG n;
    RUP(f, 0x54, 1, 0);
    for (k = 0; k < sizeof codes / sizeof *codes; k++) {
        st = rx_raw_call(f.h, &f.r, 1, &codes[k], 4, NULL, 0, &n);
        if (codes[k] == RPC_S_CALL_FAILED) { CHECK(st != RPC_S_OK); t_note("fault %d -> status %lu", codes[k], (unsigned long)st); }
        else EQ(st, codes[k]);
    }
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);       /* binding is still usable */
    EQ(st, RPC_S_OK);
    raw_down(&f);
}
TEST(ncalrpc_raw_fault_does_not_kill_other_calls) {
    rawfx f; int code = 5; ULONG n; RPC_STATUS st; int i, ok = 0;
    RUP(f, 0x55, 1, 0);
    for (i = 0; i < 20; i++) {
        st = rx_raw_call(f.h, &f.r, 1, &code, 4, NULL, 0, &n); if (st == 5) ok++;
        st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);  if (st == RPC_S_OK) ok++;
    }
    EQ(ok, 40);
    raw_down(&f);
}

/* ---- interface version matching ------------------------------------------ */
TEST(ncalrpc_raw_version_matching) {
    /* server registers 1.5.  Compatible clients: same major, client minor <= server minor. */
    static const struct { unsigned short ma, mi; int ok; } t[] = {
        {1,5,1}, {1,0,1}, {1,4,1}, {1,6,0}, {1,0xffff,0}, {2,0,0}, {2,5,0}, {0,5,0}, {0,0,0}, {0xffff,5,0} };
    rawfx f; unsigned k;
    RUP(f, 0x56, 1, 5);
    for (k = 0; k < sizeof t / sizeof *t; k++) {
        rx_raw c; ULONG n; RPC_STATUS st;
        rx_raw_init(&c, &f.u, t[k].ma, t[k].mi);
        st = rx_raw_call(f.h, &c, 4, NULL, 0, NULL, 0, &n);
        if (t[k].ok) EQ(st, RPC_S_OK); else EQ(st, RPC_S_UNKNOWN_IF);
        if (st != (t[k].ok ? RPC_S_OK : RPC_S_UNKNOWN_IF)) t_note("client %u.%u -> %lu", t[k].ma, t[k].mi, (unsigned long)st);
    }
    raw_down(&f);
}
TEST(ncalrpc_raw_unknown_uuid) {
    rawfx f; rx_raw c; UUID u; ULONG n; RPC_STATUS st;
    RUP(f, 0x57, 1, 0);
    rx_raw_uuid(&u, 0x58); rx_raw_init(&c, &u, 1, 0);
    st = rx_raw_call(f.h, &c, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_UNKNOWN_IF);
    memset(&u, 0, sizeof u); rx_raw_init(&c, &u, 1, 0);
    st = rx_raw_call(f.h, &c, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_UNKNOWN_IF);    /* nil interface uuid */
    raw_down(&f);
}
TEST(ncalrpc_raw_two_interfaces_one_server_route_by_uuid) {
    rawfx a; rx_raw b; UUID ub; RPC_BINDING_HANDLE h; ULONG n; RPC_STATUS st; DWORD v = 0;
    RUP(a, 0x59, 1, 0);
    rx_raw_uuid(&ub, 0x5a); rx_raw_init(&b, &ub, 1, 0);
    /* b's dispatch table is private: make proc 4 distinguishable by swapping it for the echo routine */
    b.fn[4] = b.fn[0];
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&b.si, NULL, NULL), RPC_S_OK);
    h = a.h;
    st = rx_raw_call(h, &a.r, 4, &v, 4, &v, 4, &n);    EQ(st, RPC_S_OK); EQ(n, 0);       /* interface a: nop -> empty reply */
    v = 0x1234;
    st = rx_raw_call(h, &b, 4, &v, 4, &v, 4, &n);      EQ(st, RPC_S_OK); EQ(n, 4);       /* interface b: echo */
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&b.si, NULL, 1), RPC_S_OK);
    st = rx_raw_call(h, &b, 4, &v, 4, &v, 4, &n);      EQ(st, RPC_S_UNKNOWN_IF);
    st = rx_raw_call(h, &a.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK);                 /* a unaffected */
    raw_down(&a);
}

/* ---- object uuids and manager types -------------------------------------- */
TEST(ncalrpc_raw_object_uuid_default_nil) {
    rawfx f; struct { UUID u; DWORD st; } r; ULONG n; RPC_STATUS st; UUID nil;
    RUP(f, 0x60, 1, 0);
    memset(&nil, 0, sizeof nil);
    st = rx_raw_call(f.h, &f.r, 5, NULL, 0, &r, sizeof r, &n);
    EQ(st, RPC_S_OK); EQ(n, sizeof r);
    EQ(r.st, RPC_S_OK); CHECK(!memcmp(&r.u, &nil, sizeof nil));
    raw_down(&f);
}
TEST(ncalrpc_raw_object_uuid_reaches_server) {
    rawfx f; struct { UUID u; DWORD st; } r; ULONG n; RPC_STATUS st; UUID obj; RPC_BINDING_HANDLE h;
    RUP(f, 0x61, 1, 0);
    rx_raw_uuid(&obj, 0x6f);
    h = rx_bind_obj(f.ep, &obj);                /* object uuid is part of the string binding */
    CHECK(h != NULL);
    st = rx_raw_call(h, &f.r, 5, NULL, 0, &r, sizeof r, &n);
    EQ(st, RPC_S_OK); EQ(r.st, RPC_S_OK); CHECK(!memcmp(&r.u, &obj, sizeof obj));
    rx_unbind(h);
    raw_down(&f);
}
TEST(ncalrpc_raw_object_uuid_via_setobject) {
    rawfx f; struct { UUID u; DWORD st; } r; ULONG n; RPC_STATUS st; UUID obj, got;
    RUP(f, 0x62, 1, 0);
    rx_raw_uuid(&obj, 0x70);
    EQ(U(RpcBindingSetObject)(f.h, &obj), RPC_S_OK);
    memset(&got, 0xcc, sizeof got);
    EQ(U(RpcBindingInqObject)(f.h, &got), RPC_S_OK);
    CHECK(!memcmp(&got, &obj, sizeof obj));
    st = rx_raw_call(f.h, &f.r, 5, NULL, 0, &r, sizeof r, &n);
    EQ(st, RPC_S_OK); CHECK(!memcmp(&r.u, &obj, sizeof obj));
    /* back to nil */
    { UUID nil; memset(&nil, 0, sizeof nil); EQ(U(RpcBindingSetObject)(f.h, &nil), RPC_S_OK); }
    st = rx_raw_call(f.h, &f.r, 5, NULL, 0, &r, sizeof r, &n);
    EQ(st, RPC_S_OK); { UUID nil; memset(&nil, 0, sizeof nil); CHECK(!memcmp(&r.u, &nil, sizeof nil)); }
    raw_down(&f);
}
TEST(ncalrpc_raw_object_uuid_null_set_means_nil) {
    rawfx f; UUID obj, got, nil; memset(&nil, 0, sizeof nil);
    RUP(f, 0x63, 1, 0);
    rx_raw_uuid(&obj, 0x71);
    EQ(U(RpcBindingSetObject)(f.h, &obj), RPC_S_OK);
    EQ(U(RpcBindingSetObject)(f.h, NULL), RPC_S_OK);       /* NULL = nil object */
    EQ(U(RpcBindingInqObject)(f.h, &got), RPC_S_OK);
    CHECK(!memcmp(&got, &nil, sizeof nil));
    raw_down(&f);
}
TEST(ncalrpc_raw_mgr_type_routing) {
    /* interface registered for manager type M only: object uuids mapped to M reach it, nil object does not */
    rawfx f; UUID mt, obj; WCHAR ep[64]; RPC_BINDING_HANDLE hn, ho; ULONG n; RPC_STATUS st;
    memset(&f, 0, sizeof f);
    rx_server_reset(); rx_ep(ep, 64);
    rx_raw_uuid(&f.u, 0x64); rx_raw_init(&f.r, &f.u, 1, 0); rx_raw_uuid(&mt, 0x65); rx_raw_uuid(&obj, 0x66);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&f.r.si, &mt, NULL), RPC_S_OK);
    EQ(U(RpcObjectSetType)(&obj, &mt), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    ho = rx_bind_obj(ep, &obj); hn = rx_bind(ep);
    st = rx_raw_call(ho, &f.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK);
    st = rx_raw_call(hn, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_UNSUPPORTED_TYPE);
    t_note("nil-object call to an interface registered for a specific manager type -> %lu", (unsigned long)st);
    EQ(U(RpcObjectSetType)(&obj, NULL), RPC_S_OK);       /* object back to nil type */
    st = rx_raw_call(ho, &f.r, 4, NULL, 0, NULL, 0, &n);
    CHECK(st != RPC_S_OK);
    t_note("call after RpcObjectSetType(obj, nil) -> %lu", (unsigned long)st);
    rx_unbind(ho); rx_unbind(hn);
    rx_server_down((RPC_IF_HANDLE)&f.r.si, &mt);
}
TEST(ncalrpc_raw_two_managers_same_if) {
    /* the same interface registered with two manager types (separate EPVs); object uuid selects the manager. */
    rawfx f; UUID m1, m2, o1, o2; WCHAR ep[64]; RPC_BINDING_HANDLE h1, h2; ULONG n; RPC_STATUS st; static void *epv1[1], *epv2[1];
    memset(&f, 0, sizeof f);
    rx_server_reset(); rx_ep(ep, 64);
    rx_raw_uuid(&f.u, 0x67); rx_raw_init(&f.r, &f.u, 1, 0);
    rx_raw_uuid(&m1, 0x68); rx_raw_uuid(&m2, 0x69); rx_raw_uuid(&o1, 0x6a); rx_raw_uuid(&o2, 0x6b);
    EQ(rx_use_ep(ep), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&f.r.si, &m1, (RPC_MGR_EPV *)epv1), RPC_S_OK);
    EQ(U(RpcServerRegisterIf)((RPC_IF_HANDLE)&f.r.si, &m2, (RPC_MGR_EPV *)epv2), RPC_S_OK);
    EQ(U(RpcObjectSetType)(&o1, &m1), RPC_S_OK);
    EQ(U(RpcObjectSetType)(&o2, &m2), RPC_S_OK);
    EQ(U(RpcServerListen)(1, 20, TRUE), RPC_S_OK);
    h1 = rx_bind_obj(ep, &o1); h2 = rx_bind_obj(ep, &o2);
    st = rx_raw_call(h1, &f.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK);
    st = rx_raw_call(h2, &f.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK);
    if (!t_is_wine()) {
        void *got = NULL;
        EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&f.r.si, &m1, (RPC_MGR_EPV **)&got), RPC_S_OK); CHECK(got == epv1);
        EQ(U(RpcServerInqIf)((RPC_IF_HANDLE)&f.r.si, &m2, (RPC_MGR_EPV **)&got), RPC_S_OK); CHECK(got == epv2);
    }
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&f.r.si, &m1, 1), RPC_S_OK);
    st = rx_raw_call(h1, &f.r, 4, NULL, 0, NULL, 0, &n); CHECK(st != RPC_S_OK);
    st = rx_raw_call(h2, &f.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK);        /* m2 still registered */
    rx_unbind(h1); rx_unbind(h2);
    rx_server_down((RPC_IF_HANDLE)&f.r.si, &m2);
}
struct ot_call { int set; UUID *obj; UUID *type; RPC_STATUS st; };
static void ot_run(void *p) {
    struct ot_call *c = p;
    c->st = c->set ? U(RpcObjectSetType)(c->obj, c->type) : U(RpcObjectInqType)(c->obj, c->type);
}
TEST(ncalrpc_raw_objecttype_inq) {
    UUID obj, mt, got, nil; struct ot_call c; DWORD x; memset(&nil, 0, sizeof nil);
    REQUIRE(U(RpcObjectInqType));
    if (t_is_wine()) { t_skip("RpcObjectInqType is a stub in wine"); return; }
    rx_raw_uuid(&obj, 0x6c); rx_raw_uuid(&mt, 0x6d);
    EQ(U(RpcObjectSetType)(&obj, &mt), RPC_S_OK);
    memset(&got, 0xcc, sizeof got);
    EQ(U(RpcObjectInqType)(&obj, &got), RPC_S_OK); CHECK(!memcmp(&got, &mt, sizeof mt));
    EQ(U(RpcObjectSetType)(&obj, NULL), RPC_S_OK);
    memset(&got, 0xcc, sizeof got);
    EQ(U(RpcObjectInqType)(&obj, &got), RPC_S_OBJECT_NOT_FOUND);     /* an object without a registered type is not found */
    /* NULL / nil object arguments: no validation on Windows, record what happens */
    c.set = 0; c.obj = NULL; c.type = &got; c.st = 0xdead; x = rx_guarded(ot_run, &c);
    t_note("RpcObjectInqType(NULL object) -> exception 0x%lx status %lu", (unsigned long)x, (unsigned long)c.st);
    c.set = 0; c.obj = &nil; c.type = &got; c.st = 0xdead; x = rx_guarded(ot_run, &c);
    t_note("RpcObjectInqType(nil object) -> exception 0x%lx status %lu", (unsigned long)x, (unsigned long)c.st);
    c.set = 1; c.obj = &nil; c.type = &mt; c.st = 0xdead; x = rx_guarded(ot_run, &c);
    t_note("RpcObjectSetType(nil object, type) -> exception 0x%lx status %lu", (unsigned long)x, (unsigned long)c.st);
    c.set = 1; c.obj = NULL; c.type = &mt; c.st = 0xdead; x = rx_guarded(ot_run, &c);
    t_note("RpcObjectSetType(NULL object, type) -> exception 0x%lx status %lu", (unsigned long)x, (unsigned long)c.st);
}

/* ---- stopping from inside a call ----------------------------------------- */
TEST(ncalrpc_raw_server_stops_itself_from_call) {
    rawfx f; DWORD st_in_server = 0xdead; ULONG n; RPC_STATUS st; DWORD t0;
    RUP(f, 0x72, 1, 0);
    st = rx_raw_call(f.h, &f.r, 6, NULL, 0, &st_in_server, 4, &n);
    EQ(st, RPC_S_OK); EQ(st_in_server, RPC_S_OK);        /* RpcMgmtStopServerListening(NULL) from the manager routine succeeds */
    for (t0 = GetTickCount(); rx_listening() && GetTickCount() - t0 < 5000; ) Sleep(10);
    EQ(U(RpcMgmtWaitServerListen)(), RPC_S_OK);
    EQ(rx_listening(), 0);
    raw_down(&f);
}

/* ---- cancel --------------------------------------------------------------- */
typedef struct { RPC_BINDING_HANDLE h; rx_raw *r; DWORD reply[3]; RPC_STATUS st; DWORD tid; } cancel_job;
static volatile DWORD g_cancel_tid;
static DWORD WINAPI cancel_client(void *p) {
    cancel_job *j = p; ULONG n = 0;
    j->tid = GetCurrentThreadId(); g_cancel_tid = j->tid;
    j->st = rx_raw_call(j->h, j->r, 7, NULL, 0, j->reply, sizeof j->reply, &n);
    return 0;
}
TEST(ncalrpc_raw_cancel_thread_reaches_server) {
    rawfx f; cancel_job j; HANDLE th, tt; DWORD t0; RPC_STATUS cst;
    REQUIRE(U(RpcCancelThread));
    if (t_is_wine()) { t_skip("RpcServerTestCancel unimplemented in wine"); return; }
    RUP(f, 0x73, 1, 0);
    memset(&j, 0, sizeof j); j.h = f.h; j.r = &f.r; j.reply[0] = 0xdead; j.st = 0xdead; g_cancel_tid = 0;
    th = rx_thread(cancel_client, &j);
    for (t0 = GetTickCount(); !g_cancel_tid && GetTickCount() - t0 < 5000; ) Sleep(5);
    Sleep(300);                                   /* let the call reach the server */
    tt = OpenThread(THREAD_ALL_ACCESS, FALSE, g_cancel_tid);
    CHECK(tt != NULL);
    cst = tt ? U(RpcCancelThread)(tt) : 0xdead;
    t_note("RpcCancelThread on the thread inside the call -> %lu (GLE %lu)", (unsigned long)cst, (unsigned long)GetLastError());
    EQ(cst, RPC_S_OK);
    CHECK(rx_join(th, 10000));
    if (tt) CloseHandle(tt);
    t_note("cancelled call: client status %lu, server TestCancel last %lu first %lu after %lu ms", (unsigned long)j.st,
           (unsigned long)j.reply[0], (unsigned long)j.reply[1], (unsigned long)j.reply[2]);
    EQ(j.st, RPC_S_OK);                           /* the cancel is delivered to the server routine; the call itself completes normally */
    EQ(j.reply[1], RPC_S_CALL_IN_PROGRESS);       /* not cancelled at the first poll */
    EQ(j.reply[0], RPC_S_OK);                     /* RpcServerTestCancel reported the cancel (OK = cancelled) */
    raw_down(&f);
}
TEST(ncalrpc_raw_cancel_thread_no_call_active) {
    RPC_STATUS st;
    REQUIRE(U(RpcCancelThread)); REQUIRE(U(RpcTestCancel));
    if (t_is_wine()) { t_skip("RpcTestCancel unimplemented in wine"); return; }
    st = U(RpcCancelThread)(GetCurrentThread());
    t_note("RpcCancelThread on a thread with no call -> %lu", (unsigned long)st);
    CHECK(st == RPC_S_OK);
    st = U(RpcTestCancel)();
    t_note("RpcTestCancel after RpcCancelThread(self) -> %lu", (unsigned long)st);
    EQ(st, RPC_S_NO_CALL_ACTIVE);                 /* the thread is not inside a client call, so the queued cancel is not observed */
    st = U(RpcTestCancel)();
    t_note("second RpcTestCancel -> %lu", (unsigned long)st);
}
TEST(ncalrpc_raw_test_cancel_uncancelled_server_call) {
    rawfx f; DWORD reply[3] = { 0xdead, 0xdead, 0 }; ULONG n; RPC_STATUS st;
    REQUIRE(U(RpcServerTestCancel));
    if (t_is_wine()) { t_skip("RpcServerTestCancel unimplemented in wine"); return; }
    RUP(f, 0x74, 1, 0);
    /* nobody cancels: the server polls RpcServerTestCancel(NULL) for at most 5 s */
    st = rx_raw_call(f.h, &f.r, 7, NULL, 0, reply, sizeof reply, &n);
    EQ(st, RPC_S_OK);
    EQ(reply[0], RPC_S_CALL_IN_PROGRESS);         /* RpcServerTestCancel: OK = cancelled, CALL_IN_PROGRESS = not cancelled */
    CHECK(reply[2] >= 4000);                      /* the handler polled the whole window */
    t_note("uncancelled call: TestCancel first %lu last %lu after %lu ms", (unsigned long)reply[1], (unsigned long)reply[0], (unsigned long)reply[2]);
    raw_down(&f);
}
TEST(ncalrpc_raw_servertestcancel_outside_call) {
    RPC_STATUS st;
    REQUIRE(U(RpcServerTestCancel));
    if (t_is_wine()) { t_skip("RpcServerTestCancel unimplemented in wine"); return; }
    st = U(RpcServerTestCancel)(NULL);
    t_note("RpcServerTestCancel(NULL) outside any call -> %lu", (unsigned long)st);
    EQ(st, RPC_S_NO_CALL_ACTIVE);
}
TEST(ncalrpc_raw_cancel_timeout_setting) {
    REQUIRE(U(RpcMgmtSetCancelTimeout));
    EQ(U(RpcMgmtSetCancelTimeout)(RPC_C_CANCEL_INFINITE_TIMEOUT), RPC_S_OK);
    EQ(U(RpcMgmtSetCancelTimeout)(5), RPC_S_OK);
    EQ(U(RpcMgmtSetCancelTimeout)(0), RPC_S_OK);
    t_note("RpcMgmtSetCancelTimeout(-2) -> %lu", (unsigned long)U(RpcMgmtSetCancelTimeout)(-2));
    EQ(U(RpcMgmtSetCancelTimeout)(RPC_C_CANCEL_INFINITE_TIMEOUT), RPC_S_OK);
}

/* ---- client-side binding bookkeeping ------------------------------------- */
TEST(ncalrpc_raw_binding_free_twice) {
    rawfx f; RPC_BINDING_HANDLE h, h2;
    RUP(f, 0x75, 1, 0);
    h = rx_bind(f.ep); h2 = h;
    EQ(U(RpcBindingFree)(&h), RPC_S_OK);
    CHECK(h == NULL);
    EQ(U(RpcBindingFree)(&h), RPC_S_INVALID_BINDING);       /* NULL handle */
    (void)h2;
    raw_down(&f);
}
TEST(ncalrpc_raw_binding_copy_independent) {
    rawfx f; RPC_BINDING_HANDLE c = NULL; ULONG n; RPC_STATUS st; UUID o, got;
    RUP(f, 0x76, 1, 0);
    rx_raw_uuid(&o, 0x77);
    EQ(U(RpcBindingSetObject)(f.h, &o), RPC_S_OK);
    EQ(U(RpcBindingCopy)(f.h, &c), RPC_S_OK);
    CHECK(c != NULL && c != f.h);
    EQ(U(RpcBindingInqObject)(c, &got), RPC_S_OK); CHECK(!memcmp(&got, &o, sizeof o));    /* copy carries the object uuid */
    EQ(U(RpcBindingFree)(&f.h), RPC_S_OK);                  /* original freed, copy keeps working */
    st = rx_raw_call(c, &f.r, 4, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK);
    rx_unbind(c);
    raw_down(&f);
}
TEST(ncalrpc_raw_binding_reset) {
    rawfx f; RPC_WSTR str = NULL, ept = NULL; RPC_STATUS st;
    REQUIRE(U(RpcBindingReset));
    RUP(f, 0x78, 1, 0);
    EQ(U(RpcBindingReset)(f.h), RPC_S_OK);
    /* the endpoint is cleared (binding becomes partially bound); not called through, since resolving it would
       go to the endpoint mapper */
    EQ(U(RpcBindingToStringBindingW)(f.h, &str), RPC_S_OK);
    st = U(RpcStringBindingParseW)(str, NULL, NULL, NULL, &ept, NULL);
    EQ(st, RPC_S_OK);
    t_note("after RpcBindingReset: string binding %ls, endpoint component %ls", str ? (WCHAR *)str : L"", ept ? (WCHAR *)ept : L"(null)");
    CHECK(!ept || !*ept || lstrcmpiW((WCHAR *)ept, f.ep) != 0);
    if (ept) U(RpcStringFreeW)(&ept);
    if (str) U(RpcStringFreeW)(&str);
    raw_down(&f);
}
TEST(ncalrpc_raw_binding_to_string_roundtrip) {
    rawfx f; RPC_WSTR s = NULL, proto = NULL, ept = NULL, addr = NULL, obj = NULL, opts = NULL;
    RPC_BINDING_HANDLE h2 = NULL; WCHAR want[96];
    RUP(f, 0x79, 1, 0);
    EQ(U(RpcBindingToStringBindingW)(f.h, &s), RPC_S_OK);
    wsprintfW(want, L"ncalrpc:[%s]", f.ep);
    CHECK(s && !lstrcmpW((WCHAR *)s, want));
    EQ(U(RpcStringBindingParseW)(s, &obj, &proto, &addr, &ept, &opts), RPC_S_OK);
    CHECK(proto && !lstrcmpW((WCHAR *)proto, L"ncalrpc"));
    CHECK(ept && !lstrcmpW((WCHAR *)ept, f.ep));
    CHECK(!obj || !*obj); CHECK(!opts || !*opts);       /* absent components: empty (or NULL) */
    t_note("netaddr component of an ncalrpc string binding is %s", addr && *addr ? "non-empty" : "empty");
    U(RpcStringFreeW)(&proto); U(RpcStringFreeW)(&ept); U(RpcStringFreeW)(&addr); U(RpcStringFreeW)(&obj); U(RpcStringFreeW)(&opts);
    EQ(U(RpcBindingFromStringBindingW)(s, &h2), RPC_S_OK);
    { ULONG n; EQ(rx_raw_call(h2, &f.r, 4, NULL, 0, NULL, 0, &n), RPC_S_OK); }
    rx_unbind(h2);
    U(RpcStringFreeW)(&s);
    raw_down(&f);
}
TEST(ncalrpc_raw_server_binding_inq_endpoint_string) {
    /* a client binding string can also be composed with an object uuid and options */
    WCHAR ep[64]; RPC_WSTR s = NULL, os = NULL; UUID o;
    rx_ep(ep, 64); rx_raw_uuid(&o, 0x7a);
    EQ(U(UuidToStringW)(&o, &os), RPC_S_OK);
    EQ(U(RpcStringBindingComposeW)(os, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)ep, NULL, &s), RPC_S_OK);
    {
        WCHAR want[160]; wsprintfW(want, L"%s@ncalrpc:[%s]", os, ep);
        CHECK(s && !lstrcmpW((WCHAR *)s, want));
    }
    U(RpcStringFreeW)(&s); U(RpcStringFreeW)(&os);
}
TEST(ncalrpc_raw_binding_from_invalid_string) {
    RPC_BINDING_HANDLE h = NULL; RPC_STATUS st;
    st = U(RpcBindingFromStringBindingW)((RPC_WSTR)L"this is not a binding", &h);
    EQ(st, RPC_S_INVALID_STRING_BINDING); CHECK(h == NULL);
    if (st == RPC_S_OK) U(RpcBindingFree)(&h);
    st = U(RpcBindingFromStringBindingW)((RPC_WSTR)L"", &h);
    t_note("empty string binding -> %lu", (unsigned long)st);
    if (st == RPC_S_OK) U(RpcBindingFree)(&h);
    st = U(RpcBindingFromStringBindingW)((RPC_WSTR)L"ncalrpc:[ep", &h);
    t_note("unterminated endpoint bracket -> %lu", (unsigned long)st);
    if (st == RPC_S_OK) U(RpcBindingFree)(&h);
    st = U(RpcBindingFromStringBindingW)((RPC_WSTR)L"not_a_protseq:[x]", &h);
    t_note("unknown protseq string binding -> %lu", (unsigned long)st);
    if (st == RPC_S_OK) U(RpcBindingFree)(&h);
}
TEST(ncalrpc_raw_binding_without_endpoint_call) {
    /* ncalrpc binding with no endpoint: the call cannot be routed (no endpoint mapper for a bare ncalrpc binding) */
    RPC_WSTR s = NULL; RPC_BINDING_HANDLE h = NULL; rx_raw r; UUID u; ULONG n; RPC_STATUS st;
    rx_raw_uuid(&u, 0x7b); rx_raw_init(&r, &u, 1, 0);
    EQ(U(RpcStringBindingComposeW)(NULL, (RPC_WSTR)L"ncalrpc", NULL, NULL, NULL, &s), RPC_S_OK);
    EQ(U(RpcBindingFromStringBindingW)(s, &h), RPC_S_OK);
    st = rx_raw_call(h, &r, 0, NULL, 0, NULL, 0, &n);
    CHECK(st != RPC_S_OK);
    t_note("call on an ncalrpc binding without endpoint -> %lu", (unsigned long)st);
    rx_unbind(h); U(RpcStringFreeW)(&s);
}
TEST(ncalrpc_raw_getbuffer_invalid_binding) {
    RPC_MESSAGE m; rx_raw r; UUID u; RPC_STATUS st;
    rx_raw_uuid(&u, 0x7c); rx_raw_init(&r, &u, 1, 0);
    memset(&m, 0, sizeof m);
    m.Handle = NULL; m.DataRepresentation = 0x10; m.BufferLength = 4; m.RpcInterfaceInformation = &r.ci;
    st = U(I_RpcGetBuffer)(&m);
    CHECK(st != RPC_S_OK);
    t_note("I_RpcGetBuffer with a NULL binding handle -> %lu", (unsigned long)st);
}
