/* Failure protocol of the binary arithmetic operators: vt-flag handling, DISP_E_* codes, divide by zero and overflow. */
#include "va.h"

static const unsigned XFLAGS[] = { VT_VECTOR, VT_BYREF, VT_ARRAY, VT_RESERVED, VT_ARRAY|VT_BYREF, VT_VECTOR|VT_ARRAY, VT_VECTOR|VT_BYREF,
    VT_VECTOR|VT_RESERVED, VT_BYREF|VT_RESERVED, VT_ARRAY|VT_RESERVED, VT_VECTOR|VT_ARRAY|VT_BYREF|VT_RESERVED };
static const unsigned BASES[] = { VT_EMPTY, VT_NULL, VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BOOL, VT_DECIMAL, VT_UI1, VT_I8 };

/* VarAdd / VarSub: a flagged operand fails with TYPEMISMATCH when VT_ARRAY (0x2000) is set, else BADVARTYPE;
 * the same code is returned whichever side carries the flag (decompiled VarAdd, native VarSub tests) */
static void flag_gate(int op, const char *name, int exact)
{
    size_t f, b;
    for (f = 0; f < sizeof(XFLAGS)/sizeof(XFLAGS[0]); f++)
        for (b = 0; b < sizeof(BASES)/sizeof(BASES[0]); b++) {
            unsigned vt = BASES[b] | XFLAGS[f];
            HRESULT want = (XFLAGS[f] & VT_ARRAY) ? DISP_E_TYPEMISMATCH : DISP_E_BADVARTYPE;
            vcell bad, good = C_I4(1);
            int side;
            memset(&bad, 0, sizeof(bad)); bad.vt = (unsigned short)vt;
            for (side = 0; side < 2; side++) {
                VARIANT L, R, res; HRESULT hr;
                if (side == 0) { va_make(&L, &bad); va_make(&R, &good); } else { va_make(&L, &good); va_make(&R, &bad); }
                memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
                hr = va_bin(op, &L, &R, &res);
                if (exact) CHECKF(hr == want, "%s with vt 0x%x on the %s: hr 0x%lx want 0x%lx", name, vt, side ? "right" : "left", (long)hr, (long)want);
                else CHECKF(hr == DISP_E_TYPEMISMATCH || hr == DISP_E_BADVARTYPE, "%s with vt 0x%x on the %s: hr 0x%lx", name, vt, side ? "right" : "left", (long)hr);
                CHECKF(V_VT(&res) == VT_EMPTY, "%s with vt 0x%x: failed result vt %u want EMPTY", name, vt, V_VT(&res));
            }
        }
}
TEST(variant_arith_ops_err_add_flag_gate) { REQUIRE(U(VarAdd)); flag_gate(VA_ADD, "VarAdd", 1); }
TEST(variant_arith_ops_err_sub_flag_gate) { REQUIRE(U(VarSub)); flag_gate(VA_SUB, "VarSub", 1); }
TEST(variant_arith_ops_err_mul_flag_gate) { REQUIRE(U(VarMul)); flag_gate(VA_MUL, "VarMul", 0); }

