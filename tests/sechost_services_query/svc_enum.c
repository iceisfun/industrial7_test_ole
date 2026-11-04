#include "svc.h"

typedef BOOL (WINAPI *EnumExW_t)(SC_HANDLE, SC_ENUM_TYPE, DWORD, DWORD, LPBYTE, DWORD, LPDWORD, LPDWORD, LPDWORD, LPCWSTR);

static SC_HANDLE enum_scm(void) { return scm_open(SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); }

/* Fetch the whole list in one call; returns malloc'd buffer (caller frees) or NULL. */
static BYTE *enum_all(SC_HANDLE scm, DWORD type, DWORD state, const WCHAR *group, DWORD *count, DWORD *cb) {
    EnumExW_t f = (EnumExW_t)S(EnumServicesStatusExW);
    DWORD need = 0, cnt = 0, resume = 0;
    f(scm, SC_ENUM_PROCESS_INFO, type, state, NULL, 0, &need, &cnt, &resume, group);
    for (int tries = 0; tries < 5; tries++) {
        DWORD size = need + 16384;
        BYTE *buf = calloc(1, size);
        resume = 0; cnt = 0;
        if (f(scm, SC_ENUM_PROCESS_INFO, type, state, buf, size, &need, &cnt, &resume, group)) { *count = cnt; *cb = size; return buf; }
        free(buf);
        if (GetLastError() != ERROR_MORE_DATA) return NULL;
        need += 16384;
    }
    return NULL;
}
static int in_range(const BYTE *b, DWORD cb, const void *p) { return (const BYTE *)p >= b && (const BYTE *)p < b + cb; }

