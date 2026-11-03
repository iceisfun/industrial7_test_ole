#include "cmg.h"

typedef BOOL (WINAPI *d2f_fn)(WORD, WORD, FILETIME *);
typedef BOOL (WINAPI *f2d_fn)(const FILETIME *, WORD *, WORD *);
typedef HRESULT (WINAPI *now_fn)(FILETIME *);
#define D2F(m) ((d2f_fn)t_proc_in(m, "CoDosDateTimeToFileTime"))
#define F2D(m) ((f2d_fn)t_proc_in(m, "CoFileTimeToDosDateTime"))
#define NOW(m) ((now_fn)t_proc_in(m, "CoFileTimeNow"))

#define U64(ft) (((ULONGLONG)(ft).dwHighDateTime << 32) | (ft).dwLowDateTime)
static void from64(FILETIME *ft, ULONGLONG v) { ft->dwLowDateTime = (DWORD)v; ft->dwHighDateTime = (DWORD)(v >> 32); }
#define DOSDATE(y, mo, d) ((WORD)((((y) - 1980) << 9) | ((mo) << 5) | (d)))
#define DOSTIME(h, mi, s) ((WORD)(((h) << 11) | ((mi) << 5) | ((s) / 2)))
#define EPOCH1980 119600064000000000ull       /* 1980-01-01 00:00:00 UTC as a FILETIME */
#define SEC100NS 10000000ull

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int dim(int y, int m) { static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31}; return m == 2 && is_leap(y) ? 29 : d[m - 1]; }
static ULONGLONG st_to_u64(int y, int mo, int d, int h, int mi, int s) {
    SYSTEMTIME st = {0}; FILETIME ft;
    st.wYear = y; st.wMonth = mo; st.wDay = d; st.wHour = h; st.wMinute = mi; st.wSecond = s;
    if (!SystemTimeToFileTime(&st, &ft)) return 0;
    return U64(ft);
}

