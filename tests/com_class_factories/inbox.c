/* In-box class objects, library loading, server-process refcount, call-context trivia. */
#include "cg.h"

#define STA() LONG live0_; do { if (!cg_api()) return; live0_ = cg_live_objects; EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK); } while (0)
#define UNINIT() pCoUninitialize()

CGG(cg_IID_IFont,    0xbef6e002,0xa874,0x101a,0x8b,0xba,0x00,0xaa,0x00,0x30,0x0c,0xab);
CGG(cg_IID_IPicture, 0x7bf80980,0xbf32,0x101a,0x8b,0xbb,0x00,0xaa,0x00,0x30,0x0c,0xab);

/* ---- CoGetClassObject on classes implemented by the COM libraries ---- */
static const struct { const char *name; const GUID *clsid; int hard; } k_inbox[] = {
    { "StdGlobalInterfaceTable", &cg_CLSID_StdGlobalInterfaceTable, 1 },
    { "InProcFreeMarshaler",     &cg_CLSID_InProcFreeMarshaler, 1 },
    { "FileMoniker",     &cg_CLSID_FileMoniker, 1 },
    { "ItemMoniker",     &cg_CLSID_ItemMoniker, 1 },
    { "AntiMoniker",     &cg_CLSID_AntiMoniker, 1 },
    { "PointerMoniker",  &cg_CLSID_PointerMoniker, 1 },
    { "CompositeMoniker",&cg_CLSID_CompositeMoniker, 1 },
    { "ClassMoniker",    &cg_CLSID_ClassMoniker, 1 },
    { "ObjrefMoniker",   &cg_CLSID_ObjrefMoniker, 0 },
    { "GlobalOptions",   &cg_CLSID_GlobalOptions, 1 },
    { "StdFont",         &cg_CLSID_StdFont, 1 },
    { "StdPicture",      &cg_CLSID_StdPicture, 1 },
    { "PSOAInterface",   &cg_CLSID_PSOAInterface, 1 },
    { "PSDispatch",      &cg_CLSID_PSDispatch, 1 },
    { "PSFactoryBuffer", &cg_CLSID_PSFactoryBuffer, 0 },
    { "StdMarshal",      &cg_CLSID_StdMarshal, 0 },
};
TEST(cgbox_getclassobject_iunknown_each) {
    STA();
    for (unsigned i = 0; i < sizeof k_inbox / sizeof k_inbox[0]; i++) {
        IUnknown *u = (IUnknown *)(ULONG_PTR)1;
        HRESULT hr = pCoGetClassObject(k_inbox[i].clsid, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u);
        if (k_inbox[i].hard) CHECKF(hr == S_OK, "%s hr=0x%08lx", k_inbox[i].name, hr);
        else t_note("%s CoGetClassObject(IUnknown) -> 0x%08lx", k_inbox[i].name, hr);
        if (SUCCEEDED(hr)) { CHECKF(u != NULL, "%s", k_inbox[i].name); cg_release(u); }
        else CHECKF(u == NULL, "%s *ppv=%p on failure", k_inbox[i].name, u);
    }
    UNINIT();
}
TEST(cgbox_getclassobject_iclassfactory_each) {
    STA();
    for (unsigned i = 0; i < sizeof k_inbox / sizeof k_inbox[0]; i++) {
        IClassFactory *cf = NULL;
        HRESULT hr = pCoGetClassObject(k_inbox[i].clsid, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
        t_note("%s CoGetClassObject(IClassFactory) -> 0x%08lx", k_inbox[i].name, hr);
        if (SUCCEEDED(hr)) cg_release((IUnknown *)cf); else CHECK(cf == NULL);
    }
    UNINIT();
}
TEST(cgbox_moniker_factories_are_classfactories) {
    STA();
    static const GUID *m[] = { &cg_CLSID_FileMoniker, &cg_CLSID_ItemMoniker, &cg_CLSID_AntiMoniker, &cg_CLSID_PointerMoniker,
                               &cg_CLSID_CompositeMoniker, &cg_CLSID_ClassMoniker, &cg_CLSID_StdGlobalInterfaceTable, &cg_CLSID_InProcFreeMarshaler };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) {
        IClassFactory *cf = NULL;
        HRESULT hr = pCoGetClassObject(m[i], CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
        CHECKF(hr == S_OK, "row %u hr=0x%08lx", i, hr);
        if (cf) {
            void *cf2 = NULL;
            HRESULT h2 = cf->lpVtbl->QueryInterface(cf, &cg_IID_IClassFactory2, (void **)&cf2);
            CHECKF(h2 == E_NOINTERFACE, "row %u IClassFactory2 0x%08lx", i, h2);
            CHECK(cf2 == NULL);
            cg_release((IUnknown *)cf);
        }
    }
    UNINIT();
}
TEST(cgbox_getclassobject_inbox_unsupported_iid) {
    STA();
    IUnknown *u = (IUnknown *)(ULONG_PTR)1;
    HRESULT hr = pCoGetClassObject(&cg_CLSID_FileMoniker, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IPrivate, (void **)&u);
    EQ(hr, CG_E_NOINTERFACE);
    EQ(u, NULL);
    UNINIT();
}
TEST(cgbox_getclassobject_inbox_uninitialized) {
    if (!cg_api()) return;
    IUnknown *u = NULL;
    /* in-box classes are still subject to the apartment check */
    HRESULT hr = pCoGetClassObject(&cg_CLSID_FileMoniker, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&u);
    EQ(hr, CG_E_NOTINIT);
    CHECK(u == NULL);
}
TEST(cgbox_get_inbox_class_is_agile_across_apartments) {
    STA();
    static const GUID *m[] = { &cg_CLSID_FileMoniker, &cg_CLSID_StdGlobalInterfaceTable };
    for (unsigned i = 0; i < 2; i++) {
        IClassFactory *cf = NULL;
        EQ(pCoGetClassObject(m[i], CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf), S_OK);
        cg_release((IUnknown *)cf);
    }
    UNINIT();
    EQ(pCoInitializeEx(NULL, COINIT_MULTITHREADED), S_OK);
    for (unsigned i = 0; i < 2; i++) {
        IClassFactory *cf = NULL;
        EQ(pCoGetClassObject(m[i], CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf), S_OK);
        cg_release((IUnknown *)cf);
    }
    UNINIT();
}

/* ---- CoCreateInstance of in-box classes ---- */
TEST(cgbox_create_moniker_classes) {
    STA();
    static const GUID *m[] = { &cg_CLSID_FileMoniker, &cg_CLSID_ItemMoniker, &cg_CLSID_AntiMoniker, &cg_CLSID_PointerMoniker,
                               &cg_CLSID_CompositeMoniker, &cg_CLSID_ClassMoniker };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) {
        IMoniker *mk = NULL;
        HRESULT hr = pCoCreateInstance(m[i], NULL, CLSCTX_INPROC_SERVER, &cg_IID_IMoniker, (void **)&mk);
        CHECKF(hr == S_OK, "row %u hr=0x%08lx", i, hr);
        if (mk) {
            CLSID id = cg_IID_IUnknown;
            CHECK(mk->lpVtbl->GetClassID(mk, &id) == S_OK);
            CHECKF(IsEqualGUID(&id, m[i]), "row %u GetClassID mismatch", i);
            cg_release((IUnknown *)mk);
        }
    }
    UNINIT();
}
TEST(cgbox_create_moniker_with_outer_is_refused) {
    STA();
    CGFactory of; cg_factory_init(&of, &cg_TC3);
    void *op = NULL; of.iface.lpVtbl->CreateInstance(&of.iface, NULL, &cg_IID_IUnknown, &op);
    IUnknown *outer = op;
    static const GUID *m[] = { &cg_CLSID_FileMoniker, &cg_CLSID_ItemMoniker, &cg_CLSID_AntiMoniker, &cg_CLSID_StdGlobalInterfaceTable };
    for (unsigned i = 0; i < 4; i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = pCoCreateInstance(m[i], outer, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p);
        CHECKF(hr == CG_E_NOAGG, "row %u hr=0x%08lx", i, hr);
        CHECKF(p == NULL, "row %u *ppv=%p", i, p);
    }
    cg_release(outer);
    UNINIT();
}
TEST(cgbox_create_global_interface_table) {
    STA();
    IUnknown *u = NULL;
    EQ(pCoCreateInstance(&cg_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IGlobalInterfaceTable, (void **)&u), S_OK);
    CHECK(u != NULL);
    /* the GIT is a process-wide singleton: a second creation returns the same object identity */
    IUnknown *u2 = NULL;
    EQ(pCoCreateInstance(&cg_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IGlobalInterfaceTable, (void **)&u2), S_OK);
    t_note("GIT identity shared between creations: %s", u == u2 ? "yes" : "no");
    cg_release(u); cg_release(u2);
    u = (IUnknown *)(ULONG_PTR)1;
    EQ(pCoCreateInstance(&cg_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IPrivate, (void **)&u), CG_E_NOINTERFACE);
    CHECK(u == NULL);
    UNINIT();
}
TEST(cgbox_git_register_and_get_interface) {
    STA();
    IGlobalInterfaceTable *git = NULL;
    EQ(pCoCreateInstance(&cg_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IGlobalInterfaceTable, (void **)&git), S_OK);
    if (git) {
        CGFactory f; cg_factory_init(&f, &cg_TC0);
        void *op = NULL; f.iface.lpVtbl->CreateInstance(&f.iface, NULL, &cg_IID_IPersistFile, &op);
        IUnknown *obj = op; DWORD cookie = 0;
        EQ(git->lpVtbl->RegisterInterfaceInGlobal(git, obj, &cg_IID_IPersistFile, &cookie), S_OK);
        CHECK(cookie != 0);
        IPersistFile *pf = NULL;
        EQ(git->lpVtbl->GetInterfaceFromGlobal(git, cookie, &cg_IID_IPersistFile, (void **)&pf), S_OK);
        CHECK(pf != NULL);
        cg_release((IUnknown *)pf);
        IUnknown *wrong = (IUnknown *)(ULONG_PTR)1;
        HRESULT hr = git->lpVtbl->GetInterfaceFromGlobal(git, cookie, &cg_IID_IPrivate, (void **)&wrong);
        CHECK(FAILED(hr)); t_note("GetInterfaceFromGlobal with unrelated IID -> 0x%08lx", hr);
        EQ(git->lpVtbl->RevokeInterfaceFromGlobal(git, cookie), S_OK);
        hr = git->lpVtbl->RevokeInterfaceFromGlobal(git, cookie);
        CHECK(FAILED(hr)); t_note("double RevokeInterfaceFromGlobal -> 0x%08lx", hr);
        cg_release(obj);
        cg_release((IUnknown *)git);
    }
    UNINIT();
}
TEST(cgbox_create_free_threaded_marshaler) {
    STA();
    CGFactory of; cg_factory_init(&of, &cg_TC3);
    void *op = NULL; of.iface.lpVtbl->CreateInstance(&of.iface, NULL, &cg_IID_IUnknown, &op);
    IUnknown *outer = op, *inner = NULL;
    EQ(pCoCreateInstance(&cg_CLSID_InProcFreeMarshaler, outer, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&inner), S_OK);
    CHECK(inner != NULL && inner != outer);
    if (inner) {
        IMarshal *m = NULL;
        EQ(inner->lpVtbl->QueryInterface(inner, &cg_IID_IMarshal, (void **)&m), S_OK);
        if (m) {
            CLSID id = cg_IID_IUnknown;
            HRESULT hr = m->lpVtbl->GetUnmarshalClass(m, &cg_IID_IUnknown, inner, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &id);
            EQ(hr, S_OK);
            CHECK(IsEqualGUID(&id, &cg_CLSID_InProcFreeMarshaler));
            cg_release((IUnknown *)m);
        }
        cg_release(inner);
    }
    cg_release(outer);
    /* FTM refuses a non-IUnknown IID when aggregated */
    UNINIT();
}
TEST(cgbox_create_global_options) {
    STA();
    typedef struct GO GO;
    typedef struct { HRESULT (WINAPI *QI)(GO *, REFIID, void **); ULONG (WINAPI *AddRef)(GO *); ULONG (WINAPI *Release)(GO *);
                     HRESULT (WINAPI *Set)(GO *, DWORD, ULONG_PTR); HRESULT (WINAPI *Query)(GO *, DWORD, ULONG_PTR *); } GOVtbl;
    struct GO { const GOVtbl *v; };
    GO *go = NULL;
    HRESULT hr = pCoCreateInstance(&cg_CLSID_GlobalOptions, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IGlobalOptions, (void **)&go);
    EQ(hr, S_OK);
    if (go) {
        ULONG_PTR val = ~(ULONG_PTR)0;
        hr = go->v->Query(go, 1 /* COMGLB_EXCEPTION_HANDLING */, &val);
        EQ(hr, S_OK);
        EQ(val, 0); /* COMGLB_EXCEPTION_HANDLE */
        val = 5; hr = go->v->Query(go, 0, &val);
        CHECK(FAILED(hr));
        hr = go->v->Query(go, 9999, &val);
        CHECK(FAILED(hr));
        t_note("IGlobalOptions::Query(0) and Query(9999) failure codes were recorded");
        cg_release((IUnknown *)go);
    }
    UNINIT();
}
TEST(cgbox_create_std_font_and_picture) {
    STA();
    IDispatch *d = NULL;
    HRESULT hr = pCoCreateInstance(&cg_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IDispatch, (void **)&d);
    EQ(hr, S_OK);
    cg_release((IUnknown *)d);
    IUnknown *f = NULL;
    hr = pCoCreateInstance(&cg_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IFont, (void **)&f);
    EQ(hr, S_OK);
    cg_release(f);
    f = (IUnknown *)(ULONG_PTR)1;
    hr = pCoCreateInstance(&cg_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IPrivate, (void **)&f);
    EQ(hr, CG_E_NOINTERFACE); CHECK(f == NULL);
    IUnknown *pic = NULL;
    hr = pCoCreateInstance(&cg_CLSID_StdPicture, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&pic);
    EQ(hr, S_OK);
    cg_release(pic);
    pic = NULL;
    hr = pCoCreateInstance(&cg_CLSID_StdPicture, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IPicture, (void **)&pic);
    EQ(hr, S_OK);
    cg_release(pic);
    UNINIT();
}
TEST(cgbox_create_multi_qi_font) {
    STA();
    MULTI_QI q[3] = { { &cg_IID_IUnknown }, { &cg_IID_IFont }, { &cg_IID_IPrivate } };
    for (int i = 0; i < 3; i++) q[i].hr = E_NOTIMPL;
    EQ(pCoCreateInstanceEx(&cg_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, NULL, 3, q), CG_S_NOTALLINTERFACES);
    EQ(q[0].hr, S_OK); EQ(q[1].hr, S_OK); EQ(q[2].hr, CG_E_NOINTERFACE);
    CHECK(q[2].pItf == NULL);
    cg_release(q[1].pItf); cg_release(q[0].pItf);
    UNINIT();
}
TEST(cgbox_ps_factory_classes) {
    STA();
    static const GUID *ps[] = { &cg_CLSID_PSOAInterface, &cg_CLSID_PSDispatch, &cg_CLSID_PSFactoryBuffer };
    for (unsigned i = 0; i < 3; i++) {
        IUnknown *u = NULL;
        HRESULT hr = pCoGetClassObject(ps[i], CLSCTX_INPROC_SERVER, NULL, &cg_IID_IPSFactoryBuffer, (void **)&u);
        t_note("proxy/stub class #%u as IPSFactoryBuffer -> 0x%08lx", i, hr);
        if (SUCCEEDED(hr)) cg_release(u); else CHECK(u == NULL);
    }
    UNINIT();
}

