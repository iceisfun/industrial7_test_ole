#include "svc.h"
/* Start/Control/Delete/Lock/Enum/Status/Notify. A service whose image does not exist never really runs. */

static DWORD start_it(SC_HANDLE s) { SET_ERR(0); BOOL r = S(StartServiceW)(s, 0, NULL); return r ? 0 : GetLastError(); }

TEST(dx_svc_start_missing_image) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(StartServiceW));
    SET_ERR(0);
    CHECK(!S(StartServiceW)(f.svc, 0, NULL)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SERVICE_STATUS st; CHECK(S(QueryServiceStatus)(f.svc, &st));
    EXPECT_EQ(st.dwCurrentState, SERVICE_STOPPED);
    t_note("exit code after failed start: %lu", st.dwWin32ExitCode);
    fx_end(&f);
}

TEST(dx_svc_start_a_variant) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(StartServiceA));
    SET_ERR(0);
    CHECK(!S(StartServiceA)(f.svc, 0, NULL)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    fx_end(&f);
}

TEST(dx_svc_start_with_args_missing_image) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    LPCWSTR argv[2] = { L"a", L"b" };
    SET_ERR(0);
    CHECK(!S(StartServiceW)(f.svc, 2, argv)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    fx_end(&f);
}

TEST(dx_svc_start_disabled) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    SET_ERR(0);
    CHECK(!S(StartServiceW)(f.svc, 0, NULL)); EXPECT_ERR(ERROR_SERVICE_DISABLED);
    /* re-enable: the error changes to the missing image */
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_DEMAND_START, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_EQ(start_it(f.svc), ERROR_FILE_NOT_FOUND);
    fx_end(&f);
}

TEST(dx_svc_start_requires_start_right) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    CHECK(o != NULL);
    if (o) { EXPECT_EQ(start_it(o), ERROR_ACCESS_DENIED); S(CloseServiceHandle)(o); }
    fx_end(&f);
}

TEST(dx_svc_start_wrong_handles) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    EXPECT_EQ(start_it(f.scm), ERROR_INVALID_HANDLE);
    EXPECT_EQ(start_it(NULL), ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_start_driver_type_path_missing) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    /* quoted path with arguments: the image file is the quoted part and is missing */
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
        L"\"C:\\olet_nonexistent_dir\\q q.exe\" -arg", NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_EQ(start_it(f.svc), ERROR_FILE_NOT_FOUND);
    /* directory missing vs file missing in an existing dir */
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
        L"C:\\Windows\\olet_nonexistent_file.exe", NULL, NULL, NULL, NULL, NULL, NULL));
    DWORD e = start_it(f.svc);
    t_note("start with missing file in existing dir -> %lu", e);
    CHECK(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND);
    fx_end(&f);
}

/* ---------------- ControlService ---------------- */

TEST(dx_svc_control_stopped_service) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_STATUS st;
    static const DWORD codes[] = { SERVICE_CONTROL_STOP, SERVICE_CONTROL_INTERROGATE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(!S(ControlService)(f.svc, codes[i], &st)); EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE);
    }
    static const DWORD others[] = { SERVICE_CONTROL_PAUSE, SERVICE_CONTROL_CONTINUE, SERVICE_CONTROL_PARAMCHANGE, 128, 255 };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        BOOL r = S(ControlService)(f.svc, others[i], &st);
        CHECKF(!r, "control 0x%lx on stopped service succeeded", others[i]);
        t_note("control %lu on stopped -> err %lu", others[i], GetLastError());
    }
    fx_end(&f);
}

TEST(dx_svc_control_status_filled_on_not_active) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_STATUS st; memset(&st, 0x55, sizeof st);
    SET_ERR(0);
    CHECK(!S(ControlService)(f.svc, SERVICE_CONTROL_STOP, &st)); EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE);
    t_note("status after failed stop: type 0x%lx state %lu accepts 0x%lx", st.dwServiceType, st.dwCurrentState, st.dwControlsAccepted);
    fx_end(&f);
}

