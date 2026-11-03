/* Thread lifetime, concurrency and stress behaviour of apartments. */
#include "cia.h"

/* ----- thread exit without CoUninitialize ------------------------------------------------------- */
static void w_init_sta_and_leak(void *p) { *(HRESULT *)p = cInitEx(NULL, CIA_COINIT_STA); /* no CoUninitialize */ }
static void w_init_mta_and_leak(void *p) { *(HRESULT *)p = cInitEx(NULL, CIA_COINIT_MTA); }
static void w_init_mta_leak_nested(void *p) { *(HRESULT *)p = cInitEx(NULL, CIA_COINIT_MTA); cInitEx(NULL, CIA_COINIT_MTA); cInitEx(NULL, CIA_COINIT_MTA); }

TEST(cia_thr_mta_released_when_thread_exits_without_uninit)
{
    HRESULT hr = E_FAIL;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetApartmentType));
    CHECK(cia_run(w_init_mta_and_leak, &hr));
    EXPECT_HR(hr, S_OK);
    CIA_CHECK_NOTINIT();             /* thread-exit cleanup dropped the process MTA reference held by that thread */
}

TEST(cia_thr_mta_nested_leak_released_on_exit)
{
    HRESULT hr = E_FAIL;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetApartmentType));
    CHECK(cia_run(w_init_mta_leak_nested, &hr));
    EXPECT_HR(hr, S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_thr_sta_exit_without_uninit_frees_main_sta_slot)
{
    HRESULT hr = E_FAIL; struct cia_apt_res r;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    CHECK(cia_run(w_init_sta_and_leak, &hr));
    EXPECT_HR(hr, S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);   /* the leaked STA is gone, so this one is the main STA */
    cUninit();
    (void)r;
}

TEST(cia_thr_mta_leaked_by_thread_does_not_hold_other_threads)
{
    HRESULT hr = E_FAIL; struct cia_apt_res r;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetApartmentType));
    CHECK(cia_run(w_init_mta_and_leak, &hr));
    memset(&r, 0, sizeof r);
    r.hr = cia_apt_hr();
    EXPECT_HR(r.hr, CIA_NOTINIT);
}

/* ----- sequential thread churn -------------------------------------------------------------------- */
static void w_churn(void *p)
{
    int *bad = (int *)p;
    DWORD fl = (DWORD)(*bad & 1) ? CIA_COINIT_STA : CIA_COINIT_MTA;
    *bad = 0;
    if (cInitEx(NULL, fl) != S_OK) *bad = 1;
    if (cInitEx(NULL, fl) != S_FALSE) *bad = 2;
    cUninit(); cUninit();
    if (cia_apt_hr() != CIA_NOTINIT) *bad = 3;
}
TEST(cia_thr_churn_60_threads)
{
    int i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 60; i++) {
        int v = i;
        if (!cia_run(w_churn, &v)) break;
        if (v != 0) { CHECKF(0, "thread %d failed step %d", i, v); break; }
    }
    CIA_CHECK_NOTINIT();
}

/* ----- concurrent MTA threads ------------------------------------------------------------------------ */
#define NT 8
struct conc { HANDLE go, ready[NT], th[NT]; HRESULT init_hr[NT]; int t[NT], q[NT]; HRESULT apt_hr[NT]; volatile LONG up; HANDLE release; };
struct conc_arg { struct conc *c; int idx; };

static DWORD WINAPI conc_proc(void *p)
{
    struct conc_arg *a = (struct conc_arg *)p; struct conc *c = a->c; int i = a->idx;
    WaitForSingleObject(c->go, 10000);
    c->init_hr[i] = cInitEx(NULL, CIA_COINIT_MTA);
    c->apt_hr[i] = cia_apt(&c->t[i], &c->q[i]);
    SetEvent(c->ready[i]);
    WaitForSingleObject(c->release, 10000);
    cUninit();
    return 0;
}

