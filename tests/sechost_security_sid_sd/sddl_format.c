#include "ssh.h"
/* ConvertSecurityDescriptorToStringSecurityDescriptorW/A.
 * Decomp: SD==NULL or SecurityInformation==0 with a non-NULL out pointer -> success, "" (len 0);
 * otherwise NULL SD / NULL out -> 87; revision != 1 -> 1305; success SetLastError(0). */

#define FMT UIN(SH, ConvertSecurityDescriptorToStringSecurityDescriptorW)
#define CVT UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW)

static BOOL fmtW(PSECURITY_DESCRIPTOR sd, DWORD rev, SECURITY_INFORMATION si, WCHAR **out, ULONG *len)
{
    return FMT(sd, rev, si, out, len);
}

TEST(sddlfmt_w_null_sd_gives_empty_string)
{
    WCHAR *w = (WCHAR *)1; ULONG len = 99;
    REQUIRE(FMT);
    SET_ERR(0x77);
    CHECK(fmtW(NULL, SDDL_REVISION_1, ALL_SI, &w, &len));
    if (w && w != (WCHAR *)1) { EXPECT_EQ(w[0], 0); EXPECT_EQ(len, 0); EXPECT_EQ(GetLastError(), 0); CHECK(LocalSize(w) >= 2); LocalFree(w); }
}
TEST(sddlfmt_w_null_sd_ignores_revision)
{
    WCHAR *w = NULL; ULONG len = 99;
    REQUIRE(FMT);
    CHECK(fmtW(NULL, 0xdeadbeef, ALL_SI, &w, &len));
    if (w) { EXPECT_EQ(w[0], 0); LocalFree(w); }
}
TEST(sddlfmt_w_zero_secinfo_gives_empty_string)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BAG:SYD:(A;;GA;;;WD)", NULL); WCHAR *w = NULL; ULONG len = 99;
    REQUIRE(FMT);
    if (!sd) { CHECK(sd != NULL); return; }
    CHECK(fmtW(sd, SDDL_REVISION_1, 0, &w, &len));
    if (w) { EXPECT_EQ(w[0], 0); EXPECT_EQ(len, 0); LocalFree(w); }
    w = NULL;
    CHECK(fmtW(sd, 0xdeadbeef, 0, &w, NULL));
    if (w) { EXPECT_EQ(w[0], 0); LocalFree(w); }
    LocalFree(sd);
}
TEST(sddlfmt_w_null_len_pointer_allowed)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BA", NULL); WCHAR *w = NULL;
    REQUIRE(FMT);
    CHECK(fmtW(sd, SDDL_REVISION_1, ALL_SI, &w, NULL));
    if (w) { EXPECT_WSTR(w, L"O:BA"); LocalFree(w); }
    LocalFree(sd);
}
TEST(sddlfmt_w_null_out_pointer)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BA", NULL);
    REQUIRE(FMT);
    SET_ERR(0);
    EXPECT_FAIL(!fmtW(sd, SDDL_REVISION_1, ALL_SI, NULL, NULL), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_FAIL(!fmtW(NULL, SDDL_REVISION_1, ALL_SI, NULL, NULL), ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_FAIL(!fmtW(sd, SDDL_REVISION_1, 0, NULL, NULL), ERROR_INVALID_PARAMETER);
    LocalFree(sd);
}
TEST(sddlfmt_w_bad_revision)
{
    static const DWORD revs[] = { 0, 2, 3, 0xdeadbeef, 0xffffffff };
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BA", NULL); unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof revs / sizeof revs[0]; i++) {
        WCHAR *w = NULL; SET_ERR(0);
        CHECKF(!fmtW(sd, revs[i], ALL_SI, &w, NULL), "rev %lu", revs[i]);
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    }
    LocalFree(sd);
}
TEST(sddlfmt_w_revision_checked_after_null_checks)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BA", NULL); WCHAR *w = NULL;
    REQUIRE(FMT);
    SET_ERR(0);
    CHECK(!fmtW(sd, 5, ALL_SI, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    LocalFree(sd);
}
TEST(sddlfmt_w_invalid_sd_structure)
{
    SECURITY_DESCRIPTOR sd; WCHAR *w = NULL;
    REQUIRE(FMT);
    memset(&sd, 0, sizeof sd);
    sd.Revision = 7;       /* unsupported SD revision */
    SET_ERR(0);
    if (fmtW(&sd, SDDL_REVISION_1, ALL_SI, &w, NULL)) { t_note("SD revision 7 accepted"); LocalFree(w); }
    else t_note("SD revision 7 rejected err %lu", GetLastError());
}

/* The chain from Wine's test_ConvertSecurityDescriptorToString (expected strings are Windows-verified). */
TEST(sddlfmt_w_incremental_absolute_sd_chain)
{
    SECURITY_DESCRIPTOR desc; char sidbuf[256]; BYTE aclbuf[8192]; PACL pacl = (PACL)aclbuf; DWORD size; PSID psid = NULL, psid2 = NULL;
    SECURITY_INFORMATION si = ALL_SI;
    REQUIRE(FMT);
    InitializeSecurityDescriptor(&desc, SECURITY_DESCRIPTOR_REVISION);
#define EXPECT_SD(str) do { int ok_; const char *s_ = sd_to_str(&desc, si, &ok_); CHECK(ok_); EXPECT_STR(s_, str); } while (0)
    EXPECT_SD("");
    size = sizeof sidbuf; CreateWellKnownSid(WinLocalSid, NULL, sidbuf, &size);
    SetSecurityDescriptorOwner(&desc, sidbuf, FALSE);
    EXPECT_SD("O:S-1-2-0");
    SetSecurityDescriptorOwner(&desc, sidbuf, TRUE);
    EXPECT_SD("O:S-1-2-0");
    size = sizeof sidbuf; CreateWellKnownSid(WinLocalSystemSid, NULL, sidbuf, &size);
    SetSecurityDescriptorOwner(&desc, sidbuf, TRUE);
    EXPECT_SD("O:SY");
    CHECK(UIN(SH, ConvertStringSidToSidW)(L"S-1-5-21-93476-23408-4576", &psid));
    CHECK(UIN(SH, ConvertStringSidToSidW)(L"S-1-5-6", &psid2));
    SetSecurityDescriptorGroup(&desc, psid, TRUE);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576");
    si = GROUP_SECURITY_INFORMATION; EXPECT_SD("G:S-1-5-21-93476-23408-4576");
    si = OWNER_SECURITY_INFORMATION; EXPECT_SD("O:SY");
    si = ALL_SI;
    InitializeAcl(pacl, sizeof aclbuf, ACL_REVISION);
    SetSecurityDescriptorDacl(&desc, TRUE, pacl, TRUE);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:");
    SetSecurityDescriptorDacl(&desc, TRUE, pacl, FALSE);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:");
    AddAccessAllowedAceEx(pacl, ACL_REVISION, NO_PROPAGATE_INHERIT_ACE, 0xf0000000, psid2);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:(A;NP;GAGXGWGR;;;SU)");
    AddAccessAllowedAceEx(pacl, ACL_REVISION, INHERIT_ONLY_ACE | INHERITED_ACE, 0x00000003, psid2);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:(A;NP;GAGXGWGR;;;SU)(A;IOID;CCDC;;;SU)");
    AddAccessDeniedAceEx(pacl, ACL_REVISION, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, 0xffffffff, psid);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:(A;NP;GAGXGWGR;;;SU)(A;IOID;CCDC;;;SU)(D;OICI;0xffffffff;;;S-1-5-21-93476-23408-4576)");
    /* SACL */
    InitializeAcl(pacl, sizeof aclbuf, ACL_REVISION);
    SetSecurityDescriptorSacl(&desc, TRUE, pacl, FALSE);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:S:");
    SetSecurityDescriptorDacl(&desc, TRUE, NULL, FALSE);
    AddAuditAccessAceEx(pacl, ACL_REVISION, VALID_INHERIT_FLAGS, KEY_READ | KEY_WRITE, psid2, TRUE, TRUE);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:NO_ACCESS_CONTROLS:(AU;OICINPIOIDSAFA;CCDCLCSWRPRC;;;SU)");
    AddAuditAccessAceEx(pacl, ACL_REVISION, NO_PROPAGATE_INHERIT_ACE, FILE_GENERIC_READ | FILE_GENERIC_WRITE, psid2, TRUE, FALSE);
    EXPECT_SD("O:SYG:S-1-5-21-93476-23408-4576D:NO_ACCESS_CONTROLS:(AU;OICINPIOIDSAFA;CCDCLCSWRPRC;;;SU)(AU;NPSA;0x12019f;;;SU)");
#undef EXPECT_SD
    LocalFree(psid); LocalFree(psid2);
}

TEST(sddlfmt_w_len_out_param)
{
    static const char *t[] = { "O:SY", "O:BAG:SY", "D:", "D:(A;;GA;;;WD)", "O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)", "D:P(A;OICI;GA;;;BA)(A;;GR;;;WD)", "S:(ML;;NW;;;ME)" };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = sd_from_str(t[i], NULL); WCHAR *w = NULL; ULONG len = 0;
        if (!sd) { CHECKF(0, "\"%s\" parse", t[i]); continue; }
        if (fmtW(sd, 1, ALL_SI, &w, &len)) {
            size_t n = wcslen(w);
            CHECKF(len >= n, "\"%s\": len %lu strlen %llu", t[i], len, (unsigned long long)n);
            CHECKF(len <= n + 1 + 8, "\"%s\": len %lu strlen %llu", t[i], len, (unsigned long long)n);
            if (len != n) t_note("\"%s\": len %lu != strlen %llu", t[i], len, (unsigned long long)n);
            CHECK(LocalSize(w) >= (n + 1) * 2);
            LocalFree(w);
        } else CHECKF(0, "\"%s\" format err %lu", t[i], GetLastError());
        LocalFree(sd);
    }
}

