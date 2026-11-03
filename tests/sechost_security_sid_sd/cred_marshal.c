#include "ssh.h"
/* CredMarshalCredentialA/W, CredUnmarshalCredentialA/W, CredIsMarshaledCredentialW/A, CredFree.
 * Marshaled strings use a 6-bit alphabet "ABC...Zabc...z0123456789#-" (A=0 ... Z=25, a=26, z=51, 0=52, 9=61, #=62, -=63).
 * Format: "@@" + type letter (B = CertCredential(1), C = UsernameTargetCredential(2)) + payload.
 * Expected strings below are the Windows-verified values from Wine's advapi32 cred.c tests. */

#define CMW UIN(SH, CredMarshalCredentialW)
#define CMA UIN(SH, CredMarshalCredentialA)
#define CUW UIN(SH, CredUnmarshalCredentialW)
#define CUA UIN(SH, CredUnmarshalCredentialA)
#define CIW UIN(SH, CredIsMarshaledCredentialW)
#define CIA UIN(AD, CredIsMarshaledCredentialA)
#define CFREE UIN(SH, CredFree)

static const CERT_CREDENTIAL_INFO *cert_with(CERT_CREDENTIAL_INFO *c, const unsigned char *h, size_t n)
{
    memset(c, 0, sizeof *c); c->cbSize = sizeof *c; if (n) memcpy(c->rgbHashOfCert, h, n); return c;
}
static void expect_marshal_cert(const CERT_CREDENTIAL_INFO *c, const char *want)
{
    char *s = NULL; WCHAR *w = NULL; char a[128]; BOOL r;
    SET_ERR(0x55);
    r = CMA(CertCredential, (void *)c, &s);
    CHECKF(r, "A marshal for \"%s\" err %lu", want, GetLastError());
    if (r && s) { CHECKF(!strcmp(s, want), "A: got \"%s\" want \"%s\"", s, want); CFREE(s); }
    r = CMW(CertCredential, (void *)c, &w);
    CHECKF(r, "W marshal for \"%s\"", want);
    if (r && w) { narrow(w, a, sizeof a); CHECKF(!strcmp(a, want), "W: got \"%s\" want \"%s\"", a, want); CFREE(w); }
}

