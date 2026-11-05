/* Shared helpers for the sechost_audit_policy / dx_sechost_audit_policy areas: the sechost Audit* exports (the
 * ntsecapi audit-policy API; advapi32 forwards to them on Windows 10). Everything is resolved with GetProcAddress.
 *
 * Facts every test here relies on were measured on Windows 10 22H2 (19045), elevated administrator ("adm") and
 * the restricted-admin token of the runner ("std"):
 *  - the catalog (9 categories, 59 subcategories, their GUIDs and English names) needs no privilege;
 *  - reading the system policy needs SeSecurityPrivilege in the token (enabled or not), writing it, the global
 *    SACLs and the audit security descriptor need it enabled; the right is checked before the argument values
 *    (ERROR_PRIVILEGE_NOT_HELD 1314) except for the per-user calls, which judge the account SID first;
 *  - ERROR_RPC_X_NULL_REF_POINTER (1780) is what a NULL GUID / policy pointer gives. */
#ifndef SAP_H
#define SAP_H
#include "t.h"
#include <sddl.h>

typedef struct { GUID Sub; ULONG Info; GUID Cat; } SAP_POLICY;
typedef struct { ULONG Count; PSID *Sids; } SAP_SIDS;
#define SAP_SUCCESS 1u
#define SAP_FAILURE 2u
#define SAP_NONE 4u
#define SAP_USER_NONE 0x10u
#define E_NULLREF 1780

UDECL(BOOLEAN, AuditEnumerateCategories, (GUID **, PULONG));
UDECL(BOOLEAN, AuditEnumerateSubCategories, (const GUID *, BOOLEAN, GUID **, PULONG));
UDECL(BOOLEAN, AuditLookupCategoryNameW, (const GUID *, PWSTR *));
UDECL(BOOLEAN, AuditLookupSubCategoryNameW, (const GUID *, PWSTR *));
UDECL(BOOLEAN, AuditQuerySystemPolicy, (const GUID *, ULONG, SAP_POLICY **));
UDECL(BOOLEAN, AuditSetSystemPolicy, (const SAP_POLICY *, ULONG));
UDECL(BOOLEAN, AuditQueryPerUserPolicy, (PSID, const GUID *, ULONG, SAP_POLICY **));
UDECL(BOOLEAN, AuditSetPerUserPolicy, (PSID, const SAP_POLICY *, ULONG));
UDECL(BOOLEAN, AuditComputeEffectivePolicyBySid, (PSID, const GUID *, ULONG, SAP_POLICY **));
UDECL(BOOLEAN, AuditEnumeratePerUserPolicy, (SAP_SIDS **));
UDECL(BOOLEAN, AuditQueryGlobalSaclW, (PCWSTR, PACL *));
UDECL(BOOLEAN, AuditSetGlobalSaclW, (PCWSTR, PACL));
UDECL(BOOLEAN, AuditQuerySecurity, (SECURITY_INFORMATION, PSECURITY_DESCRIPTOR *));
UDECL(BOOLEAN, AuditSetSecurity, (SECURITY_INFORMATION, PSECURITY_DESCRIPTOR));
UDECL(HLOCAL, AuditFree, (PVOID));
#define AP(name) ((name##_pfn)t_proc_in("sechost.dll", #name))
#define NEEDAP(name) name##_pfn p##name = AP(name); REQUIRE(p##name)

#define SAP_CATG(d1) {d1, 0x797A, 0x11D9, {0xBE, 0xD3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30}}

