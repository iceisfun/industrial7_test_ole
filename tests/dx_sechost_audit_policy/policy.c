/* State-changing Audit* behaviour: the system policy write rules, the per-user policy, the effective policy, the
 * global SACLs and the audit security descriptor. dx_ area: needs an elevated token whose SeSecurityPrivilege can be
 * enabled (t_skip otherwise); everything a test changes is restored in a finally-style tail.
 *
 * Windows 10 22H2 facts (measured): AuditSetSystemPolicy validates the whole batch first (an unknown subcategory
 * GUID or a bit outside SUCCESS|FAILURE|NONE refuses everything: 87); then every entry carrying NONE clears its
 * subcategory, then every entry ORs its SUCCESS/FAILURE bits in; the category GUID of an entry is not looked at.
 * AuditSetPerUserPolicy applies entry by entry (NONE = 0x10 clears first, then the low four bits OR in), flags above
 * 0x1f are 87. The effective policy is (system | include) & ~exclude. */
#include "../sechost_audit_policy/sap.h"

#define GATE() do { if (t_is_wine()) { t_skip("Wine's Audit* exports are aborting stubs"); return; } \
    if (sap_set_priv(TRUE) != 2) { t_skip("SeSecurityPrivilege cannot be enabled (not elevated)"); return; } } while (0)

static ULONG sys_get(int sub, DWORD *err) {
    AuditQuerySystemPolicy_pfn q = AP(AuditQuerySystemPolicy); GUID g = sap_sub_guid(sub);
    SAP_POLICY *p = NULL; ULONG v = 0xdead;
    SET_ERR(0);
    if (q(&g, 1, &p) && p) { v = p->Info; AP(AuditFree)(p); }
    if (err) *err = GetLastError();
    return v;
}
static BOOLEAN sys_set(int sub, ULONG info) {
    SAP_POLICY e = {sap_sub_guid(sub), info, SAP_CAT[SAP_SUB[sub].cat]};
    return AP(AuditSetSystemPolicy)(&e, 1);
}
/* exact write: NONE clears, then the bits */
static void sys_restore(int sub, ULONG v) { sys_set(sub, v | SAP_NONE); }

TEST(dx_sap_system_policy_write_table) {
    GATE();
    ULONG orig = sys_get(SUB_LOGON, NULL);
    t_note("original Logon policy 0x%lx", orig);
    static const struct { ULONG in, after_clean; } T[] = {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {5, 1}, {6, 2}, {7, 3}, {4, 0}};
    for (int i = 0; i < 8; i++) {
        sys_set(SUB_LOGON, SAP_NONE);                                /* clean state */
        EXPECT_EQ(sys_get(SUB_LOGON, NULL), 0);
        SET_ERR(0x5a5a);
        CHECKF(sys_set(SUB_LOGON, T[i].in), "set %lu failed %lu", T[i].in, GetLastError());
        EXPECT_EQ(sys_get(SUB_LOGON, NULL), T[i].after_clean);
    }
    /* bits OR into what is there; 0 changes nothing */
    sys_set(SUB_LOGON, SAP_NONE); sys_set(SUB_LOGON, 1);
    sys_set(SUB_LOGON, 2); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 3);
    sys_set(SUB_LOGON, 0); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 3);
    sys_set(SUB_LOGON, 1); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 3);
    /* the category GUID of the entry is not looked at */
    sys_set(SUB_LOGON, SAP_NONE);
    SAP_POLICY odd[2] = {{sap_sub_guid(SUB_LOGON), 1, SAP_CAT[0]}, {sap_sub_guid(SUB_LOGOFF), 0, {0}}};
    SET_ERR(0x5a5a); CHECKF(AP(AuditSetSystemPolicy)(odd, 2), "wrong category GUIDs refused %lu", GetLastError());
    EXPECT_EQ(sys_get(SUB_LOGON, NULL), 1);
    sys_restore(SUB_LOGON, orig);
    EXPECT_EQ(sys_get(SUB_LOGON, NULL), orig);
}

