/* OleInitialize/OleUninitialize apartment interplay, OleBuildVersion, the IRunnableObject helpers
 * (OleRun/OleIsRunning/OleLockRunning/OleSetContainedObject/OleNoteObjectVisible), OleDraw and the
 * OleQuery*FromData classifiers. Expectations come from the Win10 ole32 decompilation. */
#include "olh.h"

/* ================================================================================================
 * OleInitialize / OleUninitialize
 * ============================================================================================== */
typedef struct { HRESULT r[8]; int n; } seq_t;

static void th_nesting(void *p) {
    seq_t *s = (seq_t *)p;
    s->r[0] = U(OleInitialize)(NULL);
    s->r[1] = U(OleInitialize)(NULL);
    s->r[2] = U(OleInitialize)(NULL);
    U(OleUninitialize)(); U(OleUninitialize)(); U(OleUninitialize)();
    s->r[3] = U(OleInitialize)(NULL);       /* count is back to zero: first init again */
    U(OleUninitialize)();
    s->r[4] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED); /* apartment fully torn down */
    U(CoUninitialize)();
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) OleInitialize counting */
TEST(o2init_nesting_s_false) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_nesting, &s, 10000));
    EQ(s.r[0], S_OK); EQ(s.r[1], S_FALSE); EQ(s.r[2], S_FALSE);
    EQ(s.r[3], S_OK);
    EQ(s.r[4], S_OK);
}

static void th_after_coinit(void *p) {
    seq_t *s = (seq_t *)p;
    s->r[0] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    s->r[1] = U(OleInitialize)(NULL);            /* OLE init count is independent of the COM count: S_OK */
    U(OleUninitialize)();
    s->r[2] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED); /* COM still initialized: S_FALSE */
    U(CoUninitialize)();
    U(CoUninitialize)();
}
TEST(o2init_after_coinitialize_sta) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_after_coinit, &s, 10000));
    EQ(s.r[0], S_OK);
    EQ(s.r[1], S_OK);
    EQ(s.r[2], S_FALSE);
}

static void th_double_coinit(void *p) {
    seq_t *s = (seq_t *)p;
    s->r[0] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    s->r[1] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    s->r[2] = U(OleInitialize)(NULL);
    s->r[3] = U(OleInitialize)(NULL);
    U(OleUninitialize)(); U(OleUninitialize)();
    U(CoUninitialize)(); U(CoUninitialize)();
}
TEST(o2init_ole_count_ignores_com_count) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_double_coinit, &s, 10000));
    EQ(s.r[0], S_OK); EQ(s.r[1], S_FALSE);
    EQ(s.r[2], S_OK);      /* first OleInitialize on the thread */
    EQ(s.r[3], S_FALSE);   /* second */
}

static void th_mta(void *p) {
    seq_t *s = (seq_t *)p;
    s->r[0] = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    s->r[1] = U(OleInitialize)(NULL);   /* OLE needs an STA */
    U(CoUninitialize)();
    s->r[2] = U(OleInitialize)(NULL);   /* the failed attempt left no OLE count behind */
    U(OleUninitialize)();
}
TEST(o2init_mta_thread_changed_mode) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_mta, &s, 10000));
    EQ(s.r[0], S_OK);
    EQ(s.r[1], O2_RPC_E_CHANGED_MODE);
    EQ(s.r[2], S_OK);
}

static void th_ole_then_mta(void *p) {
    seq_t *s = (seq_t *)p;
    s->r[0] = U(OleInitialize)(NULL);
    s->r[1] = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    s->r[2] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);   /* OLE made the apartment an STA */
    U(CoUninitialize)();
    U(OleUninitialize)();
}
TEST(o2init_ole_then_mta_changed_mode) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_ole_then_mta, &s, 10000));
    EQ(s.r[0], S_OK);
    EQ(s.r[1], O2_RPC_E_CHANGED_MODE);
    EQ(s.r[2], S_FALSE);
}

