/* VariantChangeTypeEx / VariantChangeType into a BSTR with wFlags VARIANT_USE_NLS (0x80), VARIANT_NOUSEROVERRIDE (0x4) and other bits.
 * Windows 10 22H2 results. Rows with the user locale need an en-US user locale. */
#include "nls_text.h"
#define W_NLS 0x80
#define W_NUO 0x04

/* convert src to BSTR, check hr and text */
static void vct_one(const char *what, VARIANT *src, LCID lcid, USHORT wf, HRESULT want_hr, const WCHAR *want)
{
    VARIANT d; HRESULT h;
    memset(&d, 0x77, sizeof d); V_VT(&d) = VT_EMPTY;
    h = U(VariantChangeTypeEx)(&d, src, lcid, wf, VT_BSTR);
    t_check(h == want_hr, __FILE__, __LINE__, what, "wFlags 0x%x hr got 0x%08lx want 0x%08lx", wf, (unsigned long)h, (unsigned long)want_hr);
    if (h == S_OK && want) {
        t_check(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), want), __FILE__, __LINE__, what, "wFlags 0x%x text got \"%ls\"", wf, (V_VT(&d) == VT_BSTR && V_BSTR(&d)) ? V_BSTR(&d) : L"<none>");
        U(VariantClear)(&d);
    }
}
#define VCT_R8(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_R8; V_R8(&s_) = (v); vct_one("R8", &s_, __VA_ARGS__); } while (0)
#define VCT_R4(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_R4; V_R4(&s_) = (v); vct_one("R4", &s_, __VA_ARGS__); } while (0)
#define VCT_I4(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_I4; V_I4(&s_) = (v); vct_one("I4", &s_, __VA_ARGS__); } while (0)
#define VCT_I2(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_I2; V_I2(&s_) = (v); vct_one("I2", &s_, __VA_ARGS__); } while (0)
#define VCT_I1(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_I1; V_I1(&s_) = (v); vct_one("I1", &s_, __VA_ARGS__); } while (0)
#define VCT_UI1(v, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_UI1; V_UI1(&s_) = (v); vct_one("UI1", &s_, __VA_ARGS__); } while (0)
#define VCT_UI4(v, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_UI4; V_UI4(&s_) = (v); vct_one("UI4", &s_, __VA_ARGS__); } while (0)
#define VCT_I8(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_I8; V_I8(&s_) = (v); vct_one("I8", &s_, __VA_ARGS__); } while (0)
#define VCT_UI8(v, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_UI8; V_UI8(&s_) = (v); vct_one("UI8", &s_, __VA_ARGS__); } while (0)
#define VCT_CY(v, ...)  do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_CY; V_CY(&s_).int64 = (v); vct_one("CY", &s_, __VA_ARGS__); } while (0)
#define VCT_BOOL(v, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_BOOL; V_BOOL(&s_) = (v); vct_one("BOOL", &s_, __VA_ARGS__); } while (0)
#define VCT_INT(v, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_INT; V_INT(&s_) = (v); vct_one("INT", &s_, __VA_ARGS__); } while (0)
#define VCT_DATE(v, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_VT(&s_) = VT_DATE; V_DATE(&s_) = (v); vct_one("DATE", &s_, __VA_ARGS__); } while (0)
#define VCT_DEC(d, ...) do { VARIANT s_; memset(&s_, 0, sizeof s_); V_DECIMAL(&s_) = (d); V_VT(&s_) = VT_DECIMAL; vct_one("DEC", &s_, __VA_ARGS__); } while (0)

