/* SCM / service handle tests (sechost client side of the svcctl RPC interface).
 * Read-only against the real service "RpcSs" (never started, stopped, reconfigured or deleted: every
 * mutating call is made with a handle that lacks the right, so the SCM denies it before acting). */
#include "t.h"
#include <winsvc.h>
#include <sddl.h>

#define SH(n) UIN("sechost.dll", n)

static BOOL is_elevated(void)
{
    /* integrity level, not TokenIsElevated: a restricted-admin Medium token can still report elevated */
    HANDLE tok; BYTE buf[64]; DWORD n; BOOL r = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenIntegrityLevel, buf, sizeof buf, &n)) {
            PSID sid = ((TOKEN_MANDATORY_LABEL *)buf)->Label.Sid;
            r = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1) >= SECURITY_MANDATORY_HIGH_RID;
        }
        CloseHandle(tok);
    }
    return r;
}

static SC_HANDLE open_scm(DWORD access)
{
    return SH(OpenSCManagerW)(NULL, NULL, access);
}

static SC_HANDLE open_rpcss(SC_HANDLE scm, DWORD access)
{
    return SH(OpenServiceW)(scm, L"RpcSs", access);
}

/* Returns 0 and skips when the SCM / RpcSs are not there (emulators). */
static int need_scm_and_rpcss(SC_HANDLE *scm, SC_HANDLE *svc, DWORD scm_access, DWORD svc_access)
{
    *scm = *svc = NULL;
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return 0; }
    if (!SH(OpenSCManagerW) || !SH(OpenServiceW) || !SH(CloseServiceHandle)) { t_skip("missing export"); return 0; }
    *scm = open_scm(scm_access);
    if (!*scm) { t_skip("OpenSCManagerW(0x%lx) failed: %lu", scm_access, GetLastError()); return 0; }
    if (svc_access) {
        *svc = open_rpcss(*scm, svc_access);
        if (!*svc) { DWORD e = GetLastError(); SH(CloseServiceHandle)(*scm); *scm = NULL; t_skip("OpenService RpcSs(0x%lx) failed: %lu", svc_access, e); return 0; }
    }
    return 1;
}

static void close_both(SC_HANDLE scm, SC_HANDLE svc)
{
    if (svc) SH(CloseServiceHandle)(svc);
    if (scm) SH(CloseServiceHandle)(scm);
}

/* ---- OpenSCManager --------------------------------------------------------- */
TEST(scl_openscm_connect_ok)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW)); REQUIRE(SH(CloseServiceHandle));
    SC_HANDLE h = SH(OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    if (h) CHECK(SH(CloseServiceHandle)(h));
}

TEST(scl_openscm_a_connect_ok)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerA)); REQUIRE(SH(CloseServiceHandle));
    SC_HANDLE h = SH(OpenSCManagerA)(NULL, NULL, SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    if (h) CHECK(SH(CloseServiceHandle)(h));
}

TEST(scl_openscm_explicit_active_database)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW)); REQUIRE(SH(OpenSCManagerA));
    SC_HANDLE h = SH(OpenSCManagerW)(NULL, L"ServicesActive", SC_MANAGER_CONNECT);
    CHECK(h != NULL); if (h) SH(CloseServiceHandle)(h);
    h = SH(OpenSCManagerA)(NULL, "ServicesActive", SC_MANAGER_CONNECT);
    CHECK(h != NULL); if (h) SH(CloseServiceHandle)(h);
}

TEST(scl_openscm_bad_database_invalid_name)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW)); REQUIRE(SH(OpenSCManagerA));
    SET_ERR(0);
    SC_HANDLE h = SH(OpenSCManagerW)(NULL, L"olet_NoSuchDatabase", SC_MANAGER_CONNECT);
    EXPECT_FAIL(h == NULL, ERROR_INVALID_NAME);
    if (h) SH(CloseServiceHandle)(h);
    SET_ERR(0);
    h = SH(OpenSCManagerA)(NULL, "olet_NoSuchDatabase", SC_MANAGER_CONNECT);
    EXPECT_FAIL(h == NULL, ERROR_INVALID_NAME);
    if (h) SH(CloseServiceHandle)(h);
}

