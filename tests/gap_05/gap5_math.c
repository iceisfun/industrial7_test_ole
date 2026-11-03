/* VarR4CmpR8, VarR8Pow, VarDateFromUdateEx. Expectations verified against the Win10 oleaut32 decompilation. */
#define _USE_MATH_DEFINES
#include "t.h"
#include <oleauto.h>
#include <math.h>
#include <float.h>

#define VC_LT 0
#define VC_EQ 1
#define VC_GT 2
#define E_INVALIDARG_ 0x80070057u
#define DISP_E_OVERFLOW_ 0x8002000Au

typedef struct { const char *n; float l; double r; int want; } cmp_case;

TEST(gap5_r4cmpr8_table)
{
    static const cmp_case t[] = {
        { "1<2", 1.0f, 2.0, VC_LT }, { "2>1", 2.0f, 1.0, VC_GT }, { "1==1", 1.0f, 1.0, VC_EQ },
        { "0==0", 0.0f, 0.0, VC_EQ }, { "-0==+0", -0.0f, 0.0, VC_EQ }, { "+0==-0", 0.0f, -0.0, VC_EQ },
        { "-1<1", -1.0f, 1.0, VC_LT }, { "1>-1", 1.0f, -1.0, VC_GT },
        { "right rounds to float (1+1e-12)", 1.0f, 1.0 + 1e-12, VC_EQ },
        { "right rounds to float (1-1e-12)", 1.0f, 1.0 - 1e-12, VC_EQ },
        { "2^24+1 rounds to even 2^24", 16777216.0f, 16777217.0, VC_EQ },
        { "2^24 < 2^24+2", 16777216.0f, 16777218.0, VC_LT },
        { "0.1f vs 0.1 narrows to EQ", 0.1f, 0.1, VC_EQ },
        { "FLT_MAX vs 1e300 (narrows to inf)", FLT_MAX, 1e300, VC_LT },
        { "inf vs 1e300 (narrows to inf)", INFINITY, 1e300, VC_EQ },
        { "-inf vs -1e300", -INFINITY, -1e300, VC_EQ },
        { "inf > FLT_MAX", INFINITY, (double)FLT_MAX, VC_GT },
        { "-inf < -FLT_MAX", -INFINITY, -(double)FLT_MAX, VC_LT },
        { "denormal float equal", 1.401298464324817e-45f, 1.401298464324817e-45, VC_EQ },
        { "tiny double narrows to 0", 0.0f, 1e-300, VC_EQ },
        { "FLT_MIN > 0", FLT_MIN, 0.0, VC_GT },
    };
    unsigned i;
    REQUIRE(U(VarR4CmpR8));
    for (i = 0; i < (sizeof(t)/sizeof(t[0])); i++) {
        HRESULT hr = U(VarR4CmpR8)(t[i].l, t[i].r);
        t_check((int)hr == t[i].want, __FILE__, __LINE__, t[i].n, "got %ld want %d", (long)hr, t[i].want);
    }
}

TEST(gap5_r4cmpr8_nan_is_lt)
{
    /* decomp: result is 0 unless (float)right <= left; any NaN makes both comparisons false -> 0 (VARCMP_LT) */
    HRESULT a, b;
    REQUIRE(U(VarR4CmpR8));
    a = U(VarR4CmpR8)(NAN, 1.0);
    b = U(VarR4CmpR8)(1.0f, NAN);
    EXPECT_EQ(a, VC_LT);
    EXPECT_EQ(b, VC_LT);
}

TEST(gap5_r4cmpr8_never_returns_null)
{
    /* VARCMP_NULL (3) is for VT_NULL operands only; the real-number comparer yields 0..2 */
    static const float ls[] = { -1e30f, -1.f, 0.f, 1.f, 1e30f };
    static const double rs[] = { -1e300, -1.0, 0.0, 1.0, 1e300 };
    unsigned i, j;
    REQUIRE(U(VarR4CmpR8));
    for (i = 0; i < 5; i++) for (j = 0; j < 5; j++) {
        HRESULT hr = U(VarR4CmpR8)(ls[i], rs[j]);
        t_check(hr >= 0 && hr <= 2, __FILE__, __LINE__, "range", "l %g r %g got %ld", ls[i], rs[j], (long)hr);
    }
}

