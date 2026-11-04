/* Currency (CY, int64 scaled by 10000) specifics.  All expectations from the Win10 decompilation of
 * VarCyFromR8/VarR8FromCy/VarI8FromCy/VarI4FromCy/VarCyFromI8/VarCyFromUI8. */
#include "vcn.h"

static CY mkcy(LONGLONG v) { CY c; c.int64 = v; return c; }

/* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarCyFromR8 / test_VarI8FromCy (LGPL-2.1+), extended */
TEST(vcn_cy_from_r8_exact_binary_ties)
{
    REQUIRE(U(VarCyFromR8));
    /* x = k/32 -> x*10000 = k*312.5: exact binary values, ties for odd k, resolved half-to-even */
    for (int k = -64; k <= 64; k++) {
        double x = k / 32.0;
        LONGLONG want2 = (LONGLONG)k * 625;                  /* x*10000*2 = k*625, exact */
        LONGLONG q = want2 / 2, r = want2 % 2;
        LONGLONG want = q;
        if (r != 0) {                                          /* tie: round half to even */
            LONGLONG away = q + (want2 > 0 ? 1 : -1);
            want = (q % 2 == 0) ? q : away;
        }
        CY c; memset(&c, 0xCC, sizeof c);
        HRESULT hr = U(VarCyFromR8)(x, &c);
        CHECKF(hr == S_OK, "k=%d hr=0x%lx", k, (unsigned long)hr);
        CHECKF(c.int64 == want, "k=%d x=%g got %lld want %lld", k, x, (long long)c.int64, (long long)want);
    }
}

