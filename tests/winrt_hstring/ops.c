/* Compare / Concat / Substring / Replace / Trim.  Verified against the Windows 10 combase decompilation. */
#include "hs.h"

/* ------------------------------------------------------------------ Compare */

static int sgn(int v) { return v < 0 ? -1 : v > 0; }

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_ops_compare_table)
{
    struct { const WCHAR *a; UINT32 an; const WCHAR *b; UINT32 bn; int exp; } t[] = {
        { L"abc", 3, L"abc", 3, 0 },
        { L"abc", 3, L"abd", 3, -1 },
        { L"abd", 3, L"abc", 3, 1 },
        { L"abc", 3, L"abcd", 4, -1 },            /* prefix sorts first */
        { L"abcd", 4, L"abc", 3, 1 },
        { L"a", 1, L"A", 1, 1 },                  /* ordinal: case sensitive, 'a' (0x61) > 'A' (0x41) */
        { L"A", 1, L"a", 1, -1 },
        { L"Z", 1, L"a", 1, -1 },
        { L"z", 1, L"A", 1, 1 },
        { L"abc", 3, L"ABC", 3, 1 },
        { L"a\0b", 3, L"a\0b", 3, 0 },            /* embedded NULs are significant */
        { L"a\0b", 3, L"a\0c", 3, -1 },
        { L"a\0c", 3, L"a\0b", 3, 1 },
        { L"a\0", 2, L"a", 1, 1 },
        { L"a", 1, L"a\0", 2, -1 },
        { L"a\0b", 3, L"a", 1, 1 },
        { L"\1", 1, L"\2", 1, -1 },
        { L"\xD800", 1, L"\xE000", 1, -1 },       /* UTF-16 code unit order, not code point order */
        { L"\xFF00", 1, L"\xD800", 1, 1 },
        { L"\xFFFF", 1, L"\x0001", 1, 1 },
        { L"\x00e9", 1, L"e", 1, 1 },             /* no accent folding */
        { L"abc", 2, L"abc", 3, -1 },
        { L"abc", 3, L"abc", 2, 1 },
        { L"\0", 1, L"\0\0", 2, -1 },
    };
    unsigned i; REQUIRE_HS_OPS();
    for (i = 0; i < ARRAYSIZE(t); i++) {
        HSTRING a = hs_make(t[i].a, t[i].an), b = hs_make(t[i].b, t[i].bn);
        INT32 r = 0x1234;
        HRESULT hr = WCMP(a, b, &r);
        t_check(hr == S_OK, __FILE__, __LINE__, "compare S_OK", "row %u hr=0x%08lx", i, (unsigned long)hr);
        t_check(r == t[i].exp, __FILE__, __LINE__, "compare result", "row %u got %d want %d", i, r, t[i].exp);
        r = 0x1234;
        hr = WCMP(b, a, &r);                     /* antisymmetry */
        t_check(hr == S_OK && r == -t[i].exp, __FILE__, __LINE__, "compare antisymmetric", "row %u got %d", i, r);
        WDEL(a); WDEL(b);
    }
}

TEST(winrt_hstring_ops_compare_result_is_exactly_minus1_0_1)
{
    HSTRING a = hs_makez(L"a"), z = hs_makez(L"zzzzzz"); INT32 r; REQUIRE_HS_OPS();
    r = 0; EXPECT_EQ(WCMP(a, z, &r), S_OK); EXPECT_EQ(r, -1);   /* not the char difference */
    r = 0; EXPECT_EQ(WCMP(z, a, &r), S_OK); EXPECT_EQ(r, 1);
    WDEL(a); WDEL(z);
}

TEST(winrt_hstring_ops_compare_null_as_empty)
{
    HSTRING a = hs_makez(L"a"); INT32 r; REQUIRE_HS_OPS();
    r = 0x77; EXPECT_EQ(WCMP(NULL, NULL, &r), S_OK); EXPECT_EQ(r, 0);
    r = 0x77; EXPECT_EQ(WCMP(NULL, a, &r), S_OK);    EXPECT_EQ(r, -1);
    r = 0x77; EXPECT_EQ(WCMP(a, NULL, &r), S_OK);    EXPECT_EQ(r, 1);
    WDEL(a);
}

TEST(winrt_hstring_ops_compare_same_object_is_equal)
{
    HSTRING a = hs_makez(L"identical"); INT32 r = 5; REQUIRE_HS_OPS();
    EXPECT_EQ(WCMP(a, a, &r), S_OK); EXPECT_EQ(r, 0);
    WDEL(a);
}

