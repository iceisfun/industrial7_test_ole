#include "g1.h"
/* CredProtect/CredUnprotect/CredIsProtected (A and W).
 * Decomp facts: CredProtectW/UnprotectW are thin wrappers over CredProtectEx/CredUnprotectEx (fAsSelf != 0 -> 1).
 * Buffer protocol: *pcchMaxChars is the capacity in characters INCLUDING the terminating NUL of the protected string;
 * a too small buffer yields STATUS_BUFFER_TOO_SMALL -> ERROR_INSUFFICIENT_BUFFER with the required size stored back.
 * Unprotect of a string that is not a protected credential hands the input back unchanged (cch characters). */

#define CPW CR(CredProtectW)
#define CPA CR(CredProtectA)
#define CUW CR(CredUnprotectW)
#define CUA CR(CredUnprotectA)
#define CIW CR(CredIsProtectedW)
#define CIA CR(CredIsProtectedA)

static int have(void) { if (t_is_wine()) { t_skip("CredProtect family is not implemented by the emulator"); return 0; } if (!CPW || !CUW || !CIW || !CPA || !CUA || !CIA) { t_skip("missing CredProtect export"); return 0; } return 1; }

/* Protect `n` characters of `src`; returns malloc'd protected string or NULL, size in *needed. */
static WCHAR *protect_w(BOOL self, const WCHAR *src, DWORD n, DWORD *needed, CRED_PROTECTION_TYPE *type)
{
    DWORD cap = 0; WCHAR *out; BOOL r;
    SET_ERR(0); r = CPW(self, (WCHAR *)src, n, NULL, &cap, type);
    if (r || GetLastError() != ERROR_INSUFFICIENT_BUFFER || cap == 0) return NULL;
    out = calloc(cap + 4, sizeof(WCHAR));
    *needed = cap;
    r = CPW(self, (WCHAR *)src, n, out, &cap, type);
    if (!r) { free(out); return NULL; }
    return out;
}

TEST(gap1_protect_w_size_query)
{
    CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55; DWORD cap = 0; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"secret-pw", 10, NULL, &cap, &t);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(cap > 10);                          /* marshaled form is longer than the plaintext */
    EXPECT_EQ(t, 0x55);                       /* untouched on failure */
    t_note("protected length for 10 chars: %lu", cap);
}

TEST(gap1_protect_w_roundtrip)
{
    DWORD need = 0, cap, i; CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55, t2 = (CRED_PROTECTION_TYPE)0x55; WCHAR *p, out[64];
    BOOL r;
    if (!have()) return;
    p = protect_w(TRUE, L"secret-pw", 10, &need, &t);
    CHECK(p != NULL); if (!p) return;
    CHECKF(t == CredUserProtection || t == CredTrustedProtection, "type %d", t);
    EXPECT_EQ(wcslen(p), need - 1);           /* the capacity reported counts the NUL */
    CHECK(p[0] == L'@' && p[1] == L'@');
    /* Win10 emits the marshaled credential as arbitrary WCHARs after the "@@" prefix (not restricted to printable ASCII) */
    for (i = 2; i < need - 1; i++) CHECKF(p[i] != 0, "embedded NUL at %lu", i);
    /* IsProtected agrees */
    SET_ERR(0); r = CIW(p, &t2); CHECK(r); EXPECT_EQ(t2, t);
    /* Unprotect: size query, too small, exact */
    cap = 0; SET_ERR(0); r = CUW(TRUE, p, need, NULL, &cap);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, 10);
    for (i = 0; i < 64; i++) out[i] = L'X';
    cap = 9; SET_ERR(0); r = CUW(TRUE, p, need, out, &cap);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, 10); CHECK(out[0] == L'X');
    cap = 10; SET_ERR(0); r = CUW(TRUE, p, need, out, &cap);
    CHECK(r); EXPECT_EQ(cap, 10); EXPECT_WSTR(out, L"secret-pw"); CHECK(out[10] == L'X');
    cap = 64; r = CUW(TRUE, p, need, out, &cap);
    CHECK(r); EXPECT_EQ(cap, 10);             /* capacity is replaced by the length actually produced */
    free(p);
}