TEST(dx_svc_control_rights) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(o != NULL);
    if (o) {
        SERVICE_STATUS st;
        SET_ERR(0);
        CHECK(!S(ControlService)(o, SERVICE_CONTROL_STOP, &st)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECK(!S(ControlService)(o, SERVICE_CONTROL_INTERROGATE, &st)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECK(!S(ControlService)(o, SERVICE_CONTROL_PAUSE, &st)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    fx_end(&f);
}

TEST(dx_svc_control_wrong_handle) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_STATUS st;
    SET_ERR(0);
    CHECK(!S(ControlService)(f.scm, SERVICE_CONTROL_STOP, &st)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(ControlService)(NULL, SERVICE_CONTROL_STOP, &st)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_control_ex_stopped) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ControlServiceExW)); if (t_is_wine()) { t_skip("unimplemented under wine"); fx_end(&f); return; }
    SERVICE_CONTROL_STATUS_REASON_PARAMSW p; memset(&p, 0, sizeof p);
    p.dwReason = SERVICE_STOP_REASON_FLAG_PLANNED | SERVICE_STOP_REASON_MAJOR_APPLICATION | SERVICE_STOP_REASON_MINOR_MAINTENANCE;
    SET_ERR(0);
    CHECK(!S(ControlServiceExW)(f.svc, SERVICE_CONTROL_STOP, SERVICE_CONTROL_STATUS_REASON_INFO, &p));
    EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE);
    SET_ERR(0);
    CHECK(!S(ControlServiceExW)(f.svc, SERVICE_CONTROL_STOP, 7, &p));
    t_note("ControlServiceEx bad info level -> err %lu", GetLastError());
    fx_end(&f);
}

TEST(dx_svc_control_ex_interrogate_stopped) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ControlServiceExW)); if (t_is_wine()) { t_skip("unimplemented under wine"); fx_end(&f); return; }
    SERVICE_STATUS_PROCESS ssp; memset(&ssp, 0, sizeof ssp);
    SERVICE_CONTROL_STATUS_REASON_PARAMSW p; memset(&p, 0, sizeof p);
    SET_ERR(0);
    BOOL r = S(ControlServiceExW)(f.svc, SERVICE_CONTROL_INTERROGATE, SERVICE_CONTROL_STATUS_REASON_INFO, &p);
    CHECK(!r); EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE);
    fx_end(&f);
}

/* ---------------- status ---------------- */

TEST(dx_svc_status_ex_fields) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(QueryServiceStatusEx));
    SERVICE_STATUS_PROCESS ssp; DWORD need = 0;
    memset(&ssp, 0xAA, sizeof ssp);
    CHECK(S(QueryServiceStatusEx)(f.svc, SC_STATUS_PROCESS_INFO, (BYTE *)&ssp, sizeof ssp, &need));
    EXPECT_EQ(ssp.dwServiceType, SERVICE_WIN32_OWN_PROCESS);
    EXPECT_EQ(ssp.dwCurrentState, SERVICE_STOPPED);
    EXPECT_EQ(ssp.dwControlsAccepted, 0);
    EXPECT_EQ(ssp.dwProcessId, 0);
    EXPECT_EQ(ssp.dwServiceFlags, 0);
    EXPECT_EQ(ssp.dwCheckPoint, 0);
    fx_end(&f);
}

