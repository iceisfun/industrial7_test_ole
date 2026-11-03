/* HSTRING_UserSize / UserMarshal / UserUnmarshal / UserFree (and the 64 variants), plus the NDR
 * user-marshal engine driving them.  Wire layout verified against the Windows 10 combase decompilation:
 *   non-INPROC: [pad to 8]["WdtP"][u32 byte count][UTF-16 data, no terminator]
 *   INPROC    : [pad to 8]["WdtP"][pad to 8][HSTRING pointer (a duplicate: refcount+1 / deep copy of a reference)]
 * The low byte of the MIDL flags is the MSHCTX; 3 (MSHCTX_INPROC) selects the pointer form. */
#include "hs.h"

#define NV (t_is_wine() ? 1 : 2)   /* wine stubs (aborts on) the *64 routines */
#define WDT_MARK 0x50746457u                     /* "WdtP" */
#define E_RAISED_NOMEM 0x8007000eu
#define RPC_X_BAD_STUB_DATA_ 0x6f7u
#define ALIGN8(x) (((x) + 7u) & ~7u)

static ULONG v_size(int v64, ULONG *f, ULONG st, HSTRING *h)
{ return v64 ? UN(HSTRING_UserSize64)(f, st, h) : UN(HSTRING_UserSize)(f, st, h); }
static unsigned char *v_marshal(int v64, ULONG *f, unsigned char *b, HSTRING *h)
{ return v64 ? UN(HSTRING_UserMarshal64)(f, b, h) : UN(HSTRING_UserMarshal)(f, b, h); }
static unsigned char *v_unmarshal(int v64, ULONG *f, unsigned char *b, HSTRING *h)
{ return v64 ? UN(HSTRING_UserUnmarshal64)(f, b, h) : UN(HSTRING_UserUnmarshal)(f, b, h); }
static void v_free(int v64, ULONG *f, HSTRING *h)
{ if (v64) UN(HSTRING_UserFree64)(f, h); else UN(HSTRING_UserFree)(f, h); }

#define REQUIRE_MARSHAL() do { REQUIRE_HS(); REQUIRE(WCMP); \
    REQUIRE(UN(HSTRING_UserSize)); REQUIRE(UN(HSTRING_UserSize64)); REQUIRE(UN(HSTRING_UserMarshal)); \
    REQUIRE(UN(HSTRING_UserMarshal64)); REQUIRE(UN(HSTRING_UserUnmarshal)); REQUIRE(UN(HSTRING_UserUnmarshal64)); \
    REQUIRE(UN(HSTRING_UserFree)); REQUIRE(UN(HSTRING_UserFree64)); } while (0)

static unsigned char g_buf[512] __attribute__((aligned(16)));
static int has_nul(const WCHAR *s, UINT32 n) { UINT32 i; for (i = 0; s && i < n; i++) if (!s[i]) return 1; return 0; }

/* ---- exception-catching wrappers ---- */
struct call {
    int op, v64; ULONG flags; ULONG start; HSTRING *ph; unsigned char *buf; ULONG buflen; USER_MARSHAL_CB_TYPE type;
    ULONG rsize; unsigned char *rptr;
};
enum { OP_SIZE, OP_MARSHAL, OP_UNMARSHAL };
static void do_call(void *p)
{
    struct call *c = p; hs_cb cb;
    hs_cb_init(&cb, g_buf, c->buflen, c->flags, c->type);
    switch (c->op) {
    case OP_SIZE: c->rsize = v_size(c->v64, &cb.umcb.Flags, c->start, c->ph); break;
    case OP_MARSHAL: c->rptr = v_marshal(c->v64, &cb.umcb.Flags, c->buf, c->ph); break;
    case OP_UNMARSHAL: c->rptr = v_unmarshal(c->v64, &cb.umcb.Flags, c->buf, c->ph); break;
    }
}

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) -- size/marshal tables, extended */
struct sample { const WCHAR *s; UINT32 n; };
static const struct sample samples[] = {
    { NULL, 0 }, { L"a", 1 }, { L"ab", 2 }, { L"abc", 3 }, { L"marshal_test1", 13 }, { L"a\0b", 3 }, { L"\0", 1 },
    { L"0123456789abcdef0123456789abcdef", 32 },
};
static const ULONG ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_INPROC };
static const ULONG starts[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 100 };

static ULONG exp_size(ULONG ctx, ULONG start, UINT32 n)
{
    return ctx == MSHCTX_INPROC ? ALIGN8(start) + 16 : ALIGN8(start) + 8 + 2 * n;
}

