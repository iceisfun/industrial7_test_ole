/* bstr_core: VarBstrCat / VarBstrCmp / VarBstrFromXxx. Decomp-verified (Win10 x64 oleaut32):
 *  VarBstrCat: result = SysAllocStringByteLen(NULL, lenL+lenR) (so NULL+NULL is a non-NULL empty BSTR); lengths are byte lengths;
 *    sum overflow -> DISP_E_OVERFLOW (not reachable without 2 GB strings).
 *  VarBstrCmp: lcid==0 -> binary (WCHAR-wise unsigned, then trailing odd byte, then length); lcid==1 treated as 0x400;
 *    otherwise if either side is empty/NULL the result is by length (NULL == empty), else CompareStringW(lcid,flags,...)-1
 *    (failure -> E_INVALIDARG). Returns VARCMP_LT=0 / EQ=1 / GT=2; never VARCMP_NULL for BSTRs. */
#include "bc.h"

#define LCID_US 0x0409
#define LCID_ES 0x040A
#define LCID_DE 0x0407
#define LCID_SV 0x041D
#define NOUSER  0x80000000u

static BSTR mk(const WCHAR *s) { return U(SysAllocString)(s); }
static BSTR mkn(const WCHAR *s, UINT n) { return U(SysAllocStringLen)(s, n); }

/* ------------------------ VarBstrCat ------------------------ */
TEST(bstr_cat_basic)
{
    BSTR l, r, out = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    l = mk(L"Hello, "); r = mk(L"World");
    EXPECT_EQ(U(VarBstrCat)(l, r, &out), S_OK);
    CHECK(out != NULL && out != l && out != r);
    if (out) CHECK(bc_eq_w(out, L"Hello, World"));
    CHECK(bc_eq_w(l, L"Hello, ") && bc_eq_w(r, L"World"));   /* inputs untouched */
    BC_FREE(out); BC_FREE(l); BC_FREE(r);
}

TEST(bstr_cat_null_operands)
{
    BSTR s, out = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    s = mk(L"only");
    EXPECT_EQ(U(VarBstrCat)(s, NULL, &out), S_OK);
    CHECK(out != NULL && out != s && bc_eq_w(out, L"only")); BC_FREE(out);
    out = NULL;
    EXPECT_EQ(U(VarBstrCat)(NULL, s, &out), S_OK);
    CHECK(out != NULL && out != s && bc_eq_w(out, L"only")); BC_FREE(out);
    BC_FREE(s);
}

TEST(bstr_cat_null_null_gives_empty_bstr)
{
    BSTR out = (BSTR)(ULONG_PTR)0x10;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(VarBstrCat)(NULL, NULL, &out), S_OK);
    CHECK(out != NULL);
    if (out) { EXPECT_EQ(BLEN(out), 0); EXPECT_EQ(out[0], 0); BC_FREE(out); }
}

TEST(bstr_cat_empty_operands)
{
    BSTR e, s, out = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    e = mk(L""); s = mk(L"x");
    EXPECT_EQ(U(VarBstrCat)(e, e, &out), S_OK);
    CHECK(out != NULL && BLEN(out) == 0); BC_FREE(out); out = NULL;
    EXPECT_EQ(U(VarBstrCat)(e, s, &out), S_OK);
    CHECK(out != NULL && bc_eq_w(out, L"x")); BC_FREE(out); out = NULL;
    EXPECT_EQ(U(VarBstrCat)(s, e, &out), S_OK);
    CHECK(out != NULL && bc_eq_w(out, L"x")); BC_FREE(out);
    BC_FREE(e); BC_FREE(s);
}

TEST(bstr_cat_embedded_nuls_preserved)
{
    static const WCHAR a[] = { 'a', 0, 'b' };
    static const WCHAR b[] = { 0, 'c', 0 };
    static const WCHAR want[] = { 'a', 0, 'b', 0, 'c', 0 };
    BSTR l, r, out = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    l = mkn(a, 3); r = mkn(b, 3);
    EXPECT_EQ(U(VarBstrCat)(l, r, &out), S_OK);
    CHECK(out != NULL && bc_is_wstr(out, want, 6));
    BC_FREE(out); BC_FREE(l); BC_FREE(r);
}

TEST(bstr_cat_same_operand_twice)
{
    BSTR s, out = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    s = mk(L"ab");
    EXPECT_EQ(U(VarBstrCat)(s, s, &out), S_OK);
    CHECK(out != NULL && bc_eq_w(out, L"abab"));
    BC_FREE(out); BC_FREE(s);
}

TEST(bstr_cat_uses_byte_lengths)
{
    BSTR l, r, out = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    l = U(SysAllocStringByteLen)("abc", 3); r = U(SysAllocStringByteLen)("defgh", 5);
    EXPECT_EQ(U(VarBstrCat)(l, r, &out), S_OK);
    CHECK(out != NULL);
    if (out) { EXPECT_EQ(BLEN(out), 8); CHECK(!memcmp(out, "abcdefgh", 8)); EXPECT_EQ(((char *)out)[8], 0); }
    BC_FREE(out); BC_FREE(l); BC_FREE(r);
}

