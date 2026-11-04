#include "svc.h"
/* CreateServiceW/A, OpenServiceW/A, OpenSCManager, name mapping, registry side effects. */

TEST(dx_svc_scm_open_close) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = fx_scm(SC_MANAGER_CONNECT);
    if (!h) { t_skip("SCM unavailable %lu", GetLastError()); return; }
    CHECK(h != NULL);
    CHECK(S(CloseServiceHandle)(h));
    /* handle is dead now */
    SET_ERR(0);
    CHECK(!S(CloseServiceHandle)(h)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(dx_svc_close_null) {
    REQUIRE(S(CloseServiceHandle));
    SET_ERR(0);
    CHECK(!S(CloseServiceHandle)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(dx_svc_scm_open_a_w_parity) {
    REQUIRE(S(OpenSCManagerA)); REQUIRE(S(OpenSCManagerW));
    SC_HANDLE a = S(OpenSCManagerA)(NULL, "ServicesActive", SC_MANAGER_CONNECT);
    SC_HANDLE w = S(OpenSCManagerW)(NULL, L"ServicesActive", SC_MANAGER_CONNECT);
    if (!a && !w) { t_skip("SCM unavailable"); return; }
    CHECK(a != NULL); CHECK(w != NULL);
    if (a) S(CloseServiceHandle)(a);
    if (w) S(CloseServiceHandle)(w);
}

TEST(dx_svc_scm_bad_database) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE ok = fx_scm(SC_MANAGER_CONNECT);
    if (!ok) { t_skip("SCM unavailable"); return; }
    S(CloseServiceHandle)(ok);
    SET_ERR(0);
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"olet_NoSuchDatabase", SC_MANAGER_CONNECT);
    CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_NAME);
    if (h) S(CloseServiceHandle)(h);
    /* the failed-services database is a documented name but is not openable for this purpose: just note */
    h = S(OpenSCManagerW)(NULL, L"ServicesFailed", SC_MANAGER_CONNECT);
    t_note("OpenSCManager(ServicesFailed) -> %p err %lu", (void *)h, GetLastError());
    if (h) S(CloseServiceHandle)(h);
}

TEST(dx_svc_scm_database_name_case) {
    REQUIRE(S(OpenSCManagerW));
    SC_HANDLE h = S(OpenSCManagerW)(NULL, L"SERVICESACTIVE", SC_MANAGER_CONNECT);
    t_note("OpenSCManager(SERVICESACTIVE) -> %p err %lu", (void *)h, GetLastError());
    if (h) S(CloseServiceHandle)(h);
}

TEST(dx_svc_create_basic) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL);
    f.svc = s;
    if (s) {
        SERVICE_STATUS st;
        memset(&st, 0xAA, sizeof st);
        CHECK(S(QueryServiceStatus)(s, &st));
        EXPECT_EQ(st.dwServiceType, SERVICE_WIN32_OWN_PROCESS);
        EXPECT_EQ(st.dwCurrentState, SERVICE_STOPPED);
        EXPECT_EQ(st.dwControlsAccepted, 0);
        EXPECT_EQ(st.dwWin32ExitCode, ERROR_SERVICE_NEVER_STARTED);
        EXPECT_EQ(st.dwServiceSpecificExitCode, 0);
        EXPECT_EQ(st.dwCheckPoint, 0);
        EXPECT_EQ(st.dwWaitHint, 0);
    }
    fx_end(&f);
}

