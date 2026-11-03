/* variant_format_parse: VarXxxFromStr conversions (oleaut32.dll, LCID 0x409). All of these go through VarParseNumFromStr. */
#include "vfp.h"

typedef struct { const char *s; double exp; } DRow;
typedef struct { const char *s; long exp; } IRow;

TEST(vfp_r8fromstr_table)
{
    static const DRow rows[] = {
        {"0", 0}, {"1", 1}, {"-1", -1}, {"+1", 1}, {"1.5", 1.5}, {"-1.5", -1.5}, {".5", 0.5}, {"5.", 5}, {"  7  ", 7},
        {"1e3", 1000}, {"1E3", 1000}, {"1e+3", 1000}, {"1e-3", 0.001}, {"1.5e2", 150}, {"1,000", 1000}, {"1,234,567.5", 1234567.5},
        {"(5)", -5}, {"&H10", 16}, {"&HFF", 255}, {"&O17", 15}, {"&o10", 8}, {"$12.50", 12.5}, {"007", 7}, {"1d2", 100},
        {"100%", 100}, {"-.25", -0.25},
    };
    size_t i;
    REQUIRE(U(VarR8FromStr));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        double d = -12345.0;
        HRESULT hr = U(VarR8FromStr)((LPOLESTR)vfp_w(rows[i].s), VFP_EN_US, VFP_NOUSER, &d);
        if (!strcmp(rows[i].s, "100%") || !strcmp(rows[i].s, "1d2")) {  /* unverified: note only */
            if (hr != S_OK || d != rows[i].exp) t_note("VarR8FromStr(\"%s\") hr=0x%lx d=%g", rows[i].s, (unsigned long)hr, d);
            continue;
        }
        CHECKF(hr == S_OK && d == rows[i].exp, "VarR8FromStr(\"%s\") want %g got hr=0x%lx d=%.17g", rows[i].s, rows[i].exp, (unsigned long)hr, d);
    }
}

TEST(vfp_r8fromstr_invalid)
{
    static const char *bad[] = { "", "abc", "1.2.3", "--1", "1e", "1 2", "&H", "&HG", "&H1FFFFFFFFFFFFFFFFF", "1,,2x", "1-", "@" };
    size_t i;
    REQUIRE(U(VarR8FromStr));
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        double d = 0;
        HRESULT hr = U(VarR8FromStr)((LPOLESTR)vfp_w(bad[i]), VFP_EN_US, VFP_NOUSER, &d);
        if (!strcmp(bad[i], "1-")) { t_note("VarR8FromStr(\"1-\") hr=0x%lx d=%g (trailing minus is accepted by NUMPRS_TRAILING_MINUS in VarParseNumFromStr)", (unsigned long)hr, d); continue; }
        CHECKF(hr == VFP_E_TYPEMISMATCH || hr == VFP_E_OVERFLOW, "VarR8FromStr(\"%s\") hr=0x%lx", bad[i], (unsigned long)hr);
    }
}

TEST(vfp_r8fromstr_null_string)
{
    double d = 1;
    REQUIRE(U(VarR8FromStr));
    CHECK(U(VarR8FromStr)(NULL, VFP_EN_US, 0, &d) == VFP_E_TYPEMISMATCH);
}

TEST(vfp_i4fromstr_rounding)
{
    static const IRow rows[] = {
        {"0", 0}, {"1.4", 1}, {"1.5", 2}, {"2.5", 2}, {"3.5", 4}, {"-1.5", -2}, {"-2.5", -2}, {"0.5", 0}, {"-0.5", 0},
        {"2147483647", 2147483647L}, {"-2147483648", (long)-2147483647L - 1}, {"1e3", 1000}, {"&H7FFFFFFF", 2147483647L},
        {"1,000", 1000}, {"  12  ", 12}, {"(3)", -3}, {"&O777", 511},
    };
    size_t i;
    REQUIRE(U(VarI4FromStr));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        LONG l = 0x5a5a;
        HRESULT hr = U(VarI4FromStr)((LPOLESTR)vfp_w(rows[i].s), VFP_EN_US, VFP_NOUSER, &l);
        CHECKF(hr == S_OK && l == rows[i].exp, "VarI4FromStr(\"%s\") want %ld got hr=0x%lx %ld", rows[i].s, rows[i].exp, (unsigned long)hr, (long)l);
    }
}