static void th_unbalanced(void *p) {
    seq_t *s = (seq_t *)p;
    U(OleUninitialize)();                         /* nothing to undo: must not touch the apartment */
    s->r[0] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);   /* fresh thread: still S_OK */
    U(CoUninitialize)();
    s->r[1] = U(OleInitialize)(NULL);
    U(OleUninitialize)();
    U(OleUninitialize)();                         /* extra uninit after a balanced pair */
    U(OleUninitialize)();
    s->r[2] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);   /* apartment must be gone: S_OK */
    U(CoUninitialize)();
    s->r[3] = U(OleInitialize)(NULL);            /* and the OLE count must not have gone negative */
    U(OleUninitialize)();
}
TEST(o2init_unbalanced_uninitialize_is_harmless) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_unbalanced, &s, 10000));
    EQ(s.r[0], S_OK); EQ(s.r[1], S_OK); EQ(s.r[2], S_OK); EQ(s.r[3], S_OK);
}

static void th_other_thread(void *p) {
    seq_t *s = (seq_t *)p;
    s->r[0] = U(OleInitialize)(NULL);   /* another thread's OLE init does not count against this one */
    U(OleUninitialize)();
}
TEST(o2init_count_is_per_thread) {
    seq_t s; HRESULT hr;
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize));
    memset(&s, 0xAA, sizeof s);
    hr = U(OleInitialize)(NULL); EQ(hr, S_OK);
    CHECK(o2_run_thread(th_other_thread, &s, 10000));
    EQ(s.r[0], S_OK);
    hr = U(OleInitialize)(NULL); EQ(hr, S_FALSE);
    U(OleUninitialize)(); U(OleUninitialize)();
}

static void th_noinit_calls(void *p) {
    seq_t *s = (seq_t *)p;
    /* COM never initialized on this thread */
    s->r[0] = U(DoDragDrop)(NULL, NULL, 0, NULL);
    s->r[1] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    s->r[2] = U(DoDragDrop)(NULL, NULL, 0, NULL);   /* COM yes, OLE no */
    U(CoUninitialize)();
}
/* adapted from Wine dlls/ole32/tests/dragdrop.c (LGPL-2.1+) test_DoDragDrop */
TEST(o2init_dodragdrop_requires_ole) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(DoDragDrop)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_noinit_calls, &s, 10000));
    EQ(s.r[0], O2_CO_E_NOTINITIALIZED);
    EQ(s.r[1], S_OK);
    EQ(s.r[2], O2_CO_E_NOTINITIALIZED);  /* the not-initialized check wins over the invalid-argument check */
}

