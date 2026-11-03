/* Hand-written arithmetic value / promotion cases beyond the Wine-derived tables. Expectations come from the
 * decompiled oleaut32 (VarIdiv / VarMod integer paths) or follow the documented VBA semantics. */
#include "va.h"

TEST(variant_arith_ops_idiv_decomp_rules)
{
    REQUIRE(U(VarIdiv)); REQUIRE(U(SysAllocString));
    /* INT_MIN \ -1 and INT64_MIN \ -1 overflow explicitly */
    if (!t_is_wine()) { /* Wine raises an integer divide exception here */
        B(VA_IDIV, C_I4(I4_MIN), C_I4(-1), DISP_E_OVERFLOW, C_NONE);
        B(VA_IDIV, C_I8(I8_MIN), C_I8(-1), DISP_E_OVERFLOW, C_NONE);
        B(VA_IDIV, C_I8(I8_MIN), C_I4(-1), DISP_E_OVERFLOW, C_NONE);
    }
    B(VA_IDIV, C_I4(I4_MIN), C_I4(1), S_OK, C_I4(I4_MIN));
    B(VA_IDIV, C_I4(I4_MIN + 1), C_I4(-1), S_OK, C_I4(I4_MAX));
    B(VA_IDIV, C_I8(I8_MIN), C_I8(1), S_OK, C_I8(I8_MIN));
    B(VA_IDIV, C_I8(I8_MAX), C_I8(-1), S_OK, C_I8(-I8_MAX));
    /* I2 results widen to I4 once the quotient no longer fits */
    B(VA_IDIV, C_I2(-32768), C_I2(-1), S_OK, C_I4(32768));
    B(VA_IDIV, C_I2(-32768), C_I2(1), S_OK, C_I2(-32768));
    B(VA_IDIV, C_I2(32767), C_I2(1), S_OK, C_I2(32767));
    B(VA_IDIV, C_I2(32767), C_I2(-1), S_OK, C_I2(-32767));
    B(VA_IDIV, C_I2(7), C_I2(2), S_OK, C_I2(3));
    B(VA_IDIV, C_I2(-7), C_I2(2), S_OK, C_I2(-3));
    B(VA_IDIV, C_I2(7), C_I2(-2), S_OK, C_I2(-3));
    B(VA_IDIV, C_I2(-7), C_I2(-2), S_OK, C_I2(3));
    B(VA_IDIV, C_I2(1), C_I2(2), S_OK, C_I2(0));
    B(VA_IDIV, C_I2(-1), C_I2(2), S_OK, C_I2(0));
    B(VA_IDIV, C_BOOL(-1), C_I2(1), S_OK, C_I2(-1));
    B(VA_IDIV, C_BOOL(-1), C_BOOL(-1), S_OK, C_I2(1));
    B(VA_IDIV, C_EMPTY, C_I2(5), S_OK, C_I2(0));
    B(VA_IDIV, C_EMPTY, C_I4(5), S_OK, C_I4(0));
    /* unsigned bytes divide unsigned */
    B(VA_IDIV, C_UI1(200), C_UI1(3), S_OK, C_UI1(66));
    B(VA_IDIV, C_UI1(255), C_UI1(255), S_OK, C_UI1(1));
    B(VA_IDIV, C_UI1(255), C_UI1(2), S_OK, C_UI1(127));
    B(VA_IDIV, C_UI1(0), C_UI1(9), S_OK, C_UI1(0));
    B(VA_IDIV, C_UI1(200), C_I2(3), S_OK, C_I2(66));
    B(VA_IDIV, C_UI1(200), C_I4(3), S_OK, C_I4(66));
    /* I4 / I8 */
    B(VA_IDIV, C_I4(7), C_I4(2), S_OK, C_I4(3));
    B(VA_IDIV, C_I4(-7), C_I4(2), S_OK, C_I4(-3));
    B(VA_IDIV, C_I4(7), C_I4(-2), S_OK, C_I4(-3));
    B(VA_IDIV, C_I4(-7), C_I4(-2), S_OK, C_I4(3));
    B(VA_IDIV, C_I4(I4_MAX), C_I4(2), S_OK, C_I4(1073741823));
    B(VA_IDIV, C_I8(7), C_I8(2), S_OK, C_I8(3));
    B(VA_IDIV, C_I8(-7), C_I8(2), S_OK, C_I8(-3));
    B(VA_IDIV, C_I8(0x7fffffff00000000LL), C_I8(0x100000000LL), S_OK, C_I8(0x7fffffffLL));
    B(VA_IDIV, C_I8(100), C_I4(7), S_OK, C_I8(14));
    B(VA_IDIV, C_I4(100), C_I8(7), S_OK, C_I8(14));
    /* operands are rounded to even before dividing */
    B(VA_IDIV, C_R8(7.5), C_I4(2), S_OK, C_I4(4));
    B(VA_IDIV, C_R8(6.5), C_I4(2), S_OK, C_I4(3));
    B(VA_IDIV, C_R8(5.5), C_I4(2), S_OK, C_I4(3));
    B(VA_IDIV, C_R8(4.5), C_I4(2), S_OK, C_I4(2));
    B(VA_IDIV, C_R8(7.4), C_R8(2.6), S_OK, C_I4(2));
    B(VA_IDIV, C_R8(2.5), C_I4(1), S_OK, C_I4(2));
    B(VA_IDIV, C_R8(3.5), C_I4(1), S_OK, C_I4(4));
    B(VA_IDIV, C_R8(-2.5), C_I4(1), S_OK, C_I4(-2));
    B(VA_IDIV, C_R8(-3.5), C_I4(1), S_OK, C_I4(-4));
    B(VA_IDIV, C_R4(9.0f), C_R4(2.0f), S_OK, C_I4(4));
    B(VA_IDIV, C_DATE(9.0), C_I4(2), S_OK, C_I4(4));
    B(VA_IDIV, C_CY(90000), C_I4(2), S_OK, C_I4(4));
    B(VA_IDIV, C_BSTR(L"9"), C_BSTR(L"2"), S_OK, C_I4(4));
    B(VA_IDIV, C_DEC(0, 0, 0, 9), C_I4(2), S_OK, C_I4(4));
    B(VA_IDIV, C_NULL, C_I4(2), S_OK, C_NULL);
    B(VA_IDIV, C_I4(2), C_NULL, S_OK, C_NULL);
    B(VA_IDIV, C_NULL, C_NULL, S_OK, C_NULL);
    B(VA_IDIV, C_NULL, C_EMPTY, S_OK, C_NULL);
}

