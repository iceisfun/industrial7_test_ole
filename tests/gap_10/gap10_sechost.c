/* gap10_sechost: internal / undocumented sechost.dll exports. Wine has no tests for sechost: everything below
 * comes from the Win10 22H2 decompilation of the function. */
#include "gap10.h"
#include <sddl.h>

typedef BOOL (WINAPI *cvt_dom_w)(PSID, PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *);
typedef BOOL (WINAPI *cvt_dom_a)(PSID, PSID, const char *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *);
typedef BOOL (WINAPI *cvt_root_w)(PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *);
typedef BOOL (WINAPI *cvt_sd2str_root_w)(PSID, PSECURITY_DESCRIPTOR, DWORD, SECURITY_INFORMATION, WCHAR **, ULONG *);

/* S-1-5-21-1-2-3 */
static BYTE g_dom[SECURITY_MAX_SID_SIZE], g_dom2[SECURITY_MAX_SID_SIZE], g_bad[SECURITY_MAX_SID_SIZE];
static void sid_init(BYTE *b, DWORD a, DWORD c, DWORD d) {
    SID *s = (SID *)b; memset(b, 0, SECURITY_MAX_SID_SIZE);
    s->Revision = 1; s->SubAuthorityCount = 4;
    s->IdentifierAuthority.Value[5] = 5;
    s->SubAuthority[0] = 21; s->SubAuthority[1] = a; s->SubAuthority[2] = c; s->SubAuthority[3] = d;
}
static void sids(void) { sid_init(g_dom, 1, 2, 3); sid_init(g_dom2, 7, 8, 9); memcpy(g_bad, g_dom, sizeof g_bad); ((SID *)g_bad)->Revision = 0; }

static BOOL sid_is_domain_rid(PSID s, BYTE *dom, DWORD rid) {
    SID *a = s, *d = (SID *)dom;
    if (!a || a->SubAuthorityCount != d->SubAuthorityCount + 1) return FALSE;
    for (int i = 0; i < d->SubAuthorityCount; i++) if (a->SubAuthority[i] != d->SubAuthority[i]) return FALSE;
    return a->SubAuthority[d->SubAuthorityCount] == rid && !memcmp(&a->IdentifierAuthority, &d->IdentifierAuthority, 6);
}

