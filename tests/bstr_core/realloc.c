/* bstr_core: SysReAllocString / SysReAllocStringLen / SysAddRefString / SysReleaseString / SetOaNoCache.
 * Decomp-verified (Win10 x64): SysReAllocString(&b, NULL) frees b, sets NULL, returns TRUE; otherwise it is
 * SysReAllocStringLen(&b, psz, wcslen(psz)). SysReAllocStringLen treats psz == *pbstr as "psz == NULL"
 * (keep contents); overflow lengths return FALSE and leave *pbstr untouched. */
#include "bc.h"

TEST(bstr_realloc_null_to_null)
{
    BSTR b = NULL;
    REQUIRE(U(SysReAllocString));
    EXPECT_EQ(U(SysReAllocString)(&b, NULL), TRUE);
    CHECK(b == NULL);
}

TEST(bstr_realloc_null_psz_frees_and_nulls)
{
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocString));
    b = U(SysAllocString)(L"to be released");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysReAllocString)(&b, NULL), TRUE);
    CHECK(b == NULL);
}

TEST(bstr_realloc_from_null_allocates)
{
    BSTR b = NULL;
    REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(SysReAllocString)(&b, L"fresh"), TRUE);
    CHECK(b != NULL);
    if (b) { CHECK(bc_eq_w(b, L"fresh")); U(SysFreeString)(b); }
}

TEST(bstr_realloc_empty_string)
{
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"something");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysReAllocString)(&b, L""), TRUE);
    CHECK(b != NULL);
    if (b) { EXPECT_EQ(BLEN(b), 0); EXPECT_EQ(b[0], 0); U(SysFreeString)(b); }
}

TEST(bstr_realloc_grow_shrink_content)
{
    static const WCHAR *seq[] = { L"a", L"abcdefghijklmnopqrstuvwxyz0123456789", L"xy", L"", L"back again to a medium string" };
    BSTR b;
    unsigned i;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"start");
    CHECK(b != NULL); if (!b) return;
    for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        EXPECT_EQ(U(SysReAllocString)(&b, seq[i]), TRUE);
        CHECKF(b != NULL && bc_eq_w(b, seq[i]), "step %u", i);
        if (!b) return;
    }
    U(SysFreeString)(b);
}

TEST(bstr_realloc_same_pointer_arg)
{
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"same string");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysReAllocString)(&b, b), TRUE);
    CHECK(b != NULL);
    if (b) { CHECK(bc_eq_w(b, L"same string")); U(SysFreeString)(b); }
}

TEST(bstr_realloc_len_sweep_from_src)
{
    WCHAR src[100];
    UINT n, m;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    for (n = 0; n < 99; n++) src[n] = (WCHAR)('A' + n % 26);
    src[99] = 0;
    for (m = 0; m <= 60; m += 3) {
        BSTR b = U(SysAllocStringLen)(L"seed", 4);
        CHECK(b != NULL); if (!b) continue;
        for (n = 0; n <= 80; n += (n < 20 ? 1 : 7)) {
            EXPECT_EQ(U(SysReAllocStringLen)(&b, src, n), TRUE);
            CHECKF(b != NULL && bc_is_wstr(b, src, n), "m=%u n=%u", m, n);
            if (!b) break;
        }
        U(SysFreeString)(b);
    }
}

TEST(bstr_realloc_len_null_src_keeps_prefix)
{
    /* psz == NULL: only the length changes; the existing characters up to min(old,new) are kept. */
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"0123456789");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysReAllocStringLen)(&b, NULL, 4), TRUE);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 4);
    EXPECT_EQ(b[4], 0);
    CHECK(!memcmp(b, L"0123", 8));
    EXPECT_EQ(U(SysReAllocStringLen)(&b, NULL, 40), TRUE);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 40);
    EXPECT_EQ(BLEN(b), 80);
    EXPECT_EQ(b[40], 0);
    CHECK(!memcmp(b, L"0123", 8));
    U(SysFreeString)(b);
}

