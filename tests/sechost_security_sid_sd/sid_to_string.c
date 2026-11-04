#include "ssh.h"
/* ConvertSidToStringSidW/A. Decomp (sechost): NULL sid or NULL out -> STATUS_INVALID_PARAMETER -> 87;
 * RtlConvertSidToUnicodeString failures map through RtlNtStatusToDosError; success SetLastError(0). */

static const struct { const char *name; unsigned long long auth; int n; DWORD sub[8]; const char *str; } sidtab[] = {
    { "null",        0, 1, {0},                 "S-1-0-0" },
    { "everyone",    1, 1, {0},                 "S-1-1-0" },
    { "local",       2, 1, {0},                 "S-1-2-0" },
    { "creator",     3, 1, {0},                 "S-1-3-0" },
    { "creatorgrp",  3, 1, {1},                 "S-1-3-1" },
    { "owner_rts",   3, 1, {4},                 "S-1-3-4" },
    { "system",      5, 1, {18},                "S-1-5-18" },
    { "localsvc",    5, 1, {19},                "S-1-5-19" },
    { "netsvc",      5, 1, {20},                "S-1-5-20" },
    { "authusers",   5, 1, {11},                "S-1-5-11" },
    { "anonymous",   5, 1, {7},                 "S-1-5-7" },
    { "admins",      5, 2, {32, 544},           "S-1-5-32-544" },
    { "users",       5, 2, {32, 545},           "S-1-5-32-545" },
    { "guests",      5, 2, {32, 546},           "S-1-5-32-546" },
    { "domain500",   5, 5, {21, 1, 2, 3, 500},  "S-1-5-21-1-2-3-500" },
    { "domain_big",  5, 5, {21, 4294967295u, 2147483648u, 1, 1000}, "S-1-5-21-4294967295-2147483648-1-1000" },
    { "il_low",      16, 1, {4096},             "S-1-16-4096" },
    { "il_medium",   16, 1, {8192},             "S-1-16-8192" },
    { "il_high",     16, 1, {12288},            "S-1-16-12288" },
    { "il_system",   16, 1, {16384},            "S-1-16-16384" },
    { "appcontainer",15, 2, {2, 1},             "S-1-15-2-1" },
    { "auth_asserted",18,1, {1},                "S-1-18-1" },
    { "auth_max_dec",0xFFFFFFFFull, 1, {1},     "S-1-4294967295-1" },
    { "auth_hex_2p32",0x100000000ull, 1, {1},   "S-1-0x100000000-1" },
    { "auth_hex_max",0xFFFFFFFFFFFFull, 1, {7}, "S-1-0xFFFFFFFFFFFF-7" },
    { "auth_hex_mid",0x123456789abcull, 2, {1, 2}, "S-1-0x123456789ABC-1-2" },
    { "auth_hex_lo", 0x010000000000ull, 1, {5}, "S-1-0x10000000000-5" },
    { "auth_hex_b1", 0x000100000000ull, 1, {5}, "S-1-0x100000000-5" },
    { "auth_hex_b0", 0x010000000000ull, 1, {0}, "S-1-0x10000000000-0" },
    { "zero_subauth", 5, 0, {0},                "S-1-5" },
    { "subauth_zero_pad", 5, 3, {0, 0, 0},      "S-1-5-0-0-0" },
};
#define NSID (sizeof sidtab / sizeof sidtab[0])

static PSID tab_sid(SIDBUF *b, int i)
{
    PISID s = (PISID)mk_sid(b, sidtab[i].auth, 0);
    int k;
    s->SubAuthorityCount = (BYTE)sidtab[i].n;
    for (k = 0; k < sidtab[i].n; k++) s->SubAuthority[k] = sidtab[i].sub[k];
    return s;
}

TEST(sidstr_w_table)
{
    unsigned i;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    for (i = 0; i < NSID; i++) {
        SIDBUF b; WCHAR *w = NULL; char a[256]; BOOL r;
        PSID sid = tab_sid(&b, i);
        SET_ERR(0xdeadbeef);
        r = UIN(SH, ConvertSidToStringSidW)(sid, &w);
        CHECKF(r, "%s: failed err %lu", sidtab[i].name, GetLastError());
        if (!r) continue;
        narrow(w, a, sizeof a);
        CHECKF(!strcmp(a, sidtab[i].str), "%s: got %s want %s", sidtab[i].name, a, sidtab[i].str);
        LocalFree(w);
    }
}