static const GUID SAP_CAT[9] = {
    SAP_CATG(0x69979848),
    SAP_CATG(0x69979849),
    SAP_CATG(0x6997984A),
    SAP_CATG(0x6997984B),
    SAP_CATG(0x6997984C),
    SAP_CATG(0x6997984D),
    SAP_CATG(0x6997984E),
    SAP_CATG(0x6997984F),
    SAP_CATG(0x69979850)
};
static const char *const SAP_CAT_NAME[9] = {"System", "Logon/Logoff", "Object Access", "Privilege Use", "Detailed Tracking", "Policy Change", "Account Management", "DS Access", "Account Logon"};
static const unsigned SAP_CAT_COUNT[9] = {5, 11, 14, 3, 6, 6, 6, 4, 4};
/* every subcategory in the order the enumerations answer: GUID data1, category index, English name */
static const struct { DWORD d1; int cat; const char *name; } SAP_SUB[59] = {
    {0x0CCE9210, 0, "Security State Change"},
    {0x0CCE9211, 0, "Security System Extension"},
    {0x0CCE9212, 0, "System Integrity"},
    {0x0CCE9213, 0, "IPsec Driver"},
    {0x0CCE9214, 0, "Other System Events"},
    {0x0CCE9215, 1, "Logon"},
    {0x0CCE9216, 1, "Logoff"},
    {0x0CCE9217, 1, "Account Lockout"},
    {0x0CCE9218, 1, "IPsec Main Mode"},
    {0x0CCE9219, 1, "IPsec Quick Mode"},
    {0x0CCE921A, 1, "IPsec Extended Mode"},
    {0x0CCE921B, 1, "Special Logon"},
    {0x0CCE921C, 1, "Other Logon/Logoff Events"},
    {0x0CCE9243, 1, "Network Policy Server"},
    {0x0CCE9247, 1, "User / Device Claims"},
    {0x0CCE9249, 1, "Group Membership"},
    {0x0CCE921D, 2, "File System"},
    {0x0CCE921E, 2, "Registry"},
    {0x0CCE921F, 2, "Kernel Object"},
    {0x0CCE9220, 2, "SAM"},
    {0x0CCE9221, 2, "Certification Services"},
    {0x0CCE9222, 2, "Application Generated"},
    {0x0CCE9223, 2, "Handle Manipulation"},
    {0x0CCE9224, 2, "File Share"},
    {0x0CCE9225, 2, "Filtering Platform Packet Drop"},
    {0x0CCE9226, 2, "Filtering Platform Connection"},
    {0x0CCE9227, 2, "Other Object Access Events"},
    {0x0CCE9244, 2, "Detailed File Share"},
    {0x0CCE9245, 2, "Removable Storage"},
    {0x0CCE9246, 2, "Central Policy Staging"},
    {0x0CCE9228, 3, "Sensitive Privilege Use"},
    {0x0CCE9229, 3, "Non Sensitive Privilege Use"},
    {0x0CCE922A, 3, "Other Privilege Use Events"},
    {0x0CCE922B, 4, "Process Creation"},
    {0x0CCE922C, 4, "Process Termination"},
    {0x0CCE922D, 4, "DPAPI Activity"},
    {0x0CCE922E, 4, "RPC Events"},
    {0x0CCE9248, 4, "Plug and Play Events"},
    {0x0CCE924A, 4, "Token Right Adjusted Events"},
    {0x0CCE922F, 5, "Audit Policy Change"},
    {0x0CCE9230, 5, "Authentication Policy Change"},
    {0x0CCE9231, 5, "Authorization Policy Change"},
    {0x0CCE9232, 5, "MPSSVC Rule-Level Policy Change"},
    {0x0CCE9233, 5, "Filtering Platform Policy Change"},
    {0x0CCE9234, 5, "Other Policy Change Events"},
    {0x0CCE9235, 6, "User Account Management"},
    {0x0CCE9236, 6, "Computer Account Management"},
    {0x0CCE9237, 6, "Security Group Management"},
    {0x0CCE9238, 6, "Distribution Group Management"},
    {0x0CCE9239, 6, "Application Group Management"},
    {0x0CCE923A, 6, "Other Account Management Events"},
    {0x0CCE923B, 7, "Directory Service Access"},
    {0x0CCE923C, 7, "Directory Service Changes"},
    {0x0CCE923D, 7, "Directory Service Replication"},
    {0x0CCE923E, 7, "Detailed Directory Service Replication"},
    {0x0CCE923F, 8, "Credential Validation"},
    {0x0CCE9240, 8, "Kerberos Service Ticket Operations"},
    {0x0CCE9241, 8, "Other Account Logon Events"},
    {0x0CCE9242, 8, "Kerberos Authentication Service"}
};
static GUID sap_sub_guid(int i) {
    GUID g = {SAP_SUB[i].d1, 0x69AE, 0x11D9, {0xBE, 0xD3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30}};
    return g;
}
static const GUID SAP_BOGUS = {0xDEADBEEF, 0x1234, 0x4321, {0x80, 0x00, 0x6f, 0x6c, 0x65, 0x74, 0x30, 0x31}};
/* indexes used by several tests */
#define SUB_LOGON 5
#define SUB_LOGOFF 6
#define SUB_LOCKOUT 7
#define CAT_LOGON 1

