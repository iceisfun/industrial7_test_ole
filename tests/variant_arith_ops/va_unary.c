/* VarAbs / VarNot / VarNeg / VarInt / VarFix / VarRound: type gates, failure protocol and value semantics. */
#include "va.h"

static const unsigned FLAGS[] = { 0, VT_VECTOR, VT_BYREF, VT_ARRAY, VT_RESERVED, VT_ARRAY|VT_BYREF, VT_VECTOR|VT_ARRAY, VT_BYREF|VT_RESERVED };
#define NFLAGS (sizeof(FLAGS)/sizeof(FLAGS[0]))

static void fill_sample(VARIANT *v, unsigned vt, int neg)
{
    memset(v, 0, sizeof(*v));
    V_VT(v) = (VARTYPE)vt;
    switch (vt) {
    case VT_I1: V_I1(v) = neg ? -5 : 5; break;
    case VT_UI1: V_UI1(v) = 5; break;
    case VT_I2: V_I2(v) = neg ? -5 : 5; break;
    case VT_UI2: V_UI2(v) = 5; break;
    case VT_I4: case VT_INT: V_I4(v) = neg ? -5 : 5; break;
    case VT_UI4: case VT_UINT: V_UI4(v) = 5; break;
    case VT_I8: V_I8(v) = neg ? -5 : 5; break;
    case VT_UI8: V_UI8(v) = 5; break;
    case VT_R4: V_R4(v) = neg ? -5.0f : 5.0f; break;
    case VT_R8: V_R8(v) = neg ? -5.0 : 5.0; break;
    case VT_DATE: V_DATE(v) = neg ? -5.0 : 5.0; break;
    case VT_BOOL: V_BOOL(v) = VARIANT_TRUE; break;
    case VT_CY: V_CY(v).int64 = neg ? -50000 : 50000; break;
    case VT_DECIMAL: { DECIMAL d; memset(&d, 0, sizeof(d)); d.Lo64 = 5; d.sign = neg ? 0x80 : 0; V_DECIMAL(v) = d; V_VT(v) = VT_DECIMAL; break; }
    default: break;
    }
}

/* VarAbs gate, from the decompiled dispatch on the full vt word */
static HRESULT abs_expect(unsigned vt)
{
    switch (vt) {
    case VT_EMPTY: case VT_NULL: case VT_I2: case VT_I4: case VT_R4: case VT_R8: case VT_CY: case VT_DATE:
    case VT_BOOL: case VT_DECIMAL: case VT_UI1: case VT_I8:
        return S_OK;
    case VT_ERROR: case VT_UNKNOWN: return DISP_E_TYPEMISMATCH;
    }
    if (vt & 0x2000) return DISP_E_TYPEMISMATCH;
    if (vt == VT_RECORD) return DISP_E_TYPEMISMATCH;
    return DISP_E_BADVARTYPE;
}

static int skip_vt(unsigned b) { return b == VT_BSTR || b == VT_DISPATCH || b == VT_UNKNOWN; }

TEST(variant_arith_ops_abs_vt_gate)
{
    unsigned b; size_t f;
    REQUIRE(U(VarAbs));
    for (f = 0; f < NFLAGS; f++)
        for (b = 0; b <= 36; b++) {
            VARIANT v, res; HRESULT hr, want; unsigned vt = b | FLAGS[f];
            if (b == VT_UNKNOWN && FLAGS[f] == 0) continue;
            if (b == VT_DISPATCH && FLAGS[f] == 0) continue;
            if (b == VT_BSTR && FLAGS[f] == 0) continue;
            if (b > 31 && b != VT_RECORD) continue;
            fill_sample(&v, b, 1); V_VT(&v) = (VARTYPE)vt;
            memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
            hr = U(VarAbs)(&v, &res);
            want = abs_expect(vt);
            CHECKF(hr == want, "VarAbs(vt 0x%x) hr 0x%lx want 0x%lx", vt, (long)hr, (long)want);
            if (want != S_OK) CHECKF(V_VT(&res) == VT_EMPTY, "VarAbs(vt 0x%x) failed result vt %u want EMPTY", vt, V_VT(&res));
        }
}

