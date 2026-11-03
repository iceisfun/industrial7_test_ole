/* combase HSTRING operations checked exhaustively against a straightforward reference model:
 * every string over a tiny alphabet up to length 5 is pushed through each operation and compared with what the
 * operation is documented (and decompiled) to do. These sweeps complement the hand written tables of winrt_hstring. */
#include "gap8.h"

#define WCS   U(WindowsCreateString)
#define WCSR  U(WindowsCreateStringReference)
#define WDEL  U(WindowsDeleteString)
#define WDUP  U(WindowsDuplicateString)
#define WLEN  U(WindowsGetStringLen)
#define WRAW  U(WindowsGetStringRawBuffer)
#define WEMB  U(WindowsStringHasEmbeddedNull)
#define WCMP  U(WindowsCompareStringOrdinal)
#define WCAT  U(WindowsConcatString)
#define WSUB  U(WindowsSubstring)
#define WSUBL U(WindowsSubstringWithSpecifiedLength)
#define WREP  U(WindowsReplaceString)
#define WTRS  U(WindowsTrimStringStart)
#define WTRE  U(WindowsTrimStringEnd)
#define WPRE  U(WindowsPreallocateStringBuffer)
#define WPRO  U(WindowsPromoteStringBuffer)
#define WDELB U(WindowsDeleteStringBuffer)

#define NEED_CORE() do { REQUIRE(WCS); REQUIRE(WDEL); REQUIRE(WLEN); REQUIRE(WRAW); REQUIRE(WCSR); } while (0)
#define SENT ((HSTRING)(ULONG_PTR)0x1234)

typedef struct { WCHAR s[8]; UINT32 n; } S;
/* all strings over `alpha` of length 0..maxlen */
static int gen(S *out, const WCHAR *alpha, int maxlen)
{
    int na = (int)wcslen(alpha), cnt = 0, len, i, k;
    for (len = 0; len <= maxlen; len++) {
        long total = 1; for (i = 0; i < len; i++) total *= na;
        for (k = 0; k < total; k++) {
            long v = k; memset(&out[cnt], 0, sizeof out[cnt]); out[cnt].n = len;
            for (i = len - 1; i >= 0; i--) { out[cnt].s[i] = alpha[v % na]; v /= na; }
            cnt++;
        }
    }
    return cnt;
}
static HSTRING mk(const S *s) { HSTRING h = NULL; if (s->n && WCS(s->s, s->n, &h) != S_OK) return NULL; return h; }
/* verify an HSTRING against content; exact length, content, NUL terminator, NULL iff empty */
static int same(HSTRING h, const WCHAR *c, UINT32 n, const char *what, const S *ctx)
{
    UINT32 len = 0xffff; const WCHAR *b = WRAW(h, &len); int ok = 1;
    if (len != n) ok = 0; else if (n && memcmp(b, c, n * 2)) ok = 0;
    else if (b[n] != 0) ok = 0;
    if ((h == NULL) != (n == 0)) ok = 0;
    if (WLEN(h) != n) ok = 0;
    if (!ok) t_check(0, __FILE__, __LINE__, what, "len %u (want %u) for input of length %u", len, n, ctx ? ctx->n : 0);
    else t_check(1, __FILE__, __LINE__, what, NULL);
    return ok;
}

static S all2[200], all3[200];

TEST(gap8_hs_sweep_substring_matches_model)
{
    int n, i; UINT32 st; NEED_CORE(); REQUIRE(WSUB);
    n = gen(all2, L"ab", 5);
    for (i = 0; i < n; i++) {
        HSTRING h = mk(&all2[i]);
        for (st = 0; st <= all2[i].n + 2; st++) {
            HSTRING r = SENT; HRESULT hr = WSUB(h, st, &r);
            if (st > all2[i].n) { G8_EQ(hr, G8_E_BOUNDS); CHECK(r == NULL); }
            else { G8_EQ(hr, S_OK); same(r, all2[i].s + st, all2[i].n - st, "substring content", &all2[i]); WDEL(r); }
        }
        WDEL(h);
    }
}

TEST(gap8_hs_sweep_substring_with_length_matches_model)
{
    int n, i; UINT32 st, ln; NEED_CORE(); REQUIRE(WSUBL);
    n = gen(all2, L"ab", 5);
    for (i = 0; i < n; i++) {
        HSTRING h = mk(&all2[i]); UINT32 L = all2[i].n;
        for (st = 0; st <= L + 1; st++)
            for (ln = 0; ln <= L + 1; ln++) {
                HSTRING r = SENT; HRESULT hr = WSUBL(h, st, ln, &r);
                if (st > L || st + ln > L) { G8_EQ(hr, G8_E_BOUNDS); CHECK(r == NULL); }
                else { G8_EQ(hr, S_OK); same(r, all2[i].s + st, ln, "substring-with-length content", &all2[i]); WDEL(r); }
            }
        WDEL(h);
    }
}

