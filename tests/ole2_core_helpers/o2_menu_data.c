/* Menu descriptors, OleTranslateAccelerator, ReleaseStgMedium, OleDuplicateData, icon/metafile helpers and
 * the OleReg* registry readers. GDI services come from gdi32 through GetProcAddress as well. */
#include "olh.h"

#define G(n) UIN("gdi32.dll", n)

static HMENU mk_combined(int popups) {
    HMENU m = CreateMenu(); int i;
    for (i = 0; i < popups; i++) AppendMenuA(m, MF_POPUP | MF_STRING, (UINT_PTR)CreatePopupMenu(), "p");
    return m;
}
static void free_combined(HMENU m) { /* popups are owned by the menu */ DestroyMenu(m); }

/* ================================================================================================
 * OleCreateMenuDescriptor / OleDestroyMenuDescriptor
 * ============================================================================================== */
TEST(o2menu_create_null_menu_returns_descriptor) {
    OLEMENUGROUPWIDTHS w; HGLOBAL h;
    REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    memset(&w, 0, sizeof w);
    h = U(OleCreateMenuDescriptor)(NULL, &w);
    CHECK(h != NULL);
    if (h) {
        CHECK(GlobalSize(h) >= 0x34);
        EQ(U(OleDestroyMenuDescriptor)(h), S_OK);
    }
}
TEST(o2menu_create_valid_descriptor) {
    OLEMENUGROUPWIDTHS w = { { 1, 0, 1, 0, 1, 0 } }; HMENU m = mk_combined(3); HGLOBAL h; void *p;
    REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    h = U(OleCreateMenuDescriptor)(m, &w);
    CHECK(h != NULL);
    if (h) {
        CHECK(GlobalSize(h) >= 0x34 + 12 * (3 - 1));
        p = GlobalLock(h);
        CHECK(p != NULL);
        if (p) { GlobalUnlock(h); }
        EQ(U(OleDestroyMenuDescriptor)(h), S_OK);
    }
    free_combined(m);
}
TEST(o2menu_create_zero_total_width_fails) {
    OLEMENUGROUPWIDTHS w; HMENU m = mk_combined(2); HGLOBAL h;
    REQUIRE(U(OleCreateMenuDescriptor));
    memset(&w, 0, sizeof w);
    h = U(OleCreateMenuDescriptor)(m, &w);
    EQ(h, NULL);
    free_combined(m);
}
TEST(o2menu_create_size_scales_with_item_count) {
    OLEMENUGROUPWIDTHS w1 = { { 1, 0, 0, 0, 0, 0 } }, w2 = { { 1, 1, 1, 1, 1, 1 } }; HMENU m = mk_combined(6); HGLOBAL h1, h2;
    REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    h1 = U(OleCreateMenuDescriptor)(m, &w1); h2 = U(OleCreateMenuDescriptor)(m, &w2);
    CHECK(h1 != NULL && h2 != NULL);
    if (h1 && h2) {
        CHECK(GlobalSize(h2) >= GlobalSize(h1));
        CHECK(GlobalSize(h2) >= 0x34 + 12 * (6 - 1));
    }
    if (h1) U(OleDestroyMenuDescriptor)(h1);
    if (h2) U(OleDestroyMenuDescriptor)(h2);
    free_combined(m);
}
TEST(o2menu_create_independent_handles) {
    OLEMENUGROUPWIDTHS w = { { 1, 0, 0, 0, 0, 0 } }; HMENU m = mk_combined(1); HGLOBAL a, b;
    REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    a = U(OleCreateMenuDescriptor)(m, &w); b = U(OleCreateMenuDescriptor)(m, &w);
    CHECK(a && b && a != b);
    if (a) EQ(U(OleDestroyMenuDescriptor)(a), S_OK);
    if (b) EQ(U(OleDestroyMenuDescriptor)(b), S_OK);
    free_combined(m);
}
TEST(o2menu_destroy_null) {
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(OleDestroyMenuDescriptor));
    EQ(U(OleDestroyMenuDescriptor)(NULL), O2_E_HANDLE);
}
TEST(o2menu_destroy_foreign_hglobal) {
    HGLOBAL h; HRESULT hr;
    REQUIRE(U(OleDestroyMenuDescriptor));
    h = o2_hg(NULL, 0x80, GMEM_MOVEABLE);
    hr = U(OleDestroyMenuDescriptor)(h);
    EQ(hr, O2_E_HANDLE);                   /* signature check fails */
    CHECK(GlobalSize(h) == 0x80);          /* and the block is left alone */
    GlobalFree(h);
}
TEST(o2menu_destroy_frees_block) {
    OLEMENUGROUPWIDTHS w = { { 1, 0, 0, 0, 0, 0 } }; HMENU m = mk_combined(1); HGLOBAL h;
    REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    h = U(OleCreateMenuDescriptor)(m, &w);
    CHECK(h != NULL);
    if (h) {
        EQ(U(OleDestroyMenuDescriptor)(h), S_OK);
        SetLastError(0);
        EQ(GlobalSize(h), 0);              /* the block no longer exists */
    }
    free_combined(m);
}

/* ================================================================================================
 * OleSetMenuDescriptor
 * ============================================================================================== */
