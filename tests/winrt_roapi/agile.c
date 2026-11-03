/* winrt_roapi: RoGetAgileReference, factory registration negatives, apartment shutdown / server class queries. */
#include "ro_common.h"

/* ---- test object: plain IUnknown (optionally agile), counts calls ---- */
typedef struct { IUnknown u; LONG ref; int agile; DWORD thread_created; LONG calls_off_thread; DWORD last_tid; } Obj;
static HRESULT WINAPI o_qi(IUnknown *s, REFIID r, void **out) {
    Obj *o = (Obj *)s;
    if (GetCurrentThreadId() != o->thread_created) InterlockedIncrement(&o->calls_off_thread);
    o->last_tid = GetCurrentThreadId();
    if (!memcmp(r, &RIID_IUnknown, 16) || (o->agile && !memcmp(r, &RIID_IAgileObject, 16))) { *out = s; InterlockedIncrement(&o->ref); return S_OK; }
    *out = NULL; return RO_E_NOINTERFACE;
}
static ULONG WINAPI o_add(IUnknown *s) { Obj *o = (Obj *)s; if (GetCurrentThreadId() != o->thread_created) InterlockedIncrement(&o->calls_off_thread); return InterlockedIncrement(&o->ref); }
static ULONG WINAPI o_rel(IUnknown *s) { Obj *o = (Obj *)s; if (GetCurrentThreadId() != o->thread_created) InterlockedIncrement(&o->calls_off_thread); return InterlockedDecrement(&o->ref); }
static IUnknownVtbl o_vtbl = { o_qi, o_add, o_rel };
static void o_init(Obj *o, int agile) { memset(o, 0, sizeof *o); o->u.lpVtbl = &o_vtbl; o->ref = 1; o->agile = agile; o->thread_created = GetCurrentThreadId(); }

#define NEED_AR() do { REQUIRE(UN(RoGetAgileReference)); REQUIRE(U(RoInitialize)); REQUIRE(U(RoUninitialize)); } while (0)

/* ================= argument validation ================= */
RO_TEST(winrt_roapi_agile_invalid_options) {
    NEED_AR();
    Obj o; o_init(&o, 0);
    static const UINT bad[] = { 2, 3, 4, 0x10, 0x100, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IAgileRef *r = (IAgileRef *)0xdeadbeef;
        HRESULT hr = UN(RoGetAgileReference)(bad[i], &RIID_IUnknown, &o.u, (void **)&r);
        CHECKF(hr == RO_E_INVALIDARG, "options %#x hr=%08lx", bad[i], (unsigned long)hr);
        CHECKF(r == NULL, "options %#x out=%p", bad[i], r);
    }
    RO_EQ(o.ref, 1);                                       /* the object must not have been touched */
    RO_EQ(o.calls_off_thread, 0);
}
RO_TEST(winrt_roapi_agile_invalid_options_before_com_init) {
    NEED_AR();
    Obj o; o_init(&o, 0);
    IAgileRef *r = (IAgileRef *)0xdeadbeef;
    RO_EQ(UN(RoGetAgileReference)(2, &RIID_IUnknown, &o.u, (void **)&r), RO_E_INVALIDARG);   /* option check precedes any COM state check */
    CHECK(r == NULL);
}
RO_TEST(winrt_roapi_agile_iface_not_supported) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Obj o; o_init(&o, 0);
    for (UINT opt = 0; opt <= 1; opt++) {
        IAgileRef *r = (IAgileRef *)0xdeadbeef;
        HRESULT hr = UN(RoGetAgileReference)(opt, &RIID_IErrorInfo, &o.u, (void **)&r);      /* object only implements IUnknown */
        CHECKF(hr == RO_E_NOINTERFACE, "opt %u hr=%08lx", opt, (unsigned long)hr);
        CHECK(r == NULL);
    }
    RO_EQ(o.ref, 1);
    U(RoUninitialize)();
}