TEST(cia_thr_concurrent_mta_init_8_threads)
{
    static struct conc c; struct conc_arg args[NT]; int i; HANDLE all[NT];
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    memset(&c, 0, sizeof c);
    c.go = CreateEventA(NULL, TRUE, FALSE, NULL);
    c.release = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (i = 0; i < NT; i++) {
        c.ready[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
        args[i].c = &c; args[i].idx = i;
        c.th[i] = CreateThread(NULL, 0, conc_proc, &args[i], 0, NULL);
        CHECK(c.th[i] != NULL);
    }
    SetEvent(c.go);
    for (i = 0; i < NT; i++) CHECK(WaitForSingleObject(c.ready[i], 10000) == WAIT_OBJECT_0);
    for (i = 0; i < NT; i++) {
        CHECKF(c.init_hr[i] == S_OK, "thread %d init 0x%08lx", i, (unsigned long)c.init_hr[i]);   /* each thread has its own count */
        CHECKF(c.apt_hr[i] == S_OK && c.t[i] == CIA_APT_MTA && c.q[i] == CIA_Q_NONE, "thread %d apt %d/%d", i, c.t[i], c.q[i]);
    }
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);     /* this thread did nothing, but sees the MTA */
    SetEvent(c.release);
    for (i = 0; i < NT; i++) all[i] = c.th[i];
    CHECK(WaitForMultipleObjects(NT, all, TRUE, 10000) == WAIT_OBJECT_0);
    CIA_CHECK_NOTINIT();
    for (i = 0; i < NT; i++) { CloseHandle(c.th[i]); CloseHandle(c.ready[i]); }
    CloseHandle(c.go); CloseHandle(c.release);
}

static DWORD WINAPI inc_proc(void *p)
{
    struct { CO_MTA_USAGE_COOKIE c[20]; HRESULT hr[20]; } *s = p; int i;
    for (i = 0; i < 20; i++) s->hr[i] = U(CoIncrementMTAUsage)(&s->c[i]);
    return 0;
}
TEST(cia_thr_concurrent_mta_usage_increments)
{
    static struct { CO_MTA_USAGE_COOKIE c[20]; HRESULT hr[20]; } s[4]; HANDLE th[4]; int i, j;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    memset(s, 0, sizeof s);
    for (i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, inc_proc, &s[i], 0, NULL);
    CHECK(WaitForMultipleObjects(4, th, TRUE, 10000) == WAIT_OBJECT_0);
    for (i = 0; i < 4; i++) CloseHandle(th[i]);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    for (i = 0; i < 4; i++) for (j = 0; j < 20; j++) { EXPECT_HR(s[i].hr[j], S_OK); CHECK(s[i].c[j] != NULL); }
    for (i = 0; i < 4; i++) for (j = 0; j < 20; j++) {
        if (i == 3 && j == 19) CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);   /* 79 left, still alive */
        EXPECT_HR(U(CoDecrementMTAUsage)(s[i].c[j]), S_OK);
    }
    CIA_CHECK_NOTINIT();
}

/* ----- the same tables, on a worker thread ------------------------------------------------------------ */
static const DWORD flag_sta[] = { 2, 6, 10, 14 };
static const DWORD flag_mta[] = { 0, 4, 8, 12 };
static const DWORD flag_bad[] = { 1, 3, 5, 0x10, 0x20, 0x100, 0x80000000, 0xFFFFFFFF };

