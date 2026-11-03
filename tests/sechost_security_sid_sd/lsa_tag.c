#include "ssh.h"
/* LsaOpenPolicy / LsaLookupNames2 / LsaLookupSids2 / LsaFreeMemory / LsaClose (sechost RPC wrappers), the sechost-internal
 * LsaLookupOpenLocalPolicy/LsaLookupClose pair, and I_QueryTagInformation parameter validation (decomp: the validation
 * happens before the SCM RPC and returns the Win32 code directly as the function result). */

#ifndef STATUS_NONE_MAPPED
#define STATUS_NONE_MAPPED ((NTSTATUS)0xC0000073)
#endif
#ifndef STATUS_SOME_NOT_MAPPED
#define STATUS_SOME_NOT_MAPPED ((NTSTATUS)0x00000107)
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#endif

#define LOPEN UIN(SH, LsaOpenPolicy)
#define LCLOSE UIN(SH, LsaClose)
#define LFREE UIN(SH, LsaFreeMemory)
#define LNAMES UIN(SH, LsaLookupNames2)
#define LSIDS UIN(SH, LsaLookupSids2)

#define NOWINE() do { if (t_is_wine()) { t_skip("sechost Lsa* wrappers are unimplemented stubs under wine"); return; } } while (0)
static void ustr(LSA_UNICODE_STRING *u, const WCHAR *s)
{
    u->Buffer = (PWSTR)s; u->Length = (USHORT)(wcslen(s) * 2); u->MaximumLength = u->Length;
}
static LSA_HANDLE open_lookup(NTSTATUS *st)
{
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = NULL;
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    *st = LOPEN(NULL, &oa, POLICY_LOOKUP_NAMES, &h);
    return h;
}

