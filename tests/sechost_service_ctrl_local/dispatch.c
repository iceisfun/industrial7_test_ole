/* sechost service-side API exercised from a NON-service (console) process.
 * Decomp-derived expectations (sechost.dll, Windows 10 x64):
 *   StartServiceCtrlDispatcher{A,W}: a process-wide flag is set on entry; a second call fails with
 *     ERROR_SERVICE_ALREADY_RUNNING (1056) whatever the first call returned. An empty table (first
 *     lpServiceName NULL) fails with ERROR_INVALID_PARAMETER (87). Otherwise the call opens the service
 *     control channel; the SCM refuses non-service callers: ERROR_FAILED_SERVICE_CONTROLLER_CONNECT (1063).
 *   SetServiceStatus(NULL, ...) -> FALSE / ERROR_INVALID_HANDLE (6) before the status is dereferenced.
 *   I_ScSetServiceBitsA is a stub: returns 0 with ERROR_NOT_SUPPORTED (50).
 *   I_ScPnPGetServiceName returns (does not set last error) ERROR_SERVICE_NOT_IN_EXE (1083) when there is no
 *     dispatch table. RegisterServiceCtrlHandler* without a table dereferences a NULL table in sechost
 *     (ScGetDispatchEntry), so it is run only in a child process and the result is a note.
 * NULL table to StartServiceCtrlDispatcher is NOT tested (the table is walked unchecked). */
#include "t.h"
#include <winsvc.h>

static VOID WINAPI svc_main_w(DWORD argc, LPWSTR *argv) { (void)argc; (void)argv; }
static VOID WINAPI svc_main_a(DWORD argc, LPSTR *argv) { (void)argc; (void)argv; }
static VOID WINAPI svc_handler(DWORD ctl) { (void)ctl; }
static DWORD WINAPI svc_handler_ex(DWORD ctl, DWORD type, LPVOID data, LPVOID ctx) { (void)ctl; (void)type; (void)data; (void)ctx; return NO_ERROR; }

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep) { ExitProcess(ep->ExceptionRecord->ExceptionCode); return EXCEPTION_EXECUTE_HANDLER; }

#define EXIT_TRUE 0xFFFFu   /* child "call succeeded" marker */
#define EXIT_NONNULL 0xFFFEu

static int child_disp_w(BOOL (WINAPI *f)(const SERVICE_TABLE_ENTRYW *), SERVICE_TABLE_ENTRYW *t)
{
    SetLastError(0);
    if (f(t)) return EXIT_TRUE;
    return (int)GetLastError();
}
static int child_disp_a(BOOL (WINAPI *f)(const SERVICE_TABLE_ENTRYA *), SERVICE_TABLE_ENTRYA *t)
{
    SetLastError(0);
    if (f(t)) return EXIT_TRUE;
    return (int)GetLastError();
}

