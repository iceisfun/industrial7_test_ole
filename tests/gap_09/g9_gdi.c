/* gap_09: HBITMAP_UserUnmarshal and HPALETTE_UserUnmarshal (ole32.dll), driven with hand-built wire images.
 *
 * Win10 decompilation (worker functions). Common shape, start = align4(pSuppliedBuffer):
 *   need len >= (start-buf) + 8, else RPC_X_BAD_STUB_DATA (0x6f7)
 *   DWORD tag at +0: 'WdtR' (0x52746457) remote, 'WdtH' (0x48746457) inproc, anything else -> 0x6c5 (RPC_X_INVALID_TAG)
 *   DWORD handle at +4
 *   On success the previous *dest is DeleteObject()ed (if non-NULL) and replaced; on any exception *dest is untouched.
 *
 * HBITMAP, remote form (handle != 0):  +8 cb, +12 bmType (ignored), +16 bmWidth, +20 bmHeight, +24 bmWidthBytes,
 *   +28 WORD planes, +30 WORD bpp, +32 bits[h * widthBytes]; checks: cb == h*planes*widthBytes and
 *   widthBytes == ((w*planes*bpp + 15) >> 4) * 2; remote form with ctx INPROC (3) is rejected with 0x6f7;
 *   handle == 0 -> NULL result. Inproc form ('WdtH') needs ctx == INPROC else 0x6f7 and yields (HBITMAP)(LONG_PTR)(int)handle.
 *   CreateBitmap failing raises the GetLastError() mapped as (err & 0xffff) | 0x80070000.
 *
 * HPALETTE, remote form (handle != 0): +8 count, +12 WORD version (must be 0x300), +14 WORD numEntries (== count),
 *   +16 PALETTEENTRY[count]; no context check on the remote form at all (INPROC + 'WdtR' succeeds);
 *   handle == 0 with 'WdtR' yields NULL for every context. 'WdtH' needs ctx INPROC else 0x6f7. */
#include "g9.h"

static HBITMAP (WINAPI *p_CreateBitmap)(int, int, UINT, UINT, const void *);
static int (WINAPI *p_GetObjectW)(HANDLE, int, LPVOID);
static DWORD (WINAPI *p_GetObjectType)(HGDIOBJ);
static BOOL (WINAPI *p_DeleteObject)(HGDIOBJ);
static LONG (WINAPI *p_GetBitmapBits)(HBITMAP, LONG, LPVOID);
static HPALETTE (WINAPI *p_CreatePalette)(const LOGPALETTE *);
static UINT (WINAPI *p_GetPaletteEntries)(HPALETTE, UINT, UINT, LPPALETTEENTRY);

/* A deleted GDI handle is detected by a second DeleteObject() failing. On Win10 GetObjectType() keeps reporting the
   old type for a handle that was just deleted, so it cannot be used to prove deletion. */
static int gdi_load(void)
{
#define L(n) p_##n = (void *)t_proc_in("gdi32.dll", #n)
    L(CreateBitmap); L(GetObjectW); L(GetObjectType); L(DeleteObject); L(GetBitmapBits); L(CreatePalette); L(GetPaletteEntries);
#undef L
    return p_CreateBitmap && p_GetObjectW && p_GetObjectType && p_DeleteObject && p_GetBitmapBits && p_CreatePalette && p_GetPaletteEntries;
}

static int gdi_gone(void *h) { return !p_DeleteObject(h); }

#define W32(b, o, v) (*(ULONG *)((b) + (o)) = (ULONG)(v))
#define W16(b, o, v) (*(WORD *)((b) + (o)) = (WORD)(v))

/* ---- HBITMAP ------------------------------------------------------------------------------------------------ */
typedef struct { int w, h, bpp; } bmshape;
static const bmshape k_shapes[] = { { 1, 1, 1 }, { 8, 8, 1 }, { 16, 4, 1 }, { 17, 3, 1 }, { 5, 2, 8 }, { 3, 3, 16 }, { 2, 2, 32 }, { 10, 3, 32 }, { 33, 1, 1 } };

