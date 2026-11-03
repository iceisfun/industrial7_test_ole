/* dtd_ao_*: RegisterActiveObject / GetActiveObject / RevokeActiveObject (oleaut32, backed by the running object table).
 * Every test uses a CLSID made unique with the process id; registrations are revoked before the test ends (the ROT entries would
 * vanish with the process anyway).  Documented: ACTIVEOBJECT_STRONG (0) AddRefs the object, ACTIVEOBJECT_WEAK (1) does not;
 * GetActiveObject of an unregistered CLSID fails with MK_E_UNAVAILABLE; registering the same CLSID twice succeeds with
 * MK_S_MONIKERALREADYREGISTERED (ROT semantics).  Exact HRESULTs for invalid revokes are recorded with t_note. */
#include "dtd.h"

#define MK_S_ALREADY   ((HRESULT)0x000401E7)
#define MK_E_UNAVAIL   ((HRESULT)0x800401E3)
#define CO_E_NOTINIT   ((HRESULT)0x800401F0)

static LONG g_seq;
static void make_clsid(CLSID *c)
{
    LONG s = InterlockedIncrement(&g_seq);
    memset(c, 0, sizeof *c);
    c->Data1 = 0x0ae7c110;           /* "olet" test namespace */
    c->Data2 = (WORD)(GetCurrentProcessId() & 0xffff);
    c->Data3 = (WORD)(GetCurrentProcessId() >> 16) ^ 0x4f1e;
    c->Data4[0] = 0x9a; c->Data4[1] = 0x31; c->Data4[2] = 'o'; c->Data4[3] = 'l'; c->Data4[4] = 'e'; c->Data4[5] = 't';
    c->Data4[6] = (BYTE)(s & 0xff); c->Data4[7] = (BYTE)(s >> 8);
}

typedef struct Ctx { Demo d; ITypeInfo *ti; IUnknown *unk; IDispatch *disp; int com; } Ctx;

static int ctx_open(Ctx *c)
{
    HRESULT hr;
    memset(c, 0, sizeof *c);
    if (!OL(CoInitializeEx) || !OL(CoUninitialize) || !OA(CreateStdDispatch) || !OA(RegisterActiveObject)) { t_skip("missing exports"); return 0; }
    hr = OL(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    CHECKF(hr == S_OK || hr == S_FALSE, "CoInitializeEx %08lx", (unsigned long)hr);
    if (FAILED(hr)) return 0;
    c->com = 1;
    Demo_init(&c->d);
    c->ti = demo_typeinfo(0x409);
    if (!c->ti) return 0;
    c->disp = demo_stddisp(&c->d, c->ti, &c->unk);
    return c->disp != NULL;
}
static void ctx_close(Ctx *c)
{
    if (c->disp) IDispatch_Release(c->disp);
    if (c->unk) IUnknown_Release(c->unk);
    if (c->ti) ITypeInfo_Release(c->ti);
    if (c->com) OL(CoUninitialize)();
}

/* Use the registered object through whatever pointer GetActiveObject handed back: Add(30, 12) */
static int use_dispatch(IUnknown *got)
{
    IDispatch *d = NULL; VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); HRESULT hr; UINT n = 0;
    if (IUnknown_QueryInterface(got, &DI_IDispatch, (void **)&d) != S_OK || !d) return 1;
    if (IDispatch_GetTypeInfoCount(d, &n) != S_OK || n != 1) { IDispatch_Release(d); return 2; }
    v_i4(&a[0], 12); v_i4(&a[1], 30);
    vinit(&r);
    hr = IDispatch_Invoke(d, DD_ADD, &DI_NULL, 0x409, DISPATCH_METHOD, &dp, &r, NULL, NULL);
    IDispatch_Release(d);
    if (hr != S_OK) return 3;
    return (V_VT(&r) == VT_I4 && V_I4(&r) == 42) ? 0 : 4;
}

TEST(dtd_ao_get_unregistered_is_unavailable)
{
    CLSID c; IUnknown *got = (IUnknown *)(ULONG_PTR)1; HRESULT hr; Ctx x;
    REQUIRE(OA(GetActiveObject)); REQUIRE(OL(CoInitializeEx));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    hr = OA(GetActiveObject)(&c, NULL, &got);
    EXPECT_EQ(hr, MK_E_UNAVAIL);
    if (got != (IUnknown *)(ULONG_PTR)1) t_note("GetActiveObject failure sets *ppunk = %p", (void *)got);
    ctx_close(&x);
}