TEST(o2menu_set_invalid_frame_window) {
    HWND w; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleSetMenuDescriptor));
    hr = U(OleSetMenuDescriptor)(NULL, NULL, NULL, NULL, NULL);
    EQ(hr, O2_OLE_E_INVALIDHWND);
    w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    DestroyWindow(w);
    hr = U(OleSetMenuDescriptor)(NULL, w, NULL, NULL, NULL);
    EQ(hr, O2_OLE_E_INVALIDHWND);
    hr = U(OleSetMenuDescriptor)(NULL, (HWND)(ULONG_PTR)0x7FFFFFF0, NULL, NULL, NULL);   /* not a window handle */
    EQ(hr, O2_OLE_E_INVALIDHWND);
    O2_END();
}
TEST(o2menu_set_null_descriptor_clears) {
    HWND w; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleSetMenuDescriptor));
    w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    hr = U(OleSetMenuDescriptor)(NULL, w, NULL, NULL, NULL);
    EQ(hr, S_OK);
    hr = U(OleSetMenuDescriptor)(NULL, w, NULL, NULL, NULL);    /* nothing to clear: still fine */
    EQ(hr, S_OK);
    DestroyWindow(w);
    O2_END();
}
TEST(o2menu_set_invalid_active_object_window) {
    OLEMENUGROUPWIDTHS ow = { { 1, 0, 0, 0, 0, 0 } }; HMENU m = mk_combined(1); HGLOBAL d; HWND f, a; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleSetMenuDescriptor)); REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    d = U(OleCreateMenuDescriptor)(m, &ow); CHECK(d != NULL);
    f = o2_mkwnd(); a = o2_mkwnd(); if (!(f && a)) { t_skip("cannot create windows in this session"); O2_END(); return; }
    hr = U(OleSetMenuDescriptor)(d, f, NULL, NULL, NULL);
    EQ(hr, O2_OLE_E_INVALIDHWND);
    DestroyWindow(a);
    hr = U(OleSetMenuDescriptor)(d, f, a, NULL, NULL);
    EQ(hr, O2_OLE_E_INVALIDHWND);
    hr = U(OleSetMenuDescriptor)(d, NULL, a, NULL, NULL);
    EQ(hr, O2_OLE_E_INVALIDHWND);
    DestroyWindow(f);
    if (d) U(OleDestroyMenuDescriptor)(d);
    free_combined(m);
    O2_END();
}
TEST(o2menu_set_foreign_descriptor_handle) {
    HGLOBAL bad; HWND f, a; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleSetMenuDescriptor));
    f = o2_mkwnd(); a = o2_mkwnd(); if (!(f && a)) { t_skip("cannot create windows in this session"); O2_END(); return; }
    bad = o2_hg(NULL, 0x80, GMEM_MOVEABLE);
    hr = U(OleSetMenuDescriptor)(bad, f, a, NULL, NULL);
    EQ(hr, O2_E_HANDLE);
    GlobalFree(bad);
    DestroyWindow(f); DestroyWindow(a);
    O2_END();
}
TEST(o2menu_set_and_clear_descriptor) {
    OLEMENUGROUPWIDTHS ow = { { 1, 0, 1, 0, 0, 0 } }; HMENU m = mk_combined(2); HGLOBAL d; HWND f, a; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleSetMenuDescriptor)); REQUIRE(U(OleCreateMenuDescriptor)); REQUIRE(U(OleDestroyMenuDescriptor));
    d = U(OleCreateMenuDescriptor)(m, &ow); CHECK(d != NULL);
    f = o2_mkwnd(); a = o2_mkwnd(); if (!(f && a)) { t_skip("cannot create windows in this session"); O2_END(); return; }
    hr = U(OleSetMenuDescriptor)(d, f, a, NULL, NULL);
    EQ(hr, S_OK);
    t_note("pFrameFilter window property after set: %p", GetPropW(f, L"pFrameFilter"));
    hr = U(OleSetMenuDescriptor)(d, f, a, NULL, NULL);          /* replace an existing filter */
    EQ(hr, S_OK);
    hr = U(OleSetMenuDescriptor)(NULL, f, NULL, NULL, NULL);    /* remove it */
    EQ(hr, S_OK);
    if (d) U(OleDestroyMenuDescriptor)(d);
    DestroyWindow(f); DestroyWindow(a);
    free_combined(m);
    O2_END();
}

/* ================================================================================================
 * OleTranslateAccelerator
 * ============================================================================================== */