TEST(gap10_stringsd_domain_w_positive) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", BOOL, ConvertStringSDToSDDomainW, (PSID, PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *));
    sids();
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0;
    SET_ERR(0xdead);
    CHECK(ConvertStringSDToSDDomainW_(g_dom, NULL, L"O:DAG:DUD:(A;;GA;;;DA)", 1, &sd, &sz));
    EXPECT_ERR(0);                                              /* success clears last error */
    REQUIRE(sd);
    CHECK(IsValidSecurityDescriptor(sd));
    EXPECT_EQ(sz, GetSecurityDescriptorLength(sd));
    SECURITY_DESCRIPTOR_CONTROL ctl; DWORD rev;
    CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev));
    CHECK(ctl & SE_SELF_RELATIVE);
    PSID owner = NULL, grp = NULL; BOOL def;
    CHECK(GetSecurityDescriptorOwner(sd, &owner, &def));
    CHECK(sid_is_domain_rid(owner, g_dom, 512));                /* DA = domain admins = <domain>-512 */
    CHECK(GetSecurityDescriptorGroup(sd, &grp, &def));
    CHECK(sid_is_domain_rid(grp, g_dom, 513));                  /* DU = domain users = <domain>-513 */
    LocalFree(sd);
    /* a different domain gives different SIDs */
    sd = NULL;
    CHECK(ConvertStringSDToSDDomainW_(g_dom2, g_dom, L"O:DA", 1, &sd, &sz));
    if (sd) { GetSecurityDescriptorOwner(sd, &owner, &def); CHECK(sid_is_domain_rid(owner, g_dom2, 512)); LocalFree(sd); }
    /* strings that do not need the domain still work */
    sd = NULL;
    CHECK(ConvertStringSDToSDDomainW_(g_dom, NULL, L"O:BAD:(A;;GA;;;WD)", 1, &sd, &sz));
    if (sd) { CHECK(IsValidSecurityDescriptor(sd)); LocalFree(sd); }
}
TEST(gap10_stringsd_domain_w_parameter_errors) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", BOOL, ConvertStringSDToSDDomainW, (PSID, PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *));
    sids();
    PSECURITY_DESCRIPTOR sd = (void *)0x1; ULONG sz = 77;
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(NULL, NULL, L"D:", 1, &sd, &sz), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_dom, NULL, NULL, 1, &sd, &sz), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_dom, NULL, L"D:", 1, NULL, &sz), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_bad, NULL, L"D:", 1, &sd, &sz), ERROR_INVALID_PARAMETER);       /* invalid domain SID */
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_dom, g_bad, L"D:", 1, &sd, &sz), ERROR_INVALID_PARAMETER);    /* invalid root domain SID */
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_dom, NULL, L"D:", 0, &sd, &sz), ERROR_UNKNOWN_REVISION);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_dom, NULL, L"D:", 2, &sd, &sz), ERROR_UNKNOWN_REVISION);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainW_(g_dom, NULL, L"D:", 0xffffffff, &sd, &sz), ERROR_UNKNOWN_REVISION);
    CHECK(sd == (void *)0x1); EXPECT_EQ(sz, 77);                /* outputs untouched */
}
TEST(gap10_stringsd_domain_a_matches_w) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", BOOL, ConvertStringSDToSDDomainA, (PSID, PSID, const char *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *));
    FNIN("sechost.dll", BOOL, ConvertStringSDToSDDomainW, (PSID, PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *));
    sids();
    PSECURITY_DESCRIPTOR a = NULL, w = NULL; ULONG sa = 0, sw = 0;
    SET_ERR(0xdead);
    CHECK(ConvertStringSDToSDDomainA_(g_dom, NULL, "O:DAG:DUD:(A;;GA;;;DA)", 1, &a, &sa));
    EXPECT_ERR(0);
    CHECK(ConvertStringSDToSDDomainW_(g_dom, NULL, L"O:DAG:DUD:(A;;GA;;;DA)", 1, &w, &sw));
    EXPECT_EQ(sa, sw);
    if (a && w && sa == sw) CHECK(!memcmp(a, w, sa));
    if (a) LocalFree(a);
    if (w) LocalFree(w);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainA_(g_dom, NULL, NULL, 1, &a, &sa), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainA_(NULL, NULL, "D:", 1, &a, &sa), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainA_(g_dom, NULL, "D:", 1, NULL, &sa), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainA_(g_dom, g_bad, "D:", 1, &a, &sa), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDDomainA_(g_dom, NULL, "D:", 2, &a, &sa), ERROR_UNKNOWN_REVISION);
}
TEST(gap10_stringsd_rootdomain_w) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", BOOL, ConvertStringSDToSDRootDomainW, (PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *));
    sids();
    PSECURITY_DESCRIPTOR sd = (void *)0x1; ULONG sz = 77;
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDRootDomainW_(g_dom, NULL, 1, &sd, &sz), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDRootDomainW_(g_dom, L"D:", 1, NULL, &sz), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDRootDomainW_(g_dom, L"D:", 0, &sd, &sz), ERROR_UNKNOWN_REVISION);
    SET_ERR(0); EXPECT_FAIL(!ConvertStringSDToSDRootDomainW_(g_dom, L"D:", 2, &sd, &sz), ERROR_UNKNOWN_REVISION);
    CHECK(sd == (void *)0x1); EXPECT_EQ(sz, 77);
    sd = NULL;
    SET_ERR(0xdead);
    CHECK(ConvertStringSDToSDRootDomainW_(g_dom, L"O:EAG:EA", 1, &sd, &sz));       /* EA = enterprise admins = <root domain>-519 */
    EXPECT_ERR(0);
    if (sd) {
        PSID owner = NULL; BOOL def;
        CHECK(GetSecurityDescriptorOwner(sd, &owner, &def));
        CHECK(sid_is_domain_rid(owner, g_dom, 519));
        EXPECT_EQ(sz, GetSecurityDescriptorLength(sd));
        LocalFree(sd);
    }
}
TEST(gap10_sd_to_string_rootdomain_w) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", BOOL, ConvertSDToStringSDRootDomainW, (PSID, PSECURITY_DESCRIPTOR, DWORD, SECURITY_INFORMATION, WCHAR **, ULONG *));
    FNIN("sechost.dll", BOOL, ConvertStringSDToSDRootDomainW, (PSID, const WCHAR *, DWORD, PSECURITY_DESCRIPTOR *, ULONG *));
    sids();
    PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0;
    CHECK(ConvertStringSDToSDRootDomainW_(g_dom, L"O:BAG:BAD:(A;;GA;;;BA)", 1, &sd, &sz));
    REQUIRE(sd);
    WCHAR *str = (void *)0x1; ULONG n = 77;
    SET_ERR(0); EXPECT_FAIL(!ConvertSDToStringSDRootDomainW_(g_dom, NULL, 1, 7, &str, &n), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertSDToStringSDRootDomainW_(g_dom, sd, 1, 7, NULL, &n), ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_FAIL(!ConvertSDToStringSDRootDomainW_(g_dom, sd, 1, 0, &str, &n), ERROR_INVALID_PARAMETER);   /* no SECURITY_INFORMATION bits */
    SET_ERR(0); EXPECT_FAIL(!ConvertSDToStringSDRootDomainW_(g_dom, sd, 2, 7, &str, &n), ERROR_UNKNOWN_REVISION);
    SET_ERR(0); EXPECT_FAIL(!ConvertSDToStringSDRootDomainW_(g_dom, sd, 0, 7, &str, &n), ERROR_UNKNOWN_REVISION);
    CHECK(str == (void *)0x1);
    SET_ERR(0xdead);
    if (ConvertSDToStringSDRootDomainW_(g_dom, sd, 1, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &str, &n)) {
        EXPECT_ERR(0);
        CHECK(!lstrcmpW(str, L"O:BAG:BAD:(A;;GA;;;BA)"));
        t_note("ConvertSDToStringSDRootDomainW length %lu (string is %d characters)", n, lstrlenW(L"O:BAG:BAD:(A;;GA;;;BA)"));
        LocalFree(str);
    } else CHECKF(0, "ConvertSDToStringSDRootDomainW failed %lu", GetLastError());
    LocalFree(sd);
}

