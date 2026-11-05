/* The read-only half of the audit policy API under the two runner tokens, the rights model and the argument
 * order. SeSecurityPrivilege is the whole story: the adm token carries it (disabled), the std token does not.
 * Nothing here changes machine state (failing writes only). */
#include "sap.h"

static const BYTE *sid_user_any(BYTE *buf, DWORD cap) { return sap_find_user(buf, cap) ? buf : NULL; }

TEST(sap_system_policy_read_needs_privilege_present) {
    NEEDAP(AuditQuerySystemPolicy); NEEDAP(AuditFree);
    sap_set_priv(FALSE);
    int st = sap_priv_state();
    GUID g = sap_sub_guid(SUB_LOGON);
    SAP_POLICY *p = (SAP_POLICY *)(ULONG_PTR)1;
    SET_ERR(0x5a5a);
    BOOLEAN r = pAuditQuerySystemPolicy(&g, 1, &p);
    DWORD e = GetLastError();
    t_note("SeSecurityPrivilege state %d (0 absent, 1 disabled, 2 enabled)", st);
    if (st == 0) {
        CHECK(!r); EXPECT_EQ(e, 1314); EXPECT_EQ(p, 0);
        /* the right is judged before the arguments */
        p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a);
        CHECK(!pAuditQuerySystemPolicy(&SAP_BOGUS, 1, &p)); EXPECT_ERR(1314); EXPECT_EQ(p, 0);
        p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a);
        CHECK(!pAuditQuerySystemPolicy(&g, 0, &p)); EXPECT_ERR(1314);
    } else {
        CHECKF(r, "query with a present but disabled privilege failed %lu", e);   /* reading does not need it enabled */
        EXPECT_NE(p, 0);
        if (r && p) { CHECK(IsEqualGUID(&p[0].Sub, &g)); CHECK(IsEqualGUID(&p[0].Cat, &SAP_CAT[CAT_LOGON])); CHECK((p[0].Info & ~7u) == 0); pAuditFree(p); }
    }
    /* a NULL guid array is the RPC runtime's error in both cases and the output is cleared first */
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a);
    CHECK(!pAuditQuerySystemPolicy(NULL, 1, &p)); EXPECT_ERR(E_NULLREF); EXPECT_EQ(p, 0);
}

TEST(sap_system_policy_query_validation) {
    NEEDAP(AuditQuerySystemPolicy); NEEDAP(AuditFree);
    if (sap_priv_state() == 0) { t_skip("token has no SeSecurityPrivilege (the 1314 path is in sap_system_policy_read_needs_privilege_present)"); return; }
    SAP_POLICY *p = (SAP_POLICY *)(ULONG_PTR)1;
    GUID g = sap_sub_guid(SUB_LOGON);
    SET_ERR(0x5a5a); CHECK(!pAuditQuerySystemPolicy(&SAP_BOGUS, 1, &p)); EXPECT_ERR(87); EXPECT_EQ(p, 0);
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQuerySystemPolicy(&g, 0, &p)); EXPECT_ERR(87); EXPECT_EQ(p, 0);
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQuerySystemPolicy(&SAP_CAT[CAT_LOGON], 1, &p)); EXPECT_ERR(87);   /* a category is no subcategory */
    GUID two[3] = {g, SAP_BOGUS, g};
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQuerySystemPolicy(two, 3, &p)); EXPECT_ERR(87); EXPECT_EQ(p, 0);   /* one bad GUID fails the call */
    SET_ERR(0x5a5a); CHECK(!pAuditQuerySystemPolicy(&g, 1, NULL)); EXPECT_ERR(87);
}

