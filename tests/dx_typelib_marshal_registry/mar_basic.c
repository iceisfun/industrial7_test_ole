/* Typelib-driven standard marshaling (PSOAInterface) of IOletCustom: scalars, strings, variants, safearrays, pointers. */
#include "mar.h"
#define M(c, name, ...) ((c)->lpVtbl->name((c), __VA_ARGS__))
#define M0(c, name) ((c)->lpVtbl->name((c)))

/* ------------------------------------------------------------------ proxy sanity */
static void cl_apt(void *p, void *ctx)
{
    CLI(c, p); LONG tid = 0, pid = 0, apt = -2; HRESULT hr;
    hr = M(c, ApartmentInfo, &tid, &pid, &apt);
    EQ(hr, S_OK);
    CHECK((DWORD)tid != GetCurrentThreadId());
    EQ(pid, GetCurrentProcessId());
    CHECKF(apt >= 0 && apt <= 5, "apartment type %ld", (long)apt);
    *(LONG *)ctx = apt;
}
TMTEST(dxtm_mar_calls_cross_apartments)
{
    int i;
    for (i = 0; i < 3; i++) {
        LONG apt = -1;
        CHECK(tm_run_cfg(&tm_cfgs[i], tm_mk_custom, &TM_IID_IOletCustom, cl_apt, &apt));
        if (tm_cfgs[i].srv_apt == COINIT_MULTITHREADED) EQ(apt, APTTYPE_MTA);
        else CHECKF(apt == APTTYPE_STA || apt == APTTYPE_MAINSTA, "server STA reports apartment type %ld", (long)apt);
    }
    tm_leakcheck();
}

static void cl_same_apt_raw(void *p, void *ctx) { }
TMTEST(dxtm_mar_same_apartment_is_not_proxied)
{
    /* Marshal to a stream and unmarshal on the same thread: the original pointer comes back, no proxy is involved. */
    IStream *stm = NULL; IOletCustom *obj, *back = NULL; HRESULT hr;
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    obj = tm_new_custom();
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&TM_IID_IOletCustom, (IUnknown *)obj, &stm), S_OK);
    hr = U(CoGetInterfaceAndReleaseStream)(stm, &TM_IID_IOletCustom, (void **)&back);
    EQ(hr, S_OK);
    CHECK(back == obj);
    if (back) M0(back, Release);
    M0(obj, Release);
    U(CoUninitialize)();
    tm_leakcheck();
}

/* ------------------------------------------------------------------ integers */
static const LONG add_vals[][2] = { {0, 0}, {1, 2}, {-5, 3}, {0x7fffffff, 1}, {-0x7fffffff - 1, -1}, {123456789, 987654321}, {-1, -1} };
static void cl_add(void *p, void *ctx)
{
    CLI(c, p); unsigned i;
    for (i = 0; i < sizeof add_vals / sizeof add_vals[0]; i++) {
        LONG r = 0x5a5a5a5a; HRESULT hr = M(c, AddLong, add_vals[i][0], add_vals[i][1], &r);
        EQ(hr, S_OK);
        CHECKF(r == (LONG)((ULONG)add_vals[i][0] + (ULONG)add_vals[i][1]), "AddLong(%ld,%ld)=%ld", (long)add_vals[i][0], (long)add_vals[i][1], (long)r);
    }
    CHECK(tm_is_proxy_call(c));
}
TMTEST(dxtm_mar_addlong) { RUN_CUSTOM(cl_add, NULL); }

static void cl_scalars_in(void *p, void *ctx)
{
    CLI(c, p); HRESULT hr;
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, ScalarsIn, 0xFE, -32768, -2147483647 - 1, 0x7edcba9876543210LL, -0.5f, 3.141592653589793, VARIANT_TRUE, 40000.75);
    EQ(hr, S_OK);
    EQ(g_last.b, 0xFE);
    EQ(g_last.s, -32768);
    EQ(g_last.l, -2147483647 - 1);
    CHECK(g_last.h == 0x7edcba9876543210LL);
    CHECK(g_last.f == -0.5f);
    CHECK(g_last.d == 3.141592653589793);
    EQ(g_last.vb, VARIANT_TRUE);
    CHECK(g_last.dt == 40000.75);
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, ScalarsIn, 0, 0, 0, 0, 0.0f, 0.0, VARIANT_FALSE, 0.0);
    EQ(hr, S_OK);
    EQ(g_last.calls, 1);
    EQ(g_last.vb, 0);
}
TMTEST(dxtm_mar_scalars_in) { RUN_CUSTOM(cl_scalars_in, NULL); }

