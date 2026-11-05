/* The drop target registration ordinals 820 (OleRegisterDropTargetInternal) and 821 (OleRevokeDropTargetInternal), and how
 * RegisterDragDrop / RevokeDragDrop behave on a thread that has no apartment. */
#include "ext.h"

typedef HRESULT (WINAPI *rd_t)(HWND, IDropTarget *);
typedef HRESULT (WINAPI *rv_t)(HWND);

static HWND mkwnd(void) { return CreateWindowExA(0, "STATIC", "x", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandle(NULL), NULL); }

/* ---- a thread without any apartment --------------------------------------------------------------------- */
static void noinit_thread(void *unused)
{
    rd_t r820 = (rd_t)t_proc_ord("ole32.dll", 820); rv_t r821 = (rv_t)t_proc_ord("ole32.dll", 821);
    rd_t rd = (rd_t)t_proc_in("ole32.dll", "RegisterDragDrop"); rv_t rv = (rv_t)t_proc_in("ole32.dll", "RevokeDragDrop");
    HWND w; IDropTarget *t = x_drop_target();
    if (!r820 || !r821 || !rd || !rv) { t_skip("missing export"); return; }
    w = mkwnd();
    if (!w) { t_skip("cannot create windows in this session"); return; }
    EQ(x_apt_type(), 0xffff);
    /* the public pair checks its arguments first (target, then window) and only then the apartment */
    EQ(rd(w, NULL), X_E_INVALIDARG);
    EQ(rd(NULL, NULL), X_E_INVALIDARG);
    EQ(rd(NULL, t), X_DRAGDROP_E_INVALIDHWND);
    EQ(rd((HWND)(ULONG_PTR)0xdeadbeef, t), X_DRAGDROP_E_INVALIDHWND);
    EQ(rd(w, t), X_CO_E_NOTINITIALIZED);
    EQ(rv(NULL), X_DRAGDROP_E_INVALIDHWND);
    EQ(rv((HWND)(ULONG_PTR)0xdeadbeef), X_DRAGDROP_E_INVALIDHWND);
    EQ(rv(w), X_RPC_E_WRONG_THREAD);          /* the thread has no apartment id to match the window's */
    CHECK(GetPropA(w, "OleDropTargetInterface") == NULL);   /* the failed attempts leave nothing behind */
    /* the internal pair: the apartment first, whatever the arguments */
    EQ(r820(w, NULL), X_CO_E_NOTINITIALIZED);
    EQ(r820(NULL, NULL), X_CO_E_NOTINITIALIZED);
    EQ(r820(NULL, t), X_CO_E_NOTINITIALIZED);
    EQ(r820(w, t), X_CO_E_NOTINITIALIZED);
    EQ(r821(w), X_CO_E_NOTINITIALIZED);
    EQ(r821(NULL), X_CO_E_NOTINITIALIZED);
    EQ(r821((HWND)(ULONG_PTR)0xdeadbeef), X_CO_E_NOTINITIALIZED);
    DestroyWindow(w);
}
TEST(oex_drop_target_registration_without_an_apartment) { x_on_thread(noinit_thread, NULL); }

