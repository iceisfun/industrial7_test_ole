#include "ssh.h"
/* ConvertStringSecurityDescriptorToSecurityDescriptorW/A.
 * Decomp: NULL string / NULL out -> 87 (checked before the revision); revision != 1 -> ERROR_UNKNOWN_REVISION (1305);
 * success calls SetLastError(0). Result is a self-relative SD allocated with LocalAlloc. */

#define CVT UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW)

static BOOL cvtA(const char *s, DWORD rev, PSECURITY_DESCRIPTOR *sd, ULONG *sz)
{
    WCHAR w[2048]; widen(s, w, 2048);
    return CVT(w, rev, sd, sz);
}
static ACL *get_dacl(PSECURITY_DESCRIPTOR sd, BOOL *present, BOOL *def)
{
    PACL a = NULL; BOOL p = FALSE, d = FALSE;
    GetSecurityDescriptorDacl(sd, &p, &a, &d);
    if (present) *present = p; if (def) *def = d; return a;
}
static ACL *get_sacl(PSECURITY_DESCRIPTOR sd, BOOL *present)
{
    PACL a = NULL; BOOL p = FALSE, d = FALSE;
    GetSecurityDescriptorSacl(sd, &p, &a, &d);
    if (present) *present = p; return a;
}
static WORD get_ctl(PSECURITY_DESCRIPTOR sd)
{
    SECURITY_DESCRIPTOR_CONTROL c = 0; DWORD rev;
    GetSecurityDescriptorControl(sd, &c, &rev);
    return c;
}
static void *nth_ace(ACL *a, int i) { void *ace = NULL; return GetAce(a, i, &ace) ? ace : NULL; }
static BOOL sid_is(PSID s, unsigned long long auth, int n, ...)
{
    SIDBUF b; PISID e = (PISID)mk_sid(&b, auth, 0); va_list ap; int i;
    e->SubAuthorityCount = (BYTE)n;
    va_start(ap, n); for (i = 0; i < n; i++) e->SubAuthority[i] = va_arg(ap, DWORD); va_end(ap);
    return s && EqualSid(s, e);
}
#define SID_WD(s)  sid_is(s, 1, 1, 0)
#define SID_BA(s)  sid_is(s, 5, 2, 32, 544)
#define SID_SY(s)  sid_is(s, 5, 1, 18)

TEST(sddl_w_null_string)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    EXPECT_FAIL(!CVT(NULL, SDDL_REVISION_1, &sd, NULL), ERROR_INVALID_PARAMETER);
}
TEST(sddl_w_null_string_bad_revision_still_87)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    EXPECT_FAIL(!CVT(NULL, 0xdeadbeef, &sd, NULL), ERROR_INVALID_PARAMETER);
}
TEST(sddl_w_null_out)
{
    REQUIRE(CVT);
    SET_ERR(0);
    EXPECT_FAIL(!CVT(L"D:(A;;GA;;;WD)", SDDL_REVISION_1, NULL, NULL), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_FAIL(!CVT(L"D:(A;;ROB;;;WD)", 0xdeadbeef, NULL, NULL), ERROR_INVALID_PARAMETER);
}
TEST(sddl_w_bad_revision_table)
{
    static const DWORD revs[] = { 0, 2, 3, 0xff, 0x100, 0xdeadbeef, 0xffffffff };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof revs / sizeof revs[0]; i++) {
        PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR)0x1;
        SET_ERR(0);
        CHECKF(!cvtA("D:(A;;GA;;;WD)", revs[i], &sd, NULL), "rev %lu", revs[i]);
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    }
}
TEST(sddl_w_bad_revision_checked_before_parsing)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    CHECK(!cvtA("this is not sddl", 7, &sd, NULL));
    EXPECT_ERR(ERROR_UNKNOWN_REVISION);
}
TEST(sddl_w_revision_1_is_one)
{
    EXPECT_EQ(SDDL_REVISION_1, 1);
}
TEST(sddl_w_success_clears_error)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0x1234);
    CHECK(cvtA("D:(A;;GA;;;WD)", 1, &sd, NULL));
    EXPECT_EQ(GetLastError(), 0);
    LocalFree(sd);
}
TEST(sddl_w_size_out_is_optional_and_exact)
{
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0;
    REQUIRE(CVT);
    CHECK(cvtA("O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)", 1, &sd, &sz));
    if (sd) {
        CHECK(IsValidSecurityDescriptor(sd));
        EXPECT_EQ(sz, GetSecurityDescriptorLength(sd));
        CHECK(LocalSize(sd) >= sz);
        LocalFree(sd);
    }
    sd = NULL;
    CHECK(cvtA("O:BA", 1, &sd, NULL));
    LocalFree(sd);
}
TEST(sddl_w_result_is_self_relative)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    CHECK(cvtA("D:(A;;GA;;;WD)", 1, &sd, NULL));
    if (sd) {
        WORD c = get_ctl(sd);
        CHECK(c & SE_SELF_RELATIVE);
        CHECK(c & SE_DACL_PRESENT);
        CHECK(!(c & (SE_SACL_PRESENT | SE_OWNER_DEFAULTED | SE_GROUP_DEFAULTED | SE_DACL_DEFAULTED | SE_DACL_PROTECTED)));
        EXPECT_EQ(((SECURITY_DESCRIPTOR_RELATIVE *)sd)->Revision, SECURITY_DESCRIPTOR_REVISION);
        LocalFree(sd);
    }
}
TEST(sddl_w_empty_string)
{
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0;
    REQUIRE(CVT);
    SET_ERR(0);
    CHECK(CVT(L"", 1, &sd, &sz));
    if (sd) {
        CHECK(IsValidSecurityDescriptor(sd));
        t_note("empty SDDL: size %lu control 0x%x", sz, get_ctl(sd));
        EXPECT_EQ(get_ctl(sd) & (SE_DACL_PRESENT | SE_SACL_PRESENT), 0);
        LocalFree(sd);
    }
}
TEST(sddl_w_empty_dacl)
{
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0; BOOL pr; ACL *a;
    REQUIRE(CVT);
    CHECK(cvtA("D:", 1, &sd, &sz));
    if (!sd) return;
    EXPECT_EQ(sz, sizeof(SECURITY_DESCRIPTOR_RELATIVE) + sizeof(ACL));
    a = get_dacl(sd, &pr, NULL);
    CHECK(pr); CHECK(a != NULL);
    if (a) {
        EXPECT_EQ(a->AclRevision, ACL_REVISION); EXPECT_EQ(a->Sbz1, 0); EXPECT_EQ(a->AclSize, sizeof(ACL));
        EXPECT_EQ(a->AceCount, 0); EXPECT_EQ(a->Sbz2, 0);
    }
    LocalFree(sd);
}
TEST(sddl_w_empty_sacl)
{
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0; BOOL pr; ACL *a;
    REQUIRE(CVT);
    CHECK(cvtA("S:", 1, &sd, &sz));
    if (!sd) return;
    EXPECT_EQ(sz, sizeof(SECURITY_DESCRIPTOR_RELATIVE) + sizeof(ACL));
    a = get_sacl(sd, &pr);
    CHECK(pr); CHECK(a != NULL);
    if (a) { EXPECT_EQ(a->Sbz1, 0); EXPECT_EQ(a->AclSize, sizeof(ACL)); EXPECT_EQ(a->AceCount, 0); EXPECT_EQ(a->Sbz2, 0); }
    CHECK(!(get_ctl(sd) & SE_DACL_PRESENT));
    LocalFree(sd);
}
TEST(sddl_w_empty_dacl_and_sacl)
{
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0;
    REQUIRE(CVT);
    CHECK(cvtA("D:S:", 1, &sd, &sz));
    if (!sd) return;
    EXPECT_EQ(sz, sizeof(SECURITY_DESCRIPTOR_RELATIVE) + 2 * sizeof(ACL));
    EXPECT_EQ(get_ctl(sd) & (SE_DACL_PRESENT | SE_SACL_PRESENT), SE_DACL_PRESENT | SE_SACL_PRESENT);
    LocalFree(sd);
}
TEST(sddl_w_null_dacl)
{
    PSECURITY_DESCRIPTOR sd = NULL; BOOL pr; ACL *a;
    REQUIRE(CVT);
    SET_ERR(0);
    CHECK(cvtA("D:NO_ACCESS_CONTROL", 1, &sd, NULL));
    if (!sd) { t_note("D:NO_ACCESS_CONTROL rejected, err %lu", GetLastError()); return; }
    a = get_dacl(sd, &pr, NULL);
    CHECK(pr); CHECK(a == NULL);
    LocalFree(sd);
}
TEST(sddl_w_null_dacl_protected)
{
    PSECURITY_DESCRIPTOR sd = NULL; BOOL pr; ACL *a;
    REQUIRE(CVT);
    if (!cvtA("D:PNO_ACCESS_CONTROL", 1, &sd, NULL)) { t_note("D:PNO_ACCESS_CONTROL rejected err %lu", GetLastError()); return; }
    a = get_dacl(sd, &pr, NULL);
    CHECK(pr); CHECK(a == NULL); CHECK(get_ctl(sd) & SE_DACL_PROTECTED);
    LocalFree(sd);
}
TEST(sddl_w_null_dacl_with_sacl)
{
    PSECURITY_DESCRIPTOR sd = NULL; BOOL pr; ACL *a;
    REQUIRE(CVT);
    if (!cvtA("D:NO_ACCESS_CONTROLS:(AU;SA;GA;;;WD)", 1, &sd, NULL)) { t_note("rejected err %lu", GetLastError()); return; }
    CHECK(get_dacl(sd, &pr, NULL) == NULL); CHECK(pr);
    a = get_sacl(sd, &pr); CHECK(pr); CHECK(a && a->AceCount == 1);
    LocalFree(sd);
}