TEST(dx_sap_system_policy_batches) {
    GATE();
    ULONG o1 = sys_get(SUB_LOGON, NULL), o2 = sys_get(SUB_LOGOFF, NULL), o3 = sys_get(SUB_LOCKOUT, NULL);
    sys_set(SUB_LOGON, SAP_NONE); sys_set(SUB_LOGOFF, SAP_NONE); sys_set(SUB_LOCKOUT, SAP_NONE);
    GUID l1 = sap_sub_guid(SUB_LOGON), l2 = sap_sub_guid(SUB_LOGOFF), lk = sap_sub_guid(SUB_LOCKOUT);
    GUID bogus = SAP_BOGUS;
    #define E(g, v) {g, v, SAP_CAT[CAT_LOGON]}
    /* a bad entry anywhere refuses the whole batch */
    { SAP_POLICY b[2] = {E(l1, 1), E(bogus, 1)}; SET_ERR(0x5a5a); CHECK(!AP(AuditSetSystemPolicy)(b, 2)); EXPECT_ERR(87); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 0); }
    { SAP_POLICY b[2] = {E(bogus, 1), E(l1, 1)}; SET_ERR(0x5a5a); CHECK(!AP(AuditSetSystemPolicy)(b, 2)); EXPECT_ERR(87); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 0); }
    { SAP_POLICY b[2] = {E(l1, 1), E(l2, 8)}; SET_ERR(0x5a5a); CHECK(!AP(AuditSetSystemPolicy)(b, 2)); EXPECT_ERR(87); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 0); }
    { SAP_POLICY b[1] = {E(l1, 1)}; SET_ERR(0x5a5a); CHECK(!AP(AuditSetSystemPolicy)(b, 0)); EXPECT_ERR(87); }
    /* only `count` entries are read */
    { SAP_POLICY b[2] = {E(l1, 1), E(l2, 2)}; CHECK(AP(AuditSetSystemPolicy)(b, 1)); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 1); EXPECT_EQ(sys_get(SUB_LOGOFF, NULL), 0); sys_set(SUB_LOGON, SAP_NONE); }
    /* two phases: every NONE clears first, then every entry ORs its bits */
    { SAP_POLICY b[2] = {E(l1, 3), E(l1, 6)}; CHECK(AP(AuditSetSystemPolicy)(b, 2)); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 3); sys_set(SUB_LOGON, SAP_NONE); }
    { SAP_POLICY b[2] = {E(l1, 6), E(l1, 3)}; CHECK(AP(AuditSetSystemPolicy)(b, 2)); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 3); sys_set(SUB_LOGON, SAP_NONE); }
    { SAP_POLICY b[3] = {E(l1, 1), E(l2, 2), E(l1, 4)}; CHECK(AP(AuditSetSystemPolicy)(b, 3)); EXPECT_EQ(sys_get(SUB_LOGON, NULL), 1); EXPECT_EQ(sys_get(SUB_LOGOFF, NULL), 2); }
    { SAP_POLICY b[3] = {E(l1, 2), E(l2, 1), E(lk, 3)}; sys_set(SUB_LOGON, SAP_NONE); CHECK(AP(AuditSetSystemPolicy)(b, 3));
      EXPECT_EQ(sys_get(SUB_LOGON, NULL), 2); EXPECT_EQ(sys_get(SUB_LOGOFF, NULL), 3); EXPECT_EQ(sys_get(SUB_LOCKOUT, NULL), 3); }
    #undef E
    sys_restore(SUB_LOGON, o1); sys_restore(SUB_LOGOFF, o2); sys_restore(SUB_LOCKOUT, o3);
    EXPECT_EQ(sys_get(SUB_LOGON, NULL), o1); EXPECT_EQ(sys_get(SUB_LOGOFF, NULL), o2); EXPECT_EQ(sys_get(SUB_LOCKOUT, NULL), o3);
}

