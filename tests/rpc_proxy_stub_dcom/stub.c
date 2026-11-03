/* IRpcStubBuffer implementation provided by rpcrt4/combase (CStdStubBuffer_* / NdrCStdStubBuffer_Release) on stubs
 * created through the hand-built IPSFactoryBuffer.  Semantics are taken from the Win10 combase decomp:
 *   Connect      : QI(server, piid, &new); swap into pvServerObject; Release(old); returns the QI hr
 *   Disconnect   : swap NULL into pvServerObject; Release(old)
 *   IsIIDSupported: piid match && connected -> AddRef(stub), return stub; else NULL
 *   CountRefs    : connected ? 1 : 0
 *   DebugServerQueryInterface: *ppv = server; connected ? S_OK : CO_E_OBJNOTCONNECTED
 *   Invoke       : iMethod < 3 or >= DispatchTableCount -> RpcRaiseException(RPC_S_PROCNUM_OUT_OF_RANGE)  */
#include "psd.h"

typedef struct StubCtx { IPSFactoryBuffer *psf; PsdMock *srv; IRpcStubBuffer *sb; CStdStubBuffer *cs; } StubCtx;

static int ctx_up(StubCtx *c, const GUID *iid) {
    memset(c, 0, sizeof *c);
    if (!psd_init()) return 0;
    if (psd_get_factory(&c->psf) != S_OK || !c->psf) return 0;
    c->srv = psd_mock_new();
    if (IPSFactoryBuffer_CreateStub(c->psf, iid, &c->srv->iface, &c->sb) != S_OK || !c->sb) return 0;
    c->cs = (CStdStubBuffer *)c->sb;
    return 1;
}
static void ctx_down(StubCtx *c) {
    if (c->sb) { IRpcStubBuffer_Disconnect(c->sb); IRpcStubBuffer_Release(c->sb); }
    if (c->srv) psd_mock_free(c->srv);
    if (c->psf) IPSFactoryBuffer_Release(c->psf);
}
#define CTX(c, iid) StubCtx c; if (!ctx_up(&c, iid)) { t_skip("cannot build stub"); ctx_down(&c); return; }

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psds_connect_replaces_server) {
    PSD_STA(); CTX(c, &psd_IID_One);
    PsdMock *nw = psd_mock_new();
    EQ(c.srv->ref, 2); EQ(c.srv->n_release, 0);
    HRESULT hr = IRpcStubBuffer_Connect(c.sb, &nw->iface);
    EQHR(hr, S_OK);
    EQ(nw->n_qi, 1);
    CHECK(IsEqualGUID(&nw->last_iid, &psd_IID_One));      /* QI for the stub's own interface */
    EQ(c.cs->pvServerObject, &nw->iface);
    EQ(nw->ref, 2);                                        /* new server AddRef'd by its own QI */
    EQ(c.srv->n_release, 1);                               /* old server released exactly once */
    EQ(c.srv->ref, 1);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 1);
    IRpcStubBuffer_Disconnect(c.sb);
    EQ(nw->ref, 1);
    psd_mock_free(nw);
    ctx_down(&c); PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psds_connect_qi_failure) {
    PSD_STA(); CTX(c, &psd_IID_One);
    PsdMock *nw = psd_mock_new();
    nw->qi_hr = E_NOINTERFACE; nw->qi_out = NULL;
    HRESULT hr = IRpcStubBuffer_Connect(c.sb, &nw->iface);
    EQHR(hr, E_NOINTERFACE);                               /* QI result is returned verbatim */
    EQ(c.srv->n_release, 1);                               /* ... but the old server is dropped anyway */
    CHECK(c.cs->pvServerObject == NULL);                   /* whatever QI stored (NULL here) replaces it */
    EQ(IRpcStubBuffer_CountRefs(c.sb), 0);
    /* a QI that fails but stores a value: the stub keeps the stored pointer without owning anything else */
    PsdMock *w = psd_mock_new();   /* only used as an address */
    nw->qi_out = &w->iface;
    EQHR(IRpcStubBuffer_Connect(c.sb, &nw->iface), E_NOINTERFACE);
    EQ(c.cs->pvServerObject, &w->iface);
    c.cs->pvServerObject = NULL;                           /* don't let the stub release it */
    psd_mock_free(w); psd_mock_free(nw);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_connect_other_hresult) {
    PSD_STA(); CTX(c, &psd_IID_One);
    PsdMock *nw = psd_mock_new();
    nw->qi_hr = 0x8abc0001; nw->qi_out = NULL;
    EQHR(IRpcStubBuffer_Connect(c.sb, &nw->iface), 0x8abc0001);
    nw->qi_hr = S_FALSE; nw->qi_self = 0; nw->qi_out = &nw->iface;
    HRESULT hr = IRpcStubBuffer_Connect(c.sb, &nw->iface);   /* a success code that isn't S_OK passes through too */
    EQHR(hr, S_FALSE);
    EQ(c.cs->pvServerObject, &nw->iface);
    c.cs->pvServerObject = NULL;
    psd_mock_free(nw);
    ctx_down(&c); PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psds_disconnect) {
    PSD_STA(); CTX(c, &psd_IID_One);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 1);
    IRpcStubBuffer_Disconnect(c.sb);
    EQ(c.srv->n_release, 1);
    CHECK(c.cs->pvServerObject == NULL);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 0);
    IRpcStubBuffer_Disconnect(c.sb);                      /* idempotent: nothing more to release */
    EQ(c.srv->n_release, 1);
    EQ(c.srv->ref, 1);
    /* reconnect afterwards */
    EQHR(IRpcStubBuffer_Connect(c.sb, &c.srv->iface), S_OK);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 1);
    EQ(c.srv->ref, 2);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_countrefs) {
    PSD_STA(); CTX(c, &psd_IID_Two);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 1);                /* number of connected server objects, not the COM refcount */
    IRpcStubBuffer_AddRef(c.sb);
    IRpcStubBuffer_AddRef(c.sb);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 1);
    IRpcStubBuffer_Release(c.sb); IRpcStubBuffer_Release(c.sb);
    IRpcStubBuffer_Disconnect(c.sb);
    EQ(IRpcStubBuffer_CountRefs(c.sb), 0);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_is_iid_supported) {
    PSD_STA(); CTX(c, &psd_IID_One);
    EQ(c.cs->RefCount, 1);
    IRpcStubBuffer *r = IRpcStubBuffer_IsIIDSupported(c.sb, &psd_IID_One);
    EQ(r, c.sb);                                           /* the stub itself is returned ... */
    EQ(c.cs->RefCount, 2);                                 /* ... AddRef'd for the caller */
    if (c.cs->RefCount > 1) IRpcStubBuffer_Release(r);
    const GUID *no[5] = { &psd_IID_Two, &psd_IID_IUnknown, &psd_IID_IRpcStubBuffer, &psd_IID_IDispatch, &psd_IID_Unknown1 };
    for (int i = 0; i < 5; i++) {
        r = IRpcStubBuffer_IsIIDSupported(c.sb, no[i]);
        CHECKF(r == NULL, "iid %d returned %p", i, r);
    }
    EQ(c.cs->RefCount, 1);                                 /* no reference taken on a miss */
    IRpcStubBuffer_Disconnect(c.sb);
    r = IRpcStubBuffer_IsIIDSupported(c.sb, &psd_IID_One);
    CHECK(r == NULL);                                      /* a disconnected stub supports nothing */
    EQ(c.cs->RefCount, 1);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_is_iid_supported_two) {
    PSD_STA(); CTX(c, &psd_IID_Two);
    LONG rc0 = c.cs->RefCount;
    IRpcStubBuffer *r = IRpcStubBuffer_IsIIDSupported(c.sb, &psd_IID_Two);
    EQ(r, c.sb);
    if (r && c.cs->RefCount > rc0) IRpcStubBuffer_Release(r);
    CHECK(IRpcStubBuffer_IsIIDSupported(c.sb, &psd_IID_One) == NULL);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_queryinterface) {
    PSD_STA(); CTX(c, &psd_IID_One);
    const GUID *ok[2] = { &psd_IID_IUnknown, &psd_IID_IRpcStubBuffer };
    for (int i = 0; i < 2; i++) {
        void *pv = NULL;
        EQHR(IRpcStubBuffer_QueryInterface(c.sb, ok[i], &pv), S_OK);
        EQ(pv, c.sb);
        EQ(c.cs->RefCount, 2);
        IRpcStubBuffer_Release(c.sb);
    }
    const GUID *bad[5] = { &psd_IID_One, &psd_IID_IDispatch, &psd_IID_IRpcProxyBuffer, &psd_IID_IMarshal, &psd_IID_Unknown1 };
    for (int i = 0; i < 5; i++) {
        void *pv = (void *)1;
        HRESULT hr = IRpcStubBuffer_QueryInterface(c.sb, bad[i], &pv);
        CHECKF(hr == E_NOINTERFACE, "bad %d hr=0x%lx", i, hr);
        CHECK(pv == NULL);
    }
    EQ(c.cs->RefCount, 1);
    void *pv = (void *)1;
    HRESULT hr = IRpcStubBuffer_QueryInterface(c.sb, &psd_IID_ICallFactory, &pv);
    CHECKF(hr == E_NOINTERFACE, "ICallFactory on a synchronous stub hr=0x%lx", hr);   /* only async stubs expose it */
    CHECK(pv == NULL);
    pv = (void *)1;
    hr = IRpcStubBuffer_QueryInterface(c.sb, &psd_IID_IReleaseMarshalBuffers, &pv);
    t_note("QI(IReleaseMarshalBuffers) on hand-built stub = 0x%08lx", hr);
    if (hr == S_OK) IRpcStubBuffer_Release(c.sb);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_addref_release_counts) {
    PSD_STA(); CTX(c, &psd_IID_One);
    EQ(IRpcStubBuffer_AddRef(c.sb), 2);
    EQ(IRpcStubBuffer_AddRef(c.sb), 3);
    EQ(IRpcStubBuffer_Release(c.sb), 2);
    EQ(IRpcStubBuffer_Release(c.sb), 1);
    EQ(c.cs->RefCount, 1);
    ctx_down(&c); PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psds_final_release_drops_factory_ref) {
    PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    if (!psd_init() || psd_get_factory(&psf) != S_OK) { t_skip("factory"); PSD_DONE(); return; }
    LONG before = psd_psf.RefCount;
    PsdMock *srv = psd_mock_new();
    IRpcStubBuffer *sb = NULL;
    EQHR(IPSFactoryBuffer_CreateStub(psf, &psd_IID_One, &srv->iface, &sb), S_OK);
    EQ(psd_psf.RefCount, before + 1);
    EQ(IRpcStubBuffer_Release(sb), 0);
    EQ(psd_psf.RefCount, before);
    /* NdrCStdStubBuffer_Release does not disconnect the server object (the stub owner must call Disconnect first) */
    EQ(srv->n_release, 0);
    EQ(srv->ref, 2);
    psd_mock_free(srv);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psds_ndr_release_uses_second_argument) {
    PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    if (!psd_init() || psd_get_factory(&psf) != S_OK) { t_skip("factory"); PSD_DONE(); return; }
    LONG before = psd_psf.RefCount;
    PsdMock *srv = psd_mock_new();
    IRpcStubBuffer *sb = NULL;
    EQHR(IPSFactoryBuffer_CreateStub(psf, &psd_IID_One, &srv->iface, &sb), S_OK);
    EQ(psd_psf.RefCount, before + 1);
    PsdMock *fake = psd_mock_new();       /* stands in for the IPSFactoryBuffer: only its Release slot is used */
    fake->ref = 5;
    ULONG r = UIN("rpcrt4.dll", NdrCStdStubBuffer_Release)(sb, (IPSFactoryBuffer *)&fake->iface);
    EQ(r, 0);
    EQ(fake->n_release, 1);                /* Release is called on the 2nd argument, not on pPSFactory */
    EQ(psd_psf.RefCount, before + 1);      /* the real factory still holds the stub's reference */
    IPSFactoryBuffer_Release(psf);         /* balance that leaked reference */
    psd_mock_free(fake);
    psd_mock_free(srv);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psds_ndr_release_not_final) {
    PSD_STA(); CTX(c, &psd_IID_One);
    PsdMock *fake = psd_mock_new();
    IRpcStubBuffer_AddRef(c.sb);
    ULONG r = UIN("rpcrt4.dll", NdrCStdStubBuffer_Release)(c.sb, (IPSFactoryBuffer *)&fake->iface);
    EQ(r, 1);
    EQ(fake->n_release, 0);                /* the factory is only released with the last stub reference */
    EQ(c.cs->RefCount, 1);
    psd_mock_free(fake);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_debug_server_query_interface) {
    PSD_STA(); CTX(c, &psd_IID_One);
    void *pv = NULL;
    EQHR(IRpcStubBuffer_DebugServerQueryInterface(c.sb, &pv), S_OK);
    EQ(pv, &c.srv->iface);
    EQ(c.srv->ref, 2);                                     /* no reference is taken */
    IRpcStubBuffer_DebugServerRelease(c.sb, pv);           /* ... and none dropped: a no-op */
    EQ(c.srv->ref, 2);
    EQ(c.srv->n_release, 0);
    IRpcStubBuffer_Disconnect(c.sb);
    pv = (void *)1;
    EQHR(IRpcStubBuffer_DebugServerQueryInterface(c.sb, &pv), CO_E_OBJNOTCONNECTED);
    CHECK(pv == NULL);
    ctx_down(&c); PSD_DONE();
}

