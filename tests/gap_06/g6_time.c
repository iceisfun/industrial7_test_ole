/* gap_06: CoCreateGuid, CoFileTimeNow, CoDosDateTimeToFileTime, CoFileTimeToDosDateTime, StringFromIID.
 * Decomp facts (Win10):
 *  CoCreateGuid(NULL) = E_INVALIDARG; else UuidCreate (RPC_S_UUID_LOCAL_ONLY counts as success) -> S_OK.
 *  CoFileTimeNow(NULL) = E_INVALIDARG; else GetSystemTime + SystemTimeToFileTime => millisecond resolution.
 *  CoDosDateTimeToFileTime / CoFileTimeToDosDateTime: BOOL; NULL pointer => FALSE; otherwise kernel32
 *    DosDateTimeToFileTime / FileTimeToDosDateTime (no time-zone conversion; FILETIME->DOS rounds UP to 2 s).
 *  StringFromIID(NULL,..)/(..,NULL) = E_INVALIDARG; sets *out=NULL, then CoTaskMem copy of StringFromGUID2 text. */
#include "gap6.h"
#include <objbase.h>

#define GE_(a,b) ((a) >= (b))

/* ------------------------------------------------------------ CoCreateGuid */
TEST(gap6_coguid_null) {
    REQUIRE(U(CoCreateGuid));
    if (t_is_wine()) { t_skip("wine does not validate NULL here (access violation)"); return; }
    EQ(U(CoCreateGuid)(NULL), E_INVALIDARG);
}
TEST(gap6_coguid_writes_exactly_16_bytes) {
    REQUIRE(U(CoCreateGuid));
    struct { BYTE pre[8]; GUID g; BYTE post[8]; } b;
    memset(&b, 0xA5, sizeof b);
    EQ(U(CoCreateGuid)(&b.g), S_OK);
    for (int i = 0; i < 8; i++) { CHECK(b.pre[i] == 0xA5); CHECK(b.post[i] == 0xA5); }
    static const GUID nil;
    CHECK(memcmp(&b.g, &nil, sizeof nil) != 0);
}
TEST(gap6_coguid_rfc4122_variant) {
    REQUIRE(U(CoCreateGuid));
    int bad_var = 0, bad_ver = 0;
    for (int i = 0; i < 200; i++) {
        GUID g; memset(&g, 0, sizeof g);
        if (U(CoCreateGuid)(&g) != S_OK) { bad_var++; continue; }
        if ((g.Data4[0] & 0xC0) != 0x80) bad_var++;   /* RFC 4122 variant 10xx */
        if ((g.Data3 >> 12) != 4) bad_ver++;          /* version 4 (random) on Windows 10 */
    }
    EQ(bad_var, 0);
    EQ(bad_ver, 0);
}
static int cmp_guid(const void *a, const void *b) { return memcmp(a, b, sizeof(GUID)); }
TEST(gap6_coguid_unique_10000) {
    REQUIRE(U(CoCreateGuid));
    enum { N = 10000 };
    GUID *g = HeapAlloc(GetProcessHeap(), 0, N * sizeof(GUID));
    CHECK(g != NULL); if (!g) return;
    for (int i = 0; i < N; i++) EQ(U(CoCreateGuid)(&g[i]), S_OK);
    qsort(g, N, sizeof(GUID), cmp_guid);
    int dup = 0;
    for (int i = 1; i < N; i++) if (!memcmp(&g[i - 1], &g[i], sizeof(GUID))) dup++;
    EQ(dup, 0);
    HeapFree(GetProcessHeap(), 0, g);
}
static DWORD WINAPI guid_thread(void *p) {
    GUID *out = p;
    for (int i = 0; i < 500; i++) if (U(CoCreateGuid)(&out[i]) != S_OK) return 1;
    return 0;
}
TEST(gap6_coguid_threads_unique_no_com_init) {
    REQUIRE(U(CoCreateGuid));
    enum { T = 4, N = 500 };
    static GUID all[T * N];
    HANDLE h[T];
    for (int t = 0; t < T; t++) h[t] = CreateThread(NULL, 0, guid_thread, &all[t * N], 0, NULL);
    for (int t = 0; t < T; t++) { CHECK(h[t] != NULL); if (h[t]) { EQ(WaitForSingleObject(h[t], 10000), WAIT_OBJECT_0); DWORD ec = 9; GetExitCodeThread(h[t], &ec); EQ(ec, 0); CloseHandle(h[t]); } }
    qsort(all, T * N, sizeof(GUID), cmp_guid);
    int dup = 0;
    for (int i = 1; i < T * N; i++) if (!memcmp(&all[i - 1], &all[i], sizeof(GUID))) dup++;
    EQ(dup, 0);
}
TEST(gap6_coguid_matches_uuidcreate_shape) {
    REQUIRE(U(CoCreateGuid)); REQUIRE(U(UuidCreate));
    GUID a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    RPC_STATUS rs = U(UuidCreate)(&b);
    CHECK(rs == RPC_S_OK || rs == RPC_S_UUID_LOCAL_ONLY);
    EQ(U(CoCreateGuid)(&a), S_OK);
    CHECK(a.Data3 >> 12 == b.Data3 >> 12);
    CHECK((a.Data4[0] & 0xC0) == (b.Data4[0] & 0xC0));
}