TEST(o2accel_null_frame) {
    OLEINPLACEFRAMEINFO fi; MSG msg;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleTranslateAccelerator));
    memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; memset(&msg, 0, sizeof msg); msg.message = WM_KEYDOWN; msg.wParam = 'A';
    EQ(U(OleTranslateAccelerator)(NULL, &fi, &msg), O2_E_INVALIDARG);
}
TEST(o2accel_no_table_not_translated) {
    static const UINT msgs[] = { WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_DEADCHAR, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_LBUTTONDOWN, WM_NULL };
    O2Frame fr; OLEINPLACEFRAMEINFO fi; HWND w; int i;
    O2_BEGIN;
    REQUIRE(U(OleTranslateAccelerator));
    of_init(&fr); w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; fi.hwndFrame = w;
    for (i = 0; i < O2_N(msgs); i++) {
        MSG m; HRESULT hr; memset(&m, 0, sizeof m); m.hwnd = w; m.message = msgs[i]; m.wParam = 'A'; m.lParam = 0;
        hr = U(OleTranslateAccelerator)(&fr.i, &fi, &m);
        CHECKF(hr == S_FALSE, "message 0x%x: got 0x%08lx", msgs[i], (unsigned long)hr);
    }
    EQ(fr.ta_calls, 0);
    DestroyWindow(w);
    O2_END();
}
TEST(o2accel_syschar_with_alt_forwarded_to_frame) {
    O2Frame fr; OLEINPLACEFRAMEINFO fi; HWND w; MSG m; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleTranslateAccelerator));
    of_init(&fr); w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; fi.hwndFrame = w;
    memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_SYSCHAR; m.wParam = 'x'; m.lParam = 0x20000001;
    o2_wm_count = 0;
    hr = U(OleTranslateAccelerator)(&fr.i, &fi, &m);
    EQ(hr, S_OK);                          /* handled: the character is passed on to the frame window */
    EQ(o2_wm_count, 1);
    EQ(o2_wm_last_msg, WM_SYSCHAR);
    EQ(o2_wm_last_wparam, 'x');
    EQ(o2_wm_last_lparam, 0x20000001);
    DestroyWindow(w);
    O2_END();
}
TEST(o2accel_syschar_insert_key_not_forwarded) {
    O2Frame fr; OLEINPLACEFRAMEINFO fi; HWND w; MSG m; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleTranslateAccelerator));
    of_init(&fr); w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; fi.hwndFrame = w;
    memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_SYSCHAR; m.wParam = 0x2d; m.lParam = 0x20000001;
    o2_wm_count = 0;
    hr = U(OleTranslateAccelerator)(&fr.i, &fi, &m);
    EQ(hr, S_OK);
    EQ(o2_wm_count, 0);
    DestroyWindow(w);
    O2_END();
}
TEST(o2accel_syschar_without_alt_is_plain_char) {
    O2Frame fr; OLEINPLACEFRAMEINFO fi; HWND w; MSG m; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleTranslateAccelerator));
    of_init(&fr); w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; fi.hwndFrame = w;
    memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_SYSCHAR; m.wParam = 'x'; m.lParam = 0x00000001;
    o2_wm_count = 0;
    hr = U(OleTranslateAccelerator)(&fr.i, &fi, &m);
    EQ(hr, S_FALSE);
    EQ(o2_wm_count, 0);
    DestroyWindow(w);
    O2_END();
}
TEST(o2accel_matching_accelerator_calls_frame) {
    static const HRESULT rets[] = { S_OK, S_FALSE, (HRESULT)0x12345678, E_FAIL };
    ACCEL a; HACCEL ha; int i;
    O2_BEGIN;
    REQUIRE(U(OleTranslateAccelerator));
    a.fVirt = FVIRTKEY; a.key = 'A'; a.cmd = 0x1234;
    ha = CreateAcceleratorTableA(&a, 1);
    CHECK(ha != NULL);
    for (i = 0; ha && i < O2_N(rets); i++) {
        O2Frame fr; OLEINPLACEFRAMEINFO fi; HWND w; MSG m; HRESULT hr;
        of_init(&fr); fr.ta_hr = rets[i]; w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); break; }
        memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; fi.hwndFrame = w; fi.haccel = ha; fi.cAccelEntries = 1;
        memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 'A'; m.lParam = 1;
        hr = U(OleTranslateAccelerator)(&fr.i, &fi, &m);
        if (fr.ta_calls == 0) { t_note("accelerator did not match (key state?) hr=0x%08lx", (unsigned long)hr); DestroyWindow(w); break; }
        EQ(fr.ta_calls, 1);
        EQ(fr.ta_id, 0x1234);
        EQ(fr.ta_msg.message, WM_KEYDOWN);
        EQ(fr.ta_msg.wParam, 'A');
        EQ(hr, rets[i]);               /* the frame's HRESULT is returned unchanged */
        DestroyWindow(w);
    }
    if (ha) DestroyAcceleratorTable(ha);
    O2_END();
}
TEST(o2accel_nonmatching_key_with_table) {
    ACCEL a; HACCEL ha; O2Frame fr; OLEINPLACEFRAMEINFO fi; HWND w; MSG m; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleTranslateAccelerator));
    a.fVirt = FVIRTKEY; a.key = 'A'; a.cmd = 0x1234;
    ha = CreateAcceleratorTableA(&a, 1); CHECK(ha != NULL);
    of_init(&fr); w = o2_mkwnd(); if (!w) { t_skip("cannot create windows in this session"); O2_END(); return; }
    memset(&fi, 0, sizeof fi); fi.cb = sizeof fi; fi.hwndFrame = w; fi.haccel = ha; fi.cAccelEntries = 1;
    memset(&m, 0, sizeof m); m.hwnd = w; m.message = WM_KEYDOWN; m.wParam = 'B'; m.lParam = 1;
    hr = U(OleTranslateAccelerator)(&fr.i, &fi, &m);
    EQ(hr, S_FALSE); EQ(fr.ta_calls, 0);
    DestroyWindow(w);
    if (ha) DestroyAcceleratorTable(ha);
    O2_END();
}

