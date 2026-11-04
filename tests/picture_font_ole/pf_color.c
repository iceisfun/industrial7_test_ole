/* OleTranslateColor (OLE_COLOR -> COLORREF), OleIconToCursor and the OleCreatePropertyFrame* validation
 * paths. No property sheet is ever shown: only NULL-argument validation is exercised, and in a child
 * process with a short timeout. */
#include "pf.h"

#define SYS_COLOR_COUNT 25   /* COLOR_SCROLLBAR (0) .. COLOR_INFOBK (24); Win10 rejects 25..30 (incl. HOTLIGHT, gradients, MENUHILIGHT, MENUBAR) */

static HRESULT tc(OLE_COLOR c, HPALETTE pal, COLORREF *out) { return U(OleTranslateColor)(c, pal, out); }

/* ---- RGB colors ----------------------------------------------------------------------------- */
TEST(pcolor_rgb_passthrough_table) {
    static const OLE_COLOR t[] = { 0x00000000, 0x00ffffff, 0x00010203, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00123456, 0x00808080 };
    int i; REQUIRE(U(OleTranslateColor));
    for (i = 0; i < PF_N(t); i++) {
        COLORREF c = 0xdeadbeef; HRESULT hr = tc(t[i], NULL, &c);
        CHECKF(hr == S_OK, "0x%08lx hr=%08lx", t[i], hr);
        CHECKF(c == t[i], "0x%08lx -> 0x%08lx", t[i], c);
    }
}
TEST(pcolor_rgb_component_order_is_colorref) {
    COLORREF c = 0; REQUIRE(U(OleTranslateColor));
    EQ(tc(RGB(0x11, 0x22, 0x33), NULL, &c), S_OK);
    EQ(GetRValue(c), 0x11); EQ(GetGValue(c), 0x22); EQ(GetBValue(c), 0x33);
}
TEST(pcolor_rgb_high_byte_zero_means_plain_rgb) {
    COLORREF c = 0xdeadbeef; REQUIRE(U(OleTranslateColor));
    EQ(tc(0x00000000, NULL, &c), S_OK); EQ(c, 0);
}

/* ---- system colors -------------------------------------------------------------------------- */
TEST(pcolor_system_colors_match_getsyscolor) {
    int i; REQUIRE(U(OleTranslateColor));
    for (i = 0; i < SYS_COLOR_COUNT; i++) {
        COLORREF c = 0xdeadbeef; HRESULT hr = tc(0x80000000u | i, NULL, &c);
        CHECKF(hr == S_OK, "COLOR_%d hr=%08lx", i, hr);
        CHECKF(c == GetSysColor(i), "COLOR_%d: 0x%08lx vs GetSysColor 0x%08lx", i, c, GetSysColor(i));
    }
}
TEST(pcolor_system_color_named_indices) {
    static const struct { int idx; const char *n; } t[] = {
        { COLOR_SCROLLBAR, "scrollbar" }, { COLOR_BACKGROUND, "background" }, { COLOR_WINDOW, "window" },
        { COLOR_WINDOWTEXT, "windowtext" }, { COLOR_BTNFACE, "btnface" }, { COLOR_HIGHLIGHT, "highlight" },
    };
    int i; REQUIRE(U(OleTranslateColor));
    for (i = 0; i < PF_N(t); i++) {
        COLORREF c = 0xdeadbeef;
        EQ(tc(0x80000000u | t[i].idx, NULL, &c), S_OK);
        CHECKF(c == GetSysColor(t[i].idx), "%s", t[i].n);
        CHECKF((c & 0xff000000) == 0, "%s: high byte set in 0x%08lx", t[i].n, c);
    }
}
TEST(pcolor_system_color_out_of_range) {
    static const OLE_COLOR t[] = { 0x80000019, 0x8000001a, 0x8000001b, 0x8000001c, 0x8000001d, 0x8000001e, 0x8000001f, 0x80000020, 0x80000040, 0x800000ff };
    int i; REQUIRE(U(OleTranslateColor));
    for (i = 0; i < PF_N(t); i++) {
        COLORREF c = 0x5a5a5a5a; HRESULT hr = tc(t[i], NULL, &c);
        CHECKF(hr == PF_E_INVALIDARG, "0x%08lx hr=%08lx", t[i], hr);
        t_note("0x%08lx -> out untouched: %d", t[i], c == 0x5a5a5a5a);
    }
}
TEST(pcolor_system_color_index_uses_low_byte_only) {
    COLORREF a = 0, b = 1; HRESULT ha, hb; REQUIRE(U(OleTranslateColor));
    ha = tc(0x80000005, NULL, &a);
    hb = tc(0x80ab0005, NULL, &b);   /* middle bytes set */
    EQ(ha, S_OK);
    t_note("0x80ab0005 hr=%08lx value=0x%08lx (vs 0x80000005 -> 0x%08lx)", hb, b, a);
}
TEST(pcolor_system_color_with_palette) {
    COLORREF c = 0xdeadbeef; HPALETTE pal; REQUIRE(U(OleTranslateColor));
    pal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
    EQ(tc(0x80000000u | COLOR_WINDOW, pal, &c), S_OK);
    EQ(c, GetSysColor(COLOR_WINDOW));
}