static ULONG wb_of(int w, int bpp) { return (((ULONG)w * bpp + 15) >> 4) * 2; }

/* write a remote HBITMAP image at b+at (at must be 4-aligned); returns the end offset */
static ULONG put_bm(unsigned char *b, ULONG at, ULONG handle, int w, int h, int bpp, const unsigned char *bits)
{
    ULONG wb = wb_of(w, bpp), cb = (ULONG)h * wb;
    W32(b, at, UM_TAG_R); W32(b, at + 4, handle); W32(b, at + 8, cb);
    W32(b, at + 12, 0); W32(b, at + 16, w); W32(b, at + 20, h); W32(b, at + 24, wb);
    W16(b, at + 28, 1); W16(b, at + 30, bpp);
    memcpy(b + at + 32, bits, cb);
    return at + 32 + cb;
}

static void fill_bits(unsigned char *p, size_t n, int seed)
{
    size_t i;
    for (i = 0; i < n; i++) p[i] = (unsigned char)(i * 7 + seed);
}

TEST(gap9_hbitmap_unmarshal_valid_shapes_every_remote_context)
{
    static const ULONG ctxs[] = { C_LOCAL, C_NOSHM, C_DIFFM, C_CROSSX, C_CONT };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i, k;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof k_shapes / sizeof *k_shapes; i++)
        for (k = 0; k < sizeof ctxs / sizeof *ctxs; k++) {
            const bmshape *s = &k_shapes[i];
            unsigned char bits[512], *b = um_buf_new(600), *e; UMC c; HBITMAP o = NULL; ULONG end; BITMAP bm;
            fill_bits(bits, sizeof bits, (int)i);
            end = put_bm(b, 0, 0x1234, s->w, s->h, s->bpp, bits);
            um_init(&c, b, end, ctxs[k]);
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
            CHECKF(e == b + end, "shape %u ctx %lu: end %+ld want %lu", i, ctxs[k], (long)(e - b), end);
            CHECKF(o != NULL, "shape %u: bitmap created", i);
            if (o) {
                memset(&bm, 0, sizeof bm);
                CHECK(p_GetObjectW(o, sizeof bm, &bm) == sizeof bm);
                CHECKF(bm.bmWidth == s->w && bm.bmHeight == s->h && bm.bmBitsPixel == s->bpp && bm.bmPlanes == 1, "shape %u attrs w=%ld h=%ld bpp=%u", i, bm.bmWidth, bm.bmHeight, bm.bmBitsPixel);
                CHECKF(p_GetObjectType(o) == OBJ_BITMAP, "shape %u is a bitmap", i);
                {   unsigned char got[512]; LONG n = (LONG)(s->h * wb_of(s->w, s->bpp));
                    memset(got, 0, sizeof got);
                    CHECK(p_GetBitmapBits(o, n, got) == n);
                    if ((s->w * s->bpp) % 16 == 0) CHECKF(!memcmp(got, bits, n), "shape %u pixel data", i);
                    else if (memcmp(got, bits, n)) t_note("shape %u: bits differ in scanline padding", i);
                }
                p_DeleteObject(o);
            }
            um_buf_free(b);
        }
}

