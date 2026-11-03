/* Cross-apartment (in-process) standard marshaling: STA <-> MTA <-> STA through proxies.
 * Object: XpObj (IClassFactory). Expectations were taken from the Windows 10 combase decompilation
 * where an API is directly visible, and from documented COM behaviour otherwise. */
#include "xp.h"

#define STA() COINIT_APARTMENTTHREADED
#define MTA() COINIT_MULTITHREADED
#define INIT(m) do { HRESULT h_ = U(CoInitializeEx)(NULL, (m)); if (FAILED(h_)) { t_skip("CoInitializeEx failed %08lx", h_); return; } } while (0)
#define UNINIT() U(CoUninitialize)()
/* A call that reaches XpObj::CreateInstance with the "probe" IID: returns obj->probeHr, touches nothing else. */
static void f_probe_false(XpObj *o, IClassFactory *p);
static HRESULT probe(IClassFactory *p) { void *pv = NULL; return IClassFactory_CreateInstance(p, NULL, &XP_IID_IXpBogus2, &pv); }

/* ---- basic calls through a proxy ------------------------------------------------------ */
static void basic_calls(DWORD mainModel, DWORD hostModel, int expectHostThread)
{
    XpFx f; HRESULT hr;
    INIT(mainModel);
    if (!xp_fx_setup(&f, hostModel, MSHLFLAGS_NORMAL)) { CHECKF(0, "fixture failed unmarshal=%08lx", f.unmarshalHr); xp_fx_teardown(&f); UNINIT(); return; }
    CHECK(f.p != (IClassFactory *)XP_OBJ_CF(f.o));          /* cross-apartment: a proxy, not the object */
    hr = probe(f.p);   XEQ(hr, S_OK);
    f.o->probeHr = S_FALSE;
    hr = probe(f.p);   XEQ(hr, S_FALSE);                    /* success codes other than S_OK survive */
    XEQ(f.o->createCalls, 2);
    if (expectHostThread) XEQ(f.o->lastTid, f.h.tid);       /* STA objects run on their apartment thread */
    else CHECK(f.o->lastTid != f.mainTid || mainModel == COINIT_MULTITHREADED);
    f.o->probeHr = XP_CUSTOM_HR;
    hr = probe(f.p);   XEQ(hr, XP_CUSTOM_HR);
    f.o->probeHr = E_FAIL;
    hr = probe(f.p);   XEQ(hr, E_FAIL);
    XEQ(f.o->createCalls, 4);
    CHECK(IsEqualGUID(&f.o->lastCreateIid, &XP_IID_IXpBogus2));
    xp_fx_teardown(&f);
    UNINIT();
}
XT(apt_sta_to_sta_call) { XP_NEED_BASE(); basic_calls(STA(), STA(), 1); }
XT(apt_sta_to_mta_call) { XP_NEED_BASE(); basic_calls(STA(), MTA(), 0); }
XT(apt_mta_to_sta_call) { XP_NEED_BASE(); basic_calls(MTA(), STA(), 1); }

XT(apt_mta_to_mta_unmarshal_is_direct) {
    XpFx f; XP_NEED_BASE();
    INIT(MTA());
    if (xp_fx_setup(&f, MTA(), MSHLFLAGS_NORMAL)) {
        /* both threads live in the one MTA: unmarshaling returns the object itself */
        XEQ(f.p, XP_OBJ_CF(f.o));
        XEQ(probe(f.p), S_OK);
        XEQ(f.o->lastTid, f.mainTid);                 /* direct call on the calling thread */
    } else CHECKF(0, "fixture failed %08lx", f.unmarshalHr);
    xp_fx_teardown(&f);
    UNINIT();
}

XT(apt_sta_same_apartment_marshal_returns_same_pointer) {
    IStream *s; XpObj *o; IClassFactory *p = NULL; HRESULT hr;
    XP_NEED_BASE(); NEED(CreateStreamOnHGlobal);
    INIT(STA());
    s = xp_new_stream(); o = xp_obj_new();
    CHECK(s && o);
    if (s && o) {
        hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK);
        CHECKF(o->ref > 1, "stub manager must hold a reference after NORMAL marshal (ref=%ld)", o->ref);
        xp_rewind(s);
        hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, (void **)&p);
        XEQ(hr, S_OK);
        XEQ(p, XP_OBJ_CF(o));
        if (p) IClassFactory_Release(p);
        XEQ(o->ref, 1);                                 /* marshal reference released by unmarshal */
        /* the marshal data is consumed: a second unmarshal finds no connected object */
        p = NULL; xp_rewind(s);
        hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, (void **)&p);
        XEQ(hr, CO_E_OBJNOTCONNECTED);
        CHECK(p == NULL);
    }
    if (s) IStream_Release(s);
    if (o) IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

