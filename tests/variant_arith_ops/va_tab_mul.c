/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{6143,0,C_I4(4),C_I4(2),C_I4(8),S_OK},
{6144,0,C_I2(4),C_I2(2),C_I2(8),S_OK},
{6145,0,C_I2(-13),C_I4(5),C_I4(-65),S_OK},
{6146,0,C_I4(-13),C_I4(5),C_I4(-65),S_OK},
{6147,0,C_I2(7),C_R4(0.5f),C_R4(3.5f),S_OK},
{6148,0,C_R4(0.5f),C_I4(5),C_R8(2.5),S_OK},
{6149,0,C_R8(7.1),C_BOOL(0),C_R8(0),S_OK},
{6150,0,C_BSTR(L"12"),C_I2(4),C_R8(48),S_OK},
{6151,0,C_BSTR(L"12"),C_BOOL(1),C_R8(12),S_OK},
{6152,0,C_BSTR(L"12"),C_R4(0.1f),C_R8(1.2),S_OK},
{6153,0,C_BSTR(L"12"),C_BSTR(L"12"),C_R8(144),S_OK},
{6154,0,C_R4(0.2f),C_BSTR(L"12"),C_R8(2.4),S_OK},
{6155,0,C_DATE(2.25),C_I4(7),C_R8(15.75),S_OK},
{6157,0,C_UI1(UI1_MAX),C_UI1(UI1_MAX),C_I4(UI1_MAX * UI1_MAX),S_OK},
{6158,0,C_I2(I2_MAX),C_I2(I2_MAX),C_I4(I2_MAX * I2_MAX),S_OK},
{6159,0,C_I2(I2_MAX),C_I2(I2_MIN),C_I4(I2_MAX * I2_MIN),S_OK},
{6160,0,C_I2(I2_MIN),C_I2(I2_MIN),C_I4(I2_MIN * I2_MIN),S_OK},
{6161,0,C_I4(I4_MAX),C_I4(I4_MAX),C_R8((double)I4_MAX * I4_MAX),S_OK},
{6162,0,C_I4(I4_MAX),C_I4(I4_MIN),C_R8((double)I4_MAX * I4_MIN),S_OK},
{6163,0,C_I4(I4_MIN),C_I4(I4_MIN),C_R8((double)I4_MIN * I4_MIN),S_OK},
{6164,0,C_R4(R4_MAX),C_R4(R4_MAX),C_R8((double)R4_MAX * R4_MAX),S_OK},
{6165,0,C_R4(R4_MAX),C_R4(R4_MIN),C_R4(R4_MAX * R4_MIN),S_OK},
{6166,0,C_R4(R4_MIN),C_R4(R4_MIN),C_R4(R4_MIN * R4_MIN),S_OK},
{6167,0,C_R8(R8_MAX),C_R8(R8_MIN),C_R8(R8_MAX * R8_MIN),S_OK},
{6168,0,C_R8(R8_MIN),C_R8(R8_MIN),C_R8(R8_MIN * R8_MIN),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_mul_rows) { va_run_binary(VA_MUL, tab, sizeof(tab)/sizeof(tab[0]), 1); }