/* a local user account that exists on every Windows 10 install (and on the personality: Administrator) */
static BYTE g_sid[SECURITY_MAX_SID_SIZE];
static BOOL find_user(void) { return sap_find_user(g_sid, sizeof g_sid); }
static BOOLEAN user_set(int sub, ULONG flags) {
    SAP_POLICY e = {sap_sub_guid(sub), flags, SAP_CAT[SAP_SUB[sub].cat]};
    return AP(AuditSetPerUserPolicy)((PSID)g_sid, &e, 1);
}
/* the flags of two subcategories, or a Win32 error in *err when the call fails (0 when it works) */
static ULONG user_get(int sub, DWORD *err) {
    GUID g = sap_sub_guid(sub); SAP_POLICY *p = NULL; ULONG v = 0xdead;
    SET_ERR(0);
    if (AP(AuditQueryPerUserPolicy)((PSID)g_sid, &g, 1, &p) && p) { v = p->Info; AP(AuditFree)(p); }
    *err = GetLastError();
    return v;
}
static BOOL user_listed(void) {
    SAP_SIDS *s = NULL; BOOL found = FALSE;
    if (AP(AuditEnumeratePerUserPolicy)(&s) && s) {
        for (ULONG i = 0; i < s->Count; i++) if (EqualSid(s->Sids[i], (PSID)g_sid)) found = TRUE;
        AP(AuditFree)(s);
    }
    return found;
}
static void user_wipe(void) { user_set(SUB_LOGON, SAP_USER_NONE); user_set(SUB_LOGOFF, SAP_USER_NONE); user_set(SUB_LOCKOUT, SAP_USER_NONE); }

TEST(dx_sap_per_user_policy_semantics) {
    GATE();
    if (!find_user()) { t_skip("no local user account"); return; }
    user_wipe();
    DWORD e;
    /* an account without entries: 0 creates nothing, the query is ERROR_FILE_NOT_FOUND */
    CHECK(user_set(SUB_LOGON, 0));
    user_get(SUB_LOGON, &e); EXPECT_EQ(e, 2);
    CHECK(!user_listed());
    /* the flags OR in */
    static const struct { ULONG in, now; } T[] = {{1, 1}, {2, 3}, {3, 3}, {4, 7}, {8, 15}, {0xc, 15}};
    for (int i = 0; i < 6; i++) {
        SET_ERR(0x5a5a); CHECKF(user_set(SUB_LOGON, T[i].in), "set 0x%lx failed %lu", T[i].in, GetLastError());
        EXPECT_EQ(user_get(SUB_LOGON, &e), T[i].now); EXPECT_EQ(e, 0);
    }
    CHECK(user_listed());
    /* NONE clears the subcategory; with no subcategory left the account is gone */
    CHECK(user_set(SUB_LOGON, SAP_USER_NONE));
    user_get(SUB_LOGON, &e); EXPECT_EQ(e, 2);
    CHECK(!user_listed());
    CHECK(user_set(SUB_LOGON, 5)); CHECK(user_set(SUB_LOGON, 0)); CHECK(user_set(SUB_LOGON, 1));
    EXPECT_EQ(user_get(SUB_LOGON, &e), 5);
    CHECK(user_set(SUB_LOGON, SAP_USER_NONE | 3));                  /* clear, then 3 */
    EXPECT_EQ(user_get(SUB_LOGON, &e), 3);
    { SAP_POLICY b[2] = {{sap_sub_guid(SUB_LOGON), 1, SAP_CAT[CAT_LOGON]}, {sap_sub_guid(SUB_LOGON), SAP_USER_NONE, SAP_CAT[CAT_LOGON]}};
      CHECK(AP(AuditSetPerUserPolicy)((PSID)g_sid, b, 2)); user_get(SUB_LOGON, &e); EXPECT_EQ(e, 2); }          /* in order: set, then clear */
    /* a subcategory with no entry of its own reads 0 while the account has another */
    CHECK(user_set(SUB_LOGOFF, 6));
    EXPECT_EQ(user_get(SUB_LOGON, &e), 0); EXPECT_EQ(e, 0);
    EXPECT_EQ(user_get(SUB_LOGOFF, &e), 6);
    { GUID req[2] = {sap_sub_guid(SUB_LOGOFF), sap_sub_guid(SUB_LOGON)}; SAP_POLICY *p = NULL;
      CHECK(AP(AuditQueryPerUserPolicy)((PSID)g_sid, req, 2, &p));
      if (p) { EXPECT_EQ(p[0].Info, 6); EXPECT_EQ(p[1].Info, 0); CHECK(IsEqualGUID(&p[0].Sub, &req[0])); CHECK(IsEqualGUID(&p[1].Cat, &SAP_CAT[CAT_LOGON])); AP(AuditFree)(p); } }
    /* refused values change nothing */
    SET_ERR(0x5a5a); CHECK(!user_set(SUB_LOGOFF, 0x20)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!user_set(SUB_LOGOFF, 0xff)); EXPECT_ERR(87);
    EXPECT_EQ(user_get(SUB_LOGOFF, &e), 6);
    { SAP_POLICY b[2] = {{sap_sub_guid(SUB_LOGON), 1, SAP_CAT[CAT_LOGON]}, {SAP_BOGUS, 1, SAP_CAT[CAT_LOGON]}};
      SET_ERR(0x5a5a); CHECK(!AP(AuditSetPerUserPolicy)((PSID)g_sid, b, 2)); EXPECT_ERR(87); EXPECT_EQ(user_get(SUB_LOGON, &e), 0); }
    { SAP_POLICY b = {sap_sub_guid(SUB_LOGON), 1, SAP_CAT[CAT_LOGON]}; SET_ERR(0x5a5a); CHECK(!AP(AuditSetPerUserPolicy)((PSID)g_sid, &b, 0)); EXPECT_ERR(87); }
    { GUID bad = SAP_BOGUS; SAP_POLICY *p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!AP(AuditQueryPerUserPolicy)((PSID)g_sid, &bad, 1, &p)); EXPECT_ERR(87); }
    user_wipe();
    CHECK(!user_listed());
}

