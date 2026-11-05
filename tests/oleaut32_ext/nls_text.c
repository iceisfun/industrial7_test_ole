#include "nls_text.h"

TEST(nls_bstr_integer_widths_stock_record)
{
    REQUIRE(U(VarBstrFromI1)); REQUIRE(U(VarBstrFromI2)); REQUIRE(U(VarBstrFromI4)); REQUIRE(U(VarBstrFromI8));
    REQUIRE(U(VarBstrFromUI1)); REQUIRE(U(VarBstrFromUI2)); REQUIRE(U(VarBstrFromUI4)); REQUIRE(U(VarBstrFromUI8));
    NLS_EXPECT(U(VarBstrFromI4)(1234567, 0x409, NLS_NUO, &b_), S_OK, L"1,234,567");
    NLS_EXPECT(U(VarBstrFromI4)(-1234567, 0x409, NLS_NUO, &b_), S_OK, L"- 1,234,567");
    NLS_EXPECT(U(VarBstrFromI4)(0, 0x409, NLS_NUO, &b_), S_OK, L"0");
    NLS_EXPECT(U(VarBstrFromI2)(-5, 0x409, NLS_NUO, &b_), S_OK, L"- 5");
    NLS_EXPECT(U(VarBstrFromI2)(32767, 0x409, NLS_NUO, &b_), S_OK, L"32,767");
    NLS_EXPECT(U(VarBstrFromI1)(-128, 0x409, NLS_NUO, &b_), S_OK, L"- 128");
    NLS_EXPECT(U(VarBstrFromUI1)(200, 0x409, NLS_NUO, &b_), S_OK, L"200");
    NLS_EXPECT(U(VarBstrFromUI2)(65535, 0x409, NLS_NUO, &b_), S_OK, L"65,535");
    NLS_EXPECT(U(VarBstrFromUI4)(0xffffffffUL, 0x409, NLS_NUO, &b_), S_OK, L"4,294,967,295");
    NLS_EXPECT(U(VarBstrFromI8)(-9223372036854775807LL, 0x409, NLS_NUO, &b_), S_OK, L"- 9,223,372,036,854,775,807");
    NLS_EXPECT(U(VarBstrFromUI8)(18446744073709551615ULL, 0x409, NLS_NUO, &b_), S_OK, L"18,446,744,073,709,551,615");
}

TEST(nls_bstr_integer_user_locale)
{
    REQUIRE(U(VarBstrFromI4));
    if (!NLS_USER_EN()) { t_skip("user locale is not en-US"); return; }
    NLS_EXPECT(U(VarBstrFromI4)(1234567, 0x409, NLS_FLAG, &b_), S_OK, L"1,234,567");
    NLS_EXPECT(U(VarBstrFromI4)(-1234567, 0x409, NLS_FLAG, &b_), S_OK, L"-1,234,567");
    NLS_EXPECT(U(VarBstrFromI8)(-1234567, 0x409, NLS_FLAG, &b_), S_OK, L"-1,234,567");
}