TEST(winrt_hstring_marshal_usersize_table)
{
    unsigned i, j, k; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++)
    for (i = 0; i < ARRAYSIZE(samples); i++) {
        HSTRING h = hs_make(samples[i].s, samples[i].n);
        for (j = 0; j < ARRAYSIZE(ctxs); j++)
            for (k = 0; k < ARRAYSIZE(starts); k++) {
                hs_cb cb; ULONG got, want;
                hs_cb_init(&cb, NULL, 0, ctxs[j], USER_MARSHAL_CB_BUFFER_SIZE);
                got = v_size(v, &cb.umcb.Flags, starts[k], &h);
                want = exp_size(ctxs[j], starts[k], samples[i].n);
                t_check(got == want, __FILE__, __LINE__, "UserSize", "v64=%d sample %u ctx %lu start %lu got %lu want %lu", v, i, ctxs[j], starts[k], got, want);
            }
        WDEL(h);
    }
}

TEST(winrt_hstring_marshal_usersize_ignores_high_flag_bits)
{
    /* only the low byte (MSHCTX) is examined: data representation and async bits do not matter */
    HSTRING h = hs_makez(L"flags"); ULONG fl[] = { 0x00100000, 0x00000100, 0x00100100, 0xffffff00 };
    unsigned i, v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++)
        for (i = 0; i < ARRAYSIZE(fl); i++) {
            hs_cb cb; ULONG a, b;
            hs_cb_init(&cb, NULL, 0, MSHCTX_LOCAL | fl[i], USER_MARSHAL_CB_BUFFER_SIZE);
            a = v_size(v, &cb.umcb.Flags, 0, &h);
            EXPECT_EQ(a, 8 + 10);
            hs_cb_init(&cb, NULL, 0, MSHCTX_INPROC | fl[i], USER_MARSHAL_CB_BUFFER_SIZE);
            b = v_size(v, &cb.umcb.Flags, 0, &h);
            EXPECT_EQ(b, 16);
        }
    WDEL(h);
}