TEST(dx_svc_status_ex_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0; BYTE buf[64];
    SET_ERR(0);
    CHECK(!S(QueryServiceStatusEx)(f.svc, SC_STATUS_PROCESS_INFO, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, sizeof(SERVICE_STATUS_PROCESS));
    SET_ERR(0); need = 0;
    CHECK(!S(QueryServiceStatusEx)(f.svc, SC_STATUS_PROCESS_INFO, buf, sizeof(SERVICE_STATUS_PROCESS) - 1, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, sizeof(SERVICE_STATUS_PROCESS));
    CHECK(S(QueryServiceStatusEx)(f.svc, SC_STATUS_PROCESS_INFO, buf, sizeof buf, &need));
    SET_ERR(0);
    CHECK(!S(QueryServiceStatusEx)(f.svc, (SC_STATUS_TYPE)1, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_INVALID_LEVEL);
    SET_ERR(0);
    CHECK(!S(QueryServiceStatusEx)(f.svc, (SC_STATUS_TYPE)0xFFFF, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_INVALID_LEVEL);
    fx_end(&f);
}

TEST(dx_svc_status_wrong_handle_and_rights) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_STATUS st;
    SET_ERR(0);
    CHECK(!S(QueryServiceStatus)(NULL, &st)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_CONFIG);
    CHECK(o != NULL);
    if (o) {
        SET_ERR(0);
        CHECK(!S(QueryServiceStatus)(o, &st)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    fx_end(&f);
}

TEST(dx_svc_status_after_close_handle) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(o != NULL);
    if (o) {
        CHECK(S(CloseServiceHandle)(o));
        SERVICE_STATUS st;
        SET_ERR(0);
        CHECK(!S(QueryServiceStatus)(o, &st)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    fx_end(&f);
}

/* ---------------- DeleteService ---------------- */

TEST(dx_svc_delete_basic) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(DeleteService)(f.svc));
    S(CloseServiceHandle)(f.svc); f.svc = NULL;
    CHECK(fx_wait_gone(f.scm, f.name));
    SET_ERR(0);
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(o == NULL); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    CHECK(!reg_exists(f.name));
    fx_end(&f);
}

TEST(dx_svc_delete_twice_marked) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(DeleteService)(f.svc));
    SET_ERR(0);
    CHECK(!S(DeleteService)(f.svc)); EXPECT_ERR(ERROR_SERVICE_MARKED_FOR_DELETE);
    fx_end(&f);
}

TEST(dx_svc_delete_marked_blocks_open_create_change) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(DeleteService)(f.svc));
    SET_ERR(0);
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    /* Win10: OpenService on a marked service succeeds; only create/change/start refuse */
    CHECK(o != NULL);
    if (o) S(CloseServiceHandle)(o);
    SET_ERR(0);
    SC_HANDLE c = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(c == NULL); EXPECT_ERR(ERROR_SERVICE_MARKED_FOR_DELETE);
    if (c) S(CloseServiceHandle)(c);
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_SERVICE_MARKED_FOR_DELETE);
    SERVICE_DESCRIPTIONW d = { (LPWSTR)L"x" };
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &d));
    t_note("ChangeServiceConfig2 on marked service -> err %lu", GetLastError());
    EXPECT_EQ(start_it(f.svc), ERROR_SERVICE_DISABLED); /* Win10: 1058 */
    SERVICE_STATUS st; BOOL r = S(QueryServiceStatus)(f.svc, &st);
    t_note("QueryServiceStatus on marked service -> %d", r);
    CHECK(r);
    fx_end(&f);
}

TEST(dx_svc_delete_with_second_handle_open) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE h2 = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(h2 != NULL);
    CHECK(S(DeleteService)(f.svc));
    S(CloseServiceHandle)(f.svc); f.svc = NULL;
    /* h2 is still open: service not yet removed, still marked */
    SET_ERR(0);
    SC_HANDLE c = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(c == NULL); EXPECT_ERR(ERROR_SERVICE_MARKED_FOR_DELETE);
    if (c) S(CloseServiceHandle)(c);
    SERVICE_STATUS st;
    CHECK(S(QueryServiceStatus)(h2, &st));
    if (h2) S(CloseServiceHandle)(h2);
    /* last handle closed: gone */
    /* Win10: the SCM drops the record asynchronously after the last handle is closed */
    SC_HANDLE o; int i;
    for (i = 0; i < 100; i++) {
        SET_ERR(0);
        o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
        if (!o) break;
        S(CloseServiceHandle)(o); Sleep(100);
    }
    CHECK(o == NULL); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    for (i = 0; i < 100 && reg_exists(f.name); i++) Sleep(100);
    CHECK(!reg_exists(f.name));
    /* name is reusable now */
    c = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(c != NULL); f.svc = c;
    fx_end(&f);
}