TEST(dx_svc_create_a_variant) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    REQUIRE(S(CreateServiceA));
    char disp[128]; wsprintfA(disp, "olet display A %lu", GetCurrentProcessId());
    SC_HANDLE s = S(CreateServiceA)(f.scm, f.nameA, disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, "C:\\olet_nonexistent_dir\\a.exe", NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL);
    f.svc = s;
    if (s) {
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
        CHECK(b != NULL);
        if (b) {
            CHECK(!lstrcmpW(c->lpBinaryPathName, L"C:\\olet_nonexistent_dir\\a.exe"));
            WCHAR wd[128]; MultiByteToWideChar(CP_ACP, 0, disp, -1, wd, 128);
            CHECK(!lstrcmpW(c->lpDisplayName, wd));
            free(b);
        }
        SET_ERR(0);
        SC_HANDLE d = S(CreateServiceA)(f.scm, f.nameA, disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, "C:\\x.exe", NULL, NULL, NULL, NULL, NULL);
        CHECK(d == NULL); EXPECT_ERR(ERROR_SERVICE_EXISTS);
        if (d) S(CloseServiceHandle)(d);
    }
    fx_end(&f);
}

TEST(dx_svc_create_duplicate_name) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SET_ERR(0);
    SC_HANDLE d = S(CreateServiceW)(f.scm, f.name, L"olet other display", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(d == NULL); EXPECT_ERR(ERROR_SERVICE_EXISTS);
    if (d) S(CloseServiceHandle)(d);
    fx_end(&f);
}

TEST(dx_svc_create_duplicate_name_case_insensitive) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR up[96]; lstrcpyW(up, f.name); CharUpperW(up);
    SET_ERR(0);
    SC_HANDLE d = S(CreateServiceW)(f.scm, up, L"olet other display ci", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(d == NULL); EXPECT_ERR(ERROR_SERVICE_EXISTS);
    if (d) S(CloseServiceHandle)(d);
    /* OpenService is case-insensitive too */
    SC_HANDLE o = S(OpenServiceW)(f.scm, up, SERVICE_QUERY_STATUS);
    CHECK(o != NULL);
    if (o) S(CloseServiceHandle)(o);
    fx_end(&f);
}

TEST(dx_svc_create_duplicate_display_name) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR other[160]; wsprintfW(other, L"%s_dup_svc", f.name);
    SET_ERR(0);
    SC_HANDLE d = S(CreateServiceW)(f.scm, other, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    /* Win10: only a clash with another KEY name is refused; a duplicate display name is accepted */
    CHECK(d != NULL);
    fx_del(&f, other, d);
    fx_end(&f);
}

TEST(dx_svc_create_display_name_equals_other_key_name) {
    /* a display name that equals another service's KEY name is also a duplicate */
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR other[160]; wsprintfW(other, L"%s_dk_svc", f.name);
    SET_ERR(0);
    SC_HANDLE d = S(CreateServiceW)(f.scm, other, f.name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(d == NULL);
    t_note("display==other key name -> err %lu (1078 expected)", GetLastError());
    EXPECT_ERR(ERROR_DUPLICATE_SERVICE_NAME);
    fx_del(&f, other, d);
    fx_end(&f);
}

TEST(dx_svc_create_invalid_name_chars) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    static const WCHAR *const bad[] = { L"olet_a/b_svc", L"olet_a\\b_svc", L"/olet_svc", L"olet_svc\\" };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        SC_HANDLE s = S(CreateServiceW)(f.scm, bad[i], L"olet bad name display", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECKF(s == NULL, "name %d accepted", i);
        EXPECT_ERR(ERROR_INVALID_NAME);
        if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    }
    fx_end(&f);
}

TEST(dx_svc_create_empty_name) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, L"", NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s == NULL);
    t_note("empty service name -> err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_NAME);
    if (s) S(CloseServiceHandle)(s);
    fx_end(&f);
}

TEST(dx_svc_create_name_too_long) {
    /* documented limit: 256 chars */
    fx_t f; if (!fx_begin(&f, 0)) return;
    WCHAR *n = malloc(300 * 2);
    for (int i = 0; i < 257; i++) n[i] = (i < 5) ? L"olet_"[i] : L'x';
    n[257 - 4] = L'_'; n[257 - 3] = L's'; n[257 - 2] = L'v'; n[257 - 1] = L'c'; n[257] = 0;
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, n, NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s == NULL);
    t_note("257-char name -> err %lu", GetLastError());
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    free(n);
    fx_end(&f);
}

