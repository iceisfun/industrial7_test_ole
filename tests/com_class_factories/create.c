/* CoCreateInstance / CoCreateInstanceEx / CoGetInstanceFromFile / CoGetInstanceFromIStorage on private factories. */
#include "cg.h"

#define STA() do { if (!cg_api()) return; EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK); } while (0)
#define UNINIT() pCoUninitialize()
#define REG(f, clsid, ctx) do { cg_factory_init(&(f), &(clsid)); EQ(cg_register(&(f), (ctx), REGCLS_MULTIPLEUSE, &cookie), S_OK); } while (0)
#define UNREG() pCoRevokeClassObject(cookie)

static IUnknown *make_outer(CGFactory *of) {
    void *p = NULL;
    cg_factory_init(of, &cg_TC3);
    of->iface.lpVtbl->CreateInstance(&of->iface, NULL, &cg_IID_IUnknown, &p);
    return p;
}

/* ---- CoCreateInstance ---- */
TEST(cgci_null_ppv) {
    if (!cg_api()) return;
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, NULL), CG_E_INVALIDARG);   /* uninitialized: still E_INVALIDARG */
    STA();
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, NULL), CG_E_INVALIDARG);
    UNINIT();
}
TEST(cgci_uninitialized) {
    if (!cg_api()) return;
    void *p = (void *)(ULONG_PTR)1;
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p), CG_E_NOTINIT);
    EQ(p, NULL);          /* *ppv is always written (NULL) once the pointer itself is valid */
}
TEST(cgci_unregistered_class) {
    STA();
    void *p = (void *)(ULONG_PTR)1;
    EQ(pCoCreateInstance(&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p), CG_E_CLASSNOTREG);
    EQ(p, NULL);
    p = (void *)(ULONG_PTR)1;
    EQ(pCoCreateInstance(&cg_TCUNREG, NULL, CLSCTX_ALL & ~CLSCTX_REMOTE_SERVER, &cg_IID_IUnknown, &p), CG_E_CLASSNOTREG);
    EQ(p, NULL);
    UNINIT();
}
TEST(cgci_basic_and_iid_forwarding) {
    STA();
    CGFactory f; DWORD cookie;
    REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    static const GUID *iids[] = { &cg_IID_IUnknown, &cg_IID_IPersistFile, &cg_IID_IPersist, &cg_IID_IPersistStorage };
    for (unsigned i = 0; i < 4; i++) {
        void *p = NULL; LONG before = f.creates;
        HRESULT hr = pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, iids[i], &p);
        CHECKF(hr == S_OK, "iid #%u hr=0x%08lx", i, hr);
        CHECK(p != NULL);
        EQ(f.creates, before + 1);
        CHECK(IsEqualGUID(&f.lastIid, iids[i]));   /* the requested IID is handed to IClassFactory::CreateInstance as is */
        CHECK(f.lastOuter == NULL);
        cg_release(p);
    }
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgci_unsupported_iid) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    void *p = (void *)(ULONG_PTR)1;
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IPrivate, &p), CG_E_NOINTERFACE);
    EQ(p, NULL);
    EQ(f.creates, 1);
    CHECK(IsEqualGUID(&f.lastIid, &cg_IID_IPrivate));
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgci_factory_failure_is_passed_through) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    static const HRESULT hrs[] = { E_OUTOFMEMORY, E_FAIL, E_NOTIMPL, (HRESULT)0x8004ABCD, (HRESULT)0x80070005 };
    for (unsigned i = 0; i < 5; i++) {
        void *p = (void *)(ULONG_PTR)1; f.createHr = hrs[i];
        HRESULT hr = pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p);
        CHECKF(hr == hrs[i], "forced 0x%08lx got 0x%08lx", hrs[i], hr);
        CHECK(p == NULL);
    }
    UNREG(); UNINIT();
}
TEST(cgci_aggregation_supported) {
    STA();
    CGFactory f, of; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    IUnknown *outer = make_outer(&of); CHECK(outer != NULL);
    IUnknown *inner = NULL;
    EQ(pCoCreateInstance(&cg_TC0, outer, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&inner), S_OK);
    CHECK(inner != NULL);
    CHECK(f.lastOuter == outer);
    CHECK(inner != outer);
    /* the returned inner IUnknown is non-delegating: QI(IUnknown) on it returns itself */
    if (inner) { IUnknown *u = NULL; EQ(inner->lpVtbl->QueryInterface(inner, &cg_IID_IUnknown, (void **)&u), S_OK); CHECK(u == inner); cg_release(u); }
    cg_release(inner); cg_release(outer);
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgci_aggregation_refused) {
    STA();
    CGFactory f, of; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    f.aggregable = 0;
    IUnknown *outer = make_outer(&of);
    void *p = (void *)(ULONG_PTR)1;
    EQ(pCoCreateInstance(&cg_TC0, outer, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p), CG_E_NOAGG);
    EQ(p, NULL);
    EQ(f.creates, 1);
    cg_release(outer);
    UNREG(); UNINIT();
}
TEST(cgci_aggregation_needs_iunknown_iid) {
    STA();
    CGFactory f, of; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    IUnknown *outer = make_outer(&of);
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = pCoCreateInstance(&cg_TC0, outer, CLSCTX_INPROC_SERVER, &cg_IID_IPersistFile, &p);
    CHECKF(hr == CG_E_NOAGG || hr == CG_E_INVALIDARG, "hr=0x%08lx", hr);
    EQ(p, NULL);
    t_note("aggregation + non-IUnknown IID -> 0x%08lx, factory CreateInstance calls=%ld", hr, f.creates);
    /* a factory that does not police this rule shows whether COM itself rejects the combination */
    f.strictOuterIid = 0; f.creates = 0; p = NULL;
    hr = pCoCreateInstance(&cg_TC0, outer, CLSCTX_INPROC_SERVER, &cg_IID_IPersistFile, &p);
    t_note("lenient factory: hr=0x%08lx creates=%ld", hr, f.creates);
    if (SUCCEEDED(hr)) cg_release(p);
    cg_release(outer);
    UNREG(); UNINIT();
}
TEST(cgci_local_server_in_process) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC1, CLSCTX_LOCAL_SERVER);
    void *p = NULL;
    HRESULT hr = pCoCreateInstance(&cg_TC1, NULL, CLSCTX_LOCAL_SERVER, &cg_IID_IUnknown, &p);
    EQ(hr, S_OK);
    t_note("local-server registration used through CoCreateInstance: creates=%ld", f.creates);
    cg_release(p);
    UNREG(); UNINIT();
}
TEST(cgci_clsctx_modifier_flags) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    static const struct { const char *n; DWORD flag; int must; } t[] = {
        { "INPROC_HANDLER", CLSCTX_INPROC_HANDLER, 1 },
        { "NO_CODE_DOWNLOAD", CG_CLSCTX_NO_CODE_DOWNLOAD, 1 },
        { "ENABLE_CODE_DOWNLOAD", CG_CLSCTX_ENABLE_CODE_DOWNLOAD, 1 },
        { "NO_FAILURE_LOG", CG_CLSCTX_NO_FAILURE_LOG, 1 },
        { "ENABLE_CLOAKING", CG_CLSCTX_ENABLE_CLOAKING, 1 },
        { "FROM_DEFAULT_CONTEXT", CG_CLSCTX_FROM_DEFAULT_CONTEXT, 1 },
        { "DISABLE_AAA", CG_CLSCTX_DISABLE_AAA, 0 },
        { "ENABLE_AAA", CG_CLSCTX_ENABLE_AAA, 0 },
        { "ACTIVATE_AAA_AS_IU", CG_CLSCTX_ACTIVATE_AAA_AS_IU, 0 },
        { "ACTIVATE_32_BIT_SERVER", CG_CLSCTX_ACTIVATE_32_BIT_SERVER, 0 },
        { "ACTIVATE_64_BIT_SERVER", CG_CLSCTX_ACTIVATE_64_BIT_SERVER, 0 },
        { "ACTIVATE_ARM32_SERVER", CG_CLSCTX_ACTIVATE_ARM32_SERVER, 0 },
        { "NO_CUSTOM_MARSHAL", CG_CLSCTX_NO_CUSTOM_MARSHAL, 0 },
        { "APPCONTAINER", CG_CLSCTX_APPCONTAINER, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        void *p = NULL;
        HRESULT hr = pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER | t[i].flag, &cg_IID_IUnknown, &p);
        if (t[i].must) CHECKF(hr == S_OK, "INPROC_SERVER|%s hr=0x%08lx", t[i].n, hr);
        else t_note("INPROC_SERVER|%s -> 0x%08lx", t[i].n, hr);
        if (SUCCEEDED(hr)) cg_release(p);
    }
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgci_clsctx_without_server_bits) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    static const DWORD v[] = { 0, CLSCTX_REMOTE_SERVER, CG_CLSCTX_NO_CODE_DOWNLOAD, CG_CLSCTX_FROM_DEFAULT_CONTEXT };
    for (unsigned i = 0; i < 4; i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = pCoCreateInstance(&cg_TC0, NULL, v[i], &cg_IID_IUnknown, &p);
        t_note("CoCreateInstance clsctx=0x%lx -> 0x%08lx", v[i], hr);
        if (FAILED(hr)) CHECK(p == NULL); else cg_release(p);
    }
    UNREG(); UNINIT();
}
TEST(cgci_released_object_refcounts) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    LONG base = f.ref;
    IUnknown *u = NULL; IPersistFile *pf = NULL;
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&u), S_OK);
    EQ(f.ref, base);                       /* the factory reference taken for the call is dropped again */
    CGObj *o = f.lastObj;
    EQ(o->ref, 1);
    EQ(u->lpVtbl->QueryInterface(u, &cg_IID_IPersistFile, (void **)&pf), S_OK);
    EQ(o->ref, 2);
    cg_release((IUnknown *)pf); EQ(o->ref, 1);
    cg_release(u);
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
/* ---- CoCreateInstanceEx / MULTI_QI ---- */
TEST(cgcie_argument_validation) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 0, &q), CG_E_INVALIDARG);
    EQ(pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 1, NULL), CG_E_INVALIDARG);
    EQ(f.creates, 0);
    UNREG(); UNINIT();
}
TEST(cgcie_uninitialized) {
    if (!cg_api()) return;
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    HRESULT hr = pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &q);
    EQ(hr, CG_E_NOTINIT);
    CHECK(q.pItf == NULL);
    t_note("MULTI_QI.hr after CO_E_NOTINITIALIZED = 0x%08lx", q.hr);
}
TEST(cgcie_unregistered_sets_every_entry) {
    STA();
    MULTI_QI q[2] = { { &cg_IID_IUnknown, (IUnknown *)(ULONG_PTR)1, S_OK }, { &cg_IID_IPersistFile, (IUnknown *)(ULONG_PTR)1, S_OK } };
    q[0].pItf = q[1].pItf = NULL;
    EQ(pCoCreateInstanceEx(&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, NULL, 2, q), CG_E_CLASSNOTREG);
    for (int i = 0; i < 2; i++) { CHECK(q[i].pItf == NULL); EQ(q[i].hr, CG_E_CLASSNOTREG); }
    UNINIT();
}
TEST(cgcie_all_interfaces) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q[3] = { { &cg_IID_IUnknown }, { &cg_IID_IPersistFile }, { &cg_IID_IPersistStorage } };
    for (int i = 0; i < 3; i++) q[i].hr = E_NOTIMPL;
    EQ(pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 3, q), S_OK);
    for (int i = 0; i < 3; i++) { CHECK(q[i].pItf != NULL); EQ(q[i].hr, S_OK); }
    EQ(f.creates, 1);
    CHECK(IsEqualGUID(&f.lastIid, &cg_IID_IUnknown));     /* the factory sees the first IID only; the rest are QIs */
    EQ(cg_live_objects, 1);
    /* all three interface pointers belong to one object */
    IUnknown *id[3] = { NULL, NULL, NULL };
    for (int i = 0; i < 3; i++) if (q[i].pItf) q[i].pItf->lpVtbl->QueryInterface(q[i].pItf, &cg_IID_IUnknown, (void **)&id[i]);
    CHECK(id[0] == id[1] && id[1] == id[2]);
    for (int i = 0; i < 3; i++) { cg_release(id[i]); cg_release(q[i].pItf); }
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgcie_partial_interfaces) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q[3] = { { &cg_IID_IUnknown }, { &cg_IID_IPrivate }, { &cg_IID_IPersistFile } };
    for (int i = 0; i < 3; i++) q[i].hr = E_NOTIMPL;
    HRESULT hr = pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 3, q);
    EQ(hr, CG_S_NOTALLINTERFACES);
    CHECK(q[0].pItf != NULL); EQ(q[0].hr, S_OK);
    CHECK(q[1].pItf == NULL); EQ(q[1].hr, CG_E_NOINTERFACE);
    CHECK(q[2].pItf != NULL); EQ(q[2].hr, S_OK);
    cg_release(q[0].pItf); cg_release(q[2].pItf);
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgcie_no_interface_at_all) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q[2] = { { &cg_IID_IPrivate }, { &cg_IID_IClassFactory2 } };
    for (int i = 0; i < 2; i++) q[i].hr = S_OK;
    HRESULT hr = pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 2, q);
    EQ(hr, CG_E_NOINTERFACE);
    for (int i = 0; i < 2; i++) { CHECK(q[i].pItf == NULL); EQ(q[i].hr, CG_E_NOINTERFACE); }
    CHECK(IsEqualGUID(&f.lastIid, &cg_IID_IPrivate));
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(cgcie_first_iid_goes_to_factory) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q = { &cg_IID_IMoniker, NULL, S_OK };
    EQ(pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &q), CG_E_NOINTERFACE);
    CHECK(IsEqualGUID(&f.lastIid, &cg_IID_IMoniker));
    CHECK(q.pItf == NULL); EQ(q.hr, CG_E_NOINTERFACE);
    UNREG(); UNINIT();
}
TEST(cgcie_first_entry_unsupported_second_supported) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q[2] = { { &cg_IID_IPrivate }, { &cg_IID_IUnknown } };
    HRESULT hr = pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 2, q);
    t_note("mixed (unsupported, supported) MULTI_QI -> 0x%08lx; q0.hr=0x%08lx q1.hr=0x%08lx; factory creates=%ld", hr, q[0].hr, q[1].hr, f.creates);
    for (int i = 0; i < 2; i++) cg_release(q[i].pItf);
    UNREG(); UNINIT();
}
TEST(cgcie_unregistered_single) {
    STA();
    MULTI_QI q = { &cg_IID_IUnknown, NULL, S_OK };
    EQ(pCoCreateInstanceEx(&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &q), CG_E_CLASSNOTREG);
    CHECK(q.pItf == NULL); EQ(q.hr, CG_E_CLASSNOTREG);
    UNINIT();
}
TEST(cgcie_factory_failure_marks_entries) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    f.createHr = (HRESULT)0x8004ABCD;
    MULTI_QI q[2] = { { &cg_IID_IUnknown, NULL, S_OK }, { &cg_IID_IPersistFile, NULL, S_OK } };
    EQ(pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 2, q), (HRESULT)0x8004ABCD);
    for (int i = 0; i < 2; i++) { CHECK(q[i].pItf == NULL); t_note("entry %d hr after factory failure 0x8004ABCD: 0x%08lx", i, q[i].hr); CHECK(FAILED(q[i].hr)); }
    UNREG(); UNINIT();
}
TEST(cgcie_aggregation_single_iunknown) {
    STA();
    CGFactory f, of; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    IUnknown *outer = make_outer(&of);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(pCoCreateInstanceEx(&cg_TC0, outer, CLSCTX_INPROC_SERVER, NULL, 1, &q), S_OK);
    EQ(q.hr, S_OK); CHECK(q.pItf != NULL); CHECK(f.lastOuter == outer);
    cg_release(q.pItf);
    /* aggregation with a non-IUnknown first IID */
    MULTI_QI q2 = { &cg_IID_IPersistFile, NULL, E_NOTIMPL };
    HRESULT hr = pCoCreateInstanceEx(&cg_TC0, outer, CLSCTX_INPROC_SERVER, NULL, 1, &q2);
    CHECKF(hr == CG_E_NOAGG || hr == CG_E_INVALIDARG, "hr=0x%08lx", hr);
    CHECK(q2.pItf == NULL);
    cg_release(outer);
    UNREG(); UNINIT();
}
TEST(cgcie_aggregation_multiple_iids) {
    STA();
    CGFactory f, of; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    IUnknown *outer = make_outer(&of);
    MULTI_QI q[2] = { { &cg_IID_IUnknown }, { &cg_IID_IPersistFile } };
    q[0].hr = q[1].hr = E_NOTIMPL;
    HRESULT hr = pCoCreateInstanceEx(&cg_TC0, outer, CLSCTX_INPROC_SERVER, NULL, 2, q);
    t_note("aggregated CoCreateInstanceEx with 2 IIDs -> 0x%08lx (q0.hr=0x%08lx q1.hr=0x%08lx)", hr, q[0].hr, q[1].hr);
    for (int i = 1; i >= 0; i--) cg_release(q[i].pItf);   /* delegating interfaces first, the inner unknown last */
    cg_release(outer);
    UNREG(); UNINIT();
}
TEST(cgcie_local_server_in_process) {
    STA();
    CGFactory f; DWORD cookie; REG(f, cg_TC1, CLSCTX_LOCAL_SERVER);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(pCoCreateInstanceEx(&cg_TC1, NULL, CLSCTX_LOCAL_SERVER, NULL, 1, &q), S_OK);
    EQ(q.hr, S_OK); cg_release(q.pItf);
    UNREG(); UNINIT();
}
TEST(cgcie_fromapp_unregistered) {
    STA();
    typedef HRESULT (WINAPI *CCIFA)(REFCLSID, IUnknown *, DWORD, void *, ULONG, MULTI_QI *);
    CCIFA p = (CCIFA)t_proc("CoCreateInstanceFromApp");
    REQUIRE(p);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, S_OK };
    EQ(p(&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &q), CG_E_CLASSNOTREG);
    UNINIT();
}
TEST(cgcie_fromapp_registered_class_requires_registry) {
    /* CoCreateInstanceFromApp ignores classes registered with CoRegisterClassObject only when it targets the app-container
     * catalogue; record what a process-private registration does. */
    STA();
    typedef HRESULT (WINAPI *CCIFA)(REFCLSID, IUnknown *, DWORD, void *, ULONG, MULTI_QI *);
    CCIFA p = (CCIFA)t_proc("CoCreateInstanceFromApp");
    REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, S_OK };
    HRESULT hr = p(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &q);
    t_note("CoCreateInstanceFromApp on a CoRegisterClassObject class -> 0x%08lx", hr);
    if (SUCCEEDED(hr)) cg_release(q.pItf);
    UNREG(); UNINIT();
}

