/* Delegating (CStdStubBuffer2_*) and asynchronous (CStdAsyncStubBuffer_*) stub-buffer routines exercised on
 * hand-laid-out stub objects: their behaviour depends only on the documented CStdStubBuffer layout
 * (lpVtbl / RefCount / pvServerObject / ...) and on the CInterfaceStubHeader found in front of the vtable.
 * Semantics are those of the Win10 combase decomp. */
#include "psd.h"

typedef HRESULT (WINAPI *connect_fn)(IRpcStubBuffer *, IUnknown *);
typedef void (WINAPI *disconnect_fn)(IRpcStubBuffer *);
typedef ULONG (WINAPI *refs_fn)(IRpcStubBuffer *);
typedef HRESULT (WINAPI *qi_fn)(IRpcStubBuffer *, REFIID, void **);
typedef HRESULT (WINAPI *invoke_fn)(IRpcStubBuffer *, RPCOLEMESSAGE *, IRpcChannelBuffer *);

static void *cb(const char *n) { return (void *)t_proc_in("combase.dll", n); }

/* ---- a recording base stub ------------------------------------------------------------------- */
typedef struct BaseStub {
    IRpcStubBuffer iface;
    IRpcStubBufferVtbl vtbl;
    LONG n_connect, n_disconnect, n_countrefs;
    IUnknown *connect_arg;
    HRESULT connect_hr;
    ULONG countrefs;
    int order_disconnect;           /* sequence number of the Disconnect call */
} BaseStub;
static int g_seq;
static HRESULT WINAPI b_connect(IRpcStubBuffer *p, IUnknown *u) { BaseStub *b = (BaseStub *)p; b->n_connect++; b->connect_arg = u; return b->connect_hr; }
static void WINAPI b_disconnect(IRpcStubBuffer *p) { BaseStub *b = (BaseStub *)p; b->n_disconnect++; b->order_disconnect = ++g_seq; }
static ULONG WINAPI b_countrefs(IRpcStubBuffer *p) { BaseStub *b = (BaseStub *)p; b->n_countrefs++; return b->countrefs; }
static BaseStub *base_new(void) {
    BaseStub *b = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *b);
    b->vtbl.Connect = b_connect; b->vtbl.Disconnect = b_disconnect; b->vtbl.CountRefs = b_countrefs;
    b->iface.lpVtbl = &b->vtbl;
    b->connect_hr = S_OK;
    return b;
}

/* delegating stub layout: [base_obj slot][base stub][CStdStubBuffer] */
typedef struct FakeDeleg {
    void *base_obj;
    IRpcStubBuffer *base_stub;
    CStdStubBuffer stub;
} FakeDeleg;
static void deleg_init(FakeDeleg *d, BaseStub *base, PsdMock *srv) {
    memset(d, 0, sizeof *d);
    d->base_stub = base ? &base->iface : NULL;
    d->stub.lpVtbl = &psd_stub_vtbl[0]->Vtbl;       /* the header in front of it supplies piid = IID_One */
    d->stub.RefCount = 1;
    d->stub.pvServerObject = srv ? &srv->iface : NULL;
    if (srv) { srv->ref++; }
}
#define DELEG_NEED() connect_fn f_conn = cb("CStdStubBuffer2_Connect"); disconnect_fn f_disc = cb("CStdStubBuffer2_Disconnect"); \
    refs_fn f_refs = cb("CStdStubBuffer2_CountRefs"); \
    if (!f_conn || !f_disc || !f_refs || !psd_init()) { t_skip("no CStdStubBuffer2_*"); return; } \
    if (t_is_wine()) { t_skip("wine's delegating stubs live in rpcrt4 and use a different layout contract"); return; }