TEST(scl_openscm_failed_database_does_not_exist)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = SH(OpenSCManagerW)(NULL, L"ServicesFailed", SC_MANAGER_CONNECT);
    EXPECT_FAIL(h == NULL, ERROR_DATABASE_DOES_NOT_EXIST);
    if (h) SH(CloseServiceHandle)(h);
}

TEST(scl_openscm_database_name_case)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = SH(OpenSCManagerW)(NULL, L"servicesactive", SC_MANAGER_CONNECT);
    if (h) { t_note("database name is matched case-insensitively"); SH(CloseServiceHandle)(h); }
    else t_note("lower-case database name rejected, err %lu", GetLastError());
}

TEST(scl_openscm_local_machine_names)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    SC_HANDLE h = SH(OpenSCManagerW)(L"", NULL, SC_MANAGER_CONNECT);
    CHECK(h != NULL); if (h) SH(CloseServiceHandle)(h);   /* empty machine name = local */
    WCHAR name[MAX_COMPUTERNAME_LENGTH + 3] = L"\\\\"; DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(name + 2, &n)) {
        h = SH(OpenSCManagerW)(name, NULL, SC_MANAGER_CONNECT);
        if (h) SH(CloseServiceHandle)(h); else t_note("\\\\computer form failed: %lu", GetLastError());
    }
}

TEST(scl_openscm_generic_and_maximum_allowed)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    DWORD acc[] = {GENERIC_READ, GENERIC_EXECUTE, MAXIMUM_ALLOWED, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE,
                   SC_MANAGER_CONNECT | SC_MANAGER_QUERY_LOCK_STATUS};
    for (int i = 0; i < 5; i++) {
        SC_HANDLE h = SH(OpenSCManagerW)(NULL, NULL, acc[i]);
        DWORD le = GetLastError();
        if (!h && acc[i] == GENERIC_EXECUTE && !is_elevated()) {   /* maps to rights (SC_MANAGER_LOCK) a non-admin lacks */
            t_note("GENERIC_EXECUTE denied for a non-elevated token, err %lu", le);
            continue;
        }
        CHECKF(h != NULL, "access 0x%lx err %lu", acc[i], le);
        if (h) SH(CloseServiceHandle)(h);
    }
}

TEST(scl_openscm_all_access_needs_admin)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = SH(OpenSCManagerW)(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    DWORD le = GetLastError();
    BOOL el = is_elevated();   /* may reset the last error */
    SetLastError(le);
    if (el) {
        t_note("elevated: SC_MANAGER_ALL_ACCESS %s", h ? "granted" : "denied");
        CHECK(h != NULL);
    } else {
        EXPECT_FAIL(h == NULL, ERROR_ACCESS_DENIED);
    }
    if (h) SH(CloseServiceHandle)(h);
}

TEST(scl_openscm_distinct_handles)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    SC_HANDLE a = open_scm(SC_MANAGER_CONNECT), b = open_scm(SC_MANAGER_CONNECT);
    CHECK(a && b);
    CHECK(a != b);
    if (a) SH(CloseServiceHandle)(a);
    if (b) SH(CloseServiceHandle)(b);
}

TEST(scl_openscm_handle_independent_close)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenSCManagerW));
    SC_HANDLE a = open_scm(SC_MANAGER_CONNECT), b = open_scm(SC_MANAGER_CONNECT);
    REQUIRE(a); REQUIRE(b);
    CHECK(SH(CloseServiceHandle)(a));
    SC_HANDLE s = SH(OpenServiceW)(b, L"RpcSs", SERVICE_QUERY_STATUS);
    if (s) { CHECK(SH(CloseServiceHandle)(s)); } else t_note("RpcSs not openable: %lu", GetLastError());
    CHECK(SH(CloseServiceHandle)(b));
}

/* ---- CloseServiceHandle ------------------------------------------------- */
TEST(scl_closeservicehandle_null)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(CloseServiceHandle));
    SET_ERR(0);
    BOOL r = SH(CloseServiceHandle)(NULL);
    CHECK(!r);
    t_note("CloseServiceHandle(NULL) last error %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(scl_closeservicehandle_service_handle)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    CHECK(SH(CloseServiceHandle)(svc));
    CHECK(SH(CloseServiceHandle)(scm));
}

