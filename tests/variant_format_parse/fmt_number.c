/* variant_format_parse: VarFormat number / string / boolean formats (oleaut32.dll, en-US 0x409).
 * Decomp-verified: VarFormat = VarTokenizeFormatString(fmt, buf[300], iFirstDay, iFirstWeek, GetUserDefaultLCID()) then
 * VarFormatFromTokens; pbvarIn/pbstrOut NULL, iFirstDay > 7 and iFirstWeek > 3 are rejected with E_INVALIDARG before any work. */
#include "vfp.h"

typedef struct { double v; const char *fmt; const char *exp; } NumRow;

static void run_num_rows(const NumRow *rows, size_t n, int vt)
{
    size_t i;
    for (i = 0; i < n; i++) {
        VARIANT v;
        switch (vt) {
        case VT_R8: v = vfp_r8(rows[i].v); break;
        case VT_R4: v = vfp_r4((float)rows[i].v); break;
        case VT_I4: v = vfp_i4((LONG)rows[i].v); break;
        case VT_I2: v = vfp_i2((SHORT)rows[i].v); break;
        case VT_I8: v = vfp_i8((LONG64)rows[i].v); break;
        case VT_UI1: v = vfp_ui1((BYTE)rows[i].v); break;
        case VT_CY: v = vfp_cy((LONG64)(rows[i].v * 10000.0)); break;
        default: v = vfp_r8(rows[i].v); break;
        }
        VFP_ROW(v, rows[i].fmt, 0, 0, rows[i].exp);
    }
}

/* ---- named numeric formats ---------------------------------------------- */
static const NumRow named_general[] = {
    { 0, "General Number", "0" }, { 1, "General Number", "1" }, { -1, "General Number", "-1" },
    { 1234.5, "General Number", "1234.5" }, { 0.5, "General Number", "0.5" }, { -0.25, "General Number", "-0.25" },
    { 123456789, "General Number", "123456789" }, { 100, "General Number", "100" }, { 3.14159, "General Number", "3.14159" },
    { 0.001, "General Number", "0.001" }, { 99999, "General Number", "99999" }, { -987654, "General Number", "-987654" },
    { 12.34, "general number", "12.34" }, { 12.34, "GENERAL NUMBER", "12.34" }, { 7, "General number", "7" },
};
static const NumRow named_fixed[] = {
    { 0, "Fixed", "0.00" }, { 1, "Fixed", "1.00" }, { -1, "Fixed", "-1.00" }, { 1234.5, "Fixed", "1234.50" },
    { 0.5, "Fixed", "0.50" }, { -0.5, "Fixed", "-0.50" }, { 1234.567, "Fixed", "1234.57" }, { 1234567.891, "Fixed", "1234567.89" },
    { 0.004, "Fixed", "0.00" }, { 0.006, "Fixed", "0.01" }, { -1234.567, "Fixed", "-1234.57" }, { 99.999, "Fixed", "100.00" },
    { 5, "fixed", "5.00" }, { 5, "FIXED", "5.00" },
};
static const NumRow named_standard[] = {
    { 0, "Standard", "0.00" }, { 1, "Standard", "1.00" }, { -1, "Standard", "-1.00" }, { 1234.5, "Standard", "1,234.50" },
    { -1234.5, "Standard", "-1,234.50" }, { 1234567.891, "Standard", "1,234,567.89" }, { 999.994, "Standard", "999.99" },
    { 999.996, "Standard", "1,000.00" }, { 123456789, "Standard", "123,456,789.00" }, { 0.5, "Standard", "0.50" },
    { 100, "Standard", "100.00" }, { 1000, "Standard", "1,000.00" }, { -1000000, "Standard", "-1,000,000.00" },
    { 12, "standard", "12.00" },
};
static const NumRow named_percent[] = {
    { 0, "Percent", "0.00%" }, { 1, "Percent", "100.00%" }, { 0.5, "Percent", "50.00%" }, { -0.25, "Percent", "-25.00%" },
    { 0.123456, "Percent", "12.35%" }, { 12.5, "Percent", "1250.00%" }, { 0.001, "Percent", "0.10%" }, { 2, "Percent", "200.00%" },
    { 0.0001, "Percent", "0.01%" }, { 10, "Percent", "1000.00%" }, { -1, "Percent", "-100.00%" }, { 0.255, "Percent", "25.50%" },
    { 0.5, "percent", "50.00%" },
};
static const NumRow named_sci[] = {
    { 0, "Scientific", "0.00E+00" }, { 1, "Scientific", "1.00E+00" }, { -1, "Scientific", "-1.00E+00" },
    { 1234.5, "Scientific", "1.23E+03" }, { -1234.5, "Scientific", "-1.23E+03" }, { 0.00012, "Scientific", "1.20E-04" },
    { 12345678, "Scientific", "1.23E+07" }, { 0.5, "Scientific", "5.00E-01" }, { 10, "Scientific", "1.00E+01" },
    { 1e10, "Scientific", "1.00E+10" }, { 1.5e-10, "Scientific", "1.50E-10" }, { 99999, "Scientific", "1.00E+05" },
    { 5, "scientific", "5.00E+00" },
};
static const NumRow named_currency[] = {
    { 0, "Currency", "$0.00" }, { 1, "Currency", "$1.00" }, { 1234.5, "Currency", "$1,234.50" }, { 0.5, "Currency", "$0.50" },
    { 1234567.891, "Currency", "$1,234,567.89" }, { 999.995, "Currency", "$1,000.00" }, { 100, "Currency", "$100.00" },
    { 12, "currency", "$12.00" }, { -1234.5, "Currency", "($1,234.50)" }, { -1, "Currency", "($1.00)" }, { -0.5, "Currency", "($0.50)" },
};