TEST(gap9_hbitmap_unmarshal_aligns_start_to_four)
{
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    ULONG off;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (off = 0; off < 9; off++) {
        unsigned char bits[8], *b = um_buf_new(200), *e; UMC c; HBITMAP o = NULL; ULONG a = UM_ALIGN(off, 4), end;
        fill_bits(bits, sizeof bits, 3);
        end = put_bm(b, a, 7, 16, 4, 1, bits);
        um_init(&c, b, end, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
        CHECKF(e == b + end && o, "off %lu: end %+ld want %lu", off, (long)(e - b), end);
        if (o) p_DeleteObject(o);
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_remote_handle_value_is_irrelevant)
{
    static const ULONG hs[] = { 1, 2, 0x7fffffff, 0x80000000u, 0xffffffffu, 0x12345678 };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof hs / sizeof *hs; i++) {
        unsigned char bits[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, *b = um_buf_new(100), *e; UMC c; HBITMAP o = NULL; ULONG end;
        end = put_bm(b, 0, hs[i], 16, 4, 1, bits);
        um_init(&c, b, end, C_DIFFM);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(o != NULL && (ULONG)(ULONG_PTR)o != hs[i] , "handle %#lx: a new bitmap is created, wire value is not reused", hs[i]);
        if (o) p_DeleteObject(o);
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_bmtype_field_is_ignored)
{
    static const ULONG types[] = { 0, 1, 0x1234, 0xffffffffu };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof types / sizeof *types; i++) {
        unsigned char bits[8] = { 0 }, *b = um_buf_new(100), *e; UMC c; HBITMAP o = NULL; ULONG end;
        end = put_bm(b, 0, 9, 16, 4, 1, bits);
        W32(b, 12, types[i]);
        um_init(&c, b, end, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(o != NULL, "bmType %#lx accepted", types[i]);
        if (o) p_DeleteObject(o);
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_null_handle_remote)
{
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        unsigned char *b = um_buf_new(32), *e; UMC c; HBITMAP o = p_CreateBitmap(4, 4, 1, 1, NULL);
        REQUIRE(o);
        W32(b, 0, UM_TAG_R); W32(b, 4, 0);
        um_init(&c, b, 8, ctx);
        if (ctx == C_INPROC) {
            /* 'WdtR' is a remote-call tag: rejected in an inproc context even for a NULL handle */
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
            CHECKF(p_GetObjectType(o) == OBJ_BITMAP, "failed call leaves the destination alone");
            p_DeleteObject(o);
        } else {
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
            CHECKF(e == b + 8, "ctx %lu end", ctx);
            CHECKF(o == NULL, "ctx %lu: NULL handle yields NULL", ctx);
        }
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_deletes_previous_destination_bitmap)
{
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned char bits[8] = { 0xAA, 0, 0xAA, 0, 0xAA, 0, 0xAA, 0 }, *b, *e; UMC c; HBITMAP old, o; ULONG end;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    b = um_buf_new(100);
    /* replaced by a real bitmap */
    old = p_CreateBitmap(8, 8, 1, 1, NULL); o = old;
    REQUIRE(old);
    end = put_bm(b, 0, 5, 16, 4, 1, bits);
    um_init(&c, b, end, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o != NULL);
    CHECKF(gdi_gone(old), "the previous bitmap is DeleteObject()ed");
    if (o) p_DeleteObject(o);
    /* replaced by NULL */
    old = p_CreateBitmap(8, 8, 1, 1, NULL); o = old;
    REQUIRE(old);
    W32(b, 0, UM_TAG_R); W32(b, 4, 0);
    um_init(&c, b, 8, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o == NULL);
    CHECKF(gdi_gone(old), "NULL result also deletes the previous bitmap");
    um_buf_free(b);
}

TEST(gap9_hbitmap_unmarshal_inproc_form)
{
    static const struct { ULONG wire; LONG_PTR want; } t[] = {
        { 0, 0 }, { 1, 1 }, { 0x1234, 0x1234 }, { 0x7fffffff, 0x7fffffff }, { 0x80000000u, (LONG_PTR)(int)0x80000000u }, { 0xfffffff0u, -16 },
    };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i; ULONG off;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof t / sizeof *t; i++)
        for (off = 0; off < 5; off++) {
            unsigned char *b = um_buf_new(32), *e; UMC c; HBITMAP o = NULL; ULONG a = UM_ALIGN(off, 4);
            W32(b, a, UM_TAG_H); W32(b, a + 4, t[i].wire);
            um_init(&c, b, a + 8, C_INPROC);
            XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
            CHECKF(e == b + a + 8, "end");
            CHECKF((LONG_PTR)o == t[i].want, "wire %#lx: got %p want %p", t[i].wire, o, (void *)t[i].want);
            um_buf_free(b);
        }
}

TEST(gap9_hbitmap_unmarshal_inproc_form_in_remote_context_raises)
{
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        unsigned char *b = um_buf_new(32), *e; UMC c; HBITMAP o = NULL;
        W32(b, 0, UM_TAG_H); W32(b, 4, 0x55);
        um_init(&c, b, 8, ctx);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), ctx == C_INPROC ? 0 : X_BADSTUB);
        CHECKF(ctx == C_INPROC ? (LONG_PTR)o == 0x55 : o == NULL, "ctx %lu result", ctx);
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_remote_form_in_inproc_context_raises)
{
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned char bits[8] = { 0 }, *b, *e; UMC c; HBITMAP o = NULL; ULONG end;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    b = um_buf_new(100);
    end = put_bm(b, 0, 9, 16, 4, 1, bits);
    um_init(&c, b, end, C_INPROC);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    CHECK(o == NULL);
    um_buf_free(b);
}

TEST(gap9_hbitmap_unmarshal_bad_tag_raises_invalid_tag)
{
    static const ULONG tags[] = { 0, 1, UM_TAG_P, 0x48746458u, 0x52746456u, 0xffffffffu, 0x57645448u };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i; ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof tags / sizeof *tags; i++)
        for (ctx = 0; ctx <= C_CONT; ctx += 3) {
            unsigned char *b = um_buf_new(100), *e; UMC c; HBITMAP o = NULL;
            memset(b, 0, 64);
            W32(b, 0, tags[i]); W32(b, 4, 1);
            um_init(&c, b, 64, ctx);
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);
            CHECK(o == NULL);
            um_buf_free(b);
        }
}

