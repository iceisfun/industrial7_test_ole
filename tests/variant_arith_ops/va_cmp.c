/* VarCmp: ordering of numeric types, NULL / EMPTY / string rules, flags, rejected types, DECIMAL and DATE.
 * VARCMP_LT = 0, VARCMP_EQ = 1, VARCMP_GT = 2, VARCMP_NULL = 3. */
#include "va.h"

static HRESULT cmp0(const vcell *l, const vcell *r) { return va_cmp(l, r, LCID_US, 0, 0, 0); }
static HRESULT swapres(HRESULT e) { return e == VARCMP_LT ? VARCMP_GT : e == VARCMP_GT ? VARCMP_LT : e; }
static void chk_cmp(int line, const vcell *l, const vcell *r, HRESULT exp)
{
    HRESULT h = cmp0(l, r);
    CHECKF(h == exp, "VarCmp(%s,%s) [line %d] got 0x%lx want 0x%lx", va_vtname(l->vt), va_vtname(r->vt), line, (long)h, (long)exp);
}
static void chk_cmp_swap(int line, const vcell *l, const vcell *r, HRESULT exp) { chk_cmp(line, l, r, exp); chk_cmp(line, r, l, swapres(exp)); }
#define CMP(L,R,EXP) do { vcell l_ = L, r_ = R; chk_cmp(__LINE__, &l_, &r_, EXP); } while (0)
#define CMPSWAP(L,R,EXP) do { vcell l_ = L, r_ = R; chk_cmp_swap(__LINE__, &l_, &r_, EXP); } while (0)
#define CMPF(L,R,FLAGS,EXP) do { vcell l_ = L, r_ = R; HRESULT h_ = va_cmp(&l_, &r_, LCID_US, FLAGS, 0, 0); \
    CHECKF(h_ == (EXP), "VarCmp(%s,%s,flags 0x%x) [line %d] got 0x%lx want 0x%lx", va_vtname(l_.vt), va_vtname(r_.vt), (unsigned)(FLAGS), __LINE__, (long)h_, (long)(EXP)); } while (0)

/* numeric sample of value v (1, 2) for each vt */
static void numcell(vcell *c, unsigned vt, int v)
{
    memset(c, 0, sizeof(*c)); c->vt = (unsigned short)vt;
    switch (vt) {
    case VT_R4: case VT_R8: case VT_DATE: c->d = v; break;
    case VT_CY: c->i = (long long)v * 10000; break;
    case VT_BOOL: c->i = v ? -1 : 0; break;
    case VT_DECIMAL: c->i = v; c->d = 0; break;
    case VT_EMPTY: break;
    default: c->i = v; break;
    }
}

TEST(variant_arith_ops_cmp_numeric_matrix)
{
    static const unsigned good[] = { VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_UI1, VT_I8, VT_DECIMAL, VT_INT };
    size_t a, b;
    REQUIRE(U(VarCmp)); REQUIRE(U(SysAllocString));
    for (a = 0; a < sizeof(good)/sizeof(good[0]); a++)
        for (b = 0; b < sizeof(good)/sizeof(good[0]); b++) {
            vcell x1, x2, y1, y2;
            if (good[b] == VT_INT) continue;          /* INT is only accepted as the left operand */
            numcell(&x1, good[a], 1); numcell(&x2, good[a], 2);
            numcell(&y1, good[b], 1); numcell(&y2, good[b], 2);
            CMP(x1, y1, VARCMP_EQ);
            CMP(x2, y2, VARCMP_EQ);
            CMP(x1, y2, VARCMP_LT);
            CMP(x2, y1, VARCMP_GT);
        }
    /* INT on the left */
    {
        vcell i1, i2, o1; size_t k;
        numcell(&i1, VT_INT, 1); numcell(&i2, VT_INT, 2);
        for (k = 0; k < sizeof(good)/sizeof(good[0]) - 1; k++) {
            numcell(&o1, good[k], 1);
            CMP(i1, o1, VARCMP_EQ);
            CMP(i2, o1, VARCMP_GT);
        }
    }
}