CHILD(scl_disp)
{
    const char *mode = argc > 1 ? argv[1] : "";
    SetUnhandledExceptionFilter(crash_filter);
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    BOOL (WINAPI *dw)(const SERVICE_TABLE_ENTRYW *) = (void *)t_proc_in("sechost.dll", "StartServiceCtrlDispatcherW");
    BOOL (WINAPI *da)(const SERVICE_TABLE_ENTRYA *) = (void *)t_proc_in("sechost.dll", "StartServiceCtrlDispatcherA");
    HANDLE (WINAPI *rw)(LPCWSTR, LPHANDLER_FUNCTION) = (void *)t_proc_in("sechost.dll", "RegisterServiceCtrlHandlerW");
    HANDLE (WINAPI *ra)(LPCSTR, LPHANDLER_FUNCTION) = (void *)t_proc_in("sechost.dll", "RegisterServiceCtrlHandlerA");
    HANDLE (WINAPI *rxw)(LPCWSTR, LPHANDLER_FUNCTION_EX, LPVOID) = (void *)t_proc_in("sechost.dll", "RegisterServiceCtrlHandlerExW");
    HANDLE (WINAPI *rxa)(LPCSTR, LPHANDLER_FUNCTION_EX, LPVOID) = (void *)t_proc_in("sechost.dll", "RegisterServiceCtrlHandlerExA");
    if (!dw || !da || !rw || !ra || !rxw || !rxa) return 0xFFFD;

    SERVICE_TABLE_ENTRYW empty_w[1] = {{NULL, NULL}};
    SERVICE_TABLE_ENTRYA empty_a[1] = {{NULL, NULL}};
    SERVICE_TABLE_ENTRYW blank_w[2] = {{(LPWSTR)L"", svc_main_w}, {NULL, NULL}};
    SERVICE_TABLE_ENTRYA blank_a[2] = {{(LPSTR)"", svc_main_a}, {NULL, NULL}};
    SERVICE_TABLE_ENTRYW named_w[2] = {{(LPWSTR)L"olet_nosuch_svc", svc_main_w}, {NULL, NULL}};
    SERVICE_TABLE_ENTRYA named_a[2] = {{(LPSTR)"olet_nosuch_svc", svc_main_a}, {NULL, NULL}};
    SERVICE_TABLE_ENTRYW multi_w[3] = {{(LPWSTR)L"olet_a", svc_main_w}, {(LPWSTR)L"olet_b", svc_main_w}, {NULL, NULL}};
    SERVICE_TABLE_ENTRYA multi_a[3] = {{(LPSTR)"olet_a", svc_main_a}, {(LPSTR)"olet_b", svc_main_a}, {NULL, NULL}};
    SERVICE_TABLE_ENTRYW nullproc_w[2] = {{(LPWSTR)L"olet_np", NULL}, {NULL, NULL}};

    if (!strcmp(mode, "w_empty")) return child_disp_w(dw, empty_w);
    if (!strcmp(mode, "a_empty")) return child_disp_a(da, empty_a);
    if (!strcmp(mode, "w_blank")) return child_disp_w(dw, blank_w);
    if (!strcmp(mode, "a_blank")) return child_disp_a(da, blank_a);
    if (!strcmp(mode, "w_named")) return child_disp_w(dw, named_w);
    if (!strcmp(mode, "a_named")) return child_disp_a(da, named_a);
    if (!strcmp(mode, "w_multi")) return child_disp_w(dw, multi_w);
    if (!strcmp(mode, "a_multi")) return child_disp_a(da, multi_a);
    if (!strcmp(mode, "w_nullproc")) return child_disp_w(dw, nullproc_w);
    if (!strcmp(mode, "w_twice")) { child_disp_w(dw, named_w); return child_disp_w(dw, named_w); }
    if (!strcmp(mode, "a_twice")) { child_disp_a(da, named_a); return child_disp_a(da, named_a); }
    if (!strcmp(mode, "w_empty_then_named")) { child_disp_w(dw, empty_w); return child_disp_w(dw, named_w); }
    if (!strcmp(mode, "a_empty_then_named")) { child_disp_a(da, empty_a); return child_disp_a(da, named_a); }
    if (!strcmp(mode, "wa_cross")) { child_disp_w(dw, named_w); return child_disp_a(da, named_a); }
    if (!strcmp(mode, "aw_cross")) { child_disp_a(da, named_a); return child_disp_w(dw, named_w); }
    if (!strcmp(mode, "w_blank_then_empty")) { child_disp_w(dw, blank_w); return child_disp_w(dw, empty_w); }

    SetLastError(0);
    HANDLE h = NULL;
    if (!strcmp(mode, "reg_w")) h = rw(L"olet_nosuch_svc", svc_handler);
    else if (!strcmp(mode, "reg_a")) h = ra("olet_nosuch_svc", svc_handler);
    else if (!strcmp(mode, "regex_w")) h = rxw(L"olet_nosuch_svc", svc_handler_ex, (LPVOID)0x1234);
    else if (!strcmp(mode, "regex_a")) h = rxa("olet_nosuch_svc", svc_handler_ex, (LPVOID)0x1234);
    else if (!strcmp(mode, "reg_w_nullproc")) h = rw(L"olet_nosuch_svc", NULL);
    else if (!strcmp(mode, "regex_w_nullproc")) h = rxw(L"olet_nosuch_svc", NULL, NULL);
    else if (!strcmp(mode, "reg_w_empty_name")) h = rw(L"", svc_handler);
    else if (!strcmp(mode, "reg_a_empty_name")) h = ra("", svc_handler);
    else return 0xFFFC;
    if (h) return EXIT_NONNULL;
    return (int)GetLastError();
}

