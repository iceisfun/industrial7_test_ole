/* WindowsCreateString / CreateStringReference / DeleteString / Duplicate / accessors.
 * Expectations verified against the Windows 10 combase decompilation. */
#include "hs.h"

/* ------------------------------------------------------------------ CreateString */

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_core_create_basic)
{
    static const WCHAR str[] = L"abcdef";
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(str, 6, &h), S_OK);
    HS_EXPECT(h, str, 6, 0);
    EXPECT_NE(WRAW(h, NULL), str);              /* deep copy of the source */
    EXPECT_EQ(WDEL(h), S_OK);
}

TEST(winrt_hstring_core_create_length_not_nul_terminated)
{
    /* length rules, not the terminator: only the first n chars are used */
    static const WCHAR str[] = L"abcdef";
    UINT32 n; REQUIRE_HS();
    for (n = 1; n <= 6; n++) {
        HSTRING h = HS_SENTINEL;
        EXPECT_EQ(WCS(str, n, &h), S_OK);
        HS_EXPECT(h, str, n, 0);
        WDEL(h);
    }
}

TEST(winrt_hstring_core_create_prefix_of_longer_buffer_terminated)
{
    static const WCHAR str[] = L"hello world";
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(str, 5, &h), S_OK);
    HS_EXPECT(h, L"hello", 5, 0);
    CHECK(WRAW(h, NULL)[5] == 0);
    WDEL(h);
}

TEST(winrt_hstring_core_create_zero_length_yields_null)
{
    static const WCHAR str[] = L"abc";
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(str, 0, &h), S_OK);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCS(NULL, 0, &h), S_OK);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCS(L"", 0, &h), S_OK);
    CHECK(h == NULL);
}

TEST(winrt_hstring_core_create_null_source_nonzero_length_is_E_POINTER)
{
    UINT32 lens[] = { 1, 2, 100, 0x7fffffff, 0x80000000, 0xffffffff };
    unsigned i; REQUIRE_HS();
    for (i = 0; i < ARRAYSIZE(lens); i++) {
        HSTRING h = HS_SENTINEL;
        EXPECT_EQ(WCS(NULL, lens[i], &h), HS_E_POINTER);
        CHECK(h == NULL);                       /* output is cleared before validation */
    }
}

TEST(winrt_hstring_core_create_null_output_is_E_INVALIDARG)
{
    REQUIRE_HS();
    EXPECT_EQ(WCS(L"abc", 3, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCS(NULL, 0, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCS(L"abc", 0, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCS(NULL, 5, NULL), HS_E_INVALIDARG);   /* output check precedes the pointer check */
}

TEST(winrt_hstring_core_create_length_too_large)
{
    /* Sizes whose byte count (2*len + header) does not fit 32 bits fail before the source is read. */
    static const WCHAR str[] = L"x";
    UINT32 lens[] = { 0x7ffffff0, 0x7ffffff8, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe, 0xffffffff };
    unsigned i; REQUIRE_HS();
    if (t_is_wine()) { t_skip("wine reads the source before validating the length"); return; }
    for (i = 0; i < ARRAYSIZE(lens); i++) {
        HSTRING h = HS_SENTINEL;
        HRESULT hr = WCS(str, lens[i], &h);
        t_check(hr == HS_E_LENGTH, __FILE__, __LINE__, "hr == 0x80080011", "len 0x%x got 0x%08lx", lens[i], (unsigned long)hr);
        CHECK(h == NULL);
    }
}

TEST(winrt_hstring_core_create_embedded_nuls)
{
    static const WCHAR str[] = L"a\0c\0ef";
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(str, 6, &h), S_OK);
    HS_EXPECT(h, str, 6, 1);
    WDEL(h);
    /* embedded NUL as the very first / last char */
    EXPECT_EQ(WCS(L"\0abc", 4, &h), S_OK);
    HS_EXPECT(h, L"\0abc", 4, 1);
    WDEL(h);
    EXPECT_EQ(WCS(L"abc\0", 4, &h), S_OK);
    HS_EXPECT(h, L"abc\0", 4, 1);
    WDEL(h);
    /* a prefix that stops before the first NUL has none */
    EXPECT_EQ(WCS(L"abc\0def", 3, &h), S_OK);
    HS_EXPECT(h, L"abc", 3, 0);
    WDEL(h);
    /* single NUL character is a string of length 1 */
    EXPECT_EQ(WCS(L"\0", 1, &h), S_OK);
    HS_EXPECT(h, L"\0", 1, 1);
    WDEL(h);
}

TEST(winrt_hstring_core_create_all_nul_string)
{
    static const WCHAR z[8] = {0};
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(z, 8, &h), S_OK);
    HS_EXPECT(h, z, 8, 1);
    EXPECT_EQ(WLEN(h), 8);
    WDEL(h);
}

TEST(winrt_hstring_core_create_copy_is_independent_of_source)
{
    WCHAR buf[8] = L"abcdefg";
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(buf, 7, &h), S_OK);
    buf[0] = L'Z';
    HS_EXPECT(h, L"abcdefg", 7, 0);
    WDEL(h);
}

