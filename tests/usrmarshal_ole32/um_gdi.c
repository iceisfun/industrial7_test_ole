/* usrmarshal_ole32: HENHMETAFILE / HMETAFILE / HMETAFILEPICT / HBITMAP / HPALETTE wire helpers.
 * Remote (non-INPROC) wire forms, from the Win10 decompilation:
 *   HENHMETAFILE / HMETAFILE : 'WdtR', h32, n, n, bits[n]                (INPROC: 'WdtH', h32)
 *   HPALETTE                 : 'WdtR', h32, count, WORD 0x300, WORD count, PALETTEENTRY[count]
 *   HBITMAP                  : 'WdtR', h32, cb, bmType, bmWidth, bmHeight, bmWidthBytes, planes|bpp<<16, bits[cb]
 *   64-bit variants: 8-aligned, handle in a QWORD at +8, payload follows. */
#include "um.h"

static HDC (WINAPI *p_CreateEnhMetaFileW)(HDC, LPCWSTR, const RECT *, LPCWSTR);
static HENHMETAFILE (WINAPI *p_CloseEnhMetaFile)(HDC);
static BOOL (WINAPI *p_DeleteEnhMetaFile)(HENHMETAFILE);
static UINT (WINAPI *p_GetEnhMetaFileBits)(HENHMETAFILE, UINT, LPBYTE);
static HDC (WINAPI *p_CreateMetaFileW)(LPCWSTR);
static HMETAFILE (WINAPI *p_CloseMetaFile)(HDC);
static BOOL (WINAPI *p_DeleteMetaFile)(HMETAFILE);
static UINT (WINAPI *p_GetMetaFileBitsEx)(HMETAFILE, UINT, LPVOID);
static BOOL (WINAPI *p_MoveToEx)(HDC, int, int, LPPOINT);
static BOOL (WINAPI *p_LineTo)(HDC, int, int);
static HBITMAP (WINAPI *p_CreateBitmap)(int, int, UINT, UINT, const void *);
static int (WINAPI *p_GetObjectW)(HANDLE, int, LPVOID);
static DWORD (WINAPI *p_GetObjectType)(HGDIOBJ);
static BOOL (WINAPI *p_DeleteObject)(HGDIOBJ);
static LONG (WINAPI *p_GetBitmapBits)(HBITMAP, LONG, LPVOID);
static HPALETTE (WINAPI *p_CreatePalette)(const LOGPALETTE *);
static UINT (WINAPI *p_GetPaletteEntries)(HPALETTE, UINT, UINT, LPPALETTEENTRY);

static int gdi_ok;
static int gdi_load(void)
{
#define L(n) p_##n = (void *)t_proc_in("gdi32.dll", #n)
    L(CreateEnhMetaFileW); L(CloseEnhMetaFile); L(DeleteEnhMetaFile); L(GetEnhMetaFileBits);
    L(CreateMetaFileW); L(CloseMetaFile); L(DeleteMetaFile); L(GetMetaFileBitsEx); L(MoveToEx); L(LineTo);
    L(CreateBitmap); L(GetObjectW); L(GetObjectType); L(DeleteObject); L(GetBitmapBits); L(CreatePalette); L(GetPaletteEntries);
#undef L
    gdi_ok = p_CreateEnhMetaFileW && p_CloseEnhMetaFile && p_GetEnhMetaFileBits && p_CreateMetaFileW && p_CreateBitmap && p_CreatePalette;
    return gdi_ok;
}

static HENHMETAFILE mk_emf(int extra)
{
    const RECT r = { 0, 0, 100, 100 };
    HDC dc = p_CreateEnhMetaFileW(NULL, NULL, &r, L"olet usrmarshal\0emf\0\0");
    int i;
    if (!dc) return NULL;
    for (i = 0; i < extra; i++) { p_MoveToEx(dc, i, 0, NULL); p_LineTo(dc, 99, i); }
    return p_CloseEnhMetaFile(dc);
}
static HMETAFILE mk_mf(int extra)
{
    HDC dc = p_CreateMetaFileW(NULL);
    int i;
    if (!dc) return NULL;
    for (i = 0; i <= extra; i++) { p_MoveToEx(dc, i, 0, NULL); p_LineTo(dc, 50, i); }
    return p_CloseMetaFile(dc);
}

/* ---- HENHMETAFILE and HMETAFILE share the same structure; table-drive them ------------------------------ */
typedef struct { const char *pfx; int is_emf; } mf_kind;
static const mf_kind k_mf[2] = { { "HENHMETAFILE", 1 }, { "HMETAFILE", 0 } };