TEST(variant_arith_ops_abs_values)
{
    REQUIRE(U(VarAbs)); REQUIRE(U(SysAllocString));
    /* decompiled VarAbs: INT16_MIN widens to I4, INT32_MIN and INT64_MIN widen to R8 */
    N1(VA_ABS, C_I2(-32768), S_OK, C_I4(32768));
    N1(VA_ABS, C_I2(32767), S_OK, C_I2(32767));
    N1(VA_ABS, C_I2(-32767), S_OK, C_I2(32767));
    N1(VA_ABS, C_I4(I4_MIN), S_OK, C_R8(2147483648.0));
    N1(VA_ABS, C_I4(-I4_MAX), S_OK, C_I4(I4_MAX));
    N1(VA_ABS, C_I4(I4_MAX), S_OK, C_I4(I4_MAX));
    N1(VA_ABS, C_I8(I8_MIN), S_OK, C_R8(9223372036854775808.0));
    N1(VA_ABS, C_I8(-I8_MAX), S_OK, C_I8(I8_MAX));
    N1(VA_ABS, C_I8(-1), S_OK, C_I8(1));
    N1(VA_ABS, C_I8(0), S_OK, C_I8(0));
    N1(VA_ABS, C_UI1(0), S_OK, C_UI1(0));
    N1(VA_ABS, C_UI1(255), S_OK, C_UI1(255));
    N1(VA_ABS, C_BOOL(-1), S_OK, C_I2(1));
    N1(VA_ABS, C_BOOL(0), S_OK, C_I2(0));
    N1(VA_ABS, C_BOOL(1), S_OK, C_I2(1));
    N1(VA_ABS, C_EMPTY, S_OK, C_I2(0));
    N1(VA_ABS, C_NULL, S_OK, C_NULL);
    N1(VA_ABS, C_R4(-0.0f), S_OK, C_R4(0.0f));
    N1(VA_ABS, C_R4(-3.5f), S_OK, C_R4(3.5f));
    N1(VA_ABS, C_R8(-3.5), S_OK, C_R8(3.5));
    N1(VA_ABS, C_R8(-R8_MAX), S_OK, C_R8(R8_MAX));
    N1(VA_ABS, C_R8(-1e-300), S_OK, C_R8(1e-300));
    N1(VA_ABS, C_DATE(-657434.0), S_OK, C_DATE(657434.0));
    N1(VA_ABS, C_DATE(2.5), S_OK, C_DATE(2.5));
    N1(VA_ABS, C_CY(-15000), S_OK, C_CY(15000));
    N1(VA_ABS, C_CY(15000), S_OK, C_CY(15000));
    N1(VA_ABS, C_CY(0), S_OK, C_CY(0));
    N1(VA_ABS, C_DEC(0, 0x80, 0, 7), S_OK, C_DEC(0, 0, 0, 7));
    N1(VA_ABS, C_DEC(2, 0x80, 0, 12345), S_OK, C_DEC(2, 0, 0, 12345));
    N1(VA_ABS, C_DEC(0, 0, 0, 7), S_OK, C_DEC(0, 0, 0, 7));
    /* BSTR operands are parsed as R8 */
    N1(VA_ABS, C_BSTR(L"-30000"), S_OK, C_R8(30000.0));
    N1(VA_ABS, C_BSTR(L"7"), S_OK, C_R8(7.0));
    N1(VA_ABS, C_BSTR(L"  -7  "), S_OK, C_R8(7.0));
    N1(VA_ABS, C_BSTR(L"abc"), DISP_E_TYPEMISMATCH, C_NONE);
    N1(VA_ABS, C_BSTR(L""), DISP_E_TYPEMISMATCH, C_NONE);
    N1(VA_ABS, C_ERROR(DISP_E_PARAMNOTFOUND), DISP_E_TYPEMISMATCH, C_NONE);
    /* types native VarAbs rejects although they are plain numbers */
    N1(VA_ABS, C_I1(-3), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_ABS, C_UI2(3), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_ABS, C_UI4(3), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_ABS, C_UI8(3), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_ABS, C_INT(-3), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_ABS, C_UINT(3), DISP_E_BADVARTYPE, C_NONE);
}

