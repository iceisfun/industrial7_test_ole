/* usrmarshal_ole32: HWND/HICON/HMONITOR/HDC/HMENU/HACCEL/HBRUSH/HRGN wire helpers.
 * On Win10 all of these share one implementation (verified: HWND_UserSize, HDC_UserSize ... decompile to the same
 * body): wire = DWORD tag 'WdtH' + 32-bit handle, 4-aligned, 8 bytes; a non-NULL handle with a "remote" context
 * (low flags byte 2 or 5) raises 0x6c5. Free is a no-op. The *64 variants are aliases of the 32 ones. */
#include "um.h"

static const ULONG_PTR k_hvals[] = { 0, 1, 0x00123456, 0x7fffffff };
static const ULONG k_nonremote[] = { C_LOCAL, C_NOSHM, C_INPROC, C_CROSSX };

static void *hfn(const char *pfx, const char *op, const char *sfx)
{
    char n[64];
    snprintf(n, sizeof n, "%s_User%s%s", pfx, op, sfx);
    return UM_FNIN("ole32.dll", n);
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_HWND/HDC/HICON/HBRUSH */
TEST(usr_handle_exports_present)
{
    int i, s;
    for (i = 0; i < um_nhfams; i++)
        for (s = 0; s < (um_hfams[i].has64 ? 2 : 1); s++) {
            if (t_is_wine()) { t_note("wine does not export every helper"); }
            static const char *ops[] = { "Size", "Marshal", "Unmarshal", "Free" };
            int o;
            for (o = 0; o < 4; o++) {
                void *f = hfn(um_hfams[i].pfx, ops[o], s ? "64" : "");
                CHECKF(f != NULL, "%s_User%s%s missing", um_hfams[i].pfx, ops[o], s ? "64" : "");
            }
        }
}

TEST(usr_handle_size_null_obj)
{
    int i; ULONG off, ctx, r;
    for (i = 0; i < um_nhfams; i++) {
        void *f = hfn(um_hfams[i].pfx, "Size", "");
        NEED(f);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 10; off++) {
                UMC c; um_init(&c, NULL, 0, ctx);
                EXPECT_EQ(um_x_size(f, UMF(&c), off, NULL, &r), 0);
                CHECKF(r == off, "%s ctx %lu off %lu got %lu", um_hfams[i].pfx, ctx, off, r);
            }
    }
}

TEST(usr_handle_size_alignment)
{
    int i, v; ULONG off, r;
    for (i = 0; i < um_nhfams; i++) {
        void *f = hfn(um_hfams[i].pfx, "Size", "");
        NEED(f);
        for (v = 0; v < 4; v++)
            for (off = 0; off < 20; off++) {
                UMC c; HANDLE h = (HANDLE)k_hvals[v];
                um_init(&c, NULL, 0, C_LOCAL);
                EXPECT_EQ(um_x_size(f, UMF(&c), off, &h, &r), 0);
                CHECKF(r == UM_ALIGN(off, 4) + 8, "%s h=%p off %lu got %lu", um_hfams[i].pfx, h, off, r);
            }
    }
}

TEST(usr_handle_size_context_matrix)
{
    int i, k; ULONG r;
    for (i = 0; i < um_nhfams; i++) {
        void *f = hfn(um_hfams[i].pfx, "Size", "");
        NEED(f);
        for (k = 0; k < 4; k++) {   /* contexts that are not "remote" */
            UMC c; HANDLE h = (HANDLE)0x4321;
            um_init(&c, NULL, 0, k_nonremote[k]);
            EXPECT_EQ(um_x_size(f, UMF(&c), 0, &h, &r), 0);
            CHECKF(r == 8, "%s ctx %lu size %lu", um_hfams[i].pfx, k_nonremote[k], r);
        }
        for (k = 0; k < 2; k++) {   /* REMOTE_CALL(): low byte 2 or 5 */
            UMC c; HANDLE h = (HANDLE)0x4321, n = NULL;
            ULONG ctx = k ? C_CONT : C_DIFFM;
            um_init(&c, NULL, 0, ctx);
            EXPECT_EQ(um_x_size(f, UMF(&c), 0, &h, &r), X_BADTAG);
            EXPECT_EQ(um_x_size(f, UMF(&c), 0, &n, &r), 0);
            CHECKF(r == 8, "%s remote null size %lu", um_hfams[i].pfx, r);
        }
    }
}

