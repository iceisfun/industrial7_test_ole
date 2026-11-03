/* combase: RoGetActivatableClassRegistration, RoResolveRestrictedErrorInfoReference, RoReportUnhandledError,
 * RoReportFailedDelegate, RoFailFastWithErrorContext. */
#include "gap8.h"
#include <inspectable.h>

UDECL(HRESULT, RoGetActivatableClassRegistration, (HSTRING, void **));
UDECL(HRESULT, RoResolveRestrictedErrorInfoReference, (PCWSTR, void **));
UDECL(HRESULT, RoReportUnhandledError, (void *));
UDECL(HRESULT, RoReportFailedDelegate, (IUnknown *, void *));
UDECL(void, RoFailFastWithErrorContext, (HRESULT));
UDECL(BOOL, RoOriginateError, (HRESULT, HSTRING));
UDECL(HRESULT, GetRestrictedErrorInfo, (void **));
UDECL(void, RoClearError, (void));
UDECL(HRESULT, RoInitialize, (int));
UDECL(void, RoUninitialize, (void));

#define G8SFS(b) U(SysFreeString)(b)
/* ---------------- RoGetActivatableClassRegistration ---------------- */
#define NEEDAC() G8_REQ_NW(UN(RoGetActivatableClassRegistration))

TEST(gap8_ro_actreg_null_class_name)
{
    void *out = (void *)(ULONG_PTR)0x1234; NEEDAC();
    G8_EQ(UN(RoGetActivatableClassRegistration)(NULL, &out), G8_E_INVALIDARG);
    CHECK(out == NULL);                                     /* the output is cleared before the argument check */
}

TEST(gap8_ro_actreg_embedded_nul_class_name)
{
    static const WCHAR s[] = L"Windows\0Foundation.Uri";
    HSTRING h = NULL; void *out = (void *)(ULONG_PTR)0x1234; NEEDAC(); REQUIRE(U(WindowsCreateString));
    G8_EQ(U(WindowsCreateString)(s, 22, &h), S_OK);
    G8_EQ(UN(RoGetActivatableClassRegistration)(h, &out), G8_E_INVALIDARG);
    CHECK(out == NULL);
    /* the verdict is stable on a second call (the DLL caches the embedded-NUL flag in the header) */
    out = (void *)(ULONG_PTR)0x1234;
    G8_EQ(UN(RoGetActivatableClassRegistration)(h, &out), G8_E_INVALIDARG);
    CHECK(out == NULL);
    U(WindowsDeleteString)(h);
}

TEST(gap8_ro_actreg_reference_string_with_embedded_nul)
{
    static const WCHAR s[] = L"A.B\0C";
    HSTRING_HEADER hdr; HSTRING h = NULL; void *out = (void *)(ULONG_PTR)0x1234; NEEDAC(); REQUIRE(U(WindowsCreateStringReference));
    G8_EQ(U(WindowsCreateStringReference)(s, 5, &hdr, &h), S_OK);
    G8_EQ(UN(RoGetActivatableClassRegistration)(h, &out), G8_E_INVALIDARG);
    CHECK(out == NULL);
}

TEST(gap8_ro_actreg_unregistered_class_fails_with_null_output)
{
    WCHAR name[96]; HSTRING h = NULL; void *out = (void *)(ULONG_PTR)0x1234; HRESULT hr; NEEDAC(); REQUIRE(U(WindowsCreateString));
    _snwprintf(name, 96, L"Olet.Gap8.NoSuchRuntimeClass%lu", GetCurrentProcessId()); name[95] = 0;
    G8_EQ(U(WindowsCreateString)(name, lstrlenW(name), &h), S_OK);
    hr = UN(RoGetActivatableClassRegistration)(h, &out);
    CHECKF(FAILED(hr), "hr=0x%lx", hr);
    CHECK(out == NULL);
    t_note("RoGetActivatableClassRegistration(unregistered) -> 0x%08lx", hr);
    U(WindowsDeleteString)(h);
}

TEST(gap8_ro_actreg_leading_nul_and_length_one)
{
    static const WCHAR s[] = L"\0";
    HSTRING h = NULL; void *out = (void *)(ULONG_PTR)0x1234; NEEDAC(); REQUIRE(U(WindowsCreateString));
    G8_EQ(U(WindowsCreateString)(s, 1, &h), S_OK);
    G8_EQ(UN(RoGetActivatableClassRegistration)(h, &out), G8_E_INVALIDARG);
    CHECK(out == NULL);
    U(WindowsDeleteString)(h);
}

TEST(gap8_ro_actreg_no_com_init_required_for_validation)
{
    /* argument validation happens before any COM state is touched: a thread that never called CoInitialize gets the same answers */
    void *out = (void *)(ULONG_PTR)0x1234; NEEDAC();
    G8_EQ(UN(RoGetActivatableClassRegistration)(NULL, &out), G8_E_INVALIDARG);
    CHECK(out == NULL);
}