TEST(sap_system_policy_query_all_subcategories) {
    NEEDAP(AuditQuerySystemPolicy); NEEDAP(AuditFree);
    if (sap_priv_state() == 0) { t_skip("token has no SeSecurityPrivilege"); return; }
    GUID req[59];
    for (int i = 0; i < 59; i++) req[i] = sap_sub_guid(i);
    SAP_POLICY *all = NULL;
    SET_ERR(0x5a5a);
    CHECKF(pAuditQuerySystemPolicy(req, 59, &all), "query of all 59 failed %lu", GetLastError());
    if (!all) return;
    for (int i = 0; i < 59; i++) {
        CHECKF(IsEqualGUID(&all[i].Sub, &req[i]), "entry %d is not the requested subcategory", i);       /* request order */
        CHECKF(IsEqualGUID(&all[i].Cat, &SAP_CAT[SAP_SUB[i].cat]), "entry %d has the wrong category GUID", i);
        CHECKF((all[i].Info & ~7u) == 0, "entry %d info 0x%lx", i, all[i].Info);
        SAP_POLICY *one = NULL;
        if (pAuditQuerySystemPolicy(&req[i], 1, &one) && one) { CHECKF(one[0].Info == all[i].Info, "entry %d differs between a single and the batch query", i); pAuditFree(one); }
    }
    t_note("block size for 59 entries: %lu (36 bytes each)", (unsigned long)LocalSize(all));
    pAuditFree(all);
    /* the same subcategory twice and a reversed order are answered as asked */
    GUID mix[3] = {req[SUB_LOGOFF], req[SUB_LOGON], req[SUB_LOGOFF]};
    SAP_POLICY *m = NULL;
    CHECK(pAuditQuerySystemPolicy(mix, 3, &m));
    if (m) { for (int i = 0; i < 3; i++) CHECK(IsEqualGUID(&m[i].Sub, &mix[i])); EXPECT_EQ(m[0].Info, m[2].Info); pAuditFree(m); }
}

TEST(sap_per_user_sid_classes_first) {
    NEEDAP(AuditQueryPerUserPolicy); NEEDAP(AuditFree);
    GUID g = sap_sub_guid(SUB_LOGON);
    SAP_POLICY *p;
    /* the account SID is judged before the right: the same answers under both tokens */
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryPerUserPolicy((PSID)SAP_SID_EVERYONE, &g, 1, &p)); EXPECT_ERR(50); EXPECT_EQ(p, 0);
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryPerUserPolicy((PSID)SAP_SID_ADMINS, &g, 1, &p)); EXPECT_ERR(50);
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryPerUserPolicy((PSID)SAP_SID_FAKE, &g, 1, &p)); EXPECT_ERR(1332);
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryPerUserPolicy((PSID)SAP_SID_BADREV, &g, 1, &p)); EXPECT_ERR(1337);
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryPerUserPolicy(NULL, &g, 1, &p)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditQueryPerUserPolicy((PSID)SAP_SID_FAKE, &g, 1, NULL)); EXPECT_ERR(87);
}

TEST(sap_per_user_query_of_a_user_without_entries) {
    NEEDAP(AuditQueryPerUserPolicy); NEEDAP(AuditFree);
    BYTE sid[SECURITY_MAX_SID_SIZE];
    if (!sid_user_any(sid, sizeof sid)) { t_skip("no local user account to query"); return; }
    GUID g = sap_sub_guid(SUB_LOGON);
    SAP_POLICY *p = (SAP_POLICY *)(ULONG_PTR)1;
    SET_ERR(0x5a5a);
    BOOLEAN r = pAuditQueryPerUserPolicy((PSID)sid, &g, 1, &p);
    DWORD e = GetLastError();
    if (sap_priv_state() == 0) { CHECK(!r); EXPECT_EQ(e, 1314); }
    else if (r) { t_note("the account already has a per-user policy"); if (p) pAuditFree(p); }
    else { EXPECT_EQ(e, 2); EXPECT_EQ(p, 0); }                      /* no entries for the account at all */
}

TEST(sap_effective_policy_order_of_checks) {
    NEEDAP(AuditComputeEffectivePolicyBySid); NEEDAP(AuditFree);
    GUID g = sap_sub_guid(SUB_LOGON);
    SAP_POLICY *p;
    int has = sap_priv_state() != 0;
    p = (SAP_POLICY *)(ULONG_PTR)1; SET_ERR(0x5a5a);
    CHECK(!pAuditComputeEffectivePolicyBySid((PSID)SAP_SID_FAKE, &g, 1, &p));
    EXPECT_ERR(has ? 1332 : 1314);                                   /* here the right comes before the SID */
    EXPECT_EQ(p, 1);                                                  /* the output is not cleared */
    SET_ERR(0x5a5a); CHECK(!pAuditComputeEffectivePolicyBySid((PSID)SAP_SID_EVERYONE, &g, 1, &p)); EXPECT_ERR(has ? 50 : 1314);
    SET_ERR(0x5a5a); CHECK(!pAuditComputeEffectivePolicyBySid((PSID)SAP_SID_BADREV, &g, 1, &p)); EXPECT_ERR(has ? 1337 : 1314);
    SET_ERR(0x5a5a); CHECK(!pAuditComputeEffectivePolicyBySid(NULL, &g, 1, &p)); EXPECT_ERR(87);
    if (has) {
        BYTE sid[SECURITY_MAX_SID_SIZE];
        if (sid_user_any(sid, sizeof sid)) {
            SET_ERR(0x5a5a); CHECK(!pAuditComputeEffectivePolicyBySid((PSID)sid, &SAP_BOGUS, 1, &p)); EXPECT_ERR(87);
            SAP_POLICY *q = NULL;
            SET_ERR(0x5a5a);
            CHECKF(pAuditComputeEffectivePolicyBySid((PSID)sid, &g, 1, &q), "effective policy of a local user failed %lu", GetLastError());
            if (q) { CHECK(IsEqualGUID(&q[0].Sub, &g)); CHECK((q[0].Info & ~3u) == 0); pAuditFree(q); }
        }
    }
}

