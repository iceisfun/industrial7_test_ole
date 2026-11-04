/* OLE clipboard ownership tests (state-changing: takes the real clipboard; only run in the destructive pass). */
#include "olh.h"

#define G(n) UIN("gdi32.dll", n)


/* ================================================================================================
 * OLE clipboard
 * ============================================================================================== */
static int clip_usable(void) {
    if (t_is_wine()) return 0;
    if (!OpenClipboard(NULL)) return 0;
    CloseClipboard();
    return 1;
}
static UINT cf_dataobject, cf_privdata;
static void clip_formats(void) {
    cf_dataobject = RegisterClipboardFormatA("DataObject");
    cf_privdata = RegisterClipboardFormatA("Ole Private Data");
}
static int has_format(UINT cf) { int r; if (!OpenClipboard(NULL)) return -1; r = IsClipboardFormatAvailable(cf) ? 1 : 0; CloseClipboard(); return r; }

#define CLIP_BEGIN() do { if (!clip_usable()) { t_skip("clipboard unavailable in this session"); return; } O2_BEGIN; clip_formats(); } while (0)
#define CLIP_REQUIRE() do { REQUIRE(U(OleSetClipboard)); REQUIRE(U(OleGetClipboard)); REQUIRE(U(OleFlushClipboard)); REQUIRE(U(OleIsCurrentClipboard)); } while (0)

