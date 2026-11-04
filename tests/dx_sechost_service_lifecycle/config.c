#include "svc.h"
/* ChangeServiceConfig(2), QueryServiceConfig(2), object security. */

static SC_HANDLE open_q(fx_t *f, DWORD acc) { return S(OpenServiceW)(f->scm, f->name, acc); }

TEST(dx_svc_qconfig_defaults) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(c->dwServiceType, SERVICE_WIN32_OWN_PROCESS);
        EXPECT_EQ(c->dwStartType, SERVICE_DEMAND_START);
        EXPECT_EQ(c->dwErrorControl, SERVICE_ERROR_NORMAL);
        CHECK(!lstrcmpW(c->lpBinaryPathName, FX_BIN));
        CHECK(!lstrcmpW(c->lpDisplayName, f.disp));
        CHECK(c->lpServiceStartName && !lstrcmpiW(c->lpServiceStartName, L"LocalSystem"));
        EXPECT_EQ(c->dwTagId, 0);
        /* no dependencies: empty string or NULL, never garbage */
        CHECK(c->lpDependencies == NULL || c->lpDependencies[0] == 0);
        CHECK(c->lpLoadOrderGroup == NULL || c->lpLoadOrderGroup[0] == 0);
        /* strings point inside the caller's buffer */
        CHECK((BYTE *)c->lpBinaryPathName > b);
        free(b);
    }
    fx_end(&f);
}

TEST(dx_svc_qconfig_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0xFFFF;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfigW)(f.svc, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need > sizeof(QUERY_SERVICE_CONFIGW));
    DWORD n1 = need;
    /* Win10: the NULL-buffer call over-reports (it does not know the string lengths); the exact size is what a
       successful call with a large buffer returns in *pcbBytesNeeded */
    BYTE *b = malloc(need + 16);
    DWORD n2 = 0;
    CHECK(S(QueryServiceConfigW)(f.svc, (void *)b, n1 + 16, &n2));
    t_note("NULL-buffer size %lu, exact size %lu", n1, n2);
    CHECK(n2 > sizeof(QUERY_SERVICE_CONFIGW) && n2 <= n1);
    DWORD exact = n2, n3 = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfigW)(f.svc, (void *)b, exact - 1, &n3)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(n3 >= exact); /* Win10: a short buffer reports the over-estimated size (306) */
    /* sizeof(struct) alone is too small because strings follow */
    SET_ERR(0);
    CHECK(!S(QueryServiceConfigW)(f.svc, (void *)b, sizeof(QUERY_SERVICE_CONFIGW), &n3)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(S(QueryServiceConfigW)(f.svc, (void *)b, exact, &n3));
    free(b);
    fx_end(&f);
}

TEST(dx_svc_qconfig_a_matches_w) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(QueryServiceConfigA));
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfigA)(f.svc, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    BYTE *b = malloc(need);
    if (S(QueryServiceConfigA)(f.svc, (void *)b, need, &need)) {
        QUERY_SERVICE_CONFIGA *c = (void *)b;
        EXPECT_EQ(c->dwStartType, SERVICE_DEMAND_START);
        EXPECT_STR(c->lpDisplayName, ({ static char d[128]; WideCharToMultiByte(CP_ACP, 0, f.disp, -1, d, 128, NULL, NULL); d; }));
        EXPECT_STR(c->lpBinaryPathName, "C:\\olet_nonexistent_dir\\olet_svc.exe");
    } else CHECK(0);
    free(b);
    fx_end(&f);
}

TEST(dx_svc_qconfig_needs_query_config_right) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = open_q(&f, SERVICE_QUERY_STATUS);
    CHECK(o != NULL);
    if (o) {
        DWORD need = 0;
        SET_ERR(0);
        CHECK(!S(QueryServiceConfigW)(o, NULL, 0, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECK(!S(QueryServiceConfig2W)(o, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    fx_end(&f);
}

TEST(dx_svc_qconfig_wrong_handle_type) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfigW)(f.scm, NULL, 0, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(QueryServiceConfigW)(NULL, NULL, 0, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SERVICE_STATUS st;
    SET_ERR(0);
    CHECK(!S(QueryServiceStatus)(f.scm, &st)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_change_no_change_is_noop) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ChangeServiceConfigW));
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(c->dwServiceType, SERVICE_WIN32_OWN_PROCESS);
        EXPECT_EQ(c->dwStartType, SERVICE_DEMAND_START);
        EXPECT_EQ(c->dwErrorControl, SERVICE_ERROR_NORMAL);
        CHECK(!lstrcmpW(c->lpBinaryPathName, FX_BIN));
        CHECK(!lstrcmpW(c->lpDisplayName, f.disp));
        free(b);
    }
    fx_end(&f);
}

TEST(dx_svc_change_start_type_roundtrip) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    static const DWORD seq[] = { SERVICE_AUTO_START, SERVICE_DISABLED, SERVICE_DEMAND_START };
    for (int i = 0; i < 3; i++) {
        CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, seq[i], SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
        QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
        if (b) { EXPECT_EQ(c->dwStartType, seq[i]); free(b); } else CHECK(0);
        DWORD v = 99; CHECK(reg_dword(f.name, L"Start", &v)); EXPECT_EQ(v, seq[i]);
    }
    fx_end(&f);
}