/* ---- CoGetInstanceFromFile ---- */
UDECL(HRESULT, CoGetInstanceFromFile, (COSERVERINFO *, CLSID *, IUnknown *, DWORD, DWORD, OLECHAR *, DWORD, MULTI_QI *));
UDECL(HRESULT, CoGetInstanceFromIStorage, (COSERVERINFO *, CLSID *, IUnknown *, DWORD, IStorage *, DWORD, MULTI_QI *));
static const WCHAR kName[] = L"dummypath";

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(cgfile_missing_file_no_clsid) {
    STA();
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    HRESULT hr = p(NULL, NULL, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 1, &q);
    EQ(hr, (HRESULT)0x800401EA);   /* MK_E_CANTOPENFILE */
    CHECK(q.pItf == NULL); EQ(q.hr, CG_E_NOINTERFACE);
    UNINIT();
}
TEST(cgfile_class_not_registered) {
    STA();
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(p(NULL, (CLSID *)&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 1, &q), CG_E_CLASSNOTREG);
    CHECK(q.pItf == NULL); EQ(q.hr, CG_E_CLASSNOTREG);
    UNINIT();
}
TEST(cgfile_pitf_must_be_null) {
    STA();
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q = { &cg_IID_IUnknown, (IUnknown *)(ULONG_PTR)0xdeadbeef, S_OK };
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 1, &q), CG_E_INVALIDARG);
    EQ(q.pItf, 0xdeadbeef); EQ(q.hr, S_OK);
    q.hr = E_NOTIMPL;
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 1, &q), CG_E_INVALIDARG);
    EQ(q.pItf, 0xdeadbeef); EQ(q.hr, E_NOTIMPL);
    EQ(f.creates, 0);
    UNREG(); UNINIT();
}
TEST(cgfile_load_called_with_name_and_mode) {
    STA();
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    static const DWORD modes[] = { STGM_READ, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, STGM_READ | STGM_SHARE_DENY_WRITE };
    for (unsigned i = 0; i < 3; i++) {
        MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
        EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, modes[i], (OLECHAR *)kName, 1, &q), S_OK);
        EQ(q.hr, S_OK); CHECK(q.pItf != NULL);
        CGObj *o = f.lastObj;
        if (o) { EQ(o->fileLoaded, 1); CHECK(!lstrcmpW(o->file, kName)); CHECKF(o->fmode == modes[i], "mode 0x%lx got 0x%lx", modes[i], o->fmode); }
        cg_release(q.pItf);
    }
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgfile_multi_qi) {
    STA();
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q[2] = { { &cg_IID_IPersistFile }, { &cg_IID_IPrivate } };
    q[0].hr = q[1].hr = E_NOTIMPL;
    HRESULT hr = p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 2, q);
    EQ(hr, CG_S_NOTALLINTERFACES);
    CHECK(q[0].pItf != NULL); EQ(q[0].hr, S_OK);
    CHECK(q[1].pItf == NULL); EQ(q[1].hr, CG_E_NOINTERFACE);
    cg_release(q[0].pItf);
    UNREG(); UNINIT();
}
TEST(cgfile_load_failure_propagates) {
    STA();
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    f.pfLoadHr = (HRESULT)0x8000fefe;
    MULTI_QI q[2] = { { &cg_IID_IUnknown, NULL, S_OK }, { &cg_IID_IUnknown, NULL, S_OK } };
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 2, q), (HRESULT)0x8000fefe);
    for (int i = 0; i < 2; i++) { CHECK(q[i].pItf == NULL); EQ(q[i].hr, (HRESULT)0x8000fefe); }
    EQ(cg_live_objects, 0);
    UNREG(); UNINIT();
}
TEST(cgfile_uninitialized) {
    if (!cg_api()) return;
    CoGetInstanceFromFile_pfn p = UN(CoGetInstanceFromFile); REQUIRE(p);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    HRESULT hr = p(NULL, (CLSID *)&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, STGM_READ, (OLECHAR *)kName, 1, &q);
    CHECK(FAILED(hr)); CHECK(q.pItf == NULL);
    t_note("CoGetInstanceFromFile before CoInitialize -> 0x%08lx (q.hr 0x%08lx)", hr, q.hr);
}

