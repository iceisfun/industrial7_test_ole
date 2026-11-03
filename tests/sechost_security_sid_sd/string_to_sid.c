#include "ssh.h"
/* ConvertStringSidToSidW/A.
 * Decomp: NULL string or NULL out -> ERROR_INVALID_PARAMETER; parse failures STATUS_INVALID_SID -> 1337;
 * trailing characters after a full "S-..." parse -> 0x539 (1337); 2-char alias must match whole string. */

static int parse(const char *s, PSID *out)
{
    WCHAR w[512]; widen(s, w, 512); *out = NULL;
    return UIN(SH, ConvertStringSidToSidW)(w, out);
}
static void expect_sid(const char *s, unsigned long long auth, int n, ...)
{
    PSID sid; SIDBUF b; PISID e; va_list ap; int i;
    BOOL r;
    SET_ERR(0x4242);
    r = parse(s, &sid);
    CHECKF(r, "parse \"%s\" failed err %lu", s, GetLastError());
    if (!r) return;
    EXPECT_EQ(GetLastError(), 0);
    CHECKF(IsValidSid(sid), "\"%s\" invalid sid", s);
    e = (PISID)mk_sid(&b, auth, 0);
    e->SubAuthorityCount = (BYTE)n;
    va_start(ap, n);
    for (i = 0; i < n; i++) e->SubAuthority[i] = va_arg(ap, DWORD);
    va_end(ap);
    CHECKF(EqualSid(sid, &b), "\"%s\": sid differs from expected", s);
    CHECKF(GetLengthSid(sid) == 8 + 4 * (DWORD)n, "\"%s\": length %lu", s, GetLengthSid(sid));
    CHECK(LocalFree(sid) == NULL);
}
static void expect_bad(const char *s)
{
    PSID sid = (PSID)0x1; BOOL r;
    SET_ERR(0);
    r = parse(s, &sid);
    CHECKF(!r, "\"%s\" unexpectedly parsed", s);
    if (r) { LocalFree(sid); return; }
    CHECKF(GetLastError() == ERR_INVALID_SID, "\"%s\": GetLastError %lu want 1337", s, GetLastError());
}

TEST(strsid_w_numeric_basic)
{
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    expect_sid("S-1-5-18", 5, 1, 18);
    expect_sid("S-1-1-0", 1, 1, 0);
    expect_sid("S-1-0-0", 0, 1, 0);
    expect_sid("S-1-5-32-544", 5, 2, 32, 544);
    expect_sid("S-1-5-21-1-2-3-500", 5, 5, 21, 1, 2, 3, 500);
    expect_sid("S-1-16-12288", 16, 1, 12288);
    expect_sid("S-1-15-2-1", 15, 2, 2, 1);
    expect_sid("S-1-5-4294967295", 5, 1, 4294967295u);
    expect_sid("S-1-5-0-0-0", 5, 3, 0, 0, 0);
}

TEST(strsid_w_lowercase_prefix)
{
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    expect_sid("s-1-5-18", 5, 1, 18);
    expect_sid("s-1-5-32-544", 5, 2, 32, 544);
}

TEST(strsid_w_authority_forms)
{
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    expect_sid("S-1-4294967295-1", 0xFFFFFFFFull, 1, 1);
    expect_sid("S-1-4294967296-1", 0x100000000ull, 1, 1);
    expect_sid("S-1-0x5-18", 5, 1, 18);
    expect_sid("S-1-0X5-18", 5, 1, 18);
    expect_sid("S-1-0x10-1", 0x10, 1, 1);
    expect_sid("S-1-0x000100000000-1", 0x100000000ull, 1, 1);
    expect_sid("S-1-0xFFFFFFFFFFFF-1", 0xFFFFFFFFFFFFull, 1, 1);
    expect_sid("S-1-0xffffffffffff-1", 0xFFFFFFFFFFFFull, 1, 1);
    expect_sid("S-1-0x123456789abc-1-2", 0x123456789abcull, 2, 1, 2);
}

TEST(strsid_w_authority_out_of_range)
{
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    expect_bad("S-1-0x1000000000000-1");       /* needs 7 bytes */
    expect_bad("S-1-281474976710656-1");       /* 2^48 */
}

TEST(strsid_w_revision_forms)
{
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    expect_bad("S-256-5-18");                  /* revision must be < 0x100 */
    expect_bad("S-4294967296-5-18");
    expect_bad("S--1-5-18");
}

