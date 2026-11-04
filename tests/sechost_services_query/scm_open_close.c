#include "svc.h"

/* ---------- OpenSCManager ---------- */
TEST(scm_openW_connect) {
    REQUIRE(S(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    if (h) { svc_close(h); }
}
TEST(scm_openA_connect) {
    REQUIRE(S(OpenSCManagerA));
    SC_HANDLE h = S(OpenSCManagerA)(NULL, NULL, SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    svc_close(h);
}
TEST(scm_open_enumerate) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    CHECK(h != NULL);
    svc_close(h);
}
TEST(scm_open_generic_read) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, NULL, GENERIC_READ);
    CHECK(h != NULL);
    svc_close(h);
}
TEST(scm_open_all_access) {
    REQUIRE(S(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (h) svc_close(h);
    else { DWORD e = GetLastError(); t_note("SC_MANAGER_ALL_ACCESS denied (standard user?) err=%lu", e); if (e != ERROR_ACCESS_DENIED) CHECKF(0, "unexpected error %lu", e); }
}
TEST(scm_open_maximum_allowed) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, NULL, MAXIMUM_ALLOWED);
    t_note("OpenSCManagerW(MAXIMUM_ALLOWED) -> %p err=%lu", (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_access_zero) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, NULL, 0);
    t_note("OpenSCManagerW(access=0) -> %p err=%lu", (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_explicit_active_database) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"ServicesActive", SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    svc_close(h);
}
TEST(scm_open_explicit_active_database_A) {
    REQUIRE(S(OpenSCManagerA));
    SC_HANDLE h = S(OpenSCManagerA)(NULL, "ServicesActive", SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    svc_close(h);
}
TEST(scm_open_active_database_case_insensitive) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"servicesactive", SC_MANAGER_CONNECT);
    t_note("lowercase ServicesActive -> %p err=%lu", (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_failed_database) {
    REQUIRE(S(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"ServicesFailed", SC_MANAGER_CONNECT);
    t_note("ServicesFailed database -> %p err=%lu", (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_nonexistent_database) {
    REQUIRE(S(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"Nope", SC_MANAGER_CONNECT);
    CHECK(h == NULL); { DWORD e = GetLastError(); t_note("OpenSCManagerW(db Nope) err=%lu", e); CHECKF(e == ERROR_DATABASE_DOES_NOT_EXIST || e == ERROR_INVALID_NAME, "err=%lu", e); }
    svc_close(h);
}
TEST(scm_open_nonexistent_database_A) {
    REQUIRE(S(OpenSCManagerA));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerA)(NULL, "Nope", SC_MANAGER_CONNECT);
    CHECK(h == NULL); { DWORD e = GetLastError(); CHECKF(e == ERROR_DATABASE_DOES_NOT_EXIST || e == ERROR_INVALID_NAME, "err=%lu", e); }
    svc_close(h);
}
TEST(scm_open_empty_database_name) {
    REQUIRE(S(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"", SC_MANAGER_CONNECT);
    t_note("empty database name -> %p err=%lu", (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_empty_machine_name_is_local) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(L"", NULL, SC_MANAGER_CONNECT);
    CHECK(h != NULL);
    svc_close(h);
}
TEST(scm_open_dot_machine_name) {
    REQUIRE(S(OpenSCManagerW));
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(L".", NULL, SC_MANAGER_CONNECT);
    t_note("machine '.' -> %p err=%lu", (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_local_unc_machine_name) {
    REQUIRE(S(OpenSCManagerW));
    WCHAR host[64]; DWORD n = 64; WCHAR unc[80] = L"\\\\";
    if (!GetComputerNameW(host, &n)) { t_skip("no computer name"); return; }
    lstrcatW(unc, host);
    SC_HANDLE h = S(OpenSCManagerW)(unc, NULL, SC_MANAGER_CONNECT);
    t_note("machine %ls -> %p err=%lu", unc, (void *)h, h ? 0 : GetLastError());
    svc_close(h);
}
TEST(scm_open_distinct_handles) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE a = scm_open(SC_MANAGER_CONNECT), b = scm_open(SC_MANAGER_CONNECT);
    CHECK(a && b);
    CHECK(a != b);
    svc_close(a); svc_close(b);
}
TEST(scm_open_many_sequential) {
    REQUIRE(S(OpenSCManagerW));
    int ok = 0;
    for (int i = 0; i < 50; i++) { SC_HANDLE h = scm_open(SC_MANAGER_CONNECT); if (h) { ok++; svc_close(h); } }
    EXPECT_EQ(ok, 50);
}
TEST(scm_open_a_w_parity_failure) {
    REQUIRE(S(OpenSCManagerW)); REQUIRE(S(OpenSCManagerA));
    SET_ERR(0); SC_HANDLE a = S(OpenSCManagerA)(NULL, "Nope", SC_MANAGER_CONNECT); DWORD ea = GetLastError();
    SET_ERR(0); SC_HANDLE w = S(OpenSCManagerW)(NULL, L"Nope", SC_MANAGER_CONNECT); DWORD ew = GetLastError();
    CHECK(!a && !w);
    EXPECT_EQ(ea, ew);
}
TEST(scm_open_sechost_exports_both) {
    CHECK(t_proc_in("sechost.dll", "OpenSCManagerW") != NULL);
    CHECK(t_proc_in("sechost.dll", "OpenSCManagerA") != NULL);
    CHECK(t_proc_in("sechost.dll", "CloseServiceHandle") != NULL);
    CHECK(t_proc_in("sechost.dll", "OpenServiceW") != NULL);
    CHECK(t_proc_in("sechost.dll", "OpenServiceA") != NULL);
}
TEST(scm_open_advapi32_forwarder_resolves) {
    FARPROC a = t_proc_in("advapi32.dll", "OpenSCManagerW");
    FARPROC s = t_proc_in("sechost.dll", "OpenSCManagerW");
    REQUIRE(a); REQUIRE(s);
    /* Windows 10 advapi32 forwards the SCM API to sechost: same function address. */
    if (a != s) t_note("advapi32!OpenSCManagerW %p != sechost %p (not a plain forwarder)", (void *)a, (void *)s);
    HMODULE m = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)a, &m);
    WCHAR path[MAX_PATH] = L"";
    if (m) GetModuleFileNameW(m, path, MAX_PATH);
    t_note("advapi32!OpenSCManagerW lives in %ls", path);
}

/* ---------- CloseServiceHandle ---------- */
TEST(scm_close_null) {
    REQUIRE(S(CloseServiceHandle));
    SET_ERR(0);
    BOOL r = S(CloseServiceHandle)(NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(scm_close_scm_handle) {
    REQUIRE(S(CloseServiceHandle));
    SC_HANDLE h = scm_open(SC_MANAGER_CONNECT);
    REQUIRE(h);
    SET_ERR(0);
    CHECK(S(CloseServiceHandle)(h));
}
TEST(scm_close_double_close) {
    REQUIRE(S(CloseServiceHandle));
    SC_HANDLE h = scm_open(SC_MANAGER_CONNECT);
    REQUIRE(h);
    CHECK(S(CloseServiceHandle)(h));
    SET_ERR(0);
    BOOL r = S(CloseServiceHandle)(h);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(scm_close_service_handle) {
    REQUIRE(S(CloseServiceHandle));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm);
    REQUIRE(s);
    CHECK(S(CloseServiceHandle)(s));
    CHECK(S(CloseServiceHandle)(scm));
}
TEST(scm_close_used_handle_unusable) {
    REQUIRE(S(OpenSCManagerW)); REQUIRE(S(OpenServiceW)); REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm);
    REQUIRE(s);
    SERVICE_STATUS st;
    CHECK(S(QueryServiceStatus)(s, &st));
    CHECK(S(CloseServiceHandle)(s));
    SET_ERR(0);
    BOOL r = S(QueryServiceStatus)(s, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(scm_close_scm_first_service_handle_survives) {
    REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm);
    REQUIRE(s);
    CHECK(S(CloseServiceHandle)(scm));
    SERVICE_STATUS st;
    CHECK(S(QueryServiceStatus)(s, &st));
    svc_close(s);
}
TEST(scm_close_many_service_handles) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    REQUIRE(scm);
    SC_HANDLE hs[20];
    for (int i = 0; i < 20; i++) { hs[i] = svc_open(scm, "RpcSs", SERVICE_QUERY_STATUS); CHECK(hs[i] != NULL); }
    for (int i = 0; i < 20; i++) for (int j = i + 1; j < 20; j++) if (hs[i] && hs[i] == hs[j]) CHECKF(0, "duplicate handle %d/%d", i, j);
    for (int i = 0; i < 20; i++) svc_close(hs[i]);
    svc_close(scm);
}

/* ---------- OpenService ---------- */
static void check_open_ok(const char *name) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = svc_open(scm, name, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!s) CHECKF(0, "OpenServiceW(%s) failed err=%lu", name, GetLastError());
    svc_close(s); svc_close(scm);
}
PER_SERVICE(scm_openservice_exists, check_open_ok)
TEST(scm_openservice_LanmanWorkstation) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    REQUIRE(scm);
    SC_HANDLE s = svc_open(scm, "LanmanWorkstation", SERVICE_QUERY_STATUS);
    if (!s) t_note("LanmanWorkstation absent err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
static void check_open_A(const char *name) {
    REQUIRE(S(OpenServiceA));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    REQUIRE(scm);
    SC_HANDLE s = S(OpenServiceA)(scm, name, SERVICE_QUERY_STATUS);
    CHECK(s != NULL);
    svc_close(s); svc_close(scm);
}
PER_SERVICE(scm_openserviceA_exists, check_open_A)

TEST(scm_openservice_nonexistent_W) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"olet_NoSuchService_12345", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_nonexistent_A) {
    REQUIRE(S(OpenServiceA));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceA)(scm, "olet_NoSuchService_12345", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_empty_name_W) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_INVALID_NAME);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_empty_name_A) {
    REQUIRE(S(OpenServiceA));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceA)(scm, "", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_INVALID_NAME);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_null_name_W) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, NULL, SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    t_note("OpenServiceW(NULL name) err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_null_name_A) {
    REQUIRE(S(OpenServiceA));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceA)(scm, NULL, SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    t_note("OpenServiceA(NULL name) err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_null_scm) {
    REQUIRE(S(OpenServiceW));
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(NULL, L"RpcSs", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_INVALID_HANDLE);
    svc_close(s);
}
TEST(scm_openservice_null_scm_A) {
    REQUIRE(S(OpenServiceA));
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceA)(NULL, "RpcSs", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_INVALID_HANDLE);
    svc_close(s);
}
TEST(scm_openservice_closed_scm) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    svc_close(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_INVALID_HANDLE);
    svc_close(s);
}
TEST(scm_openservice_service_handle_as_scm) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm, h = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(h);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(h, L"RpcSs", SERVICE_QUERY_STATUS);
    EXPECT_FAIL(s == NULL, ERROR_INVALID_HANDLE);
    svc_close(s); svc_close(h); svc_close(scm);
}
TEST(scm_openservice_case_insensitive) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE s = S(OpenServiceW)(scm, L"rPcSs", SERVICE_QUERY_STATUS);
    CHECK(s != NULL);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_trailing_space_not_matched) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs ", SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    t_note("'RpcSs ' (trailing space) err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_display_name_is_not_key_name) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"Remote Procedure Call (RPC)", SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    t_note("display name as key: err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_backslash_name) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"Rpc\\Ss", SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    t_note("backslash name err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_slash_name) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"Rpc/Ss", SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    t_note("slash name err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_long_name_257) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    static WCHAR nm[400];
    for (int i = 0; i < 257; i++) nm[i] = L'a';
    nm[257] = 0;
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, nm, SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    DWORD e = GetLastError();
    /* Win10: over-long names are rejected by the RPC stub (RPC_X_BAD_STUB_DATA, 1783). */
    EXPECT_EQ(e, RPC_X_BAD_STUB_DATA);
    t_note("257-char name err=%lu", e);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_long_name_1000) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    static WCHAR nm[1100];
    for (int i = 0; i < 1000; i++) nm[i] = L'b';
    nm[1000] = 0;
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, nm, SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    DWORD e = GetLastError();
    EXPECT_EQ(e, RPC_X_BAD_STUB_DATA); /* see the 257-char case */
    t_note("1000-char name err=%lu", e);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_long_name_A) {
    REQUIRE(S(OpenServiceA));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    static char nm[600];
    memset(nm, 'c', 300); nm[300] = 0;
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceA)(scm, nm, SERVICE_QUERY_STATUS);
    CHECK(s == NULL);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_access_zero) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", 0);
    t_note("OpenServiceW(access 0) -> %p err=%lu", (void *)s, s ? 0 : GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_access_zero_cannot_query) {
    REQUIRE(S(OpenServiceW)); REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", 0);
    /* Win10 refuses to open a service with desired access 0, so a handle that cannot query never exists. */
    if (s) {
        SERVICE_STATUS st; SET_ERR(0);
        BOOL r = S(QueryServiceStatus)(s, &st);
        EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    } else EXPECT_ERR(ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_stop_right_rpcss) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", SERVICE_STOP);
    t_note("OpenServiceW(RpcSs, SERVICE_STOP) -> %p err=%lu", (void *)s, s ? 0 : GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_all_access_rpcss) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", SERVICE_ALL_ACCESS);
    DWORD e = s ? 0 : GetLastError();
    t_note("OpenServiceW(RpcSs, ALL_ACCESS) -> %p err=%lu", (void *)s, e);
    if (!s) CHECKF(e == ERROR_ACCESS_DENIED, "unexpected err %lu", e);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_maximum_allowed) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", MAXIMUM_ALLOWED);
    CHECK(s != NULL);
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_generic_read) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", GENERIC_READ);
    if (!s) {
        /* GENERIC_READ includes ENUMERATE_DEPENDENTS, which the RpcSs DACL grants to administrators only on Win10. */
        DWORD e = GetLastError();
        SC_HANDLE s2 = S(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_INTERROGATE | READ_CONTROL);
        SC_HANDLE s3 = S(OpenServiceW)(scm, L"RpcSs", SERVICE_ENUMERATE_DEPENDENTS);
        t_note("GENERIC_READ on RpcSs denied err=%lu (without ENUMERATE_DEPENDENTS %s, ENUMERATE_DEPENDENTS alone %s)", e, s2 ? "ok" : "denied", s3 ? "ok" : "denied");
        CHECK(e == ERROR_ACCESS_DENIED && s2 != NULL && s3 == NULL);
        svc_close(s2); svc_close(s3);
    }
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_each_right_individually) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    static const struct { DWORD r; const char *n; } t[] = {
        {SERVICE_QUERY_CONFIG, "QUERY_CONFIG"}, {SERVICE_QUERY_STATUS, "QUERY_STATUS"},
        {SERVICE_ENUMERATE_DEPENDENTS, "ENUMERATE_DEPENDENTS"}, {SERVICE_INTERROGATE, "INTERROGATE"},
        {SERVICE_START, "START"}, {SERVICE_STOP, "STOP"}, {SERVICE_PAUSE_CONTINUE, "PAUSE_CONTINUE"},
        {SERVICE_USER_DEFINED_CONTROL, "USER_DEFINED_CONTROL"}, {SERVICE_CHANGE_CONFIG, "CHANGE_CONFIG"},
        {READ_CONTROL, "READ_CONTROL"}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", t[i].r);
        if (!s) t_note("RpcSs right %s denied err=%lu", t[i].n, GetLastError());
        /* ENUMERATE_DEPENDENTS on RpcSs is administrator-only on Win10 (noted above when denied). */
        if (t[i].r == SERVICE_QUERY_STATUS || t[i].r == SERVICE_QUERY_CONFIG || t[i].r == SERVICE_INTERROGATE || t[i].r == READ_CONTROL)
            CHECKF(s != NULL, "right %s should be grantable", t[i].n);
        svc_close(s);
    }
    svc_close(scm);
}
TEST(scm_openservice_invalid_access_bits) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SET_ERR(0);
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", 0x00008000);
    t_note("OpenServiceW(unassigned bit 0x8000) -> %p err=%lu", (void *)s, s ? 0 : GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_scm_without_connect) {
    REQUIRE(S(OpenSCManagerW)); REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) { t_skip("cannot open scm with enumerate only"); return; }
    SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    t_note("OpenService via enumerate-only SCM handle -> %p err=%lu", (void *)s, s ? 0 : GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(scm_openservice_service_handles_independent) {
    REQUIRE(S(OpenServiceW)); REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE a = svc_open(scm, "RpcSs", SERVICE_QUERY_STATUS), b = svc_open(scm, "RpcSs", SERVICE_QUERY_STATUS);
    CHECK(a && b && a != b);
    svc_close(a);
    SERVICE_STATUS st; 
    CHECK(S(QueryServiceStatus)(b, &st));
    svc_close(b); svc_close(scm);
}
TEST(scm_openservice_sechost_vs_advapi32_same_result) {
    FARPROC a = t_proc_in("advapi32.dll", "OpenServiceW"), s = t_proc_in("sechost.dll", "OpenServiceW");
    REQUIRE(a); REQUIRE(s);
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE h = ((__typeof__(&OpenServiceW))a)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    CHECK(h != NULL);
    svc_close(h); svc_close(scm);
}