/* Not: exact codes, following the type sets in the oleaut32 VarNot gate */
TEST(variant_arith_ops_not_vt_gate)
{
    unsigned b; size_t f;
    REQUIRE(U(VarNot));
    for (f = 0; f < NFLAGS; f++)
        for (b = 0; b <= 36; b++) {
            VARIANT v, res; HRESULT hr, want = DISP_E_BADVARTYPE; unsigned vt = b | FLAGS[f];
            int validvt;
            if (b == VT_UNKNOWN || b == VT_DISPATCH || b == VT_BSTR) { if (FLAGS[f] == 0) continue; }
            if (b > 31) continue;
            fill_sample(&v, b, 1); V_VT(&v) = (VARTYPE)vt;
            memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
            hr = U(VarNot)(&v, &res);
            if (!FLAGS[f]) {
                switch (b) {
                case VT_I1: case VT_UI1: case VT_I2: case VT_UI2: case VT_INT: case VT_UINT: case VT_I4: case VT_UI4: case VT_R4: case VT_R8:
                case VT_DECIMAL: case VT_BOOL: case VT_NULL: case VT_EMPTY: case VT_DATE: case VT_CY: case VT_I8: case VT_UI8:
                    want = S_OK; break;
                case VT_RECORD: case VT_ERROR: want = DISP_E_TYPEMISMATCH; break;
                default:
                    validvt = (b <= VT_LPWSTR && b != 15 && (b < 24 || b > 31));
                    if (validvt) want = DISP_E_TYPEMISMATCH;
                }
            } else {
                unsigned fl = FLAGS[f];
                validvt = (b <= VT_LPWSTR && b != 15 && (b < 24 || b > 31)) && b > VT_NULL &&
                          (fl == VT_BYREF || fl == VT_ARRAY || fl == (VT_ARRAY|VT_BYREF));
                if (b == VT_RECORD) validvt = 0;
                if (validvt) want = DISP_E_TYPEMISMATCH;
                if (b == VT_RECORD && (fl & VT_ARRAY)) want = DISP_E_TYPEMISMATCH;
            }
            CHECKF(hr == want, "VarNot(vt 0x%x) hr 0x%lx want 0x%lx", vt, (long)hr, (long)want);
            if (want != S_OK) CHECKF(V_VT(&res) == VT_EMPTY, "VarNot(vt 0x%x) failed result vt %u want EMPTY", vt, V_VT(&res));
        }
}

TEST(variant_arith_ops_not_values)
{
    REQUIRE(U(VarNot)); REQUIRE(U(SysAllocString));
    N1(VA_NOT, C_I2(0), S_OK, C_I2(-1));
    N1(VA_NOT, C_I2(32767), S_OK, C_I2(-32768));
    N1(VA_NOT, C_I2(-32768), S_OK, C_I2(32767));
    N1(VA_NOT, C_UI1(0xaa), S_OK, C_UI1(0x55));
    N1(VA_NOT, C_UI1(255), S_OK, C_UI1(0));
    N1(VA_NOT, C_I4(0x0f0f0f0f), S_OK, C_I4(0xf0f0f0f0u));
    N1(VA_NOT, C_I4(I4_MIN), S_OK, C_I4(I4_MAX));
    N1(VA_NOT, C_UI4(0xffffffffu), S_OK, C_I4(0));
    N1(VA_NOT, C_UI2(0xffff), S_OK, C_I4(-65536));
    N1(VA_NOT, C_I8(0), S_OK, C_I8(-1));
    N1(VA_NOT, C_I8(I8_MAX), S_OK, C_I8(I8_MIN));
    N1(VA_NOT, C_UI8(0), S_OK, C_I4(-1));
    N1(VA_NOT, C_BOOL(-1), S_OK, C_BOOL(0));
    N1(VA_NOT, C_BOOL(0), S_OK, C_BOOL(-1));
    N1(VA_NOT, C_EMPTY, S_OK, C_I2(-1));
    N1(VA_NOT, C_NULL, S_OK, C_NULL);
    /* floating point operands are rounded (banker's) to I4 first */
    N1(VA_NOT, C_R8(0.5), S_OK, C_I4(-1));
    N1(VA_NOT, C_R8(1.5), S_OK, C_I4(-3));
    N1(VA_NOT, C_R8(2.5), S_OK, C_I4(-3));
    N1(VA_NOT, C_R8(-1.5), S_OK, C_I4(1));
    N1(VA_NOT, C_R4(4.0f), S_OK, C_I4(-5));
    N1(VA_NOT, C_DATE(0.0), S_OK, C_I4(-1));
    N1(VA_NOT, C_CY(10000), S_OK, C_I4(-2));
    N1(VA_NOT, C_CY(0), S_OK, C_I4(-1));
    N1(VA_NOT, C_DEC(0, 0, 0, 3), S_OK, C_I4(-4));
    N1(VA_NOT, C_BSTR(L"#TRUE#"), S_OK, C_BOOL(0));
    N1(VA_NOT, C_BSTR(L"#FALSE#"), S_OK, C_BOOL(-1));
    N1(VA_NOT, C_ERROR(DISP_E_PARAMNOTFOUND), DISP_E_TYPEMISMATCH, C_NONE);
    N1(VA_NOT, C_RAW(VT_CLSID), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_NOT, C_RAW(15), DISP_E_BADVARTYPE, C_NONE);
    N1(VA_NOT, C_RAW(VT_VOID), DISP_E_BADVARTYPE, C_NONE);
}