TEST(winrt_hstring_marshal_usersize_null_pointer_returns_start_unchanged)
{
    ULONG st[] = { 0, 1, 3, 9, 0xfffffff0 }; unsigned i, v; REQUIRE_MARSHAL();
    if (t_is_wine()) { t_skip("wine faults (unaligned/NULL handling)"); return; }
    for (v = 0; v < NV; v++)
        for (i = 0; i < ARRAYSIZE(st); i++) {
            hs_cb cb;
            hs_cb_init(&cb, NULL, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            EXPECT_EQ(v_size(v, &cb.umcb.Flags, st[i], NULL), st[i]);   /* not even aligned */
        }
}

TEST(winrt_hstring_marshal_usersize_depends_only_on_length_not_content)
{
    HSTRING a = hs_make(L"aaaa", 4), b = hs_make(L"b\0bb", 4); hs_cb cb; REQUIRE_MARSHAL();
    hs_cb_init(&cb, NULL, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
    EXPECT_EQ(v_size(0, &cb.umcb.Flags, 0, &a), v_size(0, &cb.umcb.Flags, 0, &b));
    WDEL(a); WDEL(b);
}

TEST(winrt_hstring_marshal_usersize_overflow_raises_out_of_memory)
{
    HSTRING h = hs_makez(L"x"); ULONG st[] = { 0xfffffff9, 0xfffffffb, 0xffffffff, 0xfffffff8 };
    unsigned i, j, v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++)
        for (j = 0; j < ARRAYSIZE(ctxs); j++)
            for (i = 0; i < ARRAYSIZE(st); i++) {
                struct call c; DWORD code;
                memset(&c, 0, sizeof(c));
                c.op = OP_SIZE; c.v64 = v; c.flags = ctxs[j]; c.start = st[i]; c.ph = &h; c.type = USER_MARSHAL_CB_BUFFER_SIZE;
                code = hs_try(do_call, &c);
                t_check(code == E_RAISED_NOMEM, __FILE__, __LINE__, "UserSize overflow raises", "v64=%d ctx %lu start 0x%lx code 0x%lx", v, ctxs[j], st[i], code);
            }
    WDEL(h);
}

/* ---- marshal / unmarshal round trips ---- */

/* adapted from Wine dlls/combase/tests/string.c (LGPL-2.1+) -- extended to every start offset and variant */
TEST(winrt_hstring_marshal_marshal_local_layout_table)
{
    unsigned i, k; int v; REQUIRE_MARSHAL();
    if (t_is_wine()) { t_skip("wine faults (unaligned/NULL handling)"); return; }
    for (v = 0; v < NV; v++)
    for (i = 0; i < ARRAYSIZE(samples); i++) {
        HSTRING h;
        if (t_is_wine() && !samples[i].s) continue;   /* wine faults on a NULL HSTRING here */
        h = hs_make(samples[i].s, samples[i].n);
        for (k = 0; k < (t_is_wine() ? 1u : 9u); k++) {   /* wine mishandles unaligned buffers */
            hs_cb cb; ULONG size; unsigned char *base = g_buf + k, *next, *w; DWORD *dw; HSTRING out = NULL; INT32 cmp = 5;
            hs_cb_init(&cb, g_buf, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = v_size(v, &cb.umcb.Flags, k, &h);
            memset(g_buf, 0xEE, sizeof(g_buf));
            hs_cb_init(&cb, g_buf, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            next = v_marshal(v, &cb.umcb.Flags, base, &h);
            t_check(next == g_buf + size, __FILE__, __LINE__, "marshal returns end", "v64=%d sample %u off %u next-%p", v, i, k, g_buf + size);
            w = (unsigned char *)ALIGN8((ULONG_PTR)base);
            dw = (DWORD *)w;
            t_check(dw[0] == WDT_MARK, __FILE__, __LINE__, "WdtP marker", "off %u got 0x%08lx", k, dw[0]);
            t_check(dw[1] == 2 * samples[i].n, __FILE__, __LINE__, "byte count", "off %u got %lu", k, dw[1]);
            t_check(!memcmp(w + 8, samples[i].s ? (const void *)samples[i].s : (const void *)"", 2 * samples[i].n), __FILE__, __LINE__, "payload", "off %u", k);
            /* unmarshal from the same spot */
            hs_cb_init(&cb, g_buf, size, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
            next = v_unmarshal(v, &cb.umcb.Flags, base, &out);
            t_check(next == g_buf + size, __FILE__, __LINE__, "unmarshal returns end", "off %u", k);
            if (h) { CHECK(out != h); CHECK(out != NULL); }
            else CHECK(out == NULL);
            t_check(WCMP(out, h, &cmp) == S_OK && cmp == 0, __FILE__, __LINE__, "roundtrip equal", "sample %u off %u", i, k);
            hs_cb_init(&cb, g_buf, size, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
            v_free(v, &cb.umcb.Flags, &out);
        }
        WDEL(h);
    }
}

TEST(winrt_hstring_marshal_marshal_inproc_layout_table)
{
    unsigned i, k; int v; REQUIRE_MARSHAL();
    if (t_is_wine()) { t_skip("wine faults (unaligned/NULL handling)"); return; }
    for (v = 0; v < NV; v++)
    for (i = 0; i < ARRAYSIZE(samples); i++) {
        HSTRING h;
        if (t_is_wine() && !samples[i].s) continue;   /* wine faults on a NULL HSTRING here */
        h = hs_make(samples[i].s, samples[i].n);
        for (k = 0; k < (t_is_wine() ? 1u : 9u); k++) {   /* wine mishandles unaligned buffers */
            hs_cb cb; ULONG size; unsigned char *base = g_buf + k, *next, *w; HSTRING out = NULL, wired;
            hs_cb_init(&cb, g_buf, 0, MSHCTX_INPROC, USER_MARSHAL_CB_BUFFER_SIZE);
            size = v_size(v, &cb.umcb.Flags, k, &h);
            EXPECT_EQ(size, ALIGN8(k) + 16);
            memset(g_buf, 0xEE, sizeof(g_buf));
            hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_MARSHALL);
            next = v_marshal(v, &cb.umcb.Flags, base, &h);
            t_check(next == g_buf + size, __FILE__, __LINE__, "inproc marshal returns end", "v64=%d sample %u off %u", v, i, k);
            w = (unsigned char *)ALIGN8((ULONG_PTR)base);
            t_check(*(DWORD *)w == WDT_MARK, __FILE__, __LINE__, "inproc marker", "off %u", k);
            wired = *(HSTRING *)(w + 8);
            if (h) t_check(wired == h, __FILE__, __LINE__, "inproc wire pointer is the string", "off %u wired %p h %p", k, wired, h);
            else CHECK(wired == NULL);
            hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_UNMARSHALL);
            next = v_unmarshal(v, &cb.umcb.Flags, base, &out);
            t_check(next == g_buf + size, __FILE__, __LINE__, "inproc unmarshal returns end", "off %u", k);
            CHECK(out == h);
            hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_FREE);
            v_free(v, &cb.umcb.Flags, &out);    /* drops the reference taken by marshal */
            if (h) HS_EXPECT(h, samples[i].s, samples[i].n, has_nul(samples[i].s, samples[i].n));
        }
        WDEL(h);
    }
}

TEST(winrt_hstring_marshal_marshal_inproc_reference_string_is_deep_copied)
{
    static const WCHAR s[] = L"fast pass string";
    HSTRING_HEADER hdr; HSTRING r, out = NULL; hs_cb cb; ULONG size; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        EXPECT_EQ(WCSR(s, 16, &hdr, &r), S_OK);
        hs_cb_init(&cb, g_buf, 0, MSHCTX_INPROC, USER_MARSHAL_CB_BUFFER_SIZE);
        size = v_size(v, &cb.umcb.Flags, 0, &r);
        hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_MARSHALL);
        v_marshal(v, &cb.umcb.Flags, g_buf, &r);
        {
            HSTRING wired = *(HSTRING *)(g_buf + 8);
            CHECK(wired != NULL);
            EXPECT_NE(wired, r);                /* a reference cannot be shared with another owner */
            EXPECT_NE(WRAW(wired, NULL), s);
            HS_EXPECT(wired, s, 16, 0);
        }
        hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_UNMARSHALL);
        v_unmarshal(v, &cb.umcb.Flags, g_buf, &out);
        HS_EXPECT(out, s, 16, 0);
        hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_FREE);
        v_free(v, &cb.umcb.Flags, &out);
    }
}