TEST(gap5_r4cmpr8_matches_narrowed_compare)
{
    static const double rs[] = { -3.5, -0.25, 0.0, 0.25, 3.5, 1e10, 0.1, 16777217.0 };
    unsigned i, j;
    REQUIRE(U(VarR4CmpR8));
    for (i = 0; i < 8; i++) for (j = 0; j < 8; j++) {
        float l = (float)rs[i], r = (float)rs[j];
        int want = l < r ? VC_LT : l > r ? VC_GT : VC_EQ;
        HRESULT a = U(VarR4CmpR8)(l, rs[j]);
        t_check((int)a == want, __FILE__, __LINE__, "R4CmpR8 == compare of narrowed", "%g %g: %ld vs %d", rs[i], rs[j], (long)a, want);
    }
}

/* ---- VarR8Pow ---- */
typedef struct { const char *n; double l, r; HRESULT hr; double want; } pow_case;

TEST(gap5_r8pow_success_table)
{
    static const pow_case t[] = {
        { "2^10", 2, 10, 0, 1024.0 }, { "2^0", 2, 0, 0, 1.0 }, { "2^1", 2, 1, 0, 2.0 }, { "2^-1", 2, -1, 0, 0.5 },
        { "10^2", 10, 2, 0, 100.0 }, { "3^3", 3, 3, 0, 27.0 }, { "4^0.5", 4, 0.5, 0, 2.0 },
        { "0^0", 0, 0, 0, 1.0 }, { "0^1", 0, 1, 0, 0.0 }, { "0^2.5", 0, 2.5, 0, 0.0 },
        { "0^-0.0", 0, -0.0, 0, 1.0 }, { "-0.0^0", -0.0, 0, 0, 1.0 },
        { "-2^3", -2, 3, 0, -8.0 }, { "-2^2", -2, 2, 0, 4.0 }, { "-2^-1", -2, -1, 0, -0.5 }, { "-2^-2", -2, -2, 0, 0.25 },
        { "-2^0", -2, 0, 0, 1.0 }, { "-1^huge even", -1, 1e300, 0, 1.0 },
        { "1^huge", 1, 1e300, 0, 1.0 }, { "1^-huge", 1, -1e300, 0, 1.0 }, { "x^0 big", 1e300, 0, 0, 1.0 },
        { "0.5^2", 0.5, 2, 0, 0.25 }, { "9^0.5", 9, 0.5, 0, 3.0 }, { "-27^3", -27, 3, 0, -19683.0 },
    };
    unsigned i;
    REQUIRE(U(VarR8Pow));
    for (i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
        double out = 777.0; HRESULT hr = U(VarR8Pow)(t[i].l, t[i].r, &out);
        t_check(hr == S_OK && out == t[i].want, __FILE__, __LINE__, t[i].n, "hr 0x%lx out %.17g want %.17g", (unsigned long)hr, out, t[i].want);
    }
}

TEST(gap5_r8pow_irrational_results)
{
    double out = 0; HRESULT hr;
    REQUIRE(U(VarR8Pow));
    hr = U(VarR8Pow)(2.0, 0.5, &out);
    EXPECT_EQ(hr, S_OK);
    CHECKF(fabs(out - 1.4142135623730951) < 1e-14, "got %.17g", out);
    hr = U(VarR8Pow)(10.0, -2.0, &out);
    EXPECT_EQ(hr, S_OK);
    CHECKF(fabs(out - 0.01) < 1e-16, "got %.17g", out);
    hr = U(VarR8Pow)(2.718281828459045, 1.0, &out);
    EXPECT_EQ(hr, S_OK);
    CHECKF(fabs(out - 2.718281828459045) < 1e-15, "got %.17g", out);
}

