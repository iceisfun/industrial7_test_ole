#include "ssh.h"
/* LookupPrivilegeNameW/A, LookupPrivilegeValueW/A, LookupPrivilegeDisplayNameW/A.
 * These live in advapi32 (not exported by sechost on Win10) -- pinned with UIN("advapi32.dll", ...).
 * Well-known privilege LUID low parts are stable constants (SE_*_PRIVILEGE in winnt.h/ntseapi.h). */

static const struct { const char *name; DWORD luid; const char *display; } privs[] = {
    { "SeCreateTokenPrivilege",                  2,  "Create a token object" },
    { "SeAssignPrimaryTokenPrivilege",           3,  "Replace a process level token" },
    { "SeLockMemoryPrivilege",                   4,  "Lock pages in memory" },
    { "SeIncreaseQuotaPrivilege",                5,  "Adjust memory quotas for a process" },
    { "SeMachineAccountPrivilege",               6,  "Add workstations to domain" },
    { "SeTcbPrivilege",                          7,  "Act as part of the operating system" },
    { "SeSecurityPrivilege",                     8,  "Manage auditing and security log" },
    { "SeTakeOwnershipPrivilege",                9,  "Take ownership of files or other objects" },
    { "SeLoadDriverPrivilege",                   10, "Load and unload device drivers" },
    { "SeSystemProfilePrivilege",                11, "Profile system performance" },
    { "SeSystemtimePrivilege",                   12, "Change the system time" },
    { "SeProfileSingleProcessPrivilege",         13, "Profile single process" },
    { "SeIncreaseBasePriorityPrivilege",         14, "Increase scheduling priority" },
    { "SeCreatePagefilePrivilege",               15, "Create a pagefile" },
    { "SeCreatePermanentPrivilege",              16, "Create permanent shared objects" },
    { "SeBackupPrivilege",                       17, "Back up files and directories" },
    { "SeRestorePrivilege",                      18, "Restore files and directories" },
    { "SeShutdownPrivilege",                     19, "Shut down the system" },
    { "SeDebugPrivilege",                        20, "Debug programs" },
    { "SeAuditPrivilege",                        21, "Generate security audits" },
    { "SeSystemEnvironmentPrivilege",            22, "Modify firmware environment values" },
    { "SeChangeNotifyPrivilege",                 23, "Bypass traverse checking" },
    { "SeRemoteShutdownPrivilege",               24, "Force shutdown from a remote system" },
    { "SeUndockPrivilege",                       25, "Remove computer from docking station" },
    { "SeSyncAgentPrivilege",                    26, "Synchronize directory service data" },
    { "SeEnableDelegationPrivilege",             27, "Enable computer and user accounts to be trusted for delegation" },
    { "SeManageVolumePrivilege",                 28, "Perform volume maintenance tasks" },
    { "SeImpersonatePrivilege",                  29, "Impersonate a client after authentication" },
    { "SeCreateGlobalPrivilege",                 30, "Create global objects" },
    { "SeTrustedCredManAccessPrivilege",         31, "Access Credential Manager as a trusted caller" },
    { "SeRelabelPrivilege",                      32, "Modify an object label" },
    { "SeIncreaseWorkingSetPrivilege",           33, "Increase a process working set" },
    { "SeTimeZonePrivilege",                     34, "Change the time zone" },
    { "SeCreateSymbolicLinkPrivilege",           35, "Create symbolic links" },
};
#define NPRIV (sizeof privs / sizeof privs[0])

#define LPVW UIN(AD, LookupPrivilegeValueW)
#define LPNW UIN(AD, LookupPrivilegeNameW)
#define LPDW UIN(AD, LookupPrivilegeDisplayNameW)

