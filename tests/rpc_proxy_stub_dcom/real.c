/* The hand-built proxy/stub pair driven by the real COM channel (standard marshaling between apartments of this
 * process), found through CoRegisterPSClsid/CoGetPSClsid + a class object for the PS CLSID. */
#include "psd.h"

/* pump messages of the calling STA until h is signalled (bounded) */
static BOOL pump_wait(HANDLE h, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    for (;;) {
        DWORD left = end - GetTickCount();
        if ((LONG)left <= 0) return FALSE;
        DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, left, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return TRUE;
        if (r == WAIT_OBJECT_0 + 1) { MSG m; while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); } }
        else if (r == WAIT_TIMEOUT) return FALSE;
        else return FALSE;
    }
}

typedef struct Work {
    IStream *stm;
    GUID iid;
    int mode;
    HRESULT hr_get, hr_call, hr_call2;
    LONG out, out2;
    void *proxy_vtbl, *proxy_ptr;
    DWORD tid;
    IUnknown *obj2;
    HRESULT hr_qi;
} Work;

static DWORD WINAPI worker(LPVOID p) {
    Work *w = p;
    w->tid = GetCurrentThreadId();
    HRESULT hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { w->hr_get = hr; return 0; }
    void *pv = NULL;
    DWORD ck = 0; IPSFactoryBuffer *psf = NULL;
    psd_register_ps(&ck, &psf);                              /* the MTA needs the PS class object too */
    w->hr_get = U(CoGetInterfaceAndReleaseStream)(w->stm, &w->iid, &pv);
    if (w->hr_get == S_OK && pv) {
        w->proxy_ptr = pv;
        w->proxy_vtbl = *(void **)pv;
        if (w->mode == 0) {                                  /* IOletOne::Add */
            IOletOne *o = pv;
            w->hr_call = o->lpVtbl->Add(o, 40, 2, &w->out);
            w->hr_call2 = o->lpVtbl->Add(o, 1, 0x5a5a5a5a, &w->out2);
        } else if (w->mode == 1) {                           /* server exception */
            IOletOne *o = pv;
            w->hr_call = o->lpVtbl->Add(o, 0x7fffdead, 1, &w->out);
        } else if (w->mode == 2) {                           /* IOletTwo::GetObj -> another proxy */
            IOletTwo *t = pv;
            IUnknown *u = NULL;
            w->hr_call = t->lpVtbl->GetObj(t, &psd_IID_One, &u);
            w->obj2 = u;
            if (u) {
                IOletOne *o = NULL;
                w->hr_qi = IUnknown_QueryInterface(u, &psd_IID_One, (void **)&o);
                if (o) { w->hr_call2 = o->lpVtbl->Add(o, 5, 6, &w->out2); IUnknown_Release((IUnknown *)o); }
                w->hr_call = w->hr_call;
            }
        } else if (w->mode == 3) {                           /* IOletTwo::Str / Echo */
            IOletTwo *t = pv;
            w->hr_call = t->lpVtbl->Echo(t, 77, &w->out);
            w->hr_call2 = t->lpVtbl->Str(t, L"across", &w->out2);
        }
        IUnknown_Release((IUnknown *)pv);
        if (w->obj2) IUnknown_Release(w->obj2);
    }
    psd_unregister_ps(ck, psf);
    U(CoUninitialize)();
    return 0;
}

static int run_worker(Work *w, const GUID *iid, int mode, PsdSrv *srv, IUnknown *obj) {
    memset(w, 0, sizeof *w);
    w->iid = *iid; w->mode = mode;
    if (U(CoMarshalInterThreadInterfaceInStream)(iid, obj, &w->stm) != S_OK) return 0;
    HANDLE th = CreateThread(NULL, 0, worker, w, 0, NULL);
    if (!th) return 0;
    BOOL ok = pump_wait(th, 10000);
    if (!ok) { t_note("worker did not finish in 10 s"); TerminateThread(th, 1); }
    CloseHandle(th);
    return ok;
}

TEST(psdr_getpsclsid_registered) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !psd_register_ps(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    CLSID c;
    memset(&c, 0, sizeof c);
    EQHR(U(CoGetPSClsid)(&psd_IID_One, &c), S_OK);
    CHECK(IsEqualGUID(&c, &psd_CLSID_PSF));
    memset(&c, 0, sizeof c);
    EQHR(U(CoGetPSClsid)(&psd_IID_Two, &c), S_OK);
    CHECK(IsEqualGUID(&c, &psd_CLSID_PSF));
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdr_getpsclsid_errors) {
    PSD_STA();
    CLSID c;
    memset(&c, 0x5a, sizeof c);
    HRESULT hr = U(CoGetPSClsid)(&psd_IID_Unknown1, &c);
    EQHR(hr, REGDB_E_IIDNOTREG);                              /* unknown interface: no ProxyStubClsid32 */
    if (!t_is_wine()) {
        EQHR(U(CoGetPSClsid)(NULL, &c), E_INVALIDARG);
        EQHR(U(CoGetPSClsid)(&psd_IID_Unknown1, NULL), E_INVALIDARG);
        EQHR(U(CoRegisterPSClsid)(NULL, &psd_CLSID_PSF), E_INVALIDARG);
        EQHR(U(CoRegisterPSClsid)(&psd_IID_Unknown1, NULL), E_INVALIDARG);
    }
    PSD_DONE();
}

