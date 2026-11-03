/* Shared helpers for gap_00 / dx_gap_00 (public exports with no prior coverage).
 * Everything is resolved with LoadLibrary/GetProcAddress (UN/UIN); nothing is linked. */
#ifndef GAP0_H
#define GAP0_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <rpc.h>
#include <rpcndr.h>
#include <sddl.h>

/* HRESULT values spelled out so the expectations do not depend on header coverage */
#define G_E_INVALIDARG     ((HRESULT)0x80070057)
#define G_E_ACCESSDENIED   ((HRESULT)0x80070005)
#define G_E_OUTOFMEMORY    ((HRESULT)0x8007000E)
#define G_E_FAIL           ((HRESULT)0x80004005)
#define G_CO_E_NOTINIT     ((HRESULT)0x800401F0)
#define G_CO_E_NOTSUPPORTED ((HRESULT)0x80004021)
#define G_CO_E_OBJNOTREG   ((HRESULT)0x800401FB)
#define G_CO_E_OBJISREG    ((HRESULT)0x800401FC)
#define G_RPC_E_NO_CONTEXT ((HRESULT)0x8001011E)
#define G_CO_E_APPNOTFOUND ((HRESULT)0x80040164)

/* ---- audit policy types (ntsecapi.h) ---- */
typedef struct { GUID AuditSubCategoryGuid; ULONG AuditingInformation; GUID AuditCategoryGuid; } G_AUDIT_POLICY;
typedef struct { ULONG UsersCount; PSID *UserSidArray; } G_AUDIT_SID_ARRAY;
#define G_AUDIT_SUCCESS 1u
#define G_AUDIT_FAILURE 2u
#define G_AUDIT_NONE    4u

UDECL(BOOLEAN, AuditEnumerateCategories, (GUID **, PULONG));
UDECL(BOOLEAN, AuditEnumerateSubCategories, (const GUID *, BOOLEAN, GUID **, PULONG));
UDECL(BOOLEAN, AuditLookupCategoryNameW, (const GUID *, PWSTR *));
UDECL(BOOLEAN, AuditLookupSubCategoryNameW, (const GUID *, PWSTR *));
UDECL(BOOLEAN, AuditQuerySystemPolicy, (const GUID *, ULONG, G_AUDIT_POLICY **));
UDECL(BOOLEAN, AuditSetSystemPolicy, (const G_AUDIT_POLICY *, ULONG));
UDECL(BOOLEAN, AuditQueryPerUserPolicy, (PSID, const GUID *, ULONG, G_AUDIT_POLICY **));
UDECL(BOOLEAN, AuditSetPerUserPolicy, (PSID, const G_AUDIT_POLICY *, ULONG));
UDECL(BOOLEAN, AuditComputeEffectivePolicyBySid, (PSID, const GUID *, ULONG, G_AUDIT_POLICY **));
UDECL(BOOLEAN, AuditEnumeratePerUserPolicy, (G_AUDIT_SID_ARRAY **));
UDECL(BOOLEAN, AuditQueryGlobalSaclW, (PCWSTR, PACL *));
UDECL(BOOLEAN, AuditSetGlobalSaclW, (PCWSTR, PACL));
UDECL(BOOLEAN, AuditQuerySecurity, (SECURITY_INFORMATION, PSECURITY_DESCRIPTOR *));
UDECL(BOOLEAN, AuditSetSecurity, (SECURITY_INFORMATION, PSECURITY_DESCRIPTOR));
UDECL(HLOCAL,  AuditFree, (PVOID));

/* ---- child-process checks: failures are counted, the exit code reports them ---- */
static int g_cf;
#define C(x) do { if (!(x)) { printf("  child check failed %s:%d: %s\n", __FILE__, __LINE__, #x); g_cf++; } } while (0)
#define CEQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ != b_) { printf("  child check failed %s:%d: %s == %s (got 0x%llx want 0x%llx)\n", __FILE__, __LINE__, #a, #b, a_, b_); g_cf++; } } while (0)
#define CEQH(a, b) CEQ((ULONG)(a), (ULONG)(b))
#define CHILD_RC() (g_cf ? 100 + (g_cf > 100 ? 100 : g_cf) : 0)
#define RUN_CHILD(n) EXPECT_EQ(t_run_child(#n, "", 30000), 0)

/* HRESULT checks in the parent that print hex values */
#define EXPECT_HR(a, b) do { unsigned long a_ = (unsigned long)(a), b_ = (unsigned long)(b); \
    t_check((unsigned long)(unsigned int)a_ == (unsigned long)(unsigned int)b_, __FILE__, __LINE__, #a " == " #b, "got 0x%08lx want 0x%08lx", (unsigned long)(unsigned int)a_, (unsigned long)(unsigned int)b_); } while (0)

static inline BOOL g_enable_priv(const char *name) {
    HANDLE tok; TOKEN_PRIVILEGES tp; BOOL ok = FALSE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return FALSE;
    tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueA(NULL, name, &tp.Privileges[0].Luid)) {
        SetLastError(0);
        ok = AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL) && GetLastError() == 0;
    }
    CloseHandle(tok);
    return ok;
}

#define WINE_STUB() do { if (t_is_wine()) { t_skip("export is an aborting stub (or differs) under Wine"); return; } } while (0)
#define NEEDN(name) name##_pfn p##name = UN(name); REQUIRE(p##name)
#define CNEED(name) name##_pfn p##name = UN(name); if (!p##name) return 0
#endif
