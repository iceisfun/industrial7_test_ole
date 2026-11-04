/* STGMEDIUM helpers: ReleaseStgMedium, OleDuplicateData, GetHGlobalFromStream.
 * ReleaseStgMedium semantics from ole32's decompilation: a medium with pUnkForRelease != NULL never frees the
 * payload (the releaser owns it) but Releases the IUnknown and clears the field; a NULL pUnkForRelease frees per tymed;
 * TYMED_ISTREAM/ISTORAGE always Release the interface; unknown/combined tymed values free nothing; tymed is
 * reset to TYMED_NULL in every case. OleDuplicateData: BITMAP/PALETTE/ENHMETAFILE/METAFILEPICT get special copies,
 * everything else is a byte copy of the HGLOBAL (default GMEM_MOVEABLE when uiFlags == 0). */
#include "slm.h"

#define GDI(name) UIN("gdi32.dll", name)

typedef struct { IUnknownVtbl *lp; LONG ref; int released; } CU;
static HRESULT WINAPI cu_QI(IUnknown *p, REFIID r, void **pp) { if (IsEqualGUID(r, &G_IUnknown)) { *pp = p; ((CU *)p)->ref++; return S_OK; } *pp = NULL; return E_NOINTERFACE; }
static ULONG WINAPI cu_AddRef(IUnknown *p) { return ++((CU *)p)->ref; }
static ULONG WINAPI cu_Release(IUnknown *p) { CU *c = (CU *)p; c->released++; return --c->ref; }
static IUnknownVtbl cu_vtbl = { cu_QI, cu_AddRef, cu_Release };
static void cu_init(CU *c) { c->lp = &cu_vtbl; c->ref = 1; c->released = 0; }

static HGLOBAL gtext(const void *d, SIZE_T n) { HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n); void *p = GlobalLock(g); memcpy(p, d, n); GlobalUnlock(g); return g; }
static int g_valid(HGLOBAL g) { return GlobalSize(g) != 0; }