TEST(lsa_open_close_policy)
{
    NTSTATUS st; LSA_HANDLE h;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LCLOSE);
    h = open_lookup(&st);
    EXPECT_EQ(st, 0); CHECK(h != NULL);
    if (h) EXPECT_EQ(LCLOSE(h), 0);
}
TEST(lsa_open_policy_multiple_handles_distinct)
{
    NTSTATUS s1, s2; LSA_HANDLE a, b;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LCLOSE);
    a = open_lookup(&s1); b = open_lookup(&s2);
    EXPECT_EQ(s1, 0); EXPECT_EQ(s2, 0); CHECK(a != b);
    if (b) LCLOSE(b);
    if (a) LCLOSE(a);
}
TEST(lsa_open_policy_view_local_information)
{
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = NULL; NTSTATUS st;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LCLOSE);
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    st = LOPEN(NULL, &oa, POLICY_VIEW_LOCAL_INFORMATION | POLICY_LOOKUP_NAMES, &h);
    CHECKF(st == 0 || st == (NTSTATUS)0xC0000022, "status 0x%lx", st);
    if (st == 0) LCLOSE(h);
}
TEST(lsa_open_policy_systemname_empty_or_null)
{
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = NULL; NTSTATUS st; LSA_UNICODE_STRING sn;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LCLOSE);
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    memset(&sn, 0, sizeof sn);
    st = LOPEN(&sn, &oa, POLICY_LOOKUP_NAMES, &h);
    t_note("empty SystemName -> 0x%lx", st);
    if (st == 0) LCLOSE(h);
}
TEST(lsa_close_returns_success_and_invalidates)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_SID2 s = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LCLOSE); REQUIRE(LNAMES);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    EXPECT_EQ(LCLOSE(h), 0);
    ustr(&n, L"Administrators");
    if (t_is_wine()) return;
    st = LNAMES(h, 0, 1, &n, &d, &s);
    t_note("lookup on closed handle -> 0x%lx", st);
    CHECK(st != 0 && st != STATUS_SOME_NOT_MAPPED);
}
TEST(lsa_lookup_names2_wellknown)
{
    static const struct { const WCHAR *name; SID_NAME_USE use; const char *dom; } t[] = {
        { L"Administrators", SidTypeAlias, "BUILTIN" }, { L"BUILTIN\\Administrators", SidTypeAlias, "BUILTIN" },
        { L"SYSTEM", SidTypeWellKnownGroup, "NT AUTHORITY" }, { L"NT AUTHORITY\\SYSTEM", SidTypeWellKnownGroup, "NT AUTHORITY" },
        { L"LOCAL SERVICE", SidTypeWellKnownGroup, "NT AUTHORITY" }, { L"NT AUTHORITY\\LocalService", SidTypeWellKnownGroup, "NT AUTHORITY" },
        { L"Users", SidTypeAlias, "BUILTIN" },
    };
    NTSTATUS st; LSA_HANDLE h; unsigned i;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LFREE);
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_SID2 s = NULL; char dn[128];
        ustr(&n, t[i].name);
        st = LNAMES(h, 0, 1, &n, &d, &s);
        CHECKF(st == 0, "%ls: status 0x%lx", t[i].name, st);
        if (st == 0 && s && d) {
            EXPECT_EQ(s[0].Use, t[i].use); EXPECT_EQ(s[0].Flags, 0); EXPECT_EQ(d->Entries, 1); EXPECT_EQ(s[0].DomainIndex, 0);
            CHECK(s[0].Sid != NULL && IsValidSid(s[0].Sid));
            if (d->Domains) { size_t k; for (k = 0; k < d->Domains[0].Name.Length / 2; k++) dn[k] = (char)d->Domains[0].Name.Buffer[k]; dn[k] = 0; CHECKF(!strcmp(dn, t[i].dom), "%ls: domain %s", t[i].name, dn); }
        }
        if (s) LFREE(s); if (d) LFREE(d);
    }
    LCLOSE(h);
}
TEST(lsa_lookup_names2_none_mapped)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = (void *)1; PLSA_TRANSLATED_SID2 s = (void *)1;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    ustr(&n, L"olet_no_such_account_xyz");
    st = LNAMES(h, 0, 1, &n, &d, &s);
    EXPECT_EQ(st, STATUS_NONE_MAPPED);
    CHECK(s != NULL && s != (void *)1);
    if (s && s != (void *)1) { EXPECT_EQ(s[0].Use, SidTypeUnknown); EXPECT_EQ(s[0].Flags, 0); }
    if (d && d != (void *)1) EXPECT_EQ(d->Entries, 0);
    if (s && s != (void *)1) LFREE(s);
    if (d && d != (void *)1) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_lookup_names2_some_not_mapped)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n[3]; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_SID2 s = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LFREE);
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    ustr(&n[0], L"LOCAL SERVICE"); ustr(&n[1], L"olet_no_such_account_xyz"); ustr(&n[2], L"NT AUTHORITY\\LocalService");
    st = LNAMES(h, 0, 3, n, &d, &s);
    EXPECT_EQ(st, STATUS_SOME_NOT_MAPPED);
    if (s) {
        EXPECT_EQ(s[0].Use, SidTypeWellKnownGroup); EXPECT_EQ(s[1].Use, SidTypeUnknown); EXPECT_EQ(s[2].Use, SidTypeWellKnownGroup);
        EXPECT_EQ(s[0].DomainIndex, 0); EXPECT_EQ(s[2].DomainIndex, 0);
        if (s[0].Sid && s[2].Sid) CHECK(EqualSid(s[0].Sid, s[2].Sid));
        LFREE(s);
    }
    if (d) { EXPECT_EQ(d->Entries, 1); LFREE(d); }
    LCLOSE(h);
}
TEST(lsa_lookup_names2_case_insensitive)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_SID2 s = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LFREE);
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    ustr(&n, L"local service");
    st = LNAMES(h, 0, 1, &n, &d, &s);
    EXPECT_EQ(st, 0);
    if (s) LFREE(s); if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_lookup_names2_length_is_bytes_not_nul_terminated)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_SID2 s = NULL;
    static const WCHAR buf[] = L"Administrators-and-trailing-garbage";
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    n.Buffer = (PWSTR)buf; n.Length = 14 * 2; n.MaximumLength = n.Length;     /* "Administrators" */
    st = LNAMES(h, 0, 1, &n, &d, &s);
    EXPECT_EQ(st, 0);
    if (s) { EXPECT_EQ(s[0].Use, SidTypeAlias); LFREE(s); }
    if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_lookup_names2_user_account_roundtrip)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_SID2 s = NULL; WCHAR un[128]; DWORD len = 128;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    GetUserNameW(un, &len);
    ustr(&n, un);
    st = LNAMES(h, 0, 1, &n, &d, &s);
    t_note("lookup of user %ls -> 0x%lx", un, st);
    if (st == 0 && s) { CHECK(s[0].Use == SidTypeUser || s[0].Use == SidTypeWellKnownGroup); LFREE(s); if (d) LFREE(d); }
    LCLOSE(h);
}
TEST(lsa_lookup_sids2_wellknown)
{
    NTSTATUS st; LSA_HANDLE h; SIDBUF a, b, c; PSID sids[3]; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_NAME n = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LSIDS); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    sids[0] = mk_sid(&a, 5, 1, 18); sids[1] = mk_sid(&b, 5, 2, 32, 544); sids[2] = mk_sid(&c, 1, 1, 0);
    st = LSIDS(h, 0, 3, sids, &d, &n);
    EXPECT_EQ(st, 0);
    if (st == 0 && n && d) {
        EXPECT_EQ(n[0].Use, SidTypeWellKnownGroup); EXPECT_EQ(n[1].Use, SidTypeAlias); EXPECT_EQ(n[2].Use, SidTypeWellKnownGroup);
        CHECK(n[0].DomainIndex != n[1].DomainIndex);
        CHECK(d->Entries >= 2);
        if (is_english_ui()) {
            CHECK(n[0].Name.Length == 12 && !memcmp(n[0].Name.Buffer, L"SYSTEM", 12));
            CHECK(n[1].Name.Length == 28 && !memcmp(n[1].Name.Buffer, L"Administrators", 28));
            CHECK(n[2].Name.Length == 16 && !memcmp(n[2].Name.Buffer, L"Everyone", 16));
        }
    }
    if (n) LFREE(n); if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_lookup_sids2_none_mapped)
{
    NTSTATUS st; LSA_HANDLE h; SIDBUF a; PSID sids[1]; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_NAME n = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LSIDS); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    sids[0] = mk_sid(&a, 5, 5, 21, 111, 222, 333, 444);
    st = LSIDS(h, 0, 1, sids, &d, &n);
    EXPECT_EQ(st, STATUS_NONE_MAPPED);
    if (n) { EXPECT_EQ(n[0].Use, SidTypeUnknown); LFREE(n); }
    if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_lookup_sids2_some_not_mapped)
{
    NTSTATUS st; LSA_HANDLE h; SIDBUF a, b; PSID sids[2]; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_NAME n = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LSIDS); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    sids[0] = mk_sid(&a, 5, 1, 18); sids[1] = mk_sid(&b, 5, 5, 21, 111, 222, 333, 444);
    st = LSIDS(h, 0, 2, sids, &d, &n);
    EXPECT_EQ(st, STATUS_SOME_NOT_MAPPED);
    if (n) { EXPECT_EQ(n[0].Use, SidTypeWellKnownGroup); EXPECT_EQ(n[1].Use, SidTypeUnknown); LFREE(n); }
    if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_lookup_sids2_label_sid)
{
    NTSTATUS st; LSA_HANDLE h; SIDBUF a; PSID sids[1]; PLSA_REFERENCED_DOMAIN_LIST d = NULL; PLSA_TRANSLATED_NAME n = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LSIDS); REQUIRE(LFREE);
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    sids[0] = mk_sid(&a, 16, 1, 12288);
    st = LSIDS(h, 0, 1, sids, &d, &n);
    EXPECT_EQ(st, 0);
    if (n) { EXPECT_EQ(n[0].Use, SidTypeLabel); LFREE(n); }
    if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_names_sids_roundtrip)
{
    NTSTATUS st; LSA_HANDLE h; LSA_UNICODE_STRING n; PLSA_REFERENCED_DOMAIN_LIST d = NULL, d2 = NULL; PLSA_TRANSLATED_SID2 s = NULL; PLSA_TRANSLATED_NAME nm = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(LNAMES); REQUIRE(LSIDS); REQUIRE(LFREE);
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    h = open_lookup(&st);
    if (!h) { t_skip("no policy"); return; }
    ustr(&n, L"BUILTIN\\Users");
    st = LNAMES(h, 0, 1, &n, &d, &s);
    EXPECT_EQ(st, 0);
    if (st == 0 && s) {
        PSID one[1]; one[0] = s[0].Sid;
        st = LSIDS(h, 0, 1, one, &d2, &nm);
        EXPECT_EQ(st, 0);
        if (nm) { EXPECT_EQ(nm[0].Use, SidTypeAlias); CHECK(nm[0].Name.Length == 10 && !memcmp(nm[0].Name.Buffer, L"Users", 10)); LFREE(nm); }
        if (d2) LFREE(d2);
        LFREE(s);
    }
    if (d) LFREE(d);
    LCLOSE(h);
}
TEST(lsa_query_account_domain)
{
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = NULL; NTSTATUS st; PVOID info = NULL;
    REQUIRE(LOPEN); NOWINE(); REQUIRE(UIN(SH, LsaQueryInformationPolicy)); REQUIRE(LFREE);
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    st = LOPEN(NULL, &oa, POLICY_VIEW_LOCAL_INFORMATION, &h);
    if (st != 0) { t_skip("cannot open policy: 0x%lx", st); return; }
    st = UIN(SH, LsaQueryInformationPolicy)(h, PolicyAccountDomainInformation, &info);
    EXPECT_EQ(st, 0);
    if (st == 0 && info) {
        POLICY_ACCOUNT_DOMAIN_INFO *ad = info; WCHAR cn[64]; DWORD n = 64;
        CHECK(ad->DomainSid != NULL && IsValidSid(ad->DomainSid));
        if (ad->DomainSid) { CHECK(*GetSidSubAuthorityCount(ad->DomainSid) == 4); CHECK(*GetSidSubAuthority(ad->DomainSid, 0) == 21); }
        GetComputerNameW(cn, &n);
        if (ad->DomainName.Length / 2 == n && !_wcsnicmp(ad->DomainName.Buffer, cn, n)) CHECK(1);
        else t_note("account domain name differs from computer name (domain controller?)");
        LFREE(info);
    }
    LCLOSE(h);
}
TEST(lsa_free_memory_null)
{
    REQUIRE(LFREE);
    if (t_is_wine()) { t_skip("wine"); return; }
    t_note("LsaFreeMemory(NULL) -> 0x%lx", LFREE(NULL));
}
TEST(lsa_lookup_open_local_policy_pair)
{
    typedef NTSTATUS (WINAPI *open_fn)(PLSA_OBJECT_ATTRIBUTES, ACCESS_MASK, PLSA_HANDLE);
    typedef NTSTATUS (WINAPI *close_fn)(LSA_HANDLE);
    open_fn op = (open_fn)t_proc_in(SH, "LsaLookupOpenLocalPolicy"); close_fn cl = (close_fn)t_proc_in(SH, "LsaLookupClose");
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = NULL; NTSTATUS st; SECURITY_QUALITY_OF_SERVICE q;
    if (!op || !cl) { t_skip("missing export"); return; }
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    memset(&q, 0, sizeof q); q.Length = sizeof q; q.ImpersonationLevel = SecurityIdentification; q.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    oa.SecurityQualityOfService = &q;
    st = op(&oa, POLICY_LOOKUP_NAMES, &h);
    EXPECT_EQ(st, 0); CHECK(h != NULL);
    if (st == 0) EXPECT_EQ(cl(h), 0);
}