TEST(winrt_hstring_core_create_two_calls_distinct_objects)
{
    HSTRING a = NULL, b = NULL; REQUIRE_HS();
    EXPECT_EQ(WCS(L"same", 4, &a), S_OK);
    EXPECT_EQ(WCS(L"same", 4, &b), S_OK);
    EXPECT_NE(a, b);                            /* no interning */
    EXPECT_NE(WRAW(a, NULL), WRAW(b, NULL));
    WDEL(a); WDEL(b);
}

TEST(winrt_hstring_core_create_non_bmp_and_unpaired_surrogates)
{
    static const WCHAR s[] = { 0xD83D, 0xDE00, 0xD800, 0xDC00, 0xDC00, 0xD800, 0xFFFF, 0xFFFE };
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCS(s, 8, &h), S_OK);             /* a string is just 16-bit units, never validated */
    HS_EXPECT(h, s, 8, 0);
    WDEL(h);
}

TEST(winrt_hstring_core_create_lengths_table)
{
    static const UINT32 lens[] = { 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 255, 256, 257,
                                   1000, 4095, 4096, 4097, 65535, 65536, 100001 };
    WCHAR *src; unsigned i; UINT32 j; REQUIRE_HS();
    src = HeapAlloc(GetProcessHeap(), 0, 100002 * sizeof(WCHAR));
    if (!src) { t_skip("alloc"); return; }
    for (j = 0; j < 100001; j++) src[j] = (WCHAR)(1 + (j % 0xfffe));
    src[100001] = 0;
    for (i = 0; i < ARRAYSIZE(lens); i++) {
        HSTRING h = HS_SENTINEL;
        EXPECT_EQ(WCS(src, lens[i], &h), S_OK);
        HS_EXPECT(h, src, lens[i], 0);
        WDEL(h);
    }
    HeapFree(GetProcessHeap(), 0, src);
}

TEST(winrt_hstring_core_create_very_long_string)
{
    const UINT32 n = 8u * 1024 * 1024;           /* 16 MB of UTF-16 */
    WCHAR *src; HSTRING h = HS_SENTINEL, d = HS_SENTINEL; UINT32 i; int ok = 1; REQUIRE_HS();
    src = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)n * sizeof(WCHAR));
    if (!src) { t_skip("alloc"); return; }
    for (i = 0; i < n; i++) src[i] = (WCHAR)(i % 251 + 1);
    EXPECT_EQ(WCS(src, n, &h), S_OK);
    EXPECT_EQ(WLEN(h), n);
    CHECK(memcmp(WRAW(h, NULL), src, (SIZE_T)n * sizeof(WCHAR)) == 0);
    CHECK(WRAW(h, NULL)[n] == 0);
    EXPECT_EQ(WDUP(h, &d), S_OK);
    EXPECT_EQ(d, h);
    WDEL(d); WDEL(h);
    HeapFree(GetProcessHeap(), 0, src);
}

