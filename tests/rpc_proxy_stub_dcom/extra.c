/* Additional NDR proxy/stub behaviour: procedure-number plumbing, size overflow, destination contexts, stub reuse,
 * parallel use from several threads. */
#include "psd.h"

#define RIGC(r, iid) PsdRig r; if (!psd_rig_up(&r, iid, 1)) { t_skip("cannot build rig"); psd_rig_down(&r); PSD_DONE(); return; }
#define ONE(r) ((IOletOne *)(r).iface)

TEST(psdx_proxy_initialize_procnum_table) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const unsigned t[] = { 0, 1, 3, 4, 31, 32, 255, 0x7fff };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
        UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, t[i]);
        CHECKF(msg.ProcNum == t[i], "ProcNum %u -> %lu", t[i], (ULONG)msg.ProcNum);
        UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    }
    EQ(r.chan->ref, 2);
    psd_rig_down(&r); PSD_DONE();
}

static void do_proxy_getbuf2(void *a) { void **v = a; UIN("rpcrt4.dll", NdrProxyGetBuffer)(v[0], v[1]); }
TEST(psdx_proxy_get_buffer_length_overflow) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const ULONG t[] = { 0xfffffffd, 0xfffffffe, 0xffffffff };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
        UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
        sm.BufferLength = t[i];
        void *args[2] = { r.iface, &sm };
        DWORD code = 0x6c6;
        int caught = psd_catch(do_proxy_getbuf2, args, &code);
        CHECKF(caught && code == 0x6c6, "BufferLength 0x%lx: caught=%d code=0x%lx", t[i], caught, code);   /* rounding to a multiple of 4 would wrap */
        UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    }
    EQ(r.chan->n_getbuf, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdx_proxy_destctx_values) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const DWORD t[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC, 0x1234 };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
        r.chan->destctx = t[i];
        UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);
        CHECKF(sm.dwDestContext == t[i], "destctx 0x%lx -> 0x%lx", t[i], (DWORD)sm.dwDestContext);
        UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    }
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdx_proxy_initialize_destctx_failure_ignored) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
    memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
    r.chan->hr_destctx = E_FAIL;
    UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, 3);   /* the HRESULT of GetDestCtx is not examined */
    EQ(r.chan->n_destctx, 1);
    CHECK(sm.pRpcChannelBuffer != NULL);
    t_note("dwDestContext after a failing GetDestCtx: 0x%lx", (DWORD)sm.dwDestContext);
    UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    r.chan->hr_destctx = S_OK;
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdx_stub_reuse_after_disconnect_connect) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    LONG out;
    for (int i = 0; i < 25; i++) {
        IRpcStubBuffer_Disconnect(r.sb);
        if (i == 0) EQ(IRpcStubBuffer_CountRefs(r.sb), 0);
        EQHR(IRpcStubBuffer_Connect(r.sb, PSD_SRV_UNK(r.srv)), S_OK);
        out = 0;
        if (ONE(r)->lpVtbl->Add(ONE(r), i, i, &out) != S_OK || out != 2 * i) { CHECKF(0, "iteration %d: out=%ld", i, out); break; }
    }
    EQ(r.srv->ref, 2);                                       /* one reference held by the stub */
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdx_stub_connect_same_server_twice) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    EQ(r.srv->ref, 2);
    EQHR(IRpcStubBuffer_Connect(r.sb, PSD_SRV_UNK(r.srv)), S_OK);   /* QI adds one, the previous one is released */
    EQ(r.srv->ref, 2);
    EQHR(IRpcStubBuffer_Connect(r.sb, PSD_SRV_UNK(r.srv)), S_OK);
    EQ(r.srv->ref, 2);
    EQ(IRpcStubBuffer_CountRefs(r.sb), 1);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdx_stub_debug_server_release_is_noop) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    IRpcStubBuffer_DebugServerRelease(r.sb, NULL);
    IRpcStubBuffer_DebugServerRelease(r.sb, PSD_SRV_UNK(r.srv));
    IRpcStubBuffer_DebugServerRelease(r.sb, (void *)1);
    EQ(r.srv->ref, 2);
    EQ(IRpcStubBuffer_CountRefs(r.sb), 1);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdx_stub_two_interfaces_independent) {
    PSD_STA();
    PsdRig a, b;
    if (!psd_rig_up(&a, &psd_IID_One, 1) || !psd_rig_up(&b, &psd_IID_Two, 1)) { t_skip("rig"); psd_rig_down(&a); psd_rig_down(&b); PSD_DONE(); return; }
    LONG o1 = 0, o2 = 0;
    EQHR(((IOletOne *)a.iface)->lpVtbl->Add((IOletOne *)a.iface, 10, 20, &o1), S_OK);
    EQHR(((IOletTwo *)b.iface)->lpVtbl->Echo((IOletTwo *)b.iface, 99, &o2), S_OK);
    EQ(o1, 30); EQ(o2, 99);
    EQ(a.srv->n_add, 1); EQ(a.srv->n_echo, 0);
    EQ(b.srv->n_echo, 1); EQ(b.srv->n_add, 0);
    CHECK(IRpcStubBuffer_IsIIDSupported(a.sb, &psd_IID_Two) == NULL);
    CHECK(IRpcStubBuffer_IsIIDSupported(b.sb, &psd_IID_One) == NULL);
    psd_rig_down(&a); psd_rig_down(&b);
    EQ(psd_psf.RefCount, 0);
    PSD_DONE();
}