/* ================================================================================================
 * ReleaseStgMedium
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_ReleaseStgMedium */
TEST(o2stg_release_null_pointer) {
    REQUIRE(U(ReleaseStgMedium));
    U(ReleaseStgMedium)(NULL);
    CHECK(1);
}
TEST(o2stg_release_tymed_null_clears) {
    STGMEDIUM m; O2Run u; REQUIRE(U(ReleaseStgMedium));
    or_init(&u);
    memset(&m, 0, sizeof m); m.tymed = TYMED_NULL; m.pUnkForRelease = (IUnknown *)&u.i;
    IUnknown_AddRef((IUnknown *)&u.i);                    /* ref 2 */
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL); EQ(m.pUnkForRelease, NULL);
    EQ(u.ref, 1);                                          /* pUnkForRelease released even for TYMED_NULL */
}
TEST(o2stg_release_hglobal_frees) {
    STGMEDIUM m; HGLOBAL h; REQUIRE(U(ReleaseStgMedium));
    h = o2_hg("abc", 4, GMEM_MOVEABLE);
    EQ(GlobalSize(h), 4);
    memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = h;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    SetLastError(0);
    EQ(GlobalSize(h), 0);
}
TEST(o2stg_release_hglobal_with_punk_keeps_block) {
    STGMEDIUM m; HGLOBAL h; O2Run u; REQUIRE(U(ReleaseStgMedium));
    or_init(&u); IUnknown_AddRef((IUnknown *)&u.i);
    h = o2_hg("abc", 4, GMEM_MOVEABLE);
    memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = h; m.pUnkForRelease = (IUnknown *)&u.i;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL); EQ(m.pUnkForRelease, NULL);
    EQ(u.ref, 1);                                         /* the owner is released exactly once... */
    EQ(GlobalSize(h), 4);                                 /* ...and the data is left to it */
    GlobalFree(h);
}
TEST(o2stg_release_hglobal_null_handle) {
    STGMEDIUM m; REQUIRE(U(ReleaseStgMedium));
    memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = NULL;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
}
TEST(o2stg_release_stream_releases_interface) {
    STGMEDIUM m; IStream *s = NULL; O2Run u; ULONG r;
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CreateStreamOnHGlobal));
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    IStream_AddRef(s);                                    /* 2 */
    memset(&m, 0, sizeof m); m.tymed = TYMED_ISTREAM; m.pstm = s;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    r = IStream_Release(s);
    EQ(r, 0);                                             /* ReleaseStgMedium dropped one reference */
    or_init(&u);
    {
        IStream *s2 = NULL; STGMEDIUM m2;
        EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s2), S_OK);
        IUnknown_AddRef((IUnknown *)&u.i);
        memset(&m2, 0, sizeof m2); m2.tymed = TYMED_ISTREAM; m2.pstm = s2; m2.pUnkForRelease = (IUnknown *)&u.i;
        IStream_AddRef(s2);
        U(ReleaseStgMedium)(&m2);
        EQ(u.ref, 1);                                     /* the stream is released even with an owner, and the owner too */
        EQ(IStream_Release(s2), 0);
    }
}
TEST(o2stg_release_storage_releases_interface) {
    STGMEDIUM m; IStorage *s; REQUIRE(U(ReleaseStgMedium));
    s = o2_mkstg(); CHECK(s != NULL);
    if (!s) return;
    IStorage_AddRef(s);
    memset(&m, 0, sizeof m); m.tymed = TYMED_ISTORAGE; m.pstg = s;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    EQ(IStorage_Release(s), 0);
}
TEST(o2stg_release_gdi_deletes_bitmap) {
    STGMEDIUM m; HBITMAP b; REQUIRE(U(ReleaseStgMedium)); REQUIRE(G(CreateBitmap)); REQUIRE(G(GetObjectType));
    b = G(CreateBitmap)(4, 4, 1, 1, NULL);
    CHECK(b != NULL);
    EQ(G(GetObjectType)(b), OBJ_BITMAP);
    memset(&m, 0, sizeof m); m.tymed = TYMED_GDI; m.hBitmap = b;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    EQ(G(GetObjectType)(b), 0);
}
TEST(o2stg_release_gdi_with_punk_keeps_bitmap) {
    STGMEDIUM m; HBITMAP b; O2Run u; REQUIRE(U(ReleaseStgMedium)); REQUIRE(G(CreateBitmap)); REQUIRE(G(GetObjectType)); REQUIRE(G(DeleteObject));
    or_init(&u); IUnknown_AddRef((IUnknown *)&u.i);
    b = G(CreateBitmap)(4, 4, 1, 1, NULL);
    memset(&m, 0, sizeof m); m.tymed = TYMED_GDI; m.hBitmap = b; m.pUnkForRelease = (IUnknown *)&u.i;
    U(ReleaseStgMedium)(&m);
    EQ(u.ref, 1);
    EQ(G(GetObjectType)(b), OBJ_BITMAP);
    G(DeleteObject)(b);
}
TEST(o2stg_release_enhmetafile) {
    STGMEDIUM m; HDC dc; HENHMETAFILE e; UINT n;
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(G(CreateEnhMetaFileA)); REQUIRE(G(CloseEnhMetaFile)); REQUIRE(G(Rectangle)); REQUIRE(G(GetEnhMetaFileBits));
    dc = G(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL);
    CHECK(dc != NULL);
    G(Rectangle)(dc, 0, 0, 10, 10);
    e = G(CloseEnhMetaFile)(dc);
    CHECK(e != NULL);
    n = G(GetEnhMetaFileBits)(e, 0, NULL);
    CHECK(n > 0);
    memset(&m, 0, sizeof m); m.tymed = TYMED_ENHMF; m.hEnhMetaFile = e;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    EQ(G(GetEnhMetaFileBits)(e, 0, NULL), 0);             /* the metafile was deleted */
}
TEST(o2stg_release_mfpict_frees_global_and_metafile) {
    STGMEDIUM m; HGLOBAL g; HICON ic; METAFILEPICT *p; HMETAFILE mf;
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(OleMetafilePictFromIconAndLabel)); REQUIRE(G(GetMetaFileBitsEx));
    ic = LoadIconA(NULL, IDI_APPLICATION);
    g = U(OleMetafilePictFromIconAndLabel)(ic, L"label", L"src", 0);
    CHECK(g != NULL);
    if (!g) return;
    p = (METAFILEPICT *)GlobalLock(g); mf = p->hMF; GlobalUnlock(g);
    CHECK(G(GetMetaFileBitsEx)(mf, 0, NULL) > 0);
    memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = g;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    SetLastError(0);
    EQ(GlobalSize(g), 0);
    EQ(G(GetMetaFileBitsEx)(mf, 0, NULL), 0);
}
TEST(o2stg_release_file_deletes_and_frees_name) {
    STGMEDIUM m; WCHAR path[MAX_PATH]; char a[MAX_PATH]; HANDLE f; WCHAR *cotask;
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CoTaskMemAlloc));
    t_tmpfile(a, sizeof a, "o2stg_release_file.tmp");
    MultiByteToWideChar(CP_ACP, 0, a, -1, path, MAX_PATH);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CHECK(f != INVALID_HANDLE_VALUE); CloseHandle(f);
    cotask = (WCHAR *)U(CoTaskMemAlloc)((lstrlenW(path) + 1) * sizeof(WCHAR));
    lstrcpyW(cotask, path);
    memset(&m, 0, sizeof m); m.tymed = TYMED_FILE; m.lpszFileName = cotask;
    U(ReleaseStgMedium)(&m);
    EQ(m.tymed, TYMED_NULL);
    EQ(GetFileAttributesW(path), INVALID_FILE_ATTRIBUTES);   /* TYMED_FILE without an owner: file deleted */
}
TEST(o2stg_release_file_with_punk_keeps_file) {
    STGMEDIUM m; WCHAR path[MAX_PATH]; char a[MAX_PATH]; HANDLE f; WCHAR *cotask; O2Run u;
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CoTaskMemAlloc));
    t_tmpfile(a, sizeof a, "o2stg_release_file2.tmp");
    MultiByteToWideChar(CP_ACP, 0, a, -1, path, MAX_PATH);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(f);
    cotask = (WCHAR *)U(CoTaskMemAlloc)((lstrlenW(path) + 1) * sizeof(WCHAR));
    lstrcpyW(cotask, path);
    or_init(&u); IUnknown_AddRef((IUnknown *)&u.i);
    memset(&m, 0, sizeof m); m.tymed = TYMED_FILE; m.lpszFileName = cotask; m.pUnkForRelease = (IUnknown *)&u.i;
    U(ReleaseStgMedium)(&m);
    EQ(u.ref, 1);
    CHECK(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    DeleteFileW(path);
}
TEST(o2stg_release_twice_is_harmless) {
    STGMEDIUM m; HGLOBAL h; REQUIRE(U(ReleaseStgMedium));
    h = o2_hg("abc", 4, GMEM_MOVEABLE);
    memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = h;
    U(ReleaseStgMedium)(&m);
    U(ReleaseStgMedium)(&m);               /* tymed is now TYMED_NULL: no double free */
    EQ(m.tymed, TYMED_NULL);
}