TEST(variant_arith_ops_cmp_rejected_types)
{
    static const unsigned bad[] = { VT_I1, VT_UI2, VT_UI4, VT_UI8, VT_UINT };
    size_t k;
    REQUIRE(U(VarCmp));
    for (k = 0; k < sizeof(bad)/sizeof(bad[0]); k++) {
        vcell b1, ok1; HRESULT h;
        numcell(&b1, bad[k], 1); numcell(&ok1, VT_I4, 1);
        h = cmp0(&b1, &ok1);
        CHECKF(h == DISP_E_TYPEMISMATCH || h == DISP_E_BADVARTYPE, "VarCmp(%s,I4) hr 0x%lx want a type error", va_vtname(bad[k]), (long)h);
        h = cmp0(&ok1, &b1);
        CHECKF(h == DISP_E_TYPEMISMATCH || h == DISP_E_BADVARTYPE, "VarCmp(I4,%s) hr 0x%lx want a type error", va_vtname(bad[k]), (long)h);
        h = cmp0(&b1, &b1);
        CHECKF(h == DISP_E_TYPEMISMATCH || h == DISP_E_BADVARTYPE, "VarCmp(%s,%s) hr 0x%lx want a type error", va_vtname(bad[k]), va_vtname(bad[k]), (long)h);
    }
    /* INT is rejected as the right operand */
    { vcell l, r; HRESULT h; numcell(&l, VT_I4, 1); numcell(&r, VT_INT, 1); h = cmp0(&l, &r);
      CHECKF(h == DISP_E_TYPEMISMATCH || h == DISP_E_BADVARTYPE, "VarCmp(I4,INT) hr 0x%lx want a type error", (long)h); }
    /* extra vt flags and invalid types */
    {
        vcell l = C_I4(1); vcell r = C_I4(1); HRESULT h;
        h = va_cmp(&l, &r, LCID_US, 0, VT_BYREF, 0);  CHECKF(FAILED(h), "VarCmp(BYREF|I4,I4) hr 0x%lx", (long)h);
        h = va_cmp(&l, &r, LCID_US, 0, 0, VT_ARRAY);  CHECKF(FAILED(h), "VarCmp(I4,ARRAY|I4) hr 0x%lx", (long)h);
        h = va_cmp(&l, &r, LCID_US, 0, VT_VECTOR, 0); CHECKF(FAILED(h), "VarCmp(VEC|I4,I4) hr 0x%lx", (long)h);
        { vcell e1 = C_ERROR(DISP_E_PARAMNOTFOUND), e2 = C_ERROR(DISP_E_PARAMNOTFOUND), u = C_UI1(0);
          CHECKF(cmp0(&e1, &e2) == VARCMP_EQ, "VarCmp(ERROR,ERROR) must be EQ");
          h = cmp0(&e1, &u); CHECKF(h == DISP_E_TYPEMISMATCH, "VarCmp(ERROR,UI1) hr 0x%lx want TYPEMISMATCH", (long)h); }
        { vcell x = C_RAW(VT_CLSID), y = C_I4(1); h = cmp0(&x, &y); CHECKF(FAILED(h), "VarCmp(CLSID,I4) hr 0x%lx", (long)h); }
        { vcell x = C_RAW(15), y = C_I4(1); h = cmp0(&x, &y); CHECKF(FAILED(h), "VarCmp(15,I4) hr 0x%lx", (long)h); }
    }
}