/* ---- OpenService ---------------------------------------------------------- */
TEST(scl_openservice_nonexistent)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    SET_ERR(0);
    SC_HANDLE h = SH(OpenServiceW)(scm, L"olet_NoSuchService_zzz", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(h == NULL, ERROR_SERVICE_DOES_NOT_EXIST);
    SET_ERR(0);
    h = SH(OpenServiceA)(scm, "olet_NoSuchService_zzz", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(h == NULL, ERROR_SERVICE_DOES_NOT_EXIST);
    close_both(scm, NULL);
}

TEST(scl_openservice_rpcss_w_and_a)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    SC_HANDLE w = SH(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    SC_HANDLE a = SH(OpenServiceA)(scm, "RpcSs", SERVICE_QUERY_STATUS);
    CHECK(w != NULL); CHECK(a != NULL);
    CHECK(w != a);
    if (w) SH(CloseServiceHandle)(w);
    if (a) SH(CloseServiceHandle)(a);
    close_both(scm, NULL);
}

TEST(scl_openservice_name_case_insensitive)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    static const WCHAR *names[] = {L"rpcss", L"RPCSS", L"rPcSs"};
    for (int i = 0; i < 3; i++) {
        SC_HANDLE h = SH(OpenServiceW)(scm, names[i], SERVICE_QUERY_STATUS);
        CHECKF(h != NULL, "name %d err %lu", i, GetLastError());
        if (h) SH(CloseServiceHandle)(h);
    }
    close_both(scm, NULL);
}

TEST(scl_openservice_invalid_names)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    static const WCHAR *names[] = {L"", L"Rpc/Ss", L"Rpc\\Ss"};
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        SC_HANDLE h = SH(OpenServiceW)(scm, names[i], SERVICE_QUERY_STATUS);
        CHECKF(h == NULL, "name %d opened", i);
        t_note("OpenServiceW(\"%ls\") err %lu", names[i], GetLastError());
        if (h) SH(CloseServiceHandle)(h);
    }
    close_both(scm, NULL);
}

TEST(scl_openservice_service_handle_as_scm)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    SET_ERR(0);
    SC_HANDLE h = SH(OpenServiceW)(svc, L"RpcSs", SERVICE_QUERY_STATUS);   /* wrong handle type */
    EXPECT_FAIL(h == NULL, ERROR_INVALID_HANDLE);
    if (h) SH(CloseServiceHandle)(h);
    close_both(scm, svc);
}

TEST(scl_openservice_null_scm)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(OpenServiceW));
    SET_ERR(0);
    SC_HANDLE h = SH(OpenServiceW)(NULL, L"RpcSs", SERVICE_QUERY_STATUS);
    CHECK(h == NULL);
    t_note("OpenServiceW(NULL scm) err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (h) SH(CloseServiceHandle)(h);
}

TEST(scl_openservice_scm_needs_connect)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    /* an SCM handle opened with access 0 cannot open services (SC_MANAGER_CONNECT is required) */
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, 0, 0)) return;
    SET_ERR(0);
    SC_HANDLE h = SH(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    if (h) { t_note("OpenService with a zero-access SCM handle succeeded"); SH(CloseServiceHandle)(h); }
    else EXPECT_ERR(ERROR_ACCESS_DENIED);
    close_both(scm, NULL);
}

TEST(scl_openservice_all_access_needs_admin)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    SET_ERR(0);
    SC_HANDLE h = SH(OpenServiceW)(scm, L"RpcSs", SERVICE_ALL_ACCESS);
    if (h) {
        t_note("SERVICE_ALL_ACCESS on RpcSs granted (elevated=%d)", is_elevated());
        CHECK(is_elevated());
        SH(CloseServiceHandle)(h);
    } else {
        EXPECT_ERR(ERROR_ACCESS_DENIED);
    }
    close_both(scm, NULL);
}

