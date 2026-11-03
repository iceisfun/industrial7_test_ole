/* variant_format_parse: VarDateFromStr, VarBstrFromDate/R8/R4/Cy/Dec/Bool/I2/I4 (en-US 0x409 and a few locales), VarUdateFromDate,
 * VarDateFromUdate.  Decomp-verified: VarDateFromStr(NULL, ...) returns DISP_E_TYPEMISMATCH (0x80020005).
 * Two digit year pivot: Windows 10 (>= 1903) maps 00-49 to 2000-2049 and 50-99 to 1950-1999 (older: 00-29 / 30-99). */
#include "vfp.h"

#define DT_EPS 1e-9
static int dbl_eq(double a, double b) { return fabs(a - b) <= DT_EPS; }

/* serial DATE of a Gregorian y-m-d (days_from_civil, offset to 1899-12-30) */
static double vfp_serial(int y, int m, int d)
{
    long long yy = y - (m <= 2), era = (yy >= 0 ? yy : yy - 399) / 400, yoe = yy - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468;      /* days since 1970-01-01 */
    return (double)(days + 25569);                      /* 1970-01-01 is 25569 */
}

typedef struct { const char *s; double v; int bad; } DRow;
#define D(s, v) { s, v, 0 }
#define DX(s)   { s, 0, 1 }

