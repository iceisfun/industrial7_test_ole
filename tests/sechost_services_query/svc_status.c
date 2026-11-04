#include "svc.h"

static const char *running_core[] = {"RpcSs", "RpcEptMapper", "DcomLaunch", "EventLog"};

/* ---------- QueryServiceStatus ---------- */
static void do_qss_basic(const char *name) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_QUERY_STATUS, &scm);
    REQUIRE(s);
    SERVICE_STATUS st; memset(&st, 0xCC, sizeof st);
    SET_ERR(0);
    CHECK(S(QueryServiceStatus)(s, &st));
    CHECKF((st.dwServiceType & (SERVICE_WIN32_SHARE_PROCESS | SERVICE_WIN32_OWN_PROCESS)) != 0, "type=0x%lx", st.dwServiceType);
    t_note("%s type=0x%lx state=%lu controls=0x%lx exit=%lu", name, st.dwServiceType, st.dwCurrentState, st.dwControlsAccepted, st.dwWin32ExitCode);
    CHECK(st.dwCurrentState >= SERVICE_STOPPED && st.dwCurrentState <= SERVICE_PAUSED);
    CHECK(st.dwCheckPoint == 0 || st.dwCurrentState != SERVICE_RUNNING);
    svc_close(s); svc_close(scm);
}
PER_SERVICE(svcstat_basic, do_qss_basic)

static void do_qss_running(const char *name) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_QUERY_STATUS, &scm);
    REQUIRE(s);
    SERVICE_STATUS st;
    CHECK(S(QueryServiceStatus)(s, &st));
    EXPECT_EQ(st.dwCurrentState, SERVICE_RUNNING);
    EXPECT_EQ(st.dwWin32ExitCode, 0);
    EXPECT_EQ(st.dwServiceSpecificExitCode, 0);
    EXPECT_EQ(st.dwCheckPoint, 0);
    if (st.dwWaitHint) t_note("%s running with dwWaitHint=%lu", name, st.dwWaitHint);
    svc_close(s); svc_close(scm);
}
TEST(svcstat_running_RpcSs)        { do_qss_running("RpcSs"); }
TEST(svcstat_running_RpcEptMapper) { do_qss_running("RpcEptMapper"); }
TEST(svcstat_running_DcomLaunch)   { do_qss_running("DcomLaunch"); }
TEST(svcstat_running_EventLog)     { do_qss_running("EventLog"); }

TEST(svcstat_controls_accepted_known_bits) {
    REQUIRE(S(QueryServiceStatus));
    for (int i = 0; i < 4; i++) {
        SC_HANDLE scm, s = svc_fixture(running_core[i], SERVICE_QUERY_STATUS, &scm);
        if (!s) { CHECKF(0, "open %s", running_core[i]); continue; }
        SERVICE_STATUS st;
        CHECK(S(QueryServiceStatus)(s, &st));
        t_note("%s controls accepted 0x%lx", running_core[i], st.dwControlsAccepted);
        /* documented bit set: STOP..SESSIONCHANGE etc; nothing above 0x7FFF is defined */
        CHECKF((st.dwControlsAccepted & ~0x7FFFu) == 0, "%s accepted=0x%lx", running_core[i], st.dwControlsAccepted);
        svc_close(s); svc_close(scm);
    }
}
TEST(svcstat_null_handle) {
    REQUIRE(S(QueryServiceStatus));
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(QueryServiceStatus)(NULL, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcstat_scm_handle) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(QueryServiceStatus)(scm, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcstat_no_query_status_right) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(QueryServiceStatus)(s, &st);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcstat_interrogate_right_alone_insufficient) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_INTERROGATE, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(QueryServiceStatus)(s, &st);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcstat_closed_handle) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    svc_close(s);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(QueryServiceStatus)(s, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcstat_repeatable) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    for (int i = 0; i < 100; i++) { SERVICE_STATUS st; if (!S(QueryServiceStatus)(s, &st) || st.dwCurrentState != SERVICE_RUNNING) { CHECKF(0, "iteration %d", i); break; } }
    svc_close(s); svc_close(scm);
}