TEST(gap5_r8pow_zero_to_negative_is_invalidarg)
{
    static const double ex[] = { -1.0, -0.5, -2.0, -1e300, -1e-300 };
    static const double bases[] = { 0.0, -0.0 };
    unsigned i, b;
    REQUIRE(U(VarR8Pow));
    for (b = 0; b < 2; b++) for (i = 0; i < 5; i++) {
        double out = 777.0; HRESULT hr = U(VarR8Pow)(bases[b], ex[i], &out);
        t_check((ULONG)hr == E_INVALIDARG_, __FILE__, __LINE__, "0^neg", "base %g exp %g hr 0x%lx", bases[b], ex[i], (unsigned long)hr);
        t_check(out == 777.0, __FILE__, __LINE__, "0^neg leaves output untouched", "out %g", out);
    }
}

TEST(gap5_r8pow_negative_base_fractional_exponent)
{
    static const double ex[] = { 0.5, 1.5, -0.5, 1.0 / 3.0, 2.0000001, -2.5 };
    unsigned i;
    REQUIRE(U(VarR8Pow));
    for (i = 0; i < 6; i++) {
        double out = 777.0; HRESULT hr = U(VarR8Pow)(-8.0, ex[i], &out);
        t_check((ULONG)hr == E_INVALIDARG_, __FILE__, __LINE__, "neg^frac", "exp %.17g hr 0x%lx", ex[i], (unsigned long)hr);
        t_check(out == 777.0, __FILE__, __LINE__, "neg^frac untouched", "out %g", out);
    }
}

TEST(gap5_r8pow_overflow)
{
    static const pow_case t[] = {
        { "10^400", 10, 400, 0, 0 }, { "2^1024", 2, 1024, 0, 0 }, { "1e200^2", 1e200, 2, 0, 0 },
        { "-10^401", -10, 401, 0, 0 }, { "1e-200^-2", 1e-200, -2, 0, 0 }, { "0.5^-2000", 0.5, -2000, 0, 0 },
    };
    unsigned i;
    REQUIRE(U(VarR8Pow));
    for (i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
        double out = 777.0; HRESULT hr = U(VarR8Pow)(t[i].l, t[i].r, &out);
        t_check((ULONG)hr == DISP_E_OVERFLOW_, __FILE__, __LINE__, t[i].n, "hr 0x%lx", (unsigned long)hr);
        t_check(out == 777.0, __FILE__, __LINE__, t[i].n, "output modified: %g", out);
    }
}

TEST(gap5_r8pow_total_underflow_is_zero_ok)
{
    /* ERANGE with a zero result is not an error in the decomp: *out = 0, S_OK */
    double out = 777.0; HRESULT hr;
    REQUIRE(U(VarR8Pow));
    hr = U(VarR8Pow)(10.0, -400.0, &out);
    EXPECT_EQ(hr, S_OK); CHECK(out == 0.0);
    out = 777.0;
    hr = U(VarR8Pow)(2.0, -5000.0, &out);
    EXPECT_EQ(hr, S_OK); CHECK(out == 0.0);
}

TEST(gap5_r8pow_infinity_note)
{
    double out = 0; HRESULT hr;
    REQUIRE(U(VarR8Pow));
    hr = U(VarR8Pow)(INFINITY, 1.0, &out);
    t_note("inf^1 -> hr 0x%lx out %g", (unsigned long)hr, out);
    hr = U(VarR8Pow)(2.0, INFINITY, &out);
    t_note("2^inf -> hr 0x%lx out %g", (unsigned long)hr, out);
    hr = U(VarR8Pow)(NAN, 2.0, &out);
    t_note("nan^2 -> hr 0x%lx out %g", (unsigned long)hr, out);
}

/* ---- VarDateFromUdateEx ---- */
static UDATE mk(int y, int mo, int d, int h, int mi, int s)
{
    UDATE u; memset(&u, 0, sizeof u);
    u.st.wYear = y; u.st.wMonth = mo; u.st.wDay = d; u.st.wHour = h; u.st.wMinute = mi; u.st.wSecond = s;
    return u;
}
#define LCID_US 0x409
#define VAR_TIMEVALUEONLY_ 1
#define VAR_DATEVALUEONLY_ 2
#define VAR_VALIDDATE_ 4