TEST(scl_openservice_stop_right_denied_for_users)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    if (is_elevated()) { t_skip("elevated"); close_both(scm, NULL); return; }
    DWORD rights[] = {SERVICE_STOP, SERVICE_START, SERVICE_CHANGE_CONFIG, SERVICE_PAUSE_CONTINUE, DELETE, WRITE_DAC, WRITE_OWNER};
    for (int i = 0; i < 7; i++) {
        SET_ERR(0);
        SC_HANDLE h = SH(OpenServiceW)(scm, L"RpcSs", rights[i]);
        CHECKF(h == NULL, "right 0x%lx granted to a standard user", rights[i]);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        if (h) SH(CloseServiceHandle)(h);
    }
    close_both(scm, NULL);
}

/* ---- handle type confusion ---------------------------------------------- */
TEST(scl_typeconf_scm_handle_to_queryservicestatus)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    SERVICE_STATUS st;
    SET_ERR(0);
    BOOL r = SH(QueryServiceStatus)(scm, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    close_both(scm, NULL);
}

TEST(scl_typeconf_scm_handle_to_queryservicestatusex)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    SERVICE_STATUS_PROCESS sp; DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceStatusEx)(scm, SC_STATUS_PROCESS_INFO, (LPBYTE)&sp, sizeof sp, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    close_both(scm, NULL);
}

TEST(scl_typeconf_scm_handle_to_queryserviceconfig)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    BYTE buf[4096]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceConfigW)(scm, (LPQUERY_SERVICE_CONFIGW)buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    close_both(scm, NULL);
}

TEST(scl_typeconf_scm_handle_to_queryserviceconfig2)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    BYTE buf[4096]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceConfig2W)(scm, SERVICE_CONFIG_DESCRIPTION, buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    close_both(scm, NULL);
}

TEST(scl_typeconf_scm_handle_to_controlservice_interrogate)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    SERVICE_STATUS st;
    SET_ERR(0);
    BOOL r = SH(ControlService)(scm, SERVICE_CONTROL_INTERROGATE, &st);   /* harmless control code */
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    close_both(scm, NULL);
}

TEST(scl_typeconf_scm_handle_to_queryobjectsecurity)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    DWORD need = 0; BYTE b[16];
    SET_ERR(0);
    BOOL r = SH(QueryServiceObjectSecurity)(scm, DACL_SECURITY_INFORMATION, b, 0, &need);
    CHECK(!r);
    t_note("QueryServiceObjectSecurity(scm handle, 0 bytes): err %lu need %lu", GetLastError(), need);
    DWORD e = GetLastError();
    CHECK(e == ERROR_INVALID_HANDLE || e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_ACCESS_DENIED);
    close_both(scm, NULL);
}

TEST(scl_typeconf_service_handle_to_enumservices)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    DWORD need = 0, cnt = 0, resume = 0; BYTE b[64];
    SET_ERR(0);
    BOOL r = SH(EnumServicesStatusExW)(svc, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    close_both(scm, svc);
}

TEST(scl_typeconf_service_handle_to_getservicedisplayname)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    WCHAR buf[128]; DWORD n = 128;
    SET_ERR(0);
    BOOL r = SH(GetServiceDisplayNameW)(svc, L"RpcSs", buf, &n);
    /* Win10 22H2: a service handle is accepted where an SCM handle is expected (no handle-type check) */
    t_note("GetServiceDisplayNameW(service handle) %s, err %lu", r ? "succeeded" : "failed", GetLastError());
    CHECK(r);
    close_both(scm, svc);
}

TEST(scl_typeconf_null_handles)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceStatus)); REQUIRE(SH(QueryServiceStatusEx)); REQUIRE(SH(QueryServiceConfigW));
    SERVICE_STATUS st; SERVICE_STATUS_PROCESS sp; BYTE buf[256]; DWORD need = 0;
    SET_ERR(0);
    CHECK(!SH(QueryServiceStatus)(NULL, &st));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!SH(QueryServiceStatusEx)(NULL, SC_STATUS_PROCESS_INFO, (LPBYTE)&sp, sizeof sp, &need));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!SH(QueryServiceConfigW)(NULL, (LPQUERY_SERVICE_CONFIGW)buf, sizeof buf, &need));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---- access matrix (RpcSs, handles lacking the needed right) --------------------- */