static void *obj_new(const mf_kind *k, int extra) { return k->is_emf ? (void *)mk_emf(extra) : (void *)mk_mf(extra); }
static UINT obj_bits(const mf_kind *k, void *h) { return k->is_emf ? p_GetEnhMetaFileBits(h, 0, NULL) : p_GetMetaFileBitsEx(h, 0, NULL); }
static UINT obj_getbits(const mf_kind *k, void *h, UINT n, void *buf) { return k->is_emf ? p_GetEnhMetaFileBits(h, n, buf) : p_GetMetaFileBitsEx(h, n, buf); }
static void obj_del(const mf_kind *k, void *h) { if (k->is_emf) p_DeleteEnhMetaFile(h); else p_DeleteMetaFile(h); }
static void *mfn(const mf_kind *k, const char *op, const char *sfx)
{
    char n[64]; snprintf(n, sizeof n, "%s_User%s%s", k->pfx, op, sfx); return UM_FNIN("ole32.dll", n);
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_HENHMETAFILE / test_marshal_HMETAFILE */
TEST(usr_metafile_size)
{
    int k, ex; ULONG ctx, off, r;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    for (k = 0; k < 2; k++) {
        void *f = mfn(&k_mf[k], "Size", ""), *f6 = mfn(&k_mf[k], "Size", "64");
        NEED(f);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 6; off++) {
                UMC c; void *h = NULL;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(f, UMF(&c), off, NULL, &r), 0); CHECK(r == off);
                XEQ(um_x_size(f, UMF(&c), off, &h, &r), 0);
                CHECKF(r == UM_ALIGN(off, 4) + 8, "%s NULL handle ctx %lu off %lu got %lu", k_mf[k].pfx, ctx, off, r);
                if (f6) XEQ(um_x_size(f6, UMF(&c), off, &h, &r), 0);
                if (f6) CHECKF(r == UM_ALIGN(off, 8) + 16, "%s64 NULL handle ctx %lu off %lu got %lu", k_mf[k].pfx, ctx, off, r);
            }
        for (ex = 0; ex < 40; ex += 13) {
            void *h = obj_new(&k_mf[k], ex); UINT n;
            if (!h) { t_note("%s creation failed", k_mf[k].pfx); continue; }
            n = obj_bits(&k_mf[k], h);
            CHECK(n > 0);
            for (ctx = 0; ctx <= C_CONT; ctx++)
                for (off = 0; off < 6; off += 5) {
                    UMC c; void *hh = h;
                    um_init(&c, NULL, 0, ctx);
                    XEQ(um_x_size(f, UMF(&c), off, &hh, &r), 0);
                    CHECKF(r == UM_ALIGN(off, 4) + 8 + (ctx == C_INPROC ? 0 : 8 + n), "%s ctx %lu off %lu bits %u got %lu", k_mf[k].pfx, ctx, off, n, r);
                    if (f6) XEQ(um_x_size(f6, UMF(&c), off, &hh, &r), 0);
                    if (f6) CHECKF(r == UM_ALIGN(off, 8) + 16 + (ctx == C_INPROC ? 0 : 12 + n), "%s64 ctx %lu off %lu bits %u got %lu", k_mf[k].pfx, ctx, off, n, r);
                }
            obj_del(&k_mf[k], h);
        }
    }
}