TEST(winrt_hstring_ops_compare_null_result_is_invalidarg)
{
    HSTRING a = hs_makez(L"a"); REQUIRE_HS_OPS();
    EXPECT_EQ(WCMP(a, a, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCMP(NULL, NULL, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCMP(a, NULL, NULL), HS_E_INVALIDARG);
    WDEL(a);
}

TEST(winrt_hstring_ops_compare_reference_vs_heap)
{
    static const WCHAR s[] = L"fast pass";
    HSTRING_HEADER hdr; HSTRING r, h = hs_makez(s); INT32 res = 9; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(s, 9, &hdr, &r), S_OK);
    EXPECT_EQ(WCMP(r, h, &res), S_OK); EXPECT_EQ(res, 0);
    res = 9; EXPECT_EQ(WCMP(h, r, &res), S_OK); EXPECT_EQ(res, 0);
    EXPECT_NE(r, h);
    WDEL(h);
}

TEST(winrt_hstring_ops_compare_transitive_sort_order)
{
    static const WCHAR *w[] = { L"", L"A", L"AB", L"B", L"a", L"ab", L"b", L"\xD800", L"\xFFFF" };
    unsigned i, j; REQUIRE_HS_OPS();
    for (i = 0; i < ARRAYSIZE(w); i++)
        for (j = 0; j < ARRAYSIZE(w); j++) {
            HSTRING a = hs_makez(w[i]), b = hs_makez(w[j]);
            INT32 r = 0x55;
            EXPECT_EQ(WCMP(a, b, &r), S_OK);
            t_check(r == (i < j ? -1 : i > j ? 1 : 0), __FILE__, __LINE__, "sorted order", "i=%u j=%u r=%d", i, j, r);
            WDEL(a); WDEL(b);
        }
}

/* ------------------------------------------------------------------ Concat */

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_ops_concat_basic)
{
    HSTRING a = hs_makez(L"abc"), b = hs_makez(L"def"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCAT(a, b, &r), S_OK);
    HS_EXPECTZ(r, L"abcdef");
    EXPECT_NE(r, a); EXPECT_NE(r, b);
    HS_EXPECTZ(a, L"abc");                      /* operands untouched */
    HS_EXPECTZ(b, L"def");
    WDEL(a); WDEL(b); WDEL(r);
}

TEST(winrt_hstring_ops_concat_same_operand_twice)
{
    HSTRING a = hs_makez(L"ab"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCAT(a, a, &r), S_OK);
    HS_EXPECTZ(r, L"abab");
    WDEL(a); WDEL(r);
}

TEST(winrt_hstring_ops_concat_null_operands)
{
    HSTRING a = hs_makez(L"abc"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCAT(NULL, NULL, &r), S_OK);
    CHECK(r == NULL);
    r = HS_SENTINEL;
    EXPECT_EQ(WCAT(a, NULL, &r), S_OK);
    EXPECT_EQ(r, a);                            /* heap string: duplicate shares the object */
    WDEL(r);
    r = HS_SENTINEL;
    EXPECT_EQ(WCAT(NULL, a, &r), S_OK);
    EXPECT_EQ(r, a);
    WDEL(r);
    HS_EXPECTZ(a, L"abc");
    WDEL(a);
}

TEST(winrt_hstring_ops_concat_null_output)
{
    HSTRING a = hs_makez(L"abc"); REQUIRE_HS_OPS();
    EXPECT_EQ(WCAT(a, a, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCAT(NULL, NULL, NULL), HS_E_INVALIDARG);
    WDEL(a);
}

TEST(winrt_hstring_ops_concat_reference_with_null_deep_copies)
{
    static const WCHAR s[] = L"refstr";
    HSTRING_HEADER hdr; HSTRING r, o = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(s, 6, &hdr, &r), S_OK);
    EXPECT_EQ(WCAT(r, NULL, &o), S_OK);
    EXPECT_NE(o, r);
    EXPECT_NE(WRAW(o, NULL), s);
    HS_EXPECT(o, s, 6, 0);
    WDEL(o);
    o = HS_SENTINEL;
    EXPECT_EQ(WCAT(NULL, r, &o), S_OK);
    EXPECT_NE(o, r);
    HS_EXPECT(o, s, 6, 0);
    WDEL(o);
}

TEST(winrt_hstring_ops_concat_two_references)
{
    static const WCHAR s1[] = L"left-", s2[] = L"right";
    HSTRING_HEADER h1, h2; HSTRING r1, r2, o = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(s1, 5, &h1, &r1), S_OK);
    EXPECT_EQ(WCSR(s2, 5, &h2, &r2), S_OK);
    EXPECT_EQ(WCAT(r1, r2, &o), S_OK);
    HS_EXPECTZ(o, L"left-right");
    EXPECT_NE(WRAW(o, NULL), s1);
    EXPECT_NE(WRAW(o, NULL), s2);
    WDEL(o);
}

TEST(winrt_hstring_ops_concat_embedded_nuls)
{
    HSTRING a = hs_make(L"a\0b", 3), b = hs_make(L"\0c", 2), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCAT(a, b, &r), S_OK);
    HS_EXPECT(r, L"a\0b\0c", 5, 1);
    WDEL(a); WDEL(b); WDEL(r);
    /* NUL-free parts never gain a NUL */
    a = hs_makez(L"x"); b = hs_makez(L"y");
    EXPECT_EQ(WCAT(a, b, &r), S_OK);
    HS_EXPECT(r, L"xy", 2, 0);
    WDEL(a); WDEL(b); WDEL(r);
}

