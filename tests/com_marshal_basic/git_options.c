/* IGlobalInterfaceTable (CLSID_StdGlobalInterfaceTable) and IGlobalOptions (CLSID_GlobalOptions). */
#include "mb.h"
#include <stdlib.h>

#define OBJ g_obj[0]
static const GUID MB_CLSID_GlobalOptions = {0x0000034B, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

static IGlobalInterfaceTable *mk_git(void) {
    IGlobalInterfaceTable *g = NULL;
    HRESULT hr = U(CoCreateInstance)(&MB_CLSID_StdGIT, NULL, CLSCTX_INPROC_SERVER, &IID_IGlobalInterfaceTable, (void **)&g);
    return SUCCEEDED(hr) ? g : NULL;
}
#define GIT_SETUP() REQUIRE(U(CoCreateInstance)); mb_obj_reset(1); MB_INIT_STA(); \
    IGlobalInterfaceTable *git = mk_git(); if (!git) { CHECK(0); MB_DONE(); return; }
#define GIT_END() do { IGlobalInterfaceTable_Release(git); MB_DONE(); } while (0)

/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_git_class_factory_has_no_git_interface) {
    REQUIRE(U(CoGetClassObject)); MB_INIT_STA();
    IClassFactory *cf = NULL; void *p = (void *)1;
    EQ(U(CoGetClassObject)(&MB_CLSID_StdGIT, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf), S_OK);
    if (cf) {
        EQ(IClassFactory_QueryInterface(cf, &IID_IGlobalInterfaceTable, &p), MB_E_NOINTERFACE);
        EQ(p, 0);
        IClassFactory_Release(cf);
    }
    MB_DONE();
}
TEST(mbasic_git_refcounts_are_static) {
    GIT_SETUP();
    EQ(IGlobalInterfaceTable_AddRef(git), 1);      /* the table is a process singleton: AddRef/Release return 1 */
    EQ(IGlobalInterfaceTable_AddRef(git), 1);
    EQ(IGlobalInterfaceTable_Release(git), 1);
    EQ(IGlobalInterfaceTable_Release(git), 1);
    GIT_END();
}
TEST(mbasic_git_is_singleton) {
    GIT_SETUP();
    IGlobalInterfaceTable *g2 = mk_git();
    CHECK(g2 != NULL);
    CHECK(g2 == git);
    if (g2) IGlobalInterfaceTable_Release(g2);
    GIT_END();
}
TEST(mbasic_git_qi) {
    GIT_SETUP();
    void *p = NULL;
    EQ(IGlobalInterfaceTable_QueryInterface(git, &IID_IUnknown, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; EQ(IGlobalInterfaceTable_QueryInterface(git, &IID_IClassFactory, &p), MB_E_NOINTERFACE); EQ(p, 0);
    GIT_END();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_git_register_get_revoke) {
    GIT_SETUP();
    DWORD cookie = 0; IUnknown *u = NULL;
    EQ(IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(0), &IID_IClassFactory, &cookie), S_OK);
    CHECK(cookie != 0);
    EQ(IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &IID_IClassFactory, (void **)&u), S_OK);
    CHECK(u != NULL);
    t_note("same-apartment GetInterfaceFromGlobal returns %s", u == MB_UNK(0) ? "the original pointer" : "a different pointer");
    if (u) IUnknown_Release(u);
    u = NULL;
    EQ(IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &IID_IUnknown, (void **)&u), S_OK);   /* different IID than registered */
    if (u) IUnknown_Release(u);
    EQ(IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie), S_OK);
    EQ(OBJ.ref, 1);
    GIT_END();
}
TEST(mbasic_git_get_after_revoke_fails) {
    GIT_SETUP();
    DWORD cookie = 0; void *p = (void *)1;
    EQ(IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(0), &IID_IClassFactory, &cookie), S_OK);
    EQ(IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie), S_OK);
    EQ(IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &IID_IClassFactory, &p), MB_E_INVALIDARG);
    EQ(IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie), MB_E_INVALIDARG);
    GIT_END();
}
TEST(mbasic_git_bad_cookies) {
    GIT_SETUP();
    static const DWORD bad[] = { 0, 0xFFFFFFFF };
    for (int i = 0; i < 2; i++) {
        void *p = (void *)1;
        EQ(IGlobalInterfaceTable_GetInterfaceFromGlobal(git, bad[i], &IID_IUnknown, &p), MB_E_INVALIDARG);
        EQ(IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, bad[i]), MB_E_INVALIDARG);
    }
    GIT_END();
}
TEST(mbasic_git_cookies_unique) {
    GIT_SETUP();
    DWORD c[20]; int n = 0;
    for (int i = 0; i < 20; i++) {
        HRESULT hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(i & 1), &IID_IClassFactory, &c[i]);
        EQ(hr, S_OK); if (hr == S_OK) n++; else c[i] = 0;
    }
    for (int i = 0; i < 20; i++) { CHECK(c[i] != 0); for (int j = i + 1; j < 20; j++) CHECK(c[i] != c[j]); }
    for (int i = 0; i < 20; i++) if (c[i]) EQ(IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, c[i]), S_OK);
    EQ(OBJ.ref, 1); EQ(g_obj[1].ref, 1);
    GIT_END();
}
TEST(mbasic_git_register_unsupported_iid) {
    GIT_SETUP();
    DWORD cookie = 0x1234;
    HRESULT hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(0), &MB_IID_Bogus, &cookie);
    /* observed on Win10 22H2: Register does not reject an IID the object lacks (S_OK, nonzero cookie) */
    CHECKF(SUCCEEDED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("Register with an IID the object lacks = 0x%lx cookie=0x%lx", (unsigned long)hr, cookie);
    if (SUCCEEDED(hr)) IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie);
    GIT_END();
}
TEST(mbasic_git_register_null_args) {
    GIT_SETUP();
    if (t_is_wine()) { t_skip("wine crashes"); GIT_END(); return; }
    DWORD cookie = 0x1234;
    HRESULT hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, NULL, &IID_IClassFactory, &cookie);
    CHECKF(FAILED(hr), "NULL object hr=0x%lx", (unsigned long)hr);
    t_note("Register(NULL object) = 0x%lx", (unsigned long)hr);
    hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(0), &IID_IClassFactory, NULL);
    CHECKF(FAILED(hr), "NULL cookie hr=0x%lx", (unsigned long)hr);
    t_note("Register(NULL cookie) = 0x%lx", (unsigned long)hr);
    void *p = (void *)1;
    hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(git, 0x1234, &IID_IUnknown, NULL);
    CHECKF(FAILED(hr), "NULL ppv hr=0x%lx", (unsigned long)hr);
    GIT_END();
}
TEST(mbasic_git_get_unsupported_iid) {
    GIT_SETUP();
    DWORD cookie = 0; void *p = (void *)1;
    EQ(IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(0), &IID_IClassFactory, &cookie), S_OK);
    HRESULT hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &MB_IID_Bogus, &p);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("Get with unsupported IID = 0x%lx p=%p", (unsigned long)hr, p);
    IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie);
    GIT_END();
}