TEST(winrt_hstring_core_create_is_heap_backed_not_stack)
{
    /* the object may be freed with a single WindowsDeleteString, and survives the source going away */
    HSTRING h = HS_SENTINEL; REQUIRE_HS();
    {
        WCHAR tmp[4] = L"tmp";
        EXPECT_EQ(WCS(tmp, 3, &h), S_OK);
        memset(tmp, 0xAA, sizeof(tmp));
    }
    HS_EXPECT(h, L"tmp", 3, 0);
    EXPECT_EQ(WDEL(h), S_OK);
}

/* ------------------------------------------------------------- CreateStringReference */

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_core_ref_basic)
{
    static const WCHAR str[] = L"abcdef";
    HSTRING_HEADER hdr; HSTRING h = HS_SENTINEL; REQUIRE_HS();
    memset(&hdr, 0xcc, sizeof(hdr));
    EXPECT_EQ(WCSR(str, 6, &hdr, &h), S_OK);
    HS_EXPECT(h, str, 6, 0);
    EXPECT_EQ(WRAW(h, NULL), str);              /* fast-pass string exposes the caller's buffer */
    EXPECT_EQ(h, (HSTRING)&hdr);                /* and the HSTRING is the header itself */
    EXPECT_EQ(WDEL(h), S_OK);                   /* no-op for a reference */
    HS_EXPECT(h, str, 6, 0);                    /* still usable after Delete */
}

TEST(winrt_hstring_core_ref_header_size_and_layout)
{
    static const WCHAR str[] = L"abcdef";
    HSTRING_HEADER hdr; HSTRING h; unsigned char *b = (unsigned char *)&hdr; REQUIRE_HS();
    CHECK(sizeof(HSTRING_HEADER) == 24);        /* x64 */
    CHECK(sizeof(void *) == 8);
    memset(&hdr, 0, sizeof(hdr));
    EXPECT_EQ(WCSR(str, 6, &hdr, &h), S_OK);
    /* Observed Windows 10 layout of the opaque header: flags (bit0 = reference) | length | pad | buffer ptr. */
    CHECK((*(DWORD *)b & 1) == 1);
    EXPECT_EQ(*(DWORD *)(b + 4), 6);
    EXPECT_EQ(*(const WCHAR **)(b + 16), str);
}

TEST(winrt_hstring_core_ref_header_alignment_is_pointer_size)
{
    /* the header must be usable at 8-byte alignment (natural for the struct) */
    static const WCHAR str[] = L"align";
    struct { char pad; HSTRING_HEADER h; } s; HSTRING h; REQUIRE_HS();
    CHECK(((ULONG_PTR)&s.h & 7) == 0);
    EXPECT_EQ(WCSR(str, 5, &s.h, &h), S_OK);
    HS_EXPECT(h, str, 5, 0);
}

TEST(winrt_hstring_core_ref_requires_nul_at_str_len)
{
    static const WCHAR str[] = L"abcdef";       /* str[6] == 0, but str[3] == 'd' */
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 3, &hdr, &h), HS_E_NOTNULLTERM);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 6, &hdr, &h), S_OK);    /* exactly at the terminator */
    CHECK(h != NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 5, &hdr, &h), HS_E_NOTNULLTERM);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 1, &hdr, &h), HS_E_NOTNULLTERM);
    CHECK(h == NULL);
}

TEST(winrt_hstring_core_ref_zero_length_checks_terminator_and_yields_null)
{
    static const WCHAR str[] = L"abc";
    static const WCHAR empty[] = L"";
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    /* str[0] is not NUL, so even length 0 is rejected */
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 0, &hdr, &h), HS_E_NOTNULLTERM);
    CHECK(h == NULL);
    /* an empty string is the NULL HSTRING; the header is not used */
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(empty, 0, &hdr, &h), S_OK);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(NULL, 0, &hdr, &h), S_OK);
    CHECK(h == NULL);
}

TEST(winrt_hstring_core_ref_null_source_nonzero_length)
{
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(NULL, 1, &hdr, &h), HS_E_POINTER);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(NULL, 0x7fffffff, &hdr, &h), HS_E_POINTER);
    CHECK(h == NULL);
}

TEST(winrt_hstring_core_ref_length_minus_one_is_invalidarg)
{
    static const WCHAR str[] = L"abc";
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    if (t_is_wine()) { t_skip("wine reads str[len] first"); return; }
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 0xffffffff, &hdr, &h), HS_E_INVALIDARG);
    CHECK(h == NULL);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(NULL, 0xffffffff, &hdr, &h), HS_E_INVALIDARG);   /* checked before the NULL source */
    CHECK(h == NULL);
}

