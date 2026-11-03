/* sechost.dll Audit* exports (advapi32 forwards to them on Windows 10). Read-only half; the state-changing
 * calls live in tests/dx_gap_00.
 *
 * Decomp facts (sechost 19041):
 *  - parameter guards return FALSE and SetLastError(ERROR_INVALID_PARAMETER) *before* any RPC:
 *      AuditEnumerateCategories / AuditEnumerateSubCategories: out-array or out-count NULL
 *      AuditQuerySystemPolicy: ppAuditPolicy NULL (otherwise *ppAuditPolicy is zeroed first)
 *      AuditQueryPerUserPolicy / AuditComputeEffectivePolicyBySid: pSid NULL or ppAuditPolicy NULL
 *      AuditSetPerUserPolicy: pSid NULL
 *      AuditQueryGlobalSaclW / AuditSetGlobalSaclW: ObjectTypeName NULL
 *      AuditSetSecurity: RtlMakeSelfRelativeSD sizing call must return STATUS_BUFFER_TOO_SMALL, else 87
 *  - every other failure is RtlNtStatusToDosError(RPC status) -> SetLastError.
 *  - returned buffers are LocalAlloc'd (AuditFree == LocalFree, the same code as CredFree).
 * Whether the RPC calls succeed depends on the caller's privileges; those paths record NOTEs and only hard-check
 * what holds on success. */
#include "gap0.h"

#define AU(name) ((name##_pfn)t_proc_in("sechost.dll", #name))
#define NEED(name) if (t_is_wine()) { t_skip("Wine's sechost Audit* exports are aborting stubs"); return; } name##_pfn p##name = AU(name); REQUIRE(p##name)
#define SENT_ERR 0x5A5A

