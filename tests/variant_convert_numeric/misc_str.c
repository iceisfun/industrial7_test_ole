/* String parsing details that the generated tables do not express: VB hex/octal accumulation in
 * VarI4FromStr/VarI8FromStr (bit-length limits taken from the decomp), digit limits, locale separators. */
#include "vcn.h"

#define NOUO 0x80000000UL

TEST(vcn_i4fromstr_hex_oct_bit_limits)
{
    REQUIRE(U(VarI4FromStr));
    /* decomp: nBaseShift*cDig must be <= 33 bits; at exactly 33 bits (11 octal digits) the first digit must be <= 3;
     * the accumulated 32-bit pattern is returned as-is, so &HFFFFFFFF is -1 and the sign is NOT range checked. */
    static const struct { const WCHAR *s; HRESULT hr; LONG v; } t[] = {
        { L"&H0", S_OK, 0 }, { L"&H1", S_OK, 1 }, { L"&HFF", S_OK, 255 }, { L"&H7FFFFFFF", S_OK, 0x7fffffff },
        { L"&H80000000", S_OK, (LONG)0x80000000UL }, { L"&HFFFFFFFF", S_OK, -1 }, { L"&HFFFFFFFE", S_OK, -2 },
        { L"&H100000000", DISP_E_OVERFLOW, 0 }, { L"&H1FFFFFFFF", DISP_E_OVERFLOW, 0 }, { L"&HFFFFFFFFF", DISP_E_OVERFLOW, 0 },
        { L"&O0", S_OK, 0 }, { L"&O7", S_OK, 7 }, { L"&O10", S_OK, 8 }, { L"&O37777777777", S_OK, -1 },
        { L"&O17777777777", S_OK, 0x7fffffff }, { L"&O20000000000", S_OK, (LONG)0x80000000UL },
        { L"&O40000000000", DISP_E_OVERFLOW, 0 }, { L"&O77777777777", DISP_E_OVERFLOW, 0 }, { L"&O400000000000", DISP_E_TYPEMISMATCH, 0 },   /* 12 octal digits: Win10 gives TYPEMISMATCH, not OVERFLOW */
        { L"&hff", S_OK, 255 }, { L"&Hff", S_OK, 255 }, { L"&hFf", S_OK, 255 }, { L"&o17", S_OK, 15 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        LONG v = 0x1234;
        HRESULT hr = U(VarI4FromStr)(t[i].s, 0x409, NOUO, &v);
        CHECKF(hr == t[i].hr, "'%ls': hr=0x%08lx want 0x%08lx", t[i].s, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK && t[i].hr == S_OK) CHECKF(v == t[i].v, "'%ls': got %ld want %ld", t[i].s, (long)v, (long)t[i].v);
        if (t[i].hr != S_OK && hr == t[i].hr) CHECKF(v == 0x1234, "'%ls': output written on failure", t[i].s);
    }
}

TEST(vcn_i8fromstr_hex_oct_bit_limits)
{
    REQUIRE(U(VarI8FromStr));
    static const struct { const WCHAR *s; HRESULT hr; LONG64 v; } t[] = {
        { L"&H0", S_OK, 0 }, { L"&HFF", S_OK, 255 }, { L"&H7FFFFFFFFFFFFFFF", S_OK, 0x7fffffffffffffffLL },
        { L"&H8000000000000000", S_OK, (-0x7fffffffffffffffLL) - 1 }, { L"&HFFFFFFFFFFFFFFFF", S_OK, -1 },
        { L"&H100000000", S_OK, 0x100000000LL }, { L"&O777", S_OK, 511 }, { L"&O1777777777777777777777", DISP_E_OVERFLOW, 0 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        LONG64 v = 0x1234;
        HRESULT hr = U(VarI8FromStr)(t[i].s, 0x409, NOUO, &v);
        if (i == 7) { t_note("VarI8FromStr('%ls') -> 0x%08lx", t[i].s, (unsigned long)hr); continue; }
        CHECKF(hr == t[i].hr, "'%ls': hr=0x%08lx want 0x%08lx", t[i].s, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK && t[i].hr == S_OK) CHECKF(v == t[i].v, "'%ls': got %lld want %lld", t[i].s, (long long)v, (long long)t[i].v);
    }
    /* decimal path: > 19 integer digits overflow immediately, 19 digits are range checked via the sign bit */
    LONG64 v;
    EXPECT_EQ(U(VarI8FromStr)(L"9223372036854775807", 0x409, NOUO, &v), S_OK); EXPECT_EQ(v, 0x7fffffffffffffffLL);
    EXPECT_EQ(U(VarI8FromStr)(L"9223372036854775808", 0x409, NOUO, &v), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI8FromStr)(L"-9223372036854775808", 0x409, NOUO, &v), S_OK); EXPECT_EQ(v, (-0x7fffffffffffffffLL) - 1);
    EXPECT_EQ(U(VarI8FromStr)(L"-9223372036854775809", 0x409, NOUO, &v), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI8FromStr)(L"10000000000000000000", 0x409, NOUO, &v), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI8FromStr)(L"1e18", 0x409, NOUO, &v), S_OK); EXPECT_EQ(v, 1000000000000000000LL);
    EXPECT_EQ(U(VarI8FromStr)(L"1e19", 0x409, NOUO, &v), DISP_E_OVERFLOW);
    /* 17 digits, pwr10 makes 19 integer digits: 9223372036854775800 still fits */
    EXPECT_EQ(U(VarI8FromStr)(L"9.2233720368547758e18", 0x409, NOUO, &v), S_OK); EXPECT_EQ(v, 9223372036854775800LL);
    EXPECT_EQ(U(VarI8FromStr)(L"9.2233720368547759e18", 0x409, NOUO, &v), DISP_E_OVERFLOW);
}

