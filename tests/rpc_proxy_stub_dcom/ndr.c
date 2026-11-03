/* Direct use of the NDR proxy/stub helper entry points that MIDL-generated code calls:
 * NdrProxyInitialize / NdrProxyGetBuffer / NdrProxySendReceive / NdrProxyFreeBuffer / NdrProxyErrorHandler,
 * NdrStubInitialize / NdrStubGetBuffer / NdrStubForwardingFunction, NdrOleAllocate / NdrOleFree.
 * Behaviour from the Win10 decomp of combase!NdrExtpProxy* / NdrExtStub* and rpcrt4's forwarding thunks. */
#include "psd.h"

#define RIGC(r, iid) PsdRig r; if (!psd_rig_up(&r, iid, 1)) { t_skip("cannot build proxy rig"); psd_rig_down(&r); PSD_DONE(); return; }

/* ---- NdrProxyErrorHandler: exception code -> HRESULT ------------------------------------ */
TEST(psdn_proxy_error_handler_table) {
    if (!UIN("rpcrt4.dll", NdrProxyErrorHandler)) { t_skip("no NdrProxyErrorHandler"); return; }
    static const struct { DWORD in; HRESULT out; } t[] = {
        { 0, 0 },
        { 1, (HRESULT)0x80070001 }, { 5, (HRESULT)0x80070005 }, { 0x6ba, (HRESULT)0x800706ba },   /* RPC_S_SERVER_UNAVAILABLE */
        { 0x6d1, (HRESULT)0x800706d1 }, { 0x6f4, (HRESULT)0x800706f4 }, { 0x725, (HRESULT)0x80070725 },
        { 0xffff, (HRESULT)0x8007ffff }, { 0x10005, (HRESULT)0x80070005 },        /* only the low 16 bits survive */
        { 0x7fffffff, (HRESULT)0x8007ffff },
        { 0x80004005, (HRESULT)0x80004005 }, { 0x80040111, (HRESULT)0x80040111 }, /* already an HRESULT: unchanged */
        { 0x800401fd, (HRESULT)0x800401fd }, { 0x80000000, (HRESULT)0x80000000 }, { 0xffffffff, (HRESULT)0xffffffff },
        { 0xC0000005, (HRESULT)0xC0000005 }, { 0xe06d7363, (HRESULT)0xe06d7363 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HRESULT hr = UIN("rpcrt4.dll", NdrProxyErrorHandler)(t[i].in);
        CHECKF(hr == t[i].out, "NdrProxyErrorHandler(0x%lx) = 0x%lx want 0x%lx", t[i].in, hr, t[i].out);
    }
}

/* ---- NdrProxyInitialize ------------------------------------------------------------------- */
TEST(psdn_proxy_initialize_connected) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    r.chan->destctx = MSHCTX_DIFFERENTMACHINE;
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    EQ(r.chan->ref, 2);
    UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
    EQ(sm.pRpcChannelBuffer, &r.chan->iface);               /* taken from the proxy buffer */
    EQ(r.chan->ref, 3);                                      /* the stub message owns its own reference */
    EQ(r.chan->n_destctx, 1);                                /* GetDestCtx is queried once */
    EQ(sm.dwDestContext, MSHCTX_DIFFERENTMACHINE);
    CHECK(sm.pvDestContext == NULL);
    EQ(sm.RpcMsg, &msg);
    EQ(sm.StubDesc, &psd_stub_desc);
    EQ(sm.IsClient, 1);
    EQ(msg.ProcNum, 3);                                      /* RPC_FLAGS_VALID_BIT is cleared again */
    EQ(r.chan->n_getbuf, 0);
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);     /* nothing valid to free; drops the channel reference */
    EQ(r.chan->ref, 2);
    CHECK(sm.pRpcChannelBuffer == NULL);
    EQ(r.chan->n_freebuf, 0);
    psd_rig_down(&r); PSD_DONE();
}