TEST(bstr_cat_sweep)
{
    WCHAR src[64];
    UINT i, j;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    for (i = 0; i < 64; i++) src[i] = (WCHAR)('a' + i);
    for (i = 0; i <= 20; i += 3) for (j = 0; j <= 20; j += 4) {
        BSTR l = mkn(src, i), r = mkn(src + 30, j), out = NULL;
        EXPECT_EQ(U(VarBstrCat)(l, r, &out), S_OK);
        CHECKF(out != NULL && BLEN(out) == (i + j) * 2 && !memcmp(out, src, i * 2) && !memcmp(out + i, src + 30, j * 2) && out[i + j] == 0, "i=%u j=%u", i, j);
        BC_FREE(out); BC_FREE(l); BC_FREE(r);
    }
}

/* ------------------------ VarBstrCmp ------------------------ */
struct cmpcase { const WCHAR *l, *r; LCID lcid; ULONG flags; HRESULT want; const char *why; };

static void run_cmp(const struct cmpcase *c, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        BSTR l = c[i].l ? mk(c[i].l) : NULL, r = c[i].r ? mk(c[i].r) : NULL;
        HRESULT hr = U(VarBstrCmp)(l, r, c[i].lcid, c[i].flags);
        CHECKF(hr == c[i].want, "#%u %s: lcid=0x%lx flags=0x%lx got %ld want %ld", i, c[i].why, (unsigned long)c[i].lcid, (unsigned long)c[i].flags, (long)hr, (long)c[i].want);
        BC_FREE(l); BC_FREE(r);
    }
}
#define CMP_NEED() do { REQUIRE(U(VarBstrCmp)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString)); } while (0)

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_cmp_null_and_empty)
{
    static const struct cmpcase c[] = {
        { NULL, NULL, 0, 0, VCMP_EQ, "NULL/NULL binary" },
        { NULL, NULL, LCID_US, 0, VCMP_EQ, "NULL/NULL en" },
        { L"Wurscht", NULL, 0, 0, VCMP_GT, "str/NULL binary" },
        { NULL, L"Wurscht", 0, 0, VCMP_LT, "NULL/str binary" },
        { L"Wurscht", NULL, LCID_US, 0, VCMP_GT, "str/NULL en" },
        { NULL, L"Wurscht", LCID_US, 0, VCMP_LT, "NULL/str en" },
        { L"", NULL, 0, 0, VCMP_EQ, "empty/NULL binary" },
        { NULL, L"", 0, 0, VCMP_EQ, "NULL/empty binary" },
        { L"", NULL, LCID_US, 0, VCMP_EQ, "empty/NULL en" },
        { NULL, L"", LCID_US, 0, VCMP_EQ, "NULL/empty en" },
        { L"", L"", LCID_US, 0, VCMP_EQ, "empty/empty" },
        { L"", L"a", LCID_US, 0, VCMP_LT, "empty/str" },
        { L"a", L"", LCID_US, 0, VCMP_GT, "str/empty" },
        { L"", L"a", 0, 0, VCMP_LT, "empty/str binary" },
        /* flags are never validated when one side is empty (no CompareStringW call) */
        { NULL, L"x", LCID_US, 0x40000000u, VCMP_LT, "bad flag, NULL side" },
        { L"x", L"", LCID_US, 0x40000000u, VCMP_GT, "bad flag, empty side" },
    };
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
}

TEST(bstr_cmp_equal_strings_and_same_pointer)
{
    BSTR a, b;
    LCID lcids[] = { 0, 1, LCID_US, 0x400, LCID_DE, LCID_SV, 0x0411 };
    unsigned i;
    CMP_NEED();
    a = mk(L"identical text"); b = mk(L"identical text");
    for (i = 0; i < sizeof(lcids) / sizeof(lcids[0]); i++) {
        CHECKF(U(VarBstrCmp)(a, b, lcids[i], 0) == VCMP_EQ, "lcid 0x%lx distinct", (unsigned long)lcids[i]);
        CHECKF(U(VarBstrCmp)(a, a, lcids[i], 0) == VCMP_EQ, "lcid 0x%lx same ptr", (unsigned long)lcids[i]);
    }
    BC_FREE(a); BC_FREE(b);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_cmp_case_en_us)
{
    static const struct cmpcase c[] = {
        { L"a", L"A", LCID_US, 0, VCMP_LT, "lowercase sorts before uppercase" },
        { L"A", L"a", LCID_US, 0, VCMP_GT, "reverse" },
        { L"a", L"A", LCID_US, NORM_IGNORECASE, VCMP_EQ, "ignorecase" },
        { L"ABC", L"abc", LCID_US, NORM_IGNORECASE, VCMP_EQ, "ignorecase word" },
        { L"abc", L"abd", LCID_US, NORM_IGNORECASE, VCMP_LT, "ignorecase differs" },
        { L"ABD", L"abc", LCID_US, NORM_IGNORECASE, VCMP_GT, "ignorecase differs 2" },
        { L"abc", L"ABD", LCID_US, 0, VCMP_LT, "culture: c < D regardless of case" },
        { L"a", L"B", LCID_US, 0, VCMP_LT, "culture sort a < B" },
        { L"10", L"9", LCID_US, 0, VCMP_LT, "string not numeric sort" },
        { L"apple", L"banana", LCID_US, 0, VCMP_LT, "alpha" },
        { L"banana", L"apple", LCID_US, 0, VCMP_GT, "alpha rev" },
        { L"a", L"ab", LCID_US, 0, VCMP_LT, "prefix" },
        { L"ab", L"a", LCID_US, 0, VCMP_GT, "prefix rev" },
    };
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
}