/* ================= same-apartment behaviour ================= */
RO_TEST(winrt_roapi_agile_same_thread_roundtrip) {
    NEED_AR();
    static const RO_INIT_TYPE models[] = { RO_INIT_SINGLETHREADED, RO_INIT_MULTITHREADED };
    for (int m = 0; m < 2; m++) for (UINT opt = 0; opt <= 1; opt++) for (int agile = 0; agile <= 1; agile++) {
        RO_EQ(U(RoInitialize)(models[m]), S_OK);
        Obj o; o_init(&o, agile);
        IAgileRef *r = NULL;
        HRESULT hr = UN(RoGetAgileReference)(opt, &RIID_IUnknown, &o.u, (void **)&r);
        CHECKF(hr == S_OK && r, "model %d opt %u agile %d hr=%08lx", m, opt, agile, (unsigned long)hr);
        if (hr == S_OK && r) {
            IUnknown *got = NULL;
            RO_EQ(r->lpVtbl->Resolve(r, &RIID_IUnknown, (void **)&got), S_OK);
            CHECKF(got == &o.u, "model %d opt %u agile %d: resolved %p want %p", m, opt, agile, got, &o.u);
            RO_RELEASE(got);
            IUnknown *again = NULL;
            RO_EQ(r->lpVtbl->Resolve(r, &RIID_IUnknown, (void **)&again), S_OK);
            CHECK(again == &o.u);
            RO_RELEASE(again);
            RO_RELEASE(r);
        }
        CHECKF(o.ref == 1, "object leaked references: %ld", o.ref);       /* adapted from Wine EXPECT_REF(unk_obj, 1) after release */
        U(RoUninitialize)();
    }
}
RO_TEST(winrt_roapi_agile_reference_holds_object) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Obj o; o_init(&o, 1);
    IAgileRef *r = NULL;
    RO_EQ(UN(RoGetAgileReference)(0, &RIID_IUnknown, &o.u, (void **)&r), S_OK); REQUIRE(r);
    CHECKF(o.ref >= 2, "agile reference does not hold the object (ref=%ld)", o.ref);
    t_note("object refcount while agile reference alive: %ld", o.ref);
    RO_RELEASE(r);
    RO_EQ(o.ref, 1);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_agile_reference_qi) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Obj o; o_init(&o, 1);
    IAgileRef *r = NULL;
    RO_EQ(UN(RoGetAgileReference)(0, &RIID_IUnknown, &o.u, (void **)&r), S_OK); REQUIRE(r);
    void *x = NULL;
    RO_EQ(ro_qi(r, &RIID_IUnknown, &x), S_OK); RO_RELEASE(*(IUnknown **)&x);
    RO_EQ(ro_qi(r, &RIID_IAgileReference, &x), S_OK); CHECK(x == r); RO_RELEASE(*(IUnknown **)&x);
    x = (void *)1; RO_EQ(ro_qi(r, &RIID_IErrorInfo, &x), RO_E_NOINTERFACE); CHECK(x == NULL);
    x = NULL; HRESULT hr = ro_qi(r, &RIID_IAgileObject, &x); t_note("IAgileReference supports IAgileObject: %d", hr == S_OK); RO_RELEASE(*(IUnknown **)&x);
    x = NULL; hr = ro_qi(r, &RIID_IMarshal, &x); t_note("IAgileReference supports IMarshal: %d", hr == S_OK); RO_RELEASE(*(IUnknown **)&x);
    RO_RELEASE(r);
    U(RoUninitialize)();
}