TEST(winrt_hstring_marshal_marshal_null_pointer_returns_buffer_unchanged)
{
    int v; REQUIRE_MARSHAL();
    if (t_is_wine()) { t_skip("wine dereferences the HSTRING pointer"); return; }
    for (v = 0; v < NV; v++) {
        hs_cb cb; unsigned char *r;
        hs_cb_init(&cb, g_buf, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        r = v_marshal(v, &cb.umcb.Flags, g_buf + 3, NULL);
        EXPECT_EQ(r, g_buf + 3);                /* no alignment, no write */
    }
}

TEST(winrt_hstring_marshal_unmarshal_inproc_marker_not_validated)
{
    HSTRING h = hs_makez(L"nomarker"), out = HS_SENTINEL; hs_cb cb; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        ULONG size;
        hs_cb_init(&cb, g_buf, 0, MSHCTX_INPROC, USER_MARSHAL_CB_BUFFER_SIZE);
        size = v_size(v, &cb.umcb.Flags, 0, &h);
        hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_MARSHALL);
        v_marshal(v, &cb.umcb.Flags, g_buf, &h);
        *(DWORD *)g_buf = 0xdeadbeef;
        out = NULL;
        hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_UNMARSHALL);
        EXPECT_EQ(v_unmarshal(v, &cb.umcb.Flags, g_buf, &out), g_buf + size);
        EXPECT_EQ(out, h);
        hs_cb_init(&cb, g_buf, size, MSHCTX_INPROC, USER_MARSHAL_CB_FREE);
        v_free(v, &cb.umcb.Flags, &out);
    }
    WDEL(h);
}

TEST(winrt_hstring_marshal_unmarshal_local_marker_not_validated)
{
    hs_cb cb; HSTRING out = NULL; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        memset(g_buf, 0, 64);
        *(DWORD *)g_buf = 0x12345678; *(DWORD *)(g_buf + 4) = 6;
        memcpy(g_buf + 8, L"xyz", 6);
        hs_cb_init(&cb, g_buf, 14, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        out = NULL;
        EXPECT_EQ(v_unmarshal(v, &cb.umcb.Flags, g_buf, &out), g_buf + 14);
        HS_EXPECTZ(out, L"xyz");
        v_free(v, &cb.umcb.Flags, &out);
    }
}

TEST(winrt_hstring_marshal_unmarshal_local_odd_and_zero_byte_counts)
{
    hs_cb cb; HSTRING out; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        /* byte count is halved: a trailing odd byte is consumed but contributes no character */
        memset(g_buf, 0, 64);
        *(DWORD *)(g_buf + 4) = 3;
        memcpy(g_buf + 8, L"ab", 4);
        hs_cb_init(&cb, g_buf, 11, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        out = NULL;
        EXPECT_EQ(v_unmarshal(v, &cb.umcb.Flags, g_buf, &out), g_buf + 11);
        HS_EXPECTZ(out, L"a");
        v_free(v, &cb.umcb.Flags, &out);
        /* 0 and 1 bytes both give the NULL HSTRING */
        *(DWORD *)(g_buf + 4) = 0;
        hs_cb_init(&cb, g_buf, 8, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        out = NULL;
        EXPECT_EQ(v_unmarshal(v, &cb.umcb.Flags, g_buf, &out), g_buf + 8);
        CHECK(out == NULL);
        *(DWORD *)(g_buf + 4) = 1;
        hs_cb_init(&cb, g_buf, 9, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        out = HS_SENTINEL; out = NULL;
        EXPECT_EQ(v_unmarshal(v, &cb.umcb.Flags, g_buf, &out), g_buf + 9);
        CHECK(out == NULL);
    }
}

TEST(winrt_hstring_marshal_unmarshal_local_embedded_nuls_roundtrip)
{
    static const WCHAR s[] = L"\0a\0";
    HSTRING h = hs_make(s, 3), out = NULL; hs_cb cb; ULONG size; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        hs_cb_init(&cb, g_buf, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = v_size(v, &cb.umcb.Flags, 0, &h);
        EXPECT_EQ(size, 8 + 6);
        hs_cb_init(&cb, g_buf, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        v_marshal(v, &cb.umcb.Flags, g_buf, &h);
        hs_cb_init(&cb, g_buf, size, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        out = NULL;
        v_unmarshal(v, &cb.umcb.Flags, g_buf, &out);
        HS_EXPECT(out, s, 3, 1);
        v_free(v, &cb.umcb.Flags, &out);
    }
    WDEL(h);
}

/* ---- short / bad buffers raise RPC exceptions ---- */

TEST(winrt_hstring_marshal_marshal_local_buffer_too_small_raises_out_of_memory)
{
    HSTRING h = hs_makez(L"abcd"); ULONG lens[] = { 0, 4, 7, 8, 15 }; unsigned i; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++)
        for (i = 0; i < ARRAYSIZE(lens); i++) {
            struct call c; DWORD code;
            memset(&c, 0, sizeof(c));
            c.op = OP_MARSHAL; c.v64 = v; c.flags = MSHCTX_LOCAL; c.ph = &h; c.buf = g_buf; c.buflen = lens[i];
            c.type = USER_MARSHAL_CB_MARSHALL;
            code = hs_try(do_call, &c);          /* 8 byte header + 8 bytes of data needed: 16 */
            t_check(code == E_RAISED_NOMEM, __FILE__, __LINE__, "marshal short buffer raises", "v64=%d len %lu code 0x%lx", v, lens[i], code);
        }
    WDEL(h);
}

TEST(winrt_hstring_marshal_marshal_local_exact_buffer_ok)
{
    HSTRING h = hs_makez(L"abcd"); struct call c; DWORD code; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        memset(&c, 0, sizeof(c));
        c.op = OP_MARSHAL; c.v64 = v; c.flags = MSHCTX_LOCAL; c.ph = &h; c.buf = g_buf; c.buflen = 16;
        c.type = USER_MARSHAL_CB_MARSHALL;
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, 0);
        EXPECT_EQ(c.rptr, g_buf + 16);
    }
    WDEL(h);
}

TEST(winrt_hstring_marshal_marshal_empty_needs_eight_bytes)
{
    struct call c; HSTRING none = NULL; DWORD code; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        memset(&c, 0, sizeof(c));
        c.op = OP_MARSHAL; c.v64 = v; c.flags = MSHCTX_LOCAL; c.ph = &none; c.buf = g_buf; c.buflen = 7;
        c.type = USER_MARSHAL_CB_MARSHALL;
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, E_RAISED_NOMEM);
        c.buflen = 8; c.rptr = NULL;
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, 0);
        EXPECT_EQ(c.rptr, g_buf + 8);
    }
}

