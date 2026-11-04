/* usrmarshal_ole32: HGLOBAL_User{Size,Marshal,Unmarshal,Free}[64].
 * Win10 x64 wire format (decompiled):
 *   32-bit, not INPROC:  DWORD 'WdtR', DWORD h32 ; if h != 0:  DWORD size, DWORD h32, DWORD size, BYTE data[size]
 *   32-bit, INPROC (3):  DWORD 'WdtP', pad to 8, QWORD handle value
 *   Size() over-estimates: align4(off)+4+12 (+ size+12 for a non-NULL handle); INPROC: align8(align4(off)+4)+8.
 *   64-bit: 8-byte alignment, QWORD handle, QWORD size, DWORD h32, DWORD size, data at +32. */
#include "um.h"

#define HG_BLOCK_SIZES_N 22
static const SIZE_T k_blk[HG_BLOCK_SIZES_N] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 100, 255, 1000, 4096, 65536 };

static HGLOBAL mk_block(UINT flags, SIZE_T n, unsigned char seed)
{
    HGLOBAL h = GlobalAlloc(flags, n);
    if (h && n) {
        unsigned char *p = GlobalLock(h); SIZE_T i;
        for (i = 0; i < n; i++) p[i] = (unsigned char)(seed + i * 7);
        GlobalUnlock(h);
    }
    return h;
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_HGLOBAL, tightened to Win10 x64 sizes */
TEST(usr_hglobal_size_null_ptr)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserSize"), *f6 = UM_FNIN("ole32.dll", "HGLOBAL_UserSize64");
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

TEST(usr_hglobal_size_null_handle)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserSize");
    ULONG off, ctx, r;
    REQUIRE(f);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 18; off++) {
            UMC c; HGLOBAL h = NULL;
            ULONG want = ctx == C_INPROC ? UM_ALIGN(UM_ALIGN(off, 4) + 4, 8) + 8 : UM_ALIGN(off, 4) + 16;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(f, UMF(&c), off, &h, &r), 0);
            CHECKF(r == want, "ctx %lu off %lu got %lu want %lu", ctx, off, r, want);
        }
    /* Wine's test: native "allocates 4/8 bytes more than it needs": 16 for off 0, on Win10 x64 */
}

TEST(usr_hglobal_size_blocks)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserSize");
    int i, fx; ULONG ctx, off, r;
    REQUIRE(f);
    for (fx = 0; fx < 2; fx++)
        for (i = 0; i < HG_BLOCK_SIZES_N; i++) {
            HGLOBAL h = mk_block(fx ? GMEM_FIXED : GMEM_MOVEABLE, k_blk[i], 1);
            SIZE_T actual;
            if (!h) { t_note("GlobalAlloc(%u) failed", (unsigned)k_blk[i]); continue; }
            actual = GlobalSize(h);
            CHECK(actual >= k_blk[i]);
            for (ctx = 0; ctx <= C_CONT; ctx++)
                for (off = 0; off < 6; off += 3) {
                    UMC c; HGLOBAL hh = h;
                    ULONG want;
                    if (ctx == C_INPROC) want = UM_ALIGN(UM_ALIGN(off, 4) + 4, 8) + 8;
                    else want = UM_ALIGN(off, 4) + 16 + (ULONG)actual + 12;
                    um_init(&c, NULL, 0, ctx);
                    XEQ(um_x_size(f, UMF(&c), off, &hh, &r), 0);
                    CHECKF(r == want, "fixed=%d size %u ctx %lu off %lu got %lu want %lu", fx, (unsigned)k_blk[i], ctx, off, r, want);
                }
            GlobalFree(h);
        }
}

