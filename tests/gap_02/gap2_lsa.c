/* gap_02: sechost LSA wrappers LsaLookupSids, LsaAddAccountRights, LsaEnumerateAccountRights,
 * LsaEnumerateAccountsWithUserRight -- everything that does not modify the LSA database (see tests/dx_gap_02 for the rest).
 *
 * Win10 decomp (sechost.dll):
 *   LsaAddAccountRights / LsaEnumerateAccountRights / LsaEnumerateAccountsWithUserRight:
 *       PolicyHandle == NULL -> STATUS_INVALID_HANDLE (0xC0000008) before anything else; no output is written in that case.
 *       Otherwise one NdrClientCall3 to the LSA RPC server; the Enumerate* wrappers zero their outputs only after the call.
 *   LsaLookupSids -> LsapLookupSids(h, flags 0, ...):
 *       Names == NULL -> STATUS_INVALID_PARAMETER (0xC000000D), checked first;
 *       then LsaICLookupSids: handle NULL -> STATUS_INVALID_HANDLE; Count == 0 -> STATUS_INVALID_PARAMETER;
 *       Count > 0x5000 -> 0xC000017E (STATUS_TOO_MANY_...);  these paths leave *ReferencedDomains alone and set *Names = NULL.
 *   The returned names block is one LocalAlloc: LSA_TRANSLATED_NAME[Count] followed by the name strings; free with LsaFreeMemory. */
#include "t.h"
#include <ntsecapi.h>

#define SH "sechost.dll"
#define ST_INVALID_HANDLE    ((NTSTATUS)0xC0000008)
#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define ST_TOO_MANY          ((NTSTATUS)0xC000017E)
#define ST_NONE_MAPPED       ((NTSTATUS)0xC0000073)
#define ST_SOME_NOT_MAPPED   ((NTSTATUS)0x00000107)
#define ST_NO_MORE_ENTRIES   ((NTSTATUS)0x8000001A)

#define LSIDS  UIN(SH, LsaLookupSids)
#define LADD   UIN(SH, LsaAddAccountRights)
#define LENUMR UIN(SH, LsaEnumerateAccountRights)
#define LENUMA UIN(SH, LsaEnumerateAccountsWithUserRight)
#define LOPEN  UIN(SH, LsaOpenPolicy)
#define LCLOSE UIN(SH, LsaClose)
#define LFREE  UIN(SH, LsaFreeMemory)
#define NOWINE() do { if (t_is_wine()) { t_skip("sechost Lsa* wrappers are unimplemented stubs under wine"); return; } } while (0)

typedef struct { BYTE b[SECURITY_MAX_SID_SIZE]; } sidbuf;
static PSID mksid(sidbuf *s, BYTE auth5, int n, DWORD a0, DWORD a1)
{
    PISID p = (PISID)s->b;
    memset(s, 0, sizeof *s);
    p->Revision = 1; p->SubAuthorityCount = (BYTE)n;
    p->IdentifierAuthority.Value[5] = auth5;
    p->SubAuthority[0] = a0; if (n > 1) p->SubAuthority[1] = a1;
    return p;
}
static void ustr(LSA_UNICODE_STRING *u, const WCHAR *s) { u->Buffer = (PWSTR)s; u->Length = (USHORT)(wcslen(s) * 2); u->MaximumLength = u->Length + 2; }
static LSA_HANDLE open_policy(NTSTATUS *st, ACCESS_MASK am)
{
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = NULL;
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    *st = LOPEN(NULL, &oa, am, &h);
    return h;
}
static int english_ui(void) { return (PRIMARYLANGID(U(GetUserDefaultUILanguage)()) == LANG_ENGLISH); }