/* ---- palette relative ---------------------------------------------------------------------- */
TEST(pcolor_palette_index_without_palette) {
    COLORREF c = 0xdeadbeef; HRESULT hr; REQUIRE(U(OleTranslateColor));
    hr = tc(0x01000003, NULL, &c);
    t_note("0x01000003 (palette index 3, no palette) hr=%08lx value=0x%08lx", hr, c);
    CHECK(hr == S_OK || hr == PF_E_INVALIDARG);
}
TEST(pcolor_palette_index_with_palette) {
    struct { WORD ver, n; PALETTEENTRY e[4]; } lp; HPALETTE pal; COLORREF c = 0xdeadbeef; HRESULT hr; REQUIRE(U(OleTranslateColor));
    lp.ver = 0x300; lp.n = 4;
    lp.e[0].peRed = 10; lp.e[0].peGreen = 20; lp.e[0].peBlue = 30; lp.e[0].peFlags = 0;
    lp.e[1].peRed = 40; lp.e[1].peGreen = 50; lp.e[1].peBlue = 60; lp.e[1].peFlags = 0;
    lp.e[2].peRed = 70; lp.e[2].peGreen = 80; lp.e[2].peBlue = 90; lp.e[2].peFlags = 0;
    lp.e[3].peRed = 100; lp.e[3].peGreen = 110; lp.e[3].peBlue = 120; lp.e[3].peFlags = 0;
    pal = CreatePalette((LOGPALETTE *)&lp);
    if (!pal) { t_skip("CreatePalette failed"); return; }
    hr = tc(0x01000002, pal, &c);
    EQ(hr, S_OK);
    t_note("palette index 2 -> 0x%08lx", c);
    c = 0xdeadbeef;
    hr = tc(0x01000010, pal, &c);
    t_note("palette index 16 on a 4 entry palette -> hr=%08lx value=0x%08lx", hr, c);
    CHECK(hr == S_OK || hr == PF_E_INVALIDARG);
    DeleteObject(pal);
}
TEST(pcolor_palette_rgb_form) {
    static const OLE_COLOR t[] = { 0x02000000, 0x02123456, 0x02ffffff };
    int i; REQUIRE(U(OleTranslateColor));
    for (i = 0; i < PF_N(t); i++) {
        COLORREF c = 0xdeadbeef; HRESULT hr = tc(t[i], NULL, &c);
        CHECKF(hr == S_OK, "0x%08lx hr=%08lx", t[i], hr);
        t_note("0x%08lx -> 0x%08lx", t[i], c);
        if (hr == S_OK) EQ(c & 0xffffff, t[i] & 0xffffff);
    }
}
TEST(pcolor_rgb_with_palette) {
    COLORREF c = 0xdeadbeef; HPALETTE pal; HRESULT hr; REQUIRE(U(OleTranslateColor));
    pal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
    hr = tc(RGB(1, 2, 3), pal, &c);
    EQ(hr, S_OK);
    t_note("RGB(1,2,3) via default palette -> 0x%08lx", c);
    EQ(c & 0xffffff, RGB(1, 2, 3));
}

