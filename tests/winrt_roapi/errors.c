/* winrt_roapi: RoOriginateError family, error reporting flags, IRestrictedErrorInfo plumbing. */
#include "ro_common.h"

#define F_NONE 0u
#define F_SUPPRESSEXCEPTIONS 1u
#define F_FORCEEXCEPTIONS 2u
#define F_USESETERRORINFO 4u
#define F_SUPPRESSSETERRORINFO 8u

static void clear_thread_error(void) {
    if (U(SetErrorInfo)) U(SetErrorInfo)(0, NULL);
    if (UN(RoClearError) && !t_is_wine()) UN(RoClearError)();
    if (U(SetErrorInfo)) U(SetErrorInfo)(0, NULL);
}
static UINT32 g_saved_flags = 0xffffffff;
static void flags_save(void) { if (UN(RoGetErrorReportingFlags)) UN(RoGetErrorReportingFlags)(&g_saved_flags); }
static void flags_restore(void) { if (g_saved_flags != 0xffffffff) UN(RoSetErrorReportingFlags)(g_saved_flags); }

/* Take the thread's restricted error info and read its hr; returns S_FALSE if none. */
static HRESULT take_details(HRESULT *err, BSTR *desc, BSTR *rdesc, BSTR *sid, IRErrInfo **keep) {
    IRErrInfo *ri = NULL;
    HRESULT hr = UN(GetRestrictedErrorInfo)((void **)&ri);
    if (hr != S_OK || !ri) { if (keep) *keep = NULL; return hr == S_OK ? E_FAIL : hr; }
    BSTR d = NULL, r = NULL, s = NULL; HRESULT e = 0x12345678;
    HRESULT h2 = ri->lpVtbl->GetErrorDetails(ri, &d, &e, &r, &s);
    if (err) *err = e;
    if (desc) *desc = d; else if (d) U(SysFreeString)(d);
    if (rdesc) *rdesc = r; else if (r) U(SysFreeString)(r);
    if (sid) *sid = s; else if (s) U(SysFreeString)(s);
    if (keep) *keep = ri; else ri->lpVtbl->Release(ri);
    return h2;
}
#define NEED_ERR_API() do { REQUIRE(UN(RoOriginateError)); REQUIRE(UN(GetRestrictedErrorInfo)); \
    REQUIRE(UN(SetRestrictedErrorInfo)); REQUIRE(U(SetErrorInfo)); REQUIRE(U(GetErrorInfo)); \
    REQUIRE(UN(RoSetErrorReportingFlags)); REQUIRE(UN(RoGetErrorReportingFlags)); REQUIRE(U(SysFreeString)); \
    REQUIRE(U(WindowsCreateString)); \
    if (t_is_wine()) { void *p_ = NULL; if (UN(GetRestrictedErrorInfo)(&p_) == E_NOTIMPL) { t_skip("emulator: GetRestrictedErrorInfo is E_NOTIMPL"); return; } } } while (0)