TEST(vcn_i4fromstr_exponent_and_rounding_paths)
{
    REQUIRE(U(VarI4FromStr));
    static const struct { const WCHAR *s; HRESULT hr; LONG v; } t[] = {
        { L"2147483647", S_OK, 2147483647 }, { L"2147483648", DISP_E_OVERFLOW, 0 }, { L"21474836470", DISP_E_OVERFLOW, 0 },
        { L"-2147483648", S_OK, (LONG)0x80000000UL }, { L"-2147483649", DISP_E_OVERFLOW, 0 },
        { L"2147483647.4", S_OK, 2147483647 }, { L"2147483647.5", DISP_E_OVERFLOW, 0 }, { L"2147483646.5", S_OK, 2147483646 },
        { L"-2147483648.5", S_OK, (LONG)0x80000000UL }, { L"-2147483648.6", DISP_E_OVERFLOW, 0 },
        { L"214748364.7e1", S_OK, 2147483647 }, { L"214748364.75e1", DISP_E_OVERFLOW, 0 }, { L"2147483647e0", S_OK, 2147483647 },
        { L"0.5", S_OK, 0 }, { L"1.5", S_OK, 2 }, { L"2.5", S_OK, 2 }, { L"3.5", S_OK, 4 }, { L"-0.5", S_OK, 0 }, { L"-1.5", S_OK, -2 }, { L"-2.5", S_OK, -2 },
        { L"0.50", S_OK, 0 }, { L"2.500", S_OK, 2 }, { L"0.5000000", S_OK, 0 }, { L"1e-1", S_OK, 0 }, { L"5e-1", S_OK, 0 }, { L"15e-1", S_OK, 2 },
        { L"25e-1", S_OK, 2 }, { L"0.51", S_OK, 1 }, { L"0.49", S_OK, 0 }, { L"2.51", S_OK, 3 }, { L"2.49", S_OK, 2 },
        { L"1e10", DISP_E_OVERFLOW, 0 }, { L"1e9", S_OK, 1000000000 }, { L"1e-9", S_OK, 0 }, { L"0.000000001", S_OK, 0 },
        { L"00000000000000000012", S_OK, 12 }, { L"000.0000000000000012", S_OK, 0 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        LONG v = 0x1234;
        HRESULT hr = U(VarI4FromStr)(t[i].s, 0x409, NOUO, &v);
        CHECKF(hr == t[i].hr, "'%ls': hr=0x%08lx want 0x%08lx", t[i].s, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK && t[i].hr == S_OK) CHECKF(v == t[i].v, "'%ls': got %ld want %ld", t[i].s, (long)v, (long)t[i].v);
    }
}

TEST(vcn_narrow_ints_chain_through_i2_or_i4)
{
    /* VarI1FromStr / VarUI1FromStr go through VarI2FromStr, VarUI2FromStr through VarI4FromStr, so the first range
     * error always comes from the wider type (all are DISP_E_OVERFLOW) */
    REQUIRE(U(VarI1FromStr)); REQUIRE(U(VarUI1FromStr)); REQUIRE(U(VarUI2FromStr)); REQUIRE(U(VarI2FromStr));
    CHAR c; BYTE b; USHORT w; SHORT s;
    EXPECT_EQ(U(VarI1FromStr)(L"127", 0x409, NOUO, &c), S_OK); EXPECT_EQ(c, 127);
    EXPECT_EQ(U(VarI1FromStr)(L"128", 0x409, NOUO, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI1FromStr)(L"-128", 0x409, NOUO, &c), S_OK); EXPECT_EQ(c, -128);
    EXPECT_EQ(U(VarI1FromStr)(L"-129", 0x409, NOUO, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI1FromStr)(L"70000", 0x409, NOUO, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI1FromStr)(L"255", 0x409, NOUO, &b), S_OK); EXPECT_EQ(b, 255);
    EXPECT_EQ(U(VarUI1FromStr)(L"256", 0x409, NOUO, &b), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI1FromStr)(L"-1", 0x409, NOUO, &b), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI1FromStr)(L"254.5", 0x409, NOUO, &b), S_OK); EXPECT_EQ(b, 254);
    EXPECT_EQ(U(VarUI1FromStr)(L"255.5", 0x409, NOUO, &b), DISP_E_OVERFLOW);        /* 255.5 -> 256 (even) -> overflow */
    EXPECT_EQ(U(VarUI1FromStr)(L"255.4", 0x409, NOUO, &b), S_OK); EXPECT_EQ(b, 255);
    EXPECT_EQ(U(VarUI1FromStr)(L"-0.5", 0x409, NOUO, &b), S_OK); EXPECT_EQ(b, 0);
    EXPECT_EQ(U(VarUI2FromStr)(L"65535", 0x409, NOUO, &w), S_OK); EXPECT_EQ(w, 65535);
    EXPECT_EQ(U(VarUI2FromStr)(L"65536", 0x409, NOUO, &w), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI2FromStr)(L"-1", 0x409, NOUO, &w), DISP_E_OVERFLOW);            /* (uint)-1 is not < 0x10000 */
    EXPECT_EQ(U(VarUI2FromStr)(L"-0", 0x409, NOUO, &w), S_OK); EXPECT_EQ(w, 0);
    EXPECT_EQ(U(VarUI2FromStr)(L"4294967295", 0x409, NOUO, &w), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI2FromStr)(L"32767", 0x409, NOUO, &s), S_OK); EXPECT_EQ(s, 32767);
    EXPECT_EQ(U(VarI2FromStr)(L"32768", 0x409, NOUO, &s), DISP_E_OVERFLOW);
}