TEST(variant_arith_ops_mod_decomp_rules)
{
    REQUIRE(U(VarMod)); REQUIRE(U(SysAllocString));
    /* the result has the sign of the dividend; x Mod -1 is 0 without trapping */
    B(VA_MOD, C_I4(7), C_I4(3), S_OK, C_I4(1));
    B(VA_MOD, C_I4(-7), C_I4(3), S_OK, C_I4(-1));
    B(VA_MOD, C_I4(7), C_I4(-3), S_OK, C_I4(1));
    B(VA_MOD, C_I4(-7), C_I4(-3), S_OK, C_I4(-1));
    B(VA_MOD, C_I4(6), C_I4(3), S_OK, C_I4(0));
    B(VA_MOD, C_I4(0), C_I4(3), S_OK, C_I4(0));
    B(VA_MOD, C_I4(2), C_I4(7), S_OK, C_I4(2));
    B(VA_MOD, C_I4(-2), C_I4(7), S_OK, C_I4(-2));
    if (!t_is_wine()) B(VA_MOD, C_I4(I4_MIN), C_I4(-1), S_OK, C_I4(0));
    B(VA_MOD, C_I4(I4_MAX), C_I4(-1), S_OK, C_I4(0));
    B(VA_MOD, C_I4(I4_MIN), C_I4(I4_MAX), S_OK, C_I4(-1));
    if (!t_is_wine()) B(VA_MOD, C_I8(I8_MIN), C_I8(-1), S_OK, C_I8(0));
    B(VA_MOD, C_I8(I8_MIN), C_I8(I8_MAX), S_OK, C_I8(-1));
    B(VA_MOD, C_I8(10000000000LL), C_I8(7), S_OK, C_I8(4));
    B(VA_MOD, C_I8(-10000000000LL), C_I8(7), S_OK, C_I8(-4));
    B(VA_MOD, C_I8(10), C_I4(4), S_OK, C_I8(2));
    B(VA_MOD, C_I2(-32768), C_I2(-1), S_OK, C_I2(0));
    B(VA_MOD, C_I2(-32768), C_I2(7), S_OK, C_I2(-1));
    B(VA_MOD, C_I2(32767), C_I2(-32768), S_OK, C_I2(32767));
    B(VA_MOD, C_I2(13), C_I2(5), S_OK, C_I2(3));
    B(VA_MOD, C_BOOL(-1), C_I2(5), S_OK, C_I2(-1));
    B(VA_MOD, C_UI1(200), C_UI1(7), S_OK, C_UI1(4));
    B(VA_MOD, C_UI1(255), C_UI1(16), S_OK, C_UI1(15));
    B(VA_MOD, C_UI1(5), C_UI1(200), S_OK, C_UI1(5));
    B(VA_MOD, C_EMPTY, C_I4(5), S_OK, C_I4(0));
    /* operands are rounded to even first */
    B(VA_MOD, C_R8(7.5), C_I4(2), S_OK, C_I4(0));
    B(VA_MOD, C_R8(6.5), C_I4(4), S_OK, C_I4(2));
    B(VA_MOD, C_R8(10.4), C_R8(3.4), S_OK, C_I4(1));
    B(VA_MOD, C_R8(10.6), C_R8(3.4), S_OK, C_I4(2));
    B(VA_MOD, C_R8(-10.6), C_R8(3.4), S_OK, C_I4(-2));
    B(VA_MOD, C_DATE(10.0), C_I4(4), S_OK, C_I4(2));
    B(VA_MOD, C_CY(100000), C_I4(4), S_OK, C_I4(2));
    B(VA_MOD, C_BSTR(L"10"), C_BSTR(L"4"), S_OK, C_I4(2));
    B(VA_MOD, C_NULL, C_I4(4), S_OK, C_NULL);
    B(VA_MOD, C_I4(4), C_NULL, S_OK, C_NULL);
    B(VA_MOD, C_NULL, C_NULL, S_OK, C_NULL);
}