/* ================================================================================================
 * OleDuplicateData
 * ============================================================================================== */
TEST(o2dup_null_handle) {
    static const WORD cfs[] = { CF_TEXT, CF_BITMAP, CF_METAFILEPICT, CF_DIB, CF_PALETTE, CF_ENHMETAFILE, CF_UNICODETEXT, 0xC123 };
    int i;
    REQUIRE(U(OleDuplicateData));
    for (i = 0; i < O2_N(cfs); i++) {
        HANDLE d = U(OleDuplicateData)(NULL, cfs[i], GMEM_MOVEABLE);
        CHECKF(d == NULL, "cf %u", cfs[i]);
    }
}
TEST(o2dup_generic_hglobal_formats) {
    static const WORD cfs[] = { CF_TEXT, CF_OEMTEXT, CF_UNICODETEXT, CF_DIB, CF_DIBV5, CF_HDROP, CF_LOCALE, CF_RIFF, CF_WAVE, CF_TIFF, CF_SYLK, CF_DIF, CF_PENDATA, 0xC001, 0xC0DE };
    int i;
    REQUIRE(U(OleDuplicateData));
    for (i = 0; i < O2_N(cfs); i++) {
        static const char data[] = "0123456789abcdef-payload";
        HGLOBAL src = o2_hg(data, sizeof data, GMEM_MOVEABLE), d; void *p;
        d = (HGLOBAL)U(OleDuplicateData)(src, cfs[i], GMEM_MOVEABLE);
        CHECKF(d != NULL && d != src, "cf %u", cfs[i]);
        if (d && d != src) {
            CHECKF(GlobalSize(d) == GlobalSize(src), "cf %u size %lu vs %lu", cfs[i], (unsigned long)GlobalSize(d), (unsigned long)GlobalSize(src));
            p = GlobalLock(d);
            CHECKF(p && !memcmp(p, data, sizeof data), "cf %u contents", cfs[i]);
            GlobalUnlock(d);
            GlobalFree(d);
        }
        GlobalFree(src);
    }
}
TEST(o2dup_flags_zero_default_moveable) {
    HGLOBAL src, d; static const char data[] = "xyz";
    REQUIRE(U(OleDuplicateData));
    src = o2_hg(data, sizeof data, GMEM_MOVEABLE);
    d = (HGLOBAL)U(OleDuplicateData)(src, CF_TEXT, 0);       /* flags 0 means GMEM_MOVEABLE */
    CHECK(d != NULL);
    if (d) {
        CHECK((GlobalFlags(d) & GMEM_DISCARDABLE) == 0);
        CHECK(GlobalLock(d) != d);                            /* a moveable block has a handle distinct from its address */
        GlobalUnlock(d);
        GlobalFree(d);
    }
    GlobalFree(src);
}
TEST(o2dup_flags_fixed_requested) {
    HGLOBAL src, d; static const char data[] = "xyz";
    REQUIRE(U(OleDuplicateData));
    src = o2_hg(data, sizeof data, GMEM_MOVEABLE);
    d = (HGLOBAL)U(OleDuplicateData)(src, CF_TEXT, GMEM_FIXED | GMEM_ZEROINIT);
    t_note("OleDuplicateData(GMEM_FIXED|GMEM_ZEROINIT): %p (GMEM_FIXED is 0, so the default moveable flags apply)", d);
    CHECK(d != NULL);
    if (d) { CHECK(!memcmp(GlobalLock(d), data, sizeof data)); GlobalUnlock(d); GlobalFree(d); }
    GlobalFree(src);
}
TEST(o2dup_ddeshare_flag) {
    HGLOBAL src, d; static const char data[] = "xyz";
    REQUIRE(U(OleDuplicateData));
    src = o2_hg(data, sizeof data, GMEM_MOVEABLE | GMEM_DDESHARE);
    d = (HGLOBAL)U(OleDuplicateData)(src, CF_TEXT, GMEM_MOVEABLE | GMEM_DDESHARE);
    CHECK(d != NULL && d != src);
    if (d) { EQ(GlobalSize(d), sizeof data); GlobalFree(d); }
    GlobalFree(src);
}
TEST(o2dup_bitmap) {
    HBITMAP b, d; BITMAP bm1, bm2;
    REQUIRE(U(OleDuplicateData)); REQUIRE(G(CreateBitmap)); REQUIRE(G(GetObjectA)); REQUIRE(G(DeleteObject));
    b = G(CreateBitmap)(13, 7, 1, 1, NULL);
    d = (HBITMAP)U(OleDuplicateData)(b, CF_BITMAP, 0);
    CHECK(d != NULL && d != b);
    if (d) {
        memset(&bm1, 0, sizeof bm1); memset(&bm2, 0, sizeof bm2);
        G(GetObjectA)(b, sizeof bm1, &bm1); G(GetObjectA)(d, sizeof bm2, &bm2);
        EQ(bm2.bmWidth, 13); EQ(bm2.bmHeight, 7); EQ(bm2.bmBitsPixel, bm1.bmBitsPixel); EQ(bm2.bmPlanes, bm1.bmPlanes);
        G(DeleteObject)(d);
    }
    G(DeleteObject)(b);
}
TEST(o2dup_palette) {
    LOGPALETTE *lp; HPALETTE p, d; PALETTEENTRY pe[4]; int i;
    REQUIRE(U(OleDuplicateData)); REQUIRE(G(CreatePalette)); REQUIRE(G(GetPaletteEntries)); REQUIRE(G(DeleteObject));
    lp = (LOGPALETTE *)calloc(1, sizeof(LOGPALETTE) + 3 * sizeof(PALETTEENTRY));
    lp->palVersion = 0x300; lp->palNumEntries = 4;
    for (i = 0; i < 4; i++) { lp->palPalEntry[i].peRed = (BYTE)(10 * i + 1); lp->palPalEntry[i].peGreen = (BYTE)(20 * i + 2); lp->palPalEntry[i].peBlue = (BYTE)(30 * i + 3); }
    p = G(CreatePalette)(lp);
    CHECK(p != NULL);
    d = (HPALETTE)U(OleDuplicateData)(p, CF_PALETTE, 0);
    CHECK(d != NULL && d != p);
    if (d) {
        EQ(G(GetPaletteEntries)(d, 0, 0, NULL), 4);
        EQ(G(GetPaletteEntries)(d, 0, 4, pe), 4);
        for (i = 0; i < 4; i++) { EQ(pe[i].peRed, lp->palPalEntry[i].peRed); EQ(pe[i].peGreen, lp->palPalEntry[i].peGreen); EQ(pe[i].peBlue, lp->palPalEntry[i].peBlue); }
        G(DeleteObject)(d);
    }
    G(DeleteObject)(p); free(lp);
}
TEST(o2dup_enhmetafile) {
    HDC dc; HENHMETAFILE e, d; UINT n1, n2;
    REQUIRE(U(OleDuplicateData)); REQUIRE(G(CreateEnhMetaFileA)); REQUIRE(G(CloseEnhMetaFile)); REQUIRE(G(Rectangle)); REQUIRE(G(GetEnhMetaFileBits)); REQUIRE(G(DeleteEnhMetaFile));
    dc = G(CreateEnhMetaFileA)(NULL, NULL, NULL, NULL); G(Rectangle)(dc, 1, 2, 30, 40); e = G(CloseEnhMetaFile)(dc);
    CHECK(e != NULL);
    d = (HENHMETAFILE)U(OleDuplicateData)(e, CF_ENHMETAFILE, 0);
    CHECK(d != NULL && d != e);
    if (d) {
        n1 = G(GetEnhMetaFileBits)(e, 0, NULL); n2 = G(GetEnhMetaFileBits)(d, 0, NULL);
        CHECK(n1 > 0); EQ(n2, n1);
        G(DeleteEnhMetaFile)(d);
    }
    G(DeleteEnhMetaFile)(e);
}
TEST(o2dup_metafilepict) {
    HGLOBAL g, d; METAFILEPICT *s, *t; HICON ic; HMETAFILE smf, dmf;
    REQUIRE(U(OleDuplicateData)); REQUIRE(U(OleMetafilePictFromIconAndLabel)); REQUIRE(G(GetMetaFileBitsEx)); REQUIRE(G(DeleteMetaFile));
    ic = LoadIconA(NULL, IDI_APPLICATION);
    g = U(OleMetafilePictFromIconAndLabel)(ic, L"lbl", NULL, 0);
    CHECK(g != NULL);
    if (!g) return;
    d = (HGLOBAL)U(OleDuplicateData)(g, CF_METAFILEPICT, 0);
    CHECK(d != NULL && d != g);
    if (d) {
        s = (METAFILEPICT *)GlobalLock(g); t = (METAFILEPICT *)GlobalLock(d);
        EQ(t->mm, s->mm); EQ(t->xExt, s->xExt); EQ(t->yExt, s->yExt);
        CHECK(t->hMF != NULL && t->hMF != s->hMF);          /* the metafile itself is copied too */
        smf = s->hMF; dmf = t->hMF;
        EQ(G(GetMetaFileBitsEx)(dmf, 0, NULL), G(GetMetaFileBitsEx)(smf, 0, NULL));
        GlobalUnlock(g); GlobalUnlock(d);
        G(DeleteMetaFile)(dmf); GlobalFree(d);
    }
    { STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = g; U(ReleaseStgMedium)(&m); }
}
TEST(o2dup_metafilepict_invalid_block) {
    HGLOBAL g, d;
    REQUIRE(U(OleDuplicateData));
    g = o2_hg("abcd", 4, GMEM_MOVEABLE);
    d = (HGLOBAL)U(OleDuplicateData)(g, CF_METAFILEPICT, 0);
    t_note("OleDuplicateData(CF_METAFILEPICT on a 4 byte block) -> %p", d);
    EQ(d, NULL);
    if (d) GlobalFree(d);
    GlobalFree(g);
}