TEST(scl_matrix_queryconfig_needs_query_config)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    BYTE buf[4096]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceConfigW)(svc, (LPQUERY_SERVICE_CONFIGW)buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = SH(QueryServiceConfigA)(svc, (LPQUERY_SERVICE_CONFIGA)buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = SH(QueryServiceConfigW)(svc, NULL, 0, &need);   /* the size probe is access checked too */
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_queryconfig2_needs_query_config)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    BYTE buf[4096]; DWORD need = 0;
    DWORD levels[] = {SERVICE_CONFIG_DESCRIPTION, SERVICE_CONFIG_FAILURE_ACTIONS, SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                      SERVICE_CONFIG_SERVICE_SID_INFO, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, SERVICE_CONFIG_PRESHUTDOWN_INFO};
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        BOOL r = SH(QueryServiceConfig2W)(svc, levels[i], buf, sizeof buf, &need);
        CHECKF(!r && GetLastError() == ERROR_ACCESS_DENIED, "level %lu err %lu", levels[i], GetLastError());
    }
    close_both(scm, svc);
}

TEST(scl_matrix_querystatus_needs_query_status)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) return;
    SERVICE_STATUS st; SERVICE_STATUS_PROCESS sp; DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceStatus)(svc, &st);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = SH(QueryServiceStatusEx)(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&sp, sizeof sp, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_interrogate_needs_interrogate)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    SERVICE_STATUS st;
    SET_ERR(0);
    BOOL r = SH(ControlService)(svc, SERVICE_CONTROL_INTERROGATE, &st);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_start_needs_start)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG)) return;
    SET_ERR(0);
    BOOL r = SH(StartServiceW)(svc, 0, NULL);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_delete_needs_delete)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG | READ_CONTROL)) return;
    SET_ERR(0);
    BOOL r = SH(DeleteService)(svc);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_changeconfig_needs_change_config)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG)) return;
    SET_ERR(0);
    /* SERVICE_NO_CHANGE for every field and NULL strings: even if the right were present this changes nothing. */
    BOOL r = SH(ChangeServiceConfigW)(svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = SH(ChangeServiceConfigA)(svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_dependents_needs_enum_dependents)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    DWORD need = 0, cnt = 0;
    SET_ERR(0);
    BOOL r = SH(EnumDependentServicesW)(svc, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    close_both(scm, svc);
}

TEST(scl_matrix_scm_connect_only_denied_calls)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    DWORD need = 0, cnt = 0, resume = 0; BYTE b[256];
    SET_ERR(0);
    BOOL r = SH(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    SC_HANDLE c = SH(CreateServiceW)(scm, L"olet_neverCreated", NULL, SERVICE_QUERY_STATUS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                                     SERVICE_ERROR_NORMAL, L"C:\\Windows\\System32\\olet_never.exe", NULL, NULL, NULL, NULL, NULL);
    EXPECT_FAIL(c == NULL, ERROR_ACCESS_DENIED);
    if (c) { SH(DeleteService)(c); SH(CloseServiceHandle)(c); }
    close_both(scm, NULL);
}

TEST(scl_matrix_scm_enumerate_needs_enumerate)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_ENUMERATE_SERVICE, 0)) return;
    DWORD need = 0, cnt = 0, resume = 0;
    SET_ERR(0);
    BOOL r = SH(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > sizeof(ENUM_SERVICE_STATUS_PROCESSW));
    close_both(scm, NULL);
}

TEST(scl_matrix_getservicename_needs_connect)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    WCHAR buf[256]; DWORD n = 256;
    BOOL r = SH(GetServiceDisplayNameW)(scm, L"RpcSs", buf, &n);
    CHECK(r);
    close_both(scm, NULL);
}