TEST(bstr_cmp_lcid_user_default_and_one)
{
    /* LOCALE_USER_DEFAULT (0x400) behaves like a real locale; lcid 1 is mapped to 0x400. Results for plain ASCII
     * case-insensitive comparisons are locale independent. */
    static const struct cmpcase c[] = {
        { L"abc", L"ABC", 0x0400, NORM_IGNORECASE, VCMP_EQ, "user default" },
        { L"abc", L"ABC", 1, NORM_IGNORECASE, VCMP_EQ, "lcid 1" },
        { L"abc", L"abd", 0x0400, 0, VCMP_LT, "user default order" },
        { L"abc", L"abd", 1, 0, VCMP_LT, "lcid 1 order" },
        { L"abc", L"ABC", 0x0800, NORM_IGNORECASE, VCMP_EQ, "system default" },
        { L"abc", L"abd", 0x1000, 0, VCMP_LT, "LOCALE_CUSTOM_DEFAULT-ish; informational" },
    };
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]) - 1);
}

TEST(bstr_cmp_binary_lcid_zero)
{
    static const struct cmpcase c[] = {
        { L"a", L"A", 0, 0, VCMP_GT, "0x61 > 0x41" },
        { L"A", L"a", 0, 0, VCMP_LT, "0x41 < 0x61" },
        { L"a", L"B", 0, 0, VCMP_GT, "binary: a(61) > B(42)" },
        { L"a", L"A", 0, NORM_IGNORECASE, VCMP_GT, "flags ignored for binary compare" },
        { L"abc", L"abd", 0, 0, VCMP_LT, "diff at last" },
        { L"ab", L"abc", 0, 0, VCMP_LT, "shorter first" },
        { L"abc", L"ab", 0, 0, VCMP_GT, "longer after" },
        { L"b", L"abc", 0, 0, VCMP_GT, "first diff wins over length" },
        { L"\xFFFF", L"\x0001", 0, 0, VCMP_GT, "unsigned WCHAR compare" },
        { L"\x8000", L"\x7FFF", 0, 0, VCMP_GT, "unsigned WCHAR compare 2" },
        { L"10", L"9", 0, 0, VCMP_LT, "'1' < '9'" },
    };
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_cmp_embedded_nul_and_odd_bytes)
{
    static const WCHAR s1[] = { 'a', 0 };
    static const WCHAR s2[] = { 'a', 0, 'b' };
    static const WCHAR e1[] = { 'a', 0, 'b' }, e2[] = { 'a', 0, 'c' };
    static const char sb1[] = { 1, 0, 1 }, sb2[] = { 1, 0, 2 };
    static const char z2[] = { 0, 0 }, z3[] = { 0, 0, 0 };
    BSTR a, b, e;
    CMP_NEED(); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysAllocStringByteLen));
    /* "a" (1 char) vs "a\0" (2 chars, stored length 4) are different strings */
    a = mk(L"a"); b = mkn(s1, 2);
    EXPECT_EQ(U(VarBstrCmp)(a, b, LCID_US, 0), VCMP_EQ);   /* CompareStringW treats the trailing NUL as no weight */
    EXPECT_EQ(U(VarBstrCmp)(a, b, 0, 0), VCMP_LT);        /* binary: length breaks the tie */
    BC_FREE(b);
    b = mkn(s2, 3);
    EXPECT_EQ(U(VarBstrCmp)(a, b, 0, 0), VCMP_LT);
    EXPECT_EQ(U(VarBstrCmp)(a, b, LCID_US, 0), VCMP_LT);
    BC_FREE(a); BC_FREE(b);
    a = mkn(e1, 3); b = mkn(e2, 3);
    EXPECT_EQ(U(VarBstrCmp)(a, b, 0, 0), VCMP_LT);
    EXPECT_EQ(U(VarBstrCmp)(b, a, 0, 0), VCMP_GT);
    BC_FREE(a); BC_FREE(b);
    /* odd byte lengths, binary: last odd byte is compared */
    a = U(SysAllocStringByteLen)(sb1, 3); b = U(SysAllocStringByteLen)(sb2, 3);
    EXPECT_EQ(U(VarBstrCmp)(a, b, 0, 0), VCMP_LT);
    EXPECT_EQ(U(VarBstrCmp)(b, a, 0, 0), VCMP_GT);
    BC_FREE(a); BC_FREE(b);
    /* {0,0} vs {0,0,0} against an empty string, both lcid modes */
    e = mk(L"");
    a = U(SysAllocStringByteLen)(z2, 2); b = U(SysAllocStringByteLen)(z3, 3);
    EXPECT_EQ(U(VarBstrCmp)(a, e, LCID_US, 0), VCMP_GT);
    EXPECT_EQ(U(VarBstrCmp)(e, a, LCID_US, 0), VCMP_LT);
    EXPECT_EQ(U(VarBstrCmp)(b, e, LCID_US, 0), VCMP_GT);
    EXPECT_EQ(U(VarBstrCmp)(b, a, LCID_US, 0), VCMP_EQ);
    EXPECT_EQ(U(VarBstrCmp)(a, e, 0, 0), VCMP_GT);
    EXPECT_EQ(U(VarBstrCmp)(e, a, 0, 0), VCMP_LT);
    EXPECT_EQ(U(VarBstrCmp)(b, e, 0, 0), VCMP_GT);
    EXPECT_EQ(U(VarBstrCmp)(b, a, 0, 0), VCMP_GT);
    BC_FREE(a); BC_FREE(b); BC_FREE(e);
}

