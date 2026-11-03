/* CoWaitForMultipleHandles / CoWaitForMultipleObjects (flags, STA message dispatch) and the ISynchronize event objects. */
#include "cx.h"

#define WM_DDE_FIRST_ 0x03E0
#define IDX_SENT 0xdeadbeefu

/* ---- window used to observe what a COM wait dispatches ---- */
static UINT g_msgs[128]; static int g_nmsgs;
static LONG g_apc_hits;
static void msgs_reset(void) { g_nmsgs = 0; }
static LRESULT CALLBACK cw_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (g_nmsgs < 128) g_msgs[g_nmsgs++] = m;
    if (m == WM_DDE_FIRST_) return 6;
    return UU(DefWindowProcW)(h, m, w, l);
}
static WCHAR g_cls[64];
static HWND make_window(void)
{
    WNDCLASSEXW wc;
    if (!g_cls[0]) wsprintfW(g_cls, L"olet_cowait_%lu", GetCurrentProcessId());
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.lpfnWndProc = cw_proc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = g_cls;
    if (!UU(RegisterClassExW)(&wc)) t_note("RegisterClassExW: %lu cls=%ls", GetLastError(), g_cls);                   /* repeated registration fails harmlessly */
    {
        HWND w = UU(CreateWindowExW)(0, g_cls, L"cowait", WS_OVERLAPPED, 0, 0, 100, 100, HWND_MESSAGE, NULL, wc.hInstance, NULL);   /* message-only: no desktop needed */
        if (!w) t_note("CreateWindowExW failed: %lu", GetLastError());
        return w;
    }
}
static int queued(HWND h, UINT m, BOOL remove)
{
    MSG msg;
    return UU(PeekMessageW)(&msg, h, m, m, remove ? PM_REMOVE : PM_NOREMOVE) ? 1 : 0;
}
static void flush_queue(void)
{
    MSG msg;
    while (UU(PeekMessageW)(&msg, NULL, 0, 0, PM_REMOVE)) {}
}
static int saw(UINT m) { int i; for (i = 0; i < g_nmsgs; i++) if (g_msgs[i] == m) return 1; return 0; }

/* ================= argument validation ================= */
static void w_cw_args(void *p)
{
    HANDLE h[2]; ULONG idx;
    h[0] = cx_event(FALSE, TRUE); h[1] = cx_event(FALSE, TRUE);

    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 0, h, &idx), CX_HR_NOSYNC);          /* no handles: RPC_E_NO_SYNC */
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(COWAIT_ALERTABLE | COWAIT_WAITALL, 0, 0, h, &idx), CX_HR_NOSYNC);
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(COWAIT_DISPATCH_CALLS, 0, 0, h, &idx), CX_HR_NOSYNC);
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, 0, 0, h, &idx), CX_HR_NOSYNC);
    CXEQ(idx, 0);

    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 1, NULL, &idx), E_INVALIDARG);       /* NULL handle array */
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 0, NULL, &idx), E_INVALIDARG);
    CXEQ(idx, 0);
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 0, h, NULL), E_INVALIDARG);          /* NULL index pointer */
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 1, h, NULL), E_INVALIDARG);
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 0, NULL, NULL), E_INVALIDARG);
    CXEQ(WaitForSingleObject(h[0], 0), WAIT_OBJECT_0);                              /* nothing was consumed */
    CloseHandle(h[0]); CloseHandle(h[1]);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_cowait_argument_errors_sta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_args, NULL); }
TEST(ctxthr_cowait_argument_errors_mta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_args, NULL); }

static void w_cw_flags(void *p)
{
    static const DWORD bad[] = { 0x20, 0x40, 0x100, 0x8000, 0x80000000u, 0xffffffe0u, 0xffffffffu, 0x21, 0x3f };
    HANDLE ev = cx_event(FALSE, TRUE);
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        ULONG idx = IDX_SENT;
        HRESULT hr = U(CoWaitForMultipleHandles)(bad[i], 0, 1, &ev, &idx);
        CHECKF(hr == E_INVALIDARG && idx == 0, "flags 0x%lx: hr 0x%08lx idx %lu", (unsigned long)bad[i], (unsigned long)hr, (unsigned long)idx);
        idx = IDX_SENT;
        hr = U(CoWaitForMultipleHandles)(bad[i], 0, 0, &ev, &idx);        /* flags are validated before the handle count */
        CHECKF(hr == E_INVALIDARG && idx == 0, "flags 0x%lx, 0 handles: hr 0x%08lx idx %lu", (unsigned long)bad[i], (unsigned long)hr, (unsigned long)idx);
    }
    CXEQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);                    /* never waited */
    CloseHandle(ev);
}
TEST(ctxthr_cowait_invalid_flag_bits) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_flags, NULL); cx_run(CXM_MTA, w_cw_flags, NULL); }