/* ---- CoGetInstanceFromIStorage ---- */
static IStorage *mem_storage(const GUID *cls) {
    __typeof__(&StgCreateDocfile) pS = U(StgCreateDocfile);
    __typeof__(&WriteClassStg) pW = U(WriteClassStg);
    IStorage *stg = NULL;
    if (!pS || FAILED(pS(NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg))) return NULL;
    if (cls && pW) pW(stg, cls);
    return stg;
}
TEST(cgstg_class_override_load_called) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    IStorage *stg = mem_storage(NULL); if (!stg) { t_skip("no storage"); UNREG(); UNINIT(); return; }
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q), S_OK);
    EQ(q.hr, S_OK); CHECK(q.pItf != NULL);
    if (f.lastObj) { EQ(f.lastObj->stgLoaded, 1); EQ(f.lastObj->fileLoaded, 0); t_note("IPersistStorage::Load received the caller's IStorage pointer: %s", f.lastObj->stg == stg ? "yes" : "no"); }
    cg_release(q.pItf); cg_release((IUnknown *)stg);
    UNREG(); UNINIT();
}
TEST(cgstg_class_from_storage) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC1, CLSCTX_INPROC_SERVER);
    IStorage *stg = mem_storage(&cg_TC1); if (!stg) { t_skip("no storage"); UNREG(); UNINIT(); return; }
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(p(NULL, NULL, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q), S_OK);   /* class taken from the storage's CLSID */
    EQ(q.hr, S_OK);
    EQ(f.creates, 1);
    if (f.lastObj) EQ(f.lastObj->stgLoaded, 1);
    cg_release(q.pItf); cg_release((IUnknown *)stg);
    UNREG(); UNINIT();
}
TEST(cgstg_override_wins_over_storage_class) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    CGFactory f0, f1; DWORD cookie, cookie1;
    cg_factory_init(&f0, &cg_TC0); cg_factory_init(&f1, &cg_TC1);
    EQ(cg_register(&f0, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie), S_OK);
    EQ(cg_register(&f1, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie1), S_OK);
    IStorage *stg = mem_storage(&cg_TC1); if (!stg) { t_skip("no storage"); goto out; }
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q), S_OK);
    EQ(f0.creates, 1); EQ(f1.creates, 0);
    cg_release(q.pItf); cg_release((IUnknown *)stg);