TEST(usr_metafile_marshal_wire)
{
    int k, ex; ULONG ctx, off;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    for (k = 0; k < 2; k++) {
        void *f = mfn(&k_mf[k], "Marshal", ""), *f6 = mfn(&k_mf[k], "Marshal", "64");
        NEED(f);
        for (ex = 0; ex < 40; ex += 13) {
            void *h = obj_new(&k_mf[k], ex); UINT n; unsigned char *bits;
            if (!h) continue;
            n = obj_bits(&k_mf[k], h);
            bits = malloc(n); obj_getbits(&k_mf[k], h, n, bits);
            for (ctx = 0; ctx <= C_CONT; ctx++)
                for (off = 0; off < 6; off += 3) {
                    UMC c; void *hh = h, *nh = NULL; unsigned char *b = um_buf_new(n + 128), *e; ULONG a = UM_ALIGN(off, 4), a8 = UM_ALIGN(off, 8);
                    /* 32-bit */
                    um_init(&c, b, n + 128, ctx);
                    XEQ(um_x_mar(f, UMF(&c), b + off, &nh, &e), 0);
                    CHECK(e == b + a + 8);
                    EXPECT_EQ(RD_U32(b, a), ctx == C_INPROC ? UM_TAG_H : UM_TAG_R); EXPECT_EQ(RD_U32(b, a + 4), 0);
                    memset(b, 0xCC, n + 128); um_init(&c, b, n + 128, ctx);
                    XEQ(um_x_mar(f, UMF(&c), b + off, &hh, &e), 0);
                    EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)h);
                    if (ctx == C_INPROC) {
                        CHECK(e == b + a + 8); EXPECT_EQ(RD_U32(b, a), UM_TAG_H);
                    } else {
                        CHECKF(e == b + a + 16 + n, "%s ctx %lu end %+ld want %lu", k_mf[k].pfx, ctx, (long)(e - b), a + 16 + n);
                        EXPECT_EQ(RD_U32(b, a), UM_TAG_R);
                        EXPECT_EQ(RD_U32(b, a + 8), n); EXPECT_EQ(RD_U32(b, a + 12), n);
                        CHECK(!memcmp(b + a + 16, bits, n));
                        if (k_mf[k].is_emf) EXPECT_EQ(RD_U32(b, a + 16), 1);          /* EMR_HEADER */
                        CHECK(um_untouched(b, a + 16 + n, n + 128));
                    }
                    CHECK(um_untouched(b, 0, a)); CHECK(um_guard_ok(b, n + 128));
                    /* 64-bit */
                    if (!f6) { um_buf_free(b); continue; }
                    memset(b, 0xCC, n + 128); um_init(&c, b, n + 128, ctx);
                    XEQ(um_x_mar(f6, UMF(&c), b + off, &hh, &e), 0);
                    EXPECT_EQ(RD_U32(b, a8), ctx == C_INPROC ? UM_TAG_P : UM_TAG_R);
                    CHECK(*(unsigned long long *)(b + a8 + 8) == (unsigned long long)(ULONG_PTR)h);
                    if (ctx == C_INPROC) CHECK(e == b + a8 + 16);
                    else {
                        CHECKF(e == b + a8 + 28 + n, "%s64 ctx %lu end %+ld", k_mf[k].pfx, ctx, (long)(e - b));
                        CHECK(*(unsigned long long *)(b + a8 + 16) == n);
                        EXPECT_EQ(RD_U32(b, a8 + 24), n);
                        CHECK(!memcmp(b + a8 + 28, bits, n));
                    }
                    CHECK(um_untouched(b, 0, a8)); CHECK(um_guard_ok(b, n + 128));
                    um_buf_free(b);
                }
            free(bits);
            obj_del(&k_mf[k], h);
        }
    }
}

TEST(usr_metafile_roundtrip_and_free)
{
    int k, w; ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    for (k = 0; k < 2; k++)
        for (w = 0; w < 2; w++) {
            void *fm = mfn(&k_mf[k], "Marshal", w ? "64" : ""), *fu = mfn(&k_mf[k], "Unmarshal", w ? "64" : "");
            void *ff = mfn(&k_mf[k], "Free", w ? "64" : "");
            NEED(fm); NEED(fu); NEED(ff);
            for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2) {
                void *h = obj_new(&k_mf[k], 7), *o = NULL; UINT n, n2; unsigned char *b, *e1, *e2, *b1, *b2; UMC c;
                if (!h) continue;
                n = obj_bits(&k_mf[k], h);
                b = um_buf_new(n + 128);
                um_init(&c, b, n + 128, ctx); XEQ(um_x_mar(fm, UMF(&c), b + 1, &h, &e1), 0);
                um_init(&c, b, (ULONG)(e1 - b), ctx); XEQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e2), 0);
                CHECK(e2 == e1);
                CHECKF(o != NULL, "%s unmarshal creates a metafile", k_mf[k].pfx);
                if (o) {
                    n2 = obj_bits(&k_mf[k], o);
                    CHECKF(n2 == n, "%s bits %u vs %u", k_mf[k].pfx, n2, n);
                    b1 = malloc(n); b2 = malloc(n2 ? n2 : 1);
                    obj_getbits(&k_mf[k], h, n, b1); obj_getbits(&k_mf[k], o, n2, b2);
                    CHECK(n2 == n && !memcmp(b1, b2, n));
                    free(b1); free(b2);
                    /* Free on a non-INPROC context destroys the metafile */
                    um_init(&c, NULL, 0, ctx); XEQ(um_x_free(ff, UMF(&c), &o), 0);
                    SetLastError(0);
                    CHECK(obj_bits(&k_mf[k], o) == 0);
                }
                /* INPROC: Free leaves the object alone */
                um_init(&c, NULL, 0, C_INPROC); XEQ(um_x_free(ff, UMF(&c), &h), 0);
                CHECK(obj_bits(&k_mf[k], h) == n);
                /* NULL handle / NULL pointer */
                o = NULL; um_init(&c, NULL, 0, C_LOCAL);
                XEQ(um_x_free(ff, UMF(&c), &o), 0); XEQ(um_x_free(ff, UMF(&c), NULL), 0);
                obj_del(&k_mf[k], h);
                um_buf_free(b);
            }
        }
}

