/* bstr_core: SysAllocString / SysAllocStringLen / SysAllocStringByteLen / SysFreeString / SysStringLen / SysStringByteLen.
 * Windows 10 x64 oleaut32 (decomp-verified): layout is [DWORD pad=0][DWORD byteLen][data...][WCHAR 0]; the BSTR
 * points at data; the pointer is 8 mod 16 on x64 (note only). */
#include "bc.h"

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_alloc_null_is_null)
{
    REQUIRE(U(SysAllocString));
    CHECK(U(SysAllocString)(NULL) == NULL);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_alloc_simple_layout)
{
    static const WCHAR t[] = L"Test";
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(t);
    CHECK(b != NULL); if (!b) return;
    CHECK(b != t);
    EXPECT_EQ(BLEN(b), 8);
    CHECK(!memcmp(b, t, sizeof(t)));
    EXPECT_EQ(b[4], 0);
    EXPECT_EQ(((ULONG_PTR)b) & 7, 0);
    if (((ULONG_PTR)b) & 15) t_note("BSTR pointer %% 16 = %u", (unsigned)(((ULONG_PTR)b) & 15));
    t_note("pad DWORD before length prefix = 0x%lx", (unsigned long)((const DWORD *)b)[-2]);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_empty_string)
{
    static const WCHAR e[] = L"";
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(e);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(U(SysStringLen)(b), 0);
    EXPECT_EQ(U(SysStringByteLen)(b), 0);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_stops_at_first_nul)
{
    static const WCHAR s[] = { 'a', 'b', 0, 'c', 'd', 0 };
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(s);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 2);
    EXPECT_EQ(BLEN(b), 4);
    CHECK(bc_is_wstr(b, s, 2));
    U(SysFreeString)(b);
}

TEST(bstr_alloc_copies_input)
{
    WCHAR src[8] = L"orig";
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(src);
    CHECK(b != NULL); if (!b) return;
    src[0] = 'X';
    EXPECT_EQ(b[0], 'o');
    b[1] = 'Z';
    EXPECT_EQ(src[1], 'r');
    U(SysFreeString)(b);
}

TEST(bstr_alloc_nonbmp_and_highchars)
{
    static const WCHAR s[] = { 0xD83D, 0xDE00, 0xFFFE, 0x00FF, 0x8000, 0 };
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(s);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 5);
    EXPECT_EQ(U(SysStringByteLen)(b), 10);
    CHECK(!memcmp(b, s, 12));
    U(SysFreeString)(b);
}

/* table: every length 0..70 through SysAllocString and SysAllocStringLen */
TEST(bstr_alloc_len_sweep)
{
    WCHAR src[96];
    UINT n, i;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    for (i = 0; i < 95; i++) src[i] = (WCHAR)(0x21 + i);
    src[95] = 0;
    for (n = 0; n <= 70; n++) {
        BSTR b = U(SysAllocStringLen)(src, n);
        CHECKF(b != NULL, "n=%u", n);
        if (!b) continue;
        CHECKF(BLEN(b) == n * 2, "n=%u prefix %lu", n, (unsigned long)BLEN(b));
        CHECKF(U(SysStringLen)(b) == n, "n=%u", n);
        CHECKF(U(SysStringByteLen)(b) == n * 2, "n=%u", n);
        CHECKF(!memcmp(b, src, n * 2), "n=%u content", n);
        CHECKF(b[n] == 0, "n=%u terminator", n);
        U(SysFreeString)(b);
    }
    for (n = 0; n <= 70; n++) {
        WCHAR tmp[96];
        BSTR b;
        memcpy(tmp, src, sizeof(tmp)); tmp[n] = 0;
        b = U(SysAllocString)(tmp);
        CHECKF(b != NULL, "n=%u", n);
        if (!b) continue;
        CHECKF(bc_is_wstr(b, src, n), "n=%u", n);
        U(SysFreeString)(b);
    }
}

