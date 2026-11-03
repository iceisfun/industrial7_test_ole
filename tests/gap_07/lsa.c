/* LsaOpenPolicy / LsaQueryInformationPolicy / LsaClose / LsaFreeMemory (sechost.dll; advapi32 forwards).
 * Client-side decomp (sechost 19041):
 *   LsaOpenPolicy:  *PolicyHandle = NULL and ObjectAttributes->RootDirectory = NULL are written FIRST, then the RPC to lsass.
 *   LsaQueryInformationPolicy: InformationClass == 13 (PolicyDnsDomainInformationInt) -> STATUS_INVALID_PARAMETER before anything
 *     else; NULL handle -> STATUS_INVALID_HANDLE (Buffer untouched); otherwise the RPC is made and *Buffer is zeroed
 *     before the server status is evaluated. */
#include "gap7.h"
#include <sddl.h>

#define ST_INVALID_HANDLE    ((NTSTATUS)0xC0000008)
#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define ST_ACCESS_DENIED     ((NTSTATUS)0xC0000022)

#define LOPEN  UIN(SH, LsaOpenPolicy)
#define LQUERY UIN(SH, LsaQueryInformationPolicy)
#define LCLOSE UIN(SH, LsaClose)
#define LFREE  UIN(SH, LsaFreeMemory)

/* Wine implements only stub LSA policy functions (constant results, no access checks, no class validation). */
#define WSKIP_LSA() do { if (t_is_wine()) { t_skip("wine LSA policy functions are stubs"); return; } } while (0)

#ifndef POLICY_LOOKUP_NAMES
#define POLICY_LOOKUP_NAMES 0x800
#endif

static NTSTATUS open_with(ACCESS_MASK access, LSA_HANDLE *h)
{
    LSA_OBJECT_ATTRIBUTES oa;
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    *h = NULL;
    return LOPEN(NULL, &oa, access, h);
}
static int ustr_ok(const LSA_UNICODE_STRING *s)
{
    return (s->Length % 2) == 0 && s->Length <= s->MaximumLength && (s->Length == 0 || s->Buffer != NULL);
}

TEST(gap7_lsa_open_policy_clears_out_handle_and_rootdirectory_first)
{
    WSKIP_LSA();
    LSA_OBJECT_ATTRIBUTES oa; LSA_HANDLE h = (LSA_HANDLE)(ULONG_PTR)0xdeadbeef; NTSTATUS st;
    REQUIRE(LOPEN); REQUIRE(LCLOSE);
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa; oa.RootDirectory = (HANDLE)(ULONG_PTR)0x1234;
    st = LOPEN(NULL, &oa, POLICY_VIEW_LOCAL_INFORMATION, &h);
    CHECKF(st == 0, "LsaOpenPolicy 0x%lx", (unsigned long)st);
    CHECK(oa.RootDirectory == NULL);               /* overwritten by the client before the RPC */
    CHECK(h != NULL && h != (LSA_HANDLE)(ULONG_PTR)0xdeadbeef);
    if (st == 0) EXPECT_EQ(LCLOSE(h), 0);
}
TEST(gap7_lsa_open_policy_access_masks)
{
    WSKIP_LSA();
    static const ACCESS_MASK ok[] = { POLICY_VIEW_LOCAL_INFORMATION, POLICY_LOOKUP_NAMES, POLICY_VIEW_LOCAL_INFORMATION | POLICY_LOOKUP_NAMES, MAXIMUM_ALLOWED };
    unsigned i;
    REQUIRE(LOPEN); REQUIRE(LCLOSE);
    for (i = 0; i < NELEM(ok); i++) {
        LSA_HANDLE h; NTSTATUS st = open_with(ok[i], &h);
        CHECKF(st == 0, "access 0x%lx: status 0x%lx", (unsigned long)ok[i], (unsigned long)st);
        if (st == 0) { CHECK(h != NULL); EXPECT_EQ(LCLOSE(h), 0); }
    }
}
TEST(gap7_lsa_open_policy_zero_and_all_access_observed)
{
    WSKIP_LSA();
    LSA_HANDLE h; NTSTATUS st;
    REQUIRE(LOPEN); REQUIRE(LCLOSE);
    st = open_with(0, &h);
    t_note("LsaOpenPolicy(access 0) -> 0x%lx", (unsigned long)st);
    if (st == 0) LCLOSE(h);
    st = open_with(POLICY_ALL_ACCESS, &h);
    t_note("LsaOpenPolicy(POLICY_ALL_ACCESS) -> 0x%lx (elevation dependent)", (unsigned long)st);
    if (st == 0) LCLOSE(h); else CHECK(h == NULL);        /* the out handle is NULL on failure */
}
TEST(gap7_lsa_open_policy_handles_distinct_and_independent)
{
    WSKIP_LSA();
    LSA_HANDLE a, b; NTSTATUS sa, sb;
    REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LQUERY);
    sa = open_with(POLICY_VIEW_LOCAL_INFORMATION, &a); sb = open_with(POLICY_VIEW_LOCAL_INFORMATION, &b);
    CHECK(sa == 0 && sb == 0);
    if (sa || sb) { if (!sa) LCLOSE(a); if (!sb) LCLOSE(b); return; }
    CHECK(a != b);
    EXPECT_EQ(LCLOSE(a), 0);
    {   /* b is still usable after a is closed */
        PVOID buf = NULL; NTSTATUS q = LQUERY(b, PolicyAccountDomainInformation, &buf);
        CHECKF(q == 0, "query on surviving handle 0x%lx", (unsigned long)q);
        if (buf) LFREE(buf);
    }
    EXPECT_EQ(LCLOSE(b), 0);
}

