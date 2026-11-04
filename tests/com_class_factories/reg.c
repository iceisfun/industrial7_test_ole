/* CoRegisterClassObject / CoRevokeClassObject / CoGetClassObject on privately registered factories. */
#include "cg.h"

#define STA() LONG live0_; do { if (!cg_api()) return; live0_ = cg_live_objects; EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK); } while (0)
#define MTA() do { if (!cg_api()) return; EQ(pCoInitializeEx(NULL, COINIT_MULTITHREADED), S_OK); } while (0)
#define UNINIT() pCoUninitialize()

static DWORD WINAPI th_get_sta(LPVOID p);
static DWORD WINAPI th_get_mta(LPVOID p);
/* --- argument validation --- */
TEST(cgreg_uninitialized_thread) {
    if (!cg_api()) return;
    CGFactory f; DWORD c = 0x1234; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), CG_E_NOTINIT);
    EQ(f.ref, 1);
    EQ(pCoRevokeClassObject(1), CG_E_NOTINIT);
    /* the uninitialized check precedes the NULL-pointer check */
    EQ(pCoRegisterClassObject(&cg_TC0, (IUnknown *)&f.iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, NULL), CG_E_NOTINIT);
}
TEST(cgreg_null_cookie_pointer) {
    STA();
    CGFactory f; cg_factory_init(&f, &cg_TC0);
    EQ(pCoRegisterClassObject(&cg_TC0, (IUnknown *)&f.iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, NULL), CG_E_INVALIDARG);
    EQ(f.ref, 1);
    UNINIT();
}
TEST(cgreg_null_unknown_zeroes_cookie) {
    STA();
    DWORD c = 0x1234;
    EQ(pCoRegisterClassObject(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), CG_E_INVALIDARG);
    EQ(c, 0);
    UNINIT();
}
TEST(cgreg_clsctx_validation_table) {
    STA();
    /* allowed bits are INPROC_SERVER(1), LOCAL_SERVER(4), INPROC_HANDLER16(8), REMOTE_SERVER(0x10); anything else is E_INVALIDARG */
    static const DWORD bad[] = { 0x2, 0x3, 0x20, 0x40, 0x80, 0x100, 0x400, 0x20000, 0x80000000u, CLSCTX_ALL /* 0x17 contains INPROC_HANDLER */ };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CGFactory f; DWORD c = 0x77; cg_factory_init(&f, &cg_TC0);
        HRESULT hr = cg_register(&f, bad[i], REGCLS_MULTIPLEUSE, &c);
        CHECKF(hr == CG_E_INVALIDARG, "clsctx 0x%lx: hr=0x%08lx", bad[i], hr);
        CHECKF(c == 0, "clsctx 0x%lx: cookie not zeroed (%lu)", bad[i], c);
        CHECKF(f.ref == 1, "clsctx 0x%lx: factory ref %ld", bad[i], f.ref);
        if (SUCCEEDED(hr)) pCoRevokeClassObject(c);
    }
    UNINIT();
}
TEST(cgreg_regcls_validation_table) {
    STA();
    /* bits above 0x10 are invalid; the four low bits all set (0xF) is an invalid combination */
    static const DWORD bad[] = { 0x20, 0x40, 0x80, 0x100, 0x8000000u, 0xF, 0x1F, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CGFactory f; DWORD c = 0x77; cg_factory_init(&f, &cg_TC0);
        HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, bad[i], &c);
        CHECKF(hr == CG_E_INVALIDARG, "regcls 0x%lx: hr=0x%08lx", bad[i], hr);
        CHECKF(c == 0, "regcls 0x%lx: cookie not zeroed (%lu)", bad[i], c);
        if (SUCCEEDED(hr)) pCoRevokeClassObject(c);
    }
    UNINIT();
}
TEST(cgreg_surrogate_requires_local_server) {
    STA();
    CGFactory f; DWORD c = 0x77; cg_factory_init(&f, &cg_TC0);
    HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_SURROGATE, &c);
    EQ(hr, CG_E_INVALIDARG); if (SUCCEEDED(hr)) pCoRevokeClassObject(c); else EQ(c, 0);
    hr = cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_SURROGATE | REGCLS_MULTIPLEUSE, &c);
    EQ(hr, CG_E_INVALIDARG); if (SUCCEEDED(hr)) pCoRevokeClassObject(c);
    UNINIT();
}
TEST(cgreg_suspended_requires_local_server) {
    STA();
    CGFactory f; DWORD c = 0x77; cg_factory_init(&f, &cg_TC0);
    HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_SUSPENDED, &c);
    t_note("INPROC_SERVER + REGCLS_SUSPENDED -> 0x%08lx", hr);
    if (SUCCEEDED(hr)) pCoRevokeClassObject(c);
    UNINIT();
}
TEST(cgreg_agile_requires_ftm) {
    STA();
    CGFactory f; DWORD c = 0x77; cg_factory_init(&f, &cg_TC0);
    /* REGCLS_AGILE demands that the object aggregate the free-threaded marshaler (IMarshal -> CLSID_InProcFreeMarshaler) */
    HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, CG_REGCLS_AGILE | REGCLS_MULTIPLEUSE, &c);
    EQ(hr, CG_E_INVALIDARG);
    if (SUCCEEDED(hr)) pCoRevokeClassObject(c); else { EQ(c, 0); EQ(f.ref, 1); }
    UNINIT();
}
TEST(cgreg_agile_with_ftm) {
    STA();
    __typeof__(&CoCreateFreeThreadedMarshaler) pFtm = U(CoCreateFreeThreadedMarshaler);
    if (!pFtm) { t_skip("CoCreateFreeThreadedMarshaler missing"); UNINIT(); return; }
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(pFtm((IUnknown *)&f.iface, &f.ftm), S_OK);
    CHECK(f.ftm != NULL);
    HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, CG_REGCLS_AGILE | REGCLS_MULTIPLEUSE, &c);
    EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(c != 0); EQ(pCoRevokeClassObject(c), S_OK); }
    cg_release(f.ftm);
    UNINIT();
}
TEST(cgreg_agile_ftm_visible_from_other_apartment) {
    /* an agile class registered from an STA is reachable from an MTA thread with CLSCTX_INPROC_SERVER */
    STA();
    __typeof__(&CoCreateFreeThreadedMarshaler) pFtm = U(CoCreateFreeThreadedMarshaler);
    if (!pFtm) { t_skip("no FTM"); UNINIT(); return; }
    static CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC1);
    pFtm((IUnknown *)&f.iface, &f.ftm);
    if (cg_register(&f, CLSCTX_INPROC_SERVER, CG_REGCLS_AGILE | REGCLS_MULTIPLEUSE, &c) == S_OK) {
        EQ((HRESULT)cg_run_thread(th_get_mta, (void *)&cg_TC1), S_OK);
        pCoRevokeClassObject(c);
    } else t_note("agile registration failed");
    cg_release(f.ftm);
    UNINIT();
}