TEST(winrt_hstring_marshal_unmarshal_local_short_buffers_raise_bad_stub_data)
{
    struct call c; HSTRING out; DWORD code; ULONG lens[] = { 0, 1, 3, 7 }; unsigned i; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++)
        for (i = 0; i < ARRAYSIZE(lens); i++) {
            memset(g_buf, 0, 64);
            out = NULL;
            memset(&c, 0, sizeof(c));
            c.op = OP_UNMARSHAL; c.v64 = v; c.flags = MSHCTX_LOCAL; c.ph = &out; c.buf = g_buf; c.buflen = lens[i];
            c.type = USER_MARSHAL_CB_UNMARSHALL;
            code = hs_try(do_call, &c);
            t_check(code == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "unmarshal short raises 0x6f7", "v64=%d len %lu code 0x%lx", v, lens[i], code);
            CHECK(out == NULL);
        }
}

TEST(winrt_hstring_marshal_unmarshal_local_count_exceeding_buffer_raises)
{
    struct call c; HSTRING out; DWORD code; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        memset(g_buf, 0, 64);
        *(DWORD *)g_buf = WDT_MARK; *(DWORD *)(g_buf + 4) = 10;      /* claims 10 bytes */
        out = NULL;
        memset(&c, 0, sizeof(c));
        c.op = OP_UNMARSHAL; c.v64 = v; c.flags = MSHCTX_LOCAL; c.ph = &out; c.buf = g_buf; c.buflen = 17;   /* only 9 follow */
        c.type = USER_MARSHAL_CB_UNMARSHALL;
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, RPC_X_BAD_STUB_DATA_);
        c.buflen = 18;                                              /* exactly enough */
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, 0);
        EXPECT_EQ(WLEN(out), 5);
        WDEL(out);
        *(DWORD *)(g_buf + 4) = 0xffffffff;                         /* absurd count */
        c.buflen = 64; out = NULL;
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, RPC_X_BAD_STUB_DATA_);
    }
}

TEST(winrt_hstring_marshal_unmarshal_inproc_short_buffer_raises_bad_stub_data)
{
    struct call c; HSTRING out; DWORD code; ULONG lens[] = { 0, 4, 8, 12 }; unsigned i; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++)
        for (i = 0; i < ARRAYSIZE(lens); i++) {
            memset(g_buf, 0, 64);
            out = NULL;
            memset(&c, 0, sizeof(c));
            c.op = OP_UNMARSHAL; c.v64 = v; c.flags = MSHCTX_INPROC; c.ph = &out; c.buf = g_buf; c.buflen = lens[i];
            c.type = USER_MARSHAL_CB_UNMARSHALL;
            code = hs_try(do_call, &c);
            t_check(code == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "inproc unmarshal short raises", "v64=%d len %lu code 0x%lx", v, lens[i], code);
        }
}