/* ---- ReleaseStgMedium ------------------------------------------------------- */
TEST(slm_rsm_null_pointer) {
    REQUIRE(U(ReleaseStgMedium));
    U(ReleaseStgMedium)(NULL);       /* tolerated */
    CHECK(1);
}
TEST(slm_rsm_tymed_null_releases_punk) {
    REQUIRE(U(ReleaseStgMedium));
    CU cu; cu_init(&cu); cu.ref = 3;
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_NULL; m.pUnkForRelease = (IUnknown *)&cu;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(cu.released, 1); EXPECT_EQ(cu.ref, 2);
    CHECK(m.pUnkForRelease == NULL);
    EXPECT_EQ(m.tymed, TYMED_NULL);
    U(ReleaseStgMedium)(&m);         /* second call is a no-op */
    EXPECT_EQ(cu.released, 1);
}
TEST(slm_rsm_tymed_null_no_punk) {
    REQUIRE(U(ReleaseStgMedium));
    STGMEDIUM m; memset(&m, 0, sizeof m);
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(m.tymed, TYMED_NULL);
    CHECK(m.pUnkForRelease == NULL);
}
TEST(slm_rsm_hglobal_freed) {
    REQUIRE(U(ReleaseStgMedium));
    HGLOBAL g = gtext("abcdef", 6);
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = g;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(m.tymed, TYMED_NULL);
    CHECK(!g_valid(g));              /* the handle was freed by GlobalFree */
}
TEST(slm_rsm_hglobal_null_handle) {
    REQUIRE(U(ReleaseStgMedium));
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = NULL;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(m.tymed, TYMED_NULL);
}
TEST(slm_rsm_hglobal_with_punk_keeps_memory) {
    REQUIRE(U(ReleaseStgMedium));
    HGLOBAL g = gtext("abcdef", 6);
    CU cu; cu_init(&cu); cu.ref = 2;
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_HGLOBAL; m.hGlobal = g; m.pUnkForRelease = (IUnknown *)&cu;
    U(ReleaseStgMedium)(&m);
    CHECK(g_valid(g));               /* owner keeps the memory */
    EXPECT_EQ(cu.released, 1); EXPECT_EQ(cu.ref, 1);
    CHECK(m.pUnkForRelease == NULL);
    EXPECT_EQ(m.tymed, TYMED_NULL);
    GlobalFree(g);
}
TEST(slm_rsm_file_deleted_and_name_freed) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CoTaskMemAlloc));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "rsm_file.tmp");
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    wchar_t wp[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    wchar_t *name = U(CoTaskMemAlloc)((wcslen(wp) + 1) * sizeof(wchar_t)); wcscpy(name, wp);
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_FILE; m.lpszFileName = name;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(GetFileAttributesA(path), INVALID_FILE_ATTRIBUTES);    /* a TYMED_FILE medium owns its file */
    EXPECT_EQ(m.tymed, TYMED_NULL);
    CHECK(m.lpszFileName == NULL);
}
TEST(slm_rsm_file_with_punk_keeps_file) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CoTaskMemAlloc));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "rsm_file2.tmp");
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    wchar_t wp[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    wchar_t *name = U(CoTaskMemAlloc)((wcslen(wp) + 1) * sizeof(wchar_t)); wcscpy(name, wp);
    CU cu; cu_init(&cu); cu.ref = 2;
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_FILE; m.lpszFileName = name; m.pUnkForRelease = (IUnknown *)&cu;
    U(ReleaseStgMedium)(&m);
    CHECK(GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
    EXPECT_EQ(cu.released, 1);
    EXPECT_EQ(m.tymed, TYMED_NULL);
    DeleteFileA(path);
}
TEST(slm_rsm_file_null_name) {
    REQUIRE(U(ReleaseStgMedium));
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_FILE; m.lpszFileName = NULL;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(m.tymed, TYMED_NULL);
}
TEST(slm_rsm_istream_released) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CreateStreamOnHGlobal));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStream *s = NULL; EXPECT_EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    if (s) {
        IStream_AddRef(s);                                   /* ours + the medium's */
        STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_ISTREAM; m.pstm = s;
        U(ReleaseStgMedium)(&m);
        EXPECT_EQ(m.tymed, TYMED_NULL);
        EXPECT_EQ(IStream_Release(s), 0);                    /* exactly one reference was dropped by ReleaseStgMedium */
    }
    slm_couninit();
}
TEST(slm_rsm_istream_with_punk) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(U(CreateStreamOnHGlobal));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStream *s = NULL; U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    CU cu; cu_init(&cu); cu.ref = 2;
    if (s) {
        IStream_AddRef(s);
        STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_ISTREAM; m.pstm = s; m.pUnkForRelease = (IUnknown *)&cu;
        U(ReleaseStgMedium)(&m);
        EXPECT_EQ(IStream_Release(s), 0);                    /* the stream is released regardless of pUnkForRelease */
        EXPECT_EQ(cu.released, 1);
    }
    slm_couninit();
}
TEST(slm_rsm_istorage_released) {
    REQUIRE(U(ReleaseStgMedium));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = NULL;
    if (FAILED(slm_mem_docfile(&stg, &lb, SM_CREATE_RW))) { CHECK(0); slm_couninit(); return; }
    IStorage_AddRef(stg);
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_ISTORAGE; m.pstg = stg;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(m.tymed, TYMED_NULL);
    EXPECT_EQ(IStorage_Release(stg), 0);
    ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_rsm_interface_null_pointer) {
    REQUIRE(U(ReleaseStgMedium));
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_ISTREAM; m.pstm = NULL;
    U(ReleaseStgMedium)(&m); EXPECT_EQ(m.tymed, TYMED_NULL);
    memset(&m, 0, sizeof m); m.tymed = TYMED_ISTORAGE; m.pstg = NULL;
    U(ReleaseStgMedium)(&m); EXPECT_EQ(m.tymed, TYMED_NULL);
}
TEST(slm_rsm_gdi_bitmap) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(GDI(CreateBitmap)); REQUIRE(GDI(GetObjectType));
    HBITMAP hb = GDI(CreateBitmap)(4, 4, 1, 32, NULL);
    CHECK(hb != NULL); if (!hb) return;
    CU cu; cu_init(&cu); cu.ref = 2;
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_GDI; m.hBitmap = hb; m.pUnkForRelease = (IUnknown *)&cu;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(GDI(GetObjectType)(hb), OBJ_BITMAP);           /* not deleted while someone else owns it */
    EXPECT_EQ(cu.released, 1);
    memset(&m, 0, sizeof m); m.tymed = TYMED_GDI; m.hBitmap = hb;
    U(ReleaseStgMedium)(&m);
    /* Windows keeps reporting OBJ_BITMAP from GetObjectType for a deleted bitmap handle (also after a plain DeleteObject),
       so DeleteObject having run is observed through GetObject failing */
    { BITMAP bm; EXPECT_EQ(GDI(GetObjectW)(hb, sizeof bm, &bm), 0); }
    EXPECT_EQ(m.tymed, TYMED_NULL);
}
TEST(slm_rsm_enhmetafile) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(GDI(CreateEnhMetaFileW)); REQUIRE(GDI(CloseEnhMetaFile)); REQUIRE(GDI(GetEnhMetaFileBits));
    HDC dc = GDI(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(dc != NULL); if (!dc) return;
    GDI(Rectangle)(dc, 1, 1, 20, 20);
    HENHMETAFILE em = GDI(CloseEnhMetaFile)(dc);
    CHECK(em != NULL); if (!em) return;
    UINT sz = GDI(GetEnhMetaFileBits)(em, 0, NULL); CHECK(sz > 0);
    CU cu; cu_init(&cu); cu.ref = 2;
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_ENHMF; m.hEnhMetaFile = em; m.pUnkForRelease = (IUnknown *)&cu;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(GDI(GetEnhMetaFileBits)(em, 0, NULL), sz);     /* still alive */
    memset(&m, 0, sizeof m); m.tymed = TYMED_ENHMF; m.hEnhMetaFile = em;
    U(ReleaseStgMedium)(&m);
    EXPECT_EQ(GDI(GetEnhMetaFileBits)(em, 0, NULL), 0);      /* DeleteEnhMetaFile */
}
TEST(slm_rsm_mfpict) {
    REQUIRE(U(ReleaseStgMedium)); REQUIRE(GDI(CreateMetaFileW)); REQUIRE(GDI(CloseMetaFile)); REQUIRE(GDI(GetMetaFileBitsEx));
    for (int with_punk = 0; with_punk <= 1; with_punk++) {
        HDC dc = GDI(CreateMetaFileW)(NULL); CHECK(dc != NULL); if (!dc) return;
        GDI(Rectangle)(dc, 1, 1, 9, 9);
        HMETAFILE mf = GDI(CloseMetaFile)(dc); CHECK(mf != NULL); if (!mf) return;
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
        METAFILEPICT *mp = GlobalLock(g); mp->mm = MM_ANISOTROPIC; mp->xExt = 10; mp->yExt = 10; mp->hMF = mf; GlobalUnlock(g);
        UINT sz = GDI(GetMetaFileBitsEx)(mf, 0, NULL); CHECK(sz > 0);
        CU cu; cu_init(&cu); cu.ref = 2;
        STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = g;
        if (with_punk) m.pUnkForRelease = (IUnknown *)&cu;
        U(ReleaseStgMedium)(&m);
        EXPECT_EQ(m.tymed, TYMED_NULL);
        if (with_punk) {
            CHECK(g_valid(g));
            EXPECT_EQ(GDI(GetMetaFileBitsEx)(mf, 0, NULL), sz);
            EXPECT_EQ(cu.released, 1);
            GDI(DeleteMetaFile)(mf); GlobalFree(g);
        } else {
            CHECK(!g_valid(g));                              /* container freed ... */
            EXPECT_EQ(GDI(GetMetaFileBitsEx)(mf, 0, NULL), 0);   /* ... and the metafile inside deleted */
        }
    }
}
TEST(slm_rsm_unknown_tymed_frees_nothing) {
    REQUIRE(U(ReleaseStgMedium));
    /* values outside the single-bit cases (combinations, undefined bits) hit no case: payload untouched, tymed reset */
    DWORD odd[] = { TYMED_HGLOBAL | TYMED_FILE, TYMED_HGLOBAL | TYMED_ISTREAM, 0x80, 0x100, 0xFFFFFFFF };
    for (int i = 0; i < 5; i++) {
        HGLOBAL g = gtext("zz", 2);
        CU cu; cu_init(&cu); cu.ref = 2;
        STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = odd[i]; m.hGlobal = g; m.pUnkForRelease = (IUnknown *)&cu;
        U(ReleaseStgMedium)(&m);
        CHECKF(g_valid(g), "tymed 0x%lx freed the handle", odd[i]);
        CHECKF(m.tymed == TYMED_NULL, "tymed 0x%lx not cleared", odd[i]);
        CHECKF(cu.released == 1 && m.pUnkForRelease == NULL, "tymed 0x%lx punk handling", odd[i]);
        GlobalFree(g);
    }
}