/* ---- I_QueryTagInformation ---- */
UDECL(ULONG, I_QueryTagInformation, (PCWSTR machine, ULONG level, PVOID info));
typedef struct { DWORD pid, tag; PWSTR name; ULONG type; } TAG_NAME_FROM_TAG;      /* level 1 */
typedef struct { DWORD pid; PWSTR module; PVOID unused; ULONG cnt; PVOID out; } TAG_NAMES_MODULE;

TEST(querytag_invalid_levels)
{
    BYTE buf[64] = { 0 }; ULONG lvl;
    REQUIRE(UN(I_QueryTagInformation)); NOWINE();
    for (lvl = 0; lvl <= 9; lvl++) {
        ULONG r;
        if (lvl >= 1 && lvl <= 3) continue;
        r = UN(I_QueryTagInformation)(NULL, lvl, buf);
        CHECKF(r == ERROR_INVALID_PARAMETER, "level %lu -> %lu", lvl, r);
        r = UN(I_QueryTagInformation)(NULL, lvl, NULL);
        CHECKF(r == ERROR_INVALID_PARAMETER, "level %lu NULL -> %lu", lvl, r);
    }
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 0xffffffff, buf), ERROR_INVALID_PARAMETER);
}
TEST(querytag_level1_validation)
{
    TAG_NAME_FROM_TAG t;
    REQUIRE(UN(I_QueryTagInformation)); NOWINE();
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 1, NULL), ERROR_INVALID_PARAMETER);
    memset(&t, 0, sizeof t);
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 1, &t), ERROR_INVALID_PARAMETER);        /* pid == 0 */
    t.pid = GetCurrentProcessId(); t.tag = 0;
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 1, &t), ERROR_INVALID_PARAMETER);        /* tag == 0 */
    t.pid = 0; t.tag = 1;
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 1, &t), ERROR_INVALID_PARAMETER);        /* pid == 0 */
    t.pid = GetCurrentProcessId(); t.tag = 1; t.type = 5;                                 /* out field not zero */
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 1, &t), ERROR_INVALID_PARAMETER);
}
TEST(querytag_level2_validation)
{
    TAG_NAMES_MODULE t;
    REQUIRE(UN(I_QueryTagInformation)); NOWINE();
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 2, NULL), ERROR_INVALID_PARAMETER);
    memset(&t, 0, sizeof t);
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 2, &t), ERROR_INVALID_PARAMETER);       /* pid == 0 */
    t.pid = GetCurrentProcessId();
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 2, &t), ERROR_INVALID_PARAMETER);       /* module NULL */
    t.module = (PWSTR)L"kernel32.dll"; t.out = (PVOID)1;
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 2, &t), ERROR_INVALID_PARAMETER);       /* out ptr must be zero */
}
TEST(querytag_level3_validation)
{
    struct { PVOID in; PVOID out; ULONG x; } t;
    REQUIRE(UN(I_QueryTagInformation)); NOWINE();
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 3, NULL), ERROR_INVALID_PARAMETER);
    memset(&t, 0, sizeof t); t.in = NULL; t.out = (PVOID)1;
    EXPECT_EQ(UN(I_QueryTagInformation)(NULL, 3, &t), ERROR_INVALID_PARAMETER);       /* output pointer must be zero */
}
TEST(querytag_level1_valid_looking_request_reaches_scm)
{
    TAG_NAME_FROM_TAG t; ULONG r;
    REQUIRE(UN(I_QueryTagInformation)); NOWINE();
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    memset(&t, 0, sizeof t); t.pid = GetCurrentProcessId(); t.tag = 0x7777;
    r = UN(I_QueryTagInformation)(NULL, 1, &t);
    t_note("level 1, own pid, tag 0x7777 -> %lu (name %ls)", r, t.name ? t.name : L"(null)");
    CHECK(r != ERROR_INVALID_PARAMETER || t.name == NULL);
    if (r == 0 && t.name) LocalFree(t.name);
}
