#include "svc.h"

/* ---------- NotifyServiceStatusChange ---------- */
static volatile LONG g_cb_count;
static volatile DWORD g_cb_status, g_cb_triggered, g_cb_state;
static void *volatile g_cb_buf;
static VOID CALLBACK notify_cb(PVOID p) {
    SERVICE_NOTIFYW *n = p;
    g_cb_buf = p;
    g_cb_status = n->dwNotificationStatus;
    g_cb_triggered = n->dwNotificationTriggered;
    g_cb_state = n->ServiceStatus.dwCurrentState;
    InterlockedIncrement(&g_cb_count);
}
static SERVICE_NOTIFYW g_nw;     /* must outlive the pending request */
static SERVICE_NOTIFYA g_na;

static void reset_cb(void) { g_cb_count = 0; g_cb_status = 0xDEAD; g_cb_triggered = 0xDEAD; g_cb_state = 0; g_cb_buf = NULL; }
static int wait_cb(DWORD ms) {
    DWORD t0 = GetTickCount();
    while (g_cb_count == 0 && GetTickCount() - t0 < ms) SleepEx(20, TRUE);
    return g_cb_count;
}

TEST(svcnotify_null_handle) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(NULL, SERVICE_NOTIFY_RUNNING, &g_nw);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
}
TEST(svcnotify_null_handle_A) {
    REQUIRE(S(NotifyServiceStatusChangeA)); WINE_UNIMPL();
    memset(&g_na, 0, sizeof g_na); g_na.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_na.pfnNotifyCallback = (PFN_SC_NOTIFY_CALLBACK)notify_cb;
    DWORD r = S(NotifyServiceStatusChangeA)(NULL, SERVICE_NOTIFY_RUNNING, &g_na);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
}
TEST(svcnotify_closed_handle) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    svc_close(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcnotify_no_query_status_right) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    EXPECT_EQ(r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcnotify_mask_zero) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, 0, &g_nw);
    t_note("mask 0 -> %lu", r);
    SleepEx(100, TRUE);
    svc_close(s); svc_close(scm);
}
TEST(svcnotify_mask_invalid_bits) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, 0x80000000u, &g_nw);
    t_note("mask 0x80000000 -> %lu", r);
    SleepEx(100, TRUE);
    svc_close(s); svc_close(scm);
}
TEST(svcnotify_version_0) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = 0; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    t_note("dwVersion 0 -> %lu", r);
    SleepEx(100, TRUE);
    svc_close(s); svc_close(scm);
}
TEST(svcnotify_version_3) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = 3; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    t_note("dwVersion 3 -> %lu", r);
    SleepEx(100, TRUE);
    svc_close(s); svc_close(scm);
}
TEST(svcnotify_running_service_immediate_callback) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == ERROR_SUCCESS) {
        int n = wait_cb(5000);
        CHECK(n == 1);
        if (n) {
            CHECK(g_cb_buf == &g_nw);
            EXPECT_EQ(g_cb_status, ERROR_SUCCESS);
            EXPECT_EQ(g_cb_triggered, SERVICE_NOTIFY_RUNNING);
            EXPECT_EQ(g_cb_state, SERVICE_RUNNING);
            CHECK(g_nw.ServiceStatus.dwProcessId != 0);
            CHECK(g_nw.ServiceStatus.dwServiceType & (SERVICE_WIN32_SHARE_PROCESS | SERVICE_WIN32_OWN_PROCESS));
        }
    }
    svc_close(s); svc_close(scm);
    SleepEx(20, TRUE);
}
TEST(svcnotify_callback_runs_on_calling_thread_apc) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    if (r == ERROR_SUCCESS) {
        /* A non-alertable wait never delivers the APC. */
        Sleep(300);
        t_note("callbacks delivered before an alertable wait: %ld", (long)g_cb_count);
        CHECK(g_cb_count == 0);
        CHECK(wait_cb(5000) == 1);
    }
    svc_close(s); svc_close(scm);
    SleepEx(20, TRUE);
}
TEST(svcnotify_running_service_stopped_mask_pending) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_STOPPED, &g_nw);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == ERROR_SUCCESS) {
        SleepEx(300, TRUE);
        EXPECT_EQ(g_cb_count, 0);   /* RpcSs never stops */
    }
    svc_close(s);
    SleepEx(200, TRUE);
    t_note("callbacks after closing handle with pending request: %ld (status=%lu)", (long)g_cb_count, (unsigned long)g_cb_status);
    svc_close(scm);
}
TEST(svcnotify_second_request_while_pending) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r1 = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_STOPPED, &g_nw);
    static SERVICE_NOTIFYW n2;
    memset(&n2, 0, sizeof n2); n2.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; n2.pfnNotifyCallback = notify_cb;
    DWORD r2 = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_STOPPED, &n2);
    t_note("first=%lu second (pending) = %lu", r1, r2);
    EXPECT_EQ(r1, ERROR_SUCCESS);
    svc_close(s);
    SleepEx(200, TRUE);
    svc_close(scm);
}
TEST(svcnotify_version_1_accepted_or_rejected) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE_1; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    t_note("version 1 -> %lu", r);
    if (r == ERROR_SUCCESS) wait_cb(2000);
    svc_close(s); svc_close(scm); SleepEx(20, TRUE);
}
TEST(svcnotify_all_state_mask_on_running) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_STOPPED | SERVICE_NOTIFY_START_PENDING | SERVICE_NOTIFY_STOP_PENDING |
        SERVICE_NOTIFY_RUNNING | SERVICE_NOTIFY_CONTINUE_PENDING | SERVICE_NOTIFY_PAUSE_PENDING | SERVICE_NOTIFY_PAUSED, &g_nw);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == ERROR_SUCCESS && wait_cb(5000)) { EXPECT_EQ(g_cb_triggered, SERVICE_NOTIFY_RUNNING); EXPECT_EQ(g_cb_state, SERVICE_RUNNING); }
    svc_close(s); svc_close(scm); SleepEx(20, TRUE);
}
TEST(svcnotify_A_running_service) {
    REQUIRE(S(NotifyServiceStatusChangeA)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_na, 0, sizeof g_na); g_na.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_na.pfnNotifyCallback = (PFN_SC_NOTIFY_CALLBACK)notify_cb;
    DWORD r = S(NotifyServiceStatusChangeA)(s, SERVICE_NOTIFY_RUNNING, &g_na);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == ERROR_SUCCESS) { CHECK(wait_cb(5000) == 1); if (g_cb_count) { EXPECT_EQ(g_cb_triggered, SERVICE_NOTIFY_RUNNING); EXPECT_EQ(g_cb_status, ERROR_SUCCESS); } }
    svc_close(s); svc_close(scm); SleepEx(20, TRUE);
}
TEST(svcnotify_alias_matches_W) {
    REQUIRE(S(NotifyServiceStatusChange)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = ((DWORD (WINAPI *)(SC_HANDLE, DWORD, PVOID))svc_p("NotifyServiceStatusChange"))(s, SERVICE_NOTIFY_RUNNING, &g_nw);
    t_note("NotifyServiceStatusChange (unsuffixed) -> %lu", r);
    if (r == ERROR_SUCCESS) wait_cb(2000);
    svc_close(s); svc_close(scm); SleepEx(20, TRUE);
}
TEST(svcnotify_scm_handle_created_deleted) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm = scm_open(SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT); REQUIRE(scm);
    reset_cb();
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(scm, SERVICE_NOTIFY_CREATED | SERVICE_NOTIFY_DELETED, &g_nw);
    t_note("SCM-handle created/deleted notification -> %lu", r);
    SleepEx(200, TRUE);
    svc_close(scm);
    SleepEx(100, TRUE);
}
TEST(svcnotify_scm_handle_running_mask_invalid) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm = scm_open(SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT); REQUIRE(scm);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(scm, SERVICE_NOTIFY_RUNNING, &g_nw);
    t_note("SCM handle with RUNNING mask -> %lu", r);
    SleepEx(100, TRUE);
    svc_close(scm);
}
TEST(svcnotify_service_handle_created_mask_invalid) {
    REQUIRE(S(NotifyServiceStatusChangeW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    memset(&g_nw, 0, sizeof g_nw); g_nw.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; g_nw.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(s, SERVICE_NOTIFY_CREATED, &g_nw);
    t_note("service handle with CREATED mask -> %lu", r);
    SleepEx(100, TRUE);
    svc_close(s); svc_close(scm); SleepEx(20, TRUE);
}

/* ---------- Subscribe/Unsubscribe, WaitServiceState, QueryServiceDynamicInformation ---------- */
typedef VOID (CALLBACK *sc_cb_t)(DWORD, PVOID);
UDECL(DWORD, SubscribeServiceChangeNotifications, (SC_HANDLE, DWORD, sc_cb_t, PVOID, PVOID *));
UDECL(VOID,  UnsubscribeServiceChangeNotifications, (PVOID));
UDECL(DWORD, WaitServiceState, (SC_HANDLE, DWORD, DWORD, HANDLE));
UDECL(BOOL,  QueryServiceDynamicInformation, (SERVICE_STATUS_HANDLE, DWORD, PVOID *));

static volatile LONG g_sub_hits;
static VOID CALLBACK sub_cb(DWORD n, PVOID ctx) { InterlockedIncrement(&g_sub_hits); }

TEST(svcsub_null_handle) {
    REQUIRE(UN(SubscribeServiceChangeNotifications)); WINE_UNIMPL();
    PVOID sub = NULL;
    DWORD r = UN(SubscribeServiceChangeNotifications)(NULL, 0, sub_cb, NULL, &sub);
    CHECK(r != ERROR_SUCCESS);
    t_note("Subscribe(NULL handle) -> %lu", r);
    if (r == ERROR_SUCCESS && sub) UN(UnsubscribeServiceChangeNotifications)(sub);
}
TEST(svcsub_bad_event_type) {
    REQUIRE(UN(SubscribeServiceChangeNotifications)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    PVOID sub = NULL;
    DWORD r = UN(SubscribeServiceChangeNotifications)(s, 77, sub_cb, NULL, &sub);
    CHECK(r != ERROR_SUCCESS);
    t_note("Subscribe(event type 77) -> %lu", r);
    if (r == ERROR_SUCCESS && sub) UN(UnsubscribeServiceChangeNotifications)(sub);
    svc_close(s); svc_close(scm);
}
TEST(svcsub_status_change_roundtrip) {
    REQUIRE(UN(SubscribeServiceChangeNotifications)); REQUIRE(UN(UnsubscribeServiceChangeNotifications)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    PVOID sub = NULL; g_sub_hits = 0;
    DWORD r = UN(SubscribeServiceChangeNotifications)(s, 2 /* SC_EVENT_STATUS_CHANGE */, sub_cb, NULL, &sub);
    t_note("Subscribe(status change, RpcSs) -> %lu sub=%p", r, sub);
    if (r == ERROR_SUCCESS) {
        CHECK(sub != NULL);
        Sleep(200);
        UN(UnsubscribeServiceChangeNotifications)(sub);
        t_note("callbacks seen: %ld", (long)g_sub_hits);
    }
    svc_close(s); svc_close(scm);
}
TEST(svcsub_database_change_roundtrip) {
    REQUIRE(UN(SubscribeServiceChangeNotifications)); REQUIRE(UN(UnsubscribeServiceChangeNotifications)); WINE_UNIMPL();
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    PVOID sub = NULL; g_sub_hits = 0;
    DWORD r = UN(SubscribeServiceChangeNotifications)(scm, 0 /* SC_EVENT_DATABASE_CHANGE */, sub_cb, NULL, &sub);
    t_note("Subscribe(database change, SCM handle) -> %lu sub=%p", r, sub);
    if (r == ERROR_SUCCESS) { CHECK(sub != NULL); Sleep(200); UN(UnsubscribeServiceChangeNotifications)(sub); }
    svc_close(scm);
}
TEST(svcsub_repeat_subscribe_unsubscribe) {
    REQUIRE(UN(SubscribeServiceChangeNotifications)); REQUIRE(UN(UnsubscribeServiceChangeNotifications)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    int ok = 0;
    for (int i = 0; i < 10; i++) {
        PVOID sub = NULL;
        if (UN(SubscribeServiceChangeNotifications)(s, 2, sub_cb, NULL, &sub) == ERROR_SUCCESS && sub) { ok++; UN(UnsubscribeServiceChangeNotifications)(sub); }
    }
    t_note("%d/10 subscribe/unsubscribe cycles succeeded", ok);
    svc_close(s); svc_close(scm);
}
TEST(svcwait_null_handle) {
    REQUIRE(UN(WaitServiceState)); WINE_UNIMPL();
    DWORD r = UN(WaitServiceState)(NULL, SERVICE_NOTIFY_RUNNING, 100, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("WaitServiceState(NULL) -> %lu", r);
}
TEST(svcwait_running_already) {
    REQUIRE(UN(WaitServiceState)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    DWORD t0 = GetTickCount();
    DWORD r = UN(WaitServiceState)(s, SERVICE_NOTIFY_RUNNING, 5000, NULL);
    DWORD dt = GetTickCount() - t0;
    t_note("WaitServiceState(RUNNING) -> %lu after %lu ms", r, dt);
    CHECK(dt < 8000);
    svc_close(s); svc_close(scm);
}
TEST(svcwait_stopped_times_out) {
    REQUIRE(UN(WaitServiceState)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    DWORD t0 = GetTickCount();
    DWORD r = UN(WaitServiceState)(s, SERVICE_NOTIFY_STOPPED, 300, NULL);
    DWORD dt = GetTickCount() - t0;
    CHECK(r != ERROR_SUCCESS);
    CHECK(dt < 8000);
    t_note("WaitServiceState(STOPPED, 300ms) -> %lu after %lu ms", r, dt);
    svc_close(s); svc_close(scm);
}
TEST(svcwait_cancel_event) {
    REQUIRE(UN(WaitServiceState)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    HANDLE ev = CreateEventW(NULL, TRUE, TRUE, NULL);
    DWORD t0 = GetTickCount();
    DWORD r = UN(WaitServiceState)(s, SERVICE_NOTIFY_STOPPED, 4000, ev);
    DWORD dt = GetTickCount() - t0;
    if (dt >= 3500) t_note("cancel event ignored (dt=%lu)", dt);
    t_note("WaitServiceState with signalled cancel event -> %lu after %lu ms", r, dt);
    CloseHandle(ev);
    svc_close(s); svc_close(scm);
}
TEST(svcdyn_null_handle) {
    REQUIRE(UN(QueryServiceDynamicInformation)); WINE_UNIMPL();
    PVOID info = NULL; SET_ERR(0);
    BOOL r = UN(QueryServiceDynamicInformation)(NULL, 1, &info);
    CHECK(!r);
    t_note("QueryServiceDynamicInformation(NULL) err=%lu", GetLastError());
}
TEST(svcdyn_not_a_service_process) {
    /* This process never called RegisterServiceCtrlHandler, so there is no status handle to resolve. */
    REQUIRE(UN(QueryServiceDynamicInformation)); WINE_UNIMPL();
    PVOID info = NULL; SET_ERR(0);
    BOOL r = UN(QueryServiceDynamicInformation)((SERVICE_STATUS_HANDLE)(ULONG_PTR)0, 0, &info);
    CHECK(!r);
}