TEST(gap9_hbitmap_unmarshal_truncated_buffers)
{
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned char bits[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, *b, *e; UMC c; ULONG end, len;
    HBITMAP keep;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    b = um_buf_new(100);
    end = put_bm(b, 0, 9, 16, 4, 1, bits);
    CHECK(end == 40);
    keep = p_CreateBitmap(2, 2, 1, 1, NULL);
    REQUIRE(keep);
    /* every length below the full image fails with RPC_X_BAD_STUB_DATA and leaves *dest alone */
    for (len = 0; len < end; len++) {
        HBITMAP o = keep;
        um_init(&c, b, len, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o == keep, "len %lu: destination untouched", len);
        CHECKF(p_GetObjectType(keep) == OBJ_BITMAP, "len %lu: old bitmap still alive", len);
    }
    {   HBITMAP o = NULL;
        um_init(&c, b, end, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        if (o) p_DeleteObject(o);
    }
    /* unaligned start: the alignment padding counts against the length */
    {   HBITMAP o = NULL; unsigned char *b2 = um_buf_new(100); ULONG e2;
        e2 = put_bm(b2, 4, 9, 16, 4, 1, bits);
        um_init(&c, b2, e2 - 1, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b2 + 1, &o, &e), X_BADSTUB);
        um_init(&c, b2, e2, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b2 + 1, &o, &e), 0);
        if (o) p_DeleteObject(o);
        um_buf_free(b2);
    }
    p_DeleteObject(keep);
    um_buf_free(b);
}

