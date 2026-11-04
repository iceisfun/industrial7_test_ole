/* gap_09: STGMEDIUM_UserSize and STGMEDIUM_UserUnmarshal (ole32.dll; implemented in combase on Win10).
 *
 * Win10 decompilation:
 *   Size(flags, off, pstg):  pstg == NULL -> off.  a = align4(off); base = a + (tymed ? 12 : 8)   (tymed, [u marker], pUnk marker)
 *     if u != NULL: payload by tymed at `base`:
 *       1 HGLOBAL_UserSize(base)            2 FILENAME: base + 12 + 2*(wcslen+1)
 *       4 / 8  WdtpInterfacePointer_UserSize(flags, base, punk, IID_IStream / IID_IStorage)
 *       0x10   base + 4, then HPALETTE_UserSize (object type 5) / HBITMAP_UserSize (type 7), else raise DV_E_TYMED (0x80040069)
 *       0x20 HMETAFILEPICT_UserSize   0x40 HENHMETAFILE_UserSize   any other tymed (including 0): nothing, no exception
 *     if pUnkForRelease != NULL: WdtpInterfacePointer_UserSize(flags, offset-so-far, pUnk, IID_IUnknown)  (even when u == NULL)
 *   Unmarshal(flags, buf, pstg): wire DWORD tymed, [DWORD u-id if tymed != 0], DWORD pUnk-marker, payload, [pUnk].
 *     tymed is stored into *pstg before the payload is validated; every inconsistency raises RPC_X_BAD_STUB_DATA (0x6f7).
 *     u-id must equal the id word that the payload helper puts on the wire (HGLOBAL/metafile handle word, GDI handle word).
 *     FILENAME payload: max, offset (must be 0), actual (== max), WCHAR[max] with the terminator exactly at max-1. */
#include "g9.h"

typedef ULONG (WINAPI *wsz_t)(ULONG *, ULONG, ULONG, IUnknown *, const GUID *);

static const GUID k_IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID k_IID_IStream  = { 0x0000000c, 0x0000, 0x0000, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };

#define TY_NULL 0
#define TY_HGLOBAL 1
#define TY_FILE 2
#define TY_ISTREAM 4
#define TY_ISTORAGE 8
#define TY_GDI 16
#define TY_MFPICT 32
#define TY_ENHMF 64

#define W32(b, o, v) (*(ULONG *)((b) + (o)) = (ULONG)(v))

static void *sfn(const char *op) { char n[64]; snprintf(n, sizeof n, "STGMEDIUM_User%s", op); return UM_FNIN("ole32.dll", n); }

static int com_up(void)
{
    HRESULT hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    return SUCCEEDED(hr);
}

/* ---- Size ---------------------------------------------------------------------------------------------------- */
TEST(gap9_stgmedium_size_null_tymed_ignores_u)
{
    void *fs = sfn("Size");
    ULONG off, ctx, r;
    REQUIRE(fs);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 10; off++) {
            UMC c; STGMEDIUM m;
            memset(&m, 0, sizeof m);
            m.hGlobal = (HGLOBAL)(ULONG_PTR)0x1000;      /* tymed 0: u is never examined as an object */
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
            CHECKF(r == UM_ALIGN(off, 4) + 8, "ctx %lu off %lu got %lu", ctx, off, r);
        }
}

TEST(gap9_stgmedium_size_unknown_tymed_adds_header_only)
{
    static const DWORD tys[] = { 3, 5, 6, 7, 9, 12, 0x80, 0x100, 0x1000, 0x80000000u };
    void *fs = sfn("Size");
    unsigned i; ULONG off, r;
    REQUIRE(fs);
    for (i = 0; i < sizeof tys / sizeof *tys; i++)
        for (off = 0; off < 6; off++) {
            UMC c; STGMEDIUM m;
            memset(&m, 0, sizeof m); m.tymed = tys[i]; m.hGlobal = (HGLOBAL)(ULONG_PTR)0x1000;
            um_init(&c, NULL, 0, C_LOCAL);
            XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
            CHECKF(r == UM_ALIGN(off, 4) + 12, "tymed %#lx off %lu got %lu", tys[i], off, r);
        }
}

TEST(gap9_stgmedium_size_file_name_every_small_length)
{
    void *fs = sfn("Size");
    unsigned n; ULONG off, ctx, r;
    REQUIRE(fs);
    for (n = 0; n <= 12; n++) {
        WCHAR name[16]; unsigned i;
        for (i = 0; i < n; i++) name[i] = (WCHAR)('a' + i);
        name[n] = 0;
        for (ctx = 0; ctx <= C_CONT; ctx += 5)
            for (off = 0; off < 9; off++) {
                UMC c; STGMEDIUM m;
                memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = name;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
                CHECKF(r == UM_ALIGN(off, 4) + 24 + 2 * (n + 1), "len %u ctx %lu off %lu got %lu", n, ctx, off, r);
            }
    }
}