TEST(nls_bstr_r8_stock_record)
{
    REQUIRE(U(VarBstrFromR8));
    NLS_EXPECT(U(VarBstrFromR8)(1234567.891, 0x409, NLS_NUO, &b_), S_OK, L"1,234,567.891");
    NLS_EXPECT(U(VarBstrFromR8)(-0.5, 0x409, NLS_NUO, &b_), S_OK, L"- 0.500");
    NLS_EXPECT(U(VarBstrFromR8)(0.0, 0x409, NLS_NUO, &b_), S_OK, L"0.000");
    NLS_EXPECT(U(VarBstrFromR8)(1e20, 0x409, NLS_NUO, &b_), S_OK, L"100,000,000,000,000,000,000.000");
    NLS_EXPECT(U(VarBstrFromR8)(1.005, 0x409, NLS_NUO, &b_), S_OK, L"1.005");
    NLS_EXPECT(U(VarBstrFromR8)(-0.0001, 0x409, NLS_NUO, &b_), S_OK, L"0.000");   /* a negative that rounds to zero loses its sign */
    NLS_EXPECT(U(VarBstrFromR8)(2.5, 0x409, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"2.5");   /* without the bit: the plain text */
}

TEST(nls_bstr_r8_user_locale)
{
    REQUIRE(U(VarBstrFromR8));
    if (!NLS_USER_EN()) { t_skip("user locale is not en-US"); return; }
    NLS_EXPECT(U(VarBstrFromR8)(-1234567.8915, 0x409, NLS_FLAG, &b_), S_OK, L"-1,234,567.89");
    NLS_EXPECT(U(VarBstrFromR8)(0.5, 0x409, NLS_FLAG, &b_), S_OK, L"0.50");
    NLS_EXPECT(U(VarBstrFromR8)(0.995, 0x409, NLS_FLAG, &b_), S_OK, L"0.99");       /* the binary value is just below .995 */
    NLS_EXPECT(U(VarBstrFromR8)(999.9995, 0x409, NLS_FLAG, &b_), S_OK, L"1,000.00");
    NLS_EXPECT(U(VarBstrFromR8)(0.0005, 0x409, NLS_FLAG, &b_), S_OK, L"0.00");
    NLS_EXPECT(U(VarBstrFromR8)(-0.0001, 0x409, NLS_FLAG, &b_), S_OK, L"0.00");
}

TEST(nls_bstr_r4_text)
{
    REQUIRE(U(VarBstrFromR4));
    NLS_EXPECT(U(VarBstrFromR4)(3.14159f, 0x409, NLS_NUO, &b_), S_OK, L"3.142");
    NLS_EXPECT(U(VarBstrFromR4)(-1234.5f, 0x409, NLS_NUO, &b_), S_OK, L"- 1,234.500");
    if (NLS_USER_EN()) NLS_EXPECT(U(VarBstrFromR4)(0.1f, 0x409, NLS_FLAG, &b_), S_OK, L"0.10");
}

TEST(nls_bstr_real_non_numbers)
{
    BSTR b = (BSTR)0x55;
    double inf = 1e308 * 10.0;
    REQUIRE(U(VarBstrFromR8));
    /* GetNumberFormat rejects the C text "inf" */
    if (NLS_USER_EN()) NLS_EXPECT(U(VarBstrFromR8)(inf, 0x409, NLS_FLAG, &b_), 0x80070057, NULL);
    (void)b;
}

TEST(nls_bstr_cy_currency_format)
{
    REQUIRE(U(VarBstrFromCy));
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456780000LL), 0x409, NLS_NUO, &b_), S_OK, L"$12,345,678.00");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-15000), 0x409, NLS_NUO, &b_), S_OK, L"($1.50)");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456789), 0x409, NLS_NUO, &b_), S_OK, L"$12,345.68");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-123456789), 0x409, NLS_NUO, &b_), S_OK, L"($12,345.68)");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(10000), 0x409, NLS_NUO, &b_), S_OK, L"$1.00");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(10050), 0x409, NLS_NUO, &b_), S_OK, L"$1.01");          /* half rounds up */
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(9950), 0x409, NLS_NUO, &b_), S_OK, L"$1.00");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(19950), 0x409, NLS_NUO, &b_), S_OK, L"$2.00");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(99995), 0x409, NLS_NUO, &b_), S_OK, L"$10.00");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-9950), 0x409, NLS_NUO, &b_), S_OK, L"($1.00)");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(0x7fffffffffffffffLL), 0x409, NLS_NUO, &b_), S_OK, L"$922,337,203,685,477.58");
    NLS_EXPECT(U(VarBstrFromCy)((CY){ .int64 = (long long)0x8000000000000000ULL }, 0x409, NLS_NUO, &b_), S_OK, L"($922,337,203,685,477.58)");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(1000), 0x409, NLS_NUO, &b_), S_OK, L"$0.10");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-1500), 0x409, NLS_NUO, &b_), S_OK, L"($0.15)");
}

/* The stock body writes the signed integer and moves its last four characters right to open the decimal point; with fewer
 * than five characters the text the currency formatter receives is not the number: four characters give ".dddd" (and ".-ddd"
 * for a negative, which it rejects), fewer give an empty text, which formats as zero. */
TEST(nls_bstr_cy_small_values_quirk)
{
    static const long long zero[] = { 0, 5, 49, 50, 51, 150, 250, 350, 450, 999, -5, -49, -50, -51, -99 };
    size_t i;
    REQUIRE(U(VarBstrFromCy));
    for (i = 0; i < sizeof zero / sizeof zero[0]; i++) { BSTR b = NULL; HRESULT h = U(VarBstrFromCy)(nls_cy(zero[i]), 0x409, NLS_NUO, &b);
        CHECKF(h == S_OK && b && !lstrcmpW(b, L"$0.00"), "cy %lld hr 0x%08lx text %ls", zero[i], (unsigned long)h, b ? b : L"<null>"); if (b) U(SysFreeString)(b); }
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-150), 0x409, NLS_NUO, &b_), 0x80070057, NULL);
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-250), 0x409, NLS_NUO, &b_), 0x80070057, NULL);
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-999), 0x409, NLS_NUO, &b_), 0x80070057, NULL);
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(1000), 0x409, NLS_NUO, &b_), S_OK, L"$0.10");
}

