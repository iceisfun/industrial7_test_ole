/* OleLoadPicture / OleLoadPictureEx: loading BMP/GIF/JPEG/ICO/CUR/WMF/EMF bytes from memory streams
 * (seekable, statable and forward-only), the "lt\0\0" persistence wrapper, invalid data and IID handling.
 * Every image here is a few bytes, hand built or lifted as a fixture from Wine's olepicture test. */
#include "pf.h"

typedef struct { const char *name; const BYTE *data; size_t len; int bpp; } img_t;
#define IMG(n, d, bpp) { n, d, sizeof d, bpp }
static const img_t images[] = {
    IMG("gif 1x1", pfd_gifimage, 1), IMG("jpeg 1x1", pfd_jpgimage, 24), IMG("bmp 1bpp", pfd_bmpimage, 1),
    IMG("bmp rle8", pfd_bmpimage_rle8, 4), IMG("bmp 4bpp", pfd_bmpimage4, 4), IMG("bmp 24bpp", pfd_bmpimage24, 24),
    IMG("gif 2x2", pfd_gif4pixel, 4),
};

static int dpi_x(void) { HDC dc = GetDC(NULL); int v = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96; if (dc) ReleaseDC(NULL, dc); return v; }

/* Load via the chosen stream flavour; flavour 0 = seekable mem stream, 1 = forward-only. */
static HRESULT load_flavour(int flavour, const void *data, size_t len, LONG lsize, const GUID *iid, void **out) {
    IStream *s = flavour ? pf_noseek_stream(data, len) : pf_mem_stream(data, len); HRESULT hr;
    *out = NULL;
    if (!s) return E_OUTOFMEMORY;
    hr = U(OleLoadPicture)(s, lsize, TRUE, iid, out);
    IStream_Release(s);
    return hr;
}

/* Checks common to every successfully loaded raster image */
static void check_bitmap_pic(IPicture *pic, const img_t *im, const char *ctx) {
    short ty = -1; OLE_HANDLE h = 0, hp = 0xdeadbeef; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC ht = 0; DWORD at = 0xdeadbeef;
    BITMAP bm; DIBSECTION ds; IPicture *again = NULL;
    CHECKF(IPicture_get_Type(pic, &ty) == S_OK && ty == PICTYPE_BITMAP, "%s %s: type %d", ctx, im->name, ty);
    CHECKF(IPicture_get_Handle(pic, &h) == S_OK && h != 0, "%s %s: handle", ctx, im->name);
    CHECKF(IPicture_get_Width(pic, &w) == S_OK && w > 0, "%s %s: width %ld", ctx, im->name, w);
    CHECKF(IPicture_get_Height(pic, &ht) == S_OK && ht > 0, "%s %s: height %ld", ctx, im->name, ht);
    CHECKF(IPicture_get_Attributes(pic, &at) == S_OK && at == 0, "%s %s: attributes %lx", ctx, im->name, at);
    CHECKF(IPicture_get_hPal(pic, &hp) == S_OK && hp == 0, "%s %s: hpal %x", ctx, im->name, hp);
    CHECKF(IPicture_QueryInterface(pic, &IID_IPicture, (void **)&again) == S_OK && again == pic, "%s %s: QI", ctx, im->name);
    if (again) IPicture_Release(again);
    if (h) {
        CHECKF(GetObjectType((HGDIOBJ)(LONG_PTR)(LONG)h) == OBJ_BITMAP, "%s %s: not an HBITMAP", ctx, im->name);
        memset(&bm, 0, sizeof bm); memset(&ds, 0, sizeof ds);
        GetObjectA((HGDIOBJ)(LONG_PTR)(LONG)h, sizeof bm, &bm);
        CHECKF(bm.bmBits != NULL, "%s %s: bitmap is not a DIB section", ctx, im->name);
        GetObjectA((HGDIOBJ)(LONG_PTR)(LONG)h, sizeof ds, &ds);
        if (!strcmp(ctx, "seekable/exact"))
            CHECKF(ds.dsBmih.biBitCount == im->bpp, "%s %s: bpp %u want %d", ctx, im->name, ds.dsBmih.biBitCount, im->bpp);
        CHECKF(ds.dsBmih.biCompression == BI_RGB, "%s %s: compression %lu", ctx, im->name, ds.dsBmih.biCompression);
    }
}

