#include "svc.h"

/* ---------- QueryServiceConfigW/A ---------- */
static int in_buf(const void *base, DWORD len, const void *p) {
    return p == NULL || ((const BYTE *)p >= (const BYTE *)base && (const BYTE *)p < (const BYTE *)base + len);
}
static void do_cfgW(const char *name) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfigW)(s, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need > sizeof(QUERY_SERVICE_CONFIGW));
    if (need > 0 && need < 65536) {
        BYTE *buf = calloc(1, need + 16);
        QUERY_SERVICE_CONFIGW *c = (void *)buf;
        DWORD n2 = 0;
        CHECK(S(QueryServiceConfigW)(s, c, need, &n2));
        t_note("%s: type=0x%lx start=%lu err=%lu tag=%lu path=%ls disp=%ls acct=%ls need=%lu n2=%lu", name, c->dwServiceType, c->dwStartType, c->dwErrorControl, c->dwTagId, c->lpBinaryPathName, c->lpDisplayName, c->lpServiceStartName, need, n2);
        CHECK(c->lpBinaryPathName && c->lpBinaryPathName[0]);
        CHECK(c->lpDisplayName && c->lpDisplayName[0]);
        CHECK(c->lpServiceStartName && c->lpServiceStartName[0]);
        CHECK(in_buf(buf, need, c->lpBinaryPathName));
        CHECK(in_buf(buf, need, c->lpDisplayName));
        CHECK(in_buf(buf, need, c->lpServiceStartName));
        CHECK(in_buf(buf, need, c->lpLoadOrderGroup));
        CHECK(in_buf(buf, need, c->lpDependencies));
        CHECK(c->lpLoadOrderGroup != NULL);
        CHECK(c->lpDependencies != NULL);
        CHECK((c->dwServiceType & (SERVICE_WIN32_SHARE_PROCESS | SERVICE_WIN32_OWN_PROCESS)) != 0);
        CHECK(c->dwStartType <= SERVICE_DISABLED);
        CHECK(c->dwErrorControl <= SERVICE_ERROR_CRITICAL);
        CHECK(c->dwStartType == SERVICE_AUTO_START || c->dwStartType == SERVICE_DEMAND_START);
        CHECK(n2 <= need);
        /* needed-1 must fail */
        DWORD n3 = 0; SET_ERR(0);
        BOOL r3 = S(QueryServiceConfigW)(s, c, need - 1, &n3);
        EXPECT_FAIL(!r3, ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(n3, need);
        free(buf);
    }
    svc_close(s); svc_close(scm);
}
PER_SERVICE(svccfg_W, do_cfgW)

static void do_cfgA(const char *name) {
    REQUIRE(S(QueryServiceConfigA)); REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD needA = 0, needW = 0;
    SET_ERR(0);
    BOOL r = S(QueryServiceConfigA)(s, NULL, 0, &needA);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    S(QueryServiceConfigW)(s, NULL, 0, &needW);
    CHECK(needA > sizeof(QUERY_SERVICE_CONFIGA));
    CHECK(needA <= needW);
    BYTE *ba = calloc(1, needA + 16), *bw = calloc(1, needW + 16);
    QUERY_SERVICE_CONFIGA *a = (void *)ba; QUERY_SERVICE_CONFIGW *w = (void *)bw;
    DWORD n;
    if (S(QueryServiceConfigA)(s, a, needA, &n) && S(QueryServiceConfigW)(s, w, needW, &n)) {
        char tmp[1024];
        EXPECT_EQ(a->dwServiceType, w->dwServiceType); EXPECT_EQ(a->dwStartType, w->dwStartType);
        EXPECT_EQ(a->dwErrorControl, w->dwErrorControl); EXPECT_EQ(a->dwTagId, w->dwTagId);
        wide_to_narrow(tmp, sizeof tmp, w->lpBinaryPathName); EXPECT_STR(a->lpBinaryPathName, tmp);
        wide_to_narrow(tmp, sizeof tmp, w->lpDisplayName); EXPECT_STR(a->lpDisplayName, tmp);
        wide_to_narrow(tmp, sizeof tmp, w->lpServiceStartName); EXPECT_STR(a->lpServiceStartName, tmp);
        CHECK(in_buf(ba, needA, a->lpBinaryPathName));
    } else CHECKF(0, "query failed err=%lu", GetLastError());
    free(ba); free(bw);
    svc_close(s); svc_close(scm);
}
PER_SERVICE(svccfg_A, do_cfgA)