TEST(psdd_stub2_countrefs) {
    DELEG_NEED();
    PsdMock *srv = psd_mock_new();
    BaseStub *base = base_new();
    base->countrefs = 5;
    FakeDeleg d;
    deleg_init(&d, base, srv);
    EQ(f_refs((IRpcStubBuffer *)&d.stub), 6);        /* own connection + base's count */
    EQ(base->n_countrefs, 1);
    d.stub.pvServerObject = NULL;
    EQ(f_refs((IRpcStubBuffer *)&d.stub), 5);
    base->countrefs = 0;
    EQ(f_refs((IRpcStubBuffer *)&d.stub), 0);
    d.base_stub = NULL;                                      /* no base stub: just the own count */
    d.stub.pvServerObject = &srv->iface;
    EQ(f_refs((IRpcStubBuffer *)&d.stub), 1);
    d.stub.pvServerObject = NULL;
    EQ(f_refs((IRpcStubBuffer *)&d.stub), 0);
    srv->ref--;
    psd_mock_free(srv); HeapFree(GetProcessHeap(), 0, base);
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdd_stub2_connect_forwards_to_base) {
    DELEG_NEED();
    PsdMock *old = psd_mock_new(), *nw = psd_mock_new();
    BaseStub *base = base_new();
    FakeDeleg d;
    deleg_init(&d, base, old);
    HRESULT hr = f_conn((IRpcStubBuffer *)&d.stub, &nw->iface);
    EQHR(hr, S_OK);
    EQ(nw->n_qi, 1);
    CHECK(IsEqualGUID(&nw->last_iid, &psd_IID_One));
    EQ(d.stub.pvServerObject, &nw->iface);
    EQ(old->n_release, 1);
    EQ(base->n_connect, 1);
    EQ(base->connect_arg, (IUnknown *)&d.base_obj);          /* the base stub is connected to the address of the base-object slot */
    /* a failing base Connect is the result of the whole call */
    base->connect_hr = (HRESULT)0x8abc0007;
    EQHR(f_conn((IRpcStubBuffer *)&d.stub, &nw->iface), (HRESULT)0x8abc0007);
    EQ(base->n_connect, 2);
    /* a failing own Connect (QueryInterface fails) never reaches the base */
    base->connect_hr = S_OK;
    PsdMock *bad = psd_mock_new();
    bad->qi_hr = E_NOINTERFACE; bad->qi_out = NULL;
    EQHR(f_conn((IRpcStubBuffer *)&d.stub, &bad->iface), E_NOINTERFACE);
    EQ(base->n_connect, 2);
    /* no base stub: behaves like CStdStubBuffer_Connect */
    d.base_stub = NULL;
    EQHR(f_conn((IRpcStubBuffer *)&d.stub, &nw->iface), S_OK);
    EQ(base->n_connect, 2);
    d.stub.pvServerObject = NULL;
    nw->ref = 1;
    psd_mock_free(old); psd_mock_free(nw); psd_mock_free(bad); HeapFree(GetProcessHeap(), 0, base);
}

TEST(psdd_stub2_disconnect_order) {
    DELEG_NEED();
    PsdMock *srv = psd_mock_new();
    BaseStub *base = base_new();
    FakeDeleg d;
    deleg_init(&d, base, srv);
    LONG rel0 = srv->n_release;
    f_disc((IRpcStubBuffer *)&d.stub);
    EQ(base->n_disconnect, 1);                               /* the base stub is disconnected ... */
    EQ(srv->n_release - rel0, 1);                            /* ... and the own server released */
    CHECK(d.stub.pvServerObject == NULL);
    f_disc((IRpcStubBuffer *)&d.stub);                       /* second call: base called again, nothing left to release */
    EQ(base->n_disconnect, 2);
    EQ(srv->n_release - rel0, 1);
    d.base_stub = NULL;
    f_disc((IRpcStubBuffer *)&d.stub);                       /* NULL base stub tolerated */
    EQ(base->n_disconnect, 2);
    srv->ref--;
    psd_mock_free(srv); HeapFree(GetProcessHeap(), 0, base);
}

/* ---- asynchronous stub buffer ---------------------------------------------------------------- */
typedef struct FakeAsync {
    CStdStubBuffer stub;
    void *pad[8];
} FakeAsync;
static void async_init(FakeAsync *a, const IRpcStubBufferVtbl *v, PsdMock *srv) {
    memset(a, 0, sizeof *a);
    a->stub.lpVtbl = v;
    a->stub.RefCount = 1;
    a->stub.pvServerObject = srv ? &srv->iface : NULL;
}

TEST(psda_async_connect_addrefs_server) {
    connect_fn f_conn = cb("CStdAsyncStubBuffer_Connect"); disconnect_fn f_disc = cb("CStdAsyncStubBuffer_Disconnect");
    if (!f_conn || !f_disc || !psd_init()) { t_skip("no CStdAsyncStubBuffer_*"); return; }
    if (t_is_wine()) { t_skip("not implemented as separate async entry points in wine"); return; }
    PsdMock *old = psd_mock_new(), *nw = psd_mock_new();
    nw->qi_hr = E_NOINTERFACE;                               /* proves QueryInterface is not used */
    FakeAsync a; async_init(&a, &psd_stub_vtbl[0]->Vtbl, old);
    old->ref = 2;
    HRESULT hr = f_conn((IRpcStubBuffer *)&a.stub, &nw->iface);
    EQHR(hr, S_OK);                                          /* always S_OK */
    EQ(nw->n_qi, 0);
    EQ(nw->n_addref, 1);                                     /* the server is AddRef'd directly */
    EQ(nw->ref, 2);
    EQ(a.stub.pvServerObject, &nw->iface);
    EQ(old->n_release, 1);                                   /* the previous server is released */
    f_disc((IRpcStubBuffer *)&a.stub);
    EQ(nw->n_release, 1);
    CHECK(a.stub.pvServerObject == NULL);
    f_disc((IRpcStubBuffer *)&a.stub);
    EQ(nw->n_release, 1);
    psd_mock_free(old); psd_mock_free(nw);
}