TEST(variant_arith_ops_err_unsupported_int_types)
{
    /* VarAdd / VarSub / VarMul do not take I1, UI2, UI4, UI8, INT, UINT even though VarAnd etc do */
    static const unsigned bad[] = { VT_I1, VT_UI2, VT_UI4, VT_UI8, VT_INT, VT_UINT };
    static const int ops[] = { VA_ADD, VA_SUB, VA_MUL };
    size_t k, o;
    REQUIRE(U(VarAdd)); REQUIRE(U(VarSub)); REQUIRE(U(VarMul));
    for (o = 0; o < 3; o++)
        for (k = 0; k < sizeof(bad)/sizeof(bad[0]); k++) {
            vcell b; memset(&b, 0, sizeof(b)); b.vt = (unsigned short)bad[k]; b.i = 1;
            { vcell g = C_I4(2), e = C_NONE; va_check_bin(__LINE__, ops[o], &b, &g, HR_TYPEERR, &e); va_check_bin(__LINE__, ops[o], &g, &b, HR_TYPEERR, &e); }
            { vcell g = C_NULL, e = C_NONE; va_check_bin(__LINE__, ops[o], &b, &g, HR_TYPEERR, &e); va_check_bin(__LINE__, ops[o], &g, &b, HR_TYPEERR, &e); }
        }
    /* UI2 and larger vt words are rejected before anything else with BADVARTYPE (decompiled VarAdd) */
    B(VA_ADD, C_UI2(1), C_I4(2), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_I4(2), C_UI4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_UI8(1), C_NULL, DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_INT(1), C_I4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_UINT(1), C_I4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_RAW(VT_VARIANT), C_I4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_RAW(VT_CLSID), C_I4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_RAW(VT_VOID), C_I4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_RAW(15), C_I4(1), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_I4(1), C_RAW(15), DISP_E_BADVARTYPE, C_NONE);
    B(VA_ADD, C_RAW(VT_RECORD), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_ADD, C_I4(1), C_RAW(VT_RECORD), DISP_E_TYPEMISMATCH, C_NONE);
    /* VT_ERROR is a type mismatch */
    B(VA_ADD, C_ERROR(DISP_E_PARAMNOTFOUND), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_SUB, C_I4(1), C_ERROR(DISP_E_PARAMNOTFOUND), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_SUB, C_NULL, C_ERROR(DISP_E_PARAMNOTFOUND), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_ADD, C_BSTR(L"abc"), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_ADD, C_I4(1), C_BSTR(L"abc"), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_SUB, C_BSTR(L""), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
    B(VA_MUL, C_BSTR(L"1x"), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
}

static void divchk(int line, int op, const char *name, vcell l, vcell r, int allow_overflow)
{
    VARIANT L, R, res; HRESULT hr;
    va_make(&L, &l); va_make(&R, &r);
    memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
    hr = va_bin(op, &L, &R, &res);
    if (allow_overflow == 2) CHECKF(FAILED(hr), "[line %d] %s(%s,%s) hr 0x%lx must fail", line, name, va_vtname(l.vt), va_vtname(r.vt), (long)hr);
    else if (allow_overflow) CHECKF(hr == DISP_E_DIVBYZERO || hr == DISP_E_OVERFLOW, "[line %d] %s(%s,%s) hr 0x%lx", line, name, va_vtname(l.vt), va_vtname(r.vt), (long)hr);
    else CHECKF(hr == DISP_E_DIVBYZERO, "[line %d] %s(%s,%s) hr 0x%lx want DIVBYZERO", line, name, va_vtname(l.vt), va_vtname(r.vt), (long)hr);
    CHECKF(V_VT(&res) == VT_EMPTY, "[line %d] %s failed result vt %u want EMPTY", line, name, V_VT(&res));
    va_free(&L); va_free(&R);
}
#define DZ(op, L, R) do { vcell l_ = L, r_ = R; divchk(__LINE__, op, va_opname(op), l_, r_, 0); } while (0)
#define DZ_ANY(op, L, R) do { vcell l_ = L, r_ = R; divchk(__LINE__, op, va_opname(op), l_, r_, 1); } while (0)
#define DZ_FAIL(op, L, R) do { vcell l_ = L, r_ = R; divchk(__LINE__, op, va_opname(op), l_, r_, 2); } while (0)

TEST(variant_arith_ops_err_divide_by_zero)
{
    REQUIRE(U(VarDiv)); REQUIRE(U(VarIdiv)); REQUIRE(U(VarMod)); REQUIRE(U(SysAllocString));
    /* VarDiv: floating division by zero is DIVBYZERO, 0 / 0 (invalid operation) is OVERFLOW */
    DZ(VA_DIV, C_R8(1.0), C_R8(0.0));
    DZ(VA_DIV, C_R8(-1.0), C_R8(0.0));
    DZ(VA_DIV, C_I4(1), C_I4(0));
    DZ(VA_DIV, C_I2(1), C_I2(0));
    DZ(VA_DIV, C_UI1(1), C_UI1(0));
    DZ(VA_DIV, C_I4(1), C_BOOL(0));
    DZ(VA_DIV, C_I4(1), C_BSTR(L"0"));
    DZ(VA_DIV, C_CY(10000), C_CY(0));
    DZ(VA_DIV, C_DATE(1.0), C_R8(0.0));
    DZ(VA_DIV, C_R4(1.0f), C_R4(0.0f));
    DZ(VA_DIV, C_DEC(0, 0, 0, 1), C_DEC(0, 0, 0, 0));
    B(VA_DIV, C_R8(0.0), C_R8(0.0), DISP_E_OVERFLOW, C_NONE);
    DZ_ANY(VA_DIV, C_I4(0), C_I4(0));
    DZ_FAIL(VA_DIV, C_I4(1), C_EMPTY);
    DZ_FAIL(VA_DIV, C_EMPTY, C_EMPTY);
    B(VA_DIV, C_NULL, C_EMPTY, S_OK, C_NULL);
    B(VA_DIV, C_NULL, C_I4(0), S_OK, C_NULL);
    B(VA_DIV, C_I4(0), C_NULL, S_OK, C_NULL);
    /* VarIdiv / VarMod: a zero divisor after rounding to integer is DIVBYZERO */
    DZ(VA_IDIV, C_I4(1), C_I4(0));
    DZ(VA_IDIV, C_I2(1), C_I2(0));
    DZ(VA_IDIV, C_UI1(1), C_UI1(0));
    DZ(VA_IDIV, C_I8(1), C_I8(0));
    DZ(VA_IDIV, C_I4(1), C_EMPTY);
    DZ(VA_IDIV, C_EMPTY, C_EMPTY);
    DZ(VA_IDIV, C_I4(1), C_BOOL(0));
    DZ(VA_IDIV, C_I4(1), C_R8(0.4));
    DZ(VA_IDIV, C_I4(1), C_R8(0.5));
    DZ(VA_IDIV, C_I4(1), C_BSTR(L"0"));
    DZ(VA_MOD, C_I4(1), C_I4(0));
    DZ(VA_MOD, C_I2(1), C_I2(0));
    DZ(VA_MOD, C_UI1(1), C_UI1(0));
    DZ(VA_MOD, C_I8(1), C_I8(0));
    DZ(VA_MOD, C_I4(1), C_EMPTY);
    DZ(VA_MOD, C_EMPTY, C_EMPTY);
    DZ(VA_MOD, C_I4(1), C_R8(0.4));
    B(VA_IDIV, C_NULL, C_I4(0), S_OK, C_NULL);
    B(VA_MOD, C_NULL, C_I4(0), S_OK, C_NULL);
}

TEST(variant_arith_ops_err_overflow)
{
    REQUIRE(U(VarDiv)); REQUIRE(U(VarMul)); REQUIRE(U(VarAdd)); REQUIRE(U(VarSub)); REQUIRE(U(VarIdiv)); REQUIRE(U(VarMod)); REQUIRE(U(VarPow));
    /* R8 results beyond DBL_MAX / inf are reported as overflow */
    B(VA_DIV, C_R8(R8_MAX), C_R8(0.5), DISP_E_OVERFLOW, C_NONE);
    B(VA_DIV, C_R8(-R8_MAX), C_R8(0.5), DISP_E_OVERFLOW, C_NONE);
    B(VA_MUL, C_R8(R8_MAX), C_R8(2.0), DISP_E_OVERFLOW, C_NONE);
    B(VA_MUL, C_R8(-R8_MAX), C_R8(2.0), DISP_E_OVERFLOW, C_NONE);
    B(VA_ADD, C_R8(R8_MAX), C_R8(R8_MAX), DISP_E_OVERFLOW, C_NONE);
    B(VA_SUB, C_R8(-R8_MAX), C_R8(R8_MAX), DISP_E_OVERFLOW, C_NONE);
    /* integer operands outside the 32-bit range of IDIV / MOD results */
    B(VA_IDIV, C_R8(3e9), C_I4(1), DISP_E_OVERFLOW, C_NONE);
    B(VA_IDIV, C_R8(-3e9), C_I4(1), DISP_E_OVERFLOW, C_NONE);
    B(VA_MOD, C_R8(3e9), C_I4(7), DISP_E_OVERFLOW, C_NONE);
    B(VA_MOD, C_I4(7), C_R8(-3e9), DISP_E_OVERFLOW, C_NONE);
    B(VA_AND, C_R8(3e9), C_I4(1), DISP_E_OVERFLOW, C_NONE);
    B(VA_OR, C_R8(-3e9), C_I4(1), DISP_E_OVERFLOW, C_NONE);
    B(VA_XOR, C_R8(1e20), C_I4(1), DISP_E_OVERFLOW, C_NONE);
}