TEST(dx_svc_create_bad_start_type) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    static const DWORD bad[] = { 5, 6, 7, 0x100, 0xFFFFFFFE };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            bad[i], SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECKF(s == NULL, "start type 0x%lx accepted", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    }
    CHECK(!reg_exists(f.name));
    fx_end(&f);
}

TEST(dx_svc_create_bad_error_control) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    static const DWORD bad[] = { 4, 5, 0x100, 0xFFFFFFFE };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, bad[i], FX_BIN, NULL, NULL, NULL, NULL, NULL);
        /* not verified in a decomp: observe only */
        t_note("error control 0x%lx -> %p err %lu", bad[i], (void *)s, GetLastError());
        if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    }
    fx_end(&f);
}

TEST(dx_svc_create_bad_service_type) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    /* SERVICE_ADAPTER (4) is reserved; 0 and combos with no valid meaning. WIN32_SHARE_PROCESS|INTERACTIVE (0x60) is accepted on Win10 */
    static const DWORD bad[] = { SERVICE_ADAPTER, 0, 0x3, 0x7FFFFFFF };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, bad[i],
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECKF(s == NULL, "service type 0x%lx accepted", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    }
    fx_end(&f);
}

TEST(dx_svc_create_adapter_type) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_ADAPTER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    fx_end(&f);
}

TEST(dx_svc_create_valid_start_types) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    /* user-mode services may be AUTO or DEMAND or DISABLED; BOOT/SYSTEM start are driver-only */
    static const DWORD ok[] = { SERVICE_AUTO_START, SERVICE_DEMAND_START, SERVICE_DISABLED };
    for (int i = 0; i < 3; i++) {
        WCHAR nm[160]; wsprintfW(nm, L"%s_st%d_svc", f.name, i);
        WCHAR dn[160]; wsprintfW(dn, L"%s st%d", f.disp, i);
        SC_HANDLE s = S(CreateServiceW)(f.scm, nm, dn, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            ok[i], SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECKF(s != NULL, "start %lu err %lu", ok[i], GetLastError());
        if (s) {
            QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
            if (b) { EXPECT_EQ(c->dwStartType, ok[i]); free(b); }
            DWORD reg = 99; CHECK(reg_dword(nm, L"Start", &reg)); EXPECT_EQ(reg, ok[i]);
        }
        fx_del(&f, nm, s);
    }
    fx_end(&f);
}

TEST(dx_svc_create_win32_share_process) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_SHARE_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL); f.svc = s;
    if (s) {
        SERVICE_STATUS st; CHECK(S(QueryServiceStatus)(s, &st));
        EXPECT_EQ(st.dwServiceType, SERVICE_WIN32_SHARE_PROCESS);
        DWORD t = 0; CHECK(reg_dword(f.name, L"Type", &t)); EXPECT_EQ(t, SERVICE_WIN32_SHARE_PROCESS);
    }
    fx_end(&f);
}

TEST(dx_svc_create_null_binary_path) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(s == NULL);
    t_note("NULL binary path (win32 service) -> err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_ADDRESS); /* Win10: 487 */
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    fx_end(&f);
}

TEST(dx_svc_create_null_scm) {
    REQUIRE(S(CreateServiceW));
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(NULL, L"olet_null_scm_svc", NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (s) S(CloseServiceHandle)(s);
}

TEST(dx_svc_create_with_service_handle_as_scm) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR other[160]; wsprintfW(other, L"%s_wh_svc", f.name);
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.svc, other, NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_del(&f, other, s);
    fx_end(&f);
}