TEST(dx_svc_delete_requires_delete_right) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_ALL_ACCESS & ~DELETE);
    CHECK(o != NULL);
    if (o) { SET_ERR(0); CHECK(!S(DeleteService)(o)); EXPECT_ERR(ERROR_ACCESS_DENIED); S(CloseServiceHandle)(o); }
    SET_ERR(0);
    CHECK(!S(DeleteService)(f.scm)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(DeleteService)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_delete_dependency_chain) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    WCHAR na[160], nb[160], deps[200]; memset(deps, 0, sizeof deps);
    SC_HANDLE a = fx_create2(&f, L"da", SERVICE_DEMAND_START, NULL, na);
    lstrcpyW(deps, na);
    SC_HANDLE b = fx_create2(&f, L"db", SERVICE_DEMAND_START, deps, nb);
    CHECK(a && b);
    if (a && b) {
        /* deleting a service that others depend on is permitted by the SCM */
        BOOL r = S(DeleteService)(a);
        t_note("delete depended-upon service -> %d err %lu", r, GetLastError());
        CHECK(r);
    }
    fx_del(&f, nb, b);
    fx_del(&f, na, a);
    fx_end(&f);
}

TEST(dx_svc_delete_removes_from_enum) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(DeleteService)(f.svc));
    S(CloseServiceHandle)(f.svc); f.svc = NULL;
    DWORD need = 0, cnt = 0, rs = 0;
    S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &rs, NULL);
    BYTE *b = malloc(need + 65536);
    rs = 0;
    int found = 0;
    if (S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, b, need + 65536, &need, &cnt, &rs, NULL)) {
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b;
        for (DWORD i = 0; i < cnt; i++) if (!lstrcmpiW(e[i].lpServiceName, f.name)) found = 1;
    }
    CHECK(!found);
    free(b);
    fx_end(&f);
}

/* ---------------- enumeration ---------------- */

static int enum_find(fx_t *f, DWORD type, DWORD state, const WCHAR *group, ENUM_SERVICE_STATUS_PROCESSW *out) {
    DWORD need = 0, cnt = 0, rs = 0;
    S(EnumServicesStatusExW)(f->scm, SC_ENUM_PROCESS_INFO, type, state, NULL, 0, &need, &cnt, &rs, group);
    DWORD cap = need + 65536; BYTE *b = malloc(cap);
    int found = 0; rs = 0;
    for (;;) {
        BOOL r = S(EnumServicesStatusExW)(f->scm, SC_ENUM_PROCESS_INFO, type, state, b, cap, &need, &cnt, &rs, group);
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b;
        for (DWORD i = 0; i < cnt; i++) if (!lstrcmpiW(e[i].lpServiceName, f->name)) { found = 1; if (out) { *out = e[i]; out->lpServiceName = NULL; out->lpDisplayName = NULL; } }
        if (r || GetLastError() != ERROR_MORE_DATA) break;
    }
    free(b);
    return found;
}

TEST(dx_svc_enum_finds_service) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(EnumServicesStatusExW));
    ENUM_SERVICE_STATUS_PROCESSW e; memset(&e, 0, sizeof e);
    CHECK(enum_find(&f, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &e));
    EXPECT_EQ(e.ServiceStatusProcess.dwServiceType, SERVICE_WIN32_OWN_PROCESS);
    EXPECT_EQ(e.ServiceStatusProcess.dwCurrentState, SERVICE_STOPPED);
    EXPECT_EQ(e.ServiceStatusProcess.dwProcessId, 0);
    fx_end(&f);
}

TEST(dx_svc_enum_state_filters) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(enum_find(&f, SERVICE_WIN32, SERVICE_INACTIVE, NULL, NULL));
    CHECK(!enum_find(&f, SERVICE_WIN32, SERVICE_ACTIVE, NULL, NULL));
    CHECK(enum_find(&f, SERVICE_WIN32_OWN_PROCESS, SERVICE_STATE_ALL, NULL, NULL));
    CHECK(!enum_find(&f, SERVICE_WIN32_SHARE_PROCESS, SERVICE_STATE_ALL, NULL, NULL));
    CHECK(!enum_find(&f, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, NULL));
    fx_end(&f);
}

TEST(dx_svc_enum_group_filter) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    /* a group name that nobody belongs to returns nothing; the empty group returns services with no group */
    CHECK(!enum_find(&f, SERVICE_WIN32, SERVICE_STATE_ALL, L"olet_no_such_group", NULL));
    int in_empty = enum_find(&f, SERVICE_WIN32, SERVICE_STATE_ALL, L"", NULL);
    t_note("found in empty-group filter: %d", in_empty);
    CHECK(in_empty);
    fx_end(&f);
}

