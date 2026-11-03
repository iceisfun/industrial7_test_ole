/* OleCreatePictureIndirect and the IPicture property surface: handle/type/attributes, HIMETRIC
 * size, ownership (fOwn), hPal, KeepOriginalFormat, Render, SelectPicture, IPictureDisp dispatch,
 * connection points, CLSID_StdPicture activation. */
#include "pf.h"

static int dpi_x(void) { HDC dc = GetDC(NULL); int v = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96; if (dc) ReleaseDC(NULL, dc); return v; }
static int dpi_y(void) { HDC dc = GetDC(NULL); int v = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96; if (dc) ReleaseDC(NULL, dc); return v; }

static HRESULT mkpic(short type, IPicture **pic, void **owned_handle) {
    PICTDESC d; HDC hdc;
    memset(&d, 0, sizeof d);
    d.cbSizeofstruct = sizeof d; d.picType = type;
    *pic = NULL; if (owned_handle) *owned_handle = NULL;
    switch (type) {
    case PICTYPE_UNINITIALIZED:
        return U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)pic);
    case PICTYPE_NONE: break;
    case PICTYPE_BITMAP:
        d.bmp.hbitmap = CreateBitmap(1, 1, 1, 1, NULL); d.bmp.hpal = (HPALETTE)(ULONG_PTR)0xbeefdead;
        if (owned_handle) *owned_handle = d.bmp.hbitmap;
        break;
    case PICTYPE_ICON:
        d.icon.hicon = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
        if (owned_handle) *owned_handle = d.icon.hicon;
        break;
    case PICTYPE_METAFILE:
        hdc = CreateMetaFileA(NULL);
        d.wmf.hmeta = CloseMetaFile(hdc); d.wmf.xExt = 1; d.wmf.yExt = 1;
        if (owned_handle) *owned_handle = d.wmf.hmeta;
        break;
    case PICTYPE_ENHMETAFILE:
        hdc = CreateEnhMetaFileA(0, NULL, NULL, NULL);
        d.emf.hemf = CloseEnhMetaFile(hdc);
        if (owned_handle) *owned_handle = d.emf.hemf;
        break;
    default: return E_NOTIMPL;
    }
    return U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)pic);
}