static void w_cw_window_msgs_flag(void *p)
{
    /* COWAIT_DISPATCH_WINDOW_MESSAGES (0x10) is only valid together with COWAIT_DISPATCH_CALLS (0x08) */
    static const DWORD notsup[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };
    HANDLE ev = cx_event(FALSE, TRUE);
    int i;
    for (i = 0; i < 8; i++) {
        ULONG idx = IDX_SENT;
        HRESULT hr = U(CoWaitForMultipleHandles)(notsup[i], 0, 1, &ev, &idx);
        CHECKF(hr == CX_HR_NOTSUPP, "flags 0x%lx: hr 0x%08lx", (unsigned long)notsup[i], (unsigned long)hr);
    }
    CXEQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);
    CloseHandle(ev);
}
TEST(ctxthr_cowait_dispatch_window_messages_requires_dispatch_calls) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_window_msgs_flag, NULL); cx_run(CXM_MTA, w_cw_window_msgs_flag, NULL); }

static void w_cwo_args(void *p)
{
    HANDLE h[2]; ULONG idx;
    static const DWORD badf[] = { 4, 8, 0x10, 0x80, 0xfffffffc, 0xffffffff };
    int i;
    h[0] = cx_event(FALSE, TRUE); h[1] = cx_event(FALSE, TRUE);
    for (i = 0; i < 6; i++) {
        idx = IDX_SENT;
        HRESULT hr = U(CoWaitForMultipleObjects)(badf[i], 0, 1, h, &idx);
        CHECKF(hr == E_INVALIDARG && idx == 0, "flags 0x%lx: hr 0x%08lx idx %lu", (unsigned long)badf[i], (unsigned long)hr, (unsigned long)idx);
    }
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleObjects)(0, 0, 57, h, &idx), E_INVALIDARG);          /* CWMO_MAX_HANDLES is 56 */
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleObjects)(0, 0, 0xffffffffu, h, &idx), E_INVALIDARG);
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleObjects)(0, 0, 0, h, &idx), CX_HR_NOSYNC);
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleObjects)(CWMO_DISPATCH_CALLS, 0, 0, h, &idx), CX_HR_NOSYNC);
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleObjects)(CWMO_DISPATCH_WINDOW_MESSAGES, 0, 1, h, &idx), CX_HR_NOTSUPP);   /* maps to the invalid combination */
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleObjects)(CWMO_DISPATCH_CALLS | CWMO_DISPATCH_WINDOW_MESSAGES, 0, 1, h, &idx), S_OK);
    CXEQ(idx, 0);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(ctxthr_cowaitobjects_argument_errors_sta) { CX_WINE_SKIP(); REQUIRE(U(CoWaitForMultipleObjects)); cx_run(CXM_STA, w_cwo_args, NULL); }
TEST(ctxthr_cowaitobjects_argument_errors_mta) { CX_WINE_SKIP(); REQUIRE(U(CoWaitForMultipleObjects)); cx_run(CXM_MTA, w_cwo_args, NULL); }

static void w_cwo_max(void *p)
{
    HANDLE h[56]; int i; ULONG idx = IDX_SENT; HRESULT hr;
    for (i = 0; i < 56; i++) h[i] = cx_event(FALSE, i == 55);
    hr = U(CoWaitForMultipleObjects)(0, 100, 56, h, &idx);       /* exactly the documented maximum */
    CXEQ_HR(hr, S_OK);
    CXEQ(idx, 55);
    idx = IDX_SENT;
    hr = U(CoWaitForMultipleObjects)(0, 0, 56, h, &idx);          /* consumed above */
    CXEQ_HR(hr, CX_HR_CALLPENDING);
    for (i = 0; i < 56; i++) CloseHandle(h[i]);
}
TEST(ctxthr_cowaitobjects_maximum_handle_count_sta) { CX_WINE_SKIP(); REQUIRE(U(CoWaitForMultipleObjects)); cx_run(CXM_STA, w_cwo_max, NULL); }
TEST(ctxthr_cowaitobjects_maximum_handle_count_mta) { CX_WINE_SKIP(); REQUIRE(U(CoWaitForMultipleObjects)); cx_run(CXM_MTA, w_cwo_max, NULL); }

/* ================= wait semantics (no window needed) ================= */
static void w_cw_sem(void *p)
{
    HANDLE h[2]; ULONG idx; HRESULT hr; DWORD t0, el;
    h[0] = CreateSemaphoreW(NULL, 1, 1, NULL); h[1] = CreateSemaphoreW(NULL, 1, 1, NULL);

    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 1, h, &idx), S_OK);                  /* acquires */
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 2, h, &idx), S_OK);                  /* only h[1] is still available */
    CXEQ(idx, 1);
    idx = IDX_SENT;
    t0 = GetTickCount();
    hr = U(CoWaitForMultipleHandles)(0, 80, 2, h, &idx);
    el = GetTickCount() - t0;
    CXEQ_HR(hr, CX_HR_CALLPENDING);                                                  /* timeout */
    CXEQ(idx, 0);
    CHECK(el >= 60);
    CHECK(el < 4000);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 0, 2, h, &idx), CX_HR_CALLPENDING);       /* zero timeout polls */
    CXEQ(idx, 0);

    ReleaseSemaphore(h[0], 1, NULL); ReleaseSemaphore(h[1], 1, NULL);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 0, 2, h, &idx), S_OK);                    /* both ready: lowest index wins */
    CXEQ(idx, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 0, 2, h, &idx), S_OK);
    CXEQ(idx, 1);

    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(ctxthr_cowait_semaphores_index_and_timeout_sta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_sem, NULL); }