/* adapted from Wine dlls/ole32/tests/clipboard.c (LGPL-2.1+) test_set_clipboard / test_get_clipboard */
TEST(o2clip_set_get_roundtrip) {
    O2Data d; IDataObject *got = NULL; FORMATETC f; STGMEDIUM m; HRESULT hr; char *txt;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "olet clipboard text");
    d.ignore_aspect = 1;
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    CHECK(d.ref >= 2);
    EQ(U(OleIsCurrentClipboard)(&d.i), S_OK);
    hr = U(OleGetClipboard)(&got);
    EQ(hr, S_OK);
    CHECK(got != NULL);
    if (got) {
        f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
        EQ(IDataObject_QueryGetData(got, &f), S_OK);
        memset(&m, 0, sizeof m);
        hr = IDataObject_GetData(got, &f, &m);
        EQ(hr, S_OK);
        if (hr == S_OK) {
            EQ(m.tymed, TYMED_HGLOBAL);
            txt = (char *)GlobalLock(m.hGlobal);
            CHECK(txt && !strcmp(txt, "olet clipboard text"));
            if (txt) GlobalUnlock(m.hGlobal);
            U(ReleaseStgMedium)(&m);
        }
        IDataObject_Release(got);
    }
    EQ(U(OleSetClipboard)(NULL), S_OK);
    EQ(U(OleIsCurrentClipboard)(&d.i), S_FALSE);
    EQ(d.ref, 1);
    O2_END();
}
TEST(o2clip_iscurrent_semantics) {
    O2Data a, b;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&a); od_init(&b);
    od_add(&a, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "a"); od_add(&b, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "b");
    EQ(U(OleIsCurrentClipboard)(&a.i), S_FALSE);            /* nothing set */
    EQ(U(OleSetClipboard)(&a.i), S_OK);
    EQ(U(OleIsCurrentClipboard)(&a.i), S_OK);
    EQ(U(OleIsCurrentClipboard)(&b.i), S_FALSE);
    if (!t_is_wine()) EQ(U(OleIsCurrentClipboard)(NULL), S_FALSE);
    EQ(U(OleSetClipboard)(&b.i), S_OK);                     /* replaces a */
    EQ(U(OleIsCurrentClipboard)(&a.i), S_FALSE);
    EQ(U(OleIsCurrentClipboard)(&b.i), S_OK);
    EQ(a.ref, 1);                                           /* the replaced object is released */
    EQ(U(OleSetClipboard)(NULL), S_OK);
    EQ(U(OleIsCurrentClipboard)(&b.i), S_FALSE);
    EQ(b.ref, 1);
    O2_END();
}
TEST(o2clip_get_null_pointer) {
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(OleGetClipboard));
    O2_BEGIN;
    EQ(U(OleGetClipboard)(NULL), O2_E_INVALIDARG);
    O2_END();
}
TEST(o2clip_win32_formats_after_set) {
    O2Data d;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "win32 view");
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    EQ(has_format(CF_TEXT), 1);                  /* the OLE clipboard publishes the object's formats (delay-rendered) */
    EQ(has_format(cf_dataobject), 1);             /* plus the "DataObject" marker format */
    t_note("\"Ole Private Data\" format present: %d", has_format(cf_privdata));
    EQ(U(OleSetClipboard)(NULL), S_OK);
    EQ(has_format(CF_TEXT), 0);                  /* clearing removes everything */
    EQ(has_format(cf_dataobject), 0);
    EQ(d.ref, 1);
    O2_END();
}
TEST(o2clip_flush_keeps_data_drops_object) {
    O2Data d; IDataObject *got = NULL; HGLOBAL h; char *p; FORMATETC f; HRESULT hr;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "flushed text");
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    hr = U(OleFlushClipboard)();
    EQ(hr, S_OK);
    EQ(U(OleIsCurrentClipboard)(&d.i), S_FALSE);   /* the source object no longer owns the clipboard */
    EQ(d.ref, 1);                                   /* and it has been released */
    EQ(has_format(CF_TEXT), 1);                    /* but the rendered data stays */
    if (OpenClipboard(NULL)) {
        h = GetClipboardData(CF_TEXT);
        CHECK(h != NULL);
        if (h) { p = (char *)GlobalLock(h); CHECK(p && !strcmp(p, "flushed text")); if (p) GlobalUnlock(h); }
        CloseClipboard();
    }
    hr = U(OleGetClipboard)(&got);
    EQ(hr, S_OK);
    if (got) {
        f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
        EQ(IDataObject_QueryGetData(got, &f), S_OK);
        IDataObject_Release(got);
    }
    if (has_format(cf_dataobject) == 1 && OpenClipboard(NULL)) {   /* the marker may remain, but must no longer name an owner window */
        HGLOBAL hm = GetClipboardData(cf_dataobject);
        HWND *own = hm ? (HWND *)GlobalLock(hm) : NULL;
        CHECK(own != NULL && GlobalSize(hm) >= sizeof(HWND));
        if (own) { CHECK(*own == NULL); GlobalUnlock(hm); }
        CloseClipboard();
    }
    EQ(U(OleSetClipboard)(NULL), S_OK);
    EQ(has_format(cf_dataobject), 0);
    O2_END();
}
TEST(o2clip_flush_empty_clipboard) {
    CLIP_REQUIRE();
    CLIP_BEGIN();
    EQ(U(OleSetClipboard)(NULL), S_OK);
    t_note("OleFlushClipboard with nothing set -> 0x%08lx", (unsigned long)U(OleFlushClipboard)());
    O2_END();
}
TEST(o2clip_clear_with_null_is_idempotent) {
    CLIP_REQUIRE();
    CLIP_BEGIN();
    EQ(U(OleSetClipboard)(NULL), S_OK);
    EQ(U(OleSetClipboard)(NULL), S_OK);
    O2_END();
}
/* adapted from Wine dlls/ole32/tests/clipboard.c (LGPL-2.1+) test_get_clipboard */
TEST(o2clip_wrapper_queryget_rules) {
    static const struct { CLIPFORMAT cf; DWORD tymed; DWORD aspect; LONG lindex; HRESULT hr; const char *what; } c[] = {
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, -1, S_OK, "plain" },
        { CF_TEXT, TYMED_HGLOBAL, 0xdeadbeef, -1, DV_E_FORMATETC, "garbage aspect" },
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_THUMBNAIL, -1, DV_E_FORMATETC, "thumbnail aspect" },
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, 256, DV_E_FORMATETC, "lindex 256" },
        { CF_RIFF, TYMED_HGLOBAL, DVASPECT_CONTENT, -1, DV_E_CLIPFORMAT, "format not offered" },
        { CF_TEXT, TYMED_FILE, DVASPECT_CONTENT, -1, S_OK, "any tymed bit set on the real clipboard" },
    };
    O2Data d; IDataObject *got = NULL; int i;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "q"); d.ignore_aspect = 1;
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    EQ(U(OleGetClipboard)(&got), S_OK);
    if (got) {
        for (i = 0; i < O2_N(c); i++) {
            FORMATETC f; HRESULT hr;
            f.cfFormat = c[i].cf; f.ptd = NULL; f.dwAspect = c[i].aspect; f.lindex = c[i].lindex; f.tymed = c[i].tymed;
            hr = IDataObject_QueryGetData(got, &f);
            CHECKF(hr == c[i].hr, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].hr);
        }
        EQ(d.qgd, 0);                      /* the wrapper answers from the clipboard, not from the source */
        IDataObject_Release(got);
    }
    EQ(U(OleSetClipboard)(NULL), S_OK);
    O2_END();
}
TEST(o2clip_wrapper_getdata_rules) {
    static const struct { CLIPFORMAT cf; DWORD tymed; DWORD aspect; LONG lindex; HRESULT hr; const char *what; } c[] = {
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, -1, S_OK, "plain" },
        { CF_TEXT, TYMED_HGLOBAL, 0xdeadbeef, -1, S_OK, "garbage aspect is ignored by GetData" },
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_THUMBNAIL, -1, S_OK, "thumbnail aspect is ignored by GetData" },
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, 256, DV_E_FORMATETC, "lindex 256" },
        { CF_RIFF, TYMED_HGLOBAL, DVASPECT_CONTENT, -1, DV_E_FORMATETC, "format not offered" },
        { CF_TEXT, TYMED_FILE, DVASPECT_CONTENT, -1, DV_E_TYMED, "medium not available" },
    };
    O2Data d; IDataObject *got = NULL; int i;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "g"); d.ignore_aspect = 1;
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    EQ(U(OleGetClipboard)(&got), S_OK);
    if (got) {
        for (i = 0; i < O2_N(c); i++) {
            FORMATETC f; STGMEDIUM m; HRESULT hr;
            f.cfFormat = c[i].cf; f.ptd = NULL; f.dwAspect = c[i].aspect; f.lindex = c[i].lindex; f.tymed = c[i].tymed;
            memset(&m, 0, sizeof m);
            hr = IDataObject_GetData(got, &f, &m);
            CHECKF(hr == c[i].hr, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].hr);
            if (SUCCEEDED(hr)) U(ReleaseStgMedium)(&m);
        }
        IDataObject_Release(got);
    }
    EQ(U(OleSetClipboard)(NULL), S_OK);
    O2_END();
}
/* adapted from Wine dlls/ole32/tests/clipboard.c (LGPL-2.1+) test_enum_fmtetc */
TEST(o2clip_wrapper_enumformatetc) {
    O2Data d; IDataObject *got = NULL; IEnumFORMATETC *e = NULL, *clone = NULL; FORMATETC f, g; HRESULT hr; int n = 0;
    CLIPFORMAT mine = (CLIPFORMAT)RegisterClipboardFormatA("olet_o2clip_private");
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d);
    od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "t");
    od_add(&d, mine, TYMED_HGLOBAL, DVASPECT_CONTENT, "p");
    od_add(&d, CF_UNICODETEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "u");
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    EQ(U(OleGetClipboard)(&got), S_OK);
    if (got) {
        LONG before = d.enumcalls;
        EQ(IDataObject_EnumFormatEtc(got, DATADIR_SET, &e), E_NOTIMPL);
        hr = IDataObject_EnumFormatEtc(got, DATADIR_GET, &e);
        EQ(hr, S_OK);
        EQ(d.enumcalls, before);                 /* answered from the clipboard's private data, not the source */
        if (e) {
            while (IEnumFORMATETC_Next(e, 1, &f, NULL) == S_OK) {
                CHECKF(n < 3 && f.cfFormat == d.f[n].cfFormat && f.dwAspect == d.f[n].dwAspect && f.lindex == d.f[n].lindex && f.tymed == d.f[n].tymed, "entry %d", n);
                n++;
            }
            EQ(n, 3);
            EQ(IEnumFORMATETC_Reset(e), S_OK);
            EQ(IEnumFORMATETC_Skip(e, 2), S_OK);
            EQ(IEnumFORMATETC_Clone(e, &clone), S_OK);
            EQ(IEnumFORMATETC_Next(e, 1, &f, NULL), S_OK);
            EQ(f.cfFormat, CF_UNICODETEXT);
            if (clone) { EQ(IEnumFORMATETC_Next(clone, 1, &g, NULL), S_OK); EQ(g.cfFormat, CF_UNICODETEXT); IEnumFORMATETC_Release(clone); }
            EQ(IEnumFORMATETC_Next(e, 1, &f, NULL), S_FALSE);
            IEnumFORMATETC_Release(e);
        }
        IDataObject_Release(got);
    }
    EQ(U(OleSetClipboard)(NULL), S_OK);
    O2_END();
}
TEST(o2clip_wrapper_after_clear_is_empty) {
    O2Data d; IDataObject *got = NULL; IEnumFORMATETC *e = NULL; FORMATETC f; HRESULT hr;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "t");
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    EQ(U(OleSetClipboard)(NULL), S_OK);
    hr = U(OleGetClipboard)(&got);
    EQ(hr, S_OK);
    if (got) {
        f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
        hr = IDataObject_QueryGetData(got, &f);
        t_note("QueryGetData(CF_TEXT) on an empty clipboard -> 0x%08lx", (unsigned long)hr);
        CHECK(hr != S_OK);
        hr = IDataObject_EnumFormatEtc(got, DATADIR_GET, &e);
        EQ(hr, S_OK);
        if (e) { EQ(IEnumFORMATETC_Next(e, 1, &f, NULL), S_FALSE); IEnumFORMATETC_Release(e); }
        IDataObject_Release(got);
    }
    O2_END();
}
TEST(o2clip_uninitialize_releases_clipboard_object) {
    O2Data d;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "t");
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    CHECK(d.ref >= 2);
    O2_END();                                 /* last OleUninitialize: flushes and drops the clipboard object */
    t_note("source refcount after OleUninitialize: %ld", (long)d.ref);
    {
        HRESULT ih = U(OleInitialize)(NULL);
        EQ(ih, S_OK);
        EQ(U(OleIsCurrentClipboard)(&d.i), S_FALSE);
        U(OleSetClipboard)(NULL);
        U(OleUninitialize)();
    }
}
TEST(o2clip_set_object_without_formats) {
    O2Data d; HRESULT hr;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d);
    hr = U(OleSetClipboard)(&d.i);
    t_note("OleSetClipboard(object offering no formats) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) { EQ(U(OleIsCurrentClipboard)(&d.i), S_OK); EQ(U(OleSetClipboard)(NULL), S_OK); }
    EQ(d.ref, 1);
    O2_END();
}
TEST(o2clip_set_object_enum_failure) {
    O2Data d; HRESULT hr;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "t");
    d.enum_hr_override = 1; d.enum_hr = E_FAIL;
    hr = U(OleSetClipboard)(&d.i);
    t_note("OleSetClipboard(EnumFormatEtc fails with E_FAIL) -> 0x%08lx", (unsigned long)hr);
    CHECK(hr == S_OK || FAILED(hr));
    if (SUCCEEDED(hr)) U(OleSetClipboard)(NULL);
    EQ(d.ref, 1);
    O2_END();
}
TEST(o2clip_text_available_to_win32_consumers) {
    O2Data d; HGLOBAL h; char *p;
    CLIP_REQUIRE();
    CLIP_BEGIN();
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "win32 consumer"); d.ignore_aspect = 1;
    EQ(U(OleSetClipboard)(&d.i), S_OK);
    if (OpenClipboard(NULL)) {
        h = GetClipboardData(CF_TEXT);               /* triggers delayed rendering through the source's GetData */
        CHECK(h != NULL);
        if (h) { p = (char *)GlobalLock(h); CHECK(p && !strcmp(p, "win32 consumer")); if (p) GlobalUnlock(h); }
        CloseClipboard();
    }
    CHECK(d.getdata >= 1);
    EQ(U(OleSetClipboard)(NULL), S_OK);
    O2_END();
}