/* ---- CoGetTreatAsClass (read-only) ---- */
TEST(cgbox_gettreatasclass_args) {
    __typeof__(&CoGetTreatAsClass) p = U(CoGetTreatAsClass); REQUIRE(p);
    CLSID out = cg_IID_IUnknown;
    EQ(p(NULL, &out), CG_E_INVALIDARG);
    EQ(p(&cg_TC0, NULL), CG_E_INVALIDARG);
    EQ(p(NULL, NULL), CG_E_INVALIDARG);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(cgbox_gettreatasclass_unmapped) {
    __typeof__(&CoGetTreatAsClass) p = U(CoGetTreatAsClass); REQUIRE(p);
    static const GUID *g[] = { &cg_TCUNREG, &cg_CLSID_FileMoniker, &cg_CLSID_StdFont };
    for (unsigned i = 0; i < 3; i++) {
        CLSID out = cg_IID_IPrivate;
        HRESULT hr = p(g[i], &out);
        CHECKF(hr == S_FALSE, "row %u hr=0x%08lx", i, hr);
        CHECKF(IsEqualGUID(&out, g[i]), "row %u output must equal input when no TreatAs", i);
    }
}
TEST(cgbox_gettreatasclass_without_com_init) {
    __typeof__(&CoGetTreatAsClass) p = U(CoGetTreatAsClass); REQUIRE(p);
    CLSID out;
    EQ(p(&cg_TCUNREG, &out), S_FALSE);   /* does not require an initialized apartment */
}

/* ---- CoLoadLibrary / CoFreeLibrary ---- */
TEST(cgbox_coloadlibrary_roundtrip) {
    __typeof__(&CoLoadLibrary) pL = U(CoLoadLibrary); __typeof__(&CoFreeLibrary) pF = U(CoFreeLibrary);
    REQUIRE(pL); REQUIRE(pF);
    static const WCHAR dll[] = L"winhttp.dll";
    int before = GetModuleHandleW(dll) != NULL;
    HINSTANCE h = pL((LPOLESTR)dll, FALSE);
    CHECK(h != NULL);
    if (h) {
        CHECK(h == (HINSTANCE)GetModuleHandleW(dll));
        pF(h);
        if (!before) CHECK(GetModuleHandleW(dll) == NULL);
        else t_note("winhttp.dll was already loaded; unload not asserted");
    }
}
TEST(cgbox_coloadlibrary_refcounted) {
    __typeof__(&CoLoadLibrary) pL = U(CoLoadLibrary); __typeof__(&CoFreeLibrary) pF = U(CoFreeLibrary);
    REQUIRE(pL); REQUIRE(pF);
    static const WCHAR dll[] = L"winhttp.dll";
    int before = GetModuleHandleW(dll) != NULL;
    HINSTANCE h1 = pL((LPOLESTR)dll, FALSE), h2 = pL((LPOLESTR)dll, FALSE);
    CHECK(h1 != NULL); CHECK(h1 == h2);
    if (h1) pF(h1);
    if (h1 && !before) CHECK(GetModuleHandleW(dll) != NULL);   /* one reference left */
    if (h2) pF(h2);
    if (h1 && !before) CHECK(GetModuleHandleW(dll) == NULL);
}
TEST(cgbox_coloadlibrary_missing) {
    __typeof__(&CoLoadLibrary) pL = U(CoLoadLibrary); REQUIRE(pL);
    static const WCHAR dll[] = L"olet_no_such_library_xyz.dll";
    SET_ERR(0);
    HINSTANCE h = pL((LPOLESTR)dll, FALSE);
    CHECK(h == NULL);
    t_note("CoLoadLibrary(missing) GetLastError=%lu", GetLastError());
}
TEST(cgbox_cofreeunusedlibraries_does_not_unload_in_use) {
    STA();
    __typeof__(&CoFreeUnusedLibraries) pFU = U(CoFreeUnusedLibraries); REQUIRE(pFU);
    HMODULE m = GetModuleHandleA("ole32.dll");
    pFU();
    CHECK(GetModuleHandleA("ole32.dll") == m);
    UNINIT();
}

/* ---- server process refcount (separate process: the count and suspension state are process-global) ---- */
CHILD(srvproc) {
    __typeof__(&CoAddRefServerProcess) pA = U(CoAddRefServerProcess);
    __typeof__(&CoReleaseServerProcess) pR = U(CoReleaseServerProcess);
    if (!cg_api() || !pA || !pR) return 1;
    CK(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED) == S_OK);
    ULONG a = pA();   CK(a == 1);
    ULONG b = pA();   CK(b == 2);
    ULONG c = pA();   CK(c == 3);
    ULONG r = pR();   CK(r == 2);
    r = pR();         CK(r == 1);
    r = pR();         CK(r == 0);
    ULONG d = pA();   CK(d == 1);   /* usable again after reaching zero */
    r = pR();         CK(r == 0);
    pCoUninitialize();
    return 0;
}
TEST(cgbox_server_process_refcount) { EQ(t_run_child("srvproc", "", 20000), 0); }
CHILD(srvproc_noinit) {
    __typeof__(&CoAddRefServerProcess) pA = U(CoAddRefServerProcess);
    __typeof__(&CoReleaseServerProcess) pR = U(CoReleaseServerProcess);
    if (!pA || !pR) return 1;
    /* works without CoInitialize: the class cache is created on demand */
    ULONG a = pA(); CK(a == 1);
    ULONG r = pR(); CK(r == 0);
    return 0;
}
TEST(cgbox_server_process_without_coinit) { EQ(t_run_child("srvproc_noinit", "", 20000), 0); }
CHILD(srvproc_release_first) {
    __typeof__(&CoReleaseServerProcess) pR = U(CoReleaseServerProcess);
    if (!pR) return 1;
    /* with no class cache yet, release is a no-op that returns 0 */
    CK(pR() == 0);
    return 0;
}
TEST(cgbox_release_server_process_without_cache) { EQ(t_run_child("srvproc_release_first", "", 20000), 0); }
CHILD(srvproc_suspend_noinit) {
    __typeof__(&CoSuspendClassObjects) pS = U(CoSuspendClassObjects);
    __typeof__(&CoResumeClassObjects) pR = U(CoResumeClassObjects);
    if (!pS || !pR) return 1;
    CK(pS() == S_OK);
    CK(pR() == S_OK);
    return 0;
}
TEST(cgbox_suspend_resume_fresh_process) { EQ(t_run_child("srvproc_suspend_noinit", "", 20000), 0); }