TEST(ctxthr_cowait_semaphores_index_and_timeout_mta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_sem, NULL); }
TEST(ctxthr_cowait_semaphores_index_and_timeout_uninitialized) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_NONE, w_cw_sem, NULL); }

static void waitall_body(int sta)
{
    HANDLE h[2]; ULONG idx; HWND w = NULL;
    h[0] = cx_event(FALSE, TRUE); h[1] = cx_event(FALSE, FALSE);
    if (sta) {
        w = make_window();
        if (!w) { CHECK(!"CreateWindow"); return; }
    }
    idx = IDX_SENT;
    if (sta) UU(PostMessageW)(w, WM_USER, 0, 0);      /* in an STA the wait is also an input wait: provide input */
    CXEQ_HR(U(CoWaitForMultipleHandles)(COWAIT_WAITALL, 30, 2, h, &idx), CX_HR_CALLPENDING);
    CXEQ(WaitForSingleObject(h[0], 0), WAIT_OBJECT_0);                               /* wait-all consumes nothing until all are ready */
    SetEvent(h[1]);
    idx = IDX_SENT;
    if (sta) UU(PostMessageW)(w, WM_USER, 0, 0);
    CXEQ_HR(U(CoWaitForMultipleHandles)(COWAIT_WAITALL, 30, 2, h, &idx), S_OK);
    CXEQ(idx, 0);
    CXEQ(WaitForSingleObject(h[0], 0), WAIT_TIMEOUT);                                /* both auto-reset events consumed */
    CXEQ(WaitForSingleObject(h[1], 0), WAIT_TIMEOUT);
    if (sta) {   /* without input the documented MWMO_WAITALL behaviour of the STA wait may keep waiting */
        HRESULT hr;
        SetEvent(h[0]); SetEvent(h[1]);
        flush_queue();
        idx = IDX_SENT;
        hr = U(CoWaitForMultipleHandles)(COWAIT_WAITALL, 30, 2, h, &idx);
        t_note("STA COWAIT_WAITALL, both signaled, empty queue: hr 0x%08lx idx %lu", (unsigned long)hr, (unsigned long)idx);
        flush_queue();
        UU(DestroyWindow)(w);
    }
    CloseHandle(h[0]); CloseHandle(h[1]);
}
static void w_cw_waitall_sta(void *p) { waitall_body(1); }
static void w_cw_waitall_mta(void *p) { waitall_body(0); }
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_cowait_waitall_sta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_waitall_sta, NULL); }
TEST(ctxthr_cowait_waitall_mta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_waitall_mta, NULL); }

typedef struct Sig { HANDLE ev; DWORD delay; } Sig;
static DWORD WINAPI sig_thread(LPVOID p) { Sig *s = p; Sleep(s->delay); SetEvent(s->ev); return 0; }
static void w_cw_cross(void *p)
{
    Sig s; HANDLE th, h; ULONG idx = IDX_SENT; DWORD t0, el; HRESULT hr;
    h = cx_event(TRUE, FALSE);
    s.ev = h; s.delay = 100;
    th = CreateThread(NULL, 0, sig_thread, &s, 0, NULL);
    t0 = GetTickCount();
    hr = U(CoWaitForMultipleHandles)(0, 5000, 1, &h, &idx);
    el = GetTickCount() - t0;
    CXEQ_HR(hr, S_OK); CXEQ(idx, 0);
    CHECK(el < 4000);
    WaitForSingleObject(th, 5000); CloseHandle(th);
    /* thread handle as the awaited object */
    th = CreateThread(NULL, 0, sig_thread, &s, 0, NULL);
    idx = IDX_SENT;
    hr = U(CoWaitForMultipleHandles)(0, 5000, 1, &th, &idx);
    CXEQ_HR(hr, S_OK); CXEQ(idx, 0);
    CloseHandle(th); CloseHandle(h);
}
TEST(ctxthr_cowait_signaled_by_other_thread_sta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_cross, NULL); }
TEST(ctxthr_cowait_signaled_by_other_thread_mta) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_cross, NULL); }

static void w_cw_infinite(void *p)
{
    HANDLE h = cx_event(TRUE, TRUE); ULONG idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, INFINITE, 1, &h, &idx), S_OK);          /* already signaled: no blocking */
    CXEQ(idx, 0);
    CloseHandle(h);
}
TEST(ctxthr_cowait_infinite_timeout_signaled_handle) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_infinite, NULL); cx_run(CXM_MTA, w_cw_infinite, NULL); }