typedef struct Par { int id; LONG bad; HANDLE ready; int did_run; } Par;
static DWORD WINAPI par_thread(LPVOID p) {
    Par *par = p;
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) { par->bad = 1000; return 0; }
    PsdRig r;
    if (psd_rig_up(&r, &psd_IID_One, 1)) {
        par->did_run = 1;
        for (int i = 0; i < 400; i++) {
            LONG out = 0;
            if (((IOletOne *)r.iface)->lpVtbl->Add((IOletOne *)r.iface, par->id * 1000, i, &out) != S_OK || out != par->id * 1000 + i) par->bad++;
        }
        if (r.srv->n_add != 400) par->bad++;
        psd_rig_down(&r);
    }
    U(CoUninitialize)();
    return 0;
}
TEST(psdx_parallel_rigs) {
    if (!psd_init()) { t_skip("init"); return; }
    enum { N = 4 };
    Par par[N]; HANDLE th[N];
    memset(par, 0, sizeof par);
    for (int i = 0; i < N; i++) { par[i].id = i + 1; th[i] = CreateThread(NULL, 0, par_thread, &par[i], 0, NULL); }
    DWORD w = WaitForMultipleObjects(N, th, TRUE, 20000);
    EQ(w, WAIT_OBJECT_0);
    for (int i = 0; i < N; i++) { CloseHandle(th[i]); CHECKF(par[i].did_run && par[i].bad == 0, "thread %d did_run=%d bad=%ld", i, par[i].did_run, par[i].bad); }
    EQ(psd_psf.RefCount, 0);                                 /* all stubs/proxies/factory references returned */
}

/* NdrExtpProxyInitialize (combase decomp) masks ProcNum with 0xffff7fff after NdrClientInitializeNew */
TEST(psdx_proxy_initialize_clears_valid_bit) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const unsigned t[] = { 0x8003, 0x8000, 0xffff, 0x8020 };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        RPC_MESSAGE msg; MIDL_STUB_MESSAGE sm;
        memset(&msg, 0, sizeof msg); memset(&sm, 0, sizeof sm);
        UIN("rpcrt4.dll", NdrProxyInitialize)(r.iface, &msg, &sm, &psd_stub_desc, t[i]);
        unsigned want = t[i] & 0x7fff;
        if (t_is_wine() && msg.ProcNum != want) t_note("wine keeps bit 0x8000 for 0x%x", t[i]);
        else CHECKF(msg.ProcNum == want, "ProcNum in 0x%x -> 0x%lx, want 0x%x", t[i], (ULONG)msg.ProcNum, want);
        UIN("rpcrt4.dll", NdrProxyFreeBuffer)(r.iface, &sm);
    }
    psd_rig_down(&r); PSD_DONE();
}