static HRESULT dfs(const char *s, LCID lcid, ULONG flags, DATE *out)
{
    *out = -12345.0;
    return U(VarDateFromStr)(s ? vfp_w(s) : NULL, lcid, flags | LOCALE_NOUSEROVERRIDE, out);
}
static void run_drows(const DRow *rows, size_t n, LCID lcid, ULONG flags)
{
    size_t i;
    for (i = 0; i < n; i++) {
        DATE out;
        HRESULT hr = dfs(rows[i].s, lcid, flags, &out);
        if (rows[i].bad) CHECKF(hr == VFP_E_TYPEMISMATCH, "\"%s\" lcid=0x%lx: want TYPEMISMATCH got hr=0x%lx out=%.17g", rows[i].s, (unsigned long)lcid, (unsigned long)hr, out);
        else CHECKF(hr == S_OK && dbl_eq(out, rows[i].v), "\"%s\" lcid=0x%lx: want %.17g got hr=0x%lx out=%.17g", rows[i].s, (unsigned long)lcid, rows[i].v, (unsigned long)hr, out);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
static const DRow time_rows[] = {
    D("1 am", 0.04166666666666666), D("0.1", 0.0006944444444444445), D("0.40", 0.02777777777777778), D("2.5", 0.08680555555555555),
    D("0:1", 0.0006944444444444445), D("0:20", 0.01388888888888889), D("0:40", 0.02777777777777778), D("3:5", 0.1284722222222222),
    D("00:00 AM", 0.0), D("00:00 a", 0.0), D("12:59 AM", 0.04097222222222222), D("12:59 A", 0.04097222222222222), D("00:00 pm", 0.5), D("00:00 p", 0.5),
    D("12:59 pm", 0.5409722222222222), D("12:59 p", 0.5409722222222222), D("13:00 AM", 0.5416666666666666), D("13:00 PM", 0.5416666666666666),
    D("0.1.0", 0.0006944444444444445), D("1.5.2", 0.04516203703703704),
    /* beyond Wine: plain times */
    D("12:30 PM", 0.5208333333333334), D("12:30", 0.5208333333333334), D("1:00 AM", 0.04166666666666666), D("12:00 AM", 0.0), D("12:00 PM", 0.5),
    D("11:59:59 PM", 0.9999884259259259), D("0:00", 0.0), D("23:59", 0.9993055555555556), D("6:00 AM", 0.25), D("6:00 PM", 0.75), D("6 PM", 0.75),
    D("6 AM", 0.25), D("06:00:00", 0.25), D("18:00:00", 0.75), D("1:30:15 PM", 0.5626736111111111), D("12:00:01 AM", 1.1574074074074073e-05),
    D("3:45 pm", 0.65625), D("3:45 PM", 0.65625), D("3:45 p", 0.65625), D("9:05 AM", 0.37847222222222227),
    D("00:00:00", 0.0), D("12:00:00", 0.5), DX("24:00"), DX("0:60"), DX("1 2 am"), DX("1 am 2"),
};
/* "24:00", "0:60", "1 2 am", "1 am 2" are invalid (Wine BadDateStrings) */
static const DRow date_rows[] = {
    D("2 January, 1970", 25570.0), D("2 January 1970", 25570.0), D("2 Jan 1970", 25570.0), D("2/Jan/1970", 25570.0), D("2-Jan-1970", 25570.0),
    D("1 2 1970", 25570.0), D("1/2/1970", 25570.0), D("1-2-1970", 25570.0), D("13-1-1970", 25581.0), D("1970-1-13", 25581.0),
    D("6/30/2011 01:20:34", 40724.05594907407), D("6/30/2011 01:20:34 AM", 40724.05594907407), D("6/30/2011 01:20:34 PM", 40724.55594907407),
    D("2013-05-14 02:04:12", 41408.08625000001), DX("2013-05-14 02:04:12.017000000"),
    /* beyond Wine */
    D("1/2/2000", 36527.0), D("2000-01-02", 36527.0), D("Jan 2, 2000", 36527.0), D("January 2, 2000", 36527.0), D("2 Jan 2000", 36527.0), D("2000/1/2", 36527.0),
    D("1/1/2000", 36526.0), D("12/31/1999", 36525.0), D("2/29/2000", 36585.0), D("3/1/2000", 36586.0), D("2/28/1900", 60.0), D("3/1/1900", 61.0),
    D("1/1/1900", 2.0), D("12/31/1899", 1.0), D("12/30/1899", 0.0), D("12/29/1899", -1.0), D("1/1/100", -657434.0), D("12/31/9999", 2958465.0),
    D("6/30/2011", 40724.0), D("June 30, 2011", 40724.0), D("30 June 2011", 40724.0), D("Jun 30 2011", 40724.0), D("2011-06-30", 40724.0), D("6-30-2011", 40724.0),
    D("6/30/2011 12:00 PM", 40724.5), D("6/30/2011 12:00 AM", 40724.0), D("6/30/2011 6:00", 40724.25), D("6/30/2011 18:00", 40724.75),
    D("2011-06-30 12:00:00", 40724.5), D("2011-06-30 23:59:59", 40724.99998842593), D("6/30/2011 1:00 PM", 40724.5416666666667),
    D("Jan 1, 2000 12:00 PM", 36526.5), D("1 Jan 2000 6:00 AM", 36526.25), D("March 5, 1999", 36224.0), D("Dec 25 2020", 44190.0), D("25 Dec 2020", 44190.0),
    D("DECEMBER 25, 2020", 44190.0), D("december 25, 2020", 44190.0), D("dec 25 2020", 44190.0), D("Sep 5, 2020", 44079.0),
    DX("2/29/1900"), DX("2/30/2000"), DX("0/1/2000"), DX("1/0/2000"), DX("13/32/2000"), DX("12/32/2000"), DX("4/31/2000"),
    DX("1/2/2000 xyz"), DX("xyz"), DX(""), DX("   "), DX("/"), 
};

TEST(vfp_datefromstr_time_only_strings) { REQUIRE(U(VarDateFromStr)); run_drows(time_rows, sizeof time_rows / sizeof *time_rows, VFP_EN_US, 0); }
TEST(vfp_datefromstr_date_strings)      { REQUIRE(U(VarDateFromStr)); run_drows(date_rows, sizeof date_rows / sizeof *date_rows, VFP_EN_US, 0); }

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_three_numbers_lcid_order)
{
    static const DRow rows[] = {
        D("1 2 3", 37623.0), D("14 2 3", 41673.0), D("2 14 3", 37666.0), D("2 3 14", 41673.0), D("32 2 3", 48247.0), D("2 3 32", 48247.0),
    };
    REQUIRE(U(VarDateFromStr));
    run_drows(rows, sizeof rows / sizeof *rows, VFP_EN_US, 0);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_two_digit_year_pivot_win10)
{
    /* Windows 10 1903+: the cutoff moved from 29 to 49. Wine records the older rule too; the corpus targets Win10 22H2. */
    static const DRow rows[] = {
        D("1 1 0", 36526.0), D("12 31 29", 47483.0), D("1 1 30", 47484.0), D("31 12 49", 54788.0), D("1 1 50", 18264.0), D("12 31 99", 36525.0), D("1 1 100", -657434.0),
        D("30 2", 47515.0), D("2 30", 47515.0), D("32 2 3", 48247.0), D("1/1/49", 54424.0), D("1/1/50", 18264.0), D("1/1/29", 47119.0), D("1/1/30", 47484.0),
        D("1/1/00", 36526.0), D("1/1/99", 36161.0), D("12/31/49", 54788.0), D("1/1/01", 36892.0),
        D("Jan 35", 49310.0), D("35 Jan", 49310.0), D("Jan-35", 49310.0), D("35-Jan", 49310.0), D("Jan/35", 49310.0), D("35/Jan", 49310.0),
    };
    REQUIRE(U(VarDateFromStr));
    run_drows(rows, sizeof rows / sizeof *rows, VFP_EN_US, 0);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_relative_to_current_year)
{
    SYSTEMTIME st;
    DATE out;
    HRESULT hr;
    REQUIRE(U(VarDateFromStr));
    GetSystemTime(&st);
    /* no year given: the current year is assumed */
    hr = dfs("1 2", VFP_EN_US, 0, &out);       EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 2)), "1 2 -> %.17g", out);
    hr = dfs("2 1", VFP_EN_US, 0, &out);       EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 2, 1)), "2 1 -> %.17g", out);
    hr = dfs("14 1", VFP_EN_US, 0, &out);      EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 14)), "14 1 -> %.17g", out);
    hr = dfs("1 14", VFP_EN_US, 0, &out);      EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 14)), "1 14 -> %.17g", out);
    hr = dfs("Jan 2", VFP_EN_US, 0, &out);     EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 2)), "Jan 2 -> %.17g", out);
    hr = dfs("2 Jan", VFP_EN_US, 0, &out);     EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 2)), "2 Jan -> %.17g", out);
    hr = dfs("3 am 1 2", VFP_EN_US, 0, &out);  EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 2) + 0.125), "3 am 1 2 -> %.17g", out);
    hr = dfs("1 2 3 am", VFP_EN_US, 0, &out);  EXPECT_EQ(hr, S_OK); CHECKF(dbl_eq(out, vfp_serial(st.wYear, 1, 2) + 0.125), "1 2 3 am -> %.17g", out);
    hr = dfs("1.2 3 4", VFP_EN_US, 0, &out);   EXPECT_EQ(hr, S_OK); CHECKF(fabs(out - (vfp_serial(st.wYear, 3, 4) + 0.04305555556)) < 1e-8, "1.2 3 4 -> %.17g", out);
    hr = dfs("3 4 1.2", VFP_EN_US, 0, &out);   EXPECT_EQ(hr, S_OK); CHECKF(fabs(out - (vfp_serial(st.wYear, 3, 4) + 0.04305555556)) < 1e-8, "3 4 1.2 -> %.17g", out);
    hr = dfs("1.2.3 4 5", VFP_EN_US, 0, &out); EXPECT_EQ(hr, S_OK); CHECKF(fabs(out - (vfp_serial(st.wYear, 4, 5) + 0.04309027778)) < 1e-8, "1.2.3 4 5 -> %.17g", out);
    hr = dfs("6/30", VFP_EN_US, 0, &out);      t_note("\"6/30\" -> hr=0x%lx %.17g", (unsigned long)hr, out);
    hr = dfs("June 30", VFP_EN_US, 0, &out);   t_note("\"June 30\" -> hr=0x%lx %.17g", (unsigned long)hr, out);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_four_to_six_elements)
{
    static const DRow rows[] = {
        D("1.2 3 4 5", 38415.04305555556), D("1 2 3 4.5", 37623.17013888889), D("1.2.3 4 5 6", 38812.04309027778), D("1 2 3 4.5.6", 37623.17020833334),
    };
    REQUIRE(U(VarDateFromStr));
    run_drows(rows, sizeof rows / sizeof *rows, VFP_EN_US, 0);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_rejected_forms)
{
    static const char *const bad[] = {
        "True", "False", "0.", ".0", "-1.1", "1.1-", "1;2;3", "1*2*3", "1@2@3", "1#2#3", "(1:2)", "<1:2>", "1|2|3", "0", "1", "0.60", "24.00", "0:60", "24:00", "1 2 am", "1 am 2",
        "1.5 2", "1 5.2", "2 32 3", "1 2 am 3", "1 2.3 4", "1.2.3 4", "1 2.3.4", "1.2 3.4", "1.2.3.4", "1 2 3 4", "1 am 2 3.4", "1 2 am 3.4", "1.2 3 am 4", "1.2 3 4 am",
        "1.2.3.4.5", "1.2.3.4 5", "1.2.3 4.5", "1.2 3.4.5", "1.2 3.4 5", "1.2 3 4.5", "1 2.3.4.5", "1 2.3.4 5", "1 2.3 4.5", "1 2.3 4 5", "1 2 3.4 5", "1 2 3 4 5",
        "1.2.3 4 am 5", "1.2.3 4 5 am", "1.2 3 am 4 5", "1.2 3 4 am 5", "1.2 3 4 5 am", "1 am 2 3.4.5", "1 2 am 3.4.5", "1 am 2 3 4.5", "1 2 am 3 4.5", "1 2 3 am 4.5",
        "1.2.3.4.5.6", "1.2.3.4.5 6", "1.2.3.4 5.6", "1.2.3.4 5 6", "1.2.3 4.5.6", "1.2.3 4.5 6", "1.2.3 4 5.6", "1.2 3.4.5.6", "1.2 3.4.5 6", "1.2 3.4 5.6", "1.2 3.4 5 6",
        "1.2 3 4.5.6", "1.2 3 4.5 6", "1.2 3 4 5.6", "1.2 3 4 5 6", "1 2.3.4.5.6", "1 2.3.4.5 6", "1 2.3.4 5.6", "1 2.3.4 5 6", "1 2.3 4.5.6", "1 2 3.4 5.6", "1 2 3.4 5 6",
        "1 2 3 4.5 6", "1 2 3 4 5.6", "1 2 3 4 5 6", "1 2 am 3 4.5.6", "1 2 3 am 4.5.6", "0.0", "32 49", "0 49",
    };
    size_t i;
    DATE out;
    REQUIRE(U(VarDateFromStr));
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = dfs(bad[i], VFP_EN_US, 0, &out);
        if (!strcmp(bad[i], "0.0") && hr == S_OK) { t_note("\"0.0\" accepted (old windows)"); continue; }
        CHECKF(hr == VFP_E_TYPEMISMATCH, "\"%s\": hr=0x%lx out=%.17g", bad[i], (unsigned long)hr, out);
    }
}
TEST(vfp_datefromstr_null_and_empty)
{
    DATE out = 5.0;
    REQUIRE(U(VarDateFromStr));
    EXPECT_EQ(U(VarDateFromStr)(NULL, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &out), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(dfs("", VFP_EN_US, 0, &out), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(dfs(" ", VFP_EN_US, 0, &out), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(dfs("not a date", VFP_EN_US, 0, &out), VFP_E_TYPEMISMATCH);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_ideographic_space)
{
    static const WCHAR s[] = { '6', '/', '3', '0', '/', '2', '0', '1', '1', 0x3000, '1', ':', '2', '0', ':', '3', '4', 0 };
    DATE out = 0;
    HRESULT hr;
    REQUIRE(U(VarDateFromStr));
    hr = U(VarDateFromStr)(s, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &out);
    EXPECT_EQ(hr, S_OK);
    CHECKF(dbl_eq(out, 40724.05594907407), "got %.17g", out);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_datefromstr_locale_date_order)
{
    static const DRow en[] = { DX("02.01.1970"), DX("02.01.1970 00:00:00") };
    static const DRow de[] = { D("02.01.1970", 25570.0), D("02.13.1970", 25612.0), D("02-13-1970", 25612.0), D("2020-01-11", 43841.0), D("2173-10-14", 100000.0), D("02.01.1970 00:00:00", 25570.0) };
    static const DRow es[] = { DX("02.01.1970") };
    REQUIRE(U(VarDateFromStr));
    run_drows(en, sizeof en / sizeof *en, VFP_EN_US, 0);
    run_drows(de, sizeof de / sizeof *de, VFP_DE_DE, 0);
    run_drows(es, sizeof es / sizeof *es, VFP_ES_ES, 0);
}
TEST(vfp_datefromstr_german_month_names)
{
    /* month names parse in the LCID's own language (observation only) */
    static const struct { const WCHAR *s; double v; } rows[] = {
        { L"1. Januar 2000", 36526.0 }, { L"2. Januar 2000", 36527.0 }, { L"31. Dezember 1999", 36525.0 }, { L"5. M\x00e4rz 2020", 43895.0 },
    };
    DATE out;
    size_t i;
    REQUIRE(U(VarDateFromStr));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        HRESULT hr = U(VarDateFromStr)(rows[i].s, VFP_DE_DE, LOCALE_NOUSEROVERRIDE, &out);
        if (hr != S_OK || !dbl_eq(out, rows[i].v)) t_note("de-DE row %u: hr=0x%lx out=%.17g want %.17g", (unsigned)i, (unsigned long)hr, out, rows[i].v);
    }
}
TEST(vfp_datefromstr_flags_time_and_date_only)
{
    DATE out;
    REQUIRE(U(VarDateFromStr));
    EXPECT_EQ(dfs("1/2/2000 12:30 PM", VFP_EN_US, 0, &out), S_OK);
    CHECKF(dbl_eq(out, 36527.5208333333333), "full: %.17g", out);
    EXPECT_EQ(dfs("1/2/2000 12:30 PM", VFP_EN_US, VAR_TIMEVALUEONLY, &out), S_OK);
    CHECKF(dbl_eq(out, 0.5208333333333334), "time only: %.17g", out);
    EXPECT_EQ(dfs("1/2/2000 12:30 PM", VFP_EN_US, VAR_DATEVALUEONLY, &out), S_OK);
    CHECKF(dbl_eq(out, 36527.0), "date only: %.17g", out);
    EXPECT_EQ(dfs("12:30 PM", VFP_EN_US, VAR_TIMEVALUEONLY, &out), S_OK);
    CHECKF(dbl_eq(out, 0.5208333333333334), "time only on time string: %.17g", out);
    t_note("both TIME and DATE only: hr=0x%lx %.17g", (unsigned long)dfs("1/2/2000 12:30 PM", VFP_EN_US, VAR_TIMEVALUEONLY | VAR_DATEVALUEONLY, &out), out);
    EXPECT_EQ(dfs("12:30 PM", VFP_EN_US, VAR_DATEVALUEONLY, &out), S_OK);
    t_note("DATEVALUEONLY on time-only string -> %.17g", out);
    EXPECT_EQ(dfs("1/2/2000", VFP_EN_US, VAR_TIMEVALUEONLY, &out), S_OK);
    t_note("TIMEVALUEONLY on date-only string -> %.17g", out);
}
TEST(vfp_datefromstr_calendar_hijri_flag)
{
    DATE out;
    HRESULT hr;
    REQUIRE(U(VarDateFromStr));
    hr = dfs("1/1/1420", VFP_EN_US, VAR_CALENDAR_HIJRI, &out);
    t_note("hijri 1/1/1420 -> hr=0x%lx %.17g", (unsigned long)hr, out);
    hr = dfs("1/1/2000", VFP_EN_US, VAR_CALENDAR_HIJRI, &out);
    t_note("hijri 1/1/2000 -> hr=0x%lx %.17g", (unsigned long)hr, out);
}

/* ---- VarBstrFrom* --------------------------------------------------------- */
typedef struct { double v; const char *en; const char *es; } RRow;
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_bstrfromr8_table)
{
    static const RRow rows[] = {
        { 0.56789, "0.56789", "0,56789" }, { 5.6789e-2, "0.056789", "0,056789" }, { 5.6789e-3, "0.0056789", NULL }, { 5.6789e-4, "0.00056789", NULL },
        { 5.6789e-5, "0.000056789", NULL }, { 5.6789e-6, "0.0000056789", NULL }, { 5.6789e-7, "0.00000056789", NULL }, { 5.6789e-8, "0.000000056789", NULL },
        { 5.6789e-9, "0.0000000056789", NULL }, { 5.6789e-10, "0.00000000056789", NULL }, { 5.6789e-11, "0.000000000056789", NULL }, { 5.6789e-12, "5.6789E-12", NULL },
        { 5.6789e-13, "5.6789E-13", NULL }, { 5.6789e-14, "5.6789E-14", NULL }, { 5.6789e-15, "5.6789E-15", NULL }, { 5.6789e-16, "5.6789E-16", NULL },
        { 1.0e8, "100000000", "100000000" }, { 1.0e12, "1000000000000", "1000000000000" }, { 1.0e14, "100000000000000", "100000000000000" },
        { 999999999999999.0, "999999999999999", "999999999999999" }, { 1000000000000000.0, "1E+15", "1E+15" }, { 1200000000000000.0, "1.2E+15", "1,2E+15" },
        { 1.0e15, "1E+15", "1E+15" }, { 1.000e16, "1E+16", "1E+16" }, { 1.234e16, "1.234E+16", "1,234E+16" }, { 3.141592653590000, "3.14159265359", "3,14159265359" },
        { 3.141592653589793, "3.14159265358979", "3,14159265358979" }, { 12.345678901234567, "12.3456789012346", "12,3456789012346" },
        { 123.456789012345678, "123.456789012346", "123,456789012346" }, { 1234.567890123456789, "1234.56789012346", "1234,56789012346" },
        { 0.5, "0.5", "0,5" }, { 0.05, "0.05", "0,05" }, { 0.005, "0.005", "0,005" }, { 0.0005, "0.0005", "0,0005" }, { 0.00005, "0.00005", "0,00005" }, { 0.000005, "0.000005", "0,000005" },
        { 0.0, "0", "0" }, { 1.0, "1", "1" }, { -1.0, "-1", "-1" }, { 100.0, "100", "100" }, { -0.5, "-0.5", "-0,5" }, { 1.5, "1.5", "1,5" }, { -1234.5, "-1234.5", "-1234,5" },
        { 123456789.0, "123456789", "123456789" }, { 1e5, "100000", "100000" }, { 0.1, "0.1", "0,1" }, { 0.25, "0.25", "0,25" }, { 2.5e-5, "0.000025", NULL },
        { 1e100, "1E+100", "1E+100" }, { 1e-100, "1E-100", "1E-100" }, { -1e15, "-1E+15", "-1E+15" }, { 1.7976931348623157e308, "1.79769313486232E+308", "1,79769313486232E+308" },
    };
    size_t i;
    REQUIRE(U(VarBstrFromR8)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        HRESULT hr = U(VarBstrFromR8)(rows[i].v, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en) && vfp_bstr_ok(b), "en %.17g: want %s got hr=0x%lx %s", rows[i].v, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
        if (rows[i].es) {
            b = NULL;
            hr = U(VarBstrFromR8)(rows[i].v, VFP_ES_ES, LOCALE_NOUSEROVERRIDE, &b);
            CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].es), "es %.17g: want %s got hr=0x%lx %s", rows[i].v, rows[i].es, (unsigned long)hr, vfp_n(b));
            if (b) U(SysFreeString)(b);
        }
    }
}
TEST(vfp_bstrfromr8_special_values)
{
    BSTR b = NULL;
    HRESULT hr;
    volatile double zero = 0.0;
    double nan = zero / zero, inf = 1.0 / zero;
    REQUIRE(U(VarBstrFromR8)); REQUIRE(U(SysFreeString));
    hr = U(VarBstrFromR8)(nan, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
    t_note("VarBstrFromR8(NaN) hr=0x%lx %s", (unsigned long)hr, vfp_n(b)); if (b) U(SysFreeString)(b); b = NULL;
    hr = U(VarBstrFromR8)(inf, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
    t_note("VarBstrFromR8(+Inf) hr=0x%lx %s", (unsigned long)hr, vfp_n(b)); if (b) U(SysFreeString)(b); b = NULL;
    hr = U(VarBstrFromR8)(-zero, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
    t_note("VarBstrFromR8(-0.0) hr=0x%lx %s", (unsigned long)hr, vfp_n(b)); if (b) U(SysFreeString)(b);
}
TEST(vfp_bstrfromr4_table)
{
    static const struct { float v; const char *en; } rows[] = {
        { 0.0f, "0" }, { 1.0f, "1" }, { -1.0f, "-1" }, { 1.5f, "1.5" }, { 0.5f, "0.5" }, { 100.0f, "100" }, { 0.25f, "0.25" }, { 1234.5f, "1234.5" },
        { 3.14159f, "3.14159" }, { 123456.0f, "123456" }, { -0.125f, "-0.125" },
    };
    size_t i;
    REQUIRE(U(VarBstrFromR4)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        HRESULT hr = U(VarBstrFromR4)(rows[i].v, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en), "R4 %.9g: want %s got hr=0x%lx %s", rows[i].v, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
    }
}
TEST(vfp_bstrfromi4_i2_table)
{
    static const struct { long v; const char *en; } rows[] = { { 0, "0" }, { 1, "1" }, { -1, "-1" }, { 1234567, "1234567" }, { -1234567, "-1234567" }, { 2147483647L, "2147483647" }, { -2147483647L - 1, "-2147483648" }, { 100000, "100000" } };
    size_t i;
    BSTR b;
    REQUIRE(U(VarBstrFromI4)); REQUIRE(U(VarBstrFromI2)); REQUIRE(U(VarBstrFromUI4)); REQUIRE(U(VarBstrFromI8)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        HRESULT hr;
        b = NULL;
        hr = U(VarBstrFromI4)(rows[i].v, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en) && vfp_bstr_ok(b), "I4 %ld: want %s got hr=0x%lx %s", rows[i].v, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
        b = NULL;
        hr = U(VarBstrFromI4)(rows[i].v, VFP_DE_DE, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en), "I4 de %ld: no digit grouping expected, got %s", rows[i].v, vfp_n(b));
        if (b) U(SysFreeString)(b);
    }
    b = NULL; EXPECT_EQ(U(VarBstrFromI2)(-32768, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"-32768")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromUI4)(4294967295UL, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"4294967295")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromI8)(-9223372036854775807LL - 1, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"-9223372036854775808")); if (b) U(SysFreeString)(b);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_bstrfromdate_table)
{
    static const struct { double v; const char *en; } rows[] = {
        { 0.0, "12:00:00 AM" }, { 3.34, "1/2/1900 8:09:36 AM" }, { 3339.34, "2/20/1909 8:09:36 AM" }, { 365.00, "12/30/1900" }, { 365.25, "12/30/1900 6:00:00 AM" },
        { 1461.0, "12/31/1903" }, { 1461.5, "12/31/1903 12:00:00 PM" }, { -49192.24, "4/24/1765 5:45:36 AM" }, { -657434.0, "1/1/100" }, { 2958465.0, "12/31/9999" },
        { 36526.0, "1/1/2000" }, { 36526.5, "1/1/2000 12:00:00 PM" }, { 36526.25, "1/1/2000 6:00:00 AM" }, { 36526.75, "1/1/2000 6:00:00 PM" }, { 0.5, "12:00:00 PM" },
        { 0.25, "6:00:00 AM" }, { 0.75, "6:00:00 PM" }, { 1.0, "12/31/1899" }, { 2.0, "1/1/1900" }, { 60.0, "2/28/1900" }, { 61.0, "3/1/1900" }, { 44197.0, "1/1/2021" },
        { 40724.05594907407, "6/30/2011 1:20:34 AM" }, { 40724.55594907407, "6/30/2011 1:20:34 PM" }, { 36585.0, "2/29/2000" },
        { 0.041666666666666664, "1:00:00 AM" }, { 0.5416666666666666, "1:00:00 PM" },
    };
    size_t i;
    REQUIRE(U(VarBstrFromDate)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        HRESULT hr = U(VarBstrFromDate)(rows[i].v, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en) && vfp_bstr_ok(b), "DATE %.17g: want \"%s\" got hr=0x%lx \"%s\"", rows[i].v, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
    }
}
TEST(vfp_bstrfromdate_flags)
{
    static const struct { double v; ULONG flags; const char *en; } rows[] = {
        { 3.34, VAR_TIMEVALUEONLY, "8:09:36 AM" }, { 3.34, VAR_DATEVALUEONLY, "1/2/1900" }, { 36526.5, VAR_TIMEVALUEONLY, "12:00:00 PM" }, { 36526.5, VAR_DATEVALUEONLY, "1/1/2000" },
        { 36526.0, VAR_DATEVALUEONLY, "1/1/2000" }, { 36526.5, VAR_FOURDIGITYEARS, "1/1/2000 12:00:00 PM" },
        { 36526.5, VAR_FOURDIGITYEARS | VAR_DATEVALUEONLY, "1/1/2000" },
    };
    size_t i;
    REQUIRE(U(VarBstrFromDate)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        HRESULT hr = U(VarBstrFromDate)(rows[i].v, VFP_EN_US, rows[i].flags | LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en), "DATE %.17g flags 0x%lx: want \"%s\" got hr=0x%lx \"%s\"", rows[i].v, rows[i].flags, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
    }
}
TEST(vfp_bstrfromdate_german_format)
{
    static const struct { double v; const char *de; } rows[] = { { 36526.0, "01.01.2000" }, { 36526.5, "01.01.2000 12:00:00" }, { 36526.75, "01.01.2000 18:00:00" }, { 0.5, "12:00:00" } };
    size_t i;
    REQUIRE(U(VarBstrFromDate)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        HRESULT hr = U(VarBstrFromDate)(rows[i].v, VFP_DE_DE, LOCALE_NOUSEROVERRIDE, &b);
        if (hr != S_OK || !b || strcmp(vfp_n(b), rows[i].de)) t_note("de-DE DATE %.17g: want \"%s\" got hr=0x%lx \"%s\"", rows[i].v, rows[i].de, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
    }
}
TEST(vfp_bstrfromdate_out_of_range)
{
    BSTR b = NULL;
    HRESULT hr;
    REQUIRE(U(VarBstrFromDate)); REQUIRE(U(SysFreeString));
    hr = U(VarBstrFromDate)(2958466.0, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("VarBstrFromDate(2958466) hr=0x%lx", (unsigned long)hr);
    if (b) U(SysFreeString)(b);
    b = NULL;
    hr = U(VarBstrFromDate)(-657435.0, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    if (b) U(SysFreeString)(b);
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_bstrfromcy_table)
{
    static const struct { LONG64 units; const char *en; const char *es; } rows[] = {
        { 0, "0", "0" }, { 10000, "1", "1" }, { 15000, "1.5", "1,5" }, { -15000, "-1.5", "-1,5" }, { 0xffffffffLL, "429496.7295", "429496,7295" }, { 0x100000000LL, "429496.7296", "429496,7296" },
        { 0x7fffffffffffffffLL, "922337203685477.5807", "922337203685477,5807" }, { 9, "0.0009", "0,0009" },
        { 1, "0.0001", "0,0001" }, { -1, "-0.0001", "-0,0001" }, { 100, "0.01", "0,01" }, { 12345678, "1234.5678", "1234,5678" }, { -12345678, "-1234.5678", "-1234,5678" },
        { 1000000, "100", "100" }, { 12500, "1.25", "1,25" }, { (LONG64)0x8000000000000000ULL, "-922337203685477.5808", "-922337203685477,5808" }, { 100000000, "10000", "10000" },
    };
    size_t i;
    REQUIRE(U(VarBstrFromCy)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        CY cy;
        HRESULT hr;
        cy.int64 = rows[i].units;
        hr = U(VarBstrFromCy)(cy, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en) && vfp_bstr_ok(b), "CY %lld en: want %s got hr=0x%lx %s", (long long)rows[i].units, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
        b = NULL;
        hr = U(VarBstrFromCy)(cy, VFP_ES_ES, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].es), "CY %lld es: want %s got hr=0x%lx %s", (long long)rows[i].units, rows[i].es, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
    }
}
/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(vfp_bstrfromdec_table)
{
    static const struct { BYTE scale, sign; ULONG hi, mid, lo; const char *en; const char *es; } rows[] = {
        { 0, 0, 0, 0, 0, "0", NULL }, { 0, 0, 0, 0, 1, "1", NULL }, { 1, 0, 0, 0, 10, "1", NULL }, { 2, 0, 0, 0, 100, "1", NULL }, { 3, 0, 0, 0, 1000, "1", NULL },
        { 1, 0, 0, 0, 15, "1.5", "1,5" }, { 2, 0, 0, 0, 150, "1.5", "1,5" }, { 3, 0, 0, 0, 1500, "1.5", "1,5" }, { 1, 0x80, 0, 0, 15, "-1.5", "-1,5" },
        { 0, 0, 0, 0, 0xffffffff, "4294967295", NULL }, { 0, 0, 0, 1, 0, "4294967296", NULL }, { 0, 0, 0, 0xffffffff, 0xffffffff, "18446744073709551615", NULL },
        { 0, 0, 1, 0, 0, "18446744073709551616", NULL }, { 0, 0, 0xffffffff, 0xffffffff, 0xffffffff, "79228162514264337593543950335", NULL },
        { 10, 0, 0, 0, 1, "0.0000000001", NULL }, { 10, 0, 0xffffffff, 0xffffffff, 0xffffffff, "7922816251426433759.3543950335", NULL },
        { 28, 0, 0xffffffff, 0xffffffff, 0xffffffff, "7.9228162514264337593543950335", NULL },
        { 4, 0, 0, 0, 9, "0.0009", "0,0009" }, { 5, 0, 0, 0, 90, "0.0009", "0,0009" }, { 6, 0, 0, 0, 900, "0.0009", "0,0009" }, { 7, 0, 0, 0, 9000, "0.0009", "0,0009" },
        { 2, 0, 0, 0, 12345, "123.45", "123,45" }, { 2, 0x80, 0, 0, 12345, "-123.45", "-123,45" }, { 0, 0x80, 0, 0, 7, "-7", NULL }, { 28, 0, 0, 0, 1, "0.0000000000000000000000000001", NULL },
    };
    size_t i;
    REQUIRE(U(VarBstrFromDec)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        BSTR b = NULL;
        DECIMAL dec;
        HRESULT hr;
        memset(&dec, 0, sizeof dec);
        dec.scale = rows[i].scale; dec.sign = rows[i].sign; dec.Hi32 = rows[i].hi; dec.Mid32 = rows[i].mid; dec.Lo32 = rows[i].lo;
        hr = U(VarBstrFromDec)(&dec, VFP_EN_US, LOCALE_NOUSEROVERRIDE, &b);
        CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].en) && vfp_bstr_ok(b), "DEC #%u en: want %s got hr=0x%lx %s", (unsigned)i, rows[i].en, (unsigned long)hr, vfp_n(b));
        if (b) U(SysFreeString)(b);
        if (rows[i].es) {
            b = NULL;
            hr = U(VarBstrFromDec)(&dec, VFP_ES_ES, LOCALE_NOUSEROVERRIDE, &b);
            CHECKF(hr == S_OK && b && !strcmp(vfp_n(b), rows[i].es), "DEC #%u es: want %s got hr=0x%lx %s", (unsigned)i, rows[i].es, (unsigned long)hr, vfp_n(b));
            if (b) U(SysFreeString)(b);
        }
    }
}
TEST(vfp_bstrfrombool_flags)
{
    BSTR b;
    REQUIRE(U(VarBstrFromBool)); REQUIRE(U(SysFreeString));
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(VARIANT_TRUE, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"True")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(VARIANT_FALSE, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"False")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(VARIANT_TRUE, VFP_EN_US, VARIANT_ALPHABOOL, &b), S_OK); CHECK(b && !wcscmp(b, L"True")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(VARIANT_FALSE, VFP_EN_US, VARIANT_ALPHABOOL, &b), S_OK); CHECK(b && !wcscmp(b, L"False")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(VARIANT_TRUE, VFP_EN_US, VARIANT_LOCALBOOL, &b), S_OK); CHECK(b && !wcscmp(b, L"True")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(VARIANT_FALSE, VFP_EN_US, VARIANT_LOCALBOOL, &b), S_OK); CHECK(b && !wcscmp(b, L"False")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(1, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"True")); if (b) U(SysFreeString)(b);
    b = NULL; EXPECT_EQ(U(VarBstrFromBool)(0x1234, VFP_EN_US, 0, &b), S_OK); CHECK(b && !wcscmp(b, L"True")); if (b) U(SysFreeString)(b);
    b = NULL; U(VarBstrFromBool)(VARIANT_TRUE, VFP_ES_ES, VARIANT_LOCALBOOL, &b); t_note("es-ES LOCALBOOL true: %s", vfp_n(b)); if (b) U(SysFreeString)(b);
    b = NULL; U(VarBstrFromBool)(VARIANT_TRUE, VFP_ES_ES, 0, &b); t_note("es-ES default true: %s", vfp_n(b)); if (b) U(SysFreeString)(b);
    b = NULL; U(VarBstrFromBool)(VARIANT_TRUE, VFP_DE_DE, VARIANT_LOCALBOOL, &b); t_note("de-DE LOCALBOOL true: %s", vfp_n(b)); if (b) U(SysFreeString)(b);
}