/* ------------------------------------------------------------ CoFileTimeNow */
static ULONGLONG ull(FILETIME f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }
TEST(gap6_coftnow_null) {
    REQUIRE(U(CoFileTimeNow));
    if (t_is_wine()) { t_skip("wine does not validate NULL here (access violation)"); return; }
    EQ(U(CoFileTimeNow)(NULL), E_INVALIDARG);
}
TEST(gap6_coftnow_tracks_system_time) {
    REQUIRE(U(CoFileTimeNow));
    FILETIME b, a, n; memset(&n, 0xA5, sizeof n);
    GetSystemTimeAsFileTime(&b);
    EQ(U(CoFileTimeNow)(&n), S_OK);
    GetSystemTimeAsFileTime(&a);
    /* GetSystemTime has ms granularity and may be truncated: allow 100 ms below, 100 ms above */
    CHECK(ull(n) + 1000000ULL >= ull(b));
    CHECK(ull(n) <= ull(a) + 1000000ULL);
}
TEST(gap6_coftnow_millisecond_resolution) {
    REQUIRE(U(CoFileTimeNow));
    int bad = 0;
    for (int i = 0; i < 200; i++) { FILETIME n; if (U(CoFileTimeNow)(&n) != S_OK || ull(n) % 10000 != 0) bad++; }
    EQ(bad, 0);   /* built from a SYSTEMTIME: whole milliseconds only */
}
TEST(gap6_coftnow_non_decreasing) {
    REQUIRE(U(CoFileTimeNow));
    FILETIME p; EQ(U(CoFileTimeNow)(&p), S_OK);
    int back = 0;
    for (int i = 0; i < 1000; i++) { FILETIME n; U(CoFileTimeNow)(&n); if (ull(n) + 500000ULL < ull(p)) back++; p = n; }
    EQ(back, 0);
}
TEST(gap6_coftnow_in_sane_range) {
    REQUIRE(U(CoFileTimeNow));
    FILETIME n; U(CoFileTimeNow)(&n);
    SYSTEMTIME st; FileTimeToSystemTime(&n, &st);
    CHECK(st.wYear >= 2020 && st.wYear < 2200);
}