/* ================= cross-apartment ================= */
typedef struct { IAgileRef *ref; Obj *obj; RO_INIT_TYPE model; HRESULT hr, hr_bad; IUnknown *got; int ptr_equal; int usable; LONG off_before; const GUID *iid; } XArg;
static DWORD WINAPI resolve_thread(void *p) {
    XArg *a = (XArg *)p;
    a->hr = U(RoInitialize)(a->model);
    if (FAILED(a->hr)) return 1;
    a->got = NULL;
    a->hr = a->ref->lpVtbl->Resolve(a->ref, &RIID_IUnknown, (void **)&a->got);
    a->ptr_equal = (a->got == &a->obj->u);
    if (a->got) {
        ULONG r1 = a->got->lpVtbl->AddRef(a->got), r2 = a->got->lpVtbl->Release(a->got);
        a->usable = (r1 == r2 + 1);
        a->got->lpVtbl->Release(a->got); a->got = NULL;
    }
    void *bad = (void *)1;
    a->hr_bad = a->ref->lpVtbl->Resolve(a->ref, &RIID_IErrorInfo, &bad);
    if (SUCCEEDED(a->hr_bad) && bad) RO_RELEASE(*(IUnknown **)&bad);
    U(RoUninitialize)();
    return 0;
}
RO_TEST(winrt_roapi_agile_resolve_agile_object_elsewhere) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    for (UINT opt = 0; opt <= 1; opt++) for (int model = 0; model <= 1; model++) {
        Obj o; o_init(&o, 1);
        XArg a = { 0 };
        RO_EQ(UN(RoGetAgileReference)(opt, &RIID_IUnknown, &o.u, (void **)&a.ref), S_OK); REQUIRE(a.ref);
        a.obj = &o; a.model = (RO_INIT_TYPE)model;
        CHECK(ro_run_thread(resolve_thread, &a, 20000) == 0);
        RO_EQ(a.hr, S_OK);
        CHECKF(a.usable, "resolved pointer misbehaved (opt %u model %d)", opt, model);
        t_note("agile object resolved in %s thread (opt %u): same pointer=%d", model ? "MTA" : "STA", opt, a.ptr_equal);
        RO_RELEASE(a.ref);
        CHECKF(o.ref == 1, "leaked references: %ld", o.ref);
    }
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_agile_resolve_nonagile_object_in_sta) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Obj o; o_init(&o, 0);                                  /* lives in the MTA (this thread) */
    XArg a = { 0 };
    RO_EQ(UN(RoGetAgileReference)(0, &RIID_IUnknown, &o.u, (void **)&a.ref), S_OK); REQUIRE(a.ref);
    a.obj = &o; a.model = RO_INIT_SINGLETHREADED;
    CHECK(ro_run_thread(resolve_thread, &a, 20000) == 0);
    RO_EQ(a.hr, S_OK);
    CHECK(!a.ptr_equal);                                   /* a proxy, not the raw pointer, in a foreign apartment */
    CHECK(a.usable);
    t_note("calls on the object from foreign threads while resolving: %ld", o.calls_off_thread);
    RO_RELEASE(a.ref);
    RO_EQ(o.ref, 1);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_agile_resolve_nonagile_object_in_other_mta_thread) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Obj o; o_init(&o, 0);
    XArg a = { 0 };
    RO_EQ(UN(RoGetAgileReference)(0, &RIID_IUnknown, &o.u, (void **)&a.ref), S_OK); REQUIRE(a.ref);
    a.obj = &o; a.model = RO_INIT_MULTITHREADED;
    CHECK(ro_run_thread(resolve_thread, &a, 20000) == 0);
    RO_EQ(a.hr, S_OK);
    CHECK(a.usable);
    t_note("non-agile object resolved in another MTA thread: raw pointer=%d", a.ptr_equal);   /* same apartment: expected raw */
    RO_RELEASE(a.ref);
    RO_EQ(o.ref, 1);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_agile_unsupported_iid_on_resolve) {
    NEED_AR(); RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Obj o; o_init(&o, 1);
    IAgileRef *r = NULL; RO_EQ(UN(RoGetAgileReference)(0, &RIID_IUnknown, &o.u, (void **)&r), S_OK); REQUIRE(r);
    void *x = (void *)1; HRESULT hr = r->lpVtbl->Resolve(r, &RIID_IErrorInfo, &x);
    CHECKF(FAILED(hr), "Resolve of unsupported IID succeeded"); t_note("Resolve(unsupported iid) = %08lx out=%p", (unsigned long)hr, x);
    RO_RELEASE(r);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_agile_no_com_init_record) {
    NEED_AR();
    Obj o; o_init(&o, 0);
    IAgileRef *r = (IAgileRef *)0xdeadbeef;
    HRESULT hr = UN(RoGetAgileReference)(0, &RIID_IUnknown, &o.u, (void **)&r);
    t_note("RoGetAgileReference with no COM initialization = %08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) RO_RELEASE(r); else CHECK(r == NULL);
    RO_EQ(o.calls_off_thread, 0);
}
/* HSTRING crossing apartments: IInspectable of an in-box class resolved in an STA, its string results compared */
typedef struct { IAgileRef *ref; HRESULT hr; int name_ok; HSTRING raw; } JArg;
static DWORD WINAPI json_thread(void *p) {
    JArg *a = (JArg *)p;
    if (FAILED(U(RoInitialize)(RO_INIT_SINGLETHREADED))) return 1;
    IInspectable *i = NULL;
    a->hr = a->ref->lpVtbl->Resolve(a->ref, &RIID_IInspectable, (void **)&i);
    if (SUCCEEDED(a->hr) && i) {
        HSTRING n = NULL;
        HRESULT h2 = i->lpVtbl->GetRuntimeClassName(i, &n);
        a->name_ok = SUCCEEDED(h2) && ro_hs_eq(n, L"Windows.Data.Json.JsonObject");
        ro_hs_free(n);
        i->lpVtbl->Release(i);
    }
    U(RoUninitialize)();
    return 0;
}
RO_TEST(winrt_roapi_agile_inspectable_across_apartments) {
    NEED_AR(); REQUIRE(U(RoActivateInstance)); RO_WINE_SKIP_IF(S_OK);
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    HSTRING c = ro_hs(L"Windows.Data.Json.JsonObject"); IInspectable *o = NULL;
    HRESULT hr = U(RoActivateInstance)(c, &o); ro_hs_free(c);
    RO_EQ(hr, S_OK); REQUIRE(o);
    JArg a = { 0 };
    RO_EQ(UN(RoGetAgileReference)(0, &RIID_IInspectable, (IUnknown *)o, (void **)&a.ref), S_OK); REQUIRE(a.ref);
    CHECK(ro_run_thread(json_thread, &a, 20000) == 0);
    RO_EQ(a.hr, S_OK);
    CHECK(a.name_ok);
    RO_RELEASE(a.ref); RO_RELEASE(o);
    U(RoUninitialize)();
}

/* ================= RoRegisterActivationFactories ================= */
static LONG g_cb_calls;
static HRESULT WINAPI cb_factory(HSTRING id, IActivationFactory **f) { InterlockedIncrement(&g_cb_calls); *f = NULL; return E_NOTIMPL; }
RO_TEST(winrt_roapi_register_requires_com_init) {
    REQUIRE(U(RoRegisterActivationFactories));
    HSTRING id = ro_hs(L"Olet.Test.NotInit"); REQUIRE(id);
    PFNGETACTIVATIONFACTORY cb = cb_factory;
    RO_REGISTRATION_COOKIE cookie = (RO_REGISTRATION_COOKIE)0xdeadbeef;
    RO_EQ(U(RoRegisterActivationFactories)(&id, &cb, 1, &cookie), RO_E_NOTINIT);
    CHECK(cookie == NULL);                                  /* cleared before the init check (decomp) */
    RO_EQ(g_cb_calls, 0);
    ro_hs_free(id);
}
RO_TEST(winrt_roapi_register_unregistered_class_rejected) {
    REQUIRE(U(RoRegisterActivationFactories)); REQUIRE(U(RoInitialize));
    WCHAR name[64]; wsprintfW(name, L"Olet.Test%lu.Widget", GetCurrentProcessId());
    HSTRING id = ro_hs(name); REQUIRE(id);
    PFNGETACTIVATIONFACTORY cb = cb_factory;
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_REGISTRATION_COOKIE cookie = (RO_REGISTRATION_COOKIE)0xdeadbeef;
    HRESULT hr = U(RoRegisterActivationFactories)(&id, &cb, 1, &cookie);
    /* combase resolves runtime class info for the id first; classes not declared in the registry/manifest cannot be registered */
    CHECKF(FAILED(hr), "registration of an undeclared class succeeded (hr=%08lx)", (unsigned long)hr);
    t_note("RoRegisterActivationFactories(undeclared class) = %08lx", (unsigned long)hr);
    CHECK(cookie == NULL);
    RO_EQ(g_cb_calls, 0);
    if (SUCCEEDED(hr) && cookie) (t_is_wine() ? (void)0 : U(RoRevokeActivationFactories)(cookie));
    /* and the failed registration must not make the class resolvable */
    void *f = (void *)1;
    RO_EQ(U(RoGetActivationFactory)(id, &RIID_IActivationFactory, &f), RO_E_CLASSNOTREG);
    ro_hs_free(id);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_register_inproc_inbox_class_rejected) {
    REQUIRE(U(RoRegisterActivationFactories)); REQUIRE(U(RoInitialize));
    HSTRING id = ro_hs(L"Windows.Foundation.PropertyValue"); REQUIRE(id);
    PFNGETACTIVATIONFACTORY cb = cb_factory;
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_REGISTRATION_COOKIE cookie = (RO_REGISTRATION_COOKIE)0xdeadbeef;
    HRESULT hr = U(RoRegisterActivationFactories)(&id, &cb, 1, &cookie);
    t_note("RoRegisterActivationFactories(in-proc in-box class) = %08lx", (unsigned long)hr);
    if (t_is_wine()) { if (SUCCEEDED(hr) && cookie) (t_is_wine() ? (void)0 : U(RoRevokeActivationFactories)(cookie)); U(RoUninitialize)(); ro_hs_free(id); t_skip("emulator"); return; }
    CHECKF(FAILED(hr), "in-proc registered class accepted for local registration");    /* decomp: 'Attempted to RoRegister inproc class' */
    CHECK(cookie == NULL);
    RO_EQ(g_cb_calls, 0);
    if (SUCCEEDED(hr) && cookie) (t_is_wine() ? (void)0 : U(RoRevokeActivationFactories)(cookie));
    ro_hs_free(id);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_register_zero_classes_record) {
    REQUIRE(U(RoRegisterActivationFactories)); REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    RO_REGISTRATION_COOKIE cookie = (RO_REGISTRATION_COOKIE)0xdeadbeef;
    HRESULT hr = U(RoRegisterActivationFactories)(NULL, NULL, 0, &cookie);
    t_note("RoRegisterActivationFactories(count=0) = %08lx cookie=%p", (unsigned long)hr, cookie);
    if (SUCCEEDED(hr) && cookie) (t_is_wine() ? (void)0 : U(RoRevokeActivationFactories)(cookie)); else CHECK(cookie == NULL);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_revoke_resolves) {
    REQUIRE(U(RoRevokeActivationFactories));    /* NULL cookie semantics are unspecified; only resolution is checked */
    CHECK(t_proc_in("combase.dll", "RoRevokeActivationFactories") != NULL);
}

/* ================= apartment shutdown / server queries ================= */
typedef struct { void **vtbl; } Sink;
static HRESULT WINAPI sk_qi(void *s, REFIID r, void **o) { *o = NULL; return RO_E_NOINTERFACE; }
static ULONG WINAPI sk_ar(void *s) { return 2; }
static ULONG WINAPI sk_rel(void *s) { return 1; }
static void WINAPI sk_on(void *s, UINT64 id) { }
static void *sk_vtbl[] = { sk_qi, sk_ar, sk_rel, sk_on };
RO_TEST(winrt_roapi_apartment_shutdown_registration) {
    UDECL(HRESULT, RoRegisterForApartmentShutdown, (void *, UINT64 *, void **));
    UDECL(HRESULT, RoUnregisterForApartmentShutdown, (void *));
    RO_WINE_UNIMPL("RoUnregisterForApartmentShutdown"); REQUIRE(UN(RoRegisterForApartmentShutdown)); REQUIRE(UN(RoUnregisterForApartmentShutdown)); REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    Sink s = { sk_vtbl };
    UINT64 id = 0x1122334455667788ull; void *cookie = (void *)0xdeadbeef;
    HRESULT hr = UN(RoRegisterForApartmentShutdown)(&s, &id, &cookie);
    t_note("RoRegisterForApartmentShutdown = %08lx", (unsigned long)hr);
    if (hr == RO_E_API_UNAVAIL) {
        /* decomp: tokens without an application id are refused before any COM call, outputs untouched */
        CHECK(id == 0x1122334455667788ull); CHECK(cookie == (void *)0xdeadbeef);
        RO_EQ(UN(RoUnregisterForApartmentShutdown)((void *)0x1234), RO_E_API_UNAVAIL);
    } else if (SUCCEEDED(hr)) {
        CHECK(cookie != NULL);
        RO_EQ(UN(RoUnregisterForApartmentShutdown)(cookie), S_OK);
    }
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_server_activatable_classes_invalid_name) {
    UDECL(HRESULT, RoGetServerActivatableClasses, (HSTRING, HSTRING **, DWORD *));
    RO_WINE_UNIMPL("RoGetServerActivatableClasses"); REQUIRE(UN(RoGetServerActivatableClasses)); REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    HSTRING empty = ro_hs(L"");
    WCHAR emb[4] = { L'a', 0, L'b', 0 }; HSTRING with_nul = NULL;
    U(WindowsCreateString)(emb, 3, &with_nul);
    HSTRING names[3] = { NULL, empty, with_nul };
    for (int i = 0; i < 3; i++) {
        if (i == 0 && t_is_wine()) continue;
        HSTRING *classes = (HSTRING *)0xdeadbeef; DWORD n = 0xdeadbeef;
        HRESULT hr = UN(RoGetServerActivatableClasses)(names[i], &classes, &n);
        CHECKF(hr == RO_E_INVALIDARG, "name #%d hr=%08lx", i, (unsigned long)hr);
        CHECK(classes == NULL); CHECK(n == 0);              /* outputs zeroed first (decomp) */
    }
    U(SetErrorInfo)(0, NULL);                               /* the failure originated a restricted error */
    ro_hs_free(empty); ro_hs_free(with_nul);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_server_activatable_classes_unknown_server) {
    UDECL(HRESULT, RoGetServerActivatableClasses, (HSTRING, HSTRING **, DWORD *));
    RO_WINE_UNIMPL("RoGetServerActivatableClasses"); REQUIRE(UN(RoGetServerActivatableClasses)); REQUIRE(U(RoInitialize));
    RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK);
    WCHAR nm[64]; wsprintfW(nm, L"olet.NoSuchServer%lu", GetCurrentProcessId());
    HSTRING h = ro_hs(nm);
    HSTRING *classes = (HSTRING *)0xdeadbeef; DWORD n = 0xdeadbeef;
    HRESULT hr = UN(RoGetServerActivatableClasses)(h, &classes, &n);
    t_note("RoGetServerActivatableClasses(unknown server) = %08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    CHECK(classes == NULL); CHECK(n == 0);
    if (SUCCEEDED(hr) && classes) U(CoTaskMemFree)(classes);
    U(SetErrorInfo)(0, NULL);
    ro_hs_free(h);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_resolve_only_failfast_and_report) {
    /* Calling these terminates the process or reports to WER: only their presence is part of the contract. */
    static const char *n[] = { "RoFailFastWithErrorContext", "RoFailFastWithErrorContextInternal", "RoFailFastWithErrorContextInternal2",
        "RoReportUnhandledError", "RoReportFailedDelegate", "RoReportCapabilityCheckFailure", "RoInspectCapturedStackBackTrace",
        "RoInspectThreadErrorInfo", "RoResolveRestrictedErrorInfoReference", "RoGetActivatableClassRegistration" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(t_proc_in("combase.dll", n[i]) != NULL, "combase!%s missing", n[i]);
}
