/* VarAnd / VarOr / VarXor / VarEqv / VarImp: Boolean truth tables, NULL propagation, bitwise semantics
 * and result-type promotion over the accepted operand types. */
#include "va.h"

static const int LOGIC[5] = { VA_AND, VA_OR, VA_XOR, VA_EQV, VA_IMP };
static const char *LNAME[5] = { "VarAnd", "VarOr", "VarXor", "VarEqv", "VarImp" };

/* three-valued Boolean tables, index 0 = FALSE, 1 = TRUE, 2 = NULL; value 3 = NULL result */
static const int T_AND[3][3] = { {0,0,0}, {0,1,3}, {0,3,3} };
static const int T_OR [3][3] = { {0,1,3}, {1,1,1}, {3,1,3} };
static const int T_XOR[3][3] = { {0,1,3}, {1,0,3}, {3,3,3} };
static const int T_EQV[3][3] = { {1,0,3}, {0,1,3}, {3,3,3} };
static const int T_IMP[3][3] = { {1,1,1}, {0,1,3}, {3,1,3} };
static const int (*TABLES[5])[3] = { T_AND, T_OR, T_XOR, T_EQV, T_IMP };

static vcell bcell(int k) { vcell c; memset(&c, 0, sizeof(c)); if (k == 2) c.vt = VT_NULL; else { c.vt = VT_BOOL; c.i = k ? -1 : 0; } return c; }

TEST(variant_arith_ops_logic_bool_truth_tables)
{
    int op, a, b;
    REQUIRE(U(VarAnd)); REQUIRE(U(VarOr)); REQUIRE(U(VarXor)); REQUIRE(U(VarEqv)); REQUIRE(U(VarImp));
    for (op = 0; op < 5; op++)
        for (a = 0; a < 3; a++)
            for (b = 0; b < 3; b++) {
                vcell l = bcell(a), r = bcell(b), e;
                int t = TABLES[op][a][b];
                memset(&e, 0, sizeof(e));
                if (t == 3) e.vt = VT_NULL; else { e.vt = VT_BOOL; e.i = t ? -1 : 0; }
                (void)LNAME;
                va_check_bin(a * 3 + b, LOGIC[op], &l, &r, S_OK, &e);
            }
}

TEST(variant_arith_ops_logic_bitwise_int)
{
    REQUIRE(U(VarAnd)); REQUIRE(U(VarOr)); REQUIRE(U(VarXor)); REQUIRE(U(VarEqv)); REQUIRE(U(VarImp));
    BC(VA_AND, C_I4(12), C_I4(10), S_OK, C_I4(8));
    BC(VA_OR,  C_I4(12), C_I4(10), S_OK, C_I4(14));
    BC(VA_XOR, C_I4(12), C_I4(10), S_OK, C_I4(6));
    BC(VA_EQV, C_I4(12), C_I4(10), S_OK, C_I4(-7));
    B(VA_IMP,  C_I4(12), C_I4(10), S_OK, C_I4(-5));
    B(VA_IMP,  C_I4(10), C_I4(12), S_OK, C_I4(-3));
    BC(VA_AND, C_I2(-1), C_I2(0x1234), S_OK, C_I2(0x1234));
    BC(VA_OR,  C_I2(0x00ff), C_I2(0x7f00), S_OK, C_I2(0x7fff));
    BC(VA_XOR, C_I2(0x7fff), C_I2(-1), S_OK, C_I2(-32768));
    BC(VA_EQV, C_I2(0x7fff), C_I2(0x7fff), S_OK, C_I2(-1));
    BC(VA_AND, C_UI1(0xf0), C_UI1(0x3c), S_OK, C_UI1(0x30));
    BC(VA_OR,  C_UI1(0xf0), C_UI1(0x3c), S_OK, C_UI1(0xfc));
    BC(VA_XOR, C_UI1(0xf0), C_UI1(0x3c), S_OK, C_UI1(0xcc));
    BC(VA_EQV, C_UI1(0xf0), C_UI1(0x3c), S_OK, C_UI1(0x33));
    B(VA_IMP,  C_UI1(0xf0), C_UI1(0x3c), S_OK, C_UI1(0x3f));
    BC(VA_AND, C_I8(0x0123456789abcdefLL), C_I8(0x00ff00ff00ff00ffLL), S_OK, C_I8(0x0023006700ab00efLL));
    BC(VA_OR,  C_I8(0x0123456789abcdefLL), C_I8(0x00ff00ff00ff00ffLL), S_OK, C_I8(0x01ff45ff89ffcdffLL));
    BC(VA_XOR, C_I8(0x0123456789abcdefLL), C_I8(0x00ff00ff00ff00ffLL), S_OK, C_I8(0x01dc45988954cd10LL));
    BC(VA_AND, C_I4(-1), C_I8(0x7fffffff00000001LL), S_OK, C_I8(0x7fffffff00000001LL));
    BC(VA_AND, C_UI4(0xf0f0f0f0u), C_UI4(0xffff0000u), S_OK, C_I4(0xf0f00000u));
    BC(VA_OR,  C_UI2(0x8000), C_UI2(0x0001), S_OK, C_I4(0x8001));
    BC(VA_AND, C_I1(-1), C_I1(0x55), S_OK, C_I4(0x55));
    BC(VA_AND, C_INT(0xff), C_INT(0x0f), S_OK, C_I4(0x0f));
    BC(VA_AND, C_UINT(0xff), C_UINT(0x0f), S_OK, C_I4(0x0f));
    BC(VA_AND, C_UI8(0xff), C_UI8(0x0f), S_OK, C_I4(0x0f));
    BC(VA_AND, C_I8(0xff), C_UI8(0x0f), S_OK, C_I8(0x0f));
}

