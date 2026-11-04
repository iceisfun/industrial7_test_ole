/* IPicture::SaveAsFile, IPersistStream Save/Load/GetSizeMax ("lt\0\0" wrapper), round trips with pixel
 * verification, OleSavePictureFile, OleLoadPicturePath, OleLoadPictureFile(Ex). */
#include "pf.h"

static IStream *newstream(void) { IStream *s = NULL; CreateStreamOnHGlobal(NULL, TRUE, &s); return s; }
static size_t bytes_of(IStream *s, BYTE *buf, size_t cap) {
    LARGE_INTEGER z; ULONG got = 0; STATSTG st;
    z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
    memset(&st, 0, sizeof st);
    if (FAILED(IStream_Stat(s, &st, STATFLAG_NONAME)) || st.cbSize.QuadPart > cap) return 0;
    IStream_Read(s, buf, (ULONG)st.cbSize.QuadPart, &got);
    return got;
}
static void rewind_s(IStream *s) { LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL); }
static IPicture *new_bitmap_pic(int w, int h, int bpp, const COLORREF *px, HBITMAP *bm_out) {
    HBITMAP bm = bpp == 1 ? CreateBitmap(w, h, 1, 1, NULL) : pf_dib(w, h, bpp, px, NULL);
    *bm_out = bm;
    return bm ? pf_pic_from_bitmap(bm, NULL, FALSE) : NULL;
}
static HRESULT save_as_file(IPicture *p, BOOL copy, LONG *size, BYTE *buf, size_t cap, size_t *got) {
    IStream *s = newstream(); HRESULT hr;
    *got = 0;
    if (!s) return E_OUTOFMEMORY;
    hr = IPicture_SaveAsFile(p, s, copy, size);
    *got = bytes_of(s, buf, cap);
    IStream_Release(s);
    return hr;
}