static DWORD run_mode(const char *mode) { return t_run_child("scl_disp", mode, 30000); }

typedef struct { const char *mode; DWORD err; } mode_exp;

TEST(scl_dispatch_table_driven)
{
    static const mode_exp tab[] = {
        /* empty table: ERROR_INVALID_PARAMETER */
        {"w_empty", ERROR_INVALID_PARAMETER}, {"a_empty", ERROR_INVALID_PARAMETER},
        /* table whose only name is empty, or a name that is not a service: the SCM refuses a console process */
        {"w_blank", ERROR_FAILED_SERVICE_CONTROLLER_CONNECT}, {"a_blank", ERROR_FAILED_SERVICE_CONTROLLER_CONNECT},
        {"w_named", ERROR_FAILED_SERVICE_CONTROLLER_CONNECT}, {"a_named", ERROR_FAILED_SERVICE_CONTROLLER_CONNECT},
        {"w_multi", ERROR_FAILED_SERVICE_CONTROLLER_CONNECT}, {"a_multi", ERROR_FAILED_SERVICE_CONTROLLER_CONNECT},
        /* a second call, W or A, fails on the process-wide flag whatever the first returned */
        {"w_twice", ERROR_SERVICE_ALREADY_RUNNING}, {"a_twice", ERROR_SERVICE_ALREADY_RUNNING},
        {"w_empty_then_named", ERROR_SERVICE_ALREADY_RUNNING}, {"a_empty_then_named", ERROR_SERVICE_ALREADY_RUNNING},
        {"wa_cross", ERROR_SERVICE_ALREADY_RUNNING}, {"aw_cross", ERROR_SERVICE_ALREADY_RUNNING},
        {"w_blank_then_empty", ERROR_SERVICE_ALREADY_RUNNING},
    };
    REQUIRE(UIN("sechost.dll", StartServiceCtrlDispatcherW));
    REQUIRE(UIN("sechost.dll", StartServiceCtrlDispatcherA));
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        if (t_is_wine() && (strstr(tab[i].mode, "then") || strstr(tab[i].mode, "cross"))) continue;  /* wine keeps no process-wide flag */
        DWORD r = run_mode(tab[i].mode);
        CHECKF(r == tab[i].err, "mode %s: exit/err %lu want %lu", tab[i].mode, r, tab[i].err);
    }
}

#define DISP_TEST(testname, modestr, want) \
    TEST(testname) { REQUIRE(UIN("sechost.dll", StartServiceCtrlDispatcherW)); \
        DWORD r = run_mode(modestr); EXPECT_EQ(r, want); }

DISP_TEST(scl_dispatch_w_empty_table_invalid_param, "w_empty", ERROR_INVALID_PARAMETER)
DISP_TEST(scl_dispatch_a_empty_table_invalid_param, "a_empty", ERROR_INVALID_PARAMETER)
DISP_TEST(scl_dispatch_w_blank_name_cannot_connect, "w_blank", 1063)
DISP_TEST(scl_dispatch_a_blank_name_cannot_connect, "a_blank", 1063)
DISP_TEST(scl_dispatch_w_named_cannot_connect, "w_named", 1063)
DISP_TEST(scl_dispatch_a_named_cannot_connect, "a_named", 1063)
DISP_TEST(scl_dispatch_w_second_call_already_running, "w_twice", 1056)
DISP_TEST(scl_dispatch_a_second_call_already_running, "a_twice", 1056)
TEST(scl_dispatch_flag_set_even_after_invalid_table)
{
    REQUIRE(UIN("sechost.dll", StartServiceCtrlDispatcherW));
    if (t_is_wine()) { t_skip("wine keeps no process-wide dispatcher flag"); return; }
    EXPECT_EQ(run_mode("w_empty_then_named"), 1056);
}
TEST(scl_dispatch_flag_shared_between_w_and_a)
{
    REQUIRE(UIN("sechost.dll", StartServiceCtrlDispatcherW));
    if (t_is_wine()) { t_skip("wine keeps no process-wide dispatcher flag"); return; }
    EXPECT_EQ(run_mode("wa_cross"), 1056);
}

