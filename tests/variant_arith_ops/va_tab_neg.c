/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{4186,0,C_BOOL(VARIANT_TRUE),C_NONE,C_I2(1),S_OK},
{4187,0,C_BOOL(VARIANT_FALSE),C_NONE,C_I2(0),S_OK},
{4188,0,C_BOOL(1),C_NONE,C_I2(-1),S_OK},
{4189,0,C_UI1(1),C_NONE,C_I2(-1),S_OK},
{4190,0,C_UI1(254),C_NONE,C_I2(-254),S_OK},
{4191,0,C_I2(-32768),C_NONE,C_I4(32768),S_OK},
{4192,0,C_I2(-1),C_NONE,C_I2(1),S_OK},
{4193,0,C_I2(1),C_NONE,C_I2(-1),S_OK},
{4194,0,C_I4(-((int)(~0u >> 1)) - 1),C_NONE,C_R8(-2147483648u),S_OK},
{4195,0,C_I4(-1),C_NONE,C_I4(1),S_OK},
{4196,0,C_I4(1),C_NONE,C_I4(-1),S_OK},
{4199,0,C_I8(1),C_NONE,C_I8(-1),S_OK},
{4200,0,C_I8(-1),C_NONE,C_I8(1),S_OK},
{4202,0,C_R4(1),C_NONE,C_R4(-1),S_OK},
{4203,0,C_R4(-1),C_NONE,C_R4(1),S_OK},
{4204,0,C_DATE(1),C_NONE,C_DATE(-1),S_OK},
{4205,0,C_DATE(-1),C_NONE,C_DATE(1),S_OK},
{4206,0,C_R8(1),C_NONE,C_R8(-1),S_OK},
{4207,0,C_R8(-1),C_NONE,C_R8(1),S_OK},
{4208,0,C_BSTR(L"-1"),C_NONE,C_R8(1),S_OK},
{4209,0,C_BSTR(L"1"),C_NONE,C_R8(-1),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_neg_rows) { va_run_unary(VA_NEG, tab, sizeof(tab)/sizeof(tab[0]), 0); }