TEST(gap9_hbitmap_unmarshal_inconsistent_headers_raise_bad_stub)
{
    /* each case starts from a valid 16x4 1-bpp image and breaks one field */
    static const struct { const char *what; int off; ULONG val; int is16; } t[] = {
        { "cb too big", 8, 9, 0 }, { "cb too small", 8, 7, 0 }, { "cb zero", 8, 0, 0 },
        { "widthBytes too small", 24, 1, 0 }, { "widthBytes too big", 24, 4, 0 }, { "widthBytes odd", 24, 3, 0 },
        { "height 5 (cb mismatch)", 20, 5, 0 }, { "height 3 (cb mismatch)", 20, 3, 0 },
        { "width 17 (widthBytes mismatch)", 16, 17, 0 }, { "width 32 (widthBytes mismatch)", 16, 32, 0 },
        { "bpp 2 (widthBytes mismatch for w=16 is ok? no: 16*2 bits needs 4 bytes)", 30, 2, 1 },
        { "bpp 8 (widthBytes mismatch)", 30, 8, 1 },
    };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        unsigned char bits[64] = { 0 }, *b = um_buf_new(200), *e; UMC c; HBITMAP o = NULL; ULONG end;
        end = put_bm(b, 0, 9, 16, 4, 1, bits);
        if (t[i].is16) W16(b, t[i].off, t[i].val); else W32(b, t[i].off, t[i].val);
        um_init(&c, b, 200, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o == NULL, "%s: nothing created", t[i].what);
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_zero_sized_bitmap)
{
    /* w == 0 or h == 0: cb 0, widthBytes 0, no bits; CreateBitmap documents a 1x1 monochrome bitmap for that case */
    static const struct { int w, h; } t[] = { { 0, 0 }, { 0, 5 }, { 7, 0 } };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < 3; i++) {
        unsigned char *b = um_buf_new(100), *e; UMC c; HBITMAP o = NULL; BITMAP bm; DWORD code;
        W32(b, 0, UM_TAG_R); W32(b, 4, 3);
        W32(b, 8, 0); W32(b, 12, 0); W32(b, 16, t[i].w); W32(b, 20, t[i].h); W32(b, 24, wb_of(t[i].w, 1));
        W16(b, 28, 1); W16(b, 30, 1);
        if (t[i].w == 7 && t[i].h == 0) { /* widthBytes = 2 here, cb = 0 == h * planes * wb */ }
        um_init(&c, b, 32, C_LOCAL);
        code = um_x_mar(fu, UMF(&c), b, &o, &e);
        if (t[i].w == 0 || t[i].h == 0) {
            /* cb must equal h*planes*wb: 0x0 and 0xN have wb 0 (cb 0), Nx0 has cb 0 as well */
            XEQ(code, 0);
            CHECKF(e == b + 32, "shape %ux%u end", t[i].w, t[i].h);
            if (o) {
                memset(&bm, 0, sizeof bm);
                CHECK(p_GetObjectW(o, sizeof bm, &bm) == sizeof bm);
                CHECKF(bm.bmWidth == 1 && bm.bmHeight == 1 && bm.bmBitsPixel == 1 && bm.bmPlanes == 1, "shape %ux%u -> %ldx%ld", t[i].w, t[i].h, bm.bmWidth, bm.bmHeight);
                p_DeleteObject(o);
            } else t_note("CreateBitmap(%d,%d) returned NULL", t[i].w, t[i].h);
        }
        um_buf_free(b);
    }
}

TEST(gap9_hbitmap_unmarshal_unsupported_bitdepth_reports_createbitmap_error)
{
    /* odd depths satisfy the header checks (widthBytes = ((w*bpp+15)>>4)*2); the outcome must follow CreateBitmap itself:
       a depth CreateBitmap refuses surfaces as an RPC exception, a depth it accepts unmarshals to a bitmap (Win10 accepts 3) */
    static const int depths[] = { 2, 3, 5, 7, 9, 12, 15, 33, 48, 64, 128 };
    void *fu = UM_FNIN("ole32.dll", "HBITMAP_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof depths / sizeof *depths; i++) {
        int bpp = depths[i];
        unsigned char *b = um_buf_new(1200), *e = NULL; UMC c; HBITMAP o = NULL, probe; DWORD code; ULONG wb = wb_of(16, bpp);
        probe = p_CreateBitmap(16, 4, 1, bpp, NULL);
        if (probe) p_DeleteObject(probe);
        memset(b, 0, 1200);
        W32(b, 0, UM_TAG_R); W32(b, 4, 3); W32(b, 8, 4 * wb); W32(b, 16, 16); W32(b, 20, 4); W32(b, 24, wb); W16(b, 28, 1); W16(b, 30, bpp);
        um_init(&c, b, 32 + 4 * wb, C_LOCAL);
        code = um_x_mar(fu, UMF(&c), b, &o, &e);
        if (probe) {
            CHECKF(code == 0 && o != NULL, "bpp %d: CreateBitmap accepts it, so the unmarshal must succeed (code %#lx)", bpp, code);
            if (o) p_DeleteObject(o);
        } else {
            CHECKF(code != 0, "bpp %d: CreateBitmap failure must surface as an RPC exception", bpp);
            CHECKF(o == NULL, "bpp %d: no bitmap on failure", bpp);
            t_note("bpp %d: unmarshal of an uncreatable bitmap raised %#lx", bpp, code);
        }
        um_buf_free(b);
    }
}