TEST(variant_arith_ops_neg_fix_int_vt_gate)
{
    static const int ops[3] = { VA_NEG, VA_FIX, VA_INT };
    static const char *names[3] = { "VarNeg", "VarFix", "VarInt" };
    unsigned b; size_t f; int k;
    REQUIRE(U(VarNeg)); REQUIRE(U(VarFix)); REQUIRE(U(VarInt));
    for (k = 0; k < 3; k++)
        for (f = 0; f < NFLAGS; f++)
            for (b = 0; b <= 36; b++) {
                VARIANT v, res; HRESULT hr; unsigned vt = b | FLAGS[f]; int ok = 0;
                if (FLAGS[f] == 0 && (b == VT_UNKNOWN || b == VT_DISPATCH || b == VT_BSTR)) continue;
                if (b > 31 && b != VT_RECORD) continue;
                fill_sample(&v, b, 1); V_VT(&v) = (VARTYPE)vt;
                memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
                hr = va_un(ops[k], &v, 0, &res);
                if (!FLAGS[f]) switch (b) {
                    case VT_UI1: case VT_I2: case VT_I4: case VT_R4: case VT_R8: case VT_DECIMAL: case VT_BOOL: case VT_NULL: case VT_EMPTY:
                    case VT_DATE: case VT_CY: case VT_I8: ok = 1; }
                if (ok) CHECKF(hr == S_OK, "%s(vt 0x%x) hr 0x%lx want S_OK", names[k], vt, (long)hr);
                else {
                    CHECKF(hr == DISP_E_TYPEMISMATCH || hr == DISP_E_BADVARTYPE, "%s(vt 0x%x) hr 0x%lx want a type error", names[k], vt, (long)hr);
                    CHECKF(V_VT(&res) == VT_EMPTY, "%s(vt 0x%x) failed result vt %u want EMPTY", names[k], vt, V_VT(&res));
                }
            }
}

TEST(variant_arith_ops_neg_values)
{
    REQUIRE(U(VarNeg)); REQUIRE(U(SysAllocString));
    N1(VA_NEG, C_UI1(0), S_OK, C_I2(0));
    N1(VA_NEG, C_UI1(255), S_OK, C_I2(-255));
    N1(VA_NEG, C_I2(-32768), S_OK, C_I4(32768));
    N1(VA_NEG, C_I2(32767), S_OK, C_I2(-32767));
    N1(VA_NEG, C_I2(0), S_OK, C_I2(0));
    N1(VA_NEG, C_I4(I4_MIN), S_OK, C_R8(2147483648.0));
    N1(VA_NEG, C_I4(I4_MAX), S_OK, C_I4(-I4_MAX));
    N1(VA_NEG, C_I8(5), S_OK, C_I8(-5));
    N1(VA_NEG, C_I8(-I8_MAX), S_OK, C_I8(I8_MAX));
    N1(VA_NEG, C_BOOL(-1), S_OK, C_I2(1));
    N1(VA_NEG, C_BOOL(0), S_OK, C_I2(0));
    N1(VA_NEG, C_EMPTY, S_OK, C_I2(0));
    N1(VA_NEG, C_NULL, S_OK, C_NULL);
    N1(VA_NEG, C_R4(2.5f), S_OK, C_R4(-2.5f));
    N1(VA_NEG, C_R8(-2.5), S_OK, C_R8(2.5));
    N1(VA_NEG, C_R8(0.0), S_OK, C_R8(-0.0));
    N1(VA_NEG, C_DATE(36526.5), S_OK, C_DATE(-36526.5));
    N1(VA_NEG, C_CY(25000), S_OK, C_CY(-25000));
    N1(VA_NEG, C_CY(-1), S_OK, C_CY(1));
    N1(VA_NEG, C_DEC(0, 0, 0, 7), S_OK, C_DEC(0, 0x80, 0, 7));
    N1(VA_NEG, C_DEC(3, 0x80, 0, 1500), S_OK, C_DEC(3, 0, 0, 1500));
    N1(VA_NEG, C_BSTR(L"-1"), S_OK, C_R8(1.0));
    N1(VA_NEG, C_BSTR(L"250"), S_OK, C_R8(-250.0));
    N1(VA_NEG, C_BSTR(L"xyz"), DISP_E_TYPEMISMATCH, C_NONE);
    N1(VA_NEG, C_I1(3), HR_TYPEERR, C_NONE);
    N1(VA_NEG, C_UI2(3), HR_TYPEERR, C_NONE);
    N1(VA_NEG, C_UI4(3), HR_TYPEERR, C_NONE);
    N1(VA_NEG, C_UI8(3), HR_TYPEERR, C_NONE);
    N1(VA_NEG, C_INT(3), HR_TYPEERR, C_NONE);
    N1(VA_NEG, C_UINT(3), HR_TYPEERR, C_NONE);
    N1(VA_NEG, C_ERROR(DISP_E_PARAMNOTFOUND), DISP_E_TYPEMISMATCH, C_NONE);
}

