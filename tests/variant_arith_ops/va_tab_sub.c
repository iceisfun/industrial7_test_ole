/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{3317,0,C_I4(4),C_I4(2),C_I4(2),S_OK},
{3318,0,C_I2(4),C_I2(2),C_I2(2),S_OK},
{3319,0,C_I2(-13),C_I4(5),C_I4(-18),S_OK},
{3320,0,C_I4(-13),C_I4(5),C_I4(-18),S_OK},
{3321,0,C_I2(7),C_R4(0.5f),C_R4(6.5f),S_OK},
{3322,0,C_R4(0.5f),C_I4(5),C_R8(-4.5),S_OK},
{3323,0,C_R8(7.1),C_BOOL(0),C_R8(7.1),S_OK},
{3324,0,C_BSTR(L"12"),C_I2(4),C_R8(8),S_OK},
{3325,0,C_BSTR(L"12"),C_BOOL(1),C_R8(11),S_OK},
{3326,0,C_BSTR(L"12"),C_R4(0.1f),C_R8(11.9),S_OK},
{3327,0,C_R4(0.2f),C_BSTR(L"12"),C_R8(-11.8),S_OK},
{3328,0,C_DATE(2.25),C_I4(7),C_DATE(-4.75),S_OK},
{3329,0,C_DATE(1.25),C_R4(-1.7f),C_DATE(2.95),S_OK},
{3331,0,C_UI1(UI1_MAX),C_UI1(UI1_MAX),C_UI1(0),S_OK},
{3332,0,C_I2(I2_MAX),C_I2(I2_MAX),C_I2(0),S_OK},
{3333,0,C_I2(I2_MIN),C_I2(I2_MIN),C_I2(0),S_OK},
{3334,0,C_I4(I4_MAX),C_I4(I4_MAX),C_I4(0),S_OK},
{3335,0,C_I4(I4_MIN),C_I4(I4_MIN),C_I4(0),S_OK},
{3336,0,C_R4(R4_MAX),C_R4(R4_MAX),C_R4(0.0f),S_OK},
{3337,0,C_R4(R4_MAX),C_R4(R4_MIN),C_R4(R4_MAX - R4_MIN),S_OK},
{3338,0,C_R4(R4_MIN),C_R4(R4_MIN),C_R4(0.0f),S_OK},
{3339,0,C_R8(R8_MAX),C_R8(R8_MIN),C_R8(R8_MAX - R8_MIN),S_OK},
{3340,0,C_R8(R8_MIN),C_R8(R8_MIN),C_R8(0.0),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_sub_rows) { va_run_binary(VA_SUB, tab, sizeof(tab)/sizeof(tab[0]), 0); }