TEST(dx_sap_per_user_policy_enumeration) {
    GATE();
    if (!find_user()) { t_skip("no local user account"); return; }
    user_wipe();
    SAP_SIDS *s = NULL;
    CHECK(AP(AuditEnumeratePerUserPolicy)(&s));
    ULONG base = s ? s->Count : 0xffff;
    if (s) AP(AuditFree)(s);
    CHECK(user_set(SUB_LOGON, 1));
    s = NULL;
    CHECK(AP(AuditEnumeratePerUserPolicy)(&s));
    if (s) {
        EXPECT_EQ(s->Count, base + 1);
        int found = 0;
        for (ULONG i = 0; i < s->Count; i++) { CHECK(IsValidSid(s->Sids[i])); if (EqualSid(s->Sids[i], (PSID)g_sid)) found++; }
        EXPECT_EQ(found, 1);
        /* the SIDs live inside the one block */
        for (ULONG i = 0; i < s->Count; i++) CHECK((BYTE *)s->Sids[i] > (BYTE *)s && (BYTE *)s->Sids[i] < (BYTE *)s + LocalSize(s));
        t_note("one-account block: %lu bytes", (unsigned long)LocalSize(s));
        AP(AuditFree)(s);
    }
    user_wipe();
    s = NULL;
    CHECK(AP(AuditEnumeratePerUserPolicy)(&s));
    if (s) { EXPECT_EQ(s->Count, base); AP(AuditFree)(s); }
}