/* ---- OleDuplicateData ---------------------------------------------------------- */
TEST(slm_odd_null_source) {
    REQUIRE(U(OleDuplicateData));
    WORD fmts[] = { CF_TEXT, CF_BITMAP, CF_METAFILEPICT, CF_DIB, CF_PALETTE, CF_UNICODETEXT, CF_ENHMETAFILE, 0x200, 0xC001 };
    UINT flags[] = { 0, GMEM_MOVEABLE, GMEM_MOVEABLE | GMEM_DDESHARE };
    for (int i = 0; i < 9; i++) for (int j = 0; j < 3; j++) {
        HANDLE r = U(OleDuplicateData)(NULL, fmts[i], flags[j]);
        CHECKF(r == NULL, "format %u flags %u", fmts[i], flags[j]);
    }
}
TEST(slm_odd_global_copy_formats) {
    REQUIRE(U(OleDuplicateData));
    WORD fmts[] = { CF_TEXT, CF_OEMTEXT, CF_UNICODETEXT, CF_DIB, CF_RIFF, CF_WAVE, CF_HDROP, CF_DIF, CF_SYLK, 0x0200, 0x0300, 0xC001, 0xFFFF };
    BYTE src[137]; for (int i = 0; i < 137; i++) src[i] = (BYTE)(i * 13 + 5);
    for (int i = 0; i < (int)(sizeof fmts / sizeof fmts[0]); i++) {
        HGLOBAL g = gtext(src, sizeof src);
        HGLOBAL d = U(OleDuplicateData)(g, fmts[i], 0);
        CHECKF(d != NULL, "format %u", fmts[i]);
        if (!d) { GlobalFree(g); continue; }
        CHECKF(d != g, "format %u: same handle", fmts[i]);
        CHECKF(GlobalSize(d) >= sizeof src && GlobalSize(d) == GlobalSize(g), "format %u: size %llu vs %llu", fmts[i], (unsigned long long)GlobalSize(d), (unsigned long long)GlobalSize(g));
        BYTE *p = GlobalLock(d);
        CHECKF(p && !memcmp(p, src, sizeof src), "format %u: content", fmts[i]);
        if (p) { p[0] ^= 0xFF; GlobalUnlock(d); }
        p = GlobalLock(g);
        CHECKF(p && p[0] == src[0], "format %u: copy not independent", fmts[i]);
        if (p) GlobalUnlock(g);
        GlobalFree(d); GlobalFree(g);
    }
}
TEST(slm_odd_flags_moveable_default) {
    REQUIRE(U(OleDuplicateData));
    HGLOBAL g = gtext("0123456789", 10);
    UINT flags[] = { 0, GMEM_MOVEABLE, GMEM_MOVEABLE | GMEM_DDESHARE, GMEM_MOVEABLE | GMEM_ZEROINIT };
    for (int i = 0; i < 4; i++) {
        HGLOBAL d = U(OleDuplicateData)(g, CF_TEXT, flags[i]);
        CHECKF(d != NULL, "flags 0x%x", flags[i]);
        if (!d) continue;
        UINT f = GlobalFlags(d);
        CHECKF(f != GMEM_INVALID_HANDLE, "flags 0x%x: result is not a moveable handle", flags[i]);
        char *p = GlobalLock(d); CHECK(p && !memcmp(p, "0123456789", 10)); if (p) GlobalUnlock(d);
        GlobalFree(d);
    }
    GlobalFree(g);
}
TEST(slm_odd_bitmap) {
    REQUIRE(U(OleDuplicateData)); REQUIRE(GDI(CreateBitmap)); REQUIRE(GDI(GetObjectW)); REQUIRE(GDI(GetBitmapBits));
    DWORD bits[5 * 4]; for (int i = 0; i < 20; i++) bits[i] = 0x00FF0000u + i * 0x010101u;
    HBITMAP hb = GDI(CreateBitmap)(5, 4, 1, 32, bits);
    CHECK(hb != NULL); if (!hb) return;
    HBITMAP d = U(OleDuplicateData)(hb, CF_BITMAP, 0);
    CHECK(d != NULL && d != hb);
    if (d) {
        EXPECT_EQ(GDI(GetObjectType)(d), OBJ_BITMAP);
        BITMAP bm; memset(&bm, 0, sizeof bm);
        EXPECT_EQ(GDI(GetObjectW)(d, sizeof bm, &bm), sizeof bm);
        EXPECT_EQ(bm.bmWidth, 5); EXPECT_EQ(bm.bmHeight, 4); EXPECT_EQ(bm.bmBitsPixel, 32);
        DWORD got[20]; memset(got, 0, sizeof got);
        GDI(GetBitmapBits)(d, sizeof got, got);
        CHECK(!memcmp(got, bits, sizeof got));
        GDI(DeleteObject)(d);
    }
    EXPECT_EQ(GDI(GetObjectType)(hb), OBJ_BITMAP);       /* source untouched */
    GDI(DeleteObject)(hb);
}
TEST(slm_odd_bitmap_monochrome) {
    REQUIRE(U(OleDuplicateData)); REQUIRE(GDI(CreateBitmap)); REQUIRE(GDI(GetObjectW));
    BYTE bits[2 * 8] = { 0xAA, 0, 0x55, 0, 0xAA, 0, 0x55, 0, 0xAA, 0, 0x55, 0, 0xAA, 0, 0x55, 0 };
    HBITMAP hb = GDI(CreateBitmap)(8, 8, 1, 1, bits);
    CHECK(hb != NULL); if (!hb) return;
    HBITMAP d = U(OleDuplicateData)(hb, CF_BITMAP, 0);
    CHECK(d != NULL && d != hb);
    if (d) {
        BITMAP bm; memset(&bm, 0, sizeof bm); GDI(GetObjectW)(d, sizeof bm, &bm);
        EXPECT_EQ(bm.bmWidth, 8); EXPECT_EQ(bm.bmHeight, 8); EXPECT_EQ(bm.bmPlanes, 1); EXPECT_EQ(bm.bmBitsPixel, 1);
        BYTE got[16] = { 0 }; GDI(GetBitmapBits)(d, 16, got);
        CHECK(got[0] == 0xAA && got[2] == 0x55);
        GDI(DeleteObject)(d);
    }
    GDI(DeleteObject)(hb);
}
TEST(slm_odd_palette) {
    REQUIRE(U(OleDuplicateData)); REQUIRE(GDI(CreatePalette)); REQUIRE(GDI(GetPaletteEntries));
    struct { WORD ver, n; PALETTEENTRY e[4]; } lp = { 0x300, 4, { {1, 2, 3, 0}, {10, 20, 30, 0}, {100, 110, 120, 0}, {255, 254, 253, 0} } };
    HPALETTE hp = GDI(CreatePalette)((LOGPALETTE *)&lp);
    CHECK(hp != NULL); if (!hp) return;
    HPALETTE d = U(OleDuplicateData)(hp, CF_PALETTE, 0);
    CHECK(d != NULL && d != hp);
    if (d) {
        EXPECT_EQ(GDI(GetObjectType)(d), OBJ_PAL);
        EXPECT_EQ(GDI(GetPaletteEntries)(d, 0, 0, NULL), 4);
        PALETTEENTRY got[4]; memset(got, 0, sizeof got);
        EXPECT_EQ(GDI(GetPaletteEntries)(d, 0, 4, got), 4);
        int bad = 0; for (int i = 0; i < 4; i++) bad += got[i].peRed != lp.e[i].peRed || got[i].peGreen != lp.e[i].peGreen || got[i].peBlue != lp.e[i].peBlue;
        EXPECT_EQ(bad, 0);
        GDI(DeleteObject)(d);
    }
    GDI(DeleteObject)(hp);
}
TEST(slm_odd_enhmetafile) {
    REQUIRE(U(OleDuplicateData)); REQUIRE(GDI(CreateEnhMetaFileW));
    HDC dc = GDI(CreateEnhMetaFileW)(NULL, NULL, NULL, NULL);
    CHECK(dc != NULL); if (!dc) return;
    GDI(Rectangle)(dc, 2, 2, 30, 30); GDI(Ellipse)(dc, 5, 5, 25, 25);
    HENHMETAFILE em = GDI(CloseEnhMetaFile)(dc); CHECK(em != NULL); if (!em) return;
    HENHMETAFILE d = U(OleDuplicateData)(em, CF_ENHMETAFILE, 0);
    CHECK(d != NULL && d != em);
    if (d) {
        UINT s1 = GDI(GetEnhMetaFileBits)(em, 0, NULL), s2 = GDI(GetEnhMetaFileBits)(d, 0, NULL);
        EXPECT_EQ(s1, s2); CHECK(s1 > 0);
        BYTE *b1 = malloc(s1), *b2 = malloc(s2);
        GDI(GetEnhMetaFileBits)(em, s1, b1); GDI(GetEnhMetaFileBits)(d, s2, b2);
        CHECK(!memcmp(b1, b2, s1 < s2 ? s1 : s2));
        free(b1); free(b2);
        GDI(DeleteEnhMetaFile)(d);
    }
    GDI(DeleteEnhMetaFile)(em);
}
TEST(slm_odd_metafilepict) {
    REQUIRE(U(OleDuplicateData)); REQUIRE(GDI(CreateMetaFileW)); REQUIRE(GDI(GetMetaFileBitsEx));
    HDC dc = GDI(CreateMetaFileW)(NULL); CHECK(dc != NULL); if (!dc) return;
    GDI(Rectangle)(dc, 1, 1, 9, 9);
    HMETAFILE mf = GDI(CloseMetaFile)(dc); CHECK(mf != NULL); if (!mf) return;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
    METAFILEPICT *mp = GlobalLock(g); mp->mm = MM_ISOTROPIC; mp->xExt = 123; mp->yExt = 456; mp->hMF = mf; GlobalUnlock(g);
    HGLOBAL d = U(OleDuplicateData)(g, CF_METAFILEPICT, 0);
    CHECK(d != NULL && d != g);
    if (d) {
        METAFILEPICT *dm = GlobalLock(d);
        CHECK(dm != NULL);
        if (dm) {
            EXPECT_EQ(dm->mm, MM_ISOTROPIC); EXPECT_EQ(dm->xExt, 123); EXPECT_EQ(dm->yExt, 456);
            CHECK(dm->hMF != NULL && dm->hMF != mf);          /* the metafile itself is copied (CopyMetaFile) */
            UINT s1 = GDI(GetMetaFileBitsEx)(mf, 0, NULL), s2 = GDI(GetMetaFileBitsEx)(dm->hMF, 0, NULL);
            CHECK(s1 > 0); EXPECT_EQ(s1, s2);
            GDI(DeleteMetaFile)(dm->hMF);
            GlobalUnlock(d);
        }
        GlobalFree(d);
    }
    GDI(DeleteMetaFile)(mf); GlobalFree(g);
}
TEST(slm_odd_metafilepict_too_small) {
    REQUIRE(U(OleDuplicateData));
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 4);
    HGLOBAL d = U(OleDuplicateData)(g, CF_METAFILEPICT, 0);
    CHECK(d == NULL);                                       /* not large enough to be a METAFILEPICT */
    if (d) GlobalFree(d);
    GlobalFree(g);
}
TEST(slm_odd_same_data_different_formats) {
    REQUIRE(U(OleDuplicateData));
    /* a 24 byte block that looks like a METAFILEPICT is only interpreted as one for CF_METAFILEPICT */
    BYTE blob[24]; memset(blob, 0x5A, sizeof blob);
    HGLOBAL g = gtext(blob, sizeof blob);
    HGLOBAL d = U(OleDuplicateData)(g, CF_TEXT, 0);
    CHECK(d != NULL);
    if (d) { BYTE *p = GlobalLock(d); CHECK(p && !memcmp(p, blob, 24)); if (p) GlobalUnlock(d); GlobalFree(d); }
    GlobalFree(g);
}