TEST(gap9_stgmedium_size_file_name_long)
{
    static const unsigned lens[] = { 255, 259, 260, 1000, 4096 };
    void *fs = sfn("Size");
    unsigned i; ULONG r;
    REQUIRE(fs);
    for (i = 0; i < sizeof lens / sizeof *lens; i++) {
        WCHAR *name = malloc((lens[i] + 1) * sizeof(WCHAR)); UMC c; STGMEDIUM m; unsigned k;
        REQUIRE(name);
        for (k = 0; k < lens[i]; k++) name[k] = L'x';
        name[lens[i]] = 0;
        memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = name;
        um_init(&c, NULL, 0, C_DIFFM);
        XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
        CHECKF(r == 24 + 2 * (lens[i] + 1), "len %u got %lu", lens[i], r);
        free(name);
    }
}

TEST(gap9_stgmedium_size_unk_for_release_counted_even_without_payload)
{
    void *fs = sfn("Size"), *wf = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserSize");
    static const DWORD tys[] = { TY_NULL, TY_HGLOBAL, TY_FILE, 3, 0x80 };
    IStream *stm = NULL; int up;
    unsigned i; ULONG ctx, off;
    REQUIRE(fs); REQUIRE(wf);
    up = com_up();
    REQUIRE(up);
    if (FAILED(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm)) || !stm) { U(CoUninitialize)(); t_skip("no stream"); return; }
    for (i = 0; i < sizeof tys / sizeof *tys; i++)
        for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx++)
            for (off = 0; off < 6; off += 3) {
                UMC c; STGMEDIUM m; ULONG r0, r1, want, flags;
                memset(&m, 0, sizeof m); m.tymed = tys[i];
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &m, &r0), 0);
                m.pUnkForRelease = (IUnknown *)stm;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &m, &r1), 0);
                flags = c.cb.Flags;
                um_init(&c, NULL, 0, ctx);
                want = ((wsz_t)wf)(UMF(&c), flags, r0, (IUnknown *)stm, &k_IID_IUnknown);
                CHECKF(r1 == want, "tymed %#lx ctx %lu off %lu: %lu want %lu (no-unk %lu)", tys[i], ctx, off, r1, want, r0);
                CHECK(r1 > r0);
            }
    stm->lpVtbl->Release(stm);
    U(CoUninitialize)();
}

TEST(gap9_stgmedium_size_istream_composes_with_interface_pointer_size)
{
    void *fs = sfn("Size"), *wf = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserSize");
    IStream *stm = NULL; int up;
    ULONG ctx, off;
    REQUIRE(fs); REQUIRE(wf);
    up = com_up();
    REQUIRE(up);
    if (FAILED(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm)) || !stm) { U(CoUninitialize)(); t_skip("no stream"); return; }
    for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx++)
        for (off = 0; off < 10; off += 3) {
            UMC c; STGMEDIUM m; ULONG r, want;
            memset(&m, 0, sizeof m); m.tymed = TY_ISTREAM; m.pstm = stm;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
            um_init(&c, NULL, 0, ctx);
            want = ((wsz_t)wf)(UMF(&c), c.cb.Flags, UM_ALIGN(off, 4) + 12, (IUnknown *)stm, &k_IID_IStream);
            CHECKF(r == want, "ctx %lu off %lu: %lu want %lu", ctx, off, r, want);
            CHECK(r > UM_ALIGN(off, 4) + 12);
        }
    stm->lpVtbl->Release(stm);
    U(CoUninitialize)();
}

TEST(gap9_stgmedium_size_gdi_palette_composes_with_hpalette_size)
{
    void *fs = sfn("Size"), *ps = UM_FNIN("ole32.dll", "HPALETTE_UserSize");
    HPALETTE (WINAPI *cp)(const LOGPALETTE *) = (void *)t_proc_in("gdi32.dll", "CreatePalette");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "DeleteObject");
    struct { WORD ver, n; PALETTEENTRY e[2]; } lpb = { 0x300, 2, { { 1, 2, 3, 0 }, { 4, 5, 6, 0 } } };
    const LOGPALETTE *lp = (const LOGPALETTE *)&lpb;
    HPALETTE hp; ULONG ctx, off;
    REQUIRE(fs); REQUIRE(ps); REQUIRE(cp); REQUIRE(del);
    hp = cp(lp);
    REQUIRE(hp);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 10; off += 3) {
            UMC c; STGMEDIUM m; HPALETTE hh = hp; ULONG r, rp;
            memset(&m, 0, sizeof m); m.tymed = TY_GDI; m.hBitmap = (HBITMAP)hp;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
            XEQ(um_x_size(ps, UMF(&c), UM_ALIGN(off, 4) + 12 + 4, &hh, &rp), 0);
            CHECKF(r == rp, "ctx %lu off %lu: %lu vs HPALETTE_UserSize %lu", ctx, off, r, rp);
        }
    del(hp);
}