XT(apt_marshal_release_data_drops_stub_reference) {
    IStream *s; XpObj *o; HRESULT hr;
    XP_NEED_BASE();
    INIT(STA());
    s = xp_new_stream(); o = xp_obj_new();
    if (s && o) {
        hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK);
        CHECK(o->ref > 1);
        xp_rewind(s);
        XEQ(U(CoReleaseMarshalData)(s), S_OK);
        XEQ(o->ref, 1);
        /* reading beyond the (now consumed position of the) data: the stream is at its end */
        hr = U(CoReleaseMarshalData)(s);
        CHECKF(FAILED(hr), "second release at end of stream returned %08lx", hr);
        XEQ(hr, STG_E_READFAULT);
    } else CHECK(0);
    if (s) IStream_Release(s);
    if (o) IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

XT(apt_proxy_released_marshal_data_is_disconnected) {
    XpFx f; HRESULT hr; IClassFactory *p = NULL;
    XP_NEED_BASE();
    INIT(STA());
    f.o = xp_obj_new(); f.o->destroyedFlag = &f.gone;
    if (xp_host_start(&f.h, STA(), XP_OBJ_UNK(f.o), &XP_IID_IClassFactory, MSHLFLAGS_NORMAL)) {
        xp_rewind(f.h.stream);
        XEQ(U(CoReleaseMarshalData)(f.h.stream), S_OK);   /* from the wrong apartment, still legal */
        XEQ(f.o->ref, 1);
        hr = xp_host_unmarshal(&f.h, &XP_IID_IClassFactory, (void **)&p);
        /* Windows hands out a proxy for the released data; using it reports a disconnect. */
        XEQ(hr, S_OK);
        if (p) {
            hr = probe(p);
            XEQ(hr, XP_RPC_E_DISCONNECTED);
            XEQ(f.o->createCalls, 0);
            IClassFactory_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&f.h);
    IClassFactory_Release(XP_OBJ_CF(f.o));
    UNINIT();
}

/* ---- proxy shape: QueryInterface, reference counting, identity --------------------------- */
XT(apt_proxy_qi_unsupported_iid_e_nointerface) {
    XpFx f; void *pv = (void *)1; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IUnknownBogus, &pv);
        XEQ(hr, E_NOINTERFACE);
        CHECK(pv == NULL);
        t_note("object saw QI for the unsupported iid: %d", IsEqualGUID(&f.o->lastQi, &XP_IID_IUnknownBogus));
        pv = (void *)1;
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IStream, &pv);
        XEQ(hr, E_NOINTERFACE);
        CHECK(pv == NULL);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_proxy_qi_supported_interfaces) {
    XpFx f; IUnknown *u = NULL, *u2 = NULL; IClassFactory *cf = NULL; void *pv; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IUnknown, (void **)&u);   XEQ(hr, S_OK);
        hr = IUnknown_QueryInterface(u, &XP_IID_IClassFactory, (void **)&cf);    XEQ(hr, S_OK);
        CHECK(cf != NULL);
        if (u) { hr = IUnknown_QueryInterface(u, &XP_IID_IUnknown, (void **)&u2); XEQ(hr, S_OK); XEQ(u, u2); if (u2) IUnknown_Release(u2); }
        /* interfaces every proxy manager implements locally */
        static const GUID *local_iids[] = { &XP_IID_IMarshal, &XP_IID_IClientSecurity, &XP_IID_IMultiQI };
        for (int i = 0; i < 3; i++) {
            pv = NULL;
            hr = IClassFactory_QueryInterface(f.p, local_iids[i], &pv);
            CHECKF(hr == S_OK && pv, "iid index %d hr=%08lx", i, hr);
            if (pv) IUnknown_Release((IUnknown *)pv);
        }
        if (cf) IClassFactory_Release(cf);
        if (u) IUnknown_Release(u);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_proxy_refcounts_are_local) {
    XpFx f; ULONG r;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        LONG objref = f.o->ref;
        r = IClassFactory_AddRef(f.p);  XEQ(r, 2);
        r = IClassFactory_AddRef(f.p);  XEQ(r, 3);
        XEQ(f.o->ref, objref);                     /* proxy AddRef never reaches the object */
        r = IClassFactory_Release(f.p); XEQ(r, 2);
        r = IClassFactory_Release(f.p); XEQ(r, 1);
        XEQ(f.o->ref, objref);
        XEQ(f.gone, 0);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_proxy_release_last_drops_stub_reference) {
    XpFx f; ULONG r;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        CHECK(f.o->ref > 1);                             /* stub holds the object while a proxy exists */
        r = IClassFactory_Release(f.p); f.p = NULL;
        XEQ(r, 0);
        /* RemRelease is synchronous; the stub manager lets go of the object */
        XEQ(f.o->ref, 1);
        XEQ(f.gone, 0);
    } else CHECK(0);
    xp_fx_teardown(&f);
    XEQ(f.gone, 1);                                /* teardown dropped the test's own reference */
    UNINIT();
}
XT(apt_proxy_identity_via_out_param) {
    XpFx f; IUnknown *id1 = NULL, *out = NULL, *id2 = NULL; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IUnknown, (void **)&id1); XEQ(hr, S_OK);
        /* CreateInstance hands the object itself back as an out interface: same OID -> same proxy manager */
        hr = IClassFactory_CreateInstance(f.p, NULL, &XP_IID_IUnknown, (void **)&out);
        XEQ(hr, S_OK);
        CHECK(IsEqualGUID(&f.o->lastCreateIid, &XP_IID_IUnknown));
        XEQ(f.o->lastCreateOuterNull, 1);
        if (out) { hr = IUnknown_QueryInterface(out, &XP_IID_IUnknown, (void **)&id2); XEQ(hr, S_OK); }
        XEQ(id1, id2);
        XEQ(out, id1);
        CHECK(out != (IUnknown *)XP_OBJ_CF(f.o));
        if (id2) IUnknown_Release(id2);
        if (out) IUnknown_Release(out);
        if (id1) IUnknown_Release(id1);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_proxy_create_instance_out_param_null_on_failure) {
    XpFx f; void *pv; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        pv = (void *)1;
        hr = IClassFactory_CreateInstance(f.p, NULL, &XP_IID_IUnknownBogus, &pv);
        XEQ(hr, E_NOINTERFACE);
        CHECK(pv == NULL);
        CHECK(IsEqualGUID(&f.o->lastCreateIid, &XP_IID_IUnknownBogus));
        XEQ(f.o->createCalls, 1);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_hresult_passthrough_table) {
    static const HRESULT codes[] = { S_OK, S_FALSE, E_FAIL, E_UNEXPECTED, E_OUTOFMEMORY, E_NOTIMPL, E_INVALIDARG,
        CLASS_E_NOAGGREGATION, CLASS_E_CLASSNOTAVAILABLE, REGDB_E_CLASSNOTREG, STG_E_FILENOTFOUND, DISP_E_TYPEMISMATCH,
        (HRESULT)0x00040001, (HRESULT)0x00000042, XP_CUSTOM_HR, (HRESULT)0x80070005, (HRESULT)0x8004DEAD, (HRESULT)0xC0000005 };
    XpFx f;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
            void *pv = NULL; HRESULT hr;
            f.o->createHr = codes[i];
            hr = IClassFactory_CreateInstance(f.p, NULL, &XP_IID_IUnknown, &pv);
            CHECKF(hr == codes[i], "index %u: sent %08lx got %08lx", i, codes[i], hr);
            if (codes[i] != S_OK && pv) { CHECK(0); }
            if (pv) IUnknown_Release((IUnknown *)pv);
        }
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}