static void w_cw_badhandle(void *p)
{
    HANDLE h = cx_event(TRUE, TRUE); ULONG idx = IDX_SENT; HRESULT hr;
    CloseHandle(h);
    hr = U(CoWaitForMultipleHandles)(0, 10, 1, &h, &idx);
    t_note("CoWaitForMultipleHandles on a closed handle: hr 0x%08lx idx %lu", (unsigned long)hr, (unsigned long)idx);
    CHECK(CX_FAILED(hr));
}
TEST(ctxthr_cowait_closed_handle_fails_observed) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_badhandle, NULL); }

static void CALLBACK apc_proc(ULONG_PTR p) { InterlockedIncrement(&g_apc_hits); }
static void w_cw_alertable(void *p)
{
    HANDLE h = cx_event(FALSE, FALSE); ULONG idx = IDX_SENT; HRESULT hr;
    int *sta = p;
    g_apc_hits = 0;
    QueueUserAPC(apc_proc, GetCurrentThread(), 0);
    hr = U(CoWaitForMultipleHandles)(COWAIT_ALERTABLE, 100, 1, &h, &idx);
    CXEQ(g_apc_hits, 1);                                  /* alertable wait runs the queued APC */
    if (*sta) {
        CXEQ_HR(hr, S_OK);                                /* APC completion is reported as success with a special index */
        CXEQ(idx, WAIT_IO_COMPLETION);
    } else t_note("MTA alertable wait interrupted by APC: hr 0x%08lx idx %lu", (unsigned long)hr, (unsigned long)idx);
    /* not alertable: the APC stays queued, the wait just times out */
    g_apc_hits = 0;
    QueueUserAPC(apc_proc, GetCurrentThread(), 0);
    idx = IDX_SENT;
    hr = U(CoWaitForMultipleHandles)(0, 50, 1, &h, &idx);
    CXEQ_HR(hr, CX_HR_CALLPENDING);
    CXEQ(g_apc_hits, 0);
    SleepEx(0, TRUE);                                     /* deliver it now */
    CXEQ(g_apc_hits, 1);
    CloseHandle(h);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_cowait_alertable_runs_apc_sta) { int sta = 1; REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_alertable, &sta); }
TEST(ctxthr_cowait_alertable_runs_apc_mta) { int sta = 0; REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_alertable, &sta); }

/* ================= STA message dispatch ================= */
static DWORD WINAPI post_dde_thread(LPVOID p) { UU(PostMessageW)((HWND)p, WM_DDE_FIRST_, 0, 0); return 0; }
static DWORD WINAPI send_dde_thread(LPVOID p) { UU(SendMessageW)((HWND)p, WM_DDE_FIRST_, 0, 0); return 0; }
static DWORD WINAPI send_and_post_user_thread(LPVOID p)
{
    Sleep(30);
    UU(SendMessageW)((HWND)p, WM_USER, 0, 0);
    UU(PostMessageW)((HWND)p, WM_USER, 0, 0);
    return 0;
}
static void run_thread_wait(LPTHREAD_START_ROUTINE fn, HWND w)
{
    HANDLE t = CreateThread(NULL, 0, fn, w, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }   /* the thread only needs the wait below to be active */
}

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
static void w_cw_sta_pump(void *p)
{
    HWND w = make_window();
    HANDLE h[2]; ULONG idx; HRESULT hr; HANDLE th;
    h[0] = CreateSemaphoreW(NULL, 1, 1, NULL); h[1] = CreateSemaphoreW(NULL, 1, 1, NULL);
    if (!w) { CHECK(!"CreateWindow"); return; }

    /* the wait is satisfied at once: messages are not looked at */
    UU(PostMessageW)(w, WM_DDE_FIRST_, 0, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 1, h, &idx), S_OK);
    CXEQ(idx, 0);
    CXEQ(queued(w, WM_DDE_FIRST_, TRUE), 1);

    UU(PostMessageW)(w, WM_DDE_FIRST_, 0, 0);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 2, h, &idx), S_OK);
    CXEQ(idx, 1);
    CXEQ(queued(w, WM_DDE_FIRST_, TRUE), 1);

    /* a real wait (both handles now taken): DDE messages are dispatched, other posted messages are not */
    msgs_reset();
    UU(PostMessageW)(w, WM_DDE_FIRST_, 0, 0);
    UU(PostMessageW)(w, WM_USER, 0, 0);
    idx = IDX_SENT;
    hr = U(CoWaitForMultipleHandles)(0, 50, 2, h, &idx);
    CXEQ_HR(hr, CX_HR_CALLPENDING);
    CXEQ(idx, 0);
    CXEQ(queued(w, WM_DDE_FIRST_, TRUE), 0);          /* consumed by the wait ... */
    CXEQ(saw(WM_DDE_FIRST_), 1);                      /* ... by dispatching it to the window */
    CXEQ(queued(w, WM_USER, TRUE), 1);                /* ordinary posted messages stay queued */
    CXEQ(saw(WM_USER), 0);

    /* messages posted / sent from another thread while waiting */
    msgs_reset();
    th = CreateThread(NULL, 0, post_dde_thread, w, 0, NULL);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 100, 2, h, &idx), CX_HR_CALLPENDING);
    CXEQ(saw(WM_DDE_FIRST_), 1);
    WaitForSingleObject(th, 5000); CloseHandle(th);

    msgs_reset();
    th = CreateThread(NULL, 0, send_dde_thread, w, 0, NULL);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 100, 2, h, &idx), CX_HR_CALLPENDING);
    CXEQ(saw(WM_DDE_FIRST_), 1);                      /* the sender is released: sent messages are delivered */
    CXEQ(WaitForSingleObject(th, 5000), WAIT_OBJECT_0);
    CloseHandle(th);

    /* order and selection with a mix of posted and sent messages */
    msgs_reset();
    UU(PostMessageW)(w, 0, 0, 0);
    UU(PostMessageW)(w, WM_DDE_FIRST_, 0, 0);
    UU(PostMessageW)(w, WM_USER + 1, 0, 0);
    UU(PostMessageW)(w, WM_DDE_FIRST_ + 1, 0, 0);
    th = CreateThread(NULL, 0, send_and_post_user_thread, w, 0, NULL);
    idx = IDX_SENT;
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 150, 2, h, &idx), CX_HR_CALLPENDING);
    CXEQ(g_nmsgs, 3);
    if (g_nmsgs >= 3) {
        CXEQ(g_msgs[0], WM_DDE_FIRST_);
        CXEQ(g_msgs[1], WM_DDE_FIRST_ + 1);
        CXEQ(g_msgs[2], WM_USER);                       /* the sent one; its posted twin stays queued */
    }
    CXEQ(queued(w, WM_USER, TRUE), 1);
    CXEQ(queued(w, WM_USER + 1, TRUE), 1);
    flush_queue();
    WaitForSingleObject(th, 5000); CloseHandle(th);

    UU(DestroyWindow)(w);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(ctxthr_cowait_sta_dispatches_dde_and_sent_messages) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_sta_pump, NULL); }