TEST(winrt_hstring_ops_concat_lengths_table)
{
    static const UINT32 ls[] = { 1, 2, 7, 8, 100, 4096 };
    WCHAR *buf; unsigned i, j; REQUIRE_HS_OPS();
    buf = HeapAlloc(GetProcessHeap(), 0, 8192 * sizeof(WCHAR));
    if (!buf) { t_skip("alloc"); return; }
    for (i = 0; i < 8192; i++) buf[i] = (WCHAR)('a' + i % 26);
    for (i = 0; i < ARRAYSIZE(ls); i++)
        for (j = 0; j < ARRAYSIZE(ls); j++) {
            HSTRING a = hs_make(buf, ls[i]), b = hs_make(buf + 100, ls[j]), r = HS_SENTINEL;
            EXPECT_EQ(WCAT(a, b, &r), S_OK);
            EXPECT_EQ(WLEN(r), ls[i] + ls[j]);
            CHECK(!memcmp(WRAW(r, NULL), buf, ls[i] * sizeof(WCHAR)));
            CHECK(!memcmp(WRAW(r, NULL) + ls[i], buf + 100, ls[j] * sizeof(WCHAR)));
            CHECK(WRAW(r, NULL)[ls[i] + ls[j]] == 0);
            WDEL(a); WDEL(b); WDEL(r);
        }
    HeapFree(GetProcessHeap(), 0, buf);
}

TEST(winrt_hstring_ops_concat_chain)
{
    HSTRING acc = NULL; int i; REQUIRE_HS_OPS();
    for (i = 0; i < 100; i++) {
        HSTRING piece = hs_makez(L"xy"), next = HS_SENTINEL;
        EXPECT_EQ(WCAT(acc, piece, &next), S_OK);
        WDEL(piece);
        WDEL(acc);
        acc = next;
    }
    EXPECT_EQ(WLEN(acc), 200);
    WDEL(acc);
}

/* ------------------------------------------------------------------ Substring */

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_ops_substring_table)
{
    static const WCHAR src[] = L"abcdef";
    unsigned start; HSTRING h = hs_makez(src); REQUIRE_HS_OPS();
    for (start = 0; start <= 6; start++) {
        HSTRING r = HS_SENTINEL;
        EXPECT_EQ(WSUB(h, start, &r), S_OK);
        HS_EXPECT(r, src + start, 6 - start, 0);
        if (start == 6) CHECK(r == NULL);       /* empty result is NULL */
        WDEL(r);
    }
    WDEL(h);
}

TEST(winrt_hstring_ops_substring_start_beyond_length_is_E_BOUNDS)
{
    HSTRING h = hs_makez(L"abcdef"), r; UINT32 st[] = { 7, 8, 100, 0x7fffffff, 0xffffffff }; unsigned i; REQUIRE_HS_OPS();
    for (i = 0; i < ARRAYSIZE(st); i++) {
        r = HS_SENTINEL;
        EXPECT_EQ(WSUB(h, st[i], &r), HS_E_BOUNDS);
        CHECK(r == NULL);
    }
    WDEL(h);
}

TEST(winrt_hstring_ops_substring_null_source)
{
    HSTRING r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WSUB(NULL, 0, &r), S_OK);
    CHECK(r == NULL);
    r = HS_SENTINEL;
    EXPECT_EQ(WSUB(NULL, 1, &r), HS_E_BOUNDS);
    CHECK(r == NULL);
}