TEST(vcn_r8fromstr_extremes)
{
    REQUIRE(U(VarR8FromStr)); REQUIRE(U(VarR4FromStr));
    double d; float f;
    /* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarR8FromStr (LGPL-2.1+) */
    d = 5; EXPECT_EQ(U(VarR8FromStr)(L"1e-94938484", 0x409, NOUO, &d), S_OK); CHECK(d == 0.0);
    d = 5; EXPECT_EQ(U(VarR8FromStr)(L"", 0x409, NOUO, &d), DISP_E_TYPEMISMATCH); CHECK(d == 5);
    d = 5; EXPECT_EQ(U(VarR8FromStr)(L" ", 0x409, NOUO, &d), DISP_E_TYPEMISMATCH); CHECK(d == 5);
    d = 5; EXPECT_EQ(U(VarR8FromStr)(NULL, 0x409, NOUO, &d), DISP_E_TYPEMISMATCH); CHECK(d == 5);
    d = 5; HRESULT hr = U(VarR8FromStr)(L"1.7976931348623157e308", 0x409, NOUO, &d);
    t_note("VarR8FromStr(DBL_MAX text) -> 0x%08lx value %.17g", (unsigned long)hr, d);
    d = 5; EXPECT_EQ(U(VarR8FromStr)(L"1e400", 0x409, NOUO, &d), DISP_E_OVERFLOW); CHECK(d == 5);
    d = 5; EXPECT_EQ(U(VarR8FromStr)(L"-1e400", 0x409, NOUO, &d), DISP_E_OVERFLOW);
    f = 5; EXPECT_EQ(U(VarR4FromStr)(L"3.4028234e38", 0x409, NOUO, &f), S_OK);
    f = 5; EXPECT_EQ(U(VarR4FromStr)(L"1e39", 0x409, NOUO, &f), DISP_E_OVERFLOW); CHECK(f == 5);
    f = 5; hr = U(VarR4FromStr)(L"1e-50", 0x409, NOUO, &f); t_note("VarR4FromStr('1e-50') -> 0x%08lx value %g", (unsigned long)hr, f);
    d = 5; EXPECT_EQ(U(VarR8FromStr)(L"123456789012345678901234", 0x409, NOUO, &d), S_OK);
    CHECKF(fabs(d / 1.2345678901234568e23 - 1.0) < 1e-12, "24 digit value parsed as %.17g", d);
}

