/* How the registry registration drives the typelib marshaler: CoGetPSClsid, PSOAInterface / PSDispatch factories,
 * and the failure modes when the Interface or TypeLib registration is incomplete. */
#include "reg.h"
#include "mar.h"

static void cl_ping(void *p, void *ctx)
{
    IOletMini *m = (IOletMini *)p; LONG r = 0; HRESULT hr;
    if (!m) { CHECKF(0, "unmarshal failed hr=0x%lx", (long)tm_client_hr); return; }
    hr = m->lpVtbl->Ping(m, 1, &r);
    EQ(hr, S_OK); EQ(r, 1001);
    *(int *)ctx = 1;
}
/* marshal `obj` (as iid) on this STA thread and call it from an MTA thread; returns the marshal hr */
static HRESULT marshal_and_ping(const GUID *iid, IOletMini *obj, int *pinged)
{
    IStream *stm = NULL; HRESULT hr;
    *pinged = 0;
    hr = U(CoMarshalInterThreadInterfaceInStream)(iid, (IUnknown *)obj, &stm);
    if (SUCCEEDED(hr)) tm_run_thread(COINIT_MULTITHREADED, stm, iid, cl_ping, pinged, 15000);
    return hr;
}

static int reg_mini(const GUID *iid, const char *leaf, TMLibSpec *out, WCHAR *path)
{
    TMLibSpec s; HRESULT hr;
    rg_spec(&s, 3, 4, 0, iid);
    hr = rg_register(&s, leaf, 1, NULL, path);
    if (out) *out = s;
    CHECKF(hr == S_OK, "register mini lib hr=0x%lx", (long)hr);
    return hr == S_OK;
}

TMTEST_BARE(dxtm_marreg_registered_interface_marshals)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; IOletMini *o; int pinged; HRESULT hr;
    if (!reg_mini(&TM_IID_MiniXu1, "mr1.tlb", &s, path)) return;
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    o = tm_new_mini(&TM_IID_MiniXu1);
    hr = marshal_and_ping(&TM_IID_MiniXu1, o, &pinged);
    EQ(hr, S_OK);
    CHECK(pinged);
    o->lpVtbl->Release(o);
    U(CoUninitialize)();
    tm_leakcheck();
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_marreg_unregistered_interface_does_not_marshal)
{
    IOletMini *o; IStream *stm = NULL; HRESULT hr;
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    o = tm_new_mini(&TM_IID_MiniXu2);
    hr = U(CoMarshalInterThreadInterfaceInStream)(&TM_IID_MiniXu2, (IUnknown *)o, &stm);
    CHECK(FAILED(hr));
    CHECK(stm == NULL);
    t_note("marshaling an interface without any registration: hr=0x%lx (E_NOINTERFACE=0x80004002, REGDB_E_IIDNOTREG=0x80040155)", (long)hr);
    if (stm) IStream_Release(stm);
    o->lpVtbl->Release(o);
    U(CoUninitialize)();
}

TMTEST_BARE(dxtm_marreg_cogetpsclsid)
{
    CLSID c; HRESULT hr; TMLibSpec s; WCHAR path[MAX_PATH];
    if (!U(CoGetPSClsid)) { t_skip("CoGetPSClsid missing"); return; }
    EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    EQ(tm_register_main(), S_OK);
    memset(&c, 0, sizeof c);
    hr = U(CoGetPSClsid)(&TM_IID_IOletCustom, &c);
    EQ(hr, S_OK); CHECK(IsEqualCLSID(&c, &TM_CLSID_PSOAInterface));
    memset(&c, 0, sizeof c);
    hr = U(CoGetPSClsid)(&TM_IID_IOletDual, &c);
    EQ(hr, S_OK); CHECK(IsEqualCLSID(&c, &TM_CLSID_PSOAInterface));
    memset(&c, 0, sizeof c);
    hr = U(CoGetPSClsid)(&TM_IID_IOletDisp, &c);
    EQ(hr, S_OK); CHECK(IsEqualCLSID(&c, &TM_CLSID_PSDispatch));
    memset(&c, 0, sizeof c);
    hr = U(CoGetPSClsid)(&IID_IDispatch, &c);
    EQ(hr, S_OK); CHECK(IsEqualCLSID(&c, &TM_CLSID_PSDispatch));
    memset(&c, 0, sizeof c);
    hr = U(CoGetPSClsid)(&IID_IEnumVARIANT, &c);
    EQ(hr, S_OK);
    t_note("PS clsid of IEnumVARIANT: %08lx", (unsigned long)c.Data1);
    /* interfaces without a ProxyStubClsid32 value */
    c = TM_IID_NULL;
    hr = U(CoGetPSClsid)(&TM_IID_IOletPlain, &c);
    EQ(hr, REGDB_E_IIDNOTREG);
    hr = U(CoGetPSClsid)(&TM_IID_IOletProxyFlag, &c);
    EQ(hr, REGDB_E_IIDNOTREG);
    hr = U(CoGetPSClsid)(&TM_IID_MiniXu3, &c);
    EQ(hr, REGDB_E_IIDNOTREG);
    /* a freshly registered interface becomes visible at once */
    if (reg_mini(&TM_IID_Mini4, "mr3.tlb", &s, path)) {
        memset(&c, 0, sizeof c);
        hr = U(CoGetPSClsid)(&TM_IID_Mini4, &c);
        EQ(hr, S_OK); CHECK(IsEqualCLSID(&c, &TM_CLSID_PSOAInterface));
        rg_unregister(&s, 1);
    }
    tm_end();
    U(CoUninitialize)();
}