/* ---- bitmap ---------------------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_bitmap_1bpp_saveasfile) {
    HBITMAP bm; IPicture *p; LONG size = -1; BYTE buf[256]; size_t n; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    hr = save_as_file(p, TRUE, &size, buf, sizeof buf, &n);
    EQ(hr, S_OK); EQ(size, 66); EQ(n, 66);
    if (n >= 2) CHECK(!memcmp(buf, "BM", 2));
    if (n >= 14) { EQ(*(DWORD *)(buf + 2), 66); EQ(*(DWORD *)(buf + 10), 62); }
    IPicture_Release(p); DeleteObject(bm);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_bitmap_no_memcopy_fails) {
    HBITMAP bm; IPicture *p; LONG size = -1; BYTE buf[256]; size_t n; REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    EQ(save_as_file(p, FALSE, &size, buf, sizeof buf, &n), PF_E_FAIL);
    EQ(size, -1);
    IPicture_Release(p); DeleteObject(bm);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_dib_bpp_table) {
    unsigned bpp; REQUIRE(U(OleCreatePictureIndirect));
    for (bpp = 4; bpp <= 32; bpp <<= 1) {
        char buffer[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256]; BITMAPINFO *info = (BITMAPINFO *)buffer;
        void *bits; HBITMAP bm; IPicture *p; PICTDESC d; LONG size = -1; BYTE buf[2048]; size_t n; DWORD expect_size, expect_bpp;
        expect_size = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (bpp <= 8 ? sizeof(RGBQUAD) * (1u << bpp) : 0) + sizeof(DWORD);
        expect_bpp = bpp <= 8 ? bpp : 24;
        memset(buffer, 0, sizeof buffer);
        info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info->bmiHeader.biWidth = 1; info->bmiHeader.biHeight = 1;
        info->bmiHeader.biPlanes = 1; info->bmiHeader.biBitCount = (WORD)bpp; info->bmiHeader.biCompression = BI_RGB;
        bm = CreateDIBSection(NULL, info, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!bm) { t_skip("CreateDIBSection %u bpp failed", bpp); continue; }
        memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_BITMAP; d.bmp.hbitmap = bm;
        p = NULL;
        EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
        if (!p) continue;
        EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
        CHECKF(size == (LONG)expect_size, "bpp %u: size %ld want %lu", bpp, size, expect_size);
        CHECKF(n == expect_size, "bpp %u: stream bytes %zu", bpp, n);
        if (n >= 54) {
            CHECK(!memcmp(buf, "BM", 2));
            CHECKF(*(WORD *)(buf + 14 + 14) == expect_bpp, "bpp %u: saved bit count %u want %lu", bpp, *(WORD *)(buf + 28), expect_bpp);
            EQ(*(DWORD *)(buf + 14 + 16), BI_RGB);
        }
        IPicture_Release(p);   /* fOwn: bitmap destroyed with the picture */
    }
}
TEST(psavepic_bitmap_2x2_32bpp_saved_as_24) {
    static const COLORREF c[4] = { RGB(1,2,3), RGB(4,5,6), RGB(7,8,9), RGB(10,11,12) };
    HBITMAP bm; IPicture *p; LONG size = -1; BYTE buf[256]; size_t n; REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(2, 2, 32, c, &bm);
    if (!p) { t_skip("no DIB section"); if (bm) DeleteObject(bm); return; }
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
    EQ(size, 14 + 40 + 16);          /* 2 rows of 6 pixel bytes padded to 8 */
    EQ(n, 70);
    if (n == 70) {
        /* bottom row first: pixels (0,1),(1,1) as BGR, then padding */
        EQ(buf[54], 9); EQ(buf[55], 8); EQ(buf[56], 7);
        EQ(buf[57], 12); EQ(buf[58], 11); EQ(buf[59], 10);
        EQ(buf[62], 3); EQ(buf[63], 2); EQ(buf[64], 1);
        EQ(buf[65], 6); EQ(buf[66], 5); EQ(buf[67], 4);
    }
    IPicture_Release(p); DeleteObject(bm);
}
TEST(psavepic_bitmap_roundtrip_pixels) {
    static const COLORREF c[4] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,255) };
    HBITMAP bm, old, dib; IPicture *p, *q = NULL; BYTE buf[256]; size_t n; LONG size = -1; HDC dc;
    OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0; REQUIRE(U(OleCreatePictureIndirect)); REQUIRE(U(OleLoadPicture));
    p = new_bitmap_pic(2, 2, 32, c, &bm);
    if (!p) { t_skip("no DIB section"); if (bm) DeleteObject(bm); return; }
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
    EQ(pf_load(buf, n, (LONG)n, &q), S_OK);
    dc = pf_render_dc(4, 4, &old, &dib);
    if (q && dc) {
        short ty = -1;
        EQ(IPicture_get_Type(q, &ty), S_OK); EQ(ty, PICTYPE_BITMAP);
        EQ(IPicture_get_Width(q, &w), S_OK); EQ(IPicture_get_Height(q, &h), S_OK);
        EQ(IPicture_Render(q, dc, 0, 0, 2, 2, 0, h, w, -h, NULL), S_OK);
        EQ(GetPixel(dc, 0, 0) & 0xffffff, c[0]); EQ(GetPixel(dc, 1, 0) & 0xffffff, c[1]);
        EQ(GetPixel(dc, 0, 1) & 0xffffff, c[2]); EQ(GetPixel(dc, 1, 1) & 0xffffff, c[3]);
    }
    if (dc) pf_render_dc_free(dc, old, dib);
    if (q) IPicture_Release(q);
    IPicture_Release(p); DeleteObject(bm);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_persiststream_bitmap) {
    HBITMAP bm; IPicture *p; IPersistStream *ps = NULL; IStream *s; ULARGE_INTEGER mx; BYTE buf[256]; size_t n; REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    EQ(IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps), S_OK);
    s = newstream();
    if (ps && s) {
        mx.QuadPart = 0; EQ(IPersistStream_GetSizeMax(ps, &mx), S_OK); EQ(mx.QuadPart, 74);
        EQ(IPersistStream_Save(ps, s, TRUE), S_OK);
        mx.QuadPart = 0; EQ(IPersistStream_GetSizeMax(ps, &mx), S_OK); EQ(mx.QuadPart, 74);
        n = bytes_of(s, buf, sizeof buf);
        EQ(n, 74);
        if (n == 74) { CHECK(!memcmp(buf, "lt\0\0", 4)); EQ(*(DWORD *)(buf + 4), 66); CHECK(!memcmp(buf + 8, "BM", 2)); }
        EQ(IPersistStream_GetSizeMax(ps, NULL), PF_E_INVALIDARG);
    } else CHECK(0);
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(psavepic_persiststream_save_leaves_position_at_end) {
    HBITMAP bm; IPicture *p; IPersistStream *ps = NULL; IStream *s; LARGE_INTEGER z; ULARGE_INTEGER pos; REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    s = newstream();
    if (ps && s) {
        EQ(IPersistStream_Save(ps, s, FALSE), S_OK);
        z.QuadPart = 0; pos.QuadPart = 0;
        EQ(IStream_Seek(s, z, STREAM_SEEK_CUR, &pos), S_OK);
        EQ(pos.QuadPart, 74);
    }
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(psavepic_persiststream_load_into_existing_picture) {
    HBITMAP bm; IPicture *p, *q = NULL; IPersistStream *ps = NULL, *qs = NULL; IStream *s; short ty = -1; OLE_HANDLE h = 0; HRESULT hr;
    REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(3, 2, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&q), S_OK);
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    if (q) IPicture_QueryInterface(q, &IID_IPersistStream, (void **)&qs);
    s = newstream();
    if (ps && qs && s) {
        EQ(IPersistStream_Save(ps, s, FALSE), S_OK);
        rewind_s(s);
        hr = IPersistStream_Load(qs, s);
        EQ(hr, S_OK);
        EQ(IPicture_get_Type(q, &ty), S_OK); EQ(ty, PICTYPE_BITMAP);
        EQ(IPicture_get_Handle(q, &h), S_OK); CHECK(h != 0);
        { OLE_XSIZE_HIMETRIC a = 0, b = 1; IPicture_get_Width(p, &a); IPicture_get_Width(q, &b); EQ(a, b); }
    } else CHECK(0);
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    if (qs) IPersistStream_Release(qs);
    if (q) IPicture_Release(q);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(psavepic_persiststream_load_bad_header) {
    static const BYTE bad[] = { 'x', 'y', 0, 0, 4, 0, 0, 0, 1, 2, 3, 4 };
    IPicture *q = NULL; IPersistStream *qs = NULL; IStream *s; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&q), S_OK);
    if (!q) return;
    IPicture_QueryInterface(q, &IID_IPersistStream, (void **)&qs);
    s = pf_mem_stream(bad, sizeof bad);
    if (qs && s) {
        hr = IPersistStream_Load(qs, s);
        CHECKF(FAILED(hr), "bad magic accepted: %08lx", hr);
        t_note("IPersistStream::Load with bad magic -> %08lx", hr);
    }
    if (s) IStream_Release(s);
    if (qs) IPersistStream_Release(qs);
    IPicture_Release(q);
}
TEST(psavepic_persiststream_load_empty_stream) {
    IPicture *q = NULL; IPersistStream *qs = NULL; IStream *s; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&q), S_OK);
    if (!q) return;
    IPicture_QueryInterface(q, &IID_IPersistStream, (void **)&qs);
    s = newstream();
    if (qs && s) {
        hr = IPersistStream_Load(qs, s);
        CHECKF(FAILED(hr), "empty stream accepted: %08lx", hr);
        t_note("IPersistStream::Load(empty) -> %08lx", hr);
    }
    if (s) IStream_Release(s);
    if (qs) IPersistStream_Release(qs);
    IPicture_Release(q);
}
TEST(psavepic_persiststream_isdirty) {
    HBITMAP bm; IPicture *p; IPersistStream *ps = NULL; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    if (ps) {
        hr = IPersistStream_IsDirty(ps);
        t_note("IsDirty hr=%08lx", hr);
        IPersistStream_Release(ps);
    }
    IPicture_Release(p); DeleteObject(bm);
}

