/* State-changing sechost Audit* calls. These touch the machine audit policy (system policy, per-user policy,
 * global SACLs, the audit-policy security descriptor), so they live in a dx_ area, need an elevated token with
 * SeSecurityPrivilege (t_skip otherwise) and always restore what they changed (finally-style tails). Leftovers
 * from an aborted earlier run (the fake account SID below) are swept at the start of each test.
 *
 * Decomp facts (sechost 19041): AuditSetSystemPolicy / AuditSetPerUserPolicy / AuditSetSecurity map the RPC
 * NTSTATUS with RtlNtStatusToDosError; AuditSetSecurity first converts the descriptor with RtlMakeSelfRelativeSD
 * (so a descriptor that is already self-relative is rejected with ERROR_INVALID_PARAMETER). */
#include "../gap_00/gap0.h"

#define AU(name) ((name##_pfn)t_proc_in("sechost.dll", #name))
#define NEED(name) name##_pfn p##name = AU(name); REQUIRE(p##name)
#define GATE() do { if (t_is_wine()) { t_skip("Wine's sechost Audit* exports are aborting stubs"); return; } \
    if (!g_enable_priv("SeSecurityPrivilege")) { t_skip("SeSecurityPrivilege not available (not elevated)"); return; } } while (0)

static const GUID SUB_LOGON  = {0x0CCE9215,0x69AE,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID SUB_LOGOFF = {0x0CCE9216,0x69AE,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};
static const GUID SUB_BOGUS  = {0xDEADBEEF,0x1234,0x4321,{0x80,0x00,0x6f,0x6c,0x65,0x74,0x30,0x31}};
static const GUID CAT_LOGON  = {0x69979849,0x797A,0x11D9,{0xBE,0xD3,0x50,0x50,0x54,0x50,0x30,0x30}};

/* S-1-5-21-1111-2222-3333-4242: an account that does not exist; nothing it owns is ever audited */
static const BYTE FAKE_SID[28] = { 1, 5, 0,0,0,0,0,5, 21,0,0,0, 0x57,4,0,0, 0xAE,8,0,0, 0x05,0x0D,0,0, 0x92,0x10,0,0 };

static BOOL query_one(const GUID *sub, ULONG *info) {
    AuditQuerySystemPolicy_pfn q = AU(AuditQuerySystemPolicy); AuditFree_pfn fr = AU(AuditFree);
    G_AUDIT_POLICY *p = NULL;
    if (!q || !q(sub, 1, &p) || !p) return FALSE;
    *info = p[0].AuditingInformation; fr(p);
    return TRUE;
}

TEST(dx_gap0_audit_system_policy_roundtrip) {
    GATE();
    NEED(AuditSetSystemPolicy);
    ULONG orig = 0;
    if (!query_one(&SUB_LOGON, &orig)) { t_skip("cannot query the system audit policy (%lu)", GetLastError()); return; }
    t_note("original Logon policy = 0x%lx", orig);
    ULONG restore = orig ? orig : G_AUDIT_NONE;   /* 0 (not configured) is restored by writing NONE */

    /* writing the current value back is accepted */
    G_AUDIT_POLICY in = { SUB_LOGON, orig, CAT_LOGON };
    SET_ERR(0);
    BOOLEAN r = pAuditSetSystemPolicy(&in, 1);
    if (!r) { t_skip("AuditSetSystemPolicy refused (err %lu): needs an elevated administrator", GetLastError()); return; }
    ULONG now = 0xFF;
    CHECK(query_one(&SUB_LOGON, &now));
    EXPECT_EQ(now, orig);

    static const ULONG values[] = { G_AUDIT_SUCCESS, G_AUDIT_FAILURE, G_AUDIT_SUCCESS | G_AUDIT_FAILURE };
    for (int i = 0; i < 3; i++) {
        in.AuditingInformation = values[i];
        SET_ERR(0);
        CHECKF(pAuditSetSystemPolicy(&in, 1), "set %lu failed: %lu", values[i], GetLastError());
        now = 0xFF;
        CHECK(query_one(&SUB_LOGON, &now));
        EXPECT_EQ(now, values[i]);
    }
    /* two entries in one call */
    G_AUDIT_POLICY two[2] = { { SUB_LOGON, G_AUDIT_FAILURE, CAT_LOGON }, { SUB_LOGOFF, G_AUDIT_SUCCESS, CAT_LOGON } };
    ULONG o2 = 0; BOOL have2 = query_one(&SUB_LOGOFF, &o2);
    CHECK(pAuditSetSystemPolicy(two, 2));
    ULONG a = 0xFF, b = 0xFF;
    CHECK(query_one(&SUB_LOGON, &a)); CHECK(query_one(&SUB_LOGOFF, &b));
    EXPECT_EQ(a, G_AUDIT_FAILURE); EXPECT_EQ(b, G_AUDIT_SUCCESS);
    /* finally: restore */
    two[0].AuditingInformation = restore; two[1].AuditingInformation = o2 ? o2 : G_AUDIT_NONE;
    if (have2) pAuditSetSystemPolicy(two, 2); else pAuditSetSystemPolicy(two, 1);
    now = 0xFF;
    CHECK(query_one(&SUB_LOGON, &now));
    EXPECT_EQ(now, orig);
    if (have2) { now = 0xFF; CHECK(query_one(&SUB_LOGOFF, &now)); EXPECT_EQ(now, o2); }
}

TEST(dx_gap0_audit_set_system_policy_unknown_subcategory) {
    GATE();
    NEED(AuditSetSystemPolicy);
    G_AUDIT_POLICY in = { SUB_BOGUS, G_AUDIT_SUCCESS, CAT_LOGON };
    SET_ERR(0);
    BOOLEAN r = pAuditSetSystemPolicy(&in, 1);
    DWORD e = GetLastError();
    t_note("set policy for unknown subcategory GUID: result=%d err=%lu", r, e);
    if (r) {   /* undo whatever was accepted */
        in.AuditingInformation = G_AUDIT_NONE; pAuditSetSystemPolicy(&in, 1);
    } else {
        CHECK(e != 0);
    }
}

TEST(dx_gap0_audit_per_user_policy_roundtrip) {
    GATE();
    NEED(AuditSetPerUserPolicy); NEED(AuditQueryPerUserPolicy); NEED(AuditEnumeratePerUserPolicy); NEED(AuditFree);
    NEED(AuditComputeEffectivePolicyBySid);
    PSID sid = (PSID)FAKE_SID;
    /* sweep: clear any leftover entry for the fake account */
    G_AUDIT_POLICY clr[2] = { { SUB_LOGON, 0, CAT_LOGON }, { SUB_LOGOFF, 0, CAT_LOGON } };
    pAuditSetPerUserPolicy(sid, clr, 2);

    /* per-user flags: 1 success-include, 2 success-exclude, 4 failure-include, 8 failure-exclude */
    G_AUDIT_POLICY in[2] = { { SUB_LOGON, 1 | 8, CAT_LOGON }, { SUB_LOGOFF, 2 | 4, CAT_LOGON } };
    SET_ERR(0);
    if (!pAuditSetPerUserPolicy(sid, in, 2)) { t_skip("AuditSetPerUserPolicy refused (err %lu)", GetLastError()); return; }
    GUID req[2] = { SUB_LOGON, SUB_LOGOFF };
    G_AUDIT_POLICY *out = NULL;
    SET_ERR(0);
    if (pAuditQueryPerUserPolicy(sid, req, 2, &out) && out) {
        CHECK(IsEqualGUID(&out[0].AuditSubCategoryGuid, &SUB_LOGON));
        CHECK(IsEqualGUID(&out[1].AuditSubCategoryGuid, &SUB_LOGOFF));
        EXPECT_EQ(out[0].AuditingInformation, 1 | 8);
        EXPECT_EQ(out[1].AuditingInformation, 2 | 4);
        pAuditFree(out);
    } else CHECKF(0, "query of the per-user policy failed: %lu", GetLastError());

    G_AUDIT_SID_ARRAY *sids = NULL;
    if (pAuditEnumeratePerUserPolicy(&sids) && sids) {
        int found = 0;
        for (ULONG i = 0; i < sids->UsersCount; i++) if (EqualSid(sids->UserSidArray[i], sid)) found = 1;
        CHECK(found);
        pAuditFree(sids);
    } else CHECKF(0, "enumerate per-user policy failed: %lu", GetLastError());

    out = (G_AUDIT_POLICY *)(ULONG_PTR)1; SET_ERR(0);
    BOOLEAN r = pAuditComputeEffectivePolicyBySid(sid, req, 2, &out);
    t_note("effective policy for the fake SID: result=%d err=%lu", r, GetLastError());

    /* finally: clear */
    CHECK(pAuditSetPerUserPolicy(sid, clr, 2));
    sids = NULL;
    if (pAuditEnumeratePerUserPolicy(&sids) && sids) {
        int found = 0;
        for (ULONG i = 0; i < sids->UsersCount; i++) if (EqualSid(sids->UserSidArray[i], sid)) found = 1;
        t_note("fake SID still enumerated after clearing: %d", found);
        pAuditFree(sids);
    }
}

TEST(dx_gap0_audit_global_sacl_roundtrip) {
    GATE();
    NEED(AuditSetGlobalSaclW); NEED(AuditQueryGlobalSaclW); NEED(AuditFree);
    static const WCHAR *TYPE = L"Key";
    PACL orig = NULL; DWORD origCount = 0; BYTE *saved = NULL; DWORD savedLen = 0;
    SET_ERR(0);
    if (pAuditQueryGlobalSaclW(TYPE, &orig) && orig) {
        ACL_SIZE_INFORMATION si;
        if (GetAclInformation(orig, &si, sizeof si, AclSizeInformation)) { origCount = si.AceCount; savedLen = si.AclBytesInUse + si.AclBytesFree; }
        saved = LocalAlloc(LPTR, savedLen ? savedLen : 8);
        if (saved) memcpy(saved, orig, si.AclBytesInUse);
        pAuditFree(orig);
        t_note("a global Key SACL already exists (%lu ACEs); it is restored afterwards", origCount);
    } else {
        t_note("no global Key SACL before the test (err %lu)", GetLastError());
    }

    BYTE buf[128];
    PACL acl = (PACL)buf;
    CHECK(InitializeAcl(acl, sizeof buf, ACL_REVISION));
    CHECK(AddAuditAccessAce(acl, ACL_REVISION, KEY_SET_VALUE, (PSID)FAKE_SID, TRUE, FALSE));
    SET_ERR(0);
    if (!pAuditSetGlobalSaclW(TYPE, acl)) { t_skip("AuditSetGlobalSaclW refused (err %lu)", GetLastError()); LocalFree(saved); return; }
    PACL got = NULL;
    SET_ERR(0);
    if (pAuditQueryGlobalSaclW(TYPE, &got) && got) {
        CHECK(IsValidAcl(got));
        ACL_SIZE_INFORMATION si;
        CHECK(GetAclInformation(got, &si, sizeof si, AclSizeInformation));
        EXPECT_EQ(si.AceCount, 1);
        void *ace = NULL;
        if (GetAce(got, 0, &ace) && ace) {
            SYSTEM_AUDIT_ACE *a = ace;
            EXPECT_EQ(a->Header.AceType, SYSTEM_AUDIT_ACE_TYPE);
            EXPECT_EQ(a->Mask, KEY_SET_VALUE);
            CHECK(EqualSid((PSID)&a->SidStart, (PSID)FAKE_SID));
            CHECK((a->Header.AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG) != 0);
            CHECK((a->Header.AceFlags & FAILED_ACCESS_ACE_FLAG) == 0);
        }
        pAuditFree(got);
    } else CHECKF(0, "query after set failed: %lu", GetLastError());

    /* finally: put things back (a missing original means: remove ours with a NULL ACL) */
    BOOLEAN rr = saved ? pAuditSetGlobalSaclW(TYPE, (PACL)saved) : pAuditSetGlobalSaclW(TYPE, NULL);
    t_note("restore of the global Key SACL: result=%d err=%lu", rr, rr ? 0 : GetLastError());
    got = NULL;
    if (pAuditQueryGlobalSaclW(TYPE, &got) && got) {
        ACL_SIZE_INFORMATION si;
        if (GetAclInformation(got, &si, sizeof si, AclSizeInformation)) EXPECT_EQ(si.AceCount, origCount);
        pAuditFree(got);
    } else EXPECT_EQ(origCount, 0);
    if (saved) LocalFree(saved);
}

static PSECURITY_DESCRIPTOR to_absolute(PSECURITY_DESCRIPTOR rel, void **bufs) {
    DWORD sdl = 0, dl = 0, sl = 0, ol = 0, gl = 0;
    MakeAbsoluteSD(rel, NULL, &sdl, NULL, &dl, NULL, &sl, NULL, &ol, NULL, &gl);
    BYTE *sd = LocalAlloc(LPTR, sdl), *d = LocalAlloc(LPTR, dl ? dl : 1), *s = LocalAlloc(LPTR, sl ? sl : 1), *o = LocalAlloc(LPTR, ol ? ol : 1), *g = LocalAlloc(LPTR, gl ? gl : 1);
    bufs[0] = sd; bufs[1] = d; bufs[2] = s; bufs[3] = o; bufs[4] = g;
    if (!MakeAbsoluteSD(rel, sd, &sdl, (PACL)d, &dl, (PACL)s, &sl, o, &ol, g, &gl)) return NULL;
    return sd;
}

TEST(dx_gap0_audit_security_descriptor_roundtrip) {
    GATE();
    NEED(AuditQuerySecurity); NEED(AuditSetSecurity); NEED(AuditFree);
    PSECURITY_DESCRIPTOR sd = NULL;
    SET_ERR(0);
    if (!pAuditQuerySecurity(DACL_SECURITY_INFORMATION, &sd) || !sd) { t_skip("AuditQuerySecurity(DACL) failed (%lu)", GetLastError()); return; }
    CHECK(IsValidSecurityDescriptor(sd));
    SECURITY_DESCRIPTOR_CONTROL ctl = 0; DWORD rev = 0;
    CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev));
    CHECK((ctl & SE_SELF_RELATIVE) != 0);              /* the query hands out a self-relative descriptor */
    BOOL present = FALSE, dflt = FALSE; PACL dacl = NULL;
    CHECK(GetSecurityDescriptorDacl(sd, &present, &dacl, &dflt));
    DWORD aces = 0;
    if (present && dacl) { ACL_SIZE_INFORMATION si; if (GetAclInformation(dacl, &si, sizeof si, AclSizeInformation)) aces = si.AceCount; }
    t_note("audit policy DACL: present=%d aces=%lu", present, aces);

    /* feeding the self-relative result straight back is rejected before any RPC */
    SET_ERR(0x5A5A);
    EXPECT_EQ(pAuditSetSecurity(DACL_SECURITY_INFORMATION, sd), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);

    /* converted to an absolute descriptor, writing the same DACL back is accepted and changes nothing */
    void *bufs[5] = { 0 };
    PSECURITY_DESCRIPTOR abs = to_absolute(sd, bufs);
    if (abs) {
        SET_ERR(0);
        BOOLEAN r = pAuditSetSecurity(DACL_SECURITY_INFORMATION, abs);
        CHECKF(r, "AuditSetSecurity(same DACL) failed: %lu", GetLastError());
        PSECURITY_DESCRIPTOR sd2 = NULL;
        if (pAuditQuerySecurity(DACL_SECURITY_INFORMATION, &sd2) && sd2) {
            PACL d2 = NULL;
            BOOL pr2 = FALSE;
            CHECK(GetSecurityDescriptorDacl(sd2, &pr2, &d2, &dflt));
            EXPECT_EQ(pr2, present);
            DWORD aces2 = 0;
            if (pr2 && d2) { ACL_SIZE_INFORMATION si; if (GetAclInformation(d2, &si, sizeof si, AclSizeInformation)) aces2 = si.AceCount; }
            EXPECT_EQ(aces2, aces);
            pAuditFree(sd2);
        }
    } else t_note("MakeAbsoluteSD failed (%lu)", GetLastError());
    for (int i = 0; i < 5; i++) if (bufs[i]) LocalFree(bufs[i]);
    pAuditFree(sd);
}
