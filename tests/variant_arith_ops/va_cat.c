/* VarCat: operand type acceptance (matrix), NULL / EMPTY handling and concatenation values. */
#include "va.h"

/* expected hr / vt for VarCat(l, r) with zero-valued operands; returns the hr, sets *rv. */
/* adapted from Wine dlls/oleaut32/tests/vartest.c test_VarCat (LGPL-2.1+) */
static HRESULT cat_model(unsigned l, unsigned r, unsigned *rv)
{
    #define IN(x) ((x) == VT_I2 || (x) == VT_I4 || (x) == VT_R4 || (x) == VT_R8 || (x) == VT_CY || (x) == VT_BOOL || (x) == VT_BSTR || (x) == VT_I1 || \
                   (x) == VT_UI1 || (x) == VT_UI2 || (x) == VT_UI4 || (x) == VT_I8 || (x) == VT_UI8 || (x) == VT_INT || (x) == VT_UINT || \
                   (x) == VT_EMPTY || (x) == VT_NULL || (x) == VT_DECIMAL || (x) == VT_DATE)
    *rv = VT_EMPTY;
    if (l == VT_NULL && r == VT_NULL) { *rv = VT_NULL; return S_OK; }
    if (l == VT_VARIANT && (r == VT_ERROR || r == VT_DATE || r == VT_DECIMAL)) return DISP_E_TYPEMISMATCH;
    if (IN(l) && IN(r)) { *rv = VT_BSTR; return S_OK; }
    if (r == VT_ERROR && l < VT_VOID) return DISP_E_TYPEMISMATCH;
    if (l == VT_ERROR && (r == VT_DATE || r == VT_ERROR || r == VT_DECIMAL)) return DISP_E_TYPEMISMATCH;
    if (r == VT_DATE || r == VT_ERROR || r == VT_DECIMAL) return DISP_E_BADVARTYPE;
    if (l == VT_ERROR || r == VT_ERROR) return DISP_E_TYPEMISMATCH;
    if (l == VT_VARIANT) return DISP_E_TYPEMISMATCH;
    if (r == VT_VARIANT && IN(l)) return DISP_E_TYPEMISMATCH;
    return DISP_E_BADVARTYPE;
    #undef IN
}

TEST(variant_arith_ops_cat_vt_matrix)
{
    unsigned l, r;
    REQUIRE(U(VarCat)); REQUIRE(U(SysAllocString));
    for (l = 0; l <= 0xfff; l++) {
        if (l > 82 && l < 4085) { l = 4084; continue; }
        if (l == VT_DISPATCH || l == VT_UNKNOWN || l == VT_RECORD || l == 15) continue;
        for (r = 0; r <= 0xfff; r++) {
            VARIANT L, R, res; HRESULT hr, want; unsigned rv; vcell lc, rc;
            if (r > 82 && r < 4085) { r = 4084; continue; }
            if (r == VT_DISPATCH || r == VT_UNKNOWN || r == VT_RECORD || r == 15) continue;
            want = cat_model(l, r, &rv);
            memset(&lc, 0, sizeof(lc)); memset(&rc, 0, sizeof(rc));
            lc.vt = (unsigned short)l; rc.vt = (unsigned short)r;
            if (l == VT_BSTR) lc.s = L""; if (r == VT_BSTR) rc.s = L"";
            va_make(&L, &lc); va_make(&R, &rc);
            if (l == VT_DECIMAL) { DECIMAL d; memset(&d, 0, sizeof(d)); V_DECIMAL(&L) = d; V_VT(&L) = VT_DECIMAL; }
            if (r == VT_DECIMAL) { DECIMAL d; memset(&d, 0, sizeof(d)); V_DECIMAL(&R) = d; V_VT(&R) = VT_DECIMAL; }
            memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
            hr = U(VarCat)(&L, &R, &res);
            CHECKF(hr == want, "VarCat(vt %u, vt %u) hr 0x%lx want 0x%lx", l, r, (long)hr, (long)want);
            if (hr == want) CHECKF(V_VT(&res) == rv, "VarCat(vt %u, vt %u) result vt %u want %u", l, r, V_VT(&res), rv);
            if (V_VT(&res) == VT_BSTR) U(SysFreeString)(V_BSTR(&res));
            va_free(&L); va_free(&R);
        }
    }
}

static void catchk(int line, vcell l, vcell r, const WCHAR *want)
{
    VARIANT L, R, res; HRESULT hr;
    va_make(&L, &l); va_make(&R, &r);
    memset(&res, 0, sizeof(res));
    hr = U(VarCat)(&L, &R, &res);
    CHECKF(hr == S_OK, "[line %d] VarCat(%s,%s) hr 0x%lx", line, va_vtname(l.vt), va_vtname(r.vt), (long)hr);
    if (hr == S_OK) {
        CHECKF(V_VT(&res) == VT_BSTR, "[line %d] result vt %u want BSTR", line, V_VT(&res));
        if (V_VT(&res) == VT_BSTR) {
            CHECKF(V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), want), "[line %d] VarCat(%s,%s) wrong string", line, va_vtname(l.vt), va_vtname(r.vt));
            CHECKF(U(SysStringLen)(V_BSTR(&res)) == (UINT)lstrlenW(want), "[line %d] SysStringLen %u want %d", line, U(SysStringLen)(V_BSTR(&res)), lstrlenW(want));
            U(SysFreeString)(V_BSTR(&res));
        }
    }
    va_free(&L); va_free(&R);
}
#define CATOK(L,R,W) do { vcell l_ = L, r_ = R; catchk(__LINE__, l_, r_, W); } while (0)