TEST(bstr_alloc_len_null_src_empty)
{
    BSTR b;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringLen)(NULL, 0);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(U(SysStringLen)(b), 0);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_len_nonnull_src_zero_len)
{
    static const WCHAR s[] = L"abc";
    BSTR b;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringLen)(s, 0);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 0);
    EXPECT_EQ(b[0], 0);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_len_null_src_uninitialised)
{
    /* len>0, NULL source: buffer is NOT initialised (contents unspecified) but the length prefix and the
     * wide NUL terminator are set. */
    UINT lens[] = { 1, 2, 3, 7, 8, 9, 16, 100, 1000 };
    unsigned i;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        BSTR b = U(SysAllocStringLen)(NULL, lens[i]);
        CHECKF(b != NULL, "len=%u", lens[i]);
        if (!b) continue;
        CHECKF(BLEN(b) == lens[i] * 2, "len=%u", lens[i]);
        CHECKF(U(SysStringLen)(b) == lens[i], "len=%u", lens[i]);
        CHECKF(b[lens[i]] == 0, "terminator len=%u", lens[i]);
        U(SysFreeString)(b);
    }
}

TEST(bstr_alloc_len_embedded_nuls)
{
    static const WCHAR s[] = { 'a', 0, 'b', 0, 0, 'c' };
    BSTR b;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringLen)(s, 6);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 6);
    EXPECT_EQ(U(SysStringByteLen)(b), 12);
    CHECK(!memcmp(b, s, sizeof(s)));
    EXPECT_EQ(b[6], 0);
    EXPECT_EQ(lstrlenW(b), 1);   /* a C-string view stops at the first NUL */
    U(SysFreeString)(b);
}

TEST(bstr_alloc_len_does_not_need_source_terminator)
{
    WCHAR src[8] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h' };   /* no NUL */
    BSTR b;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringLen)(src, 3);
    CHECK(b != NULL); if (!b) return;
    CHECK(bc_is_wstr(b, src, 3));
    EXPECT_EQ(lstrlenW(b), 3);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_len_overflow_returns_null)
{
    /* Pure arithmetic: ui*2 >= 2^32 or ui*2+0x19 wraps. No allocation is attempted on Windows 10. */
    UINT bad[] = { 0x7FFFFFF4u, 0x7FFFFFF8u, 0x7FFFFFFFu, 0x80000000u, 0x80000001u, 0xFFFFFFF0u, 0xFFFFFFFFu };
    unsigned i;
    REQUIRE(U(SysAllocStringLen));
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        BSTR b = U(SysAllocStringLen)(NULL, bad[i]);
        CHECKF(b == NULL, "len=0x%x -> %p", bad[i], b);
        if (b) U(SysFreeString)(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_alloc_bytelen_null_zero)
{
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)(NULL, 0);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 0);
    EXPECT_EQ(b[0], 0);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_bytelen_sweep)
{
    unsigned char src[80];
    UINT n;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    bc_fill(src, sizeof(src), 7);
    for (n = 0; n < 4; n++) src[n] |= 1;
    for (n = 0; n <= 70; n++) {
        const unsigned char *p;
        BSTR b = U(SysAllocStringByteLen)((LPCSTR)src, n);
        CHECKF(b != NULL, "n=%u", n);
        if (!b) continue;
        p = (const unsigned char *)b;
        CHECKF(BLEN(b) == n, "n=%u prefix=%lu", n, (unsigned long)BLEN(b));
        CHECKF(U(SysStringByteLen)(b) == n, "n=%u", n);
        CHECKF(U(SysStringLen)(b) == n / 2, "n=%u SysStringLen", n);
        CHECKF(!memcmp(p, src, n), "n=%u content", n);
        CHECKF(p[n] == 0, "n=%u byte NUL at [n]", n);
        /* the 'unicode' terminator: two zero bytes at offset (n+1)&~1 */
        CHECKF(p[(n + 1) & ~1u] == 0 && p[((n + 1) & ~1u) + 1] == 0, "n=%u wide NUL", n);
        U(SysFreeString)(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_alloc_bytelen_odd_truncates_and_terminates)
{
    static const char a[6] = { 'T', 'e', 's', 't', 0, '?' };
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)(a, 3);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 3);
    EXPECT_EQ(U(SysStringByteLen)(b), 3);
    EXPECT_EQ(U(SysStringLen)(b), 1);
    CHECK(((char *)b)[0] == 'T' && ((char *)b)[1] == 'e' && ((char *)b)[2] == 's' && ((char *)b)[3] == 0);
    EXPECT_EQ(((char *)b)[3], 0);
    EXPECT_EQ(((char *)b)[4], 0);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_bytelen_one_byte)
{
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)("\xff", 1);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringByteLen)(b), 1);
    EXPECT_EQ(U(SysStringLen)(b), 0);
    EXPECT_EQ(((unsigned char *)b)[0], 0xff);
    EXPECT_EQ(((unsigned char *)b)[1], 0);
    EXPECT_EQ(((unsigned char *)b)[2], 0);
    U(SysFreeString)(b);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_alloc_bytelen_wide_content)
{
    static const WCHAR w[] = L"Test";
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)((LPCSTR)w, 8);
    CHECK(b != NULL); if (!b) return;
    CHECK(bc_eq_w(b, w));
    EXPECT_EQ(U(SysStringLen)(b), 4);
    U(SysFreeString)(b);
}