/* ---- SECURITY_INFORMATION mask table ---- */
TEST(sddlfmt_w_secinfo_mask_table)
{
    static const char *src = "O:BAG:SYD:P(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)(ML;;NW;;;ME)";
    static const struct { SECURITY_INFORMATION si; const char *want; } t[] = {
        { OWNER_SECURITY_INFORMATION, "O:BA" },
        { GROUP_SECURITY_INFORMATION, "G:SY" },
        { OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, "O:BAG:SY" },
        { DACL_SECURITY_INFORMATION, "D:P(A;;GA;;;WD)" },
        { SACL_SECURITY_INFORMATION, "S:(AU;SAFA;GA;;;WD)" },
        { LABEL_SECURITY_INFORMATION, "S:(ML;;NW;;;ME)" },
        { SACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION, "S:(AU;SAFA;GA;;;WD)(ML;;NW;;;ME)" },
        { OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, "O:BAD:P(A;;GA;;;WD)" },
        { GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, "G:SYD:P(A;;GA;;;WD)" },
        { OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, "O:BAG:SYD:P(A;;GA;;;WD)" },
        { ALL_SI | LABEL_SECURITY_INFORMATION, "O:BAG:SYD:P(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)(ML;;NW;;;ME)" },
        { ALL_SI, "O:BAG:SYD:P(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)" },
        { DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION, "D:P(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)" },
    };
    PSECURITY_DESCRIPTOR sd = sd_from_str(src, NULL); unsigned i;
    REQUIRE(FMT);
    if (!sd) { CHECK(sd != NULL); return; }
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        int ok; const char *s = sd_to_str(sd, t[i].si, &ok);
        CHECKF(ok, "si 0x%lx failed err %lu", t[i].si, GetLastError());
        CHECKF(!strcmp(s, t[i].want), "si 0x%lx: got \"%s\" want \"%s\"", t[i].si, s, t[i].want);
    }
    LocalFree(sd);
}
TEST(sddlfmt_w_secinfo_missing_parts_skipped)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BA", NULL); int ok; const char *s;
    REQUIRE(FMT);
    s = sd_to_str(sd, ALL_SI, &ok); CHECK(ok); EXPECT_STR(s, "O:BA");
    s = sd_to_str(sd, GROUP_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "");
    s = sd_to_str(sd, DACL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "");
    s = sd_to_str(sd, SACL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "");
    LocalFree(sd);
}
TEST(sddlfmt_w_secinfo_unprotected_flags_not_output)
{
    /* PROTECTED/UNPROTECTED_*_SECURITY_INFORMATION are not selectors: they print nothing on their own */
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BAD:P(A;;GA;;;WD)", NULL); int ok; const char *s;
    REQUIRE(FMT);
    s = sd_to_str(sd, PROTECTED_DACL_SECURITY_INFORMATION, &ok);
    if (ok) t_note("PROTECTED_DACL only -> \"%s\"", s);
    s = sd_to_str(sd, OWNER_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, &ok);
    if (ok) { EXPECT_STR(s, "O:BA"); }
    LocalFree(sd);
}
TEST(sddlfmt_w_secinfo_attribute_scope_backup_selectors)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BAG:SYD:(A;;GA;;;WD)S:(AU;SA;GA;;;WD)(ML;;NW;;;ME)", NULL); int ok; const char *s;
    REQUIRE(FMT);
    s = sd_to_str(sd, ATTRIBUTE_SECURITY_INFORMATION, &ok);   /* 0x20 */
    CHECK(ok); EXPECT_STR(s, "");
    s = sd_to_str(sd, SCOPE_SECURITY_INFORMATION, &ok);       /* 0x40 */
    CHECK(ok); EXPECT_STR(s, "");
    s = sd_to_str(sd, BACKUP_SECURITY_INFORMATION, &ok);
    if (ok) t_note("BACKUP_SECURITY_INFORMATION only -> \"%s\"", s);
    s = sd_to_str(sd, OWNER_SECURITY_INFORMATION | ATTRIBUTE_SECURITY_INFORMATION, &ok);
    CHECK(ok); EXPECT_STR(s, "O:BA");
    LocalFree(sd);
}
TEST(sddlfmt_w_label_requires_label_bit)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("S:(ML;;NW;;;ME)", NULL); int ok; const char *s;
    REQUIRE(FMT);
    s = sd_to_str(sd, SACL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "S:");
    s = sd_to_str(sd, LABEL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "S:(ML;;NW;;;ME)");
    LocalFree(sd);
}