TEST(variant_arith_ops_cmp_null_empty)
{
    REQUIRE(U(VarCmp)); REQUIRE(U(SysAllocString));
    CMP(C_NULL, C_NULL, VARCMP_NULL);
    CMPSWAP(C_NULL, C_I4(0), VARCMP_NULL);
    CMPSWAP(C_NULL, C_I2(5), VARCMP_NULL);
    CMPSWAP(C_NULL, C_R8(1.5), VARCMP_NULL);
    CMPSWAP(C_NULL, C_BSTR(L"x"), VARCMP_NULL);
    CMPSWAP(C_NULL, C_BSTR(L""), VARCMP_NULL);
    CMPSWAP(C_NULL, C_EMPTY, VARCMP_NULL);
    CMPSWAP(C_NULL, C_BOOL(-1), VARCMP_NULL);
    CMPSWAP(C_NULL, C_DATE(25570.0), VARCMP_NULL);
    CMPSWAP(C_NULL, C_CY(10000), VARCMP_NULL);
    CMPSWAP(C_NULL, C_DEC(0, 0, 0, 1), VARCMP_NULL);
    CMP(C_EMPTY, C_EMPTY, VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_I2(0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_I4(0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_R8(0.0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_BOOL(0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_CY(0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_DATE(0.0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_I8(0), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_I4(1), VARCMP_LT);
    CMPSWAP(C_EMPTY, C_I4(-1), VARCMP_GT);
    CMPSWAP(C_EMPTY, C_R8(-0.5), VARCMP_GT);
    CMPSWAP(C_EMPTY, C_BSTR(L""), VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_BSTRNULL, VARCMP_EQ);
    CMPSWAP(C_EMPTY, C_BSTR(L"a"), VARCMP_LT);
    CMPSWAP(C_EMPTY, C_BSTR(L"0"), VARCMP_LT);
}

TEST(variant_arith_ops_cmp_bool_and_sign)
{
    REQUIRE(U(VarCmp));
    CMPSWAP(C_BOOL(-1), C_BOOL(-1), VARCMP_EQ);
    CMPSWAP(C_BOOL(0), C_BOOL(0), VARCMP_EQ);
    CMPSWAP(C_BOOL(0), C_BOOL(-1), VARCMP_GT);     /* FALSE = 0 is greater than TRUE = -1 */
    CMPSWAP(C_BOOL(-1), C_I2(-1), VARCMP_EQ);
    CMPSWAP(C_BOOL(-1), C_I4(-1), VARCMP_EQ);
    CMPSWAP(C_BOOL(-1), C_I4(1), VARCMP_LT);
    CMPSWAP(C_BOOL(0), C_I4(0), VARCMP_EQ);
    CMPSWAP(C_BOOL(0), C_I2(1), VARCMP_LT);
    CMPSWAP(C_UI1(255), C_I2(-1), VARCMP_GT);
    CMPSWAP(C_UI1(255), C_I2(255), VARCMP_EQ);
    CMPSWAP(C_UI1(0), C_I2(-1), VARCMP_GT);
    CMPSWAP(C_I2(-32768), C_I2(32767), VARCMP_LT);
    CMPSWAP(C_I4(I4_MIN), C_I4(I4_MAX), VARCMP_LT);
    CMPSWAP(C_I8(I8_MIN), C_I8(I8_MAX), VARCMP_LT);
    CMPSWAP(C_I8(I8_MAX), C_I8(I8_MAX - 1), VARCMP_GT);
    CMPSWAP(C_I8(I8_MAX), C_I8(I8_MAX), VARCMP_EQ);
    CMPSWAP(C_I8(-1), C_I4(-1), VARCMP_EQ);
    CMPSWAP(C_I8(0x100000000LL), C_I4(1), VARCMP_GT);
    CMPSWAP(C_I2(-1), C_I4(-1), VARCMP_EQ);
    CMPSWAP(C_I4(-1), C_UI1(1), VARCMP_LT);
    CMPSWAP(C_CY(15000), C_R8(1.5), VARCMP_EQ);
    CMPSWAP(C_CY(15001), C_R8(1.5), VARCMP_GT);
    CMPSWAP(C_CY(-15000), C_I4(-1), VARCMP_LT);
    CMPSWAP(C_CY(10000), C_I4(1), VARCMP_EQ);
    CMPSWAP(C_CY(10000), C_CY(10000), VARCMP_EQ);
    CMPSWAP(C_CY(10000), C_CY(10001), VARCMP_LT);
    CMPSWAP(C_DATE(1.5), C_R8(1.5), VARCMP_EQ);
    CMPSWAP(C_DATE(-1.5), C_DATE(-1.25), VARCMP_LT);
    CMPSWAP(C_DATE(2.0), C_I4(2), VARCMP_EQ);
    CMPSWAP(C_R4(0.5f), C_R8(0.5), VARCMP_EQ);
    CMPSWAP(C_R4(0.5f), C_R8(0.75), VARCMP_LT);
    CMPSWAP(C_R4(-0.5f), C_R4(-0.25f), VARCMP_LT);
    CMPSWAP(C_R8(-0.0), C_R8(0.0), VARCMP_EQ);
    CMPSWAP(C_R8(R8_MAX), C_R8(-R8_MAX), VARCMP_GT);
    CMPSWAP(C_R8(R8_MIN), C_R8(0.0), VARCMP_GT);
    CMPSWAP(C_R8(1.0), C_I4(1), VARCMP_EQ);
    CMPSWAP(C_R8(1.0000001), C_I4(1), VARCMP_GT);
    CMP(C_INT(-4), C_I2(-4), VARCMP_EQ);
    CMP(C_INT(-4), C_I2(-3), VARCMP_LT);
}

TEST(variant_arith_ops_cmp_decimal)
{
    REQUIRE(U(VarCmp)); REQUIRE(U(SysAllocString));
    CMPSWAP(C_DEC(0, 0, 0, 1), C_DEC(0, 0, 0, 1), VARCMP_EQ);
    CMPSWAP(C_DEC(0, 0, 0, 1), C_DEC(0, 0, 0, 2), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0x80, 0, 1), C_DEC(0, 0, 0, 1), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0x80, 0, 5), C_DEC(0, 0x80, 0, 3), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0, 0, 0), C_DEC(0, 0x80, 0, 0), VARCMP_EQ);   /* +0 == -0 */
    CMPSWAP(C_DEC(2, 0, 0, 150), C_DEC(1, 0, 0, 15), VARCMP_EQ);   /* 1.50 == 1.5 */
    CMPSWAP(C_DEC(2, 0, 0, 150), C_DEC(0, 0, 0, 2), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0, 0, 1), C_I4(1), VARCMP_EQ);
    CMPSWAP(C_DEC(1, 0, 0, 15), C_R8(1.5), VARCMP_EQ);
    CMPSWAP(C_DEC(1, 0, 0, 15), C_I4(1), VARCMP_GT);
    CMPSWAP(C_DEC(1, 0x80, 0, 15), C_I4(-1), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0, 0, 0), C_EMPTY, VARCMP_EQ);
    CMPSWAP(C_DEC(0, 0, 0, 0), C_BSTR(L"0"), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), C_DEC(0, 0, 0xffffffffu, 0xfffffffffffffffeULL), VARCMP_GT);
    CMPSWAP(C_DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), C_R8(R8_MAX), VARCMP_LT);
    CMPSWAP(C_DEC(0, 0x80, 0xffffffffu, 0xffffffffffffffffULL), C_R8(-R8_MAX), VARCMP_GT);
    CMPSWAP(C_DEC(0, 0, 1, 0), C_I8(I8_MAX), VARCMP_GT);           /* 2^64 > 2^63 - 1 */
}