TEST(winrt_hstring_ops_substring_null_output)
{
    HSTRING h = hs_makez(L"abc"); REQUIRE_HS_OPS();
    EXPECT_EQ(WSUB(h, 0, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WSUB(NULL, 0, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WSUB(h, 99, NULL), HS_E_INVALIDARG);       /* checked before the bounds */
    WDEL(h);
}

TEST(winrt_hstring_ops_substring_whole_string_is_new_object)
{
    HSTRING h = hs_makez(L"whole"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WSUB(h, 0, &r), S_OK);
    HS_EXPECTZ(r, L"whole");
    EXPECT_NE(r, h);                            /* always a fresh allocation, not a refcount bump */
    EXPECT_NE(WRAW(r, NULL), WRAW(h, NULL));
    WDEL(r); WDEL(h);
}

TEST(winrt_hstring_ops_substring_of_reference)
{
    static const WCHAR s[] = L"reference";
    HSTRING_HEADER hdr; HSTRING h, r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(s, 9, &hdr, &h), S_OK);
    EXPECT_EQ(WSUB(h, 3, &r), S_OK);
    HS_EXPECT(r, L"erence", 6, 0);
    CHECK(WRAW(r, NULL) != s + 3);              /* copied, not aliased */
    WDEL(r);
}

TEST(winrt_hstring_ops_substring_embedded_nuls)
{
    static const WCHAR s[] = L"a\0b\0c";
    HSTRING h = hs_make(s, 5), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WSUB(h, 1, &r), S_OK);
    HS_EXPECT(r, s + 1, 4, 1);
    WDEL(r);
    EXPECT_EQ(WSUB(h, 2, &r), S_OK);
    HS_EXPECT(r, s + 2, 3, 1);
    WDEL(r);
    EXPECT_EQ(WSUB(h, 4, &r), S_OK);
    HS_EXPECT(r, L"c", 1, 0);                    /* tail has no NUL */
    WDEL(r);
    WDEL(h);
}

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_ops_substring_len_table)
{
    static const WCHAR src[] = L"abcdef";
    struct { UINT32 st, n; HRESULT hr; } t[] = {
        { 0, 6, S_OK }, { 0, 5, S_OK }, { 0, 1, S_OK }, { 0, 0, S_OK }, { 1, 5, S_OK }, { 2, 3, S_OK },
        { 5, 1, S_OK }, { 6, 0, S_OK }, { 3, 0, S_OK },
        { 0, 7, HS_E_BOUNDS }, { 1, 6, HS_E_BOUNDS }, { 5, 2, HS_E_BOUNDS }, { 6, 1, HS_E_BOUNDS },
        { 7, 0, HS_E_BOUNDS }, { 7, 1, HS_E_BOUNDS }, { 100, 100, HS_E_BOUNDS },
        { 0, 0xffffffff, HS_E_BOUNDS },           /* start + length == 0xffffffff, no wrap */
        { 1, 0xffffffff, HS_E_OVERFLOW },         /* start + length wraps */
        { 6, 0xffffffff, HS_E_OVERFLOW },
        { 3, 0xfffffffe, HS_E_OVERFLOW },
        { 0xffffffff, 0, HS_E_BOUNDS },           /* start beyond the string wins over overflow */
        { 0xffffffff, 0xffffffff, HS_E_BOUNDS },
        { 7, 0xffffffff, HS_E_BOUNDS },
    };
    HSTRING h = hs_makez(src); unsigned i; REQUIRE_HS_OPS();
    for (i = 0; i < ARRAYSIZE(t); i++) {
        HSTRING r = HS_SENTINEL;
        HRESULT hr;
        if (t_is_wine() && t[i].n >= 0xfffffff0u) continue;   /* wine faults on huge lengths */
        hr = WSUBL(h, t[i].st, t[i].n, &r);
        t_check(hr == t[i].hr, __FILE__, __LINE__, "substring hr", "row %u (%u,%u) got 0x%08lx want 0x%08lx", i, t[i].st, t[i].n, (unsigned long)hr, (unsigned long)t[i].hr);
        if (t[i].hr == S_OK) {
            HS_EXPECT(r, src + t[i].st, t[i].n, 0);
            if (t[i].n == 0) CHECK(r == NULL);
        } else t_check(r == NULL, __FILE__, __LINE__, "r == NULL on failure", "row %u", i);
        if (r == HS_SENTINEL) r = NULL;     /* (a failing implementation may leave the sentinel) */
        WDEL(r);
    }
    WDEL(h);
}

TEST(winrt_hstring_ops_substring_len_null_source_and_output)
{
    HSTRING h = hs_makez(L"abc"), r; REQUIRE_HS_OPS();
    r = HS_SENTINEL; EXPECT_EQ(WSUBL(NULL, 0, 0, &r), S_OK); CHECK(r == NULL);
    r = HS_SENTINEL; EXPECT_EQ(WSUBL(NULL, 0, 1, &r), HS_E_BOUNDS); CHECK(r == NULL);
    r = HS_SENTINEL; EXPECT_EQ(WSUBL(NULL, 1, 0, &r), HS_E_BOUNDS); CHECK(r == NULL);
    EXPECT_EQ(WSUBL(h, 0, 1, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WSUBL(NULL, 0, 0, NULL), HS_E_INVALIDARG);
    WDEL(h);
}

TEST(winrt_hstring_ops_substring_len_embedded_and_reference)
{
    static const WCHAR s[] = L"01\0" L"34\0" L"7";
    HSTRING_HEADER hdr; HSTRING r, o = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(s, 7, &hdr, &r), S_OK);
    EXPECT_EQ(WSUBL(r, 1, 3, &o), S_OK);
    HS_EXPECT(o, s + 1, 3, 1);
    WDEL(o);
    EXPECT_EQ(WSUBL(r, 3, 2, &o), S_OK);
    HS_EXPECT(o, s + 3, 2, 0);
    WDEL(o);
}