TEST(dtd_ao_strong_register_get_revoke)
{
    CLSID c; Ctx x; DWORD cookie = 0xdeadbeef; IUnknown *got = NULL; HRESULT hr;
    REQUIRE(OA(RevokeActiveObject)); REQUIRE(OA(GetActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    hr = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_STRONG, &cookie);
    EXPECT_EQ(hr, S_OK);
    CHECK(cookie != 0 && cookie != 0xdeadbeef);
    if (hr == S_OK) {
        hr = OA(GetActiveObject)(&c, NULL, &got);
        EXPECT_EQ(hr, S_OK); CHECK(got != NULL);
        if (got) {
            EXPECT_EQ(use_dispatch(got), 0);
            { IUnknown *u = NULL; IUnknown_QueryInterface(got, &DI_IUnknown, (void **)&u);
              t_note("returned unknown %s the registered pointer", u == x.unk ? "equals" : "differs from");
              if (u) IUnknown_Release(u); }
            IUnknown_Release(got);
        }
        EXPECT_EQ(OA(RevokeActiveObject)(cookie, NULL), S_OK);
        got = (IUnknown *)(ULONG_PTR)1;
        EXPECT_EQ(OA(GetActiveObject)(&c, NULL, &got), MK_E_UNAVAIL);
    }
    ctx_close(&x);
}

TEST(dtd_ao_weak_register_get_revoke)
{
    CLSID c; Ctx x; DWORD cookie = 0xdeadbeef; IUnknown *got = NULL; HRESULT hr;
    REQUIRE(OA(RevokeActiveObject)); REQUIRE(OA(GetActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    hr = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &cookie);
    EXPECT_EQ(hr, S_OK);
    CHECK(cookie != 0 && cookie != 0xdeadbeef);
    if (hr == S_OK) {
        hr = OA(GetActiveObject)(&c, NULL, &got);
        EXPECT_EQ(hr, S_OK);
        if (got) { EXPECT_EQ(use_dispatch(got), 0); IUnknown_Release(got); }
        EXPECT_EQ(OA(RevokeActiveObject)(cookie, NULL), S_OK);
        EXPECT_EQ(OA(GetActiveObject)(&c, NULL, &got), MK_E_UNAVAIL);
    }
    ctx_close(&x);
}

TEST(dtd_ao_strong_and_weak_refcount_effects_noted)
{
    CLSID c; Ctx x; DWORD ck = 0; ULONG base, after_s, after_rev, after_w, after_rev_w; HRESULT hr;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    base = IUnknown_AddRef(x.unk) - 1; IUnknown_Release(x.unk);
    hr = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_STRONG, &ck);
    EXPECT_EQ(hr, S_OK);
    after_s = IUnknown_AddRef(x.unk) - 1; IUnknown_Release(x.unk);
    CHECKF(after_s > base, "strong registration should AddRef: %lu -> %lu", (unsigned long)base, (unsigned long)after_s);
    OA(RevokeActiveObject)(ck, NULL);
    after_rev = IUnknown_AddRef(x.unk) - 1; IUnknown_Release(x.unk);
    t_note("strong: base=%lu registered=%lu revoked=%lu", (unsigned long)base, (unsigned long)after_s, (unsigned long)after_rev);
    CHECK(after_rev <= after_s);
    ck = 0;
    hr = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck);
    EXPECT_EQ(hr, S_OK);
    after_w = IUnknown_AddRef(x.unk) - 1; IUnknown_Release(x.unk);
    OA(RevokeActiveObject)(ck, NULL);
    after_rev_w = IUnknown_AddRef(x.unk) - 1; IUnknown_Release(x.unk);
    t_note("weak: base=%lu registered=%lu revoked=%lu", (unsigned long)after_rev, (unsigned long)after_w, (unsigned long)after_rev_w);
    CHECKF(after_w <= after_s, "weak registration must not hold more references than strong (%lu vs %lu)", (unsigned long)after_w, (unsigned long)after_s);
    ctx_close(&x);
}

TEST(dtd_ao_double_registration_same_clsid)
{
    CLSID c; Ctx x; DWORD ck1 = 0, ck2 = 0; HRESULT hr1, hr2;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    hr1 = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck1);
    hr2 = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck2);
    EXPECT_EQ(hr1, S_OK);
    EXPECT_EQ(hr2, MK_S_ALREADY);
    CHECK(ck1 != 0 && ck2 != 0 && ck1 != ck2);
    if (SUCCEEDED(hr2)) {
        HRESULT r2 = OA(RevokeActiveObject)(ck2, NULL);
        EXPECT_EQ(r2, S_OK);
    }
    if (SUCCEEDED(hr1)) {
        HRESULT r1 = OA(RevokeActiveObject)(ck1, NULL);
        t_note("revoking the first cookie after the duplicate was revoked: %08lx", (unsigned long)r1);
    }
    ctx_close(&x);
}