TEST(sap_enumerate_per_user_policy_rights) {
    NEEDAP(AuditEnumeratePerUserPolicy); NEEDAP(AuditFree);
    SAP_SIDS *s = NULL;
    SET_ERR(0x5a5a);
    BOOLEAN r = pAuditEnumeratePerUserPolicy(&s);
    DWORD e = GetLastError();
    if (sap_priv_state() == 0) { CHECK(!r); EXPECT_EQ(e, 1314); return; }
    CHECKF(r, "enumeration failed %lu", e);
    if (!s) return;
    for (ULONG i = 0; i < s->Count; i++) CHECK(s->Sids[i] && IsValidSid(s->Sids[i]));
    t_note("accounts with a per-user audit policy: %lu, block %lu bytes", s->Count, (unsigned long)LocalSize(s));
    pAuditFree(s);
}

TEST(sap_writes_need_the_privilege_enabled) {
    NEEDAP(AuditSetSystemPolicy); NEEDAP(AuditSetPerUserPolicy); NEEDAP(AuditSetGlobalSaclW); NEEDAP(AuditSetSecurity);
    NEEDAP(AuditQueryGlobalSaclW); NEEDAP(AuditQuerySecurity);
    sap_set_priv(FALSE);                                             /* present-but-disabled at best */
    SAP_POLICY bad = {SAP_BOGUS, 1, SAP_CAT[CAT_LOGON]};
    SET_ERR(0x5a5a); CHECK(!pAuditSetSystemPolicy(&bad, 1)); EXPECT_ERR(1314);      /* before the GUID is looked at */
    SAP_POLICY pu = {sap_sub_guid(SUB_LOGON), 0, SAP_CAT[CAT_LOGON]};
    SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)SAP_SID_FAKE, &pu, 1)); EXPECT_ERR(1314);
    SET_ERR(0x5a5a); CHECK(!pAuditSetGlobalSaclW(L"olet_never_set_type", NULL)); EXPECT_ERR(1314);
    PACL a = (PACL)(ULONG_PTR)1;
    SET_ERR(0x5a5a); CHECK(!pAuditQueryGlobalSaclW(L"File", &a)); EXPECT_ERR(1314); EXPECT_EQ(a, 0);
    SET_ERR(0x5a5a); CHECK(!pAuditQueryGlobalSaclW(L"", &a)); EXPECT_ERR(1314);
    PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR)(ULONG_PTR)1;
    SET_ERR(0x5a5a); CHECK(!pAuditQuerySecurity(DACL_SECURITY_INFORMATION, &sd)); EXPECT_ERR(1314); EXPECT_EQ(sd, 0);
    SECURITY_DESCRIPTOR empty; InitializeSecurityDescriptor(&empty, SECURITY_DESCRIPTOR_REVISION);
    SET_ERR(0x5a5a); CHECK(!pAuditSetSecurity(DACL_SECURITY_INFORMATION, &empty)); EXPECT_ERR(1314);
    /* NULL names are caught by the client before any right is looked at */
    SET_ERR(0x5a5a); CHECK(!pAuditQueryGlobalSaclW(NULL, &a)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditSetGlobalSaclW(NULL, NULL)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy(NULL, &pu, 1)); EXPECT_ERR(87);
}