TEST(winrt_hstring_ops_substring_roundtrip_with_concat)
{
    static const WCHAR s[] = L"The quick brown fox";
    HSTRING h = hs_makez(s); unsigned i; REQUIRE_REP();
    for (i = 0; i <= 19; i++) {
        HSTRING l = HS_SENTINEL, r = HS_SENTINEL, j = HS_SENTINEL; INT32 c = 7;
        EXPECT_EQ(WSUBL(h, 0, i, &l), S_OK);
        EXPECT_EQ(WSUB(h, i, &r), S_OK);
        EXPECT_EQ(WCAT(l, r, &j), S_OK);
        EXPECT_EQ(WCMP(h, j, &c), S_OK);
        EXPECT_EQ(c, 0);
        WDEL(l); WDEL(r); WDEL(j);
    }
    WDEL(h);
}

/* ------------------------------------------------------------------ Replace */

struct rep { const WCHAR *src, *find, *with, *exp; };

static void run_rep_table(const struct rep *t, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        HSTRING s = t[i].src ? hs_makez(t[i].src) : NULL;
        HSTRING f = t[i].find ? hs_makez(t[i].find) : NULL;
        HSTRING w = t[i].with ? hs_makez(t[i].with) : NULL;
        HSTRING r = HS_SENTINEL;
        HRESULT hr = WREP(s, f, w, &r);
        t_check(hr == S_OK, __FILE__, __LINE__, "replace S_OK", "row %u (\"%ls\") hr=0x%08lx", i, t[i].src ? t[i].src : L"(null)", (unsigned long)hr);
        if (hr == S_OK) {
            UINT32 el = (UINT32)lstrlenW(t[i].exp);
            hs_expect(r, t[i].exp, el, 0, __FILE__, __LINE__);
            if (el == 0) t_check(r == NULL, __FILE__, __LINE__, "empty result is NULL", "row %u", i);
        }
        WDEL(r); WDEL(s); WDEL(f); WDEL(w);
    }
}

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_ops_replace_table)
{
    static const struct rep t[] = {
        { L"abcabc", L"b", L"X", L"aXcaXc" },
        { L"abcabc", L"bc", L"", L"aa" },
        { L"abcabc", L"bc", NULL, L"aa" },        /* NULL replacement deletes */
        { L"abc", L"abc", L"", L"" },
        { L"abc", L"abc", L"xyz", L"xyz" },
        { L"abc", L"x", L"y", L"abc" },           /* not found */
        { L"abc", L"abcd", L"x", L"abc" },        /* pattern longer than string */
        { L"aaa", L"aa", L"b", L"ba" },           /* non overlapping, left to right */
        { L"aaaa", L"aa", L"a", L"aa" },
        { L"aaaa", L"aa", L"aa", L"aaaa" },
        { L"aaaaa", L"aa", L"b", L"bba" },
        { L"abc", L"b", L"bb", L"abbc" },         /* longer replacement */
        { L"abc", L"b", L"xb", L"axbc" },         /* replacement text is not rescanned */
        { L"abc", L"c", L"xyz", L"abxyz" },
        { L"abc", L"a", L"xyz", L"xyzbc" },
        { L"abab", L"ab", L"ba", L"baba" },       /* same length path */
        { L"abab", L"ab", L"ab", L"abab" },
        { L"aA", L"a", L"x", L"xA" },             /* case sensitive */
        { L"Hello World", L"o", L"0", L"Hell0 W0rld" },
        { L"Hello World", L"World", L"There", L"Hello There" },
        { L"Hello World", L"l", L"", L"Heo Word" },
        { L"a b c", L" ", L"_", L"a_b_c" },
        { L"a b c", L" ", L"", L"abc" },
        { L"xyz", L"xyz", L"xyz", L"xyz" },
        { L"aXbXc", L"X", L"XX", L"aXXbXXc" },
        { L"mississippi", L"ss", L"SS", L"miSSiSSippi" },
        { L"mississippi", L"issi", L"[]", L"m[]ssippi" },
        { L"abcabc", L"cab", L"-", L"ab-c" },
    };
    REQUIRE_REP();
    run_rep_table(t, ARRAYSIZE(t));
}

TEST(winrt_hstring_ops_replace_null_string_is_empty_result)
{
    HSTRING f = hs_makez(L"a"), w = hs_makez(L"b"), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(NULL, f, w, &r), S_OK);
    CHECK(r == NULL);
    r = HS_SENTINEL;
    EXPECT_EQ(WREP(NULL, f, NULL, &r), S_OK);
    CHECK(r == NULL);
    WDEL(f); WDEL(w);
}

TEST(winrt_hstring_ops_replace_empty_search_is_invalidarg)
{
    HSTRING s = hs_makez(L"abc"), w = hs_makez(L"x"), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(s, NULL, w, &r), HS_E_INVALIDARG);
    CHECK(r == NULL);
    r = HS_SENTINEL;
    EXPECT_EQ(WREP(NULL, NULL, w, &r), HS_E_INVALIDARG);   /* checked before the string is examined */
    CHECK(r == NULL);
    r = HS_SENTINEL;
    EXPECT_EQ(WREP(s, NULL, NULL, &r), HS_E_INVALIDARG);
    CHECK(r == NULL);
    WDEL(s); WDEL(w);
}