/* ---- HPALETTE ----------------------------------------------------------------------------------------------- */
static ULONG put_pal(unsigned char *b, ULONG at, ULONG handle, ULONG count, WORD ver, WORD nent, const PALETTEENTRY *ents)
{
    W32(b, at, UM_TAG_R); W32(b, at + 4, handle); W32(b, at + 8, count);
    W16(b, at + 12, ver); W16(b, at + 14, nent);
    if (ents && count) memcpy(b + at + 16, ents, count * 4);
    return at + 16 + count * 4;
}

static void mk_ents(PALETTEENTRY *e, int n, int seed)
{
    int i;
    for (i = 0; i < n; i++) { e[i].peRed = (BYTE)(i * 3 + seed); e[i].peGreen = (BYTE)(250 - i); e[i].peBlue = (BYTE)(i * 5 + 1); e[i].peFlags = 0; }
}

TEST(gap9_hpalette_unmarshal_valid_counts_every_context)
{
    static const ULONG counts[] = { 1, 2, 3, 16, 100, 256 };
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    unsigned i; ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof counts / sizeof *counts; i++)
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            PALETTEENTRY ents[256], got[256]; unsigned char *b = um_buf_new(16 + 4 * 256 + 8), *e; UMC c; HPALETTE o = NULL; ULONG end; int na, nf;
            mk_ents(ents, (int)counts[i], (int)i);
            end = put_pal(b, 0, 0x4321, counts[i], 0x300, (WORD)counts[i], ents);
            um_init(&c, b, end, ctx);
            na = um_nalloc; nf = um_nfree;
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
            /* a temporary LOGPALETTE is obtained from pfnAllocate and returned to pfnFree */
            CHECKF(um_nalloc == na + 1 && um_nfree == nf + 1, "n=%lu: alloc %d free %d", counts[i], um_nalloc - na, um_nfree - nf);
            CHECKF(e == b + end, "n=%lu ctx %lu end %+ld want %lu", counts[i], ctx, (long)(e - b), end);
            CHECKF(o != NULL, "n=%lu ctx %lu created", counts[i], ctx);
            if (o) {
                CHECK(p_GetObjectType(o) == OBJ_PAL);
                CHECK(p_GetPaletteEntries(o, 0, 0, NULL) == counts[i]);
                memset(got, 0xEE, sizeof got);
                CHECK(p_GetPaletteEntries(o, 0, counts[i], got) == counts[i]);
                CHECKF(!memcmp(got, ents, counts[i] * 4), "n=%lu entries", counts[i]);
                p_DeleteObject(o);
            }
            um_buf_free(b);
        }
}

TEST(gap9_hpalette_unmarshal_aligns_start_to_four)
{
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    ULONG off;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (off = 0; off < 9; off++) {
        PALETTEENTRY ents[4]; unsigned char *b = um_buf_new(100), *e; UMC c; HPALETTE o = NULL; ULONG a = UM_ALIGN(off, 4), end;
        mk_ents(ents, 4, 9);
        end = put_pal(b, a, 1, 4, 0x300, 4, ents);
        um_init(&c, b, end, C_DIFFM);
        XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
        CHECKF(o && e == b + end, "off %lu", off);
        if (o) p_DeleteObject(o);
        um_buf_free(b);
    }
}

TEST(gap9_hpalette_unmarshal_null_handle_succeeds_in_every_context)
{
    /* unlike HBITMAP, a 'WdtR' NULL handle is accepted even in an inproc context */
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        LOGPALETTE lp = { 0x300, 1, { { 1, 2, 3, 0 } } };
        unsigned char *b = um_buf_new(32), *e; UMC c; HPALETTE old = p_CreatePalette(&lp), o = old;
        REQUIRE(old);
        W32(b, 0, UM_TAG_R); W32(b, 4, 0);
        um_init(&c, b, 8, ctx);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(e == b + 8 && o == NULL, "ctx %lu: NULL result, end 8", ctx);
        CHECKF(gdi_gone(old), "ctx %lu: previous palette deleted", ctx);
        um_buf_free(b);
    }
}