TEST(sap_write_argument_checks_with_the_privilege) {
    NEEDAP(AuditSetSystemPolicy); NEEDAP(AuditSetPerUserPolicy); NEEDAP(AuditSetGlobalSaclW); NEEDAP(AuditQueryGlobalSaclW);
    NEEDAP(AuditQuerySecurity); NEEDAP(AuditSetSecurity); NEEDAP(AuditFree);
    if (sap_set_priv(TRUE) != 2) { t_skip("SeSecurityPrivilege cannot be enabled (std token)"); return; }
    /* every call below is refused, so nothing changes */
    SAP_POLICY e1 = {sap_sub_guid(SUB_LOGON), 1, SAP_CAT[CAT_LOGON]};
    SAP_POLICY bad[2] = {e1, {SAP_BOGUS, 1, SAP_CAT[CAT_LOGON]}};
    SET_ERR(0x5a5a); CHECK(!pAuditSetSystemPolicy(bad, 2)); EXPECT_ERR(87);          /* the whole batch is refused */
    SET_ERR(0x5a5a); CHECK(!pAuditSetSystemPolicy(&e1, 0)); EXPECT_ERR(87);
    SET_ERR(0x5a5a); CHECK(!pAuditSetSystemPolicy(NULL, 1)); EXPECT_ERR(E_NULLREF);
    SET_ERR(0x5a5a); CHECK(!pAuditSetSystemPolicy(NULL, 0)); EXPECT_ERR(E_NULLREF);
    SAP_POLICY bits = e1;
    ULONG badbits[] = {8, 12, 0x80000001};
    for (int i = 0; i < 3; i++) { bits.Info = badbits[i]; SET_ERR(0x5a5a); CHECKF(!pAuditSetSystemPolicy(&bits, 1), "info 0x%lx accepted", badbits[i]); EXPECT_ERR(87); }
    SAP_POLICY cat = {SAP_CAT[CAT_LOGON], 1, SAP_CAT[CAT_LOGON]};
    SET_ERR(0x5a5a); CHECK(!pAuditSetSystemPolicy(&cat, 1)); EXPECT_ERR(87);          /* a category GUID in the subcategory slot */
    /* per user: SID classes, then the entries */
    SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)SAP_SID_EVERYONE, &e1, 1)); EXPECT_ERR(50);
    SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)SAP_SID_FAKE, &e1, 1)); EXPECT_ERR(1332);
    SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)SAP_SID_BADREV, &e1, 1)); EXPECT_ERR(1337);
    SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)SAP_SID_FAKE, NULL, 1)); EXPECT_ERR(E_NULLREF);
    BYTE sid[SECURITY_MAX_SID_SIZE];
    if (sid_user_any(sid, sizeof sid)) {
        SAP_POLICY u = e1;
        ULONG ubad[] = {0x20, 0xff, 0x100};
        for (int i = 0; i < 3; i++) { u.Info = ubad[i]; SET_ERR(0x5a5a); CHECKF(!pAuditSetPerUserPolicy((PSID)sid, &u, 1), "per-user flags 0x%lx accepted", ubad[i]); EXPECT_ERR(87); }
        u.Info = 1; SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)sid, &u, 0)); EXPECT_ERR(87);
        SAP_POLICY ub[2] = {e1, {SAP_BOGUS, 1, SAP_CAT[CAT_LOGON]}};
        SET_ERR(0x5a5a); CHECK(!pAuditSetPerUserPolicy((PSID)sid, ub, 2)); EXPECT_ERR(87);
    }
    /* global SACL and the audit descriptor */
    PACL a = (PACL)(ULONG_PTR)1;
    SET_ERR(0x5a5a); CHECK(!pAuditQueryGlobalSaclW(L"", &a)); EXPECT_ERR(87); EXPECT_EQ(a, 0);
    a = (PACL)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryGlobalSaclW(L"olet_never_set_type", &a)); EXPECT_ERR(2); EXPECT_EQ(a, 0);
    BYTE buf[64]; PACL allow = (PACL)buf;
    CHECK(InitializeAcl(allow, sizeof buf, ACL_REVISION));
    CHECK(AddAccessAllowedAce(allow, ACL_REVISION, GENERIC_ALL, (PSID)SAP_SID_EVERYONE));
    SET_ERR(0x5a5a); CHECK(!pAuditSetGlobalSaclW(L"olet_never_set_type", allow)); EXPECT_ERR(1336);   /* an allow ACE is not an audit ACE */
    a = (PACL)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECK(!pAuditQueryGlobalSaclW(L"olet_never_set_type", &a)); EXPECT_ERR(2);
    SECURITY_DESCRIPTOR empty; InitializeSecurityDescriptor(&empty, SECURITY_DESCRIPTOR_REVISION);
    SET_ERR(0x5a5a); CHECK(!pAuditSetSecurity(DACL_SECURITY_INFORMATION, &empty)); EXPECT_ERR(87);   /* a DACL is asked for and there is none */
    sap_set_priv(FALSE);
}