out:
    pCoRevokeClassObject(cookie1); UNREG(); UNINIT();
}
TEST(cgstg_pitf_must_be_null) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    IStorage *stg = mem_storage(NULL); if (!stg) { t_skip("no storage"); UNREG(); UNINIT(); return; }
    MULTI_QI q = { &cg_IID_IUnknown, (IUnknown *)(ULONG_PTR)0xdeadbeef, S_OK };
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q), CG_E_INVALIDARG);
    EQ(q.pItf, 0xdeadbeef);
    EQ(f.creates, 0);
    cg_release((IUnknown *)stg);
    UNREG(); UNINIT();
}
TEST(cgstg_load_failure_propagates) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    f.psLoadHr = (HRESULT)0x8000fefe;
    IStorage *stg = mem_storage(NULL); if (!stg) { t_skip("no storage"); UNREG(); UNINIT(); return; }
    MULTI_QI q = { &cg_IID_IUnknown, NULL, S_OK };
    EQ(p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q), (HRESULT)0x8000fefe);
    CHECK(q.pItf == NULL); EQ(q.hr, (HRESULT)0x8000fefe);
    cg_release((IUnknown *)stg);
    UNREG(); UNINIT();
}
TEST(cgstg_unregistered_override) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    IStorage *stg = mem_storage(NULL); if (!stg) { t_skip("no storage"); UNINIT(); return; }
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    EQ(p(NULL, (CLSID *)&cg_TCUNREG, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q), CG_E_CLASSNOTREG);
    CHECK(q.pItf == NULL); EQ(q.hr, CG_E_CLASSNOTREG);
    cg_release((IUnknown *)stg);
    UNINIT();
}
TEST(cgstg_null_storage) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    CGFactory f; DWORD cookie; REG(f, cg_TC0, CLSCTX_INPROC_SERVER);
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    HRESULT hr = p(NULL, (CLSID *)&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &q);
    t_note("CoGetInstanceFromIStorage(pstg=NULL) -> 0x%08lx (q.hr 0x%08lx, creates=%ld)", hr, q.hr, f.creates);
    if (SUCCEEDED(hr)) cg_release(q.pItf);
    UNREG(); UNINIT();
}
TEST(cgstg_storage_without_class) {
    STA();
    CoGetInstanceFromIStorage_pfn p = UN(CoGetInstanceFromIStorage); REQUIRE(p);
    IStorage *stg = mem_storage(NULL); if (!stg) { t_skip("no storage"); UNINIT(); return; }
    MULTI_QI q = { &cg_IID_IUnknown, NULL, E_NOTIMPL };
    HRESULT hr = p(NULL, NULL, NULL, CLSCTX_INPROC_SERVER, stg, 1, &q);
    CHECK(FAILED(hr)); CHECK(q.pItf == NULL);
    t_note("storage with CLSID_NULL and no override -> 0x%08lx (q.hr 0x%08lx)", hr, q.hr);
    cg_release((IUnknown *)stg);
    UNINIT();
}