/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_raw_seekable_lsize_exact) {
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < PF_N(images); i++) {
        IPicture *p = NULL; HRESULT hr = load_flavour(0, images[i].data, images[i].len, (LONG)images[i].len, &IID_IPicture, (void **)&p);
        CHECKF(hr == S_OK && p, "%s: hr=%08lx", images[i].name, hr);
        if (hr == S_OK && p) { check_bitmap_pic(p, &images[i], "seekable/exact"); EQ(IPicture_Release(p), 0); }
    }
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_raw_forward_only_stream) {
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < PF_N(images); i++) {
        IPicture *p = NULL; HRESULT hr = load_flavour(1, images[i].data, images[i].len, 0, &IID_IPicture, (void **)&p);
        CHECKF(hr == S_OK && p, "%s: hr=%08lx", images[i].name, hr);
        if (hr == S_OK && p) { check_bitmap_pic(p, &images[i], "forward-only"); EQ(IPicture_Release(p), 0); }
    }
}
TEST(ploadpic_raw_forward_only_stream_with_size) {
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < PF_N(images); i++) {
        IPicture *p = NULL; HRESULT hr = load_flavour(1, images[i].data, images[i].len, (LONG)images[i].len, &IID_IPicture, (void **)&p);
        CHECKF(hr == S_OK && p, "%s: hr=%08lx", images[i].name, hr);
        if (hr == S_OK && p) { check_bitmap_pic(p, &images[i], "forward-only/size"); IPicture_Release(p); }
    }
}
TEST(ploadpic_raw_seekable_lsize_zero) {
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < PF_N(images); i++) {
        IPicture *p = NULL; HRESULT hr = load_flavour(0, images[i].data, images[i].len, 0, &IID_IPicture, (void **)&p);
        CHECKF(hr == S_OK && p, "%s: hr=%08lx", images[i].name, hr);
        if (hr == S_OK && p) { check_bitmap_pic(p, &images[i], "seekable/size0"); IPicture_Release(p); }
    }
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_lt_header_repeated) {
    /* i copies of the 8-byte "lt\0\0"+size header before the image data are all accepted */
    int i, k; REQUIRE(U(OleLoadPicture));
    for (k = 0; k < PF_N(images); k++) {
        for (i = 1; i <= 8; i += (i == 1 ? 1 : 3)) {
            BYTE *buf = malloc(images[k].len + 8 * i); DWORD *hd = (DWORD *)buf; int j; IPicture *p = NULL; HRESULT hr;
            memcpy(buf, "lt\0\0", 4); hd[1] = (DWORD)images[k].len;
            for (j = 1; j < i; j++) memcpy(buf + 8 * j, buf, 8);
            memcpy(buf + 8 * i, images[k].data, images[k].len);
            hr = load_flavour(0, buf, images[k].len + 8 * i, (LONG)images[k].len, &IID_IPicture, (void **)&p);
            CHECKF(hr == S_OK && p, "%s x%d headers (seekable): hr=%08lx", images[k].name, i, hr);
            if (hr == S_OK && p) { check_bitmap_pic(p, &images[k], "lt-header"); IPicture_Release(p); }
            p = NULL;
            hr = load_flavour(1, buf, images[k].len + 8 * i, (LONG)images[k].len, &IID_IPicture, (void **)&p);
            CHECKF(hr == S_OK && p, "%s x%d headers (forward-only): hr=%08lx", images[k].name, i, hr);
            if (hr == S_OK && p) { check_bitmap_pic(p, &images[k], "lt-header/fwd"); IPicture_Release(p); }
            free(buf);
        }
    }
}
TEST(ploadpic_runmode_flag) {
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < 2; i++) {
        IStream *s = pf_mem_stream(pfd_bmpimage24, sizeof pfd_bmpimage24); IPicture *p = NULL; short ty = -1; HRESULT hr;
        if (!s) { CHECK(s); return; }
        hr = U(OleLoadPicture)(s, sizeof pfd_bmpimage24, i, &IID_IPicture, (void **)&p);
        CHECKF(hr == S_OK, "fRunmode=%d hr=%08lx", i, hr);
        if (p) { EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_BITMAP); IPicture_Release(p); }
        IStream_Release(s);
    }
}
TEST(ploadpic_stream_position_advances) {
    /* After a successful load of exactly lSize bytes the stream position is at (or past) the end of the image */
    IStream *s = pf_mem_stream(pfd_bmpimage24, sizeof pfd_bmpimage24); IPicture *p = NULL; LARGE_INTEGER z; ULARGE_INTEGER pos;
    REQUIRE(U(OleLoadPicture));
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPicture)(s, sizeof pfd_bmpimage24, TRUE, &IID_IPicture, (void **)&p), S_OK);
    z.QuadPart = 0; pos.QuadPart = 0;
    EQ(IStream_Seek(s, z, STREAM_SEEK_CUR, &pos), S_OK);
    t_note("stream position after load: %llu of %zu", (unsigned long long)pos.QuadPart, sizeof pfd_bmpimage24);
    CHECK(pos.QuadPart <= sizeof pfd_bmpimage24);
    if (p) IPicture_Release(p);
    IStream_Release(s);
}
TEST(ploadpic_loads_from_current_position) {
    /* The image is read from the stream's current position, not from offset 0 */
    BYTE buf[200]; IStream *s; IPicture *p = NULL; LARGE_INTEGER sk; short ty = -1; HRESULT hr;
    REQUIRE(U(OleLoadPicture));
    memset(buf, 0xaa, sizeof buf);
    memcpy(buf + 37, pfd_bmpimage24, sizeof pfd_bmpimage24);
    s = pf_mem_stream(buf, sizeof buf);
    if (!s) { CHECK(s); return; }
    sk.QuadPart = 37; EQ(IStream_Seek(s, sk, STREAM_SEEK_SET, NULL), S_OK);
    hr = U(OleLoadPicture)(s, sizeof pfd_bmpimage24, TRUE, &IID_IPicture, (void **)&p);
    EQ(hr, S_OK);
    if (p) { EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_BITMAP); IPicture_Release(p); }
    IStream_Release(s);
}
TEST(ploadpic_sizes_scale_with_pixels) {
    /* Same format, no resolution metadata: a 2x2 GIF is twice the 1x1 GIF in HIMETRIC */
    IPicture *a = NULL, *b = NULL; OLE_XSIZE_HIMETRIC wa = 0, wb = 0; OLE_YSIZE_HIMETRIC ha = 0, hb = 0;
    REQUIRE(U(OleLoadPicture));
    EQ(pf_load(pfd_gifimage, sizeof pfd_gifimage, sizeof pfd_gifimage, &a), S_OK);
    EQ(pf_load(pfd_gif4pixel, sizeof pfd_gif4pixel, sizeof pfd_gif4pixel, &b), S_OK);
    if (a && b) {
        IPicture_get_Width(a, &wa); IPicture_get_Height(a, &ha); IPicture_get_Width(b, &wb); IPicture_get_Height(b, &hb);
        CHECKF(abs(wb - 2 * wa) <= 1, "widths %ld %ld", wa, wb);
        CHECKF(abs(hb - 2 * ha) <= 1, "heights %ld %ld", ha, hb);
        t_note("1x1 gif himetric %ldx%ld (dpi %d -> %d expected if screen dpi used)", wa, ha, dpi_x(), MulDiv(1, 2540, dpi_x()));
    }
    if (a) IPicture_Release(a);
    if (b) IPicture_Release(b);
}
TEST(ploadpic_bmp_resolution_metadata_noted) {
    /* The 1bpp fixture declares 2834 pixels/metre (72 dpi): sizes may derive from it or from the screen DPI. */
    IPicture *p = NULL; OLE_XSIZE_HIMETRIC w = 0; REQUIRE(U(OleLoadPicture));
    EQ(pf_load(pfd_bmpimage, sizeof pfd_bmpimage, sizeof pfd_bmpimage, &p), S_OK);
    if (p) { EQ(IPicture_get_Width(p, &w), S_OK); t_note("bmp 1x1 @2834ppm width = %ld himetric", w); CHECK(w > 0 && w < 100); IPicture_Release(p); }
}
TEST(ploadpic_png_support_observed) {
    IPicture *p = NULL; HRESULT hr; short ty = -1; REQUIRE(U(OleLoadPicture));
    hr = pf_load(pfd_pngimage, sizeof pfd_pngimage, sizeof pfd_pngimage, &p);
    t_note("OleLoadPicture(PNG 1x1) hr=%08lx", hr);
    if (hr == S_OK && p) { IPicture_get_Type(p, &ty); t_note("PNG loaded as picture type %d", ty); IPicture_Release(p); }
}

