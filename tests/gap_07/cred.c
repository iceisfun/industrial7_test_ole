/* CredMarshalCredentialA/W, CredUnmarshalCredentialA/W, CredIsMarshaledCredentialW: depth beyond the basic vectors.
 *
 * Decomp of sechost (Win10 19041): the marshaled form is L"@@" + one type char + payload, with a 6-bit alphabet
 *   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#-"   (3 bytes -> 4 chars, little endian bit order)
 * Types accepted by CredMarshalCredentialW:
 *   1 CertCredential               'B'  20 hash bytes
 *   2 UsernameTargetCredential     'C'  u32 byte length + UTF-16 bytes
 *   3 BinaryBlobCredential         'D'  5 WCHAR prefix 07 08 0c 0a 0d, u32 byte length, blob bytes
 *   4 UsernameForPackedCredentials 'E'  same payload as type 2
 *   5 BinaryBlobForSystem          'F'  5 WCHAR prefix 08 07 0c 0a 0d, u32 byte length, blob bytes
 * Unmarshal of the blob types additionally requires a non-zero EVEN byte count. All results are LocalAlloc blocks. */
#include "gap7.h"
#include <wincred.h>

#define CMW UIN(SH, CredMarshalCredentialW)
#define CMA UIN(SH, CredMarshalCredentialA)
#define CUW UIN(SH, CredUnmarshalCredentialW)
#define CUA UIN(SH, CredUnmarshalCredentialA)
#define CIW UIN(SH, CredIsMarshaledCredentialW)
#define CIA UIN("advapi32.dll", CredIsMarshaledCredentialA)
#define CFREE UIN(SH, CredFree)
/* Wine implements only credential types 1 and 2; types 3-5 exist on Windows (sechost decomp) */
#define WSKIP345() do { if (t_is_wine()) { t_skip("wine lacks marshal types 3-5"); return; } } while (0)

enum { T_CERT = 1, T_USER = 2, T_BLOB = 3, T_PACKED = 4, T_SYSBLOB = 5 };
typedef struct { ULONG cbBlob; PUCHAR pbBlob; } BLOBCRED;

static const char ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#-";

