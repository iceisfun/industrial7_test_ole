/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{3953,0,C_BOOL(VARIANT_TRUE),C_NONE,C_I2(VARIANT_TRUE),S_OK},
{3954,0,C_BOOL(VARIANT_FALSE),C_NONE,C_I2(0),S_OK},
{3955,0,C_BOOL(1),C_NONE,C_I2(1),S_OK},
{3956,0,C_UI1(1),C_NONE,C_UI1(1),S_OK},
{3957,0,C_I2(-1),C_NONE,C_I2(-1),S_OK},
{3958,0,C_I4(-1),C_NONE,C_I4(-1),S_OK},
{3961,0,C_I8(-1),C_NONE,C_I8(-1),S_OK},
{3963,0,C_R4(1.4f),C_NONE,C_R4(1),S_OK},
{3964,0,C_R4(1.5f),C_NONE,C_R4(1),S_OK},
{3965,0,C_R4(1.6f),C_NONE,C_R4(1),S_OK},
{3966,0,C_R4(-1.4f),C_NONE,C_R4(-1),S_OK},
{3967,0,C_R4(-1.5f),C_NONE,C_R4(-1),S_OK},
{3968,0,C_R4(-1.6f),C_NONE,C_R4(-1),S_OK},
{3970,0,C_DATE(-1),C_NONE,C_DATE(-1),S_OK},
{3971,0,C_R8(-1),C_NONE,C_R8(-1),S_OK},
{3972,0,C_BSTR(L"-1"),C_NONE,C_R8(-1),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_fix_rows) { va_run_unary(VA_FIX, tab, sizeof(tab)/sizeof(tab[0]), 0); }