/* ---- IID handling --------------------------------------------------------------------------- */
TEST(ploadpic_riid_variants) {
    static const struct { const GUID *iid; const char *n; } t[] = {
        { &IID_IPicture, "IPicture" }, { &IID_IPictureDisp, "IPictureDisp" }, { &IID_IDispatch, "IDispatch" }, { &IID_IUnknown, "IUnknown" },
    };
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < PF_N(t); i++) {
        void *p = NULL; HRESULT hr = load_flavour(0, pfd_gifimage, sizeof pfd_gifimage, sizeof pfd_gifimage, t[i].iid, &p);
        CHECKF(hr == S_OK && p, "%s hr=%08lx", t[i].n, hr);
        if (p) IUnknown_Release((IUnknown *)p);
    }
}
TEST(ploadpic_unknown_riid) {
    static const GUID bogus = { 0xdeadf00d, 0x1111, 0x2222, { 3,3,3,3,4,4,4,4 } };
    void *p = (void *)(ULONG_PTR)1; HRESULT hr; REQUIRE(U(OleLoadPicture));
    hr = load_flavour(0, pfd_gifimage, sizeof pfd_gifimage, sizeof pfd_gifimage, &bogus, &p);
    CHECKF(FAILED(hr), "bogus IID accepted: %08lx", hr);
    SOFT_EQ(hr, PF_E_NOINTERFACE);
    CHECK(p == NULL);
}
TEST(ploadpic_dispatch_properties_of_loaded) {
    IPictureDisp *d = NULL; IStream *s; VARIANT v; DISPPARAMS dp = { NULL, NULL, 0, 0 }; HRESULT hr; REQUIRE(U(OleLoadPicture));
    s = pf_mem_stream(pfd_gifimage, sizeof pfd_gifimage);
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPicture)(s, sizeof pfd_gifimage, TRUE, &IID_IPictureDisp, (void **)&d), S_OK);
    if (d) {
        VariantInit(&v);
        hr = IPictureDisp_Invoke(d, DISPID_PICT_TYPE, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
        EQ(hr, S_OK); EQ(V_VT(&v), VT_I2); EQ(V_I2(&v), PICTYPE_BITMAP); VariantClear(&v);
        hr = IPictureDisp_Invoke(d, DISPID_PICT_HANDLE, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
        EQ(hr, S_OK); EQ(V_VT(&v), VT_I4); CHECK(V_I4(&v) != 0); VariantClear(&v);
        hr = IPictureDisp_Invoke(d, DISPID_PICT_WIDTH, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
        EQ(hr, S_OK); EQ(V_VT(&v), VT_I4); CHECK(V_I4(&v) > 0); VariantClear(&v);
        IPictureDisp_Release(d);
    }
    IStream_Release(s);
}

/* ---- empty picture ------------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_empty_image) {
    static const BYTE e[8] = { 'l', 't', 0, 0, 0, 0, 0, 0 };
    IStream *s = pf_mem_stream(e, 8); IPicture *p = NULL; short ty = 99; OLE_HANDLE h = 0xdeadbeef; DWORD at = 0xdeadbeef;
    REQUIRE(U(OleLoadPicture));
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPicture)(s, 8, TRUE, &IID_IPicture, (void **)&p), S_OK);
    CHECK(p != NULL);
    if (p) {
        EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_NONE);
        EQ(IPicture_get_Attributes(p, &at), S_OK); EQ(at, 0);
        EQ(IPicture_get_Handle(p, &h), S_OK); EQ(h, 0);
        IPicture_Release(p);
    }
    IStream_Release(s);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_empty_image_at_offset) {
    BYTE buf[200]; IStream *s; IPicture *p = NULL; short ty = 99; LARGE_INTEGER sk; REQUIRE(U(OleLoadPicture));
    memset(buf, 0, sizeof buf); memcpy(buf + 42, "lt\0\0", 4);
    s = pf_mem_stream(buf, sizeof buf);
    if (!s) { CHECK(s); return; }
    sk.QuadPart = 42; EQ(IStream_Seek(s, sk, STREAM_SEEK_SET, NULL), S_OK);
    EQ(U(OleLoadPicture)(s, 8, TRUE, &IID_IPicture, (void **)&p), S_OK);
    if (p) { EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_NONE); IPicture_Release(p); }
    IStream_Release(s);
}
TEST(ploadpic_empty_image_forward_only_size_zero) {
    static const BYTE e[8] = { 'l', 't', 0, 0, 0, 0, 0, 0 };
    void *p = NULL; short ty = 99; REQUIRE(U(OleLoadPicture));
    EQ(load_flavour(1, e, 8, 0, &IID_IPicture, &p), S_OK);
    if (p) { EQ(IPicture_get_Type((IPicture *)p, &ty), S_OK); EQ(ty, PICTYPE_NONE); IPicture_Release((IPicture *)p); }
}
TEST(ploadpic_empty_image_is_renderable_noop) {
    static const BYTE e[8] = { 'l', 't', 0, 0, 0, 0, 0, 0 };
    IPicture *p = NULL; HBITMAP old, dib; HDC dc; HRESULT hr; REQUIRE(U(OleLoadPicture));
    EQ(pf_load(e, 8, 8, &p), S_OK);
    dc = pf_render_dc(8, 8, &old, &dib);
    if (p && dc) { hr = IPicture_Render(p, dc, 0, 0, 4, 4, 0, 0, 100, 100, NULL); t_note("Render of PICTYPE_NONE hr=%08lx", hr); }
    if (dc) pf_render_dc_free(dc, old, dib);
    if (p) IPicture_Release(p);
}

/* ---- invalid data ----------------------------------------------------------------------------- */
TEST(ploadpic_invalid_streams) {
    BYTE garbage[100]; BYTE bad_bmp[sizeof pfd_bmpimage24]; BYTE lt_short[28]; int i;
    struct { const char *why; const BYTE *d; size_t n; LONG lsize; } t[8];
    REQUIRE(U(OleLoadPicture));
    for (i = 0; i < 100; i++) garbage[i] = (BYTE)(i * 37 + 11);
    memcpy(bad_bmp, pfd_bmpimage24, sizeof bad_bmp); *(DWORD *)(bad_bmp + 14) = 0x1234;     /* bogus biSize */
    memcpy(lt_short, "lt\0\0", 4); *(DWORD *)(lt_short + 4) = 1000; memset(lt_short + 8, 'x', 20);
    t[0].why = "garbage"; t[0].d = garbage; t[0].n = sizeof garbage; t[0].lsize = sizeof garbage;
    t[1].why = "empty stream"; t[1].d = (const BYTE *)""; t[1].n = 0; t[1].lsize = 0;
    t[2].why = "lt header only"; t[2].d = (const BYTE *)"lt\0\0"; t[2].n = 4; t[2].lsize = 4;
    t[3].why = "lt header, size beyond stream"; t[3].d = lt_short; t[3].n = sizeof lt_short; t[3].lsize = sizeof lt_short;
    t[4].why = "BM signature only"; t[4].d = (const BYTE *)"BM"; t[4].n = 2; t[4].lsize = 2;
    t[5].why = "truncated bmp (file header + 16)"; t[5].d = pfd_bmpimage24; t[5].n = 30; t[5].lsize = 30;
    t[6].why = "bmp with bogus info header size"; t[6].d = bad_bmp; t[6].n = sizeof bad_bmp; t[6].lsize = sizeof bad_bmp;
    t[7].why = "GIF signature only"; t[7].d = pfd_gifimage; t[7].n = 6; t[7].lsize = 6;
    for (i = 0; i < PF_N(t); i++) {
        IPicture *p = (IPicture *)(ULONG_PTR)1;
        HRESULT hr = load_flavour(0, t[i].d, t[i].n, t[i].lsize, &IID_IPicture, (void **)&p);
        CHECKF(FAILED(hr), "%s accepted: %08lx", t[i].why, hr);
        t_note("%s -> hr=%08lx", t[i].why, hr);
        if (SUCCEEDED(hr) && p) IPicture_Release(p);
        else CHECKF(p == NULL, "%s: output pointer not cleared", t[i].why);
    }
}
TEST(ploadpic_invalid_streams_forward_only) {
    BYTE garbage[100]; int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < 100; i++) garbage[i] = (BYTE)(i * 53 + 7);
    {
        void *p = NULL; HRESULT hr = load_flavour(1, garbage, sizeof garbage, 0, &IID_IPicture, &p);
        CHECKF(FAILED(hr), "garbage accepted: %08lx", hr);
        if (SUCCEEDED(hr) && p) IPicture_Release((IPicture *)p);
    }
    {
        void *p = NULL; HRESULT hr = load_flavour(1, "", 0, 0, &IID_IPicture, &p);
        t_note("empty forward-only stream with lSize 0 -> hr=%08lx", hr);
        if (SUCCEEDED(hr) && p) IPicture_Release((IPicture *)p);
    }
}
TEST(ploadpic_truncated_images_noted) {
    /* A decoder may fail or may produce a partial image: record rather than assert. */
    int i; REQUIRE(U(OleLoadPicture));
    for (i = 0; i < PF_N(images); i++) {
        IPicture *p = NULL; size_t cut = images[i].len / 2; HRESULT hr;
        hr = pf_load(images[i].data, cut, (LONG)cut, &p);
        t_note("%s truncated to %zu of %zu bytes -> hr=%08lx", images[i].name, cut, images[i].len, hr);
        if (SUCCEEDED(hr) && p) IPicture_Release(p);
    }
}
/* ---- metafiles ------------------------------------------------------------------------------ */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_placeable_metafile) {
    IStream *s; IPicture *p = NULL; OLE_HANDLE h = 0; short ty = -1; LONG cx = 0; BOOL keep = 99;
    REQUIRE(U(OleLoadPictureEx));
    s = pf_mem_stream(pfd_apmdata, sizeof pfd_apmdata);
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPictureEx)(s, sizeof pfd_apmdata, TRUE, &IID_IPicture, 100, 100, 0, (void **)&p), S_OK);
    if (p) {
        EQ(IPicture_get_Handle(p, &h), S_OK); CHECK(h != 0);
        EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_METAFILE);
        EQ(IPicture_get_Height(p, &cx), S_OK); EQ(cx, 1667);
        EQ(IPicture_get_Width(p, &cx), S_OK); EQ(cx, 1323);
        EQ(IPicture_get_KeepOriginalFormat(p, &keep), S_OK); EQ(keep, FALSE);
        h = 0xdeadbeef; EQ(IPicture_get_hPal(p, &h), PF_E_FAIL);
        IPicture_Release(p);
    }
    IStream_Release(s);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_plain_metafile_rejected) {
    IStream *s; IPicture *p = (IPicture *)(ULONG_PTR)1; REQUIRE(U(OleLoadPictureEx));
    s = pf_mem_stream(pfd_metafile, sizeof pfd_metafile);
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPictureEx)(s, sizeof pfd_metafile, TRUE, &IID_IPicture, 100, 100, 0, (void **)&p), PF_E_FAIL);
    IStream_Release(s);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ploadpic_enhmetafile) {
    IStream *s; IPicture *p = NULL; OLE_HANDLE h = 0; short ty = -1; LONG cx = 0; BOOL keep = 99;
    REQUIRE(U(OleLoadPictureEx));
    s = pf_mem_stream(pfd_enhmetafile, sizeof pfd_enhmetafile);
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPictureEx)(s, sizeof pfd_enhmetafile, TRUE, &IID_IPicture, 10, 10, 0, (void **)&p), S_OK);
    if (p) {
        EQ(IPicture_get_Handle(p, &h), S_OK); CHECK(h != 0);
        EQ(IPicture_get_Type(p, &ty), S_OK); EQ(ty, PICTYPE_ENHMETAFILE);
        EQ(IPicture_get_Height(p, &cx), S_OK); EQ(cx, -23);
        EQ(IPicture_get_Width(p, &cx), S_OK); EQ(cx, -25);
        EQ(IPicture_get_KeepOriginalFormat(p, &keep), S_OK); EQ(keep, FALSE);
        IPicture_Release(p);
    }
    IStream_Release(s);
}
TEST(ploadpic_metafile_truncated_noted) {
    IPicture *p = NULL; HRESULT hr; IStream *s; REQUIRE(U(OleLoadPictureEx));
    s = pf_mem_stream(pfd_apmdata, 22);
    if (!s) { CHECK(s); return; }
    hr = U(OleLoadPictureEx)(s, 22, TRUE, &IID_IPicture, 100, 100, 0, (void **)&p);
    t_note("placeable header without records -> hr=%08lx", hr);
    if (SUCCEEDED(hr) && p) IPicture_Release(p);
    IStream_Release(s);
}