/* ---- independent reference encoder (the documented 6-bit scheme) ---- */
static char *enc_bytes(char *o, const unsigned char *p, unsigned n)
{
    unsigned i, j;
    for (i = 0; i < n; i += 3) {
        unsigned k = n - i < 3 ? n - i : 3, v = 0;
        for (j = 0; j < k; j++) v |= (unsigned)p[i + j] << (8 * j);
        *o++ = ALPHA[v & 63]; *o++ = ALPHA[(v >> 6) & 63];
        if (k > 1) *o++ = ALPHA[(v >> 12) & 63];
        if (k > 2) *o++ = ALPHA[(v >> 18) & 63];
    }
    return o;
}
static char *enc_u32(char *o, unsigned v) { unsigned char b[4]; memcpy(b, &v, 4); return enc_bytes(o, b, 4); }
static void ref_blob(char *out, int type, const unsigned char *p, unsigned cb)
{
    char *o = out;
    *o++ = '@'; *o++ = '@'; *o++ = ALPHA[type];
    if (type == T_BLOB) { *o++ = 7; *o++ = 8; *o++ = 12; *o++ = 10; *o++ = 13; }
    else { *o++ = 8; *o++ = 7; *o++ = 12; *o++ = 10; *o++ = 13; }
    o = enc_u32(o, cb); o = enc_bytes(o, p, cb); *o = 0;
}
static void ref_user(char *out, int type, const WCHAR *u)
{
    char *o = out; unsigned n = (unsigned)lstrlenW(u) * 2;
    *o++ = '@'; *o++ = '@'; *o++ = ALPHA[type];
    o = enc_u32(o, n); o = enc_bytes(o, (const unsigned char *)u, n); *o = 0;
}
static void ref_cert(char *out, const unsigned char *h)
{
    char *o = out;
    *o++ = '@'; *o++ = '@'; *o++ = ALPHA[T_CERT];
    o = enc_bytes(o, h, 20); *o = 0;
}
static unsigned lcg(unsigned *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

/* marshal through W and A, compare both to the reference; returns the W string (caller CredFree) */
static void check_marshal(int type, void *cred, const char *want, const char *tag)
{
    WCHAR *w = NULL; char *a = NULL; char n[4096]; BOOL r;
    SET_ERR(0x5555);
    r = CMW((CRED_MARSHAL_TYPE)type, cred, &w);
    CHECKF(r && w, "%s: W failed err %lu", tag, GetLastError());
    if (r && w) {
        g7_narrow(w, n, sizeof n);
        CHECKF(!strcmp(n, want), "%s: W got \"%s\" want \"%s\"", tag, n, want);
        CHECKF(t_is_wine() || LocalSize(w) == (SIZE_T)(lstrlenW(w) + 1) * 2, "%s: LocalSize %llu vs strlen %d", tag, (unsigned long long)LocalSize(w), lstrlenW(w));
        CFREE(w);
    }
    r = CMA((CRED_MARSHAL_TYPE)type, cred, &a);
    CHECKF(r && a, "%s: A failed err %lu", tag, GetLastError());
    if (r && a) { CHECKF(!strcmp(a, want), "%s: A got \"%s\" want \"%s\"", tag, a, want); CFREE(a); }
}

/* ======================= binary blob types 3 and 5 ======================= */
TEST(gap7_cred_blob_marshal_matches_reference)
{
    WSKIP345();
    static const unsigned sizes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 31, 32, 33, 63, 64, 65, 100, 255, 256, 1000 };
    unsigned s, ty, seed = 1;
    REQUIRE(CMW); REQUIRE(CMA); REQUIRE(CFREE);
    for (ty = T_BLOB; ty <= T_SYSBLOB; ty += 2)
        for (s = 0; s < NELEM(sizes); s++) {
            unsigned char data[1024]; char want[4096], tag[64]; BLOBCRED b; unsigned i;
            for (i = 0; i < sizes[s]; i++) data[i] = (unsigned char)lcg(&seed);
            b.cbBlob = sizes[s]; b.pbBlob = data;
            ref_blob(want, ty, data, sizes[s]);
            wsprintfA(tag, "type %u cb %u", ty, sizes[s]);
            check_marshal(ty, &b, want, tag);
        }
}
TEST(gap7_cred_blob_marshal_fixed_vectors)
{
    WSKIP345();
    /* hand-computed: empty blob = prefix + length 0 (6 'A'); {0x41,0x42} -> length "CAAAAA", bytes "BJE" */
    BLOBCRED b; unsigned char d[2] = { 0x41, 0x42 };
    char want[128];
    REQUIRE(CMW); REQUIRE(CFREE);
    b.cbBlob = 0; b.pbBlob = d;
    sprintf(want, "@@D%c%c%c%c%cAAAAAA", 7, 8, 12, 10, 13);
    check_marshal(T_BLOB, &b, want, "empty type3");
    sprintf(want, "@@F%c%c%c%c%cAAAAAA", 8, 7, 12, 10, 13);
    check_marshal(T_SYSBLOB, &b, want, "empty type5");
    b.cbBlob = 2;
    sprintf(want, "@@D%c%c%c%c%cCAAAAABJE", 7, 8, 12, 10, 13);
    check_marshal(T_BLOB, &b, want, "2 byte type3");
}
TEST(gap7_cred_blob_prefix_differs_between_type3_and_type5)
{
    WSKIP345();
    BLOBCRED b; unsigned char d[4] = { 1, 2, 3, 4 }; WCHAR *w3 = NULL, *w5 = NULL;
    REQUIRE(CMW); REQUIRE(CFREE);
    b.cbBlob = 4; b.pbBlob = d;
    CHECK(CMW((CRED_MARSHAL_TYPE)3, &b, &w3)); CHECK(CMW((CRED_MARSHAL_TYPE)5, &b, &w5));
    if (w3 && w5) {
        CHECK(w3[2] == L'D'); CHECK(w5[2] == L'F');
        CHECK(w3[3] == 7 && w3[4] == 8 && w5[3] == 8 && w5[4] == 7);
        CHECK(!wcscmp(w3 + 5, w5 + 5));          /* identical tail */
        CHECK(lstrlenW(w3) == lstrlenW(w5));
    }
    if (w3) CFREE(w3);
    if (w5) CFREE(w5);
}
TEST(gap7_cred_blob_roundtrip_even_sizes)
{
    WSKIP345();
    unsigned cb, seed = 7, ty;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CUA); REQUIRE(CMA); REQUIRE(CFREE);
    for (ty = T_BLOB; ty <= T_SYSBLOB; ty += 2)
        for (cb = 2; cb <= 130; cb += (cb < 20 ? 2 : 14)) {
            unsigned char data[256]; BLOBCRED b; WCHAR *w = NULL; char *a = NULL; unsigned i; int k;
            for (i = 0; i < cb; i++) data[i] = (unsigned char)lcg(&seed);
            b.cbBlob = cb; b.pbBlob = data;
            if (!CMW((CRED_MARSHAL_TYPE)ty, &b, &w)) { CHECKF(0, "marshal ty %u cb %u", ty, cb); continue; }
            CHECK(CMA((CRED_MARSHAL_TYPE)ty, &b, &a));
            for (k = 0; k < 2; k++) {
                CRED_MARSHAL_TYPE rt = 0; BLOBCRED *u = NULL; BOOL r;
                SET_ERR(0x1234);
                r = k == 0 ? CUW(w, &rt, (void **)&u) : (a ? CUA(a, &rt, (void **)&u) : FALSE);
                CHECKF(r, "%s ty %u cb %u err %lu", k ? "A" : "W", ty, cb, GetLastError());
                if (!r) continue;
                CHECKF((unsigned)rt == ty, "type out %d", rt);
                CHECKF(u->cbBlob == cb, "cbBlob %lu want %u", u->cbBlob, cb);
                CHECKF(u->pbBlob && !memcmp(u->pbBlob, data, cb), "blob content cb %u", cb);
                CFREE(u);
            }
            CHECK(CIW(w));
            CFREE(w); if (a) CFREE(a);
        }
}
TEST(gap7_cred_blob_unmarshal_rejects_zero_and_odd_length)
{
    WSKIP345();
    static const unsigned sizes[] = { 0, 1, 3, 5, 7, 9, 101 };
    unsigned i, ty;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CIW); REQUIRE(CFREE);
    for (ty = T_BLOB; ty <= T_SYSBLOB; ty += 2)
        for (i = 0; i < NELEM(sizes); i++) {
            unsigned char data[128] = { 0 }; BLOBCRED b; WCHAR *w = NULL; CRED_MARSHAL_TYPE rt = 0; void *u = (void *)1;
            b.cbBlob = sizes[i]; b.pbBlob = data;
            CHECKF(CMW((CRED_MARSHAL_TYPE)ty, &b, &w), "marshal odd/zero %u must still succeed", sizes[i]);   /* marshal does not validate parity */
            if (!w) continue;
            SET_ERR(0);
            CHECKF(!CUW(w, &rt, &u), "ty %u cb %u unexpectedly unmarshaled", ty, sizes[i]);
            EXPECT_ERR(ERROR_INVALID_PARAMETER);
            CHECKF((unsigned)rt == ty, "type out %d", rt);
            CHECK(u == NULL);
            CHECK(!CIW(w));
            CFREE(w);
        }
}
TEST(gap7_cred_blob_unmarshal_requires_matching_prefix)
{
    WSKIP345();
    unsigned char d[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }; BLOBCRED b; WCHAR *w = NULL, *c;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CFREE);
    b.cbBlob = 8; b.pbBlob = d;
    CHECK(CMW((CRED_MARSHAL_TYPE)3, &b, &w));
    if (!w) return;
    c = (WCHAR *)LocalAlloc(LPTR, (lstrlenW(w) + 1) * 2);
    lstrcpyW(c, w);
    {   /* flip type letter to 'F' while keeping the type-3 prefix */
        CRED_MARSHAL_TYPE rt = 0; void *u = NULL;
        c[2] = L'F';
        SET_ERR(0); CHECK(!CUW(c, &rt, &u)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(rt, 5);
        c[2] = L'D';
        CHECK(CUW(c, &rt, &u)); if (u) CFREE(u);
        c[3] ^= 1; SET_ERR(0); u = NULL;
        CHECK(!CUW(c, &rt, &u)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        c[3] ^= 1;
    }
    LocalFree(c); CFREE(w);
}
TEST(gap7_cred_blob_unmarshal_requires_exact_length)
{
    WSKIP345();
    unsigned char d[10] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }; BLOBCRED b; WCHAR *w = NULL, tmp[512]; size_t n;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CFREE);
    b.cbBlob = 10; b.pbBlob = d;
    CHECK(CMW((CRED_MARSHAL_TYPE)3, &b, &w));
    if (!w) return;
    n = lstrlenW(w);
    {
        CRED_MARSHAL_TYPE rt; void *u = NULL;
        lstrcpyW(tmp, w);
        SET_ERR(0); tmp[n - 1] = 0; CHECK(!CUW(tmp, &rt, &u)); EXPECT_ERR(ERROR_INVALID_PARAMETER);     /* truncated by one char */
        tmp[n - 1] = w[n - 1]; tmp[n] = L'A'; tmp[n + 1] = 0;
        SET_ERR(0); CHECK(!CUW(tmp, &rt, &u)); EXPECT_ERR(ERROR_INVALID_PARAMETER);                       /* one extra char */
        tmp[n] = 0; tmp[10] = 0;
        SET_ERR(0); CHECK(!CUW(tmp, &rt, &u)); EXPECT_ERR(ERROR_INVALID_PARAMETER);                       /* cut inside the length */
        lstrcpyW(tmp, w);
        CHECK(CUW(tmp, &rt, &u)); if (u) CFREE(u);
    }
    CFREE(w);
}
TEST(gap7_cred_blob_marshal_invalid_arguments)
{
    WSKIP345();
    BLOBCRED b; unsigned char d[2] = { 1, 2 }; WCHAR *w = (WCHAR *)0xdeadbeef; char *a = (char *)0xdeadbeef; unsigned ty;
    REQUIRE(CMW); REQUIRE(CMA);
    b.cbBlob = 2; b.pbBlob = d;
    for (ty = T_BLOB; ty <= T_SYSBLOB; ty += 2) {
        SET_ERR(0); CHECK(!CMW((CRED_MARSHAL_TYPE)ty, NULL, &w)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(w, 0xdeadbeef);
        SET_ERR(0); CHECK(!CMA((CRED_MARSHAL_TYPE)ty, NULL, &a)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(a, 0xdeadbeef);
    }
}
TEST(gap7_cred_marshal_unsupported_type_values_leave_output_untouched)
{
    static const unsigned types[] = { 0, 6, 7, 8, 16, 63, 64, 255, 256, 0x10001, 0x7fffffff, 0x80000000u, 0xffffffffu };
    BLOBCRED b; unsigned char d[2] = { 1, 2 }; unsigned i;
    REQUIRE(CMW); REQUIRE(CMA);
    b.cbBlob = 2; b.pbBlob = d;
    for (i = 0; i < NELEM(types); i++) {
        WCHAR *w = (WCHAR *)0xdeadbeef; char *a = (char *)0xdeadbeef;
        SET_ERR(0); CHECKF(!CMW((CRED_MARSHAL_TYPE)types[i], &b, &w), "type 0x%x", types[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(w, 0xdeadbeef);
        SET_ERR(0); CHECKF(!CMA((CRED_MARSHAL_TYPE)types[i], &b, &a), "A type 0x%x", types[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(a, 0xdeadbeef);
    }
}

/* ======================= type 4: UsernameForPackedCredentials ======================= */
TEST(gap7_cred_packed_username_same_payload_as_type2)
{
    WSKIP345();
    static const WCHAR *names[] = { L"t", L"te", L"tes", L"test", L"user@example.com", L"DOMAIN\\user", L"\x00e9\x4e2d\xd83d\xde00" };
    unsigned i;
    REQUIRE(CMW); REQUIRE(CFREE);
    for (i = 0; i < NELEM(names); i++) {
        USERNAME_TARGET_CREDENTIAL_INFO u; WCHAR *w2 = NULL, *w4 = NULL; char want[512], tag[32];
        u.UserName = (LPWSTR)names[i];
        CHECK(CMW(UsernameTargetCredential, &u, &w2));
        CHECK(CMW((CRED_MARSHAL_TYPE)4, &u, &w4));
        if (w2 && w4) {
            CHECK(w2[2] == L'C' && w4[2] == L'E');
            CHECK(!wcscmp(w2 + 3, w4 + 3));
            CHECK(!wcsncmp(w4, L"@@", 2));
        }
        if (w2) CFREE(w2);
        if (w4) CFREE(w4);
        ref_user(want, T_PACKED, names[i]); wsprintfA(tag, "name %u", i);
        check_marshal(T_PACKED, &u, want, tag);
    }
}
TEST(gap7_cred_packed_username_roundtrip)
{
    WSKIP345();
    static const WCHAR *names[] = { L"a", L"ab", L"abc", L"abcd", L"Administrator", L"\x00e9\x00e8\x00ea", L"0123456789012345678901234567890123456789" };
    unsigned i;
    REQUIRE(CMW); REQUIRE(CUW); REQUIRE(CUA); REQUIRE(CMA); REQUIRE(CFREE);
    for (i = 0; i < NELEM(names); i++) {
        USERNAME_TARGET_CREDENTIAL_INFO u, *r = NULL; WCHAR *w = NULL; CRED_MARSHAL_TYPE rt = 0;
        u.UserName = (LPWSTR)names[i];
        CHECK(CMW((CRED_MARSHAL_TYPE)4, &u, &w));
        if (!w) continue;
        CHECKF(CUW(w, &rt, (void **)&r), "name %u err %lu", i, GetLastError());
        if (r) {
            EXPECT_EQ(rt, 4);
            CHECK(r->UserName && !wcscmp(r->UserName, names[i]));
            CFREE(r);
        }
        CHECK(CIW(w));
        CFREE(w);
    }
}
TEST(gap7_cred_packed_username_invalid)
{
    WSKIP345();
    USERNAME_TARGET_CREDENTIAL_INFO u; WCHAR *w = (WCHAR *)0xdeadbeef; char *a = (char *)0xdeadbeef;
    REQUIRE(CMW); REQUIRE(CMA);
    u.UserName = NULL;
    SET_ERR(0); CHECK(!CMW((CRED_MARSHAL_TYPE)4, &u, &w)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(w, 0xdeadbeef);
    u.UserName = (LPWSTR)L"";
    SET_ERR(0); CHECK(!CMA((CRED_MARSHAL_TYPE)4, &u, &a)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(a, 0xdeadbeef);
    SET_ERR(0); CHECK(!CMW((CRED_MARSHAL_TYPE)4, NULL, &w)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ======================= type 2 / cert: generated reference comparison ======================= */
TEST(gap7_cred_username_lengths_match_reference)
{
    WCHAR name[80]; unsigned n;
    REQUIRE(CMW); REQUIRE(CMA); REQUIRE(CFREE);
    for (n = 1; n <= 64; n++) {
        USERNAME_TARGET_CREDENTIAL_INFO u; char want[1024], tag[32]; unsigned i;
        for (i = 0; i < n; i++) name[i] = (WCHAR)(L'a' + (i * 7 + n) % 26);
        name[n] = 0; u.UserName = name;
        ref_user(want, T_USER, name); wsprintfA(tag, "len %u", n);
        check_marshal(T_USER, &u, want, tag);
        /* total characters: 3 + chars(4 bytes) + chars(2n bytes) */
        {
            WCHAR *w = NULL;
            if (CMW(UsernameTargetCredential, &u, &w) && w) {
                unsigned nb = 2 * n, chars = 3 + 6 + (nb / 3) * 4 + (nb % 3 ? nb % 3 + 1 : 0);
                CHECKF((unsigned)lstrlenW(w) == chars, "len %u strlen %d want %u", n, lstrlenW(w), chars);
                CFREE(w);
            }
        }
    }
}
TEST(gap7_cred_cert_generated_hashes_match_reference)
{
    unsigned seed = 99, k;
    REQUIRE(CMW); REQUIRE(CMA); REQUIRE(CUW); REQUIRE(CFREE);
    for (k = 0; k < 120; k++) {
        CERT_CREDENTIAL_INFO c; char want[128], tag[32]; unsigned i; WCHAR *w = NULL; CERT_CREDENTIAL_INFO *r = NULL; CRED_MARSHAL_TYPE rt = 0;
        memset(&c, 0, sizeof c); c.cbSize = sizeof c;
        for (i = 0; i < 20; i++) c.rgbHashOfCert[i] = (UCHAR)lcg(&seed);
        ref_cert(want, c.rgbHashOfCert); wsprintfA(tag, "hash %u", k);
        check_marshal(T_CERT, &c, want, tag);
        CHECK(CMW(CertCredential, &c, &w));
        if (w) {
            CHECK(lstrlenW(w) == 30);
            if (CUW(w, &rt, (void **)&r) && r) {
                EXPECT_EQ(rt, 1); CHECK(r->cbSize == sizeof *r); CHECK(!memcmp(r->rgbHashOfCert, c.rgbHashOfCert, 20));
                CFREE(r);
            } else CHECKF(0, "unmarshal hash %u", k);
            CFREE(w);
        }
    }
}
TEST(gap7_cred_cert_cbsize_boundaries)
{
    CERT_CREDENTIAL_INFO c; static const unsigned sizes[] = { 0, 1, 4, 23, 24, 25, 28, 100, 0xffffffffu };
    unsigned i;
    REQUIRE(CMW);
    memset(&c, 0, sizeof c);
    for (i = 0; i < NELEM(sizes); i++) {
        WCHAR *w = (WCHAR *)0xdeadbeef; BOOL r;
        c.cbSize = sizes[i];
        SET_ERR(0); r = CMW(CertCredential, &c, &w);
        if (sizes[i] < 24) { CHECKF(!r, "cbSize %u must fail", sizes[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(w, 0xdeadbeef); }
        else { CHECKF(r, "cbSize %u must succeed", sizes[i]); if (r && w) CFREE(w); }
    }
}

/* ======================= unmarshal parser ======================= */
static void make_cert_str(char *s, unsigned char fill) { unsigned char h[20]; memset(h, fill, 20); ref_cert(s, h); }

TEST(gap7_cred_unmarshal_type_char_sets_type_even_on_failure)
{
    unsigned t;
    REQUIRE(CUW);
    for (t = 0; t < 64; t++) {
        char s[128]; WCHAR w[128]; CRED_MARSHAL_TYPE rt = (CRED_MARSHAL_TYPE)0x77; void *u = (void *)1; BOOL r;
        make_cert_str(s, 0); s[2] = ALPHA[t]; g7_widen(s, w, 128);
        SET_ERR(0); r = CUW(w, &rt, &u);
        if (t == 1) { CHECK(r); if (r && u) CFREE(u); }
        else {
            CHECKF(!r, "type char %u unexpectedly accepted", t);
            if (!t_is_wine()) { EXPECT_ERR(ERROR_INVALID_PARAMETER); CHECK(u == NULL); }   /* wine neither sets the error nor clears the output */
        }
        CHECKF((unsigned)rt == t, "type %u: out %d", t, rt);
    }
}
TEST(gap7_cred_unmarshal_every_alphabet_char_accepted_in_payload)
{
    unsigned c, pos;
    REQUIRE(CUW); REQUIRE(CUA); REQUIRE(CIW);
    for (pos = 3; pos <= 12; pos += 9)
        for (c = 0; c < 64; c++) {
            char s[128]; WCHAR w[128]; CRED_MARSHAL_TYPE rt = 0; void *u = NULL;
            make_cert_str(s, 0x5a); s[pos] = ALPHA[c]; g7_widen(s, w, 128);
            CHECKF(CUW(w, &rt, &u), "alphabet char %u at %u rejected", c, pos);
            if (u) CFREE(u);
            u = NULL; CHECK(CUA(s, &rt, &u)); if (u) CFREE(u);
            CHECK(CIW(w));
        }
}
TEST(gap7_cred_unmarshal_non_alphabet_chars_rejected_in_payload)
{
    unsigned c, bad = 0;
    REQUIRE(CUW); REQUIRE(CIW);
    for (c = 1; c < 0x180; c++) {
        char s[128]; WCHAR w[128]; CRED_MARSHAL_TYPE rt = 0; void *u = NULL; BOOL r; int in_alpha = c < 128 && strchr(ALPHA, (int)c);
        make_cert_str(s, 0x33); g7_widen(s, w, 128);
        w[10] = (WCHAR)c;
        r = CUW(w, &rt, &u);
        if (u) CFREE(u);
        if (!!r != !!in_alpha) { bad++; CHECKF(0, "char 0x%x: unmarshal %d want %d", c, r, in_alpha); }
        CHECK(!!CIW(w) == !!in_alpha);
    }
    EXPECT_EQ(bad, 0);
    {   /* a NUL inside the string ends it early, leaving a short cert payload */
        char s[128]; WCHAR w[128]; CRED_MARSHAL_TYPE rt; void *u = NULL;
        make_cert_str(s, 0x33); g7_widen(s, w, 128); w[12] = 0;
        SET_ERR(0); CHECK(!CUW(w, &rt, &u)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap7_cred_unmarshal_requires_double_at_prefix)
{
    static const char *const hdr[] = { "@A", "A@", "AA", "@#", " @", "@ ", "@\x01", "##" };
    unsigned i;
    REQUIRE(CUA);
    for (i = 0; i < NELEM(hdr); i++) {
        char s[128]; CRED_MARSHAL_TYPE rt = 0; void *u = NULL; BOOL r;
        make_cert_str(s, 1); s[0] = hdr[i][0]; s[1] = hdr[i][1];
        SET_ERR(0); r = CUA(s, &rt, &u);
        CHECKF(!r, "header \"%s\"", hdr[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(rt, 0);
        if (r && u) CFREE(u);
    }
}
TEST(gap7_cred_unmarshal_a_w_parity_on_generated_strings)
{
    unsigned seed = 5, k;
    REQUIRE(CUA); REQUIRE(CUW); REQUIRE(CIW);
    for (k = 0; k < 300; k++) {
        char s[160]; WCHAR w[160]; unsigned n = lcg(&seed) % 40, i; CRED_MARSHAL_TYPE ta = 0, tw = 0; void *pa = NULL, *pw = NULL; BOOL ra, rw;
        s[0] = '@'; s[1] = '@'; s[2] = ALPHA[lcg(&seed) % 7];
        for (i = 0; i < n; i++) s[3 + i] = (lcg(&seed) % 10) ? ALPHA[lcg(&seed) % 64] : (char)(0x21 + lcg(&seed) % 90);
        s[3 + n] = 0; g7_widen(s, w, 160);
        SET_ERR(0x600D); ra = CUA(s, &ta, &pa); { DWORD ea = GetLastError();
        SET_ERR(0x600D); rw = CUW(w, &tw, &pw); { DWORD ew = GetLastError();
        CHECKF(!!ra == !!rw, "\"%s\" A=%d W=%d", s, ra, rw);
        CHECKF(ta == tw, "\"%s\" type A=%d W=%d", s, ta, tw);
        if (!ra && !rw) CHECKF(ea == ew, "\"%s\" error A=%lu W=%lu", s, ea, ew);
        /* decomp: CredIsMarshaledCredentialW == (CredUnmarshalCredentialW succeeded); wine's is a looser header check */
        if (!t_is_wine()) CHECK(!!CIW(w) == !!rw); } }
        if (pa) CFREE(pa);
        if (pw) CFREE(pw);
    }
}
TEST(gap7_cred_unmarshal_user_length_field_must_match_and_be_even)
{
    /* type 2 payload = u32 length then bytes: length must be even, non-zero, and exactly consume the string */
    static const unsigned lens[] = { 0, 1, 2, 3, 4, 6, 8 };
    unsigned i;
    REQUIRE(CUW);
    for (i = 0; i < NELEM(lens); i++) {
        unsigned char bytes[8] = { 'a', 0, 'b', 0, 'c', 0, 'd', 0 }; char s[128], *o = s; WCHAR w[128]; CRED_MARSHAL_TYPE rt = 0; void *u = NULL; BOOL r;
        *o++ = '@'; *o++ = '@'; *o++ = 'C';
        o = enc_u32(o, lens[i]); o = enc_bytes(o, bytes, 4); *o = 0;     /* always carries 4 payload bytes */
        g7_widen(s, w, 128);
        SET_ERR(0); r = CUW(w, &rt, &u);
        if (lens[i] == 4) { CHECK(r); if (r && u) { CHECK(!wcscmp(((USERNAME_TARGET_CREDENTIAL_INFO *)u)->UserName, L"ab")); CFREE(u); } }
        else { CHECKF(!r, "declared length %u with 4 bytes", lens[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    }
}

/* ======================= CredIsMarshaledCredential ======================= */
TEST(gap7_cred_ismarshaled_agrees_with_generated_strings)
{
    WSKIP345();
    unsigned char d[6] = { 1, 2, 3, 4, 5, 6 }, odd[5] = { 1, 2, 3, 4, 5 }; char s[256]; WCHAR w[256]; unsigned n;
    REQUIRE(CIW);
    ref_blob(s, T_BLOB, d, 6); g7_widen(s, w, 256); CHECK(CIW(w)); if (CIA) CHECK(CIA(s));
    ref_blob(s, T_SYSBLOB, d, 6); g7_widen(s, w, 256); CHECK(CIW(w));
    ref_blob(s, T_BLOB, odd, 5); g7_widen(s, w, 256); CHECK(!CIW(w)); if (CIA) CHECK(!CIA(s));
    ref_user(s, T_PACKED, L"name"); g7_widen(s, w, 256); CHECK(CIW(w)); if (CIA) CHECK(CIA(s));
    ref_user(s, T_USER, L"name"); g7_widen(s, w, 256); CHECK(CIW(w));
    /* every proper prefix of a valid string is not a marshaled credential, except where it happens to be complete */
    ref_user(s, T_USER, L"ab"); n = (unsigned)strlen(s);
    for (unsigned k = 0; k < n; k++) { char t[256]; WCHAR tw[256]; memcpy(t, s, k); t[k] = 0; g7_widen(t, tw, 256); CHECKF(!CIW(tw), "prefix of %u chars", k); }
    g7_widen(s, w, 256); CHECK(CIW(w));
}
TEST(gap7_cred_ismarshaled_does_not_modify_last_error_on_success)
{
    WCHAR w[64]; char s[64];
    REQUIRE(CIW);
    make_cert_str(s, 0); g7_widen(s, w, 64);
    SET_ERR(0x4321);
    CHECK(CIW(w));
    t_note("GetLastError after CredIsMarshaledCredentialW success: %lu", GetLastError());
}

/* ======================= forwarding from advapi32 ======================= */
TEST(gap7_cred_advapi32_and_sechost_agree)
{
    CERT_CREDENTIAL_INFO c; WCHAR *a = NULL, *b = NULL;
    __typeof__(&CredMarshalCredentialW) adv = UIN("advapi32.dll", CredMarshalCredentialW);
    REQUIRE(CMW); REQUIRE(adv);
    memset(&c, 0, sizeof c); c.cbSize = sizeof c; memset(c.rgbHashOfCert, 0xa5, 20);
    CHECK(CMW(CertCredential, &c, &a)); CHECK(adv(CertCredential, &c, &b));
    if (a && b) CHECK(!wcscmp(a, b));
    if (a) CFREE(a);
    if (b) CFREE(b);
    t_note("advapi32!CredMarshalCredentialW %s sechost's address", (FARPROC)adv == (FARPROC)CMW ? "is" : "is not");
}