/* ------------------------------------------------------ DOS date/time */
#define dd(y, m, d) ((WORD)((((y) - 1980) << 9) | ((m) << 5) | (d)))
#define dt(h, mi, s2) ((WORD)(((h) << 11) | ((mi) << 5) | (s2)))   /* s2 = seconds / 2 */
static ULONGLONG ft_of(int y, int mo, int d, int h, int mi, int s) {
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wYear = y; st.wMonth = mo; st.wDay = d; st.wHour = h; st.wMinute = mi; st.wSecond = s;
    FILETIME f; if (!SystemTimeToFileTime(&st, &f)) return 0;
    return ull(f);
}
TEST(gap6_dos2ft_null_out) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    if (t_is_wine()) { t_skip("wine does not validate NULL here (access violation)"); return; }
    EQ(U(CoDosDateTimeToFileTime)(dd(2000, 1, 1), 0, NULL), FALSE);
}
TEST(gap6_dos2ft_valid_table) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    static const struct { int y, mo, d, h, mi, s2; } t[] = {
        {1980,1,1,0,0,0}, {1980,1,1,0,0,1}, {1999,12,31,23,59,29}, {2000,2,29,12,30,15}, {2024,2,29,0,0,0},
        {2038,1,19,3,14,7}, {2107,12,31,23,59,29}, {1996,6,15,6,7,8}, {2010,7,4,13,0,0}, {1980,12,31,23,59,29} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        FILETIME f; memset(&f, 0, sizeof f);
        BOOL ok = U(CoDosDateTimeToFileTime)(dd(t[i].y, t[i].mo, t[i].d), dt(t[i].h, t[i].mi, t[i].s2), &f);
        CHECKF(ok == TRUE, "case %u", i);
        CHECKF(ull(f) == ft_of(t[i].y, t[i].mo, t[i].d, t[i].h, t[i].mi, t[i].s2 * 2), "case %u got %llx", i, ull(f));
    }
}
TEST(gap6_dos2ft_epoch_constant) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    FILETIME f; memset(&f, 0, sizeof f);
    CHECK(U(CoDosDateTimeToFileTime)(0x0021, 0, &f));   /* 1980-01-01 00:00:00 */
    EQ(ull(f), 119600064000000000ULL);
}
TEST(gap6_dos2ft_no_timezone_shift) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    /* the conversion is a plain field->time conversion: result equals the same wall clock read as UTC */
    FILETIME f; U(CoDosDateTimeToFileTime)(dd(2020, 7, 1), dt(12, 0, 0), &f);
    SYSTEMTIME st; FileTimeToSystemTime(&f, &st);
    CHECK(st.wYear == 2020 && st.wMonth == 7 && st.wDay == 1 && st.wHour == 12 && st.wMinute == 0 && st.wSecond == 0);
}
TEST(gap6_dos2ft_invalid_fields_fail) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    static const struct { WORD d, t; const char *why; } t[] = {
        { 0x0000, 0, "date 0 (month 0 day 0)" },
        { 0x0020 | 0, 0, "month 1 day 0" },
        { 0x0001, 0, "month 0 day 1" },
        { (WORD)((1 << 9) | (13 << 5) | 1), 0, "month 13" },
        { (WORD)((1 << 9) | (14 << 5) | 1), 0, "month 14" },
        { (WORD)((1 << 9) | (15 << 5) | 1), 0, "month 15" },
        { (WORD)((20 << 9) | (2 << 5) | 30), 0, "Feb 30" },
        { (WORD)((21 << 9) | (2 << 5) | 29), 0, "Feb 29 non-leap 2001" },
        { (WORD)((20 << 9) | (4 << 5) | 31), 0, "Apr 31" },
        { (WORD)((20 << 9) | (6 << 5) | 31), 0, "Jun 31" },
        { (WORD)((20 << 9) | (9 << 5) | 31), 0, "Sep 31" },
        { (WORD)((20 << 9) | (11 << 5) | 31), 0, "Nov 31" },
        { dd(2000,1,1), dt(24, 0, 0), "hour 24" },
        { dd(2000,1,1), dt(31, 0, 0), "hour 31" },
        { dd(2000,1,1), dt(0, 60, 0), "minute 60" },
        { dd(2000,1,1), dt(0, 63, 0), "minute 63" },
        { dd(2000,1,1), dt(0, 0, 30), "seconds 60" },
        { dd(2000,1,1), dt(0, 0, 31), "seconds 62" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        FILETIME f; f.dwLowDateTime = 0x11111111; f.dwHighDateTime = 0x22222222;
        BOOL ok = U(CoDosDateTimeToFileTime)(t[i].d, t[i].t, &f);
        CHECKF(ok == FALSE, "%s: unexpectedly succeeded", t[i].why);
        CHECKF(f.dwLowDateTime == 0x11111111 && f.dwHighDateTime == 0x22222222, "%s: output modified on failure", t[i].why);
    }
}
TEST(gap6_dos2ft_failure_last_error) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    FILETIME f; SET_ERR(0);
    BOOL ok = U(CoDosDateTimeToFileTime)(0, 0, &f);
    CHECK(!ok);
    DWORD e = GetLastError();
    if (e != ERROR_INVALID_PARAMETER) t_note("last error after invalid DOS date = %lu (expected 87 from STATUS_INVALID_PARAMETER)", e);
}
TEST(gap6_dos2ft_leap_year_boundaries) {
    REQUIRE(U(CoDosDateTimeToFileTime));
    static const struct { int y, m, d; BOOL ok; } t[] = {
        {2000,2,29,1},{1900+80,2,29,1},{2004,2,29,1},{2100,2,29,0},{2100,2,28,1},{2096,2,29,1},{2001,2,29,0},{2023,2,29,0},{2107,2,29,0} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        FILETIME f;
        BOOL ok = U(CoDosDateTimeToFileTime)(dd(t[i].y, t[i].m, t[i].d), 0, &f);
        CHECKF(ok == t[i].ok, "%d-%d-%d", t[i].y, t[i].m, t[i].d);
    }
}
TEST(gap6_ft2dos_null_args) {
    REQUIRE(U(CoFileTimeToDosDateTime));
    if (t_is_wine()) { t_skip("wine does not validate NULL here (access violation)"); return; }
    FILETIME f; U(CoFileTimeNow)(&f);
    WORD d = 0xAAAA, t = 0xBBBB;
    EQ(U(CoFileTimeToDosDateTime)(NULL, &d, &t), FALSE);
    EQ(U(CoFileTimeToDosDateTime)(&f, NULL, &t), FALSE);
    EQ(U(CoFileTimeToDosDateTime)(&f, &d, NULL), FALSE);
    EQ(U(CoFileTimeToDosDateTime)(NULL, NULL, NULL), FALSE);
    CHECK(d == 0xAAAA && t == 0xBBBB);
}
TEST(gap6_ft2dos_exact_values) {
    REQUIRE(U(CoFileTimeToDosDateTime));
    static const struct { int y, mo, d, h, mi, s; } t[] = {
        {1980,1,1,0,0,0}, {1980,1,1,0,0,2}, {2000,2,29,12,30,14}, {2107,12,31,23,59,58}, {1999,12,31,23,59,0}, {2025,6,15,8,9,10} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ULONGLONG v = ft_of(t[i].y, t[i].mo, t[i].d, t[i].h, t[i].mi, t[i].s);
        FILETIME f; f.dwLowDateTime = (DWORD)v; f.dwHighDateTime = (DWORD)(v >> 32);
        WORD d = 0, tm = 0;
        BOOL ok = U(CoFileTimeToDosDateTime)(&f, &d, &tm);
        CHECKF(ok, "case %u", i);
        CHECKF(d == dd(t[i].y, t[i].mo, t[i].d), "case %u date 0x%x", i, d);
        CHECKF(tm == dt(t[i].h, t[i].mi, t[i].s / 2), "case %u time 0x%x", i, tm);
    }
}
TEST(gap6_ft2dos_rounds_up_to_two_seconds) {
    REQUIRE(U(CoFileTimeToDosDateTime));
    const ULONGLONG base = 119600064000000000ULL;   /* 1980-01-01 00:00:00 */
    static const struct { ULONGLONG delta; WORD sec2; } t[] = {
        {0,0}, {1,1}, {9999999,1}, {10000000,1}, {19999999,1}, {20000000,1}, {20000001,2}, {30000000,2}, {40000000,2}, {40000001,3},
        {590000000,30}, {599999999,30} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ULONGLONG v = base + t[i].delta;
        FILETIME f; f.dwLowDateTime = (DWORD)v; f.dwHighDateTime = (DWORD)(v >> 32);
        WORD d = 0, tm = 0;
        BOOL ok = U(CoFileTimeToDosDateTime)(&f, &d, &tm);
        CHECKF(ok, "delta %llu", t[i].delta);
        /* 59.x s rounds up into the next minute: field arithmetic below only valid for sec2 <= 29 */
        if (t[i].sec2 <= 29) CHECKF(tm == t[i].sec2, "delta %llu got 0x%x want %u", t[i].delta, tm, t[i].sec2);
        else CHECKF(tm == (WORD)(1 << 5) && d == 0x21, "delta %llu rolled into next minute: d=0x%x t=0x%x", t[i].delta, d, tm);
    }
}
TEST(gap6_ft2dos_range_limits) {
    REQUIRE(U(CoFileTimeToDosDateTime));
    WORD d, tm;
    ULONGLONG lo = ft_of(1980, 1, 1, 0, 0, 0);
    ULONGLONG hi = ft_of(2107, 12, 31, 23, 59, 58);
    FILETIME f;
    f.dwLowDateTime = (DWORD)(lo - 20000000); f.dwHighDateTime = (DWORD)((lo - 20000000) >> 32);
    EQ(U(CoFileTimeToDosDateTime)(&f, &d, &tm), FALSE);   /* 1979-12-31 23:59:58 */
    f.dwLowDateTime = (DWORD)(lo - 1); f.dwHighDateTime = (DWORD)((lo - 1) >> 32);
    t_note("1979-12-31 23:59:59.9999999 -> %d (rounds up into 1980?)", (int)U(CoFileTimeToDosDateTime)(&f, &d, &tm));
    f.dwLowDateTime = (DWORD)hi; f.dwHighDateTime = (DWORD)(hi >> 32);
    EQ(U(CoFileTimeToDosDateTime)(&f, &d, &tm), TRUE);
    ULONGLONG over = ft_of(2108, 1, 1, 0, 0, 0);
    f.dwLowDateTime = (DWORD)over; f.dwHighDateTime = (DWORD)(over >> 32);
    EQ(U(CoFileTimeToDosDateTime)(&f, &d, &tm), FALSE);   /* year 2108 does not fit 7 bits */
}
TEST(gap6_ft2dos_negative_filetime_invalid_parameter) {
    REQUIRE(U(CoFileTimeToDosDateTime));
    FILETIME f; f.dwHighDateTime = 0x80000000u; f.dwLowDateTime = 0;
    WORD d = 0x1111, tm = 0x2222; SET_ERR(0);
    EQ(U(CoFileTimeToDosDateTime)(&f, &d, &tm), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(d == 0x1111 && tm == 0x2222);
}
TEST(gap6_dos_roundtrip_even_seconds) {
    REQUIRE(U(CoFileTimeToDosDateTime)); REQUIRE(U(CoDosDateTimeToFileTime));
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        WORD d = dd(1980 + (i * 7) % 128, 1 + (i * 5) % 12, 1 + (i * 3) % 28), t = dt((i * 11) % 24, (i * 13) % 60, (i * 17) % 30);
        FILETIME f; WORD d2 = 0, t2 = 0;
        if (!U(CoDosDateTimeToFileTime)(d, t, &f)) { bad++; continue; }
        if (!U(CoFileTimeToDosDateTime)(&f, &d2, &t2)) { bad++; continue; }
        if (d != d2 || t != t2) bad++;
    }
    EQ(bad, 0);
}
TEST(gap6_dos_matches_kernel32) {
    REQUIRE(U(CoFileTimeToDosDateTime)); REQUIRE(U(CoDosDateTimeToFileTime));
    int bad = 0;
    for (int i = 0; i < 500; i++) {
        WORD d = dd(1980 + i % 100, 1 + i % 12, 1 + i % 28), t = dt(i % 24, (i * 7) % 60, (i * 3) % 30);
        FILETIME a, b;
        BOOL r1 = U(CoDosDateTimeToFileTime)(d, t, &a), r2 = DosDateTimeToFileTime(d, t, &b);
        if (r1 != r2 || (r1 && ull(a) != ull(b))) bad++;
        WORD d1 = 0, t1 = 0, d2 = 0, t2 = 0;
        r1 = U(CoFileTimeToDosDateTime)(&a, &d1, &t1); r2 = FileTimeToDosDateTime(&a, &d2, &t2);
        if (r1 != r2 || d1 != d2 || t1 != t2) bad++;
    }
    EQ(bad, 0);
}