TEST(bstr_realloc_len_self_pointer_keeps_prefix)
{
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"abcdefghij");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysReAllocStringLen)(&b, b, 5), TRUE);
    CHECK(b != NULL); if (!b) return;
    CHECK(bc_is_wstr(b, L"abcde", 5));
    EXPECT_EQ(U(SysReAllocStringLen)(&b, b, 12), TRUE);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 12);
    CHECK(!memcmp(b, L"abcde", 10));
    EXPECT_EQ(b[12], 0);
    U(SysFreeString)(b);
}

TEST(bstr_realloc_len_from_null_bstr_and_null_src)
{
    BSTR b = NULL;
    REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(SysReAllocStringLen)(&b, NULL, 5), TRUE);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 10);
    EXPECT_EQ(b[5], 0);
    U(SysFreeString)(b);
}

/* SysReAllocStringLen(&b, NULL, 0) with b == NULL: a non-NULL empty BSTR. */
TEST(bstr_realloc_len_zero_from_null)
{
    BSTR b = NULL;
    REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(SysReAllocStringLen)(&b, NULL, 0), TRUE);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 0);
    EXPECT_EQ(b[0], 0);
    U(SysFreeString)(b);
}

TEST(bstr_realloc_len_embedded_nuls)
{
    static const WCHAR s[] = { 'a', 0, 'b', 0, 'c' };
    BSTR b = NULL;
    REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    EXPECT_EQ(U(SysReAllocStringLen)(&b, s, 5), TRUE);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 5);
    CHECK(!memcmp(b, s, sizeof(s)));
    EXPECT_EQ(b[5], 0);
    U(SysFreeString)(b);
}

TEST(bstr_realloc_len_overflow_keeps_bstr)
{
    UINT bad[] = { 0x7FFFFFF4u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu };
    unsigned i;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocStringLen)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        BSTR b = U(SysAllocString)(L"keep me"), orig = b;
        CHECK(b != NULL); if (!b) return;
        EXPECT_EQ(U(SysReAllocStringLen)(&b, NULL, bad[i]), FALSE);
        CHECKF(b == orig, "len=0x%x pbstr modified", bad[i]);
        CHECK(bc_eq_w(b, L"keep me"));
        U(SysFreeString)(b);
    }
    {
        BSTR b = NULL;
        EXPECT_EQ(U(SysReAllocStringLen)(&b, NULL, 0xFFFFFFFFu), FALSE);
        CHECK(b == NULL);
    }
}

TEST(bstr_realloc_returns_true_is_one)
{
    BSTR b = NULL;
    INT r;
    REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    r = U(SysReAllocString)(&b, L"x");
    EXPECT_EQ(r, 1);
    r = U(SysReAllocString)(&b, L"yz");
    EXPECT_EQ(r, 1);
    U(SysFreeString)(b);
}

TEST(bstr_realloc_pointer_stability_note)
{
    BSTR b = U(SysAllocString)(L"0123456789abcdef"), orig = b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    if (!b) return;
    U(SysReAllocString)(&b, L"01234567");
    t_note("shrink within block: pointer %s", b == orig ? "unchanged" : "moved");
    orig = b;
    U(SysReAllocString)(&b, L"01234567890123456789012345678901234567890123456789");
    t_note("grow beyond block: pointer %s", b == orig ? "unchanged" : "moved");
    U(SysFreeString)(b);
}

/* ---- pinning (SysAddRefString / SysReleaseString) ---- */
TEST(bstr_pin_keeps_alive_across_free)
{
    BSTR b, c;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    REQUIRE(UN(SysAddRefString)); REQUIRE(UN(SysReleaseString));
    b = U(SysAllocString)(L"pinned string");
    CHECK(b != NULL); if (!b) return;
    UN(SysAddRefString)(b);
    U(SysFreeString)(b);                 /* consumes the base reference; pin keeps the memory */
    c = U(SysAllocString)(L"pinned string");
    CHECK(c != b);                       /* b's block can not have been recycled while pinned */
    CHECK(bc_eq_w(b, L"pinned string"));
    EXPECT_EQ(U(SysStringLen)(b), 13);
    UN(SysReleaseString)(b);             /* last reference: freed */
    U(SysFreeString)(c);
}