typedef struct { const char *n; int y, mo, d, h, mi, s; double want; } dcase;

static void run_dcases(const dcase *t, unsigned n, ULONG flags)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        UDATE u = mk(t[i].y, t[i].mo, t[i].d, t[i].h, t[i].mi, t[i].s);
        DATE out = 12345.0;
        HRESULT hr = U(VarDateFromUdateEx)(&u, LCID_US, flags, &out);
        t_check(hr == S_OK && fabs(out - t[i].want) < 1e-9, __FILE__, __LINE__, t[i].n, "hr 0x%lx out %.12f want %.12f", (unsigned long)hr, out, t[i].want);
    }
}

TEST(gap5_udateex_known_dates)
{
    static const dcase t[] = {
        { "1899-12-30 epoch", 1899, 12, 30, 0, 0, 0, 0.0 },
        { "1899-12-31", 1899, 12, 31, 0, 0, 0, 1.0 },
        { "1900-01-01", 1900, 1, 1, 0, 0, 0, 2.0 },
        { "1900-01-01 06:00", 1900, 1, 1, 6, 0, 0, 2.25 },
        { "1900-02-28", 1900, 2, 28, 0, 0, 0, 60.0 },
        { "1900-03-01 (1900 not leap)", 1900, 3, 1, 0, 0, 0, 61.0 },
        { "1970-01-01", 1970, 1, 1, 0, 0, 0, 25569.0 },
        { "2000-01-01", 2000, 1, 1, 0, 0, 0, 36526.0 },
        { "2000-02-29 leap", 2000, 2, 29, 0, 0, 0, 36585.0 },
        { "2000-03-01", 2000, 3, 1, 0, 0, 0, 36586.0 },
        { "2000-01-01 12:00", 2000, 1, 1, 12, 0, 0, 36526.5 },
        { "2000-01-01 23:59:59", 2000, 1, 1, 23, 59, 59, 36526.0 + 86399.0 / 86400.0 },
        { "2000-01-01 06:30:15", 2000, 1, 1, 6, 30, 15, 36526.0 + 23415.0 / 86400.0 },
        { "2024-02-29", 2024, 2, 29, 0, 0, 0, 45351.0 },
        { "max 9999-12-31", 9999, 12, 31, 0, 0, 0, 2958465.0 },
        { "max 9999-12-31 23:59:59", 9999, 12, 31, 23, 59, 59, 2958465.0 + 86399.0 / 86400.0 },
        { "min 0100-01-01", 100, 1, 1, 0, 0, 0, -657434.0 },
        { "0100-01-02", 100, 1, 2, 0, 0, 0, -657433.0 },
    };
    REQUIRE(U(VarDateFromUdateEx));
    run_dcases(t, sizeof(t)/sizeof(t[0]), 0);
    run_dcases(t, sizeof(t)/sizeof(t[0]), VAR_VALIDDATE_);
}

TEST(gap5_udateex_negative_dates_subtract_time)
{
    /* before the epoch the time-of-day fraction is subtracted: 1899-12-29 06:00 -> -1.25 */
    static const dcase t[] = {
        { "1899-12-29", 1899, 12, 29, 0, 0, 0, -1.0 },
        { "1899-12-29 06:00", 1899, 12, 29, 6, 0, 0, -1.25 },
        { "1899-12-29 12:00", 1899, 12, 29, 12, 0, 0, -1.5 },
        { "1899-12-28 18:00", 1899, 12, 28, 18, 0, 0, -2.75 },
        { "1899-12-30 06:00 (day 0 adds)", 1899, 12, 30, 6, 0, 0, 0.25 },
        { "1899-12-31 06:00", 1899, 12, 31, 6, 0, 0, 1.25 },
    };
    REQUIRE(U(VarDateFromUdateEx));
    run_dcases(t, sizeof(t)/sizeof(t[0]), 0);
}