static void w_flag_tables(void *p)
{
    int *bad = (int *)p; size_t i;
    for (i = 0; i < 4; i++) {
        if (cInitEx(NULL, flag_sta[i]) != S_OK) { *bad = 10 + (int)i; return; }
        { int t, q; if (cia_apt(&t, &q) != S_OK || (t != CIA_APT_MAINSTA && t != CIA_APT_STA)) { *bad = 20 + (int)i; return; } }
        cUninit();
        if (cInitEx(NULL, flag_mta[i]) != S_OK) { *bad = 30 + (int)i; return; }
        { int t, q; if (cia_apt(&t, &q) != S_OK || t != CIA_APT_MTA) { *bad = 40 + (int)i; return; } }
        cUninit();
    }
    for (i = 0; i < sizeof flag_bad / sizeof *flag_bad; i++) {
        HRESULT hr = cInitEx(NULL, flag_bad[i]);
        if (hr != CIA_E_INVALIDARG) { *bad = 50 + (int)i; cia_drain(); return; }
    }
}
TEST(cia_thr_flag_tables_on_worker_thread)
{
    int bad = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(cia_run(w_flag_tables, &bad));
    CHECKF(bad == 0, "step %d", bad);
}

/* flag table across both models, repeated while another thread holds the MTA */
TEST(cia_thr_flag_tables_while_mta_held_elsewhere)
{
    cia_w a; HRESULT hr = 0; int bad = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    CHECK(cia_w_call(&a, cia_cmd_init_mta, &hr)); EXPECT_HR(hr, S_OK);
    CHECK(cia_run(w_flag_tables, &bad));
    CHECKF(bad == 0, "step %d", bad);
    CHECK(cia_w_call(&a, cia_cmd_uninit, NULL));
    cia_w_stop(&a);
}

/* implicit MTA from another thread's explicit init must not make this thread's STA fail or become S_FALSE */
TEST(cia_thr_sta_init_while_mta_held_elsewhere)
{
    cia_w a; HRESULT hr = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    CHECK(cia_w_call(&a, cia_cmd_init_mta, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    CHECK(cia_w_call(&a, cia_cmd_uninit, NULL));
    CIA_CHECK_NOTINIT();
    cia_w_stop(&a);
}

TEST(cia_thr_mta_init_while_mta_held_elsewhere_is_s_ok)
{
    cia_w a; HRESULT hr = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    CHECK(cia_w_call(&a, cia_cmd_init_mta, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);       /* S_FALSE is about THIS thread's count, not the process */
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_FALSE);
    cUninit(); cUninit();
    CHECK(cia_w_call(&a, cia_cmd_uninit, NULL));
    CIA_CHECK_NOTINIT();
    cia_w_stop(&a);
}

/* ----- MTA via worker, STA via main, both alive; each reports its own model --------------------------- */
TEST(cia_thr_sta_and_mta_threads_report_own_models)
{
    cia_w a; HRESULT hr = 0; struct cia_apt_res r;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CHECK(cia_w_call(&a, cia_cmd_init_mta, &hr)); EXPECT_HR(hr, S_OK);
    CHECK(cia_w_call(&a, cia_cmd_apt, &r));
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_NONE);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    CHECK(cia_w_call(&a, cia_cmd_uninit, NULL));
    cUninit();
    CIA_CHECK_NOTINIT();
    cia_w_stop(&a);
}

/* ----- observed-only behaviour --------------------------------------------------------------------------- */
TEST(cia_thr_note_main_thread_window_class)
{
    HWND h;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    h = FindWindowExA(HWND_MESSAGE, NULL, "OleMainThreadWndClass", NULL);
    t_note("OleMainThreadWndClass message window %s after STA init", h ? "exists" : "not found");
    cUninit();
    h = FindWindowExA(HWND_MESSAGE, NULL, "OleMainThreadWndClass", NULL);
    t_note("OleMainThreadWndClass message window %s after uninit", h ? "still exists" : "gone");
}

TEST(cia_thr_note_forwarder_targets)
{
    HMODULE m = GetModuleHandleA("combase.dll");
    FARPROC p = t_proc_in("ole32.dll", "CoInitializeEx");
    HMODULE owner = NULL;
    REQUIRE(U(CoInitializeEx));
    if (p) GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    CHECK(owner == m && m != NULL);
    t_note("ole32!CoInitializeEx resolves into %s", owner == m ? "combase.dll" : "another module");
}
