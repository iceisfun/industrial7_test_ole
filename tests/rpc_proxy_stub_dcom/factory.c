/* rpcrt4 NdrDllGetClassObject / NdrDllCanUnloadNow / IPSFactoryBuffer (CStdPSFactoryBuffer) over a hand-built ProxyFileInfo.
 * Expectations come from the Win10 decomp of combase!NdrOleDllGetClassObject and from Wine's cstub.c (validated on Windows). */
#include "psd.h"

#define NEED_INIT() do { if (!psd_init()) { t_skip("rpcrt4 proxy/stub exports missing"); return; } \
                         if (!UIN("rpcrt4.dll", NdrDllGetClassObject)) { t_skip("no NdrDllGetClassObject"); return; } } while (0)

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdf_getclassobject_clsid) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = (IPSFactoryBuffer *)1;
    EQ(psd_psf.RefCount, 0);
    HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_PSF, &psd_IID_IPSFactoryBuffer, (void **)&psf, psd_file_list, &psd_CLSID_PSF, &psd_psf);
    EQHR(hr, S_OK);
    CHECK(psf != NULL && psf != (IPSFactoryBuffer *)1);
    EQ(psf, &psd_psf);                                  /* the caller supplied factory object is returned */
    EQ(psd_psf.pProxyFileList, psd_file_list);          /* and records the ProxyFileInfo list */
    EQ(psd_psf.RefCount, 1);
    if (psf && psf != (IPSFactoryBuffer *)1) EQ(IPSFactoryBuffer_Release(psf), 0);
    EQ(psd_psf.RefCount, 0);
    PSD_DONE();
}

TEST(psdf_getclassobject_unknown_clsid) {
    NEED_INIT(); PSD_STA();
    void *pv = (void *)1;
    HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_Bogus, &psd_IID_IPSFactoryBuffer, &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
    EQHR(hr, CLASS_E_CLASSNOTAVAILABLE);
    CHECK(pv == NULL);                                  /* *ppv is always cleared on failure */
    EQ(psd_psf.RefCount, 0);
    /* same with no factory CLSID supplied: only interface ids in the file match */
    pv = (void *)1;
    hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_Bogus, &psd_IID_IPSFactoryBuffer, &pv, psd_file_list, NULL, &psd_psf);
    EQHR(hr, CLASS_E_CLASSNOTAVAILABLE);
    CHECK(pv == NULL);
    PSD_DONE();
}

TEST(psdf_getclassobject_by_iid) {
    NEED_INIT(); PSD_STA();
    const GUID *ids[2] = { &psd_IID_One, &psd_IID_Two };
    for (int withclsid = 0; withclsid < 2; withclsid++)
        for (int i = 0; i < 2; i++) {
            IPSFactoryBuffer *psf = NULL;
            HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(ids[i], &psd_IID_IPSFactoryBuffer, (void **)&psf, psd_file_list, withclsid ? &psd_CLSID_PSF : NULL, &psd_psf);
            CHECKF(hr == S_OK, "iid %d withclsid %d hr=0x%lx", i, withclsid, hr);
            if (hr == S_OK) { EQ(psd_psf.RefCount, 1); IPSFactoryBuffer_Release(psf); }
            EQ(psd_psf.RefCount, 0);
        }
    PSD_DONE();
}

TEST(psdf_getclassobject_iid_not_in_file) {
    NEED_INIT(); PSD_STA();
    const GUID *ids[3] = { &psd_IID_IDispatch, &psd_IID_Unknown1, &psd_IID_IUnknown };
    for (int i = 0; i < 3; i++) {
        void *pv = (void *)1;
        HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(ids[i], &psd_IID_IPSFactoryBuffer, &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
        CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE, "id %d hr=0x%lx", i, hr);
        CHECK(pv == NULL);
    }
    EQ(psd_psf.RefCount, 0);
    PSD_DONE();
}