static void do_invoke(void *a) {
    void **v = a;
    ((IRpcStubBuffer *)v[0])->lpVtbl->Invoke((IRpcStubBuffer *)v[0], (RPCOLEMESSAGE *)v[1], (IRpcChannelBuffer *)v[2]);
}

TEST(psds_invoke_bad_method_raises) {
    if (t_is_wine()) { t_skip("wine dispatches out-of-range methods instead of raising"); return; }
    PSD_STA(); CTX(c, &psd_IID_One);
    PsdChan *ch = psd_chan_new(c.sb);
    static const struct { ULONG m; } t[] = { {0}, {1}, {2}, {4}, {5}, {0x7fff}, {0x8003}, {0xffffffffu} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
        msg.dataRepresentation = 0x10;
        msg.iMethod = t[i].m;
        void *args[3] = { c.sb, &msg, &ch->iface };
        DWORD code = 0x6d1;
        int caught = psd_catch(do_invoke, args, &code);
        CHECKF(caught, "iMethod %lu: expected RPC_S_PROCNUM_OUT_OF_RANGE exception", t[i].m);
        CHECKF(code == 0x6d1, "iMethod %lu: code 0x%lx", t[i].m, code);
    }
    EQ(c.srv->n_release, 0);
    EQ(ch->n_getbuf, 0);
    HeapFree(GetProcessHeap(), 0, ch);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_invoke_bad_method_two) {
    if (t_is_wine()) { t_skip("wine dispatches out-of-range methods instead of raising"); return; }
    PSD_STA(); CTX(c, &psd_IID_Two);
    PsdChan *ch = psd_chan_new(c.sb);
    static const ULONG t[] = { 0, 2, 7, 8, 100 };       /* valid methods are 3..6 */
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
        msg.dataRepresentation = 0x10; msg.iMethod = t[i];
        void *args[3] = { c.sb, &msg, &ch->iface };
        DWORD code = 0x6d1;
        int caught = psd_catch(do_invoke, args, &code);
        CHECKF(caught && code == 0x6d1, "iMethod %lu caught=%d code=0x%lx", t[i], caught, code);
    }
    HeapFree(GetProcessHeap(), 0, ch);
    ctx_down(&c); PSD_DONE();
}