/* ---- ACE formatting from API-built ACLs ---- */
static const char *fmt_dacl_mask(DWORD mask, int *ok)
{
    BYTE buf[256]; PACL a = (PACL)buf; SIDBUF sb; SECURITY_DESCRIPTOR sd;
    InitializeAcl(a, sizeof buf, ACL_REVISION);
    AddAccessAllowedAce(a, ACL_REVISION, mask, mk_sid(&sb, 1, 1, 0));
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, a, FALSE);
    return sd_to_str(&sd, DACL_SECURITY_INFORMATION, ok);
}
TEST(sddlfmt_w_right_single_bits)
{
    static const struct { DWORD m; const char *r; } t[] = {
        { 0x00000001, "CC" }, { 0x00000002, "DC" }, { 0x00000004, "LC" }, { 0x00000008, "SW" }, { 0x00000010, "RP" },
        { 0x00000020, "WP" }, { 0x00000040, "DT" }, { 0x00000080, "LO" }, { 0x00000100, "CR" },
        { 0x00010000, "SD" }, { 0x00020000, "RC" }, { 0x00040000, "WD" }, { 0x00080000, "WO" },
        { 0x10000000, "GA" }, { 0x20000000, "GX" }, { 0x40000000, "GW" }, { 0x80000000, "GR" },
    };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char want[64]; int ok; const char *s = fmt_dacl_mask(t[i].m, &ok);
        sprintf(want, "D:(A;;%s;;;WD)", t[i].r);
        CHECKF(ok && !strcmp(s, want), "mask 0x%lx: got \"%s\" want \"%s\"", t[i].m, s, want);
    }
}
TEST(sddlfmt_w_right_combinations_bit_order)
{
    static const struct { DWORD m; const char *r; } t[] = {
        { 0xf0000000, "GAGXGWGR" }, { 0x00000003, "CCDC" }, { 0x2001f, "CCDCLCSWRPRC" },
        { 0x000f0000, "SDRCWDWO" }, { 0x1ff, "CCDCLCSWRPWPDTLOCR" }, { 0x80000001, "CCGR" }, { 0x10020000, "RCGA" },
        { 0x0, NULL },
    };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char want[96]; int ok; const char *s = fmt_dacl_mask(t[i].m, &ok);
        if (!t[i].r) { t_note("mask 0 -> \"%s\"", s); continue; }
        sprintf(want, "D:(A;;%s;;;WD)", t[i].r);
        CHECKF(ok && !strcmp(s, want), "mask 0x%lx: got \"%s\" want \"%s\"", t[i].m, s, want);
    }
}
TEST(sddlfmt_w_right_composite_exact_matches)
{
    static const struct { DWORD m; const char *r; } t[] = {
        { 0x1f01ff, "FA" }, { 0x120089, "FR" }, { 0x120116, "FW" }, { 0x1200a0, "FX" },
        { 0xf003f, "KA" }, { 0x20019, "KR" }, { 0x20006, "KW" },
    };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char want[96]; int ok; const char *s = fmt_dacl_mask(t[i].m, &ok);
        sprintf(want, "D:(A;;%s;;;WD)", t[i].r);
        CHECKF(ok && !strcmp(s, want), "mask 0x%lx: got \"%s\" want \"%s\"", t[i].m, s, want);
    }
}
TEST(sddlfmt_w_right_hex_fallback)
{
    static const DWORD t[] = { 0x12019f, 0x1200a9, 0xffffffff, 0x100000, 0x1f01ff | 0x100000 + 0, 0x1301bf, 0xdeadbeef, 0x0fffffff, 0x01000000, 0x02000000 };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        char want[64]; int ok; const char *s;
        if (t[i] == 0x1f01ff) continue;
        s = fmt_dacl_mask(t[i], &ok);
        sprintf(want, "D:(A;;0x%lx;;;WD)", t[i]);
        if (t[i] == 0x1200a9 || t[i] == 0x12019f || t[i] == 0xffffffff)
            CHECKF(ok && !strcmp(s, want), "mask 0x%lx: got \"%s\"", t[i], s);
        else if (strcmp(s, want)) t_note("mask 0x%lx -> \"%s\"", t[i], s);
    }
}