/* ---- OleCreatePictureIndirect -------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_create_uninitialized_null_desc) {
    IPicture *p = NULL; short t = 0; OLE_HANDLE h = 0xdeadbeef;
    REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    CHECK(p != NULL);
    if (!p) return;
    EQ(IPicture_get_Type(p, &t), S_OK); EQ(t, PICTYPE_UNINITIALIZED);
    EQ(IPicture_get_Handle(p, &h), S_OK); EQ(h, 0);
    EQ(IPicture_Release(p), 0);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_create_uninitialized_desc_rejected) {
    PICTDESC d; IPicture *p = (IPicture *)(ULONG_PTR)0xdeadbeef;
    REQUIRE(U(OleCreatePictureIndirect));
    d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_UNINITIALIZED;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), PF_E_UNEXPECTED);
    EQ(p, NULL);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_create_all_types) {
    short ty; REQUIRE(U(OleCreatePictureIndirect));
    for (ty = PICTYPE_UNINITIALIZED; ty <= PICTYPE_ENHMETAFILE; ty++) {
        IPicture *p; short got = 0x7777; OLE_HANDLE h = 0xdeadbeef, hp = 0xdeadbeef; HRESULT hr;
        hr = mkpic(ty, &p, NULL);
        CHECKF(hr == S_OK, "type %d hr=%08lx", ty, hr);
        if (hr != S_OK || !p) continue;
        EQ(IPicture_get_Type(p, &got), S_OK); EQ(got, ty);
        EQ(IPicture_get_Handle(p, &h), S_OK);
        if (ty == PICTYPE_UNINITIALIZED || ty == PICTYPE_NONE) EQ(h, 0); else CHECKF(h != 0 && h != 0xdeadbeef, "type %d handle %x", ty, h);
        hr = IPicture_get_hPal(p, &hp);
        if (ty == PICTYPE_BITMAP) { EQ(hr, S_OK); EQ(hp, 0xbeefdead); }
        else { EQ(hr, PF_E_FAIL); CHECKF(hp == 0xdeadbeef || hp == 0, "type %d hpal out %x", ty, hp); }
        EQ(IPicture_Release(p), 0);
    }
}
TEST(ppict_create_null_ppv_with_bad_type) {
    PICTDESC d; IPicture *p = (IPicture *)(ULONG_PTR)0xdeadbeef; HRESULT hr;
    REQUIRE(U(OleCreatePictureIndirect));
    d.cbSizeofstruct = sizeof d; d.picType = 99;
    hr = U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p);
    CHECKF(FAILED(hr), "unknown picType accepted: %08lx", hr);
    t_note("OleCreatePictureIndirect(picType=99) hr=%08lx", hr);
    CHECK(p == NULL || p == (IPicture *)(ULONG_PTR)0xdeadbeef);
    if (SUCCEEDED(hr) && p) IPicture_Release(p);
}
TEST(ppict_create_bitmap_returns_same_handle) {
    PICTDESC d; IPicture *p = NULL; OLE_HANDLE h = 0; HBITMAP bm;
    REQUIRE(U(OleCreatePictureIndirect));
    bm = CreateBitmap(3, 2, 1, 1, NULL);
    if (!bm) { t_skip("no GDI"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_BITMAP; d.bmp.hbitmap = bm;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (p) {
        EQ(IPicture_get_Handle(p, &h), S_OK);
        EQ((ULONG)h, (ULONG)(ULONG_PTR)bm);
        IPicture_Release(p);
    }
    DeleteObject(bm);
}
TEST(ppict_create_riid_table) {
    static const struct { const GUID *iid; const char *n; } t[] = {
        { &IID_IPicture, "IPicture" }, { &IID_IPictureDisp, "IPictureDisp" }, { &IID_IDispatch, "IDispatch" },
        { &IID_IUnknown, "IUnknown" }, { &IID_IPersistStream, "IPersistStream" },
    };
    int i; REQUIRE(U(OleCreatePictureIndirect));
    for (i = 0; i < PF_N(t); i++) {
        IUnknown *u = NULL; HRESULT hr = U(OleCreatePictureIndirect)(NULL, t[i].iid, TRUE, (void **)&u);
        CHECKF(hr == S_OK, "%s hr=%08lx", t[i].n, hr);
        if (u) IUnknown_Release(u);
    }
}
TEST(ppict_create_unknown_iid) {
    static const GUID bogus = { 0xdeadf00d, 0x4321, 0x1234, { 8,7,6,5,4,3,2,1 } };
    void *p = (void *)(ULONG_PTR)1; HRESULT hr;
    REQUIRE(U(OleCreatePictureIndirect));
    hr = U(OleCreatePictureIndirect)(NULL, &bogus, TRUE, &p);
    EQ(hr, PF_E_NOINTERFACE);
    EQ(p, NULL);
}

/* ---- ownership ------------------------------------------------------------------------------ */
TEST(ppict_fown_true_destroys_bitmap) {
    PICTDESC d; IPicture *p = NULL; HBITMAP bm;
    REQUIRE(U(OleCreatePictureIndirect));
    bm = CreateBitmap(2, 2, 1, 1, NULL);
    if (!bm) { t_skip("no GDI"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_BITMAP; d.bmp.hbitmap = bm;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    EQ(GetObjectType(bm), OBJ_BITMAP);
    if (p) IPicture_Release(p);
    EQ(GetObjectType(bm), 0);
}
TEST(ppict_fown_false_keeps_bitmap) {
    PICTDESC d; IPicture *p = NULL; HBITMAP bm;
    REQUIRE(U(OleCreatePictureIndirect));
    bm = CreateBitmap(2, 2, 1, 1, NULL);
    if (!bm) { t_skip("no GDI"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_BITMAP; d.bmp.hbitmap = bm;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (p) IPicture_Release(p);
    EQ(GetObjectType(bm), OBJ_BITMAP);
    DeleteObject(bm);
}
TEST(ppict_fown_true_destroys_enhmetafile) {
    PICTDESC d; IPicture *p = NULL; HDC dc; HENHMETAFILE e; ENHMETAHEADER h;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateEnhMetaFileA(0, NULL, NULL, NULL);
    if (!dc) { t_skip("no GDI"); return; }
    e = CloseEnhMetaFile(dc);
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ENHMETAFILE; d.emf.hemf = e;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    EQ(GetEnhMetaFileHeader(e, sizeof h, &h) != 0, 1);
    if (p) IPicture_Release(p);
    EQ(GetEnhMetaFileHeader(e, sizeof h, &h), 0);
}
TEST(ppict_fown_false_keeps_enhmetafile) {
    PICTDESC d; IPicture *p = NULL; HDC dc; HENHMETAFILE e; ENHMETAHEADER h;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateEnhMetaFileA(0, NULL, NULL, NULL);
    if (!dc) { t_skip("no GDI"); return; }
    e = CloseEnhMetaFile(dc);
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ENHMETAFILE; d.emf.hemf = e;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (p) IPicture_Release(p);
    EQ(GetEnhMetaFileHeader(e, sizeof h, &h) != 0, 1);
    DeleteEnhMetaFile(e);
}
TEST(ppict_fown_true_destroys_icon) {
    /* An icon built with CreateIcon is ours to give away */
    BYTE and_bits[32 * 4], xor_bits[32 * 4]; HICON ic; PICTDESC d; IPicture *p = NULL; ICONINFO ii;
    REQUIRE(U(OleCreatePictureIndirect));
    memset(and_bits, 0xff, sizeof and_bits); memset(xor_bits, 0, sizeof xor_bits);
    ic = CreateIcon(NULL, 32, 32, 1, 1, and_bits, xor_bits);
    if (!ic) { t_skip("CreateIcon failed"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ICON; d.icon.hicon = ic;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    memset(&ii, 0, sizeof ii);
    EQ(GetIconInfo(ic, &ii) != 0, 1);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (p) IPicture_Release(p);
    EQ(GetIconInfo(ic, &ii) != 0, 0);
}

/* ---- getters -------------------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_getters_null_pointers) {
    IPicture *p; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_get_Handle(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_Type(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_Attributes(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_Width(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_Height(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_hPal(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_KeepOriginalFormat(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_CurDC(p, NULL), PF_E_POINTER);
    IPicture_Release(p);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_getters_null_pointers_uninitialized) {
    IPicture *p = NULL; REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    EQ(IPicture_get_Handle(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_Type(p, NULL), PF_E_POINTER);
    EQ(IPicture_get_Attributes(p, NULL), PF_E_POINTER);
    IPicture_Release(p);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_attributes_reset_output) {
    IPicture *p = NULL; DWORD a = 0xdeadbeef; REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    EQ(IPicture_get_Attributes(p, &a), S_OK); EQ(a, 0);
    IPicture_Release(p);
}
TEST(ppict_attributes_per_type) {
    short ty; REQUIRE(U(OleCreatePictureIndirect));
    for (ty = PICTYPE_UNINITIALIZED; ty <= PICTYPE_ENHMETAFILE; ty++) {
        IPicture *p; DWORD a = 0xdeadbeef;
        if (mkpic(ty, &p, NULL) != S_OK || !p) continue;
        EQ(IPicture_get_Attributes(p, &a), S_OK);
        if (ty == PICTYPE_UNINITIALIZED || ty == PICTYPE_NONE || ty == PICTYPE_BITMAP) EQ(a, 0);
        else t_note("attributes for type %d: 0x%lx (SCALABLE=1 TRANSPARENT=2)", ty, a);
        CHECK((a & ~3UL) == 0);
        IPicture_Release(p);
    }
}
TEST(ppict_size_uninitialized_is_zero) {
    IPicture *p = NULL; OLE_XSIZE_HIMETRIC w = 77; OLE_YSIZE_HIMETRIC h = 77; REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    EQ(IPicture_get_Width(p, &w), S_OK); EQ(w, 0);
    EQ(IPicture_get_Height(p, &h), S_OK); EQ(h, 0);
    IPicture_Release(p);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_himetric_bitmap) {
    static const SIZE sz[] = { { 1, 1 }, { 7, 3 }, { 100, 50 }, { 183, 182 } };
    int i; REQUIRE(U(OleCreatePictureIndirect));
    for (i = 0; i < PF_N(sz); i++) {
        HBITMAP bm = CreateBitmap(sz[i].cx, sz[i].cy, 1, 1, NULL); IPicture *p;
        OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
        if (!bm) { t_skip("no GDI"); return; }
        p = pf_pic_from_bitmap(bm, NULL, FALSE);
        CHECK(p != NULL);
        if (p) {
            EQ(IPicture_get_Width(p, &w), S_OK);
            EQ(IPicture_get_Height(p, &h), S_OK);
            CHECKF(w == MulDiv(sz[i].cx, 2540, dpi_x()), "%dx%d width %ld want %d", (int)sz[i].cx, (int)sz[i].cy, w, MulDiv(sz[i].cx, 2540, dpi_x()));
            CHECKF(h == MulDiv(sz[i].cy, 2540, dpi_y()), "%dx%d height %ld want %d", (int)sz[i].cx, (int)sz[i].cy, h, MulDiv(sz[i].cy, 2540, dpi_y()));
            IPicture_Release(p);
        }
        DeleteObject(bm);
    }
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_himetric_icon) {
    BYTE bits[1024]; HICON ic; PICTDESC d; IPicture *p = NULL; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleCreatePictureIndirect));
    memset(bits, 0, sizeof bits);
    ic = CreateIcon(NULL, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 1, 1, bits, bits);
    if (!ic) { t_skip("CreateIcon failed"); return; }
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ICON; d.icon.hicon = ic;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, FALSE, (void **)&p), S_OK);
    if (p) {
        EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
        EQ(w, MulDiv(GetSystemMetrics(SM_CXICON), 2540, dpi_x()));
        EQ(h, MulDiv(GetSystemMetrics(SM_CYICON), 2540, dpi_y()));
        IPicture_Release(p);
    }
    DestroyIcon(ic);
}
TEST(ppict_himetric_enhmetafile_frame) {
    /* Size of an EMF picture comes from the header's rclFrame (0.01 mm units), reported in HIMETRIC. */
    HDC dc; HENHMETAFILE e; PICTDESC d; IPicture *p = NULL; RECT fr = { 0, 0, 2000, 1000 };
    OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateEnhMetaFileA(0, NULL, &fr, NULL);
    if (!dc) { t_skip("no GDI"); return; }
    e = CloseEnhMetaFile(dc);
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_ENHMETAFILE; d.emf.hemf = e;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (p) {
        EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
        t_note("EMF frame 2000x1000 -> himetric %ldx%ld", w, h);
        EQ(w, 2000); EQ(h, 1000);
        IPicture_Release(p);
    }
}
TEST(ppict_himetric_wmf_extents) {
    HDC dc; HMETAFILE m; PICTDESC d; IPicture *p = NULL; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = CreateMetaFileA(NULL);
    if (!dc) { t_skip("no GDI"); return; }
    m = CloseMetaFile(dc);
    memset(&d, 0, sizeof d); d.cbSizeofstruct = sizeof d; d.picType = PICTYPE_METAFILE; d.wmf.hmeta = m; d.wmf.xExt = 1234; d.wmf.yExt = 4321;
    EQ(U(OleCreatePictureIndirect)(&d, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (p) {
        EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
        EQ(w, 1234); EQ(h, 4321);
        IPicture_Release(p);
    }
}

/* ---- hPal / KeepOriginalFormat / misc ---------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_set_hpal_per_type) {
    short ty; REQUIRE(U(OleCreatePictureIndirect));
    for (ty = PICTYPE_UNINITIALIZED; ty <= PICTYPE_ENHMETAFILE; ty++) {
        IPicture *p; HRESULT hr; OLE_HANDLE got = 0;
        HPALETTE pal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
        if (mkpic(ty, &p, NULL) != S_OK || !p) continue;
        hr = IPicture_set_hPal(p, (OLE_HANDLE)(ULONG_PTR)pal);
        if (ty == PICTYPE_BITMAP) {
            EQ(hr, S_OK);
            EQ(IPicture_get_hPal(p, &got), S_OK);
            EQ(got, (OLE_HANDLE)(ULONG_PTR)pal);
        } else EQ(hr, PF_E_FAIL);
        IPicture_Release(p);
    }
}
TEST(ppict_keep_original_format) {
    IPicture *p; BOOL k = 99; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_get_KeepOriginalFormat(p, &k), S_OK);
    t_note("default KeepOriginalFormat for created bitmap: %d", k);
    EQ(IPicture_put_KeepOriginalFormat(p, TRUE), S_OK);
    EQ(IPicture_get_KeepOriginalFormat(p, &k), S_OK); EQ(k, TRUE);
    EQ(IPicture_put_KeepOriginalFormat(p, FALSE), S_OK);
    EQ(IPicture_get_KeepOriginalFormat(p, &k), S_OK); EQ(k, FALSE);
    IPicture_Release(p);
}
TEST(ppict_picture_changed) {
    IPicture *p; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_PictureChanged(p), S_OK);
    IPicture_Release(p);
}
TEST(ppict_cur_dc_initially_null) {
    IPicture *p; HDC dc = (HDC)(ULONG_PTR)0x1234; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_get_CurDC(p, &dc), S_OK);
    EQ(dc, NULL);
    IPicture_Release(p);
}
TEST(ppict_select_picture_bitmap) {
    HBITMAP bm = CreateBitmap(4, 4, 1, 1, NULL); IPicture *p; HDC out = NULL; OLE_HANDLE ob = 0, cur = 0; HRESULT hr; HDC cd = NULL;
    REQUIRE(U(OleCreatePictureIndirect));
    if (!bm) { t_skip("no GDI"); return; }
    p = pf_pic_from_bitmap(bm, NULL, FALSE);
    if (!p) { CHECK(p); DeleteObject(bm); return; }
    hr = IPicture_SelectPicture(p, NULL, &out, &ob);
    t_note("SelectPicture(NULL dc) hr=%08lx out=%p bmp=%x", hr, out, ob);
    if (hr == S_OK) {
        CHECK(out != NULL);
        EQ(IPicture_get_CurDC(p, &cd), S_OK);
        EQ(cd, out);
        EQ(IPicture_get_Handle(p, &cur), S_OK);
    }
    IPicture_Release(p);
    DeleteObject(bm);
}
TEST(ppict_select_picture_non_bitmap_fails) {
    IPicture *p; HDC out = (HDC)(ULONG_PTR)5; OLE_HANDLE ob = 5; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_ICON, &p, NULL), S_OK);
    if (!p) return;
    hr = IPicture_SelectPicture(p, NULL, &out, &ob);
    CHECKF(FAILED(hr), "SelectPicture on icon hr=%08lx", hr);
    SOFT_EQ(hr, PF_E_FAIL);
    IPicture_Release(p);
}
/* ---- interfaces ----------------------------------------------------------------------------- */
static void qi_expect(IUnknown *u, const GUID *iid, BOOL want, BOOL hard, const char *n) {
    void *p = (void *)(ULONG_PTR)1; HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    HRESULT w = want ? S_OK : PF_E_NOINTERFACE;
    if (hard) CHECKF(hr == w, "%s hr=%08lx", n, hr); else SOFT_EQ(hr, w);
    if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)p); else CHECKF(p == NULL, "%s out not cleared", n);
}
TEST(ppict_interfaces) {
    IPicture *p; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    qi_expect((IUnknown *)p, &IID_IPicture, TRUE, TRUE, "IPicture");
    qi_expect((IUnknown *)p, &IID_IPictureDisp, TRUE, TRUE, "IPictureDisp");
    qi_expect((IUnknown *)p, &IID_IDispatch, TRUE, TRUE, "IDispatch");
    qi_expect((IUnknown *)p, &IID_IUnknown, TRUE, TRUE, "IUnknown");
    qi_expect((IUnknown *)p, &IID_IPersist, TRUE, TRUE, "IPersist");
    qi_expect((IUnknown *)p, &IID_IPersistStream, TRUE, TRUE, "IPersistStream");
    qi_expect((IUnknown *)p, &IID_IConnectionPointContainer, TRUE, TRUE, "IConnectionPointContainer");
    qi_expect((IUnknown *)p, &IID_IPersistPropertyBag, TRUE, FALSE, "IPersistPropertyBag");
    qi_expect((IUnknown *)p, &IID_IPersistStreamInit, FALSE, FALSE, "IPersistStreamInit");
    qi_expect((IUnknown *)p, &IID_IFont, FALSE, TRUE, "IFont");
    qi_expect((IUnknown *)p, &IID_IFontDisp, FALSE, TRUE, "IFontDisp");
    qi_expect((IUnknown *)p, &IID_IOleObject, FALSE, TRUE, "IOleObject");
    qi_expect((IUnknown *)p, &IID_IClassFactory, FALSE, TRUE, "IClassFactory");
    IPicture_Release(p);
}
TEST(ppict_unknown_identity_and_refcount) {
    IPicture *p; IPictureDisp *d = NULL; IUnknown *u1 = NULL, *u2 = NULL; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_QueryInterface(p, &IID_IPictureDisp, (void **)&d), S_OK);
    EQ(IPicture_AddRef(p), 3);
    EQ(IPicture_Release(p), 2);
    if (d) {
        EQ(IPictureDisp_QueryInterface(d, &IID_IUnknown, (void **)&u1), S_OK);
        EQ(IPicture_QueryInterface(p, &IID_IUnknown, (void **)&u2), S_OK);
        EQ(u1, u2);
        if (u1) IUnknown_Release(u1);
        if (u2) IUnknown_Release(u2);
        EQ(IPictureDisp_Release(d), 1);
    }
    EQ(IPicture_Release(p), 0);
}
TEST(ppict_persist_classid) {
    IPicture *p; IPersistStream *ps = NULL; CLSID c; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_QueryInterface(p, &IID_IPersistStream, (void **)&ps), S_OK);
    if (ps) {
        memset(&c, 0, sizeof c);
        EQ(IPersistStream_GetClassID(ps, &c), S_OK);
        CHECK(IsEqualCLSID(&c, &PF_CLSID_StdPicture));
        IPersistStream_Release(ps);
    }
    IPicture_Release(p);
}

/* ---- Render --------------------------------------------------------------------------------- */
static HRESULT render(IPicture *p, HDC dc, LONG x, LONG y, LONG cx, LONG cy, LONG xs, LONG ys, LONG cxs, LONG cys) {
    return IPicture_Render(p, dc, x, y, cx, cy, xs, ys, cxs, cys, NULL);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_render_zero_dimensions_uninitialized) {
    static const LONG t[][4] = { {0,0,0,0}, {10,10,10,0}, {10,10,0,10}, {10,10,0,0}, {0,10,10,10}, {10,0,10,10}, {0,0,10,10} };
    IPicture *p = NULL; HBITMAP old, dib; HDC dc; int i; REQUIRE(U(OleCreatePictureIndirect));
    dc = pf_render_dc(100, 100, &old, &dib);
    if (!dc) { t_skip("no memory DC"); return; }
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (p) {
        for (i = 0; i < PF_N(t); i++) {
            HRESULT hr = render(p, dc, 0, 0, t[i][0], t[i][1], 0, 0, t[i][2], t[i][3]);
            CHECKF(hr == PF_CTL_E_INVALIDPROPERTYVALUE, "case %d hr=%08lx", i, hr);
        }
        EQ(render(p, dc, 0, 0, 10, 10, 0, 0, 10, 10), S_OK);
        IPicture_Release(p);
    }
    pf_render_dc_free(dc, old, dib);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_render_zero_dimensions_icon) {
    static const LONG t[][4] = { {0,0,0,0}, {10,10,10,0}, {10,10,0,10}, {10,10,0,0}, {0,10,10,10}, {10,0,10,10}, {0,0,10,10} };
    IPicture *p; HBITMAP old, dib; HDC dc; int i; REQUIRE(U(OleCreatePictureIndirect));
    dc = pf_render_dc(100, 100, &old, &dib);
    if (!dc) { t_skip("no memory DC"); return; }
    if (mkpic(PICTYPE_ICON, &p, NULL) == S_OK && p) {
        for (i = 0; i < PF_N(t); i++) {
            HRESULT hr = render(p, dc, 0, 0, t[i][0], t[i][1], 0, 0, t[i][2], t[i][3]);
            CHECKF(hr == PF_CTL_E_INVALIDPROPERTYVALUE, "case %d hr=%08lx", i, hr);
        }
        IPicture_Release(p);
    } else t_skip("no application icon");
    pf_render_dc_free(dc, old, dib);
}
TEST(ppict_render_zero_dimensions_bitmap) {
    IPicture *p; HBITMAP old, dib; HDC dc; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    dc = pf_render_dc(32, 32, &old, &dib);
    if (!dc) { t_skip("no memory DC"); return; }
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (p) {
        hr = render(p, dc, 0, 0, 0, 10, 0, 0, 10, 10);
        CHECKF(hr == PF_CTL_E_INVALIDPROPERTYVALUE, "zero cx hr=%08lx", hr);
        hr = render(p, dc, 0, 0, 10, 0, 0, 0, 10, 10);
        CHECKF(hr == PF_CTL_E_INVALIDPROPERTYVALUE, "zero cy hr=%08lx", hr);
        hr = render(p, dc, 0, 0, 10, 10, 0, 0, 0, 10);
        CHECKF(hr == PF_CTL_E_INVALIDPROPERTYVALUE, "zero cxSrc hr=%08lx", hr);
        hr = render(p, dc, 0, 0, 10, 10, 0, 0, 10, 0);
        CHECKF(hr == PF_CTL_E_INVALIDPROPERTYVALUE, "zero cySrc hr=%08lx", hr);
        IPicture_Release(p);
    }
    pf_render_dc_free(dc, old, dib);
}

/* 2x2 quadrant bitmap: TL red, TR green, BL blue, BR white (top-down) */
static const COLORREF quad[4] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,255) };
static IPicture *quad_pic(HBITMAP *bm_out) {
    HBITMAP bm = pf_dib(2, 2, 32, quad, NULL); IPicture *p;
    *bm_out = bm;
    if (!bm) return NULL;
    p = pf_pic_from_bitmap(bm, NULL, FALSE);
    return p;
}
static COLORREF px(HDC dc, int x, int y) { return GetPixel(dc, x, y) & 0xffffff; }
TEST(ppict_render_bitmap_pixels_1to1) {
    HBITMAP bm, old, dib; IPicture *p; HDC dc; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleCreatePictureIndirect));
    p = quad_pic(&bm);
    if (!p) { t_skip("cannot create DIB section"); if (bm) DeleteObject(bm); return; }
    dc = pf_render_dc(8, 8, &old, &dib);
    if (!dc) { t_skip("no memory DC"); IPicture_Release(p); DeleteObject(bm); return; }
    EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
    EQ(render(p, dc, 0, 0, 2, 2, 0, h, w, -h), S_OK);
    EQ(px(dc, 0, 0), quad[0]); EQ(px(dc, 1, 0), quad[1]);
    EQ(px(dc, 0, 1), quad[2]); EQ(px(dc, 1, 1), quad[3]);
    pf_render_dc_free(dc, old, dib);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(ppict_render_bitmap_stretched_quadrants) {
    HBITMAP bm, old, dib; IPicture *p; HDC dc; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleCreatePictureIndirect));
    p = quad_pic(&bm);
    if (!p) { t_skip("cannot create DIB section"); if (bm) DeleteObject(bm); return; }
    dc = pf_render_dc(16, 16, &old, &dib);
    if (!dc) { t_skip("no memory DC"); IPicture_Release(p); DeleteObject(bm); return; }
    EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
    EQ(render(p, dc, 4, 4, 8, 8, 0, h, w, -h), S_OK);
    EQ(px(dc, 4, 4), quad[0]); EQ(px(dc, 11, 4), quad[1]);
    EQ(px(dc, 4, 11), quad[2]); EQ(px(dc, 11, 11), quad[3]);
    /* destination outside the target rectangle is untouched */
    EQ(px(dc, 3, 3), 0); EQ(px(dc, 12, 12), 0); EQ(px(dc, 0, 15), 0);
    pf_render_dc_free(dc, old, dib);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(ppict_render_bitmap_flipped_source) {
    /* A positive cySrc starting at 0 reads the image bottom-up: rows are swapped relative to the
     * (ySrc = height, cySrc = -height) orientation. */
    HBITMAP bm, old, dib; IPicture *p; HDC dc; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0;
    REQUIRE(U(OleCreatePictureIndirect));
    p = quad_pic(&bm);
    if (!p) { t_skip("cannot create DIB section"); if (bm) DeleteObject(bm); return; }
    dc = pf_render_dc(8, 8, &old, &dib);
    if (!dc) { t_skip("no memory DC"); IPicture_Release(p); DeleteObject(bm); return; }
    EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
    if (render(p, dc, 0, 0, 4, 4, 0, 0, w, h) == S_OK) {
        SOFT_EQ(px(dc, 0, 0), quad[2]); SOFT_EQ(px(dc, 3, 0), quad[3]);
        SOFT_EQ(px(dc, 0, 3), quad[0]); SOFT_EQ(px(dc, 3, 3), quad[1]);
    }
    pf_render_dc_free(dc, old, dib);
    IPicture_Release(p); DeleteObject(bm);
}
TEST(ppict_render_uninitialized_leaves_dc_unchanged) {
    IPicture *p = NULL; HBITMAP old, dib; HDC dc; REQUIRE(U(OleCreatePictureIndirect));
    dc = pf_render_dc(8, 8, &old, &dib);
    if (!dc) { t_skip("no memory DC"); return; }
    SetPixelV(dc, 2, 2, RGB(0x22, 0x33, 0x44));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (p) {
        EQ(render(p, dc, 0, 0, 8, 8, 0, 0, 100, 100), S_OK);
        EQ(px(dc, 2, 2), RGB(0x22, 0x33, 0x44));
        EQ(px(dc, 0, 0), 0);
        IPicture_Release(p);
    }
    pf_render_dc_free(dc, old, dib);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppict_render_icon_respects_target_rect) {
    IPicture *p; HBITMAP old, dib; HDC dc; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0; COLORREF bg;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = pf_render_dc(32, 32, &old, &dib);
    if (!dc) { t_skip("no memory DC"); return; }
    if (mkpic(PICTYPE_ICON, &p, NULL) != S_OK || !p) { t_skip("no application icon"); pf_render_dc_free(dc, old, dib); return; }
    SetPixelV(dc, 0, 0, RGB(0x22, 0x33, 0x44)); SetPixelV(dc, 5, 5, RGB(0x22, 0x33, 0x44)); SetPixelV(dc, 10, 10, RGB(0x22, 0x33, 0x44));
    bg = px(dc, 0, 0);
    EQ(IPicture_get_Width(p, &w), S_OK); EQ(IPicture_get_Height(p, &h), S_OK);
    EQ(render(p, dc, 1, 1, 9, 9, 0, h, w, -h), S_OK);
    EQ(px(dc, 0, 0), bg);
    CHECKF(px(dc, 5, 5) != bg, "pixel (5,5) unchanged");
    EQ(px(dc, 10, 10), bg);
    IPicture_Release(p);
    pf_render_dc_free(dc, old, dib);
}
TEST(ppict_render_bounds_rect_accepted) {
    HBITMAP bm, old, dib; IPicture *p; HDC dc; OLE_XSIZE_HIMETRIC w = 0; OLE_YSIZE_HIMETRIC h = 0; RECT clip = { 0, 0, 4, 4 }; HRESULT hr;
    REQUIRE(U(OleCreatePictureIndirect));
    p = quad_pic(&bm);
    if (!p) { t_skip("cannot create DIB section"); if (bm) DeleteObject(bm); return; }
    dc = pf_render_dc(8, 8, &old, &dib);
    if (!dc) { t_skip("no memory DC"); IPicture_Release(p); DeleteObject(bm); return; }
    IPicture_get_Width(p, &w); IPicture_get_Height(p, &h);
    hr = IPicture_Render(p, dc, 0, 0, 2, 2, 0, h, w, -h, &clip);
    EQ(hr, S_OK);
    pf_render_dc_free(dc, old, dib);
    IPicture_Release(p); DeleteObject(bm);
}

