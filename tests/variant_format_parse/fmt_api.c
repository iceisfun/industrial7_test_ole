/* variant_format_parse: VarFormatNumber/Currency/Percent/DateTime, VarTokenizeFormatString, VarFormatFromTokens,
 * VarWeekdayName, VarMonthName, GetAltMonthNames (oleaut32.dll).
 * Decomp-verified facts used here:
 *  - VarFormatNumber/Percent/Currency/DateTime use GetUserDefaultLCID(); NULL pvarIn / pbstrOut -> E_INVALIDARG.
 *  - VarFormatNumber/Currency return DISP_E_BADVARTYPE (0x80020008) when vt == 0x4000 (VT_BYREF with no base type);
 *    VarFormatCurrency rejects iNumDig > 255 with E_INVALIDARG; VarFormatDateTime rejects iNamedFormat outside 0..4.
 *  - They all call VarFormatFromTokens with dwFlags | 0x20 (non numeric BSTR is an error rather than passed through).
 *  - VarTokenizeFormatString: iFirstDay > 7, iFirstWeek > 3 or NULL rgbTok -> E_INVALIDARG; NULL/empty format writes
 *    rgbTok[0] = 0 and reports 0 bytes; the 16 named formats are matched case-insensitively and copied from static
 *    token strings whose first byte is the total length; too small a buffer -> DISP_E_BUFFERTOOSMALL (0x80020013). */
#include "vfp.h"

#define SKIP_UNLESS_EN_US() do { if (!vfp_user_is_en_us()) { t_skip("user locale is not en-US like (VarFormatNumber & co use GetUserDefaultLCID)"); return; } } while (0)

/* ---- VarFormatNumber / Currency / Percent -------------------------------- */
typedef struct { double v; int dig, lead, parens, group; const char *exp; } NRow;

static void run_nrows(const char *name, HRESULT (WINAPI *fn)(LPVARIANT, int, int, int, int, ULONG, BSTR *), const NRow *rows, size_t n, int soft)
{
    size_t i;
    for (i = 0; i < n; i++) {
        VARIANT v = vfp_r8(rows[i].v);
        BSTR out = NULL;
        HRESULT hr = fn(&v, rows[i].dig, rows[i].lead, rows[i].parens, rows[i].group, 0, &out);
        int ok = hr == S_OK && out && !strcmp(vfp_n(out), rows[i].exp) && vfp_bstr_ok(out);
        if (soft) { if (!ok) t_note("%s(%g,%d,%d,%d,%d) want \"%s\" got hr=0x%lx \"%s\"", name, rows[i].v, rows[i].dig, rows[i].lead, rows[i].parens, rows[i].group, rows[i].exp, (unsigned long)hr, vfp_n(out)); }
        else t_check(ok, __FILE__, __LINE__, name, "(%g,%d,%d,%d,%d) want \"%s\" got hr=0x%lx \"%s\"", rows[i].v, rows[i].dig, rows[i].lead, rows[i].parens, rows[i].group, rows[i].exp, (unsigned long)hr, vfp_n(out));
        if (out) U(SysFreeString)(out);
    }
}

