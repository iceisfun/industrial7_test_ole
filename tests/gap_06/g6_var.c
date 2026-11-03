/* gap_06: VarR8Round, VariantCopyInd.
 * Decomp facts (Win10 oleaut32):
 *  VarR8Round: cDecimals as unsigned must be < 23 (0..22) else E_INVALIDARG and *out untouched. |x| > 1e16 (and NaN) is returned unchanged.
 *    Otherwise x*10^n is split into integer + fraction; fraction exactly +-0.5 on an even integer stays (banker's rounding), anything else
 *    rounds half away from zero; result = rounded / 10^n.
 *  VariantCopyInd: no VT_BYREF => plain VariantCopy. VT_BYREF: dest is cleared first (unless dest == src), the pointed-to value is copied by
 *    element size (1/2/4/8 bytes, DECIMAL 16, BSTR duplicated, IUnknown/IDispatch AddRef'd, SAFEARRAY copied), dest vt = src vt minus BYREF.
 *    VT_VARIANT|BYREF recurses (VT_VARIANT|BYREF inside VT_VARIANT|BYREF => E_INVALIDARG); BYREF to an unsupported base type => E_INVALIDARG. */
#include "gap6.h"
#include <oleauto.h>
#include <math.h>

/* ------------------------------------------------------------ VarR8Round */
TEST(gap6_r8round_decimals_range) {
    REQUIRE(U(VarR8Round));
    for (int n = 0; n <= 22; n++) { double o = -1; CHECKF(U(VarR8Round)(1.25, n, &o) == S_OK, "n=%d", n); }
    static const int bad[] = { 23, 24, 100, -1, -2, -22, 0x7fffffff, (int)0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        double o = 12345.0; HRESULT hr = U(VarR8Round)(1.25, bad[i], &o);
        CHECKF(hr == E_INVALIDARG, "n=%d hr=0x%lx", bad[i], hr);
        CHECKF(o == 12345.0, "n=%d output modified", bad[i]);
    }
}
TEST(gap6_r8round_table) {
    REQUIRE(U(VarR8Round));
    static const struct { double in; int n; double want; } t[] = {
        {0.5432,5,0.5432},{0.5432,4,0.5432},{0.5432,3,0.543},{0.5432,2,0.54},{0.5432,1,0.5},{0.5532,0,1},
        {0.5568,3,0.557},{0.5568,2,0.56},{0.5568,1,0.6},{0.5568,0,1},
        {0.4999,0,0},{0.5,0,0},{0.5001,0,1},{1.4999,0,1},{1.5,0,2},{1.5001,0,2},
        {2.5,0,2},{3.5,0,4},{4.5,0,4},{5.5,0,6},{6.5,0,6},{-0.5,0,0},{-1.5,0,-2},{-2.5,0,-2},{-3.5,0,-4},
        {-0.4999,0,0},{-0.5001,0,-1},{-1.4999,0,-1},{-1.5001,0,-2},
        {0.125,2,0.12},{0.375,2,0.38},{0.625,2,0.62},{0.875,2,0.88},{-0.125,2,-0.12},{-0.375,2,-0.38},
        {12345.678,0,12346},{12345.678,1,12345.7},{12345.678,2,12345.68},{12345.678,-0+3,12345.678},
        {0.0,0,0},{0.0,10,0},{7,0,7},{7,5,7},{-7,3,-7},{100.25,1,100.2},{100.35,1,100.4}, {1e15+0.5,0,1e15},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        double o = -999.0; HRESULT hr = U(VarR8Round)(t[i].in, t[i].n, &o);
        CHECKF(hr == S_OK, "case %u", i);
        CHECKF(o == t[i].want, "case %u: round(%.17g,%d) = %.17g want %.17g", i, t[i].in, t[i].n, o, t[i].want);
    }
}
TEST(gap6_r8round_large_values_unchanged) {
    REQUIRE(U(VarR8Round));
    static const double v[] = { 1e16 * 1.0000001, 1e17, 1.5e20, 1e100, 1.7e308, -1e17, -1e100, -1.7e308 };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        double o = 0; HRESULT hr = U(VarR8Round)(v[i], 2, &o);
        CHECKF(hr == S_OK && o == v[i], "v=%g", v[i]);
    }
    double inf = INFINITY, o = 0;
    EQ(U(VarR8Round)(inf, 3, &o), S_OK); CHECK(isinf(o) && o > 0);
    EQ(U(VarR8Round)(-inf, 3, &o), S_OK); CHECK(isinf(o) && o < 0);
    double nan_ = NAN; o = 0;
    EQ(U(VarR8Round)(nan_, 3, &o), S_OK); CHECK(isnan(o));
}
TEST(gap6_r8round_negative_zero_and_sign) {
    REQUIRE(U(VarR8Round));
    double o = 1;
    EQ(U(VarR8Round)(-0.0, 2, &o), S_OK); CHECK(o == 0.0);
    o = 1; EQ(U(VarR8Round)(-0.4, 0, &o), S_OK); CHECK(o == 0.0);
    o = 1; EQ(U(VarR8Round)(-0.6, 0, &o), S_OK); CHECK(o == -1.0);
}
TEST(gap6_r8round_idempotent) {
    REQUIRE(U(VarR8Round));
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        double x = (i - 1000) * 0.0173 + 0.0001 * i, a = 0, b = 0; int n = i % 8;
        if (U(VarR8Round)(x, n, &a) != S_OK || U(VarR8Round)(a, n, &b) != S_OK || a != b) bad++;
        if (fabs(a - x) > 0.5 * pow(10, -n) * 1.0000001) bad++;
    }
    EQ(bad, 0);
}
TEST(gap6_r8round_more_decimals_than_precision) {
    REQUIRE(U(VarR8Round));
    double o = 0;
    EQ(U(VarR8Round)(3.14159, 22, &o), S_OK); CHECK(fabs(o - 3.14159) < 1e-12);
    EQ(U(VarR8Round)(1e-30, 22, &o), S_OK); CHECK(o == 0.0 || fabs(o) < 1e-21);
}