static void do_proxy_init(void *a) {
    void **v = a;
    UIN("rpcrt4.dll", NdrProxyInitialize)(v[0], v[1], v[2], v[3], 3);
}
TEST(psdn_proxy_initialize_not_connected) {
    PSD_STA();
    PsdRig r;
    if (!psd_rig_up(&r, &psd_IID_One, 0)) { t_skip("rig"); psd_rig_down(&r); PSD_DONE(); return; }
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    void *args[4] = { r.iface, &msg, &sm, &psd_stub_desc };
    DWORD code = 0x800401FD;
    int caught = psd_catch(do_proxy_init, args, &code);
    CHECK(caught);
    EQ(code, 0x800401FD);                                    /* CO_E_OBJNOTCONNECTED raised as an RPC exception */
    CHECK(sm.pRpcChannelBuffer == NULL);
    psd_rig_down(&r); PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdn_proxy_channel_reference_lifetime) {
    PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    if (!psd_init() || psd_get_factory(&psf) != S_OK) { t_skip("factory"); PSD_DONE(); return; }
    IRpcProxyBuffer *pb = NULL; IUnknown *i = NULL;
    EQHR(IPSFactoryBuffer_CreateProxy(psf, NULL, &psd_IID_One, &pb, (void **)&i), S_OK);
    PsdChan *c = psd_chan_new(NULL);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC desc;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm); memset(&desc, 0, sizeof desc);
    EQHR(IRpcProxyBuffer_Connect(pb, &c->iface), S_OK);
    EQ(c->ref, 2);                                           /* the proxy acquired its own reference */
    EQ(IRpcChannelBuffer_Release(&c->iface), 1);              /* so the initial one can go */
    UIN("rpcrt4.dll", NdrProxyInitialize)(i, &msg, &sm, &desc, 0);
    EQ(c->ref, 2);                                           /* the stub message took another */
    CHECK(sm.pRpcChannelBuffer != NULL);
    /* The stub message holds no references on the proxy: it may be torn down underneath an in-flight call. */
    IRpcProxyBuffer_Disconnect(pb);
    EQ(c->ref, 1);
    IRpcProxyBuffer_Release(pb);
    EQ(IUnknown_Release(i), 0);
    EQ(c->ref, 1);
    /* NdrProxyFreeBuffer must not touch the (dead) proxy pointer, but still drops the channel reference */
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(i, &sm);
    EQ(c->ref, 0);
    CHECK(sm.pRpcChannelBuffer == NULL);
    HeapFree(GetProcessHeap(), 0, c);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

/* ---- NdrProxyGetBuffer / SendReceive / FreeBuffer ---------------------------------------- */
TEST(psdn_proxy_get_buffer) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
    sm.BufferLength = 6;
    UIN("rpcrt4.dll", NdrProxyGetBuffer)(r.iface, &sm);
    EQ(r.chan->n_getbuf, 1);
    if (r.chan->ngb) {
        EQ(r.chan->gb[0].cb, 8);                             /* BufferLength is rounded up to a multiple of 4 */
        EQ(r.chan->gb[0].iMethod, 3);
        EQ(r.chan->gb[0].rep, 0x10);                         /* NDR_LOCAL_DATA_REPRESENTATION */
        CHECK(IsEqualGUID(&r.chan->gb[0].iid, &psd_IID_One)); /* the proxy's interface id */
        CHECK(!r.chan->gb[0].server);
    }
    EQ(sm.BufferLength, 8);
    EQ(msg.BufferLength, 8);
    CHECK(msg.Buffer != NULL);
    EQ(sm.Buffer, msg.Buffer);
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    EQ(r.chan->n_freebuf, 1);                                /* a valid buffer is returned to the channel */
    EQ(r.chan->live_buffers, 0);
    CHECK(sm.pRpcChannelBuffer == NULL);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdn_proxy_get_buffer_rounding) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const struct { ULONG in, out; } t[] = { {0, 0}, {1, 4}, {3, 4}, {4, 4}, {5, 8}, {7, 8}, {8, 8}, {9, 12}, {100, 100}, {101, 104} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
        r.chan->ngb = 0;
        UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
        sm.BufferLength = t[i].in;
        UIN("rpcrt4.dll", NdrProxyGetBuffer)(r.iface, &sm);
        CHECKF(r.chan->ngb == 1 && r.chan->gb[0].cb == t[i].out, "BufferLength %lu -> channel saw %lu want %lu", t[i].in, r.chan->ngb ? r.chan->gb[0].cb : 9999, t[i].out);
        CHECKF(sm.BufferLength == t[i].out, "sm.BufferLength %lu want %lu", (ULONG)sm.BufferLength, t[i].out);
        UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    }
    EQ(r.chan->live_buffers, 0);
    EQ(r.chan->ref, 2);
    psd_rig_down(&r); PSD_DONE();
}