TEST(dx_svc_change_bad_values) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, 7, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, 9, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfigW)(f.svc, SERVICE_ADAPTER, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* nothing changed */
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    if (b) { EXPECT_EQ(c->dwStartType, SERVICE_DEMAND_START); EXPECT_EQ(c->dwErrorControl, SERVICE_ERROR_NORMAL); free(b); }
    fx_end(&f);
}

TEST(dx_svc_change_display_name) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(GetServiceKeyNameW));
    WCHAR nd[160]; wsprintfW(nd, L"%s changed", f.disp);
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, nd));
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    if (b) { CHECK(!lstrcmpW(c->lpDisplayName, nd)); free(b); } else CHECK(0);
    WCHAR buf[160]; DWORD n = 160;
    CHECK(S(GetServiceKeyNameW)(f.scm, nd, buf, &n)); CHECK(!lstrcmpW(buf, f.name));
    n = 160; SET_ERR(0);
    CHECK(!S(GetServiceKeyNameW)(f.scm, f.disp, buf, &n)); EXPECT_ERR(ERROR_SERVICE_DOES_NOT_EXIST);
    WCHAR r[160]; CHECK(reg_str(f.name, L"DisplayName", r, 160, NULL)); CHECK(!lstrcmpW(r, nd));
    fx_end(&f);
}

TEST(dx_svc_change_display_name_duplicate) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR n2[160]; SC_HANDLE s2 = fx_create2(&f, L"dd", SERVICE_DEMAND_START, NULL, n2);
    CHECK(s2 != NULL);
    if (s2) {
        SET_ERR(0);
        /* Win10: a duplicate display name is accepted */
        CHECK(S(ChangeServiceConfigW)(s2, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, f.disp));
        /* same display name on itself is fine */
        WCHAR own[160]; wsprintfW(own, L"%s dd", f.disp);
        CHECK(S(ChangeServiceConfigW)(s2, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, own));
    }
    fx_del(&f, n2, s2);
    fx_end(&f);
}

TEST(dx_svc_change_binary_path) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    const WCHAR *np = L"\"C:\\olet nonexistent dir\\svc two.exe\" -k olet";
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, np, NULL, NULL, NULL, NULL, NULL, NULL));
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    if (b) { CHECK(!lstrcmpW(c->lpBinaryPathName, np)); free(b); } else CHECK(0);
    WCHAR r[300]; CHECK(reg_str(f.name, L"ImagePath", r, 300, NULL)); CHECK(!lstrcmpW(r, np));
    fx_end(&f);
}

TEST(dx_svc_change_a_variant) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ChangeServiceConfigA));
    CHECK(S(ChangeServiceConfigA)(f.svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_ERROR_SEVERE, "C:\\olet_nonexistent_dir\\b.exe", NULL, NULL, NULL, NULL, NULL, "olet display A changed"));
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    if (b) {
        EXPECT_EQ(c->dwStartType, SERVICE_DISABLED); EXPECT_EQ(c->dwErrorControl, SERVICE_ERROR_SEVERE);
        CHECK(!lstrcmpW(c->lpBinaryPathName, L"C:\\olet_nonexistent_dir\\b.exe"));
        CHECK(!lstrcmpW(c->lpDisplayName, L"olet display A changed"));
        free(b);
    } else CHECK(0);
    fx_end(&f);
}