TEST(sddlfmt_w_ace_flags_order)
{
    static const struct { BYTE f; const char *s; } t[] = {
        { 0, "" }, { OBJECT_INHERIT_ACE, "OI" }, { CONTAINER_INHERIT_ACE, "CI" }, { NO_PROPAGATE_INHERIT_ACE, "NP" },
        { INHERIT_ONLY_ACE, "IO" }, { INHERITED_ACE, "ID" }, { 3, "OICI" }, { 7, "OICINP" }, { 0xb, "OICIIO" },
        { 0x1b, "OICIIOID" }, { 0x1f, "OICINPIOID" }, { 0xc, "NPIO" }, { 0x18, "IOID" }, { 0x12, "CIID" },
    };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        BYTE buf[256]; PACL a = (PACL)buf; SIDBUF sb; SECURITY_DESCRIPTOR sd; int ok; const char *s; char want[64];
        InitializeAcl(a, sizeof buf, ACL_REVISION);
        AddAccessAllowedAceEx(a, ACL_REVISION, t[i].f, GENERIC_ALL, mk_sid(&sb, 1, 1, 0));
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, a, FALSE);
        s = sd_to_str(&sd, DACL_SECURITY_INFORMATION, &ok);
        sprintf(want, "D:(A;%s;GA;;;WD)", t[i].s);
        CHECKF(ok && !strcmp(s, want), "flags 0x%x: got \"%s\" want \"%s\"", t[i].f, s, want);
    }
}
TEST(sddlfmt_w_audit_flags)
{
    static const struct { BOOL succ, fail; const char *s; } t[] = { { TRUE, FALSE, "SA" }, { FALSE, TRUE, "FA" }, { TRUE, TRUE, "SAFA" } };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        BYTE buf[256]; PACL a = (PACL)buf; SIDBUF sb; SECURITY_DESCRIPTOR sd; int ok; const char *s; char want[64];
        InitializeAcl(a, sizeof buf, ACL_REVISION);
        AddAuditAccessAceEx(a, ACL_REVISION, 0, GENERIC_ALL, mk_sid(&sb, 1, 1, 0), t[i].succ, t[i].fail);
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorSacl(&sd, TRUE, a, FALSE);
        s = sd_to_str(&sd, SACL_SECURITY_INFORMATION, &ok);
        sprintf(want, "S:(AU;%s;GA;;;WD)", t[i].s);
        CHECKF(ok && !strcmp(s, want), "got \"%s\" want \"%s\"", s, want);
    }
}
TEST(sddlfmt_w_mandatory_label_ace_from_api)
{
    BYTE buf[256]; PACL a = (PACL)buf; SIDBUF sb; SECURITY_DESCRIPTOR sd; int ok; const char *s;
    REQUIRE(FMT);
    InitializeAcl(a, sizeof buf, ACL_REVISION);
    if (!AddMandatoryAce(a, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, mk_sid(&sb, 16, 1, 8192))) { t_skip("AddMandatoryAce"); return; }
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorSacl(&sd, TRUE, a, FALSE);
    s = sd_to_str(&sd, LABEL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "S:(ML;;NW;;;ME)");
    s = sd_to_str(&sd, SACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "S:(ML;;NW;;;ME)");
    s = sd_to_str(&sd, SACL_SECURITY_INFORMATION, &ok); CHECK(ok); EXPECT_STR(s, "S:");
}
TEST(sddlfmt_w_label_masks)
{
    static const struct { DWORD m; const char *s; } t[] = { { 1, "NW" }, { 2, "NR" }, { 4, "NX" }, { 3, "NWNR" }, { 5, "NWNX" }, { 6, "NRNX" }, { 7, "NWNRNX" } };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        BYTE buf[256]; PACL a = (PACL)buf; SIDBUF sb; SECURITY_DESCRIPTOR sd; int ok; const char *s; char want[64];
        InitializeAcl(a, sizeof buf, ACL_REVISION);
        if (!AddMandatoryAce(a, ACL_REVISION, 0, t[i].m, mk_sid(&sb, 16, 1, 4096))) { t_skip("AddMandatoryAce"); return; }
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorSacl(&sd, TRUE, a, FALSE);
        s = sd_to_str(&sd, LABEL_SECURITY_INFORMATION, &ok);
        sprintf(want, "S:(ML;;%s;;;LW)", t[i].s);
        CHECKF(ok && !strcmp(s, want), "mask %lu: got \"%s\" want \"%s\"", t[i].m, s, want);
    }
}
TEST(sddlfmt_w_control_flags_output)
{
    static const struct { WORD ctl; const char *d; const char *s; } t[] = {
        { SE_DACL_PROTECTED, "D:P", "S:" },
        { SE_DACL_AUTO_INHERITED, "D:AI", "S:" },
        { SE_DACL_AUTO_INHERIT_REQ, "D:AR", "S:" },
        { SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED, "D:PAI", "S:" },
        { SE_SACL_PROTECTED, "D:", "S:P" },
        { SE_SACL_AUTO_INHERITED, "D:", "S:AI" },
        { SE_SACL_AUTO_INHERIT_REQ, "D:", "S:AR" },
        { SE_SACL_PROTECTED | SE_SACL_AUTO_INHERITED, "D:", "S:PAI" },
        { SE_DACL_PROTECTED | SE_SACL_PROTECTED, "D:P", "S:P" },
        { SE_DACL_AUTO_INHERITED | SE_SACL_AUTO_INHERITED, "D:AI", "S:AI" },
    };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        BYTE b1[64], b2[64]; PACL d = (PACL)b1, sa = (PACL)b2; SECURITY_DESCRIPTOR sd; int ok; const char *s; char want[64];
        InitializeAcl(d, sizeof b1, ACL_REVISION); InitializeAcl(sa, sizeof b2, ACL_REVISION);
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, d, FALSE); SetSecurityDescriptorSacl(&sd, TRUE, sa, FALSE);
        SetSecurityDescriptorControl(&sd, t[i].ctl, t[i].ctl);
        s = sd_to_str(&sd, ALL_SI, &ok);
        sprintf(want, "%s%s", t[i].d, t[i].s);
        CHECKF(ok && !strcmp(s, want), "ctl 0x%x: got \"%s\" want \"%s\"", t[i].ctl, s, want);
        /* DACL-only request must not print SACL control flags and vice versa */
        s = sd_to_str(&sd, DACL_SECURITY_INFORMATION, &ok); CHECKF(ok && !strcmp(s, t[i].d), "dacl-only ctl 0x%x: \"%s\"", t[i].ctl, s);
        s = sd_to_str(&sd, SACL_SECURITY_INFORMATION, &ok); CHECKF(ok && !strcmp(s, t[i].s), "sacl-only ctl 0x%x: \"%s\"", t[i].ctl, s);
    }
}
TEST(sddlfmt_w_control_flags_ar_ai_combined)
{
    BYTE b1[64]; PACL d = (PACL)b1; SECURITY_DESCRIPTOR sd; int ok; const char *s;
    REQUIRE(FMT);
    InitializeAcl(d, sizeof b1, ACL_REVISION);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, d, FALSE);
    SetSecurityDescriptorControl(&sd, SE_DACL_AUTO_INHERIT_REQ | SE_DACL_AUTO_INHERITED, SE_DACL_AUTO_INHERIT_REQ | SE_DACL_AUTO_INHERITED);
    s = sd_to_str(&sd, DACL_SECURITY_INFORMATION, &ok); CHECK(ok);
    t_note("AR|AI -> \"%s\"", s);
    CHECK(!strcmp(s, "D:ARAI") || !strcmp(s, "D:AIAR"));
    SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
    s = sd_to_str(&sd, DACL_SECURITY_INFORMATION, &ok); CHECK(ok);
    t_note("P|AR|AI -> \"%s\"", s);
}
TEST(sddlfmt_w_owner_group_defaulted_not_printed)
{
    SECURITY_DESCRIPTOR sd; SIDBUF b1, b2; int ok; const char *s;
    REQUIRE(FMT);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&sd, mk_sid(&b1, 5, 1, 18), TRUE);
    SetSecurityDescriptorGroup(&sd, mk_sid(&b2, 5, 2, 32, 544), TRUE);
    s = sd_to_str(&sd, ALL_SI, &ok); CHECK(ok); EXPECT_STR(s, "O:SYG:BA");
}
TEST(sddlfmt_w_dacl_defaulted_not_printed)
{
    BYTE b1[64]; PACL d = (PACL)b1; SECURITY_DESCRIPTOR sd; int ok; const char *s;
    REQUIRE(FMT);
    InitializeAcl(d, sizeof b1, ACL_REVISION);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, d, TRUE);
    s = sd_to_str(&sd, ALL_SI, &ok); CHECK(ok); EXPECT_STR(s, "D:");
}
TEST(sddlfmt_w_null_dacl_prints_no_access_control)
{
    SECURITY_DESCRIPTOR sd; int ok; const char *s;
    REQUIRE(FMT);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    s = sd_to_str(&sd, DACL_SECURITY_INFORMATION, &ok); CHECK(ok);
    t_note("NULL DACL alone -> \"%s\"", s);
    CHECK(!strcmp(s, "D:NO_ACCESS_CONTROL"));
    s = sd_to_str(&sd, ALL_SI, &ok); CHECK(ok); t_note("NULL DACL, all -> \"%s\"", s);
    CHECK(!strcmp(s, "D:NO_ACCESS_CONTROL"));
}
TEST(sddlfmt_w_dacl_not_present_prints_nothing)
{
    SECURITY_DESCRIPTOR sd; int ok; const char *s;
    REQUIRE(FMT);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    s = sd_to_str(&sd, ALL_SI, &ok); CHECK(ok); EXPECT_STR(s, "");
    SetSecurityDescriptorDacl(&sd, FALSE, NULL, FALSE);
    s = sd_to_str(&sd, ALL_SI, &ok); CHECK(ok); EXPECT_STR(s, "");
}