TEST(usr_metafile_unmarshal_null_and_inproc)
{
    int k;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    for (k = 0; k < 2; k++) {
        void *fm = mfn(&k_mf[k], "Marshal", ""), *fu = mfn(&k_mf[k], "Unmarshal", "");
        void *h = obj_new(&k_mf[k], 3), *o = (void *)(ULONG_PTR)0x1; UMC c; unsigned char *b = um_buf_new(64), *e;
        NEED(fm); NEED(fu);
        if (!h) continue;
        RD_U32(b, 0) = UM_TAG_R; RD_U32(b, 4) = 0;
        um_init(&c, b, 8, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(o == NULL, "%s: remote form with a zero handle unmarshals to NULL", k_mf[k].pfx);
        CHECK(e == b + 8);
        um_init(&c, b, 64, C_INPROC); XEQ(um_x_mar(fm, UMF(&c), b, &h, &e), 0);
        um_init(&c, b, (ULONG)(e - b), C_INPROC); o = NULL;
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(o == h, "%s: inproc form carries the handle value itself", k_mf[k].pfx);
        obj_del(&k_mf[k], h);
        um_buf_free(b);
    }
}

/* ---- HMETAFILEPICT ------------------------------------------------------------------------------------------ */
TEST(usr_metafilepict_wire)
{
    void *fs = UM_FNIN("ole32.dll", "HMETAFILEPICT_UserSize"), *fm = UM_FNIN("ole32.dll", "HMETAFILEPICT_UserMarshal");
    void *fu = UM_FNIN("ole32.dll", "HMETAFILEPICT_UserUnmarshal"), *ff = UM_FNIN("ole32.dll", "HMETAFILEPICT_UserFree");
    HMETAFILE mf; HGLOBAL g; METAFILEPICT *p; UINT n; ULONG ctx, off, a, r;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ff);
    mf = mk_mf(5);
    g = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
    REQUIRE(mf); REQUIRE(g);
    p = GlobalLock(g); p->mm = MM_ANISOTROPIC; p->xExt = 1234; p->yExt = 5678; p->hMF = mf; GlobalUnlock(g);
    n = p_GetMetaFileBitsEx(mf, 0, NULL);

    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 6; off += 5) {
            UMC c; HGLOBAL hh = g, nh = NULL; unsigned char *b = um_buf_new(n + 256), *e; ULONG sz;
            a = UM_ALIGN(off, 4);
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(fs, UMF(&c), off, &nh, &sz), 0);
            if (ctx != C_INPROC) CHECKF(sz == a + 8, "NULL hglobal size ctx %lu got %lu", ctx, sz);
            XEQ(um_x_size(fs, UMF(&c), off, &hh, &sz), 0);
            um_init(&c, b, n + 256, ctx);
            XEQ(um_x_mar(fm, UMF(&c), b + off, &hh, &e), 0);
            CHECKF(sz >= (ULONG)(e - b), "Size() must not under-estimate: %lu < %ld", sz, (long)(e - b));
            if (ctx == C_INPROC) {
                ULONG ptr = UM_ALIGN(a + 4, 8);
                EXPECT_EQ(RD_U32(b, a), UM_TAG_P);
                CHECK(*(HGLOBAL *)(b + ptr) == g);
                CHECK(e == b + ptr + 8);
            } else {
                CHECKF(sz == a + 8 + 32 + 12 + n, "size ctx %lu: got %lu", ctx, sz);
                EXPECT_EQ(RD_U32(b, a), UM_TAG_R);
                EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)g);
                EXPECT_EQ(RD_U32(b, a + 8), MM_ANISOTROPIC);
                EXPECT_EQ(RD_U32(b, a + 12), 1234);
                EXPECT_EQ(RD_U32(b, a + 16), 5678);
                EXPECT_EQ(RD_U32(b, a + 20), 0x72657355);            /* 'User' */
                EXPECT_EQ(RD_U32(b, a + 24), UM_TAG_R);
                EXPECT_EQ(RD_U32(b, a + 28), (ULONG)(ULONG_PTR)mf);
                EXPECT_EQ(RD_U32(b, a + 32), n);
                EXPECT_EQ(RD_U32(b, a + 36), n);
                CHECKF(e == b + a + 40 + n, "end %+ld", (long)(e - b));
                /* round trip */
                {   HGLOBAL o = NULL; unsigned char *e2; UMC c2;
                    um_init(&c2, b, (ULONG)(e - b), ctx);
                    XEQ(um_x_mar(fu, UMF(&c2), b + off, &o, &e2), 0);
                    CHECK(e2 == e);
                    CHECK(o != NULL);
                    if (o) {
                        METAFILEPICT *q = GlobalLock(o);
                        CHECK(q && q->mm == MM_ANISOTROPIC && q->xExt == 1234 && q->yExt == 5678);
                        CHECK(q && q->hMF != NULL && p_GetMetaFileBitsEx(q->hMF, 0, NULL) == n);
                        GlobalUnlock(o);
                        um_init(&c2, NULL, 0, ctx);
                        XEQ(um_x_free(ff, UMF(&c2), &o), 0);        /* deletes the inner metafile and the global */
                    } }
            }
            CHECK(um_untouched(b, 0, a)); CHECK(um_guard_ok(b, n + 256));
            um_buf_free(b);
        }
    /* NULL hglobal in a remote context */
    {
        UMC c; HGLOBAL nh = NULL; unsigned char *b = um_buf_new(32), *e;
        um_init(&c, b, 32, C_LOCAL);
        XEQ(um_x_mar(fm, UMF(&c), b, &nh, &e), 0);
        CHECK(e == b + 8); EXPECT_EQ(RD_U32(b, 0), UM_TAG_R); EXPECT_EQ(RD_U32(b, 4), 0);
        um_buf_free(b);
    }
    p_DeleteMetaFile(mf);
    GlobalFree(g);
}