TEST(nls_vct_user_locale)
{
    REQUIRE(U(VariantChangeTypeEx));
    if (!NLS_USER_EN()) { t_skip("user locale is not en-US"); return; }
    VCT_R8(9876.54321, 0x409, W_NLS, S_OK, L"9,876.54");
    VCT_R8(-1234.5678, 0x409, W_NLS, S_OK, L"-1,234.57");
    VCT_R8(1e15, 0x409, W_NLS, S_OK, L"1,000,000,000,000,000.00");
    VCT_R4(1234.5f, 0x409, W_NLS, S_OK, L"1,234.50");
    VCT_I4(-7654321, 0x409, W_NLS, S_OK, L"-7,654,321");
    VCT_I2(-12345, 0x409, W_NLS, S_OK, L"-12,345");
    VCT_I1(-100, 0x409, W_NLS, S_OK, L"-100");
    VCT_UI1(200, 0x409, W_NLS, S_OK, L"200");
    VCT_UI4(4000000000UL, 0x409, W_NLS, S_OK, L"4,000,000,000");
    VCT_I8(-123456789012LL, 0x409, W_NLS, S_OK, L"-123,456,789,012");
    VCT_UI8(18446744073709551615ULL, 0x409, W_NLS, S_OK, L"18,446,744,073,709,551,615");
    VCT_INT(-1234567, 0x409, W_NLS, S_OK, L"-1,234,567");
    VCT_CY(-98765432100LL, 0x409, W_NLS, S_OK, L"($9,876,543.21)");
    VCT_CY(1500, 0x409, W_NLS, S_OK, L"$0.15");
    VCT_CY(-1500, 0x409, W_NLS, S_OK, L"($0.15)");
    VCT_BOOL(-1, 0x409, W_NLS, S_OK, L"-1");
    VCT_BOOL(0, 0x409, W_NLS, S_OK, L"0");
}

TEST(nls_vct_stock_record)
{
    REQUIRE(U(VariantChangeTypeEx));
    VCT_R8(9876.54321, 0x409, W_NLS | W_NUO, S_OK, L"9,876.543");
    VCT_R8(-1234.5678, 0x409, W_NLS | W_NUO, S_OK, L"- 1,234.568");
    VCT_R8(1e15, 0x409, W_NLS | W_NUO, S_OK, L"1,000,000,000,000,000.000");
    VCT_R4(1234.5f, 0x409, W_NLS | W_NUO, S_OK, L"1,234.500");
    VCT_I4(-7654321, 0x409, W_NLS | W_NUO, S_OK, L"- 7,654,321");
    VCT_I2(-12345, 0x409, W_NLS | W_NUO, S_OK, L"- 12,345");
    VCT_I1(-100, 0x409, W_NLS | W_NUO, S_OK, L"- 100");
    VCT_UI1(200, 0x409, W_NLS | W_NUO, S_OK, L"200");
    VCT_I8(-123456789012LL, 0x409, W_NLS | W_NUO, S_OK, L"- 123,456,789,012");
    VCT_INT(-1234567, 0x409, W_NLS | W_NUO, S_OK, L"- 1,234,567");
    VCT_CY(-98765432100LL, 0x409, W_NLS | W_NUO, S_OK, L"($9,876,543.21)");
    VCT_CY(1500, 0x409, W_NLS | W_NUO, S_OK, L"$0.15");
    VCT_BOOL(-1, 0x409, W_NLS | W_NUO, S_OK, L"- 1");
    VCT_BOOL(0, 0x409, W_NLS | W_NUO, S_OK, L"0");
}

TEST(nls_vct_decimal_ignores_the_bit)
{
    DECIMAL d;
    REQUIRE(U(VariantChangeTypeEx));
    memset(&d, 0, sizeof d); d.scale = 2; d.Lo64 = 123456789;
    VCT_DEC(d, 0x409, W_NLS, S_OK, L"1234567.89");
    VCT_DEC(d, 0x409, W_NLS | W_NUO, S_OK, L"1234567.89");
    VCT_DEC(d, 0x409, 0, S_OK, L"1234567.89");
}