TEST(winrt_hstring_marshal_unmarshal_inproc_exact_sixteen_bytes_ok)
{
    struct call c; HSTRING out = HS_SENTINEL; DWORD code; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        memset(g_buf, 0, 64);
        out = NULL;                               /* NULL pointer on the wire is legal */
        memset(&c, 0, sizeof(c));
        c.op = OP_UNMARSHAL; c.v64 = v; c.flags = MSHCTX_INPROC; c.ph = &out; c.buf = g_buf; c.buflen = 16;
        c.type = USER_MARSHAL_CB_UNMARSHALL;
        code = hs_try(do_call, &c);
        EXPECT_EQ(code, 0);
        EXPECT_EQ(c.rptr, g_buf + 16);
        CHECK(out == NULL);
    }
}

TEST(winrt_hstring_marshal_marshal_different_machine_context_observed)
{
    /* MSHCTX_DIFFERENTMACHINE only works when the cross-VM RPC support is present; otherwise an RPC
     * exception is raised.  Record what this machine does. */
    HSTRING h = hs_makez(L"remote"); struct call c; DWORD code; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        hs_cb cb; ULONG size;
        hs_cb_init(&cb, g_buf, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
        size = v_size(v, &cb.umcb.Flags, 0, &h);
        EXPECT_EQ(size, 8 + 12);                  /* sizing treats it like LOCAL */
        memset(&c, 0, sizeof(c));
        c.op = OP_MARSHAL; c.v64 = v; c.flags = MSHCTX_DIFFERENTMACHINE; c.ph = &h; c.buf = g_buf; c.buflen = size;
        c.type = USER_MARSHAL_CB_MARSHALL;
        code = hs_try(do_call, &c);
        t_note("v64=%d marshal MSHCTX_DIFFERENTMACHINE raised 0x%lx (decomp: 0x6c5 unless cross-vm pipe namespace present)", v, code);
    }
    WDEL(h);
}

/* ---- UserFree ---- */

TEST(winrt_hstring_marshal_userfree_null_cases)
{
    hs_cb cb; HSTRING none = NULL; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        hs_cb_init(&cb, NULL, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        v_free(v, &cb.umcb.Flags, NULL);        /* NULL pointer to HSTRING */
        v_free(v, &cb.umcb.Flags, &none);       /* NULL HSTRING */
    }
    CHECK(1);
}