TEST(usr_hglobal_marshal_null_handle)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal");
    ULONG ctx, off;
    REQUIRE(f);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 10; off++) {
            UMC c; HGLOBAL h = NULL; unsigned char *b = um_buf_new(64), *e; ULONG a = UM_ALIGN(off, 4);
            um_init(&c, b, 64, ctx);
            XEQ(um_x_mar(f, UMF(&c), b + off, &h, &e), 0);
            if (ctx == C_INPROC) {
                ULONG p = UM_ALIGN(a + 4, 8);
                CHECKF(e == b + p + 8, "inproc off %lu end %+ld", off, (long)(e - b));
                EXPECT_EQ(RD_U32(b, a), UM_TAG_P);
            } else {
                /* adapted from Wine: tag WDT_REMOTE_CALL, then the (NULL) handle */
                CHECKF(e == b + a + 8, "ctx %lu off %lu end %+ld", ctx, off, (long)(e - b));
                EXPECT_EQ(RD_U32(b, a), UM_TAG_R);
                EXPECT_EQ(RD_U32(b, a + 4), 0);
                CHECK(um_untouched(b, a + 8, 64));
            }
            CHECK(um_untouched(b, 0, a));
            um_buf_free(b);
        }
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_HGLOBAL */
TEST(usr_hglobal_marshal_block_wire)
{
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal");
    int i, fx; ULONG ctx, off;
    REQUIRE(fm);
    for (fx = 0; fx < 2; fx++)
        for (i = 0; i < HG_BLOCK_SIZES_N; i++) {
            HGLOBAL h = mk_block(fx ? GMEM_FIXED : GMEM_MOVEABLE, k_blk[i], (unsigned char)i);
            ULONG actual;
            if (!h) continue;
            actual = (ULONG)GlobalSize(h);
            for (ctx = 0; ctx <= C_CONT; ctx++) {
                if (ctx == C_INPROC) continue;
                for (off = 0; off < 5; off += 2) {
                    UMC c; HGLOBAL hh = h; ULONG a = UM_ALIGN(off, 4); unsigned char *b = um_buf_new(actual + 64), *e, *src;
                    um_init(&c, b, actual + 64, ctx);
                    XEQ(um_x_mar(fm, UMF(&c), b + off, &hh, &e), 0);
                    CHECKF(e == b + a + 20 + actual, "fixed=%d size %lu ctx %lu off %lu end %+ld", fx, actual, ctx, off, (long)(e - b));
                    EXPECT_EQ(RD_U32(b, a), UM_TAG_R);
                    EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)h);
                    EXPECT_EQ(RD_U32(b, a + 8), actual);
                    EXPECT_EQ(RD_U32(b, a + 12), (ULONG)(ULONG_PTR)h);
                    EXPECT_EQ(RD_U32(b, a + 16), actual);
                    src = GlobalLock(h);
                    if (src && actual) CHECK(!memcmp(b + a + 20, src, k_blk[i] ? k_blk[i] : 0));
                    GlobalUnlock(h);
                    CHECK(um_untouched(b, 0, a));
                    CHECK(um_untouched(b, a + 20 + actual, actual + 64));
                    CHECK(um_guard_ok(b, actual + 64));
                    um_buf_free(b);
                }
            }
            GlobalFree(h);
        }
}

TEST(usr_hglobal_marshal_inproc_pointer_form)
{
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal"), *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal");
    HGLOBAL h = mk_block(GMEM_MOVEABLE, 40, 3), o = NULL; ULONG off;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(h);
    for (off = 0; off < 9; off++) {
        UMC c; unsigned char *b = um_buf_new(64), *e, *e0; ULONG a = UM_ALIGN(off, 4), p = UM_ALIGN(a + 4, 8);
        HGLOBAL hh = h;
        um_init(&c, b, 64, C_INPROC);
        XEQ(um_x_mar(fm, UMF(&c), b + off, &hh, &e), 0);
        CHECK(e == b + p + 8);
        EXPECT_EQ(RD_U32(b, a), UM_TAG_P);
        CHECK(*(HGLOBAL *)(b + p) == h);          /* the handle value itself travels, no data copy */
        o = NULL; e0 = e;
        um_init(&c, b, (ULONG)(e - b), C_INPROC);
        if (off % 4) {
            /* Win10 (decompilation): the length check counts the 4-alignment pad twice, so a buffer that ends exactly
             * at the marshalled data is refused with RPC_X_BAD_STUB_DATA; with slack the same data unmarshals. */
            XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), X_BADSTUB);
            if (off == 1) t_note("HGLOBAL_UserUnmarshal with a not 4-aligned start and an exactly sized buffer: RPC_X_BAD_STUB_DATA");
            o = NULL; um_init(&c, b, (ULONG)(e0 - b) + 8, C_INPROC);
            XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
            CHECKF(o == h, "inproc unmarshal with slack returns the same handle");
        } else {
            XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
            CHECKF(o == h, "inproc unmarshal returns the same handle");
            CHECK(e == b + p + 8);
        }
        CHECK(GlobalSize(h) >= 40);               /* still alive */
        um_buf_free(b);
    }
    GlobalFree(h);
}