static void do_proxy_getbuf(void *a) { void **v = a; UIN("rpcrt4.dll", NdrProxyGetBuffer)(v[0], v[1]); }
TEST(psdn_proxy_get_buffer_failure_raises) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const HRESULT t[] = { (HRESULT)0x8abc0004, E_OUTOFMEMORY, E_FAIL, (HRESULT)0x80010108 };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
        UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
        sm.BufferLength = 8;
        r.chan->hr_getbuf = t[i];
        void *args[2] = { r.iface, &sm };
        DWORD code = (DWORD)t[i];
        int caught = psd_catch(do_proxy_getbuf, args, &code);
        CHECKF(caught && code == (DWORD)t[i], "hr 0x%lx caught=%d code=0x%lx", (DWORD)t[i], caught, code);
        r.chan->hr_getbuf = S_OK;
        UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    }
    EQ(r.chan->ref, 2);
    EQ(r.chan->n_sendrecv, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdn_proxy_send_receive) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
    sm.BufferLength = 8;
    UIN("rpcrt4.dll", NdrProxyGetBuffer)(r.iface, &sm);
    LONG *b = (LONG *)msg.Buffer;
    b[0] = 30; b[1] = 12;
    sm.BufferStart = sm.Buffer;
    sm.Buffer = sm.Buffer + 8;                               /* "marshaled" two longs */
    UIN("rpcrt4.dll", NdrProxySendReceive)(r.iface, &sm);
    EQ(r.chan->n_sendrecv, 1);
    EQ(r.chan->req_len, 8);                                  /* length passed to the channel = Buffer - RpcMsg->Buffer */
    EQ(*(LONG *)r.chan->req, 30); EQ(*(LONG *)(r.chan->req + 4), 12);
    EQ(r.srv->n_add, 1);
    EQ(r.chan->rep_len, 8);
    EQ(sm.Buffer, msg.Buffer);                               /* now positioned at the start of the reply */
    EQ(sm.BufferStart, msg.Buffer);
    EQ(sm.BufferEnd, msg.Buffer + 8);
    EQ(msg.BufferLength, 8);
    LONG *rep = (LONG *)msg.Buffer;
    EQ(rep[0], 42); EQ(rep[1], S_OK);
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    EQ(r.chan->n_freebuf, 1);
    EQ(r.chan->live_buffers, 0);
    psd_rig_down(&r); PSD_DONE();
}

