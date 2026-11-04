#include "svc.h"

/* ---------- GetServiceDisplayName / GetServiceKeyName ---------- */
static SC_HANDLE name_scm(void) { return scm_open(SC_MANAGER_CONNECT); }

static void disp_for(const char *key) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR k[64]; MultiByteToWideChar(CP_ACP, 0, key, -1, k, 64);
    DWORD n = 0; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(scm, k, NULL, &n);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(n > 0 && n < 512);
    WCHAR buf[512]; memset(buf, 0xCC, sizeof buf);
    DWORD sz = n + 1;
    r = S(GetServiceDisplayNameW)(scm, k, buf, &sz);
    CHECK(r);
    CHECK((int)sz == lstrlenW(buf));            /* on success: length excluding the NUL */
    EXPECT_EQ(sz, n);
    CHECK(buf[0] != 0);
    t_note("%s -> %ls", key, buf);
    /* too small by one char (size == required, no room for NUL) */
    if (n > 0) {
        DWORD s2 = n; WCHAR b2[512]; SET_ERR(0);
        BOOL r2 = S(GetServiceDisplayNameW)(scm, k, b2, &s2);
        t_note("%s: size==required(%lu): r=%d err=%lu out=%lu", key, n, r2, r2 ? 0 : GetLastError(), s2);
        DWORD s3 = n - 1; SET_ERR(0);
        r2 = S(GetServiceDisplayNameW)(scm, k, b2, &s3);
        CHECK(!r2); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(s3, n);
    }
    svc_close(scm);
}
PER_SERVICE(svcname_display, disp_for)