TEST(psdf_getclassobject_riid) {
    NEED_INIT(); PSD_STA();
    /* the requested riid is satisfied through the factory's QueryInterface */
    const GUID *ok_ids[2] = { &psd_IID_IUnknown, &psd_IID_IPSFactoryBuffer };
    for (int i = 0; i < 2; i++) {
        void *pv = NULL;
        HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_PSF, ok_ids[i], &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
        EQHR(hr, S_OK);
        EQ(pv, &psd_psf);
        EQ(psd_psf.RefCount, 1);
        if (pv) IUnknown_Release((IUnknown *)pv);
    }
    const GUID *bad[3] = { &psd_IID_IDispatch, &psd_IID_IRpcStubBuffer, &psd_IID_Unknown1 };
    for (int i = 0; i < 3; i++) {
        void *pv = (void *)1;
        HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_PSF, bad[i], &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
        CHECKF(hr == E_NOINTERFACE, "bad %d hr=0x%lx", i, hr);
        CHECK(pv == NULL);
        EQ(psd_psf.RefCount, 0);
    }
    void *pv = NULL;
    HRESULT hr = UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_PSF, &psd_IID_IClassFactory, &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
    t_note("NdrDllGetClassObject(riid=IClassFactory) = 0x%08lx pv=%p", hr, pv);
    if (hr == S_OK && pv) IUnknown_Release((IUnknown *)pv);
    PSD_DONE();
}