TEST(sddl_w_owner_group_only)
{
    PSECURITY_DESCRIPTOR sd = NULL; PSID o = NULL, g = NULL; BOOL def; ULONG sz;
    REQUIRE(CVT);
    CHECK(cvtA("O:BA", 1, &sd, &sz));
    if (sd) {
        CHECK(GetSecurityDescriptorOwner(sd, &o, &def)); CHECK(SID_BA(o)); CHECK(!def);
        CHECK(GetSecurityDescriptorGroup(sd, &g, &def)); CHECK(g == NULL);
        EXPECT_EQ(sz, sizeof(SECURITY_DESCRIPTOR_RELATIVE) + 16);
        CHECK(!(get_ctl(sd) & (SE_DACL_PRESENT | SE_SACL_PRESENT)));
        LocalFree(sd);
    }
    CHECK(cvtA("G:SY", 1, &sd, &sz));
    if (sd) {
        CHECK(GetSecurityDescriptorGroup(sd, &g, &def)); CHECK(SID_SY(g)); CHECK(!def);
        CHECK(GetSecurityDescriptorOwner(sd, &o, &def)); CHECK(o == NULL);
        EXPECT_EQ(sz, sizeof(SECURITY_DESCRIPTOR_RELATIVE) + 12);
        LocalFree(sd);
    }
    CHECK(cvtA("O:BAG:SY", 1, &sd, &sz));
    if (sd) {
        GetSecurityDescriptorOwner(sd, &o, &def); CHECK(SID_BA(o));
        GetSecurityDescriptorGroup(sd, &g, &def); CHECK(SID_SY(g));
        EXPECT_EQ(sz, sizeof(SECURITY_DESCRIPTOR_RELATIVE) + 16 + 12);
        LocalFree(sd);
    }
}
TEST(sddl_w_owner_numeric_sid)
{
    PSECURITY_DESCRIPTOR sd = NULL; PSID o = NULL; BOOL def;
    REQUIRE(CVT);
    CHECK(cvtA("O:S-1-5-21-1-2-3-500G:S-1-5-21-1-2-3-513", 1, &sd, NULL));
    if (sd) {
        GetSecurityDescriptorOwner(sd, &o, &def); CHECK(sid_is(o, 5, 5, 21, 1, 2, 3, 500));
        GetSecurityDescriptorGroup(sd, &o, &def); CHECK(sid_is(o, 5, 5, 21, 1, 2, 3, 513));
        LocalFree(sd);
    }
}
TEST(sddl_w_section_order_is_free)
{
    PSECURITY_DESCRIPTOR sd = NULL; PSID o = NULL; BOOL def, pr;
    REQUIRE(CVT);
    if (!cvtA("S:(AU;SA;GA;;;WD)D:(A;;GA;;;WD)G:SYO:BA", 1, &sd, NULL)) { t_note("reordered sections rejected err %lu", GetLastError()); return; }
    GetSecurityDescriptorOwner(sd, &o, &def); CHECK(SID_BA(o));
    GetSecurityDescriptorGroup(sd, &o, &def); CHECK(SID_SY(o));
    CHECK(get_dacl(sd, &pr, NULL) && pr); CHECK(get_sacl(sd, &pr) && pr);
    LocalFree(sd);
}
TEST(sddl_w_full_task_example)
{
    PSECURITY_DESCRIPTOR sd = NULL; PSID o = NULL; BOOL def, pr; ACL *a; ACCESS_ALLOWED_ACE *ace; SYSTEM_AUDIT_ACE *au; ULONG sz;
    REQUIRE(CVT);
    CHECK(cvtA("O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)", 1, &sd, &sz));
    if (!sd) return;
    GetSecurityDescriptorOwner(sd, &o, &def); CHECK(SID_BA(o));
    GetSecurityDescriptorGroup(sd, &o, &def); CHECK(SID_SY(o));
    EXPECT_EQ(get_ctl(sd), SE_SELF_RELATIVE | SE_DACL_PRESENT | SE_SACL_PRESENT);
    a = get_dacl(sd, &pr, NULL);
    CHECK(a && a->AceCount == 1);
    if (a && (ace = nth_ace(a, 0))) {
        EXPECT_EQ(ace->Header.AceType, ACCESS_ALLOWED_ACE_TYPE); EXPECT_EQ(ace->Header.AceFlags, 0);
        EXPECT_EQ(ace->Mask, GENERIC_ALL); CHECK(SID_WD((PSID)&ace->SidStart));
        EXPECT_EQ(ace->Header.AceSize, 8 + 12);
        EXPECT_EQ(a->AclSize, 8 + 20);
    }
    a = get_sacl(sd, &pr);
    CHECK(a && a->AceCount == 1);
    if (a && (au = nth_ace(a, 0))) {
        EXPECT_EQ(au->Header.AceType, SYSTEM_AUDIT_ACE_TYPE);
        EXPECT_EQ(au->Header.AceFlags, SUCCESSFUL_ACCESS_ACE_FLAG | FAILED_ACCESS_ACE_FLAG);
        EXPECT_EQ(au->Mask, GENERIC_ALL); CHECK(SID_WD((PSID)&au->SidStart));
    }
    LocalFree(sd);
}