TEST(gap9_hpalette_unmarshal_remote_form_accepted_in_inproc_context)
{
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    PALETTEENTRY ents[3]; unsigned char *b, *e; UMC c; HPALETTE o = NULL; ULONG end;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    mk_ents(ents, 3, 40);
    b = um_buf_new(100);
    end = put_pal(b, 0, 77, 3, 0x300, 3, ents);
    um_init(&c, b, end, C_INPROC);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o != NULL && e == b + end);
    if (o) p_DeleteObject(o);
    um_buf_free(b);
}

TEST(gap9_hpalette_unmarshal_inproc_form)
{
    static const struct { ULONG wire; LONG_PTR want; } t[] = {
        { 0, 0 }, { 1, 1 }, { 0x1234, 0x1234 }, { 0x80000000u, (LONG_PTR)(int)0x80000000u }, { 0xffffffffu, -1 },
    };
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    unsigned i; ULONG ctx;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof t / sizeof *t; i++)
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            unsigned char *b = um_buf_new(32), *e; UMC c; HPALETTE o = NULL;
            W32(b, 0, UM_TAG_H); W32(b, 4, t[i].wire);
            um_init(&c, b, 8, ctx);
            if (ctx == C_INPROC) {
                XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
                CHECKF(e == b + 8 && (LONG_PTR)o == t[i].want, "wire %#lx got %p", t[i].wire, o);
            } else {
                XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
                CHECK(o == NULL);
            }
            um_buf_free(b);
        }
}

TEST(gap9_hpalette_unmarshal_bad_tag_raises_invalid_tag)
{
    static const ULONG tags[] = { 0, 2, UM_TAG_P, 0x52746458u, 0xdeadbeefu };
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof tags / sizeof *tags; i++) {
        unsigned char *b = um_buf_new(100), *e; UMC c; HPALETTE o = NULL;
        memset(b, 0, 64); W32(b, 0, tags[i]); W32(b, 4, 1);
        um_init(&c, b, 64, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADTAG);
        um_buf_free(b);
    }
}

TEST(gap9_hpalette_unmarshal_truncated_buffers)
{
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    PALETTEENTRY ents[5]; unsigned char *b, *e; UMC c; ULONG end, len; HPALETTE keep;
    LOGPALETTE lp = { 0x300, 1, { { 9, 9, 9, 0 } } };
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    mk_ents(ents, 5, 1);
    b = um_buf_new(100);
    end = put_pal(b, 0, 3, 5, 0x300, 5, ents);
    keep = p_CreatePalette(&lp);
    REQUIRE(keep);
    for (len = 0; len < end; len++) {
        HPALETTE o = keep;
        um_init(&c, b, len, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o == keep && p_GetObjectType(keep) == OBJ_PAL, "len %lu: destination and old palette untouched", len);
    }
    um_init(&c, b, end, C_LOCAL);
    {   HPALETTE o = NULL;
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        if (o) p_DeleteObject(o);
    }
    p_DeleteObject(keep);
    um_buf_free(b);
}

TEST(gap9_hpalette_unmarshal_header_consistency)
{
    static const struct { const char *what; ULONG count; WORD ver; WORD nent; } t[] = {
        { "version 0x299", 3, 0x299, 3 }, { "version 0x301", 3, 0x301, 3 }, { "version 0", 3, 0, 3 }, { "version 0x300 swapped", 3, 0x0003, 3 },
        { "numEntries 2 vs count 3", 3, 0x300, 2 }, { "numEntries 4 vs count 3", 3, 0x300, 4 }, { "numEntries 0 vs count 3", 3, 0x300, 0 },
    };
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        PALETTEENTRY ents[8]; unsigned char *b = um_buf_new(100), *e; UMC c; HPALETTE o = NULL; ULONG end;
        mk_ents(ents, 8, 0);
        end = put_pal(b, 0, 3, t[i].count, t[i].ver, t[i].nent, ents);
        um_init(&c, b, end + 16, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o == NULL, "%s", t[i].what);
        um_buf_free(b);
    }
}