/* --- register / get / revoke --- */
TEST(cgreg_inproc_each_regcls) {
    STA();
    static const DWORD modes[] = { REGCLS_SINGLEUSE, REGCLS_MULTIPLEUSE, REGCLS_MULTI_SEPARATE };
    for (unsigned i = 0; i < 3; i++) {
        CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
        HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, modes[i], &c);
        CHECKF(hr == S_OK, "regcls %lu hr=0x%08lx", modes[i], hr);
        CHECK(c != 0);
        CHECKF(f.ref > 1, "COM must hold a reference on the registered object (ref=%ld)", f.ref);
        IClassFactory *cf = NULL;
        hr = pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
        CHECKF(hr == S_OK, "regcls %lu get hr=0x%08lx", modes[i], hr);
        if (cf) cg_release((IUnknown *)cf);
        EQ(pCoRevokeClassObject(c), S_OK);
        EQ(f.ref, 1);
    }
    UNINIT();
}
TEST(cgreg_get_after_revoke_not_registered) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    EQ(pCoRevokeClassObject(c), S_OK);
    IClassFactory *cf = (IClassFactory *)(ULONG_PTR)1;
    HRESULT hr = pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
    EQ(hr, CG_E_CLASSNOTREG);
    EQ(cf, NULL);
    UNINIT();
}
TEST(cgreg_get_unregistered_class) {
    STA();
    IUnknown *u = (IUnknown *)(ULONG_PTR)1;
    HRESULT hr = pCoGetClassObject(&cg_TCUNREG, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u);
    EQ(hr, CG_E_CLASSNOTREG);
    EQ(u, NULL);
    u = (IUnknown *)(ULONG_PTR)1;
    hr = pCoGetClassObject(&cg_TCUNREG, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, NULL, &cg_IID_IClassFactory, (void **)&u);
    EQ(hr, CG_E_CLASSNOTREG);
    EQ(u, NULL);
    UNINIT();
}
TEST(cgreg_get_uninitialized) {
    if (!cg_api()) return;
    IUnknown *u = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u), CG_E_NOTINIT);
    EQ(u, NULL);
    u = (IUnknown *)(ULONG_PTR)1;
    pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u);
    if (u != NULL) t_note("CoGetClassObject leaves *ppv untouched when COM is not initialized");
    /* NULL ppv is rejected with E_INVALIDARG even though COM is uninitialized */
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, NULL), CG_E_INVALIDARG);
}
TEST(cgreg_get_null_ppv) {
    STA();
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, NULL), CG_E_INVALIDARG);
    UNINIT();
}
TEST(cgreg_get_interface_matrix) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    static const struct { const GUID *iid; HRESULT hr; } t[] = {
        { &cg_IID_IUnknown, S_OK }, { &cg_IID_IClassFactory, S_OK },
        { &cg_IID_IClassFactory2, E_NOINTERFACE }, { &cg_IID_IPrivate, E_NOINTERFACE }, { &cg_IID_IDispatch, E_NOINTERFACE },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, t[i].iid, &p);
        CHECKF(hr == t[i].hr, "row %u: hr=0x%08lx want 0x%08lx", i, hr, t[i].hr);
        if (FAILED(t[i].hr)) CHECKF(p == NULL, "row %u: *ppv=%p on failure", i, p);
        else { CHECK(p != NULL); cg_release(p); }
    }
    LONG before = f.ref;
    void *p = NULL;
    pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &p);
    t_note("same-apartment CoGetClassObject returns the raw registered pointer: %s", p == &f.iface ? "yes" : "no");
    cg_release(p);
    EQ(f.ref, before);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_get_requested_clsctx_must_match) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    IUnknown *u = NULL;
    /* an in-proc registration is not a local server: LOCAL_SERVER lookup goes to the (empty) registry */
    HRESULT hr = pCoGetClassObject(&cg_TC0, CLSCTX_LOCAL_SERVER, NULL, &cg_IID_IUnknown, (void **)&u);
    EQ(hr, CG_E_CLASSNOTREG);
    EQ(u, NULL);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_duplicate_registration_same_clsid) {
    STA();
    CGFactory f; DWORD c1 = 0, c2 = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c1), S_OK);
    HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c2);
    t_note("second registration of the same CLSID in one apartment -> 0x%08lx", hr);
    if (hr == S_OK) { CHECK(c2 != 0); CHECK(c1 != c2); EQ(pCoRevokeClassObject(c2), S_OK); }
    EQ(pCoRevokeClassObject(c1), S_OK);
    EQ(f.ref, 1);
    UNINIT();
}
TEST(cgreg_distinct_cookies) {
    STA();
    CGFactory f[4]; DWORD c[4];
    const GUID *g[4] = { &cg_TC0, &cg_TC1, &cg_TC2, &cg_TC3 };
    for (int i = 0; i < 4; i++) { cg_factory_init(&f[i], g[i]); EQ(cg_register(&f[i], CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c[i]), S_OK); CHECK(c[i] != 0); }
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) CHECK(c[i] != c[j]);
    for (int i = 0; i < 4; i++) {
        IClassFactory *cf = NULL; pCoGetClassObject(g[i], CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
        CHECK(cf != NULL);
        /* each CLSID gets its own factory */
        if (cf) { IUnknown *obj = NULL; EQ(cf->lpVtbl->CreateInstance(cf, NULL, &cg_IID_IUnknown, (void **)&obj), S_OK);
                  EQ(f[i].creates, 1); for (int j = 0; j < 4; j++) if (j != i) EQ(f[j].creates, 0 + (j < i));
                  cg_release(obj); cg_release((IUnknown *)cf); }
    }
    for (int i = 0; i < 4; i++) EQ(pCoRevokeClassObject(c[i]), S_OK);
    EQ(cg_live_objects, live0_);
    UNINIT();
}
TEST(cgreg_revoke_double) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    EQ(pCoRevokeClassObject(c), S_OK);
    HRESULT hr = pCoRevokeClassObject(c);
    /* Win10 22H2: revoking an unknown/already revoked cookie is a silent S_OK */
    EQ(hr, S_OK);
    t_note("double CoRevokeClassObject -> 0x%08lx", hr);
    EQ(f.ref, 1);
    UNINIT();
}
TEST(cgreg_revoke_bad_cookies) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK); /* make sure the class cache exists */
    static const DWORD offs[] = { 0, 1, 0x1000, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < 5; i++) {
        DWORD k = offs[i] ? (offs[i] > 0x10000 ? offs[i] : c + offs[i]) : 0;
        HRESULT hr = pCoRevokeClassObject(k);
        /* Win10 22H2: an unknown cookie is a silent S_OK, not an error */
        EQ(hr, S_OK);
        t_note("revoke bogus cookie 0x%lx -> 0x%08lx", k, hr);
    }
    /* the valid registration is unaffected */
    IUnknown *u = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u), S_OK);
    cg_release(u);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_registration_dies_with_apartment) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_SINGLEUSE, &c), S_OK);
    UNINIT();
    EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    IClassFactory *cf = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf), CG_E_CLASSNOTREG);
    EQ(f.ref, 1);   /* COM released its reference when the apartment went away */
    UNINIT();
}
TEST(cgreg_nested_init_keeps_registration) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_FALSE);
    UNINIT();   /* balances the nested init only */
    IUnknown *u = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u), S_OK);
    cg_release(u);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}