TEST(dx_svc_create_requires_create_right) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE ro = fx_scm(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    CHECK(ro != NULL);
    if (ro) {
        SET_ERR(0);
        SC_HANDLE s = S(CreateServiceW)(ro, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECK(s == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
        fx_del(&f, f.name, s);
        S(CloseServiceHandle)(ro);
    }
    fx_end(&f);
}

TEST(dx_svc_create_tag_id_without_group) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    DWORD tag = 0xDEADBEEF;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, &tag, NULL, NULL, NULL);
    t_note("tag id requested without load order group -> %p err %lu tag %lu", (void *)s, GetLastError(), tag);
    f.svc = s;
    if (s) {
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
        if (b) { t_note("config tag=%lu group=%ls", c->dwTagId, c->lpLoadOrderGroup ? c->lpLoadOrderGroup : L"(null)"); free(b); }
    }
    fx_end(&f);
}

TEST(dx_svc_create_tag_id_with_group) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    /* tags are only meaningful for kernel/boot drivers: a win32 service with a group + tag request */
    DWORD tag = 0;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, L"olet_group", &tag, NULL, NULL, NULL);
    t_note("create with group+tag -> %p err %lu tag %lu", (void *)s, GetLastError(), tag);
    f.svc = s;
    if (s) {
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
        if (b) {
            CHECK(c->lpLoadOrderGroup != NULL);
            if (c->lpLoadOrderGroup) CHECK(!lstrcmpW(c->lpLoadOrderGroup, L"olet_group"));
            free(b);
        }
    }
    fx_end(&f);
}

TEST(dx_svc_create_dependencies_multisz) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    /* deps: another throw-away service plus a pre-existing one (only referenced, never touched) */
    WCHAR dep2[160], deps[400]; memset(deps, 0, sizeof deps);
    SC_HANDLE d = fx_create2(&f, L"dep", SERVICE_DEMAND_START, NULL, dep2);
    CHECK(d != NULL);
    size_t o = 0;
    lstrcpyW(deps + o, dep2); o += lstrlenW(dep2) + 1;
    lstrcpyW(deps + o, L"RpcSs"); o += 6;
    deps[o] = 0;
    WCHAR main_[160];
    SC_HANDLE s = fx_create2(&f, L"main", SERVICE_DEMAND_START, deps, main_);
    CHECKF(s != NULL, "err %lu", GetLastError());
    if (s) {
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
        CHECK(b != NULL);
        if (b) {
            const WCHAR *p = c->lpDependencies;
            CHECK(p != NULL);
            int n = 0, f1 = 0, f2 = 0;
            while (p && *p) { if (!lstrcmpiW(p, dep2)) f1 = 1; if (!lstrcmpiW(p, L"RpcSs")) f2 = 1; p += lstrlenW(p) + 1; n++; }
            EXPECT_EQ(n, 2); CHECK(f1); CHECK(f2);
            free(b);
        }
        WCHAR rv[512]; DWORD ty = 0;
        CHECK(reg_str(main_, L"DependOnService", rv, 512, &ty));
        EXPECT_EQ(ty, REG_MULTI_SZ);
    }
    fx_del(&f, main_, s);
    fx_del(&f, dep2, d);
    fx_end(&f);
}

TEST(dx_svc_create_dependency_on_self) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    WCHAR deps[200]; memset(deps, 0, sizeof deps); lstrcpyW(deps, f.name);
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, deps, NULL, NULL);
    CHECK(s == NULL); EXPECT_ERR(ERROR_CIRCULAR_DEPENDENCY);
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    CHECK(!reg_exists(f.name));
    fx_end(&f);
}

TEST(dx_svc_create_dependency_missing) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    WCHAR deps[200]; memset(deps, 0, sizeof deps); wsprintfW(deps, L"olet_%lu_nosuchdep_svc", GetCurrentProcessId());
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, deps, NULL, NULL);
    t_note("create with missing dependency -> %p err %lu", (void *)s, GetLastError());
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    else t_note("missing dependency rejected with %lu (1075 documented)", GetLastError());
    fx_end(&f);
}

