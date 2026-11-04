/* combase/ole32 exports with no earlier coverage: CoAllowUnmarshalerCLSID, CoDisconnectContext,
 * CoGetSystemSecurityPermissions, CoHandlePriorityEventsFromMessagePump, CoInstall,
 * CoInvalidateRemoteMachineBindings, CoQueryAuthenticationServices, CoRegisterActivationFilter,
 * CoRegisterSurrogate, CoRegisterDeviceCatalog / CoRevokeDeviceCatalog.
 *
 * Several of these change process-global COM state that cannot be undone (activation filter, surrogate,
 * security initialisation), so those run in CHILD() processes, one scenario per process. */
#include "gap0.h"

UDECL(HRESULT, CoAllowUnmarshalerCLSID, (const GUID *));
UDECL(HRESULT, CoDisconnectContext, (DWORD));
UDECL(HRESULT, CoGetSystemSecurityPermissions, (DWORD, PSECURITY_DESCRIPTOR *));
UDECL(void,    CoHandlePriorityEventsFromMessagePump, (void));
typedef struct { ULONG tyspec; ULONG pad; BYTE u[48]; } G_CLSSPEC;
UDECL(HRESULT, CoInstall, (void *, DWORD, G_CLSSPEC *, void *, LPWSTR));
UDECL(HRESULT, CoInvalidateRemoteMachineBindings, (LPWSTR));
UDECL(HRESULT, CoQueryAuthenticationServices, (DWORD *, SOLE_AUTHENTICATION_SERVICE **));
UDECL(HRESULT, CoRegisterActivationFilter, (void *));
UDECL(HRESULT, CoRegisterSurrogate, (void *));
UDECL(HRESULT, CoRegisterDeviceCatalog, (PCWSTR, void **));
UDECL(HRESULT, CoRevokeDeviceCatalog, (void *));


/* the apartment helpers, always through the module */
#define CoInit(mode) U(CoInitializeEx)(NULL, (mode))
#define CoUninit() U(CoUninitialize)()

/* ===================================================================== CoAllowUnmarshalerCLSID */
static const GUID G_NULL_GUID = {0};
static const GUID CLSID_OLET1 = {0x6f6c6574,0x0001,0x4000,{0x80,0,0x6f,0x6c,0x65,0x74,0,1}};

CHILD(gap0_allow_unmarshaler_notinit) {
    CNEED(CoAllowUnmarshalerCLSID);
    /* decomp: IsApartmentInitialized() false -> CO_E_NOTINITIALIZED, before anything else */
    CEQH(pCoAllowUnmarshalerCLSID(&CLSID_OLET1), G_CO_E_NOTINIT);
    CEQH(pCoAllowUnmarshalerCLSID(&G_NULL_GUID), G_CO_E_NOTINIT);
    return CHILD_RC();
}
TEST(gap0_com_allow_unmarshaler_notinit) { WINE_STUB(); NEEDN(CoAllowUnmarshalerCLSID); RUN_CHILD(gap0_allow_unmarshaler_notinit); }