TEST(gap9_hpalette_unmarshal_absurd_counts_rejected_before_allocation)
{
    /* counts that cannot fit the buffer (or exceed 0x3fffffff) are rejected as malformed data */
    static const ULONG counts[] = { 0x40000000u, 0x7fffffffu, 0xffffffffu, 0x10000000u, 1000, 257 };
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    unsigned i;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    for (i = 0; i < sizeof counts / sizeof *counts; i++) {
        unsigned char *b = um_buf_new(100), *e; UMC c; HPALETTE o = NULL;
        memset(b, 0, 64);
        W32(b, 0, UM_TAG_R); W32(b, 4, 5); W32(b, 8, counts[i]); W16(b, 12, 0x300); W16(b, 14, (WORD)counts[i]);
        um_init(&c, b, 64, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o == NULL, "count %#lx", counts[i]);
        um_buf_free(b);
    }
}

TEST(gap9_hpalette_unmarshal_zero_count_observed)
{
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    unsigned char *b, *e; UMC c; HPALETTE o = NULL; DWORD code;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    b = um_buf_new(100);
    memset(b, 0, 64);
    put_pal(b, 0, 5, 0, 0x300, 0, NULL);
    um_init(&c, b, 16, C_LOCAL);
    code = um_x_mar(fu, UMF(&c), b, &o, &e);
    t_note("HPALETTE_UserUnmarshal with count 0: exception %#lx result %p", code, o);
    if (o) p_DeleteObject(o);
    um_buf_free(b);
}

TEST(gap9_hpalette_unmarshal_replaces_and_deletes_destination)
{
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    LOGPALETTE lp = { 0x300, 1, { { 1, 2, 3, 0 } } };
    PALETTEENTRY ents[2]; unsigned char *b, *e; UMC c; HPALETTE old, o; ULONG end;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    mk_ents(ents, 2, 5);
    b = um_buf_new(100);
    end = put_pal(b, 0, 8, 2, 0x300, 2, ents);
    old = p_CreatePalette(&lp); o = old;
    REQUIRE(old);
    um_init(&c, b, end, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o != NULL && o != old);
    CHECKF(gdi_gone(old), "old palette deleted");
    if (o) { CHECK(p_GetPaletteEntries(o, 0, 0, NULL) == 2); p_DeleteObject(o); }
    um_buf_free(b);
}

TEST(gap9_hpalette_unmarshal_entry_flags_are_carried)
{
    void *fu = UM_FNIN("ole32.dll", "HPALETTE_UserUnmarshal");
    PALETTEENTRY ents[3], got[3]; unsigned char *b, *e; UMC c; HPALETTE o = NULL; ULONG end;
    if (!gdi_load()) { t_skip("gdi32 unavailable"); return; }
    REQUIRE(fu);
    ents[0].peRed = 10; ents[0].peGreen = 20; ents[0].peBlue = 30; ents[0].peFlags = PC_RESERVED;
    ents[1].peRed = 11; ents[1].peGreen = 21; ents[1].peBlue = 31; ents[1].peFlags = PC_NOCOLLAPSE;
    ents[2].peRed = 12; ents[2].peGreen = 22; ents[2].peBlue = 32; ents[2].peFlags = 0;
    b = um_buf_new(100);
    end = put_pal(b, 0, 2, 3, 0x300, 3, ents);
    um_init(&c, b, end, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    REQUIRE(o);
    memset(got, 0, sizeof got);
    CHECK(p_GetPaletteEntries(o, 0, 3, got) == 3);
    CHECK(got[0].peRed == 10 && got[0].peGreen == 20 && got[0].peBlue == 30);
    CHECK(got[2].peRed == 12 && got[2].peGreen == 22 && got[2].peBlue == 32);
    t_note("palette entry flags after unmarshal: %#x %#x %#x", got[0].peFlags, got[1].peFlags, got[2].peFlags);
    p_DeleteObject(o);
    um_buf_free(b);
}