TEST(usr_hglobal_roundtrip_remote)
{
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal"), *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal");
    void *ff = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    int i; ULONG ctx;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(ff);
    for (i = 0; i < HG_BLOCK_SIZES_N; i++)
        for (ctx = 0; ctx <= C_CONT; ctx += (ctx == C_LOCAL ? 2 : 3)) {
            HGLOBAL h = mk_block(GMEM_MOVEABLE, k_blk[i], 0x40), o = NULL; UMC c; unsigned char *b, *e1, *e2; SIZE_T actual;
            if (!h) continue;
            actual = GlobalSize(h);
            b = um_buf_new(actual + 64);
            um_init(&c, b, (ULONG)actual + 64, ctx);
            XEQ(um_x_mar(fm, UMF(&c), b + 1, &h, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), ctx);
            XEQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e2), 0);
            CHECK(e2 == e1);
            CHECKF(o != NULL, "unmarshal of a non-NULL handle yields a handle (size %u)", (unsigned)k_blk[i]);
            if (o) {
                CHECKF(GlobalSize(o) == actual, "copy has the original size: %u vs %u", (unsigned)GlobalSize(o), (unsigned)actual);
                if (actual) {
                    unsigned char *p = GlobalLock(o), *q = GlobalLock(h);
                    CHECK(p && q && !memcmp(p, q, k_blk[i]));
                    GlobalUnlock(o); GlobalUnlock(h);
                }
                CHECK(o != h);
                /* UserFree on a non-inproc context frees the block */
                {   UMC c2; um_init(&c2, NULL, 0, ctx);
                    XEQ(um_x_free(ff, UMF(&c2), &o), 0);
                    SetLastError(0);
                    CHECK(GlobalSize(o) == 0); }
            }
            GlobalFree(h);
            um_buf_free(b);
        }
}

TEST(usr_hglobal_unmarshal_null_handle)
{
    void *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal");
    ULONG ctx;
    REQUIRE(fu);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        UMC c; HGLOBAL o = (HGLOBAL)(ULONG_PTR)0x1234; unsigned char *b = um_buf_new(32), *e;
        if (ctx == C_INPROC) continue;
        RD_U32(b, 0) = UM_TAG_R; RD_U32(b, 4) = 0;
        o = NULL;
        um_init(&c, b, 8, ctx);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECK(o == NULL); CHECK(e == b + 8);
        um_buf_free(b);
    }
}

TEST(usr_hglobal_unmarshal_reuses_matching_block)
{
    /* if *out already holds a block of exactly the wire size, the data is copied into it (handle kept) */
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal"), *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal");
    HGLOBAL src = mk_block(GMEM_MOVEABLE, 64, 9), dst = mk_block(GMEM_MOVEABLE, 64, 100), o;
    UMC c; unsigned char *b = um_buf_new(200), *e, *p;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(src); REQUIRE(dst);
    o = dst;
    um_init(&c, b, 200, C_LOCAL);
    XEQ(um_x_mar(fm, UMF(&c), b, &src, &e), 0);
    um_init(&c, b, (ULONG)(e - b), C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o == dst);
    p = GlobalLock(o);
    CHECK(p && p[0] == 9 && p[1] == (unsigned char)(9 + 7));
    GlobalUnlock(o);
    GlobalFree(src); GlobalFree(o);
    um_buf_free(b);
}