TEST(bstr_cmp_norm_ignorewidth)
{
    static const struct cmpcase c[] = {
        { L"\xFF21", L"A", LCID_US, NORM_IGNOREWIDTH, VCMP_EQ, "fullwidth A vs A" },
        { L"A", L"\xFF21", LCID_US, NORM_IGNOREWIDTH, VCMP_EQ, "A vs fullwidth A" },
        { L"\xFF41\xFF42", L"ab", LCID_US, NORM_IGNOREWIDTH, VCMP_EQ, "fullwidth word" },
        { L"\xFF21", L"B", LCID_US, NORM_IGNOREWIDTH, VCMP_LT, "width ignored but letters differ" },
        { L"\xFF21", L"a", LCID_US, NORM_IGNOREWIDTH | NORM_IGNORECASE, VCMP_EQ, "width+case" },
        { L"\xFF21", L"a", LCID_US, NORM_IGNOREWIDTH, VCMP_GT, "width ignored, case still matters (A > a)" },
    };
    BSTR a, b;
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
    a = mk(L"\xFF21"); b = mk(L"A");
    t_note("without NORM_IGNOREWIDTH fullwidth A vs A = %ld (documented: not equal)", (long)U(VarBstrCmp)(a, b, LCID_US, 0));
    BC_FREE(a); BC_FREE(b);
}

TEST(bstr_cmp_norm_ignorekanatype)
{
    static const struct cmpcase c[] = {
        { L"\x3042", L"\x30A2", LCID_US, NORM_IGNOREKANATYPE, VCMP_EQ, "hiragana A vs katakana A" },
        { L"\x30A2", L"\x3042", LCID_US, NORM_IGNOREKANATYPE, VCMP_EQ, "katakana A vs hiragana A" },
        { L"\x3042\x3044", L"\x30A2\x30A4", LCID_US, NORM_IGNOREKANATYPE, VCMP_EQ, "two kana" },
        { L"\x3042", L"\x30A4", LCID_US, NORM_IGNOREKANATYPE, VCMP_LT, "different kana (A < I)" },
    };
    BSTR a, b;
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
    a = mk(L"\x3042"); b = mk(L"\x30A2");
    t_note("without NORM_IGNOREKANATYPE hiragana vs katakana = %ld (documented: not equal)", (long)U(VarBstrCmp)(a, b, LCID_US, 0));
    BC_FREE(a); BC_FREE(b);
}

TEST(bstr_cmp_norm_ignorenonspace)
{
    static const struct cmpcase c[] = {
        { L"e\x0301", L"e", LCID_US, NORM_IGNORENONSPACE, VCMP_EQ, "combining acute ignored" },
        { L"\x00E9", L"e", LCID_US, NORM_IGNORENONSPACE, VCMP_EQ, "precomposed e-acute vs e" },
        { L"\x00E9", L"E", LCID_US, NORM_IGNORENONSPACE | NORM_IGNORECASE, VCMP_EQ, "nonspace+case" },
        { L"\x00E9t\x00E9", L"ete", LCID_US, NORM_IGNORENONSPACE, VCMP_EQ, "word" },
        { L"\x00E9", L"f", LCID_US, NORM_IGNORENONSPACE, VCMP_LT, "e < f still" },
    };
    BSTR a, b;
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
    a = mk(L"\x00E9"); b = mk(L"e");
    t_note("without NORM_IGNORENONSPACE e-acute vs e = %ld (expected GT)", (long)U(VarBstrCmp)(a, b, LCID_US, 0));
    BC_FREE(a); BC_FREE(b);
}

