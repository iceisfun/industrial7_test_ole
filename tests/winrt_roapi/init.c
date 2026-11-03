/* winrt_roapi: RoInitialize / RoUninitialize / apartments / activation preconditions. */
#include "ro_common.h"

static int is_sta(APTTYPE t) { return t == APTTYPE_STA || t == APTTYPE_MAINSTA; }

/* ---------------- RoInitialize argument validation ---------------- */
RO_TEST(winrt_roapi_init_invalid_type) {
    REQUIRE(U(RoInitialize));
    static const int bad[] = { 2, 3, -1, 0x100, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = U(RoInitialize)((RO_INIT_TYPE)bad[i]);
        CHECKF(hr == RO_E_INVALIDARG, "RoInitialize(%d) hr=%08lx", bad[i], (unsigned long)hr);
        if (SUCCEEDED(hr)) U(RoUninitialize)();
        /* a failed RoInitialize must leave the thread uninitialized */
        HRESULT ahr = ro_apt(NULL, NULL);
        CHECKF(ahr == RO_E_NOTINIT, "CoGetApartmentType after bad init hr=%08lx", (unsigned long)ahr);
    }
}

/* ---------------- basic init/uninit pairing ---------------- */
RO_TEST(winrt_roapi_init_mta_pairing) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(RoUninitialize));
    APTTYPE t; APTTYPEQUALIFIER q;
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_EQ(ro_apt(&t, &q), S_OK);
    RO_EQ(t, APTTYPE_MTA);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_FALSE);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_FALSE);
    U(RoUninitialize)();
    U(RoUninitialize)();
    RO_EQ(ro_apt(&t, &q), S_OK);            /* still initialized: count was 3 */
    RO_EQ(t, APTTYPE_MTA);
    U(RoUninitialize)();
    RO_EQ(ro_apt(&t, &q), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_sta_pairing) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(RoUninitialize));
    APTTYPE t = (APTTYPE)-1; APTTYPEQUALIFIER q;
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), S_OK);
    RO_EQ(ro_apt(&t, &q), S_OK);
    CHECKF(is_sta(t) || t == APTTYPE_NA, "apartment type after RO_INIT_SINGLETHREADED = %d", (int)t);
    t_note("RO_INIT_SINGLETHREADED apttype=%d qualifier=%d", (int)t, (int)q);
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), S_FALSE);
    U(RoUninitialize)();
    RO_EQ(ro_apt(&t, &q), S_OK);
    U(RoUninitialize)();
    RO_EQ(ro_apt(&t, &q), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_uninit_shares_count_with_co) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(RoUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    /* RoInitialize and CoInitializeEx share one per-thread init count */
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_FALSE);
    U(CoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), S_OK);
    U(CoUninitialize)();                         /* balances RoInitialize */
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_FALSE);
    U(RoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), S_OK);
    U(RoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
}

/* ---------------- model mismatch matrix ---------------- */
/* adapted from Wine dlls/combase/tests/roapi.c (LGPL-2.1+) semantics; mismatch codes verified against the decomp of RoInitializeForApartmentType */
RO_TEST(winrt_roapi_init_ro_mta_then_co_sta) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(CoInitializeEx));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), RO_E_CHANGED_MODE);
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_FALSE);
    U(CoUninitialize)();
    U(RoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_ro_sta_then_ro_mta) {
    REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), S_OK);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), RO_E_CHANGED_MODE);
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), S_FALSE);
    U(RoUninitialize)(); U(RoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_ro_mta_then_ro_sta) {
    REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), RO_E_CHANGED_MODE);
    U(RoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_co_mta_then_ro) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(CoInitializeEx));
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_FALSE);
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), RO_E_CHANGED_MODE);
    U(RoUninitialize)();
    U(CoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_co_sta_then_ro) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(CoInitializeEx));
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    HRESULT hr = U(RoInitialize)(RO_INIT_SINGLETHREADED);
    /* ASTA request (0xc0000006) against a plain STA: outcome not verifiable from the decomp */
    CHECKF(hr == S_FALSE || hr == RO_E_CHANGED_MODE, "RoInitialize(STA) after CoInitializeEx(STA) hr=%08lx", (unsigned long)hr);
    t_note("RoInitialize(SINGLETHREADED) after CoInitializeEx(STA) = %08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) U(RoUninitialize)();
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), RO_E_CHANGED_MODE);
    U(CoUninitialize)();
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_co_sta_disable_ole1dde_then_ro_sta) {
    REQUIRE(U(RoInitialize)); REQUIRE(U(CoInitializeEx));
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE), S_OK);
    HRESULT hr = U(RoInitialize)(RO_INIT_SINGLETHREADED);
    CHECKF(hr == S_FALSE || hr == RO_E_CHANGED_MODE, "hr=%08lx", (unsigned long)hr);
    t_note("RoInitialize(SINGLETHREADED) after CoInitializeEx(STA|DISABLE_OLE1DDE) = %08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) U(RoUninitialize)();
    U(CoUninitialize)();
}