TEST(dx_svc_create_dependency_group_syntax) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    /* "+Group" refers to a load order group; a nonexistent group is accepted at creation */
    WCHAR deps[64]; memset(deps, 0, sizeof deps); lstrcpyW(deps, L"+olet_nogroup");
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, deps, NULL, NULL);
    t_note("group dependency -> %p err %lu", (void *)s, GetLastError());
    f.svc = s;
    fx_end(&f);
}

TEST(dx_svc_create_account_localsystem_default) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    CHECK(b != NULL);
    if (b) {
        CHECK(c->lpServiceStartName != NULL);
        if (c->lpServiceStartName) CHECK(!lstrcmpiW(c->lpServiceStartName, L"LocalSystem"));
        free(b);
    }
    WCHAR on[64]; CHECK(reg_str(f.name, L"ObjectName", on, 64, NULL));
    CHECK(!lstrcmpiW(on, L"LocalSystem"));
    fx_end(&f);
}

TEST(dx_svc_create_account_explicit_localsystem) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, L"LocalSystem", NULL);
    CHECK(s != NULL); f.svc = s;
    if (s) {
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
        if (b) { CHECK(!lstrcmpiW(c->lpServiceStartName, L"LocalSystem")); free(b); }
    }
    fx_end(&f);
}

TEST(dx_svc_create_account_builtin_names) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    static const WCHAR *const acc[] = { L"NT AUTHORITY\\LocalService", L"NT AUTHORITY\\NetworkService" };
    for (int i = 0; i < 2; i++) {
        WCHAR nm[160]; wsprintfW(nm, L"%s_acc%d_svc", f.name, i);
        WCHAR dn[160]; wsprintfW(dn, L"%s acc%d", f.disp, i);
        SC_HANDLE s = S(CreateServiceW)(f.scm, nm, dn, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, acc[i], NULL);
        CHECKF(s != NULL, "account %d err %lu", i, GetLastError());
        if (s) {
            QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
            if (b) { CHECK(!lstrcmpiW(c->lpServiceStartName, acc[i])); free(b); }
        }
        fx_del(&f, nm, s);
    }
    fx_end(&f);
}

TEST(dx_svc_create_account_unknown_user) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, L".\\olet_no_such_user_xyz", L"pw");
    CHECK(s == NULL);
    t_note("unknown account -> err %lu (1057 ERROR_INVALID_SERVICE_ACCOUNT expected)", GetLastError());
    EXPECT_ERR(ERROR_INVALID_SERVICE_ACCOUNT);
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    fx_end(&f);
}

TEST(dx_svc_create_interactive_requires_localsystem) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL); f.svc = s;
    if (s) { SERVICE_STATUS st; CHECK(S(QueryServiceStatus)(s, &st)); EXPECT_EQ(st.dwServiceType, SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS); }
    fx_end(&f);
    if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, L"NT AUTHORITY\\LocalService", NULL);
    CHECK(s == NULL); t_note("interactive + LocalService -> err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (s) { S(DeleteService)(s); S(CloseServiceHandle)(s); }
    fx_end(&f);
}