/* ---- HBITMAP ------------------------------------------------------------------------------------------------- */
typedef struct { int w, h, planes, bpp; } bm_spec;
static const bm_spec k_bm[] = { { 16, 16, 1, 1 }, { 8, 8, 1, 1 }, { 10, 3, 1, 32 }, { 17, 5, 1, 8 }, { 32, 2, 1, 16 }, { 1, 1, 1, 1 } };

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_HBITMAP */
TEST(usr_hbitmap_size_and_wire)
{
    void *fs = UM_FNIN("ole32.dll", "HBITMAP_UserSize"), *fm = UM_FNIN("ole32.dll", "HBITMAP_UserMarshal");
    int i; ULONG ctx, off;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fs); REQUIRE(fm);
    for (i = 0; i < 6; i++) {
        unsigned char src[2048]; BITMAP bm; HBITMAP hb; ULONG cb; unsigned char *bits;
        int j;
        for (j = 0; j < (int)sizeof src; j++) src[j] = (unsigned char)(j * 13 + 5);
        hb = p_CreateBitmap(k_bm[i].w, k_bm[i].h, k_bm[i].planes, k_bm[i].bpp, src);
        if (!hb) { t_note("CreateBitmap %d failed", i); continue; }
        memset(&bm, 0, sizeof bm);
        CHECK(p_GetObjectW(hb, sizeof bm, &bm) == sizeof bm);
        cb = bm.bmPlanes * bm.bmWidthBytes * bm.bmHeight;
        bits = malloc(cb);
        CHECK((ULONG)p_GetBitmapBits(hb, cb, bits) == cb);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 6; off += 5) {
                UMC c; HBITMAP hh = hb, nh = NULL; ULONG a = UM_ALIGN(off, 4), sz; unsigned char *b = um_buf_new(cb + 128), *e;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &nh, &sz), 0); CHECK(sz == a + 8);
                XEQ(um_x_size(fs, UMF(&c), off, &hh, &sz), 0);
                CHECKF(sz == a + 8 + (ctx == C_INPROC ? 0 : 28 + cb), "bm %d ctx %lu off %lu size %lu cb %lu", i, ctx, off, sz, cb);
                um_init(&c, b, cb + 128, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &hh, &e), 0);
                EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)hb);
                if (ctx == C_INPROC) { CHECK(e == b + a + 8); EXPECT_EQ(RD_U32(b, a), UM_TAG_H); }
                else {
                    CHECKF(e == b + a + 32 + cb, "bm %d ctx %lu end %+ld cb %lu", i, ctx, (long)(e - b), cb);
                    CHECK(sz == (ULONG)(e - b) + 4);            /* Win10 over-estimates by 4 */
                    EXPECT_EQ(RD_U32(b, a), UM_TAG_R);
                    EXPECT_EQ(RD_U32(b, a + 8), cb);
                    EXPECT_EQ(RD_U32(b, a + 12), (ULONG)bm.bmType);
                    EXPECT_EQ(RD_U32(b, a + 16), (ULONG)bm.bmWidth);
                    EXPECT_EQ(RD_U32(b, a + 20), (ULONG)bm.bmHeight);
                    EXPECT_EQ(RD_U32(b, a + 24), (ULONG)bm.bmWidthBytes);
                    EXPECT_EQ(RD_U32(b, a + 28), (ULONG)bm.bmPlanes | ((ULONG)bm.bmBitsPixel << 16));
                    CHECK(!memcmp(b + a + 32, bits, cb));
                    CHECK(um_untouched(b, a + 32 + cb, cb + 128));
                }
                CHECK(um_untouched(b, 0, a)); CHECK(um_guard_ok(b, cb + 128));
                um_buf_free(b);
            }
        free(bits);
        p_DeleteObject(hb);
    }
}