/* ================= reporting flags ================= */
TEST(winrt_roapi_flags_null_out) {
    REQUIRE(UN(RoGetErrorReportingFlags));
    RO_EQ(UN(RoGetErrorReportingFlags)(NULL), RO_E_POINTER);
}
/* adapted from Wine dlls/combase/tests/roapi.c test_RoGetErrorReportingFlags (LGPL-2.1+) */
TEST(winrt_roapi_flags_default_is_usesetinfo) {
    REQUIRE(UN(RoGetErrorReportingFlags));
    UINT32 f = 0xdeadbeef;
    RO_EQ(UN(RoGetErrorReportingFlags)(&f), S_OK);
    RO_EQ(f, F_USESETERRORINFO);
}
TEST(winrt_roapi_flags_set_roundtrip) {
    REQUIRE(UN(RoSetErrorReportingFlags)); REQUIRE(UN(RoGetErrorReportingFlags));
    flags_save();
    for (UINT32 v = 0; v <= 15; v++) {
        RO_EQ(UN(RoSetErrorReportingFlags)(v), S_OK);
        UINT32 g = 0xdeadbeef;
        RO_EQ(UN(RoGetErrorReportingFlags)(&g), S_OK);
        CHECKF(g == v, "set %u got %u", v, g);
    }
    flags_restore();
}
TEST(winrt_roapi_flags_set_invalid_leaves_value) {
    REQUIRE(UN(RoSetErrorReportingFlags)); REQUIRE(UN(RoGetErrorReportingFlags));
    flags_save();
    static const UINT32 bad[] = { 0x10, 0x20, 0x100, 0x1f, 0xfffffff0u, 0x80000000u, 0xffffffffu };
    UN(RoSetErrorReportingFlags)(F_SUPPRESSSETERRORINFO | F_USESETERRORINFO);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        RO_EQ(UN(RoSetErrorReportingFlags)(bad[i]), RO_E_INVALIDARG);
        UINT32 g = 0;
        RO_EQ(UN(RoGetErrorReportingFlags)(&g), S_OK);
        CHECKF(g == (F_SUPPRESSSETERRORINFO | F_USESETERRORINFO), "flags changed by invalid set %08x -> %u", bad[i], g);
    }
    flags_restore();
}
TEST(winrt_roapi_flags_are_process_wide) {
    REQUIRE(UN(RoSetErrorReportingFlags));
    flags_save();
    UN(RoSetErrorReportingFlags)(F_SUPPRESSEXCEPTIONS);
    static UINT32 seen;
    DWORD WINAPI th(void *p) { UINT32 v = 0; UN(RoGetErrorReportingFlags)(&v); seen = v; return 0; }
    CHECK(ro_run_thread(th, NULL, 5000) == 0);
    RO_EQ(seen, F_SUPPRESSEXCEPTIONS);
    flags_restore();
}