TEST(vcn_decfromstr_layout)
{
    REQUIRE(U(VarDecFromStr));
    /* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarDecFromStr (LGPL-2.1+) */
    static const struct { const WCHAR *s; int sc, sg; ULONG hi, mid, lo; } t[] = {
        { L"-1", 0, 0x80, 0, 0, 1 }, { L"0", 0, 0, 0, 0, 0 }, { L"1", 0, 0, 0, 0, 1 }, { L"0.5", 1, 0, 0, 0, 5 },
        { L"4294967296", 0, 0, 0, 1, 0 }, { L"18446744073709551616", 0, 0, 1, 0, 0 }, { L"4294967296.0", 0, 0, 0, 1, 0 },
        { L"18446744073709551616.0", 0, 0, 1, 0, 0 }, { L"1.25", 2, 0, 0, 0, 125 }, { L"-1.25", 2, 0x80, 0, 0, 125 },
        { L"0.0001", 4, 0, 0, 0, 1 }, { L"123.456", 3, 0, 0, 0, 123456 }, { L"1e3", 0, 0, 0, 0, 1000 }, { L"1e-3", 3, 0, 0, 0, 1 },
        { L"79228162514264337593543950335", 0, 0, 0xffffffffUL, 0xffffffffUL, 0xffffffffUL },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        DECIMAL d; memset(&d, 0xCC, sizeof d);
        HRESULT hr = U(VarDecFromStr)(t[i].s, 0x409, NOUO, &d);
        CHECKF(hr == S_OK && d.scale == t[i].sc && d.sign == t[i].sg && d.Hi32 == t[i].hi && d.Mid32 == t[i].mid && d.Lo32 == t[i].lo,
               "'%ls': hr=0x%lx sc=%u sg=%02x %08lx:%08lx:%08lx", t[i].s, (unsigned long)hr, d.scale, d.sign, (unsigned long)d.Hi32, (unsigned long)d.Mid32, (unsigned long)d.Lo32);
    }
    DECIMAL d;
    EXPECT_EQ(U(VarDecFromStr)(NULL, 0x409, NOUO, &d), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(U(VarDecFromStr)(L"", 0x409, NOUO, &d), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(U(VarDecFromStr)(L"79228162514264337593543950336", 0x409, NOUO, &d), DISP_E_OVERFLOW);
}

TEST(vcn_locale_separators_fixed_lcids)
{
    REQUIRE(U(VarR8FromStr)); REQUIRE(U(VarI4FromStr));
    double d; LONG l;
    /* en-US: '.' decimal, ',' thousands */
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1,234.5", 0x409, NOUO, &d), S_OK); CHECK(d == 1234.5);
    /* de-DE: ',' decimal, '.' thousands */
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1.234,5", 0x407, NOUO, &d), S_OK); CHECK(d == 1234.5);
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"0,25", 0x407, NOUO, &d), S_OK); CHECK(d == 0.25);
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"0,25", 0x409, NOUO, &d), S_OK); CHECK(d == 25.0);   /* ',' is just a thousands separator in en-US */
    l = 0; EXPECT_EQ(U(VarI4FromStr)(L"2,5", 0x407, NOUO, &l), S_OK); EXPECT_EQ(l, 2);
    l = 0; EXPECT_EQ(U(VarI4FromStr)(L"3,5", 0x407, NOUO, &l), S_OK); EXPECT_EQ(l, 4);
    l = 0; EXPECT_EQ(U(VarI4FromStr)(L"1.000", 0x407, NOUO, &l), S_OK); EXPECT_EQ(l, 1000);
    l = 0; EXPECT_EQ(U(VarI4FromStr)(L"1.000", 0x409, NOUO, &l), S_OK); EXPECT_EQ(l, 1);
    /* fr-FR: ',' decimal */
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1,5", 0x40c, NOUO, &d), S_OK); CHECK(d == 1.5);
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"-0,75", 0x40c, NOUO, &d), S_OK); CHECK(d == -0.75);
    /* ja-JP: same separators as en-US */
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1,234.5", 0x411, NOUO, &d), S_OK); CHECK(d == 1234.5);
    /* LOCALE_NOUSEROVERRIDE does not change anything for an explicit LCID */
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1,5", 0x407, 0, &d), S_OK); CHECK(d == 1.5);
    /* the exponent marker is not localized */
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1,5e2", 0x407, NOUO, &d), S_OK); CHECK(d == 150.0);
    d = 0; EXPECT_EQ(U(VarR8FromStr)(L"1.5e2", 0x409, NOUO, &d), S_OK); CHECK(d == 150.0);
}