TEST(usr_hbitmap_roundtrip_and_free)
{
    int i, w;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    for (w = 0; w < 2; w++) {
        void *fm = UM_FNIN("ole32.dll", w ? "HBITMAP_UserMarshal64" : "HBITMAP_UserMarshal");
        void *fu = UM_FNIN("ole32.dll", w ? "HBITMAP_UserUnmarshal64" : "HBITMAP_UserUnmarshal");
        void *ff = UM_FNIN("ole32.dll", w ? "HBITMAP_UserFree64" : "HBITMAP_UserFree");
        void *fs = UM_FNIN("ole32.dll", w ? "HBITMAP_UserSize64" : "HBITMAP_UserSize");
        NEED(fm); NEED(fu); NEED(ff); NEED(fs);
        for (i = 0; i < 6; i++) {
            unsigned char src[2048]; BITMAP a, b2; HBITMAP hb, o = NULL; UMC c; unsigned char *buf, *e1, *e2, *x, *y; int j; ULONG sz, cb;
            for (j = 0; j < (int)sizeof src; j++) src[j] = (unsigned char)(j * 11 + 1);
            hb = p_CreateBitmap(k_bm[i].w, k_bm[i].h, k_bm[i].planes, k_bm[i].bpp, src);
            if (!hb) continue;
            p_GetObjectW(hb, sizeof a, &a);
            cb = a.bmPlanes * a.bmWidthBytes * a.bmHeight;
            um_init(&c, NULL, 0, C_LOCAL); um_x_size(fs, UMF(&c), 3, &hb, &sz);
            buf = um_buf_new(sz + 64);
            um_init(&c, buf, sz + 64, C_LOCAL); XEQ(um_x_mar(fm, UMF(&c), buf + 3, &hb, &e1), 0);
            CHECK((ULONG)(e1 - buf) <= sz);
            um_init(&c, buf, (ULONG)(e1 - buf), C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), buf + 3, &o, &e2), 0);
            CHECK(e2 == e1);
            CHECK(o != NULL);
            if (o) {
                CHECK(o != hb);
                memset(&b2, 0, sizeof b2);
                CHECK(p_GetObjectW(o, sizeof b2, &b2) == sizeof b2);
                CHECKF(b2.bmWidth == a.bmWidth && b2.bmHeight == a.bmHeight && b2.bmPlanes == a.bmPlanes && b2.bmBitsPixel == a.bmBitsPixel && b2.bmWidthBytes == a.bmWidthBytes, "bm %d attrs", i);
                x = malloc(cb); y = malloc(cb);
                p_GetBitmapBits(hb, cb, x); p_GetBitmapBits(o, cb, y);
                CHECKF(!memcmp(x, y, cb), "bm %d bits", i);
                free(x); free(y);
                {   HBITMAP o0 = o; BITMAP bb;
                    um_init(&c, NULL, 0, C_LOCAL); XEQ(um_x_free(ff, UMF(&c), &o), 0);
                    /* GDI batches deletions: GetObjectType may still report the type right after the call, GetObjectW already fails */
                    CHECKF(p_GetObjectW(o0, sizeof bb, &bb) == 0, "UserFree deletes the bitmap in a non-INPROC context");
                }
            }
            /* INPROC keeps the handle alive */
            um_init(&c, NULL, 0, C_INPROC); XEQ(um_x_free(ff, UMF(&c), &hb), 0);
            CHECK(p_GetObjectType(hb) == OBJ_BITMAP);
            p_DeleteObject(hb);
            um_buf_free(buf);
        }
    }
}