TEST(gap1_protect_w_exact_and_short_buffer)
{
    DWORD need = 0, cap, k; WCHAR *p, *buf; CRED_PROTECTION_TYPE t; BOOL r;
    if (!have()) return;
    p = protect_w(TRUE, L"abc", 4, &need, &t);
    CHECK(p != NULL); if (!p) return;
    buf = malloc((need + 8) * sizeof(WCHAR));
    for (k = 0; k < 4; k++) {                 /* capacities need-1 .. need+2 */
        DWORD c = need - 1 + k, i;
        for (i = 0; i < need + 8; i++) buf[i] = 0x5a5a;
        cap = c; SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"abc", 4, buf, &cap, &t);
        if (k == 0) { CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, need); CHECK(buf[0] == 0x5a5a); }
        else { CHECKF(r, "capacity %lu", c); EXPECT_EQ(cap, need); CHECK(wcslen(buf) == need - 1); CHECK(buf[need] == 0x5a5a); }
    }
    free(buf); free(p);
}

TEST(gap1_protect_w_fasself_variants)
{
    static const BOOL v[] = { FALSE, TRUE, 2, (BOOL)0x80000000 };
    unsigned i;
    if (!have()) return;
    for (i = 0; i < 4; i++) {
        DWORD need = 0, cap; CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55; WCHAR *p, out[16]; BOOL r;
        p = protect_w(v[i], L"fas", 4, &need, &t);
        CHECKF(p != NULL, "fAsSelf %x", v[i]); if (!p) continue;
        cap = 16; SET_ERR(0); r = CUW(v[i], p, need, out, &cap);
        CHECKF(r, "fAsSelf %x unprotect err %lu", v[i], GetLastError());
        if (r) { EXPECT_EQ(cap, 4); EXPECT_WSTR(out, L"fas"); }
        t_note("fAsSelf %x -> protection type %d", v[i], t);
        free(p);
    }
}

TEST(gap1_protect_w_length_matrix)
{
    static const DWORD lens[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 100, 127, 128, 255, 256, 300 };
    unsigned i;
    if (!have()) return;
    for (i = 0; i < sizeof lens / sizeof *lens; i++) {
        DWORD n = lens[i], need = 0, cap, j; WCHAR *src = malloc((n + 1) * 2), *out = malloc((n + 8) * 2), *p; CRED_PROTECTION_TYPE t; BOOL r;
        for (j = 0; j < n; j++) src[j] = (WCHAR)(L'a' + (j * 7) % 26);
        src[n] = 0;
        p = protect_w(TRUE, src, n, &need, &t);        /* n characters, no terminator */
        CHECKF(p != NULL, "len %lu", n);
        if (p) {
            cap = n + 8; for (j = 0; j < n + 8; j++) out[j] = L'#';
            SET_ERR(0); r = CUW(TRUE, p, need, out, &cap);
            CHECKF(r, "len %lu err %lu", n, GetLastError());
            if (r) { EXPECT_EQ(cap, n); CHECKF(!memcmp(out, src, n * 2), "len %lu mismatch", n); CHECKF(out[n] == L'#', "len %lu wrote terminator", n); }
            free(p);
        }
        free(src); free(out);
    }
}

TEST(gap1_protect_w_invalid_args)
{
    DWORD cap = 100, need; WCHAR out[200]; CRED_PROTECTION_TYPE t; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CPW(TRUE, NULL, 4, out, &cap, &t);           CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"abc", 4, out, NULL, &t); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 100; SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"abc", 4, NULL, &cap, &t); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 100; SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"abc", 4, NULL, &cap, NULL); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* ProtectionType is optional */
    need = 0; cap = 0; SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"abc", 4, NULL, &cap, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); need = cap;
    cap = need; SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"abc", 4, out, &cap, NULL);
    CHECK(r); EXPECT_EQ(cap, need);
}