static void do_proxy_sr(void *a) { void **v = a; UIN("rpcrt4.dll", NdrProxySendReceive)(v[0], v[1]); }
TEST(psdn_proxy_send_receive_errors) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
    sm.BufferLength = 8;
    UIN("rpcrt4.dll", NdrProxyGetBuffer)(r.iface, &sm);
    sm.BufferStart = sm.Buffer;
    void *args[2] = { r.iface, &sm };
    DWORD code;
    /* marshaling past the end of the allocated buffer is detected before the channel is involved */
    sm.Buffer = msg.Buffer + 16;
    code = 0x6e6;
    CHECK(psd_catch(do_proxy_sr, args, &code));
    EQ(code, 0x6e6);
    EQ(r.chan->n_sendrecv, 0);
    /* a failing SendReceive raises its HRESULT ... */
    sm.Buffer = msg.Buffer + 8;
    r.chan->hr_sendrecv = (HRESULT)0x8abc0005;
    code = 0x8abc0005;
    CHECK(psd_catch(do_proxy_sr, args, &code));
    EQ(code, 0x8abc0005);
    EQ(r.chan->n_sendrecv, 1);
    /* ... except RPC_E_FAULT, where the channel's pStatus is what gets raised */
    r.chan->hr_sendrecv = (HRESULT)0x80010104;
    r.chan->status = 0x6ba;
    code = 0x6ba;
    sm.Buffer = msg.Buffer + 8;
    CHECK(psd_catch(do_proxy_sr, args, &code));
    EQ(code, 0x6ba);
    r.chan->hr_sendrecv = S_OK;
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdn_proxy_free_buffer_no_channel) {
    if (t_is_wine()) { t_skip("wine dereferences the NULL channel"); return; }
    PSD_STA(); RIGC(r, &psd_IID_One);
    MIDL_STUB_MESSAGE sm;
    memset(&sm, 0, sizeof sm);
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);      /* no channel attached: no-op */
    EQ(r.chan->n_freebuf, 0);
    EQ(r.chan->ref, 2);
    psd_rig_down(&r); PSD_DONE();
}

/* ---- the same failures seen through a real proxy call -------------------------------------- */
TEST(psdn_proxy_call_maps_errors) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    IOletOne *p = r.iface;
    LONG out;
    static const struct { HRESULT hr_getbuf; HRESULT hr_sr; ULONG status; HRESULT want; } t[] = {
        { (HRESULT)0x8abc0004, S_OK, 0, (HRESULT)0x8abc0004 },
        { E_OUTOFMEMORY, S_OK, 0, E_OUTOFMEMORY },
        { S_OK, (HRESULT)0x8abc0005, 0, (HRESULT)0x8abc0005 },
        { S_OK, (HRESULT)0x80010108, 0, (HRESULT)0x80010108 },            /* RPC_E_DISCONNECTED */
        { S_OK, (HRESULT)0x80010104, 5, (HRESULT)0x80070005 },             /* RPC_E_FAULT + status 5 -> ACCESS_DENIED */
        { S_OK, (HRESULT)0x80010104, 0x6ba, (HRESULT)0x800706ba },
        { S_OK, (HRESULT)0x80010104, 0x80040155, (HRESULT)0x80040155 },    /* an HRESULT status is passed on unchanged */
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        r.chan->hr_getbuf = t[i].hr_getbuf; r.chan->hr_sendrecv = t[i].hr_sr; r.chan->status = t[i].status;
        out = 1234;
        HRESULT hr = p->lpVtbl->Add(p, 1, 2, &out);
        CHECKF(hr == t[i].want, "row %u: Add -> 0x%lx want 0x%lx", i, hr, t[i].want);
    }
    r.chan->hr_getbuf = r.chan->hr_sendrecv = S_OK; r.chan->status = 0;
    EQ(r.srv->n_add, 0);                                     /* the server never ran */
    EQHR(p->lpVtbl->Add(p, 1, 2, &out), S_OK);
    EQ(out, 3);
    EQ(r.chan->live_buffers, 0);                             /* failed calls leaked no channel buffer */
    psd_rig_down(&r); PSD_DONE();
}

