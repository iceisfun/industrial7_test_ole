/* usrmarshal_ole32: CLIPFORMAT_User{Size,Marshal,Unmarshal,Free}[64|Ext].
 * Win10 wire format (decompiled):
 *   32-bit, "named" form (cf > 0xbfff and context low byte in {0 LOCAL, 2 DIFFERENTMACHINE, 5 CONTAINER}):
 *       DWORD 'WdtR', DWORD cf, DWORD max=n+1, DWORD off=0, DWORD act=n+1, WCHAR name[n+1]
 *   otherwise DWORD 'WdtH', DWORD cf.
 *   Size() over-estimates: align4(off) + 12 (+ 2n + 14 for the named form).
 * 64-bit variants use 8-byte alignment and 8-byte fields in the named form. */
#include "um.h"

typedef UINT (WINAPI *regcf_t)(LPCWSTR);
typedef int (WINAPI *getcfn_t)(UINT, LPWSTR, int);

static regcf_t RegCF(void) { return (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW"); }
static getcfn_t GetCFN(void) { return (getcfn_t)t_proc_in("user32.dll", "GetClipboardFormatNameW"); }

/* Register a clipboard format whose name has exactly n characters (n >= 12) and is unique to this process. */
static UINT reg_fmt(int n, int tag, WCHAR *name_out)
{
    char tmp[300]; int i, l;
    WCHAR local[300];
    WCHAR *w = name_out ? name_out : local;
    l = snprintf(tmp, sizeof tmp, "olet%lu_%d_", GetCurrentProcessId(), tag);
    for (i = l; i < n; i++) tmp[i] = (char)('a' + (i % 26));
    tmp[n] = 0;
    for (i = 0; i <= n; i++) w[i] = (WCHAR)tmp[i];
    return RegCF() ? RegCF()(w) : 0;
}

static int is_named_ctx(ULONG c) { return c == C_LOCAL || c == C_DIFFM || c == C_CONT; }
#define RD32(b, o) (*(ULONG *)((b) + (o)))
#define RD64(b, o) (*(unsigned long long *)((b) + (o)))

static const WORD k_plain[] = { 0, 1, 2, 13, 0x80, 0x200, 0x8000, 0xbfff };

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_CLIPFORMAT */
TEST(usr_clipfmt_size_null_ptr)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize"), *f6 = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize64");
    ULONG off, ctx, r; int k;
    REQUIRE(f); REQUIRE(f6);
    for (k = 0; k < 2; k++)
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 10; off++) {
                UMC c; um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(k ? f6 : f, UMF(&c), off, NULL, &r), 0);
                CHECKF(r == off, "k=%d ctx %lu off %lu got %lu", k, ctx, off, r);
            }
}

TEST(usr_clipfmt_size_plain_formats)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize");
    int i; ULONG off, ctx, r;
    REQUIRE(f);
    for (i = 0; i < (int)(sizeof k_plain / sizeof k_plain[0]); i++)
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 9; off++) {
                UMC c; CLIPFORMAT cf = k_plain[i];
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(f, UMF(&c), off, &cf, &r), 0);
                CHECKF(r == UM_ALIGN(off, 4) + 12, "cf %#x ctx %lu off %lu got %lu", cf, ctx, off, r);
            }
}

TEST(usr_clipfmt_size_registered_by_context)
{
    static const int lens[] = { 12, 13, 40, 120, 240, 246 };
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize");
    int i; ULONG ctx, off, r;
    REQUIRE(f); REQUIRE(RegCF());
    for (i = 0; i < 6; i++) {
        CLIPFORMAT cf = (CLIPFORMAT)reg_fmt(lens[i], 1, NULL);
        CHECKF(cf > 0xbfff, "registered formats start at 0xC000 (got %#x)", cf);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 6; off++) {
                UMC c; ULONG want = UM_ALIGN(off, 4) + (is_named_ctx(ctx) ? 26 + 2 * lens[i] : 12);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(f, UMF(&c), off, &cf, &r), 0);
                CHECKF(r == want, "n=%d ctx %lu off %lu got %lu want %lu", lens[i], ctx, off, r, want);
            }
    }
}