/* ------------------------------------------------------------ conditional ACE helpers */
TEST(gap10_local_condition_to_string_bad_input) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, LocalGetStringForCondition, (const void *, ULONG, WCHAR **, void *, void *, void *, char));
    FNIN("sechost.dll", DWORD, LocalGetReferencedTokenTypesForCondition, (const void *, ULONG, ULONG *));
    WCHAR *str = NULL; ULONG tt = 0;
    static const BYTE good_magic[8] = { 'a', 'r', 't', 'x', 0, 0, 0, 0 };
    static const BYTE bad_magic[8] = { 'a', 'r', 't', 'y', 0, 0, 0, 0 };
    EXPECT_EQ(LocalGetStringForCondition_(NULL, 8, &str, NULL, NULL, NULL, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(LocalGetStringForCondition_(good_magic, 0, &str, NULL, NULL, NULL, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(LocalGetStringForCondition_(good_magic, 8, NULL, NULL, NULL, NULL, 0), ERROR_INVALID_PARAMETER);
    for (ULONG cb = 1; cb < 6; cb++) EXPECT_EQ(LocalGetStringForCondition_(good_magic, cb, &str, NULL, NULL, NULL, 0), ERROR_INVALID_ACE_CONDITION);   /* 805 */
    EXPECT_EQ(LocalGetStringForCondition_(bad_magic, 8, &str, NULL, NULL, NULL, 0), ERROR_INVALID_ACE_CONDITION);
    EXPECT_EQ(LocalGetReferencedTokenTypesForCondition_(NULL, 8, &tt), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(LocalGetReferencedTokenTypesForCondition_(good_magic, 0, &tt), ERROR_INVALID_PARAMETER);
    for (ULONG cb = 1; cb < 6; cb++) EXPECT_EQ(LocalGetReferencedTokenTypesForCondition_(good_magic, cb, &tt), ERROR_INVALID_ACE_CONDITION);
    EXPECT_EQ(LocalGetReferencedTokenTypesForCondition_(bad_magic, 8, &tt), ERROR_INVALID_ACE_CONDITION);
}

/* ------------------------------------------------------------ user service names */
TEST(gap10_queryuserservicenameforcontext) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, QueryUserServiceNameForContext, (const WCHAR *, ULONGLONG, WCHAR *, ULONG *));
    WCHAR buf[0x200]; ULONG n;
    /* "<name>_<context hex>" built in a 0x101 character buffer */
    n = 0x101; memset(buf, 0xEE, sizeof buf);
    EXPECT_EQ(QueryUserServiceNameForContext_(L"svc", 0x1a2b, buf, &n), ERROR_SUCCESS);
    CHECK(!lstrcmpW(buf, L"svc_1a2b"));
    EXPECT_EQ(n, 9);                                           /* length + terminator */
    n = 0x200;
    EXPECT_EQ(QueryUserServiceNameForContext_(L"CDPUserSvc", 0x123456789abcdefULL, buf, &n), ERROR_SUCCESS);
    CHECK(!lstrcmpW(buf, L"CDPUserSvc_123456789abcdef"));
    EXPECT_EQ(n, 27);
    n = 0x101;
    EXPECT_EQ(QueryUserServiceNameForContext_(L"X", 1, buf, &n), ERROR_SUCCESS);
    CHECK(!lstrcmpW(buf, L"X_1")); EXPECT_EQ(n, 4);
    /* buffer protocol: anything below 0x101 characters is rejected and the required size is reported */
    for (ULONG sz = 0x40; sz <= 0x100; sz += 0x40) {
        n = sz;
        DWORD rc = QueryUserServiceNameForContext_(L"svc", 5, buf, &n);
        CHECKF(rc == ERROR_INSUFFICIENT_BUFFER, "size %lu rc %lu", sz, rc);
        EXPECT_EQ(n, 0x101);
    }
    n = 0x100;
    EXPECT_EQ(QueryUserServiceNameForContext_(L"svc", 5, buf, &n), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n, 0x101);
    n = 0;
    EXPECT_EQ(QueryUserServiceNameForContext_(L"svc", 5, NULL, &n), ERROR_INSUFFICIENT_BUFFER);   /* size query */
    EXPECT_EQ(n, 0x101);
    /* invalid parameters */
    n = 0x101;
    EXPECT_EQ(QueryUserServiceNameForContext_(NULL, 5, buf, &n), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(QueryUserServiceNameForContext_(L"svc", 0, buf, &n), ERROR_INVALID_PARAMETER);       /* zero context */
    EXPECT_EQ(QueryUserServiceNameForContext_(L"svc", 5, buf, NULL), ERROR_INVALID_PARAMETER);
    n = 5;
    EXPECT_EQ(QueryUserServiceNameForContext_(L"svc", 5, NULL, &n), ERROR_INVALID_PARAMETER);       /* NULL buffer with a non-zero size */
}

/* ------------------------------------------------------------ misc simple helpers */
TEST(gap10_lsalookupfreememory_is_lsafreememory) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", LONG, LsaLookupFreeMemory, (void *));
    FNIN("sechost.dll", LONG, LsaFreeMemory, (void *));
    CHECK((void *)LsaLookupFreeMemory_ == (void *)LsaFreeMemory_);     /* one RVA, two export names */
    EXPECT_EQ(LsaLookupFreeMemory_(NULL), 0);
    void *p = LocalAlloc(LMEM_FIXED, 64);
    CHECK(p != NULL);
    EXPECT_EQ(LsaLookupFreeMemory_(p), 0);                             /* LocalFree'd, always STATUS_SUCCESS */
}
TEST(gap10_free_transient_object_sd_null_is_noop) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", void, FreeTransientObjectSecurityDescriptor, (void *));
    FreeTransientObjectSecurityDescriptor_(NULL);
    CHECK(1);
}
TEST(gap10_queryTransientObjectSD_invalid_args) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", LONG, QueryTransientObjectSecurityDescriptor, (int, const WCHAR *, void **));
    void *sd = (void *)0x1;
    EXPECT_EQ(QueryTransientObjectSecurityDescriptor_(0, NULL, &sd), (LONG)0xC000000D);   /* STATUS_INVALID_PARAMETER */
    EXPECT_EQ(QueryTransientObjectSecurityDescriptor_(0, L"x", NULL), (LONG)0xC000000D);
    CHECK(sd == (void *)0x1);
}
TEST(gap10_getserviceprocesstoken_null_args) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, GetServiceProcessToken, (void *, void *, void *));
    int x;
    EXPECT_EQ(GetServiceProcessToken_(NULL, &x, &x), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(GetServiceProcessToken_(&x, NULL, &x), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(GetServiceProcessToken_(&x, &x, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap10_etwqueryrealtimeconsumer_invalid_handles) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, EtwQueryRealtimeConsumer, (ULONGLONG, ULONG *, ULONG *));
    ULONG a = 1, b = 2;
    EXPECT_EQ(EtwQueryRealtimeConsumer_(0, &a, &b), ERROR_INVALID_HANDLE);
    EXPECT_EQ(EtwQueryRealtimeConsumer_(~0ULL, &a, &b), ERROR_INVALID_HANDLE);
    EXPECT_EQ(EtwQueryRealtimeConsumer_(0x1234, NULL, &b), ERROR_INVALID_PARAMETER);        /* handle plausible, NULL output */
    EXPECT_EQ(EtwQueryRealtimeConsumer_(0x1234, &a, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(EtwQueryRealtimeConsumer_(0x1234, &a, &b), ERROR_INVALID_HANDLE);              /* no such trace session */
    EXPECT_EQ(a, 1); EXPECT_EQ(b, 2);
}
TEST(gap10_release_identity_provider_enum_context_invalid) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", void, ReleaseIdentityProviderEnumContext, (ULONG_PTR));
    ReleaseIdentityProviderEnumContext_(0);
    ReleaseIdentityProviderEnumContext_(3);                    /* low bits are masked: still NULL */
    CHECK(1);
}
TEST(gap10_rpcclientcapabilitycheck_null_name) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, RpcClientCapabilityCheck, (const WCHAR *));
    EXPECT_EQ(RpcClientCapabilityCheck_(NULL), 0x80070057);
    DWORD rc = RpcClientCapabilityCheck_(L"internetClient");   /* needs an active RPC call to impersonate */
    t_note("RpcClientCapabilityCheck outside a call: 0x%lx", (unsigned long)rc);
    CHECK((LONG)rc < 0);
}
TEST(gap10_capabilitycheck_null_args) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", LONG, CapabilityCheck, (HANDLE, const WCHAR *, char *));
    char res = 0x55;
    EXPECT_EQ(CapabilityCheck_(NULL, NULL, &res), (LONG)0xC000000D);
    EXPECT_EQ(CapabilityCheck_(NULL, L"internetClient", NULL), (LONG)0xC000000D);
    LONG st = CapabilityCheck_(NULL, L"internetClient", &res);   /* current effective token */
    t_note("CapabilityCheck(internetClient) status 0x%lx result %d", (unsigned long)st, res);
}