TEST(vfp_fmt_named_general_number) { VFP_NEED_FORMAT(); run_num_rows(named_general, sizeof named_general / sizeof *named_general, VT_R8); }
TEST(vfp_fmt_named_fixed)          { VFP_NEED_FORMAT(); run_num_rows(named_fixed, sizeof named_fixed / sizeof *named_fixed, VT_R8); }
TEST(vfp_fmt_named_standard)       { VFP_NEED_FORMAT(); run_num_rows(named_standard, sizeof named_standard / sizeof *named_standard, VT_R8); }
TEST(vfp_fmt_named_percent)        { VFP_NEED_FORMAT(); run_num_rows(named_percent, sizeof named_percent / sizeof *named_percent, VT_R8); }
TEST(vfp_fmt_named_scientific)     { VFP_NEED_FORMAT(); run_num_rows(named_sci, sizeof named_sci / sizeof *named_sci, VT_R8); }
TEST(vfp_fmt_named_currency)       { VFP_NEED_FORMAT(); run_num_rows(named_currency, sizeof named_currency / sizeof *named_currency, VT_R8); }

/* the same named formats, integer sources (Wine table: VarFormat(1/0) for every numeric VT) */
typedef struct { const char *fmt, *one, *zero; } FmtRes;
static const FmtRes one_zero[] = {
    { NULL, "1", "0" }, { "", "1", "0" }, { "General Number", "1", "0" }, { "Percent", "100.00%", "0.00%" },
    { "Standard", "1.00", "0.00" }, { "Scientific", "1.00E+00", "0.00E+00" }, { "True/False", "True", "False" },
    { "On/Off", "On", "Off" }, { "Yes/No", "Yes", "No" }, { "#", "1", "" }, { "##", "1", "" }, { "#.#", "1.", "." },
    { "0", "1", "0" }, { "00", "01", "00" }, { "0.0", "1.0", "0.0" }, { "00\\c\\o\\p\\y", "01copy", "00copy" },
    { "\"pos\";\"neg\"", "pos", "pos" }, { "\"pos\";\"neg\";\"zero\"", "pos", "zero" },
    { "Fixed", "1.00", "0.00" }, { "Currency", "$1.00", "$0.00" },
};
static void run_one_zero(int vt)
{
    size_t i;
    for (i = 0; i < sizeof one_zero / sizeof *one_zero; i++) {
        VARIANT a, b;
        switch (vt) {
        case VT_I2: a = vfp_i2(1); b = vfp_i2(0); break;
        case VT_I4: a = vfp_i4(1); b = vfp_i4(0); break;
        case VT_I8: a = vfp_i8(1); b = vfp_i8(0); break;
        case VT_UI1: a = vfp_ui1(1); b = vfp_ui1(0); break;
        case VT_R4: a = vfp_r4(1); b = vfp_r4(0); break;
        case VT_CY: a = vfp_cy(10000); b = vfp_cy(0); break;
        default: a = vfp_r8(1); b = vfp_r8(0); break;
        }
        VFP_ROW(a, one_zero[i].fmt, 0, 0, one_zero[i].one);
        VFP_ROW(b, one_zero[i].fmt, 0, 0, one_zero[i].zero);
    }
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_one_zero_i2)  { VFP_NEED_FORMAT(); run_one_zero(VT_I2); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_one_zero_i4)  { VFP_NEED_FORMAT(); run_one_zero(VT_I4); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_one_zero_i8)  { VFP_NEED_FORMAT(); run_one_zero(VT_I8); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_one_zero_ui1) { VFP_NEED_FORMAT(); run_one_zero(VT_UI1); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_one_zero_r4)  { VFP_NEED_FORMAT(); run_one_zero(VT_R4); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_one_zero_r8)  { VFP_NEED_FORMAT(); run_one_zero(VT_R8); }