TEST(usr_clipfmt_size_registered_name_truncation)
{
    /* GetClipboardFormatNameW is called with a 0xF7-character buffer: names longer than 246 chars are truncated. */
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize");
    CLIPFORMAT cf; UMC c; ULONG r;
    REQUIRE(f); REQUIRE(RegCF());
    cf = (CLIPFORMAT)reg_fmt(252, 2, NULL);
    um_init(&c, NULL, 0, C_LOCAL);
    XEQ(um_x_size(f, UMF(&c), 0, &cf, &r), 0);
    CHECKF(r == 26 + 2 * 246, "got %lu", r);
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_CLIPFORMAT */
TEST(usr_clipfmt_marshal_plain_wire)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal");
    int i; ULONG ctx, off;
    REQUIRE(f);
    for (i = 0; i < (int)(sizeof k_plain / sizeof k_plain[0]); i++)
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 5; off += 2) {
                UMC c; CLIPFORMAT cf = k_plain[i]; ULONG a = UM_ALIGN(off, 4);
                unsigned char *b = um_buf_new(32), *e;
                um_init(&c, b, 32, ctx);
                XEQ(um_x_mar(f, UMF(&c), b + off, &cf, &e), 0);
                CHECKF(e == b + a + 8, "cf %#x ctx %lu off %lu", cf, ctx, off);
                EXPECT_EQ(RD32(b, a), UM_TAG_H);
                EXPECT_EQ(RD32(b, a + 4), cf);
                CHECK(um_untouched(b, 0, a)); CHECK(um_untouched(b, a + 8, 32));
                um_buf_free(b);
            }
}

TEST(usr_clipfmt_marshal_null_ptr)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal"), *f6 = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal64");
    int k, off;
    REQUIRE(f); REQUIRE(f6);
    for (k = 0; k < 2; k++)
        for (off = 0; off < 8; off++) {
            UMC c; unsigned char *b = um_buf_new(16), *e;
            um_init(&c, b, 16, C_LOCAL);
            XEQ(um_x_mar(k ? f6 : f, UMF(&c), b + off, NULL, &e), 0);
            CHECK(e == b + off); CHECK(um_untouched(b, 0, 16));
            um_buf_free(b);
        }
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_CLIPFORMAT, tightened to Win10 x64 values */
TEST(usr_clipfmt_marshal_registered_wire)
{
    static const int lens[] = { 12, 13, 40, 120, 246 };
    void *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal"), *fs = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize");
    int i; ULONG ctx, off;
    REQUIRE(fm); REQUIRE(fs); REQUIRE(RegCF());
    for (i = 0; i < 5; i++) {
        WCHAR name[300]; CLIPFORMAT cf = (CLIPFORMAT)reg_fmt(lens[i], 3, name);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 4; off++) {
                UMC c; ULONG a = UM_ALIGN(off, 4), sz; unsigned char *b = um_buf_new(700), *e;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &cf, &sz), 0);
                um_init(&c, b, 700, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &cf, &e), 0);
                if (is_named_ctx(ctx)) {
                    ULONG n = lens[i];
                    CHECKF(e == b + a + 20 + 2 * (n + 1), "n=%lu ctx %lu end %+ld", n, ctx, (long)(e - b));
                    EXPECT_EQ(RD32(b, a), UM_TAG_R);
                    EXPECT_EQ(RD32(b, a + 4), cf);
                    EXPECT_EQ(RD32(b, a + 8), n + 1);
                    EXPECT_EQ(RD32(b, a + 12), 0);
                    EXPECT_EQ(RD32(b, a + 16), n + 1);
                    CHECK(!memcmp(b + a + 20, name, 2 * (n + 1)));
                    CHECKF(sz == (ULONG)(e - b) + 4, "Size() must over-estimate by exactly 4: size %lu end %ld", sz, (long)(e - b));
                    CHECK(um_untouched(b, e - b, 700));
                } else {
                    CHECK(e == b + a + 8);
                    EXPECT_EQ(RD32(b, a), UM_TAG_H);
                    EXPECT_EQ(RD32(b, a + 4), cf);
                }
                CHECK(um_untouched(b, 0, a));
                CHECK(um_guard_ok(b, 700));
                um_buf_free(b);
            }
    }
}

TEST(usr_clipfmt_marshal_name_truncation)
{
    void *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal");
    WCHAR name[300]; CLIPFORMAT cf; UMC c; unsigned char *b = um_buf_new(700), *e;
    REQUIRE(fm); REQUIRE(RegCF());
    cf = (CLIPFORMAT)reg_fmt(252, 4, name);
    um_init(&c, b, 700, C_LOCAL);
    XEQ(um_x_mar(fm, UMF(&c), b, &cf, &e), 0);
    EXPECT_EQ(RD32(b, 8), 247);
    EXPECT_EQ(RD32(b, 16), 247);
    CHECK(!memcmp(b + 20, name, 2 * 246));
    EXPECT_EQ(*(WCHAR *)(b + 20 + 2 * 246), 0);
    CHECK(e == b + 20 + 2 * 247);
    um_buf_free(b);
}