/* ------------------------------------------------------------ CredParseUserNameWithType */
typedef struct { USHORT Length, MaximumLength; WCHAR *Buffer; } USTR;
TEST(gap10_credparseusername_formats) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", LONG, CredParseUserNameWithType, (WCHAR *, USTR *, USTR *, DWORD *));
    USTR user, dom; DWORD type;
    WCHAR in[300];
    /* DOMAIN\user -> type 2, input is split in place */
    wcscpy(in, L"CORP\\alice");
    memset(&user, 0xAA, sizeof user); memset(&dom, 0xAA, sizeof dom); type = 99;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), 0);
    EXPECT_EQ(type, 2);
    CHECK(user.Buffer == in + 5 && user.Length == 10);
    CHECK(dom.Buffer == in && dom.Length == 8);
    CHECK(!lstrcmpW(user.Buffer, L"alice")); CHECK(!lstrcmpW(dom.Buffer, L"CORP"));
    EXPECT_EQ(in[4], 0);                                        /* the backslash was overwritten by a NUL */
    /* user@realm -> type 1 (UPN), no split */
    wcscpy(in, L"bob@example.com");
    memset(&dom, 0xAA, sizeof dom); type = 99;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), 0);
    EXPECT_EQ(type, 1);
    CHECK(user.Buffer == in && user.Length == 30);
    CHECK(dom.Buffer == NULL && dom.Length == 0);
    CHECK(!lstrcmpW(in, L"bob@example.com"));
    /* plain name -> type 4 */
    wcscpy(in, L"carol");
    type = 99;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), 0);
    EXPECT_EQ(type, 4);
    CHECK(user.Buffer == in && user.Length == 10); CHECK(dom.Buffer == NULL && dom.Length == 0);
    /* either output may be NULL */
    wcscpy(in, L"D\\u"); type = 99;
    EXPECT_EQ(CredParseUserNameWithType_(in, NULL, NULL, &type), 0);
    EXPECT_EQ(type, 2);
    wcscpy(in, L"x"); type = 99;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, NULL, &type), 0);
    EXPECT_EQ(type, 4);
    /* the last backslash wins */
    wcscpy(in, L"a\\b\\c"); type = 99;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), 0);
    EXPECT_EQ(type, 2);
    CHECK(!lstrcmpW(user.Buffer, L"c")); CHECK(!lstrcmpW(dom.Buffer, L"a\\b"));
}
TEST(gap10_credparseusername_errors) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", LONG, CredParseUserNameWithType, (WCHAR *, USTR *, USTR *, DWORD *));
    USTR user, dom; DWORD type = 77;
    WCHAR in[700];
    EXPECT_EQ(CredParseUserNameWithType_(NULL, &user, &dom, &type), (LONG)0xC0000062);     /* STATUS_INVALID_ACCOUNT_NAME */
    wcscpy(in, L"");
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), (LONG)0xC0000062);
    /* plain names are limited to 0x100 characters, others to 0x201 */
    for (int i = 0; i < 0x100; i++) in[i] = L'a'; in[0x100] = 0;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), 0);
    EXPECT_EQ(type, 4);
    in[0x100] = L'a'; in[0x101] = 0;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), (LONG)0xC0000062);
    for (int i = 0; i < 0x201; i++) in[i] = L'a'; in[0x201] = 0; in[0x100] = L'\\';
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), 0);
    in[0x201] = L'a'; in[0x202] = 0;
    EXPECT_EQ(CredParseUserNameWithType_(in, &user, &dom, &type), (LONG)0xC0000062);
}