/* ---- IPictureDisp --------------------------------------------------------------------------- */
typedef struct { DISPID id; const WCHAR *name; VARTYPE vt; } pprop_t;
static const pprop_t pprops[] = {
    { DISPID_PICT_HANDLE, L"Handle", VT_I4 }, { DISPID_PICT_HPAL, L"hPal", VT_I4 }, { DISPID_PICT_TYPE, L"Type", VT_I2 },
    { DISPID_PICT_WIDTH, L"Width", VT_I4 }, { DISPID_PICT_HEIGHT, L"Height", VT_I4 },
};
TEST(ppictdisp_dispid_constants) {
    EQ(DISPID_PICT_HANDLE, 0); EQ(DISPID_PICT_HPAL, 2); EQ(DISPID_PICT_TYPE, 3);
    EQ(DISPID_PICT_WIDTH, 4); EQ(DISPID_PICT_HEIGHT, 5); EQ(DISPID_PICT_RENDER, 6);
}
TEST(ppictdisp_getidsofnames) {
    IPicture *p; IDispatch *d = NULL; int i; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
    if (d) {
        for (i = 0; i < PF_N(pprops); i++) {
            LPOLESTR nm = (LPOLESTR)pprops[i].name; DISPID id = 0x1234;
            HRESULT hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, 0, &id);
            CHECKF(hr == S_OK && id == pprops[i].id, "%ls hr=%08lx id=%ld", pprops[i].name, hr, id);
        }
        { LPOLESTR nm = L"Render"; DISPID id = 0; EQ(IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, 0, &id), S_OK); EQ(id, DISPID_PICT_RENDER); }
        { LPOLESTR nm = L"hpal"; DISPID id = 0; EQ(IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, 0, &id), S_OK); EQ(id, DISPID_PICT_HPAL); }
        { LPOLESTR nm = L"Nonexistent"; DISPID id = 0; EQ(IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, 0, &id), PF_DISP_E_UNKNOWNNAME); EQ(id, DISPID_UNKNOWN); }
        IDispatch_Release(d);
    }
    IPicture_Release(p);
}
TEST(ppictdisp_typeinfo) {
    IPicture *p; IDispatch *d = NULL; UINT n = 9; ITypeInfo *ti = NULL; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
    if (d) {
        EQ(IDispatch_GetTypeInfoCount(d, &n), S_OK); EQ(n, 1);
        EQ(IDispatch_GetTypeInfo(d, 0, 0, &ti), S_OK);
        CHECK(ti != NULL);
        if (ti) ITypeInfo_Release(ti);
        IDispatch_Release(d);
    }
    IPicture_Release(p);
}
TEST(ppictdisp_invoke_get_values) {
    IPicture *p; IDispatch *d = NULL; int i; OLE_HANDLE h = 0; short ty = 0; REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
    if (d) {
        DISPPARAMS dp = { NULL, NULL, 0, 0 };
        for (i = 0; i < PF_N(pprops); i++) {
            VARIANT v; HRESULT hr; VariantInit(&v);
            hr = IDispatch_Invoke(d, pprops[i].id, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
            CHECKF(hr == S_OK, "%ls hr=%08lx", pprops[i].name, hr);
            if (hr == S_OK) CHECKF(V_VT(&v) == pprops[i].vt, "%ls vt=%u", pprops[i].name, V_VT(&v));
            if (hr == S_OK && pprops[i].id == DISPID_PICT_TYPE) EQ(V_I2(&v), PICTYPE_BITMAP);
            if (hr == S_OK && pprops[i].id == DISPID_PICT_HPAL) EQ((ULONG)V_I4(&v), 0xbeefdead);
            VariantClear(&v);
        }
        IDispatch_Release(d);
    }
    IPicture_get_Handle(p, &h); IPicture_get_Type(p, &ty);
    IPicture_Release(p);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppictdisp_invoke_error_codes) {
    IPicture *p = NULL; IDispatch *d = NULL; VARIANTARG arg; VARIANT res; DISPPARAMS dp; HRESULT hr; REQUIRE(U(OleCreatePictureIndirect));
    EQ(U(OleCreatePictureIndirect)(NULL, &IID_IPicture, TRUE, (void **)&p), S_OK);
    if (!p) return;
    IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
    if (!d) { CHECK(d); IPicture_Release(p); return; }
    V_VT(&arg) = VT_BOOL; V_BOOL(&arg) = VARIANT_FALSE;
    dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL; dp.cArgs = 1; dp.rgvarg = &arg;
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_IPictureDisp, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_UNKNOWNNAME);
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_IUnknown, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_UNKNOWNNAME);
    dp.cArgs = 0; dp.rgvarg = NULL;
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_BADPARAMCOUNT);
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_PROPERTYPUT, NULL, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_PARAMNOTOPTIONAL);
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_PROPERTYGET, NULL, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_PARAMNOTOPTIONAL);
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_PROPERTYGET, NULL, &res, NULL, NULL);
    EQ(hr, PF_DISP_E_PARAMNOTOPTIONAL);
    VariantInit(&res);
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
    EQ(hr, S_OK);
    EQ(V_VT(&res), VT_I4);
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL);
    EQ(hr, PF_DISP_E_MEMBERNOTFOUND);
    hr = IDispatch_Invoke(d, (DISPID)0xdeadbeef, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
    EQ(hr, PF_DISP_E_MEMBERNOTFOUND);
    dp.cArgs = 1; dp.rgvarg = &arg;
    hr = IDispatch_Invoke(d, DISPID_PICT_HPAL, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
    EQ(hr, PF_DISP_E_BADPARAMCOUNT);
    IDispatch_Release(d);
    IPicture_Release(p);
}
/* adapted from Wine dlls/oleaut32/tests/olepicture.c (LGPL-2.1+) */
TEST(ppictdisp_invoke_render) {
    IPicture *p; IDispatch *d = NULL; VARIANTARG a[10]; VARIANT res; DISPPARAMS dp; HBITMAP old, dib; HDC dc; HRESULT hr; int i;
    REQUIRE(U(OleCreatePictureIndirect));
    dc = pf_render_dc(100, 100, &old, &dib);
    if (!dc) { t_skip("no memory DC"); return; }
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) { pf_render_dc_free(dc, old, dib); return; }
    IPicture_QueryInterface(p, &IID_IDispatch, (void **)&d);
    if (d) {
        for (i = 0; i < 10; i++) V_VT(&a[i]) = VT_I4;
        /* Args are in reverse order: hdc is last in the C signature => rgvarg[9] */
        V_I4(&a[0]) = 0; V_I4(&a[1]) = 10; V_I4(&a[2]) = 10; V_I4(&a[3]) = 0; V_I4(&a[4]) = 0;
        V_I4(&a[5]) = 10; V_I4(&a[6]) = 10; V_I4(&a[7]) = 0; V_I4(&a[8]) = 0; V_I4(&a[9]) = (LONG)(LONG_PTR)dc;
        dp.rgvarg = a; dp.rgdispidNamedArgs = NULL; dp.cArgs = 10; dp.cNamedArgs = 0;
        VariantInit(&res);
        hr = IDispatch_Invoke(d, DISPID_PICT_RENDER, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL);
        EQ(hr, S_OK);
        V_VT(&a[3]) = VT_I2;
        hr = IDispatch_Invoke(d, DISPID_PICT_RENDER, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL);
        EQ(hr, PF_DISP_E_TYPEMISMATCH);
        V_VT(&a[3]) = VT_I4;
        dp.cArgs = 9;
        hr = IDispatch_Invoke(d, DISPID_PICT_RENDER, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL);
        EQ(hr, PF_DISP_E_BADPARAMCOUNT);
        IDispatch_Release(d);
    }
    IPicture_Release(p);
    pf_render_dc_free(dc, old, dib);
}