TEST(psdf_factory_refcounts_qi) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    EQ(psd_psf.RefCount, 1);
    EQ(IPSFactoryBuffer_AddRef(psf), 2);
    EQ(IPSFactoryBuffer_AddRef(psf), 3);
    EQ(psd_psf.RefCount, 3);
    EQ(IPSFactoryBuffer_Release(psf), 2);
    void *pv = NULL;
    EQHR(IPSFactoryBuffer_QueryInterface(psf, &psd_IID_IUnknown, &pv), S_OK);
    EQ(pv, psf); EQ(psd_psf.RefCount, 3);
    EQ(IUnknown_Release((IUnknown *)pv), 2);
    pv = NULL;
    EQHR(IPSFactoryBuffer_QueryInterface(psf, &psd_IID_IPSFactoryBuffer, &pv), S_OK);
    EQ(pv, psf);
    EQ(IPSFactoryBuffer_Release(psf), 2);
    pv = (void *)1;
    EQHR(IPSFactoryBuffer_QueryInterface(psf, &psd_IID_IDispatch, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    pv = (void *)1;
    EQHR(IPSFactoryBuffer_QueryInterface(psf, &psd_IID_IRpcProxyBuffer, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    EQ(psd_psf.RefCount, 2);
    EQ(IPSFactoryBuffer_Release(psf), 1);
    EQ(IPSFactoryBuffer_Release(psf), 0);
    PSD_DONE();
}

TEST(psdf_can_unload_now) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    HRESULT hr = UIN("rpcrt4.dll", NdrDllCanUnloadNow)(&psd_psf);
    EQHR(hr, S_FALSE);                                  /* a factory reference is outstanding */
    IPSFactoryBuffer_Release(psf);
    hr = UIN("rpcrt4.dll", NdrDllCanUnloadNow)(&psd_psf);
    EQHR(hr, S_OK);
    /* a live stub keeps the factory alive */
    PsdRig r;
    if (psd_rig_up(&r, &psd_IID_One, 0)) {
        IPSFactoryBuffer_Release(r.psf); r.psf = NULL;   /* drop the rig's own factory reference */
        hr = UIN("rpcrt4.dll", NdrDllCanUnloadNow)(&psd_psf);
        EQHR(hr, S_FALSE);
        t_note("factory RefCount with live stub+proxy = %ld", (long)psd_psf.RefCount);
        psd_rig_down(&r);
        EQHR(UIN("rpcrt4.dll", NdrDllCanUnloadNow)(&psd_psf), S_OK);
    }
    PSD_DONE();
}

TEST(psdf_stub_vtbl_populated) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    for (int i = 0; i < 2; i++) {
        const IRpcStubBufferVtbl *v = &psd_stub_vtbl[i]->Vtbl;
        CHECKF(v->QueryInterface && v->AddRef && v->Release && v->Connect && v->Disconnect && v->Invoke &&
               v->IsIIDSupported && v->CountRefs && v->DebugServerQueryInterface && v->DebugServerRelease, "stub vtbl %d has a NULL slot", i);
        CHECK(psd_stub_vtbl[i]->header.piid == (i ? &psd_IID_Two : &psd_IID_One));
        EQ(psd_stub_vtbl[i]->header.DispatchTableCount, i ? 7 : 4);
    }
    for (int i = 0; i < 2; i++) {
        void **v = psd_proxy_vtbl[i]->Vtbl;
        int n = i ? 7 : 4;
        for (int j = 0; j < n; j++) CHECKF(v[j] != NULL, "proxy %d slot %d NULL", i, j);
        for (int j = 3; j < n; j++) if (v[j] == (void *)-1) t_note("proxy %d slot %d still (void*)-1 after NdrDllGetClassObject", i, j);
        CHECK(psd_proxy_vtbl[i]->header.piid == (i ? &psd_IID_Two : &psd_IID_One));
    }
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdf_create_proxy_basic) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    for (int i = 0; i < 2; i++) {
        IRpcProxyBuffer *pb = NULL; IUnknown *iface = NULL;
        HRESULT hr = IPSFactoryBuffer_CreateProxy(psf, NULL, i ? &psd_IID_Two : &psd_IID_One, &pb, (void **)&iface);
        EQHR(hr, S_OK);
        if (hr != S_OK) continue;
        CHECK(pb != NULL && iface != NULL);
        EQ(*(void **)iface, psd_proxy_vtbl[i]->Vtbl);   /* the interface pointer's vtable is the proxy vtbl from the file */
        EQ(IUnknown_Release(iface), 1);                   /* proxy buffer and interface share one count: it was 2 */
        EQ(IRpcProxyBuffer_Release(pb), 0);
    }
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdf_create_proxy_outer_unknown) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    PsdMock *outer = psd_mock_new();
    outer->ref = 4;
    IRpcProxyBuffer *pb = NULL; IUnknown *iface = NULL;
    HRESULT hr = IPSFactoryBuffer_CreateProxy(psf, &outer->iface, &psd_IID_One, &pb, (void **)&iface);
    EQHR(hr, S_OK);
    if (hr == S_OK) {
        EQ(outer->ref, 5);                                /* the proxy holds one reference on the controlling unknown */
        EQ(*(void **)iface, psd_proxy_vtbl[0]->Vtbl);
        EQ(IUnknown_Release(iface), 4);                    /* interface refcounting is delegated to the outer unknown */
        EQ(outer->ref, 4);
        EQ(IRpcProxyBuffer_Release(pb), 0);
        EQ(outer->ref, 4);
    }
    psd_mock_free(outer);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psdf_create_proxy_unknown_iid) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    const GUID *bad[3] = { &psd_IID_Unknown1, &psd_IID_IDispatch, &psd_IID_IUnknown };
    for (int i = 0; i < 3; i++) {
        IRpcProxyBuffer *pb = NULL; void *iface = NULL;
        HRESULT hr = IPSFactoryBuffer_CreateProxy(psf, NULL, bad[i], &pb, &iface);
        CHECKF(hr == E_NOINTERFACE, "bad %d hr=0x%lx", i, hr);
        CHECK(pb == NULL && iface == NULL);
        if (pb) IRpcProxyBuffer_Release(pb);
    }
    EQ(psd_psf.RefCount, 1);                              /* a failed CreateProxy leaks no factory reference */
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdf_create_stub_basic) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    PsdMock *srv = psd_mock_new();
    IRpcStubBuffer *sb = NULL;
    HRESULT hr = IPSFactoryBuffer_CreateStub(psf, &psd_IID_One, &srv->iface, &sb);
    EQHR(hr, S_OK);
    if (hr == S_OK && sb) {
        CStdStubBuffer *cs = (CStdStubBuffer *)sb;
        EQ(cs->RefCount, 1);
        EQ(cs->lpVtbl, &psd_stub_vtbl[0]->Vtbl);
        EQ(srv->n_qi, 1);                                  /* the stub QueryInterface()s the server for the stub's own IID */
        CHECK(IsEqualGUID(&srv->last_iid, &psd_IID_One));
        EQ(cs->pvServerObject, &srv->iface);               /* QI result is kept as the server object */
        EQ(srv->ref, 2);
        CHECK(cs->pPSFactory != NULL);
        EQ(psd_psf.RefCount, 2);                           /* and a reference on the factory */
        IRpcStubBuffer_Disconnect(sb);
        EQ(srv->ref, 1);
        EQ(IRpcStubBuffer_Release(sb), 0);
        EQ(psd_psf.RefCount, 1);
    }
    psd_mock_free(srv);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