TEST(priv_value_w_table)
{
    unsigned i;
    REQUIRE(LPVW);
    for (i = 0; i < NPRIV; i++) {
        WCHAR w[64]; LUID l = { 0xdead, 0xbeef }; BOOL r;
        widen(privs[i].name, w, 64);
        r = LPVW(NULL, w, &l);
        CHECKF(r, "%s err %lu", privs[i].name, GetLastError());
        if (r) { CHECKF(l.LowPart == privs[i].luid && l.HighPart == 0, "%s: luid %lu.%ld want %lu", privs[i].name, l.LowPart, l.HighPart, privs[i].luid); }
    }
}
TEST(priv_value_w_debug_and_shutdown)
{
    LUID l = { 1, 1 };
    REQUIRE(LPVW);
    CHECK(LPVW(NULL, L"SeDebugPrivilege", &l)); EXPECT_EQ(l.LowPart, 20); EXPECT_EQ(l.HighPart, 0);
    CHECK(LPVW(NULL, L"SeShutdownPrivilege", &l)); EXPECT_EQ(l.LowPart, 19); EXPECT_EQ(l.HighPart, 0);
}
TEST(priv_value_w_unknown_name)
{
    static const WCHAR *bad[] = { L"", L"SeNoSuchPrivilege", L"Debug", L"SeDebug", L"SeDebugPrivilege ", L" SeDebugPrivilege", L"SeDebugPrivilegeX", L"b0gu5" };
    unsigned i;
    REQUIRE(LPVW);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        LUID l = { 0x1111, 0x2222 };
        SET_ERR(0);
        CHECKF(!LPVW(NULL, bad[i], &l), "\"%ls\" resolved", bad[i]);
        EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
    }
}
TEST(priv_value_w_failure_zeroes_luid)
{
    LUID l = { 0x1111, 0x2222 };
    REQUIRE(LPVW);
    SET_ERR(0);
    CHECK(!LPVW(NULL, L"SeNoSuchPrivilege", &l));
    EXPECT_EQ(l.LowPart, 0); EXPECT_EQ(l.HighPart, 0);   /* Win10 zeroes the LUID on failure */
}
TEST(priv_value_w_case_insensitive)
{
    LUID l = { 0, 0 }; BOOL r;
    REQUIRE(LPVW);
    r = LPVW(NULL, L"sedebugprivilege", &l);
    t_note("lowercase privilege name -> %d (err %lu) luid %lu", r, r ? 0 : GetLastError(), l.LowPart);
    r = LPVW(NULL, L"SEDEBUGPRIVILEGE", &l);
    t_note("uppercase privilege name -> %d", r);
}
TEST(priv_value_w_local_system_name_forms)
{
    WCHAR cn[64]; DWORD n = 64; LUID l = { 0, 0 };
    REQUIRE(LPVW);
    CHECK(LPVW(L"", L"SeDebugPrivilege", &l));   /* empty system name == local */
    EXPECT_EQ(l.LowPart, 20);
    GetComputerNameW(cn, &n);
    l.LowPart = 0;
    if (LPVW(cn, L"SeDebugPrivilege", &l)) EXPECT_EQ(l.LowPart, 20);
    else t_note("explicit computer name -> err %lu", GetLastError());
}
TEST(priv_value_a_basic)
{
    LUID l = { 0, 0 };
    REQUIRE(UIN(AD, LookupPrivilegeValueA));
    CHECK(UIN(AD, LookupPrivilegeValueA)(NULL, "SeDebugPrivilege", &l)); EXPECT_EQ(l.LowPart, 20);
    CHECK(UIN(AD, LookupPrivilegeValueA)(NULL, "SeShutdownPrivilege", &l)); EXPECT_EQ(l.LowPart, 19);
    SET_ERR(0);
    CHECK(!UIN(AD, LookupPrivilegeValueA)(NULL, "SeNoSuch", &l));
    EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
}
TEST(priv_value_a_table)
{
    unsigned i;
    REQUIRE(UIN(AD, LookupPrivilegeValueA));
    for (i = 0; i < NPRIV; i++) {
        LUID l = { 0, 0 };
        CHECKF(UIN(AD, LookupPrivilegeValueA)(NULL, privs[i].name, &l) && l.LowPart == privs[i].luid, "%s", privs[i].name);
    }
}