TEST(dx_svc_change_dependencies_and_clear) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR deps[64]; memset(deps, 0, sizeof deps); lstrcpyW(deps, L"RpcSs");
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, deps, NULL, NULL, NULL));
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    if (b) {
        CHECK(c->lpDependencies && !lstrcmpiW(c->lpDependencies, L"RpcSs"));
        if (c->lpDependencies) CHECK(c->lpDependencies[6] == 0);
        free(b);
    } else CHECK(0);
    /* empty string deletes dependencies; NULL leaves them */
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    b = qcfg(f.svc, &c);
    if (b) { CHECK(c->lpDependencies && c->lpDependencies[0]); free(b); }
    WCHAR empty[2] = { 0, 0 };
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, empty, NULL, NULL, NULL));
    b = qcfg(f.svc, &c);
    if (b) { CHECK(c->lpDependencies == NULL || c->lpDependencies[0] == 0); free(b); }
    fx_end(&f);
}

TEST(dx_svc_change_circular_dependency) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    /* B depends on A (the fixture), then A -> B must be refused */
    WCHAR deps[200]; memset(deps, 0, sizeof deps); lstrcpyW(deps, f.name);
    WCHAR nb[160]; SC_HANDLE b = fx_create2(&f, L"cb", SERVICE_DEMAND_START, deps, nb);
    CHECK(b != NULL);
    if (b) {
        WCHAR d2[200]; memset(d2, 0, sizeof d2); lstrcpyW(d2, nb);
        SET_ERR(0);
        CHECK(!S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, d2, NULL, NULL, NULL));
        EXPECT_ERR(ERROR_CIRCULAR_DEPENDENCY);
        /* self dependency */
        memset(d2, 0, sizeof d2); lstrcpyW(d2, f.name);
        SET_ERR(0);
        CHECK(!S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, d2, NULL, NULL, NULL));
        EXPECT_ERR(ERROR_CIRCULAR_DEPENDENCY);
    }
    fx_del(&f, nb, b);
    fx_end(&f);
}

TEST(dx_svc_change_requires_right) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = open_q(&f, SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);
    CHECK(o != NULL);
    if (o) {
        SET_ERR(0);
        CHECK(!S(ChangeServiceConfigW)(o, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        SERVICE_DESCRIPTIONW d = { (LPWSTR)L"x" };
        SET_ERR(0);
        CHECK(!S(ChangeServiceConfig2W)(o, SERVICE_CONFIG_DESCRIPTION, &d)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    fx_end(&f);
}

TEST(dx_svc_change_wrong_handle) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfigW)(f.scm, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfigW)(NULL, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_change_account_roundtrip) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, L"NT AUTHORITY\\LocalService", NULL, NULL));
    QUERY_SERVICE_CONFIGW *c; BYTE *b = qcfg(f.svc, &c);
    if (b) { CHECK(c->lpServiceStartName && !lstrcmpiW(c->lpServiceStartName, L"NT AUTHORITY\\LocalService")); free(b); } else CHECK(0);
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, L"LocalSystem", NULL, NULL));
    b = qcfg(f.svc, &c);
    if (b) { CHECK(!lstrcmpiW(c->lpServiceStartName, L"LocalSystem")); free(b); }
    fx_end(&f);
}

TEST(dx_svc_change_service_type_switch) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_WIN32_SHARE_PROCESS, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    SERVICE_STATUS st; CHECK(S(QueryServiceStatus)(f.svc, &st));
    EXPECT_EQ(st.dwServiceType, SERVICE_WIN32_SHARE_PROCESS);
    DWORD t = 0; CHECK(reg_dword(f.name, L"Type", &t)); EXPECT_EQ(t, SERVICE_WIN32_SHARE_PROCESS);
    fx_end(&f);
}

TEST(dx_svc_change_tag_id_without_group) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD tag = 0;
    SET_ERR(0);
    BOOL r = S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, &tag, NULL, NULL, NULL, NULL);
    t_note("ChangeServiceConfig with tag ptr and no group -> %d err %lu tag %lu", r, GetLastError(), tag);
    fx_end(&f);
}

/* ---------------- ChangeServiceConfig2 / QueryServiceConfig2 ---------------- */