TEST(strsid_w_empty_and_short)
{
    static const char *bad[] = { "", "S", "S-", "S-1", "S-1-", "S-1-5", "S--", "-", "--" };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) expect_bad(bad[i]);
}

TEST(strsid_w_trailing_garbage)
{
    static const char *bad[] = { "S-1-5-18x", "S-1-5-18 ", "S-1-5-18-x", "S-1-5-18;", "S-1-5-18)", "S-1-5-18\t", "S-1-5-32-544z" };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) expect_bad(bad[i]);
}

/* Unverified in decomp (LocalpConvertStringSidToSid not decompiled): observation only. */
TEST(strsid_w_unverified_forms_observed)
{
    static const char *odd[] = { "S-1-5-18-", "S-1-5-18--", " S-1-5-18", "S-1--5-18", "S-1-5--18" };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        PSID sid = NULL; BOOL r = parse(odd[i], &sid);
        t_note("\"%s\" -> %d err %lu", odd[i], (int)r, r ? 0 : GetLastError());
        if (r) LocalFree(sid);
    }
}

TEST(strsid_w_leading_garbage)
{
    static const char *bad[] = { "X-1-5-18", "-1-5-18", "1-5-18", "S 1-5-18", "S_1_5_18", "SS-1-5-18" };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) expect_bad(bad[i]);
}

TEST(strsid_w_malformed_numbers)
{
    static const char *bad[] = { "S-1-a-18", "S-1-5-a", "S-1-5-1a", "S-1-5-18-a-b" };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) expect_bad(bad[i]);
}

TEST(strsid_w_null_args)
{
    PSID sid = NULL;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    SET_ERR(0);
    EXPECT_FAIL(!UIN(SH, ConvertStringSidToSidW)(NULL, &sid), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_FAIL(!UIN(SH, ConvertStringSidToSidW)(NULL, NULL), ERROR_INVALID_PARAMETER);
    if (!t_is_wine()) {
        SET_ERR(0);
        EXPECT_FAIL(!UIN(SH, ConvertStringSidToSidW)(L"S-1-5-18", NULL), ERROR_INVALID_PARAMETER);
    }
}

static const struct { const char *alias; unsigned long long auth; int n; DWORD sub[2]; } aliases[] = {
    { "WD", 1, 1, {0} },        { "SY", 5, 1, {18} },       { "BA", 5, 2, {32, 544} },
    { "BU", 5, 2, {32, 545} },  { "BG", 5, 2, {32, 546} },  { "PU", 5, 2, {32, 547} },
    { "AO", 5, 2, {32, 548} },  { "SO", 5, 2, {32, 549} },  { "PO", 5, 2, {32, 550} },
    { "BO", 5, 2, {32, 551} },  { "RE", 5, 2, {32, 552} },  { "AU", 5, 1, {11} },
    { "AN", 5, 1, {7} },        { "CO", 3, 1, {0} },        { "CG", 3, 1, {1} },
    { "PS", 5, 1, {10} },       { "LS", 5, 1, {19} },       { "NS", 5, 1, {20} },
    { "ED", 5, 1, {9} },        { "IU", 5, 1, {4} },        { "SU", 5, 1, {6} },
    { "NU", 5, 1, {2} },        { "RC", 5, 1, {12} },       { "OW", 3, 1, {4} },
    { "LW", 16, 1, {4096} },    { "ME", 16, 1, {8192} },    { "HI", 16, 1, {12288} },
    { "SI", 16, 1, {16384} },   { "RU", 5, 2, {32, 554} },  { "RD", 5, 2, {32, 555} },
    { "NO", 5, 2, {32, 556} },  { "MU", 5, 2, {32, 558} },  { "LU", 5, 2, {32, 559} },
    { "CY", 5, 2, {32, 569} },  { "ER", 5, 2, {32, 573} },  { "CD", 5, 2, {32, 574} },
    { "RA", 5, 2, {32, 575} },  { "ES", 5, 2, {32, 576} },  { "MS", 5, 2, {32, 577} },
    { "WR", 5, 1, {33} },       { "AC", 15, 2, {2, 1} },    { "IS", 5, 2, {32, 568} },
};

TEST(strsid_w_aliases_table)
{
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof aliases / sizeof aliases[0]; i++) {
        PSID sid; SIDBUF b; PISID e; BOOL r; int k;
        WCHAR w[8]; widen(aliases[i].alias, w, 8);
        sid = NULL; SET_ERR(0x4242);
        r = UIN(SH, ConvertStringSidToSidW)(w, &sid);
        CHECKF(r, "alias %s failed err %lu", aliases[i].alias, GetLastError());
        if (!r) continue;
        NOTE_ERR(0);
        e = (PISID)mk_sid(&b, aliases[i].auth, 0);
        e->SubAuthorityCount = (BYTE)aliases[i].n;
        for (k = 0; k < aliases[i].n; k++) e->SubAuthority[k] = aliases[i].sub[k];
        CHECKF(EqualSid(sid, &b), "alias %s differs", aliases[i].alias);
        CHECK(LocalFree(sid) == NULL);
    }
}