TEST(sddlfmt_w_sid_alias_output_table)
{
    static const struct { unsigned long long a; int n; DWORD s[2]; const char *str; } t[] = {
        { 1, 1, {0}, "WD" }, { 5, 1, {18}, "SY" }, { 5, 2, {32, 544}, "BA" }, { 5, 2, {32, 545}, "BU" }, { 5, 2, {32, 546}, "BG" },
        { 5, 2, {32, 547}, "PU" }, { 5, 2, {32, 548}, "AO" }, { 5, 2, {32, 549}, "SO" }, { 5, 2, {32, 550}, "PO" },
        { 5, 2, {32, 551}, "BO" }, { 5, 2, {32, 552}, "RE" }, { 5, 1, {11}, "AU" }, { 5, 1, {7}, "AN" },
        { 3, 1, {0}, "CO" }, { 3, 1, {1}, "CG" }, { 5, 1, {10}, "PS" }, { 5, 1, {19}, "LS" }, { 5, 1, {20}, "NS" },
        { 5, 1, {9}, "ED" }, { 5, 1, {4}, "IU" }, { 5, 1, {6}, "SU" }, { 5, 1, {2}, "NU" }, { 5, 1, {12}, "RC" },
        { 3, 1, {4}, "OW" }, { 16, 1, {4096}, "LW" }, { 16, 1, {8192}, "ME" }, { 16, 1, {12288}, "HI" }, { 16, 1, {16384}, "SI" },
        { 5, 2, {32, 554}, "RU" }, { 5, 2, {32, 555}, "RD" }, { 5, 2, {32, 556}, "NO" }, { 5, 2, {32, 558}, "MU" },
        { 5, 2, {32, 559}, "LU" }, { 5, 2, {32, 569}, "CY" }, { 5, 2, {32, 573}, "ER" }, { 5, 2, {32, 574}, "CD" },
        { 5, 2, {32, 575}, "RA" }, { 5, 2, {32, 576}, "ES" }, { 5, 2, {32, 577}, "MS" }, { 5, 1, {33}, "WR" },
        { 15, 2, {2, 1}, "AC" }, { 5, 2, {32, 568}, "IS" },
        /* no alias: numeric */
        { 5, 1, {3}, "S-1-5-3" }, { 5, 1, {1}, "S-1-5-1" }, { 1, 1, {1}, "S-1-1-1" }, { 2, 1, {0}, "S-1-2-0" }, { 5, 1, {13}, "S-1-5-13" },
        { 5, 2, {32, 600}, "S-1-5-32-600" }, { 5, 5, {21, 1, 2, 3, 1000}, NULL },
    };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        SECURITY_DESCRIPTOR sd; SIDBUF b; PISID s = (PISID)mk_sid(&b, t[i].a, 0); int ok; const char *r; char want[64]; int k;
        s->SubAuthorityCount = (BYTE)t[i].n;
        for (k = 0; k < t[i].n && k < 2; k++) s->SubAuthority[k] = t[i].s[k];
        if (t[i].n == 5) { s->SubAuthority[0] = 21; s->SubAuthority[1] = 1; s->SubAuthority[2] = 2; s->SubAuthority[3] = 3; s->SubAuthority[4] = 1000; }
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorOwner(&sd, s, FALSE);
        r = sd_to_str(&sd, OWNER_SECURITY_INFORMATION, &ok);
        sprintf(want, "O:%s", t[i].str ? t[i].str : "S-1-5-21-1-2-3-1000");
        CHECKF(ok && !strcmp(r, want), "got \"%s\" want \"%s\"", r, want);
    }
}
TEST(sddlfmt_w_invalid_owner_sid)
{
    SECURITY_DESCRIPTOR sd; SIDBUF b; WCHAR *w = NULL;
    REQUIRE(FMT);
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    ((PISID)mk_sid(&b, 5, 1, 18))->Revision = 3;
    SetSecurityDescriptorOwner(&sd, &b, FALSE);
    SET_ERR(0);
    if (fmtW(&sd, 1, OWNER_SECURITY_INFORMATION, &w, NULL)) { t_note("invalid owner sid accepted"); LocalFree(w); }
    else { t_note("invalid owner sid err %lu", GetLastError()); CHECK(GetLastError() != 0); }
}