/* ---- user-defined numeric formats --------------------------------------- */
static const NumRow user_digits[] = {
    /* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
    { 1, "#00000000", "00000001" }, { 1, "000###", "000001" }, { 1, "#00##00#0", "00000001" }, { 1, "1#####0000", "10001" },
    { 1, "##abcdefghijklmnopqrstuvwxyz", "1abcdefghijklmnopqrstuvwxyz" }, { 100000, "#,###,###,###", "100,000" },
    { 1, "0,000,000,000", "0,000,000,001" }, { 123456789, "#,#.#", "123,456,789." }, { 123456789, "###, ###, ###", "123, 456, 789" },
    { 1, "#;-#", "1" }, { -1, "#;-#", "-1" }, { 17, "#0", "17" }, { 4711, "#0", "4711" }, { 17, "#00", "17" }, { 100, "0##", "100" },
    { 17, "#000", "017" }, { 17, "#0.00", "17.00" }, { 17, "#0000.00", "0017.00" }, { 17, "#.00", "17.00" }, { 17, "#3", "173" },
    { 17, "#33", "1733" }, { 17, "#3.33", "173.33" }, { 17, "#3333.33", "173333.33" }, { 17, "#.33", "17.33" },
    /* beyond Wine: plain digit placeholders */
    { 0, "0", "0" }, { 5, "0", "5" }, { 5, "00", "05" }, { 5, "000", "005" }, { 5, "0000", "0005" }, { 12345, "0", "12345" },
    { 12345, "00", "12345" }, { 0, "00000", "00000" }, { 5, "#", "5" }, { 0, "#", "" }, { 0, "##", "" }, { 0, "#0", "0" }, { 0, "0#", "0" },
    { 42, "#", "42" }, { 42, "0000", "0042" }, { 42, "#,##0", "42" }, { 42, "#,###", "42" }, { 0, "#,###", "" }, { 0, "#,##0", "0" },
    { 1234, "#,##0", "1,234" }, { 1234567, "#,##0", "1,234,567" }, { 999, "#,##0", "999" }, { 1000, "#,##0", "1,000" },
    { 123, "#,##0", "123" }, { 12, "#,##0", "12" }, { 1234567890, "#,##0", "1,234,567,890" }, { -1234, "#,##0", "-1,234" },
    { -12, "#,##0", "-12" }, { 999999, "#,##0", "999,999" }, { 1000000, "#,##0", "1,000,000" }, { 100, "#,##0", "100" },
    { 1234, "0,000", "1,234" }, { 12, "0,000", "0,012" }, { 1234567, "0,000", "1,234,567" },
    { 1234, "#,##0.00", "1,234.00" }, { 1234.5, "#,##0.00", "1,234.50" }, { 0.5, "#,##0.00", "0.50" }, { 0, "#,##0.00", "0.00" },
    { 1234567.891, "#,##0.00", "1,234,567.89" }, { -1234.5, "#,##0.00", "-1,234.50" },
};
static const NumRow user_decimals[] = {
    { 1.5, "0.00", "1.50" }, { 0, "0.00", "0.00" }, { -1.5, "0.00", "-1.50" }, { 1.5, "0.0", "1.5" }, { 1.55, "0.0", "1.6" }, { 1.549, "0.0", "1.5" },
    { 1.5, "0.000", "1.500" }, { 3.14159, "0.000", "3.142" }, { 3.14159, "0.0000", "3.1416" }, { 3.14159, "0.00000", "3.14159" },
    { 3.14159, "0", "3" }, { 3.5, "0", "4" }, { 2.4, "0", "2" }, { 2.6, "0", "3" }, { -2.6, "0", "-3" }, { -2.4, "0", "-2" },
    { 0.5, "0.00", "0.50" }, { 0.05, "0.00", "0.05" }, { 0.004, "0.00", "0.00" }, { 0.006, "0.00", "0.01" }, { 99.995, "0.00", "100.00" },
    { 99.994, "0.00", "99.99" }, { 9.9999, "0.00", "10.00" }, { 0.999, "0.00", "1.00" }, { 0.999, "0", "1" }, { 0.4999, "0", "0" },
    { 0, "0.0", "0.0" }, { 100, "0.0", "100.0" }, { 100, "0.00", "100.00" }, { 7, "0.00", "7.00" }, { 7.1, "00.00", "07.10" },
    { 7.1, "000.00", "007.10" }, { 123.456, "0.00", "123.46" }, { 123.456, "0.0", "123.5" }, { 123.456, "0", "123" },
    { 123.456, "000000.0000", "000123.4560" }, { 0.5, "#.00", ".50" }, { 0.5, "#.##", ".5" }, { 0, "#.##", "." }, { 1, "#.##", "1." },
    { 1.5, "#.##", "1.5" }, { 1.25, "#.##", "1.25" }, { 1.255, "#.##", "1.26" }, { 0.5, "0.##", "0.5" }, { 1, "0.##", "1." },
    { 0, "0.##", "0." }, { 1.5, "0.##", "1.5" }, { 12.345, "0.##", "12.35" }, { 12.3, "0.00#", "12.30" }, { 12.3456, "0.00#", "12.346" },
    { 12.3456, "0.#00", "12.346" }, { 5, "0.0#", "5.0" }, { 5.25, "0.0#", "5.25" }, { 5.255, "0.0#", "5.26" },
    { -0.1, ".#", "-.1" }, { 0.099, "#.#", ".1" }, { 0.0049, "#.##", "." }, { 0.0099, "#.##", ".01" }, { 8.0995, "#.##", "8.1" },
    { 1.57, "0.00", "1.57" }, { -1.57, "0.00", "-1.57" }, { -1.57, "#.##", "-1.57" }, { 1.7, "#.00", "1.70" }, { .17, "#.00", ".17" },
    { 1.0, "#,##0.00", "1.00" }, { 1.7, "#0", "2" }, { 1.7, "#.33", "2.33" }, { 1.7, "#3", "23" }, { 9.9999999, "#0.000000", "10.000000" },
    { 1.23456789, "0#.0#0#0#0#0", "01.234567890" }, { 1.2, "0#.0#0#0#0#0", "01.200000000" }, { 9.87654321, "#0.#0#0#0#0#", "9.87654321" },
    { 9.8, "#0.#0#0#0#0#", "9.80000000" }, { 0.00000008, "#0.#0#0#0#0#0", "0.0000000800" }, { 0.00010705, "#0.##########", "0.00010705" },
    { 0.17, "#.33", ".33" },
};
static const NumRow user_percent[] = {
    { 0.5, "0%", "50%" }, { 1, "0%", "100%" }, { 0, "0%", "0%" }, { 0.256, "0.0%", "25.6%" }, { 0.126, "0%", "13%" }, { 0.124, "0%", "12%" },
    { 2.5, "0%", "250%" }, { -0.5, "0%", "-50%" }, { 0.5, "0.00%", "50.00%" }, { 0.12345, "0.00%", "12.35%" }, { 0.12344, "0.00%", "12.34%" },
    { 0.5, "#%", "50%" }, { 0, "#%", "%" }, { 0.5, "0 %", "50 %" }, { 0.075, "0.0%", "7.5%" }, { 12, "0%", "1200%" },
    { 0.5, "0.0%;(0.0%)", "50.0%" }, { -0.5, "0.0%;(0.0%)", "(50.0%)" }, { 0.001, "0.00%", "0.10%" }, { 1234.5, "#,##0%", "123,450%" },
    { 0.5, "\"pct: \"0%", "pct: 50%" }, { 0.333333, "0.0%", "33.3%" },
};
static const NumRow user_sci[] = {
    { 12345, "0.0E+00", "1.2E+04" }, { 0.00123, "0.0E+00", "1.2E-03" }, { 1, "0.0E+00", "1.0E+00" }, { -12345, "0.0E+00", "-1.2E+04" },
    { 12345, "0.0E-00", "1.2E04" }, { 0.00123, "0.0E-00", "1.2E-03" }, { 12345, "0.00E+00", "1.23E+04" }, { 12345, "0.000E+000", "1.235E+004" },
    { 12345, "0E+00", "1E+04" }, { 12345, "00.0E+00", "12.3E+03" }, { 1.5, "0.0e+00", "1.5e+00" }, { 1.5, "0.0e-00", "1.5e00" },
    { 0, "0.00E+00", "0.00E+00" }, { 1e20, "0.0E+00", "1.0E+20" }, { 1e-20, "0.0E+00", "1.0E-20" }, { 99999, "0.0E+00", "1.0E+05" },
    { 1.73245, "0.0000E+000", "1.7325E+000" }, { 1.7, "0.0000E-000", "1.7000E000" }, { 100.0001, "0.0000E+0", "1.0000E+2" },
    { 29.565331, "0.#####E-#", "2.95653E1" }, { 1.7, "0.0000e-1", "1.7000e01" }, { 47.11, ".0000E+0", ".4711E+2" },
};
static const NumRow user_literals[] = {
    { 5, "\"Total: \"0.00", "Total: 5.00" }, { 5, "0.00\" units\"", "5.00 units" }, { 5, "\"[\"0\"]\"", "[5]" }, { 1234.5, "$#,##0.00", "$1,234.50" },
    { 1234.5, "\\$#,##0.00", "$1,234.50" }, { 12, "(0)", "(12)" }, { 5551234567.0, "(000) 000-0000", "(555) 123-4567" }, { 5, "+0;-0", "+5" },
    { -5, "+0;-0", "-5" }, { 0, "+0;-0", "+0" }, { 5, "0\\x", "5x" }, { 5, "\\x0", "x5" }, { 5, "0 \"items\"", "5 items" }, { 5, "\"a\"0\"b\"", "a5b" },
    { 5, "0\\:00", "0:05" }, { 5, "0-0", "0-5" }, { 25, "00-00", "00-25" }, { 1234, "0-0-0-0", "1-2-3-4" }, { 5, "0/0", "0/5" },
    { 5, "\"\"0", "5" }, { 5, "0\"\"", "5" }, { 5, "\\0", "0" }, { 5, "\\#0", "#5" }, { 5, "0\\.0", "0.5" }, { 5, "'0'", "'5'" },
    { 3, "00\\c\\o\\p\\y", "03copy" }, { 1234.5, "\\$#,##0.00;(\\$#,##0.00)", "$1,234.50" }, { -1234.5, "\\$#,##0.00;(\\$#,##0.00)", "($1,234.50)" },
    { 0, "\\$#,##0.00;(\\$#,##0.00)", "$0.00" }, { 7, "0 ", "7 " }, { 7, " 0", " 7" }, { 7, "00 00", "00 07" }, { 7, "-0", "-7" }, { 7, "0-", "7-" },
    { 7, "$0", "$7" }, { 7, "0$", "7$" },
};
static const NumRow user_sections[] = {
    { 5, "0.00;(0.00);\"zero\"", "5.00" }, { -5, "0.00;(0.00);\"zero\"", "(5.00)" }, { 0, "0.00;(0.00);\"zero\"", "zero" },
    { 5, "\"pos\";\"neg\"", "pos" }, { -5, "\"pos\";\"neg\"", "neg" }, { 0, "\"pos\";\"neg\"", "pos" }, { 5, "\"pos\";\"neg\";\"zero\"", "pos" },
    { -5, "\"pos\";\"neg\";\"zero\"", "neg" }, { 0, "\"pos\";\"neg\";\"zero\"", "zero" }, { -1, "\"pos\";\"neg\"", "neg" },
    { 1234.5, "#,##0.00;(#,##0.00)", "1,234.50" }, { -1234.5, "#,##0.00;(#,##0.00)", "(1,234.50)" }, { 0, "#,##0.00;(#,##0.00)", "0.00" },
    { 5, "0;-0;\"nil\"", "5" }, { -5, "0;-0;\"nil\"", "-5" }, { 0, "0;-0;\"nil\"", "nil" }, { -5, "0;(0)", "(5)" }, { -5, "0;0", "5" },
    { -5, "0;-0", "-5" }, { -5, "0;--0", "--5" }, { -5, "0;\\-0", "-5" }, { -1.5, "0.0;(0.0)", "(1.5)" }, { 1.5, "0.0;(0.0)", "1.5" },
    { -1, "#;-#", "-1" }, { 1, "#;-#", "1" }, { 0, "#;-#;\"-\"", "-" }, { 0, "0;0;\"-\"", "-" }, { 3, "0.0;;\"z\"", "3.0" }, { 0, "0.0;;\"z\"", "z" },
    { -0.4, "0;(0)", "(0)" }, { 5, "0\" up\";0\" down\"", "5 up" }, { -5, "0\" up\";0\" down\"", "5 down" }, { -5, "\"-\"0", "--5" },
    { 5, "\"x\"0;\"y\"0;\"z\"", "x5" }, { -5, "\"x\"0;\"y\"0;\"z\"", "y5" }, { 0, "\"x\"0;\"y\"0;\"z\"", "z" },
};

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_user_digit_placeholders) { VFP_NEED_FORMAT(); run_num_rows(user_digits, sizeof user_digits / sizeof *user_digits, VT_R8); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_user_decimal_places)     { VFP_NEED_FORMAT(); run_num_rows(user_decimals, sizeof user_decimals / sizeof *user_decimals, VT_R8); }
TEST(vfp_fmt_user_percent)            { VARIANT p = vfp_r8(0.5); VFP_NEED_FORMAT(); run_num_rows(user_percent, sizeof user_percent / sizeof *user_percent, VT_R8); VFP_ROW_SOFT(p, "%0", 0, 0, "%50"); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_user_scientific)         { VFP_NEED_FORMAT(); run_num_rows(user_sci, sizeof user_sci / sizeof *user_sci, VT_R8); }
TEST(vfp_fmt_user_literals)           { VFP_NEED_FORMAT(); run_num_rows(user_literals, sizeof user_literals / sizeof *user_literals, VT_R8); }
TEST(vfp_fmt_user_sections)           { VFP_NEED_FORMAT(); run_num_rows(user_sections, sizeof user_sections / sizeof *user_sections, VT_R8); }