/* adapted from Wine dlls/rpcrt4/tests/cstub.c (LGPL-2.1+) */
TEST(psdf_create_stub_qi_fails) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    PsdMock *srv = psd_mock_new();
    srv->qi_hr = E_NOINTERFACE; srv->qi_out = NULL;
    IRpcStubBuffer *sb = NULL;
    HRESULT hr = IPSFactoryBuffer_CreateStub(psf, &psd_IID_One, &srv->iface, &sb);
    EQHR(hr, E_NOINTERFACE);
    CHECK(sb == NULL);
    EQ(srv->n_qi, 1);
    EQ(psd_psf.RefCount, 1);
    psd_mock_free(srv);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psdf_create_stub_unknown_iid) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    const GUID *bad[3] = { &psd_IID_Unknown1, &psd_IID_IDispatch, &psd_IID_IUnknown };
    for (int i = 0; i < 3; i++) {
        PsdMock *srv = psd_mock_new();
        IRpcStubBuffer *sb = NULL;
        HRESULT hr = IPSFactoryBuffer_CreateStub(psf, bad[i], &srv->iface, &sb);
        CHECKF(hr == E_NOINTERFACE, "bad %d hr=0x%lx", i, hr);
        CHECK(sb == NULL);
        EQ(srv->n_qi, 0);                                  /* the IID is rejected before the server is queried */
        psd_mock_free(srv);
    }
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psdf_create_stub_null_server) {
    if (t_is_wine()) { t_skip("wine dereferences a NULL server"); return; }
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    IRpcStubBuffer *sb = NULL;
    HRESULT hr = IPSFactoryBuffer_CreateStub(psf, &psd_IID_One, NULL, &sb);
    t_note("CreateStub(punkServer=NULL) = 0x%08lx stub=%p", hr, sb);
    if (hr == S_OK && sb) {
        EQ(IRpcStubBuffer_CountRefs(sb), 0);               /* nothing connected */
        EQ(IRpcStubBuffer_Release(sb), 0);
    }
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psdf_create_proxy_stub_many) {
    NEED_INIT(); PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    EQHR(psd_get_factory(&psf), S_OK);
    if (!psf) { PSD_DONE(); return; }
    enum { N = 64 };
    IRpcProxyBuffer *pb[N]; IUnknown *ifc[N]; IRpcStubBuffer *sb[N]; PsdMock *m[N];
    int bad = 0;
    for (int i = 0; i < N; i++) {
        m[i] = psd_mock_new();
        const GUID *iid = (i & 1) ? &psd_IID_Two : &psd_IID_One;
        if (IPSFactoryBuffer_CreateProxy(psf, NULL, iid, &pb[i], (void **)&ifc[i]) != S_OK) bad++;
        if (IPSFactoryBuffer_CreateStub(psf, iid, &m[i]->iface, &sb[i]) != S_OK) bad++;
    }
    EQ(bad, 0);
    EQ(psd_psf.RefCount >= 1 + N, 1);                     /* every stub holds a factory reference (proxies may too) */
    t_note("factory RefCount with %d stubs + %d proxies = %ld", N, N, (long)psd_psf.RefCount);
    for (int i = 0; i < N; i++) {
        if (bad) break;
        IUnknown_Release(ifc[i]); IRpcProxyBuffer_Release(pb[i]);
        IRpcStubBuffer_Disconnect(sb[i]); IRpcStubBuffer_Release(sb[i]);
        EQ(m[i]->ref, 1);
        psd_mock_free(m[i]);
    }
    if (!bad) EQ(psd_psf.RefCount, 1);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}