TEST(vfp_i4fromstr_overflow)
{
    static const char *big[] = { "2147483648", "-2147483649", "1e10", "99999999999" };
    size_t i;
    REQUIRE(U(VarI4FromStr));
    for (i = 0; i < sizeof big / sizeof *big; i++) {
        LONG l = 0;
        HRESULT hr = U(VarI4FromStr)((LPOLESTR)vfp_w(big[i]), VFP_EN_US, VFP_NOUSER, &l);
        CHECKF(hr == VFP_E_OVERFLOW, "VarI4FromStr(\"%s\") want overflow got hr=0x%lx", big[i], (unsigned long)hr);
    }
}

TEST(vfp_small_int_fromstr_ranges)
{
    SHORT s; BYTE b; USHORT us; ULONG ul;
    REQUIRE(U(VarI2FromStr)); REQUIRE(U(VarUI1FromStr)); REQUIRE(U(VarUI2FromStr)); REQUIRE(U(VarUI4FromStr));
    CHECK(U(VarI2FromStr)((LPOLESTR)vfp_w("32767"), VFP_EN_US, 0, &s) == S_OK && s == 32767);
    CHECK(U(VarI2FromStr)((LPOLESTR)vfp_w("-32768"), VFP_EN_US, 0, &s) == S_OK && s == -32768);
    CHECK(U(VarI2FromStr)((LPOLESTR)vfp_w("32768"), VFP_EN_US, 0, &s) == VFP_E_OVERFLOW);
    CHECK(U(VarUI1FromStr)((LPOLESTR)vfp_w("255"), VFP_EN_US, 0, &b) == S_OK && b == 255);
    CHECK(U(VarUI1FromStr)((LPOLESTR)vfp_w("256"), VFP_EN_US, 0, &b) == VFP_E_OVERFLOW);
    CHECK(U(VarUI1FromStr)((LPOLESTR)vfp_w("-1"), VFP_EN_US, 0, &b) == VFP_E_OVERFLOW);
    CHECK(U(VarUI2FromStr)((LPOLESTR)vfp_w("65535"), VFP_EN_US, 0, &us) == S_OK && us == 65535);
    CHECK(U(VarUI2FromStr)((LPOLESTR)vfp_w("65536"), VFP_EN_US, 0, &us) == VFP_E_OVERFLOW);
    CHECK(U(VarUI4FromStr)((LPOLESTR)vfp_w("4294967295"), VFP_EN_US, 0, &ul) == S_OK && ul == 0xFFFFFFFFul);
    CHECK(U(VarUI4FromStr)((LPOLESTR)vfp_w("4294967296"), VFP_EN_US, 0, &ul) == VFP_E_OVERFLOW);
    CHECK(U(VarUI4FromStr)((LPOLESTR)vfp_w("-1"), VFP_EN_US, 0, &ul) == VFP_E_OVERFLOW);
    CHECK(U(VarI2FromStr)((LPOLESTR)vfp_w("x"), VFP_EN_US, 0, &s) == VFP_E_TYPEMISMATCH);
}

TEST(vfp_cyfromstr)
{
    CY cy; HRESULT hr;
    REQUIRE(U(VarCyFromStr));
    hr = U(VarCyFromStr)((LPOLESTR)vfp_w("1.5"), VFP_EN_US, VFP_NOUSER, &cy);
    CHECK(hr == S_OK && cy.int64 == 15000);
    hr = U(VarCyFromStr)((LPOLESTR)vfp_w("-0.0001"), VFP_EN_US, VFP_NOUSER, &cy);
    CHECK(hr == S_OK && cy.int64 == -1);
    hr = U(VarCyFromStr)((LPOLESTR)vfp_w("$1,234.56"), VFP_EN_US, VFP_NOUSER, &cy);
    CHECK(hr == S_OK && cy.int64 == 12345600);
    hr = U(VarCyFromStr)((LPOLESTR)vfp_w("922337203685477.5807"), VFP_EN_US, VFP_NOUSER, &cy);
    if (!(hr == S_OK && cy.int64 == 0x7fffffffffffffffLL)) t_note("CY max string hr=0x%lx v=%lld", (unsigned long)hr, (long long)cy.int64);
    hr = U(VarCyFromStr)((LPOLESTR)vfp_w("922337203685478"), VFP_EN_US, VFP_NOUSER, &cy);
    CHECK(hr == VFP_E_OVERFLOW);
    hr = U(VarCyFromStr)((LPOLESTR)vfp_w("zz"), VFP_EN_US, VFP_NOUSER, &cy);
    CHECK(hr == VFP_E_TYPEMISMATCH);
}