TEST(winrt_hstring_core_ref_null_output_and_header)
{
    static const WCHAR str[] = L"abc";
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    EXPECT_EQ(WCSR(str, 3, &hdr, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCSR(NULL, 0, &hdr, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WCSR(str, 3, NULL, NULL), HS_E_INVALIDARG);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 3, NULL, &h), HS_E_INVALIDARG);
    EXPECT_EQ(h, HS_SENTINEL);                  /* output left untouched when the header is missing */
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 0xffffffff, NULL, &h), HS_E_INVALIDARG);
}

TEST(winrt_hstring_core_ref_embedded_nuls)
{
    static const WCHAR str[] = L"a\0c\0ef";     /* str[6] == 0 */
    HSTRING_HEADER hdr; HSTRING h = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCSR(str, 6, &hdr, &h), S_OK);
    HS_EXPECT(h, str, 6, 1);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 1, &hdr, &h), S_OK);    /* str[1] == 0 terminates a 1 char prefix */
    HS_EXPECT(h, L"a", 1, 0);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 3, &hdr, &h), S_OK);    /* str[3] == 0 */
    HS_EXPECT(h, str, 3, 1);
    h = HS_SENTINEL;
    EXPECT_EQ(WCSR(str, 2, &hdr, &h), HS_E_NOTNULLTERM);   /* str[2] == 'c' */
}

TEST(winrt_hstring_core_ref_aliases_live_buffer)
{
    WCHAR buf[6] = L"hello";
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    EXPECT_EQ(WCSR(buf, 5, &hdr, &h), S_OK);
    buf[0] = L'J';
    HS_EXPECT(h, L"Jello", 5, 0);               /* no copy was made */
}

TEST(winrt_hstring_core_ref_delete_many_times_is_noop)
{
    static const WCHAR str[] = L"keep";
    HSTRING_HEADER hdr; HSTRING h; int i; REQUIRE_HS();
    EXPECT_EQ(WCSR(str, 4, &hdr, &h), S_OK);
    for (i = 0; i < 100; i++) EXPECT_EQ(WDEL(h), S_OK);
    HS_EXPECT(h, str, 4, 0);
}

TEST(winrt_hstring_core_ref_header_reuse)
{
    static const WCHAR a[] = L"first", b[] = L"second";
    HSTRING_HEADER hdr; HSTRING h; REQUIRE_HS();
    EXPECT_EQ(WCSR(a, 5, &hdr, &h), S_OK);
    HS_EXPECT(h, a, 5, 0);
    EXPECT_EQ(WCSR(b, 6, &hdr, &h), S_OK);
    HS_EXPECT(h, b, 6, 0);
    EXPECT_EQ(WRAW(h, NULL), b);
}

TEST(winrt_hstring_core_ref_failure_does_not_modify_header_on_bad_terminator)
{
    static const WCHAR str[] = L"abc";
    HSTRING_HEADER hdr, copy; HSTRING h; REQUIRE_HS();
    memset(&hdr, 0x5a, sizeof(hdr)); copy = hdr;
    EXPECT_EQ(WCSR(str, 2, &hdr, &h), HS_E_NOTNULLTERM);
    CHECK(memcmp(&hdr, &copy, sizeof(hdr)) == 0);
    EXPECT_EQ(WCSR(NULL, 4, &hdr, &h), HS_E_POINTER);
    CHECK(memcmp(&hdr, &copy, sizeof(hdr)) == 0);
}

/* ------------------------------------------------------------------ DeleteString */

TEST(winrt_hstring_core_delete_null_is_ok)
{
    REQUIRE_HS();
    EXPECT_EQ(WDEL(NULL), S_OK);
    EXPECT_EQ(WDEL(NULL), S_OK);
}

TEST(winrt_hstring_core_delete_returns_ok_for_heap_string)
{
    HSTRING h = hs_makez(L"delete me"); REQUIRE_HS();
    EXPECT_EQ(WDEL(h), S_OK);
}