static void w_cw_mta_nopump(void *p)
{
    HANDLE ev = cx_event(FALSE, FALSE); ULONG idx = IDX_SENT;
    HWND w = make_window();
    if (!w) { CHECK(!"CreateWindow"); return; }
    msgs_reset();
    UU(PostMessageW)(w, WM_DDE_FIRST_, 0, 0);
    CXEQ_HR(U(CoWaitForMultipleHandles)(0, 50, 1, &ev, &idx), CX_HR_CALLPENDING);
    CXEQ(saw(WM_DDE_FIRST_), 0);                      /* an MTA wait never dispatches window messages */
    CXEQ(queued(w, WM_DDE_FIRST_, TRUE), 1);
    UU(DestroyWindow)(w);
    CloseHandle(ev);
}
TEST(ctxthr_cowait_mta_does_not_dispatch_messages) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_MTA, w_cw_mta_nopump, NULL); }

static void w_cw_dispatch_flags(void *p)
{
    HWND w = make_window();
    HANDLE ev = cx_event(FALSE, FALSE); ULONG idx = IDX_SENT; HRESULT hr;
    if (!w) { CHECK(!"CreateWindow"); return; }
    /* COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES: all window messages are pumped */
    msgs_reset();
    UU(PostMessageW)(w, WM_USER + 5, 0, 0);
    hr = U(CoWaitForMultipleHandles)(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, 100, 1, &ev, &idx);
    CXEQ_HR(hr, CX_HR_CALLPENDING);
    CXEQ(saw(WM_USER + 5), 1);
    CXEQ(queued(w, WM_USER + 5, TRUE), 0);
    /* COWAIT_DISPATCH_CALLS alone: calls, but window messages stay queued (observed) */
    msgs_reset();
    UU(PostMessageW)(w, WM_USER + 6, 0, 0);
    hr = U(CoWaitForMultipleHandles)(COWAIT_DISPATCH_CALLS, 100, 1, &ev, &idx);
    CXEQ_HR(hr, CX_HR_CALLPENDING);
    t_note("COWAIT_DISPATCH_CALLS alone: posted WM_USER+6 was %s", saw(WM_USER + 6) ? "dispatched" : "left queued");
    flush_queue();
    /* a wait satisfied by a signal during the pump */
    {
        Sig s; HANDLE th; s.ev = ev; s.delay = 60;
        th = CreateThread(NULL, 0, sig_thread, &s, 0, NULL);
        UU(PostMessageW)(w, WM_USER + 7, 0, 0);
        idx = IDX_SENT;
        hr = U(CoWaitForMultipleHandles)(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, 5000, 1, &ev, &idx);
        CXEQ_HR(hr, S_OK); CXEQ(idx, 0);
        WaitForSingleObject(th, 5000); CloseHandle(th);
    }
    UU(DestroyWindow)(w);
    CloseHandle(ev);
}
TEST(ctxthr_cowait_sta_dispatch_window_messages_flag) { REQUIRE(U(CoWaitForMultipleHandles)); cx_run(CXM_STA, w_cw_dispatch_flags, NULL); }