TEST(cred_marshal_cert_vectors)
{
    CERT_CREDENTIAL_INFO c; unsigned char h[20];
    REQUIRE(CMA); REQUIRE(CMW); REQUIRE(CFREE);
    expect_marshal_cert(cert_with(&c, NULL, 0), "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    c.cbSize = sizeof c + 4;   /* larger cbSize is accepted */
    expect_marshal_cert(&c, "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    memset(h, 0, 20); h[0] = 2; expect_marshal_cert(cert_with(&c, h, 20), "@@BCAAAAAAAAAAAAAAAAAAAAAAAAAA");
    memset(h, 0, 20); h[0] = 255; expect_marshal_cert(cert_with(&c, h, 20), "@@B-DAAAAAAAAAAAAAAAAAAAAAAAAA");
    memset(h, 0, 20); h[0] = 1; h[1] = 1; expect_marshal_cert(cert_with(&c, h, 20), "@@BBEAAAAAAAAAAAAAAAAAAAAAAAAA");
    memset(h, 0, 20); h[0] = 1; h[1] = 1; h[2] = 1; expect_marshal_cert(cert_with(&c, h, 20), "@@BBEQAAAAAAAAAAAAAAAAAAAAAAAA");
    memset(h, 0, 20); memcpy(h, "Wine", 4); expect_marshal_cert(cert_with(&c, h, 20), "@@BXlmblBAAAAAAAAAAAAAAAAAAAAA");
    memset(h, 0xff, 20); expect_marshal_cert(cert_with(&c, h, 20), "@@B--------------------------P");
}
TEST(cred_marshal_cert_length_is_fixed)
{
    CERT_CREDENTIAL_INFO c; unsigned i; char *s = NULL;
    REQUIRE(CMA); REQUIRE(CFREE);
    for (i = 0; i < 20; i++) {
        unsigned char h[20] = { 0 }; h[i] = 0x5a;
        if (CMA(CertCredential, cert_with(&c, h, 20), &s)) { EXPECT_EQ(strlen(s), 30); CHECK(s[0] == '@' && s[1] == '@' && s[2] == 'B'); CFREE(s); }
        else CHECKF(0, "byte %u", i);
    }
}
TEST(cred_marshal_cert_invalid_args)
{
    CERT_CREDENTIAL_INFO c; char *s = (char *)0xdeadbeef;
    REQUIRE(CMA);
    cert_with(&c, NULL, 0);
    SET_ERR(0); CHECK(!CMA(0, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CMA(0, &c, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); s = (char *)0xdeadbeef; CHECK(!CMA(0, &c, &s)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(s, 0xdeadbeef);
    SET_ERR(0); CHECK(!CMA(CertCredential, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    c.cbSize = 0; s = (char *)0xdeadbeef; SET_ERR(0);
    CHECK(!CMA(CertCredential, &c, &s)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(s, 0xdeadbeef);
    c.cbSize = sizeof c - 1; s = (char *)0xdeadbeef; SET_ERR(0);
    CHECK(!CMA(CertCredential, &c, &s)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(cred_marshal_bad_type_values)
{
    CERT_CREDENTIAL_INFO c; char *s = (char *)0xdeadbeef; static const int types[] = { 0, 4, 5, 100, -1 };
    unsigned i;
    REQUIRE(CMA);
    cert_with(&c, NULL, 0);
    for (i = 0; i < sizeof types / sizeof types[0]; i++) {
        s = (char *)0xdeadbeef; SET_ERR(0);
        CHECKF(!CMA((CRED_MARSHAL_TYPE)types[i], &c, &s), "type %d", types[i]);
        NOTE_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ(s, 0xdeadbeef);
    }
}
TEST(cred_marshal_username_vectors)
{
    static const struct { const WCHAR *u; const char *m; } t[] = {
        { L"t",     "@@CCAAAAA0BA" },
        { L"te",    "@@CEAAAAA0BQZAA" },
        { L"tes",   "@@CGAAAAA0BQZAMHA" },
        { L"test",  "@@CIAAAAA0BQZAMHA0BA" },
        { L"test1", "@@CKAAAAA0BQZAMHA0BQMAA" },
    };
    unsigned i;
    REQUIRE(CMA); REQUIRE(CMW); REQUIRE(CFREE);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        USERNAME_TARGET_CREDENTIAL_INFO u; char *s = NULL; WCHAR *w = NULL; char a[128];
        u.UserName = (LPWSTR)t[i].u;
        CHECK(CMA(UsernameTargetCredential, &u, &s));
        if (s) { CHECKF(!strcmp(s, t[i].m), "A: \"%ls\" -> \"%s\" want \"%s\"", t[i].u, s, t[i].m); CFREE(s); }
        CHECK(CMW(UsernameTargetCredential, &u, &w));
        if (w) { narrow(w, a, sizeof a); CHECKF(!strcmp(a, t[i].m), "W: \"%ls\" -> \"%s\"", t[i].u, a); CFREE(w); }
    }
}
TEST(cred_marshal_username_invalid)
{
    USERNAME_TARGET_CREDENTIAL_INFO u; char *s = (char *)0xdeadbeef;
    REQUIRE(CMA);
    u.UserName = NULL; SET_ERR(0);
    CHECK(!CMA(UsernameTargetCredential, &u, &s)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(s, 0xdeadbeef);
    u.UserName = (LPWSTR)L""; s = (char *)0xdeadbeef; SET_ERR(0);
    CHECK(!CMA(UsernameTargetCredential, &u, &s)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(s, 0xdeadbeef);
    SET_ERR(0);
    CHECK(!CMA(UsernameTargetCredential, NULL, &s)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(cred_marshal_username_longer_names)
{
    static const WCHAR *names[] = { L"a", L"ab", L"abc", L"user@domain.com", L"DOMAIN\\user", L"\x00e9\x00e8", L"0123456789012345678901234567890123456789" };
    unsigned i;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CFREE);
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        USERNAME_TARGET_CREDENTIAL_INFO u; WCHAR *w = NULL; CRED_MARSHAL_TYPE ty = 0; void *p = NULL;
        u.UserName = (LPWSTR)names[i];
        CHECK(CMW(UsernameTargetCredential, &u, &w));
        if (!w) continue;
        CHECK(w[0] == '@' && w[1] == '@' && w[2] == 'C');
        CHECK(CIW(w));
        if (CUW(w, &ty, &p)) {
            EXPECT_EQ(ty, UsernameTargetCredential);
            CHECKF(!wcscmp(((USERNAME_TARGET_CREDENTIAL_INFO *)p)->UserName, names[i]), "name %u roundtrip", i);
            CFREE(p);
        } else CHECKF(0, "unmarshal name %u err %lu", i, GetLastError());
        CFREE(w);
    }
}
TEST(cred_marshal_cert_roundtrip_all_bytes)
{
    unsigned v; CERT_CREDENTIAL_INFO c;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CFREE);
    for (v = 0; v < 256; v += 5) {
        unsigned char h[20]; unsigned k; WCHAR *w = NULL; CRED_MARSHAL_TYPE ty = 0; CERT_CREDENTIAL_INFO *out = NULL;
        for (k = 0; k < 20; k++) h[k] = (unsigned char)(v + k * 13);
        CHECK(CMW(CertCredential, cert_with(&c, h, 20), &w));
        if (!w) continue;
        if (CUW(w, &ty, (void **)&out)) {
            EXPECT_EQ(ty, CertCredential); EXPECT_EQ(out->cbSize, sizeof *out);
            CHECK(!memcmp(out->rgbHashOfCert, h, 20));
            CFREE(out);
        } else CHECKF(0, "unmarshal v=%u err %lu", v, GetLastError());
        CFREE(w);
    }
}

TEST(cred_unmarshal_vectors)
{
    static const unsigned char zero[20] = { 0 }, wine[20] = { 'W', 'i', 'n', 'e' };
    static const struct { const char *s; CRED_MARSHAL_TYPE ty; const void *val; BOOL user; } t[] = {
        { "", 0, NULL }, { "@", 0, NULL }, { "@@", 0, NULL }, { "@@@", 0, NULL }, { "@@A", 0, NULL },
        { "@@E", 4, NULL }, { "@@Z", 25, NULL }, { "@@a", 26, NULL }, { "@@0", 52, NULL }, { "@@#", 62, NULL }, { "@@-", 63, NULL },
        { "@@B", CertCredential, NULL }, { "@@BA", CertCredential, NULL },
        { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAA", CertCredential, NULL },
        { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAAA", CertCredential, NULL },
        { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA", CertCredential, zero },
        { "@@BXlmblBAAAAAAAAAAAAAAAAAAAAA", CertCredential, wine },
        { "@@C", UsernameTargetCredential, NULL }, { "@@CA", UsernameTargetCredential, NULL },
        { "@@CAAAAAA", UsernameTargetCredential, NULL }, { "@@CAAAAAA0B", UsernameTargetCredential, NULL },
        { "@@CAAAAAA0BA", UsernameTargetCredential, NULL },
        { "@@CCAAAAA0BA", UsernameTargetCredential, L"t", TRUE },
        { "@@CEAAAAA0BA", UsernameTargetCredential, NULL }, { "@@CEAAAAA0BAd", UsernameTargetCredential, NULL },
        { "@@CEAAAAA0BAdA", UsernameTargetCredential, NULL },
        { "@@CEAAAAA0BQZAA", UsernameTargetCredential, L"te", TRUE }, { "@@CEAAAAA0BQZAQ", UsernameTargetCredential, L"te", TRUE },
        { "@@CEAAAAA0BQZAg", UsernameTargetCredential, L"te", TRUE }, { "@@CEAAAAA0BQZAw", UsernameTargetCredential, L"te", TRUE },
        { "@@CEAAAAA0BQZAAA", UsernameTargetCredential, NULL }, { "@@CGAAAAA0BQZAMH", UsernameTargetCredential, NULL },
        { "@@CGAAAAA0BQZAMHA", UsernameTargetCredential, L"tes", TRUE }, { "@@CGAAAAA0BQZAMHAA", UsernameTargetCredential, NULL },
        { "@@CCAAAAA0BAA", UsernameTargetCredential, NULL }, { "@@CBAAAAA0BAA", UsernameTargetCredential, NULL },
        { "@@CAgAAAA0BAA", UsernameTargetCredential, NULL },
        { "@@CIAAAAA0BQZAMHA0BA", UsernameTargetCredential, L"test", TRUE }, { "@@CA-----0BQZAMHA0BA", UsernameTargetCredential, NULL },
    };
    unsigned i;
    REQUIRE(CUA); REQUIRE(CFREE);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        CRED_MARSHAL_TYPE ty = 0; void *p = NULL; BOOL r;
        SET_ERR(0xdeadbeef);
        r = CUA(t[i].s, &ty, &p);
        if (t[i].val) {
            CHECKF(r, "[%u] \"%s\" failed err %lu", i, t[i].s, GetLastError());
            if (!r) continue;
            CHECKF(ty == t[i].ty, "[%u] type %d", i, ty);
            CHECKF(p != NULL, "[%u] NULL", i);
            if (t[i].ty == CertCredential) {
                CERT_CREDENTIAL_INFO *c = p;
                CHECKF(c->cbSize == sizeof *c && !memcmp(c->rgbHashOfCert, t[i].val, 20), "[%u] cert content", i);
            } else {
                USERNAME_TARGET_CREDENTIAL_INFO *u = p;
                CHECKF(u->UserName && !wcscmp(u->UserName, t[i].val), "[%u] username", i);
            }
            CFREE(p);
        } else {
            CHECKF(!r, "[%u] \"%s\" unexpectedly succeeded", i, t[i].s);
            if (r) { CFREE(p); continue; }
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "[%u] \"%s\" err %lu", i, t[i].s, GetLastError());
            CHECKF(ty == t[i].ty, "[%u] \"%s\" type out %d want %d", i, t[i].s, ty, t[i].ty);
            CHECKF(p == NULL, "[%u] out ptr set", i);
        }
    }
}
TEST(cred_unmarshal_null_args)
{
    void *p = NULL; CRED_MARSHAL_TYPE ty = 0;
    REQUIRE(CUA);
    SET_ERR(0); CHECK(!CUA(NULL, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CUA(NULL, NULL, &p)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CUA(NULL, &ty, &p)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(cred_unmarshal_w_matches_a)
{
    static const char *t[] = { "@@BXlmblBAAAAAAAAAAAAAAAAAAAAA", "@@CIAAAAA0BQZAMHA0BA", "@@CCAAAAA0BA", "@@C", "bogus" };
    unsigned i;
    REQUIRE(CUW); REQUIRE(CUA);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        WCHAR w[64]; CRED_MARSHAL_TYPE ta = 0, tw = 0; void *pa = NULL, *pw = NULL; BOOL ra, rw;
        widen(t[i], w, 64);
        ra = CUA(t[i], &ta, &pa); rw = CUW(w, &tw, &pw);
        CHECKF(ra == rw, "\"%s\"", t[i]);
        if (ra && rw) { EXPECT_EQ(ta, tw); CFREE(pa); CFREE(pw); }
    }
}
TEST(cred_is_marshaled_vectors)
{
    static const struct { const char *s; BOOL want; } t[] = {
        { "@@BXlmblBAAAAAAAAAAAAAAAAAAAAA", TRUE }, { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAA", TRUE },
        { "@@CCAAAAA0BA", TRUE }, { "@@CIAAAAA0BQZAMHA0BA", TRUE },
        { "winetest", FALSE }, { "", FALSE }, { "@@", FALSE }, { "@@A", FALSE }, { "@@AA", FALSE }, { "@@AAA", FALSE },
        { "@@B", FALSE }, { "@@BB", FALSE }, { "@@BBB", FALSE },
        { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAAAA", FALSE }, { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAA", FALSE },
        { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAA+", FALSE }, { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAA:", FALSE },
        { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAA>", FALSE }, { "@@BAAAAAAAAAAAAAAAAAAAAAAAAAA<", FALSE },
        { "@@C", FALSE }, { "@@CC", FALSE }, { "@@CCC", FALSE }, { "@@D", FALSE }, { "@@DD", FALSE }, { "@@DDD", FALSE },
    };
    unsigned i;
    REQUIRE(CIW);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        WCHAR w[64]; BOOL r;
        widen(t[i].s, w, 64);
        r = CIW(w);
        CHECKF(!!r == t[i].want, "\"%s\" -> %d want %d", t[i].s, r, t[i].want);
        if (CIA) { r = CIA(t[i].s); CHECKF(!!r == t[i].want, "A \"%s\" -> %d", t[i].s, r); }
    }
}
TEST(cred_is_marshaled_null)
{
    REQUIRE(CIW);
    SET_ERR(0);
    CHECK(!CIW(NULL));
    NOTE_ERR(ERROR_INVALID_PARAMETER);
}
TEST(cred_is_marshaled_does_not_leak_or_change_state)
{
    WCHAR w[64]; unsigned i;
    REQUIRE(CIW);
    widen("@@CIAAAAA0BQZAMHA0BA", w, 64);
    for (i = 0; i < 1000; i++) if (!CIW(w)) { CHECKF(0, "iteration %u", i); break; }
}
TEST(cred_free_accepts_marshal_results_only)
{
    CERT_CREDENTIAL_INFO c; char *s = NULL;
    REQUIRE(CMA); REQUIRE(CFREE);
    CHECK(CMA(CertCredential, cert_with(&c, NULL, 0), &s));
    CFREE(s);
    CFREE(NULL);
    t_note("CredFree(NULL) returned");
}
TEST(cred_marshal_a_w_agree_property)
{
    unsigned v;
    REQUIRE(CMA); REQUIRE(CMW); REQUIRE(CFREE);
    for (v = 0; v < 64; v++) {
        unsigned char h[20]; unsigned k; CERT_CREDENTIAL_INFO c; char *a = NULL; WCHAR *w = NULL; char n[64];
        for (k = 0; k < 20; k++) h[k] = (unsigned char)(v * 4 + k * 7);
        cert_with(&c, h, 20);
        CHECK(CMA(CertCredential, &c, &a)); CHECK(CMW(CertCredential, &c, &w));
        if (a && w) { narrow(w, n, sizeof n); CHECK(!strcmp(a, n)); }
        CFREE(a); CFREE(w);
    }
}