TEST(usr_hglobal_unmarshal_errors)
{
    void *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal");
    UMC c; HGLOBAL o; unsigned char *b = um_buf_new(200), *e; ULONG len;
    REQUIRE(fu);

    /* unknown discriminant */
    RD_U32(b, 0) = 0x1234; RD_U32(b, 4) = 0; o = NULL;
    um_init(&c, b, 64, C_INPROC); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);
    /* 'WdtR' is not acceptable inproc; 'WdtH'/'WdtP' are not acceptable elsewhere */
    RD_U32(b, 0) = UM_TAG_R;
    um_init(&c, b, 64, C_INPROC); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 0) = UM_TAG_H;
    um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 0) = UM_TAG_P;
    um_init(&c, b, 64, C_DIFFM); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 0) = 0x1234;
    um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    /* 'WdtH' is accepted inproc: the 32-bit value becomes the handle */
    RD_U32(b, 0) = UM_TAG_H; RD_U32(b, 4) = 0x1234; o = NULL;
    um_init(&c, b, 64, C_INPROC); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o == (HGLOBAL)(ULONG_PTR)0x1234); CHECK(e == b + 8);

    /* truncated header, header consistency */
    RD_U32(b, 0) = UM_TAG_R; RD_U32(b, 4) = 0;
    for (len = 0; len < 8; len++) { o = NULL; um_init(&c, b, len, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB); }
    um_init(&c, b, 8, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);

    RD_U32(b, 4) = 0x77; RD_U32(b, 8) = 4; RD_U32(b, 12) = 0x77; RD_U32(b, 16) = 4; RD_U32(b, 20) = 0x11223344;
    for (len = 0; len < 24; len++) {
        o = NULL; um_init(&c, b, len, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);   /* header needs 20 bytes, header + data 24 */
        CHECK(o == NULL);
    }
    o = NULL; um_init(&c, b, 24, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o != NULL && e == b + 24);
    if (o) { unsigned char *p = GlobalLock(o); CHECK(p && *(ULONG *)p == 0x11223344); GlobalUnlock(o); GlobalFree(o); }

    RD_U32(b, 12) = 0x78;      /* second copy of the handle must match */
    o = NULL; um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 12) = 0x77; RD_U32(b, 16) = 5;     /* second copy of the size must match */
    um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 8) = 1000; RD_U32(b, 16) = 1000;   /* size larger than the buffer */
    um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    um_buf_free(b);
}

TEST(usr_hglobal_free_semantics)
{
    void *ff = UM_FNIN("ole32.dll", "HGLOBAL_UserFree"), *f6 = UM_FNIN("ole32.dll", "HGLOBAL_UserFree64");
    int k;
    REQUIRE(ff); REQUIRE(f6);
    for (k = 0; k < 2; k++) {
        void *f = k ? f6 : ff; UMC c; HGLOBAL h = NULL;
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(f, UMF(&c), NULL), 0);
        XEQ(um_x_free(f, UMF(&c), &h), 0);
        /* INPROC: the handle belongs to the caller, nothing is freed */
        h = mk_block(GMEM_MOVEABLE, 10, 1);
        um_init(&c, NULL, 0, C_INPROC);
        XEQ(um_x_free(f, UMF(&c), &h), 0);
        CHECK(GlobalSize(h) >= 10);
        /* any other context frees it */
        um_init(&c, NULL, 0, C_DIFFM);
        XEQ(um_x_free(f, UMF(&c), &h), 0);
        SetLastError(0);
        CHECK(GlobalSize(h) == 0);
    }
}

/* ---- 64-bit variants -------------------------------------------------------------------------------------- */
TEST(usr_hglobal64_size)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserSize64");
    int i; ULONG ctx, off, r;
    REQUIRE(f);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 18; off++) {
            UMC c; HGLOBAL h = NULL; um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(f, UMF(&c), off, &h, &r), 0);
            CHECKF(r == UM_ALIGN(off, 8) + 16, "null ctx %lu off %lu got %lu", ctx, off, r);
        }
    for (i = 0; i < HG_BLOCK_SIZES_N; i += 3) {
        HGLOBAL h = mk_block(GMEM_MOVEABLE, k_blk[i], 5);
        SIZE_T actual;
        if (!h) continue;
        actual = GlobalSize(h);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 10; off += 3) {
                UMC c; HGLOBAL hh = h; ULONG want = UM_ALIGN(off, 8) + 16 + (ctx == C_INPROC ? 0 : (ULONG)actual + 16);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(f, UMF(&c), off, &hh, &r), 0);
                CHECKF(r == want, "size %u ctx %lu off %lu got %lu want %lu", (unsigned)actual, ctx, off, r, want);
            }
        GlobalFree(h);
    }
}

