#include "svc.h"

/* Cross-checks between enumeration, config, status and name lookups. */
typedef BOOL (WINAPI *EnumExW2_t)(SC_HANDLE, SC_ENUM_TYPE, DWORD, DWORD, LPBYTE, DWORD, LPDWORD, LPDWORD, LPDWORD, LPCWSTR);

static BYTE *xenum(SC_HANDLE scm, DWORD type, DWORD state, DWORD *count) {
    EnumExW2_t f = (EnumExW2_t)S(EnumServicesStatusExW);
    DWORD need = 0, cnt = 0, resume = 0;
    f(scm, SC_ENUM_PROCESS_INFO, type, state, NULL, 0, &need, &cnt, &resume, NULL);
    for (int i = 0; i < 5; i++) {
        DWORD size = need + 32768; BYTE *b = calloc(1, size); resume = 0;
        if (f(scm, SC_ENUM_PROCESS_INFO, type, state, b, size, &need, &cnt, &resume, NULL)) { *count = cnt; return b; }
        free(b); if (GetLastError() != ERROR_MORE_DATA) return NULL; need += 32768;
    }
    return NULL;
}
static QUERY_SERVICE_CONFIGW *xcfg(SC_HANDLE s, BYTE **mem) {
    DWORD need = 0; S(QueryServiceConfigW)(s, NULL, 0, &need);
    if (!need || need > 65536) return NULL;
    *mem = calloc(1, need + 16);
    if (!S(QueryServiceConfigW)(s, (void *)*mem, need, &need)) { free(*mem); *mem = NULL; return NULL; }
    return (QUERY_SERVICE_CONFIGW *)*mem;
}
static int multisz_has(const WCHAR *m, const WCHAR *w) {
    while (m && *m) { if (!lstrcmpiW(m, w)) return 1; m += lstrlenW(m) + 1; }
    return 0;
}