static void factory_checks(const CLSID *clsid, const GUID *riid, int expect_ok, const char *what)
{
    IPSFactoryBuffer *f = NULL; HRESULT hr; IRpcProxyBuffer *px = NULL; void *itf = NULL; IRpcStubBuffer *stub = NULL; IOletCustom *srv = tm_new_custom();
    hr = U(CoGetClassObject)(clsid, CLSCTX_INPROC_SERVER, NULL, &IID_IPSFactoryBuffer, (void **)&f);
    CHECKF(hr == S_OK, "%s: CoGetClassObject(IPSFactoryBuffer) hr=0x%lx", what, (long)hr);
    if (f) {
        hr = IPSFactoryBuffer_CreateProxy(f, NULL, riid, &px, &itf);
        CHECKF((hr == S_OK) == !!expect_ok, "%s: CreateProxy hr=0x%lx", what, (long)hr);
        if (SUCCEEDED(hr)) {
            CHECK(px != NULL && itf != NULL);
            if (itf) {
                /* the interface pointer is the proxy for riid; QI back to it works */
                void *again = NULL;
                EQ(IUnknown_QueryInterface((IUnknown *)itf, riid, &again), S_OK);
                CHECK(again == itf);
                if (again) IUnknown_Release((IUnknown *)again);
                IUnknown_Release((IUnknown *)itf);
            }
            if (px) IRpcProxyBuffer_Release(px);
        } else { CHECK(px == NULL && itf == NULL); }
        hr = IPSFactoryBuffer_CreateStub(f, riid, (IUnknown *)srv, &stub);
        if (!strcmp(what, "PSOAInterface IOletCustom")) {
            CHECKF(hr == S_OK, "%s: CreateStub hr=0x%lx", what, (long)hr);
            if (stub) { t_note("%s: CountRefs of an unconnected stub = %lu", what, (unsigned long)IRpcStubBuffer_CountRefs(stub)); IRpcStubBuffer_Release(stub); stub = NULL; }
        } else {
            t_note("%s: CreateStub hr=0x%lx", what, (long)hr);
            if (stub) IRpcStubBuffer_Release(stub);
        }
        IPSFactoryBuffer_Release(f);
    }
    srv->lpVtbl->Release(srv);
}
TMTEST(dxtm_marreg_psoainterface_factory)
{
    EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    factory_checks(&TM_CLSID_PSOAInterface, &TM_IID_IOletCustom, 1, "PSOAInterface IOletCustom");
    factory_checks(&TM_CLSID_PSOAInterface, &TM_IID_IOletDual, 1, "PSOAInterface IOletDual");
    factory_checks(&TM_CLSID_PSOAInterface, &TM_IID_IOletSink, 1, "PSOAInterface IOletSink");
    U(CoUninitialize)();
}
TMTEST(dxtm_marreg_psoainterface_factory_rejects_unknown_iid)
{
    /* IOletPlain is not in the Interface registry: the typelib marshaler cannot locate its typeinfo */
    EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    factory_checks(&TM_CLSID_PSOAInterface, &TM_IID_IOletPlain, 0, "PSOAInterface IOletPlain");
    factory_checks(&TM_CLSID_PSOAInterface, &TM_IID_MiniXu4, 0, "PSOAInterface unregistered IID");
    U(CoUninitialize)();
}
TMTEST(dxtm_marreg_psdispatch_factory)
{
    EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    factory_checks(&TM_CLSID_PSDispatch, &IID_IDispatch, 1, "PSDispatch IDispatch");
    U(CoUninitialize)();
}