TEST(svccfg_binary_path_is_svchost_rpcss) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; S(QueryServiceConfigW)(s, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16);
    if (S(QueryServiceConfigW)(s, (void *)buf, need, &need)) {
        QUERY_SERVICE_CONFIGW *c = (void *)buf;
        t_note("RpcSs binary path: %ls; start name: %ls", c->lpBinaryPathName, c->lpServiceStartName);
        
    }
    free(buf); svc_close(s); svc_close(scm);
}
TEST(svccfg_rpcss_start_is_auto) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; S(QueryServiceConfigW)(s, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16);
    if (S(QueryServiceConfigW)(s, (void *)buf, need, &need)) {
        QUERY_SERVICE_CONFIGW *c = (void *)buf;
        EXPECT_EQ(c->dwStartType, SERVICE_AUTO_START);
        EXPECT_EQ(c->dwErrorControl, SERVICE_ERROR_NORMAL);
    }
    free(buf); svc_close(s); svc_close(scm);
}
TEST(svccfg_buffer_sizes_exact_protocol) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; S(QueryServiceConfigW)(s, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 64);
    DWORD sizes[] = {0, 1, 4, sizeof(QUERY_SERVICE_CONFIGW) - 1, sizeof(QUERY_SERVICE_CONFIGW), need / 2};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        DWORD n = 0; SET_ERR(0);
        BOOL r = S(QueryServiceConfigW)(s, (void *)buf, sizes[i], &n);
        CHECKF(!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER && n == need, "cb=%lu r=%d err=%lu n=%lu need=%lu", sizes[i], r, GetLastError(), n, need);
    }
    DWORD n = 0; CHECK(S(QueryServiceConfigW)(s, (void *)buf, need + 64, &n));
    free(buf); svc_close(s); svc_close(scm);
}
TEST(svccfg_null_handle) {
    REQUIRE(S(QueryServiceConfigW));
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfigW)(NULL, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svccfg_null_handle_A) {
    REQUIRE(S(QueryServiceConfigA));
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfigA)(NULL, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svccfg_scm_handle) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfigW)(scm, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svccfg_no_query_config_right) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfigW)(s, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svccfg_closed_handle) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    svc_close(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfigW)(s, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svccfg_service_account_note) {
    REQUIRE(S(QueryServiceConfigW));
    static const char *n[] = {"RpcSs", "RpcEptMapper", "DcomLaunch", "EventLog", "Winmgmt"};
    for (int i = 0; i < 5; i++) {
        SC_HANDLE scm, s = svc_fixture(n[i], SERVICE_QUERY_CONFIG, &scm); if (!s) continue;
        DWORD need = 0; S(QueryServiceConfigW)(s, NULL, 0, &need);
        BYTE *buf = calloc(1, need + 16);
        if (S(QueryServiceConfigW)(s, (void *)buf, need, &need))
            t_note("%s runs as %ls (typically NT AUTHORITY\\NetworkService / LocalSystem / LocalService)", n[i], ((QUERY_SERVICE_CONFIGW *)buf)->lpServiceStartName);
        free(buf); svc_close(s); svc_close(scm);
    }
}