/* ---- IMultiQI ------------------------------------------------------------------------------ */
XT(apt_proxy_imultiqi) {
    XpFx f; IMultiQI *mq = NULL; HRESULT hr; MULTI_QI q[3];
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IMultiQI, (void **)&mq);
        XEQ(hr, S_OK);
        if (mq) {
            memset(q, 0, sizeof q);
            q[0].pIID = &XP_IID_IClassFactory; q[1].pIID = &XP_IID_IUnknownBogus; q[2].pIID = &XP_IID_IUnknown;
            hr = IMultiQI_QueryMultipleInterfaces(mq, 3, q);
            XEQ(hr, S_FALSE);                       /* some, but not all, interfaces were available */
            XEQ(q[0].hr, S_OK); CHECK(q[0].pItf != NULL);
            XEQ(q[1].hr, E_NOINTERFACE); CHECK(q[1].pItf == NULL);
            XEQ(q[2].hr, S_OK); CHECK(q[2].pItf != NULL);
            for (int i = 0; i < 3; i++) if (q[i].pItf) IUnknown_Release(q[i].pItf);
            memset(q, 0, sizeof q);
            q[0].pIID = &XP_IID_IClassFactory; q[1].pIID = &XP_IID_IUnknown;
            hr = IMultiQI_QueryMultipleInterfaces(mq, 2, q);
            XEQ(hr, S_OK);
            XEQ(q[0].hr, S_OK); XEQ(q[1].hr, S_OK);
            for (int i = 0; i < 2; i++) if (q[i].pItf) IUnknown_Release(q[i].pItf);
            memset(q, 0, sizeof q);
            q[0].pIID = &XP_IID_IUnknownBogus; q[1].pIID = &XP_IID_IXpBogus2;
            hr = IMultiQI_QueryMultipleInterfaces(mq, 2, q);
            XEQ(hr, E_NOINTERFACE);                 /* none available */
            XEQ(q[0].hr, E_NOINTERFACE); XEQ(q[1].hr, E_NOINTERFACE);
            IMultiQI_Release(mq);
        }
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}