static void cl_scalars_out(void *p, void *ctx)
{
    CLI(c, p); BYTE b = 0; SHORT s = 0; LONG l = 0; LONGLONG h = 0; FLOAT f = 0; DOUBLE d = 0; VARIANT_BOOL vb = 0; DATE dt = 0; HRESULT hr;
    hr = M(c, ScalarsOut, &b, &s, &l, &h, &f, &d, &vb, &dt);
    EQ(hr, S_OK);
    EQ(b, 0xAB); EQ(s, -1234); EQ(l, 0x12345678);
    CHECK(h == 0x1122334455667788LL);
    CHECK(f == 1.5f); CHECK(d == -2.25);
    EQ(vb, VARIANT_TRUE);
    CHECK(dt == 36526.5);
}
TMTEST(dxtm_mar_scalars_out) { RUN_CUSTOM(cl_scalars_out, NULL); }

static void cl_longptrs(void *p, void *ctx)
{
    CLI(c, p); LONG in = 41, out = 0x77777777, io = 1000;
    memset(&g_last, 0, sizeof g_last);
    EQ(M(c, LongPtrs, &in, &out, &io), S_OK);
    EQ(g_last.in_long, 41);
    EQ(g_last.inout_long_before, 1000);
    EQ(in, 41);          /* [in] pointer: a modification made by the server is not copied back */
    EQ(out, 42);         /* [out] */
    EQ(io, 1100);        /* [in,out] */
}
TMTEST(dxtm_mar_pointer_directions) { RUN_CUSTOM(cl_longptrs, NULL); }

static void cl_cy(void *p, void *ctx)
{
    CLI(c, p); static const LONGLONG vals[] = { 0, 1, -1, 10000, -9876543210LL, 0x7fffffffffff0000LL }; unsigned i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        CY in, out; HRESULT hr;
        in.int64 = vals[i]; out.int64 = 0x5555;
        memset(&g_last, 0, sizeof g_last);
        hr = M(c, CurrencyEcho, in, &out);
        CHECKF(hr == S_OK, "CurrencyEcho hr=0x%lx", (long)hr);
        if (hr != S_OK) return;
        CHECKF(g_last.cy.int64 == vals[i], "server saw %lld want %lld", (long long)g_last.cy.int64, (long long)vals[i]);
        CHECKF(out.int64 == (LONGLONG)((ULONGLONG)vals[i] + 10000), "result %lld", (long long)out.int64);
    }
}
TMTEST(dxtm_mar_currency_by_value) { RUN_CUSTOM(cl_cy, NULL); }

/* ------------------------------------------------------------------ BSTR */
static BSTR mkbstr(UINT n, int seed)
{
    BSTR b = SysAllocStringLen(NULL, n); UINT i;
    for (i = 0; i < n; i++) b[i] = (WCHAR)(1 + ((i * 7 + seed) % 0xD000));
    return b;
}
static void bstr_roundtrip(IOletCustom *c, BSTR in, const char *what)
{
    BSTR out = NULL; HRESULT hr; UINT n = SysStringLen(in);
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, EchoBstr, in, &out);
    CHECKF(hr == S_OK, "%s: hr=0x%lx", what, (long)hr);
    CHECKF(out != NULL, "%s: result is NULL", what);
    CHECKF(g_last.strlen_ == n, "%s: server saw length %lu want %u", what, (unsigned long)g_last.strlen_, n);
    if (out) {
        CHECKF(SysStringLen(out) == n, "%s: result length %u want %u", what, SysStringLen(out), n);
        CHECKF(SysStringLen(out) == n && !memcmp(out, in, n * sizeof(WCHAR)), "%s: result differs", what);
        CHECKF(out[n] == 0, "%s: result is not NUL terminated", what);
        CHECKF(SysStringByteLen(out) == n * 2, "%s: byte length", what);
    }
    SysFreeString(out);
}
static void cl_bstr_echo(void *p, void *ctx)
{
    CLI(c, p); BSTR b; static const WCHAR emb[] = { L'a', 0, L'b', 0, L'c' }; static const WCHAR uni[] = { 0x4e2d, 0x6587, 0xd83d, 0xde00, 0x00e9, 0x0416 };
    b = SysAllocString(L"");           bstr_roundtrip(c, b, "empty"); SysFreeString(b);
    b = SysAllocString(L"a");          bstr_roundtrip(c, b, "one char"); SysFreeString(b);
    b = SysAllocString(L"Hello, world"); bstr_roundtrip(c, b, "ascii"); SysFreeString(b);
    b = SysAllocStringLen(emb, 5);     bstr_roundtrip(c, b, "embedded NUL"); SysFreeString(b);
    b = SysAllocStringLen(uni, 6);     bstr_roundtrip(c, b, "unicode"); SysFreeString(b);
    b = mkbstr(255, 1);                bstr_roundtrip(c, b, "255"); SysFreeString(b);
    b = mkbstr(1000, 2);               bstr_roundtrip(c, b, "1000"); SysFreeString(b);
}
TMTEST(dxtm_mar_bstr_echo) { RUN_CUSTOM(cl_bstr_echo, NULL); }