/* ---------- QueryServiceConfig2 ---------- */
static const DWORD valid_levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 12};
static void cfg2_size_query(DWORD level, DWORD min_expected, int hard) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(s, level, NULL, 0, &need);
    DWORD e = GetLastError();
    if (hard) { CHECKF(!r && e == ERROR_INSUFFICIENT_BUFFER, "level %lu r=%d err=%lu", level, r, e); CHECKF(need >= min_expected, "level %lu need=%lu", level, need); }
    else t_note("level %lu NULL query: r=%d err=%lu need=%lu", level, r, e, need);
    if (!r && e == ERROR_INSUFFICIENT_BUFFER && need && need < 65536) {
        BYTE *buf = calloc(1, need + 16); DWORD n2 = 0;
        BOOL r2 = S(QueryServiceConfig2W)(s, level, buf, need, &n2);
        CHECKF(r2, "level %lu retry err=%lu", level, GetLastError());
        DWORD n3 = 0; SET_ERR(0);
        r2 = S(QueryServiceConfig2W)(s, level, buf, need - 1, &n3);
        CHECKF(!r2 && GetLastError() == ERROR_INSUFFICIENT_BUFFER && n3 == need, "level %lu need-1: r=%d err=%lu n=%lu", level, r2, GetLastError(), n3);
        free(buf);
    }
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_size_description)          { cfg2_size_query(SERVICE_CONFIG_DESCRIPTION, sizeof(SERVICE_DESCRIPTIONW), 1); }
TEST(svccfg2_size_failure_actions)      { cfg2_size_query(SERVICE_CONFIG_FAILURE_ACTIONS, sizeof(SERVICE_FAILURE_ACTIONSW), 1); }
TEST(svccfg2_size_delayed_autostart)    { cfg2_size_query(SERVICE_CONFIG_DELAYED_AUTO_START_INFO, 4, 1); }
TEST(svccfg2_size_failure_actions_flag) { cfg2_size_query(SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, 4, 1); }
TEST(svccfg2_size_sid_info)             { cfg2_size_query(SERVICE_CONFIG_SERVICE_SID_INFO, 4, 1); }
TEST(svccfg2_size_required_privileges)  { cfg2_size_query(SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, sizeof(SERVICE_REQUIRED_PRIVILEGES_INFOW), 1); }
TEST(svccfg2_size_preshutdown)          { cfg2_size_query(SERVICE_CONFIG_PRESHUTDOWN_INFO, 4, 1); }
TEST(svccfg2_size_trigger_info)         { cfg2_size_query(SERVICE_CONFIG_TRIGGER_INFO, 0, 0); }
TEST(svccfg2_size_preferred_node)       { cfg2_size_query(SERVICE_CONFIG_PREFERRED_NODE, 0, 0); }
TEST(svccfg2_size_launch_protected)     { cfg2_size_query(SERVICE_CONFIG_LAUNCH_PROTECTED, 0, 0); }

