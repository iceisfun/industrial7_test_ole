/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{4067,0,C_BOOL(VARIANT_TRUE),C_NONE,C_I2(VARIANT_TRUE),S_OK},
{4068,0,C_BOOL(VARIANT_FALSE),C_NONE,C_I2(0),S_OK},
{4069,0,C_BOOL(1),C_NONE,C_I2(1),S_OK},
{4070,0,C_UI1(1),C_NONE,C_UI1(1),S_OK},
{4071,0,C_I2(-1),C_NONE,C_I2(-1),S_OK},
{4072,0,C_I4(-1),C_NONE,C_I4(-1),S_OK},
{4075,0,C_I8(-1),C_NONE,C_I8(-1),S_OK},
{4077,0,C_R4(1.4f),C_NONE,C_R4(1),S_OK},
{4078,0,C_R4(1.5f),C_NONE,C_R4(1),S_OK},
{4079,0,C_R4(1.6f),C_NONE,C_R4(1),S_OK},
{4080,0,C_R4(-1.4f),C_NONE,C_R4(-2),S_OK},
{4081,0,C_R4(-1.5f),C_NONE,C_R4(-2),S_OK},
{4082,0,C_R4(-1.6f),C_NONE,C_R4(-2),S_OK},
{4084,0,C_DATE(-1),C_NONE,C_DATE(-1),S_OK},
{4085,0,C_R8(-1),C_NONE,C_R8(-1),S_OK},
{4086,0,C_BSTR(L"-1"),C_NONE,C_R8(-1),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_int_rows) { va_run_unary(VA_INT, tab, sizeof(tab)/sizeof(tab[0]), 0); }