TEST(bstr_alloc_bytelen_null_src_len_set)
{
    UINT lens[] = { 1, 2, 3, 5, 17, 100, 4097 };
    unsigned i;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    for (i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        BSTR b = U(SysAllocStringByteLen)(NULL, lens[i]);
        CHECKF(b != NULL, "len=%u", lens[i]);
        if (!b) continue;
        CHECKF(BLEN(b) == lens[i], "len=%u", lens[i]);
        CHECKF(((unsigned char *)b)[lens[i]] == 0, "terminator len=%u", lens[i]);
        U(SysFreeString)(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_alloc_bytelen_overflow_returns_null)
{
    /* len + 0x19 wraps for len >= 0xFFFFFFE7 on Windows 10 (decomp), so no allocation is attempted. */
    UINT bad[] = { 0xFFFFFFE7u, 0xFFFFFFF0u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    unsigned i;
    REQUIRE(U(SysAllocStringByteLen));
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        BSTR b = U(SysAllocStringByteLen)("Test", bad[i]);
        CHECKF(b == NULL, "len=0x%x -> %p", bad[i], b);
        b = U(SysAllocStringByteLen)(NULL, bad[i]);
        CHECKF(b == NULL, "NULL src len=0x%x -> %p", bad[i], b);
    }
}

TEST(bstr_alloc_bytelen_embedded_nul_bytes)
{
    static const char s[] = { 0, 1, 0, 2, 0, 0, 3 };
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)(s, 7);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(BLEN(b), 7);
    CHECK(!memcmp(b, s, 7));
    EXPECT_EQ(U(SysStringLen)(b), 3);
    U(SysFreeString)(b);
}

TEST(bstr_stringlen_null)
{
    REQUIRE(U(SysStringLen)); REQUIRE(U(SysStringByteLen));
    EXPECT_EQ(U(SysStringLen)(NULL), 0);
    EXPECT_EQ(U(SysStringByteLen)(NULL), 0);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(bstr_stringlen_reads_prefix_not_terminator)
{
    /* A hand-built buffer: SysStringLen/SysStringByteLen only read DWORD at p[-1]. */
    struct { DWORD pad; DWORD len; WCHAR s[8]; } fake;
    BSTR p = fake.s;
    DWORD vals[] = { 0, 1, 2, 3, 4, 5, 6, 100, 101, 0xFFFFFFFEu, 0xFFFFFFFFu, 0x80000000u };
    unsigned i;
    REQUIRE(U(SysStringLen)); REQUIRE(U(SysStringByteLen));
    memset(&fake, 0, sizeof(fake));
    wcscpy(fake.s, L"abcdefg");
    for (i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        fake.len = vals[i];
        CHECKF(U(SysStringByteLen)(p) == vals[i], "val=0x%lx", (unsigned long)vals[i]);
        CHECKF(U(SysStringLen)(p) == vals[i] / 2, "val=0x%lx", (unsigned long)vals[i]);
    }
}

TEST(bstr_stringlen_with_embedded_nul_counts_all)
{
    static const WCHAR s[] = { 0, 0, 0, 0 };
    BSTR b;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringLen)(s, 4);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringLen)(b), 4);
    EXPECT_EQ(U(SysStringByteLen)(b), 8);
    U(SysFreeString)(b);
}

TEST(bstr_free_null_is_noop)
{
    REQUIRE(U(SysFreeString));
    SET_ERR(0x1234);
    U(SysFreeString)(NULL);
    U(SysFreeString)(NULL);
    CHECK(1);
}