static void th_clip_noinit(void *p) {
    seq_t *s = (seq_t *)p;
    O2Data d; od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
    s->r[0] = U(OleSetClipboard)(&d.i);
    s->r[1] = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    s->r[2] = U(OleSetClipboard)(&d.i);
    U(CoUninitialize)();
    s->n = (int)d.ref;
}
/* adapted from Wine dlls/ole32/tests/clipboard.c (LGPL-2.1+) test_set_clipboard */
TEST(o2init_setclipboard_requires_ole) {
    seq_t s; memset(&s, 0xAA, sizeof s);
    REQUIRE(U(OleSetClipboard)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(o2_run_thread(th_clip_noinit, &s, 10000));
    EQ(s.r[0], O2_CO_E_NOTINITIALIZED);
    EQ(s.r[1], S_OK);
    EQ(s.r[2], O2_CO_E_NOTINITIALIZED);
    EQ(s.n, 1);   /* a failed OleSetClipboard keeps no reference */
}

/* ---- OleBuildVersion ---------------------------------------------------------------------------- */
TEST(o2ver_build_version) {
    DWORD v;
    REQUIRE(U(OleBuildVersion));
    v = U(OleBuildVersion)();
    EQ(HIWORD(v), 23);      /* rmm */
    t_note("OleBuildVersion = 0x%08lx (rup %u)", (unsigned long)v, (unsigned)LOWORD(v));
    EQ(v, 0x170338);        /* value of the Win10 19041 ole32 */
}
TEST(o2ver_equals_cobuildversion) {
    REQUIRE(U(OleBuildVersion)); REQUIRE(U(CoBuildVersion));
    EQ(U(OleBuildVersion)(), U(CoBuildVersion)());
    /* same code: both exports resolve to the same body */
    EQ(U(OleBuildVersion)(), U(OleBuildVersion)());
}
TEST(o2ver_matches_ole2ver_constants) {
    DWORD v;
    REQUIRE(U(OleBuildVersion));
    v = U(OleBuildVersion)();
    EQ(HIWORD(v), 23);
    CHECK(LOWORD(v) > 0);
}

/* ================================================================================================
 * IRunnableObject helpers
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_OleRun */
TEST(o2run_oleRun_null) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleRun));
    EQ(U(OleRun)(NULL), O2_E_INVALIDARG);
}
TEST(o2run_oleRun_without_irunnable_is_ok) {
    O2Run o; HRESULT hr;
    REQUIRE(U(OleRun));
    or_init(&o); o.expose_runnable = 0;
    hr = U(OleRun)((IUnknown *)&o.i);
    EQ(hr, S_OK);
    EQ(o.qi_runnable, 1);
    EQ(o.run_calls, 0);
    EQ(o.ref, 1);
}
TEST(o2run_oleRun_forwards_hresult) {
    static const HRESULT vals[] = { S_OK, S_FALSE, E_FAIL, (HRESULT)0xdeadc0de, REGDB_E_CLASSNOTREG, OLE_E_CLASSDIFF, 0x00040001 };
    int i;
    REQUIRE(U(OleRun));
    for (i = 0; i < O2_N(vals); i++) {
        O2Run o; HRESULT hr;
        or_init(&o); o.run_hr = vals[i];
        hr = U(OleRun)((IUnknown *)&o.i);
        EQ(hr, vals[i]);
        EQ(o.run_calls, 1);
        EQ(o.run_ctx, NULL);        /* OleRun passes a NULL bind context */
        EQ(o.ref, 1);               /* the QueryInterface reference is released */
    }
}
TEST(o2run_oleIsRunning_null) {
    REQUIRE(U(OleIsRunning));
    EQ(U(OleIsRunning)(NULL), FALSE);
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_runnable */
TEST(o2run_oleIsRunning_semantics) {
    O2Run o; BOOL r;
    REQUIRE(U(OleIsRunning));
    or_init(&o);
    o.running = TRUE;
    r = U(OleIsRunning)((IOleObject *)&o.i);
    EQ(r, TRUE); EQ(o.isrunning_calls, 1); EQ(o.ref, 1);
    o.running = FALSE;
    r = U(OleIsRunning)((IOleObject *)&o.i);
    EQ(r, FALSE); EQ(o.isrunning_calls, 2); EQ(o.ref, 1);
    /* no IRunnableObject: the object is considered running */
    o.expose_runnable = 0; o.running = FALSE;
    r = U(OleIsRunning)((IOleObject *)&o.i);
    EQ(r, TRUE); EQ(o.isrunning_calls, 2); EQ(o.ref, 1);
}
TEST(o2run_oleLockRunning_null) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleLockRunning));
    EQ(U(OleLockRunning)(NULL, TRUE, FALSE), O2_E_INVALIDARG);
    EQ(U(OleLockRunning)(NULL, FALSE, TRUE), O2_E_INVALIDARG);
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_OleLockRunning */
TEST(o2run_oleLockRunning_without_irunnable_is_ok) {
    O2Run o;
    REQUIRE(U(OleLockRunning));
    or_init(&o); o.expose_runnable = 0;
    EQ(U(OleLockRunning)((IUnknown *)&o.i, TRUE, FALSE), S_OK);
    EQ(o.lock_calls, 0); EQ(o.ref, 1);
}
TEST(o2run_oleLockRunning_forwards_args) {
    static const struct { BOOL lock, close; HRESULT hr; } c[] = {
        { TRUE, FALSE, S_OK }, { TRUE, TRUE, S_OK }, { FALSE, FALSE, S_FALSE }, { FALSE, TRUE, E_FAIL },
        { 2, 0, (HRESULT)0x12345678 },
    };
    int i;
    REQUIRE(U(OleLockRunning));
    for (i = 0; i < O2_N(c); i++) {
        O2Run o; HRESULT hr;
        or_init(&o); o.lock_hr = c[i].hr;
        hr = U(OleLockRunning)((IUnknown *)&o.i, c[i].lock, c[i].close);
        EQ(hr, c[i].hr);
        EQ(o.lock_calls, 1);
        EQ(o.last_lock, c[i].lock);
        EQ(o.last_unlock_closes, c[i].close);
        EQ(o.ref, 1);
    }
}
TEST(o2run_oleSetContainedObject_null) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleSetContainedObject));
    EQ(U(OleSetContainedObject)(NULL, TRUE), O2_E_INVALIDARG);
    EQ(U(OleSetContainedObject)(NULL, FALSE), O2_E_INVALIDARG);
}
TEST(o2run_oleSetContainedObject_without_irunnable_is_ok) {
    O2Run o;
    REQUIRE(U(OleSetContainedObject));
    or_init(&o); o.expose_runnable = 0;
    EQ(U(OleSetContainedObject)((IUnknown *)&o.i, TRUE), S_OK);
    EQ(o.contained_calls, 0); EQ(o.ref, 1);
}
TEST(o2run_oleSetContainedObject_forwards) {
    static const struct { BOOL contained; HRESULT hr; } c[] = { { TRUE, S_OK }, { FALSE, S_OK }, { TRUE, E_UNEXPECTED }, { FALSE, S_FALSE } };
    int i;
    REQUIRE(U(OleSetContainedObject));
    for (i = 0; i < O2_N(c); i++) {
        O2Run o; HRESULT hr;
        or_init(&o); o.contained_hr = c[i].hr;
        hr = U(OleSetContainedObject)((IUnknown *)&o.i, c[i].contained);
        EQ(hr, c[i].hr);
        EQ(o.contained_calls, 1); EQ(o.last_contained, c[i].contained); EQ(o.ref, 1);
    }
}
TEST(o2run_runnable_helpers_query_only_irunnable) {
    O2Run o;
    REQUIRE(U(OleRun)); REQUIRE(U(OleIsRunning)); REQUIRE(U(OleLockRunning)); REQUIRE(U(OleSetContainedObject));
    or_init(&o);
    U(OleRun)((IUnknown *)&o.i);
    U(OleIsRunning)((IOleObject *)&o.i);
    U(OleLockRunning)((IUnknown *)&o.i, TRUE, TRUE);
    U(OleSetContainedObject)((IUnknown *)&o.i, TRUE);
    EQ(o.qi_runnable, 4);
    EQ(o.qi_other, 0);
}