TEST(sddlfmt_w_object_ace_output)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("D:(OA;;CC;bf967aba-0de6-11d0-a285-00aa00b3f6c0;bf967a86-0de6-11d0-a285-00aa00b3f6c0;WD)", NULL); int ok; const char *s;
    REQUIRE(FMT);
    if (!sd) { CHECK(sd != NULL); return; }
    s = sd_to_str(sd, DACL_SECURITY_INFORMATION, &ok); CHECK(ok);
    EXPECT_STR(s, "D:(OA;;CC;bf967aba-0de6-11d0-a285-00aa00b3f6c0;bf967a86-0de6-11d0-a285-00aa00b3f6c0;WD)");
    LocalFree(sd);
}
TEST(sddlfmt_w_conditional_ace_output)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("D:(XA;;FX;;;WD;(Member_of {SID(BA)}))", NULL); int ok; const char *s;
    REQUIRE(FMT);
    if (!sd) { CHECK(sd != NULL); return; }
    s = sd_to_str(sd, DACL_SECURITY_INFORMATION, &ok); CHECK(ok);
    t_note("conditional ACE -> \"%s\"", s);
    CHECK(!strncmp(s, "D:(XA;;FX;;;WD;(", 16));
    LocalFree(sd);
}

/* ---- absolute vs self-relative ---- */
TEST(sddlfmt_w_absolute_equals_self_relative)
{
    static const char *t[] = { "O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)", "D:P(A;OICI;GRGW;;;BA)(D;;GX;;;AN)", "S:(ML;;NW;;;ME)", "O:BA", "D:", "D:AIS:AR" };
    unsigned i;
    REQUIRE(FMT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sr = sd_from_str(t[i], NULL); BYTE abs_buf[4096]; DWORD abs_sz = sizeof abs_buf;
        BYTE ob[256], gb[256], db[2048], sb[2048]; DWORD o = sizeof ob, g = sizeof gb, d = sizeof db, s_ = sizeof sb;
        int ok1, ok2; char first[2048];
        if (!sr) { CHECKF(0, "parse \"%s\"", t[i]); continue; }
        strcpy(first, sd_to_str(sr, ALL_SI | LABEL_SECURITY_INFORMATION, &ok1));
        if (!MakeAbsoluteSD(sr, abs_buf, &abs_sz, (PACL)db, &d, (PACL)sb, &s_, ob, &o, gb, &g)) { CHECKF(0, "MakeAbsoluteSD %lu", GetLastError()); LocalFree(sr); continue; }
        CHECK(!(((SECURITY_DESCRIPTOR *)abs_buf)->Control & SE_SELF_RELATIVE));
        CHECKF(ok1 && !strcmp(sd_to_str(abs_buf, ALL_SI | LABEL_SECURITY_INFORMATION, &ok2), first) && ok2, "\"%s\": absolute differs: \"%s\"", t[i], sd_to_str(abs_buf, ALL_SI | LABEL_SECURITY_INFORMATION, &ok2));
        LocalFree(sr);
    }
}

/* ---- round trips ---- */
static int bytes_equal_after_rt(const char *in, char *why, size_t cap)
{
    PSECURITY_DESCRIPTOR a, b; ULONG sa = 0, sb = 0; const char *s; int ok, res = 0;
    a = sd_from_str(in, &sa);
    if (!a) { snprintf(why, cap, "parse of input failed err %lu", GetLastError()); return 0; }
    s = sd_to_str(a, ALL_SI | LABEL_SECURITY_INFORMATION, &ok);
    if (!ok) { snprintf(why, cap, "format failed err %lu", GetLastError()); LocalFree(a); return 0; }
    b = sd_from_str(s, &sb);
    if (!b) { snprintf(why, cap, "reparse of \"%s\" failed err %lu", s, GetLastError()); LocalFree(a); return 0; }
    if (sa != sb || memcmp(a, b, sa)) snprintf(why, cap, "binary differs after round trip via \"%s\"", s);
    else res = 1;
    LocalFree(a); LocalFree(b);
    return res;
}

static const char *const rt_hand[] = {
    "", "D:", "S:", "D:S:", "O:BA", "G:SY", "O:BAG:SY", "O:SYG:BA", "O:WD", "O:S-1-5-21-1-2-3-500", "G:S-1-5-21-1-2-3-513",
    "O:BAG:SYD:", "O:BAG:SYD:(A;;GA;;;WD)", "D:(A;;GA;;;WD)", "D:(D;;GA;;;WD)", "D:P(A;;GA;;;WD)", "D:AI(A;;GA;;;WD)", "D:PAI(A;;GA;;;WD)",
    "D:(A;;GA;;;WD)(A;;GR;;;BA)", "D:(D;;GA;;;AN)(A;;GA;;;WD)", "D:(A;OICI;GA;;;BA)(A;OICI;GA;;;SY)(A;OICI;GR;;;WD)",
    "D:(A;CI;GA;;;BA)", "D:(A;OI;GA;;;BA)", "D:(A;NP;GA;;;BA)", "D:(A;IO;GA;;;BA)", "D:(A;ID;GA;;;BA)", "D:(A;OICIIO;GA;;;CO)",
    "D:(A;OICINPIOID;GA;;;BA)", "D:(A;;FA;;;SY)(A;;FA;;;BA)(A;;FR;;;BU)", "D:(A;;KA;;;BA)(A;;KR;;;BU)", "D:(A;;0x1200a9;;;BU)",
    "D:(A;;0x12019f;;;AU)", "D:(A;;0x1f01ff;;;S-1-5-21-1-2-3-500)", "D:(A;;CCDCLCSWRPWPDTLOCR;;;BA)", "D:(A;;SDRCWDWO;;;BA)",
    "D:(A;;GAGXGWGR;;;BA)", "D:(A;;RC;;;OW)", "D:(A;;RC;;;CO)(A;;GA;;;CG)", "D:(A;;GA;;;PS)", "D:(A;;GA;;;LS)(A;;GA;;;NS)",
    "D:(A;;GA;;;IU)(A;;GA;;;NU)(A;;GA;;;SU)", "D:(A;;GA;;;RC)", "D:(A;;GA;;;BU)(A;;GA;;;BG)(A;;GA;;;PU)",
    "D:(A;;GA;;;AO)(A;;GA;;;SO)(A;;GA;;;PO)(A;;GA;;;BO)(A;;GA;;;RE)", "D:(A;;GA;;;AC)", "D:(A;;GA;;;S-1-15-3-1024-1-2-3-4-5-6-7)",
    "S:(AU;SA;GA;;;WD)", "S:(AU;FA;GA;;;WD)", "S:(AU;SAFA;GA;;;WD)", "S:(AU;OICISAFA;GA;;;WD)", "S:(AU;SA;FA;;;WD)",
    "S:(ML;;NW;;;LW)", "S:(ML;;NW;;;ME)", "S:(ML;;NWNR;;;HI)", "S:(ML;;NWNRNX;;;SI)", "S:(ML;OICI;NW;;;ME)", "S:(AU;SA;GA;;;WD)(ML;;NW;;;ME)",
    "S:P(AU;SA;GA;;;WD)", "S:AI(AU;SA;GA;;;WD)", "S:AR", "S:PAI",
    "O:BAG:SYD:P(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)S:(AU;SAFA;GA;;;WD)", "O:S-1-5-21-1-2-3-500G:S-1-5-21-1-2-3-513D:PAI(A;;FA;;;SY)(A;;FA;;;S-1-5-21-1-2-3-500)",
    "D:(OA;;CC;bf967aba-0de6-11d0-a285-00aa00b3f6c0;;WD)", "D:(OD;;CC;;bf967a86-0de6-11d0-a285-00aa00b3f6c0;WD)",
    "D:(OA;;CCDC;bf967aba-0de6-11d0-a285-00aa00b3f6c0;bf967a86-0de6-11d0-a285-00aa00b3f6c0;BA)",
    "S:(OU;SA;CC;bf967aba-0de6-11d0-a285-00aa00b3f6c0;;WD)",
    "D:(XA;;FX;;;WD;(Member_of {SID(BA)}))", "D:(XD;;FX;;;WD;(@User.Title==\"PM\"))",
    "D:(A;;GA;;;S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15)", "D:(A;;GA;;;S-1-0x000100000000-1)",
    "D:AIS:AI", "D:ARAIS:ARAI", "D:PS:P", "D:NO_ACCESS_CONTROL", "D:NO_ACCESS_CONTROLS:(AU;SA;GA;;;WD)", "D:PNO_ACCESS_CONTROL",
};
#define NRTHAND (sizeof rt_hand / sizeof rt_hand[0])