/* ---------------- cross-thread visibility ---------------- */
static APTTYPE g_xt_type; static APTTYPEQUALIFIER g_xt_q; static HRESULT g_xt_hr;
static DWORD WINAPI probe_thread(void *p) { g_xt_hr = ro_apt(&g_xt_type, &g_xt_q); return 0; }
/* adapted from Wine dlls/combase/tests/roapi.c check_thread_apartment (LGPL-2.1+) */
RO_TEST(winrt_roapi_init_mta_is_process_wide_implicit_for_other_threads) {
    REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    RO_EQ(g_xt_hr, S_OK);
    RO_EQ(g_xt_type, APTTYPE_MTA);
    RO_EQ(g_xt_q, APTTYPEQUALIFIER_IMPLICIT_MTA);
    U(RoUninitialize)();
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    RO_EQ(g_xt_hr, RO_E_NOTINIT);
}
RO_TEST(winrt_roapi_init_sta_is_thread_local) {
    REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), S_OK);
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    RO_EQ(g_xt_hr, RO_E_NOTINIT);
    U(RoUninitialize)();
}
static DWORD WINAPI init_and_leak_thread(void *p) {
    /* intentionally never calls RoUninitialize: thread exit must clean up */
    return (DWORD)U(RoInitialize)((RO_INIT_TYPE)(INT_PTR)p);
}
RO_TEST(winrt_roapi_init_thread_exit_cleans_up_apartment) {
    REQUIRE(U(RoInitialize));
    RO_EQ(ro_run_thread(init_and_leak_thread, (void *)(INT_PTR)RO_INIT_MULTITHREADED, 10000), S_OK);
    RO_EQ(ro_run_thread(init_and_leak_thread, (void *)(INT_PTR)RO_INIT_SINGLETHREADED, 10000), S_OK);
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    RO_EQ(g_xt_hr, RO_E_NOTINIT);
    /* and a new thread can init with either model again */
    RO_EQ(ro_run_thread(init_and_leak_thread, (void *)(INT_PTR)RO_INIT_MULTITHREADED, 10000), S_OK);
}