TEST(usr_handle_marshal_wire)
{
    int i, v; ULONG off;
    for (i = 0; i < um_nhfams; i++) {
        void *f = hfn(um_hfams[i].pfx, "Marshal", "");
        NEED(f);
        for (v = 0; v < 4; v++)
            for (off = 0; off < 5; off++) {
                UMC c; HANDLE h = (HANDLE)k_hvals[v];
                unsigned char *b = um_buf_new(32), *end; ULONG a = UM_ALIGN(off, 4);
                um_init(&c, b, 32, C_LOCAL);
                EXPECT_EQ(um_x_mar(f, UMF(&c), b + off, &h, &end), 0);
                CHECKF(end == b + a + 8, "%s off %lu end %+ld", um_hfams[i].pfx, off, (long)(end - b));
                EXPECT_EQ(*(ULONG *)(b + a), UM_TAG_H);
                EXPECT_EQ(*(ULONG *)(b + a + 4), (ULONG)k_hvals[v]);
                CHECK(um_untouched(b, 0, a));          /* no padding written, nothing before */
                CHECK(um_untouched(b, a + 8, 32));     /* nothing written after */
                CHECK(um_guard_ok(b, 32));
                um_buf_free(b);
            }
    }
}

TEST(usr_handle_marshal_null_obj)
{
    int i; ULONG off;
    for (i = 0; i < um_nhfams; i++) {
        void *f = hfn(um_hfams[i].pfx, "Marshal", "");
        NEED(f);
        for (off = 0; off < 4; off++) {
            UMC c; unsigned char *b = um_buf_new(16), *end;
            um_init(&c, b, 16, C_LOCAL);
            EXPECT_EQ(um_x_mar(f, UMF(&c), b + off, NULL, &end), 0);
            CHECKF(end == b + off, "%s unaligned buffer must be returned unchanged", um_hfams[i].pfx);
            CHECK(um_untouched(b, 0, 16));
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_marshal_remote_raises)
{
    int i, k;
    for (i = 0; i < um_nhfams; i++) {
        void *f = hfn(um_hfams[i].pfx, "Marshal", "");
        NEED(f);
        for (k = 0; k < 2; k++) {
            UMC c; HANDLE h = (HANDLE)0x77, n = NULL; unsigned char *b = um_buf_new(16), *end;
            um_init(&c, b, 16, k ? C_CONT : C_DIFFM);
            EXPECT_EQ(um_x_mar(f, UMF(&c), b, &h, &end), X_BADTAG);
            CHECK(um_untouched(b, 0, 16));
            EXPECT_EQ(um_x_mar(f, UMF(&c), b, &n, &end), 0);       /* NULL handle is fine remotely */
            CHECK(end == b + 8);
            EXPECT_EQ(*(ULONG *)b, UM_TAG_H);
            EXPECT_EQ(*(ULONG *)(b + 4), 0);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unmarshal_roundtrip)
{
    int i, v, k;
    for (i = 0; i < um_nhfams; i++) {
        void *fm = hfn(um_hfams[i].pfx, "Marshal", ""), *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        NEED(fm); NEED(fu);
        for (k = 0; k < 4; k++)
            for (v = 0; v < 4; v++) {
                UMC c; HANDLE h = (HANDLE)k_hvals[v], o = (HANDLE)0x5555; unsigned char *b = um_buf_new(32), *e1, *e2;
                um_init(&c, b, 32, k_nonremote[k]);
                EXPECT_EQ(um_x_mar(fm, UMF(&c), b + 1, &h, &e1), 0);
                um_init(&c, b, (ULONG)(e1 - b), k_nonremote[k]);
                EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e2), 0);
                CHECK(e2 == e1);
                CHECKF(o == h, "%s ctx %lu: %p != %p", um_hfams[i].pfx, k_nonremote[k], o, h);
                um_buf_free(b);
            }
    }
}

TEST(usr_handle_unmarshal_sign_extends)
{
    /* the wire carries 32 bits; Win10 sign-extends them into the 64-bit handle */
    int i;
    static const struct { ULONG wire; ULONG_PTR want; } t[] = {
        { 0x80000001u, 0xFFFFFFFF80000001ull }, { 0xFFFFFFFFu, (ULONG_PTR)-1 }, { 0x7FFFFFFFu, 0x7FFFFFFF }, { 0x00010002u, 0x10002 },
    };
    for (i = 0; i < um_nhfams; i++) {
        void *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        int k;
        NEED(fu);
        for (k = 0; k < 4; k++) {
            UMC c; HANDLE o = NULL; unsigned char *b = um_buf_new(16), *e;
            ULONG ctx = C_LOCAL;
            ((ULONG *)b)[0] = UM_TAG_H; ((ULONG *)b)[1] = t[k].wire;
            um_init(&c, b, 8, ctx);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
            CHECKF((ULONG_PTR)o == t[k].want, "%s wire %08lx got %p", um_hfams[i].pfx, t[k].wire, o);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unmarshal_remote)
{
    int i, k;
    for (i = 0; i < um_nhfams; i++) {
        void *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        NEED(fu);
        for (k = 0; k < 2; k++) {
            UMC c; HANDLE o = (HANDLE)9; unsigned char *b = um_buf_new(16), *e;
            ((ULONG *)b)[0] = UM_TAG_H; ((ULONG *)b)[1] = 0x1234;
            um_init(&c, b, 8, k ? C_CONT : C_DIFFM);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);
            ((ULONG *)b)[1] = 0;
            um_init(&c, b, 8, k ? C_CONT : C_DIFFM);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
            CHECK(o == NULL); CHECK(e == b + 8);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unmarshal_bad_tag)
{
    static const ULONG tags[] = { 0, UM_TAG_R, UM_TAG_P, 0x48746458u, 0xFFFFFFFFu, 1 };
    int i, t;
    for (i = 0; i < um_nhfams; i++) {
        void *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        NEED(fu);
        for (t = 0; t < 6; t++) {
            UMC c; HANDLE o = (HANDLE)9; unsigned char *b = um_buf_new(16), *e;
            ((ULONG *)b)[0] = tags[t]; ((ULONG *)b)[1] = 5;
            um_init(&c, b, 8, C_LOCAL);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);
            CHECKF(o == (HANDLE)9, "output must be untouched on failure");
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unmarshal_truncated)
{
    int i; ULONG len;
    for (i = 0; i < um_nhfams; i++) {
        void *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        NEED(fu);
        for (len = 0; len <= 9; len++) {
            UMC c; HANDLE o = NULL; unsigned char *b = um_buf_new(16), *e;
            ((ULONG *)b)[0] = UM_TAG_H; ((ULONG *)b)[1] = 5;
            um_init(&c, b, len, C_LOCAL);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b, &o, &e), len < 8 ? X_BADSTUB : 0);
            if (len >= 8) CHECK(o == (HANDLE)5);
            um_buf_free(b);
        }
        {   /* alignment is counted: supplied=b+1 aligns to b+4, needs BufferLength >= 12 */
            UMC c; HANDLE o = NULL; unsigned char *b = um_buf_new(32), *e;
            ((ULONG *)(b + 4))[0] = UM_TAG_H; ((ULONG *)(b + 4))[1] = 5;
            um_init(&c, b, 11, C_LOCAL);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e), X_BADSTUB);
            um_init(&c, b, 12, C_LOCAL);
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e), 0);
            CHECK(e == b + 12);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unmarshal_supplied_outside)
{
    int i;
    for (i = 0; i < um_nhfams; i++) {
        void *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        NEED(fu);
        {
            UMC c; HANDLE o = NULL; unsigned char *b = um_buf_new(64), *e;
            ((ULONG *)(b + 16))[0] = UM_TAG_H; ((ULONG *)(b + 16))[1] = 5;
            um_init(&c, b + 8, 16, C_LOCAL);         /* declared range [b+8, b+24] */
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 7, &o, &e), X_BADBUFPOS);   /* before */
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 25, &o, &e), X_BADBUFPOS);  /* after */
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 24, &o, &e), X_BADSTUB);    /* at the end: allowed, but empty */
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b + 16, &o, &e), 0);            /* exactly 8 bytes left: fits */
            CHECK(o == (HANDLE)5);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unmarshal_bad_signature)
{
    /* NdrGetUserMarshalInfo fails (0x57) with a bad signature; CUserMarshalInfo then skips the position check
     * and GetBufferSize() reports 0x7fffffff, so no bounds are enforced. */
    int i;
    for (i = 0; i < um_nhfams; i++) {
        void *fu = hfn(um_hfams[i].pfx, "Unmarshal", "");
        NEED(fu);
        {
            UMC c; HANDLE o = NULL; unsigned char *b = um_buf_new(16), *e;
            ((ULONG *)b)[0] = UM_TAG_H; ((ULONG *)b)[1] = 6;
            um_init(&c, b, 0, C_LOCAL);
            c.cb.Signature = 0;
            EXPECT_EQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
            CHECK(o == (HANDLE)6); CHECK(e == b + 8);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_unaligned_marshal)
{
    int i; ULONG off;
    for (i = 0; i < um_nhfams; i++) {
        void *fm = hfn(um_hfams[i].pfx, "Marshal", "");
        NEED(fm);
        for (off = 1; off < 4; off++) {
            UMC c; HANDLE h = (HANDLE)0x1020; unsigned char *b = um_buf_new(32), *e;
            um_init(&c, b, 32, C_LOCAL);
            EXPECT_EQ(um_x_mar(fm, UMF(&c), b + off, &h, &e), 0);
            CHECK(e == b + 12);      /* aligned up to 4 even though b + off is not */
            EXPECT_EQ(*(ULONG *)(b + 4), UM_TAG_H);
            CHECK(um_untouched(b, 0, 4));
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_free_is_noop)
{
    int i, k;
    for (i = 0; i < um_nhfams; i++) {
        void *ff = hfn(um_hfams[i].pfx, "Free", "");
        NEED(ff);
        for (k = 0; k <= C_CONT; k++) {
            UMC c; HANDLE h = (HANDLE)0x1234;
            int a = um_nalloc, f = um_nfree;
            um_init(&c, NULL, 0, k);
            EXPECT_EQ(um_x_free(ff, UMF(&c), &h), 0);
            CHECK(h == (HANDLE)0x1234);
            h = NULL;
            EXPECT_EQ(um_x_free(ff, UMF(&c), &h), 0);
            CHECK(a == um_nalloc && f == um_nfree);
        }
    }
}

TEST(usr_handle_flags_use_low_byte_only)
{
    int i;
    for (i = 0; i < um_nhfams; i++) {
        void *fs = hfn(um_hfams[i].pfx, "Size", ""), *fm = hfn(um_hfams[i].pfx, "Marshal", "");
        NEED(fs); NEED(fm);
        {
            UMC c; HANDLE h = (HANDLE)0x42; ULONG r; unsigned char *b = um_buf_new(16), *e;
            um_init(&c, NULL, 0, C_LOCAL);
            c.cb.Flags = 0x0100 | (0x10u << 16) | C_LOCAL;           /* aux bit 8 + data representation */
            EXPECT_EQ(um_x_size(fs, UMF(&c), 0, &h, &r), 0);
            CHECK(r == 8);
            c.cb.Flags = 0x0100 | (0x10u << 16) | C_DIFFM;
            EXPECT_EQ(um_x_size(fs, UMF(&c), 0, &h, &r), X_BADTAG);
            c.cb.Flags = 0x0000 | (0x10u << 16) | C_CONT;
            EXPECT_EQ(um_x_size(fs, UMF(&c), 0, &h, &r), X_BADTAG);
            c.cb.Flags = 0x1200 | C_LOCAL;
            um_init(&c, b, 16, C_LOCAL); c.cb.Flags = 0x1200 | C_LOCAL;
            EXPECT_EQ(um_x_mar(fm, UMF(&c), b, &h, &e), 0);
            CHECK(e == b + 8);
            um_buf_free(b);
        }
    }
}

TEST(usr_handle_64_aliases_match_32)
{
    int i;
    for (i = 0; i < um_nhfams; i++) {
        void *s32, *s64, *m32, *m64, *u32, *u64;
        if (!um_hfams[i].has64) continue;
        s32 = hfn(um_hfams[i].pfx, "Size", ""); s64 = hfn(um_hfams[i].pfx, "Size", "64");
        m32 = hfn(um_hfams[i].pfx, "Marshal", ""); m64 = hfn(um_hfams[i].pfx, "Marshal", "64");
        u32 = hfn(um_hfams[i].pfx, "Unmarshal", ""); u64 = hfn(um_hfams[i].pfx, "Unmarshal", "64");
        NEED(s32); NEED(s64); NEED(m32); NEED(m64); NEED(u32); NEED(u64);
        {
            ULONG off, r1, r2;
            for (off = 0; off < 17; off++) {
                UMC c; HANDLE h = (HANDLE)0x99; um_init(&c, NULL, 0, C_LOCAL);
                um_x_size(s32, UMF(&c), off, &h, &r1); um_x_size(s64, UMF(&c), off, &h, &r2);
                CHECKF(r1 == r2 && r1 == UM_ALIGN(off, 4) + 8, "%s64 size off %lu: %lu vs %lu (64-bit variants are 4-aligned too)", um_hfams[i].pfx, off, r2, r1);
            }
        }
        {
            UMC c; HANDLE h = (HANDLE)0x99, o = NULL; unsigned char *b1 = um_buf_new(32), *b2 = um_buf_new(32), *e1, *e2;
            um_init(&c, b1, 32, C_LOCAL); um_x_mar(m32, UMF(&c), b1 + 2, &h, &e1);
            um_init(&c, b2, 32, C_LOCAL); um_x_mar(m64, UMF(&c), b2 + 2, &h, &e2);
            CHECK(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 32));
            um_init(&c, b2, 32, C_LOCAL); um_x_mar(u64, UMF(&c), b2 + 2, &o, &e2);
            CHECK(o == h && e2 == b2 + 12);
            um_buf_free(b1); um_buf_free(b2);
        }
    }
}

TEST(usr_hwnd_ext_variants_match)
{
    void *s = UM_FNIN("ole32.dll", "HWND_UserSizeExt"), *m = UM_FNIN("ole32.dll", "HWND_UserMarshalExt"),
         *u = UM_FNIN("ole32.dll", "HWND_UserUnmarshalExt"), *f = UM_FNIN("ole32.dll", "HWND_UserFreeExt"),
         *s6 = UM_FNIN("ole32.dll", "HWND_UserSize64Ext"), *m6 = UM_FNIN("ole32.dll", "HWND_UserMarshal64Ext"),
         *u6 = UM_FNIN("ole32.dll", "HWND_UserUnmarshal64Ext"), *f6 = UM_FNIN("ole32.dll", "HWND_UserFree64Ext");
    void *sz[2] = { s, s6 }, *ma[2] = { m, m6 }, *un[2] = { u, u6 }, *fr[2] = { f, f6 };
    int k;
    REQUIRE(s); REQUIRE(m); REQUIRE(u); REQUIRE(f); REQUIRE(s6); REQUIRE(m6); REQUIRE(u6); REQUIRE(f6);
    for (k = 0; k < 2; k++) {
        UMC c; HANDLE h = (HANDLE)0x3344, o = NULL; ULONG r; unsigned char *b = um_buf_new(32), *e;
        um_init(&c, NULL, 0, C_LOCAL);
        EXPECT_EQ(um_x_size(sz[k], UMF(&c), 1, &h, &r), 0); EXPECT_EQ(r, 12);
        um_init(&c, b, 32, C_LOCAL);
        EXPECT_EQ(um_x_mar(ma[k], UMF(&c), b + 1, &h, &e), 0); CHECK(e == b + 12);
        EXPECT_EQ(*(ULONG *)(b + 4), UM_TAG_H); EXPECT_EQ(*(ULONG *)(b + 8), 0x3344);
        EXPECT_EQ(um_x_mar(un[k], UMF(&c), b + 1, &o, &e), 0); CHECK(o == h);
        EXPECT_EQ(um_x_free(fr[k], UMF(&c), &h), 0); CHECK(h == (HANDLE)0x3344);
        um_init(&c, NULL, 0, C_DIFFM);
        EXPECT_EQ(um_x_size(sz[k], UMF(&c), 0, &h, &r), X_BADTAG);
        um_buf_free(b);
    }
}

TEST(usr_handle_combase_parity)
{
    /* combase.dll exports the same helpers for most of these types; behaviour must be identical. */
    static const char *names[] = { "HWND", "HICON", "HMONITOR", "HDC", "HMENU", "HACCEL", "HBRUSH" };
    int i;
    for (i = 0; i < 7; i++) {
        char n[64]; void *so, *sc, *mo, *mc;
        snprintf(n, sizeof n, "%s_UserSize", names[i]); so = UM_FNIN("ole32.dll", n); sc = UM_FNIN("combase.dll", n);
        snprintf(n, sizeof n, "%s_UserMarshal", names[i]); mo = UM_FNIN("ole32.dll", n); mc = UM_FNIN("combase.dll", n);
        if (!sc || !mc) { t_note("combase.dll does not export %s helpers", names[i]); continue; }
        NEED(so); NEED(mo);
        {
            UMC c; HANDLE h = (HANDLE)0x606; ULONG r1, r2; unsigned char *b1 = um_buf_new(16), *b2 = um_buf_new(16), *e1, *e2;
            um_init(&c, NULL, 0, C_LOCAL);
            um_x_size(so, UMF(&c), 3, &h, &r1); um_x_size(sc, UMF(&c), 3, &h, &r2);
            CHECKF(r1 == r2, "%s: %lu vs %lu", names[i], r1, r2);
            um_init(&c, b1, 16, C_LOCAL); um_x_mar(mo, UMF(&c), b1, &h, &e1);
            um_init(&c, b2, 16, C_LOCAL); um_x_mar(mc, UMF(&c), b2, &h, &e2);
            CHECK(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 16));
            um_buf_free(b1); um_buf_free(b2);
        }
    }
}