TEST(winrt_hstring_ops_replace_null_output)
{
    HSTRING s = hs_makez(L"abc"), f = hs_makez(L"b"); REQUIRE_REP();
    EXPECT_EQ(WREP(s, f, f, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WREP(NULL, NULL, NULL, NULL), HS_E_INVALIDARG);
    WDEL(s); WDEL(f);
}

TEST(winrt_hstring_ops_replace_not_found_returns_copy_not_same_object)
{
    HSTRING s = hs_makez(L"abcdef"), f = hs_makez(L"xyz"), w = hs_makez(L"1"), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(s, f, w, &r), S_OK);
    HS_EXPECTZ(r, L"abcdef");
    EXPECT_NE(r, s);                            /* a distinct string even when nothing changed */
    EXPECT_NE(WRAW(r, NULL), WRAW(s, NULL));
    WDEL(r); WDEL(s); WDEL(f); WDEL(w);
}

TEST(winrt_hstring_ops_replace_reference_operands)
{
    static const WCHAR ss[] = L"one two one", sf[] = L"one", sw[] = L"1";
    HSTRING_HEADER h1, h2, h3; HSTRING s, f, w, r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WCSR(ss, 11, &h1, &s), S_OK);
    EXPECT_EQ(WCSR(sf, 3, &h2, &f), S_OK);
    EXPECT_EQ(WCSR(sw, 1, &h3, &w), S_OK);
    EXPECT_EQ(WREP(s, f, w, &r), S_OK);
    HS_EXPECTZ(r, L"1 two 1");
    WDEL(r);
}

TEST(winrt_hstring_ops_replace_embedded_nuls)
{
    HSTRING s = hs_make(L"a\0b\0c", 5), f = hs_make(L"\0", 1), w = hs_makez(L"-"), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(s, f, w, &r), S_OK);
    HS_EXPECT(r, L"a-b-c", 5, 0);                /* NULs matched like any other char */
    WDEL(r);
    r = HS_SENTINEL;
    WDEL(f); f = hs_makez(L"b");
    EXPECT_EQ(WREP(s, f, NULL, &r), S_OK);
    HS_EXPECT(r, L"a\0\0c", 4, 1);
    WDEL(r);
    WDEL(s); WDEL(f); WDEL(w);
}

TEST(winrt_hstring_ops_replace_with_embedded_nul_replacement)
{
    HSTRING s = hs_makez(L"a-b"), f = hs_makez(L"-"), w = hs_make(L"\0\0", 2), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(s, f, w, &r), S_OK);
    HS_EXPECT(r, L"a\0\0b", 4, 1);
    WDEL(r); WDEL(s); WDEL(f); WDEL(w);
}

TEST(winrt_hstring_ops_replace_source_not_modified)
{
    HSTRING s = hs_makez(L"banana"), f = hs_makez(L"an"), w = hs_makez(L"AN"), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(s, f, w, &r), S_OK);
    HS_EXPECTZ(r, L"bANANa");
    HS_EXPECTZ(s, L"banana");
    WDEL(r); WDEL(s); WDEL(f); WDEL(w);
}

TEST(winrt_hstring_ops_replace_result_overlap_with_same_arg)
{
    /* the same HSTRING used for string, find and replacement */
    HSTRING s = hs_makez(L"ab"), r = HS_SENTINEL; REQUIRE_REP();
    EXPECT_EQ(WREP(s, s, s, &r), S_OK);
    HS_EXPECTZ(r, L"ab");
    WDEL(r);
    WDEL(s);
}

TEST(winrt_hstring_ops_replace_growth_many_matches)
{
    WCHAR *buf; HSTRING s, f = hs_makez(L"a"), w = hs_makez(L"xyz"), r = HS_SENTINEL; REQUIRE_REP();
    buf = HeapAlloc(GetProcessHeap(), 0, 10001 * sizeof(WCHAR));
    if (!buf) { t_skip("alloc"); return; }
    { int i; for (i = 0; i < 10000; i++) buf[i] = L'a'; buf[10000] = 0; }
    s = hs_make(buf, 10000);
    EXPECT_EQ(WREP(s, f, w, &r), S_OK);
    EXPECT_EQ(WLEN(r), 30000);
    CHECK(!wcsncmp(WRAW(r, NULL), L"xyzxyzxyz", 9));
    WDEL(r); WDEL(s); WDEL(f); WDEL(w);
    HeapFree(GetProcessHeap(), 0, buf);
}

/* ------------------------------------------------------------------ Trim */

