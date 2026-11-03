/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{6005,0,C_BOOL(VARIANT_TRUE),C_BOOL(VARIANT_TRUE),C_BOOL(VARIANT_TRUE),S_OK},
{6006,0,C_BOOL(VARIANT_FALSE),C_BOOL(VARIANT_TRUE),C_BOOL(VARIANT_FALSE),S_OK},
{6007,0,C_BOOL(TRUE),C_BOOL(TRUE),C_BOOL(VARIANT_TRUE),S_OK},
{6008,0,C_BOOL(FALSE),C_BOOL(FALSE),C_BOOL(VARIANT_TRUE),S_OK},
{6009,0,C_BOOL(TRUE),C_BOOL(FALSE),C_BOOL(-2),S_OK},
{6010,0,C_BOOL(FALSE),C_BOOL(TRUE),C_BOOL(-2),S_OK},
{6011,0,C_BOOL(6),C_BOOL(7),C_BOOL(-2),S_OK},
{6012,0,C_BOOL(6),C_BOOL(6),C_BOOL(VARIANT_TRUE),S_OK},
{6013,0,C_BOOL(VARIANT_TRUE),C_I2(VARIANT_TRUE),C_I2(VARIANT_TRUE),S_OK},
{6014,0,C_BOOL(VARIANT_TRUE),C_I2(VARIANT_FALSE),C_I2(VARIANT_FALSE),S_OK},
{6015,0,C_BOOL(6),C_I2(7),C_I2(-2),S_OK},
{6016,0,C_UI1(1),C_UI1(1),C_UI1(255),S_OK},
{6017,0,C_UI1(1),C_UI1(0),C_UI1(254),S_OK},
{6018,0,C_UI1(0),C_UI1(1),C_UI1(254),S_OK},
{6021,0,C_UI4(VARIANT_FALSE),C_I8(VARIANT_FALSE),C_I8(-1),S_OK},
{6022,0,C_UI4(5),C_I8(19),C_I8(-23),S_OK},
{6023,0,C_UI4(VARIANT_FALSE),C_UI8(VARIANT_FALSE),C_I4(-1),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_eqv_rows) { va_run_binary(VA_EQV, tab, sizeof(tab)/sizeof(tab[0]), 1); }