/* ---- icon ------------------------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_icon_header) {
    PICTDESC d; IPicture *p = NULL; LONG size = -1; BYTE buf[8192]; size_t n; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ICON; d.icon.hicon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    if (!d.icon.hicon) { t_skip("no stock icon"); return; }
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (!p) return;
    hr = save_as_file(p, TRUE, &size, buf, sizeof buf, &n);
    EQ(hr, S_OK);
    t_note("system application icon saved size %ld (stream bytes %zu)", size, n);
    CHECK(size > 22 && (size_t)size == n);
    if (n >= 6) { EQ(*(DWORD *)buf, 0x00010000); CHECK(*(WORD *)(buf + 4) >= 1); }
    size = -1;
    EQ(save_as_file(p, FALSE, &size, buf, sizeof buf, &n), PF_E_FAIL);
    EQ(size, -1);
    IPicture_Release(p);
}
TEST(psavepic_icon_persiststream_layout) {
    PICTDESC d; IPicture *p = NULL; IPersistStream *ps = NULL; IStream *s; BYTE buf[8192]; size_t n; ULARGE_INTEGER mx; REQUIRE(U(OleCreatePictureIndirect));
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ICON; d.icon.hicon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    if (!d.icon.hicon) { t_skip("no stock icon"); return; }
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (!p) return;
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    s = newstream();
    if (ps && s) {
        EQ(IPersistStream_Save(ps, s, FALSE), S_OK);
        n = bytes_of(s, buf, sizeof buf);
        CHECK(n > 30);
        if (n > 12) {
            CHECK(!memcmp(buf, "lt\0\0", 4));
            EQ(*(DWORD *)(buf + 4), n - 8);
            EQ(*(DWORD *)(buf + 8), 0x00010000);
        }
        mx.QuadPart = 0; EQ(IPersistStream_GetSizeMax(ps, &mx), S_OK);
        EQ(mx.QuadPart, n);
    }
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IPicture_Release(p);
}
TEST(psavepic_icon_roundtrip) {
    BYTE ico[1200]; size_t isz = pf_build_ico(ico, 1, 16, 16); IPicture *p = NULL, *q = NULL; LONG size = -1; BYTE buf[4096]; size_t n; short ty = -1;
    REQUIRE(U(OleLoadPicture));
    EQ(pf_load(ico, isz, (LONG)isz, &p), S_OK);
    if (!p) return;
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
    CHECK(n > 22);
    if (n >= 6) { EQ(*(WORD *)buf, 0); EQ(*(WORD *)(buf + 2), 1); EQ(*(WORD *)(buf + 4), 1); }
    EQ(pf_load(buf, n, (LONG)n, &q), S_OK);
    if (q) { EQ(IPicture_get_Type(q, &ty), S_OK); EQ(ty, PICTYPE_ICON); IPicture_Release(q); }
    IPicture_Release(p);
}

/* ---- empty / uninitialized ------------------------------------------------------------------ */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_empty_picture) {
    PICTDESC d; IPicture *p = NULL; LONG size = -1; BYTE buf[64]; size_t n; IPersistStream *ps = NULL; IStream *s; ULARGE_INTEGER mx;
    REQUIRE(U(OleCreatePictureIndirect));
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_NONE;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (!p) return;
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK); EQ(size, -1); EQ(n, 0);
    size = -1;
    EQ(save_as_file(p, FALSE, &size, buf, sizeof buf, &n), S_OK); EQ(size, -1);
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    s = newstream();
    if (ps && s) {
        mx.QuadPart = 0; EQ(IPersistStream_GetSizeMax(ps, &mx), S_OK); EQ(mx.QuadPart, 8);
        EQ(IPersistStream_GetSizeMax(ps, NULL), PF_E_INVALIDARG);
        EQ(IPersistStream_Save(ps, s, TRUE), S_OK);
        n = bytes_of(s, buf, sizeof buf);
        EQ(n, 8);
        if (n == 8) { CHECK(!memcmp(buf, "lt\0\0", 4)); EQ(*(DWORD *)(buf + 4), 0); }
        rewind_s(s);
        { IPicture *q = NULL; short ty = -1; OLE_HANDLE h = 0xdeadbeef;
          EQ(U(OleLoadPicture)(s, 0, FALSE, &IID_IPicture, (void **)&q), S_OK);
          if (q) { EQ(IPicture_get_Type(q, &ty), S_OK); EQ(ty, PICTYPE_NONE); EQ(IPicture_get_Handle(q, &h), S_OK); EQ(h, 0); IPicture_Release(q); } }
    }
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IPicture_Release(p);
}
TEST(psavepic_uninitialized_picture) {
    IPicture *p = NULL; LONG size = -1; BYTE buf[64]; size_t n; HRESULT hr; IPersistStream *ps = NULL; ULARGE_INTEGER mx;
    REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    hr = save_as_file(p, TRUE, &size, buf, sizeof buf, &n);
    t_note("SaveAsFile on PICTYPE_UNINITIALIZED hr=%08lx size=%ld bytes=%zu", hr, size, n);
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    if (ps) { mx.QuadPart = 99; hr = IPersistStream_GetSizeMax(ps, &mx); t_note("GetSizeMax hr=%08lx %llu", hr, (unsigned long long)mx.QuadPart); IPersistStream_Release(ps); }
    IPicture_Release(p);
}