/* ---------------------------------------------------------------- broken registrations */
static void tamper_delete(const GUID *iid, const WCHAR *sub)
{
    WCHAR p[260];
    tm_if_path(tm_ur(), iid, sub, p, 260);
    RegDeleteTreeW(tm_ur(), p);
}
static void marshal_must_fail_(const GUID *iid, const char *what, int hard)
{
    IOletMini *o; IStream *stm = NULL; HRESULT hr;
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    o = tm_new_mini(iid);
    hr = U(CoMarshalInterThreadInterfaceInStream)(iid, (IUnknown *)o, &stm);
    if (hard) CHECKF(FAILED(hr), "%s: marshaling succeeded (hr=0x%lx)", what, (long)hr);
    t_note("%s: hr=0x%lx", what, (long)hr);
    if (stm) IStream_Release(stm);
    o->lpVtbl->Release(o);
    U(CoUninitialize)();
}

#define marshal_must_fail(i, w) marshal_must_fail_(i, w, 1)
TMTEST_BARE(dxtm_marreg_missing_typelib_subkey)
{
    TMLibSpec s; WCHAR path[MAX_PATH];
    if (!reg_mini(&TM_IID_Mini8, "mb1.tlb", &s, path)) return;
    tamper_delete(&TM_IID_Mini8, L"TypeLib");
    CHECK(!rg_if_exists(tm_ur(), &TM_IID_Mini8, L"TypeLib", 0));
    marshal_must_fail_(&TM_IID_Mini8, "Interface\\TypeLib removed", 0);   /* the lookup may fall back to a registry scan: observation only */
    tm_sweep();
}

TMTEST_BARE(dxtm_marreg_typelib_file_missing)
{
    TMLibSpec s; WCHAR path[MAX_PATH];
    if (!reg_mini(&TM_IID_Mini7, "mb2.tlb", &s, path)) return;
    CHECK(DeleteFileW(path));
    marshal_must_fail(&TM_IID_Mini7, "registered typelib file deleted");
    tm_sweep();
}

TMTEST_BARE(dxtm_marreg_typelib_version_not_registered)
{
    TMLibSpec s; WCHAR path[MAX_PATH], p[260]; HKEY k; static const WCHAR bad[] = L"9.9";
    if (!reg_mini(&TM_IID_Mini6, "mb3.tlb", &s, path)) return;
    tm_if_path(tm_ur(), &TM_IID_Mini6, L"TypeLib", p, 260);
    if (RegOpenKeyExW(tm_ur(), p, 0, KEY_WRITE, &k) == 0) {
        RegSetValueExW(k, L"Version", 0, REG_SZ, (const BYTE *)bad, sizeof bad);
        RegCloseKey(k);
    }
    marshal_must_fail(&TM_IID_Mini6, "Interface\\TypeLib Version points to an unregistered version");
    tm_sweep();
}

TMTEST_BARE(dxtm_marreg_bogus_proxystub_clsid)
{
    TMLibSpec s; WCHAR path[MAX_PATH], p[260]; HKEY k; static const WCHAR bogus[] = L"{0b1d0099-7e57-4a11-9c11-01a1c0de0001}";
    if (!reg_mini(&TM_IID_Mini5, "mb4.tlb", &s, path)) return;
    tm_if_path(tm_ur(), &TM_IID_Mini5, L"ProxyStubClsid32", p, 260);
    if (RegOpenKeyExW(tm_ur(), p, 0, KEY_WRITE, &k) == 0) {
        RegSetValueExW(k, NULL, 0, REG_SZ, (const BYTE *)bogus, sizeof bogus);
        RegCloseKey(k);
    }
    marshal_must_fail(&TM_IID_Mini5, "ProxyStubClsid32 names a class that does not exist");
    tm_sweep();
}