/* ---- NULL policy handle: exact status, outputs untouched ---- */
TEST(gap2_lsa_addrights_null_handle)
{
    sidbuf sb; PSID sid = mksid(&sb, 5, 1, 18, 0); LSA_UNICODE_STRING r;
    REQUIRE(LADD); NOWINE();
    ustr(&r, L"SeBatchLogonRight");
    EXPECT_EQ(LADD(NULL, sid, &r, 1), ST_INVALID_HANDLE);
    EXPECT_EQ(LADD(NULL, sid, &r, 0), ST_INVALID_HANDLE);
    EXPECT_EQ(LADD(NULL, NULL, NULL, 0), ST_INVALID_HANDLE);     /* the handle is checked before any argument is used */
    EXPECT_EQ(LADD(NULL, sid, NULL, 5), ST_INVALID_HANDLE);
}
TEST(gap2_lsa_enumrights_null_handle)
{
    sidbuf sb; PSID sid = mksid(&sb, 5, 1, 18, 0); PLSA_UNICODE_STRING rights = (PLSA_UNICODE_STRING)(ULONG_PTR)0x1111; ULONG cnt = 0x2222;
    REQUIRE(LENUMR); NOWINE();
    EXPECT_EQ(LENUMR(NULL, sid, &rights, &cnt), ST_INVALID_HANDLE);
    EXPECT_EQ(rights, (PLSA_UNICODE_STRING)(ULONG_PTR)0x1111);    /* outputs are only zeroed after the RPC */
    EXPECT_EQ(cnt, 0x2222);
    EXPECT_EQ(LENUMR(NULL, NULL, &rights, &cnt), ST_INVALID_HANDLE);
}
TEST(gap2_lsa_enumaccounts_null_handle)
{
    LSA_UNICODE_STRING r; PVOID buf = (PVOID)(ULONG_PTR)0x3333; ULONG cnt = 0x4444;
    REQUIRE(LENUMA); NOWINE();
    ustr(&r, L"SeInteractiveLogonRight");
    EXPECT_EQ(LENUMA(NULL, &r, &buf, &cnt), ST_INVALID_HANDLE);
    EXPECT_EQ(buf, (PVOID)(ULONG_PTR)0x3333);
    EXPECT_EQ(cnt, 0x4444);
    EXPECT_EQ(LENUMA(NULL, NULL, &buf, &cnt), ST_INVALID_HANDLE);
}
TEST(gap2_lsa_lookupsids_names_null)
{
    sidbuf sb; PSID sid = mksid(&sb, 1, 1, 0, 0); PLSA_REFERENCED_DOMAIN_LIST dom = (void *)(ULONG_PTR)0x5555;
    NTSTATUS st; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(LOPEN); REQUIRE(LCLOSE); NOWINE();
    /* Names == NULL wins over everything, even a NULL handle */
    EXPECT_EQ(LSIDS(NULL, 1, &sid, &dom, NULL), ST_INVALID_PARAMETER);
    EXPECT_EQ(dom, (void *)(ULONG_PTR)0x5555);
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    EXPECT_EQ(LSIDS(h, 1, &sid, &dom, NULL), ST_INVALID_PARAMETER);
    LCLOSE(h);
}
TEST(gap2_lsa_lookupsids_null_policy_handle)
{
    sidbuf sb; PSID sid = mksid(&sb, 1, 1, 0, 0); PLSA_REFERENCED_DOMAIN_LIST dom = (void *)(ULONG_PTR)0x5555; PLSA_TRANSLATED_NAME names = (void *)(ULONG_PTR)0x6666;
    REQUIRE(LSIDS); NOWINE();
    EXPECT_EQ(LSIDS(NULL, 1, &sid, &dom, &names), ST_INVALID_HANDLE);
    CHECK(names == NULL);                                         /* LsapLookupSids zeroes *Names when nothing came back */
    EXPECT_EQ(dom, (void *)(ULONG_PTR)0x5555);                    /* ... and never touches *ReferencedDomains on this path */
}
TEST(gap2_lsa_lookupsids_count_validation)
{
    sidbuf sb; PSID sid = mksid(&sb, 1, 1, 0, 0); PLSA_REFERENCED_DOMAIN_LIST dom = (void *)(ULONG_PTR)0x5555; PLSA_TRANSLATED_NAME names;
    NTSTATUS st; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(LOPEN); REQUIRE(LCLOSE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    names = (void *)(ULONG_PTR)0x6666;
    EXPECT_EQ(LSIDS(h, 0, &sid, &dom, &names), ST_INVALID_PARAMETER);
    CHECK(names == NULL);
    EXPECT_EQ(dom, (void *)(ULONG_PTR)0x5555);
    names = (void *)(ULONG_PTR)0x6666;
    EXPECT_EQ(LSIDS(h, 0x5001, &sid, &dom, &names), ST_TOO_MANY);   /* limit is 0x5000 SIDs per call; checked before any SID is read */
    CHECK(names == NULL);
    EXPECT_EQ(LSIDS(h, 0xFFFFFFFF, &sid, &dom, &names), ST_TOO_MANY);
    LCLOSE(h);
}
TEST(gap2_lsa_lookupsids_closed_handle)
{
    sidbuf sb; PSID sid = mksid(&sb, 1, 1, 0, 0); PLSA_REFERENCED_DOMAIN_LIST dom = NULL; PLSA_TRANSLATED_NAME names = NULL;
    NTSTATUS st, r; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(LOPEN); REQUIRE(LCLOSE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    r = LCLOSE(h); EXPECT_EQ(r, 0);        /* EXPECT_EQ evaluates its operands twice: never put a side-effecting call inside */
    r = LSIDS(h, 1, &sid, &dom, &names);
    t_note("LsaLookupSids on a closed handle -> 0x%lx", r);          /* the handle block is freed; the outcome is a use-after-close, only record it */
}

/* ---- valid lookups ---- */
typedef struct { const char *what; BYTE auth; int n; DWORD a0, a1; SID_NAME_USE use; const WCHAR *name; const WCHAR *dom; } wk;
static const wk wks[] = {
    { "Everyone S-1-1-0", 1, 1, 0, 0, SidTypeWellKnownGroup, L"Everyone", L"" },
    { "SYSTEM S-1-5-18", 5, 1, 18, 0, SidTypeWellKnownGroup, L"SYSTEM", L"NT AUTHORITY" },
    { "LOCAL SERVICE S-1-5-19", 5, 1, 19, 0, SidTypeWellKnownGroup, L"LOCAL SERVICE", L"NT AUTHORITY" },
    { "NETWORK SERVICE S-1-5-20", 5, 1, 20, 0, SidTypeWellKnownGroup, L"NETWORK SERVICE", L"NT AUTHORITY" },
    { "Authenticated Users S-1-5-11", 5, 1, 11, 0, SidTypeWellKnownGroup, L"Authenticated Users", L"NT AUTHORITY" },
    { "INTERACTIVE S-1-5-4", 5, 1, 4, 0, SidTypeWellKnownGroup, L"INTERACTIVE", L"NT AUTHORITY" },
    { "Administrators S-1-5-32-544", 5, 2, 32, 544, SidTypeAlias, L"Administrators", L"BUILTIN" },
    { "Users S-1-5-32-545", 5, 2, 32, 545, SidTypeAlias, L"Users", L"BUILTIN" },
    { "Guests S-1-5-32-546", 5, 2, 32, 546, SidTypeAlias, L"Guests", L"BUILTIN" },
};
static int sid_len(PSID p) { return 8 + 4 * ((PISID)p)->SubAuthorityCount; }
static void name_str(const LSA_UNICODE_STRING *u, WCHAR *out, size_t cap)
{
    size_t n = u->Length / 2; if (n >= cap) n = cap - 1;
    if (u->Buffer) memcpy(out, u->Buffer, n * 2); else n = 0;
    out[n] = 0;
}

TEST(gap2_lsa_lookupsids_well_known_each)
{
    unsigned i; NTSTATUS st; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    for (i = 0; i < sizeof(wks) / sizeof(wks[0]); i++) {
        sidbuf sb; PSID sid = mksid(&sb, wks[i].auth, wks[i].n, wks[i].a0, wks[i].a1), arr[1];
        PLSA_REFERENCED_DOMAIN_LIST dom = NULL; PLSA_TRANSLATED_NAME names = NULL;
        arr[0] = sid;
        st = LSIDS(h, 1, arr, &dom, &names);
        CHECKF(st == 0, "%s: status 0x%lx", wks[i].what, st);
        if (st != 0) { if (names) LFREE(names); if (dom) LFREE(dom); continue; }
        CHECK(names != NULL); CHECK(dom != NULL);
        if (names) {
            WCHAR nm[128];
            CHECKF(names[0].Use == wks[i].use, "%s: Use %d want %d", wks[i].what, names[0].Use, wks[i].use);
            name_str(&names[0].Name, nm, 128);
            if (english_ui()) CHECKF(!wcscmp(nm, wks[i].name), "%s: name '%ls'", wks[i].what, nm);
            else t_note("%s -> '%ls'", wks[i].what, nm);
            CHECK(names[0].DomainIndex >= -1);
            if (names[0].DomainIndex >= 0 && dom && (ULONG)names[0].DomainIndex < dom->Entries) {
                WCHAR dn[128];
                name_str(&dom->Domains[names[0].DomainIndex].Name, dn, 128);
                if (english_ui() && wks[i].dom[0]) CHECKF(!wcscmp(dn, wks[i].dom), "%s: domain '%ls'", wks[i].what, dn);
                else t_note("%s domain '%ls'", wks[i].what, dn);
                CHECK(dom->Domains[names[0].DomainIndex].Sid != NULL || dn[0] == 0);
            } else t_note("%s DomainIndex %ld Entries %lu", wks[i].what, names[0].DomainIndex, dom ? dom->Entries : 0);
            /* the name buffer lives inside the same LocalAlloc block, after the LSA_TRANSLATED_NAME array */
            if (names[0].Name.Length)
                CHECK((BYTE *)names[0].Name.Buffer >= (BYTE *)(names + 1));
        }
        if (names) { NTSTATUS f_ = LFREE(names); EXPECT_EQ(f_, 0); }
        if (dom) { NTSTATUS f_ = LFREE(dom); EXPECT_EQ(f_, 0); }
    }
    LCLOSE(h);
}
TEST(gap2_lsa_lookupsids_batch_and_unmapped)
{
    sidbuf s0, s1, s2; PSID arr[3]; PLSA_REFERENCED_DOMAIN_LIST dom = NULL; PLSA_TRANSLATED_NAME names = NULL; NTSTATUS st; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    arr[0] = mksid(&s0, 5, 2, 32, 544);
    {   /* S-1-5-21-1-2-3-424242: a domain no machine knows */
        PISID p = (PISID)s1.b;
        memset(&s1, 0, sizeof s1); p->Revision = 1; p->SubAuthorityCount = 5; p->IdentifierAuthority.Value[5] = 5;
        p->SubAuthority[0] = 21; p->SubAuthority[1] = 1111111; p->SubAuthority[2] = 2222222; p->SubAuthority[3] = 3333333; p->SubAuthority[4] = 424242;
        arr[1] = p;
    }
    arr[2] = mksid(&s2, 5, 1, 18, 0);
    st = LSIDS(h, 3, arr, &dom, &names);
    CHECKF(st == ST_SOME_NOT_MAPPED, "status 0x%lx", st);
    if (names) {
        EXPECT_EQ(names[0].Use, SidTypeAlias);
        EXPECT_EQ(names[1].Use, SidTypeUnknown);
        EXPECT_EQ(names[2].Use, SidTypeWellKnownGroup);
        CHECK(names[1].DomainIndex == -1 || names[1].DomainIndex >= 0);
        t_note("unmapped entry: Name.Length %u DomainIndex %ld", names[1].Name.Length, names[1].DomainIndex);
        LFREE(names);
    } else CHECK(0);
    if (dom) LFREE(dom);
    /* only the unknown SID: nothing mapped */
    names = NULL; dom = NULL;
    st = LSIDS(h, 1, &arr[1], &dom, &names);
    CHECKF(st == ST_NONE_MAPPED, "status 0x%lx", st);
    if (names) { EXPECT_EQ(names[0].Use, SidTypeUnknown); LFREE(names); }
    if (dom) LFREE(dom);
    LCLOSE(h);
}
TEST(gap2_lsa_lookupsids_duplicates_share_domain)
{
    sidbuf s0; PSID arr[2]; PLSA_REFERENCED_DOMAIN_LIST dom = NULL; PLSA_TRANSLATED_NAME names = NULL; NTSTATUS st; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    arr[0] = arr[1] = mksid(&s0, 5, 2, 32, 545);
    st = LSIDS(h, 2, arr, &dom, &names);
    CHECKF(st == 0, "status 0x%lx", st);
    if (st == 0 && names) {
        EXPECT_EQ(names[0].Use, SidTypeAlias); EXPECT_EQ(names[1].Use, SidTypeAlias);
        EXPECT_EQ(names[0].DomainIndex, names[1].DomainIndex);
        EXPECT_EQ(names[0].Name.Length, names[1].Name.Length);
        CHECK(!memcmp(names[0].Name.Buffer, names[1].Name.Buffer, names[0].Name.Length));
        CHECK(names[0].Name.Buffer != names[1].Name.Buffer);        /* each entry owns its copy of the text */
        if (dom) EXPECT_EQ(dom->Entries, 1);
    }
    if (names) LFREE(names);
    if (dom) LFREE(dom);
    LCLOSE(h);
}
TEST(gap2_lsa_lookupsids_matches_lookupsids2)
{
    sidbuf s0; PSID arr[1]; PLSA_REFERENCED_DOMAIN_LIST d1 = NULL, d2 = NULL; PLSA_TRANSLATED_NAME n1 = NULL; PLSA_TRANSLATED_NAME n2 = NULL;
    NTSTATUS st1, st2, st; LSA_HANDLE h;
    REQUIRE(LSIDS); REQUIRE(UIN(SH, LsaLookupSids2)); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    arr[0] = mksid(&s0, 5, 2, 32, 544);
    st1 = LSIDS(h, 1, arr, &d1, &n1);
    st2 = UIN(SH, LsaLookupSids2)(h, 0, 1, arr, &d2, &n2);
    EXPECT_EQ(st1, st2);
    if (n1 && n2) {
        EXPECT_EQ(n1[0].Use, n2[0].Use);
        EXPECT_EQ(n1[0].Name.Length, n2[0].Name.Length);
        CHECK(!memcmp(n1[0].Name.Buffer, n2[0].Name.Buffer, n1[0].Name.Length));
        EXPECT_EQ(n1[0].DomainIndex, n2[0].DomainIndex);
    } else CHECK(0);
    if (n1) LFREE(n1);
    if (n2) LFREE(n2);
    if (d1) LFREE(d1);
    if (d2) LFREE(d2);
    LCLOSE(h);
}

/* ---- read-only enumeration against a real policy handle ---- */
TEST(gap2_lsa_enumrights_builtin_administrators)
{
    sidbuf sb; PSID sid = mksid(&sb, 5, 2, 32, 544); PLSA_UNICODE_STRING rights = NULL; ULONG cnt = 0xFFFF; NTSTATUS st, r; LSA_HANDLE h; ULONG i;
    REQUIRE(LENUMR); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    r = LENUMR(h, sid, &rights, &cnt);
    t_note("LsaEnumerateAccountRights(BUILTIN\\Administrators) -> 0x%lx count %lu", r, cnt);
    if (r == 0) {
        CHECK(cnt > 0); CHECK(rights != NULL);
        for (i = 0; rights && i < cnt; i++) {
            CHECKF(rights[i].Length > 0 && rights[i].Buffer != NULL, "right %lu empty", i);
            CHECKF(rights[i].MaximumLength >= rights[i].Length, "right %lu max<len", i);
        }
        r = LFREE(rights); EXPECT_EQ(r, 0);
    } else {
        CHECKF(cnt == 0 && rights == NULL, "failure must zero the outputs (cnt %lu)", cnt);
    }
    LCLOSE(h);
}
TEST(gap2_lsa_enumrights_unknown_account)
{
    sidbuf sb; PISID p = (PISID)sb.b; PLSA_UNICODE_STRING rights = (void *)(ULONG_PTR)0x77; ULONG cnt = 0x88; NTSTATUS st, r; LSA_HANDLE h;
    REQUIRE(LENUMR); REQUIRE(LOPEN); REQUIRE(LCLOSE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    memset(&sb, 0, sizeof sb); p->Revision = 1; p->SubAuthorityCount = 5; p->IdentifierAuthority.Value[5] = 5;
    p->SubAuthority[0] = 21; p->SubAuthority[1] = 9100001; p->SubAuthority[2] = 9100002; p->SubAuthority[3] = 9100003; p->SubAuthority[4] = 9100004;
    r = LENUMR(h, (PSID)p, &rights, &cnt);
    CHECKF(r != 0, "an account that holds no rights must not succeed (0x%lx)", r);
    t_note("LsaEnumerateAccountRights(unknown SID) -> 0x%lx", r);
    CHECKF(rights == NULL && cnt == 0, "outputs are zeroed after the RPC even on failure (%p, %lu)", rights, cnt);
    LCLOSE(h);
}
TEST(gap2_lsa_enumaccounts_interactive_logon)
{
    LSA_UNICODE_STRING right; PVOID buf = NULL; ULONG cnt = 0xFFFF; NTSTATUS st, r; LSA_HANDLE h;
    REQUIRE(LENUMA); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    ustr(&right, L"SeInteractiveLogonRight");
    r = LENUMA(h, &right, &buf, &cnt);
    t_note("LsaEnumerateAccountsWithUserRight(SeInteractiveLogonRight) -> 0x%lx count %lu", r, cnt);
    if (r == 0) {
        PLSA_ENUMERATION_INFORMATION e = buf; ULONG i;
        CHECK(buf != NULL); CHECK(cnt > 0);
        for (i = 0; e && i < cnt; i++) {
            CHECKF(e[i].Sid != NULL, "entry %lu has no SID", i);
            if (e[i].Sid) CHECK(sid_len(e[i].Sid) >= 12);
        }
        r = LFREE(buf); EXPECT_EQ(r, 0);
    } else {
        CHECKF(buf == NULL && cnt == 0, "failure zeroes the outputs");
    }
    LCLOSE(h);
}
TEST(gap2_lsa_enumaccounts_unknown_right)
{
    LSA_UNICODE_STRING right; PVOID buf = (PVOID)(ULONG_PTR)0x1; ULONG cnt = 0x2; NTSTATUS st, r; LSA_HANDLE h;
    REQUIRE(LENUMA); REQUIRE(LOPEN); REQUIRE(LCLOSE); NOWINE();
    h = open_policy(&st, POLICY_LOOKUP_NAMES);
    if (!h) { t_skip("LsaOpenPolicy failed 0x%lx", st); return; }
    ustr(&right, L"olet_gap2_NoSuchRight");
    r = LENUMA(h, &right, &buf, &cnt);
    CHECKF(r != 0, "a right nobody holds must not enumerate successfully (0x%lx)", r);
    t_note("LsaEnumerateAccountsWithUserRight(bogus right) -> 0x%lx", r);
    CHECKF(buf == NULL && cnt == 0, "outputs zeroed after the RPC (%p, %lu)", buf, cnt);
    LCLOSE(h);
}