/* ---------- EnumServicesStatusExW ---------- */
TEST(svcenum_null_buffer_more_data) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD need = 0, cnt = 99, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > 100 * sizeof(ENUM_SERVICE_STATUS_PROCESSW));
    EXPECT_EQ(cnt, 0);
    t_note("win32 services need %lu bytes", need);
    svc_close(scm);
}
TEST(svcenum_null_buffer_more_data_A) {
    REQUIRE(S(EnumServicesStatusExA));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD need = 0, cnt = 99, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExA)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > 0);
    svc_close(scm);
}
TEST(svcenum_tiny_buffer_more_data) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[16]; DWORD need = 0, cnt = 99, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > 0); EXPECT_EQ(cnt, 0);
    svc_close(scm);
}
TEST(svcenum_win32_all_content) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt = 0, cb = 0; BYTE *buf = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cnt, &cb);
    CHECK(buf != NULL);
    if (buf) {
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
        CHECKF(cnt > 100, "count=%lu", cnt);
        int seenRpc = 0, seenDcom = 0, seenEvt = 0, bad = 0;
        for (DWORD i = 0; i < cnt; i++) {
            if (!in_range(buf, cb, e[i].lpServiceName) || !in_range(buf, cb, e[i].lpDisplayName)) { bad++; continue; }
            if (!e[i].lpServiceName[0] || !e[i].lpDisplayName[0]) bad++;
            if ((e[i].ServiceStatusProcess.dwServiceType & SERVICE_WIN32) == 0) bad++;
            if (e[i].ServiceStatusProcess.dwServiceType & SERVICE_DRIVER) bad++;
            if (e[i].ServiceStatusProcess.dwCurrentState < 1 || e[i].ServiceStatusProcess.dwCurrentState > 7) bad++;
            if (!lstrcmpiW(e[i].lpServiceName, L"RpcSs")) seenRpc = 1;
            if (!lstrcmpiW(e[i].lpServiceName, L"DcomLaunch")) seenDcom = 1;
            if (!lstrcmpiW(e[i].lpServiceName, L"EventLog")) seenEvt = 1;
        }
        EXPECT_EQ(bad, 0); CHECK(seenRpc); CHECK(seenDcom); CHECK(seenEvt);
        t_note("win32 service count = %lu", cnt);
        free(buf);
    }
    svc_close(scm);
}
TEST(svcenum_entries_pointers_after_struct_array) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt = 0, cb = 0; BYTE *buf = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cnt, &cb);
    REQUIRE(buf);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
    int bad = 0;
    for (DWORD i = 0; i < cnt; i++) {
        if ((BYTE *)e[i].lpServiceName < buf + cnt * sizeof *e) bad++;
        if ((BYTE *)e[i].lpDisplayName < buf + cnt * sizeof *e) bad++;
    }
    EXPECT_EQ(bad, 0);
    free(buf); svc_close(scm);
}
TEST(svcenum_names_unique) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt = 0, cb = 0; BYTE *buf = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cnt, &cb);
    REQUIRE(buf);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
    int dup = 0;
    for (DWORD i = 0; i < cnt && i < 1000; i++) for (DWORD j = i + 1; j < cnt && j < 1000; j++) if (!lstrcmpiW(e[i].lpServiceName, e[j].lpServiceName)) dup++;
    EXPECT_EQ(dup, 0);
    free(buf); svc_close(scm);
}
TEST(svcenum_exact_buffer_succeeds) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD need = 0, cnt = 0, resume = 0;
    S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32_OWN_PROCESS, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume, NULL);
    BYTE *buf = calloc(1, need + 4096);
    resume = 0; DWORD need2 = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32_OWN_PROCESS, SERVICE_STATE_ALL, buf, need, &need2, &cnt, &resume, NULL);
    if (!r) t_note("exact-size buffer failed err=%lu (list changed or size is a hint)", GetLastError()); else { CHECK(cnt > 0); EXPECT_EQ(resume, 0); }
    free(buf); svc_close(scm);
}
TEST(svcenum_own_process_vs_share_process) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD c1, b1, c2, b2, c3, b3;
    BYTE *own = enum_all(scm, SERVICE_WIN32_OWN_PROCESS, SERVICE_STATE_ALL, NULL, &c1, &b1);
    BYTE *shr = enum_all(scm, SERVICE_WIN32_SHARE_PROCESS, SERVICE_STATE_ALL, NULL, &c2, &b2);
    BYTE *all = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &c3, &b3);
    if (own && shr && all) {
        CHECK(c1 > 0 && c2 > 0 && c3 >= c1 && c3 >= c2);
        t_note("own %lu share %lu all %lu (user-service templates may overlap)", c1, c2, c3);
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)shr;
        for (DWORD i = 0; i < c2; i++) if (!(e[i].ServiceStatusProcess.dwServiceType & SERVICE_WIN32_SHARE_PROCESS)) { CHECKF(0, "%ls type 0x%lx", e[i].lpServiceName, e[i].ServiceStatusProcess.dwServiceType); break; }
        e = (void *)own;
        for (DWORD i = 0; i < c1; i++) if (!(e[i].ServiceStatusProcess.dwServiceType & SERVICE_WIN32_OWN_PROCESS)) { CHECKF(0, "%ls type 0x%lx", e[i].lpServiceName, e[i].ServiceStatusProcess.dwServiceType); break; }
    } else CHECK(0);
    free(own); free(shr); free(all); svc_close(scm);
}
TEST(svcenum_drivers) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt = 0, cb = 0; BYTE *buf = enum_all(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, &cnt, &cb);
    CHECK(buf != NULL);
    if (buf) {
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf; int bad = 0;
        CHECKF(cnt > 100, "driver count %lu", cnt);
        for (DWORD i = 0; i < cnt; i++) if (!(e[i].ServiceStatusProcess.dwServiceType & SERVICE_DRIVER) || (e[i].ServiceStatusProcess.dwServiceType & SERVICE_WIN32)) bad++;
        EXPECT_EQ(bad, 0);
        free(buf);
    }
    svc_close(scm);
}
TEST(svcenum_kernel_vs_filesystem_drivers) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD ck, bk, cf, bf, cd, bd;
    BYTE *k = enum_all(scm, SERVICE_KERNEL_DRIVER, SERVICE_STATE_ALL, NULL, &ck, &bk);
    BYTE *f = enum_all(scm, SERVICE_FILE_SYSTEM_DRIVER, SERVICE_STATE_ALL, NULL, &cf, &bf);
    BYTE *d = enum_all(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, &cd, &bd);
    if (k && f && d) {
        CHECKF(ck + cf <= cd + 3 && ck + cf + 20 >= cd, "kernel %lu + fs %lu vs driver %lu (recognizers/adapters differ)", ck, cf, cd);
        CHECK(ck > 0); CHECK(cf > 0);
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)f;
        for (DWORD i = 0; i < cf; i++) if (!(e[i].ServiceStatusProcess.dwServiceType & SERVICE_FILE_SYSTEM_DRIVER)) { CHECKF(0, "%ls type 0x%lx", e[i].lpServiceName, e[i].ServiceStatusProcess.dwServiceType); break; }
    } else CHECK(0);
    free(k); free(f); free(d); svc_close(scm);
}
TEST(svcenum_type_all) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD c1, b1, c2, b2, c3, b3;
    BYTE *w = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &c1, &b1);
    BYTE *d = enum_all(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, &c2, &b2);
    BYTE *a = enum_all(scm, SERVICE_TYPE_ALL, SERVICE_STATE_ALL, NULL, &c3, &b3);
    if (w && d && a) CHECKF(c3 + 3 >= c1 + c2 && c3 <= c1 + c2 + 3 + 20, "win32 %lu driver %lu all %lu", c1, c2, c3);
    else CHECK(0);
    free(w); free(d); free(a); svc_close(scm);
}
TEST(svcenum_state_active_inactive_partition) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD ca, ba, ci, bi, cl, bl;
    BYTE *a = enum_all(scm, SERVICE_WIN32, SERVICE_ACTIVE, NULL, &ca, &ba);
    BYTE *i = enum_all(scm, SERVICE_WIN32, SERVICE_INACTIVE, NULL, &ci, &bi);
    BYTE *l = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cl, &bl);
    if (a && i && l) {
        CHECKF(ca + ci >= cl - 3 && ca + ci <= cl + 3, "active %lu + inactive %lu vs all %lu", ca, ci, cl);
        CHECK(ca > 0); CHECK(ci > 0);
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)a; int seenRpc = 0;
        for (DWORD k = 0; k < ca; k++) { if (e[k].ServiceStatusProcess.dwCurrentState == SERVICE_STOPPED) { CHECKF(0, "active list holds stopped %ls", e[k].lpServiceName); break; } if (!lstrcmpiW(e[k].lpServiceName, L"RpcSs")) seenRpc = 1; }
        CHECK(seenRpc);
        e = (void *)i;
        for (DWORD k = 0; k < ci; k++) { if (e[k].ServiceStatusProcess.dwCurrentState != SERVICE_STOPPED) { CHECKF(0, "inactive list holds non-stopped %ls state %lu", e[k].lpServiceName, e[k].ServiceStatusProcess.dwCurrentState); break; } if (!lstrcmpiW(e[k].lpServiceName, L"RpcSs")) CHECK(0); }
    } else CHECK(0);
    free(a); free(i); free(l); svc_close(scm);
}
TEST(svcenum_active_entries_have_pid) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt, cb; BYTE *buf = enum_all(scm, SERVICE_WIN32, SERVICE_ACTIVE, NULL, &cnt, &cb);
    REQUIRE(buf);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf; int nopid = 0;
    for (DWORD k = 0; k < cnt; k++) if (e[k].ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING && e[k].ServiceStatusProcess.dwProcessId == 0) nopid++;
    EXPECT_EQ(nopid, 0);
    free(buf); svc_close(scm);
}
TEST(svcenum_matches_queryservicestatusex) {
    REQUIRE(S(EnumServicesStatusExW)); REQUIRE(S(QueryServiceStatusEx));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt, cb; BYTE *buf = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cnt, &cb);
    REQUIRE(buf);
    ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
    for (DWORD k = 0; k < cnt; k++) {
        if (lstrcmpiW(e[k].lpServiceName, L"RpcSs")) continue;
        SC_HANDLE s = S(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
        SERVICE_STATUS_PROCESS p; DWORD need;
        if (s && S(QueryServiceStatusEx)(s, SC_STATUS_PROCESS_INFO, (LPBYTE)&p, sizeof p, &need)) {
            EXPECT_EQ(p.dwProcessId, e[k].ServiceStatusProcess.dwProcessId);
            EXPECT_EQ(p.dwServiceType, e[k].ServiceStatusProcess.dwServiceType);
            EXPECT_EQ(p.dwCurrentState, e[k].ServiceStatusProcess.dwCurrentState);
        } else CHECK(0);
        svc_close(s);
    }
    free(buf); svc_close(scm);
}
TEST(svcenum_resume_handle_paging) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD total = 0, cb0 = 0; BYTE *all = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &total, &cb0);
    REQUIRE(all);
    enum { BUFSZ = 4096 };
    BYTE *buf = malloc(BUFSZ);
    DWORD resume = 0, seen = 0; int pages = 0, sawMore = 0, done = 0;
    static WCHAR names[3000][64];
    for (int it = 0; it < 1000 && !done; it++) {
        DWORD need = 0, cnt = 0; SET_ERR(0);
        BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, BUFSZ, &need, &cnt, &resume, NULL);
        DWORD e_ = r ? 0 : GetLastError();
        if (!r && e_ != ERROR_MORE_DATA) { CHECKF(0, "page %d err=%lu", it, e_); break; }
        CHECKF(cnt > 0, "page %d returned no entries", it);
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
        for (DWORD k = 0; k < cnt && seen < 3000; k++) { lstrcpynW(names[seen++], e[k].lpServiceName, 64); CHECK(in_range(buf, BUFSZ, e[k].lpServiceName)); }
        pages++;
        if (r) { EXPECT_EQ(resume, 0); done = 1; }
        else { sawMore = 1; CHECK(resume != 0); CHECK(need > 0); }
    }
    CHECK(done); CHECK(sawMore); CHECK(pages > 1);
    CHECKF(seen + 3 >= total && seen <= total + 3, "paged %lu vs whole %lu", seen, total);
    int dup = 0;
    for (DWORD i = 0; i < seen; i++) for (DWORD j = i + 1; j < seen; j++) if (!lstrcmpiW(names[i], names[j])) dup++;
    EXPECT_EQ(dup, 0);
    t_note("%d pages, %lu services", pages, seen);
    free(buf); free(all); svc_close(scm);
}
TEST(svcenum_resume_nonzero_beyond_end) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE *buf = malloc(65536); DWORD need = 0, cnt = 99, resume = 0x00FFFFFF; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, 65536, &need, &cnt, &resume, NULL);
    t_note("resume beyond end: r=%d err=%lu cnt=%lu resume=%lu", r, r ? 0 : GetLastError(), cnt, resume);
    free(buf); svc_close(scm);
}
TEST(svcenum_resume_pointer_null_optional) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD need = 0, cnt = 0, size = 0; BYTE *buf = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cnt, &size);
    REQUIRE(buf);
    memset(buf, 0, size); SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, size, &need, &cnt, NULL, NULL);
    CHECK(r); CHECK(cnt > 100);
    free(buf); svc_close(scm);
}
TEST(svcenum_group_empty_string) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD ca = 0, ba, cg = 0, bg;
    BYTE *a = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &ca, &ba);
    BYTE *g = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, L"", &cg, &bg);
    t_note("win32 total %lu, group \"\" %lu", ca, cg);
    CHECK(a != NULL); CHECK(g != NULL);
    if (a && g) CHECK(cg <= ca);
    free(a); free(g); svc_close(scm);
}
TEST(svcenum_group_unknown) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[4096]; DWORD need = 0, cnt = 99, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, L"olet_NoSuchGroup");
    t_note("unknown group: r=%d err=%lu cnt=%lu", r, r ? 0 : GetLastError(), cnt);
    if (r) EXPECT_EQ(cnt, 0);
    svc_close(scm);
}
TEST(svcenum_group_ndis_drivers) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cnt = 0, cb; BYTE *buf = enum_all(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, L"NDIS", &cnt, &cb);
    t_note("drivers in group NDIS: %lu", cnt);
    if (buf) {
        DWORD ca; BYTE *a = enum_all(scm, SERVICE_DRIVER, SERVICE_STATE_ALL, NULL, &ca, &cb);
        if (a) CHECK(cnt <= ca);
        free(a); free(buf);
    }
    svc_close(scm);
}
TEST(svcenum_bad_type_zero) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, 0, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
    svc_close(scm);
}
TEST(svcenum_bad_state_zero) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, 0, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
    svc_close(scm);
}
TEST(svcenum_bad_state_4) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, 4, buf, sizeof buf, &need, &cnt, &resume, NULL);
    CHECK(!r);
    t_note("state 4: err=%lu", GetLastError());
    svc_close(scm);
}
TEST(svcenum_bad_type_unknown_bits) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, 0x400, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    CHECK(!r);
    t_note("type 0x400: err=%lu", GetLastError());
    svc_close(scm);
}
TEST(svcenum_bad_infolevel_1) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, (SC_ENUM_TYPE)1, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(scm);
}
TEST(svcenum_bad_infolevel_ffff) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, (SC_ENUM_TYPE)0xFFFF, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(scm);
}
TEST(svcenum_null_scm) {
    REQUIRE(S(EnumServicesStatusExW));
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(NULL, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcenum_service_handle_as_scm) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(s, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(s); svc_close(scm);
}
TEST(svcenum_closed_scm) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm); svc_close(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcenum_scm_connect_only_denied) {
    REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(scm);
}
TEST(svcenum_A_matches_W) {
    REQUIRE(S(EnumServicesStatusExA)); REQUIRE(S(EnumServicesStatusExW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD cw, bw; BYTE *w = enum_all(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, &cw, &bw);
    DWORD need = 0, ca = 0, resume = 0;
    S(EnumServicesStatusExA)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &ca, &resume, NULL);
    BYTE *a = calloc(1, need + 16384); DWORD sa = need + 16384; resume = 0;
    BOOL r = S(EnumServicesStatusExA)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, a, sa, &need, &ca, &resume, NULL);
    CHECK(r);
    if (r && w) {
        CHECKF(ca + 3 >= cw && ca <= cw + 3, "A %lu W %lu", ca, cw);
        ENUM_SERVICE_STATUS_PROCESSA *ea = (void *)a; ENUM_SERVICE_STATUS_PROCESSW *ew = (void *)w;
        int bad = 0;
        for (DWORD i = 0; i < ca && i < cw; i++) {
            char tmp[512]; wide_to_narrow(tmp, sizeof tmp, ew[i].lpServiceName);
            if (strcmp(tmp, ea[i].lpServiceName)) bad++;
            if (!in_range(a, sa, ea[i].lpServiceName) || !in_range(a, sa, ea[i].lpDisplayName)) bad++;
        }
        EXPECT_EQ(bad, 0);
    }
    free(a); free(w); svc_close(scm);
}
TEST(svcenum_A_bad_level) {
    REQUIRE(S(EnumServicesStatusExA));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[1024]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusExA)(scm, (SC_ENUM_TYPE)5, SERVICE_WIN32, SERVICE_STATE_ALL, buf, sizeof buf, &need, &cnt, &resume, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_LEVEL);
    svc_close(scm);
}