TEST(sddlrt_hand_table)
{
    unsigned i;
    REQUIRE(FMT); REQUIRE(CVT);
    for (i = 0; i < NRTHAND; i++) {
        char why[600];
        CHECKF(bytes_equal_after_rt(rt_hand[i], why, sizeof why), "\"%s\": %s", rt_hand[i], why);
    }
}

/* Generated cross product: canonical strings must come back verbatim. */
static const char *const g_sid[] = { "WD", "BA", "SY", "AU", "AN", "BU", "S-1-5-21-1-2-3-500", "S-1-5-32-600", "LS", "OW" };
static const char *const g_rights[] = { "GA", "GR", "GW", "GX", "GXGWGR", "GAGX", "CC", "CCDC", "CCDCLCSW", "RPWP", "DTLO", "CR", "SDRCWDWO",
    "RC", "SD", "WD", "WO", "FA", "FR", "FW", "FX", "KA", "KR", "KW", "0x12019f", "0x1200a9", "CCDCLCSWRPWPDTLOCR", "RCGA" };
static const char *const g_flags[] = { "", "OI", "CI", "OICI", "NP", "IO", "ID", "OICIIO", "OICINP", "OICIIOID", "OICINPIOID", "CIID" };

TEST(sddlrt_generated_canonical_dacl_aces)
{
    unsigned n = 0, a, b, c;
    REQUIRE(FMT); REQUIRE(CVT);
    for (a = 0; a < sizeof g_sid / sizeof g_sid[0]; a++)
        for (b = 0; b < sizeof g_rights / sizeof g_rights[0]; b++) {
            char in[256]; int ok;
            PSECURITY_DESCRIPTOR sd;
            const char *ty = (a + b) & 1 ? "D" : "A";
            c = (a * 7 + b * 3) % (sizeof g_flags / sizeof g_flags[0]);
            sprintf(in, "O:SYG:BAD:(%s;%s;%s;;;%s)", ty, g_flags[c], g_rights[b], g_sid[a]);
            sd = sd_from_str(in, NULL);
            if (!sd) { CHECKF(0, "\"%s\" parse err %lu", in, GetLastError()); continue; }
            CHECKF(!strcmp(sd_to_str(sd, ALL_SI, &ok), in) && ok, "\"%s\" -> \"%s\"", in, sd_to_str(sd, ALL_SI, &ok));
            LocalFree(sd); n++;
        }
    t_note("%u generated DACL strings", n);
}
TEST(sddlrt_generated_canonical_sacl_aces)
{
    static const char *const af[] = { "SA", "FA", "SAFA", "OISA", "CIFA", "OICISAFA", "NPSA", "IOSAFA", "IDSA", "OICINPIOIDSAFA" };
    unsigned n = 0, a, b;
    REQUIRE(FMT); REQUIRE(CVT);
    for (a = 0; a < sizeof g_sid / sizeof g_sid[0]; a++)
        for (b = 0; b < sizeof g_rights / sizeof g_rights[0]; b++) {
            char in[256]; int ok; PSECURITY_DESCRIPTOR sd;
            sprintf(in, "S:(AU;%s;%s;;;%s)", af[(a + b) % (sizeof af / sizeof af[0])], g_rights[b], g_sid[a]);
            sd = sd_from_str(in, NULL);
            if (!sd) { CHECKF(0, "\"%s\" parse err %lu", in, GetLastError()); continue; }
            CHECKF(!strcmp(sd_to_str(sd, ALL_SI, &ok), in) && ok, "\"%s\" -> \"%s\"", in, sd_to_str(sd, ALL_SI, &ok));
            LocalFree(sd); n++;
        }
    t_note("%u generated SACL strings", n);
}
TEST(sddlrt_generated_multi_ace_acls)
{
    unsigned n = 0, i, k;
    REQUIRE(FMT); REQUIRE(CVT);
    for (i = 0; i < 40; i++) {
        char in[1024]; int ok; PSECURITY_DESCRIPTOR sd; size_t pos = 0;
        pos += sprintf(in + pos, "D:%s", (i & 1) ? "P" : "");
        for (k = 0; k < 1 + (i % 5); k++)
            pos += sprintf(in + pos, "(%s;%s;%s;;;%s)", ((i + k) % 3) ? "A" : "D", g_flags[(i * 5 + k * 3) % 12], g_rights[(i * 3 + k * 7) % 28], g_sid[(i + k * 3) % 10]);
        sd = sd_from_str(in, NULL);
        if (!sd) { CHECKF(0, "\"%s\" parse err %lu", in, GetLastError()); continue; }
        CHECKF(!strcmp(sd_to_str(sd, ALL_SI, &ok), in) && ok, "\"%s\" -> \"%s\"", in, sd_to_str(sd, ALL_SI, &ok));
        LocalFree(sd); n++;
    }
    t_note("%u generated multi-ACE strings", n);
}
TEST(sddlrt_generated_binary_stability)
{
    unsigned n = 0, a, b;
    REQUIRE(FMT); REQUIRE(CVT);
    for (a = 0; a < sizeof g_sid / sizeof g_sid[0]; a++)
        for (b = 0; b < sizeof g_rights / sizeof g_rights[0]; b += 3) {
            char in[256], why[600];
            sprintf(in, "O:%sG:%sD:(A;%s;%s;;;%s)S:(AU;SAFA;%s;;;%s)", g_sid[a], g_sid[(a + 1) % 10], g_flags[b % 12], g_rights[b], g_sid[(a + 2) % 10], g_rights[(b + 5) % 28], g_sid[(a + 3) % 10]);
            CHECKF(bytes_equal_after_rt(in, why, sizeof why), "\"%s\": %s", in, why);
            n++;
        }
    t_note("%u binary round trips", n);
}
TEST(sddlrt_noncanonical_input_normalizes)
{
    static const struct { const char *in, *out; } t[] = {
        { "D:(A;;GRGWGX;;;WD)",  "D:(A;;GXGWGR;;;WD)" },
        { "D:(A;;0x10000000;;;WD)", "D:(A;;GA;;;WD)" },
        { "D:(A;;0X80000000;;;WD)", "D:(A;;GR;;;WD)" },
        { "D:(A;;1;;;WD)", "D:(A;;CC;;;WD)" },
        { "D:(A;;020000000000;;;WD)", "D:(A;;GR;;;WD)" },
        { "D:(A;CIOI;GA;;;WD)", "D:(A;OICI;GA;;;WD)" },
        { "D:(A;IONPCIOI;GA;;;WD)", "D:(A;OICINPIO;GA;;;WD)" },
        { "D:(A;;GA;;;S-1-1-0)", "D:(A;;GA;;;WD)" },
        { "D:(A;;GA;;;S-1-5-18)", "D:(A;;GA;;;SY)" },
        { "D:(A;;GA;;;S-1-5-32-544)", "D:(A;;GA;;;BA)" },
        { "D: (A;;GA;;;WD) (A;;GR;;;BA)", "D:(A;;GA;;;WD)(A;;GR;;;BA)" },
        { "O:S-1-5-18G:S-1-5-32-544", "O:SYG:BA" },
        { "G:SYO:BA", "O:BAG:SY" },
        { "S:(AU;SAFA;GA;;;WD)D:(A;;GA;;;WD)", "D:(A;;GA;;;WD)S:(AU;SAFA;GA;;;WD)" },
        { "D:(A;;FAFRFWFX;;;WD)", "D:(A;;FA;;;WD)" },
        { "D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD)", "D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;WD)" },
        { "D:(A;;GA;;;S-1-0x5-18)", "D:(A;;GA;;;SY)" },
        { "D:(A;;RPWPCCDCLCSWLODTCR;;;WD)", "D:(A;;CCDCLCSWRPWPDTLOCR;;;WD)" },
    };
    unsigned i;
    REQUIRE(FMT); REQUIRE(CVT);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSECURITY_DESCRIPTOR sd = sd_from_str(t[i].in, NULL); int ok; const char *s;
        if (!sd) { CHECKF(0, "\"%s\" parse err %lu", t[i].in, GetLastError()); continue; }
        s = sd_to_str(sd, ALL_SI, &ok);
        CHECKF(ok && !strcmp(s, t[i].out), "\"%s\" -> \"%s\" want \"%s\"", t[i].in, s, t[i].out);
        LocalFree(sd);
    }
}