static void cl_bstr_big(void *p, void *ctx)
{
    CLI(c, p); BSTR b; static const UINT sizes[] = { 32767, 32768, 65535, 65536, 65537, 100000, 1000000 }; unsigned i; char what[32];
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        b = mkbstr(sizes[i], (int)i);
        sprintf(what, "%u chars", sizes[i]);
        bstr_roundtrip(c, b, what);
        SysFreeString(b);
    }
}
TMTEST(dxtm_mar_bstr_larger_than_64k) { RUN_CUSTOM(cl_bstr_big, NULL); }

static void cl_bstr_null(void *p, void *ctx)
{
    CLI(c, p); BSTR out = (BSTR)(ULONG_PTR)1; HRESULT hr;
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, EchoBstr, NULL, &out);
    EQ(hr, S_OK);
    CHECK(g_last.str_was_null);      /* NULL is preserved on the wire, it does not turn into "" */
    CHECK(out == NULL);
    out = NULL;
    {
        BSTR e = SysAllocString(L"");
        hr = M(c, EchoBstr, e, &out);
        EQ(hr, S_OK);
        CHECK(!g_last.str_was_null);
        CHECK(out != NULL);
        if (out) EQ(SysStringLen(out), 0);
        SysFreeString(e); SysFreeString(out);
    }
}
TMTEST(dxtm_mar_bstr_null_vs_empty) { RUN_CUSTOM(cl_bstr_null, NULL); }

static void cl_bstr_out(void *p, void *ctx)
{
    CLI(c, p); BSTR out = NULL; HRESULT hr;
    hr = M(c, BstrOut, &out);
    EQ(hr, S_OK);
    CHECK(out != NULL);
    if (out) { EXPECT_WSTR(out, L"server-out"); EQ(SysStringLen(out), 10); }
    SysFreeString(out);
    /* an [out] BSTR is never read: whatever the caller left in it is simply replaced (it is the caller's job to free it) */
    out = SysAllocString(L"previous");
    {
        BSTR keep = out;
        hr = M(c, BstrOut, &out);
        EQ(hr, S_OK);
        CHECK(out != NULL && out != keep);
        if (out) EXPECT_WSTR(out, L"server-out");
        SysFreeString(out); SysFreeString(keep);
    }
}
TMTEST(dxtm_mar_bstr_out_param) { RUN_CUSTOM(cl_bstr_out, NULL); }

static void cl_bstr_inout(void *p, void *ctx)
{
    CLI(c, p); BSTR io; HRESULT hr;
    io = SysAllocString(L"abc");
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, BstrInOut, &io);
    EQ(hr, S_OK);
    EQ(g_last.strlen_, 3);
    CHECK(io != NULL);
    if (io) { EXPECT_WSTR(io, L"abc+srv"); EQ(SysStringLen(io), 7); }
    SysFreeString(io);
    io = NULL;
    hr = M(c, BstrInOut, &io);
    EQ(hr, S_OK);
    CHECK(g_last.str_was_null);
    CHECK(io != NULL);
    if (io) EXPECT_WSTR(io, L"+srv");
    SysFreeString(io);
    io = mkbstr(70000, 5);
    hr = M(c, BstrInOut, &io);
    EQ(hr, S_OK);
    if (io) { EQ(SysStringLen(io), 70004); CHECK(io[70000] == L'+'); }
    SysFreeString(io);
}
TMTEST(dxtm_mar_bstr_inout_param) { RUN_CUSTOM(cl_bstr_inout, NULL); }