TEST(gap1_protect_zero_length_note)
{
    DWORD cap = 0; BOOL r; WCHAR out[8]; CRED_PROTECTION_TYPE t;
    if (!have()) return;
    SET_ERR(0); r = CPW(TRUE, (WCHAR *)L"", 0, NULL, &cap, &t);
    t_note("CredProtectW cch=0: ret %d err %lu cap %lu", r, GetLastError(), cap);
    SET_ERR(0); cap = 8; r = CPW(TRUE, (WCHAR *)L"", 1, out, &cap, &t);
    t_note("CredProtectW empty string cch=1: ret %d err %lu cap %lu", r, GetLastError(), cap);
}

TEST(gap1_unprotect_w_plain_passthrough)
{
    static const WCHAR *const s[] = { L"plain", L"a", L"hello world", L"not@@protected" };
    unsigned i;
    if (!have()) return;
    for (i = 0; i < sizeof s / sizeof *s; i++) {
        DWORD n = (DWORD)wcslen(s[i]) + 1, cap, j; WCHAR out[40]; BOOL r;
        for (j = 0; j < 40; j++) out[j] = L'#';
        cap = 40; SET_ERR(0); r = CUW(TRUE, (WCHAR *)s[i], n, out, &cap);
        CHECKF(r, "'%ls' err %lu", s[i], GetLastError());
        if (r) { EXPECT_EQ(cap, n); EXPECT_WSTR(out, s[i]); }
        cap = n - 1; SET_ERR(0); r = CUW(TRUE, (WCHAR *)s[i], n, out, &cap);
        CHECKF(!r, "'%ls' short buffer", s[i]); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, n);
        cap = 0; SET_ERR(0); r = CUW(TRUE, (WCHAR *)s[i], n, NULL, &cap);
        CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, n);
    }
}

TEST(gap1_unprotect_w_invalid_args)
{
    DWORD cap = 10; WCHAR out[16]; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CUW(TRUE, NULL, 5, out, &cap);                        CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 10; SET_ERR(0); r = CUW(TRUE, (WCHAR *)L"abc", 4, NULL, &cap);   CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 10; SET_ERR(0); r = CUW(TRUE, NULL, 0, NULL, &cap);              CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap1_is_protected_w)
{
    /* NULL arguments: STATUS_INVALID_PARAMETER -> ERROR_INVALID_PARAMETER */
    CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CIW(NULL, &t); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(t, 0x55);
    SET_ERR(0); r = CIW((WCHAR *)L"x", NULL); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = CIW(NULL, NULL); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap1_is_protected_unprotected_strings)
{
    static const WCHAR *const s[] = { L"", L"hello", L"@@", L"@@Z", L"@@B", L"@@BA", L"@@?????", L" @@", L"@@C" };
    unsigned i;
    if (!have()) return;
    for (i = 0; i < sizeof s / sizeof *s; i++) {
        CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55; BOOL r;
        SET_ERR(0); r = CIW((WCHAR *)s[i], &t);
        CHECKF(r, "'%ls' err %lu", s[i], GetLastError());
        CHECKF(t == CredUnprotected, "'%ls' type %d", s[i], t);
    }
}

TEST(gap1_is_protected_other_marshaled_credentials)
{
    /* a marshaled certificate / username-target credential is a valid "@@" string but not a protected one */
    CERT_CREDENTIAL_INFO ci; CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55; WCHAR *m = NULL; BOOL r;
    typedef BOOL (WINAPI *mar_fn)(CRED_MARSHAL_TYPE, PVOID, LPWSTR *);
    mar_fn mar = (mar_fn)g1_cred("CredMarshalCredentialW");
    if (!have() || !mar || !CR(CredFree)) { t_skip("missing CredMarshalCredentialW"); return; }
    memset(&ci, 0, sizeof ci); ci.cbSize = sizeof ci; memset(ci.rgbHashOfCert, 0x11, 20);
    SET_ERR(0); r = mar(CertCredential, &ci, &m);
    CHECK(r); if (!r) return;
    SET_ERR(0); r = CIW(m, &t); CHECK(r); EXPECT_EQ(t, CredUnprotected);
    CR(CredFree)(m);
}