TEST(bstr_cmp_norm_ignoresymbols)
{
    static const struct cmpcase c[] = {
        { L"a-b", L"ab", LCID_US, NORM_IGNORESYMBOLS, VCMP_EQ, "hyphen" },
        { L"a b", L"ab", LCID_US, NORM_IGNORESYMBOLS, VCMP_EQ, "space" },
        { L"(a)", L"a", LCID_US, NORM_IGNORESYMBOLS, VCMP_EQ, "parens" },
        { L"a-b", L"ac", LCID_US, NORM_IGNORESYMBOLS, VCMP_LT, "letters still compare" },
    };
    CMP_NEED();
    run_cmp(c, sizeof(c) / sizeof(c[0]));
}

TEST(bstr_cmp_locale_specific_collation)
{
    /* sv-SE sorts a-diaeresis AFTER z; de-DE sorts it with a. Needs locale data (available on Windows 10). */
    static const struct cmpcase c[] = {
        { L"\x00E4", L"z", LCID_SV, 0, VCMP_GT, "sv-SE a-diaeresis after z" },
        { L"z", L"\x00E4", LCID_SV, 0, VCMP_LT, "sv-SE reverse" },
        { L"\x00E4", L"z", LCID_DE, 0, VCMP_LT, "de-DE a-diaeresis before z" },
        { L"\x00E4", L"z", LCID_US, 0, VCMP_LT, "en-US a-diaeresis before z" },
    };
    CMP_NEED();
    if (t_is_wine()) { t_skip("locale collation under wine"); return; }
    run_cmp(c, sizeof(c) / sizeof(c[0]));
}

TEST(bstr_cmp_invalid_flags_note)
{
    BSTR a, b;
    HRESULT hr;
    CMP_NEED();
    a = mk(L"abc"); b = mk(L"abd");
    hr = U(VarBstrCmp)(a, b, LCID_US, 0x40000000u);
    if (FAILED(hr)) EXPECT_EQ(hr, E_INVALIDARG);   /* CompareStringW failure maps to E_INVALIDARG */
    t_note("VarBstrCmp(flags=0x40000000) -> 0x%08lx", (unsigned long)hr);
    hr = U(VarBstrCmp)(a, b, LCID_US, 0x00000040u);
    t_note("VarBstrCmp(flags=0x40) -> 0x%08lx", (unsigned long)hr);
    BC_FREE(a); BC_FREE(b);
}

TEST(bstr_cmp_never_returns_varcmp_null)
{
    BSTR e = NULL, s;
    LCID lcids[] = { 0, LCID_US };
    unsigned i;
    CMP_NEED();
    s = mk(L"s");
    for (i = 0; i < 2; i++) {
        CHECK(U(VarBstrCmp)(e, e, lcids[i], 0) != VCMP_NULL);
        CHECK(U(VarBstrCmp)(e, s, lcids[i], 0) != VCMP_NULL);
        CHECK(U(VarBstrCmp)(s, e, lcids[i], 0) != VCMP_NULL);
    }
    BC_FREE(s);
}

/* ------------------------ VarBstrFromXxx ------------------------ */
static void expect_bstr_(HRESULT hr, BSTR b, const WCHAR *want, const char *what)
{
    CHECKF(hr == S_OK, "%s hr=0x%08lx", what, (unsigned long)hr);
    if (hr == S_OK) {
        CHECKF(b != NULL, "%s null output", what);
        if (b) {
            CHECKF(!lstrcmpW(b, want), "%s got '%ls' want '%ls'", what, b, want);
            CHECKF(BLEN(b) == (DWORD)lstrlenW(want) * 2 && b[lstrlenW(want)] == 0, "%s prefix %lu", what, (unsigned long)BLEN(b));
        }
    }
}

#define expect_bstr(call, b, want, what) do { HRESULT hr_ = (call); expect_bstr_(hr_, b, want, what); } while (0)

TEST(bstr_from_i4_table)
{
    static const struct { LONG v; const WCHAR *s; } t[] = {
        { 0, L"0" }, { 1, L"1" }, { -1, L"-1" }, { 42, L"42" }, { -42, L"-42" }, { 1000, L"1000" }, { 2147483647, L"2147483647" },
        { (-2147483647 - 1), L"-2147483648" }, { 65536, L"65536" }, { -100000, L"-100000" },
    };
    unsigned i;
    LCID lcids[] = { LCID_US, LCID_ES, LCID_DE, 0, 0x400 };
    REQUIRE(U(VarBstrFromI4)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        unsigned k;
        for (k = 0; k < 5; k++) {   /* without VAR_USE_NLS the lcid is not consulted for integers */
            BSTR b = NULL;
            HRESULT hr = U(VarBstrFromI4)(t[i].v, lcids[k], k & 1 ? NOUSER : 0, &b);
            expect_bstr(hr, b, t[i].s, "I4");
            BC_FREE(b);
        }
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_i4_int_min_nouseroverride)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromI4));
    expect_bstr(U(VarBstrFromI4)((-2147483647 - 1), LCID_US, NOUSER, &b), b, L"-2147483648", "I4 min");
    BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromI4)(-42, LCID_US, NOUSER, &b), b, L"-42", "I4 -42");
    BC_FREE(b);
}