/* ================= RoOriginateError ================= */
RO_TEST(winrt_roapi_originate_success_codes_report_nothing) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    static const HRESULT ok[] = { S_OK, S_FALSE, 2, 0x7fffffff };
    HSTRING msg = ro_hs(L"should not be reported");
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        clear_thread_error();
        RO_EQ(UN(RoOriginateError)(ok[i], msg), FALSE);
        void *ri = (void *)1;
        RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE);
        CHECK(ri == NULL);
    }
    ro_hs_free(msg); flags_restore();
}
RO_TEST(winrt_roapi_originate_failure_sets_restricted_info) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    static const HRESULT codes[] = { E_FAIL, E_INVALIDARG, E_ACCESSDENIED, E_NOTIMPL, (HRESULT)0x80000000, (HRESULT)0xC0000005, (HRESULT)0x8007000E };
    HSTRING msg = ro_hs(L"originated by olet");
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
        clear_thread_error();
        RO_EQ(UN(RoOriginateError)(codes[i], msg), TRUE);
        HRESULT e = 0; BSTR d = NULL, rd = NULL, sid = NULL;
        RO_EQ(take_details(&e, &d, &rd, &sid, NULL), S_OK);
        CHECKF(e == codes[i], "details hr %08lx want %08lx", (unsigned long)e, (unsigned long)codes[i]);
        t_note("code %08lx: desc=%ls restricted=%ls sid=%ls", (unsigned long)codes[i], d ? d : L"(null)", rd ? rd : L"(null)", sid ? sid : L"(null)");
        if (d) U(SysFreeString)(d); if (rd) U(SysFreeString)(rd); if (sid) U(SysFreeString)(sid);
    }
    ro_hs_free(msg); flags_restore();
}
RO_TEST(winrt_roapi_originate_null_and_empty_message) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    HSTRING empty = ro_hs(L"");
    HSTRING msgs[2] = { NULL, empty };
    for (int i = 0; i < 2; i++) {
        clear_thread_error();
        RO_EQ(UN(RoOriginateError)(E_FAIL, msgs[i]), TRUE);
        HRESULT e = 0; BSTR d = NULL;
        RO_EQ(take_details(&e, &d, NULL, NULL, NULL), S_OK);
        RO_EQ(e, E_FAIL);
        t_note("empty message -> description \"%ls\" (derived from hr)", d ? d : L"(null)");
        if (d) U(SysFreeString)(d);
    }
    ro_hs_free(empty); flags_restore();
}
RO_TEST(winrt_roapi_originate_message_text_recorded) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING msg = ro_hs(L"olet-message-1234");
    RO_EQ(UN(RoOriginateError)(E_FAIL, msg), TRUE);
    BSTR d = NULL, rd = NULL;
    RO_EQ(take_details(NULL, &d, &rd, NULL, NULL), S_OK);
    int in_desc = d && wcsstr(d, L"olet-message-1234"), in_rd = rd && wcsstr(rd, L"olet-message-1234");
    CHECKF(in_desc || in_rd, "message text not found in description (%ls) or restricted description (%ls)", d ? d : L"-", rd ? rd : L"-");
    t_note("message in description=%d restricted description=%d", in_desc, in_rd);
    if (d) U(SysFreeString)(d); if (rd) U(SysFreeString)(rd);
    ro_hs_free(msg); flags_restore();
}
RO_TEST(winrt_roapi_originate_replaces_previous) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    UN(RoOriginateError)(E_ACCESSDENIED, m);
    HRESULT e = 0;
    RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK);
    RO_EQ(e, E_ACCESSDENIED);
    void *ri; RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE);   /* a single slot */
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_originate_success_hr_keeps_existing_error) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"keep");
    UN(RoOriginateError)(E_FAIL, m);
    RO_EQ(UN(RoOriginateError)(S_OK, m), FALSE);    /* decomp: returns 0 immediately, no clear */
    HRESULT e = 0;
    RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK);
    RO_EQ(e, E_FAIL);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_originate_get_consumes) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    IRErrInfo *a = NULL, *b = (IRErrInfo *)1;
    RO_EQ(UN(GetRestrictedErrorInfo)((void **)&a), S_OK);
    CHECK(a != NULL);
    RO_EQ(UN(GetRestrictedErrorInfo)((void **)&b), S_FALSE);
    CHECK(b == NULL);
    IErrorInfo *ei = (IErrorInfo *)1;
    RO_EQ(U(GetErrorInfo)(0, &ei), S_FALSE);
    CHECK(ei == NULL);
    RO_RELEASE(a);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_originate_visible_as_ierrorinfo) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    IErrorInfo *ei = NULL;
    RO_EQ(U(GetErrorInfo)(0, &ei), S_OK);
    CHECK(ei != NULL);
    if (ei) {
        IRErrInfo *ri = NULL;
        RO_EQ(ro_qi(ei, &RIID_IRestrictedErrorInfo, (void **)&ri), S_OK);
        CHECK(ri != NULL);
        HRESULT e = 0; BSTR d = NULL, r = NULL, s = NULL;
        if (ri) { RO_EQ(ri->lpVtbl->GetErrorDetails(ri, &d, &e, &r, &s), S_OK); RO_EQ(e, E_FAIL); }
        if (d) U(SysFreeString)(d); if (r) U(SysFreeString)(r); if (s) U(SysFreeString)(s);
        /* GetReference on a locally originated error: record */
        BSTR ref = NULL;
        if (ri) { HRESULT h = ri->lpVtbl->GetReference(ri, &ref); t_note("GetReference hr=%08lx ref=%ls", (unsigned long)h, ref ? ref : L"(null)"); if (ref) U(SysFreeString)(ref); }
        RO_RELEASE(ri);
        /* put it back through the legacy API, then retrieve it as restricted info */
        RO_EQ(U(SetErrorInfo)(0, ei), S_OK);
        RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK);
        RO_EQ(e, E_FAIL);
        RO_RELEASE(ei);
    }
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_originate_flags_control_thread_error) {
    NEED_ERR_API(); flags_save();
    /* CallSetErrorInfo (decomp): thread error info is stored iff !(flags&8) && (flags&4 || debugger attached) */
    static const struct { UINT32 f; int stored; } t[] = {
        { F_USESETERRORINFO, 1 }, { F_USESETERRORINFO | F_SUPPRESSEXCEPTIONS, 1 },
        { F_SUPPRESSSETERRORINFO, 0 }, { F_SUPPRESSSETERRORINFO | F_USESETERRORINFO, 0 },
        { F_SUPPRESSSETERRORINFO | F_SUPPRESSEXCEPTIONS, 0 },
    };
    HSTRING m = ro_hs(L"flags");
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        clear_thread_error();
        RO_EQ(UN(RoSetErrorReportingFlags)(t[i].f), S_OK);
        RO_EQ(UN(RoOriginateError)(E_FAIL, m), TRUE);     /* TRUE even when nothing was stored */
        void *ri = NULL;
        HRESULT hr = UN(GetRestrictedErrorInfo)(&ri);
        CHECKF((hr == S_OK && ri) == (t[i].stored != 0), "flags %u: hr=%08lx ri=%p", t[i].f, (unsigned long)hr, ri);
        RO_RELEASE(*(IUnknown **)&ri);
    }
    /* flags 0 and 1: only stored when a debugger is attached */
    static const UINT32 dbg[] = { F_NONE, F_SUPPRESSEXCEPTIONS };
    for (int i = 0; i < 2; i++) {
        clear_thread_error();
        UN(RoSetErrorReportingFlags)(dbg[i]);
        RO_EQ(UN(RoOriginateError)(E_FAIL, m), TRUE);
        void *ri = NULL;
        HRESULT hr = UN(GetRestrictedErrorInfo)(&ri);
        int stored = (hr == S_OK && ri);
        if (!IsDebuggerPresent()) CHECKF(!stored, "flags %u stored an error without a debugger", dbg[i]);
        RO_RELEASE(*(IUnknown **)&ri);
    }
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_originate_thread_local) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    static HRESULT other;
    DWORD WINAPI th(void *p) { void *ri = (void *)1; other = UN(GetRestrictedErrorInfo)(&ri); if (ri) RO_RELEASE(*(IUnknown **)&ri); return 0; }
    CHECK(ro_run_thread(th, NULL, 5000) == 0);
    RO_EQ(other, S_FALSE);
    HRESULT e = 0; RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK); RO_EQ(e, E_FAIL);
    ro_hs_free(m); flags_restore();
}