TEST(variant_arith_ops_cat_values)
{
    REQUIRE(U(VarCat)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysStringLen));
    CATOK(C_BSTR(L"12"), C_BSTR(L"34"), L"1234");
    CATOK(C_BSTR(L""), C_BSTR(L""), L"");
    CATOK(C_BSTR(L"abc"), C_BSTR(L""), L"abc");
    CATOK(C_BSTR(L""), C_BSTR(L"abc"), L"abc");
    CATOK(C_BSTR(L"a b"), C_BSTR(L" c"), L"a b c");
    CATOK(C_BSTRNULL, C_BSTR(L"x"), L"x");
    CATOK(C_BSTR(L"x"), C_BSTRNULL, L"x");
    CATOK(C_BSTRNULL, C_BSTRNULL, L"");
    /* integers are formatted in decimal */
    CATOK(C_I4(12), C_I4(34), L"1234");
    CATOK(C_I2(-5), C_I2(7), L"-57");
    CATOK(C_I4(I4_MIN), C_I4(I4_MAX), L"-21474836482147483647");
    CATOK(C_UI1(255), C_UI1(1), L"2551");
    CATOK(C_I1(-128), C_I1(127), L"-128127");
    CATOK(C_UI2(65535), C_UI2(0), L"655350");
    CATOK(C_UI4(0xffffffffu), C_EMPTY, L"4294967295");
    CATOK(C_I8(I8_MIN), C_EMPTY, L"-9223372036854775808");
    CATOK(C_UI8(UI8_MAX), C_EMPTY, L"18446744073709551615");
    CATOK(C_INT(12), C_UINT(34), L"1234");
    CATOK(C_I4(12), C_BSTR(L"34"), L"1234");
    CATOK(C_BSTR(L"12"), C_I4(34), L"1234");
    CATOK(C_BSTR(L"x"), C_I4(0), L"x0");
    /* EMPTY and NULL contribute nothing, NULL with another operand yields that operand's text */
    CATOK(C_EMPTY, C_EMPTY, L"");
    CATOK(C_EMPTY, C_BSTR(L"abc"), L"abc");
    CATOK(C_BSTR(L"abc"), C_EMPTY, L"abc");
    CATOK(C_NULL, C_BSTR(L"abc"), L"abc");
    CATOK(C_BSTR(L"abc"), C_NULL, L"abc");
    CATOK(C_NULL, C_EMPTY, L"");
    CATOK(C_EMPTY, C_NULL, L"");
    CATOK(C_NULL, C_I4(42), L"42");
    CATOK(C_I4(42), C_NULL, L"42");
    CATOK(C_EMPTY, C_I4(42), L"42");
    CATOK(C_I4(42), C_EMPTY, L"42");
    /* currency and floating point use the shortest decimal form (decimal point is locale dependent: integral only) */
    CATOK(C_CY(30000), C_BSTR(L"|"), L"3|");
    CATOK(C_R8(5.0), C_BSTR(L"|"), L"5|");
    CATOK(C_R4(-7.0f), C_BSTR(L"|"), L"-7|");
    CATOK(C_R8(1e10), C_BSTR(L"|"), L"10000000000|");
    CATOK(C_DEC(0, 0, 0, 77), C_BSTR(L"|"), L"77|");
    CATOK(C_DEC(0, 0x80, 0, 77), C_BSTR(L"|"), L"-77|");
}

TEST(variant_arith_ops_cat_errors)
{
    VARIANT l, r, res; HRESULT hr;
    REQUIRE(U(VarCat)); REQUIRE(U(SysAllocString));
    memset(&l, 0, sizeof(l)); memset(&r, 0, sizeof(r));
    V_VT(&l) = VT_ERROR; V_ERROR(&l) = DISP_E_PARAMNOTFOUND;
    V_VT(&r) = VT_BSTR; V_BSTR(&r) = U(SysAllocString)(L"1234");
    memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
    hr = U(VarCat)(&l, &r, &res);
    CHECKF(hr == DISP_E_TYPEMISMATCH, "VarCat(ERROR,BSTR) hr 0x%lx", (long)hr);
    CHECKF(VA_FAILRES_OK(V_VT(&res)), "VarCat(ERROR,BSTR) failed result vt %u want EMPTY", V_VT(&res));
    memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
    hr = U(VarCat)(&r, &l, &res);
    CHECKF(hr == DISP_E_TYPEMISMATCH, "VarCat(BSTR,ERROR) hr 0x%lx", (long)hr);
    CHECKF(VA_FAILRES_OK(V_VT(&res)), "VarCat(BSTR,ERROR) failed result vt %u want EMPTY", V_VT(&res));
    U(SysFreeString)(V_BSTR(&r));
    /* VT_BYREF / VT_ARRAY operands are not accepted */
    memset(&l, 0, sizeof(l)); V_VT(&l) = VT_I4 | VT_BYREF;
    memset(&r, 0, sizeof(r)); V_VT(&r) = VT_I4; V_I4(&r) = 1;
    memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
    hr = U(VarCat)(&l, &r, &res);
    CHECKF(hr == DISP_E_TYPEMISMATCH, "VarCat(BYREF|I4,I4) hr 0x%lx want TYPEMISMATCH (Win10)", (long)hr);
    CHECKF(VA_FAILRES_OK(V_VT(&res)), "failed result vt %u want EMPTY", V_VT(&res));
    V_VT(&l) = VT_I4 | VT_ARRAY;
    memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
    hr = U(VarCat)(&l, &r, &res);
    CHECKF(FAILED(hr), "VarCat(ARRAY|I4,I4) hr 0x%lx", (long)hr);
    CHECKF(VA_FAILRES_OK(V_VT(&res)), "failed result vt %u want EMPTY", V_VT(&res));
}