TEST(gap5_udateex_timevalueonly)
{
    static const dcase t[] = {
        { "time only 06:00", 2000, 1, 1, 6, 0, 0, 0.25 },
        { "time only 00:00", 2000, 1, 1, 0, 0, 0, 0.0 },
        { "time only 12:00 neg date", 1800, 5, 5, 12, 0, 0, 0.5 },
        { "time only 18:00", 1899, 12, 29, 18, 0, 0, 0.75 },
    };
    REQUIRE(U(VarDateFromUdateEx));
    run_dcases(t, sizeof(t)/sizeof(t[0]), VAR_TIMEVALUEONLY_);
}

TEST(gap5_udateex_datevalueonly)
{
    static const dcase t[] = {
        { "date only drops time", 2000, 1, 1, 18, 30, 0, 36526.0 },
        { "date only epoch", 1899, 12, 30, 23, 59, 59, 0.0 },
        { "date only negative", 1899, 12, 29, 12, 0, 0, -1.0 },
        { "date only max", 9999, 12, 31, 23, 59, 59, 2958465.0 },
    };
    REQUIRE(U(VarDateFromUdateEx));
    run_dcases(t, sizeof(t)/sizeof(t[0]), VAR_DATEVALUEONLY_);
}

TEST(gap5_udateex_timeonly_wins_over_dateonly)
{
    /* decomp: flag 1 is tested first and leaves the time fraction alone */
    UDATE u = mk(2000, 1, 1, 6, 0, 0); DATE out = 9;
    REQUIRE(U(VarDateFromUdateEx));
    EXPECT_EQ(U(VarDateFromUdateEx)(&u, LCID_US, 3, &out), S_OK);
    CHECKF(fabs(out - 0.25) < 1e-12, "got %g", out);
}

TEST(gap5_udateex_ignores_milliseconds_dow_doy)
{
    UDATE u = mk(2000, 1, 1, 6, 0, 0); DATE a = 0, b = 0;
    REQUIRE(U(VarDateFromUdateEx));
    EXPECT_EQ(U(VarDateFromUdateEx)(&u, LCID_US, 0, &a), S_OK);
    u.st.wMilliseconds = 999; u.st.wDayOfWeek = 6; u.wDayOfYear = 200;
    EXPECT_EQ(U(VarDateFromUdateEx)(&u, LCID_US, 0, &b), S_OK);
    CHECK(a == b);
}

TEST(gap5_udateex_lcid_independent_for_four_digit_years)
{
    static const LCID l[] = { 0x409, 0x407, 0x40c, 0x411, 0x0, 0x800 };
    unsigned i;
    REQUIRE(U(VarDateFromUdateEx));
    for (i = 0; i < 6; i++) {
        UDATE u = mk(2000, 1, 1, 12, 0, 0); DATE out = 0;
        HRESULT hr = U(VarDateFromUdateEx)(&u, l[i], 0, &out);
        t_check(hr == S_OK && out == 36526.5, __FILE__, __LINE__, "lcid", "lcid 0x%lx hr 0x%lx out %g", (unsigned long)l[i], (unsigned long)hr, out);
    }
}

TEST(gap5_udateex_month_normalisation_without_validdate)
{
    static const dcase t[] = {
        { "month 13 -> next Jan", 2000, 13, 1, 0, 0, 0, 36892.0 },        /* 2001-01-01 */
        { "month 14 -> next Feb", 2000, 14, 1, 0, 0, 0, 36923.0 },        /* 2001-02-01 */
        { "month 24 -> next Dec", 2000, 24, 1, 0, 0, 0, 37226.0 },        /* 2001-12-01 */
        { "month 25 -> Jan +2", 2000, 25, 1, 0, 0, 0, 37257.0 },          /* 2002-01-01 */
        { "month 0 -> prev Dec", 2000, 0, 1, 0, 0, 0, 36495.0 },          /* 1999-12-01 */
    };
    REQUIRE(U(VarDateFromUdateEx));
    run_dcases(t, sizeof(t)/sizeof(t[0]), 0);
}