static int in_set(WCHAR c, const WCHAR *set) { for (; *set; set++) if (*set == c) return 1; return 0; }

TEST(gap8_hs_sweep_trim_matches_model)
{
    static const WCHAR *const sets[] = { L"a", L"b", L"ab", L"ba", L"c", L"cb", L"aa" };
    int n, i; unsigned k; NEED_CORE(); REQUIRE(WTRS); REQUIRE(WTRE);
    n = gen(all3, L"abc", 4);
    for (i = 0; i < n; i++) {
        HSTRING h = mk(&all3[i]);
        for (k = 0; k < sizeof sets / sizeof *sets; k++) {
            HSTRING set = NULL, r = SENT; UINT32 b = 0, e = all3[i].n;
            WCS(sets[k], (UINT32)wcslen(sets[k]), &set);
            while (b < e && in_set(all3[i].s[b], sets[k])) b++;
            G8_EQ(WTRS(h, set, &r), S_OK); same(r, all3[i].s + b, e - b, "TrimStart", &all3[i]); WDEL(r);
            b = 0; while (e > b && in_set(all3[i].s[e - 1], sets[k])) e--;
            r = SENT;
            G8_EQ(WTRE(h, set, &r), S_OK); same(r, all3[i].s, e - b, "TrimEnd", &all3[i]); WDEL(r);
            WDEL(set);
        }
        WDEL(h);
    }
}

static UINT32 model_replace(const WCHAR *s, UINT32 n, const WCHAR *f, UINT32 fn, const WCHAR *w, UINT32 wn, WCHAR *out)
{
    UINT32 i = 0, o = 0;
    while (i < n) {
        if (i + fn <= n && !memcmp(s + i, f, fn * 2)) { memcpy(out + o, w, wn * 2); o += wn; i += fn; }
        else out[o++] = s[i++];
    }
    return o;
}

TEST(gap8_hs_sweep_replace_matches_model)
{
    static const WCHAR *const finds[] = { L"a", L"b", L"ab", L"ba", L"aa", L"bb", L"aba" };
    static const WCHAR *const reps[] = { L"", L"x", L"xy", L"a", L"b", L"aa", L"bab" };
    int n, i; unsigned fi, ri; NEED_CORE(); REQUIRE(WREP);
    if (t_is_wine()) { t_skip("emulator WindowsReplaceString is an aborting stub"); return; }
    n = gen(all2, L"ab", 5);
    for (i = 0; i < n; i++) {
        HSTRING h = mk(&all2[i]);
        for (fi = 0; fi < sizeof finds / sizeof *finds; fi++)
            for (ri = 0; ri < sizeof reps / sizeof *reps; ri++) {
                HSTRING f = NULL, w = NULL, r = SENT; WCHAR exp[64]; UINT32 en;
                WCS(finds[fi], (UINT32)wcslen(finds[fi]), &f);
                if (reps[ri][0]) WCS(reps[ri], (UINT32)wcslen(reps[ri]), &w);
                en = model_replace(all2[i].s, all2[i].n, finds[fi], (UINT32)wcslen(finds[fi]), reps[ri], (UINT32)wcslen(reps[ri]), exp);
                G8_EQ(WREP(h, f, w, &r), S_OK);
                same(r, exp, en, "Replace", &all2[i]);
                WDEL(r); WDEL(f); WDEL(w);
            }
        WDEL(h);
    }
}

TEST(gap8_hs_sweep_concat_matches_model)
{
    int n, i, j; NEED_CORE(); REQUIRE(WCAT);
    n = gen(all2, L"ab", 3);
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) {
        HSTRING a = mk(&all2[i]), b = mk(&all2[j]), r = SENT; WCHAR exp[16];
        memcpy(exp, all2[i].s, all2[i].n * 2); memcpy(exp + all2[i].n, all2[j].s, all2[j].n * 2);
        G8_EQ(WCAT(a, b, &r), S_OK);
        same(r, exp, all2[i].n + all2[j].n, "Concat", &all2[i]);
        WDEL(r); WDEL(a); WDEL(b);
    }
}

TEST(gap8_hs_sweep_compare_matches_ordinal_order)
{
    int n, i, j; NEED_CORE(); REQUIRE(WCMP);
    n = gen(all3, L"abC", 3);                                   /* 'C' (0x43) sorts before 'a' ordinally */
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) {
        HSTRING a = mk(&all3[i]), b = mk(&all3[j]); INT32 r = 77; int want, k, m = all3[i].n < all3[j].n ? all3[i].n : all3[j].n;
        want = 0;
        for (k = 0; k < m && !want; k++) if (all3[i].s[k] != all3[j].s[k]) want = all3[i].s[k] < all3[j].s[k] ? -1 : 1;
        if (!want) want = all3[i].n < all3[j].n ? -1 : all3[i].n > all3[j].n ? 1 : 0;
        G8_EQ(WCMP(a, b, &r), S_OK);
        G8_EQ(r, want);
        WDEL(a); WDEL(b);
    }
}