TEST(gap1_is_protected_a_parity)
{
    static const char *const s[] = { "", "hello", "@@", "@@Z" };
    unsigned i; CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55; BOOL r;
    if (!have()) return;
    for (i = 0; i < 4; i++) {
        t = (CRED_PROTECTION_TYPE)0x55; SET_ERR(0); r = CIA((char *)s[i], &t);
        CHECKF(r, "'%s' err %lu", s[i], GetLastError()); EXPECT_EQ(t, CredUnprotected);
    }
    SET_ERR(0); r = CIA(NULL, &t); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = CIA((char *)"x", NULL); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap1_protect_a_roundtrip)
{
    DWORD cap = 0, need, i; char *p, out[32]; CRED_PROTECTION_TYPE t = (CRED_PROTECTION_TYPE)0x55, t2 = (CRED_PROTECTION_TYPE)0x55; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CPA(TRUE, (char *)"secret", 7, NULL, &cap, &t);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); CHECK(cap > 7); EXPECT_EQ(t, 0x55);
    need = cap; p = calloc(need + 4, 1);
    SET_ERR(0); r = CPA(TRUE, (char *)"secret", 7, p, &cap, &t);
    CHECK(r); EXPECT_EQ(cap, need);
    CHECKF(t == CredUserProtection || t == CredTrustedProtection, "type %d", t);
    EXPECT_EQ(strlen(p), need - 1); CHECK(p[0] == '@' && p[1] == '@');
    SET_ERR(0); r = CIA(p, &t2); CHECK(r); EXPECT_EQ(t2, t);
    for (i = 0; i < 32; i++) out[i] = '#';
    cap = 0; SET_ERR(0); r = CUA(TRUE, p, need, NULL, &cap);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, 7);
    cap = 6; SET_ERR(0); r = CUA(TRUE, p, need, out, &cap);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, 7);
    cap = 7; SET_ERR(0); r = CUA(TRUE, p, need, out, &cap);
    CHECKF(r, "err %lu", GetLastError()); EXPECT_EQ(cap, 7);
    if (r) { EXPECT_STR(out, "secret"); CHECK(out[7] == '#'); }
    /* a W caller can unprotect what the A function produced (the protected form is plain ASCII) */
    {
        WCHAR wp[256], wo[16]; DWORD k;
        for (k = 0; k < need; k++) wp[k] = (WCHAR)(unsigned char)p[k];
        wp[need] = 0; cap = 16; SET_ERR(0);
        r = CUW(TRUE, wp, need, wo, &cap);
        CHECKF(r, "W unprotect of A-protected: err %lu", GetLastError());
        if (r) { EXPECT_EQ(cap, 7); EXPECT_WSTR(wo, L"secret"); }
    }
    free(p);
}

TEST(gap1_protect_a_invalid_args)
{
    DWORD cap = 100; char out[200]; CRED_PROTECTION_TYPE t; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CPA(TRUE, NULL, 4, NULL, &cap, &t); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 100; SET_ERR(0); r = CPA(TRUE, (char *)"abc", 4, NULL, &cap, &t); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 0; SET_ERR(0); r = CUA(TRUE, NULL, 4, out, &cap); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cap = 5; SET_ERR(0); r = CUA(TRUE, (char *)"abc", 4, NULL, &cap); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap1_unprotect_a_plain_passthrough)
{
    DWORD cap = 0; char out[16]; BOOL r;
    if (!have()) return;
    SET_ERR(0); r = CUA(TRUE, (char *)"plain", 6, NULL, &cap);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(cap, 6);
    memset(out, '#', sizeof out); cap = 16; SET_ERR(0); r = CUA(TRUE, (char *)"plain", 6, out, &cap);
    CHECKF(r, "err %lu", GetLastError()); if (r) { EXPECT_EQ(cap, 6); EXPECT_STR(out, "plain"); }
}