typedef struct GW { DWORD cookie; HRESULT hr_get, hr_call, hr_rev; void *got; DWORD tid; BOOL mta, revoke; } GW;
static DWORD CALLBACK git_worker(void *p) {
    GW *w = p; IClassFactory *cf = NULL;
    if (w->mta) MB_INIT_MTA(); else MB_INIT_STA();
    w->tid = GetCurrentThreadId();
    IGlobalInterfaceTable *g = mk_git();
    if (g) {
        w->hr_get = IGlobalInterfaceTable_GetInterfaceFromGlobal(g, w->cookie, &IID_IClassFactory, (void **)&cf);
        w->got = cf;
        if (cf) { void *pp = NULL; /* Win10: LockServer on the proxy is answered locally, CreateInstance reaches the object */
            w->hr_call = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, &pp); IClassFactory_LockServer(cf, FALSE); IClassFactory_Release(cf); }
        if (w->revoke) w->hr_rev = IGlobalInterfaceTable_RevokeInterfaceFromGlobal(g, w->cookie);
        IGlobalInterfaceTable_Release(g);
    }
    MB_DONE();
    return 0;
}
static void git_cross(BOOL mta, BOOL revoke_in_worker) {
    GIT_SETUP();
    GW w; memset(&w, 0, sizeof w); w.mta = mta; w.revoke = revoke_in_worker;
    EQ(IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, MB_UNK(0), &IID_IClassFactory, &w.cookie), S_OK);
    CHECK(mb_join(mb_thread(git_worker, &w), 10000, NULL));
    EQ(w.hr_get, S_OK);
    CHECK(w.got != NULL && w.got != (void *)MB_CF(0));        /* proxy in the other apartment */
    EQ(w.hr_call, E_NOTIMPL);                                 /* the object's CreateInstance ran */
    EQ(OBJ.ci_calls, 1);
    EQ(OBJ.last_tid, GetCurrentThreadId());                   /* call executed in the registering apartment */
    if (revoke_in_worker) {
        t_note("Revoke from another apartment = 0x%lx", (unsigned long)w.hr_rev);
        if (w.hr_rev != S_OK) IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, w.cookie);
    } else EQ(IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, w.cookie), S_OK);
    GIT_END();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_git_get_from_mta) { git_cross(TRUE, FALSE); }