/* S-1-1-0, S-1-5-32-544 and an account that cannot exist (S-1-5-21-1111-2222-3333-4242) as raw bytes */
static const BYTE SAP_SID_EVERYONE[12] = {1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
static const BYTE SAP_SID_ADMINS[16] = {1, 2, 0, 0, 0, 0, 0, 5, 32, 0, 0, 0, 32, 2, 0, 0};
static const BYTE SAP_SID_FAKE[28] = {1, 5, 0, 0, 0, 0, 0, 5, 21, 0, 0, 0, 0x57, 4, 0, 0, 0xAE, 8, 0, 0, 0x05, 0x0D, 0, 0, 0x92, 0x10, 0, 0};
static const BYTE SAP_SID_BADREV[8] = {9, 9, 0, 0, 0, 0, 0, 0};

/* SeSecurityPrivilege: 0 absent, 1 present but disabled, 2 enabled */
static int sap_priv_state(void) {
    HANDLE t; int st = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return 0;
    DWORD n = 0; GetTokenInformation(t, TokenPrivileges, NULL, 0, &n);
    TOKEN_PRIVILEGES *p = n ? (TOKEN_PRIVILEGES *)LocalAlloc(LPTR, n) : NULL;
    LUID want; LookupPrivilegeValueA(NULL, "SeSecurityPrivilege", &want);
    if (p && GetTokenInformation(t, TokenPrivileges, p, n, &n)) {
        for (DWORD i = 0; i < p->PrivilegeCount; i++)
            if (p->Privileges[i].Luid.LowPart == want.LowPart && p->Privileges[i].Luid.HighPart == want.HighPart)
                st = (p->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) ? 2 : 1;
    }
    if (p) LocalFree(p);
    CloseHandle(t);
    return st;
}
/* enable or disable SeSecurityPrivilege in the process token; returns the state afterwards */
static int sap_set_priv(BOOL enable) {
    HANDLE t; TOKEN_PRIVILEGES tp;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &t)) {
        tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
        if (LookupPrivilegeValueA(NULL, "SeSecurityPrivilege", &tp.Privileges[0].Luid)) AdjustTokenPrivileges(t, FALSE, &tp, 0, NULL, NULL);
        CloseHandle(t);
    }
    return sap_priv_state();
}
static int sap_english(void) { return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_ENGLISH; }
static BOOL sap_has_guid(const GUID *a, ULONG n, const GUID *g) {
    for (ULONG i = 0; i < n; i++) if (IsEqualGUID(&a[i], g)) return TRUE;
    return FALSE;
}
static void sap_narrow(const WCHAR *w, char *out, size_t cap) { WideCharToMultiByte(CP_ACP, 0, w, -1, out, (int)cap, NULL, NULL); }

/* A local user account: sechost's own LookupAccountNameLocalW (what advapi32's LookupAccountNameW calls for the local
 * machine), else advapi32's. Tries the built-in accounts of a Windows 10 install, then the personality's. */
static BOOL sap_find_user(BYTE *sid, DWORD cap) {
    typedef BOOL (WINAPI *lk_fn)(PCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE);
    static const WCHAR *names[] = {L"Guest", L"DefaultAccount", L"WDAGUtilityAccount", L"Administrator", L"User"};
    lk_fn f = (lk_fn)t_proc_in("sechost.dll", "LookupAccountNameLocalW");
    for (int i = 0; i < 5; i++) {
        DWORD sl = cap, dl = 256; WCHAR dom[256]; SID_NAME_USE use;
        BOOL ok = f ? f(names[i], sid, &sl, dom, &dl, &use) : LookupAccountNameW(NULL, names[i], sid, &sl, dom, &dl, &use);
        if (ok && use == SidTypeUser) return TRUE;
    }
    return FALSE;
}
#endif