TEST(dx_svc_create_desired_access_returned) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    /* the handle is opened with the requested access: query-only handle cannot change config */
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_QUERY_STATUS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL);
    if (s) {
        SET_ERR(0);
        CHECK(!S(DeleteService)(s)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        DWORD need;
        CHECK(!S(QueryServiceConfigW)(s, NULL, 0, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(s);
    }
    fx_end(&f);
}

TEST(dx_svc_open_basic_and_missing) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(o != NULL); if (o) S(CloseServiceHandle)(o);
    WCHAR miss[96]; wsprintfW(miss, L"olet_%lu_missing_svc", GetCurrentProcessId());
    SET_ERR(0);
    o = S(OpenServiceW)(f.scm, miss, SERVICE_QUERY_STATUS);
    CHECK(o == NULL); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    SET_ERR(0);
    o = S(OpenServiceW)(NULL, f.name, SERVICE_QUERY_STATUS);
    CHECK(o == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    o = S(OpenServiceW)(f.svc, f.name, SERVICE_QUERY_STATUS);
    CHECK(o == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_open_null_and_empty_name) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SET_ERR(0);
    SC_HANDLE o = S(OpenServiceW)(f.scm, L"", SERVICE_QUERY_STATUS);
    CHECK(o == NULL); t_note("OpenService(\"\") -> err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_NAME);
    if (o) S(CloseServiceHandle)(o);
    SET_ERR(0);
    o = S(OpenServiceW)(f.scm, L"a/b", SERVICE_QUERY_STATUS);
    CHECK(o == NULL); t_note("OpenService(a/b) -> err %lu", GetLastError());
    if (o) S(CloseServiceHandle)(o);
    fx_end(&f);
}

TEST(dx_svc_open_a_variant) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(OpenServiceA));
    SC_HANDLE o = S(OpenServiceA)(f.scm, f.nameA, SERVICE_QUERY_STATUS);
    CHECK(o != NULL); if (o) S(CloseServiceHandle)(o);
    SET_ERR(0);
    o = S(OpenServiceA)(f.scm, "olet_no_such_svc_zz", SERVICE_QUERY_STATUS);
    CHECK(o == NULL); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    fx_end(&f);
}

TEST(dx_svc_open_access_denied_on_scm_rights) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE c = fx_scm(SC_MANAGER_CONNECT);
    CHECK(c != NULL);
    if (c) {
        /* CONNECT-only SCM handle can still open services (access is checked against the service SD) */
        SC_HANDLE o = S(OpenServiceW)(c, f.name, SERVICE_QUERY_STATUS);
        CHECK(o != NULL); if (o) S(CloseServiceHandle)(o);
        S(CloseServiceHandle)(c);
    }
    fx_end(&f);
}

TEST(dx_svc_open_zero_access) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = S(OpenServiceW)(f.scm, f.name, 0);
    t_note("OpenService(access 0) -> %p err %lu", (void *)o, GetLastError());
    if (o) S(CloseServiceHandle)(o);
    fx_end(&f);
}

TEST(dx_svc_name_mapping_display_and_key) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(GetServiceDisplayNameW)); REQUIRE(S(GetServiceKeyNameW));
    WCHAR buf[256]; DWORD n = 256;
    CHECK(S(GetServiceDisplayNameW)(f.scm, f.name, buf, &n));
    CHECK(!lstrcmpW(buf, f.disp));
    EXPECT_EQ(n, lstrlenW(f.disp));
    n = 256;
    CHECK(S(GetServiceKeyNameW)(f.scm, f.disp, buf, &n));
    CHECK(!lstrcmpW(buf, f.name));
    EXPECT_EQ(n, lstrlenW(f.name));
    fx_end(&f);
}

TEST(dx_svc_name_mapping_case_insensitive) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(GetServiceDisplayNameW)); REQUIRE(S(GetServiceKeyNameW));
    WCHAR up[160], buf[256]; DWORD n = 256;
    lstrcpyW(up, f.disp); CharUpperW(up);
    SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(f.scm, up, buf, &n);
    CHECK(r);
    if (r) CHECK(!lstrcmpW(buf, f.name));
    lstrcpyW(up, f.name); CharUpperW(up); n = 256;
    r = S(GetServiceDisplayNameW)(f.scm, up, buf, &n);
    CHECK(r);
    if (r) CHECK(!lstrcmpW(buf, f.disp));
    fx_end(&f);
}