/* ---- invalid colors ----------------------------------------------------------------------- */
TEST(pcolor_invalid_high_bytes) {
    static const OLE_COLOR t[] = { 0x03000000, 0x04000000, 0x10000000, 0x7f000000, 0x81000000, 0xff000000, 0xff123456, 0xfe000000 };
    int i; REQUIRE(U(OleTranslateColor));
    for (i = 0; i < PF_N(t); i++) {
        COLORREF c = 0x5a5a5a5a; HRESULT hr = tc(t[i], NULL, &c);
        CHECKF(hr == PF_E_INVALIDARG, "0x%08lx hr=%08lx", t[i], hr);
    }
}
TEST(pcolor_invalid_color_with_palette) {
    COLORREF c = 0; HPALETTE pal; REQUIRE(U(OleTranslateColor));
    pal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
    EQ(tc(0x7f000000, pal, &c), PF_E_INVALIDARG);
    EQ(tc(0xff000000, pal, &c), PF_E_INVALIDARG);
}
TEST(pcolor_null_result_pointer) {
    HRESULT hr; REQUIRE(U(OleTranslateColor));
    hr = tc(0x00010203, NULL, NULL);
    t_note("OleTranslateColor(valid, NULL result) hr=%08lx", hr);
    hr = tc(0xff000000, NULL, NULL);
    CHECKF(FAILED(hr), "invalid color with NULL result: %08lx", hr);
}
TEST(pcolor_result_unchanged_on_failure_noted) {
    COLORREF c = 0x11223344; HRESULT hr; REQUIRE(U(OleTranslateColor));
    hr = tc(0xff000000, NULL, &c);
    EQ(hr, PF_E_INVALIDARG);
    t_note("out value after failure: 0x%08lx", c);
}
TEST(pcolor_ole_color_constants) {
    EQ(CLR_INVALID, 0xffffffff);
    EQ(PICTYPE_ICON, 3);
    CHECK(sizeof(OLE_COLOR) == 4);
}

/* ---- OleIconToCursor --------------------------------------------------------------------- */
static HICON make_icon(void) {
    BYTE a[32 * 4], x[32 * 4];
    memset(a, 0xff, sizeof a); memset(x, 0x00, sizeof x);
    return CreateIcon(NULL, 32, 32, 1, 1, a, x);
}
TEST(pcursor_icon_to_cursor_basic) {
    HICON ic; HCURSOR cur; REQUIRE(U(OleIconToCursor));
    ic = make_icon();
    if (!ic) { t_skip("CreateIcon failed"); return; }
    cur = U(OleIconToCursor)(NULL, ic);
    CHECK(cur != NULL);
    if (cur) CHECK(DestroyCursor(cur));
    {
        ICONINFO ii; memset(&ii, 0, sizeof ii);
        CHECKF(GetIconInfo(ic, &ii), "source icon destroyed by conversion");
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    }
    DestroyIcon(ic);
}
TEST(pcursor_icon_to_cursor_distinct_handles) {
    HICON ic; HCURSOR a, b; REQUIRE(U(OleIconToCursor));
    ic = make_icon();
    if (!ic) { t_skip("CreateIcon failed"); return; }
    a = U(OleIconToCursor)(NULL, ic); b = U(OleIconToCursor)(NULL, ic);
    CHECK(a != NULL && b != NULL);
    if (a && b) CHECK(a != b);
    CHECK(a != (HCURSOR)ic);
    if (a) DestroyCursor(a);
    if (b) DestroyCursor(b);
    DestroyIcon(ic);
}
TEST(pcursor_icon_to_cursor_with_instance) {
    HICON ic; HCURSOR cur; REQUIRE(U(OleIconToCursor));
    ic = make_icon();
    if (!ic) { t_skip("CreateIcon failed"); return; }
    cur = U(OleIconToCursor)(GetModuleHandleA(NULL), ic);
    CHECK(cur != NULL);
    if (cur) DestroyCursor(cur);
    DestroyIcon(ic);
}
TEST(pcursor_icon_to_cursor_result_info) {
    HICON ic; HCURSOR cur; ICONINFO ii; REQUIRE(U(OleIconToCursor));
    ic = make_icon();
    if (!ic) { t_skip("CreateIcon failed"); return; }
    cur = U(OleIconToCursor)(NULL, ic);
    if (cur) {
        memset(&ii, 0, sizeof ii);
        if (GetIconInfo(cur, &ii)) {
            t_note("cursor fIcon=%d hotspot=(%lu,%lu)", ii.fIcon, ii.xHotspot, ii.yHotspot);
            if (ii.hbmMask) DeleteObject(ii.hbmMask);
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
        } else CHECK(0);
        DestroyCursor(cur);
    }
    DestroyIcon(ic);
}
TEST(pcursor_icon_to_cursor_null_icon) {
    HCURSOR cur; REQUIRE(U(OleIconToCursor));
    SetLastError(0);
    cur = U(OleIconToCursor)(NULL, NULL);
    CHECK(cur == NULL);
    t_note("OleIconToCursor(NULL, NULL): last error %lu", GetLastError());
}
TEST(pcursor_icon_to_cursor_system_icon) {
    HICON ic = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION); HCURSOR cur; REQUIRE(U(OleIconToCursor));
    if (!ic) { t_skip("no stock icon"); return; }
    cur = U(OleIconToCursor)(NULL, ic);
    CHECK(cur != NULL);
    if (cur) DestroyCursor(cur);
}