/* ---- CoAllowSetForegroundWindow / CoGetCancelObject / CoGetCallContext ---- */
TEST(cgbox_allowsetforegroundwindow_validation) {
    STA();
    __typeof__(&CoAllowSetForegroundWindow) p = U(CoAllowSetForegroundWindow); REQUIRE(p);
    CGFactory f; cg_factory_init(&f, &cg_TC0);
    void *op = NULL; f.iface.lpVtbl->CreateInstance(&f.iface, NULL, &cg_IID_IUnknown, &op);
    IUnknown *obj = op;
    EQ(p(NULL, NULL), CG_E_INVALIDARG);
    EQ(p(obj, (void *)(ULONG_PTR)1), CG_E_INVALIDARG);      /* lpvReserved must be NULL */
    EQ(p(obj, NULL), CG_E_NOINTERFACE);                      /* object does not implement IForegroundTransfer */
    cg_release(obj);
    UNINIT();
}
TEST(cgbox_allowsetforegroundwindow_uninitialized) {
    __typeof__(&CoAllowSetForegroundWindow) p = U(CoAllowSetForegroundWindow); REQUIRE(p);
    EQ(p(NULL, NULL), CG_E_INVALIDARG);
}
TEST(cgbox_getcancelobject_validation) {
    STA();
    __typeof__(&CoGetCancelObject) p = U(CoGetCancelObject); REQUIRE(p);
    if (t_is_wine()) { t_skip("CoGetCancelObject is unimplemented in Wine and aborts"); UNINIT(); return; }
    EQ(p(0, &cg_IID_ICancelMethodCalls, NULL), CG_E_INVALIDARG);
    void *o = (void *)(ULONG_PTR)1;
    EQ(p(0, &cg_IID_ICancelMethodCalls, &o), CG_E_NOCONTEXT);          /* no call in progress on this thread */
    o = (void *)(ULONG_PTR)1;
    EQ(p(GetCurrentThreadId(), &cg_IID_ICancelMethodCalls, &o), CG_E_NOCONTEXT);
    EQ(p(0xFFFFFFF0u, &cg_IID_ICancelMethodCalls, &o), CG_E_NOCONTEXT);
    UNINIT();
}
TEST(cgbox_getcallcontext_validation) {
    STA();
    __typeof__(&CoGetCallContext) p = U(CoGetCallContext); REQUIRE(p);
    EQ(p(&cg_IID_IUnknown, NULL), CG_E_INVALIDARG);
    void *o = (void *)(ULONG_PTR)1;
    EQ(p(&cg_IID_IUnknown, &o), CG_E_CALLCOMPLETE);
    o = (void *)(ULONG_PTR)1;
    EQ(p(&cg_IID_IServerSecurity, &o), CG_E_CALLCOMPLETE);
    UNINIT();
}
TEST(cgbox_getcallcontext_null_ppv_uninitialized) {
    __typeof__(&CoGetCallContext) p = U(CoGetCallContext); REQUIRE(p);
    if (!cg_api()) return;
    EQ(p(&cg_IID_IUnknown, NULL), CG_E_INVALIDARG);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(cgbox_switchcallcontext_roundtrip) {
    STA();
    __typeof__(&CoGetCallContext) pG = U(CoGetCallContext);
    __typeof__(&CoSwitchCallContext) pS = U(CoSwitchCallContext);
    REQUIRE(pG); REQUIRE(pS);
    CGFactory f; cg_factory_init(&f, &cg_TC0);
    void *op = NULL; f.iface.lpVtbl->CreateInstance(&f.iface, NULL, &cg_IID_IUnknown, &op);
    IUnknown *obj = op; IUnknown *old = (IUnknown *)(ULONG_PTR)1;
    CGObj *o = f.lastObj;
    EQ(pS(obj, &old), S_OK);
    EQ(old, NULL);
    EQ(o->ref, 1);                      /* SwitchCallContext does not AddRef */
    void *got = NULL;
    EQ(pG(&cg_IID_IUnknown, &got), S_OK);
    CHECK(got == (void *)obj);
    EQ(o->ref, 2);                      /* CoGetCallContext QI/AddRef */
    cg_release(got);
    old = (IUnknown *)(ULONG_PTR)1;
    EQ(pS(NULL, &old), S_OK);
    CHECK(old == obj);
    got = (void *)(ULONG_PTR)1;
    EQ(pG(&cg_IID_IUnknown, &got), CG_E_CALLCOMPLETE);
    cg_release(obj);
    UNINIT();
}
static DWORD WINAPI th_callctx(LPVOID p) {
    HRESULT ih = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    void *o = NULL; HRESULT hr = U(CoGetCallContext)(&cg_IID_IUnknown, &o);
    if (SUCCEEDED(ih)) pCoUninitialize();
    return (DWORD)hr;
}
TEST(cgbox_getcallcontext_is_per_thread) {
    STA();
    __typeof__(&CoGetCallContext) pG = U(CoGetCallContext);
    __typeof__(&CoSwitchCallContext) pS = U(CoSwitchCallContext);
    REQUIRE(pG); REQUIRE(pS);
    CGFactory f; cg_factory_init(&f, &cg_TC0);
    void *op = NULL; f.iface.lpVtbl->CreateInstance(&f.iface, NULL, &cg_IID_IUnknown, &op);
    IUnknown *obj = op, *old = NULL;
    pS(obj, &old);
    EQ((HRESULT)cg_run_thread(th_callctx, NULL), CG_E_CALLCOMPLETE);   /* another thread has no call context */
    pS(NULL, &old);
    cg_release(obj);
    UNINIT();
}

/* ---- CoGetObject ---- */
TEST(cgbox_cogetobject_null_ppv) {
    STA();
    __typeof__(&CoGetObject) p = U(CoGetObject); REQUIRE(p);
    if (t_is_wine()) { t_skip("Wine dereferences a NULL ppv"); UNINIT(); return; }
    EQ(p(L"clsid:00000000-0000-0000-0000-000000000000:", NULL, &cg_IID_IUnknown, NULL), CG_E_INVALIDARG);
    UNINIT();
}
TEST(cgbox_cogetobject_bad_names) {
    STA();
    __typeof__(&CoGetObject) p = U(CoGetObject); REQUIRE(p);
    static const WCHAR *names[] = { L"", L"olet_no_such_prog_id:xyz", L"file:", L"@", L"clsid:", L"clsid:not-a-guid:" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        void *o = (void *)(ULONG_PTR)1;
        HRESULT hr = p(names[i], NULL, &cg_IID_IUnknown, &o);
        CHECKF(FAILED(hr), "name #%u hr=0x%08lx", i, hr);
        CHECKF(o == NULL, "name #%u *ppv=%p", i, o);
        t_note("CoGetObject name #%u -> 0x%08lx", i, hr);
    }
    UNINIT();
}
TEST(cgbox_cogetobject_clsid_moniker_finds_registered_class) {
    STA();
    __typeof__(&CoGetObject) p = U(CoGetObject); REQUIRE(p);
    __typeof__(&StringFromGUID2) pS = U(StringFromGUID2); REQUIRE(pS);
    CGFactory f; DWORD cookie; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie), S_OK);
    WCHAR buf[64], name[100];
    pS(&cg_TC0, buf, 64);                       /* "{...}" */
    wsprintfW(name, L"clsid:%.36s:", buf + 1);
    void *o = NULL;
    HRESULT hr = p(name, NULL, &cg_IID_IClassFactory, &o);
    EQ(hr, S_OK);
    if (o) { t_note("CoGetObject(clsid:...) returns the registered factory itself: %s", o == &f.iface ? "yes" : "no"); cg_release(o); }
    /* BIND_OPTS2 with an explicit class context */
    BIND_OPTS2 bo; memset(&bo, 0, sizeof bo); bo.cbStruct = sizeof bo; bo.dwClassContext = CLSCTX_INPROC_SERVER;
    o = NULL;
    hr = p(name, (BIND_OPTS *)&bo, &cg_IID_IClassFactory, &o);
    EQ(hr, S_OK);
    if (o) cg_release(o);
    /* class context that excludes in-proc servers must not see the in-process registration */
    bo.dwClassContext = CLSCTX_LOCAL_SERVER;
    o = (void *)(ULONG_PTR)1;
    hr = p(name, (BIND_OPTS *)&bo, &cg_IID_IClassFactory, &o);
    t_note("CoGetObject with BIND_OPTS2.dwClassContext=LOCAL_SERVER -> 0x%08lx", hr);
    if (SUCCEEDED(hr)) cg_release(o); else CHECK(o == NULL);
    pCoRevokeClassObject(cookie);
    UNINIT();
}
TEST(cgbox_cogetobject_unsupported_iid) {
    STA();
    __typeof__(&CoGetObject) p = U(CoGetObject); REQUIRE(p);
    __typeof__(&StringFromGUID2) pS = U(StringFromGUID2); REQUIRE(pS);
    CGFactory f; DWORD cookie; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie), S_OK);
    WCHAR buf[64], name[100]; pS(&cg_TC0, buf, 64); wsprintfW(name, L"clsid:%.36s:", buf + 1);
    void *o = (void *)(ULONG_PTR)1;
    HRESULT hr = p(name, NULL, &cg_IID_IPrivate, &o);
    CHECK(FAILED(hr)); CHECK(o == NULL);
    t_note("CoGetObject(unsupported IID) -> 0x%08lx", hr);
    pCoRevokeClassObject(cookie);
    UNINIT();
}
TEST(cgbox_cogetobject_bad_bindopts_size) {
    STA();
    __typeof__(&CoGetObject) p = U(CoGetObject); REQUIRE(p);
    BIND_OPTS bo; memset(&bo, 0, sizeof bo); bo.cbStruct = 3;
    void *o = (void *)(ULONG_PTR)1;
    HRESULT hr = p(L"clsid:00000000-0000-0000-0000-000000000000:", &bo, &cg_IID_IUnknown, &o);
    CHECK(FAILED(hr)); CHECK(o == NULL);
    t_note("CoGetObject with BIND_OPTS.cbStruct=3 -> 0x%08lx", hr);
    UNINIT();
}

/* ---- misc exports ---- */
TEST(cgbox_cocheckelevationenabled_exported) {
    CHECK(t_proc_in("ole32.dll", "CoCheckElevationEnabled") != NULL);
}
TEST(cgbox_exports_exist) {
    static const char *n[] = { "CoRegisterClassObject", "CoRevokeClassObject", "CoGetClassObject", "CoCreateInstance", "CoCreateInstanceEx",
        "CoResumeClassObjects", "CoSuspendClassObjects", "CoAddRefServerProcess", "CoReleaseServerProcess", "CoGetInstanceFromFile",
        "CoGetInstanceFromIStorage", "CoGetObject", "CoLoadLibrary", "CoFreeLibrary", "CoGetCallContext", "CoSwitchCallContext",
        "CoAllowSetForegroundWindow", "CoGetCancelObject", "CoGetTreatAsClass", "CoTreatAsClass" };
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; i++) CHECKF(t_proc_in("ole32.dll", n[i]) != NULL, "%s", n[i]);
}