/* ------------------------------------------------------------ StringFromIID */
static const GUID G_NIL;
static const GUID G1 = { 0x0000000Au, 0xBCDE, 0xF012, { 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x12 } };
TEST(gap6_strfromiid_null_args) {
    REQUIRE(U(StringFromIID));
    if (t_is_wine()) { t_skip("wine does not validate NULL here (access violation)"); return; }
    LPOLESTR s = (LPOLESTR)(ULONG_PTR)0x1234;
    EQ(U(StringFromIID)(NULL, &s), E_INVALIDARG);
    CHECK(s == (LPOLESTR)(ULONG_PTR)0x1234);          /* untouched when the IID is NULL */
    EQ(U(StringFromIID)(&G1, NULL), E_INVALIDARG);
    EQ(U(StringFromIID)(NULL, NULL), E_INVALIDARG);
}
TEST(gap6_strfromiid_format_upper_case_braced) {
    REQUIRE(U(StringFromIID));
    LPOLESTR s = NULL;
    EQ(U(StringFromIID)(&G1, &s), S_OK);
    CHECK(s != NULL);
    if (s) {
        EXPECT_WSTR(s, L"{0000000A-BCDE-F012-3456-789ABCDEF012}");
        EQ(lstrlenW(s), 38);
        U(CoTaskMemFree)(s);
    }
}
TEST(gap6_strfromiid_iid_null) {
    REQUIRE(U(StringFromIID));
    LPOLESTR s = NULL;
    EQ(U(StringFromIID)(&G_NIL, &s), S_OK);
    if (s) { EXPECT_WSTR(s, L"{00000000-0000-0000-0000-000000000000}"); U(CoTaskMemFree)(s); }
}
TEST(gap6_strfromiid_matches_stringfromguid2_and_roundtrips) {
    REQUIRE(U(StringFromIID)); REQUIRE(U(StringFromGUID2)); REQUIRE(U(IIDFromString)); REQUIRE(U(CoCreateGuid));
    for (int i = 0; i < 100; i++) {
        GUID g; U(CoCreateGuid)(&g);
        WCHAR a[64]; LPOLESTR b = NULL; GUID back;
        EQ(U(StringFromGUID2)(&g, a, 64), 39);
        EQ(U(StringFromIID)(&g, &b), S_OK);
        if (b) {
            CHECK(!lstrcmpW(a, b));
            EQ(U(IIDFromString)(b, &back), S_OK);
            CHECK(!memcmp(&g, &back, sizeof g));
            U(CoTaskMemFree)(b);
        }
    }
}
TEST(gap6_strfromiid_alloc_is_task_memory) {
    REQUIRE(U(StringFromIID)); REQUIRE(U(CoGetMalloc));
    LPOLESTR s = NULL;
    EQ(U(StringFromIID)(&G1, &s), S_OK);
    IMalloc *m = NULL;
    if (s && U(CoGetMalloc)(1, &m) == S_OK) {
        EQ(m->lpVtbl->DidAlloc(m, s), 1);
        SIZE_T sz = m->lpVtbl->GetSize(m, s);
        if (sz != 39 * sizeof(WCHAR)) t_note("GetSize of StringFromIID buffer = %llu (expected 78)", (unsigned long long)sz);
        m->lpVtbl->Release(m);
    }
    if (s) U(CoTaskMemFree)(s);
}
TEST(gap6_strfromiid_distinct_allocations) {
    REQUIRE(U(StringFromIID));
    LPOLESTR a = NULL, b = NULL;
    U(StringFromIID)(&G1, &a); U(StringFromIID)(&G1, &b);
    CHECK(a != NULL && b != NULL && a != b);
    if (a) U(CoTaskMemFree)(a);
    if (b) U(CoTaskMemFree)(b);
}