/* --- threads / apartments --- */
static DWORD WINAPI th_get_sta(LPVOID p) {
    IClassFactory *cf = NULL; GUID *g = p;
    HRESULT ih = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = pCoGetClassObject(g, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
    if (cf) cg_release((IUnknown *)cf);
    if (SUCCEEDED(ih)) pCoUninitialize();
    return (DWORD)hr;
}
static DWORD WINAPI th_get_mta(LPVOID p) {
    IClassFactory *cf = NULL; GUID *g = p;
    HRESULT ih = pCoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = pCoGetClassObject(g, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
    if (cf) cg_release((IUnknown *)cf);
    if (SUCCEEDED(ih)) pCoUninitialize();
    return (DWORD)hr;
}
static CGFactory g_thf; static DWORD g_thc;
static DWORD WINAPI th_reg_sta(LPVOID p) {
    DWORD c = 0; HRESULT ih = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = pCoRegisterClassObject(&cg_TC0, (IUnknown *)&g_thf.iface, CLSCTX_INPROC_SERVER, REGCLS_SINGLEUSE, &c);
    if (SUCCEEDED(ih)) pCoUninitialize();
    return (DWORD)hr;
}
static DWORD WINAPI th_revoke(LPVOID p) {
    HRESULT ih = pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = pCoRevokeClassObject((DWORD)(ULONG_PTR)p);
    if (SUCCEEDED(ih)) pCoUninitialize();
    return (DWORD)hr;
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(cgreg_inproc_registration_is_per_apartment) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_SINGLEUSE, &c), S_OK);
    EQ((HRESULT)cg_run_thread(th_get_sta, (void *)&cg_TC0), CG_E_CLASSNOTREG);
    HRESULT mh = cg_run_thread(th_get_mta, (void *)&cg_TC0);
    t_note("MTA thread lookup of an STA in-proc registration -> 0x%08lx", mh);
    CHECK(FAILED(mh));
    IUnknown *u = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u), S_OK);
    cg_release(u);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_same_clsid_other_apartment_ok) {
    STA();
    cg_factory_init(&g_thf, &cg_TC0);
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_SINGLEUSE, &c), S_OK);
    EQ((HRESULT)cg_run_thread(th_reg_sta, NULL), S_OK);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_revoke_from_other_thread) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    /* Win10 22H2: CoRevokeClassObject has no apartment-affinity check; the other thread's revoke returns S_OK
     * (the registration is not rejected with RPC_E_WRONG_THREAD as the original expectation assumed). */
    EQ((HRESULT)cg_run_thread(th_revoke, (void *)(ULONG_PTR)c), S_OK);
    t_note("CoRevokeClassObject from a thread of another apartment -> S_OK on Win10");
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_mta_registration_visible_to_mta_threads) {
    MTA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC2);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    EQ((HRESULT)cg_run_thread(th_get_mta, (void *)&cg_TC2), S_OK);
    EQ((HRESULT)cg_run_thread(th_get_sta, (void *)&cg_TC2), CG_E_CLASSNOTREG);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}