/* ---------- EnumServicesStatusW/A (SERVICE_STATUS entries) ---------- */
TEST(svcenum_legacy_W) {
    REQUIRE(S(EnumServicesStatusW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD need = 0, cnt = 99, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusW)(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > 0);
    BYTE *buf = calloc(1, need + 16384); DWORD size = need + 16384; resume = 0;
    r = S(EnumServicesStatusW)(scm, SERVICE_WIN32, SERVICE_STATE_ALL, (LPENUM_SERVICE_STATUSW)buf, size, &need, &cnt, &resume);
    CHECK(r);
    if (r) {
        CHECK(cnt > 100);
        ENUM_SERVICE_STATUSW *e = (void *)buf; int bad = 0;
        for (DWORD i = 0; i < cnt; i++) {
            if (!in_range(buf, size, e[i].lpServiceName) || !e[i].lpServiceName[0]) bad++;
            if (!(e[i].ServiceStatus.dwServiceType & SERVICE_WIN32)) bad++;
        }
        EXPECT_EQ(bad, 0);
    }
    free(buf); svc_close(scm);
}
TEST(svcenum_legacy_A) {
    REQUIRE(S(EnumServicesStatusA));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    DWORD need = 0, cnt = 99, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusA)(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > 0);
    svc_close(scm);
}
TEST(svcenum_legacy_bad_state) {
    REQUIRE(S(EnumServicesStatusW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[256]; DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusW)(scm, SERVICE_WIN32, 0, (LPENUM_SERVICE_STATUSW)buf, sizeof buf, &need, &cnt, &resume);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
    svc_close(scm);
}
TEST(svcenum_legacy_null_scm) {
    REQUIRE(S(EnumServicesStatusW));
    DWORD need = 0, cnt = 0, resume = 0; SET_ERR(0);
    BOOL r = S(EnumServicesStatusW)(NULL, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &cnt, &resume);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcenum_legacy_resume_paging) {
    REQUIRE(S(EnumServicesStatusW));
    SC_HANDLE scm = enum_scm(); REQUIRE(scm);
    BYTE buf[4096]; DWORD resume = 0, seen = 0; int pages = 0, done = 0;
    for (int it = 0; it < 1000 && !done; it++) {
        DWORD need = 0, cnt = 0; SET_ERR(0);
        BOOL r = S(EnumServicesStatusW)(scm, SERVICE_WIN32, SERVICE_STATE_ALL, (LPENUM_SERVICE_STATUSW)buf, sizeof buf, &need, &cnt, &resume);
        if (!r && GetLastError() != ERROR_MORE_DATA) { CHECKF(0, "err=%lu", GetLastError()); break; }
        seen += cnt; pages++; if (r) done = 1;
    }
    CHECK(done); CHECK(pages > 1); CHECK(seen > 100);
    svc_close(scm);
}

/* ---------- EnumDependentServices ---------- */
static void dependents_of(const char *name, DWORD state, const char *must_contain) {
    REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm, s = svc_fixture(name, SERVICE_ENUMERATE_DEPENDENTS, &scm); REQUIRE_OPEN(s);
    DWORD need = 0, cnt = 99; SET_ERR(0);
    BOOL r = S(EnumDependentServicesW)(s, state, NULL, 0, &need, &cnt);
    if (r) { t_note("%s has no dependents in state %lu", name, state); EXPECT_EQ(cnt, 0); }
    else {
        EXPECT_ERR(ERROR_MORE_DATA);
        CHECK(need >= sizeof(ENUM_SERVICE_STATUSW)); 
        BYTE *buf = calloc(1, need + 4096); DWORD size = need + 4096;
        r = S(EnumDependentServicesW)(s, state, (LPENUM_SERVICE_STATUSW)buf, size, &need, &cnt);
        CHECK(r); CHECK(cnt > 0);
        ENUM_SERVICE_STATUSW *e = (void *)buf; int found = 0, bad = 0;
        for (DWORD i = 0; i < cnt; i++) {
            if (!in_range(buf, size, e[i].lpServiceName) || !e[i].lpServiceName[0]) { bad++; continue; }
            if (must_contain) { WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, must_contain, -1, w, 64); if (!lstrcmpiW(w, e[i].lpServiceName)) found = 1; }
            if (state == SERVICE_ACTIVE && e[i].ServiceStatus.dwCurrentState == SERVICE_STOPPED) bad++;
            if (state == SERVICE_INACTIVE && e[i].ServiceStatus.dwCurrentState != SERVICE_STOPPED) bad++;
        }
        EXPECT_EQ(bad, 0);
        if (must_contain) CHECKF(found, "%s not in dependents of %s", must_contain, name);
        t_note("%s has %lu dependents (state %lu)", name, cnt, state);
        DWORD n2 = 0, c2 = 0; SET_ERR(0);
        /* Win10: the size-query value is an upper bound (the real size is returned by the successful call),
         * so need-1 can succeed; a buffer too small for the first entry must still fail with ERROR_MORE_DATA. */
        r = S(EnumDependentServicesW)(s, state, (LPENUM_SERVICE_STATUSW)buf, 1, &n2, &c2);
        CHECK(!r); EXPECT_ERR(ERROR_MORE_DATA);
        CHECK(n2 > 1);
        free(buf);
    }
    svc_close(s); svc_close(scm);
}
TEST(svcdep_RpcEptMapper_has_RpcSs_all)    { dependents_of("RpcEptMapper", SERVICE_STATE_ALL, "RpcSs"); }
TEST(svcdep_DcomLaunch_has_RpcSs_all)      { dependents_of("DcomLaunch", SERVICE_STATE_ALL, "RpcSs"); }
TEST(svcdep_RpcEptMapper_has_RpcSs_active) { dependents_of("RpcEptMapper", SERVICE_ACTIVE, "RpcSs"); }
TEST(svcdep_DcomLaunch_inactive)           { dependents_of("DcomLaunch", SERVICE_INACTIVE, NULL); }
TEST(svcdep_RpcSs_all)                     { dependents_of("RpcSs", SERVICE_STATE_ALL, NULL); }
TEST(svcdep_EventLog_all)                  { dependents_of("EventLog", SERVICE_STATE_ALL, NULL); }
TEST(svcdep_A_variant) {
    REQUIRE(S(EnumDependentServicesA));
    SC_HANDLE scm, s = svc_fixture("RpcEptMapper", SERVICE_ENUMERATE_DEPENDENTS, &scm); REQUIRE_OPEN(s);
    DWORD need = 0, cnt = 99; SET_ERR(0);
    BOOL r = S(EnumDependentServicesA)(s, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    EXPECT_FAIL(!r, ERROR_MORE_DATA);
    CHECK(need > 0);
    svc_close(s); svc_close(scm);
}
TEST(svcdep_no_enumerate_right) {
    REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm, s = svc_fixture("RpcEptMapper", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    DWORD need = 0, cnt = 0; SET_ERR(0);
    BOOL r = S(EnumDependentServicesW)(s, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(s); svc_close(scm);
}
TEST(svcdep_null_handle) {
    REQUIRE(S(EnumDependentServicesW));
    DWORD need = 0, cnt = 0; SET_ERR(0);
    BOOL r = S(EnumDependentServicesW)(NULL, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcdep_scm_handle) {
    REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    DWORD need = 0, cnt = 0; SET_ERR(0);
    BOOL r = S(EnumDependentServicesW)(scm, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
TEST(svcdep_bad_state) {
    REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm, s = svc_fixture("RpcEptMapper", SERVICE_ENUMERATE_DEPENDENTS, &scm); REQUIRE_OPEN(s);
    BYTE buf[256]; DWORD need = 0, cnt = 0; SET_ERR(0);
    BOOL r = S(EnumDependentServicesW)(s, 0, (LPENUM_SERVICE_STATUSW)buf, sizeof buf, &need, &cnt);
    CHECK(!r);
    t_note("dependents state 0: err=%lu", GetLastError());
    svc_close(s); svc_close(scm);
}
TEST(svcdep_closed_handle) {
    REQUIRE(S(EnumDependentServicesW));
    SC_HANDLE scm, s = svc_fixture("RpcEptMapper", SERVICE_ENUMERATE_DEPENDENTS, &scm); REQUIRE_OPEN(s);
    svc_close(s);
    DWORD need = 0, cnt = 0; SET_ERR(0);
    BOOL r = S(EnumDependentServicesW)(s, SERVICE_STATE_ALL, NULL, 0, &need, &cnt);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(scm);
}