TEST(nls_vct_without_the_bit_is_plain)
{
    static const USHORT plain[] = { 0x4, 0x20, 0x40, 0x100 };
    size_t i;
    REQUIRE(U(VariantChangeTypeEx));
    for (i = 0; i < sizeof plain / sizeof plain[0]; i++) {
        VCT_R8(9876.54321, 0x409, plain[i], S_OK, L"9876.54321");
        VCT_R8(-1234.5678, 0x409, plain[i], S_OK, L"-1234.5678");
        VCT_R8(1e15, 0x409, plain[i], S_OK, L"1E+15");
        VCT_R4(1234.5f, 0x409, plain[i], S_OK, L"1234.5");
        VCT_I4(-7654321, 0x409, plain[i], S_OK, L"-7654321");
        VCT_UI4(4000000000UL, 0x409, plain[i], S_OK, L"4000000000");
        VCT_I8(-123456789012LL, 0x409, plain[i], S_OK, L"-123456789012");
        VCT_CY(-98765432100LL, 0x409, plain[i], S_OK, L"-9876543.21");
        VCT_CY(1500, 0x409, plain[i], S_OK, L"0.15");
        VCT_BOOL(-1, 0x409, plain[i], S_OK, L"-1");
    }
}

TEST(nls_vct_alpha_bool_and_special_sources)
{
    VARIANT s;
    REQUIRE(U(VariantChangeTypeEx));
    if (!NLS_USER_EN()) { t_skip("user locale is not en-US"); return; }
    VCT_BOOL(-1, 0x409, 0x80 | 0x02, S_OK, L"True");
    VCT_BOOL(0, 0x409, 0x80 | 0x02, S_OK, L"False");
    VCT_BOOL(-1, 0x409, 0x80 | 0x10, S_OK, L"True");
    memset(&s, 0, sizeof s); V_VT(&s) = VT_NULL;
    vct_one("NULL", &s, 0x409, W_NLS, (HRESULT)0x80020005, NULL);
    memset(&s, 0, sizeof s); V_VT(&s) = VT_EMPTY;
    vct_one("EMPTY", &s, 0x409, W_NLS, S_OK, L"");
}

TEST(nls_vct_other_destinations_and_sources)
{
    VARIANT s, d; HRESULT h;
    REQUIRE(U(VariantChangeTypeEx)); REQUIRE(U(SysAllocString));
    /* a numeric destination ignores the bit */
    memset(&s, 0, sizeof s); memset(&d, 0, sizeof d); V_VT(&s) = VT_R8; V_R8(&s) = 9876.54321;
    h = U(VariantChangeTypeEx)(&d, &s, 0x409, W_NLS | W_NUO, VT_I4);
    EXPECT_EQ(h, S_OK); EXPECT_EQ(V_VT(&d), VT_I4); EXPECT_EQ(V_I4(&d), 9877);
    /* a BSTR source copies */
    memset(&s, 0, sizeof s); memset(&d, 0, sizeof d); V_VT(&s) = VT_BSTR; V_BSTR(&s) = U(SysAllocString)(L"1,234.5");
    h = U(VariantChangeTypeEx)(&d, &s, 0x409, W_NLS | W_NUO, VT_BSTR);
    EXPECT_EQ(h, S_OK); CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), L"1,234.5"));
    U(VariantClear)(&d);
    h = U(VariantChangeTypeEx)(&d, &s, 0x409, W_NLS, VT_R8);
    EXPECT_EQ(h, S_OK); EXPECT_EQ(V_VT(&d), VT_R8); CHECK(V_R8(&d) == 1234.5);
    U(VariantClear)(&s);
    /* in place */
    memset(&s, 0, sizeof s); V_VT(&s) = VT_R8; V_R8(&s) = 9876.54321;
    h = U(VariantChangeTypeEx)(&s, &s, 0x409, W_NLS | W_NUO, VT_BSTR);
    EXPECT_EQ(h, S_OK); CHECK(V_VT(&s) == VT_BSTR && V_BSTR(&s) && !lstrcmpW(V_BSTR(&s), L"9,876.543"));
    U(VariantClear)(&s);
}