TEST(sidstr_w_success_clears_last_error)
{
    SIDBUF b; WCHAR *w = NULL;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    SET_ERR(0x1234);
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 5, 1, 18), &w));
    EXPECT_EQ(GetLastError(), 0);
    LocalFree(w);
}

TEST(sidstr_w_null_sid)
{
    WCHAR *w = (WCHAR *)0x1;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    SET_ERR(0);
    EXPECT_FAIL(!UIN(SH, ConvertSidToStringSidW)(NULL, &w), ERROR_INVALID_PARAMETER);
}

TEST(sidstr_w_null_out)
{
    SIDBUF b;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    if (t_is_wine()) { t_skip("NULL out pointer faults under wine"); return; }
    SET_ERR(0);
    EXPECT_FAIL(!UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 5, 1, 18), NULL), ERROR_INVALID_PARAMETER);
}

TEST(sidstr_w_both_null)
{
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    SET_ERR(0);
    EXPECT_FAIL(!UIN(SH, ConvertSidToStringSidW)(NULL, NULL), ERROR_INVALID_PARAMETER);
}

TEST(sidstr_w_bad_revision)
{
    SIDBUF b; WCHAR *w = NULL; int rev;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    for (rev = 0; rev <= 3; rev++) {
        PSID s = mk_sid(&b, 5, 1, 18);
        ((PISID)s)->Revision = (BYTE)rev;
        SET_ERR(0);
        if (rev == 1) { CHECK(UIN(SH, ConvertSidToStringSidW)(s, &w)); LocalFree(w); continue; }
        EXPECT_FAIL(!UIN(SH, ConvertSidToStringSidW)(s, &w), ERR_INVALID_SID);
    }
}

TEST(sidstr_w_subauth_count_limit)
{
    SIDBUF b; WCHAR *w = NULL; char a[512]; int n;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    for (n = 0; n <= 15; n++) {
        PISID s = (PISID)mk_sid(&b, 5, 0); int k;
        s->SubAuthorityCount = (BYTE)n;
        for (k = 0; k < n; k++) s->SubAuthority[k] = k + 1;
        SET_ERR(0);
        CHECKF(UIN(SH, ConvertSidToStringSidW)(s, &w), "n=%d", n);
        if (n == 15) {
            narrow(w, a, sizeof a);
            EXPECT_STR(a, "S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15");
        }
        LocalFree(w);
    }
    for (n = 16; n <= 17; n++) {
        PISID s = (PISID)mk_sid(&b, 5, 0);
        s->SubAuthorityCount = (BYTE)n;     /* SIDBUF is zero filled beyond 15 slots; max-size buffer covers 15 only */
        SET_ERR(0);
        EXPECT_FAIL(!UIN(SH, ConvertSidToStringSidW)(s, &w), ERR_INVALID_SID);
    }
}

TEST(sidstr_w_allocation_is_localalloc)
{
    SIDBUF b; WCHAR *w = NULL;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 5, 2, 32, 544), &w));
    if (w) {
        SIZE_T sz = LocalSize(w);
        CHECKF(sz >= (wcslen(w) + 1) * 2, "LocalSize %llu", (unsigned long long)sz);
        EXPECT_EQ(wcslen(w), 12);
        CHECK(LocalFree(w) == NULL);
    }
}

TEST(sidstr_w_output_null_terminated_exact)
{
    SIDBUF b; WCHAR *w = NULL;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 1, 1, 0), &w));
    if (w) { EXPECT_EQ(wcslen(w), 7); EXPECT_EQ(w[7], 0); LocalFree(w); }
}

TEST(sidstr_w_independent_calls)
{
    SIDBUF b; WCHAR *w1 = NULL, *w2 = NULL;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 1, 1, 0), &w1));
    CHECK(UIN(SH, ConvertSidToStringSidW)(&b, &w2));
    CHECK(w1 != w2);
    if (w1 && w2) EXPECT_WSTR(w1, w2);
    LocalFree(w1); LocalFree(w2);
}

TEST(sidstr_w_hex_authority_threshold)
{
    /* authority printed decimal iff the two high bytes are zero (value < 2^32) */
    SIDBUF b; WCHAR *w = NULL; char a[128];
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 0xFFFFFFFFull, 1, 1), &w));
    narrow(w, a, sizeof a); EXPECT_STR(a, "S-1-4294967295-1"); LocalFree(w);
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 0x100000000ull, 1, 1), &w));
    narrow(w, a, sizeof a); EXPECT_STR(a, "S-1-0x100000000-1"); LocalFree(w);
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 0x1ull, 1, 1), &w));
    narrow(w, a, sizeof a); EXPECT_STR(a, "S-1-1-1"); LocalFree(w);
}