TEST(variant_arith_ops_add_promotion)
{
    REQUIRE(U(VarAdd)); REQUIRE(U(SysAllocString));
    BC(VA_ADD, C_I2(32767), C_I2(1), S_OK, C_I4(32768));
    BC(VA_ADD, C_I2(-32768), C_I2(-1), S_OK, C_I4(-32769));
    BC(VA_ADD, C_I2(100), C_I2(200), S_OK, C_I2(300));
    BC(VA_ADD, C_UI1(255), C_UI1(1), S_OK, C_I2(256));
    BC(VA_ADD, C_UI1(200), C_I2(100), S_OK, C_I2(300));
    BC(VA_ADD, C_UI1(1), C_UI1(2), S_OK, C_UI1(3));
    BC(VA_ADD, C_I4(I4_MAX), C_I4(1), S_OK, C_R8(2147483648.0));
    BC(VA_ADD, C_I4(I4_MIN), C_I4(-1), S_OK, C_R8(-2147483649.0));
    BC(VA_ADD, C_I4(I4_MAX), C_I4(-1), S_OK, C_I4(I4_MAX - 1));
    BC(VA_ADD, C_I4(1000000), C_I2(5), S_OK, C_I4(1000005));
    BC(VA_ADD, C_I4(1), C_R4(0.5f), S_OK, C_R8(1.5));
    BC(VA_ADD, C_I2(1), C_R4(0.5f), S_OK, C_R4(1.5f));
    BC(VA_ADD, C_UI1(1), C_R4(0.5f), S_OK, C_R4(1.5f));
    BC(VA_ADD, C_R4(1.25f), C_R4(0.5f), S_OK, C_R4(1.75f));
    BC(VA_ADD, C_R4(1.25f), C_R8(0.5), S_OK, C_R8(1.75));
    BC(VA_ADD, C_I8(5), C_I8(6), S_OK, C_I8(11));
    BC(VA_ADD, C_I8(5), C_I4(6), S_OK, C_I8(11));
    BC(VA_ADD, C_I8(5), C_UI1(6), S_OK, C_I8(11));
    BC(VA_ADD, C_I8(5), C_R8(0.5), S_OK, C_R8(5.5));
    BC(VA_ADD, C_I8(5), C_R4(0.5f), S_OK, C_R8(5.5));
    BC(VA_ADD, C_DATE(2.5), C_I4(1), S_OK, C_DATE(3.5));
    BC(VA_ADD, C_DATE(2.5), C_DATE(1.25), S_OK, C_DATE(3.75));
    BC(VA_ADD, C_DATE(2.5), C_R8(1.25), S_OK, C_DATE(3.75));
    BC(VA_ADD, C_DATE(2.5), C_CY(10000), S_OK, C_DATE(3.5));
    BC(VA_ADD, C_CY(10000), C_CY(25000), S_OK, C_CY(35000));
    BC(VA_ADD, C_CY(10000), C_I4(2), S_OK, C_CY(30000));
    BC(VA_ADD, C_CY(10000), C_R8(0.5), S_OK, C_CY(15000));
    BC(VA_ADD, C_CY(10000), C_UI1(2), S_OK, C_CY(30000));
    BC(VA_ADD, C_BOOL(-1), C_BOOL(-1), S_OK, C_I2(-2));
    BC(VA_ADD, C_BOOL(0), C_BOOL(0), S_OK, C_I2(0));
    BC(VA_ADD, C_BOOL(-1), C_I4(5), S_OK, C_I4(4));
    BC(VA_ADD, C_EMPTY, C_EMPTY, S_OK, C_I2(0));
    BC(VA_ADD, C_EMPTY, C_UI1(9), S_OK, C_UI1(9));
    BC(VA_ADD, C_EMPTY, C_I2(9), S_OK, C_I2(9));
    BC(VA_ADD, C_EMPTY, C_I4(9), S_OK, C_I4(9));
    BC(VA_ADD, C_EMPTY, C_R4(9.5f), S_OK, C_R4(9.5f));
    BC(VA_ADD, C_EMPTY, C_R8(9.5), S_OK, C_R8(9.5));
    BC(VA_ADD, C_NULL, C_I4(9), S_OK, C_NULL);
    BC(VA_ADD, C_NULL, C_BSTR(L"x"), S_OK, C_NULL);
    BC(VA_ADD, C_NULL, C_EMPTY, S_OK, C_NULL);
    BC(VA_ADD, C_NULL, C_NULL, S_OK, C_NULL);
    /* strings: Add concatenates two strings (or a string and EMPTY), otherwise parses the string as R8 */
    B(VA_ADD, C_BSTR(L"12"), C_BSTR(L"34"), S_OK, C_BSTR(L"1234"));
    B(VA_ADD, C_BSTR(L"a"), C_BSTR(L"b"), S_OK, C_BSTR(L"ab"));
    B(VA_ADD, C_BSTR(L""), C_BSTR(L""), S_OK, C_BSTR(L""));
    B(VA_ADD, C_BSTR(L"a"), C_EMPTY, S_OK, C_BSTR(L"a"));
    B(VA_ADD, C_EMPTY, C_BSTR(L"a"), S_OK, C_BSTR(L"a"));
    B(VA_ADD, C_BSTR(L"12"), C_I4(34), S_OK, C_R8(46.0));
    B(VA_ADD, C_I4(34), C_BSTR(L"12"), S_OK, C_R8(46.0));
    B(VA_ADD, C_BSTR(L"12"), C_BOOL(-1), S_OK, C_R8(11.0));
    B(VA_ADD, C_BSTR(L"-1"), C_UI1(1), S_OK, C_R8(0.0));
    B(VA_ADD, C_BSTR(L"12"), C_DATE(1.0), S_OK, C_DATE(13.0));
}