TEST(strsid_w_alias_trailing_chars_rejected)
{
    static const char *bad[] = { "SYX", "BA ", "WDWD", "AU-", "ANA", "SY\t", "BA," };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) expect_bad(bad[i]);
}

TEST(strsid_w_alias_unknown)
{
    static const char *bad[] = { "ZZ", "QQ", "XX", "Q", "B", "W", "#$" };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) expect_bad(bad[i]);
}

TEST(strsid_w_alias_case)
{
    /* alias lookup case sensitivity is not decomp-verified: observe only */
    PSID sid = NULL; WCHAR w[] = L"sy";
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    SET_ERR(0);
    if (UIN(SH, ConvertStringSidToSidW)(w, &sid)) { t_note("lowercase alias 'sy' accepted"); LocalFree(sid); }
    else t_note("lowercase alias 'sy' rejected, err %lu", GetLastError());
}

TEST(strsid_w_roundtrip_table)
{
    static const char *tab[] = {
        "S-1-0-0", "S-1-1-0", "S-1-2-0", "S-1-2-1", "S-1-3-0", "S-1-3-1", "S-1-3-2", "S-1-3-3", "S-1-3-4",
        "S-1-5-1", "S-1-5-2", "S-1-5-3", "S-1-5-4", "S-1-5-6", "S-1-5-7", "S-1-5-8", "S-1-5-9", "S-1-5-10",
        "S-1-5-11", "S-1-5-12", "S-1-5-13", "S-1-5-14", "S-1-5-15", "S-1-5-17", "S-1-5-18", "S-1-5-19", "S-1-5-20",
        "S-1-5-32-544", "S-1-5-32-545", "S-1-5-32-546", "S-1-5-32-547", "S-1-5-32-548", "S-1-5-32-549",
        "S-1-5-32-550", "S-1-5-32-551", "S-1-5-32-552", "S-1-5-32-555", "S-1-5-64-10", "S-1-5-64-14", "S-1-5-64-21",
        "S-1-5-21-1004336348-1177238915-682003330-512", "S-1-5-21-1-2-3", "S-1-5-21-0-0-0-500",
        "S-1-5-80-0", "S-1-5-83-0", "S-1-5-90-0", "S-1-5-113", "S-1-5-114",
        "S-1-16-0", "S-1-16-4096", "S-1-16-8192", "S-1-16-8448", "S-1-16-12288", "S-1-16-16384", "S-1-16-20480", "S-1-16-28672",
        "S-1-15-2-1", "S-1-15-2-2", "S-1-15-3-1", "S-1-15-3-1024-1-2-3-4-5-6-7",
        "S-1-18-1", "S-1-18-2", "S-1-18-3", "S-1-4294967295-0", "S-1-9-1",
        "S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15",
        "S-1-0x000100000000-1", "S-1-0xffffffffffff-4294967295", "S-1-0x010000000000-2",
    };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW)); REQUIRE(UIN(SH, ConvertSidToStringSidW));
    for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        PSID sid = NULL; WCHAR w[300], *back = NULL; char a[300];
        widen(tab[i], w, 300);
        if (!UIN(SH, ConvertStringSidToSidW)(w, &sid)) { CHECKF(0, "parse %s err %lu", tab[i], GetLastError()); continue; }
        CHECKF(UIN(SH, ConvertSidToStringSidW)(sid, &back), "%s back", tab[i]);
        if (back) { narrow(back, a, sizeof a); CHECKF(!strcmp(a, tab[i]), "%s -> %s", tab[i], a); LocalFree(back); }
        LocalFree(sid);
    }
}