static void w_cwo_pump(void *p)
{
    HWND w = make_window();
    HANDLE ev = cx_event(FALSE, FALSE); ULONG idx = IDX_SENT;
    if (!w) { CHECK(!"CreateWindow"); return; }
    msgs_reset();
    UU(PostMessageW)(w, WM_USER + 5, 0, 0);
    CXEQ_HR(U(CoWaitForMultipleObjects)(CWMO_DISPATCH_CALLS | CWMO_DISPATCH_WINDOW_MESSAGES, 100, 1, &ev, &idx), CX_HR_CALLPENDING);
    CXEQ(saw(WM_USER + 5), 1);
    msgs_reset();
    UU(PostMessageW)(w, WM_DDE_FIRST_, 0, 0);
    CXEQ_HR(U(CoWaitForMultipleObjects)(0, 100, 1, &ev, &idx), CX_HR_CALLPENDING);
    CXEQ(saw(WM_DDE_FIRST_), 1);                       /* CWMO_DEFAULT still services DDE like flags 0 */
    flush_queue();
    UU(DestroyWindow)(w);
    CloseHandle(ev);
}
TEST(ctxthr_cowaitobjects_sta_flag_mapping) { CX_WINE_SKIP(); REQUIRE(U(CoWaitForMultipleObjects)); cx_run(CXM_STA, w_cwo_pump, NULL); }

/* ================= ISynchronize: CLSID_ManualResetEvent ================= */
static ISynchronize *create_sync(const GUID *clsid, HRESULT *hr_out)
{
    void *p = NULL;
    HRESULT hr = U(CoCreateInstance)(clsid, NULL, CLSCTX_INPROC_SERVER, &CX_IID_ISynchronize, &p);
    if (hr_out) *hr_out = hr;
    return CX_FAILED(hr) ? NULL : (ISynchronize *)p;
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
static void w_mre_basic(void *p)
{
    HRESULT hr; ISynchronize *s1, *s2; LONG ref;
    s1 = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    CXEQ_HR(hr, S_OK);
    if (!s1) return;
    CXEQ_HR(ISynchronize_Wait(s1, 0, 5), CX_HR_CALLPENDING);       /* starts non-signaled */
    CXEQ_HR(ISynchronize_Reset(s1), S_OK);
    CXEQ_HR(ISynchronize_Signal(s1), S_OK);
    CXEQ_HR(ISynchronize_Wait(s1, 0, 5), S_OK);
    CXEQ_HR(ISynchronize_Wait(s1, 0, 5), S_OK);                    /* manual reset: stays signaled */
    CXEQ_HR(ISynchronize_Wait(s1, 0, INFINITE), S_OK);
    CXEQ_HR(ISynchronize_Signal(s1), S_OK);                        /* idempotent */
    CXEQ_HR(ISynchronize_Reset(s1), S_OK);
    CXEQ_HR(ISynchronize_Wait(s1, 0, 5), CX_HR_CALLPENDING);
    CXEQ_HR(ISynchronize_Reset(s1), S_OK);                         /* idempotent */
    CXEQ_HR(ISynchronize_Wait(s1, 0, 0), CX_HR_CALLPENDING);

    s2 = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    CXEQ_HR(hr, S_OK);
    CXNE(s1, s2);
    if (s2) {
        CXEQ_HR(ISynchronize_Signal(s1), S_OK);
        CXEQ_HR(ISynchronize_Wait(s2, 0, 5), CX_HR_CALLPENDING);   /* instances are independent */
        CXEQ_HR(ISynchronize_Wait(s1, 0, 5), S_OK);
        ref = ISynchronize_AddRef(s1); CXEQ(ref, 2);
        ref = ISynchronize_AddRef(s1); CXEQ(ref, 3);
        ref = ISynchronize_Release(s1); CXEQ(ref, 2);
        ref = ISynchronize_Release(s2); CXEQ(ref, 0);
        ref = ISynchronize_Release(s1); CXEQ(ref, 1);
        ref = ISynchronize_Release(s1); CXEQ(ref, 0);
    } else ISynchronize_Release(s1);
}
TEST(ctxthr_sync_manual_reset_event_semantics_sta) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_STA, w_mre_basic, NULL); }
TEST(ctxthr_sync_manual_reset_event_semantics_mta) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_MTA, w_mre_basic, NULL); }

static void w_mre_uninit(void *p)
{
    void *pv = (void *)0xdead;
    CXEQ_HR(U(CoCreateInstance)(&CX_CLSID_ManualResetEvent, NULL, CLSCTX_INPROC_SERVER, &CX_IID_ISynchronize, &pv), CX_HR_NOTINIT);
    CXEQ(pv, 0);
}
TEST(ctxthr_sync_manual_reset_event_requires_com) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_NONE, w_mre_uninit, NULL); }