TEST(vcn_cy_from_r8_decimal_literals)
{
    REQUIRE(U(VarCyFromR8)); REQUIRE(U(VarR8FromCy));
    static const struct { double x; LONGLONG cy; } t[] = {
        { 0.0, 0 }, { 0.0001, 1 }, { -0.0001, -1 }, { 0.5, 5000 }, { 1.0, 10000 }, { 1.2345, 12345 }, { -1.2345, -12345 },
        { 99.9999, 999999 }, { 123456.789, 1234567890 }, { 0.1, 1000 }, { 0.3, 3000 }, { 922337203685477.0, 9223372036854770000LL },
        { 1e14, 1000000000000000000LL }, { -1e14, -1000000000000000000LL }, { 12345.6789, 123456789 }, { 4.2, 42000 },
        { 0.0002, 2 }, { 0.00014, 1 }, { 0.00016, 2 }, { -0.00016, -2 }, { 2.00004, 20000 }, { 2.00006, 20001 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        CY c; HRESULT hr = U(VarCyFromR8)(t[i].x, &c);
        CHECKF(hr == S_OK && c.int64 == t[i].cy, "x=%.17g hr=0x%lx got %lld want %lld", t[i].x, (unsigned long)hr, (long long)c.int64, (long long)t[i].cy);
        if (t[i].x == 0.00014 || t[i].x == 0.00016 || t[i].x == -0.00016 || t[i].x == 2.00006 || t[i].x == 2.00004 || t[i].x == 0.0002) continue;
        double back = 0; hr = U(VarR8FromCy)(mkcy(t[i].cy), &back);
        /* R8FromCy is (double)int64 / 10000.0: correctly rounded quotient == nearest double of the decimal */
        CHECKF(hr == S_OK && back == t[i].x, "cy=%lld back=%.17g want %.17g", (long long)t[i].cy, back, t[i].x);
    }
}

TEST(vcn_cy_from_r8_range_edges)
{
    REQUIRE(U(VarCyFromR8)); REQUIRE(U(VarR8FromCy));
    CY c;
    /* the limit is the double nearest 922337203685477.5807 (= 922337203685477.625), exclusive on both sides */
    const double bnd = 922337203685477.5807;
    CHECK(bnd == 922337203685477.625);
    CY mx = mkcy(0x7fffffffffffffffLL); double d = 0;
    CHECK(U(VarR8FromCy)(mx, &d) == S_OK && d == bnd);
    memset(&c, 0xCC, sizeof c);
    EXPECT_EQ(U(VarCyFromR8)(bnd, &c), DISP_E_OVERFLOW);
    CHECK(vcn_untouched(&c, sizeof c));
    EXPECT_EQ(U(VarCyFromR8)(-bnd, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromR8)(922337203685477.5, &c), S_OK);
    EXPECT_EQ(c.int64, 9223372036854775000LL);
    EXPECT_EQ(U(VarCyFromR8)(-922337203685477.5, &c), S_OK);
    EXPECT_EQ(c.int64, -9223372036854775000LL);
    EXPECT_EQ(U(VarCyFromR8)(nextafter(bnd, 0.0), &c), S_OK);
    EXPECT_EQ(U(VarCyFromR8)(nextafter(bnd, 1e300), &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromR8)(INFINITY, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromR8)(-INFINITY, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromR8)(NAN, &c), S_OK);   /* Win10: NaN passes the range test and yields 0x8000000000000000 */
    EXPECT_EQ(c.int64, (LONGLONG)0x8000000000000000ULL);
    /* VarCyFromDate is the same code (one RVA, two ordinals) */
    if (U(VarCyFromDate)) {
        EXPECT_EQ(U(VarCyFromDate)(bnd, &c), DISP_E_OVERFLOW);
        EXPECT_EQ(U(VarCyFromDate)(0.5, &c), S_OK);
        EXPECT_EQ(c.int64, 5000);
    }
}

TEST(vcn_cy_i8_ui8_integer_limits)
{
    REQUIRE(U(VarCyFromI8)); REQUIRE(U(VarCyFromUI8)); REQUIRE(U(VarCyFromUI4));
    CY c;
    /* VarCyFromI8 accepts |v| <= 922337203685476 (the lower/upper limits are 0x346dc5d638864 based) */
    EXPECT_EQ(U(VarCyFromI8)(922337203685476LL, &c), S_OK);
    EXPECT_EQ(c.int64, 9223372036854760000LL);
    EXPECT_EQ(U(VarCyFromI8)(-922337203685476LL, &c), S_OK);
    EXPECT_EQ(c.int64, -9223372036854760000LL);
    EXPECT_EQ(U(VarCyFromI8)(922337203685477LL, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromI8)(-922337203685477LL, &c), DISP_E_OVERFLOW);   /* even though -922337203685477.5808 is a valid CY */
    EXPECT_EQ(U(VarCyFromI8)(0x7fffffffffffffffLL, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromI8)((-0x7fffffffffffffffLL) - 1, &c), DISP_E_OVERFLOW);
    /* UI8 goes through the double route: limit is the double 922337203685477.625 */
    EXPECT_EQ(U(VarCyFromUI8)(922337203685477ULL, &c), S_OK);
    EXPECT_EQ(c.int64, 9223372036854770000LL);
    EXPECT_EQ(U(VarCyFromUI8)(922337203685478ULL, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromUI8)(0xffffffffffffffffULL, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromUI4)(0xffffffffUL, &c), S_OK);
    EXPECT_EQ(c.int64, 42949672950000LL);
}

TEST(vcn_i8_from_cy_win32_negative_bug)
{
    REQUIRE(U(VarI8FromCy)); REQUIRE(U(VarI4FromCy));
    /* VarI8FromCy keeps the old Win32 bug: the remainder test uses the unsigned low dword, so EVERY
     * negative value is rounded one further down (-1.0 -> -2).  VarI4FromCy does not have it.
     * adapted from Wine dlls/oleaut32/tests/vartype.c test_VarI8FromCy (LGPL-2.1+); the decomp agrees */
    static const struct { LONGLONG cy; LONG64 i8; LONG i4; } t[] = {
        { -1280000, -129, -128 }, { -10000, -2, -1 }, { 0, 0, 0 }, { 10000, 1, 1 }, { 1270000, 127, 127 },
        { -15000, -2, -2 }, { -6000, -1, -1 }, { -5000, -1, 0 }, { -4000, -1, 0 }, { 4000, 0, 0 },
        { 5000, 0, 0 }, { 6000, 1, 1 }, { 15000, 2, 2 }, { 25000, 2, 2 }, { 35000, 4, 4 }, { -25000, -3, -2 },
        { -1, -1, 0 }, { 1, 0, 0 }, { 9999, 1, 1 }, { -9999, -1, -1 }, { -20000, -3, -2 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        LONG64 o8 = 0x1234; LONG o4 = 0x1234;
        HRESULT h8 = U(VarI8FromCy)(mkcy(t[i].cy), &o8);
        HRESULT h4 = U(VarI4FromCy)(mkcy(t[i].cy), &o4);
        CHECKF(h8 == S_OK && o8 == t[i].i8, "I8FromCy(%lld) hr=0x%lx got %lld want %lld", (long long)t[i].cy, (unsigned long)h8, (long long)o8, (long long)t[i].i8);
        CHECKF(h4 == S_OK && o4 == t[i].i4, "I4FromCy(%lld) hr=0x%lx got %ld want %ld", (long long)t[i].cy, (unsigned long)h4, (long)o4, (long)t[i].i4);
    }
}

TEST(vcn_cy_to_small_ints_round_half_even)
{
    REQUIRE(U(VarI2FromCy)); REQUIRE(U(VarUI1FromCy)); REQUIRE(U(VarI1FromCy)); REQUIRE(U(VarUI2FromCy)); REQUIRE(U(VarI4FromCy));
    static const struct { LONGLONG cy; int i4, ui1, i1, i2, ui2; } t[] = {   /* -99999 == overflow */
        { 5000, 0, 0, 0, 0, 0 }, { 15000, 2, 2, 2, 2, 2 }, { 25000, 2, 2, 2, 2, 2 }, { 35000, 4, 4, 4, 4, 4 },
        { -5000, 0, 0, 0, 0, 0 }, { -5001, -1, -99999, -1, -1, -99999 }, { 2555000, 256, -99999, -99999, 256, 256 },
        { 2545000, 254, 254, -99999, 254, 254 }, { 1275000, 128, 128, -99999, 128, 128 }, { 1265000, 126, 126, 126, 126, 126 },
        { -1285000, -128, -99999, -128, -128, -99999 }, { -1285001, -129, -99999, -99999, -129, -99999 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        LONG a; BYTE b; CHAR c; SHORT d; USHORT e;
        HRESULT ha = U(VarI4FromCy)(mkcy(t[i].cy), &a);
        HRESULT hb = U(VarUI1FromCy)(mkcy(t[i].cy), &b);
        HRESULT hc = U(VarI1FromCy)(mkcy(t[i].cy), &c);
        HRESULT hd = U(VarI2FromCy)(mkcy(t[i].cy), &d);
        HRESULT he = U(VarUI2FromCy)(mkcy(t[i].cy), &e);
        CHECKF(ha == S_OK && a == t[i].i4, "I4FromCy %lld hr=%lx got %ld", (long long)t[i].cy, (unsigned long)ha, (long)a);
        if (t[i].ui1 == -99999) CHECKF(hb == DISP_E_OVERFLOW, "UI1FromCy %lld hr=%lx", (long long)t[i].cy, (unsigned long)hb);
        else CHECKF(hb == S_OK && b == t[i].ui1, "UI1FromCy %lld hr=%lx got %d", (long long)t[i].cy, (unsigned long)hb, b);
        if (t[i].i1 == -99999) CHECKF(hc == DISP_E_OVERFLOW, "I1FromCy %lld hr=%lx", (long long)t[i].cy, (unsigned long)hc);
        else CHECKF(hc == S_OK && c == t[i].i1, "I1FromCy %lld hr=%lx got %d", (long long)t[i].cy, (unsigned long)hc, c);
        CHECKF(hd == S_OK && d == t[i].i2, "I2FromCy %lld hr=%lx got %d", (long long)t[i].cy, (unsigned long)hd, d);
        if (t[i].ui2 == -99999) CHECKF(he == DISP_E_OVERFLOW, "UI2FromCy %lld hr=%lx", (long long)t[i].cy, (unsigned long)he);
        else CHECKF(he == S_OK && e == t[i].ui2, "UI2FromCy %lld hr=%lx got %d", (long long)t[i].cy, (unsigned long)he, e);
    }
}

TEST(vcn_cy_r8_r4_date_from_cy_exact)
{
    REQUIRE(U(VarR8FromCy)); REQUIRE(U(VarR4FromCy)); REQUIRE(U(VarDateFromCy));
    static const LONGLONG t[] = { 0, 1, -1, 5000, 12345, -12345, 123456789, 9999, 10001, 1234567890123LL, 0x7fffffffffffffffLL, (-0x7fffffffffffffffLL) - 1, 4294967296LL, 77 };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        double d = 0; float f = 0; DATE dt = 0;
        EXPECT_EQ(U(VarR8FromCy)(mkcy(t[i]), &d), S_OK);
        CHECKF(d == (double)t[i] / 10000.0, "R8FromCy(%lld)=%.17g", (long long)t[i], d);
        EXPECT_EQ(U(VarR4FromCy)(mkcy(t[i]), &f), S_OK);
        CHECKF(f == (float)((double)t[i] / 10000.0), "R4FromCy(%lld)=%.9g", (long long)t[i], f);
        HRESULT hr = U(VarDateFromCy)(mkcy(t[i]), &dt);
        double v = (double)t[i] / 10000.0;
        if (v > -657435.0 && v < 2958466.0) CHECKF(hr == S_OK && dt == v, "DateFromCy(%lld) hr=%lx", (long long)t[i], (unsigned long)hr);
        else CHECKF(hr == DISP_E_OVERFLOW, "DateFromCy(%lld) hr=%lx", (long long)t[i], (unsigned long)hr);
    }
}

TEST(vcn_cy_dec_roundtrip)
{
    REQUIRE(U(VarDecFromCy)); REQUIRE(U(VarCyFromDec));
    static const LONGLONG t[] = { 0, 1, -1, 10000, -10000, 123456789, -123456789, 0x7fffffffffffffffLL, (-0x7fffffffffffffffLL) - 1, 4294967296LL, -4294967296LL };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        DECIMAL d; memset(&d, 0xCC, sizeof d);
        EXPECT_EQ(U(VarDecFromCy)(mkcy(t[i]), &d), S_OK);
        CHECKF(d.scale == 4, "scale %d", d.scale);
        CHECKF(d.sign == (t[i] < 0 ? 0x80 : 0), "sign 0x%x for %lld", d.sign, (long long)t[i]);
        CHECKF(d.Hi32 == 0, "Hi32 0x%lx", (unsigned long)d.Hi32);
        unsigned long long mag = t[i] < 0 ? 0ULL - (unsigned long long)t[i] : (unsigned long long)t[i];
        CHECKF(d.Lo64 == mag, "Lo64 %llx want %llx", (unsigned long long)d.Lo64, mag);
        CY c; c.int64 = 0x5555;
        EXPECT_EQ(U(VarCyFromDec)(&d, &c), S_OK);
        CHECKF(c.int64 == t[i], "roundtrip %lld -> %lld", (long long)t[i], (long long)c.int64);
    }
}