/* --- local server registrations, suspend / resume --- */
TEST(cgreg_local_server_roundtrip) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    CHECK(c != 0);
    IClassFactory *cf = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_LOCAL_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf), S_OK);
    if (cf) cg_release((IUnknown *)cf);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_local_server_each_regcls) {
    STA();
    static const DWORD modes[] = { REGCLS_SINGLEUSE, REGCLS_MULTIPLEUSE, REGCLS_MULTI_SEPARATE };
    for (unsigned i = 0; i < 3; i++) {
        CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
        HRESULT hr = cg_register(&f, CLSCTX_LOCAL_SERVER, modes[i], &c);
        CHECKF(hr == S_OK, "regcls %lu hr=0x%08lx", modes[i], hr);
        if (hr == S_OK) EQ(pCoRevokeClassObject(c), S_OK);
        EQ(f.ref, 1);
    }
    UNINIT();
}
TEST(cgreg_suspended_local_then_resume) {
    STA();
    __typeof__(&CoResumeClassObjects) pResume = U(CoResumeClassObjects);
    REQUIRE(pResume);
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &c), S_OK);
    CHECK(c != 0);
    IClassFactory *cf = NULL;
    HRESULT hr = pCoGetClassObject(&cg_TC0, CLSCTX_LOCAL_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf);
    t_note("local lookup of a REGCLS_SUSPENDED class in the registering process -> 0x%08lx", hr);
    if (cf) cg_release((IUnknown *)cf);
    EQ(pResume(), S_OK);
    cf = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_LOCAL_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf), S_OK);
    if (cf) cg_release((IUnknown *)cf);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_suspend_resume_without_registrations) {
    STA();
    __typeof__(&CoResumeClassObjects) pResume = U(CoResumeClassObjects);
    __typeof__(&CoSuspendClassObjects) pSuspend = U(CoSuspendClassObjects);
    REQUIRE(pResume); REQUIRE(pSuspend);
    EQ(pResume(), S_OK);
    EQ(pSuspend(), S_OK);
    EQ(pResume(), S_OK);
    UNINIT();
}
TEST(cgreg_suspend_resume_idempotent_with_class) {
    STA();
    __typeof__(&CoResumeClassObjects) pResume = U(CoResumeClassObjects);
    __typeof__(&CoSuspendClassObjects) pSuspend = U(CoSuspendClassObjects);
    REQUIRE(pResume); REQUIRE(pSuspend);
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    EQ(pSuspend(), S_OK);
    EQ(pSuspend(), S_OK);
    /* Win10 22H2: Resume after Suspend with a registered non-suspended LOCAL_SERVER class fails the SCM
     * re-registration with RPC_X_INVALID_BOUND (0x800706c6), every time; the registration is still revocable. */
    HRESULT rh1 = pResume(), rh2 = pResume();
    EQ(rh1, (HRESULT)0x800706c6); EQ(rh2, (HRESULT)0x800706c6);
    t_note("CoResumeClassObjects after double suspend -> 0x%08lx, 0x%08lx", rh1, rh2);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgreg_inproc_unaffected_by_suspend) {
    STA();
    __typeof__(&CoSuspendClassObjects) pSuspend = U(CoSuspendClassObjects);
    __typeof__(&CoResumeClassObjects) pResume = U(CoResumeClassObjects);
    REQUIRE(pSuspend); REQUIRE(pResume);
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    pSuspend();
    IUnknown *u = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IUnknown, (void **)&u), S_OK);
    cg_release(u);
    pResume();
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}

/* --- LockServer on the registered factory --- */
TEST(cgreg_lockserver_via_factory) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    IClassFactory *cf = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, (void **)&cf), S_OK);
    if (cf) {
        EQ(cf->lpVtbl->LockServer(cf, TRUE), S_OK);
        EQ(cf->lpVtbl->LockServer(cf, TRUE), S_OK);
        EQ(f.locks, 2);
        EQ(cf->lpVtbl->LockServer(cf, FALSE), S_OK);
        EQ(cf->lpVtbl->LockServer(cf, FALSE), S_OK);
        EQ(f.locks, 0);
        EQ(f.lockCalls, 2); EQ(f.unlockCalls, 2);
        cg_release((IUnknown *)cf);
    }
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