/* ---------------- LsaQueryInformationPolicy ---------------- */
TEST(gap7_lsa_query_null_handle_returns_invalid_handle_and_leaves_buffer)
{
    WSKIP_LSA();
    static const int classes[] = { PolicyAuditLogInformation, PolicyAuditEventsInformation, PolicyPrimaryDomainInformation,
        PolicyAccountDomainInformation, PolicyLsaServerRoleInformation, PolicyDefaultQuotaInformation, PolicyModificationInformation,
        PolicyDnsDomainInformation, 0, 14, 99, 1000 };
    unsigned i;
    REQUIRE(LQUERY);
    for (i = 0; i < NELEM(classes); i++) {
        PVOID buf = (PVOID)(ULONG_PTR)0xfeedf00d; NTSTATUS st = LQUERY(NULL, (POLICY_INFORMATION_CLASS)classes[i], &buf);
        CHECKF(st == ST_INVALID_HANDLE, "class %d: 0x%lx", classes[i], (unsigned long)st);
        CHECKF(buf == (PVOID)(ULONG_PTR)0xfeedf00d, "class %d: Buffer modified", classes[i]);
    }
}
TEST(gap7_lsa_query_class_13_is_invalid_parameter_before_handle_check)
{
    WSKIP_LSA();
    LSA_HANDLE h; NTSTATUS st; PVOID buf = (PVOID)(ULONG_PTR)0xfeedf00d;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE);
    st = LQUERY(NULL, (POLICY_INFORMATION_CLASS)13, &buf);
    CHECKF(st == ST_INVALID_PARAMETER, "NULL handle: 0x%lx", (unsigned long)st);
    CHECK(buf == (PVOID)(ULONG_PTR)0xfeedf00d);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) == 0) {
        st = LQUERY(h, (POLICY_INFORMATION_CLASS)13, &buf);
        CHECKF(st == ST_INVALID_PARAMETER, "valid handle: 0x%lx", (unsigned long)st);
        CHECK(buf == (PVOID)(ULONG_PTR)0xfeedf00d);
        LCLOSE(h);
    }
}
TEST(gap7_lsa_query_unknown_class_with_valid_handle_fails_and_nulls_buffer)
{
    WSKIP_LSA();
    static const int classes[] = { 0, 14, 15, 100 };
    LSA_HANDLE h; unsigned i;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    for (i = 0; i < NELEM(classes); i++) {
        PVOID buf = (PVOID)(ULONG_PTR)0xfeedf00d; NTSTATUS st = LQUERY(h, (POLICY_INFORMATION_CLASS)classes[i], &buf);
        CHECKF((LONG)st < 0, "class %d unexpectedly succeeded 0x%lx", classes[i], (unsigned long)st);
        CHECKF(buf == NULL, "class %d: *Buffer must be cleared (%p)", classes[i], buf);
        t_note("LsaQueryInformationPolicy(class %d) -> 0x%lx", classes[i], (unsigned long)st);
        if (buf) LFREE(buf);
    }
    LCLOSE(h);
}
TEST(gap7_lsa_query_account_domain_contents)
{
    WSKIP_LSA();
    LSA_HANDLE h; PVOID buf = NULL; NTSTATUS st;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    st = LQUERY(h, PolicyAccountDomainInformation, &buf);
    CHECKF(st == 0, "0x%lx", (unsigned long)st);
    if (st == 0 && buf) {
        POLICY_ACCOUNT_DOMAIN_INFO *d = buf; SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
        CHECK(ustr_ok(&d->DomainName)); CHECK(d->DomainName.Length > 0);
        CHECK(d->DomainSid != NULL && IsValidSid(d->DomainSid));
        if (d->DomainSid && IsValidSid(d->DomainSid)) {
            SID *s = d->DomainSid;
            CHECK(!memcmp(&s->IdentifierAuthority, &nt, sizeof nt));
            CHECK(s->SubAuthority[0] == 21);
            CHECKF(s->SubAuthorityCount == 4, "SubAuthorityCount %u", s->SubAuthorityCount);
        }
        {
            WCHAR host[MAX_COMPUTERNAME_LENGTH + 2]; DWORD n = NELEM(host);
            if (GetComputerNameW(host, &n) && d->DomainName.Length / 2 == n && !_wcsnicmp(d->DomainName.Buffer, host, n))
                t_note("account domain name equals the computer name");
            else t_note("account domain name differs from the computer name (domain controller?)");
        }
        EXPECT_EQ(LFREE(buf), 0);
    }
    LCLOSE(h);
}
TEST(gap7_lsa_query_primary_domain_contents)
{
    WSKIP_LSA();
    LSA_HANDLE h; PVOID buf = NULL; NTSTATUS st;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    st = LQUERY(h, PolicyPrimaryDomainInformation, &buf);
    CHECKF(st == 0, "0x%lx", (unsigned long)st);
    if (st == 0 && buf) {
        POLICY_PRIMARY_DOMAIN_INFO *d = buf;
        CHECK(ustr_ok(&d->Name));
        if (d->Sid) CHECK(IsValidSid(d->Sid));
        t_note("primary domain \"%.*ls\" sid %s", d->Name.Length / 2, d->Name.Buffer ? d->Name.Buffer : L"", d->Sid ? "present (domain member)" : "NULL (workgroup)");
        EXPECT_EQ(LFREE(buf), 0);
    }
    LCLOSE(h);
}
TEST(gap7_lsa_query_dns_domain_information)
{
    WSKIP_LSA();
    LSA_HANDLE h; PVOID buf = NULL; NTSTATUS st;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    st = LQUERY(h, PolicyDnsDomainInformation, &buf);
    CHECKF(st == 0, "0x%lx", (unsigned long)st);
    if (st == 0 && buf) {
        POLICY_DNS_DOMAIN_INFO *d = buf;
        CHECK(ustr_ok(&d->Name)); CHECK(ustr_ok(&d->DnsDomainName)); CHECK(ustr_ok(&d->DnsForestName));
        if (d->Sid) CHECK(IsValidSid(d->Sid));
        if (!d->Sid) CHECK(d->DnsDomainName.Length == 0);   /* workgroup machine: no DNS domain */
        EXPECT_EQ(LFREE(buf), 0);
    }
    LCLOSE(h);
}
TEST(gap7_lsa_query_view_local_information_classes_succeed)
{
    WSKIP_LSA();
    static const int classes[] = { PolicyAccountDomainInformation, PolicyPrimaryDomainInformation, PolicyLsaServerRoleInformation,
        PolicyDefaultQuotaInformation, PolicyModificationInformation, PolicyDnsDomainInformation };
    LSA_HANDLE h; unsigned i;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    for (i = 0; i < NELEM(classes); i++) {
        PVOID buf = (PVOID)(ULONG_PTR)0xfeedf00d; NTSTATUS st = LQUERY(h, (POLICY_INFORMATION_CLASS)classes[i], &buf);
        CHECKF(st == 0, "class %d: 0x%lx", classes[i], (unsigned long)st);
        if (st == 0) { CHECKF(buf != NULL && buf != (PVOID)(ULONG_PTR)0xfeedf00d, "class %d buffer", classes[i]); EXPECT_EQ(LFREE(buf), 0); }
    }
    LCLOSE(h);
}
TEST(gap7_lsa_query_server_role_values)
{
    WSKIP_LSA();
    LSA_HANDLE h; PVOID buf = NULL;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    if (LQUERY(h, PolicyLsaServerRoleInformation, &buf) == 0 && buf) {
        POLICY_LSA_SERVER_ROLE_INFO *r = buf;
        CHECKF(r->LsaServerRole == PolicyServerRoleBackup || r->LsaServerRole == PolicyServerRolePrimary, "role %d", r->LsaServerRole);
        t_note("LSA server role %d (2 backup, 3 primary)", r->LsaServerRole);
        LFREE(buf);
    } else CHECKF(0, "query failed");
    LCLOSE(h);
}
TEST(gap7_lsa_query_requires_matching_access_right)
{
    WSKIP_LSA();
    LSA_HANDLE h; PVOID buf = (PVOID)(ULONG_PTR)0xfeedf00d; NTSTATUS st;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE);
    /* a handle holding only POLICY_LOOKUP_NAMES cannot read local information */
    if (open_with(POLICY_LOOKUP_NAMES, &h) != 0) { t_skip("cannot open policy with POLICY_LOOKUP_NAMES"); return; }
    st = LQUERY(h, PolicyAccountDomainInformation, &buf);
    CHECKF(st == ST_ACCESS_DENIED, "0x%lx", (unsigned long)st);
    CHECKF(buf == NULL, "*Buffer cleared on server failure (%p)", buf);
    LCLOSE(h);
    /* audit information needs POLICY_VIEW_AUDIT_INFORMATION, which a VIEW_LOCAL handle does not have */
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) == 0) {
        buf = (PVOID)(ULONG_PTR)0xfeedf00d;
        st = LQUERY(h, PolicyAuditEventsInformation, &buf);
        CHECKF(st == ST_ACCESS_DENIED, "audit events with view-local handle: 0x%lx", (unsigned long)st);
        CHECK(buf == NULL);
        LCLOSE(h);
    }
}
TEST(gap7_lsa_query_repeated_calls_return_fresh_buffers)
{
    WSKIP_LSA();
    LSA_HANDLE h; PVOID a = NULL, b = NULL; unsigned i;
    REQUIRE(LQUERY); REQUIRE(LOPEN); REQUIRE(LCLOSE); REQUIRE(LFREE);
    if (open_with(POLICY_VIEW_LOCAL_INFORMATION, &h) != 0) { t_skip("cannot open policy"); return; }
    CHECK(LQUERY(h, PolicyAccountDomainInformation, &a) == 0);
    CHECK(LQUERY(h, PolicyAccountDomainInformation, &b) == 0);
    CHECK(a != NULL && b != NULL && a != b);
    if (a && b) {
        POLICY_ACCOUNT_DOMAIN_INFO *x = a, *y = b;
        CHECK(x->DomainName.Length == y->DomainName.Length && EqualSid(x->DomainSid, y->DomainSid));
    }
    if (a) LFREE(a);
    if (b) LFREE(b);
    for (i = 0; i < 200; i++) { PVOID p = NULL; if (LQUERY(h, PolicyAccountDomainInformation, &p) != 0) { CHECKF(0, "iteration %u", i); break; } LFREE(p); }
    LCLOSE(h);
}
TEST(gap7_lsa_free_memory_null_and_forwarding)
{
    WSKIP_LSA();
    __typeof__(&LsaOpenPolicy) adv = UIN("advapi32.dll", LsaOpenPolicy);
    LSA_HANDLE h; LSA_OBJECT_ATTRIBUTES oa; NTSTATUS st;
    REQUIRE(LFREE); REQUIRE(adv); REQUIRE(LCLOSE);
    EQ(LFREE(NULL), 0);
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa; h = NULL;
    st = adv(NULL, &oa, POLICY_VIEW_LOCAL_INFORMATION, &h);
    CHECKF(st == 0, "advapi32!LsaOpenPolicy 0x%lx", (unsigned long)st);
    if (st == 0) EXPECT_EQ(LCLOSE(h), 0);
    t_note("advapi32!LsaOpenPolicy %s sechost's address", (FARPROC)adv == (FARPROC)LOPEN ? "is" : "is not");
}