/* ---- ACL / control flags ---- */
TEST(sddl_w_dacl_control_flag_table)
{
    static const struct { const char *s; WORD want; } t[] = {
        { "D:",                        SE_DACL_PRESENT },
        { "D:P",                       SE_DACL_PRESENT | SE_DACL_PROTECTED },
        { "D:AI",                      SE_DACL_PRESENT | SE_DACL_AUTO_INHERITED },
        { "D:AR",                      SE_DACL_PRESENT | SE_DACL_AUTO_INHERIT_REQ },
        { "D:PAI",                     SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED },
        { "D:PAR",                     SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_DACL_AUTO_INHERIT_REQ },
        { "D:ARAI",                    SE_DACL_PRESENT | SE_DACL_AUTO_INHERIT_REQ | SE_DACL_AUTO_INHERITED },
        { "D:PARAI",                   SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_DACL_AUTO_INHERIT_REQ | SE_DACL_AUTO_INHERITED },
        { "D:P(A;;GA;;;WD)",           SE_DACL_PRESENT | SE_DACL_PROTECTED },
        { "D:AI(A;;GA;;;WD)",          SE_DACL_PRESENT | SE_DACL_AUTO_INHERITED },
        { "D:AR(A;;GA;;;WD)",          SE_DACL_PRESENT | SE_DACL_AUTO_INHERIT_REQ },
        { "D:PAI(A;;GA;;;WD)",         SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED },
        { "S:",                        SE_SACL_PRESENT },
        { "S:P",                       SE_SACL_PRESENT | SE_SACL_PROTECTED },
        { "S:AI",                      SE_SACL_PRESENT | SE_SACL_AUTO_INHERITED },
        { "S:AR",                      SE_SACL_PRESENT | SE_SACL_AUTO_INHERIT_REQ },
        { "S:PAI",                     SE_SACL_PRESENT | SE_SACL_PROTECTED | SE_SACL_AUTO_INHERITED },
        { "S:PARAI",                   SE_SACL_PRESENT | SE_SACL_PROTECTED | SE_SACL_AUTO_INHERIT_REQ | SE_SACL_AUTO_INHERITED },
        { "D:PS:P",                    SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_SACL_PRESENT | SE_SACL_PROTECTED },
        { "D:PS:AI",                   SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_SACL_PRESENT | SE_SACL_AUTO_INHERITED },
        { "D:AIS:AR",                  SE_DACL_PRESENT | SE_DACL_AUTO_INHERITED | SE_SACL_PRESENT | SE_SACL_AUTO_INHERIT_REQ },
        { "D:P(A;;GA;;;WD)S:P(AU;SA;GA;;;WD)", SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_SACL_PRESENT | SE_SACL_PROTECTED },
        { "D:P(A;;GA;;;BA)S:(AU;FA;GA;;;WD)", SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_SACL_PRESENT },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL;
        if (!cvtA(t[i].s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", t[i].s, GetLastError()); continue; }
        CHECKF(get_ctl(sd) == (t[i].want | SE_SELF_RELATIVE), "\"%s\": control 0x%x want 0x%x", t[i].s, get_ctl(sd), t[i].want | SE_SELF_RELATIVE);
        LocalFree(sd);
    }
}

/* ---- ACE types ---- */
typedef struct { const char *s; BYTE type; BYTE flags; DWORD mask; } AceCase;
static void check_ace_cases(const AceCase *t, unsigned n, BOOL sacl)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACE_HEADER *h; BOOL pr;
        if (!cvtA(t[i].s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", t[i].s, GetLastError()); continue; }
        a = sacl ? get_sacl(sd, &pr) : get_dacl(sd, &pr, NULL);
        CHECKF(a && a->AceCount == 1, "\"%s\": acl %p", t[i].s, a);
        if (a && (h = nth_ace(a, 0))) {
            CHECKF(h->AceType == t[i].type, "\"%s\": type 0x%x want 0x%x", t[i].s, h->AceType, t[i].type);
            CHECKF(h->AceFlags == t[i].flags, "\"%s\": flags 0x%x want 0x%x", t[i].s, h->AceFlags, t[i].flags);
            CHECKF(((ACCESS_ALLOWED_ACE *)h)->Mask == t[i].mask, "\"%s\": mask 0x%lx want 0x%lx", t[i].s, ((ACCESS_ALLOWED_ACE *)h)->Mask, t[i].mask);
        }
        LocalFree(sd);
    }
}
TEST(sddl_w_dacl_ace_types_simple)
{
    static const AceCase t[] = {
        { "D:(A;;GA;;;WD)",   ACCESS_ALLOWED_ACE_TYPE, 0, GENERIC_ALL },
        { "D:(D;;GA;;;WD)",   ACCESS_DENIED_ACE_TYPE,  0, GENERIC_ALL },
        { "D:(A;;GR;;;BA)",   ACCESS_ALLOWED_ACE_TYPE, 0, GENERIC_READ },
        { "D:(D;;GW;;;SY)",   ACCESS_DENIED_ACE_TYPE,  0, GENERIC_WRITE },
        { "D:(A;;GX;;;AU)",   ACCESS_ALLOWED_ACE_TYPE, 0, GENERIC_EXECUTE },
    };
    REQUIRE(CVT);
    check_ace_cases(t, sizeof t / sizeof t[0], FALSE);
}
TEST(sddl_w_sacl_ace_types_audit_alarm)
{
    static const AceCase t[] = {
        { "S:(AU;SA;GA;;;WD)",   SYSTEM_AUDIT_ACE_TYPE, SUCCESSFUL_ACCESS_ACE_FLAG, GENERIC_ALL },
        { "S:(AU;FA;GA;;;WD)",   SYSTEM_AUDIT_ACE_TYPE, FAILED_ACCESS_ACE_FLAG, GENERIC_ALL },
        { "S:(AU;SAFA;GA;;;WD)", SYSTEM_AUDIT_ACE_TYPE, SUCCESSFUL_ACCESS_ACE_FLAG | FAILED_ACCESS_ACE_FLAG, GENERIC_ALL },
        { "S:(AU;FASA;GR;;;BA)", SYSTEM_AUDIT_ACE_TYPE, SUCCESSFUL_ACCESS_ACE_FLAG | FAILED_ACCESS_ACE_FLAG, GENERIC_READ },
        { "S:(AL;SA;GA;;;WD)",   SYSTEM_ALARM_ACE_TYPE, SUCCESSFUL_ACCESS_ACE_FLAG, GENERIC_ALL },
        { "S:(AL;SAFA;GW;;;WD)", SYSTEM_ALARM_ACE_TYPE, SUCCESSFUL_ACCESS_ACE_FLAG | FAILED_ACCESS_ACE_FLAG, GENERIC_WRITE },
        { "S:(AU;OICISAFA;GA;;;WD)", SYSTEM_AUDIT_ACE_TYPE, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | SUCCESSFUL_ACCESS_ACE_FLAG | FAILED_ACCESS_ACE_FLAG, GENERIC_ALL },
    };
    REQUIRE(CVT);
    check_ace_cases(t, sizeof t / sizeof t[0], TRUE);
}
TEST(sddl_w_mandatory_label_ace)
{
    static const struct { const char *s; DWORD mask; DWORD rid; BYTE flags; } t[] = {
        { "S:(ML;;NW;;;LW)", SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, 4096, 0 },
        { "S:(ML;;NWNR;;;S-1-16-12288)", 3, 12288, 0 },
        { "S:(ML;;NRNWNX;;;ME)", 7, 8192, 0 },
        { "S:(ML;;NX;;;HI)", 4, 12288, 0 },
        { "S:(ML;;NW;;;SI)", 1, 16384, 0 },
        { "S:(ML;OICI;NW;;;ME)", 1, 8192, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE },
        { "S:(ML;NP;NW;;;ME)", 1, 8192, NO_PROPAGATE_INHERIT_ACE },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; ACL *a; SYSTEM_MANDATORY_LABEL_ACE *ace; BOOL pr;
        if (!cvtA(t[i].s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", t[i].s, GetLastError()); continue; }
        a = get_sacl(sd, &pr);
        CHECKF(a && a->AceCount == 1, "\"%s\"", t[i].s);
        if (a && (ace = nth_ace(a, 0))) {
            EXPECT_EQ(ace->Header.AceType, SYSTEM_MANDATORY_LABEL_ACE_TYPE);
            CHECKF(ace->Header.AceFlags == t[i].flags, "\"%s\": flags 0x%x", t[i].s, ace->Header.AceFlags);
            CHECKF(ace->Mask == t[i].mask, "\"%s\": mask 0x%lx want 0x%lx", t[i].s, ace->Mask, t[i].mask);
            CHECKF(sid_is((PSID)&ace->SidStart, 16, 1, t[i].rid), "\"%s\": sid", t[i].s);
            if (a->AclRevision != ACL_REVISION) t_note("\"%s\": ACL revision %u", t[i].s, a->AclRevision);
        }
        LocalFree(sd);
    }
}
TEST(sddl_w_label_in_sacl_with_audit)
{
    PSECURITY_DESCRIPTOR sd = NULL; ACL *a; BOOL pr; ACE_HEADER *h;
    REQUIRE(CVT);
    CHECK(cvtA("S:(AU;SA;GA;;;WD)(ML;;NW;;;ME)", 1, &sd, NULL));
    if (!sd) return;
    a = get_sacl(sd, &pr);
    CHECK(a && a->AceCount == 2);
    if (a) {
        h = nth_ace(a, 0); CHECK(h && h->AceType == SYSTEM_AUDIT_ACE_TYPE);
        h = nth_ace(a, 1); CHECK(h && h->AceType == SYSTEM_MANDATORY_LABEL_ACE_TYPE);
    }
    LocalFree(sd);
}
TEST(sddl_w_task_wine_label_sample)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    CHECK(cvtA("D:P(A;;GRGW;;;BA)(A;;GRGW;;;S-1-5-21-0-0-0-1000)S:(ML;;NWNR;;;S-1-16-12288)", 1, &sd, NULL));
    LocalFree(sd);
}

#define GUID1 "bf967aba-0de6-11d0-a285-00aa00b3f6c0"
#define GUID2 "bf967a86-0de6-11d0-a285-00aa00b3f6c0"
TEST(sddl_w_object_aces)
{
    static const struct { const char *s; BYTE type; DWORD flags; BOOL sacl; } t[] = {
        { "D:(OA;;CC;" GUID1 ";;WD)",              ACCESS_ALLOWED_OBJECT_ACE_TYPE, ACE_OBJECT_TYPE_PRESENT, FALSE },
        { "D:(OD;;CC;" GUID1 ";;WD)",              ACCESS_DENIED_OBJECT_ACE_TYPE,  ACE_OBJECT_TYPE_PRESENT, FALSE },
        { "D:(OA;;CC;;" GUID2 ";WD)",              ACCESS_ALLOWED_OBJECT_ACE_TYPE, ACE_INHERITED_OBJECT_TYPE_PRESENT, FALSE },
        { "D:(OA;;CC;" GUID1 ";" GUID2 ";WD)",     ACCESS_ALLOWED_OBJECT_ACE_TYPE, ACE_OBJECT_TYPE_PRESENT | ACE_INHERITED_OBJECT_TYPE_PRESENT, FALSE },
        { "D:(OA;;CC;;;WD)",                       ACCESS_ALLOWED_OBJECT_ACE_TYPE, 0, FALSE },
        { "S:(OU;SA;CC;" GUID1 ";;WD)",            SYSTEM_AUDIT_OBJECT_ACE_TYPE,   ACE_OBJECT_TYPE_PRESENT, TRUE },
        { "S:(OL;SA;CC;;" GUID2 ";WD)",            SYSTEM_ALARM_OBJECT_ACE_TYPE,   ACE_INHERITED_OBJECT_TYPE_PRESENT, TRUE },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACE_HEADER *h; BOOL pr;
        if (!cvtA(t[i].s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", t[i].s, GetLastError()); continue; }
        a = t[i].sacl ? get_sacl(sd, &pr) : get_dacl(sd, &pr, NULL);
        CHECKF(a && a->AceCount == 1, "\"%s\"", t[i].s);
        if (a && (h = nth_ace(a, 0))) {
            CHECKF(h->AceType == t[i].type, "\"%s\": type 0x%x", t[i].s, h->AceType);
            CHECKF(((ACCESS_ALLOWED_OBJECT_ACE *)h)->Flags == t[i].flags, "\"%s\": flags 0x%lx", t[i].s, ((ACCESS_ALLOWED_OBJECT_ACE *)h)->Flags);
            CHECKF(a->AclRevision == ACL_REVISION_DS, "\"%s\": ACL revision %u", t[i].s, a->AclRevision);
            CHECK(((ACCESS_ALLOWED_OBJECT_ACE *)h)->Mask == ADS_RIGHT_DS_CREATE_CHILD);
        }
        LocalFree(sd);
    }
}
TEST(sddl_w_object_ace_guid_bytes)
{
    PSECURITY_DESCRIPTOR sd = NULL; ACL *a; BOOL pr; ACCESS_ALLOWED_OBJECT_ACE *ace;
    static const GUID g = { 0xbf967aba, 0x0de6, 0x11d0, { 0xa2, 0x85, 0x00, 0xaa, 0x00, 0xb3, 0xf6, 0xc0 } };
    REQUIRE(CVT);
    CHECK(cvtA("D:(OA;;CC;" GUID1 ";;WD)", 1, &sd, NULL));
    if (!sd) return;
    a = get_dacl(sd, &pr, NULL);
    ace = a ? nth_ace(a, 0) : NULL;
    if (ace) {
        CHECK(!memcmp(&ace->ObjectType, &g, sizeof g));
        CHECK(SID_WD((PSID)((BYTE *)&ace->ObjectType + sizeof(GUID))));
        EXPECT_EQ(ace->Header.AceSize, 8 + 4 + 16 + 12);
    }
    LocalFree(sd);
}
TEST(sddl_w_object_ace_bad_guid)
{
    static const char *bad[] = { "D:(OA;;CC;nothing;;WD)", "D:(OA;;CC;bf967aba-0de6-11d0-a285-00aa00b3f6c;;WD)", "D:(OA;;CC;bf967aba-0de6-11d0-a285-00aa00b3f6c0z;;WD)", "D:(OA;;CC;bf967aba0de611d0a28500aa00b3f6c0;;WD)" };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; SET_ERR(0);
        CHECKF(!cvtA(bad[i], 1, &sd, NULL), "\"%s\"", bad[i]);
        if (sd) LocalFree(sd);
        t_note("\"%s\": err %lu", bad[i], GetLastError());
    }
}
TEST(sddl_w_conditional_aces)
{
    static const struct { const char *s; BYTE type; } t[] = {
        { "D:(XA;;FX;;;S-1-1-0;(@User.Title==\"PM\"))",           ACCESS_ALLOWED_CALLBACK_ACE_TYPE },
        { "D:(XD;;FX;;;WD;(@User.Title==\"PM\"))",                ACCESS_DENIED_CALLBACK_ACE_TYPE },
        { "D:(XA;;FX;;;WD;(Member_of {SID(BA)}))",                ACCESS_ALLOWED_CALLBACK_ACE_TYPE },
        { "D:(XA;;GA;;;WD;(Member_of_Any {SID(BA),SID(SY)}))",    ACCESS_ALLOWED_CALLBACK_ACE_TYPE },
        { "D:(XA;;GA;;;WD;(Exists @User.Title))",                 ACCESS_ALLOWED_CALLBACK_ACE_TYPE },
        { "D:(XA;;GA;;;WD;(@User.Level>=3 && @User.Level<=7))",   ACCESS_ALLOWED_CALLBACK_ACE_TYPE },
        { "D:(XA;;GA;;;WD;(!(Exists @Device.x)))",                ACCESS_ALLOWED_CALLBACK_ACE_TYPE },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACE_HEADER *h; BOOL pr;
        if (!cvtA(t[i].s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", t[i].s, GetLastError()); continue; }
        a = get_dacl(sd, &pr, NULL);
        CHECKF(a && a->AceCount == 1, "\"%s\"", t[i].s);
        if (a && (h = nth_ace(a, 0))) {
            BYTE *sid, *cond;
            DWORD sidlen;
            CHECKF(h->AceType == t[i].type, "\"%s\": type 0x%x", t[i].s, h->AceType);
            sid = (BYTE *)h + 8; sidlen = GetLengthSid(sid);
            cond = sid + sidlen;
            CHECKF(h->AceSize > 8 + sidlen + 4, "\"%s\": size %u", t[i].s, h->AceSize);
            CHECKF(cond[0] == 'a' && cond[1] == 'r' && cond[2] == 't' && cond[3] == 'x', "\"%s\": missing artx signature", t[i].s);
            CHECKF(a->AclRevision >= ACL_REVISION_DS || 1, "");
            if (a->AclRevision != ACL_REVISION_DS) t_note("\"%s\": ACL revision %u", t[i].s, a->AclRevision);
            CHECK((h->AceSize & 3) == 0);
        }
        LocalFree(sd);
    }
}
TEST(sddl_w_conditional_ace_invalid)
{
    static const char *bad[] = {
        "D:(XA;;FX;;;WD;(@User.Title==))", "D:(XA;;FX;;;WD;(Member_of))", "D:(XA;;FX;;;WD;)",
        "D:(XA;;FX;;;WD;(@User.Title==\"PM\")", "D:(XA;;FX;;;WD)(A;;GA;;;WD;(Exists @User.x))",
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; SET_ERR(0);
        BOOL r = cvtA(bad[i], 1, &sd, NULL);
        if (r) { t_note("\"%s\" parsed", bad[i]); LocalFree(sd); }
        else t_note("\"%s\" rejected err %lu", bad[i], GetLastError());
    }
}
TEST(sddl_w_plain_ace_with_condition_field_rejected)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    if (cvtA("D:(A;;GA;;;WD;(Exists @User.x))", 1, &sd, NULL)) { t_note("accepted"); LocalFree(sd); }
    else t_note("rejected err %lu", GetLastError());
}

/* ---- ACE flags ---- */
TEST(sddl_w_ace_flag_table)
{
    static const struct { const char *f; BYTE v; } t[] = {
        { "", 0 }, { "OI", OBJECT_INHERIT_ACE }, { "CI", CONTAINER_INHERIT_ACE }, { "NP", NO_PROPAGATE_INHERIT_ACE },
        { "IO", INHERIT_ONLY_ACE }, { "ID", INHERITED_ACE },
        { "OICI", 3 }, { "CIOI", 3 }, { "OICIIO", 11 }, { "OICINP", 7 }, { "OICIIOID", 0x1b }, { "OICINPIOID", 0x1f },
        { "NPIO", 12 }, { "IOID", 0x18 }, { "OIIO", 9 }, { "CIID", 0x12 }, { "OIOI", 1 },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char s[128]; PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACE_HEADER *h; BOOL pr;
        sprintf(s, "D:(A;%s;GA;;;WD)", t[i].f);
        if (!cvtA(s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", s, GetLastError()); continue; }
        a = get_dacl(sd, &pr, NULL);
        if (a && (h = nth_ace(a, 0))) CHECKF(h->AceFlags == t[i].v, "\"%s\": flags 0x%x want 0x%x", s, h->AceFlags, t[i].v);
        else CHECKF(0, "\"%s\": no ace", s);
        LocalFree(sd);
    }
}
TEST(sddl_w_ace_flag_invalid)
{
    static const char *bad[] = { "D:(A;XX;GA;;;WD)", "D:(A;O;GA;;;WD)", "D:(A;OIC;GA;;;WD)", "D:(A;oi;GA;;;WD)", "D:(A;OI,CI;GA;;;WD)" };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; SET_ERR(0);
        CHECKF(!cvtA(bad[i], 1, &sd, NULL), "\"%s\"", bad[i]);
        if (sd) LocalFree(sd); else NOTE_ERR(ERROR_INVALID_ACL);
    }
}

/* ---- rights ---- */
static const struct { const char *r; DWORD v; } rights[] = {
    { "GA", GENERIC_ALL }, { "GR", GENERIC_READ }, { "GW", GENERIC_WRITE }, { "GX", GENERIC_EXECUTE },
    { "RC", READ_CONTROL }, { "SD", DELETE }, { "WD", WRITE_DAC }, { "WO", WRITE_OWNER },
    { "RP", ADS_RIGHT_DS_READ_PROP }, { "WP", ADS_RIGHT_DS_WRITE_PROP }, { "CC", ADS_RIGHT_DS_CREATE_CHILD },
    { "DC", ADS_RIGHT_DS_DELETE_CHILD }, { "LC", ADS_RIGHT_ACTRL_DS_LIST }, { "SW", ADS_RIGHT_DS_SELF },
    { "LO", ADS_RIGHT_DS_LIST_OBJECT }, { "DT", ADS_RIGHT_DS_DELETE_TREE }, { "CR", ADS_RIGHT_DS_CONTROL_ACCESS },
    { "FA", FILE_ALL_ACCESS }, { "FR", FILE_GENERIC_READ & ~SYNCHRONIZE | 0 }, { "FW", 0x120116 & ~SYNCHRONIZE | 0 },
    { "FX", 0x1200a0 & ~SYNCHRONIZE | 0 },
    { "KA", KEY_ALL_ACCESS }, { "KR", KEY_READ }, { "KW", KEY_WRITE }, { "KX", KEY_EXECUTE },
};
TEST(sddl_w_right_alias_values)
{
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof rights / sizeof rights[0]; i++) {
        char s[64]; PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACCESS_ALLOWED_ACE *ace; BOOL pr;
        DWORD want = rights[i].v;
        /* composite file aliases carry SYNCHRONIZE in their real values */
        if (!strcmp(rights[i].r, "FR")) want = 0x120089;
        if (!strcmp(rights[i].r, "FW")) want = 0x120116;
        if (!strcmp(rights[i].r, "FX")) want = 0x1200a0;
        if (!strcmp(rights[i].r, "FA")) want = 0x1f01ff;
        sprintf(s, "D:(A;;%s;;;WD)", rights[i].r);
        if (!cvtA(s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", s, GetLastError()); continue; }
        a = get_dacl(sd, &pr, NULL);
        ace = a ? nth_ace(a, 0) : NULL;
        CHECKF(ace && ace->Mask == want, "\"%s\": mask 0x%lx want 0x%lx", s, ace ? ace->Mask : 0, want);
        LocalFree(sd);
    }
}
TEST(sddl_w_right_alias_concatenation_ors)
{
    static const struct { const char *r; DWORD v; } t[] = {
        { "GRGW", GENERIC_READ | GENERIC_WRITE }, { "GRGWGX", GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE },
        { "GAGRGWGX", 0xf0000000 }, { "RCSDWDWO", 0xf0000 }, { "RPWPCCDCLCSWLODTCR", 0x1ff },
        { "CCDC", 3 }, { "GAGA", GENERIC_ALL }, { "RCRC", READ_CONTROL }, { "FAFRFWFX", 0x1f01ff },
        { "KAKRKWKX", 0xf003f }, { "RPCR", 0x110 },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char s[96]; PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACCESS_ALLOWED_ACE *ace; BOOL pr;
        sprintf(s, "D:(A;;%s;;;WD)", t[i].r);
        if (!cvtA(s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", s, GetLastError()); continue; }
        a = get_dacl(sd, &pr, NULL); ace = a ? nth_ace(a, 0) : NULL;
        CHECKF(ace && ace->Mask == t[i].v, "\"%s\": mask 0x%lx want 0x%lx", s, ace ? ace->Mask : 0, t[i].v);
        LocalFree(sd);
    }
}
TEST(sddl_w_right_numeric_forms)
{
    static const struct { const char *r; DWORD v; } t[] = {
        { "1", 1 }, { "10", 10 }, { "0x1", 1 }, { "0X40000000", GENERIC_WRITE }, { "0xFFFFFFFF", 0xffffffff },
        { "0xffffffff", 0xffffffff }, { "0x12019f", 0x12019f }, { "020000000000", GENERIC_READ }, { "07", 7 }, { "010", 8 },
        { "0x10000000", GENERIC_ALL }, { "0xDeAdBeEf", 0xdeadbeef }, { "0x1f01ff", 0x1f01ff }, { "4294967295", 0xffffffff },
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char s[96]; PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACCESS_ALLOWED_ACE *ace; BOOL pr;
        sprintf(s, "D:(A;;%s;;;WD)", t[i].r);
        if (!cvtA(s, 1, &sd, NULL)) { CHECKF(0, "\"%s\" failed err %lu", s, GetLastError()); continue; }
        a = get_dacl(sd, &pr, NULL); ace = a ? nth_ace(a, 0) : NULL;
        CHECKF(ace && ace->Mask == t[i].v, "\"%s\": mask 0x%lx want 0x%lx", s, ace ? ace->Mask : 0, t[i].v);
        LocalFree(sd);
    }
}
TEST(sddl_w_right_empty_is_zero)
{
    PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACCESS_ALLOWED_ACE *ace; BOOL pr;
    REQUIRE(CVT);
    CHECK(cvtA("D:(A;;;;;WD)", 1, &sd, NULL));
    if (!sd) return;
    a = get_dacl(sd, &pr, NULL); ace = a ? nth_ace(a, 0) : NULL;
    CHECK(ace && ace->Mask == 0);
    LocalFree(sd);
}
TEST(sddl_w_right_mixed_alias_and_hex_wine_cases)
{
    static const char *ok[] = { "S:(AU;;GR0xFFFFFFFF;;;WD)", "S:(AU;;0xFFFFFFFFGR;;;WD)", "S:(AU;;0xFFFFFGR;;;WD)" };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL;
        CHECKF(cvtA(ok[i], 1, &sd, NULL), "\"%s\" err %lu", ok[i], GetLastError());
        LocalFree(sd);
    }
}
TEST(sddl_w_right_invalid)
{
    static const char *bad[] = { "D:(A;;ROB;;;WD)", "D:(A;;GAX;;;WD)", "D:(A;;ga;;;WD)", "D:(A;;0xG;;;WD)", "D:(A;;GA GR;;;WD)", "D:(A;;-1;;;WD)", "D:(A;;ZZ;;;WD)" };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; SET_ERR(0);
        BOOL r = cvtA(bad[i], 1, &sd, NULL);
        if (r) { t_note("\"%s\" parsed", bad[i]); LocalFree(sd); continue; }
        if (!strcmp(bad[i], "D:(A;;ROB;;;WD)")) EXPECT_ERR(ERROR_INVALID_ACL); else NOTE_ERR(ERROR_INVALID_ACL);
    }
}

/* ---- multiple ACEs / layout ---- */
TEST(sddl_w_multiple_aces_order_and_size)
{
    PSECURITY_DESCRIPTOR sd = NULL; ACL *a; ACE_HEADER *h; BOOL pr;
    REQUIRE(CVT);
    CHECK(cvtA("D: (D;OICI;GA;;;BG) (D;OICI;GA;;;AN) (A;OICI;GAGRGWGX;;;AU) (A;OICI;GA;;;BA)", 1, &sd, NULL));
    if (!sd) return;
    a = get_dacl(sd, &pr, NULL);
    CHECK(a != NULL);
    if (a) {
        EXPECT_EQ(a->AclSize, 96); EXPECT_EQ(a->AceCount, 4);
        h = nth_ace(a, 0); CHECK(h && h->AceType == ACCESS_DENIED_ACE_TYPE && h->AceSize == 24);
        h = nth_ace(a, 1); CHECK(h && h->AceType == ACCESS_DENIED_ACE_TYPE && h->AceSize == 20);
        h = nth_ace(a, 2); CHECK(h && h->AceType == ACCESS_ALLOWED_ACE_TYPE && h->AceSize == 20 && ((ACCESS_ALLOWED_ACE *)h)->Mask == 0xf0000000);
        h = nth_ace(a, 3); CHECK(h && h->AceType == ACCESS_ALLOWED_ACE_TYPE && h->AceSize == 24);
    }
    LocalFree(sd);
}
TEST(sddl_w_aclsize_is_header_plus_aces)
{
    static const char *t[] = {
        "D:(A;;GA;;;WD)", "D:(A;;GA;;;WD)(A;;GR;;;BA)", "D:(A;;GA;;;S-1-5-21-1-2-3-500)", "D:(D;;GA;;;BA)(A;;GR;;;WD)(A;;GW;;;SY)(A;;CC;;;AU)",
        "S:(AU;SA;GA;;;WD)(AU;FA;GA;;;BA)", "D:(A;;GA;;;S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15)",
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; ACL *a; BOOL pr; int k; DWORD sum = 8;
        if (!cvtA(t[i], 1, &sd, NULL)) { CHECKF(0, "\"%s\" err %lu", t[i], GetLastError()); continue; }
        a = t[i][0] == 'D' ? get_dacl(sd, &pr, NULL) : get_sacl(sd, &pr);
        if (a) {
            for (k = 0; k < a->AceCount; k++) { ACE_HEADER *h = nth_ace(a, k); if (h) sum += h->AceSize; }
            CHECKF(a->AclSize == sum, "\"%s\": AclSize %u sum %lu", t[i], a->AclSize, sum);
            CHECK(IsValidAcl(a));
        }
        LocalFree(sd);
    }
}
TEST(sddl_w_layout_offsets_self_relative)
{
    PSECURITY_DESCRIPTOR sd = NULL; SECURITY_DESCRIPTOR_RELATIVE *r;
    REQUIRE(CVT);
    CHECK(cvtA("O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)", 1, &sd, NULL));
    if (!sd) return;
    r = sd;
    CHECK(r->Owner != 0 && r->Group != 0 && r->Dacl != 0 && r->Sacl != 0);
    CHECK(r->Owner >= sizeof *r && r->Group >= sizeof *r && r->Dacl >= sizeof *r && r->Sacl >= sizeof *r);
    EXPECT_EQ(r->Sbz1, 0);
    LocalFree(sd);
}
TEST(sddl_w_sacl_before_dacl_layout_valid)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    CHECK(cvtA("S:(AU;SA;GA;;;WD)D:(A;;GA;;;WD)", 1, &sd, NULL));
    if (sd) { CHECK(IsValidSecurityDescriptor(sd)); LocalFree(sd); }
}

/* ---- whitespace tolerance (Wine tests, verified against Windows) ---- */
TEST(sddl_w_whitespace_accepted)
{
    static const char *ok[] = {
        " D:(D;;GA;;;WD)", "D: (D;;GA;;;WD)", "D:( D;;GA;;;WD)", "D:(D; ;GA;;;WD)", "D:(D;; GA;;;WD)",
        "D:(D;;GA; ;;WD)", "D:(D;;GA;; ;WD)", "D:(D;;GA;;; WD)", "D:(D;;GA;;;WD )",
        "D: (D;OICI;GA;;;BG) (D;OICI;GA;;;AN) (A;OICI;GAGRGWGX;;;AU) (A;OICI;GA;;;BA)",
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL;
        CHECKF(cvtA(ok[i], 1, &sd, NULL), "\"%s\" err %lu", ok[i], GetLastError());
        LocalFree(sd);
    }
}
TEST(sddl_w_whitespace_rejected)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    CHECK(!cvtA("D:(D;;GA ;;;WD)", 1, &sd, NULL)); EXPECT_ERR(ERROR_INVALID_ACL);
    SET_ERR(0);
    CHECK(!cvtA("D:(D ;;GA;;;WD)", 1, &sd, NULL));
    { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_ACL || e == RPC_S_INVALID_STRING_UUID, "err %lu", e); }
}

/* ---- invalid strings ---- */
TEST(sddl_w_invalid_unknown_section)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    EXPECT_FAIL(!cvtA("ERROR:(D;;GA;;;WD)", 1, &sd, NULL), ERROR_INVALID_PARAMETER);
}
TEST(sddl_w_invalid_sid_in_ace)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(CVT);
    SET_ERR(0);
    EXPECT_FAIL(!cvtA("D:(D;;GA;;;WDANDSUCH)", 1, &sd, NULL), ERROR_INVALID_ACL);
    SET_ERR(0);
    EXPECT_FAIL(!cvtA("D:(D;;GA;;;Nonexistent account)", 1, &sd, NULL), ERROR_INVALID_ACL);
    SET_ERR(0);
    CHECK(cvtA("D:(D;;GA;;;S-1-0-0)", 1, &sd, NULL));
    LocalFree(sd);
}
TEST(sddl_w_invalid_structural_table)
{
    static const char *bad[] = {
        "D:(A;;GA;;;WD", "D:A;;GA;;;WD)", "D:()", "D:(A)", "D:(A;;GA)", "D:(A;;GA;;;)", "D:(A;;GA;;)", "D:((A;;GA;;;WD))",
        "D:(Q;;GA;;;WD)", "D:(AA;;GA;;;WD)", "D:(a;;GA;;;WD)", "D:(;;GA;;;WD)", "D:(A;;GA;;;WD)(", "D:(A;;GA;;;WD)x",
        "D:(A;;GA;;;WD)(A", "O:", "O:XX", "G:ZZ", "O:BA G:SY", "O:S-1-5", "O:S-1", "X:", "Z:(A;;GA;;;WD)", "(A;;GA;;;WD)",
        "D:(A;;GA;;;WD;;)", "D:(A;;GA;;;WD;x;y)", "D:(A;;GA;;;WD);", "S:(AU;SA)", "S:(ML;;NW)",
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; BOOL r; SET_ERR(0);
        r = cvtA(bad[i], 1, &sd, NULL);
        if (r) { t_note("\"%s\" unexpectedly parsed", bad[i]); LocalFree(sd); continue; }
        CHECKF(GetLastError() != 0, "\"%s\": no last error", bad[i]);
        t_note("\"%s\" -> err %lu", bad[i], GetLastError());
    }
}
TEST(sddl_w_invalid_hard_failures)
{
    static const char *bad[] = {
        "D:(A;;GA;;;WD", "D:()", "D:(Q;;GA;;;WD)", "D:(A;;GA;;;)", "D:(A;;GA;;;ZZ)", "O:XX", "G:ZZ", "X:",
    };
    unsigned i;
    REQUIRE(CVT);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        PSECURITY_DESCRIPTOR sd = NULL; BOOL r; SET_ERR(0);
        r = cvtA(bad[i], 1, &sd, NULL);
        CHECKF(!r, "\"%s\" parsed", bad[i]);
        if (r) LocalFree(sd);
    }
}

/* ---- A variant ---- */
TEST(sddl_a_basic)
{
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0; BOOL pr; ACL *a;
    REQUIRE(UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA));
    SET_ERR(0x99);
    CHECK(UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA)("D:(A;;GA;;;WD)", SDDL_REVISION_1, &sd, &sz));
    if (sd) { a = get_dacl(sd, &pr, NULL); CHECK(a && a->AceCount == 1); EXPECT_EQ(sz, GetSecurityDescriptorLength(sd)); LocalFree(sd); }
}
TEST(sddl_a_errors)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA));
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA)("D:(A;;GA;;;WD)", 0xdeadbeef, &sd, NULL));
    EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA)("D:(A;;ROB;;;WD)", SDDL_REVISION_1, &sd, NULL));
    EXPECT_ERR(ERROR_INVALID_ACL);
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA)("D:(A;;ROB;;;WD)", SDDL_REVISION_1, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA)(NULL, 0xdeadbeef, &sd, NULL));
    NOTE_ERR(ERROR_INVALID_PARAMETER);
}
TEST(sddl_a_matches_w)
{
    static const char *t[] = { "O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)", "D:P(A;OICI;GRGW;;;BA)", "S:(ML;;NW;;;ME)", "D:" };
    unsigned i;
    REQUIRE(UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR a = NULL, w = NULL; ULONG sa = 0, sw = 0;
        BOOL ra = UIN(AD, ConvertStringSecurityDescriptorToSecurityDescriptorA)(t[i], 1, &a, &sa);
        BOOL rw = cvtA(t[i], 1, &w, &sw);
        CHECKF(ra && rw, "\"%s\"", t[i]);
        if (ra && rw) { EXPECT_EQ(sa, sw); CHECKF(!memcmp(a, w, sa), "\"%s\": bytes differ", t[i]); }
        LocalFree(a); LocalFree(w);
    }
}
TEST(sddl_w_deterministic_bytes)
{
    PSECURITY_DESCRIPTOR a = NULL, b = NULL; ULONG sa = 0, sb = 0;
    REQUIRE(CVT);
    CHECK(cvtA("O:BAG:SYD:P(A;OICI;GA;;;WD)(D;;GW;;;AN)S:(AU;SAFA;GA;;;WD)", 1, &a, &sa));
    CHECK(cvtA("O:BAG:SYD:P(A;OICI;GA;;;WD)(D;;GW;;;AN)S:(AU;SAFA;GA;;;WD)", 1, &b, &sb));
    EXPECT_EQ(sa, sb);
    if (a && b && sa == sb) CHECK(!memcmp(a, b, sa));
    LocalFree(a); LocalFree(b);
}