TEST(strsid_w_noncanonical_normalizes)
{
    /* parse forms that differ textually, print canonical */
    static const struct { const char *in, *out; } tab[] = {
        { "s-1-5-18", "S-1-5-18" }, { "S-1-0x5-18", "S-1-5-18" }, { "S-1-5-018", "S-1-5-18" },
        { "S-1-05-18", "S-1-5-18" }, { "S-1-4294967296-1", "S-1-0x000100000000-1" },
        { "S-001-5-18", "S-1-5-18" },
    };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSidToSidW)); REQUIRE(UIN(SH, ConvertSidToStringSidW));
    for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        PSID sid = NULL; WCHAR w[100], *back = NULL; char a[100];
        widen(tab[i].in, w, 100);
        if (!UIN(SH, ConvertStringSidToSidW)(w, &sid)) { t_note("%s rejected (err %lu)", tab[i].in, GetLastError()); continue; }
        if (UIN(SH, ConvertSidToStringSidW)(sid, &back)) {
            narrow(back, a, sizeof a);
            if (strcmp(a, tab[i].out)) t_note("%s -> %s (expected %s)", tab[i].in, a, tab[i].out);
            LocalFree(back);
        }
        LocalFree(sid);
    }
}

TEST(strsid_w_subauth_count_limits)
{
    /* 15 sub authorities is the SID maximum and parses; the 16 case is observed only */
    PSID sid = NULL; WCHAR w[300];
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    widen("S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15", w, 300);
    CHECK(UIN(SH, ConvertStringSidToSidW)(w, &sid));
    if (sid) { CHECK(IsValidSid(sid)); EXPECT_EQ(*GetSidSubAuthorityCount(sid), 15); LocalFree(sid); }
    widen("S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15-16", w, 300);
    sid = NULL; SET_ERR(0);
    if (UIN(SH, ConvertStringSidToSidW)(w, &sid)) { t_note("16 sub-authorities parsed; IsValidSid=%d", IsValidSid(sid)); LocalFree(sid); }
    else t_note("16 sub-authorities rejected, err %lu", GetLastError());
}

TEST(strsid_w_failure_leaves_out_untouched_or_null)
{
    PSID sid = (PSID)0x1;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    SET_ERR(0);
    CHECK(!UIN(SH, ConvertStringSidToSidW)(L"S-1-5-18x", &sid));
    CHECKF(sid == NULL || sid == (PSID)0x1, "out ptr %p", sid);
}

TEST(strsid_w_result_independent_allocations)
{
    PSID a = NULL, b = NULL;
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    CHECK(UIN(SH, ConvertStringSidToSidW)(L"S-1-5-18", &a));
    CHECK(UIN(SH, ConvertStringSidToSidW)(L"S-1-5-18", &b));
    CHECK(a != b);
    if (a && b) CHECK(EqualSid(a, b));
    if (a) CHECKF(LocalSize(a) >= 12, "LocalSize %llu", (unsigned long long)LocalSize(a));
    LocalFree(a); LocalFree(b);
}

/* ---- A variant ---- */
TEST(strsid_a_basic)
{
    PSID sid = NULL; SIDBUF b;
    REQUIRE(UIN(AD, ConvertStringSidToSidA));
    CHECK(UIN(AD, ConvertStringSidToSidA)("S-1-5-18", &sid));
    if (sid) { CHECK(EqualSid(sid, mk_sid(&b, 5, 1, 18))); LocalFree(sid); }
    sid = NULL;
    CHECK(UIN(AD, ConvertStringSidToSidA)("BA", &sid));
    if (sid) { CHECK(EqualSid(sid, mk_sid(&b, 5, 2, 32, 544))); LocalFree(sid); }
}

TEST(strsid_a_invalid)
{
    static const char *bad[] = { "", "S-1-5-18x", "ZZ", "S-1", "BAX" };
    unsigned i;
    REQUIRE(UIN(AD, ConvertStringSidToSidA));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSID sid = NULL; SET_ERR(0);
        CHECKF(!UIN(AD, ConvertStringSidToSidA)(bad[i], &sid), "\"%s\"", bad[i]);
        NOTE_ERR(ERR_INVALID_SID);
    }
}

TEST(strsid_a_null_string)
{
    PSID sid = NULL;
    REQUIRE(UIN(AD, ConvertStringSidToSidA));
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertStringSidToSidA)(NULL, &sid));
    NOTE_ERR(ERROR_INVALID_PARAMETER);
}