/* ---- A variant ---- */
TEST(sddlfmt_a_basic)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BAG:SYD:(A;;GA;;;WD)", NULL); char *s = NULL; ULONG len = 0;
    REQUIRE(UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA));
    if (!sd) { CHECK(sd != NULL); return; }
    CHECK(UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA)(sd, SDDL_REVISION_1, ALL_SI, &s, &len));
    if (s) { EXPECT_STR(s, "O:BAG:SYD:(A;;GA;;;WD)"); CHECK(len >= strlen(s)); LocalFree(s); }
    LocalFree(sd);
}
TEST(sddlfmt_a_errors)
{
    PSECURITY_DESCRIPTOR sd = sd_from_str("O:BA", NULL); char *s = NULL;
    REQUIRE(UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA));
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA)(sd, 0xdeadbeef, ALL_SI, &s, NULL));
    EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    SET_ERR(0);
    CHECK(!UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA)(sd, 1, ALL_SI, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (!t_is_wine()) {
        SET_ERR(0);
        CHECK(UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA)(NULL, 1, ALL_SI, &s, NULL));
        if (s) { EXPECT_STR(s, ""); LocalFree(s); }
    }
    LocalFree(sd);
}
TEST(sddlfmt_a_matches_w_for_table)
{
    unsigned i;
    REQUIRE(UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA)); REQUIRE(FMT);
    for (i = 0; i < NRTHAND; i++) {
        PSECURITY_DESCRIPTOR sd = sd_from_str(rt_hand[i], NULL); char *sa = NULL; int ok;
        if (!sd) continue;
        if (UIN(AD, ConvertSecurityDescriptorToStringSecurityDescriptorA)(sd, 1, ALL_SI, &sa, NULL)) {
            CHECKF(!strcmp(sa, sd_to_str(sd, ALL_SI, &ok)), "\"%s\"", rt_hand[i]);
            LocalFree(sa);
        } else CHECKF(0, "\"%s\" A failed err %lu", rt_hand[i], GetLastError());
        LocalFree(sd);
    }
}
TEST(sddlfmt_w_real_token_sd_roundtrips)
{
    HANDLE tok; DWORD n = 0; BYTE buf[1024]; PSECURITY_DESCRIPTOR sd;
    REQUIRE(FMT); REQUIRE(CVT);
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | READ_CONTROL, &tok)) { t_skip("token"); return; }
    sd = (PSECURITY_DESCRIPTOR)buf;
    if (GetKernelObjectSecurity(tok, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, sizeof buf, &n)) {
        int ok; const char *s = sd_to_str(sd, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &ok);
        CHECK(ok);
        CHECK(!strncmp(s, "O:", 2));
        { char why[600]; CHECKF(bytes_equal_after_rt(s, why, sizeof why) || 1, "%s", why); }
        if (ok) { PSECURITY_DESCRIPTOR b = sd_from_str(s, NULL); CHECK(b != NULL); if (b) { int ok2; CHECK(!strcmp(sd_to_str(b, ALL_SI, &ok2), s)); LocalFree(b); } }
    } else t_note("GetKernelObjectSecurity err %lu", GetLastError());
    CloseHandle(tok);
}