TEST(sidstr_w_hex_digits_uppercase)
{
    SIDBUF b; WCHAR *w = NULL; char a[128];
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    CHECK(UIN(SH, ConvertSidToStringSidW)(mk_sid(&b, 0xABCDEF012345ull, 1, 0xABCDEF), &w));
    narrow(w, a, sizeof a); EXPECT_STR(a, "S-1-0xABCDEF012345-11259375"); LocalFree(w);
}

/* ---- A variant (advapi32 only, converts the W result) ---- */
TEST(sidstr_a_table)
{
    unsigned i;
    REQUIRE(UIN(AD, ConvertSidToStringSidA));
    for (i = 0; i < NSID; i++) {
        SIDBUF b; char *s = NULL; BOOL r;
        PSID sid = tab_sid(&b, i);
        r = UIN(AD, ConvertSidToStringSidA)(sid, &s);
        CHECKF(r, "%s failed err %lu", sidtab[i].name, GetLastError());
        if (r) { CHECKF(!strcmp(s, sidtab[i].str), "%s: got %s", sidtab[i].name, s); LocalFree(s); }
    }
}

TEST(sidstr_a_null_sid)
{
    char *s = (char *)1;
    REQUIRE(UIN(AD, ConvertSidToStringSidA));
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertSidToStringSidA)(NULL, &s));
    NOTE_ERR(ERROR_INVALID_PARAMETER);
}

TEST(sidstr_a_bad_revision)
{
    SIDBUF b; char *s = NULL;
    REQUIRE(UIN(AD, ConvertSidToStringSidA));
    ((PISID)mk_sid(&b, 5, 1, 18))->Revision = 2;
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertSidToStringSidA)(&b, &s));
    NOTE_ERR(ERR_INVALID_SID);
}

/* A and W agree for every table entry */
TEST(sidstr_a_matches_w)
{
    unsigned i;
    REQUIRE(UIN(AD, ConvertSidToStringSidA)); REQUIRE(UIN(SH, ConvertSidToStringSidW));
    for (i = 0; i < NSID; i++) {
        SIDBUF b; char *s = NULL, a[256]; WCHAR *w = NULL;
        PSID sid = tab_sid(&b, i);
        if (UIN(AD, ConvertSidToStringSidA)(sid, &s) && UIN(SH, ConvertSidToStringSidW)(sid, &w)) {
            narrow(w, a, sizeof a);
            CHECKF(!strcmp(a, s), "%s", sidtab[i].name);
        } else CHECKF(0, "%s: a call failed", sidtab[i].name);
        if (s) LocalFree(s);
        if (w) LocalFree(w);
    }
}

/* Cross-check against the advapi32 route (forwarder): same results. */
TEST(sidstr_advapi_forwarder_matches_sechost)
{
    SIDBUF b; WCHAR *w1 = NULL, *w2 = NULL;
    REQUIRE(UIN(AD, ConvertSidToStringSidW)); REQUIRE(UIN(SH, ConvertSidToStringSidW));
    mk_sid(&b, 5, 2, 32, 544);
    CHECK(UIN(AD, ConvertSidToStringSidW)(&b, &w1));
    CHECK(UIN(SH, ConvertSidToStringSidW)(&b, &w2));
    if (w1 && w2) EXPECT_WSTR(w1, w2);
    LocalFree(w1); LocalFree(w2);
    SET_ERR(0);
    EXPECT_FAIL(!UIN(AD, ConvertSidToStringSidW)(NULL, &w1), ERROR_INVALID_PARAMETER);
}

TEST(sidstr_w_real_token_user_sid)
{
    HANDLE tok; BYTE buf[256]; DWORD n = 0; WCHAR *w = NULL;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    if (GetTokenInformation(tok, TokenUser, buf, sizeof buf, &n)) {
        PSID s = ((TOKEN_USER *)buf)->User.Sid;
        CHECK(UIN(SH, ConvertSidToStringSidW)(s, &w));
        if (w) {
            CHECK(w[0] == L'S' && w[1] == L'-' && w[2] == L'1' && w[3] == L'-');
            CHECK(wcsncmp(w, L"S-1-5-", 6) == 0);
            LocalFree(w);
        }
    }
    CloseHandle(tok);
}