TEST(psdr_getpsclsid_stock_interfaces) {
    PSD_STA();
    CLSID c; memset(&c, 0, sizeof c);
    GUID ps_dispatch = {0x00020420, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    EQHR(U(CoGetPSClsid)(&psd_IID_IDispatch, &c), S_OK);       /* PSDispatch */
    CHECK(IsEqualGUID(&c, &ps_dispatch));
    static const struct { const GUID *iid; const char *name; } t[] = {
        { &psd_IID_IUnknown, "IUnknown" }, { &psd_IID_ITypeInfo, "ITypeInfo" }, { &psd_IID_IMarshal, "IMarshal" },
        { &psd_IID_IStream, "IStream" }, { &psd_IID_IRpcChannelBuffer, "IRpcChannelBuffer" }, { &psd_IID_IPSFactoryBuffer, "IPSFactoryBuffer" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        memset(&c, 0, sizeof c);
        HRESULT hr = U(CoGetPSClsid)(t[i].iid, &c);
        t_note("CoGetPSClsid(%s) = 0x%08lx {%08lx-%04x}", t[i].name, hr, c.Data1, c.Data2);
    }
    PSD_DONE();
}

TEST(psdr_register_psclsid_roundtrip) {
    PSD_STA();
    /* process-local registration: overrides the (absent) registry mapping */
    GUID iid = psd_IID_Unknown2;
    CLSID got;
    EQHR(U(CoGetPSClsid)(&iid, &got), REGDB_E_IIDNOTREG);
    EQHR(U(CoRegisterPSClsid)(&iid, &psd_CLSID_Bogus), S_OK);
    memset(&got, 0, sizeof got);
    EQHR(U(CoGetPSClsid)(&iid, &got), S_OK);
    CHECK(IsEqualGUID(&got, &psd_CLSID_Bogus));
    EQHR(U(CoRegisterPSClsid)(&iid, &psd_CLSID_PSF), S_OK);    /* may be re-registered */
    memset(&got, 0, sizeof got);
    EQHR(U(CoGetPSClsid)(&iid, &got), S_OK);
    CHECK(IsEqualGUID(&got, &psd_CLSID_PSF));
    PSD_DONE();
}

TEST(psdr_cross_apartment_call) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !psd_register_ps(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    PsdSrv *srv = psd_srv_new();
    Work w;
    int ok = run_worker(&w, &psd_IID_One, 0, srv, PSD_SRV_UNK(srv));
    CHECK(ok);
    if (ok) {
        EQHR(w.hr_get, S_OK);
        EQHR(w.hr_call, S_OK);
        EQ(w.out, 42);
        EQHR(w.hr_call2, E_FAIL);                              /* server HRESULT travels back through the real channel */
        EQ(srv->n_add, 2);
        EQ(srv->last_tid, GetCurrentThreadId());               /* executed in the server's apartment (this STA) */
        CHECK(w.proxy_ptr != (void *)&srv->one);              /* the worker (MTA) got a proxy, not the object */
        EQ(w.proxy_vtbl, psd_proxy_vtbl[0]->Vtbl);             /* ... created by our IPSFactoryBuffer */
    }
    srv->one.lpVtbl->Release(&srv->one);
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdr_cross_apartment_two_interface) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !psd_register_ps(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    PsdSrv *srv = psd_srv_new();
    Work w;
    int ok = run_worker(&w, &psd_IID_Two, 3, srv, (IUnknown *)&srv->two);
    CHECK(ok);
    if (ok) {
        EQHR(w.hr_get, S_OK);
        EQHR(w.hr_call, S_OK); EQ(w.out, 77);
        EQHR(w.hr_call2, S_OK); EQ(w.out2, 6);
        CHECK(!lstrcmpW(srv->last_str, L"across"));
        EQ(w.proxy_vtbl, psd_proxy_vtbl[1]->Vtbl);
    }
    srv->one.lpVtbl->Release(&srv->one);
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdr_cross_apartment_interface_return) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !psd_register_ps(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    PsdSrv *srv = psd_srv_new();
    Work w;
    int ok = run_worker(&w, &psd_IID_Two, 2, srv, (IUnknown *)&srv->two);
    CHECK(ok);
    if (ok) {
        EQHR(w.hr_get, S_OK);
        EQHR(w.hr_call, S_OK);                                 /* GetObj returned an interface pointer through the channel */
        EQHR(w.hr_qi, S_OK);
        EQHR(w.hr_call2, S_OK);
        EQ(w.out2, 11);                                        /* the returned proxy is usable */
        EQ(srv->n_getobj, 1);
        EQ(srv->n_add, 1);
    }
    srv->one.lpVtbl->Release(&srv->one);
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdr_server_exception_becomes_serverfault) {
    if (t_is_wine()) { t_skip("exception in a server stub is not converted by wine"); return; }
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !psd_register_ps(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    PsdSrv *srv = psd_srv_new();
    srv->raise_in_add = TRUE;
    Work w;
    int ok = run_worker(&w, &psd_IID_One, 1, srv, PSD_SRV_UNK(srv));
    CHECK(ok);
    if (ok) {
        EQHR(w.hr_get, S_OK);
        EQHR(w.hr_call, (HRESULT)0x80010105);                  /* RPC_E_SERVERFAULT */
    }
    srv->one.lpVtbl->Release(&srv->one);
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdr_marshal_without_ps_registration) {
    PSD_STA();
    /* IID_Unknown1 has no proxy/stub registration: the standard marshaler cannot create a stub */
    PsdMock *m = psd_mock_new();
    m->only = 1; m->only_iid = psd_IID_Unknown1;
    IStream *stm = NULL;
    EQHR(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm), S_OK);
    HRESULT hr = U(CoMarshalInterface)(stm, &psd_IID_Unknown1, &m->iface, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    CHECK(FAILED(hr));
    t_note("CoMarshalInterface(unregistered proxy/stub IID) = 0x%08lx", hr);
    if (stm) IStream_Release(stm);
    psd_mock_free(m);
    PSD_DONE();
}