/* ------------------------------------------------------------------ VARIANT */
typedef struct { VARTYPE vt; int idx; } VCase;
static void fill_variant(VARIANT *v, int idx)
{
    VariantInit(v);
    switch (idx) {
    case 0: break;
    case 1: V_VT(v) = VT_NULL; break;
    case 2: V_VT(v) = VT_I2; V_I2(v) = -7; break;
    case 3: V_VT(v) = VT_I4; V_I4(v) = 0x7fffffff; break;
    case 4: V_VT(v) = VT_R4; V_R4(v) = 3.25f; break;
    case 5: V_VT(v) = VT_R8; V_R8(v) = -1.5e300; break;
    case 6: V_VT(v) = VT_CY; V_CY(v).int64 = -9876543210LL; break;
    case 7: V_VT(v) = VT_DATE; V_DATE(v) = 40000.25; break;
    case 8: V_VT(v) = VT_BSTR; V_BSTR(v) = SysAllocString(L"variant string"); break;
    case 9: V_VT(v) = VT_BOOL; V_BOOL(v) = VARIANT_TRUE; break;
    case 10: V_VT(v) = VT_BOOL; V_BOOL(v) = VARIANT_FALSE; break;
    case 11: V_VT(v) = VT_ERROR; V_ERROR(v) = DISP_E_PARAMNOTFOUND; break;
    case 12: V_VT(v) = VT_I1; V_I1(v) = -5; break;
    case 13: V_VT(v) = VT_UI1; V_UI1(v) = 250; break;
    case 14: V_VT(v) = VT_UI2; V_UI2(v) = 65000; break;
    case 15: V_VT(v) = VT_UI4; V_UI4(v) = 4000000000u; break;
    case 16: V_VT(v) = VT_I8; V_I8(v) = -0x123456789abcdefLL; break;
    case 17: V_VT(v) = VT_UI8; V_UI8(v) = 0xfedcba9876543210ULL; break;
    case 18: V_VT(v) = VT_INT; V_INT(v) = -99; break;
    case 19: V_VT(v) = VT_UINT; V_UINT(v) = 99; break;
    case 20: V_VT(v) = VT_DECIMAL; DECIMAL_SETZERO(V_DECIMAL(v)); V_DECIMAL(v).scale = 2; V_DECIMAL(v).sign = DECIMAL_NEG; V_DECIMAL(v).Lo64 = 12345; V_VT(v) = VT_DECIMAL; break;
    case 21: V_VT(v) = VT_UNKNOWN; V_UNKNOWN(v) = NULL; break;
    case 22: V_VT(v) = VT_DISPATCH; V_DISPATCH(v) = NULL; break;
    case 23: V_VT(v) = VT_BSTR; V_BSTR(v) = NULL; break;
    case 24: V_VT(v) = VT_BSTR; V_BSTR(v) = SysAllocString(L""); break;
    }
}
#define NVCASES 25
static int veq(const VARIANT *a, const VARIANT *b)
{
    if (V_VT(a) != V_VT(b)) return 0;
    switch (V_VT(a)) {
    case VT_EMPTY: case VT_NULL: return 1;
    case VT_I2: return V_I2(a) == V_I2(b);
    case VT_I4: case VT_INT: return V_I4(a) == V_I4(b);
    case VT_R4: return V_R4(a) == V_R4(b);
    case VT_R8: return V_R8(a) == V_R8(b);
    case VT_CY: return V_CY(a).int64 == V_CY(b).int64;
    case VT_DATE: return V_DATE(a) == V_DATE(b);
    case VT_BSTR: if (!V_BSTR(a) || !V_BSTR(b)) return V_BSTR(a) == V_BSTR(b);
                  return SysStringLen(V_BSTR(a)) == SysStringLen(V_BSTR(b)) && !memcmp(V_BSTR(a), V_BSTR(b), SysStringLen(V_BSTR(a)) * 2);
    case VT_BOOL: return V_BOOL(a) == V_BOOL(b);
    case VT_ERROR: return V_ERROR(a) == V_ERROR(b);
    case VT_I1: return V_I1(a) == V_I1(b);
    case VT_UI1: return V_UI1(a) == V_UI1(b);
    case VT_UI2: return V_UI2(a) == V_UI2(b);
    case VT_UI4: case VT_UINT: return V_UI4(a) == V_UI4(b);
    case VT_I8: return V_I8(a) == V_I8(b);
    case VT_UI8: return V_UI8(a) == V_UI8(b);
    case VT_DECIMAL: return V_DECIMAL(a).Lo64 == V_DECIMAL(b).Lo64 && V_DECIMAL(a).Hi32 == V_DECIMAL(b).Hi32 && V_DECIMAL(a).scale == V_DECIMAL(b).scale && V_DECIMAL(a).sign == V_DECIMAL(b).sign;
    case VT_UNKNOWN: return V_UNKNOWN(a) == V_UNKNOWN(b);
    case VT_DISPATCH: return V_DISPATCH(a) == V_DISPATCH(b);
    }
    return 0;
}
static void cl_variant_echo(void *p, void *ctx)
{
    CLI(c, p); int i;
    for (i = 0; i < NVCASES; i++) {
        VARIANT in, out; HRESULT hr;
        fill_variant(&in, i);
        memset(&out, 0xcc, sizeof out);
        memset(&g_last, 0, sizeof g_last);
        hr = M(c, EchoVariant, in, &out);
        CHECKF(hr == S_OK, "variant case %d (vt %u): hr=0x%lx", i, V_VT(&in), (long)hr);
        if (hr == S_OK) {
            CHECKF(g_last.vt == V_VT(&in), "case %d: server saw vt %u want %u", i, g_last.vt, V_VT(&in));
            CHECKF(V_VT(&out) == V_VT(&in), "case %d: result vt %u want %u", i, V_VT(&out), V_VT(&in));
            if (i == 23) { if (!veq(&in, &out)) t_note("VT_BSTR with a NULL bstr comes back as %s", V_BSTR(&out) ? "an empty string" : "?"); }
            else CHECKF(veq(&in, &out), "case %d (vt %u): value differs", i, V_VT(&in));
            VariantClear(&out);
        }
        VariantClear(&in);
    }
}
TMTEST(dxtm_mar_variant_echo_types) { RUN_CUSTOM(cl_variant_echo, NULL); }