TEST(dtd_ao_double_registration_strong_then_weak)
{
    CLSID c; Ctx x; DWORD ck1 = 0, ck2 = 0; HRESULT hr1, hr2;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    hr1 = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_STRONG, &ck1);
    hr2 = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck2);
    EXPECT_EQ(hr1, S_OK); EXPECT_EQ(hr2, MK_S_ALREADY);
    if (SUCCEEDED(hr2)) OA(RevokeActiveObject)(ck2, NULL);
    if (SUCCEEDED(hr1)) OA(RevokeActiveObject)(ck1, NULL);
    ctx_close(&x);
}

TEST(dtd_ao_register_after_revoke_is_plain_ok)
{
    CLSID c; Ctx x; DWORD ck1 = 0, ck2 = 0; HRESULT hr;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck1), S_OK);
    EXPECT_EQ(OA(RevokeActiveObject)(ck1, NULL), S_OK);
    hr = OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck2);
    EXPECT_EQ(hr, S_OK);
    CHECK(ck2 != 0);
    if (SUCCEEDED(hr)) OA(RevokeActiveObject)(ck2, NULL);
    ctx_close(&x);
}

TEST(dtd_ao_independent_clsids)
{
    CLSID c1, c2; Ctx x; DWORD k1 = 0, k2 = 0; IUnknown *got = NULL;
    REQUIRE(OA(RevokeActiveObject)); REQUIRE(OA(GetActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c1); make_clsid(&c2);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c1, ACTIVEOBJECT_WEAK, &k1), S_OK);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c2, ACTIVEOBJECT_WEAK, &k2), S_OK);   /* different CLSID: no "already registered" */
    CHECK(k1 != k2);
    EXPECT_EQ(OA(RevokeActiveObject)(k1, NULL), S_OK);
    EXPECT_EQ(OA(GetActiveObject)(&c1, NULL, &got), MK_E_UNAVAIL);
    got = NULL;
    EXPECT_EQ(OA(GetActiveObject)(&c2, NULL, &got), S_OK);
    if (got) IUnknown_Release(got);
    EXPECT_EQ(OA(RevokeActiveObject)(k2, NULL), S_OK);
    ctx_close(&x);
}

TEST(dtd_ao_revoke_twice_fails_second_time)
{
    CLSID c; Ctx x; DWORD ck = 0; HRESULT hr;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck), S_OK);
    EXPECT_EQ(OA(RevokeActiveObject)(ck, NULL), S_OK);
    hr = OA(RevokeActiveObject)(ck, NULL);
    CHECKF(FAILED(hr), "second revoke hr=%08lx", (unsigned long)hr);
    t_note("second RevokeActiveObject(cookie) = %08lx", (unsigned long)hr);
    ctx_close(&x);
}

TEST(dtd_ao_revoke_invalid_cookies_fail)
{
    static const DWORD bad[] = { 0, 0xdeadbeef, 0xffffffff, 0x7fffffff };
    unsigned i; Ctx x;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = OA(RevokeActiveObject)(bad[i], NULL);
        CHECKF(FAILED(hr), "cookie %08lx hr=%08lx", (unsigned long)bad[i], (unsigned long)hr);
        t_note("RevokeActiveObject(0x%08lx) = %08lx", (unsigned long)bad[i], (unsigned long)hr);
    }
    ctx_close(&x);
}

TEST(dtd_ao_cookie_from_other_clsid_registration_not_reusable)
{
    CLSID c1, c2; Ctx x; DWORD k1 = 0, k2 = 0; IUnknown *got = NULL;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c1); make_clsid(&c2);
    OA(RegisterActiveObject)(x.unk, &c1, ACTIVEOBJECT_WEAK, &k1);
    OA(RegisterActiveObject)(x.unk, &c2, ACTIVEOBJECT_WEAK, &k2);
    EXPECT_EQ(OA(RevokeActiveObject)(k1, NULL), S_OK);
    CHECK(FAILED(OA(RevokeActiveObject)(k1, NULL)));
    EXPECT_EQ(OA(GetActiveObject)(&c2, NULL, &got), S_OK);   /* revoking c1's cookie twice must not have touched c2 */
    if (got) IUnknown_Release(got);
    OA(RevokeActiveObject)(k2, NULL);
    ctx_close(&x);
}