/* ---- an OLE-initialized thread: the internal pair is the public pair ------------------------------------- */
static void ole_thread(void *unused)
{
    rd_t r820 = (rd_t)t_proc_ord("ole32.dll", 820); rv_t r821 = (rv_t)t_proc_ord("ole32.dll", 821);
    rd_t rd = (rd_t)t_proc_in("ole32.dll", "RegisterDragDrop"); rv_t rv = (rv_t)t_proc_in("ole32.dll", "RevokeDragDrop");
    HWND w; IDropTarget *t = x_drop_target();
    if (!r820 || !r821 || !rd || !rv) { t_skip("missing export"); return; }
    w = mkwnd();
    if (!w) { t_skip("cannot create windows in this session"); return; }
    EQ(OleInitialize(NULL), S_OK);
    EQ(r820(w, NULL), X_E_INVALIDARG);
    EQ(r820(NULL, t), X_DRAGDROP_E_INVALIDHWND);
    EQ(r820((HWND)(ULONG_PTR)0xdeadbeef, t), X_DRAGDROP_E_INVALIDHWND);
    EQ(r821(NULL), X_DRAGDROP_E_INVALIDHWND);
    EQ(r821((HWND)(ULONG_PTR)0xdeadbeef), X_DRAGDROP_E_INVALIDHWND);
    EQ(r821(w), X_DRAGDROP_E_NOTREGISTERED);
    EQ(r820(w, t), S_OK);
    CHECK(GetPropA(w, "OleDropTargetInterface") == (HANDLE)t);
    EQ(r820(w, t), X_DRAGDROP_E_ALREADYREGISTERED);
    EQ(rd(w, t), X_DRAGDROP_E_ALREADYREGISTERED);            /* one registration table */
    EQ(r821(w), S_OK);
    EQ(rv(w), X_DRAGDROP_E_NOTREGISTERED);
    EQ(rd(w, t), S_OK);
    EQ(r820(w, t), X_DRAGDROP_E_ALREADYREGISTERED);
    EQ(r821(w), S_OK);                                        /* revokes what RegisterDragDrop registered */
    EQ(r821(w), X_DRAGDROP_E_NOTREGISTERED);
    EQ(r820(w, t), S_OK);
    EQ(rv(w), S_OK);                                          /* and the other way round */
    OleUninitialize();
    DestroyWindow(w);
}
TEST(oex_drop_target_registration_internal_pair_matches_the_public_pair) { x_on_thread(ole_thread, NULL); }

/* ---- COM alone is an apartment enough (no OleInitialize needed); an MTA thread registers too ------------------ */
static void coinit_thread(void *arg)
{
    DWORD model = (DWORD)(ULONG_PTR)arg;
    rd_t r820 = (rd_t)t_proc_ord("ole32.dll", 820); rv_t r821 = (rv_t)t_proc_ord("ole32.dll", 821);
    rd_t rd = (rd_t)t_proc_in("ole32.dll", "RegisterDragDrop"); rv_t rv = (rv_t)t_proc_in("ole32.dll", "RevokeDragDrop");
    HWND w; IDropTarget *t = x_drop_target();
    if (!r820 || !r821 || !rd || !rv) { t_skip("missing export"); return; }
    w = mkwnd();
    if (!w) { t_skip("cannot create windows in this session"); return; }
    EQ(CoInitializeEx(NULL, model), S_OK);
    EQ(rv(w), X_DRAGDROP_E_NOTREGISTERED);        /* with an apartment of either kind the unregistered answer is NOTREGISTERED */
    EQ(r821(w), X_DRAGDROP_E_NOTREGISTERED);
    EQ(rd(w, t), S_OK);
    EQ(rv(w), S_OK);
    EQ(r820(w, t), S_OK);
    EQ(r821(w), S_OK);
    CoUninitialize();
    EQ(r821(w), X_CO_E_NOTINITIALIZED);           /* once the apartment is gone the internal pair asks for one again */
    DestroyWindow(w);
}
TEST(oex_drop_target_registration_in_a_com_only_sta) { x_on_thread(coinit_thread, (void *)(ULONG_PTR)COINIT_APARTMENTTHREADED); }
TEST(oex_drop_target_registration_in_an_mta) { x_on_thread(coinit_thread, (void *)(ULONG_PTR)COINIT_MULTITHREADED); }

/* A thread that had COM and let it go is not a thread that never had it: RevokeDragDrop of an unregistered window is NOTREGISTERED
 * (the never-initialized thread of oex_drop_target_registration_without_an_apartment gets WRONG_THREAD). */
static void after_uninit_thread(void *unused)
{
    rv_t rv = (rv_t)t_proc_in("ole32.dll", "RevokeDragDrop");
    HWND w;
    if (!rv) { t_skip("missing export: RevokeDragDrop"); return; }
    w = mkwnd();
    if (!w) { t_skip("cannot create windows in this session"); return; }
    EQ(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    CoUninitialize();
    EQ(x_apt_type(), 0xffff);
    EQ(rv(w), X_DRAGDROP_E_NOTREGISTERED);
    DestroyWindow(w);
}
TEST(oex_revoke_drag_drop_after_com_was_uninitialized_is_notregistered) { x_on_thread(after_uninit_thread, NULL); }