TEST(psds_invoke_dispatches_to_server) {
    PSD_STA();
    PsdRig r;
    if (!psd_rig_up(&r, &psd_IID_One, 0)) { t_skip("rig"); PSD_DONE(); return; }
    /* hand-built request: Add(20, 22) */
    RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
    LONG req[2] = { 20, 22 };
    msg.dataRepresentation = 0x10;                        /* NDR_LOCAL_DATA_REPRESENTATION */
    msg.iMethod = 3;
    msg.cbBuffer = sizeof req;
    msg.Buffer = HeapAlloc(GetProcessHeap(), 0, 64);
    memcpy(msg.Buffer, req, sizeof req);
    void *old = msg.Buffer;
    r.chan->depth = 1;                                     /* mark GetBuffer calls as server-side */
    HRESULT hr = IRpcStubBuffer_Invoke(r.sb, &msg, &r.chan->iface);
    r.chan->depth = 0;
    EQHR(hr, S_OK);
    EQ(r.srv->n_add, 1);
    EQ(r.srv->last_a, 20); EQ(r.srv->last_b, 22);
    EQ(r.chan->n_getbuf, 1);                               /* the stub allocated the reply through the channel */
    CHECK(r.chan->ngb > 0 && r.chan->gb[0].server);
    CHECK(IsEqualGUID(&r.chan->gb[0].iid, &psd_IID_One));  /* ... for the stub's interface */
    CHECK(msg.Buffer != NULL);
    if (msg.Buffer) {
        LONG *rep = msg.Buffer;
        EQ(msg.cbBuffer, 8);                               /* reply: [out] long, then the HRESULT */
        EQ(rep[0], 42);
        EQ(rep[1], S_OK);
        HeapFree(GetProcessHeap(), 0, msg.Buffer);
        r.chan->live_buffers--;
    }
    (void)old;
    psd_rig_down(&r);
    PSD_DONE();
}