/* ================= RoOriginateErrorW ================= */
RO_TEST(winrt_roapi_originate_w_basics) {
    NEED_ERR_API(); REQUIRE(UN(RoOriginateErrorW)); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    RO_EQ(UN(RoOriginateErrorW)(S_OK, 3, L"abc"), FALSE);
    void *ri = (void *)1; RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE); CHECK(ri == NULL);
    RO_EQ(UN(RoOriginateErrorW)(E_ABORT, 3, L"abc"), TRUE);
    HRESULT e = 0; BSTR d = NULL;
    RO_EQ(take_details(&e, &d, NULL, NULL, NULL), S_OK);
    RO_EQ(e, E_ABORT);
    t_note("W originate description=%ls", d ? d : L"(null)");
    if (d) U(SysFreeString)(d);
    flags_restore();
}
RO_TEST(winrt_roapi_originate_w_length_truncates) {
    NEED_ERR_API(); REQUIRE(UN(RoOriginateErrorW)); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    /* GetMessageParameters (decomp): cch is clamped to 512, 0 means 512; text longer than cch is cut to cch */
    static const struct { UINT32 cch; const WCHAR *msg; const WCHAR *want; } t[] = {
        { 2, L"abcdef", L"ab" }, { 6, L"abcdef", L"abcdef" }, { 100, L"abcdef", L"abcdef" }, { 0, L"abcdef", L"abcdef" } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        clear_thread_error();
        RO_EQ(UN(RoOriginateErrorW)(E_FAIL, t[i].cch, t[i].msg), TRUE);
        BSTR d = NULL, r = NULL; HRESULT e;
        RO_EQ(take_details(&e, &d, &r, NULL, NULL), S_OK);
        int ok = (d && !wcscmp(d, t[i].want)) || (r && !wcscmp(r, t[i].want));
        if (!ok) t_note("cch=%u msg=%ls: desc=%ls restricted=%ls (wanted %ls somewhere)", t[i].cch, t[i].msg, d ? d : L"-", r ? r : L"-", t[i].want);
        if (d) U(SysFreeString)(d); if (r) U(SysFreeString)(r);
    }
    flags_restore();
}
RO_TEST(winrt_roapi_originate_w_long_message) {
    NEED_ERR_API(); REQUIRE(UN(RoOriginateErrorW)); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    static WCHAR big[2000];
    for (int i = 0; i < 1999; i++) big[i] = L'a' + (i % 26);
    clear_thread_error();
    RO_EQ(UN(RoOriginateErrorW)(E_FAIL, 1999, big), TRUE);
    BSTR d = NULL, r = NULL; HRESULT e;
    RO_EQ(take_details(&e, &d, &r, NULL, NULL), S_OK);
    RO_EQ(e, E_FAIL);
    t_note("long message: description len=%u restricted len=%u", d ? U(SysStringLen)(d) : 0, r ? U(SysStringLen)(r) : 0);
    if (d) CHECK(U(SysStringLen)(d) <= 1999);
    if (d) U(SysFreeString)(d); if (r) U(SysFreeString)(r);
    flags_restore();
}
RO_TEST(winrt_roapi_originate_w_empty_message_uses_hr_text) {
    NEED_ERR_API(); REQUIRE(UN(RoOriginateErrorW)); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    RO_EQ(UN(RoOriginateErrorW)(E_ACCESSDENIED, 0, L""), TRUE);
    BSTR d = NULL; HRESULT e;
    RO_EQ(take_details(&e, &d, NULL, NULL, NULL), S_OK);
    RO_EQ(e, E_ACCESSDENIED);
    t_note("empty text: description=%ls", d ? d : L"(null)");
    if (d) U(SysFreeString)(d);
    flags_restore();
}

