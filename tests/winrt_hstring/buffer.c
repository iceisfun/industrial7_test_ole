/* WindowsPreallocateStringBuffer / PromoteStringBuffer / DeleteStringBuffer.
 * Verified against the Windows 10 combase decompilation. */
#include "hs.h"

struct delb_ctx { HSTRING_BUFFER b; HRESULT hr; };
static void do_delb(void *p) { struct delb_ctx *c = p; c->hr = WDELB(c->b); }

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) */
TEST(winrt_hstring_buf_prealloc_promote_basic)
{
    static const WCHAR src[] = L"abcdef";
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(6, &buf, &b), S_OK);
    CHECK(buf != NULL);
    CHECK(b != NULL);
    EXPECT_EQ(buf[6], 0);                       /* terminator already in place */
    memcpy(buf, src, 6 * sizeof(WCHAR));
    EXPECT_EQ(WPRO(b, &h), S_OK);
    HS_EXPECT(h, src, 6, 0);
    EXPECT_EQ(WRAW(h, NULL), buf);              /* promotion hands over the same storage */
    EXPECT_EQ(WDEL(h), S_OK);
}

TEST(winrt_hstring_buf_prealloc_zero_length)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(0, &buf, &b), S_OK);
    CHECK(buf != NULL);                         /* a one char buffer holding the terminator */
    CHECK(b != NULL);
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(WPRO(b, &h), S_OK);
    CHECK(h == NULL);                           /* empty promotion is the NULL HSTRING (buffer released) */
}

TEST(winrt_hstring_buf_prealloc_zero_length_then_delete)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(0, &buf, &b), S_OK);
    EXPECT_EQ(WDELB(b), S_OK);
}

TEST(winrt_hstring_buf_prealloc_then_delete_without_promote)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; unsigned i; REQUIRE_HS_BUF();
    for (i = 1; i < 300; i += 7) {
        EXPECT_EQ(WPRE(i, &buf, &b), S_OK);
        memset(buf, 'x', i * sizeof(WCHAR));
        EXPECT_EQ(WDELB(b), S_OK);
    }
}

TEST(winrt_hstring_buf_delete_buffer_null_is_ok)
{
    REQUIRE_HS_BUF();
    EXPECT_EQ(WDELB(NULL), S_OK);
}

TEST(winrt_hstring_buf_prealloc_null_arguments)
{
    HSTRING_BUFFER b = (HSTRING_BUFFER)(ULONG_PTR)0x77; WCHAR *buf = (WCHAR *)(ULONG_PTR)0x55; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(4, NULL, &b), HS_E_POINTER);
    EXPECT_EQ(b, (HSTRING_BUFFER)(ULONG_PTR)0x77);        /* untouched */
    EXPECT_EQ(WPRE(4, &buf, NULL), HS_E_POINTER);
    EXPECT_EQ(buf, (WCHAR *)(ULONG_PTR)0x55);
    EXPECT_EQ(WPRE(4, NULL, NULL), HS_E_POINTER);
    EXPECT_EQ(WPRE(0, NULL, NULL), HS_E_POINTER);
}

TEST(winrt_hstring_buf_prealloc_length_too_large)
{
    UINT32 lens[] = { 0x7ffffff0, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff };
    unsigned i; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    for (i = 0; i < ARRAYSIZE(lens); i++) {
        HSTRING_BUFFER b = (HSTRING_BUFFER)(ULONG_PTR)0x77; WCHAR *buf = (WCHAR *)(ULONG_PTR)0x55;
        HRESULT hr = WPRE(lens[i], &buf, &b);
        t_check(hr == HS_E_LENGTH, __FILE__, __LINE__, "prealloc length hr", "len 0x%x got 0x%08lx", lens[i], (unsigned long)hr);
        CHECK(buf == NULL);                     /* both outputs are cleared first */
        CHECK(b == NULL);
    }
}

TEST(winrt_hstring_buf_prealloc_outputs_cleared_on_failure)
{
    HSTRING_BUFFER b = (HSTRING_BUFFER)(ULONG_PTR)0x77; WCHAR *buf = (WCHAR *)(ULONG_PTR)0x55; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WPRE(0xffffffff, &buf, &b), HS_E_LENGTH);
    CHECK(buf == NULL && b == NULL);
}