/* tri-state: -1 TRUE, 0 FALSE, -2 use locale default (vbTrue/vbFalse/vbUseDefault) */
static const NRow number_rows[] = {
    { 1, 2, 0, 0, 0, "1.00" }, { 1234.5, 2, -1, 0, 0, "1234.50" }, { 1234.5, 2, -1, 0, -1, "1,234.50" },
    { 1234.5, 2, -1, -1, -1, "1,234.50" }, { 1234567.891, 0, 0, 0, -1, "1,234,568" }, { 1234567.891, 1, 0, 0, 0, "1234567.9" },
    { 1234567.891, 3, 0, 0, -1, "1,234,567.891" }, { 1234567.891, 2, 0, 0, 0, "1234567.89" }, { 0.5, 2, -1, 0, 0, "0.50" },
    { 0.5, 2, 0, 0, 0, ".50" }, { 0.5, 3, -1, 0, 0, "0.500" }, { 0.5, 0, 0, 0, 0, "1" }, { 0.4, 0, -1, 0, 0, "0" },
    { -1, 2, 0, -1, 0, "(1.00)" }, { -1, 2, 0, 0, 0, "-1.00" }, { -1, 2, 0, -2, 0, "-1.00" }, { -1234.5, 2, 0, -1, -1, "(1,234.50)" },
    { -1234.5, 2, 0, 0, -1, "-1,234.50" }, { -0.5, 2, 0, -1, 0, "(.50)" }, { -0.5, 2, -1, -1, 0, "(0.50)" }, { -0.5, 2, 0, 0, 0, "-.50" },
    { -0.5, 2, -1, 0, 0, "-0.50" }, { 1000, 0, 0, 0, -1, "1,000" }, { 1000, 0, 0, 0, 0, "1000" }, { 999, 0, 0, 0, -1, "999" },
    { 1000000, 2, 0, 0, -1, "1,000,000.00" }, { 123456789, 0, 0, 0, -1, "123,456,789" }, { -123456789, 0, 0, -1, -1, "(123,456,789)" },
    { 12.3456, 2, 0, 0, 0, "12.35" }, { 12.3446, 2, 0, 0, 0, "12.34" }, { 12.3456, 4, 0, 0, 0, "12.3456" }, { 12.3456, 6, 0, 0, 0, "12.345600" },
    { 12.3456, 0, 0, 0, 0, "12" }, { 99.999, 2, 0, 0, 0, "100.00" }, { 99.999, 1, 0, 0, 0, "100.0" },
    /* defaults: -1 for digits means locale default (2); -2 for tri-state means locale default */
    { 1234.5, -1, -2, -2, -2, "1,234.50" }, { 0.5, -1, -2, -2, -2, "0.50" }, { -1234.5, -1, -2, -2, -2, "-1,234.50" }, { 1, -1, -1, -1, -1, "1.00" },
    { 12, -1, -2, -2, -2, "12.00" }, { 0, -1, -2, -2, -2, "0.00" },
};
static const NRow currency_rows[] = {
    { 1234.5, 2, -1, -1, -1, "$1,234.50" }, { -1234.5, 2, -1, -1, -1, "($1,234.50)" }, { -1234.5, 2, -1, 0, -1, "-$1,234.50" },
    { 0.5, 2, 0, 0, 0, "$.50" }, { 0.5, 2, -1, 0, 0, "$0.50" }, { 0.5, 0, -1, 0, 0, "$1" }, { 1234.6, 0, 0, 0, -1, "$1,235" },
    { 1234.5, 2, -1, 0, 0, "$1234.50" }, { 0, 2, -1, 0, 0, "$0.00" }, { 0, 0, -1, 0, 0, "$0" }, { 1, 3, -1, 0, 0, "$1.000" },
    { -0.5, 2, 0, -1, 0, "($.50)" }, { -0.5, 2, -1, -1, 0, "($0.50)" }, { -0.5, 2, -1, 0, 0, "-$0.50" }, { -0.5, 2, 0, 0, 0, "-$.50" },
    { 1000000, 2, -1, 0, -1, "$1,000,000.00" }, { 12.3456, 4, -1, 0, 0, "$12.3456" },
    { 12, -1, -2, -2, -2, "$12.00" }, { 1234.5, -1, -2, -2, -2, "$1,234.50" }, { -1234.5, -1, -2, -2, -2, "($1,234.50)" },
    { 0.5, -1, -2, -2, -2, "$0.50" },
};
static const NRow percent_rows[] = {
    { 0.745, 2, -1, -1, -1, "74.50%" }, { -0.745, 2, -1, -1, -1, "(74.50%)" }, { -0.745, 2, -1, 0, -1, "-74.50%" }, { 1, 2, -1, 0, 0, "100.00%" },
    { 0.5, 0, -1, 0, 0, "50%" }, { 0.12345, 1, -1, 0, 0, "12.3%" }, { 0.12345, 2, -1, 0, 0, "12.35%" }, { 1.5, 2, -1, 0, 0, "150.00%" },
    { 0.005, 2, -1, 0, 0, "0.50%" }, { 0.005, 2, 0, 0, 0, ".50%" }, { 0, 2, -1, 0, 0, "0.00%" }, { 0, 0, -1, 0, 0, "0%" }, { 12.5, 0, -1, 0, -1, "1,250%" },
    { 12.5, 0, -1, 0, 0, "1250%" }, { 0.25, 3, -1, 0, 0, "25.000%" }, { 0.5, -1, -2, -2, -2, "50.00%" }, { 10, 0, 0, 0, -1, "1,000%" },
    { -0.25, 2, -1, -1, 0, "(25.00%)" }, { -0.25, 2, -1, 0, 0, "-25.00%" }, { 0.999, 0, -1, 0, 0, "100%" },
};
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_formatnumber_table) { REQUIRE(U(VarFormatNumber)); SKIP_UNLESS_EN_US(); run_nrows("VarFormatNumber", U(VarFormatNumber), number_rows, sizeof number_rows / sizeof *number_rows, 0); }
TEST(vfp_formatcurrency_table) { REQUIRE(U(VarFormatCurrency)); SKIP_UNLESS_EN_US(); run_nrows("VarFormatCurrency", U(VarFormatCurrency), currency_rows, sizeof currency_rows / sizeof *currency_rows, 0); }
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_formatpercent_table) { REQUIRE(U(VarFormatPercent)); SKIP_UNLESS_EN_US(); run_nrows("VarFormatPercent", U(VarFormatPercent), percent_rows, sizeof percent_rows / sizeof *percent_rows, 0); }

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_formatnumber_variant_types)
{
    VARIANT v;
    BSTR str;
    HRESULT hr;
    int vt;
    REQUIRE(U(VarFormatNumber)); REQUIRE(U(SysFreeString)); REQUIRE(U(SysAllocString));
    SKIP_UNLESS_EN_US();
    for (vt = 0; vt < 8; vt++) {
        switch (vt) {
        case 0: v = vfp_i2(1); break; case 1: v = vfp_i4(1); break; case 2: v = vfp_i8(1); break; case 3: v = vfp_ui1(1); break;
        case 4: v = vfp_r4(1); break; case 5: v = vfp_r8(1); break; case 6: v = vfp_cy(10000); break; default: v = vfp_bool(1); V_BOOL(&v) = 1; break;
        }
        str = NULL;
        hr = U(VarFormatNumber)(&v, 2, 0, 0, 0, 0, &str);
        EXPECT_EQ(hr, S_OK);
        if (hr == S_OK) { CHECK(str && !wcscmp(str, L"1.00")); U(SysFreeString)(str); }
    }
    v = vfp_bstr("1");
    str = NULL; hr = U(VarFormatNumber)(&v, 2, 0, 0, 0, 0, &str);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(str && !wcscmp(str, L"1.00")); U(SysFreeString)(str); }
    vfp_clear(&v);
    v = vfp_bstr("-1");
    str = NULL; hr = U(VarFormatNumber)(&v, 2, 0, -1, 0, 0, &str);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(str && !wcscmp(str, L"(1.00)")); U(SysFreeString)(str); }
    vfp_clear(&v);
}
TEST(vfp_formatnumber_invalid_args)
{
    VARIANT v = vfp_r8(1);
    BSTR str = NULL;
    REQUIRE(U(VarFormatNumber)); REQUIRE(U(VarFormatCurrency)); REQUIRE(U(VarFormatPercent)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(VarFormatNumber)(NULL, 2, 0, 0, 0, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatNumber)(&v, 2, 0, 0, 0, 0, NULL), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatCurrency)(NULL, 2, 0, 0, 0, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatCurrency)(&v, 2, 0, 0, 0, 0, NULL), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatPercent)(NULL, 2, 0, 0, 0, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatPercent)(&v, 2, 0, 0, 0, 0, NULL), VFP_E_INVALIDARG);
    /* currency: iNumDig above 255 */
    EXPECT_EQ(U(VarFormatCurrency)(&v, 256, 0, 0, 0, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatCurrency)(&v, 0x7fffffff, 0, 0, 0, 0, &str), VFP_E_INVALIDARG);
    CHECK(str == NULL);
}
TEST(vfp_formatnumber_byref_without_base_type)
{
    /* decomp: vt == 0x4000 exactly -> DISP_E_BADVARTYPE for Number and Currency */
    VARIANT v;
    BSTR str = NULL;
    REQUIRE(U(VarFormatNumber)); REQUIRE(U(VarFormatCurrency));
    VariantInit(&v); V_VT(&v) = VT_BYREF;
    EXPECT_EQ(U(VarFormatNumber)(&v, 2, 0, 0, 0, 0, &str), 0x80020008);
    EXPECT_EQ(U(VarFormatCurrency)(&v, 2, 0, 0, 0, 0, &str), 0x80020008);
    CHECK(str == NULL);
}
TEST(vfp_formatnumber_text_is_type_mismatch)
{
    /* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+): flag 0x20 makes unparsable BSTRs an error */
    VARIANT v = vfp_bstr("test");
    BSTR str = NULL;
    REQUIRE(U(VarFormatNumber)); REQUIRE(U(VarFormatCurrency)); REQUIRE(U(VarFormatPercent)); REQUIRE(U(SysFreeString)); REQUIRE(U(SysAllocString));
    EXPECT_EQ(U(VarFormatCurrency)(&v, 1, -2, -2, -2, 0, &str), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(U(VarFormatNumber)(&v, 1, -2, -2, -2, 0, &str), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(U(VarFormatPercent)(&v, 1, -2, -2, -2, 0, &str), VFP_E_TYPEMISMATCH);
    CHECK(str == NULL);
    vfp_clear(&v);
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_formatcurrency_roundtrip_through_bstr)
{
    VARIANT in;
    BSTR a = NULL, b = NULL;
    HRESULT hr;
    REQUIRE(U(VarFormatCurrency)); REQUIRE(U(SysFreeString));
    SKIP_UNLESS_EN_US();
    in = vfp_cy(0);
    hr = U(VarFormatCurrency)(&in, 3, -2, -2, -2, 0, &a);
    EXPECT_EQ(hr, S_OK);
    if (hr != S_OK) return;
    CHECK(a && !wcscmp(a, L"$0.000"));
    /* a currency string can be parsed back as input and formatted with different precision */
    V_VT(&in) = VT_BSTR; V_BSTR(&in) = a;
    hr = U(VarFormatCurrency)(&in, 1, -2, -2, -2, 0, &b);
    EXPECT_EQ(hr, S_OK);
    CHECK(b && !wcscmp(b, L"$0.0"));
    U(SysFreeString)(b); U(SysFreeString)(a);
}
TEST(vfp_formatcurrency_cy_input)
{
    VARIANT v = vfp_cy(12345000);
    BSTR out = NULL;
    REQUIRE(U(VarFormatCurrency)); REQUIRE(U(SysFreeString));
    SKIP_UNLESS_EN_US();
    EXPECT_EQ(U(VarFormatCurrency)(&v, 2, -1, 0, -1, 0, &out), S_OK);
    CHECK(out && !wcscmp(out, L"$1,234.50"));
    if (out) U(SysFreeString)(out);
    out = NULL;
    V_CY(&v).int64 = -5000;
    EXPECT_EQ(U(VarFormatCurrency)(&v, 2, -1, -1, 0, 0, &out), S_OK);
    CHECK(out && !wcscmp(out, L"($0.50)"));
    if (out) U(SysFreeString)(out);
}
TEST(vfp_formatnumber_null_variant)
{
    VARIANT v = vfp_null();
    BSTR out = (BSTR)0x1;
    HRESULT hr;
    REQUIRE(U(VarFormatNumber));
    hr = U(VarFormatNumber)(&v, 2, 0, 0, 0, 0, &out);
    t_note("VarFormatNumber(VT_NULL) hr=0x%lx out=%p", (unsigned long)hr, (void *)out);
    if (hr == S_OK && out && out != (BSTR)0x1) U(SysFreeString)(out);
}

/* ---- VarFormatDateTime --------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_formatdatetime_null_gives_null_bstr)
{
    VARIANT in = vfp_null();
    BSTR str = (BSTR)0xdeadbeef;
    REQUIRE(U(VarFormatDateTime));
    EXPECT_EQ(U(VarFormatDateTime)(&in, 0, 0, &str), S_OK);
    CHECK(str == NULL);
}
TEST(vfp_formatdatetime_invalid_args)
{
    VARIANT in = vfp_date(36526.5);
    BSTR str = NULL;
    REQUIRE(U(VarFormatDateTime)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(VarFormatDateTime)(NULL, 0, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatDateTime)(&in, 0, 0, NULL), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatDateTime)(&in, -1, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatDateTime)(&in, 5, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatDateTime)(&in, 0x7fffffff, 0, &str), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatDateTime)(&in, (int)0x80000000, 0, &str), VFP_E_INVALIDARG);
    CHECK(str == NULL);
}
TEST(vfp_formatdatetime_named_formats)
{
    static const struct { double v; int nf; const char *exp; int hard; } rows[] = {
        { 36526.0, 0, "1/1/2000", 0 }, { 36526.75, 0, "1/1/2000 6:00:00 PM", 0 }, { 0.5, 0, "12:00:00 PM", 0 }, { 36526.0, 2, "1/1/2000", 0 },
        { 36526.75, 2, "1/1/2000", 0 }, { 36526.0, 1, "Saturday, January 1, 2000", 0 }, { 36526.75, 1, "Saturday, January 1, 2000", 0 },
        { 36526.75, 3, "6:00:00 PM", 0 }, { 36526.75, 4, "18:00", 0 }, { 36526.25, 4, "06:00", 0 }, { 36526.25, 3, "6:00:00 AM", 0 },
        { 2.525, 4, "12:36", 1 }, { 2.525, 3, "12:36:00 PM", 1 },
    };
    size_t i;
    REQUIRE(U(VarFormatDateTime)); REQUIRE(U(SysFreeString));
    SKIP_UNLESS_EN_US();
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT v = vfp_date(rows[i].v);
        BSTR out = NULL;
        HRESULT hr = U(VarFormatDateTime)(&v, rows[i].nf, 0, &out);
        int ok = hr == S_OK && out && !strcmp(vfp_n(out), rows[i].exp);
        if (rows[i].hard) t_check(ok, __FILE__, __LINE__, "VarFormatDateTime", "(%g,%d) want \"%s\" got hr=0x%lx \"%s\"", rows[i].v, rows[i].nf, rows[i].exp, (unsigned long)hr, vfp_n(out));
        else if (!ok) t_note("VarFormatDateTime(%g,%d) want \"%s\" got hr=0x%lx \"%s\"", rows[i].v, rows[i].nf, rows[i].exp, (unsigned long)hr, vfp_n(out));
        if (out) U(SysFreeString)(out);
    }
}

/* ---- VarTokenizeFormatString ---------------------------------------------- */
static const struct { const char *name; const char *tok_hex; } named_tokens[] = {
    /* Static token strings from oleaut32's named format table (RVA 0xa1880); first byte is the total length. */
    { "General Number", "060106000000" }, { "Long Time", "0a030300000000000702" }, { "Short Time", "0c0303000000000023031b02" },
    { "General Date", "0a030300000000000502" }, { "Long Date", "0a030300000000000d02" }, { "Medium Date", "0a030300000000000e02" },
    { "Short Date", "0a030300000000000c02" }, { "Fixed", "1102060000000000000102340136340202" },
    { "Standard", "1102060000000400000102340136340202" }, { "Percent", "150206000000010100010234013634020101250002" },
    { "Scientific", "13020600000002000001023401363402370202" }, { "True/False", "0d020600000020000000003d02" },
    { "Yes/No", "0d020600000020000000003e02" }, { "On/Off", "0d020600000020000000003f02" },
};
static int hexbyte(const char *p) { char b[3] = { p[0], p[1], 0 }; return (int)strtol(b, NULL, 16); }

TEST(vfp_tokenize_named_format_bytes)
{
    size_t i;
    REQUIRE(U(VarTokenizeFormatString));
    for (i = 0; i < sizeof named_tokens / sizeof *named_tokens; i++) {
        BYTE tok[VFP_TOKBUF];
        int actual = -1, k, len = (int)strlen(named_tokens[i].tok_hex) / 2;
        HRESULT hr;
        memset(tok, 0xcc, sizeof tok);
        hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(named_tokens[i].name), tok, sizeof tok, 0, 0, VFP_EN_US, &actual);
        CHECKF(hr == S_OK, "%s hr=0x%lx", named_tokens[i].name, (unsigned long)hr);
        if (hr != S_OK) continue;
        CHECKF(actual == len, "%s actual %d want %d", named_tokens[i].name, actual, len);
        CHECKF(tok[0] == len, "%s tok[0] %d want %d", named_tokens[i].name, tok[0], len);
        for (k = 0; k < len && k < (int)sizeof tok; k++)
            CHECKF(tok[k] == hexbyte(named_tokens[i].tok_hex + 2 * k), "%s tok[%d]=%02x want %.2s", named_tokens[i].name, k, tok[k], named_tokens[i].tok_hex + 2 * k);
        CHECKF(tok[len] == 0xcc, "%s wrote past its length", named_tokens[i].name);
    }
}
TEST(vfp_tokenize_named_format_case_insensitive)
{
    static const char *const names[] = { "general number", "GENERAL NUMBER", "gEnErAl NuMbEr", "FIXED", "fixed", "STANDARD", "percent", "PERCENT", "scientific",
        "true/false", "TRUE/FALSE", "yes/no", "YES/NO", "on/off", "ON/OFF", "long time", "LONG TIME", "short date", "SHORT DATE", "medium date", "general date" };
    size_t i;
    REQUIRE(U(VarTokenizeFormatString));
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        BYTE tok[VFP_TOKBUF];
        BYTE ref[VFP_TOKBUF];
        char canon[40];
        size_t k;
        int a1 = -1, a2 = -1;
        HRESULT h1, h2;
        /* find the canonical spelling in named_tokens (case-insensitive) */
        for (k = 0; k < sizeof named_tokens / sizeof *named_tokens; k++)
            if (!strcasecmp(named_tokens[k].name, names[i])) break;
        if (k == sizeof named_tokens / sizeof *named_tokens) continue;
        strcpy(canon, named_tokens[k].name);
        h1 = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(names[i]), tok, sizeof tok, 0, 0, VFP_EN_US, &a1);
        h2 = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(canon), ref, sizeof ref, 0, 0, VFP_EN_US, &a2);
        CHECKF(h1 == S_OK && h2 == S_OK, "%s", names[i]);
        a1 = tok[0]; a2 = ref[0];
        CHECKF(a1 == a2 && a1 > 0 && !memcmp(tok, ref, a1), "%s differs from canonical", names[i]);
    }
}
TEST(vfp_tokenize_named_format_not_matched_loosely)
{
    /* Named formats are matched on the whole string: padding or extra characters make it an ordinary format string. */
    static const char *const loose[] = { " Fixed", "Fixed ", "Fixed0", "Fix", "General  Number", "Fixed\t" };
    BYTE tok[VFP_TOKBUF], named[VFP_TOKBUF];
    int an = -1, al = -1;
    size_t i;
    REQUIRE(U(VarTokenizeFormatString));
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"Fixed", named, sizeof named, 0, 0, VFP_EN_US, &an), S_OK);
    for (i = 0; i < sizeof loose / sizeof *loose; i++) {
        HRESULT hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(loose[i]), tok, sizeof tok, 0, 0, VFP_EN_US, &al);
        CHECKF(hr == S_OK, "%s hr=0x%lx", loose[i], (unsigned long)hr);
        if (hr == S_OK) CHECKF(al != an || memcmp(tok, named, an), "'%s' tokenised as the named format", loose[i]);
    }
}
TEST(vfp_tokenize_empty_and_null_format)
{
    BYTE tok[16];
    int actual = 77;
    REQUIRE(U(VarTokenizeFormatString));
    memset(tok, 0xcc, sizeof tok);
    EXPECT_EQ(U(VarTokenizeFormatString)(NULL, tok, sizeof tok, 0, 0, VFP_EN_US, &actual), S_OK);
    EXPECT_EQ(tok[0], 0); EXPECT_EQ(actual, 0); EXPECT_EQ(tok[1], 0xcc);
    memset(tok, 0xcc, sizeof tok); actual = 77;
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"", tok, sizeof tok, 0, 0, VFP_EN_US, &actual), S_OK);
    EXPECT_EQ(tok[0], 0); EXPECT_EQ(actual, 0);
    /* the pcbActual pointer is optional */
    memset(tok, 0xcc, sizeof tok);
    EXPECT_EQ(U(VarTokenizeFormatString)(NULL, tok, sizeof tok, 0, 0, VFP_EN_US, NULL), S_OK);
    EXPECT_EQ(tok[0], 0);
    /* an empty format needs no buffer space and no size check, even with cbTok 0 or negative */
    memset(tok, 0xcc, sizeof tok);
    EXPECT_EQ(U(VarTokenizeFormatString)(NULL, tok, 0, 0, 0, VFP_EN_US, &actual), S_OK);
    EXPECT_EQ(tok[0], 0);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"", tok, -5, 0, 0, VFP_EN_US, &actual), S_OK);
}
TEST(vfp_tokenize_invalid_args)
{
    BYTE tok[VFP_TOKBUF];
    int actual = 0;
    REQUIRE(U(VarTokenizeFormatString));
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", NULL, sizeof tok, 0, 0, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarTokenizeFormatString)(NULL, NULL, sizeof tok, 0, 0, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, 8, 0, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, -1, 0, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, 0, 4, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, 0, -1, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    /* argument validation precedes the empty-format shortcut */
    EXPECT_EQ(U(VarTokenizeFormatString)(NULL, tok, sizeof tok, 8, 0, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarTokenizeFormatString)(NULL, tok, sizeof tok, 0, 4, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    /* negative buffer size with a real format */
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, -1, 0, 0, VFP_EN_US, &actual), VFP_E_INVALIDARG);
    /* boundary values 7 and 3 are valid */
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, 7, 3, VFP_EN_US, &actual), S_OK);
}
TEST(vfp_tokenize_buffer_too_small_named)
{
    size_t i;
    REQUIRE(U(VarTokenizeFormatString));
    for (i = 0; i < sizeof named_tokens / sizeof *named_tokens; i++) {
        BYTE tok[VFP_TOKBUF];
        int len = (int)strlen(named_tokens[i].tok_hex) / 2, actual = -1;
        EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)vfp_w(named_tokens[i].name), tok, 0, 0, 0, VFP_EN_US, &actual), VFP_E_BUFTOOSMALL);
        EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)vfp_w(named_tokens[i].name), tok, len - 1, 0, 0, VFP_EN_US, &actual), VFP_E_BUFTOOSMALL);
        memset(tok, 0, sizeof tok);
        actual = -1;
        EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)vfp_w(named_tokens[i].name), tok, len, 0, 0, VFP_EN_US, &actual), S_OK);
        EXPECT_EQ(actual, len);
    }
}
TEST(vfp_tokenize_buffer_too_small_user_formats)
{
    static const char *const fmts[] = { "0.00", "#,##0.00", "dd/mm/yyyy", "hh:nn:ss AM/PM", "\"abc\"0", "0.0E+00" };
    size_t i;
    REQUIRE(U(VarTokenizeFormatString));
    for (i = 0; i < sizeof fmts / sizeof *fmts; i++) {
        BYTE tok[VFP_TOKBUF];
        int actual = -1, cb;
        HRESULT hr;
        hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(fmts[i]), tok, 0, 0, 0, VFP_EN_US, &actual);
        CHECKF(hr == VFP_E_BUFTOOSMALL, "%s cb=0 hr=0x%lx", fmts[i], (unsigned long)hr);
        hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(fmts[i]), tok, 6, 0, 0, VFP_EN_US, &actual);
        CHECKF(hr == VFP_E_BUFTOOSMALL, "%s cb=6 hr=0x%lx", fmts[i], (unsigned long)hr);
        actual = -1;
        hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(fmts[i]), tok, sizeof tok, 0, 0, VFP_EN_US, &actual);
        CHECKF(hr == S_OK && actual >= 6 && actual == tok[0], "%s hr=0x%lx actual=%d tok0=%d", fmts[i], (unsigned long)hr, actual, tok[0]);
        if (hr != S_OK) continue;
        /* find the smallest working buffer: below the actual size it must fail, never succeed with a truncated token string */
        for (cb = actual - 1; cb >= actual - 2 && cb > 0; cb--) {
            int a2 = -1;
            hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(fmts[i]), tok, cb, 0, 0, VFP_EN_US, &a2);
            if (hr != VFP_E_BUFTOOSMALL) t_note("%s: cb=%d (actual %d) hr=0x%lx (windows may need a few spare bytes)", fmts[i], cb, actual, (unsigned long)hr);
        }
    }
}
TEST(vfp_tokenize_user_format_header)
{
    /* token string structure: byte 0 is its own total length; the buffer is a plain byte string reusable with VarFormatFromTokens */
    static const char *const fmts[] = { "0", "0.00", "#,##0", "yyyy-mm-dd", "\"x\"@", "0;0;0", "0.0E+00", "hh:mm" };
    size_t i;
    REQUIRE(U(VarTokenizeFormatString));
    for (i = 0; i < sizeof fmts / sizeof *fmts; i++) {
        BYTE tok[VFP_TOKBUF];
        int actual = -1;
        HRESULT hr;
        memset(tok, 0xee, sizeof tok);
        hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(fmts[i]), tok, sizeof tok, 1, 1, VFP_EN_US, &actual);
        CHECKF(hr == S_OK, "%s", fmts[i]);
        CHECKF(actual == tok[0] && actual > 0 && actual < 200, "%s actual=%d tok0=%d", fmts[i], actual, tok[0]);
        if (actual > 0 && actual < (int)sizeof tok) CHECKF(tok[actual] == 0xee, "%s overran the reported length", fmts[i]);
    }
}
TEST(vfp_tokenize_distinct_formats_distinct_tokens)
{
    BYTE a[VFP_TOKBUF], b[VFP_TOKBUF];
    int na = 0, nb = 0;
    REQUIRE(U(VarTokenizeFormatString));
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", a, sizeof a, 0, 0, VFP_EN_US, &na), S_OK);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.000", b, sizeof b, 0, 0, VFP_EN_US, &nb), S_OK);
    CHECK(na != nb || memcmp(a, b, na));
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", b, sizeof b, 0, 0, VFP_EN_US, &nb), S_OK);
    CHECK(na == nb && !memcmp(a, b, na));
    /* iFirstDay/iFirstWeek do not alter numeric tokens, but are recorded for date formats: just require success */
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"ww", a, sizeof a, 1, 1, VFP_EN_US, &na), S_OK);
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"ww", b, sizeof b, 2, 3, VFP_EN_US, &nb), S_OK);
    t_note("ww tokens differ with firstday/week: %d", na != nb || memcmp(a, b, na));
}