/* ---- status / config protocol --------------------------------------------- */
TEST(scl_status_rpcss_running)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    SERVICE_STATUS st = {0};
    BOOL r = SH(QueryServiceStatus)(svc, &st);
    CHECK(r);
    EXPECT_EQ(st.dwCurrentState, SERVICE_RUNNING);
    CHECK(st.dwServiceType & SERVICE_WIN32);
    EXPECT_EQ(st.dwCheckPoint, 0);
    EXPECT_EQ(st.dwWaitHint, 0);
    t_note("RpcSs type 0x%lx accepted 0x%lx", st.dwServiceType, st.dwControlsAccepted);
    close_both(scm, svc);
}

TEST(scl_status_ex_size_protocol)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    SERVICE_STATUS_PROCESS sp; DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceStatusEx)(svc, SC_STATUS_PROCESS_INFO, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, sizeof(SERVICE_STATUS_PROCESS));
    need = 0;
    SET_ERR(0);
    r = SH(QueryServiceStatusEx)(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&sp, sizeof sp - 1, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, sizeof(SERVICE_STATUS_PROCESS));
    memset(&sp, 0xcc, sizeof sp);
    r = SH(QueryServiceStatusEx)(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&sp, sizeof sp, &need);
    CHECK(r);
    EXPECT_EQ(sp.dwCurrentState, SERVICE_RUNNING);
    CHECK(sp.dwProcessId != 0);
    SET_ERR(0);
    union { SERVICE_STATUS_PROCESS sp; BYTE pad[sizeof(SERVICE_STATUS_PROCESS) + 100]; } big;   /* the buffer must really be that large */
    r = SH(QueryServiceStatusEx)(svc, SC_STATUS_PROCESS_INFO, big.pad, sizeof big.pad, &need);   /* larger is fine */
    CHECK(r);
    close_both(scm, svc);
}

TEST(scl_status_ex_invalid_level)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS)) return;
    SERVICE_STATUS_PROCESS sp; DWORD need = 0;
    DWORD levels[] = {1, 2, 99, 0xffffffff};
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = SH(QueryServiceStatusEx)(svc, (SC_STATUS_TYPE)levels[i], (LPBYTE)&sp, sizeof sp, &need);
        CHECKF(!r && GetLastError() == ERROR_INVALID_LEVEL, "level %lu err %lu", levels[i], GetLastError());
    }
    close_both(scm, svc);
}

TEST(scl_config_w_size_protocol)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) return;
    DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceConfigW)(svc, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(QUERY_SERVICE_CONFIGW));
    DWORD first = need;
    BYTE *buf = calloc(1, first + 64);
    DWORD need2 = 0;
    SET_ERR(0);
    r = SH(QueryServiceConfigW)(svc, (LPQUERY_SERVICE_CONFIGW)buf, 4, &need2);   /* below the structure size */
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need2, first);
    need2 = 0;
    r = SH(QueryServiceConfigW)(svc, (LPQUERY_SERVICE_CONFIGW)buf, first, &need2);
    CHECKF(r, "err %lu need %lu", GetLastError(), need2);
    if (r) {
        QUERY_SERVICE_CONFIGW *c = (QUERY_SERVICE_CONFIGW *)buf;
        EXPECT_EQ(c->dwStartType, SERVICE_AUTO_START);
        CHECK(c->dwServiceType & SERVICE_WIN32);
        CHECK(c->lpBinaryPathName != NULL && c->lpBinaryPathName[0]);
        CHECK(c->lpServiceStartName != NULL && c->lpServiceStartName[0]);
        CHECK(c->lpDisplayName != NULL && c->lpDisplayName[0]);
        CHECK((BYTE *)c->lpBinaryPathName >= buf && (BYTE *)c->lpBinaryPathName < buf + first);   /* strings live in the caller buffer */
        t_note("RpcSs path %ls start-name %ls type 0x%lx", c->lpBinaryPathName, c->lpServiceStartName, c->dwServiceType);
    }
    free(buf);
    close_both(scm, svc);
}