TEST(nls_bstr_cy_user_locale)
{
    REQUIRE(U(VarBstrFromCy));
    if (!NLS_USER_EN()) { t_skip("user locale is not en-US"); return; }
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456789), 0x409, NLS_FLAG, &b_), S_OK, L"$12,345.68");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-123456789), 0x409, NLS_FLAG, &b_), S_OK, L"($12,345.68)");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456789), 0x409, NLS_NUO, &b_), S_OK, L"$12,345.68");
}

TEST(nls_bstr_dec_ignores_the_flag)
{
    DECIMAL d;
    REQUIRE(U(VarBstrFromDec));
    d = nls_dec(2, 0, 0, 123456789);
    NLS_EXPECT(U(VarBstrFromDec)(&d, 0x409, NLS_NUO, &b_), S_OK, L"1234567.89");
    NLS_EXPECT(U(VarBstrFromDec)(&d, 0x409, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"1234567.89");
    d = nls_dec(0, 0x80, 0, 1234567);
    NLS_EXPECT(U(VarBstrFromDec)(&d, 0x409, NLS_NUO, &b_), S_OK, L"-1234567");
    d = nls_dec(5, 0, 0, 1);
    NLS_EXPECT(U(VarBstrFromDec)(&d, 0x409, NLS_NUO, &b_), S_OK, L"0.00001");
}

/* the plain currency text without the bit stays the bare number */
TEST(nls_bstr_without_the_bit_is_plain)
{
    REQUIRE(U(VarBstrFromCy)); REQUIRE(U(VarBstrFromI4));
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456789), 0x409, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"12345.6789");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(5000), 0x409, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"0.5");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-5000), 0x409, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"-0.5");
    NLS_EXPECT(U(VarBstrFromI4)(-1234567, 0x409, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"-1234567");
}

/* other locales: separators, symbol position and the negative picture come from the locale; the same text rules */
TEST(nls_bstr_german_locale)
{
    REQUIRE(U(VarBstrFromR8)); REQUIRE(U(VarBstrFromCy)); REQUIRE(U(VarBstrFromI4));
    NLS_EXPECT(U(VarBstrFromR8)(-1234567.891, 0x407, NLS_FLAG, &b_), S_OK, L"-1.234.567,89");
    NLS_EXPECT(U(VarBstrFromR8)(-1234567.891, 0x407, NLS_NUO, &b_), S_OK, L"-1.234.567,89");
    NLS_EXPECT(U(VarBstrFromR8)(1234567.891, 0x407, LOCALE_NOUSEROVERRIDE, &b_), S_OK, L"1234567,891");
    NLS_EXPECT(U(VarBstrFromI4)(-1234567, 0x407, NLS_FLAG, &b_), S_OK, L"-1.234.567");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-123456789), 0x407, NLS_FLAG, &b_), S_OK, L"-12.345,68 \x20ac");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456789), 0x407, NLS_FLAG, &b_), S_OK, L"12.345,68 \x20ac");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(10050), 0x407, NLS_FLAG, &b_), S_OK, L"1,01 \x20ac");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(150), 0x407, NLS_FLAG, &b_), S_OK, L"0,00 \x20ac");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-150), 0x407, NLS_FLAG, &b_), 0x80070057, NULL);
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-51), 0x407, NLS_FLAG, &b_), S_OK, L"0,00 \x20ac");
}

/* the %f text is exact: ties follow the binary value, then the formatter keeps the digits it is given */
TEST(nls_bstr_real_rounding_text)
{
    REQUIRE(U(VarBstrFromR8));
    NLS_EXPECT(U(VarBstrFromR8)(0.125, 0x407, NLS_FLAG, &b_), S_OK, L"0,12");
    NLS_EXPECT(U(VarBstrFromR8)(0.375, 0x407, NLS_FLAG, &b_), S_OK, L"0,38");
    NLS_EXPECT(U(VarBstrFromR8)(0.045, 0x407, NLS_FLAG, &b_), S_OK, L"0,04");
    NLS_EXPECT(U(VarBstrFromR8)(2.5, 0x407, NLS_FLAG, &b_), S_OK, L"2,50");
    NLS_EXPECT(U(VarBstrFromR8)(1e-7, 0x407, NLS_FLAG, &b_), S_OK, L"0,00");
    NLS_EXPECT(U(VarBstrFromR8)(-1e-7, 0x407, NLS_FLAG, &b_), S_OK, L"0,00");
    NLS_EXPECT(U(VarBstrFromR8)(123456789.125, 0x407, NLS_FLAG, &b_), S_OK, L"123.456.789,12");
}