TEST(winrt_hstring_buf_promote_null_buffer)
{
    HSTRING h = HS_SENTINEL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRO(NULL, &h), S_OK);
    CHECK(h == NULL);
}

TEST(winrt_hstring_buf_promote_null_output)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRO(NULL, NULL), HS_E_POINTER);
    EXPECT_EQ(WPRE(3, &buf, &b), S_OK);
    EXPECT_EQ(WPRO(b, NULL), HS_E_POINTER);     /* buffer survives and can still be used */
    memcpy(buf, L"abc", 6);
    {
        HSTRING h = HS_SENTINEL;
        EXPECT_EQ(WPRO(b, &h), S_OK);
        HS_EXPECTZ(h, L"abc");
        WDEL(h);
    }
}

TEST(winrt_hstring_buf_promote_regular_hstring_is_invalidarg)
{
    HSTRING s = hs_makez(L"regular"), h = HS_SENTINEL; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WPRO((HSTRING_BUFFER)s, &h), HS_E_INVALIDARG);
    CHECK(h == NULL);
    HS_EXPECTZ(s, L"regular");                  /* the string is not consumed */
    WDEL(s);
}

TEST(winrt_hstring_buf_promote_reference_string_is_invalidarg)
{
    static const WCHAR str[] = L"ref";
    HSTRING_HEADER hdr; HSTRING r, h = HS_SENTINEL; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WCSR(str, 3, &hdr, &r), S_OK);
    EXPECT_EQ(WPRO((HSTRING_BUFFER)r, &h), HS_E_INVALIDARG);
    CHECK(h == NULL);
    HS_EXPECT(r, str, 3, 0);
}

TEST(winrt_hstring_buf_promote_twice_second_fails)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL, h2 = HS_SENTINEL; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WPRE(2, &buf, &b), S_OK);
    buf[0] = L'o'; buf[1] = L'k';
    EXPECT_EQ(WPRO(b, &h), S_OK);
    HS_EXPECTZ(h, L"ok");
    EXPECT_EQ(WPRO(b, &h2), HS_E_INVALIDARG);   /* the buffer handle is now an ordinary HSTRING */
    CHECK(h2 == NULL);
    HS_EXPECTZ(h, L"ok");
    WDEL(h);
}

TEST(winrt_hstring_buf_promote_corrupted_terminator_is_invalidarg_and_recoverable)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WPRE(3, &buf, &b), S_OK);
    memcpy(buf, L"abc", 6);
    buf[3] = L'X';                              /* overwrite the terminator slot */
    EXPECT_EQ(WPRO(b, &h), HS_E_INVALIDARG);
    CHECK(h == NULL);
    buf[3] = 0;                                 /* repair: the handle is still a live buffer */
    EXPECT_EQ(WPRO(b, &h), S_OK);
    HS_EXPECTZ(h, L"abc");
    WDEL(h);
}

TEST(winrt_hstring_buf_promote_corrupted_terminator_then_delete)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WPRE(1, &buf, &b), S_OK);
    buf[0] = L'a'; buf[1] = L'b';
    EXPECT_EQ(WPRO(b, &h), HS_E_INVALIDARG);
    EXPECT_EQ(WDELB(b), S_OK);
}

TEST(winrt_hstring_buf_promote_embedded_nuls)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(5, &buf, &b), S_OK);
    memcpy(buf, L"a\0b\0c", 5 * sizeof(WCHAR));
    EXPECT_EQ(WPRO(b, &h), S_OK);
    HS_EXPECT(h, L"a\0b\0c", 5, 1);
    WDEL(h);
}

TEST(winrt_hstring_buf_promote_all_chars_filled_table)
{
    static const UINT32 lens[] = { 1, 2, 3, 15, 16, 17, 255, 256, 1000, 65536 };
    unsigned i; REQUIRE_HS_BUF();
    for (i = 0; i < ARRAYSIZE(lens); i++) {
        HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; UINT32 j;
        EXPECT_EQ(WPRE(lens[i], &buf, &b), S_OK);
        for (j = 0; j < lens[i]; j++) buf[j] = (WCHAR)('A' + j % 26);
        EXPECT_EQ(WPRO(b, &h), S_OK);
        EXPECT_EQ(WLEN(h), lens[i]);
        CHECK(WRAW(h, NULL) == buf);
        CHECK(WRAW(h, NULL)[lens[i]] == 0);
        for (j = 0; j < lens[i]; j++) if (WRAW(h, NULL)[j] != (WCHAR)('A' + j % 26)) { CHECK(0); break; }
        WDEL(h);
    }
}