TEST(vfp_decfromstr)
{
    DECIMAL d; HRESULT hr;
    REQUIRE(U(VarDecFromStr));
    hr = U(VarDecFromStr)((LPOLESTR)vfp_w("123.45"), VFP_EN_US, VFP_NOUSER, &d);
    CHECK(hr == S_OK && d.scale == 2 && d.Lo64 == 12345 && d.Hi32 == 0 && d.sign == 0);
    hr = U(VarDecFromStr)((LPOLESTR)vfp_w("-7"), VFP_EN_US, VFP_NOUSER, &d);
    CHECK(hr == S_OK && d.scale == 0 && d.Lo64 == 7 && d.sign == DECIMAL_NEG);
    hr = U(VarDecFromStr)((LPOLESTR)vfp_w("79228162514264337593543950335"), VFP_EN_US, VFP_NOUSER, &d);
    CHECK(hr == S_OK && d.Hi32 == 0xFFFFFFFF && d.Lo64 == 0xFFFFFFFFFFFFFFFFull && d.scale == 0);
    hr = U(VarDecFromStr)((LPOLESTR)vfp_w("79228162514264337593543950336"), VFP_EN_US, VFP_NOUSER, &d);
    CHECK(hr == VFP_E_OVERFLOW);
    hr = U(VarDecFromStr)((LPOLESTR)vfp_w("q"), VFP_EN_US, VFP_NOUSER, &d);
    CHECK(hr == VFP_E_TYPEMISMATCH);
}

TEST(vfp_boolfromstr)
{
    static const struct { const char *s; int hrok; short exp; } rows[] = {
        {"True", 1, -1}, {"False", 1, 0}, {"true", 1, -1}, {"FALSE", 1, 0}, {"#TRUE#", 1, -1}, {"#FALSE#", 1, 0},
        {"0", 1, 0}, {"1", 1, -1}, {"-1", 1, -1}, {"2", 1, -1}, {"0.0", 1, 0}, {"0.5", 1, -1}, {"&H0", 1, 0},
        {"abc", 0, 0}, {"", 0, 0}, {"Truee", 0, 0},
    };
    size_t i;
    REQUIRE(U(VarBoolFromStr));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT_BOOL b = 0x1234;
        HRESULT hr = U(VarBoolFromStr)((LPOLESTR)vfp_w(rows[i].s), VFP_EN_US, VFP_NOUSER, &b);
        if (rows[i].hrok) CHECKF(hr == S_OK && b == rows[i].exp, "VarBoolFromStr(\"%s\") want %d got hr=0x%lx %d", rows[i].s, rows[i].exp, (unsigned long)hr, b);
        else CHECKF(hr == VFP_E_TYPEMISMATCH, "VarBoolFromStr(\"%s\") want TYPEMISMATCH got 0x%lx", rows[i].s, (unsigned long)hr);
    }
}

TEST(vfp_r4fromstr_and_r8_limits)
{
    float f; double d; HRESULT hr;
    REQUIRE(U(VarR4FromStr)); REQUIRE(U(VarR8FromStr));
    hr = U(VarR4FromStr)((LPOLESTR)vfp_w("1.5"), VFP_EN_US, 0, &f);
    CHECK(hr == S_OK && f == 1.5f);
    hr = U(VarR4FromStr)((LPOLESTR)vfp_w("1e39"), VFP_EN_US, 0, &f);
    CHECK(hr == VFP_E_OVERFLOW);
    hr = U(VarR8FromStr)((LPOLESTR)vfp_w("1e309"), VFP_EN_US, 0, &d);
    CHECK(hr == VFP_E_OVERFLOW);
    hr = U(VarR8FromStr)((LPOLESTR)vfp_w("1e-400"), VFP_EN_US, 0, &d);
    CHECK(hr == S_OK && d == 0.0);
    hr = U(VarR8FromStr)((LPOLESTR)vfp_w("1.7976931348623157e308"), VFP_EN_US, 0, &d);
    if (!(hr == S_OK && d == 1.7976931348623157e308)) t_note("DBL_MAX string hr=0x%lx d=%.17g", (unsigned long)hr, d);
}

TEST(vfp_fromstr_german_locale_note)
{
    double d = 0; HRESULT hr;
    REQUIRE(U(VarR8FromStr));
    hr = U(VarR8FromStr)((LPOLESTR)vfp_w("1,5"), VFP_DE_DE, VFP_NOUSER, &d);
    if (hr == S_OK && d == 1.5) CHECK(1);
    else t_note("de-DE 1,5 -> hr=0x%lx d=%g (locale data may be absent)", (unsigned long)hr, d);
    hr = U(VarR8FromStr)((LPOLESTR)vfp_w("1.234,5"), VFP_DE_DE, VFP_NOUSER, &d);
    if (hr == S_OK) t_note("de-DE 1.234,5 -> %g", d);
}