TEST(dx_sap_effective_policy_formula) {
    GATE();
    if (!find_user()) { t_skip("no local user account"); return; }
    user_wipe();
    ULONG o1 = sys_get(SUB_LOGON, NULL), o2 = sys_get(SUB_LOGOFF, NULL);
    /* measured over every combination: rows = system policy 0..3, columns = per-user flags 0..15.
     * An include adds its bit whatever the system policy says; an exclude removes the system policy's bit. */
    static const unsigned char M[4][16] = {
        {0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3},
        {1, 1, 0, 1, 3, 3, 2, 3, 1, 1, 0, 1, 3, 3, 2, 3},
        {2, 3, 2, 3, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3},
        {3, 3, 2, 3, 3, 3, 2, 3, 1, 1, 0, 1, 3, 3, 2, 3},
    };
    GUID req[2] = {sap_sub_guid(SUB_LOGON), sap_sub_guid(SUB_LOGOFF)};
    for (int sys = 0; sys < 4; sys++) {
        for (int u = 0; u < 16; u++) {
            sys_set(SUB_LOGON, SAP_NONE | sys); sys_set(SUB_LOGOFF, SAP_NONE | sys);
            user_wipe();
            if (u) { user_set(SUB_LOGON, u); user_set(SUB_LOGOFF, u); }
            SAP_POLICY *p = NULL;
            SET_ERR(0x5a5a);
            CHECKF(AP(AuditComputeEffectivePolicyBySid)((PSID)g_sid, req, 2, &p), "system %d user 0x%x failed %lu", sys, u, GetLastError());
            if (!p) continue;
            CHECKF(p[0].Info == M[sys][u] && p[1].Info == M[sys][u], "system %d user 0x%x: got %lu/%lu want %u", sys, u, p[0].Info, p[1].Info, M[sys][u]);
            CHECK(IsEqualGUID(&p[0].Sub, &req[0])); CHECK(IsEqualGUID(&p[1].Cat, &SAP_CAT[CAT_LOGON]));
            AP(AuditFree)(p);
        }
    }
    user_wipe();
    sys_restore(SUB_LOGON, o1); sys_restore(SUB_LOGOFF, o2);
    EXPECT_EQ(sys_get(SUB_LOGON, NULL), o1); EXPECT_EQ(sys_get(SUB_LOGOFF, NULL), o2);
}

static PACL mk_audit_acl(BYTE *buf, DWORD cap, DWORD mask, const BYTE *sid) {
    PACL a = (PACL)buf;
    if (!InitializeAcl(a, cap, ACL_REVISION)) return NULL;
    if (!AddAuditAccessAce(a, ACL_REVISION, mask, (PSID)sid, TRUE, FALSE)) return NULL;
    return a;
}