/* ---------- QueryServiceStatusEx ---------- */
static void do_qsx(const char *name) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; memset(&p, 0xCC, sizeof p);
    SET_ERR(0);
    CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need));
    EXPECT_EQ(sizeof(SERVICE_STATUS_PROCESS), 36);
    if (need != sizeof p) t_note("pcbBytesNeeded on success = %lu", need);
    EXPECT_EQ(p.dwCurrentState, SERVICE_RUNNING);
    CHECKF(p.dwServiceType & (SERVICE_WIN32_SHARE_PROCESS | SERVICE_WIN32_OWN_PROCESS), "type=0x%lx", p.dwServiceType);
    CHECK(p.dwProcessId != 0);
    t_note("%s pid=%lu flags=0x%lx type=0x%lx controls=0x%lx", name, p.dwProcessId, p.dwServiceFlags, p.dwServiceType, p.dwControlsAccepted);
    CHECK((p.dwServiceFlags & ~SERVICE_RUNS_IN_SYSTEM_PROCESS) == 0);
    /* same data as QueryServiceStatus */
    SERVICE_STATUS st;
    if (S(QueryServiceStatus)(s, &st)) { EXPECT_EQ(st.dwServiceType, p.dwServiceType); EXPECT_EQ(st.dwCurrentState, p.dwCurrentState); EXPECT_EQ(st.dwControlsAccepted, p.dwControlsAccepted); }
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_full_RpcSs)        { do_qsx("RpcSs"); }
TEST(svcstatex_full_RpcEptMapper) { do_qsx("RpcEptMapper"); }
TEST(svcstatex_full_DcomLaunch)   { do_qsx("DcomLaunch"); }
TEST(svcstatex_full_EventLog)     { do_qsx("EventLog"); }

TEST(svcstatex_pid_is_live_process) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need;
    CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.dwProcessId);
    if (!h) t_note("cannot open service process %lu: err=%lu", p.dwProcessId, GetLastError());
    else {
        char path[MAX_PATH]; DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameA(h, 0, path, &n)) t_note("RpcSs host process: %s", path);
        CloseHandle(h);
    }
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_null_buffer_size_query) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, 36);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_buffer_sizes) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    BYTE buf[128];
    for (DWORD cb = 0; cb < 36; cb += (cb < 4 ? 1 : 8)) {
        DWORD need = 0; SET_ERR(0);
        BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, buf, cb, &need);
        CHECKF(!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need == 36, "cb=%lu r=%d err=%lu need=%lu", cb, r, GetLastError(), need);
    }
    { DWORD need = 0; SET_ERR(0); BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, buf, 35, &need); EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(need, 36); }
    { DWORD need = 0; BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, buf, 36, &need); CHECK(r); t_note("pcbBytesNeeded on exact-size success = %lu", need); }
    { DWORD need = 0; BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, buf, 128, &need); CHECK(r); t_note("pcbBytesNeeded on larger buffer = %lu", need); }
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_large_buffer_tail_untouched) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    BYTE buf[64]; memset(buf, 0xA5, sizeof buf); DWORD need;
    CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, buf, sizeof buf, &need));
    int clean = 1;
    for (int i = 36; i < 64; i++) if (buf[i] != 0xA5) clean = 0;
    if (!clean) t_note("bytes past SERVICE_STATUS_PROCESS were modified");
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_invalid_level_1) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, (SC_STATUS_TYPE)1, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_invalid_level_large) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, (SC_STATUS_TYPE)0x1000, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_invalid_level_negative) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, (SC_STATUS_TYPE)0xFFFFFFFFu, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_invalid_level_with_null_buffer) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, (SC_STATUS_TYPE)1, NULL, 0, &need);
    CHECK(!r);
    t_note("level 1 + NULL buffer: err=%lu need=%lu", GetLastError(), need);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_null_handle) {
    REQUIRE(S(QueryServiceStatusEx));
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(NULL, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcstatex_scm_handle) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(scm, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcstatex_closed_handle) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    svc_close(s);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcstatex_no_query_status_right) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_dwservicetype_share_process_rpcss) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need;
    CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need));
    t_note("RpcSs dwServiceType=0x%lx", p.dwServiceType);
    CHECK((p.dwServiceType & ~(SERVICE_WIN32_SHARE_PROCESS | SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS | 0xC0)) == 0);
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_pids_differ_from_self) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("EventLog", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS p; DWORD need;
    CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need));
    CHECK(p.dwProcessId != GetCurrentProcessId());
    svc_close(s); svc_close(scm);
}
TEST(svcstatex_stopped_service_has_zero_pid) {
    /* find any stopped win32 service and verify pid==0 */
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD need = 0, cnt = 0, resume = 0;
    S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_INACTIVE, NULL, 0, &need, &cnt, &resume, NULL);
    if (need == 0) { t_skip("no inactive services"); svc_close(scm); return; }
    BYTE *buf = malloc(need + 4096);
    resume = 0;
    if (S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_INACTIVE, buf, need + 4096, &need, &cnt, &resume, NULL) && cnt) {
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
        for (DWORD i = 0; i < cnt && i < 20; i++) {
            CHECKF(e[i].ServiceStatusProcess.dwProcessId == 0, "%ls pid=%lu", e[i].lpServiceName, e[i].ServiceStatusProcess.dwProcessId);
            CHECKF(e[i].ServiceStatusProcess.dwCurrentState == SERVICE_STOPPED, "%ls state=%lu", e[i].lpServiceName, e[i].ServiceStatusProcess.dwCurrentState);
        }
    }
    free(buf); svc_close(scm);
}