/* ---------------- activation preconditions ---------------- */
/* adapted from Wine dlls/combase/tests/roapi.c test_implicit_mta (LGPL-2.1+) */
RO_TEST(winrt_roapi_getfactory_requires_com_init) {
    REQUIRE(U(RoGetActivationFactory));
    HSTRING h = ro_hs(L"Does.Not.Exist"); REQUIRE(h);
    void *f = (void *)0xdeadbeef;
    RO_EQ(U(RoGetActivationFactory)(h, &RIID_IActivationFactory, &f), RO_E_NOTINIT);
    CHECK(f == NULL);
    RO_EQ(ro_apt(NULL, NULL), RO_E_NOTINIT);   /* must not implicitly initialize COM on this thread */
    ro_hs_free(h);
}
RO_TEST(winrt_roapi_activate_requires_com_init) {
    REQUIRE(U(RoActivateInstance));
    HSTRING h = ro_hs(L"Does.Not.Exist"); REQUIRE(h);
    IInspectable *i = (IInspectable *)0xdeadbeef;
    RO_EQ(U(RoActivateInstance)(h, &i), RO_E_NOTINIT);
    CHECK(i == NULL);                              /* out param cleared first (decomp) */
    ro_hs_free(h);
}
RO_TEST(winrt_roapi_getfactory_unknown_class_each_model) {
    REQUIRE(U(RoGetActivationFactory)); REQUIRE(U(RoInitialize));
    HSTRING h = ro_hs(L"Does.Not.Exist"); REQUIRE(h);
    static const RO_INIT_TYPE types[] = { RO_INIT_SINGLETHREADED, RO_INIT_MULTITHREADED };
    for (int i = 0; i < 2; i++) {
        RO_EQ(U(RoInitialize)(types[i]), S_OK);
        for (int k = 0; k < 2; k++) {
            void *f = (void *)0xdeadbeef;
            RO_EQ(U(RoGetActivationFactory)(h, &RIID_IActivationFactory, &f), RO_E_CLASSNOTREG);
            CHECK(f == NULL);
        }
        U(RoUninitialize)();
    }
    ro_hs_free(h);
}
RO_TEST(winrt_roapi_activate_unknown_class) {
    REQUIRE(U(RoActivateInstance)); REQUIRE(U(RoInitialize));
    HSTRING h = ro_hs(L"Does.Not.Exist"); REQUIRE(h);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    IInspectable *i = (IInspectable *)0xdeadbeef;
    RO_EQ(U(RoActivateInstance)(h, &i), RO_E_CLASSNOTREG);
    CHECK(i == NULL);
    U(RoUninitialize)();
    ro_hs_free(h);
}
/* RoGetActivationFactory on an STA thread references the implicit MTA (visible from other threads) */
/* adapted from Wine dlls/combase/tests/roapi.c test_implicit_mta (LGPL-2.1+) */
RO_TEST(winrt_roapi_getfactory_creates_implicit_mta_from_sta) {
    REQUIRE(U(RoGetActivationFactory)); REQUIRE(U(CoInitializeEx));
    HSTRING h = ro_hs(L"Does.Not.Exist"); REQUIRE(h);
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    RO_EQ(g_xt_hr, RO_E_NOTINIT);
    void *f = NULL;
    RO_EQ(U(RoGetActivationFactory)(h, &RIID_IActivationFactory, &f), RO_E_CLASSNOTREG);
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    if (g_xt_hr == S_OK) {
        RO_EQ(g_xt_type, APTTYPE_MTA);
        RO_EQ(g_xt_q, APTTYPEQUALIFIER_IMPLICIT_MTA);
    } else t_note("no implicit MTA after RoGetActivationFactory on STA (hr=%08lx; Win8 behaviour)", (unsigned long)g_xt_hr);
    U(CoUninitialize)();
    RO_EQ(ro_run_thread(probe_thread, NULL, 10000), 0);
    RO_EQ(g_xt_hr, RO_E_NOTINIT);
    ro_hs_free(h);
}
RO_TEST(winrt_roapi_getfactory_in_mta_leaves_apartment_type_unchanged) {
    REQUIRE(U(RoGetActivationFactory)); REQUIRE(U(RoInitialize));
    HSTRING h = ro_hs(L"Does.Not.Exist"); REQUIRE(h);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    void *f;
    RO_EQ(U(RoGetActivationFactory)(h, &RIID_IActivationFactory, &f), RO_E_CLASSNOTREG);
    APTTYPE t; APTTYPEQUALIFIER q;
    RO_EQ(ro_apt(&t, &q), S_OK);
    RO_EQ(t, APTTYPE_MTA);
    RO_EQ(q, APTTYPEQUALIFIER_NONE);
    U(RoUninitialize)();
    ro_hs_free(h);
}
/* class-id spellings: not verifiable in decomp, so only record behaviour */
RO_TEST(winrt_roapi_getfactory_odd_class_ids_record) {
    REQUIRE(U(RoGetActivationFactory)); REQUIRE(U(RoInitialize));
    static const WCHAR *names[] = { L"", L"X", L".", L"..", L"A.", L".A", L"A..B", L"has space.Class",
        L"windows.foundation.uri", L"WINDOWS.DATA.JSON.JSONOBJECT", L"Windows.Foundation.Uri " };
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        HSTRING h = ro_hs(names[i]);
        void *f = (void *)0xdeadbeef;
        HRESULT hr = U(RoGetActivationFactory)(h, &RIID_IActivationFactory, &f);
        CHECKF(FAILED(hr) == (f == NULL), "hr=%08lx f=%p for \"%ls\"", (unsigned long)hr, f, names[i]);
        t_note("RoGetActivationFactory(\"%ls\") = %08lx", names[i], (unsigned long)hr);
        if (SUCCEEDED(hr) && f) RO_RELEASE(*(IUnknown **)&f);
        ro_hs_free(h);
    }
    /* NULL HSTRING is the empty string */
    if (t_is_wine()) { t_skip("NULL HSTRING crashes the emulator"); U(RoUninitialize)(); return; }
    void *f = (void *)0xdeadbeef;
    HRESULT hr = U(RoGetActivationFactory)(NULL, &RIID_IActivationFactory, &f);
    CHECK(FAILED(hr)); CHECK(f == NULL);
    t_note("RoGetActivationFactory(NULL hstring) = %08lx", (unsigned long)hr);
    U(RoUninitialize)();
}