/* ---- connection points ---------------------------------------------------------------------- */
typedef struct { IPropertyNotifySink iface; LONG ref; int changed; DISPID last; } pns_t;
static HRESULT WINAPI pn_qi(IPropertyNotifySink *i, REFIID r, void **pp) {
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPropertyNotifySink)) { *pp = i; ((pns_t *)i)->ref++; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pn_ar(IPropertyNotifySink *i) { return ++((pns_t *)i)->ref; }
static ULONG WINAPI pn_rel(IPropertyNotifySink *i) { return --((pns_t *)i)->ref; }
static HRESULT WINAPI pn_changed(IPropertyNotifySink *i, DISPID d) { ((pns_t *)i)->changed++; ((pns_t *)i)->last = d; return S_OK; }
static HRESULT WINAPI pn_req(IPropertyNotifySink *i, DISPID d) { return S_OK; }
static IPropertyNotifySinkVtbl pn_vtbl = { pn_qi, pn_ar, pn_rel, pn_changed, pn_req };
TEST(ppict_connection_point_property_notify) {
    IPicture *p; IConnectionPointContainer *c = NULL; IConnectionPoint *cp = NULL; pns_t s; DWORD ck = 0; HRESULT hr;
    REQUIRE(U(OleCreatePictureIndirect));
    EQ(mkpic(PICTYPE_BITMAP, &p, NULL), S_OK);
    if (!p) return;
    EQ(IPicture_QueryInterface(p, &IID_IConnectionPointContainer, (void **)&c), S_OK);
    if (c) {
        hr = IConnectionPointContainer_FindConnectionPoint(c, &IID_IPropertyNotifySink, &cp);
        t_note("FindConnectionPoint(IPropertyNotifySink) hr=%08lx", hr);
        if (hr == S_OK && cp) {
            memset(&s, 0, sizeof s); s.iface.lpVtbl = &pn_vtbl; s.ref = 1;
            EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
            EQ(IPicture_PictureChanged(p), S_OK);
            t_note("PictureChanged -> OnChanged calls=%d last=%ld", s.changed, s.last);
            s.changed = 0;
            EQ(IPicture_set_hPal(p, 0), S_OK);
            t_note("set_hPal -> OnChanged calls=%d last=%ld", s.changed, s.last);
            EQ(IConnectionPoint_Unadvise(cp, ck), S_OK);
            EQ(IConnectionPoint_Unadvise(cp, ck), PF_CONNECT_E_NOCONNECTION);
            IConnectionPoint_Release(cp);
        }
        hr = IConnectionPointContainer_FindConnectionPoint(c, &IID_IFont, &cp);
        EQ(hr, PF_CONNECT_E_NOCONNECTION);
        IConnectionPointContainer_Release(c);
    }
    IPicture_Release(p);
}

/* ---- CLSID_StdPicture ------------------------------------------------------------------------ */
TEST(ppictclass_cocreateinstance) {
    IPicture *p = NULL; short ty = 0x7777; OLE_HANDLE h = 0xdeadbeef; HRESULT hr;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoCreateInstance)); REQUIRE(U(CoUninitialize));
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    hr = U(CoCreateInstance)(&PF_CLSID_StdPicture, NULL, CLSCTX_INPROC_SERVER, &IID_IPicture, (void **)&p);
    EQ(hr, S_OK);
    if (p) {
        EQ(IPicture_get_Type(p, &ty), S_OK);
        t_note("CoCreateInstance(StdPicture) initial type=%d", ty);
        SOFT_EQ(ty, PICTYPE_UNINITIALIZED);
        EQ(IPicture_get_Handle(p, &h), S_OK); EQ(h, 0);
        EQ(IPicture_Release(p), 0);
    }
    U(CoUninitialize)();
}
TEST(ppictclass_class_factory) {
    IClassFactory *cf = NULL; IPicture *p = NULL; HRESULT hr;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetClassObject)); REQUIRE(U(CoUninitialize));
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    hr = U(CoGetClassObject)(&PF_CLSID_StdPicture, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    EQ(hr, S_OK);
    if (cf) {
        EQ(IClassFactory_CreateInstance(cf, NULL, &IID_IPicture, (void **)&p), S_OK);
        if (p) IPicture_Release(p);
        p = (IPicture *)(ULONG_PTR)1;
        EQ(IClassFactory_CreateInstance(cf, NULL, &IID_IFont, (void **)&p), PF_E_NOINTERFACE);
        EQ(p, NULL);
        EQ(IClassFactory_LockServer(cf, TRUE), S_OK);
        EQ(IClassFactory_LockServer(cf, FALSE), S_OK);
        IClassFactory_Release(cf);
    }
    U(CoUninitialize)();
}