TEST(variant_arith_ops_int_fix_values)
{
    static const struct { double in; double fix, intv; } t[] = {
        { 2.5, 2, 2 }, { 2.9, 2, 2 }, { 2.0, 2, 2 }, { 0.5, 0, 0 }, { 0.0, 0, 0 }, { -0.5, -0, -1 }, { -0.1, -0, -1 },
        { -1.0, -1, -1 }, { -1.5, -1, -2 }, { -2.5, -2, -3 }, { -2.9, -2, -3 }, { 1e10 + 0.5, 1e10, 1e10 },
        { -1e10 - 0.5, -1e10, -1e10 - 1 }, { 12345.6789, 12345, 12345 }, { -12345.6789, -12345, -12346 },
    };
    size_t i;
    REQUIRE(U(VarFix)); REQUIRE(U(VarInt)); REQUIRE(U(SysAllocString));
    for (i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
        N1(VA_FIX, C_R8(t[i].in), S_OK, C_R8(t[i].fix));
        N1(VA_INT, C_R8(t[i].in), S_OK, C_R8(t[i].intv));
        N1(VA_FIX, C_DATE(t[i].in), S_OK, C_DATE(t[i].fix));
        N1(VA_INT, C_DATE(t[i].in), S_OK, C_DATE(t[i].intv));
        if (fabs(t[i].in) < 1e6) {
            N1(VA_FIX, C_R4(t[i].in), S_OK, C_R4(t[i].fix));
            N1(VA_INT, C_R4(t[i].in), S_OK, C_R4(t[i].intv));
        }
    }
    /* integer types are returned unchanged, BOOL and EMPTY become I2 */
    N1(VA_FIX, C_I2(-7), S_OK, C_I2(-7));
    N1(VA_INT, C_I2(-7), S_OK, C_I2(-7));
    N1(VA_FIX, C_I4(I4_MIN), S_OK, C_I4(I4_MIN));
    N1(VA_INT, C_I4(I4_MAX), S_OK, C_I4(I4_MAX));
    N1(VA_FIX, C_I8(I8_MIN), S_OK, C_I8(I8_MIN));
    N1(VA_INT, C_I8(I8_MAX), S_OK, C_I8(I8_MAX));
    N1(VA_FIX, C_UI1(200), S_OK, C_UI1(200));
    N1(VA_INT, C_UI1(200), S_OK, C_UI1(200));
    N1(VA_FIX, C_BOOL(-1), S_OK, C_I2(-1));
    N1(VA_INT, C_BOOL(-1), S_OK, C_I2(-1));
    N1(VA_FIX, C_EMPTY, S_OK, C_I2(0));
    N1(VA_INT, C_EMPTY, S_OK, C_I2(0));
    N1(VA_FIX, C_NULL, S_OK, C_NULL);
    N1(VA_INT, C_NULL, S_OK, C_NULL);
    /* currency is rounded in CY units */
    N1(VA_FIX, C_CY(-15000), S_OK, C_CY(-10000));
    N1(VA_INT, C_CY(-15000), S_OK, C_CY(-20000));
    N1(VA_FIX, C_CY(15000), S_OK, C_CY(10000));
    N1(VA_INT, C_CY(15000), S_OK, C_CY(10000));
    N1(VA_FIX, C_CY(-1), S_OK, C_CY(0));
    N1(VA_INT, C_CY(-1), S_OK, C_CY(-10000));
    N1(VA_FIX, C_CY(9999), S_OK, C_CY(0));
    N1(VA_INT, C_CY(9999), S_OK, C_CY(0));
    N1(VA_FIX, C_I1(1), HR_TYPEERR, C_NONE);
    N1(VA_INT, C_UI2(1), HR_TYPEERR, C_NONE);
    N1(VA_FIX, C_ERROR(DISP_E_PARAMNOTFOUND), DISP_E_TYPEMISMATCH, C_NONE);
}