TEST(gap9_stgmedium_size_gdi_other_object_types_raise_dv_e_tymed)
{
    void *fs = sfn("Size");
    HPEN (WINAPI *cpen)(int, int, COLORREF) = (void *)t_proc_in("gdi32.dll", "CreatePen");
    HBRUSH (WINAPI *cbr)(COLORREF) = (void *)t_proc_in("gdi32.dll", "CreateSolidBrush");
    HFONT (WINAPI *cfont)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) = (void *)t_proc_in("gdi32.dll", "CreateFontA");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "DeleteObject");
    HGDIOBJ objs[3]; unsigned i;
    REQUIRE(fs); REQUIRE(cpen); REQUIRE(cbr); REQUIRE(cfont); REQUIRE(del);
    objs[0] = cpen(PS_SOLID, 1, 0); objs[1] = cbr(RGB(1, 2, 3)); objs[2] = cfont(10, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    for (i = 0; i < 3; i++) {
        UMC c; STGMEDIUM m; ULONG r;
        if (!objs[i]) continue;
        memset(&m, 0, sizeof m); m.tymed = TY_GDI; m.hBitmap = (HBITMAP)objs[i];
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0x80040069);
        del(objs[i]);
    }
}

TEST(gap9_stgmedium_size_flag_bit8_applies_only_with_nonnull_u_and_non_stream)
{
    /* bit 8 rewrite (low byte -> MSHCTX_DIFFERENTMACHINE) is skipped for TYMED_ISTREAM/ISTORAGE and when u == NULL */
    void *fs = sfn("Size");
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 20);
    IStream *stm = NULL; int up;
    UMC c; STGMEDIUM m; ULONG r;
    REQUIRE(fs); REQUIRE(g);
    up = com_up();
    REQUIRE(up);
    if (FAILED(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm)) || !stm) { U(CoUninitialize)(); GlobalFree(g); t_skip("no stream"); return; }
    memset(&m, 0, sizeof m); m.tymed = TY_ISTREAM; m.pstm = stm;
    um_init(&c, NULL, 0, C_INPROC); c.cb.Flags |= 0x100;
    XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
    CHECKF((c.cb.Flags & 0xff) == C_INPROC, "TYMED_ISTREAM keeps its context byte, got %lu", c.cb.Flags & 0xff);
    memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = (LPOLESTR)L"x";
    um_init(&c, NULL, 0, C_INPROC); c.cb.Flags |= 0x100;
    XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
    CHECKF((c.cb.Flags & 0xff) == C_DIFFM, "TYMED_FILE with bit8 INPROC -> DIFFERENTMACHINE, got %lu", c.cb.Flags & 0xff);
    CHECKF((c.cb.Flags >> 16) == 0x10, "upper flag bits are preserved: %#lx", c.cb.Flags);
    memset(&m, 0, sizeof m); m.tymed = TY_MFPICT;
    um_init(&c, NULL, 0, C_LOCAL); c.cb.Flags |= 0x100;
    XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
    CHECKF((c.cb.Flags & 0xff) == C_LOCAL, "u == NULL: flags untouched");
    /* without bit 8 nothing is rewritten */
    memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = (LPOLESTR)L"x";
    um_init(&c, NULL, 0, C_INPROC);
    XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
    CHECK((c.cb.Flags & 0xff) == C_INPROC);
    stm->lpVtbl->Release(stm);
    U(CoUninitialize)();
    GlobalFree(g);
}

TEST(gap9_stgmedium_size_hglobal_inproc_vs_remote_differ_only_by_payload)
{
    void *fs = sfn("Size"), *hs = UM_FNIN("ole32.dll", "HGLOBAL_UserSize");
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 33);
    ULONG off, r_in, r_rm, h_in, h_rm; HGLOBAL hg = g;
    REQUIRE(fs); REQUIRE(hs); REQUIRE(g);
    for (off = 0; off < 6; off++) {
        UMC c; STGMEDIUM m;
        memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
        um_init(&c, NULL, 0, C_INPROC); XEQ(um_x_size(fs, UMF(&c), off, &m, &r_in), 0);
        um_init(&c, NULL, 0, C_LOCAL);  XEQ(um_x_size(fs, UMF(&c), off, &m, &r_rm), 0);
        um_init(&c, NULL, 0, C_INPROC); XEQ(um_x_size(hs, UMF(&c), UM_ALIGN(off, 4) + 12, &hg, &h_in), 0);
        um_init(&c, NULL, 0, C_LOCAL);  XEQ(um_x_size(hs, UMF(&c), UM_ALIGN(off, 4) + 12, &hg, &h_rm), 0);
        CHECK(r_in == h_in);
        CHECK(r_rm == h_rm);
        CHECK(r_rm > r_in);
    }
    GlobalFree(g);
}