/* ---------------- RoResolveRestrictedErrorInfoReference ---------------- */
#define NEEDRR() G8_REQ_NW(UN(RoResolveRestrictedErrorInfoReference))

TEST(gap8_ro_resolveref_invalid_reference_strings)
{
    static const WCHAR *const refs[] = { L"", L" ", L"not a reference", L"0", L"::::", L"1234:5678:9", L"{00000000-0000-0000-0000-000000000000}" };
    unsigned i; NEEDRR();
    for (i = 0; i < sizeof refs / sizeof *refs; i++) {
        void *out = (void *)(ULONG_PTR)0x1234; HRESULT hr = UN(RoResolveRestrictedErrorInfoReference)(refs[i], &out);
        CHECKF(FAILED(hr), "ref \"%ls\" -> 0x%lx", refs[i], hr);
        CHECKF(out == NULL, "ref \"%ls\" output not cleared", refs[i]);
        t_note("resolve(\"%ls\") -> 0x%08lx", refs[i], hr);
    }
}

static struct { HRESULT origin, got_details_hr; int ran; char why[96]; } g_rt;
static DWORD WINAPI roundtrip_thread(void *p)
{
    typedef struct IREI { const struct IREIVtbl { HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
        HRESULT (WINAPI *GetErrorDetails)(void *, BSTR *, HRESULT *, BSTR *, BSTR *); HRESULT (WINAPI *GetReference)(void *, BSTR *); } *v; } IREI;
    HRESULT hr; HSTRING msg = NULL; IREI *info = NULL, *info2 = NULL; BSTR ref = NULL, d1 = NULL, r1 = NULL, s1 = NULL, d2 = NULL, r2 = NULL, s2 = NULL; HRESULT e1 = 0, e2 = 0;
    static const WCHAR text[] = L"olet gap8 roundtrip";
    hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { strcpy(g_rt.why, "CoInitializeEx"); return 1; }
    U(WindowsCreateString)(text, lstrlenW(text), &msg);
    SetLastError(0);
    UN(RoOriginateError)(g_rt.origin, msg);
    hr = UN(GetRestrictedErrorInfo)((void **)&info);
    if (hr != S_OK || !info) { strcpy(g_rt.why, "GetRestrictedErrorInfo"); goto done; }
    hr = info->v->GetReference(info, &ref);
    if (hr != S_OK || !ref) { sprintf(g_rt.why, "GetReference hr=%lx", hr); goto done; }
    info->v->GetErrorDetails(info, &d1, &e1, &r1, &s1);
    hr = UN(RoResolveRestrictedErrorInfoReference)(ref, (void **)&info2);
    g_rt.got_details_hr = hr;
    if (hr != S_OK || !info2) { sprintf(g_rt.why, "resolve hr=%lx", hr); goto done; }
    hr = info2->v->GetErrorDetails(info2, &d2, &e2, &r2, &s2);
    if (hr != S_OK) { sprintf(g_rt.why, "GetErrorDetails hr=%lx", hr); goto done; }
    t_check(e1 == g_rt.origin, __FILE__, __LINE__, "original error code", "0x%lx", e1);
    t_check(e2 == e1, __FILE__, __LINE__, "resolved error code equals original", "0x%lx vs 0x%lx", e2, e1);
    t_check((d1 == NULL) == (d2 == NULL) && (!d1 || !lstrcmpW(d1, d2)), __FILE__, __LINE__, "description equal", NULL);
    t_check((r1 == NULL) == (r2 == NULL) && (!r1 || !lstrcmpW(r1, r2)), __FILE__, __LINE__, "restricted description equal", NULL);
    t_check((s1 == NULL) == (s2 == NULL) && (!s1 || !lstrcmpW(s1, s2)), __FILE__, __LINE__, "capability sid equal", NULL);
    t_note("reference string: %ls", ref);
    g_rt.ran = 1;
done:
    { BSTR all[7] = { d1, r1, s1, d2, r2, s2, ref }; int k; for (k = 0; k < 7; k++) if (all[k]) G8SFS(all[k]); }
    if (info2) info2->v->Release(info2);
    if (info) info->v->Release(info);
    if (msg) U(WindowsDeleteString)(msg);
    if (UN(RoClearError)) UN(RoClearError)();
    U(CoUninitialize)();
    return 0;
}

TEST(gap8_ro_resolveref_roundtrip_same_process)
{
    NEEDRR(); REQUIRE(UN(RoOriginateError)); REQUIRE(UN(GetRestrictedErrorInfo)); REQUIRE(U(WindowsCreateString)); REQUIRE(U(CoInitializeEx));
    memset(&g_rt, 0, sizeof g_rt); g_rt.origin = (HRESULT)0x80070005;
    G8_EQ(g8_run_thread(roundtrip_thread, NULL, 20000), 0);
    CHECKF(g_rt.ran, "roundtrip did not complete: %s", g_rt.why);
}