static void cl_variant_inout(void *p, void *ctx)
{
    CLI(c, p); VARIANT v; HRESULT hr;
    VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 41;
    hr = M(c, VariantInOut, &v);
    EQ(hr, S_OK); EQ(V_VT(&v), VT_I4); EQ(V_I4(&v), 42);
    VariantClear(&v);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"var");
    hr = M(c, VariantInOut, &v);
    EQ(hr, S_OK); EQ(V_VT(&v), VT_BSTR);
    if (V_VT(&v) == VT_BSTR && V_BSTR(&v)) EXPECT_WSTR(V_BSTR(&v), L"var+srv");
    VariantClear(&v);
    VariantInit(&v); V_VT(&v) = VT_R8; V_R8(&v) = 2.5;
    hr = M(c, VariantInOut, &v);
    EQ(hr, S_OK); EQ(V_VT(&v), VT_R8); CHECK(V_R8(&v) == 2.5);
    VariantInit(&v);
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, VariantInOut, &v);
    EQ(hr, S_OK); EQ(V_VT(&v), VT_EMPTY); EQ(g_last.calls, 1);
}
TMTEST(dxtm_mar_variant_inout) { RUN_CUSTOM(cl_variant_inout, NULL); }

static void cl_variant_array(void *p, void *ctx)
{
    CLI(c, p); VARIANT in, out; SAFEARRAY *sa; LONG i; HRESULT hr;
    sa = SafeArrayCreateVector(VT_I4, 2, 5);
    for (i = 2; i < 7; i++) { LONG v = i * 100; SafeArrayPutElement(sa, &i, &v); }
    VariantInit(&in); V_VT(&in) = VT_ARRAY | VT_I4; V_ARRAY(&in) = sa;
    VariantInit(&out);
    hr = M(c, EchoVariant, in, &out);
    EQ(hr, S_OK);
    EQ(V_VT(&out), VT_ARRAY | VT_I4);
    if (V_VT(&out) == (VT_ARRAY | VT_I4) && V_ARRAY(&out)) {
        LONG lb = -1, ub = -1, got = 0;
        EQ(SafeArrayGetDim(V_ARRAY(&out)), 1);
        SafeArrayGetLBound(V_ARRAY(&out), 1, &lb); SafeArrayGetUBound(V_ARRAY(&out), 1, &ub);
        EQ(lb, 2); EQ(ub, 6);
        for (i = 2; i < 7; i++) { SafeArrayGetElement(V_ARRAY(&out), &i, &got); CHECKF(got == i * 100, "elem %ld = %ld", (long)i, (long)got); }
    }
    VariantClear(&out); VariantClear(&in);
    /* BSTR array in a VARIANT */
    {
        SAFEARRAY *sb = SafeArrayCreateVector(VT_BSTR, 0, 3); LONG k; BSTR s;
        for (k = 0; k < 3; k++) { WCHAR t[8]; _snwprintf(t, 8, L"s%ld", (long)k); s = SysAllocString(t); SafeArrayPutElement(sb, &k, s); SysFreeString(s); }
        VariantInit(&in); V_VT(&in) = VT_ARRAY | VT_BSTR; V_ARRAY(&in) = sb;
        hr = M(c, EchoVariant, in, &out);
        EQ(hr, S_OK);
        if (hr == S_OK && V_VT(&out) == (VT_ARRAY | VT_BSTR)) {
            BSTR got = NULL; k = 2; SafeArrayGetElement(V_ARRAY(&out), &k, &got);
            CHECK(got && !lstrcmpW(got, L"s2")); SysFreeString(got);
        } else CHECKF(0, "bstr array variant came back as vt %u", V_VT(&out));
        VariantClear(&out); VariantClear(&in);
    }
}
TMTEST(dxtm_mar_variant_with_safearray) { RUN_CUSTOM(cl_variant_array, NULL); }