/* ---- Unmarshal ------------------------------------------------------------------------------------------------ */
TEST(gap9_stgmedium_unmarshal_tymed_null_wire)
{
    void *fu = sfn("Unmarshal");
    ULONG off;
    REQUIRE(fu);
    for (off = 0; off < 9; off++) {
        unsigned char *b = um_buf_new(40), *e; UMC c; STGMEDIUM o; ULONG a = UM_ALIGN(off, 4);
        W32(b, a, 0); W32(b, a + 4, 0);
        memset(&o, 0, sizeof o); o.tymed = 99;
        um_init(&c, b, a + 8, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
        CHECKF(e == b + a + 8, "off %lu end %+ld", off, (long)(e - b));
        CHECK(o.tymed == 0 && o.pUnkForRelease == NULL && o.hGlobal == NULL);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_tymed_null_truncation)
{
    void *fu = sfn("Unmarshal");
    ULONG len;
    REQUIRE(fu);
    for (len = 0; len < 8; len++) {
        unsigned char *b = um_buf_new(40), *e; UMC c; STGMEDIUM o;
        W32(b, 0, 0); W32(b, 4, 0);
        memset(&o, 0, sizeof o);
        um_init(&c, b, len, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_unknown_tymed_consumes_header_only)
{
    static const DWORD tys[] = { 3, 5, 6, 7, 9, 12, 0x80, 0x100, 0x1000 };
    void *fu = sfn("Unmarshal");
    unsigned i;
    REQUIRE(fu);
    for (i = 0; i < sizeof tys / sizeof *tys; i++) {
        unsigned char *b = um_buf_new(40), *e; UMC c; STGMEDIUM o;
        /* with a non-zero u marker and a zero pUnk marker */
        W32(b, 0, tys[i]); W32(b, 4, 0x4444); W32(b, 8, 0);
        memset(&o, 0, sizeof o);
        um_init(&c, b, 12, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(e == b + 12, "tymed %#lx end %+ld", tys[i], (long)(e - b));
        CHECK(o.tymed == tys[i] && o.hGlobal == NULL && o.pUnkForRelease == NULL);
        /* and with a zero u marker */
        W32(b, 4, 0);
        memset(&o, 0, sizeof o);
        um_init(&c, b, 12, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECKF(e == b + 12 && o.tymed == tys[i], "tymed %#lx (null u)", tys[i]);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_nonnull_tymed_header_truncation)
{
    static const DWORD tys[] = { TY_HGLOBAL, TY_FILE, TY_ISTREAM, TY_ISTORAGE, TY_GDI, TY_MFPICT, TY_ENHMF, 0x80 };
    void *fu = sfn("Unmarshal");
    unsigned i; ULONG len;
    REQUIRE(fu);
    for (i = 0; i < sizeof tys / sizeof *tys; i++)
        for (len = 0; len < 12; len++) {
            unsigned char *b = um_buf_new(40), *e; UMC c; STGMEDIUM o;
            W32(b, 0, tys[i]); W32(b, 4, 0); W32(b, 8, 0);
            memset(&o, 0, sizeof o);
            um_init(&c, b, len, C_LOCAL);
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
            um_buf_free(b);
        }
}

TEST(gap9_stgmedium_unmarshal_null_u_marker_clears_stale_member_for_tymed_null)
{
    /* tymed 0 with a non-NULL stale u: the member is reset to NULL without freeing anything (ReleaseStgMedium(TYMED_NULL)) */
    void *fu = sfn("Unmarshal");
    unsigned char *b = um_buf_new(40), *e; UMC c; STGMEDIUM o; void *blk;
    REQUIRE(fu);
    blk = U(CoTaskMemAlloc)(16);
    REQUIRE(blk);
    W32(b, 0, 0); W32(b, 4, 0);
    memset(&o, 0, sizeof o); o.hGlobal = blk;
    um_init(&c, b, 8, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o.tymed == 0 && o.hGlobal == NULL);
    memset(blk, 0xAB, 16);                       /* still our allocation */
    U(CoTaskMemFree)(blk);
    um_buf_free(b);
}

/* build a FILENAME wire: tymed 2, u-id, pUnk 0, max, off, act, chars */
static ULONG put_file(unsigned char *b, ULONG at, ULONG max, ULONG offs, ULONG act, const WCHAR *chars, ULONG nchars)
{
    W32(b, at, TY_FILE); W32(b, at + 4, 0x2000); W32(b, at + 8, 0);
    W32(b, at + 12, max); W32(b, at + 16, offs); W32(b, at + 20, act);
    if (nchars) memcpy(b + at + 24, chars, nchars * 2);
    return at + 24 + nchars * 2;
}

TEST(gap9_stgmedium_unmarshal_file_name_valid_lengths)
{
    void *fu = sfn("Unmarshal");
    unsigned n; ULONG off;
    REQUIRE(fu);
    for (n = 0; n <= 9; n++)
        for (off = 0; off < 5; off += 3) {
            WCHAR name[16]; unsigned i; unsigned char *b = um_buf_new(200), *e; UMC c; STGMEDIUM o; ULONG a = UM_ALIGN(off, 4), end; int before;
            for (i = 0; i < n; i++) name[i] = (WCHAR)(0x100 + i);
            name[n] = 0;
            end = put_file(b, a, n + 1, 0, n + 1, name, n + 1);
            memset(&o, 0, sizeof o);
            um_init(&c, b, end, C_DIFFM);
            before = um_nalloc;
            XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
            CHECKF(e == b + end, "len %u off %lu end %+ld want %lu", n, off, (long)(e - b), end);
            CHECK(o.tymed == TY_FILE);
            if (um_nalloc != before + 1) t_note("len %u: pfnAllocate called %d times for the file name", n, um_nalloc - before);
            CHECK(o.lpszFileName && !lstrcmpW(o.lpszFileName, name));
            if (o.lpszFileName) U(CoTaskMemFree)(o.lpszFileName);
            um_buf_free(b);
        }
}

TEST(gap9_stgmedium_unmarshal_file_name_reuses_existing_buffer)
{
    void *fu = sfn("Unmarshal");
    static const WCHAR nm[] = L"abcd";
    unsigned char *b = um_buf_new(100), *e; UMC c; STGMEDIUM o; WCHAR *buf; ULONG end; int before;
    REQUIRE(fu);
    end = put_file(b, 0, 5, 0, 5, nm, 5);
    /* a destination string that is long enough is overwritten in place, no allocation */
    buf = U(CoTaskMemAlloc)(40);
    REQUIRE(buf);
    lstrcpyW(buf, L"0123456789");
    memset(&o, 0, sizeof o); o.lpszFileName = buf;
    um_init(&c, b, end, C_LOCAL);
    before = um_nalloc;
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o.lpszFileName == buf);
    if (um_nalloc != before) t_note("pfnAllocate called %d times although the destination string was reused", um_nalloc - before);
    CHECK(!lstrcmpW(buf, nm));
    /* exactly the same length is fine too */
    lstrcpyW(buf, L"WXYZ");
    um_init(&c, b, end, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o.lpszFileName == buf && !lstrcmpW(buf, nm));
    /* shorter than the incoming string: refused */
    lstrcpyW(buf, L"ab");
    um_init(&c, b, end, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    CHECK(o.lpszFileName == buf && !lstrcmpW(buf, L"ab"));
    U(CoTaskMemFree)(buf);
    um_buf_free(b);
}

TEST(gap9_stgmedium_unmarshal_file_name_malformed_wire)
{
    static const WCHAR abc[] = L"abc";
    static const WCHAR noterm[] = { L'a', L'b', L'c', L'd' };
    static const WCHAR midnul[] = { L'a', 0, L'c', 0 };
    void *fu = sfn("Unmarshal");
    struct { const char *what; ULONG max, offs, act; const WCHAR *ch; ULONG nch; } t[] = {
        { "offset != 0", 4, 1, 4, abc, 4 },
        { "actual < max", 4, 0, 3, abc, 4 },
        { "actual > max", 4, 0, 5, abc, 4 },
        { "max 0", 0, 0, 0, abc, 0 },
        { "no terminator", 4, 0, 4, noterm, 4 },
        { "terminator before the end", 4, 0, 4, midnul, 4 },
        { "max larger than data present", 40, 0, 40, abc, 4 },
        { "huge max", 0x7fffffffu, 0, 0x7fffffffu, abc, 4 },
        { "max 0xffffffff", 0xffffffffu, 0, 0xffffffffu, abc, 4 },
    };
    unsigned i;
    REQUIRE(fu);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        unsigned char *b = um_buf_new(200), *e; UMC c; STGMEDIUM o; ULONG end;
        end = put_file(b, 0, t[i].max, t[i].offs, t[i].act, t[i].ch, t[i].nch);
        memset(&o, 0, sizeof o);
        um_init(&c, b, end, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o.lpszFileName == NULL, "%s: no string allocated", t[i].what);
        CHECKF(o.tymed == TY_FILE, "%s: tymed is stored before validation", t[i].what);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_file_name_truncated)
{
    static const WCHAR nm[] = L"abcd";
    void *fu = sfn("Unmarshal");
    unsigned char *b = um_buf_new(100), *e; ULONG end, len;
    REQUIRE(fu);
    end = put_file(b, 0, 5, 0, 5, nm, 5);
    for (len = 0; len < end; len++) {
        UMC c; STGMEDIUM o; memset(&o, 0, sizeof o);
        um_init(&c, b, len, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o.lpszFileName == NULL, "len %lu", len);
    }
    {   UMC c; STGMEDIUM o; memset(&o, 0, sizeof o);
        um_init(&c, b, end, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        if (o.lpszFileName) U(CoTaskMemFree)(o.lpszFileName);
    }
    um_buf_free(b);
}

/* marshal a TYMED_HGLOBAL STGMEDIUM and return the wire in a fresh buffer */
static unsigned char *marshal_hglobal(HGLOBAL g, ULONG ctx, ULONG *len)
{
    void *fs = sfn("Size"), *fm = sfn("Marshal");
    UMC c; STGMEDIUM m; ULONG sz; unsigned char *b, *e;
    memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
    um_init(&c, NULL, 0, ctx);
    if (um_x_size(fs, UMF(&c), 0, &m, &sz)) return NULL;
    b = um_buf_new(sz + 64);
    um_init(&c, b, sz + 64, ctx);
    if (um_x_mar(fm, UMF(&c), b, &m, &e)) { um_buf_free(b); return NULL; }
    *len = (ULONG)(e - b);
    return b;
}

TEST(gap9_stgmedium_unmarshal_hglobal_id_consistency)
{
    void *fu = sfn("Unmarshal"), *fm = sfn("Marshal");
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 40);
    ULONG len = 0; unsigned char *b, *e; UMC c; STGMEDIUM o;
    REQUIRE(fu); REQUIRE(fm); REQUIRE(g);
    b = marshal_hglobal(g, C_LOCAL, &len);
    REQUIRE(b);
    CHECK(RD_U32(b, 0) == TY_HGLOBAL);
    CHECK(RD_U32(b, 4) == (ULONG)(ULONG_PTR)g);
    CHECK(RD_U32(b, 12) == UM_TAG_R);
    CHECK(RD_U32(b, 16) == (ULONG)(ULONG_PTR)g);
    /* untouched wire works */
    memset(&o, 0, sizeof o);
    um_init(&c, b, len, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o.tymed == TY_HGLOBAL && o.hGlobal != NULL);
    if (o.hGlobal) GlobalFree(o.hGlobal);
    /* u marker differs from the HGLOBAL wire handle */
    W32(b, 4, RD_U32(b, 4) + 4);
    memset(&o, 0, sizeof o);
    um_init(&c, b, len, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    CHECK(o.hGlobal == NULL);
    W32(b, 4, RD_U32(b, 4) - 4);
    /* handle word inside the payload differs */
    W32(b, 16, RD_U32(b, 16) ^ 0x10);
    memset(&o, 0, sizeof o);
    um_init(&c, b, len, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    CHECK(o.hGlobal == NULL);
    um_buf_free(b);
    GlobalFree(g);
}

TEST(gap9_stgmedium_unmarshal_hglobal_truncated_payload)
{
    void *fu = sfn("Unmarshal");
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 40);
    ULONG len = 0, n; unsigned char *b, *e;
    REQUIRE(fu); REQUIRE(g);
    b = marshal_hglobal(g, C_DIFFM, &len);
    REQUIRE(b);
    for (n = 0; n < len; n++) {
        UMC c; STGMEDIUM o; memset(&o, 0, sizeof o);
        um_init(&c, b, n, C_DIFFM);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    }
    {   UMC c; STGMEDIUM o; memset(&o, 0, sizeof o);
        um_init(&c, b, len, C_DIFFM);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        if (o.hGlobal) GlobalFree(o.hGlobal);
    }
    um_buf_free(b);
    GlobalFree(g);
}

TEST(gap9_stgmedium_unmarshal_flag_bit8_rewrites_context_to_remote)
{
    /* wire produced under LOCAL (remote HGLOBAL form); unmarshalled with INPROC|bit8 the context becomes DIFFERENTMACHINE and the wire is accepted */
    void *fu = sfn("Unmarshal");
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 40);
    ULONG len = 0; unsigned char *b, *e; UMC c; STGMEDIUM o;
    REQUIRE(fu); REQUIRE(g);
    b = marshal_hglobal(g, C_LOCAL, &len);
    REQUIRE(b);
    memset(&o, 0, sizeof o);
    um_init(&c, b, len, C_INPROC); c.cb.Flags |= 0x100;
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECKF((c.cb.Flags & 0xff) == C_DIFFM, "context byte rewritten to DIFFERENTMACHINE, got %lu", c.cb.Flags & 0xff);
    CHECK(o.tymed == TY_HGLOBAL && o.hGlobal != NULL);
    if (o.hGlobal) GlobalFree(o.hGlobal);
    /* NOSHAREDMEM (1) is exempt */
    memset(&o, 0, sizeof o);
    um_init(&c, b, len, C_NOSHM); c.cb.Flags |= 0x100;
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK((c.cb.Flags & 0xff) == C_NOSHM);
    if (o.hGlobal) GlobalFree(o.hGlobal);
    /* without bit 8 an inproc context does not accept the remote HGLOBAL form */
    memset(&o, 0, sizeof o);
    um_init(&c, b, len, C_INPROC);
    {   DWORD code = um_x_mar(fu, UMF(&c), b, &o, &e);
        t_note("INPROC without bit 8 raised %#lx", code);
        if (!code && o.hGlobal) GlobalFree(o.hGlobal);
    }
    um_buf_free(b);
    GlobalFree(g);
}

TEST(gap9_stgmedium_unmarshal_gdi_object_type_dispatch)
{
    static const DWORD bad[] = { 0, 1, 2, 3, 4, 6, 8, 9, 0x10, 0xffffffffu };
    void *fu = sfn("Unmarshal");
    unsigned i;
    REQUIRE(fu);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        unsigned char *b = um_buf_new(100), *e; UMC c; STGMEDIUM o;
        memset(b, 0, 64);
        W32(b, 0, TY_GDI); W32(b, 4, 0x77); W32(b, 8, 0); W32(b, 12, bad[i]);
        W32(b, 16, UM_TAG_R); W32(b, 20, 0x77);
        memset(&o, 0, sizeof o);
        um_init(&c, b, 64, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0x80040069);
        CHECKF(o.hBitmap == NULL, "object type %lu", bad[i]);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_gdi_handle_id_must_match)
{
    void *fu = sfn("Unmarshal");
    static const DWORD types[] = { 5, 7 };
    unsigned i;
    REQUIRE(fu);
    for (i = 0; i < 2; i++) {
        unsigned char *b = um_buf_new(100), *e; UMC c; STGMEDIUM o;
        memset(b, 0, 64);
        W32(b, 0, TY_GDI); W32(b, 4, 0x77); W32(b, 8, 0); W32(b, 12, types[i]);
        W32(b, 16, UM_TAG_R); W32(b, 20, 0x78);            /* id differs from the u marker */
        memset(&o, 0, sizeof o);
        um_init(&c, b, 64, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECK(o.hBitmap == NULL);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_gdi_truncated_before_object_type)
{
    void *fu = sfn("Unmarshal");
    ULONG len;
    REQUIRE(fu);
    for (len = 12; len < 24; len++) {
        unsigned char *b = um_buf_new(100), *e; UMC c; STGMEDIUM o;
        memset(b, 0, 64);
        W32(b, 0, TY_GDI); W32(b, 4, 0x77); W32(b, 8, 0); W32(b, 12, 7); W32(b, 16, UM_TAG_R); W32(b, 20, 0x77);
        memset(&o, 0, sizeof o);
        um_init(&c, b, len, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        um_buf_free(b);
    }
}

TEST(gap9_stgmedium_unmarshal_gdi_palette_roundtrip)
{
    void *fs = sfn("Size"), *fm = sfn("Marshal"), *fu = sfn("Unmarshal"), *ff = sfn("Free");
    HPALETTE (WINAPI *cp)(const LOGPALETTE *) = (void *)t_proc_in("gdi32.dll", "CreatePalette");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "DeleteObject");
    DWORD (WINAPI *gtype)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "GetObjectType");
    UINT (WINAPI *gpe)(HPALETTE, UINT, UINT, LPPALETTEENTRY) = (void *)t_proc_in("gdi32.dll", "GetPaletteEntries");
    struct { WORD ver, n; PALETTEENTRY e[3]; } lpb = { 0x300, 3, { { 1, 2, 3, 0 }, { 4, 5, 6, 0 }, { 7, 8, 9, 0 } } };
    const LOGPALETTE *lp = (const LOGPALETTE *)&lpb;
    HPALETTE hp; ULONG ctx;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ff); REQUIRE(cp); REQUIRE(del); REQUIRE(gtype); REQUIRE(gpe);
    hp = cp(lp);
    REQUIRE(hp);
    for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2) {
        UMC c; STGMEDIUM m, o; ULONG sz; unsigned char *b, *e, *e2; PALETTEENTRY pe[3];
        memset(&m, 0, sizeof m); m.tymed = TY_GDI; m.hBitmap = (HBITMAP)hp;
        um_init(&c, NULL, 0, ctx); XEQ(um_x_size(fs, UMF(&c), 2, &m, &sz), 0);
        b = um_buf_new(sz + 32);
        um_init(&c, b, sz + 32, ctx); XEQ(um_x_mar(fm, UMF(&c), b + 2, &m, &e), 0);
        CHECK(RD_U32(b, 4 + 12) == 5);
        memset(&o, 0, sizeof o);
        um_init(&c, b, (ULONG)(e - b), ctx); XEQ(um_x_mar(fu, UMF(&c), b + 2, &o, &e2), 0);
        CHECK(e2 == e);
        CHECKF(o.tymed == TY_GDI && o.hBitmap && gtype(o.hBitmap) == OBJ_PAL, "palette comes back as a palette");
        if (o.hBitmap) {
            memset(pe, 0, sizeof pe);
            { UINT n = gpe((HPALETTE)o.hBitmap, 0, 3, pe);
              CHECKF(n == 3 && pe[2].peBlue == 9 && pe[0].peRed == 1, "n=%u e0=%u,%u,%u,%u e1=%u,%u,%u,%u e2=%u,%u,%u,%u", n,
                     pe[0].peRed, pe[0].peGreen, pe[0].peBlue, pe[0].peFlags, pe[1].peRed, pe[1].peGreen, pe[1].peBlue, pe[1].peFlags,
                     pe[2].peRed, pe[2].peGreen, pe[2].peBlue, pe[2].peFlags); }
            um_init(&c, NULL, 0, ctx); XEQ(um_x_free(ff, UMF(&c), &o), 0);
            CHECKF(!del(o.hBitmap), "UserFree deleted the palette (second DeleteObject fails; GetObjectType still reports it on Win10)");
        }
        um_buf_free(b);
    }
    del(hp);
}

TEST(gap9_stgmedium_unmarshal_istream_roundtrip_and_refcount)
{
    void *fs = sfn("Size"), *fm = sfn("Marshal"), *fu = sfn("Unmarshal"), *ff = sfn("Free");
    IStream *stm = NULL; int up;
    UMC c; STGMEDIUM m, o; ULONG sz; unsigned char *b, *e, *e2; ULONG refs;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ff);
    up = com_up();
    REQUIRE(up);
    if (FAILED(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm)) || !stm) { U(CoUninitialize)(); t_skip("no stream"); return; }
    {   ULONG w = 0; stm->lpVtbl->Write(stm, "olet-gap9", 10, &w); }
    memset(&m, 0, sizeof m); m.tymed = TY_ISTREAM; m.pstm = stm;
    um_init(&c, NULL, 0, C_LOCAL);
    XEQ(um_x_size(fs, UMF(&c), 0, &m, &sz), 0);
    b = um_buf_new(sz + 64);
    um_init(&c, b, sz + 64, C_LOCAL);
    if (um_x_mar(fm, UMF(&c), b, &m, &e)) { t_note("marshal of the stream failed"); um_buf_free(b); stm->lpVtbl->Release(stm); U(CoUninitialize)(); return; }
    CHECK(RD_U32(b, 0) == TY_ISTREAM);
    memset(&o, 0, sizeof o);
    um_init(&c, b, (ULONG)(e - b), C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e2), 0);
    CHECK(e2 == e);
    CHECKF(o.tymed == TY_ISTREAM && o.pstm != NULL, "interface unmarshalled");
    if (o.pstm) {
        LARGE_INTEGER z; char buf[16] = { 0 }; ULONG rd = 0;
        z.QuadPart = 0;
        o.pstm->lpVtbl->Seek(o.pstm, z, STREAM_SEEK_SET, NULL);
        o.pstm->lpVtbl->Read(o.pstm, buf, 10, &rd);
        CHECKF(rd == 10 && !strcmp(buf, "olet-gap9"), "data visible through the unmarshalled pointer (rd=%lu)", rd);
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(ff, UMF(&c), &o), 0);            /* ReleaseStgMedium: releases the interface */
    }
    refs = stm->lpVtbl->Release(stm);
    CHECKF(refs == 0, "all references released (remaining %lu)", refs);
    um_buf_free(b);
    U(CoUninitialize)();
}

TEST(gap9_stgmedium_unmarshal_istream_null_u_marker_is_noop_payload)
{
    void *fu = sfn("Unmarshal");
    static const DWORD tys[] = { TY_ISTREAM, TY_ISTORAGE };
    unsigned i;
    REQUIRE(fu);
    for (i = 0; i < 2; i++) {
        unsigned char *b = um_buf_new(40), *e; UMC c; STGMEDIUM o;
        W32(b, 0, tys[i]); W32(b, 4, 0); W32(b, 8, 0);
        memset(&o, 0, sizeof o);
        um_init(&c, b, 12, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
        CHECK(e == b + 12 && o.tymed == tys[i] && o.pstm == NULL);
        um_buf_free(b);
    }
}