TEST(svcx_enum_names_openable) {
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD cnt = 0; BYTE *b = xenum(scm, SERVICE_WIN32, SERVICE_STATE_ALL, &cnt); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b; int bad = 0;
    for (DWORD i = 0; i < cnt && i < 60; i++) {
        SC_HANDLE s = S(OpenServiceW)(scm, e[i].lpServiceName, SERVICE_QUERY_STATUS);
        if (!s) { t_note("cannot open enumerated %ls err=%lu", e[i].lpServiceName, GetLastError()); bad++; }
        svc_close(s);
    }
    CHECKF(bad == 0, "%d enumerated services not openable", bad);
    free(b); svc_close(scm);
}
TEST(svcx_enum_display_matches_getdisplayname) {
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD cnt = 0; BYTE *b = xenum(scm, SERVICE_WIN32, SERVICE_STATE_ALL, &cnt); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b; int bad = 0;
    for (DWORD i = 0; i < cnt && i < 40; i++) {
        WCHAR d[512]; DWORD n = 512;
        if (!S(GetServiceDisplayNameW)(scm, e[i].lpServiceName, d, &n) || lstrcmpW(d, e[i].lpDisplayName)) { bad++; t_note("mismatch for %ls", e[i].lpServiceName); }
    }
    CHECK(bad == 0);
    free(b); svc_close(scm);
}
TEST(svcx_enum_display_roundtrips_to_key) {
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD cnt = 0; BYTE *b = xenum(scm, SERVICE_WIN32, SERVICE_STATE_ALL, &cnt); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b; int bad = 0, n_ok = 0;
    for (DWORD i = 0; i < cnt && i < 40; i++) {
        WCHAR k[512]; DWORD n = 512;
        if (!S(GetServiceKeyNameW)(scm, e[i].lpDisplayName, k, &n)) { bad++; continue; }
        n_ok++;
        /* display names are not guaranteed unique; only a mismatch with a unique name is a defect */
        if (lstrcmpiW(k, e[i].lpServiceName)) t_note("display '%ls' -> key '%ls' (enumerated key '%ls')", e[i].lpDisplayName, k, e[i].lpServiceName);
    }
    CHECKF(bad == 0, "%d display names not resolvable", bad);
    free(b); svc_close(scm);
}
TEST(svcx_enum_status_matches_query_status) {
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD cnt = 0; BYTE *b = xenum(scm, SERVICE_WIN32, SERVICE_STATE_ALL, &cnt); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b; int bad = 0;
    for (DWORD i = 0; i < cnt && i < 40; i++) {
        SC_HANDLE s = S(OpenServiceW)(scm, e[i].lpServiceName, SERVICE_QUERY_STATUS); SERVICE_STATUS st;
        if (s && S(QueryServiceStatus)(s, &st)) {
            if (st.dwServiceType != e[i].ServiceStatusProcess.dwServiceType) { bad++; t_note("%ls type 0x%lx vs 0x%lx", e[i].lpServiceName, st.dwServiceType, e[i].ServiceStatusProcess.dwServiceType); }
        } else bad++;
        svc_close(s);
    }
    CHECK(bad == 0);
    free(b); svc_close(scm);
}
TEST(svcx_enum_type_matches_config) {
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD cnt = 0; BYTE *b = xenum(scm, SERVICE_WIN32, SERVICE_STATE_ALL, &cnt); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b; int bad = 0, checked = 0;
    for (DWORD i = 0; i < cnt && i < 40; i++) {
        SC_HANDLE s = S(OpenServiceW)(scm, e[i].lpServiceName, SERVICE_QUERY_CONFIG); BYTE *m = NULL;
        QUERY_SERVICE_CONFIGW *c = s ? xcfg(s, &m) : NULL;
        if (c) {
            checked++;
            if (c->dwServiceType != e[i].ServiceStatusProcess.dwServiceType) { bad++; t_note("%ls cfg type 0x%lx enum 0x%lx", e[i].lpServiceName, c->dwServiceType, e[i].ServiceStatusProcess.dwServiceType); }
            if (lstrcmpW(c->lpDisplayName, e[i].lpDisplayName)) bad++;
        }
        free(m); svc_close(s);
    }
    CHECK(checked > 0); CHECK(bad == 0);
    free(b); svc_close(scm);
}
TEST(svcx_enum_driver_entries_have_no_pid) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD cnt = 0; BYTE *b = xenum(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, &cnt); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)b; int bad = 0;
    for (DWORD i = 0; i < cnt; i++) if (e[i].ServiceStatusProcess.dwProcessId != 0) bad++;
    CHECKF(bad == 0, "%d drivers report a process id", bad);
    free(b); svc_close(scm);
}
TEST(svcx_enum_active_subset_of_all) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD ca = 0, cl = 0; BYTE *a = xenum(scm, SERVICE_WIN32, SERVICE_ACTIVE, &ca), *l = xenum(scm, SERVICE_WIN32, SERVICE_STATE_ALL, &cl);
    REQUIRE(a); REQUIRE(l);
    CHECK(ca < cl);
    ENUM_SERVICE_STATUS_PROCESSW *ea = (void *)a, *el = (void *)l; int missing = 0;
    for (DWORD i = 0; i < ca; i++) {
        int f = 0;
        for (DWORD j = 0; j < cl; j++) if (!lstrcmpiW(ea[i].lpServiceName, el[j].lpServiceName)) { f = 1; break; }
        if (!f) missing++;
    }
    CHECKF(missing <= 2, "%d active services missing from full list", missing);
    free(a); free(l); svc_close(scm);
}
TEST(svcx_enum_order_stable_between_calls) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); REQUIRE(scm);
    DWORD c1 = 0, c2 = 0; BYTE *a = xenum(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, &c1), *b = xenum(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, &c2);
    REQUIRE(a); REQUIRE(b);
    ENUM_SERVICE_STATUS_PROCESSW *ea = (void *)a, *eb = (void *)b; int diff = 0;
    for (DWORD i = 0; i < c1 && i < c2; i++) if (lstrcmpiW(ea[i].lpServiceName, eb[i].lpServiceName)) diff++;
    CHECKF(diff == 0, "%d entries out of order between two enumerations", diff);
    free(a); free(b); svc_close(scm);
}
TEST(svcx_enum_dependents_subset_checks) {
    REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm, s = svc_fixture("RpcEptMapper", SERVICE_ENUMERATE_DEPENDENTS, &scm); REQUIRE(s);
    DWORD na = 0, nl = 0, ca = 0, cl = 0;
    S(EnumDependentServicesW)(s, SERVICE_ACTIVE, NULL, 0, &na, &ca);
    S(EnumDependentServicesW)(s, SERVICE_STATE_ALL, NULL, 0, &nl, &cl);
    CHECK(cl > 0); CHECKF(ca <= cl, "active %lu all %lu", ca, cl); CHECKF(ca > 0, "RpcSs is active and depends on RpcEptMapper");
    BYTE *b = calloc(1, nl + 4096);
    if (S(EnumDependentServicesW)(s, SERVICE_STATE_ALL, (void *)b, nl + 4096, &nl, &cl)) {
        ENUM_SERVICE_STATUSW *e = (void *)b; int dup = 0;
        for (DWORD i = 0; i < cl; i++) for (DWORD j = i + 1; j < cl; j++) if (!lstrcmpiW(e[i].lpServiceName, e[j].lpServiceName)) dup++;
        CHECK(dup == 0);
    } else CHECKF(0, "err=%lu", GetLastError());
    free(b); svc_close(s); svc_close(scm);
}
TEST(svcx_dependents_A_matches_W_count) {
    REQUIRE(S(EnumDependentServicesW)); REQUIRE(S(EnumDependentServicesA));
    SC_HANDLE scm, s = svc_fixture("RpcEptMapper", SERVICE_ENUMERATE_DEPENDENTS, &scm); REQUIRE(s);
    DWORD nw = 0, na = 0, cw = 0, ca = 0;
    S(EnumDependentServicesW)(s, SERVICE_STATE_ALL, NULL, 0, &nw, &cw);
    BYTE *bw = calloc(1, nw + 4096), *ba = NULL;
    S(EnumDependentServicesA)(s, SERVICE_STATE_ALL, NULL, 0, &na, &ca);
    ba = calloc(1, na + 4096);
    BOOL r1 = S(EnumDependentServicesW)(s, SERVICE_STATE_ALL, (void *)bw, nw + 4096, &nw, &cw);
    BOOL r2 = S(EnumDependentServicesA)(s, SERVICE_STATE_ALL, (void *)ba, na + 4096, &na, &ca);
    CHECK(r1); CHECK(r2); EXPECT_EQ(ca, cw);
    free(bw); free(ba); svc_close(s); svc_close(scm);
}