/* OleNoteObjectVisible is CoLockObjectExternal(punk, fVisible, TRUE). */
TEST(o2run_noteObjectVisible_null) {
    HRESULT hr;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    O2_BEGIN;
    REQUIRE(U(OleNoteObjectVisible));
    hr = U(OleNoteObjectVisible)(NULL, TRUE);
    CHECK(FAILED(hr));
    t_note("OleNoteObjectVisible(NULL, TRUE) -> 0x%08lx", (unsigned long)hr);
    hr = U(OleNoteObjectVisible)(NULL, FALSE);
    CHECK(FAILED(hr));
    O2_END();
}
TEST(o2run_noteObjectVisible_lock_unlock) {
    O2Run o; HRESULT hr; LONG locked;
    O2_BEGIN;
    REQUIRE(U(OleNoteObjectVisible));
    or_init(&o);
    hr = U(OleNoteObjectVisible)((IUnknown *)&o.i, TRUE);
    EQ(hr, S_OK);
    locked = o.ref;
    t_note("refcount while object is visible-locked: %ld", (long)locked);
    CHECK(locked >= 1);
    hr = U(OleNoteObjectVisible)((IUnknown *)&o.i, FALSE);
    EQ(hr, S_OK);
    EQ(o.ref, 1);       /* unlock with fLastUnlockReleases drops the external lock reference */
    O2_END();
}
TEST(o2run_noteObjectVisible_matches_colockobjectexternal) {
    O2Run a, b; HRESULT h1, h2; LONG ra, rb;
    O2_BEGIN;
    REQUIRE(U(OleNoteObjectVisible)); REQUIRE(U(CoLockObjectExternal));
    or_init(&a); or_init(&b);
    h1 = U(OleNoteObjectVisible)((IUnknown *)&a.i, TRUE);
    h2 = U(CoLockObjectExternal)((IUnknown *)&b.i, TRUE, TRUE);
    EQ(h1, h2);
    ra = a.ref; rb = b.ref;
    EQ(ra, rb);
    U(OleNoteObjectVisible)((IUnknown *)&a.i, FALSE);
    U(CoLockObjectExternal)((IUnknown *)&b.i, FALSE, TRUE);
    EQ(a.ref, b.ref);
    O2_END();
}