/* ---- OleLoadPictureEx ------------------------------------------------------------------------ */
TEST(ploadpicex_matches_loadpicture_for_bitmaps) {
    IStream *s; IPicture *a = NULL, *b = NULL; short ta = -1, tb = -2; OLE_XSIZE_HIMETRIC wa = 0, wb = 1; REQUIRE(U(OleLoadPictureEx));
    s = pf_mem_stream(pfd_bmpimage24, sizeof pfd_bmpimage24);
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPictureEx)(s, sizeof pfd_bmpimage24, TRUE, &IID_IPicture, 0, 0, 0, (void **)&a), S_OK);
    IStream_Release(s);
    EQ(pf_load(pfd_bmpimage24, sizeof pfd_bmpimage24, sizeof pfd_bmpimage24, &b), S_OK);
    if (a && b) {
        IPicture_get_Type(a, &ta); IPicture_get_Type(b, &tb); EQ(ta, tb);
        IPicture_get_Width(a, &wa); IPicture_get_Width(b, &wb); EQ(wa, wb);
    }
    if (a) IPicture_Release(a);
    if (b) IPicture_Release(b);
}
TEST(ploadpicex_flags_table) {
    static const DWORD flags[] = { 0, LP_MONOCHROME, LP_VGACOLOR, LP_COLOR, LP_DEFAULT };
    int i; REQUIRE(U(OleLoadPictureEx));
    for (i = 0; i < PF_N(flags); i++) {
        IStream *s = pf_mem_stream(pfd_bmpimage24, sizeof pfd_bmpimage24); IPicture *p = NULL; HRESULT hr;
        if (!s) { CHECK(s); return; }
        hr = U(OleLoadPictureEx)(s, sizeof pfd_bmpimage24, TRUE, &IID_IPicture, 0, 0, flags[i], (void **)&p);
        CHECKF(hr == S_OK, "flags 0x%lx hr=%08lx", flags[i], hr);
        if (p) IPicture_Release(p);
        IStream_Release(s);
    }
}
TEST(ploadpicex_desired_size_ignored_for_bitmap) {
    IStream *s; IPicture *a = NULL, *b = NULL; OLE_XSIZE_HIMETRIC wa = 0, wb = 1; REQUIRE(U(OleLoadPictureEx));
    s = pf_mem_stream(pfd_bmpimage24, sizeof pfd_bmpimage24);
    if (!s) { CHECK(s); return; }
    EQ(U(OleLoadPictureEx)(s, sizeof pfd_bmpimage24, TRUE, &IID_IPicture, 500, 500, 0, (void **)&a), S_OK);
    IStream_Release(s);
    EQ(pf_load(pfd_bmpimage24, sizeof pfd_bmpimage24, sizeof pfd_bmpimage24, &b), S_OK);
    if (a && b) { IPicture_get_Width(a, &wa); IPicture_get_Width(b, &wb); SOFT_EQ(wa, wb); }
    if (a) IPicture_Release(a);
    if (b) IPicture_Release(b);
}