TEST(svccfg2_fixed_sizes_are_four) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    static const DWORD lv[] = {SERVICE_CONFIG_DELAYED_AUTO_START_INFO, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, SERVICE_CONFIG_SERVICE_SID_INFO, SERVICE_CONFIG_PRESHUTDOWN_INFO};
    for (int i = 0; i < 4; i++) {
        DWORD need = 0; SET_ERR(0);
        BOOL r = S(QueryServiceConfig2W)(s, lv[i], NULL, 0, &need);
        CHECKF(!r && need == 4, "level %lu r=%d need=%lu", lv[i], r, need);
    }
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_delayed_autostart_value) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    SERVICE_DELAYED_AUTO_START_INFO d; DWORD need = 0; d.fDelayedAutostart = 0x55;
    CHECK(S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (LPBYTE)&d, sizeof d, &need));
    CHECK(d.fDelayedAutostart == FALSE);   /* RpcSs is plain AUTO_START */
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_preshutdown_value) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    SERVICE_PRESHUTDOWN_INFO d; DWORD need = 0; d.dwPreshutdownTimeout = 0xFFFF;
    BOOL r = S(QueryServiceConfig2W)(s, SERVICE_CONFIG_PRESHUTDOWN_INFO, (LPBYTE)&d, sizeof d, &need);
    CHECK(r);
    if (r) { t_note("RpcSs preshutdown timeout=%lu ms (default 180000)", d.dwPreshutdownTimeout); CHECK(d.dwPreshutdownTimeout != 0xFFFF); }
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_sid_info_value) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    SERVICE_SID_INFO d; DWORD need = 0; d.dwServiceSidType = 0xFF;
    BOOL r = S(QueryServiceConfig2W)(s, SERVICE_CONFIG_SERVICE_SID_INFO, (LPBYTE)&d, sizeof d, &need);
    CHECK(r);
    if (r) { t_note("RpcSs service SID type %lu", d.dwServiceSidType); CHECK(d.dwServiceSidType == 0 || d.dwServiceSidType == 1 || d.dwServiceSidType == 3); }
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_failure_actions_flag_value) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    SERVICE_FAILURE_ACTIONS_FLAG d; DWORD need = 0; d.fFailureActionsOnNonCrashFailures = 0x55;
    BOOL r = S(QueryServiceConfig2W)(s, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, (LPBYTE)&d, sizeof d, &need);
    CHECK(r);
    if (r) CHECK(d.fFailureActionsOnNonCrashFailures == TRUE || d.fFailureActionsOnNonCrashFailures == FALSE);
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_description_content) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16);
    if (S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DESCRIPTION, buf, need, &need)) {
        SERVICE_DESCRIPTIONW *d = (void *)buf;
        t_note("RpcSs description: %ls", d->lpDescription ? d->lpDescription : L"(null)");
        CHECK(in_buf(buf, need, d->lpDescription));
        if (d->lpDescription) CHECK(d->lpDescription[0] != 0);
    } else CHECKF(0, "err=%lu", GetLastError());
    free(buf); svc_close(s); svc_close(scm);
}
TEST(svccfg2_failure_actions_content) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; S(QueryServiceConfig2W)(s, SERVICE_CONFIG_FAILURE_ACTIONS, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16);
    if (S(QueryServiceConfig2W)(s, SERVICE_CONFIG_FAILURE_ACTIONS, buf, need, &need)) {
        SERVICE_FAILURE_ACTIONSW *d = (void *)buf;
        t_note("RpcSs failure actions: reset=%lu actions=%lu", d->dwResetPeriod, d->cActions);
        CHECK(in_buf(buf, need, d->lpsaActions) || d->cActions == 0);
        CHECK(in_buf(buf, need, d->lpRebootMsg)); CHECK(in_buf(buf, need, d->lpCommand));
        for (DWORD i = 0; i < d->cActions && i < 10; i++) CHECK(d->lpsaActions[i].Type <= SC_ACTION_RUN_COMMAND);
    } else CHECKF(0, "err=%lu", GetLastError());
    free(buf); svc_close(s); svc_close(scm);
}
TEST(svccfg2_required_privileges_content) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; S(QueryServiceConfig2W)(s, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16);
    if (S(QueryServiceConfig2W)(s, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, buf, need, &need)) {
        SERVICE_REQUIRED_PRIVILEGES_INFOW *d = (void *)buf;
        CHECK(in_buf(buf, need, d->pmszRequiredPrivileges));
        if (d->pmszRequiredPrivileges) { const WCHAR *p = d->pmszRequiredPrivileges; int n = 0; while (*p && n < 64) { t_note("RpcSs required privilege: %ls", p); p += lstrlenW(p) + 1; n++; } }
    } else CHECKF(0, "err=%lu", GetLastError());
    free(buf); svc_close(s); svc_close(scm);
}
TEST(svccfg2_trigger_info_query) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("EventLog", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(s, SERVICE_CONFIG_TRIGGER_INFO, NULL, 0, &need);
    t_note("EventLog trigger info NULL query: r=%d err=%lu need=%lu", r, GetLastError(), need);
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_all_valid_levels_never_invalid_level) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    for (unsigned i = 0; i < sizeof valid_levels / sizeof valid_levels[0]; i++) {
        DWORD need = 0; SET_ERR(0);
        S(QueryServiceConfig2W)(s, valid_levels[i], NULL, 0, &need);
        CHECKF(GetLastError() != ERROR_INVALID_LEVEL, "level %lu rejected as invalid", valid_levels[i]);
    }
    svc_close(s); svc_close(scm);
}
static void cfg2_bad_level(DWORD level, int hard) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    BYTE buf[256]; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(s, level, buf, sizeof buf, &need);
    DWORD e = GetLastError();
    CHECKF(!r, "level %lu unexpectedly succeeded", level);
    if (hard) CHECKF(e == ERROR_INVALID_LEVEL, "level %lu err=%lu", level, e); else t_note("level %lu err=%lu", level, e);
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_level_0)     { cfg2_bad_level(0, 1); }
TEST(svccfg2_level_10)    { cfg2_bad_level(10, 0); }
TEST(svccfg2_level_11)    { cfg2_bad_level(11, 0); }
TEST(svccfg2_level_13)    { cfg2_bad_level(13, 0); }
TEST(svccfg2_level_100)   { cfg2_bad_level(100, 1); }
TEST(svccfg2_level_ffff)  { cfg2_bad_level(0xFFFFFFFFu, 1); }
TEST(svccfg2_null_handle) {
    REQUIRE(S(QueryServiceConfig2W));
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(NULL, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svccfg2_null_handle_A) {
    REQUIRE(S(QueryServiceConfig2A));
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2A)(NULL, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svccfg2_scm_handle) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(scm, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svccfg2_no_query_config_right) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svccfg2_closed_handle) {
    REQUIRE(S(QueryServiceConfig2W));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    svc_close(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svccfg2_A_W_description_parity) {
    REQUIRE(S(QueryServiceConfig2W)); REQUIRE(S(QueryServiceConfig2A));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD na = 0, nw = 0;
    SET_ERR(0);
    BOOL r = S(QueryServiceConfig2A)(s, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &na);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &nw);
    CHECK(na >= sizeof(SERVICE_DESCRIPTIONA)); CHECK(na <= nw);
    BYTE *ba = calloc(1, na + 16), *bw = calloc(1, nw + 16);
    if (S(QueryServiceConfig2A)(s, SERVICE_CONFIG_DESCRIPTION, ba, na, &na) && S(QueryServiceConfig2W)(s, SERVICE_CONFIG_DESCRIPTION, bw, nw, &nw)) {
        SERVICE_DESCRIPTIONA *a = (void *)ba; SERVICE_DESCRIPTIONW *w = (void *)bw;
        CHECK((a->lpDescription == NULL) == (w->lpDescription == NULL));
        if (a->lpDescription && w->lpDescription) { char tmp[4096]; wide_to_narrow(tmp, sizeof tmp, w->lpDescription); EXPECT_STR(a->lpDescription, tmp); }
    }
    free(ba); free(bw); svc_close(s); svc_close(scm);
}
TEST(svccfg2_A_bad_level) {
    REQUIRE(S(QueryServiceConfig2A));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    BYTE buf[256]; DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceConfig2A)(s, 100, buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(s); svc_close(scm);
}