/* same placeholder table through the integer VT_I4 source (value is converted exactly, no rounding differences) */
TEST(vfp_fmt_user_digits_i4)
{
    static const NumRow rows[] = {
        { 0, "0", "0" }, { 7, "00", "07" }, { 1234, "#,##0", "1,234" }, { -1234, "#,##0", "-1,234" }, { 0, "#,##0", "0" },
        { 2147483647, "#,##0", "2,147,483,647" }, { -2147483647, "#,##0", "-2,147,483,647" }, { 5, "0.00", "5.00" }, { 5, "Fixed", "5.00" },
        { 5, "Standard", "5.00" }, { 1000, "Standard", "1,000.00" }, { 5, "Percent", "500.00%" }, { 5, "Currency", "$5.00" },
        { 12, "Scientific", "1.20E+01" }, { 0, "0;0;\"z\"", "z" }, { -3, "0;(0)", "(3)" }, { 123, "000000", "000123" },
    };
    VFP_NEED_FORMAT();
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_I4);
}
TEST(vfp_fmt_user_digits_i2_ui1)
{
    static const NumRow rows[] = { { 0, "0", "0" }, { 7, "00", "07" }, { 100, "#,##0", "100" }, { 33, "0.0", "33.0" }, { 25, "0%", "2500%" } };
    VFP_NEED_FORMAT();
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_I2);
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_UI1);
}
TEST(vfp_fmt_user_digits_i8)
{
    static const NumRow rows[] = { { 0, "0", "0" }, { 123456789012.0, "#,##0", "123,456,789,012" }, { -5, "0.00", "-5.00" } };
    VFP_NEED_FORMAT();
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_I8);
}
TEST(vfp_fmt_user_digits_cy)
{
    static const NumRow rows[] = { { 0, "0.00", "0.00" }, { 1234.56, "#,##0.00", "1,234.56" }, { -0.5, "0.00", "-0.50" }, { 12.5, "Currency", "$12.50" }, { 12.5, "General Number", "12.5" } };
    VFP_NEED_FORMAT();
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_CY);
}
TEST(vfp_fmt_user_digits_r4)
{
    static const NumRow rows[] = { { 0.5, "0.00", "0.50" }, { 1.25, "0.0", "1.3" }, { 100, "Standard", "100.00" }, { 0.25, "Percent", "25.00%" } };
    VFP_NEED_FORMAT();
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_R4);
}