TMTEST_BARE(dxtm_marreg_interface_absent_from_registered_typelib)
{
    /* Interface\{iid} claims the (registered) main library, which does not describe that IID */
    WCHAR p[260], g[40], lib[40]; HKEY k; DWORD d; IOletMini *o; IStream *stm = NULL; HRESULT hr;
    if (FAILED(tm_register_main())) { t_skip("register main"); return; }
    tm_guid_str(&TM_IID_MiniDual, g); tm_guid_str(&TM_LIBID_Main, lib);
    _snwprintf(p, 260, L"%lsInterface\\%ls", tm_ur() == HKEY_CLASSES_ROOT ? L"" : L"Software\\Classes\\", g);
    if (RegCreateKeyExW(tm_ur(), p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &d)) { t_skip("cannot create key"); return; }
    RegCloseKey(k);
    { WCHAR q[300]; WCHAR ps[40]; tm_guid_str(&TM_CLSID_PSOAInterface, ps);
      _snwprintf(q, 300, L"%ls\\ProxyStubClsid32", p); RegCreateKeyExW(tm_ur(), q, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &d); RegSetValueExW(k, NULL, 0, REG_SZ, (BYTE *)ps, (lstrlenW(ps) + 1) * 2); RegCloseKey(k);
      _snwprintf(q, 300, L"%ls\\TypeLib", p); RegCreateKeyExW(tm_ur(), q, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &d); RegSetValueExW(k, NULL, 0, REG_SZ, (BYTE *)lib, (lstrlenW(lib) + 1) * 2); RegSetValueExW(k, L"Version", 0, REG_SZ, (BYTE *)L"b.1a", 10); RegCloseKey(k); }
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    o = tm_new_mini(&TM_IID_MiniDual);
    hr = U(CoMarshalInterThreadInterfaceInStream)(&TM_IID_MiniDual, (IUnknown *)o, &stm);
    CHECK(FAILED(hr));
    t_note("IID missing from its registered typelib: hr=0x%lx (TYPE_E_ELEMENTNOTFOUND=0x8002802B)", (long)hr);
    if (stm) IStream_Release(stm);
    o->lpVtbl->Release(o);
    U(CoUninitialize)();
    RegDeleteTreeW(tm_ur(), p);
    tm_end();
}

static void cl_add_once(void *p, void *ctx)
{
    IOletCustom *c = (IOletCustom *)p; LONG r = 0;
    if (!c) { CHECKF(0, "unmarshal failed hr=0x%lx", (long)tm_client_hr); return; }
    EQ(c->lpVtbl->AddLong(c, 20, 22, &r), S_OK); EQ(r, 42);
}
TMTEST(dxtm_marreg_reregistering_keeps_marshaling_working)
{
    int i;
    for (i = 0; i < 3; i++) EQ(tm_register_main(), S_OK);
    RUN_CUSTOM(cl_add_once, NULL);
}

/* ---------------------------------------------------------------- system registrations the marshaler depends on */
TMTEST_BARE(dxtm_marreg_stdole_typelib_registered)
{
    static const GUID libid = {0x00020430, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    BSTR q = NULL; HRESULT hr; ITypeLib *tl = NULL; WCHAR sys[MAX_PATH];
    hr = U(QueryPathOfRegTypeLib)(&libid, 2, 0, 0, &q);
    EQ(hr, S_OK);
    if (q) {
        const WCHAR *leaf = wcsrchr(q, L'\\');
        CHECKF(leaf && !lstrcmpiW(leaf + 1, L"stdole2.tlb"), "stdole path %ls", q);
        GetSystemDirectoryW(sys, MAX_PATH);
        t_note("stdole2 registered at %ls (system directory %ls)", q, sys);
        SysFreeString(q);
    }
    hr = U(LoadRegTypeLib)(&libid, 2, 0, 0, &tl);
    EQ(hr, S_OK);
    if (tl) {
        ITypeInfo *ti = NULL;
        EQ(ITypeLib_GetTypeInfoOfGuid(tl, &IID_IDispatch, &ti), S_OK);
        if (ti) ITypeInfo_Release(ti);
        EQ(ITypeLib_GetTypeInfoOfGuid(tl, &IID_IUnknown, &ti), S_OK);
        if (ti) ITypeInfo_Release(ti);
        ITypeLib_Release(tl);
    }
}

TMTEST_BARE(dxtm_marreg_system_idispatch_registration)
{
    WCHAR v[300], g[40], p[260];
    tm_guid_str(&TM_CLSID_PSDispatch, g);
    tm_if_path(HKEY_CLASSES_ROOT, &IID_IDispatch, L"ProxyStubClsid32", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    CHECK(tm_wieq(v, g));
    tm_guid_str(&TM_CLSID_PSEnumVariant, g);
    tm_if_path(HKEY_CLASSES_ROOT, &IID_IEnumVARIANT, L"ProxyStubClsid32", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    t_note("IEnumVARIANT ProxyStubClsid32 = %ls (PSEnumVariant would be %ls)", v, g);
}