/* ------------------------------------------------------------------ DuplicateString */

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_core_dup_heap_string_shares_object)
{
    HSTRING h = hs_makez(L"shared"), d = HS_SENTINEL, d2 = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WDUP(h, &d), S_OK);
    EXPECT_EQ(d, h);                            /* refcount bump, same object */
    EXPECT_EQ(WRAW(d, NULL), WRAW(h, NULL));
    EXPECT_EQ(WDUP(d, &d2), S_OK);
    EXPECT_EQ(d2, h);
    HS_EXPECTZ(d2, L"shared");
    EXPECT_EQ(WDEL(h), S_OK);                   /* still alive: two refs left */
    HS_EXPECTZ(d, L"shared");
    EXPECT_EQ(WDEL(d), S_OK);
    HS_EXPECTZ(d2, L"shared");
    EXPECT_EQ(WDEL(d2), S_OK);
}

TEST(winrt_hstring_core_dup_null)
{
    HSTRING d = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WDUP(NULL, &d), S_OK);
    CHECK(d == NULL);
}

TEST(winrt_hstring_core_dup_null_output)
{
    HSTRING h = hs_makez(L"x"); REQUIRE_HS();
    EXPECT_EQ(WDUP(h, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WDUP(NULL, NULL), HS_E_INVALIDARG);
    WDEL(h);
}

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_core_dup_reference_is_deep_copy)
{
    static const WCHAR str[] = L"reference";
    HSTRING_HEADER hdr; HSTRING r, d = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCSR(str, 9, &hdr, &r), S_OK);
    EXPECT_EQ(WDUP(r, &d), S_OK);
    EXPECT_NE(d, r);
    CHECK(d != NULL);
    EXPECT_NE(WRAW(d, NULL), str);              /* owns its own buffer */
    EXPECT_NE(WRAW(d, NULL), WRAW(r, NULL));
    HS_EXPECT(d, str, 9, 0);
    /* the copy is a heap string: duplicating it again shares */
    {
        HSTRING d2 = HS_SENTINEL;
        EXPECT_EQ(WDUP(d, &d2), S_OK);
        EXPECT_EQ(d2, d);
        WDEL(d2);
    }
    WDEL(d);
}

TEST(winrt_hstring_core_dup_reference_copy_outlives_source_buffer)
{
    WCHAR buf[8] = L"volatil";
    HSTRING_HEADER hdr; HSTRING r, d = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCSR(buf, 7, &hdr, &r), S_OK);
    EXPECT_EQ(WDUP(r, &d), S_OK);
    { int k; for (k = 0; k < 7; k++) buf[k] = L'Q'; }
    HS_EXPECT(d, L"volatil", 7, 0);
    HS_EXPECT(r, L"QQQQQQQ", 7, 0);             /* the reference still aliases */
    WDEL(d);
}

TEST(winrt_hstring_core_dup_reference_with_embedded_nuls)
{
    static const WCHAR str[] = L"a\0b\0c";
    HSTRING_HEADER hdr; HSTRING r, d = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WCSR(str, 5, &hdr, &r), S_OK);
    EXPECT_EQ(WDUP(r, &d), S_OK);
    HS_EXPECT(d, str, 5, 1);
    WDEL(d);
}

TEST(winrt_hstring_core_dup_clears_output_first)
{
    HSTRING d = HS_SENTINEL; REQUIRE_HS();
    EXPECT_EQ(WDUP(NULL, &d), S_OK);
    CHECK(d == NULL);
}

TEST(winrt_hstring_core_dup_many_refs)
{
    HSTRING h = hs_makez(L"many"), d[200]; int i; REQUIRE_HS();
    for (i = 0; i < 200; i++) { d[i] = HS_SENTINEL; EXPECT_EQ(WDUP(h, &d[i]), S_OK); EXPECT_EQ(d[i], h); }
    for (i = 0; i < 200; i++) EXPECT_EQ(WDEL(d[i]), S_OK);
    HS_EXPECTZ(h, L"many");
    WDEL(h);
}

/* ------------------------------------------------------------------ accessors */