/* ---- GetHGlobalFromStream ----------------------------------------------------------- */
TEST(slm_ghfs_default_handle) {
    REQUIRE(U(GetHGlobalFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStream *s = NULL; EXPECT_EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    if (s) {
        HGLOBAL h = NULL;
        EXPECT_EQ(U(GetHGlobalFromStream)(s, &h), S_OK);
        CHECK(h != NULL);
        ULONG w; IStream_Write(s, "stream-data", 11, &w);
        HGLOBAL h2 = NULL; EXPECT_EQ(U(GetHGlobalFromStream)(s, &h2), S_OK);
        CHECK(h2 != NULL && GlobalSize(h2) >= 11);
        char *p = h2 ? GlobalLock(h2) : NULL;
        CHECK(p && !memcmp(p, "stream-data", 11));
        if (p) GlobalUnlock(h2);
        IStream_Release(s);
    }
    slm_couninit();
}
TEST(slm_ghfs_supplied_handle) {
    REQUIRE(U(GetHGlobalFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    HGLOBAL g = gtext("0123456789", 10);
    IStream *s = NULL; EXPECT_EQ(U(CreateStreamOnHGlobal)(g, FALSE, &s), S_OK);
    if (s) {
        HGLOBAL h = NULL; EXPECT_EQ(U(GetHGlobalFromStream)(s, &h), S_OK);
        EXPECT_EQ(h, g);
        STATSTG st; memset(&st, 0, sizeof st); IStream_Stat(s, &st, STATFLAG_NONAME);
        EXPECT_EQ(st.cbSize.QuadPart, GlobalSize(g));
        IStream_Release(s);
        CHECK(g_valid(g));                         /* fDeleteOnRelease == FALSE */
    }
    GlobalFree(g);
    slm_couninit();
}
TEST(slm_ghfs_invalid_args) {
    REQUIRE(U(GetHGlobalFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    HGLOBAL h = NULL;
    EXPECT_EQ(U(GetHGlobalFromStream)(NULL, &h), H_INVALIDARG);
    IStream *s = NULL; U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    if (s) { EXPECT_EQ(U(GetHGlobalFromStream)(s, NULL), H_INVALIDARG); IStream_Release(s); }
    slm_couninit();
}
TEST(slm_ghfs_foreign_stream) {
    REQUIRE(U(GetHGlobalFromStream));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = NULL; IStream *s = NULL;
    if (FAILED(slm_mem_docfile(&stg, &lb, SM_CREATE_RW))) { CHECK(0); slm_couninit(); return; }
    IStorage_CreateStream(stg, L"x", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s);
    HGLOBAL h = (HGLOBAL)(ULONG_PTR)0x77;
    if (s) {
        EXPECT_EQ(U(GetHGlobalFromStream)(s, &h), H_INVALIDARG);    /* only CreateStreamOnHGlobal streams qualify */
        IStream_Release(s);
    }
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_ghfs_clone_shares_handle) {
    REQUIRE(U(GetHGlobalFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStream *s = NULL, *c = NULL; U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    if (s) {
        ULONG w; IStream_Write(s, "abc", 3, &w);
        HRESULT hr = IStream_Clone(s, &c);
        EXPECT_EQ(hr, S_OK);
        if (c) {
            HGLOBAL h1 = NULL, h2 = NULL;
            EXPECT_EQ(U(GetHGlobalFromStream)(s, &h1), S_OK);
            EXPECT_EQ(U(GetHGlobalFromStream)(c, &h2), S_OK);
            EXPECT_EQ(h1, h2);
            IStream_Release(c);
        }
        IStream_Release(s);
    }
    slm_couninit();
}
TEST(slm_ghfs_after_growth) {
    REQUIRE(U(GetHGlobalFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStream *s = NULL; U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    if (s) {
        BYTE blk[1000]; memset(blk, 0xA7, sizeof blk);
        for (int i = 0; i < 40; i++) { ULONG w; IStream_Write(s, blk, sizeof blk, &w); }
        HGLOBAL h = NULL; EXPECT_EQ(U(GetHGlobalFromStream)(s, &h), S_OK);
        CHECK(h && GlobalSize(h) >= 40000);
        BYTE *p = h ? GlobalLock(h) : NULL;
        if (p) { int bad = 0; for (int i = 0; i < 40000; i++) bad += p[i] != 0xA7; EXPECT_EQ(bad, 0); GlobalUnlock(h); }
        IStream_Release(s);
    }
    slm_couninit();
}