TEST(dx_svc_enum_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0, cnt = 99, rs = 0;
    SET_ERR(0);
    CHECK(!S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &rs, NULL));
    EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(need > 0);
    EXPECT_EQ(cnt, 0);
    BYTE small[sizeof(ENUM_SERVICE_STATUS_PROCESSW) + 8];
    rs = 0; cnt = 0;
    SET_ERR(0);
    CHECK(!S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, small, sizeof small, &need, &cnt, &rs, NULL));
    EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(need > 0);
    fx_end(&f);
}

TEST(dx_svc_enum_resume_handle_pagination) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    BYTE buf[2048]; DWORD need, cnt, rs = 0, total = 0, pages = 0;
    for (;;) {
        cnt = 0;
        BOOL r = S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &rs, NULL);
        total += cnt; pages++;
        if (r) break;
        if (GetLastError() != ERROR_MORE_DATA) { CHECKF(0, "enum err %lu", GetLastError()); break; }
        if (cnt == 0) { CHECKF(0, "MORE_DATA page without entries"); break; }
        if (pages > 5000) break;
    }
    CHECK(total > 0);
    t_note("enumerated %lu services in %lu pages", total, pages);
    fx_end(&f);
}

TEST(dx_svc_enum_bad_args) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need, cnt, rs = 0; BYTE b[256]; BOOL r1;
    SET_ERR(0);
    CHECK(!S(EnumServicesStatusExW)(f.scm, (SC_ENUM_TYPE)1, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &rs, NULL));
    EXPECT_ERR(ERROR_INVALID_LEVEL);
    SET_ERR(0);
    r1 = S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, 0x100, b, sizeof b, &need, &cnt, &rs, NULL);
    t_note("bad state mask 0x100 -> %d err %lu", r1, GetLastError());
    SET_ERR(0);
    r1 = S(EnumServicesStatusExW)(f.scm, SC_ENUM_PROCESS_INFO, 0, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &rs, NULL);
    t_note("service type 0 -> %d err %lu", r1, GetLastError());
    SET_ERR(0);
    CHECK(!S(EnumServicesStatusExW)(NULL, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &rs, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(EnumServicesStatusExW)(f.svc, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &rs, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_enum_requires_enumerate_right) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE c = fx_scm(SC_MANAGER_CONNECT);
    CHECK(c != NULL);
    if (c) {
        DWORD need, cnt, rs = 0; BYTE b[256];
        SET_ERR(0);
        CHECK(!S(EnumServicesStatusExW)(c, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, b, sizeof b, &need, &cnt, &rs, NULL));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(c);
    }
    fx_end(&f);
}

TEST(dx_svc_enum_dependents) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(EnumDependentServicesW));
    WCHAR nb[160], deps[200]; memset(deps, 0, sizeof deps); lstrcpyW(deps, f.name);
    SC_HANDLE b = fx_create2(&f, L"dep1", SERVICE_DEMAND_START, deps, nb);
    CHECK(b != NULL);
    DWORD need = 0, cnt = 99;
    SET_ERR(0);
    CHECK(!S(EnumDependentServicesW)(f.svc, SERVICE_STATE_ALL, NULL, 0, &need, &cnt)); EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(need > 0);
    BYTE *buf = malloc(need + 16);
    if (S(EnumDependentServicesW)(f.svc, SERVICE_STATE_ALL, (void *)buf, need, &need, &cnt)) {
        EXPECT_EQ(cnt, 1);
        ENUM_SERVICE_STATUSW *e = (void *)buf;
        if (cnt >= 1) CHECK(!lstrcmpiW(e[0].lpServiceName, nb));
    } else CHECKF(0, "err %lu", GetLastError());
    /* active-only: none */
    cnt = 99;
    CHECK(S(EnumDependentServicesW)(f.svc, SERVICE_ACTIVE, (void *)buf, need, &need, &cnt));
    EXPECT_EQ(cnt, 0);
    free(buf);
    /* a service with no dependents */
    cnt = 99;
    CHECK(S(EnumDependentServicesW)(b, SERVICE_STATE_ALL, NULL, 0, &need, &cnt));
    EXPECT_EQ(cnt, 0);
    fx_del(&f, nb, b);
    fx_end(&f);
}