/* ---------------- RoGetApartmentIdentifier ---------------- */
RO_TEST(winrt_roapi_apartment_identifier_matrix) {
    REQUIRE(U(RoGetApartmentIdentifier)); REQUIRE(U(RoInitialize));
    UINT64 id = 0x1122334455667788ull;
    HRESULT hr = U(RoGetApartmentIdentifier)(&id);
    /* desktop (non-packaged) tokens carry no application id: ERROR_API_UNAVAILABLE and *out untouched (decomp) */
    CHECKF(hr == RO_E_API_UNAVAIL || hr == RO_E_NOTINIT || hr == S_OK, "no-init hr=%08lx", (unsigned long)hr);
    if (hr == RO_E_API_UNAVAIL) {
        CHECK(id == 0x1122334455667788ull);
        RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
        id = 0x1122334455667788ull;
        RO_EQ(U(RoGetApartmentIdentifier)(&id), RO_E_API_UNAVAIL);
        CHECK(id == 0x1122334455667788ull);
        U(RoUninitialize)();
        t_note("RoGetApartmentIdentifier unavailable for this token (expected for unpackaged desktop processes)");
        return;
    }
    if (t_is_wine()) { t_skip("emulator reports an id without COM init"); return; }
    RO_EQ(hr, RO_E_NOTINIT);
    CHECK(id == 0);
    UINT64 a = 0, b = 0;
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_EQ(U(RoGetApartmentIdentifier)(&a), S_OK);
    RO_EQ(U(RoGetApartmentIdentifier)(&b), S_OK);
    CHECK(a != 0); CHECK(a == b);
    U(RoUninitialize)();
}

/* ---------------- resolve-only surface ---------------- */
TEST(winrt_roapi_exports_resolve_in_combase) {
    static const char *names[] = {
        "RoInitialize","RoUninitialize","RoGetActivationFactory","RoActivateInstance","RoRegisterActivationFactories",
        "RoRevokeActivationFactories","RoGetApartmentIdentifier","RoGetAgileReference","RoOriginateError","RoOriginateErrorW",
        "RoOriginateLanguageException","RoGetErrorReportingFlags","RoSetErrorReportingFlags","RoTransformError","RoTransformErrorW",
        "RoReportUnhandledError","RoCaptureErrorContext","RoClearError","RoGetMatchingRestrictedErrorInfo","GetRestrictedErrorInfo",
        "SetRestrictedErrorInfo","RoInspectCapturedStackBackTrace","RoInspectThreadErrorInfo","RoFailFastWithErrorContext",
        "RoFailFastWithErrorContextInternal","RoGetServerActivatableClasses","RoGetActivatableClassRegistration",
        "RoRegisterForApartmentShutdown","RoUnregisterForApartmentShutdown","RoResolveRestrictedErrorInfoReference",
        "RoGetParameterizedTypeInstanceIID","RoFreeParameterizedTypeExtra","RoParameterizedTypeExtraGetTypeSignature",
        "RoReportFailedDelegate","RoReportCapabilityCheckFailure","CoCreateErrorInfo","CoGetErrorInfo","CoSetErrorInfo",
        "CreateErrorInfo","GetErrorInfo","SetErrorInfo","WindowsCreateString","WindowsDeleteString",
    };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC p = t_proc_in("combase.dll", names[i]);
        CHECKF(p != NULL, "combase.dll export %s missing", names[i]);
    }
}
TEST(winrt_roapi_getprocaddress_negative_ro) {
    HMODULE m = t_module("combase.dll"); REQUIRE(m);
    static const char *bad[] = { "RoInitializeX", "roinitialize", "RoGetActivationFactoryW", "RoActivateInstanceA", "" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        CHECKF(GetProcAddress(m, bad[i]) == NULL, "%s resolved", bad[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(winrt_roapi_ole32_forwards_agile_reference) {
    FARPROC a = t_proc_in("ole32.dll", "RoGetAgileReference");
    FARPROC b = t_proc_in("combase.dll", "RoGetAgileReference");
    REQUIRE(a); REQUIRE(b);
    CHECK(a == b);   /* ole32 export is a forwarder to combase */
}
TEST(winrt_roapi_ole32_error_info_forwarders) {
    static const char *n[] = { "CreateErrorInfo", "GetErrorInfo", "SetErrorInfo" };
    for (int i = 0; i < 3; i++) {
        FARPROC o = t_proc_in("ole32.dll", n[i]), c = t_proc_in("combase.dll", n[i]);
        REQUIRE(o); REQUIRE(c);
        CHECKF(o == c, "%s: ole32 %p combase %p", n[i], o, c);
    }
}