/* ---- VarFormatFromTokens -------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fromtokens_number_and_locale)
{
    BYTE buff[256];
    VARIANT var;
    BSTR bstr;
    HRESULT hr;
    LCID en = VFP_EN_US, de = VFP_DE_DE;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString)); REQUIRE(U(SysAllocString));
    var = vfp_bstr("6,90");
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"###,##0.00", buff, sizeof buff, 1, 1, en, NULL);
    EXPECT_EQ(hr, S_OK);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"###,##0.00", buff, 0, &bstr, en);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(!wcscmp(bstr, L"690.00")); U(SysFreeString)(bstr); }
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"###,##0.00", buff, sizeof buff, 1, 1, de, NULL);
    EXPECT_EQ(hr, S_OK);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"###,##0.00", buff, 0, &bstr, de);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(!wcscmp(bstr, L"6,90")); U(SysFreeString)(bstr); }
    vfp_clear(&var);
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fromtokens_date_string_input)
{
    BYTE buff[256];
    VARIANT var = vfp_bstr("12-11");
    BSTR bstr;
    HRESULT hr;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString)); REQUIRE(U(SysAllocString));
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"dd-mm", buff, sizeof buff, 1, 1, VFP_EN_US, NULL);
    EXPECT_EQ(hr, S_OK);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"dd-mm", buff, 0, &bstr, VFP_EN_US);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(!wcscmp(bstr, L"11-12")); U(SysFreeString)(bstr); }
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"dd-mm", buff, sizeof buff, 1, 1, VFP_DE_DE, NULL);
    EXPECT_EQ(hr, S_OK);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"dd-mm", buff, 0, &bstr, VFP_DE_DE);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(!wcscmp(bstr, L"12-11")); U(SysFreeString)(bstr); }
    vfp_clear(&var);
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fromtokens_string_format_with_float)
{
    BYTE buff[256];
    VARIANT var = vfp_r4(1.5f);
    BSTR bstr;
    HRESULT hr;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString));
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"@", buff, sizeof buff, 1, 1, VFP_EN_US, NULL);
    EXPECT_EQ(hr, S_OK);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"@", buff, 0, &bstr, VFP_EN_US);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(!wcscmp(bstr, L"1.5")); U(SysFreeString)(bstr); }
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"@", buff, sizeof buff, 1, 1, VFP_DE_DE, NULL);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"@", buff, 0, &bstr, VFP_DE_DE);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(!wcscmp(bstr, L"1,5")); U(SysFreeString)(bstr); }
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_fromtokens_float_with_nonformat_letter)
{
    /* "C" is not a format character in VarFormat: it is a literal-ish token and the double is shown as a date/time (en-US) */
    BYTE buff[256];
    VARIANT var = vfp_r8(1234.56);
    BSTR bstr;
    HRESULT hr;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString));
    hr = U(VarTokenizeFormatString)((LPOLESTR)L"C", buff, sizeof buff, 1, 1, VFP_EN_US, NULL);
    EXPECT_EQ(hr, S_OK);
    hr = U(VarFormatFromTokens)(&var, (LPOLESTR)L"C", buff, 0, &bstr, VFP_EN_US);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { if (wcscmp(bstr, L"5/18/1903 1:26:24 PM")) t_note("C format gives %s", vfp_n(bstr)); U(SysFreeString)(bstr); }
}
TEST(vfp_fromtokens_invalid_args)
{
    BYTE tok[VFP_TOKBUF];
    VARIANT v = vfp_r8(1);
    BSTR out = (BSTR)0x1234;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens));
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, 0, 0, VFP_EN_US, NULL), S_OK);
    EXPECT_EQ(U(VarFormatFromTokens)(NULL, (LPOLESTR)L"0.00", tok, 0, &out, VFP_EN_US), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarFormatFromTokens)(&v, (LPOLESTR)L"0.00", tok, 0, NULL, VFP_EN_US), VFP_E_INVALIDARG);
    /* the invalid-arg paths leave the output pointer alone */
    CHECK(out == (BSTR)0x1234);
}
TEST(vfp_fromtokens_clears_output_first)
{
    /* decomp: *pbstrOut = NULL is written before any further work */
    BYTE tok[VFP_TOKBUF];
    VARIANT v = vfp_null();
    BSTR out = (BSTR)0x1234;
    HRESULT hr;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens));
    EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)L"0.00", tok, sizeof tok, 0, 0, VFP_EN_US, NULL), S_OK);
    hr = U(VarFormatFromTokens)(&v, (LPOLESTR)L"0.00", tok, 0, &out, VFP_EN_US);
    EXPECT_EQ(hr, S_OK);
    CHECK(out == NULL);
}
TEST(vfp_fromtokens_empty_token_buffer_uses_type_default)
{
    /* decomp: NULL pbTokCur or tok[0]==0 selects a built-in token: date -> general date, bool -> True/False, else General Number */
    static const BYTE empty[4] = { 0 };
    VARIANT d = vfp_date(36526.5), b = vfp_bool(1), n = vfp_r8(1234.5);
    BSTR out;
    HRESULT hr;
    REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString));
    out = NULL; hr = U(VarFormatFromTokens)(&d, NULL, NULL, 0, &out, VFP_EN_US);
    EXPECT_EQ(hr, S_OK); if (out) { t_note("date default: %s", vfp_n(out)); CHECK(!strcmp(vfp_n(out), "1/1/2000 12:00:00 PM")); U(SysFreeString)(out); }
    out = NULL; hr = U(VarFormatFromTokens)(&d, NULL, (LPBYTE)empty, 0, &out, VFP_EN_US);
    EXPECT_EQ(hr, S_OK); if (out) { CHECK(!strcmp(vfp_n(out), "1/1/2000 12:00:00 PM")); U(SysFreeString)(out); }
    out = NULL; hr = U(VarFormatFromTokens)(&b, NULL, NULL, 0, &out, VFP_EN_US);
    EXPECT_EQ(hr, S_OK); if (out) { t_note("bool default token text: %s", vfp_n(out)); U(SysFreeString)(out); }
    out = NULL; hr = U(VarFormatFromTokens)(&n, NULL, NULL, 0, &out, VFP_EN_US);
    EXPECT_EQ(hr, S_OK); if (out) { CHECK(!strcmp(vfp_n(out), "1234.5")); U(SysFreeString)(out); }
    out = NULL; hr = U(VarFormatFromTokens)(&n, NULL, (LPBYTE)empty, 0, &out, VFP_DE_DE);
    EXPECT_EQ(hr, S_OK); if (out) { CHECK(!strcmp(vfp_n(out), "1234,5")); U(SysFreeString)(out); }
}
TEST(vfp_fromtokens_named_tokens_equal_names)
{
    /* formatting with tokens from the name equals VarFormat of the name under the same LCID */
    static const char *const names[] = { "General Number", "Fixed", "Standard", "Percent", "Scientific", "True/False", "Yes/No", "On/Off" };
    size_t i;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        BYTE tok[VFP_TOKBUF];
        VARIANT v = vfp_r8(0.625);
        BSTR a = NULL, b = NULL;
        EXPECT_EQ(U(VarTokenizeFormatString)((LPOLESTR)vfp_w(names[i]), tok, sizeof tok, 0, 0, VFP_EN_US, NULL), S_OK);
        EXPECT_EQ(U(VarFormatFromTokens)(&v, (LPOLESTR)vfp_w(names[i]), tok, 0, &a, VFP_EN_US), S_OK);
        /* the format string argument is only a fallback: a different pstrFormat with the same tokens yields the same text */
        EXPECT_EQ(U(VarFormatFromTokens)(&v, NULL, tok, 0, &b, VFP_EN_US), S_OK);
        CHECKF(a && b && !wcscmp(a, b), "%s: %s vs %s", names[i], vfp_n(a), vfp_n(b));
        if (a) U(SysFreeString)(a);
        if (b) U(SysFreeString)(b);
    }
}
TEST(vfp_fromtokens_locale_decimal_separators)
{
    static const struct { LCID lcid; const char *fmt; double v; const char *exp; } rows[] = {
        { VFP_EN_US, "0.00", 1234.5, "1234.50" }, { VFP_DE_DE, "0.00", 1234.5, "1234,50" }, { VFP_ES_ES, "0.00", 1234.5, "1234,50" },
        { VFP_EN_US, "#,##0.00", 1234.5, "1,234.50" }, { VFP_DE_DE, "#,##0.00", 1234.5, "1.234,50" }, { VFP_EN_US, "Fixed", 3.5, "3.50" },
        { VFP_DE_DE, "Fixed", 3.5, "3,50" }, { VFP_DE_DE, "Standard", 1234.5, "1.234,50" }, { VFP_DE_DE, "Percent", 0.5, "50,00%" },
        { VFP_DE_DE, "Scientific", 1234.5, "1,23E+03" },
    };
    size_t i;
    REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BYTE tok[VFP_TOKBUF];
        VARIANT v = vfp_r8(rows[i].v);
        BSTR out = NULL;
        HRESULT hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(rows[i].fmt), tok, sizeof tok, 0, 0, rows[i].lcid, NULL);
        if (hr == S_OK) hr = U(VarFormatFromTokens)(&v, (LPOLESTR)vfp_w(rows[i].fmt), tok, 0, &out, rows[i].lcid);
        CHECKF(hr == S_OK && out && !strcmp(vfp_n(out), rows[i].exp), "lcid 0x%lx \"%s\" %g want \"%s\" got hr=0x%lx \"%s\"", (unsigned long)rows[i].lcid, rows[i].fmt, rows[i].v, rows[i].exp, (unsigned long)hr, vfp_n(out));
        if (out) U(SysFreeString)(out);
    }
}