/* ---- VarRound ---- */
static HRESULT round_expect(unsigned vt)
{
    switch (vt) {
    case VT_EMPTY: case VT_NULL: case VT_I2: case VT_I4: case VT_R4: case VT_R8: case VT_CY: case VT_DATE:
    case VT_BOOL: case VT_DECIMAL: case VT_UI1: case VT_I8: case VT_UI8:
        return S_OK;
    case VT_ERROR: case VT_UNKNOWN: return DISP_E_TYPEMISMATCH;
    }
    if (vt & 0x2000) return DISP_E_TYPEMISMATCH;
    if (vt == VT_RECORD) return DISP_E_TYPEMISMATCH;
    return DISP_E_BADVARTYPE;
}

TEST(variant_arith_ops_round_vt_gate)
{
    unsigned b; size_t f;
    REQUIRE(U(VarRound));
    for (f = 0; f < NFLAGS; f++)
        for (b = 0; b <= 36; b++) {
            VARIANT v, res; HRESULT hr, want; unsigned vt = b | FLAGS[f];
            if (FLAGS[f] == 0 && (b == VT_UNKNOWN || b == VT_DISPATCH || b == VT_BSTR)) continue;
            if (b > 31 && b != VT_RECORD) continue;
            fill_sample(&v, b, 0); V_VT(&v) = (VARTYPE)vt;
            memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
            hr = U(VarRound)(&v, 2, &res);
            want = round_expect(vt);
            CHECKF(hr == want, "VarRound(vt 0x%x) hr 0x%lx want 0x%lx", vt, (long)hr, (long)want);
            if (want != S_OK) CHECKF(V_VT(&res) == VT_EMPTY, "VarRound(vt 0x%x) failed result vt %u want EMPTY", vt, V_VT(&res));
            else if (vt == VT_EMPTY || vt == VT_BOOL) CHECKF(V_VT(&res) == VT_I2, "VarRound(vt %u) result vt %u want I2", vt, V_VT(&res));
            else CHECKF(V_VT(&res) == vt, "VarRound(vt %u) result vt %u want same", vt, V_VT(&res));
        }
}

TEST(variant_arith_ops_round_negative_decimals)
{
    /* cDecimals < 0 is rejected with E_INVALIDARG before the operand type is looked at (decompiled VarRound) */
    static const int nd[] = { -1, -2, -100, I4_MIN };
    static const unsigned vts[] = { VT_EMPTY, VT_NULL, VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BOOL, VT_DECIMAL, VT_UI1, VT_I8, VT_ERROR, VT_I1 };
    size_t i, j;
    REQUIRE(U(VarRound));
    for (i = 0; i < sizeof(nd)/sizeof(nd[0]); i++)
        for (j = 0; j < sizeof(vts)/sizeof(vts[0]); j++) {
            VARIANT v, res; HRESULT hr;
            fill_sample(&v, vts[j], 0);
            memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
            hr = U(VarRound)(&v, nd[i], &res);
            CHECKF(hr == E_INVALIDARG, "VarRound(%s, %d) hr 0x%lx want E_INVALIDARG", va_vtname(vts[j]), nd[i], (long)hr);
            CHECKF(V_VT(&res) == VT_EMPTY, "VarRound(%s, %d) result vt %u want EMPTY", va_vtname(vts[j]), nd[i], V_VT(&res));
        }
}