TEST(dtd_ao_register_flags_variants_noted)
{
    static const DWORD fl[] = { 2, 3, 0x10, 0xffffffff };
    unsigned i; Ctx x;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    for (i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        CLSID c; DWORD ck = 0; HRESULT hr;
        make_clsid(&c);
        hr = OA(RegisterActiveObject)(x.unk, &c, fl[i], &ck);
        t_note("RegisterActiveObject(dwFlags=0x%lx) = %08lx cookie=%lx", (unsigned long)fl[i], (unsigned long)hr, (unsigned long)ck);
        if (SUCCEEDED(hr)) OA(RevokeActiveObject)(ck, NULL);
    }
    CHECK(1);
    ctx_close(&x);
}

TEST(dtd_ao_cookie_values_distinct_across_registrations)
{
    CLSID c[8]; DWORD k[8]; Ctx x; int i, j, ok = 0;
    REQUIRE(OA(RevokeActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    for (i = 0; i < 8; i++) { make_clsid(&c[i]); k[i] = 0; if (OA(RegisterActiveObject)(x.unk, &c[i], ACTIVEOBJECT_WEAK, &k[i]) == S_OK) ok++; }
    EXPECT_EQ(ok, 8);
    for (i = 0; i < 8; i++) for (j = i + 1; j < 8; j++) CHECKF(k[i] != k[j], "cookies %d and %d equal", i, j);
    for (i = 0; i < 8; i++) if (k[i]) OA(RevokeActiveObject)(k[i], NULL);
    ctx_close(&x);
}

TEST(dtd_ao_registered_plain_iunknown_object)
{
    /* any IUnknown can be registered; it is handed back through the ROT as an IUnknown */
    CLSID c; Ctx x; DWORD ck = 0; IUnknown *got = NULL; Demo plain;
    REQUIRE(OA(RevokeActiveObject)); REQUIRE(OA(GetActiveObject));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    Demo_init(&plain);
    make_clsid(&c);
    EXPECT_EQ(OA(RegisterActiveObject)((IUnknown *)&plain, &c, ACTIVEOBJECT_WEAK, &ck), S_OK);
    EXPECT_EQ(OA(GetActiveObject)(&c, NULL, &got), S_OK);
    CHECK(got != NULL);
    if (got) IUnknown_Release(got);
    EXPECT_EQ(OA(RevokeActiveObject)(ck, NULL), S_OK);
    ctx_close(&x);
}

/* ---- other threads / apartments ---- */
typedef struct TW { CLSID clsid; DWORD sta; HRESULT hr_get; int use; HRESULT hr_after; } TW;

static DWORD WINAPI ao_thread(LPVOID p)
{
    TW *w = p; IUnknown *got = NULL; HRESULT hr;
    hr = OL(CoInitializeEx)(NULL, w->sta ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    if (FAILED(hr)) { w->hr_get = hr; return 1; }
    w->hr_get = OA(GetActiveObject)(&w->clsid, NULL, &got);
    if (w->hr_get == S_OK && got) { w->use = use_dispatch(got); IUnknown_Release(got); }
    OL(CoUninitialize)();
    return 0;
}

static void run_thread_case(DWORD sta)
{
    CLSID c; Ctx x; DWORD ck = 0; TW w; HANDLE h;
    if (!OA(GetActiveObject) || !OL(CoInitializeEx)) { t_skip("missing exports"); return; }
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck), S_OK);
    memset(&w, 0, sizeof w); w.clsid = c; w.sta = sta; w.use = -1;
    h = CreateThread(NULL, 0, ao_thread, &w, 0, NULL);
    CHECK(h != NULL);
    if (h) {
        DWORD wr = WaitForSingleObject(h, 20000);
        EXPECT_EQ(wr, WAIT_OBJECT_0);
        if (wr != WAIT_OBJECT_0) TerminateThread(h, 1);
        CloseHandle(h);
    }
    EXPECT_EQ(w.hr_get, S_OK);
    EXPECT_EQ(w.use, 0);
    OA(RevokeActiveObject)(ck, NULL);
    ctx_close(&x);
}

TEST(dtd_ao_get_from_mta_thread)
{
    run_thread_case(0);
}

TEST(dtd_ao_get_from_sta_thread)
{
    run_thread_case(1);
}

static DWORD WINAPI ao_thread_unreg(LPVOID p)
{
    TW *w = p; IUnknown *got = NULL;
    if (FAILED(OL(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) return 1;
    w->hr_get = OA(GetActiveObject)(&w->clsid, NULL, &got);
    if (got) IUnknown_Release(got);
    OL(CoUninitialize)();
    return 0;
}

TEST(dtd_ao_get_unregistered_from_other_thread_is_unavailable)
{
    TW w; HANDLE h; CLSID c;
    REQUIRE(OA(GetActiveObject)); REQUIRE(OL(CoInitializeEx));
    make_clsid(&c);
    memset(&w, 0, sizeof w); w.clsid = c;
    h = CreateThread(NULL, 0, ao_thread_unreg, &w, 0, NULL);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(WaitForSingleObject(h, 20000), WAIT_OBJECT_0); CloseHandle(h); }
    EXPECT_EQ(w.hr_get, MK_E_UNAVAIL);
}

CHILD(ao_getter)
{
    /* argv[1] = clsid string, argv[2] = "weak"/"strong" (informational) */
    CLSID c; WCHAR ws[64]; IUnknown *got = NULL; HRESULT hr; int use; CLSID other;
    if (argc < 2) return 10;
    if (!OL(CoInitializeEx) || !OL(CLSIDFromString) || !OA(GetActiveObject)) return 11;
    if (FAILED(OL(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) return 12;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, ws, 64);
    if (FAILED(OL(CLSIDFromString)(ws, &c))) return 13;
    hr = OA(GetActiveObject)(&c, NULL, &got);
    if (hr != S_OK || !got) return 20;
    use = use_dispatch(got);
    IUnknown_Release(got);
    if (use) return 30 + use;
    other = c; other.Data4[7] ^= 0x55;
    got = NULL;
    hr = OA(GetActiveObject)(&other, NULL, &got);
    if (hr != MK_E_UNAVAIL) return 40;
    OL(CoUninitialize)();
    return 0;
}

static void run_child_case(DWORD flags)
{
    CLSID c; Ctx x; DWORD ck = 0; WCHAR ws[64]; char arg[160]; DWORD code;
    if (!OA(GetActiveObject) || !OL(StringFromGUID2)) { t_skip("missing exports"); return; }
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c, flags, &ck), S_OK);
    OL(StringFromGUID2)(&c, ws, 64);
    snprintf(arg, sizeof arg, "%ls %s", ws, flags ? "weak" : "strong");
    code = t_run_child("ao_getter", arg, 30000);
    CHECKF(code == 0, "child exit code %lu (0xDEAD0000 = spawn failed, 0xDEAD0001 = timeout)", (unsigned long)code);
    OA(RevokeActiveObject)(ck, NULL);
    ctx_close(&x);
}

TEST(dtd_ao_get_from_child_process_strong)
{
    run_child_case(ACTIVEOBJECT_STRONG);
}

TEST(dtd_ao_get_from_child_process_weak)
{
    run_child_case(ACTIVEOBJECT_WEAK);
}

CHILD(ao_nocominit)
{
    CLSID c; IUnknown *got = NULL; HRESULT hr;
    memset(&c, 0, sizeof c); c.Data1 = 0x0ae7c119; c.Data4[0] = 0x80; c.Data4[7] = 2;
    if (!OA(GetActiveObject)) return 11;
    hr = OA(GetActiveObject)(&c, NULL, &got);
    if (hr == MK_E_UNAVAIL) return 0;
    if (hr == CO_E_NOTINIT) return 1;
    return 2;
}

TEST(dtd_ao_get_without_com_init_in_fresh_process)
{
    DWORD code;
    REQUIRE(OA(GetActiveObject));
    code = t_run_child("ao_nocominit", "", 20000);
    CHECKF(code == 0 || code == 1, "child exit code %lu", (unsigned long)code);
    t_note("GetActiveObject without CoInitialize in a fresh process: %s", code == 0 ? "MK_E_UNAVAILABLE" : code == 1 ? "CO_E_NOTINITIALIZED" : "other");
}

TEST(dtd_ao_rot_entry_visible_via_class_moniker_noted)
{
    CLSID c; Ctx x; DWORD ck = 0; IRunningObjectTable *rot = NULL; IMoniker *mk = NULL; HRESULT hr;
    REQUIRE(OL(GetRunningObjectTable)); REQUIRE(OL(CreateClassMoniker));
    if (!ctx_open(&x)) { ctx_close(&x); return; }
    make_clsid(&c);
    EXPECT_EQ(OA(RegisterActiveObject)(x.unk, &c, ACTIVEOBJECT_WEAK, &ck), S_OK);
    if (OL(GetRunningObjectTable)(0, &rot) == S_OK && rot) {
        if (OL(CreateClassMoniker)(&c, &mk) == S_OK && mk) {
            hr = IRunningObjectTable_IsRunning(rot, mk);
            t_note("ROT IsRunning(class moniker of the registered CLSID) = %08lx", (unsigned long)hr);
            IMoniker_Release(mk);
        }
        IRunningObjectTable_Release(rot);
    }
    OA(RevokeActiveObject)(ck, NULL);
    CHECK(1);
    ctx_close(&x);
}