TEST(variant_arith_ops_sub_mul_div_promotion)
{
    REQUIRE(U(VarSub)); REQUIRE(U(VarMul)); REQUIRE(U(VarDiv)); REQUIRE(U(VarPow)); REQUIRE(U(SysAllocString));
    B(VA_SUB, C_I2(5), C_I2(7), S_OK, C_I2(-2));
    B(VA_SUB, C_I4(5), C_I4(7), S_OK, C_I4(-2));
    B(VA_SUB, C_UI1(9), C_UI1(4), S_OK, C_UI1(5));
    B(VA_SUB, C_UI1(9), C_I2(10), S_OK, C_I2(-1));
    B(VA_SUB, C_I8(5), C_I8(7), S_OK, C_I8(-2));
    B(VA_SUB, C_R4(1.5f), C_R4(0.25f), S_OK, C_R4(1.25f));
    B(VA_SUB, C_DATE(5.5), C_DATE(2.25), S_OK, C_R8(3.25));
    B(VA_SUB, C_DATE(5.5), C_I4(2), S_OK, C_DATE(3.5));
    B(VA_SUB, C_I4(2), C_DATE(5.5), S_OK, C_DATE(-3.5));
    B(VA_SUB, C_CY(30000), C_CY(5000), S_OK, C_CY(25000));
    B(VA_SUB, C_CY(30000), C_I4(1), S_OK, C_CY(20000));
    B(VA_SUB, C_EMPTY, C_I2(3), S_OK, C_I2(-3));
    B(VA_SUB, C_EMPTY, C_EMPTY, S_OK, C_I2(0));
    B(VA_SUB, C_BSTR(L"5"), C_BSTR(L"3"), S_OK, C_R8(2.0));
    B(VA_SUB, C_EMPTY, C_BSTR(L"3"), S_OK, C_R8(-3.0));
    B(VA_SUB, C_BSTR(L"3"), C_EMPTY, S_OK, C_R8(3.0));
    B(VA_SUB, C_NULL, C_I4(3), S_OK, C_NULL);
    B(VA_SUB, C_I4(3), C_NULL, S_OK, C_NULL);

    BC(VA_MUL, C_I2(300), C_I2(300), S_OK, C_I4(90000));
    BC(VA_MUL, C_I2(200), C_I2(100), S_OK, C_I2(20000));
    BC(VA_MUL, C_I2(-32768), C_I2(-1), S_OK, C_I4(32768));
    BC(VA_MUL, C_I2(7), C_I2(6), S_OK, C_I2(42));
    BC(VA_MUL, C_I4(65536), C_I4(65536), S_OK, C_R8(4294967296.0));
    BC(VA_MUL, C_I4(46341), C_I4(46341), S_OK, C_R8(2147488281.0));
    BC(VA_MUL, C_I4(46340), C_I4(46340), S_OK, C_I4(2147395600));
    BC(VA_MUL, C_I4(-7), C_I4(6), S_OK, C_I4(-42));
    BC(VA_MUL, C_I8(1000000), C_I8(1000000), S_OK, C_I8(1000000000000LL));
    BC(VA_MUL, C_I4(3), C_R4(0.5f), S_OK, C_R8(1.5));
    BC(VA_MUL, C_I2(3), C_R4(0.5f), S_OK, C_R4(1.5f));
    BC(VA_MUL, C_R4(3.0f), C_R4(0.5f), S_OK, C_R4(1.5f));
    BC(VA_MUL, C_R8(3.0), C_R4(0.5f), S_OK, C_R8(1.5));
    BC(VA_MUL, C_DATE(3.0), C_I4(2), S_OK, C_R8(6.0));
    BC(VA_MUL, C_CY(15000), C_I4(2), S_OK, C_CY(30000));
    BC(VA_MUL, C_CY(15000), C_CY(20000), S_OK, C_CY(30000));
    BC(VA_MUL, C_BOOL(-1), C_BOOL(-1), S_OK, C_I2(1));
    BC(VA_MUL, C_BOOL(-1), C_I4(7), S_OK, C_I4(-7));
    BC(VA_MUL, C_EMPTY, C_I4(7), S_OK, C_I4(0));
    BC(VA_MUL, C_EMPTY, C_EMPTY, S_OK, C_I2(0));
    BC(VA_MUL, C_NULL, C_I4(7), S_OK, C_NULL);
    BC(VA_MUL, C_BSTR(L"6"), C_I4(7), S_OK, C_R8(42.0));
    BC(VA_MUL, C_BSTR(L"6"), C_BSTR(L"7"), S_OK, C_R8(42.0));

    /* VarDiv always produces floating point (R4 only when the other operand cannot force R8) */
    B(VA_DIV, C_I4(1), C_I4(2), S_OK, C_R8(0.5));
    B(VA_DIV, C_I2(1), C_I2(2), S_OK, C_R8(0.5));
    B(VA_DIV, C_I4(6), C_I4(3), S_OK, C_R8(2.0));
    B(VA_DIV, C_UI1(5), C_UI1(2), S_OK, C_R8(2.5));
    B(VA_DIV, C_I4(-1), C_I4(4), S_OK, C_R8(-0.25));
    B(VA_DIV, C_R4(1.0f), C_R4(4.0f), S_OK, C_R4(0.25f));
    B(VA_DIV, C_I2(1), C_R4(4.0f), S_OK, C_R4(0.25f));
    B(VA_DIV, C_R4(1.0f), C_I2(4), S_OK, C_R4(0.25f));
    B(VA_DIV, C_UI1(1), C_R4(4.0f), S_OK, C_R4(0.25f));
    B(VA_DIV, C_I4(1), C_R4(4.0f), S_OK, C_R8(0.25));
    B(VA_DIV, C_R4(1.0f), C_I4(4), S_OK, C_R8(0.25));
    B(VA_DIV, C_R8(1.0), C_R4(4.0f), S_OK, C_R8(0.25));
    B(VA_DIV, C_I8(1), C_I8(4), S_OK, C_R8(0.25));
    B(VA_DIV, C_CY(10000), C_CY(40000), S_OK, C_R8(0.25));
    B(VA_DIV, C_DATE(1.0), C_I4(4), S_OK, C_R8(0.25));
    B(VA_DIV, C_BOOL(-1), C_BOOL(-1), S_OK, C_R8(1.0));
    B(VA_DIV, C_EMPTY, C_I4(4), S_OK, C_R8(0.0));
    B(VA_DIV, C_I4(1), C_NULL, S_OK, C_NULL);
    B(VA_DIV, C_NULL, C_I4(1), S_OK, C_NULL);

    /* VarPow */
    B(VA_POW, C_I4(2), C_I4(10), S_OK, C_R8(1024.0));
    B(VA_POW, C_I4(2), C_I4(-1), S_OK, C_R8(0.5));
    B(VA_POW, C_I4(-2), C_I4(3), S_OK, C_R8(-8.0));
    B(VA_POW, C_I4(-2), C_I4(2), S_OK, C_R8(4.0));
    B(VA_POW, C_I4(0), C_I4(0), S_OK, C_R8(1.0));
    B(VA_POW, C_I4(0), C_I4(5), S_OK, C_R8(0.0));
    B(VA_POW, C_I4(7), C_I4(0), S_OK, C_R8(1.0));
    B(VA_POW, C_I4(1), C_I4(1000), S_OK, C_R8(1.0));
    B(VA_POW, C_I4(10), C_I4(15), S_OK, C_R8(1e15));
    B(VA_POW, C_R8(4.0), C_R8(0.5), S_OK, C_R8(2.0));
    B(VA_POW, C_R8(2.0), C_R8(0.5), S_OK, C_R8(1.4142135623730951));
    B(VA_POW, C_R8(0.0), C_R8(0.0), S_OK, C_R8(1.0));
    B(VA_POW, C_UI1(2), C_UI1(8), S_OK, C_R8(256.0));
    B(VA_POW, C_I2(3), C_I2(3), S_OK, C_R8(27.0));
    B(VA_POW, C_BOOL(-1), C_I4(2), S_OK, C_R8(1.0));
    B(VA_POW, C_BOOL(0), C_BOOL(0), S_OK, C_R8(1.0));
    B(VA_POW, C_EMPTY, C_EMPTY, S_OK, C_R8(1.0));
    B(VA_POW, C_EMPTY, C_I4(3), S_OK, C_R8(0.0));
    B(VA_POW, C_I4(3), C_EMPTY, S_OK, C_R8(1.0));
    B(VA_POW, C_CY(20000), C_I4(3), S_OK, C_R8(8.0));
    B(VA_POW, C_DATE(2.0), C_I4(3), S_OK, C_R8(8.0));
    B(VA_POW, C_BSTR(L"2"), C_BSTR(L"3"), S_OK, C_R8(8.0));
    B(VA_POW, C_I8(2), C_I8(40), S_OK, C_R8(1099511627776.0));
    B(VA_POW, C_NULL, C_I4(3), S_OK, C_NULL);
    B(VA_POW, C_I4(3), C_NULL, S_OK, C_NULL);
    B(VA_POW, C_NULL, C_NULL, S_OK, C_NULL);
}