TEST(dx_svc_cfg2_description_roundtrip) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ChangeServiceConfig2W)); REQUIRE(S(QueryServiceConfig2W));
    /* default: no description */
    SERVICE_DESCRIPTIONW *d = (void *)qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    CHECK(d != NULL);
    if (d) { CHECK(d->lpDescription == NULL); free(d); }
    WCHAR txt[] = L"olet description \x00e9\x4e2d text";
    SERVICE_DESCRIPTIONW in = { txt };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &in));
    d = (void *)qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    CHECK(d != NULL);
    if (d) { CHECK(d->lpDescription && !lstrcmpW(d->lpDescription, txt)); free(d); }
    WCHAR r[200]; DWORD ty = 0;
    CHECK(reg_str(f.name, L"Description", r, 200, &ty)); CHECK(!lstrcmpW(r, txt)); EXPECT_EQ(ty, REG_SZ);
    /* NULL lpDescription: unchanged */
    SERVICE_DESCRIPTIONW nul = { NULL };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &nul));
    d = (void *)qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    if (d) { CHECK(d->lpDescription && !lstrcmpW(d->lpDescription, txt)); free(d); }
    /* empty string: deleted */
    SERVICE_DESCRIPTIONW emp = { (LPWSTR)L"" };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &emp));
    d = (void *)qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    if (d) { CHECK(d->lpDescription == NULL || d->lpDescription[0] == 0); free(d); }
    CHECK(!reg_str(f.name, L"Description", r, 200, &ty));
    fx_end(&f);
}

TEST(dx_svc_cfg2_description_a_variant) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ChangeServiceConfig2A)); REQUIRE(S(QueryServiceConfig2A));
    SERVICE_DESCRIPTIONA in = { (LPSTR)"olet ansi description" };
    CHECK(S(ChangeServiceConfig2A)(f.svc, SERVICE_CONFIG_DESCRIPTION, &in));
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfig2A)(f.svc, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    BYTE *b = malloc(need);
    if (S(QueryServiceConfig2A)(f.svc, SERVICE_CONFIG_DESCRIPTION, b, need, &need)) {
        SERVICE_DESCRIPTIONA *d = (void *)b;
        CHECK(d->lpDescription != NULL);
        if (d->lpDescription) EXPECT_STR(d->lpDescription, "olet ansi description");
    } else CHECK(0);
    free(b);
    SERVICE_DESCRIPTIONW *w = (void *)qcfg2(f.svc, SERVICE_CONFIG_DESCRIPTION);
    if (w) { CHECK(w->lpDescription && !lstrcmpW(w->lpDescription, L"olet ansi description")); free(w); }
    fx_end(&f);
}

TEST(dx_svc_cfg2_query_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_DESCRIPTIONW in = { (LPWSTR)L"0123456789" };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, &in));
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(SERVICE_DESCRIPTIONW) + 11 * 2);
    BYTE *b = malloc(need + 8); DWORD n2 = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, b, need - 1, &n2)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n2, need);
    CHECK(S(QueryServiceConfig2W)(f.svc, SERVICE_CONFIG_DESCRIPTION, b, need, &n2));
    free(b);
    fx_end(&f);
}

TEST(dx_svc_cfg2_bad_level) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    BYTE buf[256]; DWORD need = 0;
    static const DWORD bad[] = { 0, 99, 0xFFFFFFFF };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(!S(QueryServiceConfig2W)(f.svc, bad[i], buf, sizeof buf, &need)); EXPECT_ERR(ERROR_INVALID_LEVEL);
        SERVICE_DESCRIPTIONW d = { (LPWSTR)L"x" };
        SET_ERR(0);
        CHECK(!S(ChangeServiceConfig2W)(f.svc, bad[i], &d)); EXPECT_ERR(ERROR_INVALID_LEVEL);
    }
    fx_end(&f);
}

TEST(dx_svc_cfg2_failure_actions_roundtrip) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_ACTION acts[3] = { { SC_ACTION_RESTART, 1000 }, { SC_ACTION_RUN_COMMAND, 2000 }, { SC_ACTION_NONE, 3000 } };
    SERVICE_FAILURE_ACTIONSW in = { 86400, (LPWSTR)L"olet reboot msg", (LPWSTR)L"C:\\Windows\\System32\\cmd.exe /c exit", 3, acts };
    BOOL ok = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS, &in);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        SERVICE_FAILURE_ACTIONSW *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS);
        CHECK(o != NULL);
        if (o) {
            EXPECT_EQ(o->dwResetPeriod, 86400);
            CHECK(o->lpRebootMsg && !lstrcmpW(o->lpRebootMsg, L"olet reboot msg"));
            CHECK(o->lpCommand && !lstrcmpW(o->lpCommand, L"C:\\Windows\\System32\\cmd.exe /c exit"));
            EXPECT_EQ(o->cActions, 3);
            if (o->cActions == 3 && o->lpsaActions) {
                for (int i = 0; i < 3; i++) { EXPECT_EQ(o->lpsaActions[i].Type, acts[i].Type); EXPECT_EQ(o->lpsaActions[i].Delay, acts[i].Delay); }
            }
            free(o);
        }
        /* update only reset period: actions stay when cActions==0 and lpsaActions==NULL */
        SERVICE_FAILURE_ACTIONSW rp = { 100, NULL, NULL, 0, NULL };
        BOOL ok2 = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS, &rp);
        t_note("reset-period-only update -> %d err %lu", ok2, GetLastError());
        o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS);
        if (o) { if (ok2) EXPECT_EQ(o->dwResetPeriod, 86400); /* Win10: reset-only update (no actions) leaves the period unchanged */ t_note("after reset-only: cActions=%lu", o->cActions); free(o); }
        /* clear all actions: cActions=0 with non-NULL lpsaActions? document: cActions 0 + NULL deletes */
    }
    fx_end(&f);
}

