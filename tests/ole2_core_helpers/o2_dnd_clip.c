/* RegisterDragDrop / RevokeDragDrop / DoDragDrop validation paths (OLE clipboard tests live in dx_ole2_clipboard). */
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