/* ---- NdrStubInitialize / NdrStubGetBuffer ----------------------------------------------------- */
TEST(psdn_stub_initialize) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    r.chan->destctx = MSHCTX_NOSHAREDMEM;
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    BYTE buf[32] = {0};
    msg.Buffer = buf; msg.BufferLength = 24; msg.DataRepresentation = 0x10; msg.ProcNum = 3;
    LONG ref0 = r.chan->ref;
    UIN("rpcrt4.dll", NdrStubInitialize)(&msg, &sm, &psd_stub_desc, &r.chan->iface);
    EQ(sm.RpcMsg, &msg);
    EQ(sm.pRpcChannelBuffer, &r.chan->iface);
    EQ(r.chan->ref, ref0);                                   /* the channel is borrowed, not AddRef'd */
    EQ(r.chan->n_destctx, 1);
    EQ(sm.dwDestContext, MSHCTX_NOSHAREDMEM);
    EQ(sm.StubDesc, &psd_stub_desc);
    EQ(sm.IsClient, 0);
    EQ(sm.Buffer, buf); EQ(sm.BufferStart, buf); EQ(sm.BufferEnd, buf + 24);
    EQ(sm.pfnAllocate, psd_stub_desc.pfnAllocate);
    EQ(sm.pfnFree, psd_stub_desc.pfnFree);
    psd_rig_down(&r); PSD_DONE();
}

static void do_stub_init(void *a) { void **v = a; UIN("rpcrt4.dll", NdrStubInitialize)(v[0], v[1], v[2], v[3]); }
TEST(psdn_stub_initialize_new_version_rejected) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    MIDL_STUB_DESC desc = psd_stub_desc;
    BYTE buf[8] = {0};
    msg.Buffer = buf;
    static const ULONG vers[] = { 0xa0001, 0xa0002, 0x100000, 0x7fffffff };
    for (unsigned i = 0; i < sizeof vers / sizeof *vers; i++) {
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm); msg.Buffer = buf; msg.BufferLength = 8;
        desc.Version = vers[i];
        void *args[4] = { &msg, &sm, &desc, &r.chan->iface };
        DWORD code = 0x725;
        int caught = psd_catch(do_stub_init, args, &code);
        CHECKF(caught && code == 0x725, "Version 0x%lx caught=%d code=0x%lx", vers[i], caught, code);   /* RPC_X_WRONG_STUB_VERSION */
    }
    static const ULONG ok[] = { 0x20000, 0x50002, 0x60001, 0x9ffff };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm); msg.Buffer = buf; msg.BufferLength = 8;
        desc.Version = ok[i];
        void *args[4] = { &msg, &sm, &desc, &r.chan->iface };
        DWORD code = 0x725;
        int caught = psd_catch(do_stub_init, args, &code);
        CHECKF(!caught, "Version 0x%lx raised 0x%lx", ok[i], code);
    }
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdn_stub_get_buffer) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    BYTE buf[16] = {0};
    msg.Buffer = buf; msg.BufferLength = 8; msg.DataRepresentation = 0x10; msg.ProcNum = 3;
    UIN("rpcrt4.dll", NdrStubInitialize)(&msg, &sm, &psd_stub_desc, &r.chan->iface);
    sm.BufferLength = 21;                                    /* the stub does not round the size */
    UIN("rpcrt4.dll", NdrStubGetBuffer)(r.sb, &r.chan->iface, &sm);
    EQ(r.chan->n_getbuf, 1);
    if (r.chan->ngb) {
        EQ(r.chan->gb[0].cb, 21);
        EQ(r.chan->gb[0].rep, 0x10);
        CHECK(IsEqualGUID(&r.chan->gb[0].iid, &psd_IID_One)); /* the stub's interface id (from its header) */
    }
    CHECK(msg.Buffer != (void *)buf && msg.Buffer != NULL);
    EQ(sm.Buffer, msg.Buffer);
    EQ(msg.BufferLength, 21);
    if (msg.Buffer != (void *)buf && msg.Buffer) { HeapFree(GetProcessHeap(), 0, msg.Buffer); r.chan->live_buffers--; }
    psd_rig_down(&r); PSD_DONE();
}