struct trim { const WCHAR *src, *set, *start, *end; };

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_ops_trim_table)
{
    static const struct trim t[] = {
        { L"  abc  ", L" ", L"abc  ", L"  abc" },
        { L"abc", L" ", L"abc", L"abc" },
        { L"xyabcyx", L"xy", L"abcyx", L"xyabc" },
        { L"xyabcyx", L"yx", L"abcyx", L"xyabc" },    /* set, not a sequence */
        { L"xyxy", L"xy", L"", L"" },                  /* everything trimmed */
        { L"x", L"x", L"", L"" },
        { L"xxxx", L"x", L"", L"" },
        { L"Abc", L"a", L"Abc", L"Abc" },              /* case sensitive */
        { L"abcA", L"A", L"abcA", L"abc" },
        { L"aXbXa", L"a", L"XbXa", L"aXbX" },
        { L"..a.b..", L".", L"a.b..", L"..a.b" },
        { L"\t\r\n x \n", L" \t\r\n", L"x \n", L"\t\r\n x" },
        { L"123abc321", L"0123456789", L"abc321", L"123abc" },
        { L"abc", L"abcdef", L"", L"" },               /* trim set larger than string */
        { L"abc", L"aaaaaa", L"bc", L"abc" },          /* duplicates in the set */
        { L"abc", L"c", L"abc", L"ab" },
        { L"abc", L"a", L"bc", L"abc" },
        { L"ababab", L"ab", L"", L"" },
        { L"a b", L" ", L"a b", L"a b" },              /* interior chars are never trimmed */
    };
    unsigned i; REQUIRE_HS_OPS();
    for (i = 0; i < ARRAYSIZE(t); i++) {
        HSTRING s = hs_makez(t[i].src), set = hs_makez(t[i].set), r = HS_SENTINEL;
        UINT32 n;
        EXPECT_EQ(WTRS(s, set, &r), S_OK);
        n = (UINT32)lstrlenW(t[i].start);
        t_check(WLEN(r) == n && !memcmp(WRAW(r, NULL), t[i].start, n * 2), __FILE__, __LINE__, "TrimStart result", "row %u", i);
        if (n == 0) CHECK(r == NULL);
        WDEL(r); r = HS_SENTINEL;
        EXPECT_EQ(WTRE(s, set, &r), S_OK);
        n = (UINT32)lstrlenW(t[i].end);
        t_check(WLEN(r) == n && !memcmp(WRAW(r, NULL), t[i].end, n * 2), __FILE__, __LINE__, "TrimEnd result", "row %u", i);
        if (n == 0) CHECK(r == NULL);
        if (n) CHECK(WRAW(r, NULL)[n] == 0);
        WDEL(r);
        HS_EXPECT(s, t[i].src, (UINT32)lstrlenW(t[i].src), 0);   /* source untouched */
        WDEL(s); WDEL(set);
    }
}

TEST(winrt_hstring_ops_trim_nothing_trimmed_shares_heap_string)
{
    HSTRING s = hs_makez(L"abc"), set = hs_makez(L"x"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WTRS(s, set, &r), S_OK);
    EXPECT_EQ(r, s);                            /* duplicate (refcount) */
    WDEL(r); r = HS_SENTINEL;
    EXPECT_EQ(WTRE(s, set, &r), S_OK);
    EXPECT_EQ(r, s);
    WDEL(r);
    WDEL(s); WDEL(set);
}

TEST(winrt_hstring_ops_trim_nothing_trimmed_reference_deep_copies)
{
    static const WCHAR str[] = L"abc";
    HSTRING_HEADER hdr; HSTRING s, set = hs_makez(L"x"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(str, 3, &hdr, &s), S_OK);
    EXPECT_EQ(WTRS(s, set, &r), S_OK);
    EXPECT_NE(r, s);
    EXPECT_NE(WRAW(r, NULL), str);
    HS_EXPECT(r, str, 3, 0);
    WDEL(r); r = HS_SENTINEL;
    EXPECT_EQ(WTRE(s, set, &r), S_OK);
    EXPECT_NE(r, s);
    HS_EXPECT(r, str, 3, 0);
    WDEL(r);
    WDEL(set);
}

TEST(winrt_hstring_ops_trim_something_trimmed_is_new_object)
{
    HSTRING s = hs_makez(L" abc"), set = hs_makez(L" "), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WTRS(s, set, &r), S_OK);
    HS_EXPECTZ(r, L"abc");
    EXPECT_NE(r, s);
    WDEL(r);
    WDEL(s); WDEL(set);
}

TEST(winrt_hstring_ops_trim_empty_trimstring_is_invalidarg)
{
    HSTRING s = hs_makez(L"abc"), r; REQUIRE_HS_OPS();
    r = HS_SENTINEL; EXPECT_EQ(WTRS(s, NULL, &r), HS_E_INVALIDARG); CHECK(r == NULL);
    r = HS_SENTINEL; EXPECT_EQ(WTRE(s, NULL, &r), HS_E_INVALIDARG); CHECK(r == NULL);
    r = HS_SENTINEL; EXPECT_EQ(WTRS(NULL, NULL, &r), HS_E_INVALIDARG); CHECK(r == NULL);  /* before the source check */
    r = HS_SENTINEL; EXPECT_EQ(WTRE(NULL, NULL, &r), HS_E_INVALIDARG); CHECK(r == NULL);
    WDEL(s);
}