/* ---- enhanced metafile / metafile ----------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(psavepic_emf) {
    HDC dc; PICTDESC d; IPicture *p = NULL; LONG size = -1; BYTE buf[512]; size_t n; IPersistStream *ps = NULL; IStream *s;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateEnhMetaFileA(0, NULL, NULL, NULL);
    if (!dc) { t_skip("no GDI"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ENHMETAFILE; d.emf.hemf = CloseEnhMetaFile(dc);
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
    EQ(size, 128); EQ(n, 128);
    if (n >= 44) { EQ(*(DWORD *)buf, EMR_HEADER); EQ(*(DWORD *)(buf + 40), ENHMETA_SIGNATURE); }
    size = -1;
    EQ(save_as_file(p, FALSE, &size, buf, sizeof buf, &n), PF_E_FAIL);
    EQ(size, -1);
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    s = newstream();
    if (ps && s) {
        EQ(IPersistStream_Save(ps, s, TRUE), S_OK);
        n = bytes_of(s, buf, sizeof buf);
        EQ(n, 136);
        if (n == 136) { CHECK(!memcmp(buf, "lt\0\0", 4)); EQ(*(DWORD *)(buf + 4), 128); EQ(*(DWORD *)(buf + 8), EMR_HEADER); }
    }
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IPicture_Release(p);
}
TEST(psavepic_emf_roundtrip) {
    HDC dc; PICTDESC d; IPicture *p = NULL, *q = NULL; LONG size = -1; BYTE buf[512]; size_t n; short ty = -1; HBRUSH br; RECT r = { 0, 0, 10, 10 }; REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateEnhMetaFileA(0, NULL, NULL, NULL);
    if (!dc) { t_skip("no GDI"); return; }
    br = CreateSolidBrush(RGB(10, 20, 30)); FillRect(dc, &r, br); DeleteObject(br);
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ENHMETAFILE; d.emf.hemf = CloseEnhMetaFile(dc);
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
    CHECK(n > 128 && (size_t)size == n);
    EQ(pf_load(buf, n, (LONG)n, &q), S_OK);
    if (q) { EQ(IPicture_get_Type(q, &ty), S_OK); EQ(ty, PICTYPE_ENHMETAFILE); IPicture_Release(q); }
    IPicture_Release(p);
}
TEST(psavepic_wmf_observed) {
    HDC dc; PICTDESC d; IPicture *p = NULL; LONG size = -1; BYTE buf[512]; size_t n; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateMetaFileA(NULL);
    if (!dc) { t_skip("no GDI"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_METAFILE; d.wmf.hmeta = CloseMetaFile(dc); d.wmf.xExt = 100; d.wmf.yExt = 100;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    hr = save_as_file(p, TRUE, &size, buf, sizeof buf, &n);
    t_note("SaveAsFile(PICTYPE_METAFILE) hr=%08lx size=%ld bytes=%zu first dword=%08lx", hr, size, n, n >= 4 ? *(DWORD *)buf : 0);
    if (hr == S_OK && n >= 4) SOFT_EQ(*(DWORD *)buf, 0x9ac6cdd7);   /* placeable metafile key */
    IPicture_Release(p);
}