TEST(bstr_from_small_ints)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromI1)); REQUIRE(U(VarBstrFromI2)); REQUIRE(U(VarBstrFromUI1)); REQUIRE(U(VarBstrFromUI2)); REQUIRE(U(VarBstrFromUI4));
    expect_bstr(U(VarBstrFromI1)(-128, LCID_US, 0, &b), b, L"-128", "I1 min"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromI1)(127, LCID_US, 0, &b), b, L"127", "I1 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromI2)(-32768, LCID_US, 0, &b), b, L"-32768", "I2 min"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromI2)(32767, LCID_US, 0, &b), b, L"32767", "I2 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI1)(255, LCID_US, 0, &b), b, L"255", "UI1 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI1)(0, LCID_US, 0, &b), b, L"0", "UI1 0"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI2)(65535, LCID_US, 0, &b), b, L"65535", "UI2 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI4)(4294967295u, LCID_US, 0, &b), b, L"4294967295", "UI4 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI4)(2147483648u, LCID_US, 0, &b), b, L"2147483648", "UI4 2^31"); BC_FREE(b);
}

TEST(bstr_from_i8_ui8)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromI8)); REQUIRE(U(VarBstrFromUI8));
    expect_bstr(U(VarBstrFromI8)(0, LCID_US, 0, &b), b, L"0", "I8 0"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromI8)(-9223372036854775807LL - 1, LCID_US, 0, &b), b, L"-9223372036854775808", "I8 min"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromI8)(9223372036854775807LL, LCID_US, 0, &b), b, L"9223372036854775807", "I8 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI8)(18446744073709551615ULL, LCID_US, 0, &b), b, L"18446744073709551615", "UI8 max"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromUI8)(4294967296ULL, LCID_US, 0, &b), b, L"4294967296", "UI8 2^32"); BC_FREE(b);
}

TEST(bstr_from_bool)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromBool)); REQUIRE(U(SysFreeString));
    expect_bstr(U(VarBstrFromBool)(VARIANT_TRUE, LCID_US, 0, &b), b, L"True", "TRUE"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromBool)(VARIANT_FALSE, LCID_US, 0, &b), b, L"False", "FALSE"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromBool)(1, LCID_US, 0, &b), b, L"True", "1 is true"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromBool)((VARIANT_BOOL)0x1234, LCID_US, 0, &b), b, L"True", "any nonzero"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromBool)(VARIANT_TRUE, LCID_US, 0x10 /*VAR_LOCALBOOL*/, &b), b, L"True", "local en TRUE"); BC_FREE(b); b = NULL;
    expect_bstr(U(VarBstrFromBool)(VARIANT_FALSE, LCID_US, 0x10, &b), b, L"False", "local en FALSE"); BC_FREE(b); b = NULL;
    /* without VAR_LOCALBOOL the lcid is ignored */
    expect_bstr(U(VarBstrFromBool)(VARIANT_TRUE, LCID_DE, 0, &b), b, L"True", "non-local, de"); BC_FREE(b);
}

TEST(bstr_from_bool_null_output)
{
    REQUIRE(U(VarBstrFromBool));
    EXPECT_EQ(U(VarBstrFromBool)(VARIANT_TRUE, LCID_US, 0, NULL), E_INVALIDARG);
    EXPECT_EQ(U(VarBstrFromBool)(VARIANT_FALSE, LCID_US, 0x10, NULL), E_INVALIDARG);
}

TEST(bstr_from_bool_localized_note)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromBool));
    if (S_OK == U(VarBstrFromBool)(VARIANT_TRUE, LCID_DE, 0x10, &b) && b) { t_note("VAR_LOCALBOOL de-DE TRUE = '%ls'", b); BC_FREE(b); b = NULL; }
    if (S_OK == U(VarBstrFromBool)(VARIANT_FALSE, LCID_DE, 0x10, &b) && b) { t_note("VAR_LOCALBOOL de-DE FALSE = '%ls'", b); BC_FREE(b); b = NULL; }
    if (S_OK == U(VarBstrFromBool)(VARIANT_TRUE, LCID_ES, 0x10, &b) && b) { t_note("VAR_LOCALBOOL es-ES TRUE = '%ls'", b); BC_FREE(b); }
}

