/* Generated table of operand/result rows. Rows adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+)
 * with the Windows-side expectation kept; each row is {wine source line, ...}. */
#include "va.h"
static const vrow tab[] = {
{2977,0,C_BOOL(VARIANT_TRUE),C_NONE,C_I2(-VARIANT_TRUE),S_OK},
{2978,0,C_BOOL(VARIANT_FALSE),C_NONE,C_I2(VARIANT_FALSE),S_OK},
{2979,0,C_EMPTY,C_NONE,C_I2(0),S_OK},
{2980,0,C_EMPTY,C_NONE,C_I2(0),S_OK},
{2981,0,C_NULL,C_NONE,C_NULL,S_OK},
{2982,0,C_NULL,C_NONE,C_NULL,S_OK},
{2983,0,C_I2(1),C_NONE,C_I2(1),S_OK},
{2984,0,C_I2(-1),C_NONE,C_I2(1),S_OK},
{2985,0,C_I4(1),C_NONE,C_I4(1),S_OK},
{2986,0,C_I4(-1),C_NONE,C_I4(1),S_OK},
{2987,0,C_UI1(1),C_NONE,C_UI1(1),S_OK},
{2988,0,C_R4(1),C_NONE,C_R4(1),S_OK},
{2989,0,C_R4(-1),C_NONE,C_R4(1),S_OK},
{2990,0,C_R8(1),C_NONE,C_R8(1),S_OK},
{2991,0,C_R8(-1),C_NONE,C_R8(1),S_OK},
{2992,0,C_R4(1.40129846432481707e-45),C_NONE,C_R4(1.40129846432481707e-45),S_OK},
{2993,0,C_R8(4.94065645841246544e-324),C_NONE,C_R8(4.94065645841246544e-324),S_OK},
{2994,0,C_DATE(1),C_NONE,C_DATE(1),S_OK},
{2995,0,C_DATE(-1),C_NONE,C_DATE(1),S_OK},
};
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(variant_arith_ops_abs_rows) { va_run_unary(VA_ABS, tab, sizeof(tab)/sizeof(tab[0]), 0); }