static void cl_variant_unk(void *p, void *ctx)
{
    CLI(c, p); VARIANT in, out; IOletSink *sink = tm_new_sink(); HRESULT hr; IOletSink *back = NULL; LONG t = 0;
    VariantInit(&in); V_VT(&in) = VT_UNKNOWN; V_UNKNOWN(&in) = (IUnknown *)sink;
    VariantInit(&out);
    hr = M(c, EchoVariant, in, &out);
    EQ(hr, S_OK);
    EQ(V_VT(&out), VT_UNKNOWN);
    if (V_VT(&out) == VT_UNKNOWN && V_UNKNOWN(&out)) {
        hr = IUnknown_QueryInterface(V_UNKNOWN(&out), &TM_IID_IOletSink, (void **)&back);
        EQ(hr, S_OK);
        if (back) { sink->lpVtbl->Notify(sink, 9, NULL); EQ(back->lpVtbl->Total(back, &t), S_OK); EQ(t, 9); back->lpVtbl->Release(back); }
    }
    VariantClear(&out);
    sink->lpVtbl->Release(sink);
}
TMTEST(dxtm_mar_variant_with_interface_pointer) { RUN_CUSTOM(cl_variant_unk, NULL); }

/* ------------------------------------------------------------------ SAFEARRAY */
static SAFEARRAY *make_ints(LONG lb, LONG n, LONG first)
{
    SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, lb, n); LONG i;
    for (i = 0; i < n; i++) { LONG idx = lb + i, v = first + i; SafeArrayPutElement(sa, &idx, &v); }
    return sa;
}
static void cl_sa_sum(void *p, void *ctx)
{
    CLI(c, p); LONG sum = 0; SAFEARRAY *sa; HRESULT hr;
    sa = make_ints(0, 100, 1);
    hr = M(c, SumArray, sa, &sum); EQ(hr, S_OK); EQ(sum, 5050);
    SafeArrayDestroy(sa);
    sa = make_ints(-3, 7, 10);
    hr = M(c, SumArray, sa, &sum); EQ(hr, S_OK); EQ(sum, 7 * 10 + 21);
    SafeArrayDestroy(sa);
    sa = make_ints(0, 0, 0);
    sum = 77; hr = M(c, SumArray, sa, &sum); EQ(hr, S_OK); EQ(sum, 0);
    SafeArrayDestroy(sa);
    sa = make_ints(1, 1, -9);
    hr = M(c, SumArray, sa, &sum); EQ(hr, S_OK); EQ(sum, -9);
    SafeArrayDestroy(sa);
}
TMTEST(dxtm_mar_safearray_in) { RUN_CUSTOM(cl_sa_sum, NULL); }