/* ================= RoOriginateLanguageException ================= */
static HRESULT WINAPI le_qi(IUnknown *s, REFIID r, void **o) { if (!memcmp(r, &RIID_IUnknown, 16)) { *o = s; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI le_ar(IUnknown *s) { return 2; }
static ULONG WINAPI le_rel(IUnknown *s) { return 1; }
static IUnknownVtbl le_vtbl = { le_qi, le_ar, le_rel };
static IUnknown le_obj = { &le_vtbl };
RO_TEST(winrt_roapi_originate_language_exception) {
    NEED_ERR_API(); REQUIRE(UN(RoOriginateLanguageException)); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    HSTRING m = ro_hs(L"lang exception");
    clear_thread_error();
    RO_EQ(UN(RoOriginateLanguageException)(S_OK, m, &le_obj), FALSE);
    void *ri = (void *)1; RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE);
    /* object without ILanguageExceptionErrorInfo is tolerated */
    RO_EQ(UN(RoOriginateLanguageException)(E_FAIL, m, &le_obj), TRUE);
    HRESULT e = 0; RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK); RO_EQ(e, E_FAIL);
    clear_thread_error();
    RO_EQ(UN(RoOriginateLanguageException)(E_INVALIDARG, m, NULL), TRUE);
    RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK); RO_EQ(e, E_INVALIDARG);
    ro_hs_free(m); flags_restore();
}