/* ---- CoDosDateTimeToFileTime ---- */
TEST(time_dos2ft_epoch) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    FILETIME ft = {0};
    CHECK(f(DOSDATE(1980, 1, 1), 0, &ft) != 0);
    CHECK(U64(ft) == EPOCH1980);
    CHECK(st_to_u64(1980, 1, 1, 0, 0, 0) == EPOCH1980);   /* sanity of the constant */
}
TEST(time_dos2ft_grid_matches_systemtime) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    static const int years[] = {1980, 1981, 1999, 2000, 2024, 2038, 2099, 2100, 2107};
    static const int times[][3] = {{0,0,0},{23,59,58},{12,30,30},{1,2,4},{13,45,30}};
    for (size_t yi = 0; yi < sizeof years / sizeof *years; yi++)
        for (int mo = 1; mo <= 12; mo++)
            for (int di = 0; di < 2; di++) for (int ti = 0; ti < 5; ti++) {
                int y = years[yi], d = di ? dim(y, mo) : 1;
                FILETIME ft = {0};
                BOOL r = f(DOSDATE(y, mo, d), DOSTIME(times[ti][0], times[ti][1], times[ti][2]), &ft);
                ULONGLONG want = st_to_u64(y, mo, d, times[ti][0], times[ti][1], times[ti][2]);
                if (!r || U64(ft) != want) CHECKF(0, "%04d-%02d-%02d %02d:%02d:%02d r=%d got=%llx want=%llx", y, mo, d, times[ti][0], times[ti][1], times[ti][2], r, (unsigned long long)U64(ft), (unsigned long long)want);
            }
    CHECK(1);
}
TEST(time_dos2ft_no_timezone_shift) {
    /* the DOS value is interpreted as-is (no local-time conversion) */
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    FILETIME ft;
    CHECK(f(DOSDATE(2000, 6, 15), DOSTIME(12, 0, 0), &ft));
    CHECK(U64(ft) == st_to_u64(2000, 6, 15, 12, 0, 0));
}
TEST(time_dos2ft_max_date) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    FILETIME ft;
    CHECK(f(0xFF9F, 0xBF7D, &ft));                       /* 2107-12-31 23:59:58 */
    CHECK(U64(ft) == st_to_u64(2107, 12, 31, 23, 59, 58));
}
TEST(time_dos2ft_invalid_dates) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    /* explicit raw encodings: day 0, month 0, month 13-15, day 32 does not exist (5 bits => max 31) */
    static const WORD dates[] = {
        0x0000, 0x0020 /*m1 d0*/, 0x0001 /*m0 d1*/, (WORD)((1 << 9) | (13 << 5) | 1), (WORD)((1 << 9) | (14 << 5) | 1), (WORD)((1 << 9) | (15 << 5) | 1),
        DOSDATE(1980, 2, 30), DOSDATE(1980, 2, 31), DOSDATE(1981, 2, 29), DOSDATE(1983, 2, 29), DOSDATE(2100, 2, 29),
        DOSDATE(1980, 4, 31), DOSDATE(1980, 6, 31), DOSDATE(1980, 9, 31), DOSDATE(1980, 11, 31),
    };
    for (size_t i = 0; i < sizeof dates / sizeof *dates; i++) {
        FILETIME ft; ft.dwLowDateTime = ft.dwHighDateTime = 0xCCCCCCCC;
        BOOL r = f(dates[i], DOSTIME(0, 0, 0), &ft);
        CHECKF(r == 0, "date 0x%04x accepted", dates[i]);
        CHECKF(ft.dwLowDateTime == 0xCCCCCCCC && ft.dwHighDateTime == 0xCCCCCCCC, "output written on failure for 0x%04x", dates[i]);
    }
}
TEST(time_dos2ft_leap_days_valid) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    static const int leap[] = {1980, 1984, 2000, 2004, 2096, 2104};
    for (size_t i = 0; i < sizeof leap / sizeof *leap; i++) { FILETIME ft; CHECKF(f(DOSDATE(leap[i], 2, 29), 0, &ft), "%d-02-29 rejected", leap[i]); }
}
TEST(time_dos2ft_invalid_times) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    static const WORD times[] = {
        (WORD)(24 << 11), (WORD)(25 << 11), (WORD)(31 << 11),            /* hour >= 24 */
        (WORD)(60 << 5), (WORD)(61 << 5), (WORD)(63 << 5),              /* minute >= 60 */
        30, 31,                                                         /* seconds field 30,31 = 60,62 s */
        0xFFFF, 0xC000 | 0x7FF, (WORD)((24 << 11) | (59 << 5) | 29),
    };
    for (size_t i = 0; i < sizeof times / sizeof *times; i++) {
        FILETIME ft; ft.dwLowDateTime = ft.dwHighDateTime = 0xCCCCCCCC;
        BOOL r = f(DOSDATE(2000, 1, 1), times[i], &ft);
        CHECKF(r == 0, "time 0x%04x accepted", times[i]);
        CHECK(ft.dwLowDateTime == 0xCCCCCCCC);
    }
}
TEST(time_dos2ft_all_times_sweep) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    int bad = 0;
    for (unsigned t = 0; t < 65536; t++) {
        unsigned h = t >> 11, mi = (t >> 5) & 63, s2 = t & 31;
        int valid = h < 24 && mi < 60 && s2 < 30;
        FILETIME ft = {0};
        BOOL r = f(DOSDATE(2001, 3, 4), (WORD)t, &ft);
        if ((r != 0) != valid) bad++;
        else if (valid && U64(ft) != st_to_u64(2001, 3, 4, h, mi, s2 * 2)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(time_dos2ft_all_dates_sweep) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    int bad = 0;
    for (int y = 1980; y <= 2107; y++) for (int mo = 0; mo <= 15; mo++) for (int d = 0; d <= 31; d++) {
        int valid = mo >= 1 && mo <= 12 && d >= 1 && d <= dim(y, mo);
        FILETIME ft = {0};
        BOOL r = f((WORD)(((y - 1980) << 9) | (mo << 5) | d), DOSTIME(10, 20, 30), &ft);
        if ((r != 0) != valid) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(time_dos2ft_null_output) {
    d2f_fn f = D2F("ole32.dll"); REQUIRE(f);
    if (t_is_wine()) { t_skip("wine dereferences the NULL output"); return; }
    EXPECT_EQ(f(DOSDATE(1980, 1, 1), 0, NULL), 0);
    EXPECT_EQ(f(0, 0, NULL), 0);
}

/* ---- CoFileTimeToDosDateTime: rounds UP to the next 2-second step ---- */
TEST(time_ft2dos_exact_values) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    WORD d = 0, t = 0;
    FILETIME ft; from64(&ft, EPOCH1980);
    CHECK(f(&ft, &d, &t)); EXPECT_EQ(d, 0x0021); EXPECT_EQ(t, 0);
    from64(&ft, st_to_u64(2000, 6, 15, 12, 34, 56));
    CHECK(f(&ft, &d, &t)); EXPECT_EQ(d, DOSDATE(2000, 6, 15)); EXPECT_EQ(t, DOSTIME(12, 34, 56));
    from64(&ft, st_to_u64(2107, 12, 31, 23, 59, 58));
    CHECK(f(&ft, &d, &t)); EXPECT_EQ(d, 0xFF9F); EXPECT_EQ(t, 0xBF7D);
}
TEST(time_ft2dos_rounds_up_to_even_second) {
    /* decomp: (ft + 19999999) is converted, so the result is ceil(ft to 2 s), not floor */
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    ULONGLONG base = st_to_u64(2010, 5, 5, 10, 20, 30);
    static const struct { ULONGLONG delta; int add_s; } t[] = {
        {0, 0}, {1, 2}, {SEC100NS - 1, 2}, {SEC100NS, 2}, {SEC100NS + 1, 2}, {2 * SEC100NS - 1, 2}, {2 * SEC100NS, 2},
        {2 * SEC100NS + 1, 4}, {3 * SEC100NS, 4}, {4 * SEC100NS, 4}, {4 * SEC100NS + 1, 6},
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        FILETIME ft; from64(&ft, base + t[i].delta);
        WORD d = 0, tm = 0;
        BOOL r = f(&ft, &d, &tm);
        CHECKF(r, "delta %llu", (unsigned long long)t[i].delta);
        WORD wd = DOSDATE(2010, 5, 5), wt = DOSTIME(10, 20, 30 + t[i].add_s);
        CHECKF(d == wd && tm == wt, "delta %llu: got %04x/%04x want %04x/%04x", (unsigned long long)t[i].delta, d, tm, wd, wt);
    }
}
TEST(time_ft2dos_odd_second_input) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    for (int s = 0; s < 60; s++) {
        FILETIME ft; from64(&ft, st_to_u64(1999, 12, 31, 23, 58, s));
        WORD d, t;
        CHECK(f(&ft, &d, &t));
        int want_s = (s + 1) & ~1;             /* odd seconds round up */
        WORD wd = DOSDATE(1999, 12, 31), wt = DOSTIME(23, 58, want_s);
        if (want_s == 60) { wt = DOSTIME(23, 59, 0); }
        CHECKF(d == wd && t == wt, "sec %d: got %04x/%04x want %04x/%04x", s, d, t, wd, wt);
    }
}
TEST(time_ft2dos_midnight_rollover) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    FILETIME ft; from64(&ft, st_to_u64(2003, 12, 31, 23, 59, 59));
    WORD d, t;
    CHECK(f(&ft, &d, &t));
    EXPECT_EQ(d, DOSDATE(2004, 1, 1)); EXPECT_EQ(t, 0);     /* rounds into the next day/year */
    from64(&ft, st_to_u64(2004, 2, 28, 23, 59, 59));
    CHECK(f(&ft, &d, &t));
    EXPECT_EQ(d, DOSDATE(2004, 2, 29)); EXPECT_EQ(t, 0);    /* leap day */
    from64(&ft, st_to_u64(2003, 2, 28, 23, 59, 59));
    CHECK(f(&ft, &d, &t));
    EXPECT_EQ(d, DOSDATE(2003, 3, 1)); EXPECT_EQ(t, 0);
}
TEST(time_ft2dos_lower_range_limit) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    WORD d = 0xAAAA, t = 0xBBBB; FILETIME ft;
    from64(&ft, EPOCH1980 - 19999999);                   /* still rounds up to 1980-01-01 00:00:00 */
    CHECK(f(&ft, &d, &t)); EXPECT_EQ(d, 0x0021); EXPECT_EQ(t, 0);
    d = 0xAAAA; t = 0xBBBB;
    from64(&ft, EPOCH1980 - 20000000);                   /* rounds to 1979-12-31 23:59:59-ish: out of range */
    EXPECT_EQ(f(&ft, &d, &t), 0);
    CHECK(d == 0xAAAA && t == 0xBBBB);                   /* outputs untouched on failure */
    from64(&ft, st_to_u64(1979, 12, 31, 23, 59, 58));
    EXPECT_EQ(f(&ft, &d, &t), 0);
    from64(&ft, st_to_u64(1970, 1, 1, 0, 0, 0));
    EXPECT_EQ(f(&ft, &d, &t), 0);
    from64(&ft, st_to_u64(1601, 1, 1, 0, 0, 1));
    EXPECT_EQ(f(&ft, &d, &t), 0);
}
TEST(time_ft2dos_upper_range_limit) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    WORD d = 0xAAAA, t = 0xBBBB; FILETIME ft;
    ULONGLONG last = st_to_u64(2107, 12, 31, 23, 59, 58);
    from64(&ft, last);
    CHECK(f(&ft, &d, &t)); EXPECT_EQ(d, 0xFF9F); EXPECT_EQ(t, 0xBF7D);
    d = 0xAAAA; t = 0xBBBB;
    from64(&ft, last + 1);                               /* rounds up into year 2108 */
    EXPECT_EQ(f(&ft, &d, &t), 0);
    CHECK(d == 0xAAAA && t == 0xBBBB);
    from64(&ft, last + SEC100NS);
    EXPECT_EQ(f(&ft, &d, &t), 0);
    from64(&ft, st_to_u64(2108, 1, 1, 0, 0, 0));
    EXPECT_EQ(f(&ft, &d, &t), 0);
    from64(&ft, st_to_u64(3000, 1, 1, 0, 0, 0));
    EXPECT_EQ(f(&ft, &d, &t), 0);
}
TEST(time_ft2dos_negative_filetime_is_invalid_parameter) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    static const ULONGLONG v[] = { 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0xC000000000000000ull, 0xFFFFFFFF00000000ull };
    for (size_t i = 0; i < sizeof v / sizeof *v; i++) {
        FILETIME ft; from64(&ft, v[i]); WORD d = 1, t = 1;
        SET_ERR(0);
        EXPECT_EQ(f(&ft, &d, &t), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECK(d == 1 && t == 1);
    }
}
TEST(time_ft2dos_null_arguments) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    FILETIME ft; from64(&ft, EPOCH1980); WORD d = 0, t = 0;
    EXPECT_EQ(f(&ft, &d, &t), 1);
    if (t_is_wine()) { t_skip("wine dereferences NULL arguments"); return; }
    EXPECT_EQ(f(NULL, &d, &t), 0);
    EXPECT_EQ(f(&ft, NULL, &t), 0);
    EXPECT_EQ(f(&ft, &d, NULL), 0);
    EXPECT_EQ(f(NULL, NULL, NULL), 0);
    EXPECT_EQ(f(&ft, &d, &t), 1);
}
TEST(time_ft2dos_zero_filetime) {
    f2d_fn f = F2D("ole32.dll"); REQUIRE(f);
    FILETIME ft = {0}; WORD d, t;
    EXPECT_EQ(f(&ft, &d, &t), 0);
}
TEST(time_dos_roundtrip_dates) {
    d2f_fn a = D2F("ole32.dll"); f2d_fn b = F2D("ole32.dll"); REQUIRE(a && b);
    int bad = 0;
    for (int y = 1980; y <= 2107; y++) for (int mo = 1; mo <= 12; mo++) for (int d = 1; d <= dim(y, mo); d++) {
        FILETIME ft; WORD d2, t2;
        WORD dd = DOSDATE(y, mo, d), tt = DOSTIME(13, 45, 30);
        if (!a(dd, tt, &ft) || !b(&ft, &d2, &t2) || d2 != dd || t2 != tt) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(time_dos_roundtrip_times) {
    d2f_fn a = D2F("ole32.dll"); f2d_fn b = F2D("ole32.dll"); REQUIRE(a && b);
    int bad = 0;
    for (unsigned h = 0; h < 24; h++) for (unsigned mi = 0; mi < 60; mi++) for (unsigned s = 0; s < 60; s += 2) {
        FILETIME ft; WORD d2, t2;
        WORD tt = DOSTIME(h, mi, s);
        if (!a(DOSDATE(2020, 2, 29), tt, &ft) || !b(&ft, &d2, &t2) || d2 != DOSDATE(2020, 2, 29) || t2 != tt) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(time_ft_to_dos_to_ft_never_earlier) {
    /* rounding up means the round-tripped time is >= the input and less than 2 s later */
    d2f_fn a = D2F("ole32.dll"); f2d_fn b = F2D("ole32.dll"); REQUIRE(a && b);
    ULONGLONG base = st_to_u64(2015, 7, 7, 7, 7, 6);
    unsigned rng = 77; int bad = 0;
    for (int i = 0; i < 5000; i++) {
        rng = rng * 1664525u + 1013904223u;
        ULONGLONG v = base + (ULONGLONG)(rng % 4000000000u);
        FILETIME ft, ft2; WORD d, t; from64(&ft, v);
        if (!b(&ft, &d, &t) || !a(d, t, &ft2)) { bad++; continue; }
        ULONGLONG w = U64(ft2);
        if (w < v || w - v >= 2 * SEC100NS) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(time_dostime_combase_absent) {
    /* the DOS conversion helpers are ole32-only exports */
    t_note("combase CoDosDateTimeToFileTime export: %s", D2F("combase.dll") ? "present" : "absent");
    t_note("combase CoFileTimeToDosDateTime export: %s", F2D("combase.dll") ? "present" : "absent");
}

/* ---- CoFileTimeNow ---- */
TEST(time_cofiletimenow_null) {
    FOR_MODS(m) { if (!NOW(m)) { t_skip("missing"); return; } EXPECT_EQ(NOW(m)(NULL), CMG_E_INVALIDARG); }
}
TEST(time_cofiletimenow_ok_close_to_system_time) {
    FOR_MODS(m) {
        if (!NOW(m)) { t_skip("missing"); return; }
        FILETIME a, b, c; memset(&b, 0xCC, sizeof b);
        GetSystemTimeAsFileTime(&a);
        EXPECT_EQ(NOW(m)(&b), S_OK);
        GetSystemTimeAsFileTime(&c);
        CHECK(U64(b) + 10 * 10000ull >= U64(a));             /* within a few ms of the bracketing system times */
        CHECK(U64(b) <= U64(c) + 10 * 10000ull);
    }
}
TEST(time_cofiletimenow_millisecond_resolution) {
    FOR_MODS(m) {
        if (!NOW(m)) { t_skip("missing"); return; }
        int bad = 0;
        for (int i = 0; i < 200; i++) { FILETIME ft; NOW(m)(&ft); if (U64(ft) % 10000 != 0) bad++; }   /* built from SYSTEMTIME: whole milliseconds */
        EXPECT_EQ(bad, 0);
    }
}
TEST(time_cofiletimenow_monotonic) {
    FOR_MODS(m) {
        if (!NOW(m)) { t_skip("missing"); return; }
        ULONGLONG prev = 0; int bad = 0;
        for (int i = 0; i < 5000; i++) { FILETIME ft; if (NOW(m)(&ft) != S_OK) { bad++; break; } if (U64(ft) < prev) bad++; prev = U64(ft); }
        EXPECT_EQ(bad, 0);
    }
}
TEST(time_cofiletimenow_advances) {
    FOR_MODS(m) {
        if (!NOW(m)) { t_skip("missing"); return; }
        FILETIME a, b; NOW(m)(&a); Sleep(60); NOW(m)(&b);
        CHECK(U64(b) > U64(a));
        CHECK(U64(b) - U64(a) >= 40 * 10000ull);             /* at least ~40ms passed (sleep was 60) */
        CHECK(U64(b) - U64(a) < 5000 * 10000ull);
    }
}
TEST(time_cofiletimenow_is_utc_and_sane) {
    FOR_MODS(m) {
        if (!NOW(m)) { t_skip("missing"); return; }
        FILETIME ft; NOW(m)(&ft);
        SYSTEMTIME st, sys; FileTimeToSystemTime(&ft, &st); GetSystemTime(&sys);
        CHECK(st.wYear >= 2024 && st.wYear < 2200);
        int dd = (int)st.wDay - (int)sys.wDay;
        CHECK(dd == 0 || dd == 1 || dd == -1 || dd > 26 || dd < -26);        /* same UTC day (allow midnight race) */
        CHECK(st.wYear == sys.wYear || st.wYear + 1 == sys.wYear || st.wYear == sys.wYear + 1);
    }
}
TEST(time_cofiletimenow_feeds_dos_conversion) {
    d2f_fn a = D2F("ole32.dll"); f2d_fn b = F2D("ole32.dll"); now_fn n = NOW("ole32.dll");
    REQUIRE(a && b && n);
    FILETIME ft, ft2; WORD d, t;
    EXPECT_EQ(n(&ft), S_OK);
    CHECK(b(&ft, &d, &t));
    CHECK(a(d, t, &ft2));
    CHECK(U64(ft2) >= U64(ft) && U64(ft2) - U64(ft) < 2 * SEC100NS);
}