TEST(usr_hglobal64_marshal_wire)
{
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal64");
    int i; ULONG ctx, off;
    REQUIRE(fm);
    for (i = 0; i < HG_BLOCK_SIZES_N; i += 2) {
        HGLOBAL h = mk_block(GMEM_MOVEABLE, k_blk[i], 5);
        ULONG actual;
        if (!h) continue;
        actual = (ULONG)GlobalSize(h);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 10; off += 3) {
                UMC c; HGLOBAL hh = h, nh = NULL; ULONG a = UM_ALIGN(off, 8); unsigned char *b = um_buf_new(actual + 128), *e, *src;
                um_init(&c, b, actual + 128, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &nh, &e), 0);
                CHECK(e == b + a + 16);
                EXPECT_EQ(RD_U32(b, a), ctx == C_INPROC ? UM_TAG_P : UM_TAG_R);
                CHECK(*(unsigned long long *)(b + a + 8) == 0);
                memset(b, 0xCC, actual + 128);
                um_init(&c, b, actual + 128, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &hh, &e), 0);
                EXPECT_EQ(RD_U32(b, a), ctx == C_INPROC ? UM_TAG_P : UM_TAG_R);
                CHECK(*(HGLOBAL *)(b + a + 8) == h);
                if (ctx == C_INPROC) { CHECK(e == b + a + 16); }
                else {
                    CHECKF(e == b + a + 32 + actual, "size %lu ctx %lu off %lu end %+ld", actual, ctx, off, (long)(e - b));
                    CHECK(*(unsigned long long *)(b + a + 16) == actual);
                    EXPECT_EQ(RD_U32(b, a + 24), (ULONG)(ULONG_PTR)h);
                    EXPECT_EQ(RD_U32(b, a + 28), actual);
                    src = GlobalLock(h);
                    if (actual) CHECK(!memcmp(b + a + 32, src, actual));
                    GlobalUnlock(h);
                }
                CHECK(um_untouched(b, 0, a));
                CHECK(um_guard_ok(b, actual + 128));
                um_buf_free(b);
            }
        GlobalFree(h);
    }
}

TEST(usr_hglobal64_roundtrip)
{
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal64"), *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal64");
    int i; ULONG ctx;
    REQUIRE(fm); REQUIRE(fu);
    for (i = 0; i < HG_BLOCK_SIZES_N; i += 3)
        for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2) {
            HGLOBAL h = mk_block(GMEM_MOVEABLE, k_blk[i], 0x21), o = NULL; UMC c; unsigned char *b, *e1, *e2; SIZE_T actual;
            if (!h) continue;
            actual = GlobalSize(h);
            b = um_buf_new(actual + 128);
            um_init(&c, b, (ULONG)actual + 128, ctx);
            XEQ(um_x_mar(fm, UMF(&c), b + 3, &h, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), ctx);
            XEQ(um_x_mar(fu, UMF(&c), b + 3, &o, &e2), 0);
            CHECK(e2 == e1);
            CHECK(o != NULL);
            if (o) {
                CHECK(GlobalSize(o) == actual);
                if (actual) { unsigned char *p = GlobalLock(o), *q = GlobalLock(h); CHECK(!memcmp(p, q, k_blk[i])); GlobalUnlock(o); GlobalUnlock(h); }
                GlobalFree(o);
            }
            /* truncation by one byte of the data area */
            if (actual) {
                o = NULL;
                um_init(&c, b, (ULONG)(e1 - b) - 1, ctx);
                XEQ(um_x_mar(fu, UMF(&c), b + 3, &o, &e2), X_BADSTUB);
            }
            GlobalFree(h);
            um_buf_free(b);
        }
}