/* ------------------------------------------------------------ user-service name queries (open the SCM read-only first) */
TEST(gap10_queryuserservicename_invalid_args) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, QueryUserServiceName, (const WCHAR *, ULONGLONG, ULONGLONG, WCHAR *, ULONG *));
    WCHAR buf[0x110]; ULONG n;
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM access (%lu)", GetLastError()); return; }
    CloseServiceHandle(scm);
    n = 7;
    EXPECT_EQ(QueryUserServiceName_(NULL, 1, 0, buf, &n), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(n, 0x101);                                    /* the size is always rewritten on the way out */
    n = 7;
    EXPECT_EQ(QueryUserServiceName_(L"svc", 0, 0, buf, &n), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(n, 0x101);
    n = 5;
    EXPECT_EQ(QueryUserServiceName_(L"svc", 1, 0, NULL, &n), ERROR_INVALID_PARAMETER);       /* NULL buffer but non-zero size */
    EXPECT_EQ(n, 0x101);
}
TEST(gap10_querylocaluserservicename_invalid_args) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, QueryLocalUserServiceName, (const WCHAR *, WCHAR *, ULONG *));
    WCHAR buf[0x110]; ULONG n = 0x101;
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM access (%lu)", GetLastError()); return; }
    CloseServiceHandle(scm);
    EXPECT_EQ(QueryLocalUserServiceName_(NULL, buf, &n), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(QueryLocalUserServiceName_(L"svc", buf, NULL), ERROR_INVALID_PARAMETER);
    n = 9;
    EXPECT_EQ(QueryLocalUserServiceName_(L"svc", NULL, &n), ERROR_INVALID_PARAMETER);
}