TEST(nls_bstr_japanese_locale)
{
    REQUIRE(U(VarBstrFromR8)); REQUIRE(U(VarBstrFromCy));
    /* no fraction digits in the yen: half rounds up at the formatter */
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(123456789), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"12,346");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-123456789), 0x411, NLS_FLAG, &b_), S_OK, L"-\x00a5" L"12,346");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(5000), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"1");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(15000), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"2");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(25000), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"3");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(-5000), 0x411, NLS_FLAG, &b_), S_OK, L"-\x00a5" L"1");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(1005000), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"101");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(9999900), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"1,000");
    NLS_EXPECT(U(VarBstrFromCy)(nls_cy(500), 0x411, NLS_FLAG, &b_), S_OK, L"\x00a5" L"0");
    NLS_EXPECT(U(VarBstrFromR8)(-1234567.891, 0x411, NLS_NUO, &b_), S_OK, L"-1,234,567.89");
}

/* VarBstrFromDate with the bit: GetDateFormat + " " + GetTimeFormat, the time is always written, years before 1601 are rejected */
TEST(nls_bstr_from_date_bit)
{
    REQUIRE(U(VarBstrFromDate));
    NLS_EXPECT(U(VarBstrFromDate)(36526.0, 0x409, NLS_FLAG, &b_), S_OK, L"1/1/2000 12:00:00 AM");
    NLS_EXPECT(U(VarBstrFromDate)(36526.0, 0x409, NLS_NUO, &b_), S_OK, L"1/1/2000 12:00:00 AM");
    NLS_EXPECT(U(VarBstrFromDate)(36526.75, 0x409, NLS_NUO, &b_), S_OK, L"1/1/2000 6:00:00 PM");
    NLS_EXPECT(U(VarBstrFromDate)(0.0, 0x409, NLS_NUO, &b_), S_OK, L"12/30/1899 12:00:00 AM");
    NLS_EXPECT(U(VarBstrFromDate)(-1.5, 0x409, NLS_NUO, &b_), S_OK, L"12/29/1899 12:00:00 PM");
    NLS_EXPECT(U(VarBstrFromDate)(43000.123, 0x409, NLS_NUO, &b_), S_OK, L"9/22/2017 2:57:07 AM");
    NLS_EXPECT(U(VarBstrFromDate)(2958465.5, 0x409, NLS_NUO, &b_), S_OK, L"12/31/9999 12:00:00 PM");
    NLS_EXPECT(U(VarBstrFromDate)(-657434.5, 0x409, NLS_NUO, &b_), 0x80070057, NULL);
    NLS_EXPECT(U(VarBstrFromDate)(2958466.5, 0x409, NLS_NUO, &b_), 0x80070057, NULL);
    /* the plain text: midnight is the date alone, the other flag words keep their meaning */
    NLS_EXPECT(U(VarBstrFromDate)(36526.0, 0x409, 0, &b_), S_OK, L"1/1/2000");
    NLS_EXPECT(U(VarBstrFromDate)(36526.75, 0x409, 0, &b_), S_OK, L"1/1/2000 6:00:00 PM");
    NLS_EXPECT(U(VarBstrFromDate)(-657434.5, 0x409, 0, &b_), S_OK, L"1/1/100 12:00:00 PM");
    /* the locale pictures */
    NLS_EXPECT(U(VarBstrFromDate)(36526.0, 0x407, NLS_FLAG, &b_), S_OK, L"01.01.2000 00:00:00");
    NLS_EXPECT(U(VarBstrFromDate)(36526.75, 0x407, NLS_FLAG, &b_), S_OK, L"01.01.2000 18:00:00");
    NLS_EXPECT(U(VarBstrFromDate)(36526.0, 0x411, NLS_FLAG, &b_), S_OK, L"2000/01/01 0:00:00");
    NLS_EXPECT(U(VarBstrFromDate)(36526.75, 0x411, NLS_FLAG, &b_), S_OK, L"2000/01/01 18:00:00");
}