TEST(scl_config_a_matches_w)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) return;
    DWORD nw = 0, na = 0;
    SH(QueryServiceConfigW)(svc, NULL, 0, &nw);
    SH(QueryServiceConfigA)(svc, NULL, 0, &na);
    CHECK(nw && na);
    BYTE *bw = calloc(1, nw + 64), *ba = calloc(1, na + 64);
    BOOL rw = SH(QueryServiceConfigW)(svc, (LPQUERY_SERVICE_CONFIGW)bw, nw, &nw);
    BOOL ra = SH(QueryServiceConfigA)(svc, (LPQUERY_SERVICE_CONFIGA)ba, na, &na);
    CHECK(rw && ra);
    if (rw && ra) {
        QUERY_SERVICE_CONFIGW *w = (void *)bw; QUERY_SERVICE_CONFIGA *a = (void *)ba;
        EXPECT_EQ(w->dwServiceType, a->dwServiceType);
        EXPECT_EQ(w->dwStartType, a->dwStartType);
        EXPECT_EQ(w->dwErrorControl, a->dwErrorControl);
        EXPECT_EQ(w->dwTagId, a->dwTagId);
        char conv[1024];
        WideCharToMultiByte(CP_ACP, 0, w->lpBinaryPathName, -1, conv, sizeof conv, NULL, NULL);
        EXPECT_STR(conv, a->lpBinaryPathName);
        WideCharToMultiByte(CP_ACP, 0, w->lpServiceStartName, -1, conv, sizeof conv, NULL, NULL);
        EXPECT_STR(conv, a->lpServiceStartName);
    }
    free(bw); free(ba);
    close_both(scm, svc);
}

TEST(scl_config2_description_size_protocol)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) return;
    DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceConfig2W)(svc, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need);
    if (r) { t_note("description query with 0 bytes succeeded"); close_both(scm, svc); return; }
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(SERVICE_DESCRIPTIONW));
    BYTE *b = calloc(1, need + 16);
    r = SH(QueryServiceConfig2W)(svc, SERVICE_CONFIG_DESCRIPTION, b, need, &need);
    CHECK(r);
    free(b);
    SET_ERR(0);
    BYTE small[4];
    r = SH(QueryServiceConfig2W)(svc, SERVICE_CONFIG_DESCRIPTION, small, sizeof small, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    close_both(scm, svc);
}

TEST(scl_config2_invalid_level)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) return;
    BYTE buf[512]; DWORD need = 0;
    DWORD levels[] = {0, 99, 0x7fffffff};
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL r = SH(QueryServiceConfig2W)(svc, levels[i], buf, sizeof buf, &need);
        CHECKF(!r && GetLastError() == ERROR_INVALID_LEVEL, "level %lu err %lu", levels[i], GetLastError());
    }
    close_both(scm, svc);
}

TEST(scl_config2_sid_info_rpcss)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, SERVICE_QUERY_CONFIG)) return;
    SERVICE_SID_INFO si; DWORD need = 0;
    BOOL r = SH(QueryServiceConfig2W)(svc, SERVICE_CONFIG_SERVICE_SID_INFO, (LPBYTE)&si, sizeof si, &need);
    CHECK(r);
    if (r) { t_note("RpcSs service SID type %lu", si.dwServiceSidType); CHECK(si.dwServiceSidType <= SERVICE_SID_TYPE_RESTRICTED); }
    close_both(scm, svc);
}

/* ---- display name / key name ------------------------------------------------ */
TEST(scl_displayname_rpcss_roundtrip)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    WCHAR disp[256]; DWORD n = 256;
    SET_ERR(0);
    BOOL r = SH(GetServiceDisplayNameW)(scm, L"RpcSs", disp, &n);
    CHECK(r);
    if (r) {
        CHECK(n > 0);
        EXPECT_EQ(n, lstrlenW(disp));   /* count excludes the terminator */
        WCHAR key[256]; DWORD kn = 256;
        r = SH(GetServiceKeyNameW)(scm, disp, key, &kn);
        CHECK(r);
        if (r) { CHECK(!lstrcmpiW(key, L"RpcSs")); EXPECT_EQ(kn, lstrlenW(key)); }
        t_note("RpcSs display name: %ls", disp);
    }
    close_both(scm, NULL);
}