TEST(dx_sap_global_sacl_store) {
    GATE();
    static const WCHAR *T1 = L"olet_audit_type_one", *T1U = L"OLET_AUDIT_TYPE_ONE";
    BYTE b128[128], b64[64];
    PACL a128 = mk_audit_acl(b128, sizeof b128, KEY_SET_VALUE, SAP_SID_EVERYONE), a64 = mk_audit_acl(b64, sizeof b64, GENERIC_ALL, SAP_SID_ADMINS);
    REQUIRE(a128 && a64);
    PACL g = (PACL)(ULONG_PTR)1;
    SET_ERR(0x5a5a); CHECK(!AP(AuditQueryGlobalSaclW)(T1, &g)); EXPECT_ERR(2); EXPECT_EQ(g, 0);    /* never set */
    /* any object type name is accepted */
    SET_ERR(0x5a5a); CHECKF(AP(AuditSetGlobalSaclW)(T1, a128), "set failed %lu", GetLastError());
    g = NULL; SET_ERR(0x5a5a);
    CHECKF(AP(AuditQueryGlobalSaclW)(T1, &g), "query failed %lu", GetLastError());
    if (g) {
        CHECK(IsValidAcl(g));
        EXPECT_EQ(g->AclSize, 128);                                 /* the whole ACL as passed, free space included */
        EXPECT_EQ(LocalSize(g), 128);
        ACL_SIZE_INFORMATION si; CHECK(GetAclInformation(g, &si, sizeof si, AclSizeInformation));
        EXPECT_EQ(si.AceCount, 1);
        void *ace = NULL;
        if (GetAce(g, 0, &ace) && ace) {
            SYSTEM_AUDIT_ACE *s = ace;
            EXPECT_EQ(s->Header.AceType, SYSTEM_AUDIT_ACE_TYPE); EXPECT_EQ(s->Mask, KEY_SET_VALUE);
            CHECK(EqualSid((PSID)&s->SidStart, (PSID)SAP_SID_EVERYONE));
            CHECK(s->Header.AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG); CHECK(!(s->Header.AceFlags & FAILED_ACCESS_ACE_FLAG));
        }
        AP(AuditFree)(g);
    }
    /* type names compare without case; a second set replaces */
    g = NULL; CHECK(AP(AuditQueryGlobalSaclW)(T1U, &g)); CHECK(g != NULL); if (g) AP(AuditFree)(g);
    CHECK(AP(AuditSetGlobalSaclW)(T1U, a64));
    g = NULL; CHECK(AP(AuditQueryGlobalSaclW)(T1, &g));
    if (g) { EXPECT_EQ(g->AclSize, 64); EXPECT_EQ(g->AceCount, 1); AP(AuditFree)(g); }
    /* only audit ACEs are acceptable; a refused ACL leaves the stored one */
    BYTE al[64]; PACL allow = (PACL)al;
    CHECK(InitializeAcl(allow, sizeof al, ACL_REVISION)); CHECK(AddAccessAllowedAce(allow, ACL_REVISION, GENERIC_ALL, (PSID)SAP_SID_EVERYONE));
    SET_ERR(0x5a5a); CHECK(!AP(AuditSetGlobalSaclW)(T1, allow)); EXPECT_ERR(1336);
    g = NULL; CHECK(AP(AuditQueryGlobalSaclW)(T1, &g)); if (g) { EXPECT_EQ(g->AclSize, 64); AP(AuditFree)(g); }
    /* an empty ACL is fine */
    BYTE empty[8]; CHECK(InitializeAcl((PACL)empty, sizeof empty, ACL_REVISION));
    CHECK(AP(AuditSetGlobalSaclW)(T1, (PACL)empty));
    g = NULL; SET_ERR(0x5a5a); CHECK(AP(AuditQueryGlobalSaclW)(T1, &g)); if (g) { EXPECT_EQ(g->AceCount, 0); AP(AuditFree)(g); }
    /* NULL clears to "present with no ACL": success and a NULL result, no longer FILE_NOT_FOUND */
    SET_ERR(0x5a5a); CHECK(AP(AuditSetGlobalSaclW)(T1, NULL));
    g = (PACL)(ULONG_PTR)1; SET_ERR(0x5a5a);
    CHECKF(AP(AuditQueryGlobalSaclW)(T1, &g), "query after NULL set failed %lu", GetLastError());
    EXPECT_EQ(g, 0);
}

TEST(dx_sap_audit_security_roundtrip) {
    GATE();
    PSECURITY_DESCRIPTOR sd = NULL;
    SET_ERR(0x5a5a);
    REQUIRE(AP(AuditQuerySecurity)(DACL_SECURITY_INFORMATION, &sd) && sd);
    DWORD len0 = GetSecurityDescriptorLength(sd);
    PSECURITY_DESCRIPTOR keep = LocalAlloc(LPTR, len0);
    memcpy(keep, sd, len0);                                          /* the descriptor to restore */
    /* the same DACL back is accepted and changes nothing */
    SET_ERR(0x5a5a); CHECKF(AP(AuditSetSecurity)(DACL_SECURITY_INFORMATION, sd), "same DACL refused %lu", GetLastError());
    PSECURITY_DESCRIPTOR sd2 = NULL;
    CHECK(AP(AuditQuerySecurity)(DACL_SECURITY_INFORMATION, &sd2));
    if (sd2) { EXPECT_EQ(GetSecurityDescriptorLength(sd2), len0); CHECK(!memcmp(sd, sd2, len0)); AP(AuditFree)(sd2); }
    /* a descriptor without a DACL cannot satisfy a request for the DACL */
    SECURITY_DESCRIPTOR none; InitializeSecurityDescriptor(&none, SECURITY_DESCRIPTOR_REVISION);
    SET_ERR(0x5a5a); CHECK(!AP(AuditSetSecurity)(DACL_SECURITY_INFORMATION, &none)); EXPECT_ERR(87);
    AP(AuditFree)(sd);
    LocalFree(keep);
}