TEST(svcname_display_zero_size_with_buffer) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR buf[8] = {L'x'}; DWORD n = 0; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(scm, L"RpcSs", buf, &n);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(n > 0);
    svc_close(scm);
}
TEST(svcname_display_A) {
    REQUIRE(S(GetServiceDisplayNameA)); REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    DWORD na = 0, nw = 0; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameA)(scm, "RpcSs", NULL, &na);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    S(GetServiceDisplayNameW)(scm, L"RpcSs", NULL, &nw);
    CHECK(na > 0);
    char a[512]; WCHAR w[512]; DWORD sa = sizeof a, sw = 512;
    if (S(GetServiceDisplayNameA)(scm, "RpcSs", a, &sa) && S(GetServiceDisplayNameW)(scm, L"RpcSs", w, &sw)) {
        char tmp[512]; wide_to_narrow(tmp, sizeof tmp, w); EXPECT_STR(a, tmp);
        /* Win10: the ANSI variant leaves *lpcchBuffer at the buffer size on success (W variant returns the length). */
        EXPECT_EQ(sa, sizeof a);
    } else CHECK(0);
    svc_close(scm);
}
TEST(svcname_display_case_insensitive) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR a[256], b[256]; DWORD na = 256, nb = 256;
    CHECK(S(GetServiceDisplayNameW)(scm, L"RpcSs", a, &na));
    CHECK(S(GetServiceDisplayNameW)(scm, L"rpcss", b, &nb));
    EXPECT_WSTR(a, b);
    svc_close(scm);
}
TEST(svcname_display_nonexistent) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR buf[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(scm, L"olet_NoSuchService_12345", buf, &n);
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(scm);
}
TEST(svcname_display_nonexistent_A) {
    REQUIRE(S(GetServiceDisplayNameA));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    char buf[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameA)(scm, "olet_NoSuchService_12345", buf, &n);
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(scm);
}
TEST(svcname_display_empty_name) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR buf[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(scm, L"", buf, &n);
    CHECK(!r);
    t_note("GetServiceDisplayNameW(empty) err=%lu", GetLastError());
    svc_close(scm);
}
TEST(svcname_display_null_scm) {
    REQUIRE(S(GetServiceDisplayNameW));
    WCHAR buf[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(NULL, L"RpcSs", buf, &n);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcname_display_service_handle_as_scm) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    WCHAR buf[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(s, L"RpcSs", buf, &n);
    /* Win10 does not check the handle type here: a service handle works as the SCM handle. */
    CHECK(r);
    if (r) EXPECT_WSTR(buf, L"Remote Procedure Call (RPC)");
    svc_close(s); svc_close(scm);
}
TEST(svcname_display_null_name) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR buf[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceDisplayNameW)(scm, NULL, buf, &n);
    CHECK(!r);
    t_note("GetServiceDisplayNameW(NULL name) err=%lu", GetLastError());
    svc_close(scm);
}
TEST(svcname_display_rpcss_en_us) {
    REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR buf[256]; DWORD n = 256;
    if (S(GetServiceDisplayNameW)(scm, L"RpcSs", buf, &n)) {
        if (lstrcmpW(buf, L"Remote Procedure Call (RPC)")) t_note("RpcSs display name differs (locale?): %ls", buf);
        else CHECK(1);
    }
    svc_close(scm);
}
TEST(svcname_display_matches_config) {
    REQUIRE(S(GetServiceDisplayNameW)); REQUIRE(S(QueryServiceConfigW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_CONFIG, &scm); REQUIRE_OPEN(s);
    DWORD need = 0; S(QueryServiceConfigW)(s, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16); WCHAR d[256]; DWORD n = 256;
    if (S(QueryServiceConfigW)(s, (void *)buf, need, &need) && S(GetServiceDisplayNameW)(scm, L"RpcSs", d, &n)) {
        if (lstrcmpW(d, ((QUERY_SERVICE_CONFIGW *)buf)->lpDisplayName)) t_note("display name differs: GetServiceDisplayName '%ls' vs config '%ls'", d, ((QUERY_SERVICE_CONFIGW *)buf)->lpDisplayName);
    }
    free(buf); svc_close(s); svc_close(scm);
}

static void key_roundtrip(const char *key) {
    REQUIRE(S(GetServiceDisplayNameW)); REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR k[64], disp[512], back[512]; MultiByteToWideChar(CP_ACP, 0, key, -1, k, 64);
    DWORD nd = 512, nk = 0; SET_ERR(0);
    CHECK(S(GetServiceDisplayNameW)(scm, k, disp, &nd));
    BOOL r = S(GetServiceKeyNameW)(scm, disp, NULL, &nk);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(nk, lstrlenW(k));
    nk = 512;
    CHECK(S(GetServiceKeyNameW)(scm, disp, back, &nk));
    CHECKF(!lstrcmpiW(back, k), "got %ls want %ls", back, k);
    EXPECT_EQ(nk, lstrlenW(back));
    svc_close(scm);
}
PER_SERVICE(svcname_key_roundtrip, key_roundtrip)

TEST(svcname_key_size_protocol) {
    REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR b[64]; DWORD n = 0; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(scm, L"Remote Procedure Call (RPC)", b, &n);
    if (r) { t_note("display-name lookup succeeded with size 0?"); svc_close(scm); return; }
    if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) { t_note("locale differs: display name not found"); svc_close(scm); return; }
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, 5);
    n = 5; SET_ERR(0); r = S(GetServiceKeyNameW)(scm, L"Remote Procedure Call (RPC)", b, &n);
    t_note("key name buffer==5 chars: r=%d err=%lu", r, r ? 0 : GetLastError());
    n = 6; r = S(GetServiceKeyNameW)(scm, L"Remote Procedure Call (RPC)", b, &n);
    CHECK(r); if (r) { EXPECT_WSTR(b, L"RpcSs"); EXPECT_EQ(n, 5); }
    svc_close(scm);
}
TEST(svcname_key_A) {
    REQUIRE(S(GetServiceKeyNameA)); REQUIRE(S(GetServiceDisplayNameA));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    char d[512], k[512]; DWORD nd = sizeof d, nk = sizeof k;
    CHECK(S(GetServiceDisplayNameA)(scm, "RpcSs", d, &nd));
    CHECK(S(GetServiceKeyNameA)(scm, d, k, &nk));
    CHECK(!lstrcmpiA(k, "RpcSs"));
    svc_close(scm);
}
TEST(svcname_key_nonexistent) {
    REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR b[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(scm, L"olet no such display name", b, &n);
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(scm);
}
TEST(svcname_key_nonexistent_A) {
    REQUIRE(S(GetServiceKeyNameA));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    char b[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameA)(scm, "olet no such display name", b, &n);
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(scm);
}
TEST(svcname_key_null_scm) {
    REQUIRE(S(GetServiceKeyNameW));
    WCHAR b[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(NULL, L"x", b, &n);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svcname_key_service_handle_as_scm) {
    REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    WCHAR b[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(s, L"x", b, &n);
    /* Win10 does not reject the service handle; the lookup of the bogus display name fails normally. */
    EXPECT_FAIL(!r, ERROR_SERVICE_DOES_NOT_EXIST);
    svc_close(s); svc_close(scm);
}
TEST(svcname_key_empty_name) {
    REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR b[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(scm, L"", b, &n);
    CHECK(!r);
    t_note("GetServiceKeyNameW(empty) err=%lu", GetLastError());
    svc_close(scm);
}
TEST(svcname_key_given_key_name_not_display) {
    REQUIRE(S(GetServiceKeyNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR b[64]; DWORD n = 64; SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(scm, L"RpcSs", b, &n);
    t_note("GetServiceKeyNameW('RpcSs') -> %d err=%lu", r, r ? 0 : GetLastError());
    svc_close(scm);
}
TEST(svcname_key_case_insensitive_display) {
    REQUIRE(S(GetServiceKeyNameW)); REQUIRE(S(GetServiceDisplayNameW));
    SC_HANDLE scm = name_scm(); REQUIRE(scm);
    WCHAR d[256], up[256], k[64]; DWORD nd = 256, nk = 64;
    CHECK(S(GetServiceDisplayNameW)(scm, L"RpcSs", d, &nd));
    lstrcpyW(up, d); CharUpperW(up);
    SET_ERR(0);
    BOOL r = S(GetServiceKeyNameW)(scm, up, k, &nk);
    CHECK(r);
    svc_close(scm);
}

/* ---------- QueryServiceLockStatus ---------- */
TEST(svclock_size_query) {
    REQUIRE(S(QueryServiceLockStatusW));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_QUERY_LOCK_STATUS | SC_MANAGER_CONNECT);
    if (!scm) { t_note("QUERY_LOCK_STATUS denied err=%lu", GetLastError()); return; }
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceLockStatusW)(scm, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(QUERY_SERVICE_LOCK_STATUSW));
    svc_close(scm);
}
TEST(svclock_query_unlocked) {
    REQUIRE(S(QueryServiceLockStatusW));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_QUERY_LOCK_STATUS | SC_MANAGER_CONNECT);
    if (!scm) { t_skip("QUERY_LOCK_STATUS denied"); return; }
    DWORD need = 0; S(QueryServiceLockStatusW)(scm, NULL, 0, &need);
    BYTE *buf = calloc(1, need + 16);
    if (S(QueryServiceLockStatusW)(scm, (LPQUERY_SERVICE_LOCK_STATUSW)buf, need, &need)) {
        QUERY_SERVICE_LOCK_STATUSW *l = (void *)buf;
        t_note("lock: locked=%lu owner=%ls duration=%lu", l->fIsLocked, l->lpLockOwner ? l->lpLockOwner : L"(null)", l->dwLockDuration);
        CHECK(l->lpLockOwner != NULL);
        CHECK((BYTE *)l->lpLockOwner >= buf && (BYTE *)l->lpLockOwner < buf + need);
    } else CHECKF(0, "err=%lu", GetLastError());
    free(buf); svc_close(scm);
}
TEST(svclock_query_A) {
    REQUIRE(S(QueryServiceLockStatusA));
    SC_HANDLE scm = S(OpenSCManagerW)(NULL, NULL, SC_MANAGER_QUERY_LOCK_STATUS | SC_MANAGER_CONNECT);
    if (!scm) { t_skip("QUERY_LOCK_STATUS denied"); return; }
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceLockStatusA)(scm, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(QUERY_SERVICE_LOCK_STATUSA));
    svc_close(scm);
}
TEST(svclock_without_right) {
    REQUIRE(S(QueryServiceLockStatusW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT); REQUIRE(scm);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceLockStatusW)(scm, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    svc_close(scm);
}
TEST(svclock_null_handle) {
    REQUIRE(S(QueryServiceLockStatusW));
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceLockStatusW)(NULL, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}
TEST(svclock_service_handle) {
    REQUIRE(S(QueryServiceLockStatusW));
    SC_HANDLE scm, s = svc_fixture("RpcSs", SERVICE_QUERY_STATUS, &scm); REQUIRE_OPEN(s);
    DWORD need = 0; SET_ERR(0);
    BOOL r = S(QueryServiceLockStatusW)(s, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    svc_close(s); svc_close(scm);
}

/* ---------- internal I_Sc* exports ---------- */
static const char *isc_names[] = {"I_ScIsSecurityProcess", "I_ScQueryServiceConfig", "I_ScValidatePnPService", "I_ScPnPGetServiceName",
    "I_ScSendPnPMessage", "I_ScSendTSMessage", "I_ScSetServiceBitsA", "I_ScSetServiceBitsW", "I_ScRpcBindA", "I_ScRpcBindW",
    "I_ScBroadcastServiceControlMessage", "I_ScRegisterDeviceNotification", "I_ScUnregisterDeviceNotification",
    "I_ScReparseServiceDatabase", "I_ScRegisterPreshutdownRestart"};
TEST(svcisc_exports_present) {
    for (unsigned i = 0; i < sizeof isc_names / sizeof isc_names[0]; i++) {
        CHECKF(t_proc_in("sechost.dll", isc_names[i]) != NULL, "sechost!%s missing", isc_names[i]);
    }
}
TEST(svcisc_advapi32_exports_or_forwards) {
    for (unsigned i = 0; i < sizeof isc_names / sizeof isc_names[0]; i++)
        if (!t_proc_in("advapi32.dll", isc_names[i])) t_note("advapi32 does not export %s", isc_names[i]);
}
UDECL(BOOL, I_ScIsSecurityProcess, (void));
TEST(svcisc_is_security_process_current) {
    FARPROC p = t_proc_in("sechost.dll", "I_ScIsSecurityProcess");
    REQUIRE(p); WINE_UNIMPL();
    /* No documented signature; we only record that it can be located. */
    t_note("I_ScIsSecurityProcess at %p", (void *)p);
}
/* Several name-based APIs are present in sechost under both W and ANSI names. */
TEST(svcexports_sechost_scm_api_surface) {
    static const char *n[] = {"OpenSCManagerA", "OpenSCManagerW", "OpenServiceA", "OpenServiceW", "CloseServiceHandle", "QueryServiceStatus",
        "QueryServiceStatusEx", "QueryServiceConfigA", "QueryServiceConfigW", "QueryServiceConfig2A", "QueryServiceConfig2W",
        "QueryServiceObjectSecurity", "EnumServicesStatusExW", "EnumDependentServicesW", "GetServiceDisplayNameW", "GetServiceKeyNameW",
        "ControlService", "ControlServiceExA", "ControlServiceExW", "NotifyServiceStatusChangeA", "NotifyServiceStatusChangeW",
        "NotifyServiceStatusChange", "QueryServiceDynamicInformation", "SubscribeServiceChangeNotifications",
        "UnsubscribeServiceChangeNotifications", "WaitServiceState"};
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; i++) CHECKF(t_proc_in("sechost.dll", n[i]) != NULL, "sechost!%s missing", n[i]);
}
TEST(svcexports_advapi32_only_names) {
    /* A-variants and legacy names that Windows 10 keeps in advapi32 itself. */
    static const char *n[] = {"EnumServicesStatusA", "EnumServicesStatusW", "EnumServicesStatusExA", "EnumDependentServicesA",
        "GetServiceDisplayNameA", "GetServiceKeyNameA", "QueryServiceLockStatusA", "QueryServiceLockStatusW"};
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; i++) CHECKF(t_proc_in("advapi32.dll", n[i]) != NULL, "advapi32!%s missing", n[i]);
}
TEST(svcexports_ordinal_and_name_same_proc) {
    FARPROC a = t_proc_in("sechost.dll", "NotifyServiceStatusChange"), b = t_proc_in("sechost.dll", "NotifyServiceStatusChangeW");
    REQUIRE(a); REQUIRE(b);
    t_note("NotifyServiceStatusChange %p / ...W %p (%s)", (void *)a, (void *)b, a == b ? "alias" : "distinct");
}