struct dtest { double v; const WCHAR *s; };

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_r8_en_table)
{
    static const struct dtest t[] = {
        { 0.5, L"0.5" }, { 0.56789, L"0.56789" }, { 5.6789e-2, L"0.056789" }, { 5.6789e-3, L"0.0056789" }, { 5.6789e-4, L"0.00056789" },
        { 5.6789e-5, L"0.000056789" }, { 5.6789e-6, L"0.0000056789" }, { 5.6789e-7, L"0.00000056789" }, { 5.6789e-8, L"0.000000056789" },
        { 5.6789e-9, L"0.0000000056789" }, { 5.6789e-10, L"0.00000000056789" }, { 5.6789e-11, L"0.000000000056789" },
        { 5.6789e-12, L"5.6789E-12" }, { 5.6789e-13, L"5.6789E-13" }, { 5.6789e-14, L"5.6789E-14" }, { 5.6789e-15, L"5.6789E-15" }, { 5.6789e-16, L"5.6789E-16" },
        { 1.0e8, L"100000000" }, { 1.0e12, L"1000000000000" }, { 1.0e14, L"100000000000000" }, { 999999999999999.0, L"999999999999999" },
        { 1000000000000000.0, L"1E+15" }, { 1200000000000000.0, L"1.2E+15" }, { 1.0e15, L"1E+15" }, { 1.0e16, L"1E+16" }, { 1.234e16, L"1.234E+16" },
        { 3.14159265359, L"3.14159265359" }, { 3.141592653589793, L"3.14159265358979" }, { 12.345678901234567, L"12.3456789012346" },
        { 123.456789012345678, L"123.456789012346" }, { 1234.567890123456789, L"1234.56789012346" },
        { 0.0, L"0" }, { 1.0, L"1" }, { -1.0, L"-1" }, { -0.5, L"-0.5" }, { 100.0, L"100" }, { 1.5, L"1.5" }, { -123.25, L"-123.25" },
    };
    unsigned i;
    REQUIRE(U(VarBstrFromR8)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        BSTR b = NULL;
        char what[64];
        sprintf(what, "R8[%u] %.17g", i, t[i].v);
        expect_bstr(U(VarBstrFromR8)(t[i].v, LCID_US, NOUSER, &b), b, t[i].s, what);
        BC_FREE(b);
    }
}