TEST(variant_arith_ops_logic_coercions)
{
    REQUIRE(U(VarAnd)); REQUIRE(U(VarOr)); REQUIRE(U(VarXor)); REQUIRE(U(VarEqv)); REQUIRE(U(SysAllocString));
    /* floating point and currency operands are rounded (to even) to I4 */
    BC(VA_AND, C_R8(7.5), C_I4(-1), S_OK, C_I4(8));
    BC(VA_AND, C_R8(6.5), C_I4(-1), S_OK, C_I4(6));
    BC(VA_AND, C_R8(-0.5), C_I4(-1), S_OK, C_I4(0));
    BC(VA_AND, C_R4(2.5f), C_I4(-1), S_OK, C_I4(2));
    BC(VA_AND, C_R4(3.5f), C_I4(-1), S_OK, C_I4(4));
    BC(VA_OR,  C_DATE(5.0), C_I4(2), S_OK, C_I4(7));
    BC(VA_OR,  C_CY(30000), C_I4(4), S_OK, C_I4(7));
    BC(VA_OR,  C_DEC(0, 0, 0, 9), C_I4(6), S_OK, C_I4(15));
    /* numeric strings are parsed as numbers, boolean strings as BOOL */
    BC(VA_AND, C_BSTR(L"12"), C_I4(10), S_OK, C_I4(8));
    /* Win10: a numeric string widens an I2 partner to I4 */
    BC(VA_AND, C_BSTR(L"12"), C_I2(10), S_OK, C_I4(8));
    BC(VA_OR,  C_BSTR(L"#TRUE#"), C_BOOL(0), S_OK, C_BOOL(-1));
    BC(VA_AND, C_BSTR(L"#TRUE#"), C_BOOL(-1), S_OK, C_BOOL(-1));
    BC(VA_AND, C_BSTR(L"#FALSE#"), C_BOOL(-1), S_OK, C_BOOL(0));
    BC(VA_XOR, C_BSTR(L"#TRUE#"), C_BSTR(L"#FALSE#"), S_OK, C_BOOL(-1));
    BC(VA_XOR, C_BSTR(L"#TRUE#"), C_BSTR(L"#TRUE#"), S_OK, C_BOOL(0));
    BC(VA_EQV, C_BSTR(L"#FALSE#"), C_BSTR(L"#FALSE#"), S_OK, C_BOOL(-1));
    BC(VA_AND, C_BSTR(L"notanumber"), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
    BC(VA_OR,  C_BSTR(L""), C_I4(1), DISP_E_TYPEMISMATCH, C_NONE);
    BC(VA_XOR, C_BSTR(L"abc"), C_BOOL(-1), DISP_E_TYPEMISMATCH, C_NONE);
    /* EMPTY is zero, typed I2 */
    BC(VA_AND, C_EMPTY, C_EMPTY, S_OK, C_I2(0));
    BC(VA_OR,  C_EMPTY, C_EMPTY, S_OK, C_I2(0));
    BC(VA_OR,  C_EMPTY, C_I2(5), S_OK, C_I2(5));
    BC(VA_XOR, C_EMPTY, C_I4(5), S_OK, C_I4(5));
    BC(VA_XOR, C_EMPTY, C_UI1(5), S_OK, C_I2(5));
    BC(VA_EQV, C_EMPTY, C_EMPTY, S_OK, C_I2(-1));
    /* BOOL combined with a number is bitwise on the number's width (BOOL TRUE is all ones) */
    BC(VA_AND, C_BOOL(-1), C_I2(0x1234), S_OK, C_I2(0x1234));
    BC(VA_AND, C_BOOL(-1), C_I4(0x12345678), S_OK, C_I4(0x12345678));
    BC(VA_AND, C_BOOL(0), C_I4(0x12345678), S_OK, C_I4(0));
    BC(VA_OR,  C_BOOL(-1), C_I4(0x12345678), S_OK, C_I4(-1));
    BC(VA_AND, C_BOOL(-1), C_UI1(0x55), S_OK, C_I2(0x55));
    /* NULL combined with numbers */
    BC(VA_AND, C_NULL, C_I4(0), S_OK, C_I4(0));
    BC(VA_AND, C_NULL, C_I4(5), S_OK, C_NULL);
    BC(VA_OR,  C_NULL, C_I4(0), S_OK, C_NULL);
    BC(VA_OR,  C_NULL, C_I4(5), S_OK, C_I4(5));
    BC(VA_XOR, C_NULL, C_I4(5), S_OK, C_NULL);
    BC(VA_EQV, C_NULL, C_I4(5), S_OK, C_NULL);
    BC(VA_EQV, C_NULL, C_NULL, S_OK, C_NULL);
    BC(VA_XOR, C_NULL, C_NULL, S_OK, C_NULL);
}

/* result vt for non-NULL, non-string operands of the logical operators */
static unsigned logic_vt(unsigned l, unsigned r, int *fail)
{
    #define H(a) (l == (a) || r == (a))
    *fail = 0;
    if ((l == VT_I8 && r == VT_INT) || (l == VT_INT && r == VT_I8)) { *fail = 1; return 0; }
    if (H(VT_I8)) return VT_I8;
    if (H(VT_I4) || H(VT_UINT) || H(VT_INT) || H(VT_R4) || H(VT_R8) || H(VT_CY) || H(VT_DATE) || H(VT_I1) || H(VT_UI2) || H(VT_UI4) || H(VT_UI8) || H(VT_DECIMAL)) return VT_I4;
    if (H(VT_UI1) || H(VT_I2) || H(VT_EMPTY)) return (l == VT_UI1 && r == VT_UI1) ? VT_UI1 : VT_I2;
    if (H(VT_BOOL)) return VT_BOOL;
    *fail = 1; return 0;
    #undef H
}

TEST(variant_arith_ops_logic_vt_matrix)
{
    static const unsigned vts[] = { VT_EMPTY, VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BOOL, VT_DECIMAL, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT };
    size_t a, b; int op;
    REQUIRE(U(VarAnd)); REQUIRE(U(VarOr)); REQUIRE(U(VarXor)); REQUIRE(U(VarEqv)); REQUIRE(U(VarImp));
    for (op = 0; op < 5; op++)
        for (a = 0; a < sizeof(vts)/sizeof(vts[0]); a++)
            for (b = 0; b < sizeof(vts)/sizeof(vts[0]); b++) {
                unsigned l = vts[a], r = vts[b]; int fail; unsigned want;
                VARIANT L, R, res; HRESULT hr; vcell lc, rc;
                want = logic_vt(l, r, &fail);
                /* Win10: VarImp(INT, I8) succeeds with an I8 result (the other And/Or/Imp INT/I8 orders fail) */
                if (LOGIC[op] == VA_IMP && l == VT_INT && r == VT_I8) { fail = 0; want = VT_I8; }
                if (((l == VT_I8 && r == VT_INT) || (l == VT_INT && r == VT_I8)) && (LOGIC[op] == VA_XOR || LOGIC[op] == VA_EQV)) continue;
                memset(&lc, 0, sizeof(lc)); memset(&rc, 0, sizeof(rc));
                lc.vt = (unsigned short)l; rc.vt = (unsigned short)r;
                /* operand values: l = 12, r = 10 (BOOL TRUE, EMPTY zero) */
                lc.i = (l == VT_BOOL) ? -1 : 12; rc.i = (r == VT_BOOL) ? -1 : 10;
                if (l == VT_R4 || l == VT_R8 || l == VT_DATE) lc.d = 12; if (r == VT_R4 || r == VT_R8 || r == VT_DATE) rc.d = 10;
                if (l == VT_CY) lc.i = 120000; if (r == VT_CY) rc.i = 100000;
                if (l == VT_EMPTY) lc.i = 0; if (r == VT_EMPTY) rc.i = 0;
                if (l == VT_DECIMAL) lc.d = 0; if (r == VT_DECIMAL) rc.d = 0;
                va_make(&L, &lc); va_make(&R, &rc);
                memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
                hr = va_bin(LOGIC[op], &L, &R, &res);
                if (fail) {
                    CHECKF(FAILED(hr), "%s(%s,%s) unexpectedly succeeded", LNAME[op], va_vtname(l), va_vtname(r));
                    CHECKF(VA_FAILRES_OK(V_VT(&res)), "%s(%s,%s) failed result vt %u want EMPTY", LNAME[op], va_vtname(l), va_vtname(r), V_VT(&res));
                } else {
                    long long x = (l == VT_BOOL) ? -1 : (l == VT_EMPTY ? 0 : 12), y = (r == VT_BOOL) ? -1 : (r == VT_EMPTY ? 0 : 10), z = 0, got = 0;
                    CHECKF(hr == S_OK, "%s(%s,%s) hr 0x%lx", LNAME[op], va_vtname(l), va_vtname(r), (long)hr);
                    if (hr == S_OK) {
                        CHECKF(V_VT(&res) == want, "%s(%s,%s) result vt %s want %s", LNAME[op], va_vtname(l), va_vtname(r), va_vtname(V_VT(&res)), va_vtname(want));
                        switch (op) { case 0: z = x & y; break; case 1: z = x | y; break; case 2: z = x ^ y; break; case 3: z = ~(x ^ y); break; case 4: z = ~x | y; break; }
                        switch (V_VT(&res)) {
                        case VT_UI1: got = V_UI1(&res); z &= 0xff; break;
                        case VT_I2: case VT_BOOL: got = V_I2(&res); z = (short)z; break;
                        case VT_I4: got = V_I4(&res); z = (int)z; break;
                        case VT_I8: got = V_I8(&res); break;
                        }
                        CHECKF(got == z, "%s(%s,%s) value %lld want %lld", LNAME[op], va_vtname(l), va_vtname(r), got, z);
                    }
                }
                va_free(&L); va_free(&R);
            }
}

TEST(variant_arith_ops_logic_type_errors)
{
    static const int ops[] = { VA_AND, VA_OR, VA_XOR, VA_EQV, VA_IMP };
    size_t k;
    REQUIRE(U(VarAnd)); REQUIRE(U(VarOr)); REQUIRE(U(VarXor)); REQUIRE(U(VarEqv)); REQUIRE(U(VarImp));
    for (k = 0; k < 5; k++) {
        B(ops[k], C_ERROR(DISP_E_PARAMNOTFOUND), C_I4(1), HR_TYPEERR, C_NONE);
        B(ops[k], C_I4(1), C_ERROR(DISP_E_PARAMNOTFOUND), HR_TYPEERR, C_NONE);
        B(ops[k], C_RAW(VT_CLSID), C_I4(1), HR_TYPEERR, C_NONE);
        B(ops[k], C_I4(1), C_RAW(VT_CLSID), HR_TYPEERR, C_NONE);
        B(ops[k], C_RAW(15), C_I4(1), HR_TYPEERR, C_NONE);
        B(ops[k], C_RAW(VT_VARIANT), C_I4(1), HR_TYPEERR, C_NONE);
        B(ops[k], C_RAWI(VT_I4 | VT_BYREF, 0), C_I4(1), HR_TYPEERR, C_NONE);
        B(ops[k], C_I4(1), C_RAWI(VT_I4 | VT_ARRAY, 0), HR_TYPEERR, C_NONE);
        B(ops[k], C_RAWI(VT_I4 | VT_VECTOR, 0), C_I4(1), HR_TYPEERR, C_NONE);
        B(ops[k], C_RAWI(VT_I4 | VT_RESERVED, 0), C_I4(1), HR_TYPEERR, C_NONE);
        /* I8 with INT is not a supported combination of And / Or / Imp */
        if (ops[k] == VA_AND || ops[k] == VA_OR || ops[k] == VA_IMP) {
            B(ops[k], C_I8(1), C_INT(1), HR_TYPEERR, C_NONE);
            if (ops[k] == VA_IMP) B(ops[k], C_INT(1), C_I8(1), S_OK, C_I8(-1)); /* Win10: succeeds */
            else B(ops[k], C_INT(1), C_I8(1), HR_TYPEERR, C_NONE);
        }
    }
}