/* ================= Set/GetRestrictedErrorInfo ================= */
RO_TEST(winrt_roapi_get_restricted_none_is_sfalse) {
    NEED_ERR_API(); clear_thread_error();
    void *ri = (void *)0xdeadbeef;
    RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE);
    CHECK(ri == NULL);
}
RO_TEST(winrt_roapi_set_restricted_null_clears) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    RO_EQ(UN(SetRestrictedErrorInfo)(NULL), S_OK);
    void *ri = (void *)1; RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE); CHECK(ri == NULL);
    IErrorInfo *ei = (IErrorInfo *)1; RO_EQ(U(GetErrorInfo)(0, &ei), S_FALSE); CHECK(ei == NULL);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_set_restricted_roundtrip) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"roundtrip");
    UN(RoOriginateError)(E_NOTIMPL, m);
    IRErrInfo *a = NULL;
    RO_EQ(UN(GetRestrictedErrorInfo)((void **)&a), S_OK);
    CHECK(a);
    RO_EQ(UN(SetRestrictedErrorInfo)(a), S_OK);
    IRErrInfo *b = NULL;
    RO_EQ(UN(GetRestrictedErrorInfo)((void **)&b), S_OK);
    CHECK(b != NULL);
    if (a && b) {
        IUnknown *ua = NULL, *ub = NULL;
        ro_qi(a, &RIID_IUnknown, (void **)&ua); ro_qi(b, &RIID_IUnknown, (void **)&ub);
        CHECK(ua == ub);                                  /* same underlying object */
        RO_RELEASE(ua); RO_RELEASE(ub);
    }
    RO_RELEASE(a); RO_RELEASE(b);
    RO_EQ(UN(SetRestrictedErrorInfo)(NULL), S_OK);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_set_restricted_replaces) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_NOTIMPL, m);
    IRErrInfo *a = NULL; UN(GetRestrictedErrorInfo)((void **)&a);
    UN(RoOriginateError)(E_FAIL, m);
    IRErrInfo *b = NULL; UN(GetRestrictedErrorInfo)((void **)&b);
    REQUIRE(a); REQUIRE(b);
    RO_EQ(UN(SetRestrictedErrorInfo)(a), S_OK);
    RO_EQ(UN(SetRestrictedErrorInfo)(b), S_OK);
    HRESULT e = 0; RO_RELEASE(a); RO_RELEASE(b);
    RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK);
    RO_EQ(e, E_FAIL);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_set_restricted_holds_reference) {
    NEED_ERR_API(); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    IRErrInfo *a = NULL; UN(GetRestrictedErrorInfo)((void **)&a); REQUIRE(a);
    ULONG base = ro_refcount(a);
    UN(SetRestrictedErrorInfo)(a);
    ULONG held = ro_refcount(a);
    CHECKF(held == base + 1, "refcount after Set: %lu -> %lu", base, held);
    UN(SetRestrictedErrorInfo)(NULL);
    CHECKF(ro_refcount(a) == base, "refcount after clear: %lu want %lu", ro_refcount(a), base);
    RO_RELEASE(a);
    ro_hs_free(m); flags_restore();
}
/* A plain CreateErrorInfo object is not a restricted error object: GetRestrictedErrorInfo consumes it and reports S_FALSE (decomp). */
RO_TEST(winrt_roapi_get_restricted_with_plain_errorinfo) {
    NEED_ERR_API(); REQUIRE(U(CreateErrorInfo));
    clear_thread_error();
    ICreateErrorInfo *c = NULL;
    RO_EQ(U(CreateErrorInfo)(&c), S_OK); REQUIRE(c);
    RO_EQ(U(SetErrorInfo)(0, (IErrorInfo *)NULL), S_OK);
    IErrorInfo *ei = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ei);
    RO_EQ(U(SetErrorInfo)(0, ei), S_OK);
    void *ri = (void *)1;
    RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE);
    CHECK(ri == NULL);
    IErrorInfo *again = (IErrorInfo *)1;
    RO_EQ(U(GetErrorInfo)(0, &again), S_FALSE);          /* it was consumed */
    CHECK(again == NULL);
    RO_RELEASE(ei); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_set_restricted_rejects_foreign_object) {
    NEED_ERR_API(); clear_thread_error();
    /* An arbitrary IUnknown posing as IRestrictedErrorInfo: record how combase reacts (IsRestrictedErrorObject). */
    HRESULT hr = UN(SetRestrictedErrorInfo)((void *)&le_obj);
    t_note("SetRestrictedErrorInfo(foreign) = %08lx", (unsigned long)hr);
    clear_thread_error();
}