/* ---- KeepOriginalFormat ------------------------------------------------------------------------ */
TEST(psavepic_loaded_gif_default_save_format) {
    IPicture *p = NULL; LONG size = -1; BYTE buf[512]; size_t n; BOOL keep = 99; REQUIRE(U(OleLoadPicture));
    EQ(pf_load(pfd_gifimage, sizeof pfd_gifimage, sizeof pfd_gifimage, &p), S_OK);
    if (!p) return;
    EQ(IPicture_get_KeepOriginalFormat(p, &keep), S_OK);
    t_note("KeepOriginalFormat after loading GIF: %d", keep);
    EQ(save_as_file(p, TRUE, &size, buf, sizeof buf, &n), S_OK);
    CHECK(n > 0 && (size_t)size == n);
    if (n >= 4) t_note("GIF saved without KeepOriginalFormat starts with %.2s (BM expected for re-encoded DIB)", (char *)buf);
    IPicture_Release(p);
}
TEST(psavepic_keep_original_format_gif) {
    IPicture *p = NULL; LONG size = -1; BYTE buf[512]; size_t n; HRESULT hr; REQUIRE(U(OleLoadPicture));
    EQ(pf_load(pfd_gifimage, sizeof pfd_gifimage, sizeof pfd_gifimage, &p), S_OK);
    if (!p) return;
    /* Win10: put_KeepOriginalFormat is not supported (E_FAIL) and the image is always saved as a DIB */
    EQ(IPicture_put_KeepOriginalFormat(p, TRUE), E_FAIL);
    hr = save_as_file(p, TRUE, &size, buf, sizeof buf, &n);
    EQ(hr, S_OK);
    t_note("GIF with KeepOriginalFormat saved size=%ld bytes=%zu starts %.2s", size, n, n >= 2 ? (char *)buf : "");
    if (n >= 2) CHECK(!memcmp(buf, "BM", 2));
    IPicture_Release(p);
}
TEST(psavepic_keep_original_format_jpeg) {
    IPicture *p = NULL; LONG size = -1; BYTE buf[1024]; size_t n; HRESULT hr; REQUIRE(U(OleLoadPicture));
    EQ(pf_load(pfd_jpgimage, sizeof pfd_jpgimage, sizeof pfd_jpgimage, &p), S_OK);
    if (!p) return;
    /* Win10: put_KeepOriginalFormat is not supported (E_FAIL) and the image is always saved as a DIB */
    EQ(IPicture_put_KeepOriginalFormat(p, TRUE), E_FAIL);
    hr = save_as_file(p, TRUE, &size, buf, sizeof buf, &n);
    EQ(hr, S_OK);
    t_note("JPEG with KeepOriginalFormat saved size=%ld (original %zu)", size, sizeof pfd_jpgimage);
    if (n >= 2) CHECK(!memcmp(buf, "BM", 2));
    IPicture_Release(p);
}
TEST(psavepic_persist_stream_of_loaded_gif) {
    IPicture *p = NULL; IPersistStream *ps = NULL; IStream *s; BYTE buf[1024]; size_t n; REQUIRE(U(OleLoadPicture));
    EQ(pf_load(pfd_gifimage, sizeof pfd_gifimage, sizeof pfd_gifimage, &p), S_OK);
    if (!p) return;
    IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps);
    s = newstream();
    if (ps && s) {
        EQ(IPersistStream_Save(ps, s, FALSE), S_OK);
        n = bytes_of(s, buf, sizeof buf);
        CHECK(n > 8);
        if (n > 8) { CHECK(!memcmp(buf, "lt\0\0", 4)); EQ(*(DWORD *)(buf + 4), n - 8); }
    }
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IPicture_Release(p);
}