TEST(scl_dispatch_null_service_proc_still_cannot_connect)
{
    REQUIRE(UIN("sechost.dll", StartServiceCtrlDispatcherW));
    /* the table is only copied before the connect attempt; a NULL main routine is not validated up front */
    if (t_is_wine()) { t_skip("wine validates the main routine"); return; }
    DWORD r = run_mode("w_nullproc");
    EXPECT_EQ(r, 1063);
}

TEST(scl_dispatch_error_constants)
{
    EXPECT_EQ(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT, 1063);
    EXPECT_EQ(ERROR_SERVICE_ALREADY_RUNNING, 1056);
    EXPECT_EQ(ERROR_SERVICE_NOT_IN_EXE, 1083);
    EXPECT_EQ(ERROR_INVALID_HANDLE, 6);
    EXPECT_EQ(ERROR_INVALID_PARAMETER, 87);
}

static void reg_child_expect(const char *mode)
{
    DWORD r = run_mode(mode);
    /* The decomp shows ScGetDispatchEntry walking an unallocated table: an access violation is the expected
     * outcome in a non-service process. Whatever happens, a non-service process must never get a status handle. */
    CHECKF(r != EXIT_NONNULL, "mode %s returned a non-NULL status handle", mode);
    CHECKF(r != 0xDEAD0000 && r != 0xDEAD0001, "mode %s: child did not complete (0x%lx)", mode, r);
    t_note("%s -> child exit 0x%lx (%s)", mode, r, r == 0xC0000005 ? "access violation" : r == 1083 ? "ERROR_SERVICE_NOT_IN_EXE" : "last error");
}

TEST(scl_register_handler_w_non_service)  { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerW)); reg_child_expect("reg_w"); }
TEST(scl_register_handler_a_non_service)  { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerA)); reg_child_expect("reg_a"); }
TEST(scl_register_handler_exw_non_service) { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerExW)); reg_child_expect("regex_w"); }
TEST(scl_register_handler_exa_non_service) { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerExA)); reg_child_expect("regex_a"); }
TEST(scl_register_handler_w_null_proc_non_service) { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerW)); reg_child_expect("reg_w_nullproc"); }
TEST(scl_register_handler_exw_null_proc_non_service) { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerExW)); reg_child_expect("regex_w_nullproc"); }
TEST(scl_register_handler_w_empty_name_non_service) { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerW)); reg_child_expect("reg_w_empty_name"); }
TEST(scl_register_handler_a_empty_name_non_service) { REQUIRE(UIN("sechost.dll", RegisterServiceCtrlHandlerA)); reg_child_expect("reg_a_empty_name"); }

TEST(scl_register_handler_advapi32_forwarders)
{
    /* advapi32 forwards the registration/dispatch entry points to sechost: same code address after resolution */
    static const char *names[] = {"RegisterServiceCtrlHandlerW", "RegisterServiceCtrlHandlerA",
        "RegisterServiceCtrlHandlerExW", "RegisterServiceCtrlHandlerExA", "StartServiceCtrlDispatcherW",
        "StartServiceCtrlDispatcherA", "SetServiceStatus"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC s = t_proc_in("sechost.dll", names[i]);
        FARPROC a = t_proc_in("advapi32.dll", names[i]);
        CHECKF(s != NULL, "sechost export %s", names[i]);
        if (!a) { t_note("advapi32 does not export %s", names[i]); continue; }
        /* forwarders resolve to the sechost body on Windows 10; unverified in the decomp, so only noted */
        if (s != a) t_note("%s: advapi32 (%p) and sechost (%p) differ", names[i], (void *)a, (void *)s);
    }
}