TEST(dx_svc_cfg2_failure_actions_default_empty) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_FAILURE_ACTIONSW *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS);
    CHECK(o != NULL);
    if (o) { EXPECT_EQ(o->cActions, 0); t_note("default reset period %lu", o->dwResetPeriod); free(o); }
    fx_end(&f);
}

TEST(dx_svc_cfg2_failure_actions_a_variant) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(ChangeServiceConfig2A));
    SC_ACTION acts[2] = { { SC_ACTION_RESTART, 5000 }, { SC_ACTION_NONE, 0 } };
    SERVICE_FAILURE_ACTIONSA in = { 3600, NULL, (LPSTR)"cmd.exe /c exit", 2, acts };
    CHECK(S(ChangeServiceConfig2A)(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS, &in));
    SERVICE_FAILURE_ACTIONSW *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS);
    if (o) {
        EXPECT_EQ(o->dwResetPeriod, 3600); EXPECT_EQ(o->cActions, 2);
        CHECK(o->lpCommand && !lstrcmpW(o->lpCommand, L"cmd.exe /c exit"));
        free(o);
    } else CHECK(0);
    fx_end(&f);
}

TEST(dx_svc_cfg2_failure_actions_flag) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_FAILURE_ACTIONS_FLAG *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG);
    CHECK(o != NULL);
    if (o) { CHECK(o->fFailureActionsOnNonCrashFailures == FALSE); free(o); }
    SERVICE_FAILURE_ACTIONS_FLAG in = { TRUE };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &in));
    o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG);
    if (o) { CHECK(o->fFailureActionsOnNonCrashFailures == TRUE); free(o); } else CHECK(0);
    DWORD v = 99; CHECK(reg_dword(f.name, L"FailureActionsOnNonCrashFailures", &v)); EXPECT_EQ(v, 1);
    in.fFailureActionsOnNonCrashFailures = FALSE;
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &in));
    o = (void *)qcfg2(f.svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG);
    if (o) { CHECK(o->fFailureActionsOnNonCrashFailures == FALSE); free(o); }
    fx_end(&f);
}

TEST(dx_svc_cfg2_delayed_auto_start) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_DELAYED_AUTO_START_INFO in = { TRUE };
    /* only valid for auto-start services */
    SET_ERR(0);
    BOOL r = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &in);
    t_note("delayed autostart on demand-start service -> %d err %lu", r, GetLastError());
    CHECK(S(ChangeServiceConfigW)(f.svc, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &in));
    SERVICE_DELAYED_AUTO_START_INFO *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO);
    CHECK(o != NULL);
    if (o) { CHECK(o->fDelayedAutostart == TRUE); free(o); }
    DWORD v = 99; CHECK(reg_dword(f.name, L"DelayedAutostart", &v)); EXPECT_EQ(v, 1);
    in.fDelayedAutostart = FALSE;
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &in));
    o = (void *)qcfg2(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO);
    if (o) { CHECK(o->fDelayedAutostart == FALSE); free(o); }
    fx_end(&f);
}

TEST(dx_svc_cfg2_delayed_auto_start_on_demand_start) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_DELAYED_AUTO_START_INFO in = { TRUE };
    SET_ERR(0);
    BOOL r = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &in);
    /* documented: the setting is ignored unless the service is auto-start; whether it is stored is a note */
    t_note("delayed flag on demand-start service -> %d err %lu", r, GetLastError());
    SERVICE_DELAYED_AUTO_START_INFO *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO);
    if (o) { t_note("stored flag on demand-start service: %d", o->fDelayedAutostart); free(o); }
    fx_end(&f);
}