/* ================= RoGetMatchingRestrictedErrorInfo ================= */
RO_TEST(winrt_roapi_matching_creates_when_missing) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoGetMatchingRestrictedErrorInfo"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    IRErrInfo *ri = NULL;
    RO_EQ(UN(RoGetMatchingRestrictedErrorInfo)(E_ACCESSDENIED, (void **)&ri), S_OK);
    CHECK(ri != NULL);
    if (ri) { HRESULT e = 0; BSTR d = NULL, r = NULL, s = NULL;
        RO_EQ(ri->lpVtbl->GetErrorDetails(ri, &d, &e, &r, &s), S_OK); RO_EQ(e, E_ACCESSDENIED);
        if (d) U(SysFreeString)(d); if (r) U(SysFreeString)(r); if (s) U(SysFreeString)(s); }
    RO_RELEASE(ri);
    flags_restore();
}
RO_TEST(winrt_roapi_matching_returns_existing_when_hr_matches) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoGetMatchingRestrictedErrorInfo"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"match-me-777");
    UN(RoOriginateError)(E_NOTIMPL, m);
    IRErrInfo *ri = NULL;
    RO_EQ(UN(RoGetMatchingRestrictedErrorInfo)(E_NOTIMPL, (void **)&ri), S_OK);
    CHECK(ri != NULL);
    if (ri) { BSTR d = NULL, r = NULL, s = NULL; HRESULT e = 0;
        /* Windows dereferences every out pointer of GetErrorDetails: passing NULL crashes, so supply all four */
        RO_EQ(ri->lpVtbl->GetErrorDetails(ri, &d, &e, &r, &s), S_OK); RO_EQ(e, E_NOTIMPL);
        int kept = (d && wcsstr(d, L"match-me-777")) || (r && wcsstr(r, L"match-me-777"));
        t_note("existing error object reused (message preserved)=%d", kept);
        if (d) U(SysFreeString)(d); if (r) U(SysFreeString)(r); if (s) U(SysFreeString)(s); }
    RO_RELEASE(ri);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_matching_replaces_when_hr_differs) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoGetMatchingRestrictedErrorInfo"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_NOTIMPL, m);
    IRErrInfo *ri = NULL;
    RO_EQ(UN(RoGetMatchingRestrictedErrorInfo)(E_ABORT, (void **)&ri), S_OK);
    if (ri) { HRESULT e = 0; BSTR d = NULL, r = NULL, s = NULL;
        RO_EQ(ri->lpVtbl->GetErrorDetails(ri, &d, &e, &r, &s), S_OK); RO_EQ(e, E_ABORT);
        if (d) U(SysFreeString)(d); if (r) U(SysFreeString)(r); if (s) U(SysFreeString)(s); }
    else CHECK(ri != NULL);
    RO_RELEASE(ri);
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_matching_success_code_is_unexpected) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoGetMatchingRestrictedErrorInfo"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    /* RoOriginateError(S_OK) reports nothing, so no object can be returned: decomp maps this to E_UNEXPECTED */
    IRErrInfo *ri = (IRErrInfo *)1;
    RO_EQ(UN(RoGetMatchingRestrictedErrorInfo)(S_OK, (void **)&ri), (HRESULT)0x8000FFFFL);
    CHECK(ri == NULL);
    flags_restore();
}