/* ---- VarUdateFromDate / VarDateFromUdate ------------------------------------- */
static void check_u_from_d(int line, DATE dt, ULONG flags, HRESULT r, WORD d, WORD m, WORD y, WORD h, WORD mn, WORD s, WORD ms, WORD dw, WORD dy)
{
    UDATE ud;
    HRESULT res;
    memset(&ud, 0, sizeof ud);
    res = U(VarUdateFromDate)(dt, flags, &ud);
    t_check(r == res && (res != S_OK || (ud.st.wYear == y && ud.st.wMonth == m && ud.st.wDay == d && ud.st.wHour == h && ud.st.wMinute == mn && ud.st.wSecond == s &&
            ud.st.wMilliseconds == ms && ud.st.wDayOfWeek == dw && ud.wDayOfYear == dy)), __FILE__, line, "VarUdateFromDate",
            "%.16g flags 0x%lx want hr=%lx %d/%d/%d %d:%d:%d.%d dow %d doy %d, got hr=%lx %d/%d/%d %d:%d:%d.%d dow %d doy %d", dt, flags, (unsigned long)r, m, d, y, h, mn, s, ms, dw, dy,
            (unsigned long)res, ud.st.wMonth, ud.st.wDay, ud.st.wYear, ud.st.wHour, ud.st.wMinute, ud.st.wSecond, ud.st.wMilliseconds, ud.st.wDayOfWeek, ud.wDayOfYear);
}
#define DT2UD(dt, flags, r, d, m, y, h, mn, s, ms, dw, dy) check_u_from_d(__LINE__, dt, flags, r, d, m, y, h, mn, s, ms, dw, dy)