TEST(dx_svc_cfg2_sid_info) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_SID_INFO *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_SERVICE_SID_INFO);
    CHECK(o != NULL);
    if (o) { EXPECT_EQ(o->dwServiceSidType, SERVICE_SID_TYPE_NONE); free(o); }
    static const DWORD types[] = { SERVICE_SID_TYPE_UNRESTRICTED, SERVICE_SID_TYPE_RESTRICTED, SERVICE_SID_TYPE_NONE };
    for (int i = 0; i < 3; i++) {
        SERVICE_SID_INFO in = { types[i] };
        BOOL ok = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_SERVICE_SID_INFO, &in);
        CHECKF(ok, "type %lu err %lu", types[i], GetLastError());
        o = (void *)qcfg2(f.svc, SERVICE_CONFIG_SERVICE_SID_INFO);
        if (o) { EXPECT_EQ(o->dwServiceSidType, types[i]); free(o); } else CHECK(0);
        DWORD v = 99; CHECK(reg_dword(f.name, L"ServiceSidType", &v) || types[i] == SERVICE_SID_TYPE_NONE);
        if (types[i] != SERVICE_SID_TYPE_NONE) EXPECT_EQ(v, types[i]);
    }
    fx_end(&f);
}

TEST(dx_svc_cfg2_sid_info_invalid) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    static const DWORD bad[] = { 2, 4, 0x100, 0xFFFFFFFF };
    for (int i = 0; i < 4; i++) {
        SERVICE_SID_INFO in = { bad[i] };
        SET_ERR(0);
        CHECKF(!S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_SERVICE_SID_INFO, &in), "sid type 0x%lx accepted", bad[i]);
        t_note("invalid sid type 0x%lx -> err %lu (87 expected)", bad[i], GetLastError());
    }
    fx_end(&f);
}

TEST(dx_svc_cfg2_required_privileges) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_REQUIRED_PRIVILEGES_INFOW *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO);
    CHECK(o != NULL);
    if (o) { CHECK(o->pmszRequiredPrivileges == NULL || o->pmszRequiredPrivileges[0] == 0); free(o); }
    WCHAR privs[] = L"SeChangeNotifyPrivilege\0SeImpersonatePrivilege\0";
    SERVICE_REQUIRED_PRIVILEGES_INFOW in = { privs };
    BOOL ok = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &in);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        o = (void *)qcfg2(f.svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO);
        CHECK(o != NULL);
        if (o && o->pmszRequiredPrivileges) {
            const WCHAR *p = o->pmszRequiredPrivileges; int n = 0, a = 0, b = 0;
            while (*p) { if (!lstrcmpiW(p, L"SeChangeNotifyPrivilege")) a = 1; if (!lstrcmpiW(p, L"SeImpersonatePrivilege")) b = 1; p += lstrlenW(p) + 1; n++; }
            EXPECT_EQ(n, 2); CHECK(a); CHECK(b);
        } else CHECK(0);
        free(o);
        WCHAR rv[200]; DWORD ty = 0;
        CHECK(reg_str(f.name, L"RequiredPrivileges", rv, 200, &ty)); EXPECT_EQ(ty, REG_MULTI_SZ);
    }
    fx_end(&f);
}

TEST(dx_svc_cfg2_required_privileges_invalid_name) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    WCHAR privs[] = L"SeOletNoSuchPrivilege\0";
    SERVICE_REQUIRED_PRIVILEGES_INFOW in = { privs };
    SET_ERR(0);
    BOOL ok = S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &in);
    t_note("unknown privilege name -> %d err %lu", ok, GetLastError());
    fx_end(&f);
}

TEST(dx_svc_cfg2_preshutdown) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_PRESHUTDOWN_INFO *o = (void *)qcfg2(f.svc, SERVICE_CONFIG_PRESHUTDOWN_INFO);
    CHECK(o != NULL);
    if (o) { t_note("default preshutdown timeout %lu ms (build dependent: 180000 on >=15063)", o->dwPreshutdownTimeout); free(o); }
    SERVICE_PRESHUTDOWN_INFO in = { 12345 };
    CHECK(S(ChangeServiceConfig2W)(f.svc, SERVICE_CONFIG_PRESHUTDOWN_INFO, &in));
    o = (void *)qcfg2(f.svc, SERVICE_CONFIG_PRESHUTDOWN_INFO);
    CHECK(o != NULL);
    if (o) { EXPECT_EQ(o->dwPreshutdownTimeout, 12345); free(o); }
    DWORD v = 99; CHECK(reg_dword(f.name, L"PreshutdownTimeout", &v)); EXPECT_EQ(v, 12345);
    fx_end(&f);
}