/* ================================================================================================
 * OleDraw
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_OleDraw */
TEST(o2draw_null_object) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    RECT rc = { 0, 0, 10, 10 };
    REQUIRE(U(OleDraw));
    EQ(U(OleDraw)(NULL, DVASPECT_CONTENT, NULL, &rc), O2_E_INVALIDARG);
    EQ(U(OleDraw)(NULL, DVASPECT_CONTENT, (HDC)0x1, NULL), O2_E_INVALIDARG);
}
TEST(o2draw_object_without_viewobject) {
    O2Run o; RECT rc = { 0, 0, 10, 10 };
    REQUIRE(U(OleDraw));
    or_init(&o);
    EQ(U(OleDraw)((IUnknown *)&o.i, DVASPECT_CONTENT, (HDC)0x1, &rc), O2_DV_E_NOIVIEWOBJECT);
    EQ(o.ref, 1);
}
TEST(o2draw_forwards_arguments) {
    static const DWORD aspects[] = { DVASPECT_CONTENT, DVASPECT_THUMBNAIL, DVASPECT_ICON, DVASPECT_DOCPRINT };
    int i; HDC dc;
    REQUIRE(U(OleDraw));
    dc = UIN("gdi32.dll", CreateCompatibleDC)(NULL);
    for (i = 0; i < O2_N(aspects); i++) {
        O2View v; RECT rc = { 1, 2, 30, 40 }; HRESULT hr;
        ov_init(&v);
        hr = U(OleDraw)((IUnknown *)&v.i, aspects[i], dc, &rc);
        EQ(hr, S_OK);
        EQ(v.draws, 1);
        EQ(v.aspect, aspects[i]);
        EQ(v.lindex, -1);
        EQ(v.hdc, dc);
        EQ(v.pvAspect, NULL); EQ(v.ptd, NULL); EQ(v.hicTarget, NULL); EQ(v.wbounds, NULL);
        EQ(v.cont, NULL); EQ(v.dwcont, 0);
        EQ(v.bounds, &rc);
        EQ(v.ref, 1);
    }
    UIN("gdi32.dll", DeleteDC)(dc);
}
TEST(o2draw_null_bounds_and_hdc_forwarded) {
    O2View v; HRESULT hr;
    REQUIRE(U(OleDraw));
    ov_init(&v);
    hr = U(OleDraw)((IUnknown *)&v.i, DVASPECT_CONTENT, NULL, NULL);
    EQ(hr, S_OK); EQ(v.draws, 1); EQ(v.hdc, NULL); EQ(v.bounds, NULL);
}
TEST(o2draw_forwards_hresult) {
    static const HRESULT vals[] = { S_FALSE, E_ABORT, VIEW_E_DRAW, OLE_E_BLANK, (HRESULT)0x00123456 };
    int i;
    REQUIRE(U(OleDraw));
    for (i = 0; i < O2_N(vals); i++) {
        O2View v; RECT rc = { 0, 0, 1, 1 };
        ov_init(&v); v.draw_hr = vals[i];
        EQ(U(OleDraw)((IUnknown *)&v.i, DVASPECT_CONTENT, NULL, &rc), vals[i]);
    }
}