TEST(variant_arith_ops_cmp_strings)
{
    REQUIRE(U(VarCmp)); REQUIRE(U(SysAllocString));
    CMP(C_BSTR(L"abc"), C_BSTR(L"abc"), VARCMP_EQ);
    CMP(C_BSTR(L""), C_BSTR(L""), VARCMP_EQ);
    CMP(C_BSTRNULL, C_BSTR(L""), VARCMP_EQ);
    CMP(C_BSTRNULL, C_BSTRNULL, VARCMP_EQ);
    CMPSWAP(C_BSTR(L"a"), C_BSTR(L"b"), VARCMP_LT);
    CMPSWAP(C_BSTR(L"ab"), C_BSTR(L"abc"), VARCMP_LT);
    CMPSWAP(C_BSTR(L""), C_BSTR(L"a"), VARCMP_LT);
    CMPSWAP(C_BSTRNULL, C_BSTR(L"a"), VARCMP_LT);
    /* strings compare as text, not numbers, when both are strings */
    CMPSWAP(C_BSTR(L"10"), C_BSTR(L"9"), VARCMP_LT);
    CMPSWAP(C_BSTR(L"-1"), C_BSTR(L"7"), VARCMP_LT);
    CMPSWAP(C_BSTR(L"7"), C_BSTR(L"42"), VARCMP_GT);
    /* NORM_IGNORECASE makes case differences vanish */
    CMPF(C_BSTR(L"abc"), C_BSTR(L"ABC"), NORM_IGNORECASE, VARCMP_EQ);
    CMPF(C_BSTR(L"ABC"), C_BSTR(L"abc"), NORM_IGNORECASE, VARCMP_EQ);
    CMPF(C_BSTR(L"abc"), C_BSTR(L"ABD"), NORM_IGNORECASE, VARCMP_LT);
    CMPF(C_BSTR(L"abd"), C_BSTR(L"ABC"), NORM_IGNORECASE, VARCMP_GT);
    { vcell a = C_BSTR(L"abc"), b = C_BSTR(L"ABC"); HRESULT h = cmp0(&a, &b);
      CHECKF(h == VARCMP_LT || h == VARCMP_GT, "VarCmp(abc,ABC) without NORM_IGNORECASE must not be EQ (got %ld)", (long)h); }
    CMPF(C_BSTR(L"a"), C_BSTR(L"a"), NORM_IGNORECASE | NORM_IGNOREWIDTH, VARCMP_EQ);
    /* a string against a number: the string parsed as a number is not used, it sorts after (flags 0) */
    CMP(C_BSTR(L"7"), C_R8(7.0), VARCMP_GT);
    CMP(C_I4(7), C_BSTR(L"7"), VARCMP_LT);
    CMP(C_BSTR(L"abc"), C_I4(3), VARCMP_GT);
    CMP(C_I4(3), C_BSTR(L"abc"), VARCMP_LT);
    CMP(C_BSTR(L""), C_I2(-1), VARCMP_GT);
}