TEST(dx_svc_cfg2_launch_protected_query) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0; BYTE buf[64];
    SET_ERR(0);
    BOOL ok = S(QueryServiceConfig2W)(f.svc, 12 /* SERVICE_CONFIG_LAUNCH_PROTECTED */, buf, sizeof buf, &need);
    t_note("query launch protected -> %d err %lu", ok, GetLastError());
    if (ok) EXPECT_EQ(*(DWORD *)buf, SERVICE_LAUNCH_PROTECTED_NONE);
    fx_end(&f);
}

TEST(dx_svc_cfg2_trigger_info_query_default) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0;
    SET_ERR(0);
    BOOL ok = S(QueryServiceConfig2W)(f.svc, 8 /* TRIGGER_INFO */, NULL, 0, &need);
    DWORD e = GetLastError();
    t_note("query trigger info (NULL,0) -> %d err %lu need %lu", ok, e, need);
    fx_end(&f);
}

TEST(dx_svc_cfg2_wrong_handle_and_access) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SERVICE_DESCRIPTIONW d = { (LPWSTR)L"x" };
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfig2W)(f.scm, SERVICE_CONFIG_DESCRIPTION, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!S(ChangeServiceConfig2W)(NULL, SERVICE_CONFIG_DESCRIPTION, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    DWORD need; BYTE b[64];
    SET_ERR(0);
    CHECK(!S(QueryServiceConfig2W)(f.scm, SERVICE_CONFIG_DESCRIPTION, b, sizeof b, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_cfg2_failure_actions_need_change_config_right) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = open_q(&f, SERVICE_QUERY_CONFIG);
    CHECK(o != NULL);
    if (o) {
        SERVICE_FAILURE_ACTIONS_FLAG in = { TRUE };
        SET_ERR(0);
        CHECK(!S(ChangeServiceConfig2W)(o, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &in)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    fx_end(&f);
}

/* ---------------- object security ---------------- */

TEST(dx_svc_sec_query_buffer_protocol) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(QueryServiceObjectSecurity));
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, NULL, 0, &need)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need > sizeof(SECURITY_DESCRIPTOR_RELATIVE) - 1);
    BYTE *b = malloc(need + 8); DWORD n2 = 0;
    SET_ERR(0);
    CHECK(!S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, b, need - 1, &n2)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n2, need);
    CHECK(S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, b, need, &n2));
    CHECK(IsValidSecurityDescriptor(b));
    SECURITY_DESCRIPTOR_CONTROL ctl; DWORD rev;
    CHECK(GetSecurityDescriptorControl(b, &ctl, &rev));
    CHECK(ctl & SE_SELF_RELATIVE);
    BOOL present = FALSE, def = FALSE; PACL acl = NULL;
    CHECK(GetSecurityDescriptorDacl(b, &present, &acl, &def));
    CHECK(present); CHECK(acl != NULL);
    if (acl) CHECK(acl->AceCount >= 1);
    free(b);
    fx_end(&f);
}

TEST(dx_svc_sec_query_owner_group) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0;
    S(QueryServiceObjectSecurity)(f.svc, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, NULL, 0, &need);
    BYTE *b = malloc(need + 8);
    if (S(QueryServiceObjectSecurity)(f.svc, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, b, need, &need)) {
        PSID o = NULL, g = NULL; BOOL d;
        GetSecurityDescriptorOwner(b, &o, &d); GetSecurityDescriptorGroup(b, &g, &d);
        CHECK(o != NULL && IsValidSid(o)); CHECK(g != NULL && IsValidSid(g));
        BOOL present = TRUE; PACL acl; GetSecurityDescriptorDacl(b, &present, &acl, &d);
        CHECK(!present);
    } else CHECK(0);
    free(b);
    fx_end(&f);
}