static void cl_sa_null(void *p, void *ctx)
{
    CLI(c, p); LONG sum = 0; HRESULT hr;
    hr = M(c, SumArray, NULL, &sum);
    EQ(hr, S_FALSE);       /* NULL SAFEARRAY* survives the trip and the server sees NULL */
    EQ(sum, -1);
}
TMTEST(dxtm_mar_safearray_null_in) { RUN_CUSTOM(cl_sa_null, NULL); }

static void cl_sa_make(void *p, void *ctx)
{
    CLI(c, p); static const LONG ns[] = { 0, 1, 2, 17, 1000 }; unsigned k;
    for (k = 0; k < sizeof ns / sizeof ns[0]; k++) {
        SAFEARRAY *sa = (SAFEARRAY *)(ULONG_PTR)1; HRESULT hr = M(c, MakeArray, ns[k], &sa);
        EQ(hr, S_OK);
        CHECK(sa != NULL);
        if (sa && sa != (SAFEARRAY *)(ULONG_PTR)1) {
            LONG lb = 0, ub = 0, i, got; VARTYPE vt = 0;
            EQ(SafeArrayGetDim(sa), 1);
            EQ(SafeArrayGetLBound(sa, 1, &lb), S_OK); EQ(SafeArrayGetUBound(sa, 1, &ub), S_OK);
            EQ(lb, 5); EQ(ub, 5 + ns[k] - 1);
            SafeArrayGetVartype(sa, &vt); EQ(vt, VT_I4);
            EQ(sa->cbElements, 4);
            EQ(sa->cDims, 1);
            for (i = 0; i < ns[k]; i++) { LONG idx = 5 + i; got = 0; SafeArrayGetElement(sa, &idx, &got); if (got != i + 1) { CHECKF(0, "n=%ld element %ld = %ld", (long)ns[k], (long)i, (long)got); break; } }
            SafeArrayDestroy(sa);
        }
    }
}
TMTEST(dxtm_mar_safearray_out) { RUN_CUSTOM(cl_sa_make, NULL); }

static void cl_sa_fail(void *p, void *ctx)
{
    CLI(c, p); SAFEARRAY *sa = (SAFEARRAY *)(ULONG_PTR)1; HRESULT hr = M(c, MakeArray, -1, &sa);
    EQ(hr, E_INVALIDARG);
    t_note("[out] SAFEARRAY* after a failed call: %p", sa);
}
TMTEST(dxtm_mar_safearray_out_failure) { RUN_CUSTOM(cl_sa_fail, NULL); }

static void cl_sa_big(void *p, void *ctx)
{
    CLI(c, p); SAFEARRAY *sa = make_ints(0, 100000, 0); LONG sum = 0; HRESULT hr;
    hr = M(c, SumArray, sa, &sum);
    EQ(hr, S_OK);
    EQ(sum, (LONG)(99999LL * 100000LL / 2));
    SafeArrayDestroy(sa);
    sa = NULL;
    hr = M(c, MakeArray, 100000, &sa);
    EQ(hr, S_OK);
    if (sa) { LONG ub = 0; SafeArrayGetUBound(sa, 1, &ub); EQ(ub, 100004); SafeArrayDestroy(sa); }
}
TMTEST(dxtm_mar_safearray_100k_elements) { RUN_CUSTOM(cl_sa_big, NULL); }