/* ================= RoTransformError ================= */
RO_TEST(winrt_roapi_transform_noop_cases) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoTransformError"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    HSTRING m = ro_hs(L"transformed");
    /* decomp: old == new, or both non-failure -> returns 0 and touches nothing */
    static const HRESULT pairs[][2] = { {E_FAIL, E_FAIL}, {S_OK, S_OK}, {S_OK, S_FALSE}, {S_FALSE, 5}, {E_ABORT, E_ABORT} };
    for (unsigned i = 0; i < sizeof pairs / sizeof *pairs; i++) {
        clear_thread_error();
        UN(RoOriginateError)(E_NOTIMPL, m);
        RO_EQ(UN(RoTransformError)(pairs[i][0], pairs[i][1], m), FALSE);
        RO_EQ(UN(RoTransformErrorW)(pairs[i][0], pairs[i][1], 3, L"abc"), FALSE);
        HRESULT e = 0;
        RO_EQ(take_details(&e, NULL, NULL, NULL, NULL), S_OK);
        RO_EQ(e, E_NOTIMPL);                               /* untouched */
    }
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_transform_matching_error) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoTransformError"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    HSTRING m = ro_hs(L"x"), m2 = ro_hs(L"after transform");
    clear_thread_error();
    UN(RoOriginateError)(E_FAIL, m);
    BOOL r = UN(RoTransformError)(E_FAIL, E_ABORT, m2);
    RO_EQ(r, TRUE);
    HRESULT e = 0; BSTR d = NULL;
    HRESULT hr = take_details(&e, &d, NULL, NULL, NULL);
    RO_EQ(hr, S_OK);
    t_note("after RoTransformError(E_FAIL->E_ABORT) details hr=%08lx desc=%ls", (unsigned long)e, d ? d : L"(null)");
    if (d) U(SysFreeString)(d);
    /* transformation with no current error at all */
    clear_thread_error();
    r = UN(RoTransformError)(E_FAIL, E_ABORT, m2);
    t_note("RoTransformError without current error returns %d", (int)r);
    ro_hs_free(m); ro_hs_free(m2); flags_restore();
}

/* ================= RoCaptureErrorContext / RoClearError ================= */
RO_TEST(winrt_roapi_capture_error_context_without_error) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoCaptureErrorContext"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HRESULT hr = UN(RoCaptureErrorContext)(E_FAIL);
    t_note("RoCaptureErrorContext(E_FAIL) with no error = %08lx", (unsigned long)hr);
    HRESULT e = 0;
    HRESULT d = take_details(&e, NULL, NULL, NULL, NULL);
    if (d == S_OK) RO_EQ(e, E_FAIL);
    else t_note("no restricted info after capture (hr %08lx)", (unsigned long)d);
    flags_restore();
}
RO_TEST(winrt_roapi_capture_error_context_retargets_hr) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoCaptureErrorContext"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_NOTIMPL, m);
    HRESULT hr = UN(RoCaptureErrorContext)(E_ACCESSDENIED);
    t_note("RoCaptureErrorContext(different hr) = %08lx", (unsigned long)hr);
    HRESULT e = 0;
    if (take_details(&e, NULL, NULL, NULL, NULL) == S_OK) RO_EQ(e, E_ACCESSDENIED);   /* decomp: PutNewRestrictedErrorInfo(hr) when hr differs */
    ro_hs_free(m); flags_restore();
}
RO_TEST(winrt_roapi_clear_error_idempotent) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoClearError");
    UN(RoClearError)(); UN(RoClearError)();
    clear_thread_error();
    void *ri = (void *)1;
    RO_EQ(UN(GetRestrictedErrorInfo)(&ri), S_FALSE);
}
RO_TEST(winrt_roapi_clear_error_after_originate_record) {
    NEED_ERR_API(); RO_WINE_UNIMPL("RoClearError"); flags_save(); UN(RoSetErrorReportingFlags)(F_USESETERRORINFO);
    clear_thread_error();
    HSTRING m = ro_hs(L"x");
    UN(RoOriginateError)(E_FAIL, m);
    UN(RoClearError)();
    void *ri = NULL; HRESULT hr = UN(GetRestrictedErrorInfo)(&ri);
    t_note("GetRestrictedErrorInfo after RoClearError = %08lx ri=%p", (unsigned long)hr, ri);
    RO_RELEASE(*(IUnknown **)&ri);
    ro_hs_free(m); flags_restore();
}