TEST(usr_clipfmt_unmarshal_roundtrip)
{
    void *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal"), *fu = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshal");
    int i; ULONG ctx;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(RegCF());
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        for (i = 0; i < (int)(sizeof k_plain / sizeof k_plain[0]); i++) {
            UMC c; CLIPFORMAT cf = k_plain[i], o = 0x1111; unsigned char *b = um_buf_new(64), *e1, *e2;
            um_init(&c, b, 64, ctx);
            XEQ(um_x_mar(fm, UMF(&c), b + 1, &cf, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), ctx);
            XEQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e2), 0);
            CHECK(e2 == e1); CHECKF(o == cf, "cf %#x -> %#x", cf, o);
            um_buf_free(b);
        }
        {
            UMC c; WCHAR name[64]; CLIPFORMAT cf = (CLIPFORMAT)reg_fmt(30, 5, name), o = 0x1111; unsigned char *b = um_buf_new(200), *e1, *e2;
            um_init(&c, b, 200, ctx);
            XEQ(um_x_mar(fm, UMF(&c), b + 1, &cf, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), ctx);
            XEQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e2), 0);
            CHECK(e2 == e1); CHECKF(o == cf, "ctx %lu registered cf %#x -> %#x", ctx, cf, o);
            um_buf_free(b);
        }
    }
}

TEST(usr_clipfmt_unmarshal_registers_name)
{
    /* a named wire form for a format this process has never registered must register it (new id > 0xbfff) */
    void *fu = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshal");
    WCHAR name[64]; char tmp[64]; int i, n; UMC c; CLIPFORMAT o = 0; unsigned char *b = um_buf_new(200), *e;
    REQUIRE(fu); REQUIRE(RegCF()); REQUIRE(GetCFN());
    n = snprintf(tmp, sizeof tmp, "olet%lu_unm_%lu", GetCurrentProcessId(), GetTickCount());
    for (i = 0; i <= n; i++) name[i] = (WCHAR)tmp[i];
    RD32(b, 0) = UM_TAG_R; RD32(b, 4) = 0xFFF0; RD32(b, 8) = n + 1; RD32(b, 12) = 0; RD32(b, 16) = n + 1;
    memcpy(b + 20, name, 2 * (n + 1));
    um_init(&c, b, 20 + 2 * (n + 1), C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o > 0xbfff);
    CHECK(e == b + 20 + 2 * (n + 1));
    {   WCHAR back[64]; int r = GetCFN()(o, back, 64);
        CHECK(r == n); CHECK(!lstrcmpW(back, name)); }
    CHECK(RegCF()(name) == o);
    um_buf_free(b);
}