TEST(usr_hbitmap_null_and_inproc)
{
    void *fm = UM_FNIN("ole32.dll", "HBITMAP_UserMarshal"), *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    void *fm6 = UM_FNIN("ole32.dll", "HBITMAP_UserMarshal64"), *fs6 = UM_FNIN("ole32.dll", "HBITMAP_UserSize64");
    HBITMAP hb, nh = NULL, o = NULL; UMC c; unsigned char *b = um_buf_new(64), *e; ULONG ctx, sz;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fm); REQUIRE(fu); REQUIRE(fm6); REQUIRE(fs6);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        um_init(&c, b, 64, ctx); memset(b, 0xCC, 64);
        XEQ(um_x_mar(fm, UMF(&c), b, &nh, &e), 0);
        CHECK(e == b + 8); EXPECT_EQ(RD_U32(b, 0), ctx == C_INPROC ? UM_TAG_H : UM_TAG_R); EXPECT_EQ(RD_U32(b, 4), 0);
        memset(b, 0xCC, 64);
        XEQ(um_x_mar(fm6, UMF(&c), b + 1, &nh, &e), 0);
        /* Win10: the 64-bit form aligns the tag to 8 (+8) and the handle follows at +16 */
        CHECK(e == b + 24); EXPECT_EQ(RD_U32(b, 8), ctx == C_INPROC ? UM_TAG_P : UM_TAG_R);
        XEQ(um_x_size(fs6, UMF(&c), 1, &nh, &sz), 0); CHECK(sz == 8 + 16);
    }
    /* inproc: the handle value travels */
    hb = p_CreateBitmap(4, 4, 1, 1, NULL);
    um_init(&c, b, 64, C_INPROC);
    XEQ(um_x_mar(fm, UMF(&c), b + 1, &hb, &e), 0);
    CHECK(e == b + 12); EXPECT_EQ(RD_U32(b, 4), UM_TAG_H); EXPECT_EQ(RD_U32(b, 8), (ULONG)(ULONG_PTR)hb);
    um_init(&c, b, 12, C_INPROC);
    XEQ(um_x_mar(fu, UMF(&c), b + 1, &o, &e), 0);
    CHECK(o != NULL);
    if (o != hb) t_note("inproc HBITMAP unmarshal returned a different handle (%p vs %p)", o, hb);
    p_DeleteObject(hb);
    um_buf_free(b);
}

/* ---- HPALETTE -------------------------------------------------------------------------------------------------- */
static HPALETTE mk_pal(int n, PALETTEENTRY *ents)
{
    LOGPALETTE *lp = malloc(sizeof(LOGPALETTE) + n * sizeof(PALETTEENTRY));
    HPALETTE h; int i;
    lp->palVersion = 0x300; lp->palNumEntries = (WORD)n;
    for (i = 0; i < n; i++) { lp->palPalEntry[i].peRed = (BYTE)(i * 3); lp->palPalEntry[i].peGreen = (BYTE)(255 - i); lp->palPalEntry[i].peBlue = (BYTE)(i * 7 + 1); lp->palPalEntry[i].peFlags = 0; ents[i] = lp->palPalEntry[i]; }
    h = p_CreatePalette(lp);
    free(lp);
    return h;
}

TEST(usr_hpalette_size_and_wire)
{
    static const int ns[] = { 1, 2, 16, 100, 256 };
    void *fs = UM_FNIN("ole32.dll", "HPALETTE_UserSize"), *fm = UM_FNIN("ole32.dll", "HPALETTE_UserMarshal");
    void *fs6 = UM_FNIN("ole32.dll", "HPALETTE_UserSize64"), *fm6 = UM_FNIN("ole32.dll", "HPALETTE_UserMarshal64");
    void *fse = UM_FNIN("ole32.dll", "HPALETTE_UserSizeExt"), *fme = UM_FNIN("ole32.dll", "HPALETTE_UserMarshalExt");
    int i; ULONG ctx, off;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fs6); REQUIRE(fm6); REQUIRE(fse); REQUIRE(fme);
    for (i = 0; i < 5; i++) {
        PALETTEENTRY ents[256]; HPALETTE hp = mk_pal(ns[i], ents); UINT n;
        if (!hp) { t_note("CreatePalette failed"); continue; }
        n = p_GetPaletteEntries(hp, 0, 0, NULL);
        CHECK(n == (UINT)ns[i]);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 6; off += 5) {
                UMC c; HPALETTE hh = hp, nh = NULL; ULONG a = UM_ALIGN(off, 4), a8 = UM_ALIGN(off, 8), sz, sz6, sze; unsigned char *b = um_buf_new(n * 4 + 128), *e, *e6, *b6 = um_buf_new(n * 4 + 128);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &nh, &sz), 0); CHECK(sz == a + 8);
                XEQ(um_x_size(fs, UMF(&c), off, &hh, &sz), 0);
                XEQ(um_x_size(fs6, UMF(&c), off, &hh, &sz6), 0);
                XEQ(um_x_size(fse, UMF(&c), off, &hh, &sze), 0);
                CHECK(sze == sz);
                um_init(&c, b, n * 4 + 128, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &hh, &e), 0);
                EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)hp);
                if (ctx == C_INPROC) {
                    CHECK(e == b + a + 8); EXPECT_EQ(RD_U32(b, a), UM_TAG_H); CHECK(sz == a + 8);
                } else {
                    CHECKF(e == b + a + 16 + 4 * n, "n=%u ctx %lu end %+ld", n, ctx, (long)(e - b));
                    CHECKF(sz == a + 16 + 4 * n, "n=%u ctx %lu size %lu", n, ctx, sz);
                    EXPECT_EQ(RD_U32(b, a), UM_TAG_R);
                    EXPECT_EQ(RD_U32(b, a + 8), n);
                    EXPECT_EQ(*(WORD *)(b + a + 12), 0x300);
                    EXPECT_EQ(*(WORD *)(b + a + 14), n);
                    CHECK(!memcmp(b + a + 16, ents, 4 * n));
                    CHECK(um_untouched(b, a + 16 + 4 * n, n * 4 + 128));
                    /* Ext alias */
                    memset(b, 0xCC, n * 4 + 128);
                    um_init(&c, b, n * 4 + 128, ctx);
                    XEQ(um_x_mar(fme, UMF(&c), b + off, &hh, &e6), 0);
                    CHECK(e6 == e && RD_U32(b, a + 8) == n);
                }
                CHECK(um_untouched(b, 0, a));
                /* 64-bit */
                um_init(&c, b6, n * 4 + 128, ctx);
                XEQ(um_x_mar(fm6, UMF(&c), b6 + off, &hh, &e6), 0);
                CHECK(*(unsigned long long *)(b6 + a8 + 8) == (unsigned long long)(ULONG_PTR)hp);
                if (ctx == C_INPROC) { CHECK(e6 == b6 + a8 + 16); EXPECT_EQ(RD_U32(b6, a8), UM_TAG_P); CHECK(sz6 == a8 + 16); }
                else {
                    CHECKF(e6 == b6 + a8 + 28 + 4 * n, "64 n=%u ctx %lu end %+ld", n, ctx, (long)(e6 - b6));
                    CHECKF(sz6 == a8 + 28 + 4 * n, "64 n=%u ctx %lu size %lu", n, ctx, sz6);
                    CHECK(*(unsigned long long *)(b6 + a8 + 16) == n);
                    EXPECT_EQ(*(WORD *)(b6 + a8 + 24), 0x300); EXPECT_EQ(*(WORD *)(b6 + a8 + 26), n);
                    CHECK(!memcmp(b6 + a8 + 28, ents, 4 * n));
                }
                um_buf_free(b); um_buf_free(b6);
            }
        p_DeleteObject(hp);
    }
}