TEST(dx_svc_enum_dependents_rights) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_CONFIG);
    CHECK(o != NULL);
    if (o) {
        DWORD need, cnt;
        SET_ERR(0);
        CHECK(!S(EnumDependentServicesW)(o, SERVICE_STATE_ALL, NULL, 0, &need, &cnt)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    SET_ERR(0);
    DWORD need, cnt;
    CHECK(!S(EnumDependentServicesW)(f.scm, SERVICE_STATE_ALL, NULL, 0, &need, &cnt)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

/* ---------------- lock ---------------- */

TEST(dx_svc_lock_unlock) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    REQUIRE(S(LockServiceDatabase)); REQUIRE(S(UnlockServiceDatabase)); REQUIRE(S(QueryServiceLockStatusW));
    SC_LOCK lock = S(LockServiceDatabase)(f.scm);
    CHECKF(lock != NULL, "err %lu", GetLastError());
    if (lock) {
        BYTE buf[1024]; DWORD need = 0;
        BOOL r = S(QueryServiceLockStatusW)(f.scm, (void *)buf, sizeof buf, &need);
        CHECK(r);
        if (r) {
            QUERY_SERVICE_LOCK_STATUSW *q = (void *)buf;
            CHECK(q->fIsLocked != 0);
            CHECK(q->lpLockOwner != NULL);
            EXPECT_EQ(q->dwLockDuration < 60, 1);
        }
        /* a second lock from another SCM handle is refused */
        SC_HANDLE s2 = fx_scm(SC_MANAGER_LOCK);
        if (s2) {
            SET_ERR(0);
            SC_LOCK l2 = S(LockServiceDatabase)(s2);
            CHECK(l2 == NULL); EXPECT_ERR(ERROR_SERVICE_DATABASE_LOCKED);
            if (l2) S(UnlockServiceDatabase)(l2);
            S(CloseServiceHandle)(s2);
        }
        CHECK(S(UnlockServiceDatabase)(lock));
        SET_ERR(0);
        CHECK(!S(UnlockServiceDatabase)(lock));
        t_note("second unlock -> err %lu (1071 expected)", GetLastError());
        EXPECT_ERR(ERROR_INVALID_SERVICE_LOCK);
        BOOL r2 = S(QueryServiceLockStatusW)(f.scm, (void *)buf, sizeof buf, &need);
        CHECK(r2);
        if (r2) CHECK(((QUERY_SERVICE_LOCK_STATUSW *)buf)->fIsLocked == 0);
    }
    fx_end(&f);
}

TEST(dx_svc_lock_errors) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(LockServiceDatabase)); REQUIRE(S(UnlockServiceDatabase));
    SET_ERR(0);
    CHECK(S(LockServiceDatabase)(NULL) == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(S(LockServiceDatabase)(f.svc) == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(UnlockServiceDatabase)(NULL)); EXPECT_ERR(ERROR_INVALID_SERVICE_LOCK);
    SC_HANDLE c = fx_scm(SC_MANAGER_CONNECT);
    if (c) {
        SET_ERR(0);
        CHECK(S(LockServiceDatabase)(c) == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(c);
    }
    fx_end(&f);
}

TEST(dx_svc_lock_status_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    REQUIRE(S(QueryServiceLockStatusW));
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceLockStatusW)(f.scm, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(QUERY_SERVICE_LOCK_STATUSW));
    BYTE *b = malloc(need);
    CHECK(S(QueryServiceLockStatusW)(f.scm, (void *)b, need, &need));
    QUERY_SERVICE_LOCK_STATUSW *q = (void *)b;
    CHECK(q->fIsLocked == 0);
    free(b);
    fx_end(&f);
}

/* ---------------- notifications ---------------- */

static volatile LONG g_cb_hits;
static VOID CALLBACK notify_cb(PVOID p) { InterlockedIncrement(&g_cb_hits); (void)p; }

static int wait_hits(LONG target, DWORD ms) {
    DWORD t0 = GetTickCount();
    while (g_cb_hits < target && GetTickCount() - t0 < ms) SleepEx(50, TRUE);
    return g_cb_hits >= target;
}

static SERVICE_NOTIFYW g_nt;

TEST(dx_svc_notify_created_on_scm) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    REQUIRE(S(NotifyServiceStatusChangeW));
    g_cb_hits = 0; memset(&g_nt, 0, sizeof g_nt);
    g_nt.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
    g_nt.pfnNotifyCallback = notify_cb;
    g_nt.pContext = &f;
    DWORD r = S(NotifyServiceStatusChangeW)(f.scm, SERVICE_NOTIFY_CREATED, &g_nt);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == ERROR_SUCCESS) {
        SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECK(s != NULL); f.svc = s;
        int got = wait_hits(1, 8000);
        CHECK(got);
        if (got) {
            CHECK(g_nt.dwNotificationStatus == ERROR_SUCCESS);
            CHECK(g_nt.dwNotificationTriggered & SERVICE_NOTIFY_CREATED);
            if (g_nt.pszServiceNames) {
                int match = 0; const WCHAR *p = g_nt.pszServiceNames;
                while (*p) { t_note("created notification name: %ls", p); if (!lstrcmpiW(p[0] == L'/' ? p + 1 : p, f.name)) match = 1; /* created names carry a leading '/' */ p += lstrlenW(p) + 1; }
                CHECK(match);
                LocalFree(g_nt.pszServiceNames);
            } else CHECK(0);
        }
    }
    fx_end(&f);
}