TEST(winrt_hstring_core_len_null_is_zero)
{
    REQUIRE_HS();
    EXPECT_EQ(WLEN(NULL), 0);
}

TEST(winrt_hstring_core_len_values)
{
    static const WCHAR s[] = L"0123456789";
    unsigned i; REQUIRE_HS();
    for (i = 1; i <= 10; i++) {
        HSTRING h = hs_make(s, i);
        EXPECT_EQ(WLEN(h), i);
        WDEL(h);
    }
}

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_core_rawbuffer_null_hstring)
{
    UINT32 len = 0xdeadbeef; const WCHAR *p, *p2; REQUIRE_HS();
    p = WRAW(NULL, &len);
    CHECK(p != NULL);
    EXPECT_EQ(len, 0);
    if (p) CHECK(p[0] == 0);
    p2 = WRAW(NULL, NULL);
    CHECK(p2 != NULL);
    if (p2) CHECK(p2[0] == 0);
    EXPECT_EQ(p, p2);                           /* the same static empty string */
}

TEST(winrt_hstring_core_rawbuffer_length_out_optional)
{
    HSTRING h = hs_makez(L"opt"); REQUIRE_HS();
    CHECK(WRAW(h, NULL) != NULL);
    EXPECT_WSTR(WRAW(h, NULL), L"opt");
    WDEL(h);
}

TEST(winrt_hstring_core_rawbuffer_stable_across_calls)
{
    HSTRING h = hs_makez(L"stable"); const WCHAR *a, *b; REQUIRE_HS();
    a = WRAW(h, NULL); b = WRAW(h, NULL);
    EXPECT_EQ(a, b);
    WDEL(h);
}

TEST(winrt_hstring_core_rawbuffer_length_counts_chars_not_bytes)
{
    HSTRING h = hs_make(L"abcd\0efg", 8); UINT32 len = 0; REQUIRE_HS();
    WRAW(h, &len);
    EXPECT_EQ(len, 8);
    WDEL(h);
}

TEST(winrt_hstring_core_isempty_values)
{
    HSTRING_HEADER hdr; HSTRING r; HSTRING h; REQUIRE_HS();
    EXPECT_EQ(WEMPTY(NULL), TRUE);
    h = hs_makez(L"a");
    EXPECT_EQ(WEMPTY(h), FALSE);
    WDEL(h);
    h = hs_make(L"\0", 1);
    EXPECT_EQ(WEMPTY(h), FALSE);                /* NUL chars still count */
    WDEL(h);
    EXPECT_EQ(WCSR(L"x", 1, &hdr, &r), S_OK);
    EXPECT_EQ(WEMPTY(r), FALSE);
    EXPECT_EQ(WCSR(L"", 0, &hdr, &r), S_OK);
    EXPECT_EQ(WEMPTY(r), TRUE);
}

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_core_embedded_null_table)
{
    struct { const WCHAR *s; UINT32 n; int nul; } t[] = {
        { L"a", 1, 0 }, { L"abc", 3, 0 }, { L"\0", 1, 1 }, { L"a\0", 2, 1 }, { L"\0a", 2, 1 },
        { L"ab\0cd", 5, 1 }, { L"ab\0cd", 2, 0 }, { L"ab\0cd", 3, 1 }, { L"\0\0\0", 3, 1 },
        { L"abcdefghijklmnopqrstuvwxyz", 26, 0 },
    };
    unsigned i; REQUIRE_HS();
    for (i = 0; i < ARRAYSIZE(t); i++) {
        HSTRING h = hs_make(t[i].s, t[i].n);
        BOOL b = 0xdead;
        EXPECT_EQ(WEMB(h, &b), S_OK);
        EXPECT_EQ(b, t[i].nul);
        b = 0xdead;                             /* second query (cached internally) must agree */
        EXPECT_EQ(WEMB(h, &b), S_OK);
        EXPECT_EQ(b, t[i].nul);
        WDEL(h);
    }
}

TEST(winrt_hstring_core_embedded_null_null_hstring_and_args)
{
    BOOL b = 0xdead; HSTRING h = hs_makez(L"x"); REQUIRE_HS();
    EXPECT_EQ(WEMB(NULL, &b), S_OK);
    EXPECT_EQ(b, FALSE);
    EXPECT_EQ(WEMB(h, NULL), HS_E_INVALIDARG);
    EXPECT_EQ(WEMB(NULL, NULL), HS_E_INVALIDARG);
    WDEL(h);
}

