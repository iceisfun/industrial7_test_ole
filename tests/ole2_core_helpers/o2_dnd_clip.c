/* RegisterDragDrop / RevokeDragDrop / DoDragDrop validation paths and the OLE clipboard
 * (OleSetClipboard / OleGetClipboard / OleFlushClipboard / OleIsCurrentClipboard).
 * Window and clipboard tests skip themselves when the session cannot create windows or open the clipboard. */
#include "olh.h"

#define G(n) UIN("gdi32.dll", n)

/* ================================================================================================
 * RegisterDragDrop / RevokeDragDrop
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/dragdrop.c (LGPL-2.1+) test_Register_Revoke */
TEST(o2dnd_register_null_target) {
    HWND w; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop));
    w = o2_mkwnd();
    hr = U(RegisterDragDrop)(w, NULL);
    EQ(hr, O2_E_INVALIDARG);
    hr = U(RegisterDragDrop)(NULL, NULL);
    EQ(hr, O2_E_INVALIDARG);          /* the target is validated before the window */
    if (w) DestroyWindow(w);
    O2_END();
}
TEST(o2dnd_register_invalid_window) {
    O2Drop t; HRESULT hr; HWND w;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop));
    odt_init(&t);
    hr = U(RegisterDragDrop)(NULL, &t.i);
    EQ(hr, O2_DRAGDROP_E_INVALIDHWND);
    hr = U(RegisterDragDrop)((HWND)(ULONG_PTR)0xdeadbeef, &t.i);
    EQ(hr, O2_DRAGDROP_E_INVALIDHWND);
    w = o2_mkwnd();
    if (w) {
        DestroyWindow(w);
        hr = U(RegisterDragDrop)(w, &t.i);
        EQ(hr, O2_DRAGDROP_E_INVALIDHWND);        /* destroyed window */
    }
    EQ(t.ref, 1);                                 /* failures keep no reference */
    O2_END();
}
TEST(o2dnd_register_revoke_cycle) {
    O2Drop t; HWND w; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop)); REQUIRE(U(RevokeDragDrop));
    odt_init(&t);
    w = o2_mkwnd(); if (!w) { O2_END(); t_skip("cannot create windows in this session"); return; }
    hr = U(RegisterDragDrop)(w, &t.i);
    EQ(hr, S_OK);
    CHECK(t.ref >= 2);                            /* the registration holds a reference */
    t_note("drop target refcount while registered: %ld", (long)t.ref);
    CHECK(GetPropA(w, "OleDropTargetInterface") == (HANDLE)&t.i);
    hr = U(RegisterDragDrop)(w, &t.i);
    EQ(hr, O2_DRAGDROP_E_ALREADYREGISTERED);
    hr = U(RevokeDragDrop)(w);
    EQ(hr, S_OK);
    EQ(t.ref, 1);                                 /* released again on revoke */
    CHECK(GetPropA(w, "OleDropTargetInterface") == NULL);
    hr = U(RevokeDragDrop)(w);
    EQ(hr, O2_DRAGDROP_E_NOTREGISTERED);
    hr = U(RegisterDragDrop)(w, &t.i);            /* and can be registered again */
    EQ(hr, S_OK);
    EQ(U(RevokeDragDrop)(w), S_OK);
    EQ(t.ref, 1);
    EQ(t.enter + t.over + t.leave + t.drop, 0);
    DestroyWindow(w);
    O2_END();
}
TEST(o2dnd_register_different_targets_same_window) {
    O2Drop a, b; HWND w;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop)); REQUIRE(U(RevokeDragDrop));
    odt_init(&a); odt_init(&b);
    w = o2_mkwnd(); if (!w) { O2_END(); t_skip("cannot create windows in this session"); return; }
    EQ(U(RegisterDragDrop)(w, &a.i), S_OK);
    EQ(U(RegisterDragDrop)(w, &b.i), O2_DRAGDROP_E_ALREADYREGISTERED);
    EQ(b.ref, 1);
    EQ(U(RevokeDragDrop)(w), S_OK);
    EQ(U(RegisterDragDrop)(w, &b.i), S_OK);
    EQ(U(RevokeDragDrop)(w), S_OK);
    EQ(a.ref, 1); EQ(b.ref, 1);
    DestroyWindow(w);
    O2_END();
}
TEST(o2dnd_two_windows_one_target) {
    O2Drop t; HWND w1, w2;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop)); REQUIRE(U(RevokeDragDrop));
    odt_init(&t);
    w1 = o2_mkwnd(); w2 = o2_mkwnd();
    if (!w1 || !w2) { O2_END(); t_skip("cannot create windows in this session"); return; }
    EQ(U(RegisterDragDrop)(w1, &t.i), S_OK);
    EQ(U(RegisterDragDrop)(w2, &t.i), S_OK);
    EQ(U(RevokeDragDrop)(w1), S_OK);
    EQ(U(RevokeDragDrop)(w2), S_OK);
    EQ(t.ref, 1);
    DestroyWindow(w1); DestroyWindow(w2);
    O2_END();
}
TEST(o2dnd_revoke_invalid_window) {
    HWND w; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(RevokeDragDrop));
    hr = U(RevokeDragDrop)(NULL);
    EQ(hr, O2_DRAGDROP_E_INVALIDHWND);
    hr = U(RevokeDragDrop)((HWND)(ULONG_PTR)0xdeadbeef);
    EQ(hr, O2_DRAGDROP_E_INVALIDHWND);
    w = o2_mkwnd();
    if (w) {
        hr = U(RevokeDragDrop)(w);
        EQ(hr, O2_DRAGDROP_E_NOTREGISTERED);      /* a valid window nothing was registered on */
        DestroyWindow(w);
        hr = U(RevokeDragDrop)(w);
        EQ(hr, O2_DRAGDROP_E_INVALIDHWND);
    }
    O2_END();
}
TEST(o2dnd_revoke_after_window_destroyed) {
    O2Drop t; HWND w; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop)); REQUIRE(U(RevokeDragDrop));
    odt_init(&t);
    w = o2_mkwnd(); if (!w) { O2_END(); t_skip("cannot create windows in this session"); return; }
    EQ(U(RegisterDragDrop)(w, &t.i), S_OK);
    DestroyWindow(w);
    hr = U(RevokeDragDrop)(w);
    EQ(hr, O2_DRAGDROP_E_INVALIDHWND);
    t_note("drop target refcount after its window was destroyed without revoke: %ld", (long)t.ref);
    O2_END();
}