/* ================================================================================================
 * OleMetafilePictFromIconAndLabel / OleGetIconOfClass / OleGetIconOfFile
 * ============================================================================================== */
static void check_mfpict(HGLOBAL g, const char *what) {
    METAFILEPICT *p; UINT n;
    CHECKF(g != NULL, "%s: NULL", what);
    if (!g) return;
    CHECKF(GlobalSize(g) >= sizeof(METAFILEPICT), "%s: block too small", what);
    p = (METAFILEPICT *)GlobalLock(g);
    CHECKF(p != NULL, "%s: lock", what);
    if (p) {
        EQ(p->mm, MM_ANISOTROPIC);
        CHECKF(p->xExt > 0 && p->yExt > 0, "%s: ext %ld x %ld", what, (long)p->xExt, (long)p->yExt);
        CHECKF(p->hMF != NULL, "%s: hMF", what);
        if (p->hMF) { n = G(GetMetaFileBitsEx)(p->hMF, 0, NULL); CHECKF(n > 0, "%s: metafile bits %u", what, n); }
        GlobalUnlock(g);
    }
}
static void free_mfpict(HGLOBAL g) { STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = g; U(ReleaseStgMedium)(&m); }

TEST(o2icon_metafilepict_null_icon) {
    REQUIRE(U(OleMetafilePictFromIconAndLabel));
    EQ(U(OleMetafilePictFromIconAndLabel)(NULL, L"label", L"source", 0), NULL);
    EQ(U(OleMetafilePictFromIconAndLabel)(NULL, NULL, NULL, 0), NULL);
}
TEST(o2icon_metafilepict_label_variants) {
    static const struct { const WCHAR *label, *src; UINT idx; } c[] = {
        { L"Label", L"C:\\path\\x.exe", 0 }, { L"Label", NULL, 0 }, { NULL, L"C:\\path\\x.exe", 3 }, { NULL, NULL, 0 },
        { L"", L"", 0 }, { L"A rather long label text that exceeds forty two characters in length", L"src", 7 },
    };
    HICON ic; int i;
    REQUIRE(U(OleMetafilePictFromIconAndLabel)); REQUIRE(G(GetMetaFileBitsEx));
    ic = LoadIconA(NULL, IDI_APPLICATION);
    for (i = 0; i < O2_N(c); i++) {
        HGLOBAL g = U(OleMetafilePictFromIconAndLabel)(ic, c[i].label, c[i].src, c[i].idx);
        char what[32]; sprintf(what, "case %d", i);
        check_mfpict(g, what);
        free_mfpict(g);
    }
}
TEST(o2icon_metafilepict_label_changes_width) {
    HICON ic; HGLOBAL a, b; METAFILEPICT *pa, *pb; LONG wa = 0, wb = 0;
    REQUIRE(U(OleMetafilePictFromIconAndLabel));
    ic = LoadIconA(NULL, IDI_APPLICATION);
    a = U(OleMetafilePictFromIconAndLabel)(ic, L"", NULL, 0);        /* no label: icon width + 25% */
    b = U(OleMetafilePictFromIconAndLabel)(ic, L"Label", NULL, 0);   /* label: at least three icon widths */
    CHECK(a && b);
    if (a && b) {
        pa = (METAFILEPICT *)GlobalLock(a); pb = (METAFILEPICT *)GlobalLock(b);
        wa = pa->xExt; wb = pb->xExt;
        GlobalUnlock(a); GlobalUnlock(b);
        CHECKF(wb > wa, "xExt with label %ld, without %ld", (long)wb, (long)wa);
    }
    free_mfpict(a); free_mfpict(b);
}
TEST(o2icon_metafilepict_independent_results) {
    HICON ic; HGLOBAL a, b; METAFILEPICT *pa, *pb;
    REQUIRE(U(OleMetafilePictFromIconAndLabel));
    ic = LoadIconA(NULL, IDI_APPLICATION);
    a = U(OleMetafilePictFromIconAndLabel)(ic, L"x", NULL, 0); b = U(OleMetafilePictFromIconAndLabel)(ic, L"x", NULL, 0);
    CHECK(a && b && a != b);
    if (a && b) {
        pa = (METAFILEPICT *)GlobalLock(a); pb = (METAFILEPICT *)GlobalLock(b);
        CHECK(pa->hMF != pb->hMF);
        EQ(pa->xExt, pb->xExt); EQ(pa->yExt, pb->yExt);
        GlobalUnlock(a); GlobalUnlock(b);
    }
    free_mfpict(a); free_mfpict(b);
}
TEST(o2icon_geticonofclass_unregistered) {
    HGLOBAL g;
    REQUIRE(U(OleGetIconOfClass)); REQUIRE(G(GetMetaFileBitsEx));
    g = U(OleGetIconOfClass)(&O2_CLSID_A, L"My Label", FALSE);
    check_mfpict(g, "label given");
    free_mfpict(g);
    g = U(OleGetIconOfClass)(&O2_CLSID_A, NULL, FALSE);
    check_mfpict(g, "no label");
    free_mfpict(g);
    g = U(OleGetIconOfClass)(&O2_CLSID_A, NULL, TRUE);            /* label from the class's user type, falls back to a default */
    check_mfpict(g, "type as label");
    free_mfpict(g);
}
TEST(o2icon_geticonofclass_stock_classes) {
    static const GUID *stock[] = { &O2_CLSID_STDFONT, &O2_CLSID_STDPICTURE };
    int i;
    REQUIRE(U(OleGetIconOfClass)); REQUIRE(G(GetMetaFileBitsEx));
    for (i = 0; i < 2; i++) {
        HGLOBAL g = U(OleGetIconOfClass)(stock[i], NULL, TRUE);
        char what[32]; sprintf(what, "stock %d", i);
        check_mfpict(g, what);
        free_mfpict(g);
    }
}
TEST(o2icon_geticonofclass_clsid_null) {
    HGLOBAL g;
    REQUIRE(U(OleGetIconOfClass)); REQUIRE(G(GetMetaFileBitsEx));
    g = U(OleGetIconOfClass)(&O2_GUID_NULL, L"x", FALSE);
    t_note("OleGetIconOfClass(CLSID_NULL) -> %p", g);
    if (g) { check_mfpict(g, "CLSID_NULL"); free_mfpict(g); }
}
TEST(o2icon_geticonoffile_existing_file) {
    char a[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE f; HGLOBAL g;
    REQUIRE(U(OleGetIconOfFile)); REQUIRE(G(GetMetaFileBitsEx));
    t_tmpfile(a, sizeof a, "o2icon_file.txt");
    MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
    f = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(f);
    g = U(OleGetIconOfFile)(w, FALSE);
    t_note("OleGetIconOfFile(existing .txt) -> %p", g);
    if (g) { check_mfpict(g, "file icon"); free_mfpict(g); }
    g = U(OleGetIconOfFile)(w, TRUE);
    if (g) { check_mfpict(g, "file icon with file label"); free_mfpict(g); }
    DeleteFileW(w);
}

/* ================================================================================================
 * OleReg*
 * ============================================================================================== */
TEST(o2reg_getusertype_null_out) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL out pointers"); return; }
    REQUIRE(U(OleRegGetUserType));
    EQ(U(OleRegGetUserType)(&O2_CLSID_A, USERCLASSTYPE_FULL, NULL), O2_E_INVALIDARG);
}
TEST(o2reg_getusertype_unregistered) {
    static const DWORD forms[] = { USERCLASSTYPE_FULL, USERCLASSTYPE_SHORT, USERCLASSTYPE_APPNAME };
    int i;
    REQUIRE(U(OleRegGetUserType));
    for (i = 0; i < O2_N(forms); i++) {
        LPOLESTR s = (LPOLESTR)(ULONG_PTR)1; HRESULT hr = U(OleRegGetUserType)(&O2_CLSID_A, forms[i], &s);
        EQ(hr, O2_REGDB_E_CLASSNOTREG);
        EQ(s, NULL);
    }
}
TEST(o2reg_getusertype_stock_classes) {
    static const GUID *stock[] = { &O2_CLSID_STDFONT, &O2_CLSID_STDPICTURE };
    int i;
    REQUIRE(U(OleRegGetUserType)); REQUIRE(U(CoTaskMemFree));
    for (i = 0; i < 2; i++) {
        LPOLESTR s = NULL; HRESULT hr = U(OleRegGetUserType)(stock[i], USERCLASSTYPE_FULL, &s);
        if (SUCCEEDED(hr)) { CHECK(s != NULL && s[0] != 0); t_note("stock %d user type: %ls", i, s ? s : L"(null)"); U(CoTaskMemFree)(s); }
        else { t_note("stock %d OleRegGetUserType -> 0x%08lx", i, (unsigned long)hr); CHECK(s == NULL); }
    }
}
TEST(o2reg_getmiscstatus_unregistered) {
    static const DWORD asp[] = { DVASPECT_CONTENT, DVASPECT_THUMBNAIL, DVASPECT_ICON, DVASPECT_DOCPRINT };
    int i;
    REQUIRE(U(OleRegGetMiscStatus));
    for (i = 0; i < O2_N(asp); i++) {
        DWORD st = 0xCCCCCCCC; HRESULT hr = U(OleRegGetMiscStatus)(&O2_CLSID_A, asp[i], &st);
        CHECKF(hr == O2_REGDB_E_CLASSNOTREG, "aspect %lu: 0x%08lx", (unsigned long)asp[i], (unsigned long)hr);
        EQ(st, 0);                    /* output zeroed before the lookup */
    }
}
TEST(o2reg_getmiscstatus_null_out) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL out pointers"); return; }
    REQUIRE(U(OleRegGetMiscStatus));
    EQ(U(OleRegGetMiscStatus)(&O2_CLSID_A, DVASPECT_CONTENT, NULL), O2_E_INVALIDARG);
}
TEST(o2reg_getmiscstatus_stock_classes) {
    static const GUID *stock[] = { &O2_CLSID_STDFONT, &O2_CLSID_STDPICTURE };
    int i;
    REQUIRE(U(OleRegGetMiscStatus));
    for (i = 0; i < 2; i++) {
        DWORD st = 0xCCCCCCCC; HRESULT hr = U(OleRegGetMiscStatus)(stock[i], DVASPECT_CONTENT, &st);
        t_note("stock %d OleRegGetMiscStatus -> 0x%08lx status 0x%08lx", i, (unsigned long)hr, (unsigned long)st);
        CHECK(st != 0xCCCCCCCC);
    }
}
TEST(o2reg_enumformatetc_invalid_args) {
    IEnumFORMATETC *e;
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(OleRegEnumFormatEtc));
    EQ(U(OleRegEnumFormatEtc)(&O2_CLSID_A, DATADIR_GET, NULL), O2_E_INVALIDARG);
    e = (IEnumFORMATETC *)(ULONG_PTR)1;
    EQ(U(OleRegEnumFormatEtc)(&O2_CLSID_A, 4, &e), O2_E_INVALIDARG);      /* only DATADIR_GET (1) / DATADIR_SET (2) and 0, 3 pass the mask */
    EQ(e, NULL);
    e = (IEnumFORMATETC *)(ULONG_PTR)1;
    EQ(U(OleRegEnumFormatEtc)(&O2_CLSID_A, 0x100, &e), O2_E_INVALIDARG);
    EQ(e, NULL);
}
TEST(o2reg_enumformatetc_unregistered) {
    IEnumFORMATETC *e; HRESULT hr; int i;
    REQUIRE(U(OleRegEnumFormatEtc));
    for (i = DATADIR_GET; i <= DATADIR_SET; i++) {
        e = (IEnumFORMATETC *)(ULONG_PTR)1;
        hr = U(OleRegEnumFormatEtc)(&O2_CLSID_A, i, &e);
        t_note("OleRegEnumFormatEtc(unregistered, %d) -> 0x%08lx", i, (unsigned long)hr);
        if (FAILED(hr)) CHECK(e == NULL);
        else if (e) {
            FORMATETC f; ULONG got = 0;
            CHECK(IEnumFORMATETC_Next(e, 1, &f, &got) != S_OK || got == 1);
            IEnumFORMATETC_Release(e);
        }
    }
}
TEST(o2reg_enumverbs_unregistered) {
    IEnumOLEVERB *e = (IEnumOLEVERB *)(ULONG_PTR)1; HRESULT hr;
    REQUIRE(U(OleRegEnumVerbs));
    hr = U(OleRegEnumVerbs)(&O2_CLSID_A, &e);
    t_note("OleRegEnumVerbs(unregistered) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    EQ(e, NULL);
}
TEST(o2reg_enumverbs_null_out) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL out pointers"); return; }
    REQUIRE(U(OleRegEnumVerbs));
    EQ(U(OleRegEnumVerbs)(&O2_CLSID_A, NULL), O2_E_INVALIDARG);
}
