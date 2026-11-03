/* In-process activation through HKCU\Software\Classes\CLSID\{..}\InprocServer32 (the test exe is the server). */
#include "dxcommon.h"

#define NEED() do { DX_BEGIN(); if (!dx_selfload_ok()) { t_skip("exe cannot act as in-proc server"); return; } } while (0)
static const GUID NULLG;
#define INITA() dx_coinit(COINIT_APARTMENTTHREADED)

static HRESULT WINAPI o_qi(IUnknown *u, REFIID r, void **p) { *p = NULL; if (IsEqualGUID(r, &IID_IUnknown)) { *p = u; return S_OK; } return E_NOINTERFACE; }
static ULONG WINAPI o_ar(IUnknown *u) { return 2; }
static IUnknownVtbl g_ovt = { o_qi, o_ar, o_ar };
static IUnknown g_outer = { &g_ovt };

TEST(dxinproc_getclassobject_factory) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); dx_reset_counters();
    IUnknown *u = (IUnknown *)(ULONG_PTR)1; IClassFactory *f = (IClassFactory *)(ULONG_PTR)1;
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, (void **)&u), S_OK);
    CHECK(u && u != (IUnknown *)1); if (u && u != (IUnknown *)1) u->lpVtbl->Release(u);
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&f), S_OK);
    CHECK(f && f != (IClassFactory *)1);
    CHECK(dx_nclassobj >= 2 && IsEqualGUID(&dx_gco_clsid, &c));
    if (f && f != (IClassFactory *)1) {
        EQ(f->lpVtbl->LockServer(f, TRUE), S_OK); EQ(dx_nlock, 1);
        EQ(f->lpVtbl->LockServer(f, FALSE), S_OK); EQ(dx_nlock, 0);
        f->lpVtbl->Release(f);
    }
    dx_couninit();
}
TEST(dxinproc_getclassobject_unsupported_iid) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA();
    void *p = (void *)1;
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IStream, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    dx_couninit();
}
TEST(dxinproc_getclassobject_unregistered) {
    REQUIRE(U(CoGetClassObject)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); INITA();
    void *p = (void *)1;
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    dx_couninit();
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxinproc_getclassobject_null_output) {
    REQUIRE(U(CoGetClassObject)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); INITA();
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, NULL), E_INVALIDARG);
    dx_couninit();
}
CHILD(dxuninit) {
    /* argv: dxuninit <clsid> res=.. ; nothing initialised COM in this fresh process */
    GUID c, psc; int bad = 0; void *p = (void *)1; HRESULT hr;
    if (argc < 2 || !dx_parse_guid(argv[1], &c)) return 90;
    hr = U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, &p);
    if (hr != CO_E_NOTINITIALIZED || p) { dx_log(argc, argv, "CoGetClassObject hr=%08lx p=%p", hr, p); bad |= 1; }
    p = (void *)1;
    hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p);
    if (hr != CO_E_NOTINITIALIZED) { dx_log(argc, argv, "CoCreateInstance hr=%08lx", hr); bad |= 2; }
    hr = U(CoGetPSClsid)(&IID_IClassFactory, &psc);
    if (hr != CO_E_NOTINITIALIZED) { dx_log(argc, argv, "CoGetPSClsid hr=%08lx", hr); bad |= 4; }
    hr = U(CoRegisterPSClsid)(&IID_IClassFactory, &c);
    if (hr != CO_E_NOTINITIALIZED) { dx_log(argc, argv, "CoRegisterPSClsid hr=%08lx", hr); bad |= 8; }
    return bad;
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxinproc_uninitialized_thread_errors) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; char gs[40], log[512]; dx_reg_inproc(&c, "Both", NULL, 0); dx_guidstr(&c, gs);
    DWORD code = dx_child("dxuninit", gs, 20000, log, sizeof log);
    CHECKF(code == 0, "child mask=%lx: %s", code, log);
}
TEST(dxinproc_createinstance_basic) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); dx_reset_counters();
    IPersist *p = NULL; dx_info i;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IPersist, (void **)&p), S_OK);
    CHECK(p != NULL);
    if (p) {
        CHECK(dx_is_direct((IUnknown *)p));   /* same apartment: no proxy */
        EQ(dx_getinfo((IUnknown *)p, &i), S_OK);
        CHECK(i.marker_ok); EQ(i.idx, c.Data3); EQ(i.tid, GetCurrentThreadId()); EQ(i.pid, GetCurrentProcessId());
        EQ(dx_live, 1);
        CLSID id; EQ(p->lpVtbl->GetClassID(p, &id), S_OK);
        EQ(p->lpVtbl->AddRef(p), 2); EQ(p->lpVtbl->Release(p), 1);
        EQ(p->lpVtbl->Release(p), 0);
        EQ(dx_live, 0);
    }
    EQ(dx_ncreate, 1); CHECK(IsEqualGUID(&dx_ci_iid, &IID_IPersist)); CHECK(dx_ci_outer == NULL);
    dx_couninit();
}
TEST(dxinproc_createinstance_unsupported_iid) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); dx_reset_counters();
    void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IStream, &p), E_NOINTERFACE);
    CHECK(p == NULL); EQ(dx_live, 0);   /* the failed instance was released */
    dx_couninit();
}
TEST(dxinproc_createinstance_unregistered) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); INITA(); void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    EQ(U(CoCreateInstance)(&NULLG, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    dx_couninit();
}
TEST(dxinproc_createinstance_null_output) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); INITA();
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, NULL), E_INVALIDARG);   /* decomp: first check */
    dx_couninit();
}
TEST(dxinproc_createinstance_multiple_independent) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); dx_reset_counters();
    IUnknown *a = NULL, *b = NULL;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&a), S_OK);
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&b), S_OK);
    CHECK(a && b && a != b); EQ(dx_live, 2);
    if (a) EQ(a->lpVtbl->Release(a), 0);
    EQ(dx_live, 1);
    if (b) EQ(b->lpVtbl->Release(b), 0);
    EQ(dx_live, 0);
    dx_couninit();
}
TEST(dxinproc_aggregation_refused) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); dx_reset_counters();
    void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, &g_outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), CLASS_E_NOAGGREGATION);
    CHECK(p == NULL); CHECK(dx_ci_outer == &g_outer);
    p = (void *)1;
    EQ(U(CoCreateInstance)(&c, &g_outer, CLSCTX_INPROC_SERVER, &IID_IPersist, &p), CLASS_E_NOAGGREGATION);
    CHECK(p == NULL);
    dx_couninit();
}
TEST(dxinproc_aggregation_supported) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_AGG); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet agg");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, dx_server_path()); dx_set(sub, "ThreadingModel", "Both");
    INITA(); dx_reset_counters();
    IUnknown *inner = NULL;
    EQ(U(CoCreateInstance)(&c, &g_outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&inner), S_OK);
    CHECK(inner != NULL); CHECK(dx_ci_outer == &g_outer);   /* pUnkOuter reaches IClassFactory::CreateInstance unchanged */
    if (inner) {
        IPersist *ps = NULL;
        EQ(inner->lpVtbl->QueryInterface(inner, &IID_IPersist, (void **)&ps), S_OK);   /* QI on the non-delegating IUnknown */
        if (ps) ps->lpVtbl->Release(ps);   /* delegating Release goes to g_outer */
        inner->lpVtbl->Release(inner);
    }
    void *p = (void *)1;   /* aggregation requires riid == IID_IUnknown */
    EQ(U(CoCreateInstance)(&c, &g_outer, CLSCTX_INPROC_SERVER, &IID_IPersist, &p), CLASS_E_NOAGGREGATION);
    dx_couninit();
}
TEST(dxinproc_createinstanceex_multi_qi) {
    REQUIRE(U(CoCreateInstanceEx)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA();
    MULTI_QI q[3]; memset(q, 0, sizeof q);
    q[0].pIID = &IID_IPersist; q[1].pIID = &IID_IStream; q[2].pIID = &IID_IUnknown;
    HRESULT hr = U(CoCreateInstanceEx)(&c, NULL, CLSCTX_INPROC_SERVER, NULL, 3, q);
    EQ(hr, CO_S_NOTALLINTERFACES);
    EQ(q[0].hr, S_OK); CHECK(q[0].pItf != NULL);
    EQ(q[1].hr, E_NOINTERFACE); CHECK(q[1].pItf == NULL);
    EQ(q[2].hr, S_OK); CHECK(q[2].pItf != NULL);
    for (int i = 0; i < 3; i++) if (q[i].pItf) q[i].pItf->lpVtbl->Release(q[i].pItf);
    EQ(dx_live, 0);
    dx_couninit();
}
TEST(dxinproc_createinstanceex_all_supported) {
    REQUIRE(U(CoCreateInstanceEx)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA();
    MULTI_QI q[2]; memset(q, 0, sizeof q); q[0].pIID = &IID_IUnknown; q[1].pIID = &IID_IPersist;
    EQ(U(CoCreateInstanceEx)(&c, NULL, CLSCTX_INPROC_SERVER, NULL, 2, q), S_OK);
    EQ(q[0].hr, S_OK); EQ(q[1].hr, S_OK);
    for (int i = 0; i < 2; i++) if (q[i].pItf) q[i].pItf->lpVtbl->Release(q[i].pItf);
    dx_couninit();
}
TEST(dxinproc_createinstanceex_none_supported) {
    REQUIRE(U(CoCreateInstanceEx)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA();
    MULTI_QI q[2]; memset(q, 0, sizeof q); q[0].pIID = &IID_IStream; q[1].pIID = &IID_IStorage;
    HRESULT hr = U(CoCreateInstanceEx)(&c, NULL, CLSCTX_INPROC_SERVER, NULL, 2, q);
    EQ(hr, E_NOINTERFACE);
    EQ(q[0].hr, E_NOINTERFACE); EQ(q[1].hr, E_NOINTERFACE);
    dx_couninit();
}
TEST(dxinproc_clsctx_local_only_on_inproc_class) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_LOCAL_SERVER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_HANDLER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), S_OK);
    if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_SERVER, &IID_IUnknown, &p), S_OK);
    if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_ALL, &IID_IUnknown, &p), S_OK);
    if (p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    dx_couninit();
}
TEST(dxinproc_clsctx_handler_only_class) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet handler");
    snprintf(sub, sizeof sub, "%s\\InprocHandler32", k); dx_set(sub, NULL, dx_server_path());
    INITA(); void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    dx_reset_counters();
    HRESULT hr = U(CoGetClassObject)(&c, CLSCTX_INPROC_HANDLER, NULL, &IID_IUnknown, &p);
    t_note("CoGetClassObject(INPROC_HANDLER) on InprocHandler32-only class: hr=%08lx DllGetClassObject calls=%ld", hr, dx_nclassobj);
    if (SUCCEEDED(hr) && p) ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p);
    dx_couninit();
}
TEST(dxinproc_inproc_preferred_over_local) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160]; dx_reg_inproc(&c, "Both", k, sizeof k);
    snprintf(sub, sizeof sub, "%s\\LocalServer32", k); dx_set(sub, NULL, "\"C:\\olet\\does\\not\\exist.exe\"");
    INITA(); IUnknown *u = NULL; dx_info i;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_ALL, &IID_IPersist, (void **)&u), S_OK);
    if (u) { CHECK(dx_is_direct(u)); EQ(dx_getinfo(u, &i), S_OK); EQ(i.pid, GetCurrentProcessId()); u->lpVtbl->Release(u); }
    dx_couninit();
}
TEST(dxinproc_server_reports_class_not_available) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NOCLASS); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet noclass");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, dx_server_path()); dx_set(sub, "ThreadingModel", "Both");
    INITA(); void *p = (void *)1;
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, &p), CLASS_E_CLASSNOTAVAILABLE);   /* propagated from DllGetClassObject */
    CHECK(p == NULL);
    p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), CLASS_E_CLASSNOTAVAILABLE);
    CHECK(p == NULL);
    dx_couninit();
}
TEST(dxinproc_factory_failure_propagates) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NOCREATE); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet nocreate");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, dx_server_path()); dx_set(sub, "ThreadingModel", "Both");
    INITA(); void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), E_OUTOFMEMORY);
    CHECK(p == NULL);
    dx_couninit();
}
TEST(dxinproc_class_key_without_server_keys) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN();
    GUID c; char k[128]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet no servers");
    INITA(); void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_ALL, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    dx_couninit();
}
TEST(dxinproc_server_dll_missing) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet missing dll");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, "C:\\olet\\no\\such\\server.dll"); dx_set(sub, "ThreadingModel", "Both");
    INITA(); void *p = (void *)1;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    t_note("InprocServer32 pointing to a missing file: hr=%08lx (CO_E_DLLNOTFOUND=800401f8, HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)=8007007e)", hr);
    dx_couninit();
}
TEST(dxinproc_server_key_without_default_value) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet no default");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, "ThreadingModel", "Both");
    INITA(); void *p = (void *)1;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    t_note("InprocServer32 without default value: hr=%08lx", hr);
    dx_couninit();
}
TEST(dxinproc_server_path_basename_only) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160]; const char *full = dx_server_path(); const char *base = strrchr(full, '\\');
    dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet basename");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, base ? base + 1 : full); dx_set(sub, "ThreadingModel", "Both");
    INITA(); IUnknown *u = NULL;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u);
    t_note("file-name-only InprocServer32 (resolved through the DLL search order, app dir first): hr=%08lx", hr);
    EQ(hr, S_OK);
    if (u) u->lpVtbl->Release(u);
    dx_couninit();
}
TEST(dxinproc_server_path_expand_sz) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160], var[64], val[MAX_PATH];
    snprintf(var, sizeof var, "OLET_DIR_%lu", GetCurrentProcessId());
    strcpy(val, dx_server_path()); char *sl = strrchr(val, '\\'); char leaf[MAX_PATH]; strcpy(leaf, sl + 1); *sl = 0;
    SetEnvironmentVariableA(var, val);
    dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet expand");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k);
    char reg[MAX_PATH * 2]; snprintf(reg, sizeof reg, "%%%s%%\\%s", var, leaf);
    dx_setx(sub, NULL, reg); dx_set(sub, "ThreadingModel", "Both");
    INITA(); IUnknown *u = NULL;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u);
    t_note("REG_EXPAND_SZ InprocServer32 using a process-local variable: hr=%08lx", hr);
    if (u) u->lpVtbl->Release(u);
    dx_couninit(); SetEnvironmentVariableA(var, NULL);
}
TEST(dxinproc_server_path_quoted) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160], q[MAX_PATH + 4]; dx_reg_inproc(&c, "Both", k, sizeof k);
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); snprintf(q, sizeof q, "\"%s\"", dx_server_path()); dx_set(sub, NULL, q);
    INITA(); IUnknown *u = NULL;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u);
    t_note("quoted InprocServer32 path: hr=%08lx", hr);
    if (u) u->lpVtbl->Release(u);
    dx_couninit();
}
TEST(dxinproc_32bit_registry_view_not_used_by_64bit_process) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k);
    dx_view(KEY_WOW64_32KEY);
    dx_set(k, NULL, "olet 32-bit view only");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, dx_server_path()); dx_set(sub, "ThreadingModel", "Both");
    int in32 = dx_exists(k);
    dx_view(KEY_WOW64_64KEY); int in64 = dx_exists(k);
    dx_view(0);
    if (!in32 || in64) { t_skip("registry views not separate here (in32=%d in64=%d)", in32, in64); dx_view(KEY_WOW64_32KEY); dx_del(k); dx_view(0); return; }
    INITA(); void *p = (void *)1;
    EQ(U(CoGetClassObject)(&c, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    dx_couninit();
    dx_view(KEY_WOW64_32KEY); dx_del(k); dx_view(0);
}
TEST(dxinproc_64bit_registry_view_explicit) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k);
    dx_view(KEY_WOW64_64KEY);
    dx_set(k, NULL, "olet 64-bit view");
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k); dx_set(sub, NULL, dx_server_path()); dx_set(sub, "ThreadingModel", "Both");
    dx_view(0);
    INITA(); IUnknown *u = NULL;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u), S_OK);   /* default view of a 64-bit process is the 64-bit one */
    if (u) u->lpVtbl->Release(u);
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER | CLSCTX_ACTIVATE_64_BIT_SERVER, &IID_IUnknown, (void **)&u);
    t_note("CLSCTX_ACTIVATE_64_BIT_SERVER on a 64-bit process: hr=%08lx", hr);
    if (SUCCEEDED(hr) && u) u->lpVtbl->Release(u);
    dx_couninit();
}
TEST(dxinproc_clsctx_zero_and_reserved) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); IUnknown *u = NULL;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, 0, &IID_IUnknown, (void **)&u);
    t_note("dwClsContext = 0: hr=%08lx", hr); if (u) { u->lpVtbl->Release(u); u = NULL; }
    hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER | CLSCTX_NO_CODE_DOWNLOAD | CLSCTX_NO_FAILURE_LOG, &IID_IUnknown, (void **)&u);
    EQ(hr, S_OK); if (u) u->lpVtbl->Release(u);
    dx_couninit();
}
TEST(dxinproc_instance_survives_unregistration) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128]; dx_reg_inproc(&c, "Both", k, sizeof k); INITA(); IPersist *p = NULL;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IPersist, (void **)&p), S_OK);
    dx_del(k);
    if (p) { CLSID id; EQ(p->lpVtbl->GetClassID(p, &id), S_OK); p->lpVtbl->Release(p); }
    void *q = (void *)1;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &q);
    t_note("create after deleting the CLSID key (class cache?): hr=%08lx", hr);
    if (SUCCEEDED(hr) && q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
    dx_couninit();
}
TEST(dxinproc_cofreeunusedlibraries_keeps_busy_server) {
    REQUIRE(U(CoCreateInstance)); REQUIRE(U(CoFreeUnusedLibraries)); NEED();
    GUID c; dx_reg_inproc(&c, "Both", NULL, 0); INITA(); IUnknown *u = NULL;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u), S_OK);
    if (u) u->lpVtbl->Release(u);
    U(CoFreeUnusedLibraries)();   /* DllCanUnloadNow returns S_FALSE: the module must stay */
    CHECK(GetModuleHandleA(NULL) != NULL);
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u), S_OK);
    if (u) u->lpVtbl->Release(u);
    dx_couninit();
}
TEST(dxinproc_hkcr_registration_visible_in_merged_view) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], gs[40]; dx_reg_inproc(&c, "Both", k, sizeof k); dx_guidstr(&c, gs);
    char path[200], v[MAX_PATH]; LONG n = sizeof v;
    snprintf(path, sizeof path, "CLSID\\%s\\InprocServer32", gs);
    EQ(RegQueryValueA(HKEY_CLASSES_ROOT, path, v, &n), ERROR_SUCCESS);
    EXPECT_STR(v, dx_server_path());
}