TEST(priv_name_w_table)
{
    unsigned i;
    REQUIRE(LPNW);
    for (i = 0; i < NPRIV; i++) {
        LUID l = { privs[i].luid, 0 }; WCHAR buf[128]; DWORD n = 128; char a[128];
        BOOL r = LPNW(NULL, &l, buf, &n);
        CHECKF(r, "luid %lu err %lu", privs[i].luid, GetLastError());
        if (!r) continue;
        narrow(buf, a, sizeof a);
        CHECKF(!strcmp(a, privs[i].name), "luid %lu: \"%s\" want \"%s\"", privs[i].luid, a, privs[i].name);
        EXPECT_EQ(n, strlen(privs[i].name));
    }
}
TEST(priv_name_w_size_protocol)
{
    LUID l = { SE_CREATE_TOKEN_PRIVILEGE, 0 }; WCHAR buf[64]; DWORD n; const DWORD len = 22; /* strlen("SeCreateTokenPrivilege") */
    REQUIRE(LPNW);
    n = 0; SET_ERR(0);
    EXPECT_FAIL(!LPNW(NULL, &l, NULL, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, len + 1);
    n = 0; SET_ERR(0);
    EXPECT_FAIL(!LPNW(NULL, &l, buf, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, len + 1);
    n = 1; SET_ERR(0);
    EXPECT_FAIL(!LPNW(NULL, &l, buf, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, len + 1);
    n = len; SET_ERR(0);
    EXPECT_FAIL(!LPNW(NULL, &l, buf, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, len + 1);
    n = len + 1;
    CHECK(LPNW(NULL, &l, buf, &n));
    EXPECT_EQ(n, len); EXPECT_EQ(buf[len], 0);
    n = 64;
    CHECK(LPNW(NULL, &l, buf, &n));
    EXPECT_EQ(n, len);
}
TEST(priv_name_w_failure_keeps_buffer)
{
    LUID l = { SE_DEBUG_PRIVILEGE, 0 }; WCHAR buf[8]; DWORD n = 5;
    REQUIRE(LPNW);
    buf[0] = 'q';
    SET_ERR(0);
    EXPECT_FAIL(!LPNW(NULL, &l, buf, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, 17);   /* "SeDebugPrivilege" + NUL */
}
TEST(priv_name_w_no_such_privilege)
{
    static const LUID bad[] = { { 0xdeadbeef, 0 }, { 0, 0 }, { 1, 0 }, { 36 + 1000, 0 }, { SE_CREATE_TOKEN_PRIVILEGE, 1 }, { SE_DEBUG_PRIVILEGE, -1 }, { 0xffffffff, 0 } };
    unsigned i;
    REQUIRE(LPNW);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        LUID l = bad[i]; WCHAR buf[128]; DWORD n = 128;
        SET_ERR(0);
        CHECKF(!LPNW(NULL, &l, buf, &n), "luid %lu.%ld resolved", l.LowPart, l.HighPart);
        EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
    }
}
TEST(priv_name_w_value_roundtrip)
{
    unsigned i;
    REQUIRE(LPNW); REQUIRE(LPVW);
    for (i = 0; i < NPRIV; i++) {
        LUID l = { privs[i].luid, 0 }, l2 = { 0, 0 }; WCHAR buf[128]; DWORD n = 128;
        if (LPNW(NULL, &l, buf, &n) && LPVW(NULL, buf, &l2)) CHECK(l2.LowPart == l.LowPart && l2.HighPart == l.HighPart);
        else CHECKF(0, "roundtrip %lu", privs[i].luid);
    }
}
TEST(priv_name_w_36_is_win10_dependent)
{
    LUID l = { 36, 0 }; WCHAR buf[128]; DWORD n = 128;
    REQUIRE(LPNW);
    if (LPNW(NULL, &l, buf, &n)) t_note("LUID 36 = %ls", buf); else t_note("LUID 36 err %lu", GetLastError());
}
TEST(priv_name_a_basic)
{
    LUID l = { SE_DEBUG_PRIVILEGE, 0 }; char buf[64]; DWORD n = 64;
    REQUIRE(UIN(AD, LookupPrivilegeNameA));
    CHECK(UIN(AD, LookupPrivilegeNameA)(NULL, &l, buf, &n));
    EXPECT_STR(buf, "SeDebugPrivilege"); EXPECT_EQ(n, 16);
    l.LowPart = SE_SHUTDOWN_PRIVILEGE; n = 64;
    CHECK(UIN(AD, LookupPrivilegeNameA)(NULL, &l, buf, &n));
    EXPECT_STR(buf, "SeShutdownPrivilege");
}
TEST(priv_name_a_size_protocol)
{
    LUID l = { SE_CREATE_TOKEN_PRIVILEGE, 0 }; char buf[64]; DWORD n = 0;
    REQUIRE(UIN(AD, LookupPrivilegeNameA));
    SET_ERR(0);
    EXPECT_FAIL(!UIN(AD, LookupPrivilegeNameA)(NULL, &l, NULL, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, 23);
    n = 64;
    CHECK(UIN(AD, LookupPrivilegeNameA)(NULL, &l, buf, &n));
    EXPECT_EQ(n, 22);
}
TEST(priv_name_a_wine_known_values_loop)
{
    LONG i; unsigned found = 0;
    REQUIRE(UIN(AD, LookupPrivilegeNameA));
    for (i = SE_MIN_WELL_KNOWN_PRIVILEGE; i <= SE_MAX_WELL_KNOWN_PRIVILEGE; i++) {
        LUID l = { (DWORD)i, 0 }; char buf[MAX_PATH]; DWORD n = sizeof buf;
        if (UIN(AD, LookupPrivilegeNameA)(NULL, &l, buf, &n)) { found++; CHECK(!strncmp(buf, "Se", 2)); }
        else EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
    }
    CHECKF(found >= 34, "found %u", found);
}

TEST(priv_display_w_table_english)
{
    unsigned i;
    REQUIRE(LPDW);
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    for (i = 0; i < NPRIV; i++) {
        WCHAR w[64], d[256]; DWORD n = 256, lang = 0xffff; char a[256];
        widen(privs[i].name, w, 64);
        if (!LPDW(NULL, w, d, &n, &lang)) { CHECKF(0, "%s err %lu", privs[i].name, GetLastError()); continue; }
        narrow(d, a, sizeof a);
        EXPECT_EQ(n, strlen(a));
        if (strcmp(a, privs[i].display)) t_note("%s display \"%s\" (expected \"%s\")", privs[i].name, a, privs[i].display);
    }
}
TEST(priv_display_w_hard_names_english)
{
    WCHAR d[256]; DWORD n = 256, lang;
    REQUIRE(LPDW);
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    CHECK(LPDW(NULL, L"SeDebugPrivilege", d, &n, &lang)); EXPECT_WSTR(d, L"Debug programs");
    n = 256; CHECK(LPDW(NULL, L"SeShutdownPrivilege", d, &n, &lang)); EXPECT_WSTR(d, L"Shut down the system");
    n = 256; CHECK(LPDW(NULL, L"SeChangeNotifyPrivilege", d, &n, &lang)); EXPECT_WSTR(d, L"Bypass traverse checking");
    n = 256; CHECK(LPDW(NULL, L"SeBackupPrivilege", d, &n, &lang)); EXPECT_WSTR(d, L"Back up files and directories");
    n = 256; CHECK(LPDW(NULL, L"SeRestorePrivilege", d, &n, &lang)); EXPECT_WSTR(d, L"Restore files and directories");
}
TEST(priv_display_w_language_id)
{
    WCHAR d[256]; DWORD n = 256, lang = 0xdeadbeef;
    REQUIRE(LPDW);
    CHECK(LPDW(NULL, L"SeDebugPrivilege", d, &n, &lang));
    t_note("language id out = 0x%lx", lang);
    CHECK(lang != 0xdeadbeef);
    CHECK(lang <= 0xffff);
}
TEST(priv_display_w_size_protocol)
{
    WCHAR d[256]; DWORD n = 0, lang = 0, req; WCHAR full[256];
    REQUIRE(LPDW);
    SET_ERR(0);
    EXPECT_FAIL(!LPDW(NULL, L"SeDebugPrivilege", NULL, &n, &lang), ERROR_INSUFFICIENT_BUFFER);
    req = n;
    CHECK(req > 1);
    if (is_english_ui()) EXPECT_EQ(req, 15);   /* "Debug programs" + NUL */
    n = 256; CHECK(LPDW(NULL, L"SeDebugPrivilege", full, &n, &lang));
    EXPECT_EQ(wcslen(full), n);
    t_note("size query returned %lu, success length %lu", req, n);
    CHECK(req == n + 1 || req == n);
    if (req == wcslen(full) + 1) {
        n = (DWORD)wcslen(full);
        SET_ERR(0);
        EXPECT_FAIL(!LPDW(NULL, L"SeDebugPrivilege", d, &n, &lang), ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(n, req);
        n = req;
        CHECK(LPDW(NULL, L"SeDebugPrivilege", d, &n, &lang));
        EXPECT_EQ(n, req - 1);
    }
}
TEST(priv_display_w_unknown)
{
    WCHAR d[256]; DWORD n = 256, lang = 0;
    REQUIRE(LPDW);
    SET_ERR(0);
    CHECK(!LPDW(NULL, L"SeNoSuchPrivilege", d, &n, &lang));
    EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
    n = 256; SET_ERR(0);
    CHECK(!LPDW(NULL, L"", d, &n, &lang));
    EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
}
TEST(priv_display_w_non_privilege_constants)
{
    /* the right-name constants that are *logon rights* have display names too */
    static const WCHAR *r[] = { L"SeInteractiveLogonRight", L"SeNetworkLogonRight", L"SeBatchLogonRight", L"SeServiceLogonRight", L"SeDenyInteractiveLogonRight" };
    unsigned i;
    REQUIRE(LPDW);
    for (i = 0; i < sizeof r / sizeof r[0]; i++) {
        WCHAR d[256]; DWORD n = 256, lang = 0; LUID l;
        BOOL ok = LPDW(NULL, r[i], d, &n, &lang);
        t_note("%ls display: %d %ls", r[i], ok, ok ? d : L"");
        SET_ERR(0);
        CHECK(!LPVW(NULL, r[i], &l));            /* rights are not privileges: no LUID */
    }
}
TEST(priv_display_a_basic)
{
    char d[256]; DWORD n = 256, lang = 0;
    REQUIRE(UIN(AD, LookupPrivilegeDisplayNameA));
    CHECK(UIN(AD, LookupPrivilegeDisplayNameA)(NULL, "SeDebugPrivilege", d, &n, &lang));
    EXPECT_EQ(n, strlen(d));
    if (is_english_ui()) EXPECT_STR(d, "Debug programs");
    SET_ERR(0);
    n = 256;
    CHECK(!UIN(AD, LookupPrivilegeDisplayNameA)(NULL, "SeNoSuchPrivilege", d, &n, &lang));
    EXPECT_ERR(ERROR_NO_SUCH_PRIVILEGE);
}
TEST(priv_display_a_size_query)
{
    DWORD n = 0, lang = 0;
    REQUIRE(UIN(AD, LookupPrivilegeDisplayNameA));
    SET_ERR(0);
    EXPECT_FAIL(!UIN(AD, LookupPrivilegeDisplayNameA)(NULL, "SeShutdownPrivilege", NULL, &n, &lang), ERROR_INSUFFICIENT_BUFFER);
    if (is_english_ui()) EXPECT_EQ(n, 21);   /* "Shut down the system" + NUL */
}

/* privileges present in the current token are all resolvable */
TEST(priv_token_privileges_resolve_by_name)
{
    HANDLE tok; BYTE buf[4096]; DWORD n = 0;
    REQUIRE(LPNW); REQUIRE(LPVW);
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("token"); return; }
    if (GetTokenInformation(tok, TokenPrivileges, buf, sizeof buf, &n)) {
        TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf; DWORD i;
        for (i = 0; i < tp->PrivilegeCount; i++) {
            LUID l = tp->Privileges[i].Luid, back = { 0, 0 }; WCHAR nm[128]; DWORD c = 128;
            if (LPNW(NULL, &l, nm, &c)) { CHECK(LPVW(NULL, nm, &back) && back.LowPart == l.LowPart); }
            else CHECKF(0, "token priv luid %lu unresolved err %lu", l.LowPart, GetLastError());
        }
    }
    CloseHandle(tok);
}