static void cl_sa_bstr(void *p, void *ctx)
{
    CLI(c, p); SAFEARRAY *in, *out = NULL; LONG i; HRESULT hr; static const WCHAR *w[4] = { L"zero", L"", L"two words", L"\x00e9\x4e2d" };
    in = SafeArrayCreateVector(VT_BSTR, 10, 4);
    for (i = 0; i < 4; i++) { LONG idx = 10 + i; BSTR s = SysAllocString(w[i]); SafeArrayPutElement(in, &idx, s); SysFreeString(s); }
    hr = M(c, EchoBstrArray, in, &out);
    EQ(hr, S_OK);
    CHECK(out != NULL);
    if (out) {
        LONG lb = 0, ub = 0; VARTYPE vt = 0;
        SafeArrayGetVartype(out, &vt); EQ(vt, VT_BSTR);
        SafeArrayGetLBound(out, 1, &lb); SafeArrayGetUBound(out, 1, &ub); EQ(lb, 10); EQ(ub, 13);
        for (i = 0; i < 4; i++) {
            LONG idx = 10 + i; BSTR got = NULL;
            SafeArrayGetElement(out, &idx, &got);
            CHECKF(got && !lstrcmpW(got, w[i]), "element %ld", (long)i);
            SysFreeString(got);
        }
        SafeArrayDestroy(out);
    }
    SafeArrayDestroy(in);
    out = (SAFEARRAY *)(ULONG_PTR)1;
    hr = M(c, EchoBstrArray, NULL, &out);
    EQ(hr, S_OK);
    CHECK(out == NULL);
}
TMTEST(dxtm_mar_safearray_bstr_elements) { RUN_CUSTOM(cl_sa_bstr, NULL); }

/* ------------------------------------------------------------------ HRESULTs */
static const LONG hrs[] = { S_OK, S_FALSE, 2, 0x00040000, 0x7fffffff, E_FAIL, E_INVALIDARG, E_NOTIMPL, E_UNEXPECTED, E_OUTOFMEMORY,
    E_ABORT, E_ACCESSDENIED, (LONG)0x80070005, (LONG)0x8abc1234, DISP_E_TYPEMISMATCH, TYPE_E_ELEMENTNOTFOUND, RPC_E_DISCONNECTED, (LONG)0x80000000 };
static void cl_hrs(void *p, void *ctx)
{
    CLI(c, p); unsigned i;
    for (i = 0; i < sizeof hrs / sizeof hrs[0]; i++) {
        HRESULT hr = M(c, ReturnHr, hrs[i]);
        CHECKF(hr == hrs[i], "ReturnHr(0x%lx) came back as 0x%lx", (long)hrs[i], (long)hr);
    }
}
TMTEST(dxtm_mar_hresult_propagation) { RUN_CUSTOM(cl_hrs, NULL); }

static void cl_failout(void *p, void *ctx)
{
    CLI(c, p); BSTR pb = NULL; LONG pl = 0; IOletSink *ps = NULL; HRESULT hr;
    hr = M(c, FailWithOut, S_OK, &pb, &pl, &ps);
    EQ(hr, S_OK);
    CHECK(pb != NULL); if (pb) EXPECT_WSTR(pb, L"fail-out");
    EQ(pl, 77);
    CHECK(ps != NULL);
    if (ps) { LONG t = -1; EQ(ps->lpVtbl->Total(ps, &t), S_OK); EQ(t, 0); ps->lpVtbl->Release(ps); }
    SysFreeString(pb);
    pb = NULL; pl = 0; ps = NULL;
    hr = M(c, FailWithOut, S_FALSE, &pb, &pl, &ps);
    EQ(hr, S_FALSE);
    t_note("S_FALSE with [out] parameters: pb=%p pl=%ld ps=%p", pb, (long)pl, ps);
    SysFreeString(pb); if (ps) ps->lpVtbl->Release(ps);
    pb = NULL; pl = 0; ps = NULL;
    hr = M(c, FailWithOut, E_FAIL, &pb, &pl, &ps);
    EQ(hr, E_FAIL);
    t_note("E_FAIL with [out] parameters: pb=%p pl=%ld ps=%p", pb, (long)pl, ps);
    SysFreeString(pb); if (ps) ps->lpVtbl->Release(ps);
}
TMTEST(dxtm_mar_out_params_with_failure_hresult) { RUN_CUSTOM(cl_failout, NULL); }

/* ------------------------------------------------------------------ ordering of calls and statefulness */
static void cl_many_calls(void *p, void *ctx)
{
    CLI(c, p); int i; LONG acc = 0, r;
    for (i = 0; i < 500; i++) { M(c, AddLong, acc, 1, &r); acc = r; }
    EQ(acc, 500);
}
TMTEST(dxtm_mar_many_sequential_calls) { RUN_CUSTOM(cl_many_calls, NULL); }