TEST(dx_svc_notify_delete_pending) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(NotifyServiceStatusChangeW));
    SC_HANDLE watcher = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(watcher != NULL);
    if (watcher) {
        g_cb_hits = 0; memset(&g_nt, 0, sizeof g_nt);
        g_nt.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
        g_nt.pfnNotifyCallback = notify_cb;
        DWORD r = S(NotifyServiceStatusChangeW)(watcher, SERVICE_NOTIFY_DELETE_PENDING, &g_nt);
        EXPECT_EQ(r, ERROR_SUCCESS);
        if (r == ERROR_SUCCESS) {
            CHECK(S(DeleteService)(f.svc));
            int got = wait_hits(1, 8000);
            CHECK(got);
            if (got) {
                CHECK(g_nt.dwNotificationStatus == ERROR_SUCCESS);
                CHECK(g_nt.dwNotificationTriggered & SERVICE_NOTIFY_DELETE_PENDING);
            }
        }
        S(CloseServiceHandle)(watcher);
    }
    fx_end(&f);
}

TEST(dx_svc_notify_invalid_args) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(NotifyServiceStatusChangeW));
    static SERVICE_NOTIFYW n;
    memset(&n, 0, sizeof n); n.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; n.pfnNotifyCallback = notify_cb;
    DWORD r = S(NotifyServiceStatusChangeW)(NULL, SERVICE_NOTIFY_STOPPED, &n);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); /* Win10: NULL handle -> 87 */
    r = S(NotifyServiceStatusChangeW)(f.scm, SERVICE_NOTIFY_STOPPED, &n);
    t_note("STOPPED mask on SCM handle -> %lu", r);
    memset(&n, 0, sizeof n); n.dwVersion = 99; n.pfnNotifyCallback = notify_cb;
    r = S(NotifyServiceStatusChangeW)(f.svc, SERVICE_NOTIFY_RUNNING, &n);
    t_note("bad notify version -> %lu", r);
    CHECK(r != ERROR_SUCCESS);
    memset(&n, 0, sizeof n); n.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE; n.pfnNotifyCallback = notify_cb;
    r = S(NotifyServiceStatusChangeW)(f.svc, 0, &n);
    t_note("zero mask -> %lu", r);
    CHECK(r != ERROR_SUCCESS);
    fx_end(&f);
}

TEST(dx_svc_notify_a_variant_exported) {
    CHECK(t_proc_in("sechost.dll", "NotifyServiceStatusChangeA") != NULL);
    CHECK(t_proc_in("sechost.dll", "NotifyServiceStatusChangeW") != NULL);
    CHECK(t_proc_in("sechost.dll", "NotifyServiceStatusChange") != NULL);
}