static void w_mre_qi(void *p)
{
    HRESULT hr; ISynchronize *s = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    static const struct { const GUID *iid; const char *name; int must; } t[] = {
        { &CX_IID_IUnknown, "IUnknown", 1 }, { &CX_IID_ISynchronize, "ISynchronize", 1 }, { &CX_IID_ISynchronizeHandle, "ISynchronizeHandle", 1 },
        { &CX_IID_ISynchronizeEvent, "ISynchronizeEvent", 0 }, { &CX_IID_IMarshal, "IMarshal", 0 }, { &CX_IID_IDispatch, "IDispatch", 0 } };
    int i;
    CXEQ_HR(hr, S_OK);
    if (!s) return;
    for (i = 0; i < 6; i++) {
        void *pv = (void *)0xdead;
        hr = ISynchronize_QueryInterface(s, t[i].iid, &pv);
        if (t[i].must) { CHECKF(hr == S_OK && pv, "%s: 0x%08lx", t[i].name, (unsigned long)hr); }
        else t_note("ManualResetEvent QueryInterface(%s) = 0x%08lx", t[i].name, (unsigned long)hr);
        if (!CX_FAILED(hr) && pv) IUnknown_Release((IUnknown *)pv); else CXEQ(pv, 0);
    }
    {
        void *pv = (void *)0xdead;
        CXEQ_HR(ISynchronize_QueryInterface(s, &CX_IID_Bogus, &pv), CX_HR_NOINTERFACE);
        CXEQ(pv, 0);
    }
    ISynchronize_Release(s);
}
TEST(ctxthr_sync_manual_reset_event_interfaces) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_STA, w_mre_qi, NULL); cx_run(CXM_MTA, w_mre_qi, NULL); }

static void w_mre_handle(void *p)
{
    HRESULT hr; ISynchronize *s = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    ISynchronizeHandle *sh = NULL; HANDLE h = NULL, h2 = NULL;
    if (!s) { CHECK(!"create"); return; }
    CXEQ_HR(ISynchronize_QueryInterface(s, &CX_IID_ISynchronizeHandle, (void **)&sh), S_OK);
    if (!sh) { ISynchronize_Release(s); return; }
    CXEQ_HR(ISynchronizeHandle_GetHandle(sh, &h), S_OK);
    CHECK(h != NULL && h != INVALID_HANDLE_VALUE);
    CXEQ_HR(ISynchronizeHandle_GetHandle(sh, &h2), S_OK);
    CXEQ(h, h2);                                                 /* one event for the object's lifetime */
    CXEQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CXEQ_HR(ISynchronize_Signal(s), S_OK);
    CXEQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);              /* the handle follows the object's state */
    CXEQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CXEQ_HR(ISynchronize_Reset(s), S_OK);
    CXEQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    SetEvent(h);                                                 /* and the reverse */
    CXEQ_HR(ISynchronize_Wait(s, 0, 0), S_OK);
    ResetEvent(h);
    CXEQ_HR(ISynchronize_Wait(s, 0, 0), CX_HR_CALLPENDING);
    ISynchronizeHandle_Release(sh);
    ISynchronize_Release(s);
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(ctxthr_sync_manual_reset_event_handle_follows_state) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_STA, w_mre_handle, NULL); cx_run(CXM_MTA, w_mre_handle, NULL); }

typedef struct SyncX { ISynchronize *s; HANDLE ready, go; HRESULT hr; DWORD ms; } SyncX;
static DWORD WINAPI sync_waiter(LPVOID p)
{
    SyncX *x = p;
    HRESULT init = cx_init(CXM_MTA);
    SetEvent(x->ready);
    x->hr = ISynchronize_Wait(x->s, 0, x->ms);
    if (!CX_FAILED(init)) cx_uninit();
    return 0;
}
static void w_mre_cross(void *p)
{
    HRESULT hr; ISynchronize *s = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    SyncX x[2]; HANDLE th[2]; int i;
    if (!s) { CHECK(!"create"); return; }
    for (i = 0; i < 2; i++) { x[i].s = s; x[i].ready = cx_event(TRUE, FALSE); x[i].hr = E_UNEXPECTED; x[i].ms = 5000; th[i] = CreateThread(NULL, 0, sync_waiter, &x[i], 0, NULL); }
    for (i = 0; i < 2; i++) WaitForSingleObject(x[i].ready, 5000);
    Sleep(50);
    CXEQ_HR(ISynchronize_Signal(s), S_OK);
    for (i = 0; i < 2; i++) {
        CXEQ(WaitForSingleObject(th[i], 8000), WAIT_OBJECT_0);
        CXEQ_HR(x[i].hr, S_OK);                                  /* a manual-reset event releases every waiter */
        CloseHandle(th[i]); CloseHandle(x[i].ready);
    }
    /* timeout expiry from another thread */
    x[0].ready = cx_event(TRUE, FALSE); x[0].ms = 60; ISynchronize_Reset(s);
    th[0] = CreateThread(NULL, 0, sync_waiter, &x[0], 0, NULL);
    CXEQ(WaitForSingleObject(th[0], 8000), WAIT_OBJECT_0);
    CXEQ_HR(x[0].hr, CX_HR_CALLPENDING);
    CloseHandle(th[0]); CloseHandle(x[0].ready);
    ISynchronize_Release(s);
}
TEST(ctxthr_sync_manual_reset_event_signal_releases_all_waiters_sta) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_STA, w_mre_cross, NULL); }
TEST(ctxthr_sync_manual_reset_event_signal_releases_all_waiters_mta) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_MTA, w_mre_cross, NULL); }