TEST(gap5_udateex_day_overflow_without_validdate)
{
    static const dcase t[] = {
        { "Jan 32 -> Feb 1", 2000, 1, 32, 0, 0, 0, 36557.0 },
        { "Jan 0 -> Dec 31", 2000, 1, 0, 0, 0, 0, 36525.0 },
        { "Feb 30 2000 -> Mar 1", 2000, 2, 30, 0, 0, 0, 36586.0 },
        { "Feb 29 1999 -> Mar 1", 1999, 2, 29, 0, 0, 0, 36220.0 },
        { "Jan 1 hour 24 -> next day", 2000, 1, 1, 24, 0, 0, 36527.0 },
        { "minute 90", 2000, 1, 1, 0, 90, 0, 36526.0 + 5400.0 / 86400.0 },
        { "second 3600", 2000, 1, 1, 0, 0, 3600, 36526.0 + 3600.0 / 86400.0 },
    };
    REQUIRE(U(VarDateFromUdateEx));
    run_dcases(t, sizeof(t)/sizeof(t[0]), 0);
}

TEST(gap5_udateex_validdate_rejects)
{
    static const dcase bad[] = {
        { "month 13", 2000, 13, 1, 0, 0, 0, 0 }, { "month 14", 2000, 14, 1, 0, 0, 0, 0 },
        { "day 0", 2000, 1, 0, 0, 0, 0, 0 }, { "Jan 32", 2000, 1, 32, 0, 0, 0, 0 },
        { "Feb 30 leap", 2000, 2, 30, 0, 0, 0, 0 }, { "Feb 29 1999", 1999, 2, 29, 0, 0, 0, 0 },
        { "Feb 29 1900", 1900, 2, 29, 0, 0, 0, 0 }, { "Feb 29 2100", 2100, 2, 29, 0, 0, 0, 0 },
        { "Apr 31", 2000, 4, 31, 0, 0, 0, 0 }, { "Jun 31", 2000, 6, 31, 0, 0, 0, 0 },
        { "Sep 31", 2000, 9, 31, 0, 0, 0, 0 }, { "Nov 31", 2000, 11, 31, 0, 0, 0, 0 },
        { "hour 24", 2000, 1, 1, 24, 0, 0, 0 }, { "minute 60", 2000, 1, 1, 0, 60, 0, 0 },
        { "second 60", 2000, 1, 1, 0, 0, 60, 0 },
    };
    static const dcase good[] = {
        { "Feb 29 2000 (div by 400)", 2000, 2, 29, 0, 0, 0, 36585.0 },
        { "Feb 29 2004", 2004, 2, 29, 0, 0, 0, 38046.0 },
        { "Dec 31 2000", 2000, 12, 31, 23, 59, 59, 36891.0 + 86399.0 / 86400.0 },
        { "Apr 30", 2000, 4, 30, 0, 0, 0, 36646.0 },
        { "Jan 31", 2000, 1, 31, 0, 0, 0, 36556.0 },
    };
    unsigned i;
    REQUIRE(U(VarDateFromUdateEx));
    for (i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        UDATE u = mk(bad[i].y, bad[i].mo, bad[i].d, bad[i].h, bad[i].mi, bad[i].s);
        DATE out = 12345.0; HRESULT hr = U(VarDateFromUdateEx)(&u, LCID_US, VAR_VALIDDATE_, &out);
        t_check((ULONG)hr == E_INVALIDARG_ && out == 12345.0, __FILE__, __LINE__, bad[i].n, "hr 0x%lx out %g", (unsigned long)hr, out);
    }
    run_dcases(good, sizeof(good)/sizeof(good[0]), VAR_VALIDDATE_);
}

