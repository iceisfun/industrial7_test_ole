/* gap_06: Var{Bool,Cy,Date,Dec,I1,I2,I4,I8,R4,R8,UI1,UI2,UI4,UI8}FromDisp.
 * Decomp: every one is VariantChangeTypeEx(&out, &{VT_DISPATCH, pdisp}, lcid, 0, targetVt) and copies the result to the caller only on S_OK.
 * VariantChangeTypeEx on a VT_DISPATCH source asks the object for its default property (DISPID_VALUE, DISPATCH_PROPERTYGET, no args)
 * and converts the returned value; a failing Invoke gives DISP_E_TYPEMISMATCH. The numeric conversion rules are those of the VarXFromY family
 * (banker's rounding for real->integer, DISP_E_OVERFLOW out of range, VT_NULL/unparsable BSTR => DISP_E_TYPEMISMATCH). */
#include "gap6.h"
#include <math.h>

#define E_TYPEMIS ((HRESULT)0x80020005L)
#define E_OVERFL  ((HRESULT)0x8002000AL)

typedef struct {
    IDispatch iface;
    LONG ref;
    LONG invokes, others;
    VARIANT result;          /* what Invoke returns */
    HRESULT hr;              /* Invoke result */
    /* captured on the last Invoke */
    DISPID dispid; GUID riid; LCID lcid; WORD flags; UINT cargs, cnamed; BOOL had_res, had_ei, had_argerr, res_empty;
} DD;
static DD *dd_of(IDispatch *i) { return (DD *)i; }
static HRESULT WINAPI dd_qi(IDispatch *i, REFIID r, void **p) {
    if (!memcmp(r, &G6_IID_IUnknown, sizeof(GUID)) || !memcmp(r, &G6_IID_IDispatch, sizeof(GUID))) { *p = i; dd_of(i)->ref++; return S_OK; }
    *p = NULL; dd_of(i)->others++; return E_NOINTERFACE;
}
static ULONG WINAPI dd_ar(IDispatch *i) { return ++dd_of(i)->ref; }
static ULONG WINAPI dd_rl(IDispatch *i) { return --dd_of(i)->ref; }
static HRESULT WINAPI dd_gtic(IDispatch *i, UINT *n) { dd_of(i)->others++; *n = 0; return S_OK; }
static HRESULT WINAPI dd_gti(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { dd_of(i)->others++; *t = NULL; return E_NOTIMPL; }
static HRESULT WINAPI dd_gids(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { dd_of(i)->others++; return DISP_E_UNKNOWNNAME; }
static HRESULT WINAPI dd_inv(IDispatch *i, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *res, EXCEPINFO *ei, UINT *ae) {
    DD *d = dd_of(i);
    d->invokes++; d->dispid = id; d->riid = *r; d->lcid = l; d->flags = f;
    d->cargs = p ? p->cArgs : 99; d->cnamed = p ? p->cNamedArgs : 99;
    d->had_res = res != NULL; d->had_ei = ei != NULL; d->had_argerr = ae != NULL;
    d->res_empty = res && V_VT(res) == VT_EMPTY;
    if (FAILED(d->hr)) return d->hr;
    if (res) { *res = d->result; if (V_VT(&d->result) == VT_BSTR) V_BSTR(res) = U(SysAllocString)(V_BSTR(&d->result)); }
    return S_OK;
}
static IDispatchVtbl dd_vtbl = { dd_qi, dd_ar, dd_rl, dd_gtic, dd_gti, dd_gids, dd_inv };
static void dd_init(DD *d) { memset(d, 0, sizeof *d); d->iface.lpVtbl = &dd_vtbl; d->ref = 1; d->hr = S_OK; }

typedef HRESULT (WINAPI *conv_fn)(IDispatch *, LCID, void *);
typedef enum { K_INT, K_UINT, K_R4, K_R8, K_BOOL, K_CY, K_DATE, K_DEC } kind_t;
typedef struct { const char *name; const char *proc; kind_t k; unsigned size; double lo, hi; } conv_t;
static const conv_t CV[] = {
    { "Bool", "VarBoolFromDisp", K_BOOL, 2, 0, 0 }, { "Cy", "VarCyFromDisp", K_CY, 8, 0, 0 }, { "Date", "VarDateFromDisp", K_DATE, 8, 0, 0 },
    { "Dec", "VarDecFromDisp", K_DEC, 16, 0, 0 },
    { "I1", "VarI1FromDisp", K_INT, 1, -128, 127 }, { "I2", "VarI2FromDisp", K_INT, 2, -32768, 32767 }, { "I4", "VarI4FromDisp", K_INT, 4, -2147483648.0, 2147483647.0 },
    { "I8", "VarI8FromDisp", K_INT, 8, -9223372036854775808.0, 9223372036854775807.0 },
    { "R4", "VarR4FromDisp", K_R4, 4, 0, 0 }, { "R8", "VarR8FromDisp", K_R8, 8, 0, 0 },
    { "UI1", "VarUI1FromDisp", K_UINT, 1, 0, 255 }, { "UI2", "VarUI2FromDisp", K_UINT, 2, 0, 65535 }, { "UI4", "VarUI4FromDisp", K_UINT, 4, 0, 4294967295.0 },
    { "UI8", "VarUI8FromDisp", K_UINT, 8, 0, 18446744073709551615.0 } };
#define NCV (sizeof CV / sizeof *CV)

static double bankers(double d) { double f = floor(d), diff = d - f; if (diff < 0.5) return f; if (diff > 0.5) return f + 1; return fmod(f, 2.0) == 0 ? f : f + 1; }

/* run conversion i on a dispatch returning *val; fills raw out bytes; returns hr */
static HRESULT run(unsigned i, DD *d, LCID lcid, BYTE *out /* 16 bytes, pre-poisoned by caller */) {
    conv_fn f = (conv_fn)t_proc(CV[i].proc);
    return f ? f(&d->iface, lcid, out) : (HRESULT)0xDEADBEEF;
}
#define LCID_US 0x0409

static void check_numeric(const char *tag, VARIANT res, double v, int integral_src) {
    for (unsigned i = 0; i < NCV; i++) {
        if (!t_proc(CV[i].proc)) { CHECKF(0, "%s missing", CV[i].proc); continue; }
        DD d; dd_init(&d); d.result = res;
        BYTE out[16]; memset(out, 0xA5, sizeof out);
        HRESULT hr = run(i, &d, LCID_US, out);
        BYTE ref[16]; memset(ref, 0xA5, sizeof ref);
        const conv_t *c = &CV[i];
        HRESULT want = S_OK; double wv = 0;
        switch (c->k) {
        case K_INT: case K_UINT: {
            double r = bankers(v);
            if (r < c->lo || r > c->hi || (c->k == K_INT && c->size == 8 && r >= 9223372036854775808.0) || (c->k == K_UINT && c->size == 8 && r >= 18446744073709551616.0)) want = E_OVERFL; else wv = r;
            break; }
        case K_BOOL: wv = v != 0 ? -1 : 0; break;
        case K_R4: case K_R8: wv = v; break;
        case K_DATE:
            if (integral_src ? (v < -657434.0 || v > 2958465.0) : (v <= -657435.0 || v >= 2958466.0)) want = E_OVERFL; else wv = v;
            break;
        case K_CY: wv = bankers(v * 10000); if (fabs(v * 10000) >= 9223372036854775808.0) want = E_OVERFL; break;
        case K_DEC: wv = v; if (fabs(v) >= 7.9228162514264337593543950336e28) want = E_OVERFL; break;
        }
        CHECKF(hr == want, "%s -> %s: hr 0x%lx want 0x%lx", tag, c->name, hr, want);
        if (hr != S_OK) { CHECKF(!memcmp(out, ref, 16), "%s -> %s: output modified on failure", tag, c->name); continue; }
        switch (c->k) {
        case K_INT: { long long x = 0; memcpy(&x, out, c->size); if (c->size < 8) { long long sh = 64 - 8 * c->size; x = (x << sh) >> sh; } CHECKF((double)x == wv, "%s -> %s: got %lld want %.0f", tag, c->name, x, wv); break; }
        case K_UINT: { unsigned long long x = 0; memcpy(&x, out, c->size); CHECKF((double)x == wv, "%s -> %s: got %llu want %.0f", tag, c->name, x, wv); break; }
        case K_BOOL: { short x; memcpy(&x, out, 2); CHECKF(x == (short)wv, "%s -> %s: got %d", tag, c->name, x); break; }
        case K_R4: { float x; memcpy(&x, out, 4); CHECKF(x == (float)wv, "%s -> %s: got %g", tag, c->name, x); break; }
        case K_R8: case K_DATE: { double x; memcpy(&x, out, 8); CHECKF(x == wv, "%s -> %s: got %g", tag, c->name, x); break; }
        case K_CY: { long long x; memcpy(&x, out, 8); CHECKF((double)x == wv, "%s -> %s: got %lld", tag, c->name, x); break; }
        case K_DEC: {
            DECIMAL x; memcpy(&x, out, sizeof x);
            if (integral_src) CHECKF(x.scale == 0 && x.sign == (v < 0 ? DECIMAL_NEG : 0) && x.Hi32 == 0 && x.Lo64 == (ULONGLONG)fabs(v), "%s -> Dec: scale %u sign %u hi %lu lo %llu", tag, x.scale, x.sign, x.Hi32, x.Lo64);
            break; }
        }
    }
}

TEST(gap6_fromdisp_all_exports_present) {
    for (unsigned i = 0; i < NCV; i++) CHECKF(t_proc(CV[i].proc) != NULL, "%s", CV[i].proc);
}
TEST(gap6_fromdisp_i4_source_table) {
    static const int vals[] = { 0, 1, -1, 42, 127, 128, -128, -129, 255, 256, 300, 32767, 32768, -32768, -32769, 65535, 65536, 70000, 2147483647, (int)0x80000000 };
    for (unsigned k = 0; k < sizeof vals / sizeof *vals; k++) {
        VARIANT r; VariantInit(&r); V_VT(&r) = VT_I4; V_I4(&r) = vals[k];
        char tag[32]; wsprintfA(tag, "I4(%d)", vals[k]);
        check_numeric(tag, r, (double)vals[k], 1);
    }
}
TEST(gap6_fromdisp_r8_source_table) {
    static const double vals[] = { 0.0, 0.4, 0.5, 0.6, 1.5, 2.5, 3.5, -0.5, -1.5, -2.5, 254.5, 255.5, 127.5, 128.5, 3e9, 4294967295.0, 4294967295.6, 1e19, -1e19, 1e30, 123456.789 };
    for (unsigned k = 0; k < sizeof vals / sizeof *vals; k++) {
        VARIANT r; VariantInit(&r); V_VT(&r) = VT_R8; V_R8(&r) = vals[k];
        char tag[32]; wsprintfA(tag, "R8(%g)", vals[k]);
        check_numeric(tag, r, vals[k], 0);
    }
}
TEST(gap6_fromdisp_i2_i8_ui1_sources) {
    VARIANT r; VariantInit(&r);
    V_VT(&r) = VT_I2; V_I2(&r) = -5; check_numeric("I2(-5)", r, -5, 1);
    V_VT(&r) = VT_UI1; V_UI1(&r) = 200; check_numeric("UI1(200)", r, 200, 1);
    V_VT(&r) = VT_I8; V_I8(&r) = 5000000000LL; check_numeric("I8(5e9)", r, 5e9, 1);
    V_VT(&r) = VT_R4; V_R4(&r) = 2.5f; check_numeric("R4(2.5)", r, 2.5, 0);
    V_VT(&r) = VT_UI4; V_UI4(&r) = 4000000000u; check_numeric("UI4(4e9)", r, 4e9, 1);
}
TEST(gap6_fromdisp_empty_result_is_zero) {
    VARIANT r; VariantInit(&r);                       /* VT_EMPTY converts to 0 / FALSE for every numeric target */
    check_numeric("EMPTY", r, 0, 1);
}
TEST(gap6_fromdisp_null_result_type_mismatch) {
    VARIANT r; VariantInit(&r); V_VT(&r) = VT_NULL;
    for (unsigned i = 0; i < NCV; i++) {
        DD d; dd_init(&d); d.result = r;
        BYTE out[16]; memset(out, 0x5A, sizeof out); BYTE ref[16]; memset(ref, 0x5A, sizeof ref);
        HRESULT hr = run(i, &d, LCID_US, out);
        CHECKF(hr == E_TYPEMIS, "%s hr 0x%lx", CV[i].name, hr);
        CHECKF(!memcmp(out, ref, sizeof out), "%s output modified", CV[i].name);
    }
}
TEST(gap6_fromdisp_bstr_result) {
    static const struct { const wchar_t *s; double v; int ok; } t[] = { {L"17", 17, 1}, {L"-3", -3, 1}, {L"  42  ", 42, 1}, {L"0", 0, 1}, {L"abc", 0, 0}, {L"", 0, 0}, {L"12x", 0, 0}, {L"300", 300, 1} };
    for (unsigned k = 0; k < sizeof t / sizeof *t; k++) {
        VARIANT r; VariantInit(&r); V_VT(&r) = VT_BSTR; V_BSTR(&r) = (BSTR)t[k].s;   /* Invoke copies it */
        for (unsigned i = 4; i < 10; i++) {   /* I1 .. R8 */
            if (!strcmp(CV[i].name, "R4") && 0) continue;
            DD d; dd_init(&d); d.result = r;
            BYTE out[16]; memset(out, 0xA5, sizeof out);
            HRESULT hr = run(i, &d, LCID_US, out);
            if (!t[k].ok) { CHECKF(hr == E_TYPEMIS || FAILED(hr), "\"%ls\" -> %s hr 0x%lx", t[k].s, CV[i].name, hr); continue; }
            if (t[k].v == 300 && (!strcmp(CV[i].name, "I1"))) { CHECKF(hr == E_OVERFL, "300 -> I1 hr 0x%lx", hr); continue; }
            CHECKF(hr == S_OK, "\"%ls\" -> %s hr 0x%lx", t[k].s, CV[i].name, hr);
            if (hr == S_OK && !strcmp(CV[i].name, "I4")) { int x; memcpy(&x, out, 4); CHECKF(x == (int)t[k].v, "\"%ls\" -> I4 = %d", t[k].s, x); }
            if (hr == S_OK && !strcmp(CV[i].name, "R8")) { double x; memcpy(&x, out, 8); CHECKF(x == t[k].v, "\"%ls\" -> R8 = %g", t[k].s, x); }
        }
    }
}
TEST(gap6_fromdisp_invoke_failure_is_type_mismatch) {
    static const HRESULT hrs[] = { E_FAIL, DISP_E_MEMBERNOTFOUND, E_NOTIMPL, DISP_E_EXCEPTION, E_ACCESSDENIED };
    for (unsigned h = 0; h < sizeof hrs / sizeof *hrs; h++) {
        for (unsigned i = 0; i < NCV; i++) {
            DD d; dd_init(&d); d.hr = hrs[h]; V_VT(&d.result) = VT_I4; V_I4(&d.result) = 5;
            BYTE out[16]; memset(out, 0x77, sizeof out); BYTE ref[16]; memset(ref, 0x77, sizeof ref);
            HRESULT hr = run(i, &d, LCID_US, out);
            CHECKF(hr == E_TYPEMIS, "invoke hr 0x%lx %s -> 0x%lx", hrs[h], CV[i].name, hr);
            CHECKF(!memcmp(out, ref, sizeof out), "%s output modified", CV[i].name);
            CHECKF(d.invokes == 1, "%s invokes=%ld", CV[i].name, d.invokes);
        }
    }
}
TEST(gap6_fromdisp_invoke_call_shape) {
    REQUIRE(U(VarI4FromDisp));
    DD d; dd_init(&d); V_VT(&d.result) = VT_I4; V_I4(&d.result) = 8;
    LONG out = -1;
    EQ(U(VarI4FromDisp)(&d.iface, 0x0407, &out), S_OK);
    EQ(out, 8);
    EQ(d.invokes, 1);
    CHECKF(d.dispid == DISPID_VALUE, "dispid %ld", d.dispid);
    CHECK(!memcmp(&d.riid, &G6_NIL, sizeof(GUID)));
    CHECKF((d.flags & DISPATCH_PROPERTYGET) != 0, "flags 0x%x", d.flags);
    EQ(d.cargs, 0); EQ(d.cnamed, 0);
    CHECK(d.had_res);
    t_note("Invoke flags=0x%x lcid=0x%lx ei=%d argerr=%d res_initially_empty=%d others=%ld", d.flags, d.lcid, d.had_ei, d.had_argerr, d.res_empty, d.others);
    CHECK(d.res_empty);
}
TEST(gap6_fromdisp_lifetime_net_zero) {
    for (unsigned i = 0; i < NCV; i++) {
        DD d; dd_init(&d); V_VT(&d.result) = VT_I4; V_I4(&d.result) = 1;
        BYTE out[16];
        run(i, &d, LCID_US, out);
        CHECKF(d.ref == 1, "%s: ref=%ld after call", CV[i].name, d.ref);
    }
}
TEST(gap6_fromdisp_result_is_object_chain) {
    /* Invoke returns another dispatch: its default value is read in turn (VariantChangeType follows VT_DISPATCH results) */
    REQUIRE(U(VarI4FromDisp));
    DD inner; dd_init(&inner); V_VT(&inner.result) = VT_I4; V_I4(&inner.result) = 99;
    DD outer; dd_init(&outer); V_VT(&outer.result) = VT_DISPATCH; V_DISPATCH(&outer.result) = &inner.iface; inner.ref++;
    LONG out = -1;
    HRESULT hr = U(VarI4FromDisp)(&outer.iface, LCID_US, &out);
    t_note("dispatch returning a dispatch: hr=0x%lx out=%ld inner invokes=%ld", hr, out, inner.invokes);
    if (hr == S_OK) EQ(out, 99);
}
TEST(gap6_fromdisp_null_dispatch) {
    REQUIRE(U(VarI4FromDisp));
    LONG out = 55;
    HRESULT hr = U(VarI4FromDisp)(NULL, LCID_US, &out);
    t_note("VarI4FromDisp(NULL) hr=0x%lx out=%ld", hr, out);
    CHECK(FAILED(hr));
    CHECK(out == 55);
}
TEST(gap6_fromdisp_matches_variantchangetypeex) {
    REQUIRE(U(VariantChangeTypeEx)); REQUIRE(U(VarR8FromDisp)); REQUIRE(U(VarI2FromDisp));
    static const double vals[] = { 3.7, -3.7, 1e10, 0.5, 2.5 };
    for (unsigned k = 0; k < sizeof vals / sizeof *vals; k++) {
        DD d; dd_init(&d); V_VT(&d.result) = VT_R8; V_R8(&d.result) = vals[k];
        VARIANT src, dst; VariantInit(&src); VariantInit(&dst); V_VT(&src) = VT_DISPATCH; V_DISPATCH(&src) = &d.iface;
        HRESULT h1 = U(VariantChangeTypeEx)(&dst, &src, LCID_US, 0, VT_I2);
        SHORT o2 = 0x7777; HRESULT h2 = U(VarI2FromDisp)(&d.iface, LCID_US, &o2);
        CHECKF(h1 == h2, "val %g: 0x%lx vs 0x%lx", vals[k], h1, h2);
        if (h1 == S_OK) CHECKF(V_VT(&dst) == VT_I2 && V_I2(&dst) == o2, "val %g", vals[k]);
    }
}
TEST(gap6_fromdisp_decimal_precision) {
    REQUIRE(U(VarDecFromDisp));
    DD d; dd_init(&d); V_VT(&d.result) = VT_R8; V_R8(&d.result) = 2.5;
    DECIMAL x; memset(&x, 0xEE, sizeof x);
    EQ(U(VarDecFromDisp)(&d.iface, LCID_US, &x), S_OK);
    CHECK(x.sign == 0 && x.scale == 1 && x.Hi32 == 0 && x.Lo64 == 25);
    V_R8(&d.result) = -0.125;
    EQ(U(VarDecFromDisp)(&d.iface, LCID_US, &x), S_OK);
    CHECK(x.sign == DECIMAL_NEG && x.scale == 3 && x.Hi32 == 0 && x.Lo64 == 125);
}
TEST(gap6_fromdisp_cy_scaling) {
    REQUIRE(U(VarCyFromDisp));
    DD d; dd_init(&d); V_VT(&d.result) = VT_R8; V_R8(&d.result) = 12.3456;
    CY c; c.int64 = 0;
    EQ(U(VarCyFromDisp)(&d.iface, LCID_US, &c), S_OK);
    EQ(c.int64, 123456);
    V_R8(&d.result) = 1e15;           /* 1e15 * 10000 = 1e19 > 2^63 */
    c.int64 = 5;
    EQ(U(VarCyFromDisp)(&d.iface, LCID_US, &c), E_OVERFL);
    EQ(c.int64, 5);
}
TEST(gap6_fromdisp_bool_from_variants) {
    REQUIRE(U(VarBoolFromDisp));
    static const struct { VARTYPE vt; double v; short want; } t[] = { {VT_I4,0,0},{VT_I4,1,-1},{VT_I4,-7,-1},{VT_R8,0.0,0},{VT_R8,0.001,-1},{VT_I2,0,0},{VT_I2,5,-1},{VT_UI1,0,0},{VT_UI1,255,-1} };
    for (unsigned k = 0; k < sizeof t / sizeof *t; k++) {
        DD d; dd_init(&d); V_VT(&d.result) = t[k].vt;
        switch (t[k].vt) { case VT_I4: V_I4(&d.result) = (LONG)t[k].v; break; case VT_R8: V_R8(&d.result) = t[k].v; break; case VT_I2: V_I2(&d.result) = (SHORT)t[k].v; break; case VT_UI1: V_UI1(&d.result) = (BYTE)t[k].v; break; }
        VARIANT_BOOL o = 0x1234;
        HRESULT hr = U(VarBoolFromDisp)(&d.iface, LCID_US, &o);
        CHECKF(hr == S_OK && o == t[k].want, "vt %u v %g: hr 0x%lx out %d", t[k].vt, t[k].v, hr, o);
    }
}
TEST(gap6_fromdisp_bool_bstr_true_false) {
    REQUIRE(U(VarBoolFromDisp));
    static const struct { const wchar_t *s; short want; } t[] = { {L"True", -1}, {L"False", 0}, {L"true", -1}, {L"false", 0}, {L"#TRUE#", -1}, {L"#FALSE#", 0}, {L"1", -1}, {L"0", 0} };
    for (unsigned k = 0; k < sizeof t / sizeof *t; k++) {
        DD d; dd_init(&d); V_VT(&d.result) = VT_BSTR; V_BSTR(&d.result) = (BSTR)t[k].s;
        VARIANT_BOOL o = 0x1234;
        HRESULT hr = U(VarBoolFromDisp)(&d.iface, 0x0409, &o);
        CHECKF(hr == S_OK && o == t[k].want, "\"%ls\": hr 0x%lx out %d", t[k].s, hr, o);
    }
}
TEST(gap6_fromdisp_date_value) {
    REQUIRE(U(VarDateFromDisp));
    DD d; dd_init(&d); V_VT(&d.result) = VT_R8; V_R8(&d.result) = 36526.25;
    DATE o = 0;
    EQ(U(VarDateFromDisp)(&d.iface, LCID_US, &o), S_OK);
    CHECK(o == 36526.25);
}