/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_udatefromdate_table)
{
    REQUIRE(U(VarUdateFromDate));
    DT2UD(29221.0, 0, S_OK, 1, 1, 1980, 0, 0, 0, 0, 2, 1);
    DT2UD(29222.0, 0, S_OK, 2, 1, 1980, 0, 0, 0, 0, 3, 2);
    DT2UD(33238.0, 0, S_OK, 31, 12, 1990, 0, 0, 0, 0, 1, 365);
    DT2UD(0.0, 0, S_OK, 30, 12, 1899, 0, 0, 0, 0, 6, 364);
    DT2UD(-657434.0, 0, S_OK, 1, 1, 100, 0, 0, 0, 0, 5, 1);
    DT2UD(-657435.0, 0, VFP_E_INVALIDARG, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DT2UD(2958465.0, 0, S_OK, 31, 12, 9999, 0, 0, 0, 0, 5, 365);
    DT2UD(2958466.0, 0, VFP_E_INVALIDARG, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DT2UD(-657435.0, VAR_VALIDDATE, VFP_E_INVALIDARG, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DT2UD(2958466.0, VAR_VALIDDATE, VFP_E_INVALIDARG, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DT2UD(29221.25, 0, S_OK, 1, 1, 1980, 6, 0, 0, 0, 2, 1);
    DT2UD(29221.33333333, 0, S_OK, 1, 1, 1980, 8, 0, 0, 0, 2, 1);
    DT2UD(29221.5, 0, S_OK, 1, 1, 1980, 12, 0, 0, 0, 2, 1);
    DT2UD(29221.9888884444, 0, S_OK, 1, 1, 1980, 23, 44, 0, 0, 2, 1);
    DT2UD(29221.7508765432, 0, S_OK, 1, 1, 1980, 18, 1, 16, 0, 2, 1);
    DT2UD(-5.25, 0, S_OK, 25, 12, 1899, 6, 0, 0, 0, 1, 359);
    DT2UD(-5.9999884259259, 0, S_OK, 25, 12, 1899, 23, 59, 59, 0, 1, 359);
    DT2UD(-4.0, 0, S_OK, 26, 12, 1899, 0, 0, 0, 0, 2, 360);
    DT2UD(-0.25, 0, S_OK, 30, 12, 1899, 6, 0, 0, 0, 6, 364);
    DT2UD(0.25, 0, S_OK, 30, 12, 1899, 6, 0, 0, 0, 6, 364);
}
/* wDayOfWeek 0 = Sunday, wDayOfYear 1-based; values computed from the proleptic Gregorian calendar (OLE has no 29 Feb 1900) */
TEST(vfp_udatefromdate_calendar_facts)
{
    REQUIRE(U(VarUdateFromDate));
    DT2UD(36585.0, 0, S_OK, 29, 2, 2000, 0, 0, 0, 0, 2, 60);
    DT2UD(36586.0, 0, S_OK, 1, 3, 2000, 0, 0, 0, 0, 3, 61);
    DT2UD(36525.0, 0, S_OK, 31, 12, 1999, 0, 0, 0, 0, 5, 365);
    DT2UD(36526.0, 0, S_OK, 1, 1, 2000, 0, 0, 0, 0, 6, 1);
    DT2UD(36891.0, 0, S_OK, 31, 12, 2000, 0, 0, 0, 0, 0, 366);
    DT2UD(44197.0, 0, S_OK, 1, 1, 2021, 0, 0, 0, 0, 5, 1);
    DT2UD(44561.0, 0, S_OK, 31, 12, 2021, 0, 0, 0, 0, 5, 365);
    DT2UD(2.0, 0, S_OK, 1, 1, 1900, 0, 0, 0, 0, 1, 1);
    DT2UD(59.0, 0, S_OK, 27, 2, 1900, 0, 0, 0, 0, 2, 58);
    DT2UD(60.0, 0, S_OK, 28, 2, 1900, 0, 0, 0, 0, 3, 59);
    DT2UD(61.0, 0, S_OK, 1, 3, 1900, 0, 0, 0, 0, 4, 60);
    DT2UD(1.0, 0, S_OK, 31, 12, 1899, 0, 0, 0, 0, 0, 365);
    DT2UD(-1.0, 0, S_OK, 29, 12, 1899, 0, 0, 0, 0, 5, 363);
    DT2UD(40724.55594907407, 0, S_OK, 30, 6, 2011, 13, 20, 34, 0, 4, 181);
    DT2UD(35065.0, 0, S_OK, 1, 1, 1996, 0, 0, 0, 0, 1, 1);
    DT2UD(0.5, 0, S_OK, 30, 12, 1899, 12, 0, 0, 0, 6, 364);
}
static void check_d_from_u(int line, WORD d, WORD m, WORD y, WORD h, WORD mn, WORD s, WORD ms, WORD dw, WORD dy, ULONG flags, HRESULT r, DATE dt)
{
    UDATE ud;
    DOUBLE out = -777.0;
    HRESULT res;
    memset(&ud, 0, sizeof ud);
    ud.st.wYear = y; ud.st.wMonth = m; ud.st.wDay = d; ud.st.wHour = h; ud.st.wMinute = mn; ud.st.wSecond = s; ud.st.wMilliseconds = ms; ud.st.wDayOfWeek = dw; ud.wDayOfYear = dy;
    res = U(VarDateFromUdate)(&ud, flags, &out);
    t_check(r == res && (r != S_OK || dbl_eq(out, dt)), __FILE__, line, "VarDateFromUdate", "%d/%d/%d %d:%d:%d flags 0x%lx want hr=%lx %.16g got hr=%lx %.16g", m, d, y, h, mn, s, flags, (unsigned long)r, dt, (unsigned long)res, out);
}
#define UD2T(d, m, y, h, mn, s, ms, dw, dy, flags, r, dt) check_d_from_u(__LINE__, d, m, y, h, mn, s, ms, dw, dy, flags, r, dt)

/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_datefromudate_table)
{
    REQUIRE(U(VarDateFromUdate));
    UD2T(1, 1, 1980, 0, 0, 0, 0, 2, 1, 0, S_OK, 29221.0);
    UD2T(2, 1, 1980, 0, 0, 0, 0, 3, 2, 0, S_OK, 29222.0);
    UD2T(2, 1, 1980, 0, 0, 0, 0, 4, 5, 0, S_OK, 29222.0);          /* wDayOfWeek / wDayOfYear are ignored */
    UD2T(31, 12, 1990, 0, 0, 0, 0, 0, 0, 0, S_OK, 33238.0);
    UD2T(31, 12, 90, 0, 0, 0, 0, 0, 0, 0, S_OK, 33238.0);          /* year < 100 is 1900 + year */
    UD2T(30, 12, 1899, 0, 0, 0, 0, 6, 364, 0, S_OK, 0.0);
    UD2T(1, 1, 100, 0, 0, 0, 0, 0, 0, 0, S_OK, -657434.0);
    UD2T(31, 12, 9999, 0, 0, 0, 0, 0, 0, 0, S_OK, 2958465.0);
    UD2T(1, 1, 10000, 0, 0, 0, 0, 0, 0, 0, VFP_E_INVALIDARG, 0.0);
    UD2T(1, 1, -10000, 0, 0, 0, 0, 0, 0, 0, VFP_E_INVALIDARG, 0.0);
    UD2T(30, 12, 1899, 0, 0, 0, 999, 0, 0, 0, S_OK, 0.0);          /* milliseconds are ignored */
    UD2T(1, 1, 1980, 18, 1, 16, 0, 2, 1, 0, S_OK, 29221.75087962963);
    UD2T(1, 300, 1980, 18, 1, 16, 0, 2, 1, 0, S_OK, 38322.75087962963);   /* month rolls forward */
    UD2T(300, 1, 1980, 18, 1, 16, 0, 2, 1, 0, S_OK, 29520.75087962963);   /* day rolls forward */
    UD2T(0, 1, 1980, 42, 1, 16, 0, 2, 1, 0, S_OK, 29221.75087962963);     /* hours roll forward */
    UD2T(1, 1, 1980, 17, 61, 16, 0, 2, 1, 0, S_OK, 29221.75087962963);
    UD2T(1, 1, 1980, 18, 0, 76, 0, 2, 1, 0, S_OK, 29221.75087962963);
    UD2T(1, -300, 1980, 18, 1, 16, 0, 2, 1, 0, S_OK, 20059.75087962963);
    UD2T(-300, 1, 1980, 18, 1, 16, 0, 2, 1, 0, S_OK, 28920.75087962963);
    UD2T(3, 1, 1980, -30, 1, 16, 0, 2, 1, 0, S_OK, 29221.75087962963);
    UD2T(1, 1, 1980, 20, -119, 16, 0, 2, 1, 0, S_OK, 29221.75087962963);
    UD2T(1, 1, 1980, 18, 3, -104, 0, 2, 1, 0, S_OK, 29221.75087962963);
    UD2T(1, 12001, -1020, 18, 1, 16, 0, 0, 0, 0, S_OK, 29221.75087962963);
    UD2T(1, -23, 1982, 18, 1, 16, 0, 0, 0, 0, S_OK, 29221.75087962963);
    UD2T(-59, 3, 1980, 18, 1, 16, 0, 0, 0, 0, S_OK, 29221.75087962963);
    UD2T(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, S_OK, 36526);                      /* year 0 is 2000 */
    UD2T(0, 0, 1980, 0, 0, 0, 0, 0, 0, 0, S_OK, 29189);
    UD2T(0, 1, 1980, 0, 0, 0, 0, 2, 1, 0, S_OK, 29220.0);                 /* day 0 is the last day of the previous month */
    UD2T(-1, 1, 1980, 18, 1, 16, 0, 0, 0, 0, S_OK, 29219.75087962963);
    UD2T(1, 1, -1, 18, 1, 16, 0, 0, 0, 0, S_OK, 36161.75087962963);       /* year -1 is 1999 */
    UD2T(1, -1, 1980, 18, 1, 16, 0, 0, 0, 0, S_OK, 29160.7508796296);
    UD2T(1, 13, 1980, 0, 0, 0, 0, 2, 1, 0, S_OK, 29587.0);
    UD2T(25, 12, 1899, 6, 0, 0, 0, 1, 359, 0, S_OK, -5.25);
    UD2T(25, 12, 1899, 23, 59, 59, 0, 1, 359, 0, S_OK, -5.9999884259259);
    UD2T(26, 12, 1899, 0, 0, 0, 0, 2, 360, 0, S_OK, -4.0);
    UD2T(30, 12, 1899, 6, 0, 0, 0, 6, 364, 0, S_OK, 0.25);
    UD2T(1, 1, 1980, 18, 1, 16, 0, 2, 1, VAR_TIMEVALUEONLY, S_OK, 0.7508796296296296);
    UD2T(1, 1, 1980, 18, 1, 16, 0, 2, 1, VAR_DATEVALUEONLY, S_OK, 29221.0);
    UD2T(25, 12, 1899, 6, 0, 0, 0, 1, 359, VAR_TIMEVALUEONLY, S_OK, 0.25);
    UD2T(25, 12, 1899, 6, 0, 0, 0, 1, 359, VAR_DATEVALUEONLY, S_OK, -5.0);
    UD2T(1, -1, 1980, 18, 1, 16, 0, 0, 0, VAR_TIMEVALUEONLY | VAR_DATEVALUEONLY, S_OK, 0.7508796296296296);
}
TEST(vfp_datefromudate_calendar_facts)
{
    REQUIRE(U(VarDateFromUdate));
    UD2T(29, 2, 2000, 12, 0, 0, 0, 0, 0, 0, S_OK, 36585.5);
    UD2T(31, 12, 9999, 23, 59, 59, 0, 0, 0, 0, S_OK, 2958465.99998842593);
    UD2T(1, 1, 99, 0, 0, 0, 0, 0, 0, 0, S_OK, 36161.0);                   /* 99 -> 1999 */
    UD2T(1, 1, 30, 0, 0, 0, 0, 0, 0, 0, S_OK, 47484.0);                   /* VarDateFromUdate = VarDateFromUdateEx(lcid 0x409): Win10 two digit year pivot, 30 -> 2030 */
    UD2T(1, 1, 29, 0, 0, 0, 0, 0, 0, 0, S_OK, 47119.0);
    UD2T(1, 1, 2000, 24, 0, 0, 0, 0, 0, 0, S_OK, 36527.0);                /* hour 24 rolls to next day */
    UD2T(31, 4, 2000, 0, 0, 0, 0, 0, 0, 0, S_OK, 36647.0);                /* 31 April rolls to 1 May */
    UD2T(29, 2, 2001, 0, 0, 0, 0, 0, 0, 0, S_OK, 36951.0);                /* 29 Feb 2001 rolls to 1 March */
}
TEST(vfp_udate_roundtrip)
{
    UDATE ud;
    int i;
    REQUIRE(U(VarUdateFromDate)); REQUIRE(U(VarDateFromUdate));
    for (i = 0; i < 4000; i++) {
        double v = 100.0 + i * 13.75 + (i % 24) / 24.0;
        DATE back = -1;
        memset(&ud, 0, sizeof ud);
        if (U(VarUdateFromDate)(v, 0, &ud) != S_OK) { CHECKF(0, "VarUdateFromDate(%.6f) failed", v); break; }
        if (U(VarDateFromUdate)(&ud, 0, &back) != S_OK) { CHECKF(0, "VarDateFromUdate failed for %.6f", v); break; }
        if (fabs(back - v) > 2e-8) { CHECKF(0, "roundtrip %.9f -> %.9f", v, back); break; }
    }
}
TEST(vfp_udate_from_date_midnight_time_note)
{
    UDATE ud;
    REQUIRE(U(VarUdateFromDate));
    memset(&ud, 0xcc, sizeof ud);
    EXPECT_EQ(U(VarUdateFromDate)(36526.0, 0, &ud), S_OK);
    EXPECT_EQ(ud.st.wHour, 0); EXPECT_EQ(ud.st.wMinute, 0); EXPECT_EQ(ud.st.wSecond, 0); EXPECT_EQ(ud.st.wMilliseconds, 0);
    EXPECT_EQ(ud.st.wDayOfWeek, 6); EXPECT_EQ(ud.wDayOfYear, 1);
}
