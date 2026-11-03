/* variant_core: VT_DATE <-> SYSTEMTIME / DOS date-time, VarConversionLocaleSetting */
#include "vc.h"

#define EXPORT_OK() do { REQUIRE(U(SystemTimeToVariantTime)); REQUIRE(U(VariantTimeToSystemTime)); } while (0)
VCW(INT, SystemTimeToVariantTime, (LPSYSTEMTIME st, double *out), (st, out))
VCW(INT, VariantTimeToSystemTime, (double d, LPSYSTEMTIME st), (d, st))
VCW(INT, DosDateTimeToVariantTime, (USHORT dd, USHORT dt, double *out), (dd, dt, out))
VCW(INT, VariantTimeToDosDateTime, (double d, USHORT *dd, USHORT *dt), (d, dd, dt))

/* proleptic Gregorian days since 1970-01-01 (H. Hinnant) */
static long days_from_civil(long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}
static void civil_from_days(long z, long *y, unsigned *m, unsigned *d) {
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yy = (long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = yy + (*m <= 2);
}
static const long OLE_EPOCH = 0; /* filled lazily */
static long ole_serial(long y, unsigned m, unsigned d) { return days_from_civil(y, m, d) - days_from_civil(1899, 12, 30); }
static unsigned weekday(long y, unsigned m, unsigned d) { long z = days_from_civil(y, m, d); long w = (z + 4) % 7; if (w < 0) w += 7; return (unsigned)w; } /* 0 = Sunday */

static SYSTEMTIME mkst(int y, int m, int d, int h, int mi, int s, int ms) {
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wYear = (WORD)y; st.wMonth = (WORD)m; st.wDay = (WORD)d; st.wHour = (WORD)h; st.wMinute = (WORD)mi; st.wSecond = (WORD)s; st.wMilliseconds = (WORD)ms;
    return st;
}
static int st2dt(int y, int m, int d, int h, int mi, int s, int ms, double *out) { SYSTEMTIME st = mkst(y, m, d, h, mi, s, ms); return w_SystemTimeToVariantTime(&st, out); }

/* ---------------------------------------------------------------- SystemTimeToVariantTime */
typedef struct { int y, m, d, h, mi, s, ms; int ret; double want; } st2case;
TEST(vc_time_st2vt_table) {
    REQUIRE(U(SystemTimeToVariantTime));
    static const st2case t[] = {
        { 1980, 1, 1, 0, 0, 0, 0, 1, 29221.0 }, { 1980, 1, 2, 0, 0, 0, 0, 1, 29222.0 },
        { 1899, 12, 30, 0, 0, 0, 0, 1, 0.0 }, { 1899, 12, 31, 0, 0, 0, 0, 1, 1.0 }, { 1900, 1, 1, 0, 0, 0, 0, 1, 2.0 },
        { 1899, 12, 29, 0, 0, 0, 0, 1, -1.0 }, { 1899, 12, 25, 6, 0, 0, 0, 1, -5.25 }, { 1899, 12, 26, 0, 0, 0, 0, 1, -4.0 },
        { 1899, 12, 30, 6, 0, 0, 0, 1, 0.25 }, { 1899, 12, 30, 18, 0, 0, 0, 1, 0.75 }, { 1899, 12, 29, 18, 0, 0, 0, 1, -1.75 },
        { 100, 1, 1, 0, 0, 0, 0, 1, -657434.0 }, { 9999, 12, 31, 0, 0, 0, 0, 1, 2958465.0 }, { 9999, 1, 1, 0, 0, 0, 0, 1, 2958101.0 },
        { 2000, 1, 1, 0, 0, 0, 0, 1, 36526.0 }, { 2000, 2, 29, 0, 0, 0, 0, 1, 36585.0 }, { 2000, 3, 1, 0, 0, 0, 0, 1, 36586.0 },
        { 1900, 2, 28, 0, 0, 0, 0, 1, 60.0 }, { 1900, 3, 1, 0, 0, 0, 0, 1, 61.0 },
        { 1980, 1, 1, 12, 0, 0, 0, 1, 29221.5 }, { 1980, 1, 1, 6, 0, 0, 0, 1, 29221.25 }, { 1980, 1, 1, 23, 0, 0, 0, 1, 29221.0 + 23.0 / 24.0 },
        { 1980, 1, 1, 18, 1, 16, 0, 1, 29221.75087962963 }, { 1980, 1, 1, 0, 0, 1, 0, 1, 29221.0 + 1.0 / 86400.0 },
        { 1980, 1, 1, 23, 59, 59, 0, 1, 29221.0 + 86399.0 / 86400.0 },
        /* explicit rejections (range checks in the decomp: month<13, day<32, hour<24, min<60, sec<60, year<10000) */
        { 1980, 13, 1, 0, 0, 0, 0, 0, 0 }, { 1980, 1, 32, 0, 0, 0, 0, 0, 0 }, { 10000, 1, 1, 0, 0, 0, 0, 0, 0 }, { 65535, 1, 1, 0, 0, 0, 0, 0, 0 },
        { 1980, 1, 1, 24, 0, 0, 0, 0, 0 }, { 1980, 1, 1, 0, 60, 0, 0, 0, 0 }, { 1980, 1, 1, 0, 0, 60, 0, 0, 0 }, { 1980, 1, 1, 255, 0, 0, 0, 0, 0 },
        { 1980, 1, 1, 0, 255, 0, 0, 0, 0 }, { 1980, 1, 1, 0, 0, 255, 0, 0, 0 }, { 1980, 14, 1, 0, 0, 0, 0, 0, 0 }, { 1980, 12, 33, 0, 0, 0, 0, 0, 0 },
        /* day 0 rolls back one day */
        { 1980, 1, 0, 0, 0, 0, 0, 1, 29220.0 }, { 1980, 3, 0, 0, 0, 0, 0, 1, 29280.0 },
        /* days past the end of the month roll forward */
        { 1981, 2, 29, 0, 0, 0, 0, 1, 29646.0 }, { 1981, 2, 30, 0, 0, 0, 0, 1, 29647.0 }, { 1980, 2, 30, 0, 0, 0, 0, 1, 29281.0 },
        { 1980, 4, 31, 0, 0, 0, 0, 1, 29342.0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        double out = -12345.0;
        int r = st2dt(t[i].y, t[i].m, t[i].d, t[i].h, t[i].mi, t[i].s, t[i].ms, &out);
        CHECKF(r == t[i].ret, "case %u (%d-%d-%d %d:%d:%d): ret %d want %d", i, t[i].y, t[i].m, t[i].d, t[i].h, t[i].mi, t[i].s, r, t[i].ret);
        if (t[i].ret && r) CHECKF(vc_dbl_eq(out, t[i].want) || fabs(out - t[i].want) < 1e-9, "case %u: got %.12g want %.12g", i, out, t[i].want);
        if (!t[i].ret) CHECKF(out == -12345.0, "case %u: output modified on failure", i);
    }
}
TEST(vc_time_st2vt_milliseconds_and_weekday_ignored) {
    REQUIRE(U(SystemTimeToVariantTime));
    double a, b, c;
    CHECK(st2dt(1980, 1, 1, 18, 1, 16, 0, &a) == 1);
    CHECK(st2dt(1980, 1, 1, 18, 1, 16, 999, &b) == 1);
    CHECK(st2dt(1980, 1, 1, 18, 1, 16, 65535, &c) == 1);
    CHECK(a == b); CHECK(a == c);
    SYSTEMTIME st = mkst(1980, 1, 1, 0, 0, 0, 0); double x, y;
    st.wDayOfWeek = 0; CHECK(w_SystemTimeToVariantTime(&st, &x) == 1);
    st.wDayOfWeek = 6; CHECK(w_SystemTimeToVariantTime(&st, &y) == 1);
    st.wDayOfWeek = 0xffff; CHECK(w_SystemTimeToVariantTime(&st, &c) == 1);
    CHECK(x == y); CHECK(x == c);
}
TEST(vc_time_st2vt_two_digit_years) {
    REQUIRE(U(SystemTimeToVariantTime));
    double v;
    CHECK(st2dt(0, 1, 1, 0, 0, 0, 0, &v) == 1);  CHECK(v == 36526.0);   /* 2000 */
    CHECK(st2dt(1, 1, 1, 0, 0, 0, 0, &v) == 1);  CHECK(v == 36892.0);   /* 2001 */
    CHECK(st2dt(29, 1, 1, 0, 0, 0, 0, &v) == 1); CHECK(v == 47119.0);   /* 2029 */
    CHECK(st2dt(99, 12, 31, 0, 0, 0, 0, &v) == 1); CHECK(v == 36525.0); /* 1999 */
    CHECK(st2dt(80, 1, 1, 0, 0, 0, 0, &v) == 1); CHECK(v == 29221.0);   /* 1980 */
    CHECK(st2dt(90, 1, 1, 0, 0, 0, 0, &v) == 1); CHECK(v == 32874.0);   /* 1990 */
    /* century pivot depends on the calendar's two digit year max (default 2049): informational */
    for (int y = 30; y <= 50; y += 10) { double r = 0; int ok = st2dt(y, 1, 1, 0, 0, 0, 0, &r); t_note("two digit year %d -> ret=%d serial=%.0f", y, ok, r); }
    /* year 100 is a real year, not a two digit one */
    CHECK(st2dt(100, 1, 1, 0, 0, 0, 0, &v) == 1); CHECK(v == -657434.0);
    CHECK(st2dt(101, 1, 1, 0, 0, 0, 0, &v) == 1); CHECK(v == (double)ole_serial(101, 1, 1));
}
TEST(vc_time_st2vt_every_year_first_and_last_day) {
    REQUIRE(U(SystemTimeToVariantTime));
    int bad = 0;
    for (int y = 100; y <= 9999; y++) {
        double a, b;
        int ra = st2dt(y, 1, 1, 0, 0, 0, 0, &a), rb = st2dt(y, 12, 31, 0, 0, 0, 0, &b);
        if (ra != 1 || rb != 1 || a != (double)ole_serial(y, 1, 1) || b != (double)ole_serial(y, 12, 31)) {
            if (bad++ < 5) CHECKF(0, "year %d: jan1=%.0f/%d dec31=%.0f/%d want %ld %ld", y, a, ra, b, rb, ole_serial(y, 1, 1), ole_serial(y, 12, 31));
        }
    }
    CHECK(bad == 0);
}
TEST(vc_time_st2vt_leap_day_validity) {
    REQUIRE(U(SystemTimeToVariantTime));
    static const struct { int y; int leap; } yrs[] = { { 1900, 0 }, { 2000, 1 }, { 2100, 0 }, { 2400, 1 }, { 1996, 1 }, { 1999, 0 }, { 1600, 1 }, { 1700, 0 }, { 400, 1 }, { 300, 0 }, { 104, 1 }, { 9996, 1 } };
    for (unsigned i = 0; i < sizeof yrs / sizeof *yrs; i++) {
        double feb29, mar1;
        CHECK(st2dt(yrs[i].y, 2, 29, 0, 0, 0, 0, &feb29) == 1);   /* accepted everywhere; rolls to Mar 1 in non-leap years */
        CHECK(st2dt(yrs[i].y, 3, 1, 0, 0, 0, 0, &mar1) == 1);
        if (yrs[i].leap) CHECKF(mar1 - feb29 == 1.0, "year %d leap: feb29=%.0f mar1=%.0f", yrs[i].y, feb29, mar1);
        else CHECKF(feb29 == mar1, "year %d non-leap: feb29=%.0f mar1=%.0f", yrs[i].y, feb29, mar1);
    }
}
TEST(vc_time_st2vt_matches_civil_calendar_1900_to_2100) {
    REQUIRE(U(SystemTimeToVariantTime));
    int bad = 0;
    for (long z = days_from_civil(1900, 1, 1); z <= days_from_civil(2100, 12, 31); z++) {
        long y; unsigned m, d; civil_from_days(z, &y, &m, &d);
        double v;
        int r = st2dt((int)y, (int)m, (int)d, 0, 0, 0, 0, &v);
        if (!r || v != (double)(z - days_from_civil(1899, 12, 30))) { if (bad++ < 3) CHECKF(0, "%ld-%u-%u -> %g", y, m, d, v); }
    }
    CHECK(bad == 0);
}
TEST(vc_time_st2vt_time_of_day_is_fraction) {
    REQUIRE(U(SystemTimeToVariantTime));
    int bad = 0;
    for (int h = 0; h < 24; h++) for (int mi = 0; mi < 60; mi += 7) for (int s = 0; s < 60; s += 13) {
        double v;
        if (!st2dt(2001, 1, 1, h, mi, s, 0, &v)) { bad++; continue; }
        double want = 36892.0 + (h * 3600.0 + mi * 60.0 + s) / 86400.0;
        if (fabs(v - want) > 1e-9) { if (bad++ < 3) CHECKF(0, "%02d:%02d:%02d -> %.12g want %.12g", h, mi, s, v, want); }
    }
    CHECK(bad == 0);
}
TEST(vc_time_st2vt_negative_dates_time_is_added_in_absolute_terms) {
    REQUIRE(U(SystemTimeToVariantTime));
    /* before 1899-12-30 the fraction is stored as |fraction| so 1899-12-29 18:00 is -1.75, not -0.25 */
    double v;
    CHECK(st2dt(1899, 12, 29, 18, 0, 0, 0, &v) == 1); CHECK(v == -1.75);
    CHECK(st2dt(1899, 12, 28, 6, 0, 0, 0, &v) == 1); CHECK(v == -2.25);
    CHECK(st2dt(1000, 6, 15, 12, 0, 0, 0, &v) == 1);
    CHECK(v == (double)ole_serial(1000, 6, 15) - 0.5);
}
TEST(vc_time_st2vt_null_vs_garbage_fields_are_validated_individually) {
    REQUIRE(U(SystemTimeToVariantTime));
    double v = 5.0;
    SYSTEMTIME st = mkst(1980, 1, 1, 0, 0, 0, 0);
    st.wYear = 0x8000; CHECK(w_SystemTimeToVariantTime(&st, &v) == 0);    /* negative as a short */
    st = mkst(1980, 1, 1, 0, 0, 0, 0); st.wMonth = 0x8001; CHECK(w_SystemTimeToVariantTime(&st, &v) == 0);
    st = mkst(1980, 1, 1, 0, 0, 0, 0); st.wDay = 0x8001; CHECK(w_SystemTimeToVariantTime(&st, &v) == 0);
    st = mkst(1980, 1, 1, 0, 0, 0, 0); st.wHour = 0x8001; CHECK(w_SystemTimeToVariantTime(&st, &v) == 0);
    st = mkst(1980, 1, 1, 0, 0, 0, 0); st.wMinute = 0x8001; CHECK(w_SystemTimeToVariantTime(&st, &v) == 0);
    st = mkst(1980, 1, 1, 0, 0, 0, 0); st.wSecond = 0x8001; CHECK(w_SystemTimeToVariantTime(&st, &v) == 0);
    CHECK(v == 5.0);
}

/* ---------------------------------------------------------------- VariantTimeToSystemTime */
typedef struct { double dt; int ret; int y, m, d, h, mi, s, dow; } vt2case;
TEST(vc_time_vt2st_table) {
    REQUIRE(U(VariantTimeToSystemTime));
    static const vt2case t[] = {
        { 29221.0, 1, 1980, 1, 1, 0, 0, 0, 2 }, { 29222.0, 1, 1980, 1, 2, 0, 0, 0, 3 }, { 33238.0, 1, 1990, 12, 31, 0, 0, 0, 1 },
        { 0.0, 1, 1899, 12, 30, 0, 0, 0, 6 }, { 1.0, 1, 1899, 12, 31, 0, 0, 0, 0 }, { 2.0, 1, 1900, 1, 1, 0, 0, 0, 1 },
        { 59.0, 1, 1900, 2, 27, 0, 0, 0, 2 }, { 60.0, 1, 1900, 2, 28, 0, 0, 0, 3 }, /* OLE dates have no 1900-02-29 */
        { 61.0, 1, 1900, 3, 1, 0, 0, 0, 4 },
        { -1.0, 1, 1899, 12, 29, 0, 0, 0, 5 },
        { -657434.0, 1, 100, 1, 1, 0, 0, 0, 5 }, { -657435.0, 0 }, { 2958465.0, 1, 9999, 12, 31, 0, 0, 0, 5 }, { 2958466.0, 0 },
        { 29221.25, 1, 1980, 1, 1, 6, 0, 0, 2 }, { 29221.5, 1, 1980, 1, 1, 12, 0, 0, 2 }, { 29221.75, 1, 1980, 1, 1, 18, 0, 0, 2 },
        { 29221.7508765432, 1, 1980, 1, 1, 18, 1, 16, 2 }, { 29221.9888884444, 1, 1980, 1, 1, 23, 44, 0, 2 },
        { -5.25, 1, 1899, 12, 25, 6, 0, 0, 1 }, { -5.9999884259259, 1, 1899, 12, 25, 23, 59, 59, 1 }, { -4.0, 1, 1899, 12, 26, 0, 0, 0, 2 },
        { -0.25, 1, 1899, 12, 30, 6, 0, 0, 6 }, { 0.25, 1, 1899, 12, 30, 6, 0, 0, 6 }, { 0.5, 1, 1899, 12, 30, 12, 0, 0, 6 },
        { 36526.0, 1, 2000, 1, 1, 0, 0, 0, 6 }, { 36585.0, 1, 2000, 2, 29, 0, 0, 0, 2 }, { 73050.0, 1, 2099, 12, 31, 0, 0, 0, 4 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SYSTEMTIME st; memset(&st, 0x7e, sizeof st);
        int r = w_VariantTimeToSystemTime(t[i].dt, &st);
        CHECKF(r == t[i].ret, "case %u (%.12g): ret %d want %d", i, t[i].dt, r, t[i].ret);
        if (r && t[i].ret) {
            CHECKF(st.wYear == t[i].y && st.wMonth == t[i].m && st.wDay == t[i].d && st.wHour == t[i].h && st.wMinute == t[i].mi && st.wSecond == t[i].s,
                   "case %u (%.12g): got %u-%u-%u %u:%u:%u", i, t[i].dt, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            CHECKF(st.wDayOfWeek == t[i].dow, "case %u (%.12g): dow %u want %d", i, t[i].dt, st.wDayOfWeek, t[i].dow);
            CHECKF(st.wMilliseconds == 0, "case %u: ms %u", i, st.wMilliseconds);
        } else if (!t[i].ret) {
            CHECKF(st.wYear == 0x7e7e && st.wMilliseconds == 0x7e7e, "case %u: output modified on failure", i);
        }
    }
}
TEST(vc_time_vt2st_milliseconds_always_zero) {
    REQUIRE(U(VariantTimeToSystemTime));
    for (double f = 0.0; f < 1.0; f += 0.0713) {
        SYSTEMTIME st; memset(&st, 0xff, sizeof st);
        CHECK(w_VariantTimeToSystemTime(40000.0 + f, &st) == 1);
        EXPECT_EQ(st.wMilliseconds, 0);
    }
}
TEST(vc_time_vt2st_subsecond_rounding_observed) {
    REQUIRE(U(VariantTimeToSystemTime));
    static const double fr[] = { 0.4 / 86400.0, 0.49 / 86400.0, 0.5 / 86400.0, 0.51 / 86400.0, 0.9 / 86400.0, 0.999 / 86400.0, 59.4 / 86400.0, 59.5 / 86400.0, 59.999 / 86400.0, 86399.5 / 86400.0, 86399.999 / 86400.0 };
    for (unsigned i = 0; i < sizeof fr / sizeof *fr; i++) {
        SYSTEMTIME st; memset(&st, 0, sizeof st);
        int r = w_VariantTimeToSystemTime(36526.0 + fr[i], &st);
        t_note("2000-01-01 + %.4f s -> ret=%d %04u-%02u-%02u %02u:%02u:%02u", fr[i] * 86400.0, r, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        CHECK(r == 1);
    }
}
TEST(vc_time_vt2st_whole_days_match_civil_calendar) {
    REQUIRE(U(VariantTimeToSystemTime));
    int bad = 0;
    for (long n = -30000; n <= 60000; n++) {
        long y; unsigned m, d; civil_from_days(n + days_from_civil(1899, 12, 30), &y, &m, &d);
        SYSTEMTIME st; memset(&st, 0, sizeof st);
        int r = w_VariantTimeToSystemTime((double)n, &st);
        if (!r || st.wYear != y || st.wMonth != m || st.wDay != d || st.wDayOfWeek != weekday(y, m, d) || st.wHour || st.wMinute || st.wSecond) {
            if (bad++ < 3) CHECKF(0, "serial %ld -> %u-%u-%u dow %u (want %ld-%u-%u)", n, st.wYear, st.wMonth, st.wDay, st.wDayOfWeek, y, m, d);
        }
    }
    CHECK(bad == 0);
}
TEST(vc_time_vt2st_range_boundaries_other_years) {
    REQUIRE(U(VariantTimeToSystemTime));
    int bad = 0;
    for (int y = 100; y <= 9999; y += 1) {
        long n = ole_serial(y, 2, 28) + 1;   /* Feb 29 or Mar 1 */
        long yy; unsigned m, d; civil_from_days(n + days_from_civil(1899, 12, 30), &yy, &m, &d);
        SYSTEMTIME st; memset(&st, 0, sizeof st);
        int r = w_VariantTimeToSystemTime((double)n, &st);
        if (!r || st.wYear != yy || st.wMonth != m || st.wDay != d) { if (bad++ < 3) CHECKF(0, "year %d: %u-%u-%u want %ld-%u-%u", y, st.wYear, st.wMonth, st.wDay, yy, m, d); }
    }
    CHECK(bad == 0);
}
TEST(vc_time_vt2st_out_of_range_and_special_values) {
    REQUIRE(U(VariantTimeToSystemTime));
    SYSTEMTIME st; memset(&st, 0x11, sizeof st);
    static const double bad[] = { -657435.0, -657436.5, -1e9, 2958466.0, 2958467.5, 1e9, 1e300, -1e300 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) CHECKF(w_VariantTimeToSystemTime(bad[i], &st) == 0, "%g accepted", bad[i]);
    CHECK(st.wYear == 0x1111);
    double nan = 0.0; nan = nan / nan;
    int r = w_VariantTimeToSystemTime(nan, &st);
    t_note("NaN -> ret=%d", r);
    double inf = 1e308 * 10;
    CHECK(w_VariantTimeToSystemTime(inf, &st) == 0);
    CHECK(w_VariantTimeToSystemTime(-inf, &st) == 0);
}
TEST(vc_time_vt2st_roundtrip_through_st2vt) {
    REQUIRE(U(VariantTimeToSystemTime)); REQUIRE(U(SystemTimeToVariantTime));
    int bad = 0;
    for (int y = 100; y <= 9999; y += 37) for (int m = 1; m <= 12; m += 5) {
        SYSTEMTIME a = mkst(y, m, 15, 13, 45, 30, 0), b; double v;
        if (!w_SystemTimeToVariantTime(&a, &v) || !w_VariantTimeToSystemTime(v, &b)) { bad++; continue; }
        if (b.wYear != y || b.wMonth != m || b.wDay != 15 || b.wHour != 13 || b.wMinute != 45 || b.wSecond != 30) { if (bad++ < 3) CHECKF(0, "roundtrip %d-%d: %u-%u-%u %u:%u:%u", y, m, b.wYear, b.wMonth, b.wDay, b.wHour, b.wMinute, b.wSecond); }
    }
    CHECK(bad == 0);
}

/* ---------------------------------------------------------------- DosDateTimeToVariantTime */
#define DOSD(d, m, y) ((USHORT)(((d) & 0x1f) | (((m) & 0xf) << 5) | ((((y) - 1980) & 0x7f) << 9)))
#define DOST(h, m, s) ((USHORT)((((s) >> 1) & 0x1f) | (((m) & 0x3f) << 5) | (((h) & 0x1f) << 11)))
typedef struct { USHORT dd, dt; int ret; double want; } dos2case;
TEST(vc_time_dos2vt_table) {
    REQUIRE(U(DosDateTimeToVariantTime));
    static const dos2case t[] = {
        { DOSD(1, 1, 1980), 0, 1, 29221.0 }, { DOSD(2, 1, 1980), 0, 1, 29222.0 }, { DOSD(31, 12, 1990), 0, 1, 33238.0 },
        { DOSD(31, 12, 2099), 0, 1, 73050.0 }, { DOSD(1, 1, 2000), 0, 1, 36526.0 }, { DOSD(29, 2, 1980), 0, 1, 29280.0 },
        { DOSD(29, 2, 2000), 0, 1, 36585.0 }, { DOSD(31, 12, 1999), 0, 1, 36525.0 }, { DOSD(28, 2, 2099), 0, 1, 72744.0 },
        { DOSD(0, 1, 1980), 0, 1, 29220.0 },        /* day 0 rolls back */
        { DOSD(1, 0, 1980), 0, 1, 29190.0 },        /* month 0 rolls back */
        { DOSD(0, 0, 1980), 0, 1, 29189.0 },
        { DOSD(29, 2, 1981), 0, 1, 29646.0 }, { DOSD(30, 2, 1981), 0, 1, 29647.0 }, /* past end of month rolls forward */
        { DOSD(2, 13, 1980), 0, 0, 0 }, { (USHORT)(DOSD(2, 15, 1980)), 0, 0, 0 },
        /* time */
        { DOSD(1, 1, 1980), DOST(0, 0, 29), 1, 29221.0 + 28.0 / 86400.0 }, { DOSD(1, 1, 1980), DOST(0, 0, 31), 1, 29221.0 + 30.0 / 86400.0 },
        { DOSD(1, 1, 1980), DOST(0, 59, 0), 1, 29221.0 + 59.0 / 1440.0 }, { DOSD(1, 1, 1980), DOST(23, 0, 0), 1, 29221.0 + 23.0 / 24.0 },
        { DOSD(1, 1, 1980), DOST(12, 0, 0), 1, 29221.5 }, { DOSD(1, 1, 1980), DOST(23, 59, 58), 1, 29221.0 + 86398.0 / 86400.0 },
        { DOSD(1, 1, 1980), DOST(18, 1, 16), 1, 29221.75087962963 },
        { DOSD(1, 1, 1980), (USHORT)(60 << 5), 0, 0 }, { DOSD(1, 1, 1980), (USHORT)(24 << 11), 0, 0 }, { DOSD(1, 1, 1980), (USHORT)30, 0, 0 },
        { DOSD(1, 1, 1980), (USHORT)(31 << 11), 0, 0 }, { DOSD(1, 1, 1980), (USHORT)(63 << 5), 0, 0 }, { DOSD(1, 1, 1980), 31, 0, 0 },
        /* odd seconds are not representable: low bit of the date/time word pair is simply the next field */
        { DOSD(1, 1, 1980), 1, 1, 29221.0 + 2.0 / 86400.0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        double out = -777.0;
        int r = w_DosDateTimeToVariantTime(t[i].dd, t[i].dt, &out);
        CHECKF(r == t[i].ret, "case %u (date 0x%04x time 0x%04x): ret %d want %d", i, t[i].dd, t[i].dt, r, t[i].ret);
        if (r && t[i].ret) CHECKF(fabs(out - t[i].want) < 1e-9, "case %u: got %.12g want %.12g", i, out, t[i].want);
        if (!t[i].ret) CHECKF(out == -777.0, "case %u: output modified on failure", i);
    }
}
TEST(vc_time_dos2vt_year_limits) {
    REQUIRE(U(DosDateTimeToVariantTime));
    double v;
    /* year field is 7 bits: 1980..2107, but results are only accepted up to 2099 */
    CHECK(w_DosDateTimeToVariantTime(DOSD(1, 1, 2099), 0, &v) == 1);
    CHECK(w_DosDateTimeToVariantTime(DOSD(1, 1, 2100), 0, &v) == 0);
    CHECK(w_DosDateTimeToVariantTime(DOSD(31, 12, 2100), 0, &v) == 0);
    CHECK(w_DosDateTimeToVariantTime(DOSD(1, 1, 2107), 0, &v) == 0);
    CHECK(w_DosDateTimeToVariantTime(0xFFFF, 0, &v) == 0);
    CHECK(w_DosDateTimeToVariantTime(0, 0, &v) == 1); /* year 1980, month 0 day 0: rolls back to 1979-11-30 */
    CHECK(v == 29189.0);
}
TEST(vc_time_dos2vt_all_years_matches_civil) {
    REQUIRE(U(DosDateTimeToVariantTime));
    int bad = 0;
    for (int y = 1980; y <= 2099; y++) for (int m = 1; m <= 12; m++) for (int d = 1; d <= 28; d += 9) {
        double v;
        int r = w_DosDateTimeToVariantTime(DOSD(d, m, y), DOST(5, 6, 8), &v);
        double want = (double)ole_serial(y, m, d) + (5 * 3600.0 + 6 * 60.0 + 8) / 86400.0;
        if (!r || fabs(v - want) > 1e-9) { if (bad++ < 3) CHECKF(0, "%d-%d-%d: %.10g want %.10g", y, m, d, v, want); }
    }
    CHECK(bad == 0);
}

/* ---------------------------------------------------------------- VariantTimeToDosDateTime */
typedef struct { double dt; int ret; USHORT dd, dtm; } vt2dos;
TEST(vc_time_vt2dos_table) {
    REQUIRE(U(VariantTimeToDosDateTime));
    static const vt2dos t[] = {
        { 29221.0, 1, DOSD(1, 1, 1980), 0 }, { 73050.0, 1, DOSD(31, 12, 2099), 0 }, { 36526.0, 1, DOSD(1, 1, 2000), 0 },
        { 29220.0, 0, 0, 0 }, { 73415.0, 0, 0, 0 }, { 0.0, 0, 0, 0 }, { -1.0, 0, 0, 0 }, { 2958465.0, 0, 0, 0 }, { -657434.0, 0, 0, 0 },
        { 29221.00032407407, 1, DOSD(1, 1, 1980), DOST(0, 0, 29) }, { 29221.00034722222, 1, DOSD(1, 1, 1980), DOST(0, 0, 31) },
        { 29221.04097222222, 1, DOSD(1, 1, 1980), DOST(0, 59, 0) }, { 29221.95833333333, 1, DOSD(1, 1, 1980), DOST(23, 0, 0) },
        { 29221.5, 1, DOSD(1, 1, 1980), DOST(12, 0, 0) },
        { 36585.0, 1, DOSD(29, 2, 2000), 0 }, { 73049.0, 1, DOSD(30, 12, 2099), 0 }, { 29586.0, 1, DOSD(31, 12, 1980), 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        USHORT dd = 0x5555, tm = 0x6666;
        int r = w_VariantTimeToDosDateTime(t[i].dt, &dd, &tm);
        CHECKF(r == t[i].ret, "case %u (%.12g): ret %d want %d", i, t[i].dt, r, t[i].ret);
        if (r && t[i].ret) CHECKF(dd == t[i].dd && tm == t[i].dtm, "case %u (%.12g): got date 0x%04x time 0x%04x want 0x%04x 0x%04x", i, t[i].dt, dd, tm, t[i].dd, t[i].dtm);
        if (!t[i].ret) CHECKF(dd == 0x5555 && tm == 0x6666, "case %u: output modified on failure", i);
    }
}
TEST(vc_time_vt2dos_seconds_are_halved) {
    REQUIRE(U(VariantTimeToDosDateTime));
    for (int s = 0; s < 60; s += 1) {
        double v = 36526.0 + (10 * 3600.0 + 20 * 60.0 + s + 0.3) / 86400.0;
        USHORT dd = 0, tm = 0;
        int r = w_VariantTimeToDosDateTime(v, &dd, &tm);
        CHECKF(r == 1, "sec %d ret %d", s, r);
        if (r) {
            CHECKF(((tm >> 11) & 0x1f) == 10 && ((tm >> 5) & 0x3f) == 20, "sec %d: h/m wrong 0x%04x", s, tm);
            int got = (tm & 0x1f) * 2;
            CHECKF(got == s - (s & 1), "sec %d -> %d", s, got);
        }
    }
}
TEST(vc_time_vt2dos_year_limits_and_fields) {
    REQUIRE(U(VariantTimeToDosDateTime));
    USHORT dd, tm;
    CHECK(w_VariantTimeToDosDateTime((double)ole_serial(1980, 1, 1), &dd, &tm) == 1);
    CHECK(w_VariantTimeToDosDateTime((double)ole_serial(1979, 12, 31) + 0.9999, &dd, &tm) == 0);
    CHECK(w_VariantTimeToDosDateTime((double)ole_serial(2099, 12, 31) + 0.5, &dd, &tm) == 1);
    EXPECT_EQ(dd, DOSD(31, 12, 2099)); EXPECT_EQ(tm, DOST(12, 0, 0));
    CHECK(w_VariantTimeToDosDateTime((double)ole_serial(2100, 1, 1), &dd, &tm) == 0);
    CHECK(w_VariantTimeToDosDateTime(1e300, &dd, &tm) == 0);
    CHECK(w_VariantTimeToDosDateTime(-1e300, &dd, &tm) == 0);
}
TEST(vc_time_dos_roundtrip) {
    REQUIRE(U(VariantTimeToDosDateTime)); REQUIRE(U(DosDateTimeToVariantTime));
    int bad = 0;
    for (int y = 1980; y <= 2099; y += 3) for (int m = 1; m <= 12; m++) for (int d = 1; d <= 28; d += 7) {
        USHORT dd = DOSD(d, m, y), tm = DOST((d + m) % 24, (d * m) % 60, ((d + y) % 30) * 2), dd2, tm2; double v;
        if (!w_DosDateTimeToVariantTime(dd, tm, &v) || !w_VariantTimeToDosDateTime(v, &dd2, &tm2)) { bad++; continue; }
        if (dd2 != dd || tm2 != tm) { if (bad++ < 3) CHECKF(0, "roundtrip %d-%d-%d: 0x%04x/0x%04x vs 0x%04x/0x%04x", y, m, d, dd, tm, dd2, tm2); }
    }
    CHECK(bad == 0);
}
TEST(vc_time_vt2dos_rounding_observed) {
    REQUIRE(U(VariantTimeToDosDateTime));
    static const double fr[] = { 0.4, 0.5, 0.9, 1.4, 1.5, 1.9, 58.9, 59.4, 59.5, 59.999 };
    for (unsigned i = 0; i < sizeof fr / sizeof *fr; i++) {
        USHORT dd = 0, tm = 0;
        int r = w_VariantTimeToDosDateTime(36526.0 + 3600.0 / 86400.0 + fr[i] / 86400.0, &dd, &tm);
        t_note("01:00:00 + %.3f s -> ret=%d time=%02u:%02u:%02u", fr[i], r, tm >> 11, (tm >> 5) & 0x3f, (tm & 0x1f) * 2);
        CHECK(r == 1);
    }
}

/* ---------------------------------------------------------------- Get/SetVarConversionLocaleSetting */
UDECL(HRESULT, GetVarConversionLocaleSetting, (DWORD *));
UDECL(HRESULT, SetVarConversionLocaleSetting, (DWORD));
TEST(vc_localesetting_roundtrip_and_restore) {
    REQUIRE(UN(GetVarConversionLocaleSetting)); REQUIRE(UN(SetVarConversionLocaleSetting));
    if (t_is_wine()) { t_skip("unimplemented in wine"); return; }
    DWORD orig = 0xdeadbeef;
    HR_EQ(UN(GetVarConversionLocaleSetting)(&orig), S_OK);
    t_note("initial VarConversionLocaleSetting = %lu", (unsigned long)orig);
    CHECK(orig <= 2);
    for (DWORD v = 0; v <= 2; v++) {
        HR_EQ(UN(SetVarConversionLocaleSetting)(v), S_OK);
        DWORD got = 0xdeadbeef;
        HR_EQ(UN(GetVarConversionLocaleSetting)(&got), S_OK);
        EXPECT_EQ(got, v);
    }
    HR_EQ(UN(SetVarConversionLocaleSetting)(orig), S_OK);
    DWORD chk = 0xdeadbeef; HR_EQ(UN(GetVarConversionLocaleSetting)(&chk), S_OK); EXPECT_EQ(chk, orig);
}
TEST(vc_localesetting_invalid_values_rejected_and_state_kept) {
    REQUIRE(UN(GetVarConversionLocaleSetting)); REQUIRE(UN(SetVarConversionLocaleSetting));
    if (t_is_wine()) { t_skip("unimplemented in wine"); return; }
    DWORD orig = 0; UN(GetVarConversionLocaleSetting)(&orig);
    static const DWORD bad[] = { 3, 4, 7, 0x80000000u, 0xffffffffu, 0x100 };
    HR_EQ(UN(SetVarConversionLocaleSetting)(1), S_OK);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = UN(SetVarConversionLocaleSetting)(bad[i]);
        CHECKF(hr == VC_INVALID, "Set(0x%lx) hr 0x%08lx", (unsigned long)bad[i], (unsigned long)hr);
        DWORD got = 99; UN(GetVarConversionLocaleSetting)(&got);
        CHECKF(got == 1, "Set(0x%lx) changed the setting to %lu", (unsigned long)bad[i], (unsigned long)got);
    }
    UN(SetVarConversionLocaleSetting)(orig);
}
TEST(vc_localesetting_affects_user_override_conversion) {
    /* observable effect: with setting 0 (no per-user cache) vs 1; just exercise conversions across settings */
    REQUIRE(UN(GetVarConversionLocaleSetting)); REQUIRE(UN(SetVarConversionLocaleSetting));
    if (t_is_wine()) { t_skip("unimplemented in wine"); return; } VC_REQ();
    DWORD orig = 0; UN(GetVarConversionLocaleSetting)(&orig);
    for (DWORD v = 0; v <= 2; v++) {
        UN(SetVarConversionLocaleSetting)(v);
        VARIANT s, d; V_VT(&s) = VT_R8; V_R8(&s) = 1.5; w_VariantInit(&d);
        HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_US, VARIANT_NOUSEROVERRIDE, VT_BSTR), S_OK);
        EXPECT_WSTR(V_BSTR(&d), L"1.5");
        w_VariantClear(&d);
    }
    UN(SetVarConversionLocaleSetting)(orig);
}