TEST(bstr_pin_two_pins)
{
    BSTR b, c;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    REQUIRE(UN(SysAddRefString)); REQUIRE(UN(SysReleaseString));
    b = U(SysAllocString)(L"double pinned");
    CHECK(b != NULL); if (!b) return;
    UN(SysAddRefString)(b);
    UN(SysAddRefString)(b);
    U(SysFreeString)(b);
    UN(SysReleaseString)(b);
    c = U(SysAllocString)(L"double pinned");
    CHECK(c != b);
    CHECK(bc_eq_w(b, L"double pinned"));
    UN(SysReleaseString)(b);
    U(SysFreeString)(c);
}

TEST(bstr_pin_release_unpinned_acts_as_free)
{
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(UN(SysReleaseString));
    b = U(SysAllocString)(L"released without addref");
    CHECK(b != NULL); if (!b) return;
    UN(SysReleaseString)(b);
    CHECK(1);   /* must not crash or leak into later allocations */
    b = U(SysAllocString)(L"after");
    CHECK(b != NULL && bc_eq_w(b, L"after"));
    BC_FREE(b);
}

TEST(bstr_pin_realloc_pinned_gets_new_block)
{
    BSTR b, old;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysReAllocString)); REQUIRE(U(SysFreeString));
    REQUIRE(UN(SysAddRefString)); REQUIRE(UN(SysReleaseString));
    b = U(SysAllocString)(L"will be pinned");
    CHECK(b != NULL); if (!b) return;
    old = b;
    UN(SysAddRefString)(b);
    EXPECT_EQ(U(SysReAllocString)(&b, L"replacement"), TRUE);
    CHECK(b != NULL);
    CHECK(b != old);
    if (b) CHECK(bc_eq_w(b, L"replacement"));
    BC_FREE(b);
    UN(SysReleaseString)(old);   /* the pin we added; base reference was consumed by the realloc */
}

/* ---- SetOaNoCache: process-wide; run in a child so other tests keep default caching ---- */
CHILD(nocache)
{
    int mode = argc > 1 ? atoi(argv[1]) : 0;   /* 0 = default caching, 1 = after SetOaNoCache */
    int reuse = 0, bad = 0, i;
    if (mode) {
        if (!UN(SetOaNoCache)) return 100;
        UN(SetOaNoCache)();
    }
    for (i = 0; i < 200; i++) {
        BSTR a = U(SysAllocString)(L"cache probe string 0123456789");
        BSTR b;
        if (!a) return 101;
        U(SysFreeString)(a);
        b = U(SysAllocString)(L"cache probe string 0123456789");
        if (!b) return 102;
        if (a == b) reuse++;
        if (!bc_eq_w(b, L"cache probe string 0123456789")) bad++;
        U(SysFreeString)(b);
    }
    if (bad) return 103;
    /* functionality after the switch: allocate/grow/free */
    {
        BSTR b = U(SysAllocStringLen)(NULL, 10);
        if (!b || BLEN(b) != 20) return 104;
        if (!U(SysReAllocStringLen)(&b, L"xyz", 3) || !bc_is_wstr(b, L"xyz", 3)) return 105;
        U(SysFreeString)(b);
    }
    return 200 + (reuse > 50 ? 1 : 0);   /* 200: little reuse, 201: heavy reuse */
}

TEST(bstr_nocache_default_mode_child)
{
    DWORD r;
    REQUIRE(U(SysAllocString));
    r = t_run_child("nocache", "0", 20000);
    CHECKF(r == 200 || r == 201, "child exit %lu", (unsigned long)r);
    t_note("default caching: pointer reuse %s", r == 201 ? "heavy (cache active)" : "light");
}

TEST(bstr_nocache_after_setoanocache_child)
{
    DWORD r;
    REQUIRE(U(SysAllocString));
    REQUIRE(UN(SetOaNoCache));
    r = t_run_child("nocache", "1", 20000);
    CHECKF(r == 200 || r == 201, "child exit %lu (100=no export,101-105=alloc failures)", (unsigned long)r);
    t_note("after SetOaNoCache: pointer reuse %s", r == 201 ? "heavy" : "light/none");
}