TEST(variant_arith_ops_round_values)
{
    REQUIRE(U(VarRound)); REQUIRE(U(SysAllocString));
    /* integer-like types ignore cDecimals */
    RND(C_I2(1234), 0, S_OK, C_I2(1234));
    RND(C_I2(1234), 5, S_OK, C_I2(1234));
    RND(C_I4(-123456), 2, S_OK, C_I4(-123456));
    RND(C_UI1(200), 1, S_OK, C_UI1(200));
    RND(C_BOOL(-1), 0, S_OK, C_I2(-1));
    RND(C_EMPTY, 3, S_OK, C_I2(0));
    RND(C_NULL, 3, S_OK, C_NULL);
    RND(C_I8(I8_MAX), 0, S_OK, C_I8(I8_MAX));
    RND(C_I8(-5), 10, S_OK, C_I8(-5));
    RND(C_UI8(123), 0, S_OK, C_UI8(123));
    /* floating point: round half to even at the requested digit */
    RND(C_R8(0.5), 0, S_OK, C_R8(0.0));
    RND(C_R8(1.5), 0, S_OK, C_R8(2.0));
    RND(C_R8(2.5), 0, S_OK, C_R8(2.0));
    RND(C_R8(3.5), 0, S_OK, C_R8(4.0));
    RND(C_R8(-0.5), 0, S_OK, C_R8(0.0));
    RND(C_R8(-1.5), 0, S_OK, C_R8(-2.0));
    RND(C_R8(-2.5), 0, S_OK, C_R8(-2.0));
    RND(C_R8(1234.5678), 2, S_OK, C_R8(1234.57));
    RND(C_R8(1234.5678), 0, S_OK, C_R8(1235.0));
    RND(C_R8(-1234.5678), 3, S_OK, C_R8(-1234.568));
    RND(C_R8(1234.5678), 4, S_OK, C_R8(1234.5678));
    RND(C_R8(1234.5678), 10, S_OK, C_R8(1234.5678));
    RND(C_R4(2.5f), 0, S_OK, C_R4(2.0f));
    RND(C_R4(3.5f), 0, S_OK, C_R4(4.0f));
    RND(C_R4(1.2345f), 2, S_OK, C_R4(1.23f));
    RND(C_DATE(36526.75), 0, S_OK, C_DATE(36527.0));
    RND(C_DATE(36526.25), 0, S_OK, C_DATE(36526.0));
    RND(C_DATE(36526.5), 0, S_OK, C_DATE(36526.0));
    /* currency */
    RND(C_CY(54321), 3, S_OK, C_CY(54320));
    RND(C_CY(54321), 4, S_OK, C_CY(54321));
    RND(C_CY(54321), 0, S_OK, C_CY(50000));
    RND(C_CY(55321), 0, S_OK, C_CY(60000));
    RND(C_CY(-55321), 0, S_OK, C_CY(-60000));
    RND(C_CY(55568), 1, S_OK, C_CY(56000));
    /* decimal goes through VarDecRound */
    RND(C_DEC(2, 0, 0, 155), 1, S_OK, C_DEC(1, 0, 0, 16));
    RND(C_DEC(2, 0x80, 0, 155), 1, S_OK, C_DEC(1, 0x80, 0, 16));
    RND(C_DEC(2, 0, 0, 199), 2, S_OK, C_DEC(2, 0, 0, 199));
    RND(C_DEC(0, 0, 0, 77), 5, S_OK, C_DEC(0, 0, 0, 77));
    /* strings are parsed as R8 first */
    RND(C_BSTR(L"7"), 0, S_OK, C_R8(7.0));
    RND(C_BSTR(L"nonumber"), 0, DISP_E_TYPEMISMATCH, C_NONE);
    /* rejected operand types */
    RND(C_I1(1), 0, DISP_E_BADVARTYPE, C_NONE);
    RND(C_UI2(1), 0, DISP_E_BADVARTYPE, C_NONE);
    RND(C_UI4(1), 0, DISP_E_BADVARTYPE, C_NONE);
    RND(C_INT(1), 0, DISP_E_BADVARTYPE, C_NONE);
    RND(C_UINT(1), 0, DISP_E_BADVARTYPE, C_NONE);
    RND(C_ERROR(DISP_E_PARAMNOTFOUND), 0, DISP_E_TYPEMISMATCH, C_NONE);
}