TEST(winrt_hstring_ops_trim_null_source_is_empty)
{
    HSTRING set = hs_makez(L"x"), r; REQUIRE_HS_OPS();
    r = HS_SENTINEL; EXPECT_EQ(WTRS(NULL, set, &r), S_OK); CHECK(r == NULL);
    r = HS_SENTINEL; EXPECT_EQ(WTRE(NULL, set, &r), S_OK); CHECK(r == NULL);
    WDEL(set);
}

TEST(winrt_hstring_ops_trim_null_output)
{
    HSTRING s = hs_makez(L"abc"), set = hs_makez(L"a"); REQUIRE_HS_OPS();
    EXPECT_EQ(WTRS(s, set, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WTRE(s, set, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WTRS(NULL, NULL, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WTRE(NULL, NULL, NULL), HS_E_INVALIDARG);
    WDEL(s); WDEL(set);
}

TEST(winrt_hstring_ops_trim_embedded_nuls_in_source)
{
    HSTRING s = hs_make(L"\0\0ab\0", 5), set = hs_make(L"\0", 1), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WTRS(s, set, &r), S_OK);
    HS_EXPECT(r, L"ab\0", 3, 1);
    WDEL(r);
    r = HS_SENTINEL;
    EXPECT_EQ(WTRE(s, set, &r), S_OK);
    HS_EXPECT(r, L"\0\0ab", 4, 1);
    WDEL(r);
    WDEL(s); WDEL(set);
}

TEST(winrt_hstring_ops_trim_start_all_nul_set_observed)
{
    /* TrimStart scans one element past the end (the terminator), so a trim set containing NUL
     * and a string made only of trimmed chars is an odd corner: observation only. */
    HSTRING s = hs_make(L"\0\0", 2), set = hs_make(L"\0", 1), r = HS_SENTINEL; HRESULT hr; REQUIRE_HS_OPS();
    hr = WTRS(s, set, &r);
    t_note("TrimStart(all-NUL, {NUL}) hr=0x%08lx result=%p", (unsigned long)hr, (void *)r);
    if (SUCCEEDED(hr)) WDEL(r);
    r = HS_SENTINEL;
    hr = WTRE(s, set, &r);
    t_note("TrimEnd(all-NUL, {NUL}) hr=0x%08lx result=%p", (unsigned long)hr, (void *)r);
    /* Windows 10: TrimEnd of an all-NUL string by a {NUL} set fails with HS_E_LENGTH (0x80080011)
     * and leaves the output NULL (observed); S_OK with an empty result is also tolerated. */
    CHECK(hr == S_OK || hr == HS_E_LENGTH);
    CHECK(r == NULL);
    WDEL(s); WDEL(set);
}

TEST(winrt_hstring_ops_trim_set_may_be_reference_and_same_object)
{
    static const WCHAR set[] = L"-+";
    HSTRING_HEADER hdr; HSTRING hset, s = hs_makez(L"+-text-+"), r = HS_SENTINEL; REQUIRE_HS_OPS();
    EXPECT_EQ(WCSR(set, 2, &hdr, &hset), S_OK);
    EXPECT_EQ(WTRS(s, hset, &r), S_OK);
    HS_EXPECTZ(r, L"text-+");
    WDEL(r); r = HS_SENTINEL;
    EXPECT_EQ(WTRE(s, hset, &r), S_OK);
    HS_EXPECTZ(r, L"+-text");
    WDEL(r); r = HS_SENTINEL;
    EXPECT_EQ(WTRS(s, s, &r), S_OK);            /* string trimmed by itself: first char is in the set */
    t_note("TrimStart(s,s) len=%u", WLEN(r));
    WDEL(r);
    WDEL(s);
}

TEST(winrt_hstring_ops_trim_long_runs)
{
    WCHAR *buf; HSTRING s, set = hs_makez(L"z"), r = HS_SENTINEL; int i; REQUIRE_HS_OPS();
    buf = HeapAlloc(GetProcessHeap(), 0, 20001 * sizeof(WCHAR));
    if (!buf) { t_skip("alloc"); return; }
    for (i = 0; i < 20000; i++) buf[i] = (i < 5000 || i >= 15000) ? L'z' : L'q';
    s = hs_make(buf, 20000);
    EXPECT_EQ(WTRS(s, set, &r), S_OK);
    EXPECT_EQ(WLEN(r), 15000);
    WDEL(r); r = HS_SENTINEL;
    EXPECT_EQ(WTRE(s, set, &r), S_OK);
    EXPECT_EQ(WLEN(r), 15000);
    WDEL(r);
    WDEL(s); WDEL(set);
    HeapFree(GetProcessHeap(), 0, buf);
}