TEST(gap8_hs_sweep_embedded_null_flag_matches_content)
{
    int n, i; NEED_CORE(); REQUIRE(WEMB);
    {
        WCHAR alpha[3] = { L'a', 0, 0 }; S s[64]; int cnt = 0, len, k, j;
        for (len = 0; len <= 5; len++)
            for (k = 0; k < (1 << len); k++) { memset(&s[cnt], 0, sizeof s[cnt]); s[cnt].n = len; for (j = 0; j < len; j++) s[cnt].s[j] = (k >> j) & 1 ? 0 : L'a'; cnt++; }
        n = cnt; (void)alpha;
        for (i = 0; i < n; i++) {
            int has = 0, j; HSTRING h = mk(&s[i]), ref = NULL; HSTRING_HEADER hdr; BOOL f = 0xdead;
            for (j = 0; j < (int)s[i].n; j++) if (!s[i].s[j]) has = 1;
            G8_EQ(WEMB(h, &f), S_OK); G8_EQ(f, has);
            if (s[i].n) {
                f = 0xdead;
                G8_EQ(WCSR(s[i].s, s[i].n, &hdr, &ref), S_OK);
                G8_EQ(WEMB(ref, &f), S_OK); G8_EQ(f, has);
            }
            WDEL(h);
        }
    }
}

TEST(gap8_hs_sweep_duplicate_is_equal_and_independent)
{
    int n, i; NEED_CORE(); REQUIRE(WDUP);
    n = gen(all2, L"ab", 4);
    for (i = 0; i < n; i++) {
        HSTRING h = mk(&all2[i]), d = SENT, rd = SENT; HSTRING_HEADER hdr;
        G8_EQ(WDUP(h, &d), S_OK); same(d, all2[i].s, all2[i].n, "Duplicate (heap)", &all2[i]);
        WDEL(h);                                                  /* the duplicate outlives the original */
        same(d, all2[i].s, all2[i].n, "Duplicate after original deleted", &all2[i]);
        if (all2[i].n) {
            HSTRING ref = NULL; WCHAR local[8]; memcpy(local, all2[i].s, all2[i].n * 2); local[all2[i].n] = 0;
            G8_EQ(WCSR(local, all2[i].n, &hdr, &ref), S_OK);
            G8_EQ(WRAW(ref, NULL), local);                        /* a reference aliases the caller's buffer */
            G8_EQ(WDUP(ref, &rd), S_OK);
            CHECK(WRAW(rd, NULL) != local);                       /* its duplicate is a heap copy */
            memset(local, 'z', sizeof local);
            same(rd, all2[i].s, all2[i].n, "Duplicate of reference survives buffer reuse", &all2[i]);
            WDEL(rd);
        }
        WDEL(d);
    }
}

TEST(gap8_hs_sweep_preallocate_promote_lengths)
{
    UINT32 n, i; NEED_CORE(); REQUIRE(WPRE); REQUIRE(WPRO);
    for (n = 1; n <= 80; n++) {
        WCHAR *buf = NULL, exp[96]; HSTRING_BUFFER hb = NULL; HSTRING h = SENT;
        G8_EQ(WPRE(n, &buf, &hb), S_OK);
        CHECK(buf != NULL && hb != NULL);
        if (!buf || !hb) continue;
        for (i = 0; i < n; i++) buf[i] = (WCHAR)(L'A' + (i % 26));
        for (i = 0; i < n; i++) exp[i] = buf[i];
        G8_EQ(WPRO(hb, &h), S_OK);
        same(h, exp, n, "promoted string", NULL);
        WDEL(h);
    }
}

TEST(gap8_hs_sweep_reference_string_lengths)
{
    static WCHAR big[300]; UINT32 n; NEED_CORE();
    for (n = 0; n <= 256; n += (n < 8 ? 1 : 37)) {
        HSTRING_HEADER hdr; HSTRING h = SENT; UINT32 len = 0xffff, k;
        for (k = 0; k < 300; k++) big[k] = k < n ? (WCHAR)(L'a' + k % 26) : 0;      /* references need a NUL at [length] */
        G8_EQ(WCSR(big, n, &hdr, &h), S_OK);
        if (n == 0) { CHECK(h == NULL); continue; }
        G8_EQ(WLEN(h), n);
        G8_EQ(WRAW(h, &len), big); G8_EQ(len, n);
    }
}