/* ---- files ------------------------------------------------------------------------------------ */
static void write_file(const char *path, const void *d, size_t n) {
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); DWORD w;
    if (f != INVALID_HANDLE_VALUE) { WriteFile(f, d, (DWORD)n, &w, NULL); CloseHandle(f); }
}
static void widen(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }

/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpath_invalid_parameters) {
    static WCHAR emptyW[] = { 0 };
    const struct { LPOLESTR path; const GUID *iid; int has_out; } t[] = {
        { NULL, NULL, 0 }, { NULL, NULL, 1 }, { NULL, &IID_IPicture, 0 }, { NULL, &IID_IPicture, 1 },
        { emptyW, NULL, 0 }, { emptyW, &IID_IPicture, 0 },
    };
    int i; REQUIRE(U(OleLoadPicturePath));
    for (i = 0; i < PF_N(t); i++) {
        IPicture *p = (IPicture *)(ULONG_PTR)0xdeadbeef;
        HRESULT hr = U(OleLoadPicturePath)(t[i].path, NULL, 0, 0, t[i].iid, t[i].has_out ? (void **)&p : NULL);
        CHECKF(hr == PF_E_INVALIDARG, "case %d hr=%08lx", i, hr);
        CHECKF(p == (IPicture *)(ULONG_PTR)0xdeadbeef, "case %d: output modified", i);
    }
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpath_empty_path_unknown_protocol) {
    static WCHAR emptyW[] = { 0 };
    IPicture *p = (IPicture *)(ULONG_PTR)0xdeadbeef; HRESULT hr; REQUIRE(U(OleLoadPicturePath));
    hr = U(OleLoadPicturePath)(emptyW, NULL, 0, 0, NULL, (void **)&p);
    EQ(hr, PF_INET_E_UNKNOWN_PROTOCOL); EQ(p, NULL);
    p = (IPicture *)(ULONG_PTR)0xdeadbeef;
    hr = U(OleLoadPicturePath)(emptyW, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, PF_INET_E_UNKNOWN_PROTOCOL); EQ(p, NULL);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpath_file_variants) {
    char path[MAX_PATH]; WCHAR w[MAX_PATH + 16], url[MAX_PATH + 16], *q; IPicture *p = NULL; HRESULT hr; short ty = -1;
    REQUIRE(U(OleLoadPicturePath));
    t_tmpfile(path, sizeof path, "pf_path.bmp");
    write_file(path, pfd_bmpimage, sizeof pfd_bmpimage);
    widen(path, w, MAX_PATH);
    hr = U(OleLoadPicturePath)(w, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, S_OK);
    if (p) { EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_BITMAP); IPicture_Release(p); p = NULL; }
    lstrcpyW(url, L"file:///"); lstrcatW(url, w);
    hr = U(OleLoadPicturePath)(url, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, S_OK);
    if (p) { IPicture_Release(p); p = NULL; }
    for (q = url + 8; *q; q++) if (*q == '\\') *q = '/';
    hr = U(OleLoadPicturePath)(url, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, S_OK);
    if (p) { IPicture_Release(p); p = NULL; }
    hr = U(OleLoadPicturePath)(w, NULL, 0, 0, &IID_IPictureDisp, (void **)&p);
    EQ(hr, S_OK);
    if (p) IPicture_Release(p);
    DeleteFileA(path);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpath_missing_file) {
    char path[MAX_PATH]; WCHAR w[MAX_PATH + 16], url[MAX_PATH + 16]; IPicture *p = NULL; HRESULT hr; REQUIRE(U(OleLoadPicturePath));
    t_tmpfile(path, sizeof path, "pf_missing.bmp");
    DeleteFileA(path);
    widen(path, w, MAX_PATH);
    hr = U(OleLoadPicturePath)(w, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, PF_INET_E_RESOURCE_NOT_FOUND);
    lstrcpyW(url, L"file:///"); lstrcatW(url, w);
    hr = U(OleLoadPicturePath)(url, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, PF_INET_E_RESOURCE_NOT_FOUND);
}
TEST(ploadpath_ico_and_garbage_files) {
    char path[MAX_PATH]; WCHAR w[MAX_PATH + 16]; BYTE ico[1200]; size_t n = pf_build_ico(ico, 1, 16, 16); IPicture *p = NULL; HRESULT hr; short ty = -1;
    BYTE junk[64]; int i; REQUIRE(U(OleLoadPicturePath));
    t_tmpfile(path, sizeof path, "pf_icon.ico"); write_file(path, ico, n); widen(path, w, MAX_PATH);
    hr = U(OleLoadPicturePath)(w, NULL, 0, 0, &IID_IPicture, (void **)&p);
    EQ(hr, S_OK);
    if (p) { EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_ICON); IPicture_Release(p); p = NULL; }
    DeleteFileA(path);
    for (i = 0; i < 64; i++) junk[i] = (BYTE)(i * 29 + 3);
    t_tmpfile(path, sizeof path, "pf_junk.bmp"); write_file(path, junk, sizeof junk); widen(path, w, MAX_PATH);
    hr = U(OleLoadPicturePath)(w, NULL, 0, 0, &IID_IPicture, (void **)&p);
    CHECKF(FAILED(hr), "garbage file accepted: %08lx", hr);
    t_note("OleLoadPicturePath(garbage file) hr=%08lx", hr);
    if (SUCCEEDED(hr) && p) IPicture_Release(p);
    DeleteFileA(path);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadfile_variants) {
    char path[MAX_PATH]; WCHAR w[MAX_PATH + 16], url[MAX_PATH + 16]; VARIANT v; IDispatch *d = NULL; HRESULT hr; REQUIRE(U(OleLoadPictureFile));
    t_tmpfile(path, sizeof path, "pf_file.bmp");
    write_file(path, pfd_bmpimage, sizeof pfd_bmpimage);
    widen(path, w, MAX_PATH);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(w);
    hr = U(OleLoadPictureFile)(v, &d);
    EQ(hr, S_OK);
    if (d) {
        IPicture *pic = NULL; EQ(IDispatch_QueryInterface(d, &IID_IPicture, (void **)&pic), S_OK);
        if (pic) { short ty = -1; EQ(IPicture_get_Type(pic, &ty), S_OK); EQ(ty, PICTYPE_BITMAP); IPicture_Release(pic); }
        IDispatch_Release(d); d = NULL;
    }
    VariantClear(&v);
    /* "file:///" is not a plain path: reported as a path/file access error */
    lstrcpyW(url, L"file:///"); lstrcatW(url, w);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(url);
    hr = U(OleLoadPictureFile)(v, &d);
    EQ(hr, PF_CTL_E_PATHFILEACCESSERROR);
    VariantClear(&v);
    DeleteFileA(path);
    /* missing file */
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(w);
    hr = U(OleLoadPictureFile)(v, &d);
    EQ(hr, PF_CTL_E_FILENOTFOUND);
    VariantClear(&v);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(url);
    hr = U(OleLoadPictureFile)(v, &d);
    EQ(hr, PF_CTL_E_PATHFILEACCESSERROR);
    VariantClear(&v);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadfile_integer_variant_is_file_number) {
    VARIANT v; IDispatch *d = NULL; HRESULT hr; REQUIRE(U(OleLoadPictureFile));
    VariantInit(&v); V_VT(&v) = VT_INT; V_INT(&v) = 762;
    hr = U(OleLoadPictureFile)(v, &d);
    EQ(hr, PF_CTL_E_FILENOTFOUND);
    VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 762;
    hr = U(OleLoadPictureFile)(v, &d);
    t_note("OleLoadPictureFile(VT_I4 762) hr=%08lx", hr);
    VariantInit(&v);
    d = NULL;
    hr = U(OleLoadPictureFile)(v, &d);
    /* Win10: a VT_EMPTY file name is accepted and yields an (empty) picture object */
    CHECKF(hr == S_OK, "VT_EMPTY: hr=%08lx want S_OK", hr);
    t_note("OleLoadPictureFile(VT_EMPTY) hr=%08lx obj=%d", hr, d != NULL);
    if (d) IDispatch_Release(d);
}
TEST(ploadfile_empty_and_directory_names) {
    VARIANT v; IDispatch *d = NULL; HRESULT hr; char dir[MAX_PATH]; WCHAR w[MAX_PATH]; REQUIRE(U(OleLoadPictureFile));
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"");
    d = NULL;
    hr = U(OleLoadPictureFile)(v, &d);
    /* Win10: an empty file name is accepted like VT_EMPTY and yields an (empty) picture object */
    CHECKF(hr == S_OK, "empty path: hr=%08lx want S_OK", hr);
    t_note("OleLoadPictureFile(\"\") hr=%08lx obj=%d", hr, d != NULL);
    if (d) { IDispatch_Release(d); d = NULL; }
    VariantClear(&v);
    lstrcpyA(dir, t_tmpdir()); widen(dir, w, MAX_PATH);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(w);
    hr = U(OleLoadPictureFile)(v, &d);
    CHECKF(FAILED(hr), "directory accepted: %08lx", hr);
    t_note("OleLoadPictureFile(directory) hr=%08lx", hr);
    VariantClear(&v);
}
TEST(ploadfileex_sizes_and_flags) {
    char path[MAX_PATH]; WCHAR w[MAX_PATH + 16]; VARIANT v; IDispatch *d = NULL; HRESULT hr; REQUIRE(U(OleLoadPictureFileEx));
    if (t_is_wine()) { t_skip("OleLoadPictureFileEx is not implemented by the emulator"); return; }
    t_tmpfile(path, sizeof path, "pf_fileex.bmp");
    write_file(path, pfd_bmpimage24, sizeof pfd_bmpimage24);
    widen(path, w, MAX_PATH);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(w);
    hr = U(OleLoadPictureFileEx)(v, 0, 0, LP_DEFAULT, &d);
    EQ(hr, S_OK);
    if (d) IDispatch_Release(d);
    d = NULL;
    hr = U(OleLoadPictureFileEx)(v, 32, 32, LP_COLOR, &d);
    EQ(hr, S_OK);
    if (d) IDispatch_Release(d);
    VariantClear(&v);
    DeleteFileA(path);
}
TEST(psavefile_bitmap_to_file) {
    HBITMAP bm; IPicture *p; IDispatch *d = NULL; char path[MAX_PATH]; WCHAR w[MAX_PATH]; BSTR b; HRESULT hr; BYTE data[256]; DWORD got = 0; HANDLE f;
    REQUIRE(U(OleSavePictureFile)); REQUIRE(U(OleCreatePictureIndirect));
    if (t_is_wine()) { t_skip("emulator stubs OleSavePictureFile"); return; }
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (!p) { t_skip("no GDI bitmap"); if (bm) DeleteObject(bm); return; }
    IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
    t_tmpfile(path, sizeof path, "pf_saved.bmp"); DeleteFileA(path); widen(path, w, MAX_PATH);
    b = SysAllocString(w);
    hr = U(OleSavePictureFile)(d, b);
    EQ(hr, S_OK);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) {
        ReadFile(f, data, sizeof data, &got, NULL); CloseHandle(f);
        EQ(got, 66);
        if (got >= 2) CHECK(!memcmp(data, "BM", 2));
    }
    SysFreeString(b);
    DeleteFileA(path);
    if (d) IDispatch_Release(d);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(psavefile_invalid_arguments) {
    HBITMAP bm; IPicture *p; IDispatch *d = NULL; BSTR b; HRESULT hr; REQUIRE(U(OleSavePictureFile)); REQUIRE(U(OleCreatePictureIndirect));
    if (t_is_wine()) { t_skip("emulator stubs OleSavePictureFile"); return; }
    b = SysAllocString(L"");
    hr = U(OleSavePictureFile)(NULL, b);
    CHECKF(FAILED(hr), "NULL picture accepted: %08lx", hr);
    t_note("OleSavePictureFile(NULL, \"\") hr=%08lx", hr);
    SysFreeString(b);
    p = new_bitmap_pic(1, 1, 1, NULL, &bm);
    if (p) {
        IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
        hr = U(OleSavePictureFile)(d, NULL);
        CHECKF(FAILED(hr), "NULL file name accepted: %08lx", hr);
        t_note("OleSavePictureFile(pic, NULL) hr=%08lx", hr);
        b = SysAllocString(L"Z:\\no\\such\\dir\\x.bmp");
        hr = U(OleSavePictureFile)(d, b);
        CHECKF(FAILED(hr), "unwritable path accepted: %08lx", hr);
        t_note("OleSavePictureFile(pic, bad dir) hr=%08lx", hr);
        SysFreeString(b);
        if (d) IDispatch_Release(d);
        IPicture_Release(p); DeleteObject(bm);
    }
}