TEST(sap_audit_security_descriptor_shape) {
    NEEDAP(AuditQuerySecurity); NEEDAP(AuditFree);
    if (sap_set_priv(TRUE) != 2) { t_skip("SeSecurityPrivilege cannot be enabled (std token)"); return; }
    PSECURITY_DESCRIPTOR sd = NULL; DWORD dlen = 0;
    SET_ERR(0x5a5a);
    CHECKF(pAuditQuerySecurity(DACL_SECURITY_INFORMATION, &sd), "query failed %lu", GetLastError());
    if (sd) {
        CHECK(IsValidSecurityDescriptor(sd));
        dlen = GetSecurityDescriptorLength(sd);
        SECURITY_DESCRIPTOR_CONTROL ctl = 0; DWORD rev = 0;
        CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev));
        EXPECT_EQ(rev, 1);
        EXPECT_EQ(ctl & ~SE_DACL_AUTO_INHERITED, SE_SELF_RELATIVE | SE_DACL_PRESENT);
        PSID owner = (PSID)1, group = (PSID)1; BOOL d = FALSE;
        CHECK(GetSecurityDescriptorOwner(sd, &owner, &d)); CHECK(GetSecurityDescriptorGroup(sd, &group, &d));
        EXPECT_EQ(owner, 0); EXPECT_EQ(group, 0);                    /* only the DACL is returned */
        BOOL present = FALSE; PACL dacl = NULL;
        CHECK(GetSecurityDescriptorDacl(sd, &present, &dacl, &d));
        CHECK(present && dacl);
        int sy = 0, ba = 0;
        if (dacl) {
            for (WORD i = 0; i < dacl->AceCount; i++) {
                void *ace; if (!GetAce(dacl, i, &ace)) continue;
                ACCESS_ALLOWED_ACE *aa = ace;
                if (aa->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && EqualSid((PSID)&aa->SidStart, (PSID)SAP_SID_ADMINS)) ba = 1;
                if (aa->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && IsWellKnownSid((PSID)&aa->SidStart, WinLocalSystemSid)) sy = 1;
            }
        }
        CHECK(ba); CHECK(sy);                                         /* Administrators and SYSTEM */
        t_note("audit descriptor: %lu bytes, %u ACEs, block %lu", GetSecurityDescriptorLength(sd), dacl ? dacl->AceCount : 0, (unsigned long)LocalSize(sd));
        pAuditFree(sd);
    }
    /* the owner/group bits are ignored; the SACL bit alone gives an empty descriptor; no DACL/SACL bit is an error */
    PSECURITY_DESCRIPTOR all = NULL, sacl = NULL, none = (PSECURITY_DESCRIPTOR)(ULONG_PTR)1;
    CHECK(pAuditQuerySecurity(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION, &all));
    SECURITY_INFORMATION bad[] = {0, OWNER_SECURITY_INFORMATION, GROUP_SECURITY_INFORMATION, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION};
    for (int i = 0; i < 4; i++) { none = (PSECURITY_DESCRIPTOR)(ULONG_PTR)1; SET_ERR(0x5a5a); CHECKF(!pAuditQuerySecurity(bad[i], &none), "info 0x%lx accepted", bad[i]); EXPECT_ERR(87); EXPECT_EQ(none, 0); }
    CHECK(pAuditQuerySecurity(SACL_SECURITY_INFORMATION, &sacl));
    if (all) { EXPECT_EQ(GetSecurityDescriptorLength(all), dlen); pAuditFree(all); }
    if (sacl) {
        SECURITY_DESCRIPTOR_CONTROL ctl = 0; DWORD rev = 0;
        CHECK(GetSecurityDescriptorControl(sacl, &ctl, &rev));
        EXPECT_EQ(ctl, SE_SELF_RELATIVE);
        EXPECT_EQ(GetSecurityDescriptorLength(sacl), 20);
        pAuditFree(sacl);
    }
    sap_set_priv(FALSE);
}