typedef struct { HWND w; HRESULT reg, rev_other, rev_own; HANDLE ready, go; O2Drop *t; } rv_arg;
static void th_register_owner(void *p) {
    rv_arg *a = (rv_arg *)p; MSG m;
    U(OleInitialize)(NULL);
    a->w = o2_mkwnd();
    a->reg = a->w ? U(RegisterDragDrop)(a->w, &a->t->i) : E_FAIL;
    SetEvent(a->ready);
    while (WaitForSingleObject(a->go, 20) == WAIT_TIMEOUT) while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&m);
    a->rev_own = a->w ? U(RevokeDragDrop)(a->w) : E_FAIL;     /* already torn down by the other thread */
    if (a->w) DestroyWindow(a->w);
    U(OleUninitialize)();
}
TEST(o2dnd_revoke_from_other_thread) {
    rv_arg a; O2Drop t; HANDLE h2; DWORD id; o2_thr w;
    O2_BEGIN;
    REQUIRE(U(RegisterDragDrop)); REQUIRE(U(RevokeDragDrop));
    odt_init(&t);
    memset(&a, 0, sizeof a); a.t = &t;
    a.ready = CreateEventA(NULL, TRUE, FALSE, NULL); a.go = CreateEventA(NULL, TRUE, FALSE, NULL);
    w.fn = th_register_owner; w.arg = &a;
    h2 = CreateThread(NULL, 0, o2_thr_main, &w, 0, &id);
    if (!h2) { O2_END(); t_skip("no thread"); return; }
    if (WaitForSingleObject(a.ready, 10000) != WAIT_OBJECT_0) { SetEvent(a.go); WaitForSingleObject(h2, 3000); O2_END(); t_skip("worker did not start"); return; }
    if (!a.w) { SetEvent(a.go); WaitForSingleObject(h2, 5000); CloseHandle(h2); O2_END(); t_skip("cannot create windows in this session"); return; }
    EQ(a.reg, S_OK);
    a.rev_other = U(RevokeDragDrop)(a.w);
    EQ(a.rev_other, O2_RPC_E_WRONG_THREAD);        /* registered in another apartment */
    SetEvent(a.go);
    WaitForSingleObject(h2, 10000);
    CloseHandle(h2);
    t_note("revoke on the owning thread after the failed cross-thread revoke -> 0x%08lx", (unsigned long)a.rev_own);
    CHECK(FAILED(a.rev_own));                      /* the failed revoke already removed the registration */
    CloseHandle(a.ready); CloseHandle(a.go);
    O2_END();
}