/* ---------- QueryServiceObjectSecurity ---------- */
static void security_ok(DWORD secinfo, DWORD open_access) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm, s = svc_fixture("RpcSs", open_access, &scm); REQUIRE(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceObjectSecurity)(s, secinfo, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(SECURITY_DESCRIPTOR_RELATIVE));
    if (need && need < 65536) {
        BYTE *buf = calloc(1, need + 16); DWORD n2 = 0;
        CHECK(S(QueryServiceObjectSecurity)(s, secinfo, (PSECURITY_DESCRIPTOR)buf, need, &n2));
        CHECK(IsValidSecurityDescriptor((PSECURITY_DESCRIPTOR)buf));
        SECURITY_DESCRIPTOR_CONTROL ctl; DWORD rev;
        if (GetSecurityDescriptorControl((PSECURITY_DESCRIPTOR)buf, &ctl, &rev)) {
            CHECK(ctl & SE_SELF_RELATIVE); EXPECT_EQ(rev, 1);
            if (secinfo & DACL_SECURITY_INFORMATION) CHECK(ctl & SE_DACL_PRESENT);
            else CHECK(!(ctl & SE_DACL_PRESENT));
        }
        BOOL present = 0, def = 0; PACL acl = NULL; PSID owner = NULL;
        GetSecurityDescriptorOwner((PSECURITY_DESCRIPTOR)buf, &owner, &def);
        if (secinfo & OWNER_SECURITY_INFORMATION) CHECK(owner != NULL); else CHECK(owner == NULL);
        GetSecurityDescriptorGroup((PSECURITY_DESCRIPTOR)buf, &owner, &def);
        if (secinfo & GROUP_SECURITY_INFORMATION) CHECK(owner != NULL); else CHECK(owner == NULL);
        GetSecurityDescriptorDacl((PSECURITY_DESCRIPTOR)buf, &present, &acl, &def);
        if (secinfo & DACL_SECURITY_INFORMATION) { CHECK(present); CHECK(acl != NULL); }
        DWORD n3 = 0; SET_ERR(0);
        BOOL r3 = S(QueryServiceObjectSecurity)(s, secinfo, (PSECURITY_DESCRIPTOR)buf, need - 1, &n3);
        EXPECT_FAIL(!r3, ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(n3, need);
        free(buf);
    }
    svc_close(s); svc_close(scm);
}
TEST(svcsec_dacl)       { security_ok(DACL_SECURITY_INFORMATION, READ_CONTROL); }
TEST(svcsec_owner)      { security_ok(OWNER_SECURITY_INFORMATION, READ_CONTROL); }
TEST(svcsec_group)      { security_ok(GROUP_SECURITY_INFORMATION, READ_CONTROL); }
TEST(svcsec_all_three)  { security_ok(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, READ_CONTROL); }
TEST(svcsec_dacl_with_query_status) { security_ok(DACL_SECURITY_INFORMATION, READ_CONTROL | SERVICE_QUERY_STATUS); }
TEST(svcsec_no_read_control) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    DWORD need = 0; BYTE buf[2048]; SET_ERR(0);
    BOOL r = S(QueryServiceObjectSecurity)(s, DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcsec_null_handle) {
    REQUIRE(S(QueryServiceObjectSecurity));
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceObjectSecurity)(NULL, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcsec_closed_handle) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm, s = svc_fixture("RpcSs", READ_CONTROL, &scm); REQUIRE(s);
    svc_close(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceObjectSecurity)(s, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcsec_sacl_without_privilege) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm, s = svc_fixture("RpcSs", READ_CONTROL, &scm); REQUIRE(s);
    DWORD need = 0; BYTE buf[2048]; SET_ERR(0);
    BOOL r = S(QueryServiceObjectSecurity)(s, SACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need);
    t_note("SACL query with READ_CONTROL only: r=%d err=%lu", r, r ? 0 : GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(svcsec_scm_handle) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, READ_CONTROL); 
    if (!scm) { t_skip("cannot open scm with READ_CONTROL"); return; }
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceObjectSecurity)(scm, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    t_note("QueryServiceObjectSecurity on SCM handle: r=%d err=%lu need=%lu", r, GetLastError(), need);
    CHECK(!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
    svc_close(scm);
}
TEST(svcsec_dacl_grants_system_full) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm, s = svc_fixture("RpcSs", READ_CONTROL, &scm); REQUIRE(s);
    BYTE buf[4096]; DWORD need = 0;
    if (S(QueryServiceObjectSecurity)(s, DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need)) {
        BOOL present, def; PACL acl;
        GetSecurityDescriptorDacl((PSECURITY_DESCRIPTOR)buf, &present, &acl, &def);
        CHECK(present && acl);
        if (acl) { CHECK(acl->AceCount >= 1); t_note("RpcSs DACL has %u ACEs", acl->AceCount); }
    }
    svc_close(s); svc_close(scm);
}