TEST(mbasic_git_get_from_other_sta) { git_cross(FALSE, FALSE); }
TEST(mbasic_git_revoke_from_other_apartment) { git_cross(TRUE, TRUE); }

/* ---------------------------------------------------------------- IGlobalOptions */

static IGlobalOptions *mk_opts(void) {
    IGlobalOptions *g = NULL;
    HRESULT hr = U(CoCreateInstance)(&MB_CLSID_GlobalOptions, NULL, CLSCTX_INPROC_SERVER, &IID_IGlobalOptions, (void **)&g);
    return SUCCEEDED(hr) ? g : NULL;
}
#define OPT_SETUP() REQUIRE(U(CoCreateInstance)); MB_INIT_STA(); IGlobalOptions *go = mk_opts(); \
    if (!go) { CHECK(0); MB_DONE(); return; }
#define OPT_END() do { IGlobalOptions_Release(go); MB_DONE(); } while (0)

TEST(mbasic_glbopt_create_and_qi) {
    OPT_SETUP();
    void *p = NULL;
    EQ(IGlobalOptions_QueryInterface(go, &IID_IUnknown, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; EQ(IGlobalOptions_QueryInterface(go, &MB_IID_Bogus, &p), MB_E_NOINTERFACE); EQ(p, 0);
    OPT_END();
}
TEST(mbasic_glbopt_query_null_out) {
    REQUIRE(U(CoCreateInstance)); if (t_is_wine()) { t_skip("wine crashes"); return; }
    MB_INIT_STA(); IGlobalOptions *go = mk_opts(); if (!go) { CHECK(0); MB_DONE(); return; }
    EQ(IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, NULL), MB_E_INVALIDARG);
    EQ(IGlobalOptions_Query(go, COMGLB_RO_SETTINGS, NULL), MB_E_INVALIDARG);
    OPT_END();
}
TEST(mbasic_glbopt_query_unknown_property) {
    OPT_SETUP();
    ULONG_PTR v = 0x1234;
    EQ(IGlobalOptions_Query(go, (GLOBALOPT_PROPERTIES)0, &v), MB_E_INVALIDARG);
    EQ(IGlobalOptions_Query(go, (GLOBALOPT_PROPERTIES)100, &v), MB_E_INVALIDARG);
    EQ(IGlobalOptions_Query(go, (GLOBALOPT_PROPERTIES)-1, &v), MB_E_INVALIDARG);
    EQ(v, 0x1234);
    OPT_END();
}
TEST(mbasic_glbopt_query_defaults) {
    OPT_SETUP();
    ULONG_PTR v = 0x1234;
    EQ(IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, &v), S_OK);
    EQ(v, COMGLB_EXCEPTION_HANDLE);
    v = 0x1234; EQ(IGlobalOptions_Query(go, COMGLB_RPC_THREADPOOL_SETTING, &v), S_OK);
    EQ(v, 1);                                          /* Win10: Query returns I_RpcMgmtQueryDedicatedThreadPool() != 0; a dedicated pool is the default */
    v = 0x1234; HRESULT hr = IGlobalOptions_Query(go, COMGLB_RO_SETTINGS, &v);
    EQ(hr, S_OK); t_note("COMGLB_RO_SETTINGS default = 0x%llx", (unsigned long long)v);
    v = 0x1234; hr = IGlobalOptions_Query(go, COMGLB_UNMARSHALING_POLICY, &v);
    EQ(hr, S_OK); t_note("COMGLB_UNMARSHALING_POLICY default = 0x%llx", (unsigned long long)v);
    OPT_END();
}
TEST(mbasic_glbopt_query_appid) {
    OPT_SETUP();
    GUID g; memset(&g, 0xCC, sizeof g);
    HRESULT hr = IGlobalOptions_Query(go, COMGLB_APPID, (ULONG_PTR *)&g);
    t_note("Query(COMGLB_APPID) = 0x%lx (E_FAIL before CoInitializeSecurity)", (unsigned long)hr);
    CHECK(hr == S_OK || hr == (HRESULT)0x80004005);
    OPT_END();
}
TEST(mbasic_glbopt_set_exception_handling_invalid) {
    OPT_SETUP();
    static const ULONG_PTR bad[] = { 3, 4, 0x100, 0xFFFFFFFF, (ULONG_PTR)-1 };
    for (int i = 0; i < 5; i++) {
        HRESULT hr = IGlobalOptions_Set(go, COMGLB_EXCEPTION_HANDLING, bad[i]);
        CHECKF(hr == MB_E_INVALIDARG, "value 0x%llx hr=0x%lx", (unsigned long long)bad[i], (unsigned long)hr);
    }
    ULONG_PTR v = 99; EQ(IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, &v), S_OK); EQ(v, 0);
    OPT_END();
}
TEST(mbasic_glbopt_set_threadpool_invalid) {
    OPT_SETUP();
    EQ(IGlobalOptions_Set(go, COMGLB_RPC_THREADPOOL_SETTING, 0), MB_E_INVALIDARG);   /* only 1 (enable) is accepted */
    EQ(IGlobalOptions_Set(go, COMGLB_RPC_THREADPOOL_SETTING, 2), MB_E_INVALIDARG);
    EQ(IGlobalOptions_Set(go, COMGLB_RPC_THREADPOOL_SETTING, (ULONG_PTR)-1), MB_E_INVALIDARG);
    OPT_END();
}
TEST(mbasic_glbopt_set_ro_settings_invalid) {
    OPT_SETUP();
    EQ(IGlobalOptions_Set(go, COMGLB_RO_SETTINGS, 0x800), MB_E_INVALIDARG);           /* bits above 0x7FF */
    EQ(IGlobalOptions_Set(go, COMGLB_RO_SETTINGS, 0x1000), MB_E_INVALIDARG);
    EQ(IGlobalOptions_Set(go, COMGLB_RO_SETTINGS, (ULONG_PTR)1 << 32), MB_E_INVALIDARG);
    EQ(IGlobalOptions_Set(go, COMGLB_RO_SETTINGS, 6), MB_E_INVALIDARG);               /* 2 and 4 are mutually exclusive */
    EQ(IGlobalOptions_Set(go, COMGLB_RO_SETTINGS, 7), MB_E_INVALIDARG);
    OPT_END();
}
TEST(mbasic_glbopt_set_appid_null) {
    OPT_SETUP();
    HRESULT hr = IGlobalOptions_Set(go, COMGLB_APPID, 0);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);     /* E_INVALIDARG after CoInitializeSecurity, E_FAIL before */
    CHECK(hr == MB_E_INVALIDARG || hr == (HRESULT)0x80004005);
    OPT_END();
}