TEST(dx_svc_name_mapping_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(GetServiceDisplayNameW));
    DWORD len = lstrlenW(f.disp);
    WCHAR buf[256];
    /* NULL buffer, size 0: size query */
    DWORD n = 0;
    SET_ERR(0);
    CHECK(!S(GetServiceDisplayNameW)(f.scm, f.name, NULL, &n)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(n >= len);
    if (n != len) t_note("size query returned %lu for length %lu", n, len);
    /* exact (len, no room for the terminator) */
    n = len; buf[0] = 0x7777;
    SET_ERR(0);
    CHECK(!S(GetServiceDisplayNameW)(f.scm, f.name, buf, &n)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    /* len+1 fits */
    n = len + 1;
    CHECK(S(GetServiceDisplayNameW)(f.scm, f.name, buf, &n));
    CHECK(!lstrcmpW(buf, f.disp));
    EXPECT_EQ(n, len);
    /* one too small */
    n = 1;
    SET_ERR(0);
    CHECK(!S(GetServiceDisplayNameW)(f.scm, f.name, buf, &n)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    fx_end(&f);
}

TEST(dx_svc_name_mapping_missing) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    REQUIRE(S(GetServiceDisplayNameW)); REQUIRE(S(GetServiceKeyNameW));
    WCHAR buf[256]; DWORD n = 256;
    SET_ERR(0);
    CHECK(!S(GetServiceKeyNameW)(f.scm, L"olet no such display name zz", buf, &n)); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    n = 256;
    SET_ERR(0);
    CHECK(!S(GetServiceDisplayNameW)(f.scm, L"olet_no_such_key_zz", buf, &n)); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    n = 256;
    SET_ERR(0);
    CHECK(!S(GetServiceDisplayNameW)(NULL, f.name, buf, &n)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_name_mapping_no_display_name) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL); f.svc = s;
    WCHAR buf[256]; DWORD n = 256;
    /* when no DisplayName is configured, the display name is the key name */
    CHECK(S(GetServiceDisplayNameW)(f.scm, f.name, buf, &n));
    CHECK(!lstrcmpW(buf, f.name));
    WCHAR rv[8]; CHECK(!reg_str(f.name, L"DisplayName", rv, 8, NULL));
    fx_end(&f);
}

TEST(dx_svc_name_mapping_a_variant) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    typedef BOOL (WINAPI *pfnA)(SC_HANDLE, LPCSTR, LPSTR, LPDWORD);
    pfnA gd = (pfnA)svc_proc("GetServiceDisplayNameA"), gk = (pfnA)svc_proc("GetServiceKeyNameA");
    if (!gd || !gk) { t_note("A variants not exported by sechost/advapi32"); fx_end(&f); return; }
    char buf[256]; DWORD n = 256;
    CHECK(gd(f.scm, f.nameA, buf, &n));
    char want[128]; WideCharToMultiByte(CP_ACP, 0, f.disp, -1, want, 128, NULL, NULL);
    EXPECT_STR(buf, want);
    n = 256;
    CHECK(gk(f.scm, want, buf, &n));
    EXPECT_STR(buf, f.nameA);
    fx_end(&f);
}

TEST(dx_svc_registry_side_effects) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD v = 99;
    CHECK(reg_exists(f.name));
    CHECK(reg_dword(f.name, L"Type", &v)); EXPECT_EQ(v, SERVICE_WIN32_OWN_PROCESS);
    CHECK(reg_dword(f.name, L"Start", &v)); EXPECT_EQ(v, SERVICE_DEMAND_START);
    CHECK(reg_dword(f.name, L"ErrorControl", &v)); EXPECT_EQ(v, SERVICE_ERROR_NORMAL);
    WCHAR s[512]; DWORD ty = 0;
    CHECK(reg_str(f.name, L"ImagePath", s, 512, &ty));
    CHECK(!lstrcmpW(s, FX_BIN));
    CHECKF(ty == REG_EXPAND_SZ || ty == REG_SZ, "ImagePath type %lu", ty);
    t_note("ImagePath registry type %lu (REG_EXPAND_SZ=2)", ty);
    CHECK(reg_str(f.name, L"DisplayName", s, 512, &ty)); CHECK(!lstrcmpW(s, f.disp)); EXPECT_EQ(ty, REG_SZ);
    CHECK(reg_str(f.name, L"ObjectName", s, 512, &ty)); CHECK(!lstrcmpiW(s, L"LocalSystem"));
    fx_end(&f);
    { int i; for (i = 0; i < 100 && reg_exists(f.name); i++) Sleep(100); CHECK(!reg_exists(f.name)); } /* removal is asynchronous */
}