TEST(winrt_hstring_core_embedded_null_reference_string)
{
    static const WCHAR s1[] = L"abc", s2[] = L"a\0c";
    HSTRING_HEADER h1, h2; HSTRING r1, r2; BOOL b; REQUIRE_HS();
    EXPECT_EQ(WCSR(s1, 3, &h1, &r1), S_OK);
    EXPECT_EQ(WCSR(s2, 3, &h2, &r2), S_OK);
    b = 0xdead; EXPECT_EQ(WEMB(r1, &b), S_OK); EXPECT_EQ(b, FALSE);
    b = 0xdead; EXPECT_EQ(WEMB(r2, &b), S_OK); EXPECT_EQ(b, TRUE);
    /* the result is exactly 0/1 */
    b = 0xdead; WEMB(r2, &b); EXPECT_EQ(b, 1);
    /* header still describes the same string afterwards */
    HS_EXPECT(r1, s1, 3, 0);
    HS_EXPECT(r2, s2, 3, 1);
    EXPECT_EQ(WRAW(r2, NULL), s2);
}

/* -------------------------------------------------- state sharing / thread safety */

struct dup_ctx { HSTRING h; int iters; volatile LONG fail; };
static DWORD WINAPI dup_thread(void *p)
{
    struct dup_ctx *c = p; int i;
    for (i = 0; i < c->iters; i++) {
        HSTRING d = NULL;
        if (WDUP(c->h, &d) != S_OK || d != c->h) { InterlockedIncrement(&c->fail); break; }
        if (WRAW(d, NULL)[0] != L's') { InterlockedIncrement(&c->fail); break; }
        WDEL(d);
    }
    return 0;
}

TEST(winrt_hstring_core_threads_duplicate_delete_balanced)
{
    struct dup_ctx c; HANDLE th[4]; int i; DWORD r; REQUIRE_HS();
    c.h = hs_makez(L"shared string"); c.iters = 20000; c.fail = 0;
    for (i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, dup_thread, &c, 0, NULL);
    r = WaitForMultipleObjects(4, th, TRUE, 10000);
    CHECK(r == WAIT_OBJECT_0);
    for (i = 0; i < 4; i++) CloseHandle(th[i]);
    EXPECT_EQ(c.fail, 0);
    HS_EXPECTZ(c.h, L"shared string");
    EXPECT_EQ(WDEL(c.h), S_OK);
}

TEST(winrt_hstring_core_threads_reference_dup_deep_copies)
{
    static const WCHAR str[] = L"stable reference";
    HSTRING_HEADER hdr; HSTRING r; int i; REQUIRE_HS();
    EXPECT_EQ(WCSR(str, 16, &hdr, &r), S_OK);
    for (i = 0; i < 1000; i++) {
        HSTRING d = NULL;
        EXPECT_EQ(WDUP(r, &d), S_OK);
        if (d == r || !d) { CHECK(d != r && d); break; }
        WDEL(d);
    }
}

/* ---------------------------------------------------------- internals (observations only) */

TEST(winrt_hstring_core_internal_heap_string_layout_notes)
{
    HSTRING h = hs_makez(L"layout"), d = NULL; unsigned char *b = (unsigned char *)h; REQUIRE_HS();
    /* Win10 combase: flags(+0, bit0 clear for heap strings), length(+4), buffer ptr(+16), refcount(+24), data(+28) */
    t_note("heap hstring: flags=0x%lx len=%lu buf==inline? %d refcount=%ld",
           (unsigned long)*(DWORD *)b, (unsigned long)*(DWORD *)(b + 4),
           *(void **)(b + 16) == (void *)(b + 28), (long)*(LONG *)(b + 24));
    WDUP(h, &d);
    t_note("after duplicate refcount=%ld", (long)*(LONG *)(b + 24));
    WDEL(d);
    t_note("after delete refcount=%ld", (long)*(LONG *)(b + 24));
    WDEL(h);
}