TEST(gap8_ro_resolveref_roundtrip_other_codes)
{
    static const HRESULT codes[] = { (HRESULT)0x80004005, (HRESULT)0x8000FFFF, (HRESULT)0x80070057, (HRESULT)0x800401F0 };
    unsigned i; NEEDRR(); REQUIRE(UN(RoOriginateError)); REQUIRE(UN(GetRestrictedErrorInfo)); REQUIRE(U(WindowsCreateString)); REQUIRE(U(CoInitializeEx));
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        memset(&g_rt, 0, sizeof g_rt); g_rt.origin = codes[i];
        G8_EQ(g8_run_thread(roundtrip_thread, NULL, 20000), 0);
        CHECKF(g_rt.ran, "code 0x%lx: %s", codes[i], g_rt.why);
    }
}

/* ---------------- mock IRestrictedErrorInfo / IUnknown ---------------- */
typedef struct MOCK { const void **v; LONG ref; int nqi; GUID qi[16]; } MOCK;
static HRESULT WINAPI m_qi(MOCK *m, REFIID r, void **o)
{
    if (m->nqi < 16) m->qi[m->nqi] = *r;
    m->nqi++;
    *o = NULL; return G8_E_NOINTERFACE;
}
static ULONG WINAPI m_add(MOCK *m) { return InterlockedIncrement(&m->ref); }
static ULONG WINAPI m_rel(MOCK *m) { return InterlockedDecrement(&m->ref); }
static HRESULT WINAPI m_nimpl(MOCK *m) { return E_NOTIMPL; }
static const void *mock_vtbl[] = { m_qi, m_add, m_rel, m_nimpl, m_nimpl, m_nimpl, m_nimpl };
static void mock_init(MOCK *m) { memset(m, 0, sizeof *m); m->v = mock_vtbl; m->ref = 1; }

static int has_iid(const MOCK *m, const GUID *g)
{
    int i; for (i = 0; i < m->nqi && i < 16; i++) if (!memcmp(&m->qi[i], g, sizeof *g)) return 1; return 0;
}

TEST(gap8_ro_report_unhandled_error_with_foreign_object_does_not_crash)
{
    /* The report is routed to the configured error forwarder (CallErrorForwarder). With a foreign IRestrictedErrorInfo
     * the DLL first asks it for its internal interface; the outcome after that depends on whether a forwarder class is
     * registered on the machine, so only robustness and the QI traffic are checked. */
    MOCK m; HRESULT hr; G8_REQ_NW(UN(RoReportUnhandledError)); mock_init(&m);
    hr = UN(RoReportUnhandledError)(&m);
    t_note("RoReportUnhandledError(foreign) -> 0x%08lx, QueryInterface calls: %d", hr, m.nqi);
    G8_EQ(m.ref, 1);                                        /* every reference the DLL took was released */
    if (m.nqi == 0) t_note("no QueryInterface traffic (forwarder one-time init failed?)");
}

TEST(gap8_ro_report_failed_delegate_with_foreign_objects)
{
    static const GUID iid_probe = { 0x5524fe34, 0x8da7, 0x40a8, { 0x81,0x65,0xe8,0xb3,0x7a,0x8b,0x4a,0x4b } };
    MOCK d, e; HRESULT hr; G8_REQ_NW(UN(RoReportFailedDelegate)); mock_init(&d); mock_init(&e);
    hr = UN(RoReportFailedDelegate)((IUnknown *)&d, &e);
    t_note("RoReportFailedDelegate(foreign) -> 0x%08lx; delegate QI=%d errinfo QI=%d", hr, d.nqi, e.nqi);
    CHECK(has_iid(&d, &iid_probe));                         /* the delegate is asked for the 5524fe34-... source interface first */
    G8_EQ(d.ref, 1); G8_EQ(e.ref, 1);
}

/* ---------------- RoFailFastWithErrorContext ---------------- */
CHILD(gap8_failfast)
{
    HRESULT hr = (HRESULT)(argc > 0 ? strtoul(argv[0], NULL, 16) : 0x80004005);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (!UN(RoFailFastWithErrorContext)) return 3;
    UN(RoFailFastWithErrorContext)(hr);
    return 4;                                               /* must never return */
}
TEST(gap8_ro_failfast_terminates_process)
{
    static const char *const args[] = { "80004005", "80070057", "00000000" };
    unsigned i; G8_REQ_NW(UN(RoFailFastWithErrorContext));
    for (i = 0; i < sizeof args / sizeof *args; i++) {
        DWORD rc = t_run_child("gap8_failfast", args[i], 20000);
        CHECKF(rc != 3 && rc != 4 && rc != 0xDEAD0000 && rc != 0xDEAD0001, "child exit 0x%lx for hr %s", rc, args[i]);
        CHECKF(rc == 0xC0000409, "fail-fast exit code 0x%lx for hr %s", rc, args[i]);   /* STATUS_STACK_BUFFER_OVERRUN / __fastfail */
    }
}