/* ---- SetServiceStatus ------------------------------------------------- */
TEST(scl_setservicestatus_null_handle)
{
    REQUIRE(UIN("sechost.dll", SetServiceStatus));
    SERVICE_STATUS st = {SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP, 0, 0, 0, 0};
    SET_ERR(0);
    BOOL r = UIN("sechost.dll", SetServiceStatus)(NULL, &st);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}

TEST(scl_setservicestatus_null_handle_null_status)
{
    REQUIRE(UIN("sechost.dll", SetServiceStatus));
    SET_ERR(0);
    BOOL r = UIN("sechost.dll", SetServiceStatus)(NULL, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
}

TEST(scl_setservicestatus_null_handle_each_state)
{
    REQUIRE(UIN("sechost.dll", SetServiceStatus));
    /* The handle test precedes the state-dependent paths (stopped removes the dispatch entry, running adjusts priority). */
    for (DWORD state = 0; state <= 8; state++) {
        SERVICE_STATUS st = {SERVICE_WIN32_OWN_PROCESS, state, 0, 0, 0, 0, 0};
        SET_ERR(0);
        BOOL r = UIN("sechost.dll", SetServiceStatus)(NULL, &st);
        CHECKF(!r, "state %lu", state);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}

TEST(scl_setservicestatus_null_handle_clears_previous_error)
{
    REQUIRE(UIN("sechost.dll", SetServiceStatus));
    SERVICE_STATUS st = {0};
    SET_ERR(0xdead);
    BOOL r = UIN("sechost.dll", SetServiceStatus)(NULL, &st);
    EXPECT_FAIL(!r, 6);
}

/* ---- I_Sc* private entry points ----------------------------------------- */
TEST(scl_isc_setservicebits_a_not_supported)
{
    UDECL(int, I_ScSetServiceBitsA, (HANDLE, DWORD, int, int, char *));
    I_ScSetServiceBitsA_pfn f = (I_ScSetServiceBitsA_pfn)t_proc_in("sechost.dll", "I_ScSetServiceBitsA");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("unimplemented under wine (aborts)"); return; }
    SET_ERR(0);
    int r = f(NULL, 1, 1, 0, NULL);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
}

TEST(scl_isc_setservicebits_a_ignores_arguments)
{
    UDECL(int, I_ScSetServiceBitsA, (HANDLE, DWORD, int, int, char *));
    I_ScSetServiceBitsA_pfn f = (I_ScSetServiceBitsA_pfn)t_proc_in("sechost.dll", "I_ScSetServiceBitsA");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("unimplemented under wine (aborts)"); return; }
    static const DWORD masks[] = {0, 1, 0x10, 0xffffffff};
    for (int i = 0; i < 4; i++) for (int set = 0; set < 2; set++) {
        SET_ERR(0);
        int r = f(NULL, masks[i], set, 1, NULL);
        CHECKF(r == 0 && GetLastError() == ERROR_NOT_SUPPORTED, "mask %lx set %d: r=%d err=%lu", masks[i], set, r, GetLastError());
    }
}

TEST(scl_isc_pnp_get_service_name_no_dispatch_table)
{
    UDECL(DWORD, I_ScPnPGetServiceName, (HANDLE, WCHAR *, DWORD));
    I_ScPnPGetServiceName_pfn f = (I_ScPnPGetServiceName_pfn)t_proc_in("sechost.dll", "I_ScPnPGetServiceName");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("unimplemented under wine (aborts)"); return; }
    WCHAR buf[64] = L"sentinel";
    SET_ERR(0);
    DWORD r = f((HANDLE)(ULONG_PTR)0x1234, buf, 64);
    EXPECT_EQ(r, ERROR_SERVICE_NOT_IN_EXE);   /* returned, not set */
    EXPECT_WSTR(buf, L"sentinel");
    r = f(NULL, buf, 64);
    EXPECT_EQ(r, ERROR_SERVICE_NOT_IN_EXE);
    r = f(NULL, NULL, 0);
    EXPECT_EQ(r, ERROR_SERVICE_NOT_IN_EXE);
}