TEST(usr_hpalette_roundtrip_and_free)
{
    int w;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    for (w = 0; w < 2; w++) {
        void *fm = UM_FNIN("ole32.dll", w ? "HPALETTE_UserMarshal64" : "HPALETTE_UserMarshal");
        void *fu = UM_FNIN("ole32.dll", w ? "HPALETTE_UserUnmarshal64" : "HPALETTE_UserUnmarshal");
        void *ff = UM_FNIN("ole32.dll", w ? "HPALETTE_UserFree64" : "HPALETTE_UserFree");
        void *fue = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshalExt"), *ffe = UM_FNIN("ole32.dll", "HPALETTE_UserFreeExt");
        PALETTEENTRY ents[40], got[40]; HPALETTE hp, o = NULL; UMC c; unsigned char *b, *e1, *e2;
        NEED(fm); NEED(fu); NEED(ff);
        hp = mk_pal(40, ents);
        if (!hp) continue;
        b = um_buf_new(400);
        um_init(&c, b, 400, C_DIFFM); XEQ(um_x_mar(fm, UMF(&c), b + 2, &hp, &e1), 0);
        um_init(&c, b, (ULONG)(e1 - b), C_DIFFM); XEQ(um_x_mar(fu, UMF(&c), b + 2, &o, &e2), 0);
        CHECK(e1 == e2);
        CHECK(o != NULL);
        if (o) {
            CHECK(p_GetPaletteEntries(o, 0, 0, NULL) == 40);
            CHECK(p_GetPaletteEntries(o, 0, 40, got) == 40 && !memcmp(got, ents, sizeof ents));
            um_init(&c, NULL, 0, C_DIFFM); XEQ(um_x_free(ff, UMF(&c), &o), 0);
            /* GDI batches deletions: GetObjectType may lag, GetObjectW fails at once */
            { WORD pw = 0; CHECKF(p_GetObjectW(o, sizeof pw, &pw) == 0, "palette deleted by UserFree"); }
        }
        um_init(&c, NULL, 0, C_INPROC); XEQ(um_x_free(ff, UMF(&c), &hp), 0);
        CHECK(p_GetObjectType(hp) == OBJ_PAL);
        if (!w && fue && ffe) {
            o = NULL;
            um_init(&c, b, (ULONG)(e1 - b), C_DIFFM); XEQ(um_x_mar(fue, UMF(&c), b + 2, &o, &e2), 0);
            CHECK(o != NULL);
            um_init(&c, NULL, 0, C_DIFFM); XEQ(um_x_free(ffe, UMF(&c), &o), 0);
        }
        p_DeleteObject(hp);
        um_buf_free(b);
    }
}