/* ---- property frame ---------------------------------------------------------------------- */
TEST(pframe_ocpfiparams_layout) {
    OCPFIPARAMS p;
    EQ(sizeof(OCPFIPARAMS), 0x48);
    EQ((char *)&p.hWndOwner - (char *)&p, 8);
    EQ((char *)&p.x - (char *)&p, 16);
    EQ((char *)&p.lpszCaption - (char *)&p, 24);
    EQ((char *)&p.cObjects - (char *)&p, 32);
    EQ((char *)&p.lplpUnk - (char *)&p, 40);
    EQ((char *)&p.cPages - (char *)&p, 48);
    EQ((char *)&p.lpPages - (char *)&p, 56);
    EQ((char *)&p.lcid - (char *)&p, 64);
    EQ((char *)&p.dispidInitialProperty - (char *)&p, 68);
}
CHILD(pframe_null_params) {
    HRESULT hr = U(OleCreatePropertyFrameIndirect)(NULL);
    return (int)hr;
}
TEST(pframe_indirect_null_params) {
    DWORD rc; REQUIRE(U(OleCreatePropertyFrameIndirect));
    rc = t_run_child("pframe_null_params", "", 8000);
    if (rc == 0xDEAD0000 || rc == 0xDEAD0001 || (rc & 0xff000000) == 0xc0000000) {
        t_note("OleCreatePropertyFrameIndirect(NULL) did not return (child exit 0x%08lx)", rc);
        return;
    }
    CHECKF((HRESULT)rc < 0, "NULL params accepted: 0x%08lx", rc);
    t_note("OleCreatePropertyFrameIndirect(NULL) hr=0x%08lx", rc);
    SOFT_EQ((HRESULT)rc, PF_E_POINTER);
}
TEST(pframe_exports_present) {
    HMODULE m = t_module("oleaut32.dll");
    CHECK(m != NULL);
    if (!m) return;
    CHECK(GetProcAddress(m, "OleCreatePropertyFrame") != NULL);
    CHECK(GetProcAddress(m, "OleCreatePropertyFrameIndirect") != NULL);
    CHECK(GetProcAddress(m, "OleTranslateColor") != NULL);
    CHECK(GetProcAddress(m, "OleIconToCursor") != NULL);
    CHECK(GetProcAddress(m, "OleCreateFontIndirect") != NULL);
    CHECK(GetProcAddress(m, "OleCreatePictureIndirect") != NULL);
    CHECK(GetProcAddress(m, "OleLoadPicture") != NULL);
    CHECK(GetProcAddress(m, "OleLoadPictureEx") != NULL);
    CHECK(GetProcAddress(m, "OleLoadPicturePath") != NULL);
    CHECK(GetProcAddress(m, "OleLoadPictureFile") != NULL);
    CHECK(GetProcAddress(m, "OleSavePictureFile") != NULL);
}
TEST(pframe_exports_via_ordinals) {
    /* Ordinals from the Win10 oleaut32 export table (decomp annotations) */
    static const struct { unsigned ord; const char *name; } t[] = {
        { 415, "OleIconToCursor" }, { 416, "OleCreatePropertyFrameIndirect" }, { 417, "OleCreatePropertyFrame" },
        { 418, "OleLoadPicture" }, { 419, "OleCreatePictureIndirect" }, { 420, "OleCreateFontIndirect" },
        { 421, "OleTranslateColor" }, { 423, "OleSavePictureFile" }, { 424, "OleLoadPicturePath" }, { 401, "OleLoadPictureEx" },
    };
    int i;
    for (i = 0; i < PF_N(t); i++) {
        FARPROC byord = t_proc_ord("oleaut32.dll", t[i].ord), byname = t_proc_in("oleaut32.dll", t[i].name);
        CHECKF(byord != NULL && byord == byname, "ordinal %u vs %s", t[i].ord, t[i].name);
    }
}