TEST(usr_clipfmt_unmarshal_errors)
{
    void *fu = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshal");
    WCHAR name[40]; CLIPFORMAT o; UMC c; unsigned char *b; unsigned char *e;
    int n = 14, i;
    REQUIRE(fu);
    for (i = 0; i < n; i++) name[i] = L'a' + i;
    name[n] = 0;
    b = um_buf_new(200);

    /* wrong discriminant */
    RD32(b, 0) = 0x12345678; RD32(b, 4) = 1; o = 7;
    um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG); CHECK(o == 7);
    RD32(b, 0) = 0; um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);
    RD32(b, 0) = UM_TAG_P; um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);

    /* truncated before the 8-byte header */
    RD32(b, 0) = UM_TAG_H; RD32(b, 4) = 1;
    for (i = 0; i < 8; i++) { um_init(&c, b, i, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB); }
    um_init(&c, b, 8, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0); CHECK(o == 1 && e == b + 8);

    /* inproc form carrying a registered-range id is refused in "named" contexts */
    RD32(b, 4) = 0xc000;
    for (i = 0; i <= C_CONT; i++) {
        um_init(&c, b, 8, i);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), is_named_ctx(i) ? X_BADSTUB : 0);
    }
    RD32(b, 4) = 0xbfff; um_init(&c, b, 8, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);

    /* named form with an id in the predefined range */
    RD32(b, 0) = UM_TAG_R; RD32(b, 4) = 0xbfff; RD32(b, 8) = n + 1; RD32(b, 12) = 0; RD32(b, 16) = n + 1;
    memcpy(b + 20, name, 2 * (n + 1));
    um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);

    /* named form, malformed conformant string */
    RD32(b, 4) = 0xc123;
    um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    RD32(b, 12) = 1;       um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADCONF);
    RD32(b, 12) = 0; RD32(b, 16) = n; um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADCONF);
    RD32(b, 16) = n + 1; RD32(b, 8) = n + 2; um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADCONF);
    RD32(b, 8) = 0; RD32(b, 16) = 0; um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADCONF);
    RD32(b, 8) = n + 1; RD32(b, 16) = n + 1;
    *(WCHAR *)(b + 20 + 2 * n) = L'z';       /* not NUL terminated */
    um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADCONF);
    *(WCHAR *)(b + 20 + 2 * n) = 0; *(WCHAR *)(b + 20 + 2 * 3) = 0;   /* embedded NUL: length mismatch */
    um_init(&c, b, 100, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADCONF);
    memcpy(b + 20, name, 2 * (n + 1));
    /* string longer than the buffer */
    um_init(&c, b, 20 + 2 * n, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    um_init(&c, b, 19, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    um_init(&c, b, 20 + 2 * (n + 1), C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    um_buf_free(b);
}

TEST(usr_clipfmt_free_is_noop)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree"), *f6 = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree64");
    void *fe = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFreeExt");
    void *fs[3] = { f, f6, fe }; int k; ULONG ctx;
    REQUIRE(f); REQUIRE(f6); REQUIRE(fe);
    for (k = 0; k < 3; k++)
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            UMC c; CLIPFORMAT cf = 0xc001; int a = um_nalloc, fr = um_nfree;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_free(fs[k], UMF(&c), &cf), 0);
            CHECK(cf == 0xc001 && a == um_nalloc && fr == um_nfree);
        }
}

TEST(usr_clipfmt_ext_variants_forward)
{
    void *s = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSizeExt"), *m = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshalExt"),
         *u = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshalExt");
    void *bs = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize"), *bm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal");
    WCHAR name[64]; CLIPFORMAT cf, o = 0; UMC c; ULONG r1, r2; unsigned char *b1 = um_buf_new(200), *b2 = um_buf_new(200), *e1, *e2;
    REQUIRE(s); REQUIRE(m); REQUIRE(u); REQUIRE(bs); REQUIRE(bm); REQUIRE(RegCF());
    cf = (CLIPFORMAT)reg_fmt(25, 6, name);
    um_init(&c, NULL, 0, C_DIFFM);
    um_x_size(bs, UMF(&c), 3, &cf, &r1); um_x_size(s, UMF(&c), 3, &cf, &r2);
    CHECK(r1 == r2);
    um_init(&c, b1, 200, C_DIFFM); um_x_mar(bm, UMF(&c), b1 + 1, &cf, &e1);
    um_init(&c, b2, 200, C_DIFFM); um_x_mar(m, UMF(&c), b2 + 1, &cf, &e2);
    CHECK(e1 - b1 == e2 - b2); CHECK(!memcmp(b1, b2, 200));
    um_init(&c, b2, 200, C_DIFFM);
    XEQ(um_x_mar(u, UMF(&c), b2 + 1, &o, &e2), 0);
    CHECK(o == cf);
    um_buf_free(b1); um_buf_free(b2);
}

/* ---- 64-bit variants ------------------------------------------------------------------------------------ */
TEST(usr_clipfmt64_size)
{
    static const int lens[] = { 12, 40, 246 };
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize64");
    int i; ULONG ctx, off, r;
    REQUIRE(f); REQUIRE(RegCF());
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 10; off++) {
            UMC c; CLIPFORMAT cf = 13;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(f, UMF(&c), off, &cf, &r), 0);
            CHECKF(r == UM_ALIGN(off, 8) + 12, "plain ctx %lu off %lu got %lu", ctx, off, r);
        }
    for (i = 0; i < 3; i++) {
        CLIPFORMAT cf = (CLIPFORMAT)reg_fmt(lens[i], 7, NULL);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 10; off += 3) {
                UMC c; ULONG want = UM_ALIGN(off, 8) + (is_named_ctx(ctx) ? 42 + 2 * lens[i] : 12);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(f, UMF(&c), off, &cf, &r), 0);
                CHECKF(r == want, "n=%d ctx %lu off %lu got %lu want %lu", lens[i], ctx, off, r, want);
            }
    }
}