TEST(winrt_hstring_marshal_userfree_releases_one_reference_without_clearing_slot)
{
    HSTRING h = hs_makez(L"refcounted"), d = NULL, slot; hs_cb cb; int v; REQUIRE_MARSHAL();
    for (v = 0; v < NV; v++) {
        EXPECT_EQ(WDUP(h, &d), S_OK);
        slot = d;
        hs_cb_init(&cb, NULL, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        v_free(v, &cb.umcb.Flags, &slot);
        EXPECT_EQ(slot, d);                     /* the slot is not zeroed */
        HS_EXPECTZ(h, L"refcounted");           /* the original reference is still alive */
    }
    WDEL(h);
}

TEST(winrt_hstring_marshal_userfree_reference_string_is_noop)
{
    static const WCHAR s[] = L"stays";
    HSTRING_HEADER hdr; HSTRING r; hs_cb cb; int v; REQUIRE_MARSHAL();
    EXPECT_EQ(WCSR(s, 5, &hdr, &r), S_OK);
    for (v = 0; v < NV; v++) {
        hs_cb_init(&cb, NULL, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        v_free(v, &cb.umcb.Flags, &r);
        HS_EXPECT(r, s, 5, 0);
    }
}

/* ---- 32/64 variants agree ---- */

TEST(winrt_hstring_marshal_variants_produce_identical_bytes)
{
    unsigned i, j; REQUIRE_MARSHAL();
    if (t_is_wine()) { t_skip("wine lacks HSTRING_User*64"); return; }
    for (i = 0; i < ARRAYSIZE(samples); i++)
        for (j = 0; j < ARRAYSIZE(ctxs); j++) {
            HSTRING h = hs_make(samples[i].s, samples[i].n);
            unsigned char b0[128], b1[128]; hs_cb cb; ULONG s0, s1; HSTRING dummy;
            hs_cb_init(&cb, g_buf, 0, ctxs[j], USER_MARSHAL_CB_BUFFER_SIZE);
            s0 = v_size(0, &cb.umcb.Flags, 3, &h); s1 = v_size(1, &cb.umcb.Flags, 3, &h);
            EXPECT_EQ(s0, s1);
            memset(g_buf, 0x77, 128);
            hs_cb_init(&cb, g_buf, s0, ctxs[j], USER_MARSHAL_CB_MARSHALL);
            v_marshal(0, &cb.umcb.Flags, g_buf + 3, &h);
            memcpy(b0, g_buf, 128);
            if (ctxs[j] == MSHCTX_INPROC) { dummy = h; hs_cb_init(&cb, g_buf, s0, ctxs[j], USER_MARSHAL_CB_FREE); v_free(0, &cb.umcb.Flags, &dummy); }
            memset(g_buf, 0x77, 128);
            hs_cb_init(&cb, g_buf, s1, ctxs[j], USER_MARSHAL_CB_MARSHALL);
            v_marshal(1, &cb.umcb.Flags, g_buf + 3, &h);
            memcpy(b1, g_buf, 128);
            if (ctxs[j] == MSHCTX_INPROC) { dummy = h; hs_cb_init(&cb, g_buf, s0, ctxs[j], USER_MARSHAL_CB_FREE); v_free(1, &cb.umcb.Flags, &dummy); }
            t_check(!memcmp(b0, b1, 128), __FILE__, __LINE__, "32/64 marshal bytes identical", "sample %u ctx %lu", i, ctxs[j]);
            WDEL(h);
        }
}

TEST(winrt_hstring_marshal_roundtrip_all_contexts_alignment_sweep)
{
    unsigned i, j, k; REQUIRE_MARSHAL();
    for (i = 0; i < ARRAYSIZE(samples); i++)
        for (j = 0; j < ARRAYSIZE(ctxs); j++)
            for (k = 0; k < 8; k++) {
                HSTRING h = hs_make(samples[i].s, samples[i].n), out = NULL; hs_cb cb; ULONG size; INT32 c = 9; unsigned v = t_is_wine() ? 0 : (i + j + k) & 1;
                hs_cb_init(&cb, g_buf, 0, ctxs[j], USER_MARSHAL_CB_BUFFER_SIZE);
                size = v_size(v, &cb.umcb.Flags, k, &h);
                hs_cb_init(&cb, g_buf, size, ctxs[j], USER_MARSHAL_CB_MARSHALL);
                CHECK(v_marshal(v, &cb.umcb.Flags, g_buf + k, &h) == g_buf + size);
                hs_cb_init(&cb, g_buf, size, ctxs[j], USER_MARSHAL_CB_UNMARSHALL);
                CHECK(v_unmarshal(v, &cb.umcb.Flags, g_buf + k, &out) == g_buf + size);
                t_check(WCMP(h, out, &c) == S_OK && c == 0, __FILE__, __LINE__, "roundtrip", "sample %u ctx %lu off %u", i, ctxs[j], k);
                hs_cb_init(&cb, g_buf, size, ctxs[j], USER_MARSHAL_CB_FREE);
                v_free(v, &cb.umcb.Flags, &out);
                WDEL(h);
            }
}

/* ---- the NDR engine driving the HSTRING routines (NdrUserMarshal*) ---- */

static void *WINAPI my_alloc(SIZE_T n) { return HeapAlloc(GetProcessHeap(), 0, n); }
static void WINAPI my_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }

/* FC_USER_MARSHAL: type, flags|alignment-1, quadruple index, memory size, buffer size, type offset */
static unsigned char g_ufmt[] = { 0xb4, 0x07, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

struct eng {
    MIDL_STUB_MESSAGE stub; RPC_MESSAGE rpc; MIDL_STUB_DESC desc; USER_MARSHAL_ROUTINE_QUADRUPLE quad;
};
static void eng_init(struct eng *e, ULONG ctx, int v64)
{
    memset(e, 0, sizeof(*e));
    e->quad.pfnBufferSize = (USER_MARSHAL_SIZING_ROUTINE)(v64 ? (void *)UN(HSTRING_UserSize64) : (void *)UN(HSTRING_UserSize));
    e->quad.pfnMarshall = (USER_MARSHAL_MARSHALLING_ROUTINE)(v64 ? (void *)UN(HSTRING_UserMarshal64) : (void *)UN(HSTRING_UserMarshal));
    e->quad.pfnUnmarshall = (USER_MARSHAL_UNMARSHALLING_ROUTINE)(v64 ? (void *)UN(HSTRING_UserUnmarshal64) : (void *)UN(HSTRING_UserUnmarshal));
    e->quad.pfnFree = (USER_MARSHAL_FREEING_ROUTINE)(v64 ? (void *)UN(HSTRING_UserFree64) : (void *)UN(HSTRING_UserFree));
    e->desc.pfnAllocate = my_alloc; e->desc.pfnFree = my_free;
    e->desc.aUserMarshalQuadruple = &e->quad;
    e->stub.StubDesc = &e->desc;
    e->stub.pfnAllocate = my_alloc; e->stub.pfnFree = my_free;
    e->stub.RpcMsg = &e->rpc;
    e->stub.dwDestContext = ctx;
    e->stub.IsClient = 1;
}
static void eng_buffer(struct eng *e, void *buf, ULONG len)
{
    e->rpc.Buffer = buf; e->rpc.BufferLength = len;
    e->stub.Buffer = buf; e->stub.BufferStart = buf; e->stub.BufferEnd = (unsigned char *)buf + len;
    e->stub.BufferLength = 0;
}

TEST(winrt_hstring_marshal_ndr_usermarshal_engine_roundtrip)
{
    static const ULONG ctx[] = { MSHCTX_LOCAL, MSHCTX_INPROC };
    unsigned i, j; int v; REQUIRE_MARSHAL();
    REQUIRE(U(NdrUserMarshalBufferSize)); REQUIRE(U(NdrUserMarshalMarshall)); REQUIRE(U(NdrUserMarshalUnmarshall)); REQUIRE(U(NdrUserMarshalFree));
    for (v = 0; v < NV; v++)
    for (j = 0; j < 2; j++)
    for (i = 0; i < ARRAYSIZE(samples); i++) {
        struct eng e; HSTRING h = hs_make(samples[i].s, samples[i].n), out = NULL; unsigned char *pmem; ULONG want, size; INT32 c = 3;
        eng_init(&e, ctx[j], v);
        U(NdrUserMarshalBufferSize)(&e.stub, (unsigned char *)&h, g_ufmt);
        want = exp_size(ctx[j], 0, samples[i].n);
        size = e.stub.BufferLength;
        t_check(size == want, __FILE__, __LINE__, "engine buffer size", "v64=%d ctx %lu sample %u got %lu want %lu", v, ctx[j], i, size, want);
        memset(g_buf, 0xEE, sizeof(g_buf));
        eng_init(&e, ctx[j], v); eng_buffer(&e, g_buf, size);
        U(NdrUserMarshalMarshall)(&e.stub, (unsigned char *)&h, g_ufmt);
        t_check(e.stub.Buffer == g_buf + want, __FILE__, __LINE__, "engine marshal advances Buffer", "ctx %lu sample %u", ctx[j], i);
        t_check(*(DWORD *)g_buf == WDT_MARK, __FILE__, __LINE__, "engine wrote WdtP", NULL);
        eng_init(&e, ctx[j], v); eng_buffer(&e, g_buf, size);
        pmem = (unsigned char *)&out;
        U(NdrUserMarshalUnmarshall)(&e.stub, &pmem, g_ufmt, 0);
        t_check(e.stub.Buffer == g_buf + want, __FILE__, __LINE__, "engine unmarshal advances Buffer", "ctx %lu sample %u", ctx[j], i);
        t_check(WCMP(h, out, &c) == S_OK && c == 0, __FILE__, __LINE__, "engine roundtrip equal", "ctx %lu sample %u", ctx[j], i);
        eng_init(&e, ctx[j], v);
        U(NdrUserMarshalFree)(&e.stub, (unsigned char *)&out, g_ufmt);   /* drops the unmarshalled/dup'd reference */
        WDEL(h);
    }
}

TEST(winrt_hstring_marshal_ndr_engine_aligns_to_eight_after_preceding_data)
{
    struct eng e; HSTRING h = hs_makez(L"align"), out = NULL; unsigned char *pmem; ULONG size; INT32 c = 4; int v; REQUIRE_MARSHAL();
    REQUIRE(U(NdrUserMarshalBufferSize)); REQUIRE(U(NdrUserMarshalMarshall)); REQUIRE(U(NdrUserMarshalUnmarshall)); REQUIRE(U(NdrUserMarshalFree));
    for (v = 0; v < NV; v++) {
        eng_init(&e, MSHCTX_LOCAL, v);
        e.stub.BufferLength = 4;                  /* four bytes of other data precede the string */
        U(NdrUserMarshalBufferSize)(&e.stub, (unsigned char *)&h, g_ufmt);
        size = e.stub.BufferLength;
        EXPECT_EQ(size, 8 + 8 + 10);
        memset(g_buf, 0xEE, sizeof(g_buf));
        eng_init(&e, MSHCTX_LOCAL, v); eng_buffer(&e, g_buf, size);
        e.stub.Buffer = g_buf + 4;
        U(NdrUserMarshalMarshall)(&e.stub, (unsigned char *)&h, g_ufmt);
        EXPECT_EQ(e.stub.Buffer, g_buf + size);
        EXPECT_EQ(*(DWORD *)(g_buf + 8), WDT_MARK);
        EXPECT_EQ(*(DWORD *)(g_buf + 12), 10);
        eng_init(&e, MSHCTX_LOCAL, v); eng_buffer(&e, g_buf, size);
        e.stub.Buffer = g_buf + 4;
        pmem = (unsigned char *)&out;
        U(NdrUserMarshalUnmarshall)(&e.stub, &pmem, g_ufmt, 0);
        t_check(WCMP(h, out, &c) == S_OK && c == 0, __FILE__, __LINE__, "aligned engine roundtrip", NULL);
        eng_init(&e, MSHCTX_LOCAL, v);
        U(NdrUserMarshalFree)(&e.stub, (unsigned char *)&out, g_ufmt);
        out = NULL;
    }
    WDEL(h);
}