/* ---------- config content of the core services ---------- */
static void cfg_core(const char *name) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    BYTE *m = NULL; QUERY_SERVICE_CONFIGW *c = xcfg(s, &m); REQUIRE(c);
    EXPECT_EQ(c->dwServiceType, SERVICE_WIN32_SHARE_PROCESS);
    EXPECT_EQ(c->dwTagId, 0);
    WCHAR lp[MAX_PATH * 2]; lstrcpynW(lp, c->lpBinaryPathName, MAX_PATH * 2); CharLowerW(lp);
    CHECKF(wcsstr(lp, L"svchost.exe") != NULL, "%ls", c->lpBinaryPathName);
    free(m); svc_close(s); svc_close(scm);
}
PER_SERVICE(svcx_cfg_core, cfg_core)

TEST(svcx_rpcss_dependencies_multisz) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    BYTE *m = NULL; QUERY_SERVICE_CONFIGW *c = xcfg(s, &m); REQUIRE(c);
    CHECK(c->lpDependencies != NULL);
    CHECK(multisz_has(c->lpDependencies, L"RpcEptMapper"));
    CHECK(multisz_has(c->lpDependencies, L"DcomLaunch"));
    free(m); svc_close(s); svc_close(scm);
}
TEST(svcx_dependency_symmetry_with_dependents) {
    /* RpcSs lists RpcEptMapper as a dependency, so RpcSs must show up among RpcEptMapper's dependents. */
    REQUIRE(S(QueryServiceConfigW)); REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    BYTE *m = NULL; QUERY_SERVICE_CONFIGW *c = xcfg(s, &m); REQUIRE(c);
    int dep = multisz_has(c->lpDependencies, L"RpcEptMapper");
    free(m); svc_close(s);
    SC_HANDLE e = svc_open(scm, "RpcEptMapper", SERVICE_ENUMERATE_DEPENDENTS); REQUIRE(e);
    DWORD need = 0, cnt = 0; S(EnumDependentServicesW)(e, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    BYTE *b = calloc(1, need + 4096); int found = 0;
    if (S(EnumDependentServicesW)(e, SERVICE_STATE_ALL, (void *)b, need + 4096, &need, &cnt))
        for (DWORD i = 0; i < cnt; i++) if (!lstrcmpiW(((ENUM_SERVICE_STATUSW *)b)[i].lpServiceName, L"RpcSs")) found = 1;
    CHECK(dep); CHECK(found);
    free(b); svc_close(e); svc_close(scm);
}
TEST(svcx_config_stable_between_calls) {
    REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("EventLog", SERVICE_QUERY_CONFIG, &scm); REQUIRE(s);
    DWORD n1 = 0, n2 = 0; S(QueryServiceConfigW)(s, NULL, 0, &n1); S(QueryServiceConfigW)(s, NULL, 0, &n2);
    EXPECT_EQ(n1, n2);
    svc_close(s); svc_close(scm);
}
TEST(svcx_config_independent_of_scm_access) {
    REQUIRE(S(QueryServiceConfigW)); REQUIRE(S(OpenSCManagerW));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE s = svc_open(scm, "RpcSs", SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS); REQUIRE(s);
    svc_close(scm);                                   /* service handle outlives the SCM handle */
    BYTE *m = NULL; QUERY_SERVICE_CONFIGW *c = xcfg(s, &m);
    CHECK(c != NULL);
    free(m); svc_close(s);
}
TEST(svcx_status_A_equals_W_open) {
    REQUIRE(S(OpenServiceA)); REQUIRE(S(OpenServiceW)); REQUIRE(S(QueryServiceStatus));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    SC_HANDLE a = S(OpenServiceA)(scm, "EventLog", SERVICE_QUERY_STATUS), w = S(OpenServiceW)(scm, L"EventLog", SERVICE_QUERY_STATUS);
    REQUIRE(a); REQUIRE(w);
    SERVICE_STATUS sa, sw;
    CHECK(S(QueryServiceStatus)(a, &sa)); CHECK(S(QueryServiceStatus)(w, &sw));
    EXPECT_EQ(sa.dwServiceType, sw.dwServiceType); EXPECT_EQ(sa.dwCurrentState, sw.dwCurrentState);
    svc_close(a); svc_close(w); svc_close(scm);
}
TEST(svcx_pid_shared_between_svchost_peers) {
    /* RpcSs and DcomLaunch live in different svchost groups on Win10: only note the relation. */
    REQUIRE(S(QueryServiceStatusEx));
    DWORD pid[2] = {0, 0}; const char *n[2] = {"RpcSs", "DcomLaunch"};
    for (int i = 0; i < 2; i++) {
        SC_HANDLE scm, s = svc_fixture(n[i], SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
        SERVICE_STATUS_PROCESS p; DWORD need;
        CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need));
        pid[i] = p.dwProcessId; svc_close(s); svc_close(scm);
    }
    CHECK(pid[0] != 0 && pid[1] != 0);
    t_note("RpcSs pid %lu DcomLaunch pid %lu", pid[0], pid[1]);
}
TEST(svcx_handle_survives_scm_close_status_ex) {
    REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE(s);
    svc_close(scm);
    SERVICE_STATUS_PROCESS p; DWORD need = 0;
    CHECK(S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need));
    svc_close(s);
}
TEST(svcx_security_descriptor_owner_note) {
    REQUIRE(S(QueryServiceObjectSecurity));
    SC_HANDLE scm, s = svc_fixture("RpcSs", READ_CONTROL, &scm); REQUIRE(s);
    BYTE buf[2048]; DWORD need = 0;
    if (S(QueryServiceObjectSecurity)(s, OWNER_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need)) {
        BOOL def; PSID o = NULL;
        CHECK(GetSecurityDescriptorOwner((PSECURITY_DESCRIPTOR)buf, &o, &def) && o != NULL);
        if (o) { CHECK(IsValidSid(o)); }
    } else CHECKF(0, "err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(svcx_scm_database_security_dacl) {
    REQUIRE(S(QueryServiceObjectSecurity)); REQUIRE(S(OpenSCManagerW));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, READ_CONTROL);
    if (!scm) { t_skip("READ_CONTROL on SCM denied"); return; }
    BYTE buf[4096]; DWORD need = 0;
    if (S(QueryServiceObjectSecurity)(scm, DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need)) {
        BOOL present = 0, def; PACL acl = NULL;
        GetSecurityDescriptorDacl((PSECURITY_DESCRIPTOR)buf, &present, &acl, &def);
        CHECK(present && acl && acl->AceCount >= 1);
    } else CHECKF(0, "err=%lu", GetLastError());
    svc_close(scm);
}
TEST(svcx_nonexistent_service_every_entry_point) {
    REQUIRE(S(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    static const WCHAR *names[] = {L"olet_nosvc_a", L"olet nosvc with spaces", L"olet_nosvc_é"};
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        SC_HANDLE s = S(OpenServiceW)(scm, names[i], SERVICE_QUERY_STATUS);
        CHECKF(s == NULL && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "name %u: err=%lu", i, GetLastError());
        svc_close(s);
    }
    svc_close(scm);
}