TEST(bstr_alloc_many_live_distinct)
{
    enum { N = 1500 };
    static BSTR arr[N];
    WCHAR buf[40];
    int i;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    for (i = 0; i < N; i++) {
        wsprintfW(buf, L"item-%d", i);
        arr[i] = U(SysAllocString)(buf);
        CHECKF(arr[i] != NULL, "i=%d", i);
    }
    for (i = 0; i < N; i++) {
        int j;
        if (!arr[i]) continue;
        wsprintfW(buf, L"item-%d", i);
        CHECKF(bc_eq_w(arr[i], buf), "i=%d content", i);
        for (j = i + 1; j < i + 4 && j < N; j++) CHECKF(arr[i] != arr[j], "distinct %d %d", i, j);
    }
    for (i = 0; i < N; i++) U(SysFreeString)(arr[i]);
}

TEST(bstr_alloc_large_string)
{
    enum { N = 1000000 };
    WCHAR *src = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (N + 1) * sizeof(WCHAR));
    BSTR b;
    UINT i;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    if (!src) { t_skip("no memory"); return; }
    for (i = 0; i < N; i++) src[i] = (WCHAR)(1 + i % 60000);
    b = U(SysAllocStringLen)(src, N);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(U(SysStringLen)(b), N);
        EXPECT_EQ(U(SysStringByteLen)(b), 2 * N);
        CHECK(!memcmp(b, src, N * sizeof(WCHAR)));
        EXPECT_EQ(b[N], 0);
        U(SysFreeString)(b);
    }
    HeapFree(GetProcessHeap(), 0, src);
}

TEST(bstr_alloc_block_size_notes)
{
    /* Windows rounds the request to (bytes+0x19)&~0xf; the BSTR sits 8 bytes into the block. Observational. */
    UINT n;
    REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    for (n = 0; n <= 64; n += 8) {
        BSTR b = U(SysAllocStringLen)(NULL, n);
        if (!b) continue;
        SIZE_T hs = HeapSize(GetProcessHeap(), 0, (char *)b - 8);
        t_note("n=%u chars: HeapSize(process heap, p-8) = %lld (expected rounded ~%u)", n, (long long)(SSIZE_T)hs,
               (unsigned)((n * 2 + 0x19) & ~0xfu));
        U(SysFreeString)(b);
    }
}

/* ---- cross-thread ---- */
static BSTR g_thread_bstr;
static DWORD WINAPI th_alloc(LPVOID p)
{
    g_thread_bstr = U(SysAllocString)(L"made on a worker thread");
    return 0;
}
static DWORD WINAPI th_free(LPVOID p)
{
    U(SysFreeString)((BSTR)p);
    return 0;
}

TEST(bstr_alloc_free_across_threads)
{
    HANDLE h;
    BSTR b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    /* allocated on a thread that has exited, used and freed here */
    g_thread_bstr = NULL;
    h = CreateThread(NULL, 0, th_alloc, NULL, 0, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    CloseHandle(h);
    b = g_thread_bstr;
    CHECK(b != NULL);
    if (b) {
        CHECK(bc_eq_w(b, L"made on a worker thread"));
        U(SysFreeString)(b);
    }
    /* allocated here, freed on another thread */
    b = U(SysAllocString)(L"made on main thread, freed elsewhere");
    CHECK(b != NULL);
    if (b) {
        h = CreateThread(NULL, 0, th_free, b, 0, NULL);
        CHECK(h != NULL);
        if (h) { EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0); CloseHandle(h); }
    }
}

TEST(bstr_alloc_free_reuse_note)
{
    BSTR a, b;
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    a = U(SysAllocString)(L"reuse-me");
    CHECK(a != NULL); if (!a) return;
    U(SysFreeString)(a);
    b = U(SysAllocString)(L"reuse-me");
    CHECK(b != NULL);
    t_note("small block pointer reuse after free: %s (per-thread cache)", a == b ? "yes" : "no");
    if (b) U(SysFreeString)(b);
}

TEST(bstr_alloc_ansi_roundtrip_bytelen)
{
    static const char ansi[] = "hello, world";   /* 12 bytes + NUL */
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)(ansi, 12);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(SysStringByteLen)(b), 12);
    EXPECT_EQ(U(SysStringLen)(b), 6);
    CHECK(!strcmp((const char *)b, ansi));
    U(SysFreeString)(b);
}