TEST(gap5_udateex_year_range)
{
    UDATE u; DATE out; HRESULT hr;
    REQUIRE(U(VarDateFromUdateEx));
    u = mk(10000, 1, 1, 0, 0, 0); out = 12345.0;
    hr = U(VarDateFromUdateEx)(&u, LCID_US, 0, &out);
    t_check((ULONG)hr == E_INVALIDARG_ && out == 12345.0, __FILE__, __LINE__, "year 10000", "hr 0x%lx", (unsigned long)hr);
    u = mk(9999, 13, 1, 0, 0, 0); out = 12345.0;       /* normalises to year 10000 */
    hr = U(VarDateFromUdateEx)(&u, LCID_US, 0, &out);
    t_check((ULONG)hr == E_INVALIDARG_, __FILE__, __LINE__, "9999 month 13 -> 10000", "hr 0x%lx", (unsigned long)hr);
    u = mk(9999, 12, 32, 0, 0, 0); out = 12345.0;      /* day overflow past 9999-12-31 */
    hr = U(VarDateFromUdateEx)(&u, LCID_US, 0, &out);
    t_check((ULONG)hr == E_INVALIDARG_, __FILE__, __LINE__, "9999-12-32", "hr 0x%lx", (unsigned long)hr);
    u = mk(100, 1, 0, 0, 0, 0); out = 12345.0;         /* day before 0100-01-01 */
    hr = U(VarDateFromUdateEx)(&u, LCID_US, 0, &out);
    t_check((ULONG)hr == E_INVALIDARG_, __FILE__, __LINE__, "0100-01-00", "hr 0x%lx", (unsigned long)hr);
}

TEST(gap5_udateex_two_digit_year_note)
{
    int y; UDATE u; DATE out;
    REQUIRE(U(VarDateFromUdateEx));
    for (y = 0; y < 100; y += 10) {
        UDATE* p; u = mk(y, 1, 1, 0, 0, 0); p = &u; out = 0;
        HRESULT hr = U(VarDateFromUdateEx)(p, LCID_US, 0, &out);
        t_note("two-digit year %02d -> hr 0x%lx date %g", y, (unsigned long)hr, out);
    }
    /* result must be monotonic in year within each century window and always succeed */
    u = mk(50, 1, 1, 0, 0, 0); out = 0;
    EXPECT_EQ(U(VarDateFromUdateEx)(&u, LCID_US, 0, &out), S_OK);
}

TEST(gap5_udateex_unknown_flag_bits_ignored)
{
    static const ULONG fl[] = { 0x10, 0x20, 0x40, 0x10000000, 0x00000100 };
    unsigned i;
    REQUIRE(U(VarDateFromUdateEx));
    for (i = 0; i < 5; i++) {
        UDATE u = mk(2000, 1, 1, 12, 0, 0); DATE out = 0;
        HRESULT hr = U(VarDateFromUdateEx)(&u, LCID_US, fl[i], &out);
        t_check(hr == S_OK && out == 36526.5, __FILE__, __LINE__, "flag", "flags 0x%lx hr 0x%lx out %g", (unsigned long)fl[i], (unsigned long)hr, out);
    }
}

TEST(gap5_udateex_matches_vardatefromudate_and_roundtrip)
{
    int y, mo; REQUIRE(U(VarDateFromUdateEx)); REQUIRE(U(VarDateFromUdate)); REQUIRE(U(VarUdateFromDate));
    for (y = 1900; y <= 2100; y += 37) for (mo = 1; mo <= 12; mo++) {
        UDATE u = mk(y, mo, 15, 13, 45, 30), r; DATE a = 0, b = 0, c = 0;
        HRESULT h1 = U(VarDateFromUdateEx)(&u, LCID_US, 0, &a);
        HRESULT h2 = U(VarDateFromUdate)(&u, 0, &b);
        t_check(h1 == S_OK && h2 == S_OK && a == b, __FILE__, __LINE__, "Ex(lcid us) == plain", "%d-%d %g %g", y, mo, a, b);
        (void)c;
        memset(&r, 0, sizeof r);
        t_check(U(VarUdateFromDate)(a, 0, &r) == S_OK && r.st.wYear == y && r.st.wMonth == mo && r.st.wDay == 15 &&
                r.st.wHour == 13 && r.st.wMinute == 45 && r.st.wSecond == 30, __FILE__, __LINE__, "roundtrip", "%d-%d", y, mo);
    }
}