/* ------------------------------------------------------------ VariantCopyInd */
typedef struct { IUnknown iface; LONG ref; } CU;
static HRESULT WINAPI cu_qi(IUnknown *i, REFIID r, void **p) { *p = i; ((CU *)i)->ref++; return S_OK; }
static ULONG WINAPI cu_ar(IUnknown *i) { return ++((CU *)i)->ref; }
static ULONG WINAPI cu_rl(IUnknown *i) { return --((CU *)i)->ref; }
static IUnknownVtbl cu_vtbl = { cu_qi, cu_ar, cu_rl };
static void cu_init(CU *c) { c->iface.lpVtbl = &cu_vtbl; c->ref = 1; }

#define POISON(v) memset(&(v), 0xCC, sizeof(v))

TEST(gap6_vci_byref_scalar_table) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    union { signed char i1; BYTE u1; short i2; USHORT u2; int i4; UINT u4; LONG l; float r4; double r8; LONGLONG i8; ULONGLONG u8; VARIANT_BOOL b; SCODE sc; CY cy; DATE d; } src;
    VARIANT s, d;
    static const struct { VARTYPE vt; unsigned size; } t[] = {
        {VT_I1,1},{VT_UI1,1},{VT_I2,2},{VT_UI2,2},{VT_BOOL,2},{VT_I4,4},{VT_UI4,4},{VT_R4,4},{VT_ERROR,4},{VT_INT,4},{VT_UINT,4},
        {VT_R8,8},{VT_CY,8},{VT_DATE,8},{VT_I8,8},{VT_UI8,8} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        memset(&src, 0, sizeof src);
        for (unsigned k = 0; k < t[i].size; k++) ((BYTE *)&src)[k] = (BYTE)(0x91 + 7 * k);
        VariantInit(&s); VariantInit(&d);
        V_VT(&s) = t[i].vt | VT_BYREF; V_BYREF(&s) = &src;
        POISON(d); V_VT(&d) = VT_EMPTY;
        HRESULT hr = U(VariantCopyInd)(&d, &s);
        CHECKF(hr == S_OK, "vt %u hr 0x%lx", t[i].vt, hr);
        CHECKF(V_VT(&d) == t[i].vt, "vt %u got %u", t[i].vt, V_VT(&d));
        CHECKF(!memcmp(&V_I1(&d), &src, t[i].size), "vt %u payload", t[i].vt);
        CHECKF(V_VT(&s) == (t[i].vt | VT_BYREF) && V_BYREF(&s) == &src, "vt %u source modified", t[i].vt);
    }
}
TEST(gap6_vci_non_byref_is_variantcopy) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear)); REQUIRE(U(SysAllocString));
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&s) = VT_BSTR; V_BSTR(&s) = U(SysAllocString)(L"copy me");
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) != V_BSTR(&s) && !lstrcmpW(V_BSTR(&d), L"copy me"));
    U(VariantClear)(&s); U(VariantClear)(&d);
    V_VT(&s) = 0x00F0;          /* invalid vt: VariantCopy rejects it */
    HRESULT hr = U(VariantCopyInd)(&d, &s);
    CHECK(hr == DISP_E_BADVARTYPE);
}
TEST(gap6_vci_byref_bstr_duplicates) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear)); REQUIRE(U(SysAllocString));
    BSTR b = U(SysAllocString)(L"referenced");
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) != b && !lstrcmpW(V_BSTR(&d), L"referenced"));
    U(VariantClear)(&d);
    U(SysFreeString)(b);
}
TEST(gap6_vci_byref_null_bstr) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    BSTR b = NULL;
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b;
    HRESULT hr = U(VariantCopyInd)(&d, &s);
    CHECK(hr == S_OK);
    CHECK(V_VT(&d) == VT_BSTR);
    t_note("VT_BSTR|BYREF to a NULL BSTR: result bstrVal=%p", V_BSTR(&d));
    U(VariantClear)(&d);
}
TEST(gap6_vci_dest_is_cleared_first) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    CU o; cu_init(&o);
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    int v = 1234;
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = &o.iface; o.ref = 5;
    V_VT(&s) = VT_I4 | VT_BYREF; V_I4REF(&s) = &v;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    EQ(o.ref, 4);                              /* old content released exactly once */
    CHECK(V_VT(&d) == VT_I4 && V_I4(&d) == 1234);
    /* non-byref source: VariantCopy also clears */
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = &o.iface; o.ref = 5;
    V_VT(&s) = VT_I4; V_I4(&s) = 5;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    EQ(o.ref, 4);
}
TEST(gap6_vci_byref_unknown_dispatch_addref) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    CU o; cu_init(&o);
    IUnknown *p = &o.iface;
    VARTYPE vts[2] = { VT_UNKNOWN, VT_DISPATCH };
    for (int i = 0; i < 2; i++) {
        o.ref = 1;
        VARIANT s, d; VariantInit(&s); VariantInit(&d);
        V_VT(&s) = vts[i] | VT_BYREF; V_BYREF(&s) = &p;
        EQ(U(VariantCopyInd)(&d, &s), S_OK);
        CHECK(V_VT(&d) == vts[i]);
        CHECK(V_UNKNOWN(&d) == &o.iface);
        EQ(o.ref, 2);
        U(VariantClear)(&d);
        EQ(o.ref, 1);
    }
    IUnknown *np = NULL;
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&s) = VT_UNKNOWN | VT_BYREF; V_BYREF(&s) = &np;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    CHECK(V_VT(&d) == VT_UNKNOWN && V_UNKNOWN(&d) == NULL);
}
TEST(gap6_vci_byref_decimal) {
    REQUIRE(U(VariantCopyInd));
    DECIMAL dec; memset(&dec, 0, sizeof dec);
    dec.scale = 3; dec.sign = DECIMAL_NEG; dec.Hi32 = 0x11; dec.Lo64 = 0x123456789ABCDEFULL;
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&s) = VT_DECIMAL | VT_BYREF; V_BYREF(&s) = &dec;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    CHECK(V_VT(&d) == VT_DECIMAL);
    CHECK(V_DECIMAL(&d).scale == 3 && V_DECIMAL(&d).sign == DECIMAL_NEG && V_DECIMAL(&d).Hi32 == 0x11 && V_DECIMAL(&d).Lo64 == 0x123456789ABCDEFULL);
}
TEST(gap6_vci_byref_variant_recurses) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    int v = 77;
    VARIANT inner, mid, outer, d; VariantInit(&inner); VariantInit(&mid); VariantInit(&outer); VariantInit(&d);
    V_VT(&inner) = VT_I4 | VT_BYREF; V_I4REF(&inner) = &v;          /* inner holds a ref to int */
    V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
    EQ(U(VariantCopyInd)(&d, &outer), S_OK);
    CHECK(V_VT(&d) == VT_I4 && V_I4(&d) == 77);                      /* both levels of indirection are removed */
    /* inner plain value */
    V_VT(&mid) = VT_R8; V_R8(&mid) = 6.5;
    V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &mid;
    EQ(U(VariantCopyInd)(&d, &outer), S_OK);
    CHECK(V_VT(&d) == VT_R8 && V_R8(&d) == 6.5);
}
TEST(gap6_vci_variant_byref_to_variant_byref_invalid) {
    REQUIRE(U(VariantCopyInd));
    VARIANT inner, outer, d; VariantInit(&inner); VariantInit(&outer); VariantInit(&d);
    VARIANT x; VariantInit(&x);
    V_VT(&inner) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&inner) = &x;
    V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
    EQ(U(VariantCopyInd)(&d, &outer), E_INVALIDARG);
}
TEST(gap6_vci_byref_unsupported_base_types) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    static const VARTYPE bad[] = { VT_EMPTY, VT_NULL, VT_VOID, VT_HRESULT, VT_PTR, VT_LPSTR, VT_LPWSTR, VT_RECORD + 1, 0x30, 0x1FFF };
    int x[4] = { 0 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        VARIANT s, d; VariantInit(&s); VariantInit(&d);
        V_VT(&s) = bad[i] | VT_BYREF; V_BYREF(&s) = x;
        HRESULT hr = U(VariantCopyInd)(&d, &s);
        CHECKF(hr == E_INVALIDARG, "base vt 0x%x hr 0x%lx", bad[i], hr);
        U(VariantClear)(&d);
    }
}
TEST(gap6_vci_byref_array) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAY *a = U(SafeArrayCreateVector)(VT_I4, 0, 3);
    CHECK(a != NULL); if (!a) return;
    int *p = a->pvData; p[0] = 9; p[1] = 8; p[2] = 7;
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&s) = VT_ARRAY | VT_I4 | VT_BYREF; V_ARRAYREF(&s) = &a;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    CHECK(V_VT(&d) == (VT_ARRAY | VT_I4));
    CHECK(V_ARRAY(&d) != NULL && V_ARRAY(&d) != a);
    if (V_ARRAY(&d)) { int *q = V_ARRAY(&d)->pvData; CHECK(q[0] == 9 && q[1] == 8 && q[2] == 7); }
    U(VariantClear)(&d);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_vci_self_copy_scalar) {
    REQUIRE(U(VariantCopyInd));
    int i4 = -31337; double r8 = 123456.789; short b = VARIANT_TRUE;
    VARIANT v; VariantInit(&v);
    V_VT(&v) = VT_I4 | VT_BYREF; V_I4REF(&v) = &i4;
    EQ(U(VariantCopyInd)(&v, &v), S_OK);
    CHECK(V_VT(&v) == VT_I4 && V_I4(&v) == -31337);
    V_VT(&v) = VT_R8 | VT_BYREF; V_R8REF(&v) = &r8;
    EQ(U(VariantCopyInd)(&v, &v), S_OK);
    CHECK(V_VT(&v) == VT_R8 && V_R8(&v) == 123456.789);
    V_VT(&v) = VT_BOOL | VT_BYREF; V_BOOLREF(&v) = &b;
    EQ(U(VariantCopyInd)(&v, &v), S_OK);
    CHECK(V_VT(&v) == VT_BOOL && V_BOOL(&v) == VARIANT_TRUE);
}
TEST(gap6_vci_self_copy_bstr) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear)); REQUIRE(U(SysAllocString));
    BSTR b = U(SysAllocString)(L"self");
    VARIANT v; VariantInit(&v);
    V_VT(&v) = VT_BSTR | VT_BYREF; V_BSTRREF(&v) = &b;
    EQ(U(VariantCopyInd)(&v, &v), S_OK);
    CHECK(V_VT(&v) == VT_BSTR && V_BSTR(&v) != b && !lstrcmpW(V_BSTR(&v), L"self"));
    U(VariantClear)(&v);
    U(SysFreeString)(b);
}
TEST(gap6_vci_byref_nested_variant_clears_dest_before_failure) {
    REQUIRE(U(VariantCopyInd));
    CU o; cu_init(&o); o.ref = 3;
    VARIANT x, inner, outer, d; VariantInit(&x); VariantInit(&inner); VariantInit(&outer); VariantInit(&d);
    V_VT(&inner) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&inner) = &x;
    V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = &o.iface;
    HRESULT hr = U(VariantCopyInd)(&d, &outer);
    CHECK(hr == E_INVALIDARG);
    EQ(o.ref, 2);                             /* dest was cleared before the type walk failed */
    CHECK(V_VT(&d) == VT_EMPTY);
}
TEST(gap6_vci_byref_does_not_copy_into_overlap_dest_types) {
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantClear));
    /* destination of a different, previously set type is replaced, including its vt */
    short sh = 5;
    VARIANT s, d; VariantInit(&s); VariantInit(&d);
    V_VT(&d) = VT_R8; V_R8(&d) = 9.5;
    V_VT(&s) = VT_I2 | VT_BYREF; V_I2REF(&s) = &sh;
    EQ(U(VariantCopyInd)(&d, &s), S_OK);
    CHECK(V_VT(&d) == VT_I2 && V_I2(&d) == 5);
}
