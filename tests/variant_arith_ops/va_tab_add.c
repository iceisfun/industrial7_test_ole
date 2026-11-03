/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{6323,0,C_I4(4),C_I4(2),C_I4(6),S_OK},
{6324,0,C_I2(4),C_I2(2),C_I2(6),S_OK},
{6325,0,C_I2(-13),C_I4(5),C_I4(-8),S_OK},
{6326,0,C_I4(-13),C_I4(5),C_I4(-8),S_OK},
{6327,0,C_I2(7),C_R4(0.5f),C_R4(7.5f),S_OK},
{6328,0,C_R4(0.5f),C_I4(5),C_R8(5.5),S_OK},
{6329,0,C_R8(7.1),C_BOOL(0),C_R8(7.1),S_OK},
{6330,0,C_BSTR(L"12"),C_I2(4),C_R8(16),S_OK},
{6331,0,C_BSTR(L"12"),C_BOOL(1),C_R8(13),S_OK},
{6332,0,C_BSTR(L"12"),C_R4(0.1f),C_R8(12.1),S_OK},
{6333,0,C_R4(0.2f),C_BSTR(L"12"),C_R8(12.2),S_OK},
{6334,0,C_DATE(2.25),C_I4(7),C_DATE(9.25),S_OK},
{6335,0,C_DATE(1.25),C_R4(-1.7f),C_DATE(-0.45),S_OK},
{6337,0,C_UI1(UI1_MAX),C_UI1(UI1_MAX),C_I2(UI1_MAX + UI1_MAX),S_OK},
{6338,0,C_I2(I2_MAX),C_I2(I2_MAX),C_I4(I2_MAX + I2_MAX),S_OK},
{6339,0,C_I2(I2_MAX),C_I2(I2_MIN),C_I2(I2_MAX + I2_MIN),S_OK},
{6340,0,C_I2(I2_MIN),C_I2(I2_MIN),C_I4(I2_MIN + I2_MIN),S_OK},
{6341,0,C_I4(I4_MAX),C_I4(I4_MIN),C_I4(I4_MAX + I4_MIN),S_OK},
{6342,0,C_I4(I4_MAX),C_I4(I4_MAX),C_R8((double)I4_MAX + I4_MAX),S_OK},
{6343,0,C_I4(I4_MIN),C_I4(I4_MIN),C_R8((double)I4_MIN + I4_MIN),S_OK},
{6344,0,C_R4(R4_MAX),C_R4(R4_MAX),C_R8((double)R4_MAX + R4_MAX),S_OK},
{6345,0,C_R4(R4_MAX),C_R4(R4_MIN),C_R4(R4_MAX + R4_MIN),S_OK},
{6346,0,C_R4(R4_MIN),C_R4(R4_MIN),C_R4(R4_MIN + R4_MIN),S_OK},
{6347,0,C_R8(R8_MAX),C_R8(R8_MIN),C_R8(R8_MAX + R8_MIN),S_OK},
{6348,0,C_R8(R8_MIN),C_R8(R8_MIN),C_R8(R8_MIN + R8_MIN),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_add_rows) { va_run_binary(VA_ADD, tab, sizeof(tab)/sizeof(tab[0]), 1); }