TEST(dx_svc_sec_query_needs_read_control) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    SC_HANDLE o = open_q(&f, SERVICE_QUERY_STATUS);
    CHECK(o != NULL);
    if (o) {
        DWORD need = 0;
        SET_ERR(0);
        CHECK(!S(QueryServiceObjectSecurity)(o, DACL_SECURITY_INFORMATION, NULL, 0, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    fx_end(&f);
}

TEST(dx_svc_sec_query_scm_handle) {
    fx_t f; if (!fx_begin(&f, 0)) return;
    DWORD need = 0;
    SET_ERR(0);
    BOOL ok = S(QueryServiceObjectSecurity)(f.scm, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    CHECK(!ok); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(!S(QueryServiceObjectSecurity)(NULL, DACL_SECURITY_INFORMATION, NULL, 0, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fx_end(&f);
}

TEST(dx_svc_sec_set_roundtrip) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    REQUIRE(S(SetServiceObjectSecurity));
    PSECURITY_DESCRIPTOR sd = NULL;
    /* SYSTEM + Administrators: all; Authenticated Users: query status+config */
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;CCLCRC;;;AU)", SDDL_REVISION_1, &sd, NULL)) { t_skip("sddl conv"); fx_end(&f); return; }
    CHECK(S(SetServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, sd));
    DWORD need = 0;
    S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    BYTE *b = malloc(need + 8);
    CHECK(S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, b, need, &need));
    BOOL pr, df; PACL acl = NULL;
    GetSecurityDescriptorDacl(b, &pr, &acl, &df);
    CHECK(pr && acl);
    if (acl) EXPECT_EQ(acl->AceCount, 3);
    free(b);
    LocalFree(sd);
    fx_end(&f);
}

TEST(dx_svc_sec_set_requires_write_dac) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;SY)(A;;GA;;;BA)", SDDL_REVISION_1, &sd, NULL)) { t_skip("sddl conv"); fx_end(&f); return; }
    SC_HANDLE o = open_q(&f, SERVICE_QUERY_CONFIG | READ_CONTROL);
    CHECK(o != NULL);
    if (o) {
        SET_ERR(0);
        CHECK(!S(SetServiceObjectSecurity)(o, DACL_SECURITY_INFORMATION, sd)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        S(CloseServiceHandle)(o);
    }
    LocalFree(sd);
    fx_end(&f);
}

TEST(dx_svc_sec_set_invalid_descriptor) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    /* a descriptor with only a revision field is structurally invalid (not an AV: a valid pointer) */
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SET_ERR(0);
    BOOL ok = S(SetServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, &sd);
    t_note("set absolute SD with no DACL (null DACL absolute) -> %d err %lu", ok, GetLastError());
    fx_end(&f);
}

TEST(dx_svc_sec_deny_ace_blocks_open) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    /* the DACL is consulted on OpenService: we keep BA full so we can still clean up, just prove a restricted DACL applies */
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;CCLCRC;;;BA)(A;;GA;;;SY)", SDDL_REVISION_1, &sd, NULL)) { t_skip("sddl conv"); fx_end(&f); return; }
    /* handle keeps WRITE_DAC from creation so we can restore */
    CHECK(S(SetServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, sd));
    SC_HANDLE o = open_q(&f, SERVICE_CHANGE_CONFIG);
    t_note("open CHANGE_CONFIG with restricted DACL -> %p err %lu (access denied expected for non-SYSTEM)", (void *)o, GetLastError());
    if (o) S(CloseServiceHandle)(o);
    SC_HANDLE q = open_q(&f, SERVICE_QUERY_CONFIG);
    CHECK(q != NULL);
    if (q) S(CloseServiceHandle)(q);
    LocalFree(sd);
    fx_end(&f);
}

TEST(dx_svc_sec_default_dacl_admin_all_access) {
    fx_t f; if (!fx_begin(&f, 1)) return;
    DWORD need = 0;
    S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    BYTE *b = malloc(need + 8);
    if (S(QueryServiceObjectSecurity)(f.svc, DACL_SECURITY_INFORMATION, b, need, &need)) {
        BOOL pr, df; PACL acl = NULL; GetSecurityDescriptorDacl(b, &pr, &acl, &df);
        int found_sy = 0, found_ba = 0;
        if (acl) for (DWORD i = 0; i < acl->AceCount; i++) {
            ACCESS_ALLOWED_ACE *a; if (!GetAce(acl, i, (void **)&a)) continue;
            if (a->Header.AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
            SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
            PSID s = &a->SidStart;
            if (!memcmp(GetSidIdentifierAuthority(s), &nt, 6) && *GetSidSubAuthorityCount(s) >= 1) {
                DWORD sa = *GetSidSubAuthority(s, 0);
                if (sa == SECURITY_LOCAL_SYSTEM_RID) found_sy = 1;
                if (sa == SECURITY_BUILTIN_DOMAIN_RID && *GetSidSubAuthorityCount(s) == 2 && *GetSidSubAuthority(s, 1) == DOMAIN_ALIAS_RID_ADMINS) found_ba = 1;
            }
        }
        CHECK(found_sy); CHECK(found_ba);
    } else CHECK(0);
    free(b);
    fx_end(&f);
}