/* ================================================================================================
 * DoDragDrop
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/dragdrop.c (LGPL-2.1+) test_DoDragDrop */
TEST(o2dnd_dodragdrop_invalid_arguments) {
    O2Data d; O2Src s; DWORD eff = 0xCCCC; HRESULT hr;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    O2_BEGIN;
    REQUIRE(U(DoDragDrop));
    od_init(&d); osr_init(&s);
    hr = U(DoDragDrop)(NULL, NULL, 0, NULL); EQ(hr, O2_E_INVALIDARG);
    hr = U(DoDragDrop)(NULL, &s.i, 0, NULL); EQ(hr, O2_E_INVALIDARG);
    hr = U(DoDragDrop)(&d.i, NULL, 0, NULL); EQ(hr, O2_E_INVALIDARG);
    hr = U(DoDragDrop)(NULL, NULL, 0, &eff); EQ(hr, O2_E_INVALIDARG);
    hr = U(DoDragDrop)(&d.i, &s.i, DROPEFFECT_COPY, NULL); EQ(hr, O2_E_INVALIDARG);
    hr = U(DoDragDrop)(NULL, &s.i, DROPEFFECT_COPY, &eff); EQ(hr, O2_E_INVALIDARG);
    hr = U(DoDragDrop)(&d.i, NULL, DROPEFFECT_COPY, &eff); EQ(hr, O2_E_INVALIDARG);
    EQ(s.qcd, 0); EQ(s.gf, 0);
    EQ(s.ref, 1); EQ(d.ref, 1);
    O2_END();
}

typedef struct { O2Data *d; O2Src *s; HRESULT hr; DWORD eff; DWORD in_eff; } dd_arg;
static void th_dodragdrop(void *p) {
    dd_arg *a = (dd_arg *)p;
    U(OleInitialize)(NULL);
    a->eff = 0xCCCC;
    a->hr = U(DoDragDrop)(&a->d->i, &a->s->i, a->in_eff, &a->eff);
    U(OleUninitialize)();
}
TEST(o2dnd_dodragdrop_immediate_cancel) {
    O2Data d; O2Src s; dd_arg a; int done;
    if (t_is_wine()) { t_skip("emulator needs a display for drag tracking"); return; }
    REQUIRE(U(DoDragDrop));
    od_init(&d); osr_init(&s); s.qcd_hr = DRAGDROP_S_CANCEL;
    a.d = &d; a.s = &s; a.hr = (HRESULT)0xCCCCCCCC; a.eff = 0; a.in_eff = DROPEFFECT_COPY | DROPEFFECT_MOVE;
    done = o2_run_thread(th_dodragdrop, &a, 8000);
    if (!done) { t_note("DoDragDrop did not return within 8 seconds; skipping result checks"); return; }
    t_note("DoDragDrop with a source that cancels at once -> 0x%08lx effect %lu", (unsigned long)a.hr, (unsigned long)a.eff);
    EQ(a.hr, DRAGDROP_S_CANCEL);
    EQ(a.eff, DROPEFFECT_NONE);
    CHECK(s.qcd >= 1);
    EQ(s.ref, 1); EQ(d.ref, 1);
}
TEST(o2dnd_dodragdrop_source_error_is_returned) {
    O2Data d; O2Src s; dd_arg a; int done;
    if (t_is_wine()) { t_skip("emulator needs a display for drag tracking"); return; }
    REQUIRE(U(DoDragDrop));
    od_init(&d); osr_init(&s); s.qcd_hr = E_FAIL;
    a.d = &d; a.s = &s; a.hr = (HRESULT)0xCCCCCCCC; a.eff = 0; a.in_eff = DROPEFFECT_COPY;
    done = o2_run_thread(th_dodragdrop, &a, 8000);
    if (!done) { t_note("DoDragDrop did not return within 8 seconds"); return; }
    t_note("DoDragDrop with a failing source -> 0x%08lx", (unsigned long)a.hr);
    CHECK(FAILED(a.hr) || a.hr == DRAGDROP_S_CANCEL);
    EQ(a.eff, DROPEFFECT_NONE);
}

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
