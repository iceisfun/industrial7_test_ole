/* Additional dx_sechost_service_lifecycle tests (review pass). Documented MS behaviour only. */
#include "svc.h"

TEST(dx_svc_x_open_twice_distinct_handles) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE a = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    SC_HANDLE b = S(OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(a && b); CHECK(a != b);
    SERVICE_STATUS sa, sb;
    CHECK(a && S(QueryServiceStatus)(a, &sa)); CHECK(b && S(QueryServiceStatus)(b, &sb));
    EXPECT_EQ(sa.dwCurrentState, SERVICE_STOPPED); EXPECT_EQ(sb.dwCurrentState, SERVICE_STOPPED);
    if (a) S(CloseServiceHandle)(a);
    /* closing one handle does not invalidate the other */
    CHECK(b && S(QueryServiceStatus)(b, &sb));
    if (b) S(CloseServiceHandle)(b);
    fx_end(&f);
}

TEST(dx_svc_x_close_handle_twice) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    SC_HANDLE a = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT);
    CHECK(a);
    CHECK(S(CloseServiceHandle)(a));
    SET_ERR(0);
    CHECK(!S(CloseServiceHandle)(a)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_x_status_initial_stopped_fields) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_STATUS st; memset(&st, 0xAA, sizeof st);
    CHECK(S(QueryServiceStatus)(f.svc, &st));
    EXPECT_EQ(st.dwServiceType, SERVICE_WIN32_OWN_PROCESS);
    EXPECT_EQ(st.dwCurrentState, SERVICE_STOPPED);
    EXPECT_EQ(st.dwControlsAccepted, 0);
    EXPECT_EQ(st.dwCheckPoint, 0);
    EXPECT_EQ(st.dwWaitHint, 0);
    fx_end(&f);
}

TEST(dx_svc_x_description_clear_with_null_string) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ChangeServiceConfig2W)); REQUIRE(S(QueryServiceConfig2W));
    SERVICE_DESCRIPTIONW d = { (LPWSTR)L"olet some description" };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &d));
    BYTE *b = qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    CHECK(b);
    if (b) { SERVICE_DESCRIPTIONW *q = (void *)b; CHECK(q->lpDescription && !lstrcmpW(q->lpDescription, L"olet some description")); free(b); }
    /* NULL lpDescription means "leave unchanged"; an empty string deletes it (MS docs) */
    d.lpDescription = (LPWSTR)L"";
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &d));
    b = qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    CHECK(b);
    if (b) { SERVICE_DESCRIPTIONW *q = (void *)b; CHECKF(!q->lpDescription || !q->lpDescription[0], "description not cleared"); free(b); }
    fx_end(&f);
}

TEST(dx_svc_x_query_config_start_name_default) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    CHECK(b);
    if (b) {
        CHECK(c->lpServiceStartName && !lstrcmpiW(c->lpServiceStartName, L"LocalSystem"));
        CHECK(c->lpBinaryPathName && !lstrcmpW(c->lpBinaryPathName, FX_BIN));
        CHECK(c->lpDisplayName && !lstrcmpW(c->lpDisplayName, f.disp));
        EXPECT_EQ(c->dwStartType, SERVICE_DEMAND_START);
        EXPECT_EQ(c->dwServiceType, SERVICE_WIN32_OWN_PROCESS);
        EXPECT_EQ(c->dwTagId, 0);
        free(b);
    }
    fx_end(&f);
}

TEST(dx_svc_x_enum_status_old_api_finds_service) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(EnumServicesStatusW));
    DWORD cap = 1 << 18, need = 0, cnt = 0, resume = 0; BYTE *buf = malloc(cap);
    int found = 0;
    do {
        BOOL ok = S(EnumServicesStatusW)(f.scm, SERVICE_WIN32, SERVICE_INACTIVE, (void *)buf, cap, &need, &cnt, &resume);
        DWORD e = GetLastError();
        CHECKF(ok || e == ERROR_MORE_DATA, "enum err %lu", e);
        ENUM_SERVICE_STATUSW *es = (void *)buf;
        for (DWORD i = 0; i < cnt; i++) if (!lstrcmpiW(es[i].lpServiceName, f.name)) {
            found = 1;
            CHECK(!lstrcmpW(es[i].lpDisplayName, f.disp));
            EXPECT_EQ(es[i].ServiceStatus.dwCurrentState, SERVICE_STOPPED);
        }
        if (ok) break;
    } while (!found);
    CHECK(found);
    free(buf);
    fx_end(&f);
}

TEST(dx_svc_x_control_user_code_range_on_stopped) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_STATUS st;
    SET_ERR(0);
    CHECK(!S(ControlService)(f.svc, 0, &st));
    t_note("control code 0 -> %lu", GetLastError());
    fx_end(&f);
}

TEST(dx_svc_x_delete_then_recreate_same_name) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(DeleteService)(f.svc));
    S(CloseServiceHandle)(f.svc); f.svc = NULL;
    SC_HANDLE s = S(CreateServiceW)(f.scm, f.name, f.disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
    CHECKF(s != NULL, "recreate after full delete failed %lu", GetLastError());
    f.svc = s;
    fx_end(&f);
}

TEST(dx_svc_x_registry_display_name_value) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR v[260]; DWORD t = 0;
    CHECK(reg_str(f.name, L"DisplayName", v, 260, &t));
    CHECK(!lstrcmpW(v, f.disp));
    DWORD ec = 99;
    CHECK(reg_dword(f.name, L"ErrorControl", &ec)); EXPECT_EQ(ec, SERVICE_ERROR_NORMAL);
    fx_end(&f);
}