/* ---- boolean named formats ---------------------------------------------- */
TEST(vfp_fmt_bool_named)
{
    static const struct { const char *fmt, *t, *f; } rows[] = {
        { "True/False", "True", "False" }, { "true/false", "True", "False" }, { "Yes/No", "Yes", "No" }, { "yes/no", "Yes", "No" },
        { "On/Off", "On", "Off" }, { "on/off", "On", "Off" }, { "ON/OFF", "On", "Off" },
    };
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT t = vfp_bool(1), f = vfp_bool(0);
        VFP_ROW(t, rows[i].fmt, 0, 0, rows[i].t);
        VFP_ROW(f, rows[i].fmt, 0, 0, rows[i].f);
    }
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_bool_byref)
{
    VARIANT_BOOL t = VARIANT_TRUE, f = VARIANT_FALSE;
    VARIANT v;
    VFP_NEED_FORMAT();
    VariantInit(&v); V_VT(&v) = VT_BOOL | VT_BYREF; V_BOOLREF(&v) = &t;
    VFP_ROW(v, "True/False", 0, 0, "True");
    V_BOOLREF(&v) = &f;
    VFP_ROW(v, "True/False", 0, 0, "False");
    V_BOOLREF(&v) = &t;
    VFP_ROW(v, "Yes/No", 0, 0, "Yes");
    VFP_ROW(v, "On/Off", 0, 0, "On");
}
TEST(vfp_fmt_numeric_as_boolean_names)
{
    /* any non-zero numeric is "true" for the boolean named formats */
    static const NumRow rows[] = {
        { 1, "Yes/No", "Yes" }, { 0, "Yes/No", "No" }, { -1, "Yes/No", "Yes" }, { 2, "Yes/No", "Yes" }, { 0.5, "Yes/No", "Yes" },
        { 100, "True/False", "True" }, { 0, "True/False", "False" }, { -3, "On/Off", "On" }, { 0, "On/Off", "Off" }, { 0.001, "On/Off", "On" },
    };
    VFP_NEED_FORMAT();
    run_num_rows(rows, sizeof rows / sizeof *rows, VT_R8);
    { size_t i; for (i = 0; i < sizeof rows / sizeof *rows; i++) if (rows[i].v == floor(rows[i].v)) run_num_rows(rows + i, 1, VT_I4); }
}
TEST(vfp_fmt_bool_numeric_formats)
{
    /* VT_BOOL True is -1 (VARIANT_TRUE) when a numeric format is applied: observation only */
    VARIANT t = vfp_bool(1), f = vfp_bool(0);
    VFP_NEED_FORMAT();
    VFP_ROW_SOFT(t, "0", 0, 0, "-1");
    VFP_ROW_SOFT(f, "0", 0, 0, "0");
    VFP_ROW_SOFT(t, "General Number", 0, 0, "-1");
    /* decomp: no token buffer + VT_BOOL selects a built-in boolean token ({0e,04,06}); its text form is not verified */
    VFP_ROW_SOFT(t, NULL, 0, 0, "True");
    VFP_ROW_SOFT(f, NULL, 0, 0, "False");
    VFP_ROW_SOFT(t, "", 0, 0, "True");
}