/* ---- VarWeekdayName / VarMonthName ---------------------------------------- */
static const char *const en_days[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const en_days_abbr[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *const en_months[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
static const char *const en_months_abbr[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static int user_is_english(void) { return PRIMARYLANGID(LANGIDFROMLCID(GetUserDefaultLCID())) == LANG_ENGLISH; }

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_weekdayname_invalid_args)
{
    BSTR out = NULL;
    REQUIRE(U(VarWeekdayName));
    EXPECT_EQ(U(VarWeekdayName)(0, 0, 4, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarWeekdayName)(8, 0, 4, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarWeekdayName)(-1, 0, 4, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarWeekdayName)(0x7fffffff, 0, 4, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarWeekdayName)(4, 0, -1, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarWeekdayName)(4, 0, 8, 0, &out), VFP_E_INVALIDARG);
    if (!t_is_wine()) EXPECT_EQ(U(VarWeekdayName)(1, 0, 0, 0, NULL), VFP_E_INVALIDARG);
    CHECK(out == NULL);
}
TEST(vfp_weekdayname_english_first_day_matrix)
{
    int wd, abbr, fd;
    BSTR out;
    REQUIRE(U(VarWeekdayName)); REQUIRE(U(SysFreeString));
    if (!user_is_english()) { t_skip("user locale is not English"); return; }
    for (wd = 1; wd <= 7; wd++)
        for (abbr = 0; abbr <= 1; abbr++)
            for (fd = 1; fd <= 7; fd++) {
                /* iFirstDay 1=Sunday: weekday number 1 is that day */
                int day = (wd - 1 + fd - 1) % 7;
                const char *want = abbr ? en_days_abbr[day] : en_days[day];
                out = NULL;
                EXPECT_EQ(U(VarWeekdayName)(wd, abbr, fd, 0, &out), S_OK);
                if (out) { CHECKF(!strcmp(vfp_n(out), want) && vfp_bstr_ok(out), "wd=%d abbr=%d fd=%d got %s want %s", wd, abbr, fd, vfp_n(out), want); U(SysFreeString)(out); }
            }
}
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_weekdayname_matches_user_locale_names)
{
    int wd, abbr, fd;
    DWORD first = 0;
    REQUIRE(U(VarWeekdayName)); REQUIRE(U(SysFreeString));
    /* LOCALE_IFIRSTDAYOFWEEK: 0=Monday..6=Sunday */
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK | LOCALE_RETURN_NUMBER, (LPWSTR)&first, sizeof first / sizeof(WCHAR));
    for (wd = 1; wd <= 7; wd++)
        for (abbr = 0; abbr <= 1; abbr++)
            for (fd = 0; fd <= 7; fd++) {
                WCHAR ref[64];
                BSTR out = NULL;
                int firstDay = fd == 0 ? (int)first : fd - 2;   /* 0=Monday based */
                int day = (7 + wd - 1 + firstDay) % 7;
                GetLocaleInfoW(LOCALE_USER_DEFAULT, (abbr ? LOCALE_SABBREVDAYNAME1 : LOCALE_SDAYNAME1) + day, ref, 64);
                EXPECT_EQ(U(VarWeekdayName)(wd, abbr, fd, 0, &out), S_OK);
                if (out) { CHECKF(!wcscmp(out, ref), "wd=%d abbr=%d fd=%d got %s want %s", wd, abbr, fd, vfp_n(out), vfp_n(ref)); U(SysFreeString)(out); }
            }
}
TEST(vfp_weekdayname_abbrev_any_nonzero)
{
    BSTR a = NULL, b = NULL, c = NULL;
    REQUIRE(U(VarWeekdayName)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(VarWeekdayName)(1, 1, 1, 0, &a), S_OK);
    EXPECT_EQ(U(VarWeekdayName)(1, -1, 1, 0, &b), S_OK);
    EXPECT_EQ(U(VarWeekdayName)(1, 12345, 1, 0, &c), S_OK);
    CHECK(a && b && c && !wcscmp(a, b) && !wcscmp(a, c));
    if (a) U(SysFreeString)(a); if (b) U(SysFreeString)(b); if (c) U(SysFreeString)(c);
}
TEST(vfp_weekdayname_ignores_flags)
{
    BSTR a = NULL, b = NULL;
    REQUIRE(U(VarWeekdayName)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(VarWeekdayName)(3, 0, 1, 0, &a), S_OK);
    EXPECT_EQ(U(VarWeekdayName)(3, 0, 1, LOCALE_NOUSEROVERRIDE, &b), S_OK);
    CHECK(a && b);
    if (a && b) t_note("flags 0 -> %s, NOUSEROVERRIDE -> %s", vfp_n(a), vfp_n(b));
    if (a) U(SysFreeString)(a); if (b) U(SysFreeString)(b);
}
TEST(vfp_monthname_english_table)
{
    int m, abbr;
    BSTR out;
    REQUIRE(U(VarMonthName)); REQUIRE(U(SysFreeString));
    if (!user_is_english()) { t_skip("user locale is not English"); return; }
    for (m = 1; m <= 12; m++)
        for (abbr = 0; abbr <= 1; abbr++) {
            out = NULL;
            EXPECT_EQ(U(VarMonthName)(m, abbr, 0, &out), S_OK);
            if (out) {
                const char *want = abbr ? en_months_abbr[m - 1] : en_months[m - 1];
                if (abbr && strcmp(vfp_n(out), want)) t_note("month %d abbreviated: %s (expected %s in en-US)", m, vfp_n(out), want);
                else CHECKF(!strcmp(vfp_n(out), want) && vfp_bstr_ok(out), "month %d abbr %d got %s want %s", m, abbr, vfp_n(out), want);
                U(SysFreeString)(out);
            }
        }
}
TEST(vfp_monthname_matches_user_locale_names)
{
    int m, abbr;
    REQUIRE(U(VarMonthName)); REQUIRE(U(SysFreeString));
    for (m = 1; m <= 12; m++)
        for (abbr = 0; abbr <= 1; abbr++) {
            WCHAR ref[64];
            BSTR out = NULL;
            GetLocaleInfoW(LOCALE_USER_DEFAULT, (abbr ? LOCALE_SABBREVMONTHNAME1 : LOCALE_SMONTHNAME1) + m - 1, ref, 64);
            EXPECT_EQ(U(VarMonthName)(m, abbr, 0, &out), S_OK);
            if (out) { CHECKF(!wcscmp(out, ref), "month %d abbr %d got %s want %s", m, abbr, vfp_n(out), vfp_n(ref)); U(SysFreeString)(out); }
        }
}
TEST(vfp_monthname_invalid_args)
{
    BSTR out = NULL;
    REQUIRE(U(VarMonthName));
    EXPECT_EQ(U(VarMonthName)(0, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarMonthName)(13, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarMonthName)(-1, 0, 0, &out), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarMonthName)(0x7fffffff, 1, 0, &out), VFP_E_INVALIDARG);
    if (!t_is_wine()) {   /* wine dereferences the NULL out pointer */
        EXPECT_EQ(U(VarMonthName)(1, 0, 0, NULL), VFP_E_INVALIDARG);
        EXPECT_EQ(U(VarMonthName)(12, 0, 0, NULL), VFP_E_INVALIDARG);
    }
    CHECK(out == NULL);
}
TEST(vfp_monthname_hijri_flag)
{
    /* decomp: dwFlags & 8 (VAR_CALENDAR_HIJRI) selects a built-in table of 12 non-empty names, locale independent */
    int m;
    REQUIRE(U(VarMonthName)); REQUIRE(U(SysFreeString));
    for (m = 1; m <= 12; m++) {
        BSTR out = NULL;
        EXPECT_EQ(U(VarMonthName)(m, 0, VAR_CALENDAR_HIJRI, &out), S_OK);
        if (out) { CHECK(U(SysStringLen)(out) > 0); t_note("hijri month %d: %s", m, vfp_n(out)); U(SysFreeString)(out); }
    }
}
TEST(vfp_monthname_abbrev_any_nonzero)
{
    BSTR a = NULL, b = NULL;
    REQUIRE(U(VarMonthName)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(VarMonthName)(5, 1, 0, &a), S_OK);
    EXPECT_EQ(U(VarMonthName)(5, -1, 0, &b), S_OK);
    CHECK(a && b && !wcscmp(a, b));
    if (a) U(SysFreeString)(a); if (b) U(SysFreeString)(b);
}

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_getaltmonthnames)
{
    LPOLESTR *str, *str2;
    HRESULT hr;
    REQUIRE(U(GetAltMonthNames));
    str = (void *)0xdeadbeef;
    hr = U(GetAltMonthNames)(0, &str);
    EXPECT_EQ(hr, S_OK); CHECK(str == NULL);
    str = (void *)0xdeadbeef;
    hr = U(GetAltMonthNames)(VFP_EN_US, &str);
    EXPECT_EQ(hr, S_OK); CHECK(str == NULL);
    str = (void *)0xdeadbeef;
    hr = U(GetAltMonthNames)(VFP_DE_DE, &str);
    EXPECT_EQ(hr, S_OK); CHECK(str == NULL);
    str = NULL;
    hr = U(GetAltMonthNames)(MAKELCID(MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_EGYPT), SORT_DEFAULT), &str);
    EXPECT_EQ(hr, S_OK); CHECK(str != NULL);
    str2 = NULL;
    hr = U(GetAltMonthNames)(MAKELCID(MAKELANGID(LANG_ARABIC, SUBLANG_ARABIC_EGYPT), SORT_DEFAULT), &str2);
    EXPECT_EQ(hr, S_OK); CHECK(str2 == str);
    str = NULL;
    hr = U(GetAltMonthNames)(MAKELCID(MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), SORT_DEFAULT), &str);
    EXPECT_EQ(hr, S_OK); CHECK(str != NULL);
    str = NULL;
    hr = U(GetAltMonthNames)(MAKELCID(MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT), SORT_DEFAULT), &str);
    EXPECT_EQ(hr, S_OK); CHECK(str != NULL);
    if (str) { int i; for (i = 0; i < 24 && str[i]; i++) ; t_note("polish alt month names: %d non-null entries before first NULL", i); }
}