TEST(scl_isc_send_ts_message_aliases_broadcast)
{
    FARPROC a = t_proc_in("sechost.dll", "I_ScSendTSMessage");
    FARPROC b = t_proc_in("sechost.dll", "I_ScBroadcastServiceControlMessage");
    REQUIRE(a); REQUIRE(b);
    if (t_is_wine()) { t_skip("wine stubs these separately"); return; }
    CHECK(a == b);   /* two export names, one function body (RVA 0x1bc00 in the decomp) */
}

TEST(scl_isc_resolve_only)
{
    static const char *names[] = {"I_ScSendPnPMessage", "I_ScSendTSMessage", "I_ScSetServiceBitsW",
        "I_ScSetServiceBitsA", "I_ScPnPGetServiceName", "I_ScIsSecurityProcess", "I_ScQueryServiceConfig",
        "I_ScRegisterDeviceNotification", "I_ScUnregisterDeviceNotification", "I_ScValidatePnPService",
        "I_ScReparseServiceDatabase", "I_ScRpcBindA", "I_ScRpcBindW", "I_ScRegisterPreshutdownRestart",
        "I_ScBroadcastServiceControlMessage", "I_RegisterSvchostNotificationCallback"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        CHECKF(t_proc_in("sechost.dll", names[i]) != NULL, "sechost!%s", names[i]);
}

TEST(scl_notifybootconfigstatus_resolve_only)
{
    /* NotifyBootConfigStatus must NOT be called: it would tell the SCM that boot is good. */
    FARPROC p = t_proc_in("advapi32.dll", "NotifyBootConfigStatus");
    CHECK(p != NULL);
    if (p && t_proc_in("sechost.dll", "NotifyBootConfigStatus") == NULL) t_note("NotifyBootConfigStatus lives only in advapi32");
}

TEST(scl_service_ctrl_exports_resolve)
{
    static const char *names[] = {"SetServiceStatus", "StartServiceCtrlDispatcherA", "StartServiceCtrlDispatcherW",
        "RegisterServiceCtrlHandlerA", "RegisterServiceCtrlHandlerW", "RegisterServiceCtrlHandlerExA",
        "RegisterServiceCtrlHandlerExW", "OpenSCManagerA", "OpenSCManagerW", "OpenServiceA", "OpenServiceW",
        "CloseServiceHandle", "QueryServiceStatus", "QueryServiceStatusEx", "QueryServiceConfigA",
        "QueryServiceConfigW", "QueryServiceConfig2A", "QueryServiceConfig2W", "QueryServiceObjectSecurity",
        "SetServiceObjectSecurity", "GetServiceDisplayNameW", "GetServiceKeyNameW", "EnumServicesStatusExW",
        "ControlService", "StartServiceW", "CreateServiceW", "DeleteService", "ChangeServiceConfigW"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        CHECKF(t_proc_in("sechost.dll", names[i]) != NULL, "sechost!%s", names[i]);
}

TEST(scl_lock_exports_live_in_advapi32)
{
    /* sechost does not export the database lock APIs; they are implemented in advapi32 itself. */
    CHECK(t_proc_in("advapi32.dll", "LockServiceDatabase") != NULL);
    CHECK(t_proc_in("advapi32.dll", "UnlockServiceDatabase") != NULL);
    CHECK(t_proc_in("advapi32.dll", "QueryServiceLockStatusW") != NULL);
    CHECK(t_proc_in("advapi32.dll", "QueryServiceLockStatusA") != NULL);
    CHECK(t_proc_in("sechost.dll", "LockServiceDatabase") == NULL);
    CHECK(t_proc_in("sechost.dll", "UnlockServiceDatabase") == NULL);
}