/* ---- string (BSTR) formats ---------------------------------------------- */
TEST(vfp_fmt_string_placeholders)
{
    /* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
    static const struct { const char *in, *fmt, *exp; } rows[] = {
        { "testing", "", "testing" }, { "testing", "@", "testing" }, { "testing", "&", "testing" },
        { "testing", "\\x@\\x@", "xtxesting" }, { "testing", "\\x&\\x&", "xtxesting" }, { "testing", "@\\x", "txesting" },
        { "testing", "@@@@@@@@", " testing" }, { "testing", "@\\x@@@@@@@", " xtesting" }, { "testing", "&&&&&&&&", "testing" },
        { "testing", "!&&&&&&&", "testing" }, { "testing", "&&&&&&&!", "testing" }, { "testing", ">&&", "TESTING" },
        { "testing", "<&&", "testing" }, { "testing", "<&>&", "testing" },
        /* beyond Wine */
        { "Hello", ">", "HELLO" }, { "Hello", "<@", "hello" }, { "Hello", ">@", "HELLO" }, { "Hello", "@@@@@@@@@", "    Hello" },
        { "Hello", "!@@@@@@@@@", "Hello    " }, { "Hello", "&&&&&&&", "Hello" }, { "Hello", "@@", "Hello" }, { "Hello", "@@@@@", "Hello" },
        { "Hello", "\"<\"@\">\"", "<H>ello" }, { "Hello", "@@@@@@", " Hello" }, { "ab", "(@@@)", "( ab)" },
        { "", "@", " " }, { "", ">@", " " }, { "Hello", "\"Name: \"@", "Name: Hello" }, { "Hello", "@\" there\"", "H thereello" },
        { "MiXeD", ">&", "MIXED" }, { "MiXeD", "<&", "mixed" }, { "abc", "&-&-&", "a-b-c" }, { "abc", "@-@-@", "a-b-c" },
        { "abcdef", "&-&", "a-bcdef" }, { "abcdef", "@-@", "a-bcdef" }, { "ab", "&&&-&", "ab-" },
    };
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT v = vfp_bstr(rows[i].in);
        if (!strcmp(rows[i].in, "abcdef") || !strcmp(rows[i].in, "ab") || !strcmp(rows[i].in, "abc") || !strcmp(rows[i].in, "Hello") || !strcmp(rows[i].in, ""))
            VFP_ROW_SOFT(v, rows[i].fmt, 0, 0, rows[i].exp);
        else
            VFP_ROW(v, rows[i].fmt, 0, 0, rows[i].exp);
        vfp_clear(&v);
    }
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_string_parsed_as_number_or_date)
{
    static const struct { const char *in, *fmt, *exp; int soft; } rows[] = {
        { "39697.11", "hh:mm", "02:38", 0 }, { "39697.11", "mm-dd-yy", "09-06-08", 0 },
        { "1234.5", "#,##0.00", "1,234.50", 0 }, { "1234.5", "0.00", "1234.50", 0 }, { "1", "0.00", "1.00", 0 }, { "-1", "0.00", "-1.00", 0 },
        { "0.25", "Percent", "25.00%", 0 }, { "12", "000", "012", 0 }, { "1e3", "0", "1000", 1 }, { "0", "Yes/No", "No", 1 }, { "5", "Yes/No", "Yes", 1 },
        { "abc", "0.00", "abc", 1 }, { "", "0.00", "", 1 }, { "abc", "@@@@@", "  abc", 1 },
    };
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT v = vfp_bstr(rows[i].in);
        if (rows[i].soft) VFP_ROW_SOFT(v, rows[i].fmt, 0, 0, rows[i].exp); else VFP_ROW(v, rows[i].fmt, 0, 0, rows[i].exp);
        vfp_clear(&v);
    }
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_numeric_through_string_format)
{
    /* a number formatted with a string format is first converted to its string form */
    VARIANT v = vfp_i4(1);
    VFP_NEED_FORMAT();
    VFP_ROW(v, "<&>&", 0, 0, "1");
    v = vfp_i4(42); VFP_ROW_SOFT(v, ">&", 0, 0, "42");
    v = vfp_i4(42); VFP_ROW_SOFT(v, "@@@@", 0, 0, "  42");
}