static const GUID CAT_SYSTEM  = {0x69979848,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_LOGON   = {0x69979849,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_OBJECT  = {0x6997984A,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_PRIV    = {0x6997984B,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_TRACK   = {0x6997984C,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_POLICY  = {0x6997984D,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_ACCMGMT = {0x6997984E,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_DSACC   = {0x6997984F,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID CAT_ACCLOGON= {0x69979850,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID SUB_LOGON   = {0x0CCE9215,0x69AE,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID SUB_LOGOFF  = {0x0CCE9216,0x69AE,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID SUB_BOGUS   = {0xDEADBEEF,0x1234,0x4321,{0x80,0x00,0x6f,0x6c,0x65,0x74,0x30,0x30}};

static const struct { const GUID *g; const char *en; } cats[] = {
    { &CAT_SYSTEM, "System" }, { &CAT_LOGON, "Logon/Logoff" }, { &CAT_OBJECT, "Object Access" },
    { &CAT_PRIV, "Privilege Use" }, { &CAT_TRACK, "Detailed Tracking" }, { &CAT_POLICY, "Policy Change" },
    { &CAT_ACCMGMT, "Account Management" }, { &CAT_DSACC, "DS Access" }, { &CAT_ACCLOGON, "Account Logon" },
};
#define NCATS ((int)(sizeof cats / sizeof cats[0]))

/* S-1-1-0 (Everyone) as raw bytes */
static const BYTE SID_EVERYONE[12] = { 1, 1, 0,0,0,0,0,1, 0,0,0,0 };

static int has_guid(const GUID *a, ULONG n, const GUID *g) {
    for (ULONG i = 0; i < n; i++) if (IsEqualGUID(&a[i], g)) return 1;
    return 0;
}

TEST(gap0_audit_exports_present) {
    static const char *names[] = { "AuditComputeEffectivePolicyBySid", "AuditEnumerateCategories", "AuditEnumeratePerUserPolicy",
        "AuditEnumerateSubCategories", "AuditFree", "AuditLookupCategoryNameW", "AuditLookupSubCategoryNameW",
        "AuditQueryGlobalSaclW", "AuditQueryPerUserPolicy", "AuditQuerySecurity", "AuditQuerySystemPolicy",
        "AuditSetGlobalSaclW", "AuditSetPerUserPolicy", "AuditSetSecurity", "AuditSetSystemPolicy" };
    if (t_is_wine() || !t_proc_in("sechost.dll", "AuditFree")) { t_skip("sechost has no Audit exports"); return; }
    for (int i = 0; i < 15; i++) {
        char buf[96]; snprintf(buf, sizeof buf, "export %s", names[i]);
        t_check(t_proc_in("sechost.dll", names[i]) != NULL, __FILE__, __LINE__, buf, NULL);
    }
}

TEST(gap0_audit_free_is_credfree) {
    NEED(AuditFree);
    /* decomp: AuditFree and CredFree share RVA 0xea10 (both are a tail jump to LocalFree) */
    CHECK(t_proc_in("sechost.dll", "CredFree") == t_proc_in("sechost.dll", "AuditFree"));
}

TEST(gap0_audit_free_null_and_heap) {
    NEED(AuditFree);
    EXPECT_EQ(pAuditFree(NULL), NULL);
    for (int sz = 1; sz <= 4096; sz *= 4) {
        void *p = LocalAlloc(LPTR, sz);
        CHECK(p != NULL);
        if (p) EXPECT_EQ(pAuditFree(p), NULL);   /* LocalFree success returns NULL */
    }
    /* LocalAlloc(LMEM_MOVEABLE) handle freed through AuditFree too (it is LocalFree) */
    HLOCAL h = LocalAlloc(LMEM_MOVEABLE, 32);
    CHECK(h != NULL);
    if (h) EXPECT_EQ(pAuditFree(h), NULL);
}

/* ---- parameter validation: every case is rejected before RPC with ERROR_INVALID_PARAMETER ---- */
TEST(gap0_audit_enum_categories_null_args) {
    NEED(AuditEnumerateCategories);
    GUID *arr = (GUID *)(ULONG_PTR)0x1; ULONG cnt = 0x77;
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditEnumerateCategories(NULL, &cnt), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditEnumerateCategories(&arr, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditEnumerateCategories(NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(arr, 0x1);   /* outputs untouched on rejection */
    EXPECT_EQ(cnt, 0x77);
}

TEST(gap0_audit_enum_subcategories_null_args) {
    NEED(AuditEnumerateSubCategories);
    GUID *arr = (GUID *)(ULONG_PTR)0x1; ULONG cnt = 0x77;
    for (int all = 0; all <= 1; all++) {
        SET_ERR(SENT_ERR); EXPECT_EQ(pAuditEnumerateSubCategories(&CAT_LOGON, (BOOLEAN)all, NULL, &cnt), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(SENT_ERR); EXPECT_EQ(pAuditEnumerateSubCategories(&CAT_LOGON, (BOOLEAN)all, &arr, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(SENT_ERR); EXPECT_EQ(pAuditEnumerateSubCategories(NULL, (BOOLEAN)all, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    EXPECT_EQ(arr, 0x1); EXPECT_EQ(cnt, 0x77);
}

TEST(gap0_audit_query_system_policy_null_out) {
    NEED(AuditQuerySystemPolicy);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQuerySystemPolicy(&SUB_LOGON, 1, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQuerySystemPolicy(NULL, 0, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap0_audit_per_user_null_sid_or_out) {
    NEED(AuditQueryPerUserPolicy); NEED(AuditSetPerUserPolicy); NEED(AuditComputeEffectivePolicyBySid);
    G_AUDIT_POLICY *pol = (G_AUDIT_POLICY *)(ULONG_PTR)0x1;
    G_AUDIT_POLICY in = { SUB_LOGON, G_AUDIT_SUCCESS, CAT_LOGON };
    SID *sid = (SID *)SID_EVERYONE;
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQueryPerUserPolicy(NULL, &SUB_LOGON, 1, &pol), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQueryPerUserPolicy(sid, &SUB_LOGON, 1, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQueryPerUserPolicy(NULL, NULL, 0, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(pol, 0x1);   /* rejected before the zero-initialisation of *ppAuditPolicy */
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetPerUserPolicy(NULL, &in, 1), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetPerUserPolicy(NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditComputeEffectivePolicyBySid(NULL, &SUB_LOGON, 1, &pol), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditComputeEffectivePolicyBySid(sid, &SUB_LOGON, 1, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditComputeEffectivePolicyBySid(NULL, NULL, 0, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(pol, 0x1);
}

TEST(gap0_audit_global_sacl_null_name) {
    NEED(AuditQueryGlobalSaclW); NEED(AuditSetGlobalSaclW);
    PACL acl = (PACL)(ULONG_PTR)0x1;
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQueryGlobalSaclW(NULL, &acl), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditQueryGlobalSaclW(NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(acl, 0x1);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetGlobalSaclW(NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    BYTE aclbuf[64]; InitializeAcl((PACL)aclbuf, sizeof aclbuf, ACL_REVISION);
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetGlobalSaclW(NULL, (PACL)aclbuf), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap0_audit_set_security_bad_descriptor) {
    NEED(AuditSetSecurity);
    SECURITY_DESCRIPTOR sd;
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    /* unknown revision: RtlMakeSelfRelativeSD fails with something other than STATUS_BUFFER_TOO_SMALL -> 87 */
    ((BYTE *)&sd)[0] = 7;
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetSecurity(SACL_SECURITY_INFORMATION, &sd), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ((BYTE *)&sd)[0] = 0;
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetSecurity(DACL_SECURITY_INFORMATION, &sd), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* an already self-relative descriptor is rejected by RtlMakeSelfRelativeSD (STATUS_BAD_DESCRIPTOR_FORMAT) */
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    BYTE rel[256]; DWORD len = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &len));
    SET_ERR(SENT_ERR); EXPECT_EQ(pAuditSetSecurity(SACL_SECURITY_INFORMATION, rel), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---- RPC-backed read paths ---- */
TEST(gap0_audit_enum_categories) {
    NEED(AuditEnumerateCategories); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    GUID *arr = NULL; ULONG cnt = 0;
    SET_ERR(0);
    if (!pAuditEnumerateCategories(&arr, &cnt)) {
        t_note("AuditEnumerateCategories failed, GetLastError=%lu (privilege/SKU dependent)", GetLastError());
        EXPECT_EQ(arr, NULL); EXPECT_EQ(cnt, 0);
        return;
    }
    CHECK(arr != NULL);
    CHECK(cnt >= 9);
    t_note("category count = %lu (Windows 10 ships 9)", cnt);
    if (!arr) return;
    for (int i = 0; i < NCATS; i++) CHECKF(has_guid(arr, cnt, cats[i].g), "category %d (%s) missing", i, cats[i].en);
    for (ULONG i = 0; i < cnt; i++) for (ULONG j = i + 1; j < cnt; j++) CHECK(!IsEqualGUID(&arr[i], &arr[j]));   /* unique */
    EXPECT_EQ(LocalSize(arr), cnt * sizeof(GUID));          /* LocalAlloc(LPTR, count*16) */
    EXPECT_EQ(pAuditFree(arr), NULL);
}

TEST(gap0_audit_enum_categories_repeatable) {
    NEED(AuditEnumerateCategories); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    GUID *a = NULL, *b = NULL; ULONG na = 0, nb = 0;
    if (!pAuditEnumerateCategories(&a, &na) || !pAuditEnumerateCategories(&b, &nb)) { t_note("enumerate failed (%lu)", GetLastError()); return; }
    EXPECT_EQ(na, nb);
    CHECK(a != b);                                           /* each call returns a fresh allocation */
    if (a && b && na == nb) CHECK(!memcmp(a, b, na * sizeof(GUID)));   /* stable order */
    pAuditFree(a); pAuditFree(b);
}

TEST(gap0_audit_enum_subcategories) {
    NEED(AuditEnumerateSubCategories); NEED(AuditEnumerateCategories); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    GUID *all = NULL; ULONG nall = 0;
    if (!pAuditEnumerateSubCategories(NULL, TRUE, &all, &nall)) { t_note("enumerate all subcategories failed (%lu)", GetLastError()); return; }
    CHECK(all != NULL);
    t_note("subcategory count = %lu", nall);
    CHECK(nall >= 50);
    if (!all) return;
    CHECK(has_guid(all, nall, &SUB_LOGON));
    CHECK(has_guid(all, nall, &SUB_LOGOFF));
    EXPECT_EQ(LocalSize(all), nall * sizeof(GUID));
    /* per category: subsets of the full list, no category GUID leaks in, and they add up to the total */
    ULONG sum = 0;
    for (int i = 0; i < NCATS; i++) {
        GUID *sub = NULL; ULONG n = 0;
        if (!pAuditEnumerateSubCategories(cats[i].g, FALSE, &sub, &n)) { t_note("category %s: subcategories failed (%lu)", cats[i].en, GetLastError()); continue; }
        CHECKF(n >= 1, "category %s has no subcategories", cats[i].en);
        for (ULONG k = 0; k < n; k++) CHECK(has_guid(all, nall, &sub[k]));
        sum += n;
        if (i == 1) { CHECK(has_guid(sub, n, &SUB_LOGON)); CHECK(has_guid(sub, n, &SUB_LOGOFF)); }
        pAuditFree(sub);
    }
    if (sum) EXPECT_EQ(sum, nall);
    pAuditFree(all);
}

TEST(gap0_audit_enum_subcategories_unknown_category) {
    NEED(AuditEnumerateSubCategories);
    g_enable_priv("SeSecurityPrivilege");
    GUID *sub = NULL; ULONG n = 0x99;
    SET_ERR(0);
    BOOLEAN r = pAuditEnumerateSubCategories(&SUB_BOGUS, FALSE, &sub, &n);
    t_note("unknown category GUID: result=%d err=%lu count=%lu", r, GetLastError(), n);
    if (r) { CHECK(n == 0 || sub != NULL); if (sub) { AU(AuditFree) ? AU(AuditFree)(sub) : 0; } }
    else { EXPECT_EQ(sub, NULL); }
}

TEST(gap0_audit_lookup_category_names) {
    NEED(AuditLookupCategoryNameW); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    for (int i = 0; i < NCATS; i++) {
        PWSTR name = NULL;
        SET_ERR(0);
        if (!pAuditLookupCategoryNameW(cats[i].g, &name)) { t_note("lookup category %d failed (%lu)", i, GetLastError()); EXPECT_EQ(name, NULL); continue; }
        CHECK(name != NULL);
        if (!name) continue;
        size_t len = lstrlenW(name);
        CHECK(len > 0);
        CHECK(LocalSize(name) >= (len + 1) * sizeof(WCHAR));
        char a[128]; WideCharToMultiByte(CP_ACP, 0, name, -1, a, sizeof a, NULL, NULL);
        if (strcmp(a, cats[i].en)) t_note("category %d name \"%s\" (en-US expected \"%s\"; locale dependent)", i, a, cats[i].en);
        pAuditFree(name);
    }
}

TEST(gap0_audit_lookup_subcategory_names) {
    NEED(AuditLookupSubCategoryNameW); NEED(AuditEnumerateSubCategories); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    PWSTR n1 = NULL, n2 = NULL;
    SET_ERR(0);
    if (!pAuditLookupSubCategoryNameW(&SUB_LOGON, &n1)) { t_note("lookup Logon subcategory failed (%lu)", GetLastError()); return; }
    CHECK(n1 && lstrlenW(n1) > 0);
    if (n1) t_note("Logon subcategory name: %ls", n1);
    if (pAuditLookupSubCategoryNameW(&SUB_LOGOFF, &n2) && n1 && n2) CHECK(lstrcmpW(n1, n2) != 0);
    /* every enumerated subcategory resolves to a distinct non-empty name */
    GUID *all = NULL; ULONG nall = 0;
    if (pAuditEnumerateSubCategories(NULL, TRUE, &all, &nall) && all) {
        for (ULONG i = 0; i < nall && i < 100; i++) {
            PWSTR nm = NULL;
            if (!pAuditLookupSubCategoryNameW(&all[i], &nm)) { CHECKF(0, "lookup of enumerated subcategory %lu failed: %lu", i, GetLastError()); continue; }
            CHECK(nm && lstrlenW(nm) > 0);
            pAuditFree(nm);
        }
        pAuditFree(all);
    }
    pAuditFree(n1); pAuditFree(n2);
}

TEST(gap0_audit_lookup_unknown_guid) {
    NEED(AuditLookupCategoryNameW); NEED(AuditLookupSubCategoryNameW);
    g_enable_priv("SeSecurityPrivilege");
    PWSTR n = NULL;
    SET_ERR(0);
    BOOLEAN r = pAuditLookupCategoryNameW(&SUB_BOGUS, &n);
    t_note("category lookup of unknown GUID: result=%d err=%lu", r, GetLastError());
    if (!r) EXPECT_EQ(n, NULL); else if (n) LocalFree(n);
    n = NULL; SET_ERR(0);
    r = pAuditLookupSubCategoryNameW(&SUB_BOGUS, &n);
    t_note("subcategory lookup of unknown GUID: result=%d err=%lu", r, GetLastError());
    if (!r) EXPECT_EQ(n, NULL); else if (n) LocalFree(n);
    /* a category GUID is not a subcategory GUID and vice versa (informational) */
    n = NULL;
    r = pAuditLookupSubCategoryNameW(&CAT_LOGON, &n);
    t_note("subcategory lookup of a category GUID: result=%d err=%lu", r, GetLastError());
    if (r && n) LocalFree(n);
}

TEST(gap0_audit_query_system_policy) {
    NEED(AuditQuerySystemPolicy); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    GUID req[2] = { SUB_LOGON, SUB_LOGOFF };
    G_AUDIT_POLICY *pol = (G_AUDIT_POLICY *)(ULONG_PTR)0x1;
    SET_ERR(0);
    if (!pAuditQuerySystemPolicy(req, 2, &pol)) {
        DWORD e = GetLastError();
        t_note("AuditQuerySystemPolicy failed, GetLastError=%lu (needs audit-policy read right)", e);
        EXPECT_EQ(pol, NULL);   /* *ppAuditPolicy is zeroed before the RPC */
        return;
    }
    CHECK(pol != NULL);
    if (!pol) return;
    for (int i = 0; i < 2; i++) {
        CHECK(IsEqualGUID(&pol[i].AuditSubCategoryGuid, &req[i]));    /* same order as requested */
        CHECKF((pol[i].AuditingInformation & ~7u) == 0, "info 0x%lx", pol[i].AuditingInformation);
        if (!IsEqualGUID(&pol[i].AuditCategoryGuid, &CAT_LOGON)) t_note("policy[%d] category GUID is not Logon/Logoff", i);
    }
    t_note("Logon=0x%lx Logoff=0x%lx", pol[0].AuditingInformation, pol[1].AuditingInformation);
    pAuditFree(pol);
}

TEST(gap0_audit_query_system_policy_single_and_unknown) {
    NEED(AuditQuerySystemPolicy); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    G_AUDIT_POLICY *pol = (G_AUDIT_POLICY *)(ULONG_PTR)0x1;
    SET_ERR(0);
    if (pAuditQuerySystemPolicy(&SUB_LOGON, 1, &pol)) {
        CHECK(pol && IsEqualGUID(&pol[0].AuditSubCategoryGuid, &SUB_LOGON));
        pAuditFree(pol);
    } else { EXPECT_EQ(pol, NULL); t_note("single query failed (%lu)", GetLastError()); return; }
    pol = (G_AUDIT_POLICY *)(ULONG_PTR)0x1; SET_ERR(0);
    BOOLEAN r = pAuditQuerySystemPolicy(&SUB_BOGUS, 1, &pol);
    t_note("unknown subcategory: result=%d err=%lu", r, GetLastError());
    if (r) pAuditFree(pol); else EXPECT_EQ(pol, NULL);
}

TEST(gap0_audit_query_per_user_policy_everyone) {
    NEED(AuditQueryPerUserPolicy); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    G_AUDIT_POLICY *pol = (G_AUDIT_POLICY *)(ULONG_PTR)0x1;
    SET_ERR(0);
    if (!pAuditQueryPerUserPolicy((PSID)SID_EVERYONE, &SUB_LOGON, 1, &pol)) {
        t_note("per-user query for Everyone failed, GetLastError=%lu (no per-user policy is the usual state)", GetLastError());
        EXPECT_EQ(pol, NULL);
        return;
    }
    CHECK(pol != NULL);
    if (pol) { CHECK(IsEqualGUID(&pol[0].AuditSubCategoryGuid, &SUB_LOGON)); pAuditFree(pol); }
}

TEST(gap0_audit_compute_effective_policy) {
    NEED(AuditComputeEffectivePolicyBySid); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    G_AUDIT_POLICY *pol = (G_AUDIT_POLICY *)(ULONG_PTR)0x1;
    SET_ERR(0);
    BOOLEAN r = pAuditComputeEffectivePolicyBySid((PSID)SID_EVERYONE, &SUB_LOGON, 1, &pol);
    t_note("AuditComputeEffectivePolicyBySid: result=%d err=%lu", r, GetLastError());
    /* decomp: on success *ppAuditPolicy is stored as NULL (the stub's result buffer is freed internally) */
    if (r) EXPECT_EQ(pol, NULL);
}

TEST(gap0_audit_enum_per_user_policy) {
    NEED(AuditEnumeratePerUserPolicy); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    G_AUDIT_SID_ARRAY *arr = NULL;
    SET_ERR(0);
    if (!pAuditEnumeratePerUserPolicy(&arr)) { t_note("AuditEnumeratePerUserPolicy failed, GetLastError=%lu (2/259/1314/5 are usual when none are configured or no privilege)", GetLastError()); return; }
    CHECK(arr != NULL);
    if (!arr) return;
    for (ULONG i = 0; i < arr->UsersCount; i++) CHECK(arr->UserSidArray[i] && IsValidSid(arr->UserSidArray[i]));
    t_note("per-user policy SIDs: %lu", arr->UsersCount);
    pAuditFree(arr);
}

TEST(gap0_audit_query_global_sacl) {
    NEED(AuditQueryGlobalSaclW); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    static const WCHAR *types[] = { L"File", L"Key", L"NoSuchObjectType_olet" };
    for (int i = 0; i < 3; i++) {
        PACL acl = NULL;
        SET_ERR(0);
        if (pAuditQueryGlobalSaclW(types[i], &acl)) {
            CHECK(acl != NULL);
            if (acl) { CHECK(IsValidAcl(acl)); pAuditFree(acl); }
        } else {
            t_note("global SACL query for %ls failed, GetLastError=%lu", types[i], GetLastError());
        }
    }
}

TEST(gap0_audit_query_security) {
    NEED(AuditQuerySecurity); NEED(AuditFree);
    g_enable_priv("SeSecurityPrivilege");
    PSECURITY_DESCRIPTOR sd = NULL;
    SET_ERR(0);
    if (!pAuditQuerySecurity(DACL_SECURITY_INFORMATION, &sd)) {
        t_note("AuditQuerySecurity(DACL) failed, GetLastError=%lu", GetLastError());
        return;
    }
    if (sd) { CHECK(IsValidSecurityDescriptor(sd)); pAuditFree(sd); }
    else t_note("AuditQuerySecurity returned success with a NULL descriptor");
}