TEST(winrt_hstring_buf_promoted_string_behaves_like_heap_string)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL, d = HS_SENTINEL, r = HS_SENTINEL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(4, &buf, &b), S_OK);
    memcpy(buf, L"wxyz", 8);
    EXPECT_EQ(WPRO(b, &h), S_OK);
    EXPECT_EQ(WDUP(h, &d), S_OK);
    EXPECT_EQ(d, h);                            /* refcounted like any other heap string */
    EXPECT_EQ(WCAT(h, d, &r), S_OK);
    HS_EXPECTZ(r, L"wxyzwxyz");
    WDEL(r); WDEL(d); WDEL(h);
}

TEST(winrt_hstring_buf_delete_buffer_on_regular_hstring_raises)
{
    HSTRING s = hs_makez(L"not a buffer"); struct delb_ctx c; DWORD code; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    c.b = (HSTRING_BUFFER)s; c.hr = 0x1234;
    code = hs_try(do_delb, &c);
    EXPECT_EQ(code, 0xc000000d);                /* STATUS_INVALID_PARAMETER raised, not returned */
    HS_EXPECTZ(s, L"not a buffer");
    WDEL(s);
}

TEST(winrt_hstring_buf_delete_buffer_on_reference_raises)
{
    static const WCHAR str[] = L"ref";
    HSTRING_HEADER hdr; HSTRING r; struct delb_ctx c; DWORD code; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WCSR(str, 3, &hdr, &r), S_OK);
    c.b = (HSTRING_BUFFER)r;
    code = hs_try(do_delb, &c);
    EXPECT_EQ(code, 0xc000000d);
}

TEST(winrt_hstring_buf_delete_buffer_on_promoted_string_raises)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; HSTRING h = HS_SENTINEL; struct delb_ctx c; DWORD code; REQUIRE_HS_BUF();
    if (t_is_wine()) { t_skip("wine does not validate buffer handles/lengths"); return; }
    EXPECT_EQ(WPRE(2, &buf, &b), S_OK);
    buf[0] = L'a'; buf[1] = L'b';
    EXPECT_EQ(WPRO(b, &h), S_OK);
    c.b = b;                                    /* same pointer, but no longer a buffer */
    code = hs_try(do_delb, &c);
    EXPECT_EQ(code, 0xc000000d);
    HS_EXPECTZ(h, L"ab");
    WDEL(h);
}

TEST(winrt_hstring_buf_prealloc_distinct_buffers)
{
    HSTRING_BUFFER b1 = NULL, b2 = NULL; WCHAR *p1 = NULL, *p2 = NULL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(4, &p1, &b1), S_OK);
    EXPECT_EQ(WPRE(4, &p2, &b2), S_OK);
    EXPECT_NE(b1, b2);
    EXPECT_NE(p1, p2);
    WDELB(b1); WDELB(b2);
}

TEST(winrt_hstring_buf_buffer_after_end_is_writable_terminator_slot)
{
    /* the allocation always holds len+1 chars: both slots are usable */
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(10, &buf, &b), S_OK);
    memset(buf, 0x41, 11 * sizeof(WCHAR));
    buf[10] = 0;
    {
        HSTRING h = HS_SENTINEL;
        EXPECT_EQ(WPRO(b, &h), S_OK);
        EXPECT_EQ(WLEN(h), 10);
        WDEL(h);
    }
}

TEST(winrt_hstring_buf_internal_magic_note)
{
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; REQUIRE_HS_BUF();
    EXPECT_EQ(WPRE(2, &buf, &b), S_OK);
    t_note("buffer handle flags dword = 0x%08lx (win10 decomp: 0xf8b1a8be)", (unsigned long)*(DWORD *)b);
    WDELB(b);
}