/* SubscribeServiceChangeNotifications is not in the SDK headers used here */
typedef VOID (CALLBACK *sc_notify_cb_t)(DWORD, PVOID);
UDECL(DWORD, SubscribeServiceChangeNotifications, (SC_HANDLE, int, sc_notify_cb_t, PVOID, PVOID *));
UDECL(VOID, UnsubscribeServiceChangeNotifications, (PVOID));

static volatile LONG g_sub_hits; static volatile DWORD g_sub_last;
static VOID CALLBACK sub_cb(DWORD notify, PVOID ctx) { g_sub_last = notify; InterlockedIncrement(&g_sub_hits); (void)ctx; }

TEST(dx_svc_subscribe_database_change) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    if (t_is_wine() || !UN(SubscribeServiceChangeNotifications) || !UN(UnsubscribeServiceChangeNotifications)) { t_skip("export missing or unimplemented under wine"); fx_end(&f); return; }
    PVOID reg = NULL; g_sub_hits = 0;
    DWORD r = UN(SubscribeServiceChangeNotifications)(f.scm, 0 /* SC_EVENT_DATABASE_CHANGE */, sub_cb, NULL, &reg);
    EXPECT_EQ(r, ERROR_SUCCESS);
    CHECK(reg != NULL);
    if (r == ERROR_SUCCESS) {
        SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECK(s != NULL); f.svc = s;
        DWORD t0 = GetTickCount();
        while (!g_sub_hits && GetTickCount() - t0 < 8000) Sleep(50);
        t_note("database change callbacks after create: %ld (last notify 0x%lx)", g_sub_hits, g_sub_last);
        CHECK(g_sub_hits > 0);
        if (reg) UN(UnsubscribeServiceChangeNotifications)(reg);
    }
    fx_end(&f);
}

TEST(dx_svc_subscribe_invalid_args) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    if (t_is_wine() || !UN(SubscribeServiceChangeNotifications)) { t_skip("export missing or unimplemented under wine"); fx_end(&f); return; }
    PVOID reg = NULL;
    DWORD r = UN(SubscribeServiceChangeNotifications)(f.svc, 99, sub_cb, NULL, &reg);
    CHECK(r != ERROR_SUCCESS);
    t_note("bad event type -> %lu", r);
    if (r == ERROR_SUCCESS && reg) UN(UnsubscribeServiceChangeNotifications)(reg);
    reg = NULL;
    r = UN(SubscribeServiceChangeNotifications)(NULL, 2, sub_cb, NULL, &reg);
    CHECK(r != ERROR_SUCCESS);
    t_note("NULL handle -> %lu", r);
    if (r == ERROR_SUCCESS && reg) UN(UnsubscribeServiceChangeNotifications)(reg);
    fx_end(&f);
}

TEST(dx_svc_subscribe_status_change_stopped_service) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    if (t_is_wine() || !UN(SubscribeServiceChangeNotifications)) { t_skip("export missing or unimplemented under wine"); fx_end(&f); return; }
    PVOID reg = NULL; g_sub_hits = 0;
    DWORD r = UN(SubscribeServiceChangeNotifications)(f.svc, 2 /* SC_EVENT_STATUS_CHANGE */, sub_cb, NULL, &reg);
    t_note("status change subscribe -> %lu", r);
    if (r == ERROR_SUCCESS && reg) {
        Sleep(200);
        t_note("callbacks without activity: %ld", g_sub_hits);
        UN(UnsubscribeServiceChangeNotifications)(reg);
    }
    fx_end(&f);
}

TEST(dx_svc_exported_internal_entry_points) {
    /* the private I_Sc* / Wait* exports sechost provides on Win10 */
    static const char *const names[] = { "I_ScBroadcastServiceControlMessage", "I_ScPnPGetServiceName", "I_ScQueryServiceConfig",
        "I_ScSetServiceBitsW", "WaitServiceState", "QueryServiceDynamicInformation", "GetServiceRegistryStateKey",
        "GetServiceDirectory", "CreateServiceEx", "ControlServiceExW", "SubscribeServiceChangeNotifications" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC p = t_proc_in("sechost.dll", names[i]);
        CHECKF(p != NULL, "sechost!%s missing", names[i]);
    }
}