CHILD(gap0_allow_unmarshaler_security) {
    CNEED(CoAllowUnmarshalerCLSID);
    HRESULT hr = CoInit(COINIT_MULTITHREADED); CEQH(hr, S_OK);
    /* decomp: without CoInitializeSecurity -> E_FAIL ("Must be called after CoInitializeSecurity") */
    HRESULT before = pCoAllowUnmarshalerCLSID(&CLSID_OLET1);
    printf("  NOTE CoAllowUnmarshalerCLSID before CoInitializeSecurity: 0x%08lx (decomp: E_FAIL 0x80004005)\n", (unsigned long)before);
    hr = U(CoInitializeSecurity)(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    CEQH(hr, S_OK);
    if (hr == S_OK) {
        CEQH(pCoAllowUnmarshalerCLSID(&CLSID_OLET1), S_OK);
        HRESULT again = pCoAllowUnmarshalerCLSID(&CLSID_OLET1);
        printf("  NOTE registering the same CLSID twice: 0x%08lx\n", (unsigned long)again);
    }
    CoUninit();
    /* after the last CoUninitialize the thread is no longer initialised again */
    CEQH(pCoAllowUnmarshalerCLSID(&CLSID_OLET1), G_CO_E_NOTINIT);
    return CHILD_RC();
}
TEST(gap0_com_allow_unmarshaler_after_security) { WINE_STUB(); NEEDN(CoAllowUnmarshalerCLSID); RUN_CHILD(gap0_allow_unmarshaler_security); }

/* ===================================================================== CoDisconnectContext */
CHILD(gap0_disconnect_context) {
    CNEED(CoDisconnectContext);
    /* no COM on this thread -> no current context: RPC_E_NO_CONTEXT */
    CEQH(pCoDisconnectContext(0), G_RPC_E_NO_CONTEXT);
    CEQH(pCoDisconnectContext(1000), G_RPC_E_NO_CONTEXT);
    HRESULT hr = CoInit(COINIT_APARTMENTTHREADED); CEQH(hr, S_OK);
    /* the default context is not a disconnectable (dynamic) one: CO_E_NOT_SUPPORTED, for any timeout */
    CEQH(pCoDisconnectContext(0), G_CO_E_NOTSUPPORTED);
    CEQH(pCoDisconnectContext(INFINITE), G_CO_E_NOTSUPPORTED);
    CoUninit();
    CEQH(pCoDisconnectContext(0), G_RPC_E_NO_CONTEXT);
    hr = CoInit(COINIT_MULTITHREADED); CEQH(hr, S_OK);
    CEQH(pCoDisconnectContext(0), G_CO_E_NOTSUPPORTED);
    CoUninit();
    return CHILD_RC();
}
TEST(gap0_com_disconnect_context_states) { WINE_STUB(); NEEDN(CoDisconnectContext); RUN_CHILD(gap0_disconnect_context); }

/* ===================================================================== CoGetSystemSecurityPermissions */
TEST(gap0_com_get_system_security_permissions_invalid) { WINE_STUB();
    NEEDN(CoGetSystemSecurityPermissions);
    PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR)(ULONG_PTR)0x1;
    /* comSDType must be 0..3 (COMSD) and ppSD non-NULL: E_INVALIDARG with no side effects */
    for (DWORD t = 0; t < 4; t++) EXPECT_HR(pCoGetSystemSecurityPermissions(t, NULL), G_E_INVALIDARG);
    static const DWORD bad[] = { 4, 5, 7, 0x100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    for (int i = 0; i < 7; i++) {
        sd = (PSECURITY_DESCRIPTOR)(ULONG_PTR)0x1;
        EXPECT_HR(pCoGetSystemSecurityPermissions(bad[i], &sd), G_E_INVALIDARG);
        EXPECT_EQ(sd, 0x1);
        EXPECT_HR(pCoGetSystemSecurityPermissions(bad[i], NULL), G_E_INVALIDARG);
    }
}

TEST(gap0_com_get_system_security_permissions_valid) { WINE_STUB();
    NEEDN(CoGetSystemSecurityPermissions);
    static const char *nm[4] = { "launch permissions", "access permissions", "launch restrictions", "access restrictions" };
    HRESULT ihr = CoInit(COINIT_MULTITHREADED);
    for (DWORD t = 0; t < 4; t++) {
        PSECURITY_DESCRIPTOR sd = NULL;
        HRESULT hr = pCoGetSystemSecurityPermissions(t, &sd);
        if (hr != S_OK) { t_note("%s: hr=0x%08lx", nm[t], (unsigned long)hr); continue; }
        CHECK(sd != NULL);
        if (!sd) continue;
        CHECKF(IsValidSecurityDescriptor(sd), "%s: not a valid security descriptor", nm[t]);
        SECURITY_DESCRIPTOR_CONTROL ctl = 0; DWORD rev = 0;
        if (GetSecurityDescriptorControl(sd, &ctl, &rev)) { EXPECT_EQ(rev, 1); t_note("%s: control=0x%x", nm[t], ctl); }
        { HLOCAL fr = LocalFree(sd); EXPECT_EQ(fr, NULL); }    /* documented: free with LocalFree */
    }
    if (SUCCEEDED(ihr)) CoUninit();
}

TEST(gap0_com_get_system_security_permissions_stable) { WINE_STUB();
    NEEDN(CoGetSystemSecurityPermissions);
    HRESULT ihr = CoInit(COINIT_MULTITHREADED);
    PSECURITY_DESCRIPTOR a = NULL, b = NULL;
    if (pCoGetSystemSecurityPermissions(0, &a) == S_OK && pCoGetSystemSecurityPermissions(0, &b) == S_OK && a && b) {
        CHECK(a != b);                                    /* each call hands out its own buffer */
        CHECK(GetSecurityDescriptorLength(a) == GetSecurityDescriptorLength(b));
        CHECK(!memcmp(a, b, GetSecurityDescriptorLength(a)));
    } else t_note("could not fetch the launch permissions twice");
    if (a) LocalFree(a);
    if (b) LocalFree(b);
    if (SUCCEEDED(ihr)) CoUninit();
}

/* ===================================================================== CoHandlePriorityEventsFromMessagePump */
TEST(gap0_com_handle_priority_events_noop) {
    NEEDN(CoHandlePriorityEventsFromMessagePump);
    /* no COM state, STA and MTA: returns without effect (only modern-STA wait contexts act on it) */
    pCoHandlePriorityEventsFromMessagePump();
    HRESULT hr = CoInit(COINIT_APARTMENTTHREADED);
    CHECK(SUCCEEDED(hr));
    for (int i = 0; i < 100; i++) pCoHandlePriorityEventsFromMessagePump();
    if (SUCCEEDED(hr)) CoUninit();
    hr = CoInit(COINIT_MULTITHREADED);
    CHECK(SUCCEEDED(hr));
    pCoHandlePriorityEventsFromMessagePump();
    if (SUCCEEDED(hr)) CoUninit();
    pCoHandlePriorityEventsFromMessagePump();
    CHECK(1);
}

/* ===================================================================== CoInstall */
TEST(gap0_com_install_null_spec) {
    NEEDN(CoInstall);
    EXPECT_HR(pCoInstall(NULL, 0, NULL, NULL, NULL), G_E_INVALIDARG);
    EXPECT_HR(pCoInstall(NULL, 0xFFFFFFFF, NULL, NULL, L"\\\\nowhere\\share\\pkg.msi"), G_E_INVALIDARG);
}

TEST(gap0_com_install_non_clsid_spec) {
    NEEDN(CoInstall);
    /* only tyspec 0 (TYSPEC_CLSID) is implemented; every other uCLSSPEC type fails before touching anything else
     * (TYSPEC_MIMETYPE=1 FILEEXT=2 OBJECTID=3 PROGID=4 PACKAGENAME=5 */
    static const ULONG specs[] = { 1, 2, 3, 4, 5, 6, 7, 100, 0x80000000, 0xFFFFFFFF };
    for (int i = 0; i < 10; i++) {
        G_CLSSPEC s; memset(&s, 0, sizeof s); s.tyspec = specs[i];
        t_check(pCoInstall(NULL, 0, &s, NULL, NULL) == G_CO_E_APPNOTFOUND, __FILE__, __LINE__, "CoInstall tyspec != CLSID", "tyspec 0x%lx -> 0x%08lx want 0x80040164", specs[i], (unsigned long)pCoInstall(NULL, 0, &s, NULL, NULL));
        EXPECT_HR(pCoInstall(NULL, 0x12345, &s, NULL, L"x"), G_CO_E_APPNOTFOUND);   /* flags and codebase are irrelevant */
    }
}

/* ===================================================================== CoInvalidateRemoteMachineBindings */
TEST(gap0_com_invalidate_remote_bindings_null) { WINE_STUB();
    NEEDN(CoInvalidateRemoteMachineBindings);
    EXPECT_HR(pCoInvalidateRemoteMachineBindings(NULL), G_E_INVALIDARG);
    HRESULT ihr = CoInit(COINIT_MULTITHREADED);
    EXPECT_HR(pCoInvalidateRemoteMachineBindings(NULL), G_E_INVALIDARG);
    if (SUCCEEDED(ihr)) CoUninit();
}

TEST(gap0_com_invalidate_remote_bindings_named) { WINE_STUB();
    NEEDN(CoInvalidateRemoteMachineBindings);
    WCHAR host[96];
    wsprintfW(host, L"olet-no-such-host-%lu", GetCurrentProcessId());
    HRESULT ihr = CoInit(COINIT_MULTITHREADED);
    HRESULT hr = pCoInvalidateRemoteMachineBindings(host);
    t_note("flush of unknown machine: 0x%08lx (flush is a cache operation, S_OK expected)", (unsigned long)hr);
    HRESULT hr2 = pCoInvalidateRemoteMachineBindings(host);
    EXPECT_HR(hr2, hr);                         /* idempotent */
    hr = pCoInvalidateRemoteMachineBindings(L"");
    t_note("flush of empty name: 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(ihr)) CoUninit();
}

/* ===================================================================== CoQueryAuthenticationServices */
static void free_services(DWORD n, SOLE_AUTHENTICATION_SERVICE *s) {
    if (!s) return;
    for (DWORD i = 0; i < n; i++) if (s[i].pPrincipalName) U(CoTaskMemFree)(s[i].pPrincipalName);
    U(CoTaskMemFree)(s);
}

CHILD(gap0_query_auth_default) {
    CNEED(CoQueryAuthenticationServices);
    HRESULT hr = CoInit(COINIT_MULTITHREADED); CEQH(hr, S_OK);
    DWORD n = 0x77; SOLE_AUTHENTICATION_SERVICE *s = (void *)(ULONG_PTR)1;
    /* ppAuthSvc NULL -> E_INVALIDARG */
    CEQH(pCoQueryAuthenticationServices(&n, NULL), G_E_INVALIDARG);
    CEQ(n, 0x77);
    hr = pCoQueryAuthenticationServices(&n, &s);
    CEQH(hr, S_OK);
    if (hr == S_OK) {
        printf("  NOTE services registered before CoInitializeSecurity: %lu\n", n);
        if (n == 0) CEQ(s, 0); else C(s != NULL);
        for (DWORD i = 0; i < n; i++) {
            C(s[i].pPrincipalName != NULL);
            CEQH(s[i].hr, S_OK);
            printf("  NOTE svc[%lu] authn=%lu authz=%lu\n", i, s[i].dwAuthnSvc, s[i].dwAuthzSvc);
        }
        free_services(n, s);
    }
    CoUninit();
    return CHILD_RC();
}
TEST(gap0_com_query_auth_services_default) { WINE_STUB(); NEEDN(CoQueryAuthenticationServices); RUN_CHILD(gap0_query_auth_default); }

CHILD(gap0_query_auth_explicit) {
    CNEED(CoQueryAuthenticationServices);
    HRESULT hr = CoInit(COINIT_MULTITHREADED); CEQH(hr, S_OK);
    SOLE_AUTHENTICATION_SERVICE svc;
    svc.dwAuthnSvc = RPC_C_AUTHN_WINNT; svc.dwAuthzSvc = RPC_C_AUTHZ_NONE; svc.pPrincipalName = (OLECHAR *)(LONG_PTR)-1; svc.hr = 0x1234;
    hr = U(CoInitializeSecurity)(NULL, 1, &svc, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (hr != S_OK) {
        printf("  NOTE CoInitializeSecurity with one explicit service: 0x%08lx; skipping\n", (unsigned long)hr);
    } else {
        DWORD n = 0; SOLE_AUTHENTICATION_SERVICE *s = NULL;
        hr = pCoQueryAuthenticationServices(&n, &s);
        CEQH(hr, S_OK);
        CEQ(n, 1);
        if (hr == S_OK && n == 1 && s) {
            CEQ(s[0].dwAuthnSvc, RPC_C_AUTHN_WINNT);
            CEQ(s[0].dwAuthzSvc, 0);
            CEQH(s[0].hr, S_OK);              /* the result slot is always zeroed */
            C(s[0].pPrincipalName != NULL);
            printf("  NOTE principal name for COLE_DEFAULT_PRINCIPAL: \"%ls\"\n", s[0].pPrincipalName ? s[0].pPrincipalName : L"(null)");
        }
        free_services(n, s);
        /* repeatable, independent allocations */
        DWORD n2 = 0; SOLE_AUTHENTICATION_SERVICE *s2 = NULL;
        CEQH(pCoQueryAuthenticationServices(&n2, &s2), S_OK);
        CEQ(n2, 1);
        free_services(n2, s2);
    }
    CoUninit();
    return CHILD_RC();
}
TEST(gap0_com_query_auth_services_explicit) { WINE_STUB(); NEEDN(CoQueryAuthenticationServices); RUN_CHILD(gap0_query_auth_explicit); }

/* ===================================================================== CoRegisterActivationFilter */
typedef struct { const void *vtbl; LONG refs; } AFObj;
static HRESULT WINAPI af_qi(void *t, REFIID r, void **o) { *o = t; return S_OK; }
static ULONG WINAPI af_addref(void *t) { return InterlockedIncrement(&((AFObj *)t)->refs); }
static ULONG WINAPI af_release(void *t) { return InterlockedDecrement(&((AFObj *)t)->refs); }
static HRESULT WINAPI af_handle(void *t, DWORD type, REFCLSID clsid, CLSID *repl) { return S_OK; }
static const void *af_vtbl[4] = { af_qi, af_addref, af_release, af_handle };

CHILD(gap0_activation_filter) {
    CNEED(CoRegisterActivationFilter);
    AFObj a = { af_vtbl, 1 }, b = { af_vtbl, 1 };
    CEQH(pCoRegisterActivationFilter(NULL), G_E_INVALIDARG);
    CEQH(pCoRegisterActivationFilter(&a), S_OK);              /* first filter wins... */
    CEQH(pCoRegisterActivationFilter(&a), S_OK);              /* ...registering the identical pointer again is accepted */
    CEQH(pCoRegisterActivationFilter(&b), G_CO_E_NOTSUPPORTED);   /* a different filter is refused (no revoke exists) */
    CEQH(pCoRegisterActivationFilter(&b), G_CO_E_NOTSUPPORTED);
    CEQH(pCoRegisterActivationFilter(NULL), G_E_INVALIDARG);  /* NULL is checked first, even when a filter is set */
    CEQH(pCoRegisterActivationFilter(&a), S_OK);
    /* the filter is stored without AddRef */
    CEQ(a.refs, 1);
    CEQ(b.refs, 1);
    return CHILD_RC();
}
TEST(gap0_com_register_activation_filter) { WINE_STUB(); NEEDN(CoRegisterActivationFilter); RUN_CHILD(gap0_activation_filter); }

CHILD(gap0_activation_filter_after_com_init) {
    CNEED(CoRegisterActivationFilter);
    HRESULT hr = CoInit(COINIT_MULTITHREADED); CEQH(hr, S_OK);
    AFObj a = { af_vtbl, 1 };
    CEQH(pCoRegisterActivationFilter(&a), S_OK);              /* needs no COM state; works while initialised too */
    CEQH(pCoRegisterActivationFilter(&a), S_OK);
    CoUninit();
    CEQH(pCoRegisterActivationFilter(&a), S_OK);              /* registration survives CoUninitialize */
    return CHILD_RC();
}
TEST(gap0_com_register_activation_filter_survives_uninit) { WINE_STUB(); NEEDN(CoRegisterActivationFilter); RUN_CHILD(gap0_activation_filter_after_com_init); }

/* ===================================================================== CoRegisterSurrogate */
typedef struct { const void *vtbl; LONG refs; int loads, frees; } SurObj;
static HRESULT WINAPI su_qi(void *t, REFIID r, void **o) { *o = t; ((SurObj *)t)->refs++; return S_OK; }
static ULONG WINAPI su_addref(void *t) { return ++((SurObj *)t)->refs; }
static ULONG WINAPI su_release(void *t) { return --((SurObj *)t)->refs; }
static HRESULT WINAPI su_load(void *t, REFCLSID c) { ((SurObj *)t)->loads++; return S_OK; }
static HRESULT WINAPI su_free(void *t) { ((SurObj *)t)->frees++; return S_OK; }
static const void *su_vtbl[5] = { su_qi, su_addref, su_release, su_load, su_free };

CHILD(gap0_register_surrogate) {
    CNEED(CoRegisterSurrogate);
    SurObj s = { su_vtbl, 1, 0, 0 };
    /* decomp: the apartment check comes first */
    CEQH(pCoRegisterSurrogate(&s), G_CO_E_NOTINIT);
    CEQH(pCoRegisterSurrogate(NULL), G_CO_E_NOTINIT);
    HRESULT hr = CoInit(COINIT_MULTITHREADED); CEQH(hr, S_OK);
    CEQH(pCoRegisterSurrogate(NULL), G_E_INVALIDARG);          /* IsValidInterface(NULL) fails */
    hr = pCoRegisterSurrogate(&s);
    printf("  NOTE CoRegisterSurrogate(valid ISurrogate) in a non-surrogate process: 0x%08lx (loads=%d frees=%d refs=%ld)\n", (unsigned long)hr, s.loads, s.frees, s.refs);
    HRESULT hr2 = pCoRegisterSurrogate(&s);
    printf("  NOTE second registration: 0x%08lx\n", (unsigned long)hr2);
    CoUninit();
    return CHILD_RC();
}
TEST(gap0_com_register_surrogate) { WINE_STUB(); NEEDN(CoRegisterSurrogate); RUN_CHILD(gap0_register_surrogate); }

/* ===================================================================== CoRegisterDeviceCatalog / CoRevokeDeviceCatalog */
CHILD(gap0_device_catalog) {
    CNEED(CoRegisterDeviceCatalog); CNEED(CoRevokeDeviceCatalog);
    WCHAR id[128];
    wsprintfW(id, L"ROOT\\OLET\\%lu", GetCurrentProcessId());
    void *cookie = NULL;
    HRESULT hr = pCoRegisterDeviceCatalog(id, &cookie);
    printf("  NOTE CoRegisterDeviceCatalog(\"%ls\"): 0x%08lx cookie=%p\n", id, (unsigned long)hr, cookie);
    if (hr == S_OK) {
        C(cookie != NULL);
        void *cookie2 = NULL;
        HRESULT hr2 = pCoRegisterDeviceCatalog(id, &cookie2);
        printf("  NOTE registering the same device id again: 0x%08lx cookie=%p\n", (unsigned long)hr2, cookie2);
        if (hr2 == S_OK) { C(cookie2 != cookie); CEQH(pCoRevokeDeviceCatalog(cookie2), S_OK); }
        CEQH(pCoRevokeDeviceCatalog(cookie), S_OK);
        HRESULT rr = pCoRevokeDeviceCatalog(cookie);
        printf("  NOTE revoking an already revoked cookie: 0x%08lx\n", (unsigned long)rr);
    }
    return CHILD_RC();
}
TEST(gap0_com_device_catalog_register_revoke) { NEEDN(CoRegisterDeviceCatalog); NEEDN(CoRevokeDeviceCatalog); RUN_CHILD(gap0_device_catalog); }

CHILD(gap0_device_catalog_revoke_unknown) {
    CNEED(CoRevokeDeviceCatalog);
    /* decomp: the revoke path ignores unknown cookies and always reports S_OK */
    CEQH(pCoRevokeDeviceCatalog(NULL), S_OK);
    return CHILD_RC();
}
TEST(gap0_com_device_catalog_revoke_null) { NEEDN(CoRevokeDeviceCatalog); RUN_CHILD(gap0_device_catalog_revoke_unknown); }