/* ---- null / empty / invalid arguments ------------------------------------ */
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_null_variant_gives_null_bstr)
{
    VARIANT v = vfp_null();
    BSTR out = (BSTR)0x1;
    HRESULT hr;
    VFP_NEED_FORMAT();
    hr = U(VarFormat)(&v, NULL, 0, 0, 0, &out);
    EXPECT_EQ(hr, S_OK);
    CHECK(out == NULL);
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fmt_out_not_cleared_on_entry)
{
    /* the output slot is overwritten, never freed: a poisoned slot must not crash and is replaced */
    VARIANT v = vfp_i4(5);
    BSTR out = (BSTR)0x1;
    HRESULT hr;
    VFP_NEED_FORMAT();
    hr = U(VarFormat)(&v, NULL, 0, 0, 0, &out);
    EXPECT_EQ(hr, S_OK);
    CHECK(out != (BSTR)0x1 && out != NULL);
    if (out && out != (BSTR)0x1) U(SysFreeString)(out);
}
TEST(vfp_fmt_invalid_args)
{
    /* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+): decomp shows E_INVALIDARG for these before any formatting */
    VARIANT v = vfp_i4(1);
    BSTR out = NULL;
    VFP_NEED_FORMAT();
    EXPECT_EQ(U(VarFormat)(&v, NULL, 0, 0, 0, NULL), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(NULL, NULL, 0, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(&v, NULL, -1, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(&v, NULL, 8, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(&v, NULL, 0, -1, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(&v, NULL, 0, 4, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(&v, NULL, 0x7fffffff, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormat)(&v, NULL, 0, 0x7fffffff, 0, &out), VFP_E_INVALIDARG);
    CHECK(out == NULL);
    /* boundary values that are valid */
    EXPECT_EQ(U(VarFormat)(&v, NULL, 7, 3, 0, &out), S_OK);
    if (out) { U(SysFreeString)(out); out = NULL; }
    EXPECT_EQ(U(VarFormat)(&v, NULL, 0, 0, 0, &out), S_OK);
    if (out) U(SysFreeString)(out);
}
TEST(vfp_fmt_unsupported_variant_types)
{
    VARIANT v;
    BSTR out = NULL;
    HRESULT hr;
    VFP_NEED_FORMAT();
    VariantInit(&v);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = NULL;
    hr = U(VarFormat)(&v, NULL, 0, 0, 0, &out);
    CHECK(FAILED(hr));
    t_note("VarFormat(VT_UNKNOWN NULL) hr=0x%lx", (unsigned long)hr);
    if (out) U(SysFreeString)(out);
    out = NULL;
    V_VT(&v) = VT_ERROR; V_ERROR(&v) = DISP_E_PARAMNOTFOUND;
    hr = U(VarFormat)(&v, NULL, 0, 0, 0, &out);
    t_note("VarFormat(VT_ERROR) hr=0x%lx", (unsigned long)hr);
    if (out) U(SysFreeString)(out);
}
TEST(vfp_fmt_empty_variant)
{
    VARIANT v = vfp_empty();
    BSTR out = NULL;
    HRESULT hr;
    VFP_NEED_FORMAT();
    hr = U(VarFormat)(&v, NULL, 0, 0, 0, &out);
    t_note("VarFormat(VT_EMPTY, NULL) hr=0x%lx out=\"%s\"", (unsigned long)hr, vfp_n(out));
    if (out) { CHECK(vfp_bstr_ok(out)); U(SysFreeString)(out); }
}
TEST(vfp_fmt_byref_deref)
{
    double d = 1234.5;
    LONG l = 77;
    VARIANT v;
    VFP_NEED_FORMAT();
    VariantInit(&v); V_VT(&v) = VT_R8 | VT_BYREF; V_R8REF(&v) = &d;
    VFP_ROW(v, "#,##0.00", 0, 0, "1,234.50");
    V_VT(&v) = VT_I4 | VT_BYREF; V_I4REF(&v) = &l;
    VFP_ROW(v, "000", 0, 0, "077");
    VFP_ROW(v, "Standard", 0, 0, "77.00");
}
TEST(vfp_fmt_result_is_valid_bstr)
{
    VARIANT v = vfp_r8(3.5);
    BSTR out = NULL;
    VFP_NEED_FORMAT();
    EXPECT_EQ(U(VarFormat)(&v, (LPOLESTR)L"0.00", 0, 0, 0, &out), S_OK);
    CHECK(out != NULL);
    if (out) {
        EXPECT_EQ(((DWORD *)out)[-1], 8);
        EXPECT_EQ(U(SysStringLen)(out), 4);
        EXPECT_EQ(out[4], 0);
        U(SysFreeString)(out);
    }
}
TEST(vfp_fmt_does_not_modify_input)
{
    VARIANT v = vfp_r8(2.5);
    BSTR out = NULL;
    VFP_NEED_FORMAT();
    EXPECT_EQ(U(VarFormat)(&v, (LPOLESTR)L"0.0", 0, 0, 0, &out), S_OK);
    EXPECT_EQ(V_VT(&v), VT_R8);
    CHECK(V_R8(&v) == 2.5);
    if (out) U(SysFreeString)(out);
}