/* ---------- ControlService ---------- */
TEST(svcctl_null_handle) {
    REQUIRE(S(ControlService));
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(ControlService)(NULL, SERVICE_CONTROL_INTERROGATE, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcctl_scm_handle) {
    REQUIRE(S(ControlService));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(ControlService)(scm, SERVICE_CONTROL_INTERROGATE, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcctl_closed_handle) {
    REQUIRE(S(ControlService));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    svc_close(s);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(ControlService)(s, SERVICE_CONTROL_INTERROGATE, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
/* Controls on a QUERY_STATUS-only handle: access check precedes delivery, so nothing reaches the service. */
static void ctl_denied(DWORD code) {
    REQUIRE(S(ControlService));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(ControlService)(s, code, &st);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcctl_stop_without_stop_right)        { ctl_denied(SERVICE_CONTROL_STOP); }
TEST(svcctl_pause_without_right)            { ctl_denied(SERVICE_CONTROL_PAUSE); }
TEST(svcctl_continue_without_right)         { ctl_denied(SERVICE_CONTROL_CONTINUE); }
TEST(svcctl_interrogate_without_right)      { ctl_denied(SERVICE_CONTROL_INTERROGATE); }
static void ctl_denied_note(DWORD code) {
    REQUIRE(S(ControlService));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS st; SET_ERR(0);
    BOOL r = S(ControlService)(s, code, &st);
    CHECK(!r);
    t_note("ControlService(code %lu) on QUERY_STATUS handle err=%lu", code, GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(svcctl_paramchange_without_right)      { ctl_denied_note(SERVICE_CONTROL_PARAMCHANGE); }
TEST(svcctl_netbindadd_without_right)       { ctl_denied_note(SERVICE_CONTROL_NETBINDADD); }
TEST(svcctl_netbindremove_without_right)    { ctl_denied_note(SERVICE_CONTROL_NETBINDREMOVE); }
TEST(svcctl_netbindenable_without_right)    { ctl_denied_note(SERVICE_CONTROL_NETBINDENABLE); }
TEST(svcctl_netbinddisable_without_right)   { ctl_denied_note(SERVICE_CONTROL_NETBINDDISABLE); }
TEST(svcctl_shutdown_code_not_allowed)      { ctl_denied_note(SERVICE_CONTROL_SHUTDOWN); }
TEST(svcctl_user_defined_128_without_right) { ctl_denied_note(128); }
TEST(svcctl_user_defined_255_without_right) { ctl_denied_note(255); }
TEST(svcctl_unassigned_code_11)             { ctl_denied_note(11); }
TEST(svcctl_unassigned_code_256)            { ctl_denied_note(256); }
TEST(svcctl_code_zero)                      { ctl_denied_note(0); }
TEST(svcctl_interrogate_core_services) {
    REQUIRE(S(ControlService));
    for (int i = 0; i < 4; i++) {
        SC_HANDLE scm, s = svc_fixture(running_core[i], SERVICE_INTERROGATE | SERVICE_QUERY_STATUS, &scm);
        if (!s) { t_note("%s: INTERROGATE right denied err=%lu", running_core[i], GetLastError()); continue; }
        SERVICE_STATUS st; memset(&st, 0, sizeof st); SET_ERR(0);
        BOOL r = S(ControlService)(s, SERVICE_CONTROL_INTERROGATE, &st);
        t_note("%s interrogate: %d err=%lu state=%lu", running_core[i], r, r ? 0 : GetLastError(), st.dwCurrentState);
        if (r) EXPECT_EQ(st.dwCurrentState, SERVICE_RUNNING);
        else { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_SERVICE_CONTROL || e == ERROR_SERVICE_CANNOT_ACCEPT_CTRL || e == ERROR_ACCESS_DENIED || e == ERROR_SERVICE_REQUEST_TIMEOUT, "err=%lu", e); }
        svc_close(s); svc_close(scm);
    }
}
TEST(svcctl_exW_null_handle) {
    REQUIRE(S(ControlServiceExW)); WINE_UNIMPL();
    SET_ERR(0);
    /* Win10 validates the info level before the handle: level 0 gives ERROR_INVALID_LEVEL (124). */
    BOOL r = S(ControlServiceExW)(NULL, SERVICE_CONTROL_INTERROGATE, 0, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    SERVICE_CONTROL_STATUS_REASON_PARAMSW p; memset(&p, 0, sizeof p); SET_ERR(0);
    r = S(ControlServiceExW)(NULL, SERVICE_CONTROL_INTERROGATE, SERVICE_CONTROL_STATUS_REASON_INFO, &p);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcctl_exA_null_handle) {
    REQUIRE(S(ControlServiceExA)); WINE_UNIMPL();
    SET_ERR(0);
    /* Win10 validates the info level before the handle: level 0 gives ERROR_INVALID_LEVEL (124). */
    BOOL r = S(ControlServiceExA)(NULL, SERVICE_CONTROL_INTERROGATE, 0, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    SERVICE_CONTROL_STATUS_REASON_PARAMSA p; memset(&p, 0, sizeof p); SET_ERR(0);
    r = S(ControlServiceExA)(NULL, SERVICE_CONTROL_INTERROGATE, SERVICE_CONTROL_STATUS_REASON_INFO, &p);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcctl_exW_stop_without_right) {
    REQUIRE(S(ControlServiceExW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    SERVICE_STATUS_PROCESS dummy; memset(&dummy, 0, sizeof dummy);
    SET_ERR(0);
    BOOL r = S(ControlServiceExW)(s, SERVICE_CONTROL_STOP, 1, &dummy);
    CHECK(!r);
    t_note("ControlServiceExW(STOP, level 1) w/o right err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(svcctl_exW_bad_info_level) {
    REQUIRE(S(ControlServiceExW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS | SERVICE_INTERROGATE, &scm); REQUIRE_OPEN(s);
    BYTE buf[64] = {0};
    SET_ERR(0);
    BOOL r = S(ControlServiceExW)(s, SERVICE_CONTROL_INTERROGATE, 0x77, buf);
    CHECK(!r);
    t_note("ControlServiceExW bad info level err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(svcctl_exW_level0_interrogate) {
    REQUIRE(S(ControlServiceExW)); WINE_UNIMPL();
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS | SERVICE_INTERROGATE, &scm);
    if (!s) { t_skip("INTERROGATE denied"); return; }
    SERVICE_STATUS st; memset(&st, 0, sizeof st);
    SET_ERR(0);
    BOOL r = S(ControlServiceExW)(s, SERVICE_CONTROL_INTERROGATE, 0, &st);
    t_note("ControlServiceExW(INTERROGATE, level 0): %d err=%lu state=%lu", r, r ? 0 : GetLastError(), st.dwCurrentState);
    svc_close(s); svc_close(scm);
}