TEST(variant_arith_ops_cmp_does_not_modify)
{
    VARIANT l, r; HRESULT h; vcell lc = C_I4(7), rc = C_BSTR(L"7"); VARIANT l0, r0;
    REQUIRE(U(VarCmp)); REQUIRE(U(SysAllocString));
    va_make(&l, &lc); va_make(&r, &rc); l0 = l; r0 = r;
    h = U(VarCmp)(&l, &r, LCID_US, 0);
    CHECKF(h == VARCMP_LT, "VarCmp(I4 7, \"7\") got 0x%lx", (long)h);
    CHECK(!memcmp(&l, &l0, sizeof(l)));
    CHECK(!memcmp(&r, &r0, sizeof(r)));
    va_free(&l); va_free(&r);
    /* EMPTY operand with garbage payload is not touched */
    memset(&l, 0, sizeof(l)); V_VT(&l) = VT_EMPTY; V_I4(&l) = 19;
    memset(&r, 0, sizeof(r)); V_VT(&r) = VT_I2; V_I2(&r) = 0;
    h = U(VarCmp)(&l, &r, LCID_US, 0);
    CHECKF(h == VARCMP_EQ, "VarCmp(EMPTY, I2 0) got 0x%lx", (long)h);
    CHECK(V_VT(&l) == VT_EMPTY && V_I4(&l) == 19);
}