TEST(scl_displayname_buffer_protocol)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    WCHAR disp[256]; DWORD n = 256;
    if (!SH(GetServiceDisplayNameW)(scm, L"RpcSs", disp, &n)) { t_skip("display name unavailable"); close_both(scm, NULL); return; }
    DWORD want = n;
    /* NULL buffer */
    n = 0;
    SET_ERR(0);
    BOOL r = SH(GetServiceDisplayNameW)(scm, L"RpcSs", NULL, &n);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, want);
    /* exactly the length: no room for the terminator */
    WCHAR tmp[256];
    n = want;
    SET_ERR(0);
    r = SH(GetServiceDisplayNameW)(scm, L"RpcSs", tmp, &n);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, want);
    /* length + 1 works */
    n = want + 1;
    r = SH(GetServiceDisplayNameW)(scm, L"RpcSs", tmp, &n);
    CHECK(r);
    EXPECT_EQ(n, want);
    EXPECT_EQ(lstrlenW(tmp), want);
    /* A parity */
    char ad[512]; DWORD an = 512;
    if (!UIN("advapi32.dll", GetServiceDisplayNameA)) { close_both(scm, NULL); return; }
    r = UIN("advapi32.dll", GetServiceDisplayNameA)(scm, "RpcSs", ad, &an);
    CHECK(r);
    if (r) {
        /* Win10 leaves *an at the input size on success for the A variant */
        t_note("GetServiceDisplayNameA success count %lu (string length %d)", an, lstrlenA(ad));
        CHECK(lstrlenA(ad) == lstrlenW(tmp));
    }
    close_both(scm, NULL);
}

TEST(scl_displayname_unknown_service)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    WCHAR b[64]; DWORD n = 64;
    SET_ERR(0);
    BOOL r = SH(GetServiceDisplayNameW)(scm, L"olet_NoSuchService_zzz", b, &n);
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    n = 64;
    SET_ERR(0);
    r = SH(GetServiceKeyNameW)(scm, L"olet no such display name", b, &n);
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    close_both(scm, NULL);
}

TEST(scl_keyname_of_key_name_is_itself)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_CONNECT, 0)) return;
    WCHAR b[128]; DWORD n = 128;
    /* GetServiceKeyName also accepts a service name when no display name matches */
    BOOL r = SH(GetServiceKeyNameW)(scm, L"RpcSs", b, &n);
    if (r) { CHECK(!lstrcmpiW(b, L"RpcSs")); t_note("GetServiceKeyNameW accepts a key name"); }
    else t_note("GetServiceKeyNameW(key name) err %lu", GetLastError());
    close_both(scm, NULL);
}

/* ---- enumeration protocol ---------------------------------------------------- */
TEST(scl_enum_services_size_protocol)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT, 0)) return;
    DWORD need = 0, cnt = 99, resume = 0;
    SET_ERR(0);
    BOOL r = SH(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    EXPECT_EQ(cnt, 0);
    CHECK(need > 0);
    BYTE *buf = calloc(1, need + 4096);
    resume = 0;
    r = SH(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, need + 4096, &need, &cnt, &resume, NULL);
    CHECKF(r || GetLastError() == ERROR_MORE_DATA, "err %lu", GetLastError());
    CHECK(cnt > 10);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
    int found = 0;
    for (DWORD i = 0; i < cnt; i++) if (!lstrcmpiW(e[i].lpServiceName, L"RpcSs")) found++;
    if (r) EXPECT_EQ(found, 1);
    free(buf);
    close_both(scm, NULL);
}

TEST(scl_enum_services_invalid_level_and_types)
{
    SC_HANDLE scm, svc;
    if (!need_scm_and_rpcss(&scm, &svc, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT, 0)) return;
    DWORD need = 0, cnt = 0, resume = 0; BYTE b[64];
    SET_ERR(0);
    BOOL r = SH(EnumServicesStatusExW)(scm, (SC_ENUM_TYPE)5, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    SET_ERR(0);
    r = SH(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, 0, b, sizeof b, &need, &cnt, &resume, NULL);
    CHECK(!r);
    t_note("EnumServicesStatusExW(state 0) err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = SH(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, 0, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &resume, NULL);
    CHECK(!r);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    close_both(scm, NULL);
}