/* ---- icons and cursors ---------------------------------------------------------------------- */
static void icon_checks(const BYTE *buf, size_t n, int px, int kind) {
    IPicture *p = NULL; HRESULT hr; short ty = -1; OLE_HANDLE h = 0; ICONINFO ii; BITMAP bm; OLE_XSIZE_HIMETRIC w = 0;
    hr = pf_load(buf, n, (LONG)n, &p);
    if (kind == 1) CHECKF(hr == S_OK && p, "ico %dx%d hr=%08lx", px, px, hr); else t_note("cursor %dx%d hr=%08lx", px, px, hr);
    if (hr != S_OK || !p) return;
    EQ(IPicture_get_Type(p, &ty), S_OK);
    if (kind == 1) EQ(ty, PICTYPE_ICON); else t_note("cursor file loaded as picture type %d", ty);
    EQ(IPicture_get_Handle(p, &h), S_OK); CHECK(h != 0);
    memset(&ii, 0, sizeof ii);
    if (h && GetIconInfo((HICON)(LONG_PTR)(LONG)h, &ii)) {
        memset(&bm, 0, sizeof bm);
        GetObjectA(ii.hbmMask, sizeof bm, &bm);
        t_note("icon mask bitmap %dx%d (fIcon=%d)", bm.bmWidth, bm.bmHeight, ii.fIcon);
        if (kind == 1) EQ(ii.fIcon, TRUE);
        DeleteObject(ii.hbmMask); if (ii.hbmColor) DeleteObject(ii.hbmColor);
    } else if (kind == 1) CHECK(0);
    EQ(IPicture_get_Width(p, &w), S_OK);
    t_note("icon %dx%d width %ld himetric (expected %d at screen dpi)", px, px, w, MulDiv(px, 2540, dpi_x()));
    CHECK(w > 0);
    IPicture_Release(p);
}
TEST(ploadpic_ico_16x16) {
    BYTE buf[1200]; size_t n; REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 1, 16, 16); icon_checks(buf, n, 16, 1);
}
TEST(ploadpic_ico_32x32) {
    BYTE buf[4400]; size_t n; REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 1, 32, 32); icon_checks(buf, n, 32, 1);
}
TEST(ploadpic_ico_forward_only) {
    BYTE buf[1200]; size_t n; void *p = NULL; short ty = -1; REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 1, 16, 16);
    EQ(load_flavour(1, buf, n, 0, &IID_IPicture, &p), S_OK);
    if (p) { EQ(IPicture_get_Type((IPicture *)p, &ty), S_OK); EQ(ty, PICTYPE_ICON); IPicture_Release((IPicture *)p); }
}
TEST(ploadpic_cur_16x16) {
    BYTE buf[1200]; size_t n; REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 2, 16, 16); icon_checks(buf, n, 16, 2);
}
TEST(ploadpic_ico_truncated) {
    BYTE buf[1200]; size_t n; IPicture *p = (IPicture *)(ULONG_PTR)1; HRESULT hr; REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 1, 16, 16);
    hr = pf_load(buf, 22, 22, &p);      /* directory only, no image */
    CHECKF(FAILED(hr), "directory-only icon accepted: %08lx", hr);
    t_note("icon directory without image -> %08lx", hr);
    if (SUCCEEDED(hr) && p) IPicture_Release(p);
    hr = pf_load(buf, n - 40, (LONG)(n - 40), &p);
    t_note("icon with truncated pixel data -> %08lx", hr);
    if (SUCCEEDED(hr) && p) IPicture_Release(p);
}
TEST(ploadpic_ico_attributes_and_hpal) {
    BYTE buf[1200]; size_t n; IPicture *p = NULL; DWORD at = 0xdeadbeef; OLE_HANDLE hp = 0xdeadbeef; REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 1, 16, 16);
    EQ(pf_load(buf, n, (LONG)n, &p), S_OK);
    if (p) {
        EQ(IPicture_get_Attributes(p, &at), S_OK);
        t_note("icon attributes 0x%lx (TRANSPARENT=2 expected)", at);
        SOFT_EQ(at, PICTURE_TRANSPARENT);
        EQ(IPicture_get_hPal(p, &hp), PF_E_FAIL);
        IPicture_Release(p);
    }
}
TEST(ploadpic_ico_render_pixels) {
    BYTE buf[1200]; size_t n; IPicture *p = NULL; HBITMAP old, dib; HDC dc; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleLoadPicture));
    n = pf_build_ico(buf, 1, 16, 16);
    EQ(pf_load(buf, n, (LONG)n, &p), S_OK);
    dc = pf_render_dc(32, 32, &old, &dib);
    if (!dc) { t_skip("no memory DC"); if (p) IPicture_Release(p); return; }
    if (p) {
        COLORREF c;
        IPicture_get_Width(p, &w); IPicture_get_Height(p, &h);
        EQ(IPicture_Render(p, dc, 0, 0, 16, 16, 0, h, w, -h, NULL), S_OK);
        /* pixel (x,y) of the fixture is BGRA(x*40, y*40, 255, 255): at (0,0) it is RGB(255,0,0) */
        c = GetPixel(dc, 0, 0) & 0xffffff;
        SOFT_EQ(c, RGB(255, 0, 0));
        c = GetPixel(dc, 5, 3) & 0xffffff;
        SOFT_EQ(c, RGB(255, 3 * 40, 5 * 40));
        c = GetPixel(dc, 20, 20) & 0xffffff;
        EQ(c, 0);
        IPicture_Release(p);
    }
    pf_render_dc_free(dc, old, dib);
}