TEST(bstr_from_r8_es_decimal_comma)
{
    static const struct dtest t[] = {
        { 0.5, L"0,5" }, { 0.05, L"0,05" }, { 0.000005, L"0,000005" }, { 1200000000000000.0, L"1,2E+15" }, { 1.234e16, L"1,234E+16" },
        { 3.141592653589793, L"3,14159265358979" }, { 1234.567890123456789, L"1234,56789012346" }, { 1.0e8, L"100000000" }, { -2.5, L"-2,5" },
    };
    unsigned i;
    REQUIRE(U(VarBstrFromR8)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        BSTR b = NULL;
        expect_bstr(U(VarBstrFromR8)(t[i].v, LCID_ES, NOUSER, &b), b, t[i].s, "R8 es");
        BC_FREE(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_r4_table)
{
    static const struct { float v; const WCHAR *en, *es; } t[] = {
        { 0.5f, L"0.5", L"0,5" }, { 0.05f, L"0.05", L"0,05" }, { 0.005f, L"0.005", L"0,005" }, { 0.0005f, L"0.0005", L"0,0005" },
        { 0.00005f, L"0.00005", L"0,00005" }, { 0.000005f, L"0.000005", L"0,000005" },
        { 1.0e8f, L"1E+08", L"1E+08" }, { 1.0e12f, L"1E+12", L"1E+12" }, { 1.0e14f, L"1E+14", L"1E+14" }, { 1.0e15f, L"1E+15", L"1E+15" },
        { 1.0e16f, L"1E+16", L"1E+16" }, { 1.234e16f, L"1.234E+16", L"1,234E+16" },
        { 3.14159265f, L"3.141593", L"3,141593" }, { 12.3456789f, L"12.34568", L"12,34568" }, { 123.45678901f, L"123.4568", L"123,4568" },
        { 1234.56789012f, L"1234.568", L"1234,568" }, { 654322.23456f, L"654322.3", L"654322,3" }, { 0.0f, L"0", L"0" }, { -1.5f, L"-1.5", L"-1,5" },
    };
    unsigned i;
    REQUIRE(U(VarBstrFromR4)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        BSTR b = NULL;
        char what[64];
        sprintf(what, "R4[%u] %.9g", i, (double)t[i].v);
        expect_bstr(U(VarBstrFromR4)(t[i].v, LCID_US, NOUSER, &b), b, t[i].en, what); BC_FREE(b); b = NULL;
        expect_bstr(U(VarBstrFromR4)(t[i].v, LCID_ES, NOUSER, &b), b, t[i].es, what); BC_FREE(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_r4_negative_zero)
{
    BSTR b = NULL;
    float f = -0.0f;
    REQUIRE(U(VarBstrFromR4));
    CHECK(U(VarBstrFromR4)(f, LCID_US, 0, &b) == S_OK && b != NULL);
    if (b) {
        CHECK(!lstrcmpW(b, L"0") || !lstrcmpW(b, L"-0"));
        t_note("VarBstrFromR4(-0.0) = '%ls'", b);
        BC_FREE(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_date_table)
{
    static const struct { DATE d; const WCHAR *s; } t[] = {
        { 0.0, L"12:00:00 AM" }, { 3.34, L"1/2/1900 8:09:36 AM" }, { 3339.34, L"2/20/1909 8:09:36 AM" }, { 365.00, L"12/30/1900" },
        { 365.25, L"12/30/1900 6:00:00 AM" }, { 1461.0, L"12/31/1903" }, { 1461.5, L"12/31/1903 12:00:00 PM" },
        { -49192.24, L"4/24/1765 5:45:36 AM" }, { -657434.0, L"1/1/100" }, { 2958465.0, L"12/31/9999" },
    };
    unsigned i;
    REQUIRE(U(VarBstrFromDate)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        BSTR b = NULL;
        expect_bstr(U(VarBstrFromDate)(t[i].d, LCID_US, NOUSER, &b), b, t[i].s, "date");
        BC_FREE(b);
    }
}

static void cy_case(LONG hi, ULONG lo, const WCHAR *s, LCID lcid, const char *w)
{
    CY cy; BSTR b = NULL;
    cy.Hi = hi; cy.Lo = lo;
    expect_bstr(U(VarBstrFromCy)(cy, lcid, NOUSER, &b), b, s, w);
    BC_FREE(b);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_cy_table)
{
    REQUIRE(U(VarBstrFromCy)); REQUIRE(U(SysFreeString));
    cy_case(0, 0, L"0", LCID_US, "0");
    cy_case(0, 10000, L"1", LCID_US, "1");
    cy_case(0, 15000, L"1.5", LCID_US, "1.5");
    cy_case(-1, (ULONG)(-15000), L"-1.5", LCID_US, "-1.5");
    cy_case(0, 0xffffffff, L"429496.7295", LCID_US, "2^32-1");
    cy_case(1, 0, L"429496.7296", LCID_US, "2^32");
    cy_case(0x7fffffff, 0xffffffff, L"922337203685477.5807", LCID_US, "max");
    cy_case(0, 9, L"0.0009", LCID_US, "0.0009");
    cy_case(0, 9, L"0,0009", LCID_ES, "0,0009 es");
}

static void dec_case(BYTE scale, BYTE sign, ULONG hi, ULONG mid, ULONG lo, const WCHAR *s, LCID lcid, const char *w)
{
    DECIMAL d; BSTR b = NULL;
    memset(&d, 0, sizeof(d));
    d.scale = scale; d.sign = sign; d.Hi32 = hi; d.Mid32 = mid; d.Lo32 = lo;
    expect_bstr(U(VarBstrFromDec)(&d, lcid, NOUSER, &b), b, s, w);
    BC_FREE(b);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_from_dec_table)
{
    REQUIRE(U(VarBstrFromDec)); REQUIRE(U(SysFreeString));
    dec_case(0, 0, 0, 0, 0, L"0", LCID_US, "0");
    dec_case(0, 0, 0, 0, 1, L"1", LCID_US, "1");
    dec_case(1, 0, 0, 0, 10, L"1", LCID_US, "1 scale1");
    dec_case(3, 0, 0, 0, 1000, L"1", LCID_US, "1 scale3");
    dec_case(1, 0, 0, 0, 15, L"1.5", LCID_US, "1.5");
    dec_case(3, 0, 0, 0, 1500, L"1.5", LCID_US, "1.5 scale3");
    dec_case(1, 0x80, 0, 0, 15, L"-1.5", LCID_US, "-1.5");
    dec_case(0, 0, 0, 0, 0xffffffff, L"4294967295", LCID_US, "2^32-1");
    dec_case(0, 0, 0, 1, 0, L"4294967296", LCID_US, "2^32");
    dec_case(0, 0, 0, 0xffffffff, 0xffffffff, L"18446744073709551615", LCID_US, "2^64-1");
    dec_case(0, 0, 1, 0, 0, L"18446744073709551616", LCID_US, "2^64");
    dec_case(0, 0, 0xffffffff, 0xffffffff, 0xffffffff, L"79228162514264337593543950335", LCID_US, "2^96-1");
    dec_case(10, 0, 0, 0, 1, L"0.0000000001", LCID_US, "1e-10");
    dec_case(10, 0, 0xffffffff, 0xffffffff, 0xffffffff, L"7922816251426433759.3543950335", LCID_US, "max scale10");
    dec_case(28, 0, 0xffffffff, 0xffffffff, 0xffffffff, L"7.9228162514264337593543950335", LCID_US, "max scale28");
    dec_case(4, 0, 0, 0, 9, L"0.0009", LCID_US, "lead zeros 4");
    dec_case(5, 0, 0, 0, 90, L"0.0009", LCID_US, "lead zeros 5");
    dec_case(7, 0, 0, 0, 9000, L"0.0009", LCID_US, "lead zeros 7");
    dec_case(4, 0, 0, 0, 9, L"0,0009", LCID_ES, "es 4");
    dec_case(6, 0, 0, 0, 900, L"0,0009", LCID_ES, "es 6");
}

TEST(bstr_from_result_is_ole_bstr)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromI4)); REQUIRE(U(SysStringLen)); REQUIRE(U(SysStringByteLen));
    EXPECT_EQ(U(VarBstrFromI4)(123456, LCID_US, 0, &b), S_OK);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(U(SysStringLen)(b), 6);
        EXPECT_EQ(U(SysStringByteLen)(b), 12);
        EXPECT_EQ(((ULONG_PTR)b) & 7, 0);
        BC_FREE(b);
    }
}