/* ------------------------------------------------------------ local RPC security helpers: argument and name-suffix validation */
TEST(gap10_localrpc_security_helpers_validate_arguments) {
    NOT_WINE("sechost internals are unimplemented stubs");
    FNIN("sechost.dll", DWORD, LocalRpcBindingSetAuthInfoEx, (void *, const WCHAR *));
    FNIN("sechost.dll", DWORD, SetLocalRpcServerInterfaceSecurity, (void *, const WCHAR *, void *));
    EXPECT_EQ(LocalRpcBindingSetAuthInfoEx_(NULL, L"X\\Server"), 0x80070057);
    EXPECT_EQ(LocalRpcBindingSetAuthInfoEx_((void *)1, NULL), 0x80070057);
    /* the object name must end in "\Server" (binding) or "\Interface" (interface): anything else fails before
     * the handle is touched */
    DWORD rc = LocalRpcBindingSetAuthInfoEx_((void *)1, L"Microsoft.Test\\Client");
    CHECKF((LONG)rc < 0, "wrong suffix accepted: 0x%lx", (unsigned long)rc);
    t_note("LocalRpcBindingSetAuthInfoEx wrong suffix hr=0x%lx", (unsigned long)rc);
    EXPECT_EQ(SetLocalRpcServerInterfaceSecurity_(NULL, L"X\\Interface", NULL), 0x80070057);
    EXPECT_EQ(SetLocalRpcServerInterfaceSecurity_((void *)1, NULL, NULL), 0x80070057);
    rc = SetLocalRpcServerInterfaceSecurity_((void *)1, L"Microsoft.Test\\Server", NULL);
    CHECKF((LONG)rc < 0, "wrong suffix accepted: 0x%lx", (unsigned long)rc);
    t_note("SetLocalRpcServerInterfaceSecurity wrong suffix hr=0x%lx", (unsigned long)rc);
}