TEST(nls_vct_variantchangetype_uses_the_user_locale)
{
    VARIANT s, d; HRESULT h;
    REQUIRE(U(VariantChangeType));
    if (!NLS_USER_EN()) { t_skip("user locale is not en-US"); return; }
    memset(&s, 0, sizeof s); memset(&d, 0, sizeof d); V_VT(&s) = VT_R8; V_R8(&s) = 9876.54321;
    h = U(VariantChangeType)(&d, &s, W_NLS, VT_BSTR);
    EXPECT_EQ(h, S_OK); CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), L"9,876.54")); U(VariantClear)(&d);
    V_VT(&s) = VT_I4; V_I4(&s) = -7654321;
    h = U(VariantChangeType)(&d, &s, W_NLS, VT_BSTR);
    EXPECT_EQ(h, S_OK); CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), L"-7,654,321")); U(VariantClear)(&d);
    V_VT(&s) = VT_CY; V_CY(&s).int64 = -98765432100LL;
    h = U(VariantChangeType)(&d, &s, W_NLS, VT_BSTR);
    EXPECT_EQ(h, S_OK); CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), L"($9,876,543.21)")); U(VariantClear)(&d);
    V_VT(&s) = VT_R8; V_R8(&s) = 9876.54321;
    h = U(VariantChangeType)(&d, &s, W_NLS | W_NUO, VT_BSTR);
    EXPECT_EQ(h, S_OK); CHECK(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), L"9,876.543")); U(VariantClear)(&d);
}

TEST(nls_vct_german_locale)
{
    REQUIRE(U(VariantChangeTypeEx));
    VCT_R8(2.5, 0x407, W_NLS, S_OK, L"2,50");
    VCT_R8(2.5, 0x407, W_NLS | W_NUO, S_OK, L"2,50");   /* the stock 3-digit record is en-US only */
    VCT_R8(2.5, 0x407, 0, S_OK, L"2,5");
}

/* with the bit a DATE is GetDateFormat + " " + GetTimeFormat: the time is written at midnight too */
TEST(nls_vct_date_always_has_a_time)
{
    REQUIRE(U(VariantChangeTypeEx));
    VCT_DATE(36526.0, 0x409, W_NLS | W_NUO, S_OK, L"1/1/2000 12:00:00 AM");
    VCT_DATE(36526.5, 0x409, W_NLS | W_NUO, S_OK, L"1/1/2000 12:00:00 PM");
    VCT_DATE(36526.75, 0x409, W_NLS | W_NUO, S_OK, L"1/1/2000 6:00:00 PM");
    VCT_DATE(36526.0, 0x409, 0x80, S_OK, L"1/1/2000 12:00:00 AM");
    VCT_DATE(36526.0, 0x409, 0x82, S_OK, L"1/1/2000 12:00:00 AM");
    VCT_DATE(36526.0, 0x409, 0x180, S_OK, L"1/1/2000 12:00:00 AM");
    VCT_DATE(0.0, 0x409, W_NLS | W_NUO, S_OK, L"12/30/1899 12:00:00 AM");
    VCT_DATE(-1.5, 0x409, W_NLS | W_NUO, S_OK, L"12/29/1899 12:00:00 PM");
    VCT_DATE(2958465.5, 0x409, W_NLS | W_NUO, S_OK, L"12/31/9999 12:00:00 PM");
    /* GetDateFormat takes years from 1601: 1/1/100 has a text without the bit and is E_INVALIDARG with it */
    VCT_DATE(-657434.5, 0x409, W_NLS | W_NUO, (HRESULT)0x80070057, NULL);
    VCT_DATE(-657434.5, 0x409, W_NUO, S_OK, L"1/1/100 12:00:00 PM");
    /* without the bit midnight is the date alone */
    VCT_DATE(36526.0, 0x409, W_NUO, S_OK, L"1/1/2000");
    VCT_DATE(36526.0, 0x409, 0x100, S_OK, L"1/1/2000");
    VCT_DATE(36526.5, 0x409, W_NUO, S_OK, L"1/1/2000 12:00:00 PM");
}