TEST(dx_svc_registry_key_removed_on_delete) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(reg_exists(f.name));
    CHECK(S(DeleteService)(f.svc));
    S(CloseServiceHandle)(f.svc); f.svc = NULL;
    /* last handle closed: key is gone (removal is asynchronous on Win10) */
    { int i; for (i = 0; i < 100 && reg_exists(f.name); i++) Sleep(100);
      t_note("registry key removed after %d ms", i * 100); CHECK(!reg_exists(f.name)); }
    fx_end(&f);
}

TEST(dx_svc_registry_env_in_imagepath_not_expanded) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, L"%SystemRoot%\\olet_nonexistent\\x.exe", NULL, NULL, NULL, NULL, NULL);
    CHECK(s != NULL); f.svc = s;
    if (s) {
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
        if (b) {
            /* Win10: QueryServiceConfig returns the ImagePath REG_EXPAND_SZ value already expanded; the registry value is raw */
            WCHAR ex[MAX_PATH]; ExpandEnvironmentStringsW(L"%SystemRoot%\\olet_nonexistent\\x.exe", ex, MAX_PATH);
            t_note("QueryServiceConfig binary path: %ls", c->lpBinaryPathName);
            CHECK(!lstrcmpiW(c->lpBinaryPathName, ex)); free(b);
        }
        WCHAR p[260]; DWORD ty = 0;
        CHECK(reg_str(f.name, L"ImagePath", p, 260, &ty));
        CHECK(!lstrcmpW(p, L"%SystemRoot%\\olet_nonexistent\\x.exe"));
    }
    fx_end(&f);
}

TEST(dx_svc_error_control_values_roundtrip) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    static const DWORD ec[] = { SERVICE_ERROR_IGNORE, SERVICE_ERROR_NORMAL, SERVICE_ERROR_SEVERE, SERVICE_ERROR_CRITICAL };
    for (int i = 0; i < 4; i++) {
        WCHAR nm[160]; wsprintfW(nm, L"%s_ec%d_svc", f.name, i);
        WCHAR dn[160]; wsprintfW(dn, L"%s ec%d", f.disp, i);
        SC_HANDLE s = S(CreateServiceW)(f.scm, nm, dn, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START, ec[i], FX_BIN, NULL, NULL, NULL, NULL, NULL);
        CHECKF(s != NULL, "ec %lu err %lu", ec[i], GetLastError());
        if (s) {
            QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(s, &c);
            if (b) { EXPECT_EQ(c->dwErrorControl, ec[i]); free(b); }
            DWORD v = 99; CHECK(reg_dword(nm, L"ErrorControl", &v)); EXPECT_EQ(v, ec[i]);
        }
        fx_del(&f, nm, s);
    }
    fx_end(&f);
}

TEST(dx_svc_create_ex_exported) {
    REQUIRE(S(OpenSCManagerW));
    FARPROC p = t_proc_in("sechost.dll", "CreateServiceEx");
    CHECK(p != NULL);
}

TEST(dx_svc_create_many_unique) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE h[6]; WCHAR nm[6][160];
    for (int i = 0; i < 6; i++) {
        WCHAR suf[8]; wsprintfW(suf, L"m%d", i);
        h[i] = fx_create2(&f, suf, SERVICE_DEMAND_START, NULL, nm[i]);
        CHECK(h[i] != NULL);
    }
    for (int i = 0; i < 6; i++) fx_del(&f, nm[i], h[i]);
    fx_end(&f);
}