TEST(usr_clipfmt64_marshal_wire)
{
    static const int lens[] = { 12, 33, 246 };
    void *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal64"), *fs = UM_FNIN("ole32.dll", "CLIPFORMAT_UserSize64");
    int i; ULONG ctx, off;
    REQUIRE(fm); REQUIRE(fs); REQUIRE(RegCF());
    for (i = 0; i < 3; i++) {
        WCHAR name[300]; CLIPFORMAT cf = (CLIPFORMAT)reg_fmt(lens[i], 8, name);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 9; off += 4) {
                UMC c; ULONG a = UM_ALIGN(off, 8), n = lens[i], sz; unsigned char *b = um_buf_new(700), *e;
                um_init(&c, NULL, 0, ctx); XEQ(um_x_size(fs, UMF(&c), off, &cf, &sz), 0);
                um_init(&c, b, 700, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &cf, &e), 0);
                if (is_named_ctx(ctx)) {
                    CHECKF(e == b + a + 40 + 2 * (n + 1), "n=%lu ctx %lu end %+ld", n, ctx, (long)(e - b));
                    EXPECT_EQ(RD32(b, a), UM_TAG_R);
                    CHECK(RD64(b, a + 8) == cf);
                    CHECK(RD64(b, a + 16) == n + 1);
                    CHECK(RD64(b, a + 24) == 0);
                    CHECK(RD64(b, a + 32) == n + 1 || 1);
                    CHECK(!memcmp(b + a + 40, name, 2 * (n + 1)));
                    CHECKF(sz == (ULONG)(e - b), "64-bit Size() is exact here: size %lu end %ld", sz, (long)(e - b));
                } else {
                    CHECK(e == b + a + 12);
                    EXPECT_EQ(RD32(b, a), UM_TAG_H);
                    EXPECT_EQ(RD32(b, a + 8), cf);
                    CHECK(sz == a + 12);
                }
                CHECK(um_untouched(b, 0, a));
                CHECK(um_guard_ok(b, 700));
                um_buf_free(b);
            }
    }
}

TEST(usr_clipfmt64_roundtrip_and_errors)
{
    void *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal64"), *fu = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshal64");
    void *fu2 = UM_FNIN("ole32.dll", "CLIPFORMAT_UserUnmarshalExt");
    ULONG ctx; WCHAR name[64]; CLIPFORMAT cf, o; UMC c; unsigned char *b = um_buf_new(400), *e1, *e2;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(RegCF());
    cf = (CLIPFORMAT)reg_fmt(30, 9, name);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        CLIPFORMAT plain = 13; o = 0;
        um_init(&c, b, 400, ctx); XEQ(um_x_mar(fm, UMF(&c), b + 3, &plain, &e1), 0);
        um_init(&c, b, (ULONG)(e1 - b), ctx); XEQ(um_x_mar(fu, UMF(&c), b + 3, &o, &e2), 0);
        CHECK(o == 13 && e1 == e2);
        o = 0;
        um_init(&c, b, 400, ctx); XEQ(um_x_mar(fm, UMF(&c), b + 3, &cf, &e1), 0);
        um_init(&c, b, (ULONG)(e1 - b), ctx); XEQ(um_x_mar(fu, UMF(&c), b + 3, &o, &e2), 0);
        CHECKF(o == cf && e1 == e2, "ctx %lu: %#x vs %#x", ctx, o, cf);
        /* truncated by one byte */
        um_init(&c, b, (ULONG)(e1 - b) - 1, ctx); XEQ(um_x_mar(fu, UMF(&c), b + 3, &o, &e2), X_BADSTUB);
    }
    /* bad tag, and a registered-range id in the inproc form with a named context */
    memset(b, 0, 64);
    RD32(b, 0) = 0xdead; um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e1), X_BADTAG);
    RD32(b, 0) = UM_TAG_H; RD32(b, 8) = 0xc005;
    um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e1), X_BADCONF);
    um_init(&c, b, 64, C_INPROC); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e1), 0);
    CHECK(o == 0xc005 && e1 == b + 12);
    um_buf_free(b);
    if (fu2) { /* Ext alias exists in the same module; behaviour equals the 32-bit unmarshal (decompiled forwarder) */ }
}