static void do_stub_gb(void *a) { void **v = a; UIN("rpcrt4.dll", NdrStubGetBuffer)(v[0], v[1], v[2]); }
TEST(psdn_stub_get_buffer_failure_raises) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    BYTE buf[16] = {0};
    msg.Buffer = buf; msg.BufferLength = 8;
    UIN("rpcrt4.dll", NdrStubInitialize)(&msg, &sm, &psd_stub_desc, &r.chan->iface);
    sm.BufferLength = 8;
    r.chan->hr_getbuf = (HRESULT)0x8abc0006;
    void *args[3] = { r.sb, &r.chan->iface, &sm };
    DWORD code = 0x8abc0006;
    CHECK(psd_catch(do_stub_gb, args, &code));
    EQ(code, 0x8abc0006);
    r.chan->hr_getbuf = S_OK;
    psd_rig_down(&r); PSD_DONE();
}

/* ---- NdrStubForwardingFunction (delegating stubs) --------------------------------------------- */
static int base_invoke_called;
static void *base_msg_seen, *base_chan_seen;
static HRESULT WINAPI base_invoke(IRpcStubBuffer *This, RPCOLEMESSAGE *msg, IRpcChannelBuffer *chan) {
    base_invoke_called++; base_msg_seen = msg; base_chan_seen = chan;
    return S_OK;
}
/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdn_stub_forwarding_function) {
    if (!UIN("rpcrt4.dll", NdrStubForwardingFunction)) { t_skip("no NdrStubForwardingFunction"); return; }
    IRpcStubBufferVtbl base_vtbl;
    memset(&base_vtbl, 0, sizeof base_vtbl);
    base_vtbl.Invoke = base_invoke;
    const IRpcStubBufferVtbl *base_vtbl_ptr = &base_vtbl;
    IRpcStubBuffer *base_stub = (IRpcStubBuffer *)&base_vtbl_ptr;
    void *This[5];
    memset(This, 0xcc, sizeof This);
    This[0] = base_stub;                                     /* the delegating stub keeps its base stub in front of itself */
    IRpcStubBuffer *real_this = (IRpcStubBuffer *)&This[1];
    IRpcChannelBuffer *chan = (IRpcChannelBuffer *)0xdeadbeef;
    RPC_MESSAGE *msg = (RPC_MESSAGE *)0xcafebabe;
    DWORD *phase = (DWORD *)0x12345678;
    base_invoke_called = 0;
    UIN("rpcrt4.dll", NdrStubForwardingFunction)(real_this, chan, msg, phase);
    EQ(base_invoke_called, 1);
    EQ(base_msg_seen, msg);                                  /* RPC_MESSAGE is passed through unchanged as the RPCOLEMESSAGE */
    EQ(base_chan_seen, chan);
}

/* ---- NdrOleAllocate / NdrOleFree --------------------------------------------------------------- */
TEST(psdn_ole_allocate_free) {
    if (!UIN("rpcrt4.dll", NdrOleAllocate) || !UIN("rpcrt4.dll", NdrOleFree)) { t_skip("no NdrOleAllocate"); return; }
    static const size_t sizes[] = { 1, 8, 16, 100, 4096, 65536, 1u << 20 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        BYTE *p = UIN("rpcrt4.dll", NdrOleAllocate)(sizes[i]);
        CHECKF(p != NULL, "NdrOleAllocate(%zu) = NULL", sizes[i]);
        if (!p) continue;
        memset(p, 0xa5, sizes[i]);
        CHECK(p[sizes[i] - 1] == 0xa5);
        UIN("rpcrt4.dll", NdrOleFree)(p);
    }
    /* the memory comes from the COM task allocator */
    PSD_STA();
    IMalloc *m = NULL;
    if (U(CoGetMalloc)(MEMCTX_TASK, &m) == S_OK && m) {
        void *p = UIN("rpcrt4.dll", NdrOleAllocate)(64);
        if (p) {
            SIZE_T sz = IMalloc_GetSize(m, p);
            t_note("IMalloc::GetSize on NdrOleAllocate(64) = %lld", (long long)sz);
            EQ(IMalloc_DidAlloc(m, p), 1);
            UIN("rpcrt4.dll", NdrOleFree)(p);
        }
        IMalloc_Release(m);
    }
    PSD_DONE();
}