TEST(psda_async_addref_and_qi) {
    refs_fn f_add = cb("CStdAsyncStubBuffer_AddRef");
    qi_fn f_qi = cb("CStdAsyncStubBuffer_QueryInterface");
    if (!f_add || !f_qi || !psd_init()) { t_skip("no CStdAsyncStubBuffer_*"); return; }
    if (t_is_wine()) { t_skip("not implemented as separate async entry points in wine"); return; }
    IRpcStubBufferVtbl v = psd_stub_vtbl[0]->Vtbl;
    v.AddRef = (void *)f_add;
    FakeAsync a; async_init(&a, &v, NULL);
    EQ(f_add((IRpcStubBuffer *)&a.stub), 2);
    EQ(f_add((IRpcStubBuffer *)&a.stub), 3);
    a.stub.RefCount = 1;
    const GUID *ok[2] = { &psd_IID_IUnknown, &psd_IID_IRpcStubBuffer };
    for (int i = 0; i < 2; i++) {
        void *pv = NULL;
        EQHR(f_qi((IRpcStubBuffer *)&a.stub, ok[i], &pv), S_OK);
        EQ(pv, &a.stub);
        EQ(a.stub.RefCount, 2);
        a.stub.RefCount = 1;
    }
    const GUID *bad[3] = { &psd_IID_IDispatch, &psd_IID_One, &psd_IID_IRpcProxyBuffer };
    for (int i = 0; i < 3; i++) {
        void *pv = (void *)1;
        HRESULT hr = f_qi((IRpcStubBuffer *)&a.stub, bad[i], &pv);
        CHECKF(hr == E_NOINTERFACE, "bad %d hr=0x%lx", i, hr);
        CHECK(pv == NULL);
    }
    EQ(a.stub.RefCount, 1);
}

static void do_async_invoke(void *a) {
    void **v = a;
    invoke_fn f = (invoke_fn)v[3];
    f((IRpcStubBuffer *)v[0], (RPCOLEMESSAGE *)v[1], (IRpcChannelBuffer *)v[2]);
}
TEST(psda_async_invoke_validation) {
    invoke_fn f = cb("CStdAsyncStubBuffer_Invoke");
    if (!f || !psd_init()) { t_skip("no CStdAsyncStubBuffer_Invoke"); return; }
    if (t_is_wine()) { t_skip("wine would dispatch instead of raising"); return; }
    FakeAsync a; async_init(&a, &psd_stub_vtbl[0]->Vtbl, NULL);   /* IOletOne: valid iMethod is 3 only */
    PsdChan *ch = psd_chan_new(NULL);
    static const ULONG bad[] = { 0, 1, 2, 4, 5, 0x8003, 0xffffffffu };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
        msg.iMethod = bad[i]; msg.dataRepresentation = 0x10;
        void *args[4] = { &a.stub, &msg, &ch->iface, (void *)f };
        DWORD code = 0x6d1;
        int caught = psd_catch(do_async_invoke, args, &code);
        CHECKF(caught && code == 0x6d1, "iMethod %lu: caught=%d code=0x%lx", bad[i], caught, code);   /* RPC_S_PROCNUM_OUT_OF_RANGE */
    }
    /* a valid method on a stub whose MIDL version predates async support */
    MIDL_STUB_DESC old = psd_stub_desc;
    old.MIDLVersion = 0x5000087;
    MIDL_SERVER_INFO si = psd_server_info1;
    si.pStubDesc = &old;
    CInterfaceStubVtbl sv = *psd_stub_vtbl[0];
    sv.header.pServerInfo = &si;
    async_init(&a, &sv.Vtbl, NULL);
    RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
    msg.iMethod = 3; msg.dataRepresentation = 0x10;
    void *args[4] = { &a.stub, &msg, &ch->iface, (void *)f };
    DWORD code = 0x6e6;
    int caught = psd_catch(do_async_invoke, args, &code);
    CHECKF(caught && code == 0x6e6, "old MIDL version: caught=%d code=0x%lx", caught, code);
    HeapFree(GetProcessHeap(), 0, ch);
}

TEST(psda_dcom_async_helpers_resolve) {
    HMODULE r = t_module("rpcrt4.dll");
    static const char *const n[] = { "NdrDcomAsyncClientCall", "NdrDcomAsyncStubCall", "NdrAsyncClientCall", "NdrAsyncServerCall",
        "NdrpCreateProxy", "NdrpCreateStub", "NdrServerInitializePartial" };   /* NdrStubInitializePartial is not an rpcrt4 export on Win10 */
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        FARPROC p = GetProcAddress(r, n[i]);
        if (t_is_wine() && !p) { t_note("wine lacks rpcrt4!%s", n[i]); continue; }
        CHECKF(p != NULL, "rpcrt4!%s", n[i]);
    }
}