/* ================================================================================================
 * OleQueryCreateFromData / OleQueryLinkFromData
 * ============================================================================================== */
static CLIPFORMAT cf_embedsrc, cf_embedobj, cf_filename, cf_filenamew, cf_linksrc, cf_objdesc, cf_ownerlink;
static void cf_init(void) {
    cf_embedsrc  = (CLIPFORMAT)RegisterClipboardFormatA("Embed Source");
    cf_embedobj  = (CLIPFORMAT)RegisterClipboardFormatA("Embedded Object");
    cf_filename  = (CLIPFORMAT)RegisterClipboardFormatA("FileName");
    cf_filenamew = (CLIPFORMAT)RegisterClipboardFormatA("FileNameW");
    cf_linksrc   = (CLIPFORMAT)RegisterClipboardFormatA("Link Source");
    cf_objdesc   = (CLIPFORMAT)RegisterClipboardFormatA("Object Descriptor");
    cf_ownerlink = (CLIPFORMAT)RegisterClipboardFormatA("OwnerLink");
}
TEST(o2query_createfromdata_null) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleQueryCreateFromData));
    EQ(U(OleQueryCreateFromData)(NULL), O2_E_INVALIDARG);
}
TEST(o2query_linkfromdata_null) {
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleQueryLinkFromData));
    EQ(U(OleQueryLinkFromData)(NULL), O2_E_INVALIDARG);
}
TEST(o2query_createfromdata_formats) {
    struct { CLIPFORMAT *cf1; CLIPFORMAT fixed1; CLIPFORMAT *cf2; CLIPFORMAT fixed2; HRESULT want; const char *what; } c[] = {
        { &cf_embedobj, 0, NULL, 0, S_OK, "Embedded Object" },
        { &cf_embedsrc, 0, NULL, 0, S_OK, "Embed Source" },
        { &cf_filename, 0, NULL, 0, S_OK, "FileName" },
        { &cf_filenamew, 0, NULL, 0, S_OK, "FileNameW" },
        { NULL, CF_TEXT, NULL, 0, S_FALSE, "CF_TEXT" },
        { NULL, CF_UNICODETEXT, NULL, 0, S_FALSE, "CF_UNICODETEXT" },
        { &cf_linksrc, 0, NULL, 0, S_FALSE, "Link Source" },
        { &cf_objdesc, 0, NULL, 0, S_FALSE, "Object Descriptor" },
        { NULL, CF_BITMAP, NULL, 0, O2_OLE_S_STATIC, "CF_BITMAP" },
        { NULL, CF_METAFILEPICT, NULL, 0, O2_OLE_S_STATIC, "CF_METAFILEPICT" },
        { NULL, CF_DIB, NULL, 0, O2_OLE_S_STATIC, "CF_DIB" },
        { NULL, CF_ENHMETAFILE, NULL, 0, O2_OLE_S_STATIC, "CF_ENHMETAFILE" },
        { NULL, CF_TEXT, NULL, CF_DIB, O2_OLE_S_STATIC, "CF_TEXT+CF_DIB" },
        { NULL, CF_DIB, &cf_embedsrc, 0, S_OK, "CF_DIB+Embed Source (embed wins over static)" },
        { &cf_embedsrc, 0, NULL, CF_DIB, S_OK, "Embed Source+CF_DIB" },
        { NULL, CF_TEXT, &cf_filenamew, 0, S_OK, "CF_TEXT+FileNameW" },
    };
    int i;
    REQUIRE(U(OleQueryCreateFromData));
    O2_BEGIN;
    cf_init();
    for (i = 0; i < O2_N(c); i++) {
        O2Data d; HRESULT hr; CLIPFORMAT a = c[i].cf1 ? *c[i].cf1 : c[i].fixed1;
        od_init(&d); od_add(&d, a, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
        if (c[i].cf2 || c[i].fixed2) od_add(&d, c[i].cf2 ? *c[i].cf2 : c[i].fixed2, TYMED_HGLOBAL, DVASPECT_CONTENT, "y");
        hr = U(OleQueryCreateFromData)(&d.i);
        CHECKF(hr == c[i].want, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].want);
        EQ(d.ref, 1);
    }
    O2_END();
}
TEST(o2query_createfromdata_empty_object) {
    O2Data d; HRESULT hr;
    REQUIRE(U(OleQueryCreateFromData));
    O2_BEGIN;
    od_init(&d);
    hr = U(OleQueryCreateFromData)(&d.i);
    EQ(hr, S_FALSE);
    CHECK(d.enumcalls >= 1);
    EQ(d.ref, 1);
    O2_END();
}
TEST(o2query_createfromdata_enum_failure) {
    O2Data d; HRESULT hr;
    REQUIRE(U(OleQueryCreateFromData));
    O2_BEGIN;
    cf_init();
    od_init(&d); od_add(&d, cf_embedsrc, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
    d.enum_hr_override = 1; d.enum_hr = E_NOTIMPL;
    hr = U(OleQueryCreateFromData)(&d.i);
    t_note("OleQueryCreateFromData with failing EnumFormatEtc -> 0x%08lx", (unsigned long)hr);
    CHECK(hr == S_FALSE || FAILED(hr));
    O2_END();
}
TEST(o2query_linkfromdata_formats) {
    struct { CLIPFORMAT *cf; CLIPFORMAT fixed; HRESULT want; const char *what; } c[] = {
        { &cf_linksrc, 0, S_OK, "Link Source" },
        { &cf_filename, 0, S_OK, "FileName" },
        { &cf_filenamew, 0, S_OK, "FileNameW" },
        { &cf_embedsrc, 0, S_FALSE, "Embed Source" },
        { &cf_embedobj, 0, S_FALSE, "Embedded Object" },
        { &cf_ownerlink, 0, S_FALSE, "OwnerLink" },
        { NULL, CF_TEXT, S_FALSE, "CF_TEXT" },
        { NULL, CF_DIB, S_FALSE, "CF_DIB" },
    };
    int i;
    REQUIRE(U(OleQueryLinkFromData));
    O2_BEGIN;
    cf_init();
    for (i = 0; i < O2_N(c); i++) {
        O2Data d; HRESULT hr;
        od_init(&d); od_add(&d, c[i].cf ? *c[i].cf : c[i].fixed, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
        hr = U(OleQueryLinkFromData)(&d.i);
        CHECKF(hr == c[i].want, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].want);
        EQ(d.ref, 1);
    }
    O2_END();
}
TEST(o2query_linkfromdata_mixed_and_empty) {
    O2Data d; HRESULT hr;
    REQUIRE(U(OleQueryLinkFromData));
    O2_BEGIN;
    cf_init();
    od_init(&d);
    EQ(U(OleQueryLinkFromData)(&d.i), S_FALSE);
    od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "a");
    od_add(&d, CF_DIB, TYMED_HGLOBAL, DVASPECT_CONTENT, "b");
    EQ(U(OleQueryLinkFromData)(&d.i), S_FALSE);
    od_add(&d, cf_linksrc, TYMED_HGLOBAL, DVASPECT_CONTENT, "c");
    EQ(U(OleQueryLinkFromData)(&d.i), S_OK);       /* found behind unrelated formats */
    hr = U(OleQueryLinkFromData)(&d.i);
    EQ(hr, S_OK);
    EQ(d.ref, 1);
    O2_END();
}
TEST(o2query_createandlink_independent) {
    O2Data d;
    REQUIRE(U(OleQueryLinkFromData)); REQUIRE(U(OleQueryCreateFromData));
    O2_BEGIN;
    cf_init();
    od_init(&d);
    od_add(&d, cf_embedsrc, TYMED_HGLOBAL, DVASPECT_CONTENT, "e");
    od_add(&d, cf_linksrc, TYMED_HGLOBAL, DVASPECT_CONTENT, "l");
    EQ(U(OleQueryCreateFromData)(&d.i), S_OK);
    EQ(U(OleQueryLinkFromData)(&d.i), S_OK);
    O2_END();
}