typedef struct OW { HRESULT hr_set, hr_q; ULONG_PTR v; } OW;
static DWORD CALLBACK opt_uninit_worker(void *p) {
    OW *w = p; IGlobalOptions *go = (IGlobalOptions *)w->v;
    w->hr_set = IGlobalOptions_Set(go, COMGLB_EXCEPTION_HANDLING, 1);
    ULONG_PTR v = 0;
    w->hr_q = IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, &v);
    return 0;
}
TEST(mbasic_glbopt_uninitialized_thread) {
    OPT_SETUP();
    OW w; memset(&w, 0, sizeof w); w.v = (ULONG_PTR)go;
    CHECK(mb_join(mb_thread(opt_uninit_worker, &w), 10000, NULL));
    EQ(w.hr_set, MB_CO_E_NOTINITIALIZED);              /* decomp: IsApartmentInitialized() == FALSE */
    EQ(w.hr_q, MB_CO_E_NOTINITIALIZED);
    ULONG_PTR v = 5; EQ(IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, &v), S_OK); EQ(v, 0);   /* nothing was changed */
    OPT_END();
}

/* process-wide mutation is done in a child process */
CHILD(glbopt_eh) {
    U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    IGlobalOptions *go = mk_opts(); int rc = 0;
    if (!go) return 90;
    ULONG_PTR v = 99;
    if (IGlobalOptions_Set(go, COMGLB_EXCEPTION_HANDLING, 1) != S_OK) rc = 1;
    if (IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, &v) != S_OK || v != 1) rc = 2;
    if (IGlobalOptions_Set(go, COMGLB_EXCEPTION_HANDLING, 2) != S_OK) rc = 3;
    if (IGlobalOptions_Query(go, COMGLB_EXCEPTION_HANDLING, &v) != S_OK || v != 2) rc = 4;
    HRESULT h0 = IGlobalOptions_Set(go, COMGLB_EXCEPTION_HANDLING, 0);
    printf("  NOTE child: Set(EXCEPTION_HANDLING,0) = 0x%lx\n", (unsigned long)h0);
    if (h0 != S_OK && h0 != (HRESULT)0x80004005) rc = 5;
    IGlobalOptions_Release(go);
    U(CoUninitialize)();
    return rc;
}
TEST(mbasic_glbopt_exception_handling_roundtrip_child) {
    REQUIRE(U(CoCreateInstance));
    EQ(t_run_child("glbopt_eh", "", 20000), 0);
}
CHILD(glbopt_pool) {
    U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    IGlobalOptions *go = mk_opts(); int rc = 0;
    if (!go) return 90;
    HRESULT h = IGlobalOptions_Set(go, COMGLB_RPC_THREADPOOL_SETTING, 1);
    printf("  NOTE child: Set(RPC_THREADPOOL_SETTING,1) = 0x%lx\n", (unsigned long)h);
    ULONG_PTR v = 99;
    if (IGlobalOptions_Query(go, COMGLB_RPC_THREADPOOL_SETTING, &v) != S_OK) rc = 2;
    if (h == S_OK && v != 1) rc = 3;
    if (h != S_OK && v != 0) rc = 4;
    IGlobalOptions_Release(go);
    U(CoUninitialize)();
    return rc;
}
TEST(mbasic_glbopt_threadpool_setting_child) {
    REQUIRE(U(CoCreateInstance));
    EQ(t_run_child("glbopt_pool", "", 20000), 0);
}
CHILD(glbopt_ro) {
    U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    IGlobalOptions *go = mk_opts(); int rc = 0;
    if (!go) return 90;
    ULONG_PTR v0 = 0;
    IGlobalOptions_Query(go, COMGLB_RO_SETTINGS, &v0);
    HRESULT h = IGlobalOptions_Set(go, COMGLB_RO_SETTINGS, v0 | 1);
    printf("  NOTE child: Set(RO_SETTINGS, default|1) = 0x%lx (default 0x%llx)\n", (unsigned long)h, (unsigned long long)v0);
    ULONG_PTR v = 0;
    if (h == S_OK) { IGlobalOptions_Query(go, COMGLB_RO_SETTINGS, &v); if (v != (v0 | 1)) rc = 2; }
    IGlobalOptions_Release(go);
    U(CoUninitialize)();
    return rc;
}
TEST(mbasic_glbopt_ro_settings_roundtrip_child) {
    REQUIRE(U(CoCreateInstance));
    EQ(t_run_child("glbopt_ro", "", 20000), 0);
}