static void w_mre_sta_pump(void *p)
{
    HRESULT hr; ISynchronize *s = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    HWND w = make_window();
    Sig sg; HANDLE th; DWORD t0;
    if (!s || !w) { CHECK(!"setup"); return; }
    sg.ev = NULL;
    /* signal from another thread after 80 ms; a window message is posted first */
    {
        ISynchronizeHandle *sh = NULL; HANDLE h = NULL;
        ISynchronize_QueryInterface(s, &CX_IID_ISynchronizeHandle, (void **)&sh);
        if (sh) { ISynchronizeHandle_GetHandle(sh, &h); ISynchronizeHandle_Release(sh); }
        sg.ev = h; sg.delay = 80;
    }
    msgs_reset();
    UU(PostMessageW)(w, WM_USER + 9, 0, 0);
    th = CreateThread(NULL, 0, sig_thread, &sg, 0, NULL);
    t0 = GetTickCount();
    hr = ISynchronize_Wait(s, COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, 5000);
    CXEQ_HR(hr, S_OK);
    CHECK(GetTickCount() - t0 < 4500);
    CXEQ(saw(WM_USER + 9), 1);                                   /* the STA wait pumped the window */
    WaitForSingleObject(th, 5000); CloseHandle(th);
    ISynchronize_Reset(s);
    hr = ISynchronize_Wait(s, COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, 40);
    CXEQ_HR(hr, CX_HR_CALLPENDING);
    flush_queue();
    UU(DestroyWindow)(w);
    ISynchronize_Release(s);
}
TEST(ctxthr_sync_manual_reset_event_wait_pumps_sta_messages) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_STA, w_mre_sta_pump, NULL); }

static void w_mre_flags(void *p)
{
    HRESULT hr; ISynchronize *s = create_sync(&CX_CLSID_ManualResetEvent, &hr);
    if (!s) { CHECK(!"create"); return; }
    ISynchronize_Signal(s);
    CXEQ_HR(ISynchronize_Wait(s, COWAIT_WAITALL, 0), S_OK);
    CXEQ_HR(ISynchronize_Wait(s, COWAIT_ALERTABLE, 0), S_OK);
    CXEQ_HR(ISynchronize_Wait(s, COWAIT_INPUTAVAILABLE, 0), S_OK);
    ISynchronize_Reset(s);
    CXEQ_HR(ISynchronize_Wait(s, COWAIT_WAITALL, 0), CX_HR_CALLPENDING);
    hr = ISynchronize_Wait(s, 0x20, 0);
    t_note("ISynchronize::Wait with an undefined flag bit (0x20): 0x%08lx", (unsigned long)hr);
    ISynchronize_Release(s);
}
TEST(ctxthr_sync_manual_reset_event_wait_flags) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_MTA, w_mre_flags, NULL); }

/* ---- the other synchronization classes: observed, not asserted ---- */
static void w_std_event(void *p)
{
    HRESULT hr; ISynchronize *s = create_sync(&CX_CLSID_StdEvent, &hr);
    t_note("CoCreateInstance(CLSID_StdEvent, ISynchronize) = 0x%08lx", (unsigned long)hr);
    if (s) {
        HRESULT w1, w2;
        ISynchronize_Signal(s);
        w1 = ISynchronize_Wait(s, 0, 0);
        w2 = ISynchronize_Wait(s, 0, 0);
        t_note("StdEvent: Wait after Signal = 0x%08lx, second Wait = 0x%08lx (%s)", (unsigned long)w1, (unsigned long)w2, w2 == S_OK ? "manual reset" : "auto reset");
        CXEQ_HR(w1, S_OK);
        {
            void *pv = NULL;
            hr = ISynchronize_QueryInterface(s, &CX_IID_ISynchronizeEvent, &pv);
            t_note("StdEvent QueryInterface(ISynchronizeEvent) = 0x%08lx", (unsigned long)hr);
            if (pv) IUnknown_Release((IUnknown *)pv);
        }
        ISynchronize_Release(s);
    }
}
TEST(ctxthr_sync_std_event_observed) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_MTA, w_std_event, NULL); }

static void w_sync_container(void *p)
{
    void *pv = NULL; HRESULT hr;
    hr = U(CoCreateInstance)(&CX_CLSID_SynchronizeContainer, NULL, CLSCTX_INPROC_SERVER, &CX_IID_ISynchronizeContainer, &pv);
    t_note("CoCreateInstance(CLSID_SynchronizeContainer, ISynchronizeContainer) = 0x%08lx", (unsigned long)hr);
    if (pv) IUnknown_Release((IUnknown *)pv); else CXEQ(pv, 0);
}
TEST(ctxthr_sync_container_creation_observed) { REQUIRE(U(CoCreateInstance)); cx_run(CXM_MTA, w_sync_container, NULL); }