/* ---- wrong thread / uninitialized ------------------------------------------------------------ */
struct wt { IClassFactory *cf; HRESULT hrCall, hrQi, hrCall2; int model; HRESULT initHr; };
static DWORD wt_proc(void *p)
{
    struct wt *w = p; IUnknown *u = NULL; void *pv = NULL;
    w->hrCall = IClassFactory_CreateInstance(w->cf, NULL, &XP_IID_IUnknown, (void **)&u);
    if (u) IUnknown_Release(u);
    w->hrQi = IClassFactory_QueryInterface(w->cf, &XP_IID_IStream, &pv);
    w->hrCall2 = probe(w->cf);
    return 0;
}
static void wrong_thread_case(int model, DWORD hostModel)
{
    XpFx f; struct wt w;
    XP_NEED_BASE(); INIT(hostModel == STA() ? STA() : STA());
    if (xp_fx_setup(&f, hostModel, MSHLFLAGS_NORMAL)) {
        memset(&w, 0, sizeof w); w.cf = f.p; w.model = model;
        XEQ(xp_thread_run(model, wt_proc, &w, 20000), 0);
        XEQ(f.o->createCalls, 0);
        XEQ(f.o->createCalls, 0);
        if (model < 0) {
            CHECKF(FAILED(w.hrCall) && FAILED(w.hrCall2), "calls from an uninitialized thread: %08lx %08lx", w.hrCall, w.hrCall2);
            t_note("proxy call from uninitialized thread: %08lx (%s) / %08lx", w.hrCall, xp_hrname(w.hrCall), w.hrCall2);
            t_note("QueryInterface(unsupported iid) on a proxy used in an uninitialized thread: %08lx (%s)", w.hrQi, xp_hrname(w.hrQi));
        } else {
            XEQ(w.hrCall, XP_RPC_E_WRONG_THREAD);
            XEQ(w.hrCall2, XP_RPC_E_WRONG_THREAD);
            XEQ(w.hrQi, XP_RPC_E_WRONG_THREAD);
        }
        /* the proxy still works in its own apartment */
        XEQ(probe(f.p), S_OK);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_proxy_used_from_other_sta_wrong_thread) { wrong_thread_case(COINIT_APARTMENTTHREADED, STA()); }
XT(apt_proxy_used_from_mta_wrong_thread) { wrong_thread_case(COINIT_MULTITHREADED, STA()); }
XT(apt_proxy_used_from_uninitialized_thread) { wrong_thread_case(-1, STA()); }
XT(apt_mta_proxy_used_from_sta_wrong_thread) {
    /* a proxy living in the main MTA (host is an STA) used from an STA thread */
    XpFx f; struct wt w;
    XP_NEED_BASE(); INIT(MTA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        memset(&w, 0, sizeof w); w.cf = f.p;
        XEQ(xp_thread_run(COINIT_APARTMENTTHREADED, wt_proc, &w, 20000), 0);
        t_note("MTA-resident proxy used from an STA: CreateInstance=%08lx QI=%08lx", w.hrCall, w.hrQi);
        CHECK(FAILED(w.hrCall) || w.hrCall == S_OK);
        XEQ(probe(f.p), S_OK);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}

struct umt { IStream *s; HRESULT hr; HRESULT hrM; IStream *s2; };
static DWORD umt_proc(void *p)
{
    struct umt *u = p; void *pv = NULL; XpObj *o = xp_obj_new();
    u->hr = U(CoUnmarshalInterface)(u->s, &XP_IID_IClassFactory, &pv);
    u->hrM = U(CoMarshalInterface)(u->s2, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    IClassFactory_Release(XP_OBJ_CF(o));
    return 0;
}
XT(apt_marshal_unmarshal_without_coinitialize) {
    XpFx f; struct umt u;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        memset(&u, 0, sizeof u); u.s = f.h.stream; u.s2 = xp_new_stream();
        xp_rewind(u.s);
        XEQ(xp_thread_run(-1, umt_proc, &u, 20000), 0);
        XEQ(u.hr, XP_CO_E_NOTINITIALIZED);
        XEQ(u.hrM, XP_CO_E_NOTINITIALIZED);
        if (u.s2) IStream_Release(u.s2);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}

/* ---- disconnects ----------------------------------------------------------------------------------- */
static void do_disconnect(void *arg) { XpObj *o = arg; HRESULT hr = U(CoDisconnectObject)(XP_OBJ_UNK(o), 0); o->hookHr = hr; }
XT(apt_disconnect_object_rpc_e_disconnected) {
    XpFx f; HRESULT hr; void *pv = NULL;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        XEQ(probe(f.p), S_OK);
        CHECK(f.o->ref > 1);
        CHECK(xp_host_run(&f.h, do_disconnect, f.o));
        XEQ(f.o->hookHr, S_OK);
        XEQ(f.o->ref, 1);                                /* stub manager released the object */
        hr = probe(f.p);
        XEQ(hr, XP_RPC_E_DISCONNECTED);
        XEQ(f.o->createCalls, 1);                          /* the call never reached the object */
        hr = IClassFactory_CreateInstance(f.p, NULL, &XP_IID_IUnknown, &pv);
        XEQ(hr, XP_RPC_E_DISCONNECTED);
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IUnknownBogus, &pv);
        CHECKF(FAILED(hr), "QI after disconnect %08lx", hr);
        t_note("QueryInterface(unsupported iid) after CoDisconnectObject: %08lx (%s)", hr, xp_hrname(hr));
        /* IUnknown on the proxy itself still works locally */
        CHECK(IClassFactory_AddRef(f.p) >= 2); IClassFactory_Release(f.p);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_disconnect_object_invalid_and_unmarshaled) {
    XpObj *o; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    hr = U(CoDisconnectObject)(NULL, 0);
    XEQ(hr, E_INVALIDARG);
    o = xp_obj_new();
    hr = U(CoDisconnectObject)(XP_OBJ_UNK(o), 0);            /* never marshaled: nothing to disconnect */
    XEQ(hr, S_OK);
    XEQ(o->ref, 1);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
static const HRESULT dead_set[] = { (HRESULT)0x80010108, (HRESULT)0x800401FD, (HRESULT)0x800706BA, (HRESULT)0x80010012, (HRESULT)0x800706BE, (HRESULT)0x80010007 };
static int in_dead_set(HRESULT hr) { for (unsigned i = 0; i < sizeof dead_set / sizeof *dead_set; i++) if (dead_set[i] == hr) return 1; return 0; }
static void apt_teardown_case(DWORD hostModel)
{
    XpFx f; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, hostModel, MSHLFLAGS_NORMAL)) {
        XEQ(probe(f.p), S_OK);
        xp_host_stop(&f.h);                                    /* host apartment is torn down (CoUninitialize) */
        XEQ(f.o->ref, 1);                                /* its stubs were released with it */
        hr = probe(f.p);
        CHECKF(FAILED(hr), "call into a destroyed apartment returned %08lx (%s)", hr, xp_hrname(hr));
        t_note("call into destroyed %s apartment: %08lx (%s)", hostModel == STA() ? "STA" : "MTA", hr, xp_hrname(hr));
        XEQ(f.o->createCalls, 1);
        XEQ(IClassFactory_Release(f.p), 0); f.p = NULL;
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_stub_sta_apartment_shutdown) { apt_teardown_case(STA()); }
XT(apt_stub_mta_apartment_shutdown) { apt_teardown_case(MTA()); }

XT(apt_proxy_apartment_shutdown_object_not_connected) {
    XpFx f; IUnknown *u = NULL; HRESULT hr; ULONG r;
    XP_NEED_BASE(); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        CHECK(f.o->ref > 1);
        UNINIT();                                              /* destroys the proxy's apartment; balances INIT above */
        XEQ(f.o->ref, 1);                                /* stub reference released */
        hr = IClassFactory_CreateInstance(f.p, NULL, &XP_IID_IUnknown, (void **)&u);
        XEQ(hr, XP_CO_E_OBJNOTCONNECTED);
        r = IClassFactory_Release(f.p); f.p = NULL;
        XEQ(r, 0);
        xp_fx_teardown(&f);
        return;
    }
    CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}

/* ---- CoGetCallerTID inside a call ---------------------------------------------------------------- */
XT(apt_caller_tid_inside_call) {
    XpFx f; DWORD tid = 0xDEAD; HRESULT hr;
    XP_NEED_BASE(); NEED(CoGetCallerTID); INIT(STA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        XEQ(probe(f.p), S_OK);
        XEQ(f.o->lastCallerTidHr, S_OK);                 /* caller is a thread of this process */
        XEQ(f.o->lastCallerTid, f.mainTid);
        hr = U(CoGetCallerTID)(&tid);
        t_note("CoGetCallerTID outside any call: hr=%08lx tid=%lu", hr, tid);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_caller_tid_from_mta_caller) {
    XpFx f;
    XP_NEED_BASE(); NEED(CoGetCallerTID); INIT(MTA());
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        XEQ(probe(f.p), S_OK);
        XEQ(f.o->lastTid, f.h.tid);
        XEQ(f.o->lastCallerTidHr, S_OK);
        XEQ(f.o->lastCallerTid, f.mainTid);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(apt_caller_tid_null) {
    NEED(CoGetCallerTID);
    XEQ(U(CoGetCallerTID)(NULL), E_INVALIDARG);
}

/* ---- re-entrancy while an STA is waiting for an outgoing call -------------------------------------- */
typedef struct { IStream *toMain; HRESULT hrUnmarshal, hrInner; IClassFactory *back; } ReentCtx;
static void reent_hook(XpObj *o, int what, BOOL arg)
{
    ReentCtx *c = o->user;
    if (what != XP_HOOK_CREATE) return;
    /* we are inside a call from the main STA: call back into the main STA through its proxy */
    c->hrUnmarshal = U(CoUnmarshalInterface)(c->toMain, &XP_IID_IClassFactory, (void **)&c->back);
    if (c->back) {
        c->hrInner = probe(c->back);
        IClassFactory_Release(c->back); c->back = NULL;
    }
}
XT(apt_sta_reentrancy_incoming_call_while_waiting) {
    XpFx f; XpObj *mainObj; ReentCtx c; IStream *s; HRESULT hr;
    XP_NEED_BASE(); NEED(CoGetCallerTID); INIT(STA());
    mainObj = xp_obj_new(); s = xp_new_stream();
    CHECK(mainObj && s);
    if (mainObj && s && xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        memset(&c, 0, sizeof c);
        /* marshal an object owned by the main STA for the host to call back */
        hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(mainObj), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK);
        xp_rewind(s);
        c.toMain = s;
        f.o->user = &c; f.o->hook = reent_hook;
        c.hrInner = (HRESULT)0xDEADBEEF;
        hr = probe(f.p);               /* main STA blocks in COM, dispatching the nested call */
        XEQ(hr, S_OK);
        XEQ(c.hrUnmarshal, S_OK);
        XEQ(c.hrInner, S_OK);
        XEQ(mainObj->createCalls, 1);
        XEQ(mainObj->lastTid, f.mainTid);                 /* the nested call ran on the main STA thread */
        XEQ(mainObj->lastCallerTid, f.h.tid);
        f.o->hook = NULL;
    } else CHECK(0);
    xp_fx_teardown(&f);
    if (s) IStream_Release(s);
    if (mainObj) IClassFactory_Release(XP_OBJ_CF(mainObj));
    UNINIT();
}

/* ---- IMessageFilter ------------------------------------------------------------------------------ */
typedef struct { IMessageFilter iface; volatile LONG ref; volatile LONG handle, retry, pending; DWORD script[8]; int nscript; DWORD lastCallType, lastRejectType, retryRet; } XpFilter;
static XpFilter *FI(IMessageFilter *f) { return (XpFilter *)f; }
static HRESULT WINAPI f_qi(IMessageFilter *f, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_IMessageFilter)) { *ppv = f; InterlockedIncrement(&FI(f)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_addref(IMessageFilter *f) { return InterlockedIncrement(&FI(f)->ref); }
static ULONG WINAPI f_release(IMessageFilter *f) { return InterlockedDecrement(&FI(f)->ref); }
static DWORD WINAPI f_incoming(IMessageFilter *f, DWORD callType, HTASK tid, DWORD tick, LPINTERFACEINFO info)
{
    XpFilter *x = FI(f); LONG n = InterlockedIncrement(&x->handle) - 1;
    x->lastCallType = callType;
    return (n < x->nscript) ? x->script[n] : SERVERCALL_ISHANDLED;
}
static DWORD WINAPI f_retry(IMessageFilter *f, HTASK tid, DWORD tick, DWORD rejectType)
{
    XpFilter *x = FI(f); InterlockedIncrement(&x->retry); x->lastRejectType = rejectType; return x->retryRet;
}
static DWORD WINAPI f_pending(IMessageFilter *f, HTASK tid, DWORD tick, DWORD pendingType)
{ InterlockedIncrement(&FI(f)->pending); return PENDINGMSG_WAITDEFPROCESS; }
static IMessageFilterVtbl f_vtbl = { f_qi, f_addref, f_release, f_incoming, f_retry, f_pending };
static void filter_init(XpFilter *x) { memset(x, 0, sizeof *x); x->iface.lpVtbl = &f_vtbl; x->ref = 1; }

XT(apt_message_filter_server_rejects_call) {
    XpFx f; XpFilter flt; HRESULT hr; XpHost h; XpObj *o; IClassFactory *p = NULL;
    XP_NEED_BASE(); NEED(CoRegisterMessageFilter); INIT(STA());
    filter_init(&flt); flt.script[0] = SERVERCALL_REJECTED; flt.nscript = 1;
    o = xp_obj_new();
    if (xp_host_start_ex(&h, STA(), XP_OBJ_UNK(o), &XP_IID_IClassFactory, MSHLFLAGS_NORMAL, &flt.iface)) {
        XEQ(xp_host_unmarshal(&h, &XP_IID_IClassFactory, (void **)&p), S_OK);
        if (p) {
            void *pv = NULL;
            hr = IClassFactory_CreateInstance(p, NULL, &XP_IID_IUnknown, &pv);
            XEQ(hr, XP_RPC_E_CALL_REJECTED);
            XEQ(o->createCalls, 0);                        /* rejected before dispatch */
            XEQ(flt.handle, 1);
            XEQ(flt.lastCallType, CALLTYPE_TOPLEVEL);
            hr = IClassFactory_CreateInstance(p, NULL, &XP_IID_IUnknown, &pv);
            XEQ(hr, S_OK);                                 /* now SERVERCALL_ISHANDLED */
            XEQ(o->createCalls, 1);
            if (pv) IUnknown_Release((IUnknown *)pv);
            IClassFactory_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    IClassFactory_Release(XP_OBJ_CF(o));
    XEQ(flt.ref, 1);                                       /* the apartment released its filter reference on teardown */
    UNINIT();
}
XT(apt_message_filter_retrylater_client_retries) {
    XpFilter srv, cli; XpHost h; XpObj *o; IClassFactory *p = NULL; IMessageFilter *old = NULL; HRESULT hr;
    XP_NEED_BASE(); NEED(CoRegisterMessageFilter); INIT(STA());
    filter_init(&srv); srv.script[0] = SERVERCALL_RETRYLATER; srv.nscript = 1;
    filter_init(&cli); cli.retryRet = 0;                         /* RetryRejectedCall: retry immediately */
    o = xp_obj_new();
    if (xp_host_start_ex(&h, STA(), XP_OBJ_UNK(o), &XP_IID_IClassFactory, MSHLFLAGS_NORMAL, &srv.iface)) {
        XEQ(xp_host_unmarshal(&h, &XP_IID_IClassFactory, (void **)&p), S_OK);
        if (p) {
            void *pv = NULL;
            hr = U(CoRegisterMessageFilter)(&cli.iface, &old);
            XEQ(hr, S_OK);
            CHECK(old == NULL);
            hr = IClassFactory_CreateInstance(p, NULL, &XP_IID_IUnknown, &pv);
            XEQ(hr, S_OK);
            XEQ(cli.retry, 1);
            XEQ(cli.lastRejectType, SERVERCALL_RETRYLATER);
            XEQ(srv.handle, 2);
            XEQ(o->createCalls, 1);
            if (pv) IUnknown_Release((IUnknown *)pv);
            old = NULL;
            hr = U(CoRegisterMessageFilter)(NULL, &old);
            XEQ(hr, S_OK);
            XEQ(old, &cli.iface);
            if (old) IMessageFilter_Release(old);
            IClassFactory_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    IClassFactory_Release(XP_OBJ_CF(o));
    XEQ(cli.ref, 1);
    UNINIT();
}
XT(apt_message_filter_retry_declined_returns_retrylater) {
    XpFilter srv, cli; XpHost h; XpObj *o; IClassFactory *p = NULL; IMessageFilter *old = NULL; HRESULT hr;
    XP_NEED_BASE(); NEED(CoRegisterMessageFilter); INIT(STA());
    filter_init(&srv); srv.script[0] = SERVERCALL_RETRYLATER; srv.nscript = 1;
    filter_init(&cli); cli.retryRet = (DWORD)-1;                 /* cancel the call */
    o = xp_obj_new();
    if (xp_host_start_ex(&h, STA(), XP_OBJ_UNK(o), &XP_IID_IClassFactory, MSHLFLAGS_NORMAL, &srv.iface)) {
        XEQ(xp_host_unmarshal(&h, &XP_IID_IClassFactory, (void **)&p), S_OK);
        if (p) {
            void *pv = NULL;
            XEQ(U(CoRegisterMessageFilter)(&cli.iface, &old), S_OK);
            hr = IClassFactory_CreateInstance(p, NULL, &XP_IID_IUnknown, &pv);
            XEQ(cli.retry, 1);
            XEQ(o->createCalls, 0);
            CHECKF(FAILED(hr), "declined retry returned %08lx", hr);
            t_note("RETRYLATER with client declining the retry: %08lx (%s)", hr, xp_hrname(hr));
            old = NULL;
            XEQ(U(CoRegisterMessageFilter)(NULL, &old), S_OK);
            if (old) IMessageFilter_Release(old);
            IClassFactory_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(apt_message_filter_register_semantics) {
    XpFilter a, b; IMessageFilter *old = NULL, *old2 = NULL; HRESULT hr;
    XP_NEED_BASE(); NEED(CoRegisterMessageFilter); INIT(STA());
    filter_init(&a); filter_init(&b);
    hr = U(CoRegisterMessageFilter)(&a.iface, &old);
    XEQ(hr, S_OK); CHECK(old == NULL);
    XEQ(a.ref, 2);                                         /* registration AddRefs the filter */
    hr = U(CoRegisterMessageFilter)(&b.iface, &old);
    XEQ(hr, S_OK); XEQ(old, &a.iface);
    XEQ(a.ref, 2);                                         /* ownership of the previous reference moved to the caller */
    XEQ(b.ref, 2);
    if (old) IMessageFilter_Release(old);
    XEQ(a.ref, 1);
    hr = U(CoRegisterMessageFilter)(NULL, NULL);                 /* unregister without asking for the old one */
    XEQ(hr, S_OK);
    XEQ(b.ref, 1);
    hr = U(CoRegisterMessageFilter)(NULL, &old2);                /* nothing registered */
    XEQ(hr, S_OK); CHECK(old2 == NULL);
    UNINIT();
}
static DWORD mf_mta_proc(void *p) { XpFilter *f = p; IMessageFilter *old = NULL; HRESULT hr = U(CoRegisterMessageFilter)(&f->iface, &old); f->lastCallType = (DWORD)hr; return 0; }
XT(apt_message_filter_not_allowed_in_mta) {
    XpFilter a;
    XP_NEED_BASE(); NEED(CoRegisterMessageFilter);
    filter_init(&a);
    XEQ(xp_thread_run(COINIT_MULTITHREADED, mf_mta_proc, &a, 10000), 0);
    XEQ((HRESULT)a.lastCallType, XP_CO_E_NOT_SUPPORTED);
    XEQ(a.ref, 1);
}
/* ---- inter-thread marshal helpers ------------------------------------------------------------------ */
XT(apt_interthread_stream_helpers_invalid_args) {
    IStream *s = NULL; XpObj *o; void *pv = (void *)1;
    XP_NEED_BASE(); NEED(CoMarshalInterThreadInterfaceInStream); NEED(CoGetInterfaceAndReleaseStream);
    if (t_is_wine()) { t_skip("emulator dereferences NULL arguments"); return; }
    o = xp_obj_new();
    XEQ(U(CoMarshalInterThreadInterfaceInStream)(NULL, XP_OBJ_UNK(o), &s), E_INVALIDARG);
    XEQ(U(CoMarshalInterThreadInterfaceInStream)(&XP_IID_IClassFactory, NULL, &s), E_INVALIDARG);
    XEQ(U(CoMarshalInterThreadInterfaceInStream)(&XP_IID_IClassFactory, XP_OBJ_UNK(o), NULL), E_INVALIDARG);
    CHECK(s == NULL);
    if (!t_is_wine()) XEQ(U(CoGetInterfaceAndReleaseStream)(NULL, &XP_IID_IClassFactory, &pv), E_INVALIDARG);
    s = xp_new_stream();
    XEQ(U(CoGetInterfaceAndReleaseStream)(s, NULL, &pv), E_INVALIDARG);
    XEQ(U(CoGetInterfaceAndReleaseStream)(s, &XP_IID_IClassFactory, NULL), E_INVALIDARG);
    XEQ(IStream_Release(s), 0);                            /* invalid-argument paths do not consume the stream */
    XEQ(o->ref, 1);
    IClassFactory_Release(XP_OBJ_CF(o));
}
struct itw { XpObj *o; IStream *s; HRESULT hr; };
static void it_work(void *a)
{
    struct itw *w = a;
    w->hr = U(CoMarshalInterThreadInterfaceInStream)(&XP_IID_IClassFactory, XP_OBJ_UNK(w->o), &w->s);
}
XT(apt_interthread_marshal_roundtrip) {
    struct itw w; XpHost h; IClassFactory *p = NULL; HRESULT hr; volatile LONG gone = 0;
    XP_NEED_BASE(); NEED(CoMarshalInterThreadInterfaceInStream); NEED(CoGetInterfaceAndReleaseStream);
    INIT(STA());
    memset(&w, 0, sizeof w);
    w.o = xp_obj_new(); w.o->destroyedFlag = &gone;
    if (xp_host_start_ex(&h, STA(), NULL, NULL, 0, NULL)) {
        CHECK(xp_host_run(&h, it_work, &w));
        XEQ(w.hr, S_OK);
        CHECK(w.s != NULL);
        CHECK(w.o->ref > 1);
        if (w.s) {
            hr = U(CoGetInterfaceAndReleaseStream)(w.s, &XP_IID_IClassFactory, (void **)&p);
            XEQ(hr, S_OK);
            CHECK(p != NULL && p != XP_OBJ_CF(w.o));
            if (p) {
                f_probe_false(w.o, p);
                XEQ(w.o->lastTid, h.tid);
                IClassFactory_Release(p);
            }
        }
        XEQ(w.o->ref, 1);
    } else CHECK(0);
    xp_host_stop(&h);
    IClassFactory_Release(XP_OBJ_CF(w.o));
    XEQ(gone, 1);
    UNINIT();
}
XT(apt_interthread_marshal_unmarshal_as_other_iid) {
    /* unmarshal the same data as IUnknown: allowed for the interface that was marshaled and its base */
    struct itw w; XpHost h; IUnknown *u = NULL; IClassFactory *cf = NULL; HRESULT hr;
    XP_NEED_BASE(); NEED(CoMarshalInterThreadInterfaceInStream); NEED(CoGetInterfaceAndReleaseStream);
    INIT(STA());
    memset(&w, 0, sizeof w);
    w.o = xp_obj_new();
    if (xp_host_start_ex(&h, STA(), NULL, NULL, 0, NULL)) {
        CHECK(xp_host_run(&h, it_work, &w));
        hr = w.s ? U(CoGetInterfaceAndReleaseStream)(w.s, &XP_IID_IUnknown, (void **)&u) : E_FAIL;
        XEQ(hr, S_OK);
        if (u) {
            hr = IUnknown_QueryInterface(u, &XP_IID_IClassFactory, (void **)&cf);
            XEQ(hr, S_OK);
            if (cf) { XEQ(probe(cf), S_OK); IClassFactory_Release(cf); }
            IUnknown_Release(u);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    IClassFactory_Release(XP_OBJ_CF(w.o));
    UNINIT();
}

static void f_probe_false(XpObj *o, IClassFactory *p) { HRESULT hr; o->probeHr = S_FALSE; hr = probe(p); XEQ(hr, S_FALSE); }

XT(apt_lockserver_through_proxy) {
    XpFx f; HRESULT hr;
    XP_NEED_BASE(); INIT(STA());
    if (t_is_wine()) { t_skip("emulator stubs IClassFactory::LockServer proxy"); UNINIT(); return; }
    if (xp_fx_setup(&f, STA(), MSHLFLAGS_NORMAL)) {
        hr = IClassFactory_LockServer(f.p, TRUE);   XEQ(hr, S_OK);
        hr = IClassFactory_LockServer(f.p, FALSE);  XEQ(hr, S_FALSE);
        XEQ(f.o->lockCalls, 2);
        XEQ(f.o->lastLock, 0);
        XEQ(f.o->lastTid, f.h.tid);
        f.o->lockHrTrue = XP_CUSTOM_HR;
        hr = IClassFactory_LockServer(f.p, TRUE);   XEQ(hr, XP_CUSTOM_HR);
        XEQ(f.o->lastLock, 1);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
