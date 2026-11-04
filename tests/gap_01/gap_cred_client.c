#include "g1.h"
/* Credential manager client-side behaviour (sechost.dll). Decomp facts used:
 *  - strings arguments are converted by CredpAllocStrFromStr; a NULL mandatory string yields ERROR_INVALID_PARAMETER
 *    before any RPC. CredEnumerate accepts a NULL filter.
 *  - CredWrite* convert the CREDENTIAL first: NULL -> ERROR_INVALID_PARAMETER; AttributeCount * 24 + 0x58 overflowing
 *    32 bits -> ERROR_ARITHMETIC_OVERFLOW (534); nothing is sent to the service in either case.
 *  - CredEnumerate/CredReadDomainCredentials zero *Count and *Credentials before doing anything else.
 *  - everything else is mapped from the service's NTSTATUS (STATUS_NOT_FOUND -> ERROR_NOT_FOUND, ...). */

static const char *gname(void) { static char n[80]; if (!n[0]) snprintf(n, sizeof n, "olet_gap1_%lu_nosuchtarget", GetCurrentProcessId()); return n; }
static const WCHAR *gnamew(void) { static WCHAR n[80]; if (!n[0]) { char a[80]; snprintf(a, sizeof a, "%s", gname()); g1_wide(a, n, 80); } return n; }

TEST(gap1_cred_exports_present)
{
    static const char *const names[] = {
        "CredDeleteA","CredDeleteW","CredEnumerateA","CredEnumerateW","CredFindBestCredentialA","CredFindBestCredentialW",
        "CredGetSessionTypes","CredGetTargetInfoA","CredGetTargetInfoW","CredIsProtectedA","CredIsProtectedW",
        "CredProtectA","CredProtectW","CredReadA","CredReadDomainCredentialsA","CredReadDomainCredentialsW","CredReadW",
        "CredUnprotectA","CredUnprotectW","CredWriteA","CredWriteDomainCredentialsA","CredWriteDomainCredentialsW","CredWriteW" };
    unsigned i;
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        if (t_is_wine() && !g1_cred(names[i])) { t_note("%s missing (emulator)", names[i]); continue; }
        CHECKF(g1_cred(names[i]) != NULL, "%s missing", names[i]);
        if (!t_is_wine()) {
            /* advapi32 forwards these to sechost on Win10 */
            FARPROC a = t_proc_in("advapi32.dll", names[i]);
            CHECKF(a != NULL, "advapi32!%s missing", names[i]);
        }
    }
}

TEST(gap1_cred_write_null_credential)
{
    DWORD flags[] = { 0, CRED_PRESERVE_CREDENTIAL_BLOB, 0xffffffff };
    unsigned i;
    REQUIRE(CR(CredWriteA)); REQUIRE(CR(CredWriteW));
    for (i = 0; i < 3; i++) {
        SET_ERR(0); CHECK(!CR(CredWriteA)(NULL, flags[i])); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0); CHECK(!CR(CredWriteW)(NULL, flags[i])); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(gap1_cred_write_attribute_count_overflow)
{
    /* CredpConvertOneCredentialSize: AttributeCount*0x18 must fit in 32 bits together with the 0x58 byte header. */
    static const DWORD counts[] = { 0x0aaaaaaa, 0x0aaaaaab, 0x10000000, 0x7fffffff, 0xffffffff };
    unsigned i;
    REQUIRE(CR(CredWriteA)); REQUIRE(CR(CredWriteW));
    for (i = 0; i < sizeof counts / sizeof *counts; i++) {
        CREDENTIALW w; CREDENTIALA a;
        memset(&w, 0, sizeof w); memset(&a, 0, sizeof a);
        w.Type = a.Type = CRED_TYPE_GENERIC; w.Persist = a.Persist = CRED_PERSIST_SESSION;
        w.TargetName = (WCHAR *)gnamew(); a.TargetName = (char *)gname();
        w.AttributeCount = a.AttributeCount = counts[i];   /* Attributes stays NULL: never dereferenced */
        SET_ERR(0); CHECKF(!CR(CredWriteW)(&w, 0), "count %lx", counts[i]); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
        SET_ERR(0); CHECKF(!CR(CredWriteA)(&a, 0), "count %lx", counts[i]); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
        /* the conversion happens before anything is validated by the service, so a NULL TargetName still overflows first */
        w.TargetName = NULL; a.TargetName = NULL;
        SET_ERR(0); CHECK(!CR(CredWriteW)(&w, 0)); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
        SET_ERR(0); CHECK(!CR(CredWriteA)(&a, 0)); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
    }
}

TEST(gap1_cred_write_domain_client_validation)
{
    CREDENTIALW w; CREDENTIALA a; CREDENTIAL_TARGET_INFORMATIONW tw; CREDENTIAL_TARGET_INFORMATIONA ta;
    REQUIRE(CR(CredWriteDomainCredentialsW)); REQUIRE(CR(CredWriteDomainCredentialsA));
    memset(&w, 0, sizeof w); memset(&a, 0, sizeof a); memset(&tw, 0, sizeof tw); memset(&ta, 0, sizeof ta);
    w.Type = a.Type = CRED_TYPE_DOMAIN_PASSWORD; w.Persist = a.Persist = CRED_PERSIST_SESSION;
    w.TargetName = (WCHAR *)gnamew(); a.TargetName = (char *)gname();
    /* NULL credential */
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsW)(&tw, NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsA)(&ta, NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsW)(NULL, NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* valid credential, NULL target info: rejected locally */
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsW)(NULL, &w, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsA)(NULL, &a, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* the credential is converted first: an unconvertible one beats a NULL target info */
    w.AttributeCount = a.AttributeCount = 0x10000000;
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsW)(NULL, &w, 0)); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsA)(NULL, &a, 0)); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
    SET_ERR(0); CHECK(!CR(CredWriteDomainCredentialsW)(&tw, &w, 0)); EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
}

TEST(gap1_cred_read_domain_null_target_info)
{
    DWORD cw = 0x77, ca = 0x77; PCREDENTIALW *pw = (PCREDENTIALW *)(ULONG_PTR)0x1234; PCREDENTIALA *pa = (PCREDENTIALA *)(ULONG_PTR)0x1234;
    REQUIRE(CR(CredReadDomainCredentialsW)); REQUIRE(CR(CredReadDomainCredentialsA));
    SET_ERR(0); CHECK(!CR(CredReadDomainCredentialsW)(NULL, 0, &cw, &pw)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* count and array are zeroed before anything else happens */
    EXPECT_EQ(cw, 0); EXPECT_EQ(pw, 0);
    SET_ERR(0); CHECK(!CR(CredReadDomainCredentialsA)(NULL, 0, &ca, &pa)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ca, 0); EXPECT_EQ(pa, 0);
}

TEST(gap1_cred_read_domain_not_found)
{
    /* adapted from Wine dlls/advapi32/tests/cred.c (LGPL-2.1+): test_CredReadDomainCredentialsA */
    CREDENTIAL_TARGET_INFORMATIONA ta; CREDENTIAL_TARGET_INFORMATIONW tw;
    DWORD c; PCREDENTIALA *pa; PCREDENTIALW *pw; BOOL r;
    REQUIRE(CR(CredReadDomainCredentialsW)); REQUIRE(CR(CredReadDomainCredentialsA));
    memset(&ta, 0, sizeof ta); memset(&tw, 0, sizeof tw);
    ta.TargetName = ta.DnsServerName = (char *)gname();
    tw.TargetName = tw.DnsServerName = (WCHAR *)gnamew();
    c = 2; pa = (void *)0x12345; SET_ERR(0);
    r = CR(CredReadDomainCredentialsA)(&ta, 0, &c, &pa);
    if (!r && g1_no_cred_session(GetLastError())) return;
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(c, 0); EXPECT_EQ(pa, 0);
    c = 2; pw = (void *)0x12345; SET_ERR(0);
    r = CR(CredReadDomainCredentialsW)(&tw, 0, &c, &pw);
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(c, 0); EXPECT_EQ(pw, 0);
    /* with only a DnsServerName / only a TargetName; Wine's test accepts NOT_FOUND or INVALID_PARAMETER on Vista+ */
    ta.TargetName = NULL; c = 2; pa = (void *)0x12345; SET_ERR(0);
    r = CR(CredReadDomainCredentialsA)(&ta, 0, &c, &pa);
    CHECK(!r); { DWORD e = GetLastError(); CHECKF(e == ERROR_NOT_FOUND || e == ERROR_INVALID_PARAMETER, "err %lu", e); t_note("TargetName NULL, DnsServerName set: error %lu", e); }
    EXPECT_EQ(c, 0); EXPECT_EQ(pa, 0);
    ta.DnsServerName = NULL; c = 2; pa = (void *)0x12345; SET_ERR(0);
    r = CR(CredReadDomainCredentialsA)(&ta, 0, &c, &pa);
    CHECK(!r); { DWORD e = GetLastError(); CHECKF(e == ERROR_NOT_FOUND || e == ERROR_INVALID_PARAMETER, "err %lu", e); t_note("empty target info: error %lu", e); }
}

TEST(gap1_cred_read_null_target)
{
    PCREDENTIALA pa = (PCREDENTIALA)(ULONG_PTR)0xdead0001; PCREDENTIALW pw = (PCREDENTIALW)(ULONG_PTR)0xdead0001;
    DWORD t;
    REQUIRE(CR(CredReadA)); REQUIRE(CR(CredReadW));
    for (t = 0; t < 8; t++) {
        SET_ERR(0); CHECK(!CR(CredReadA)(NULL, t, 0, &pa)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0); CHECK(!CR(CredReadW)(NULL, t, 0, &pw)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    /* the output is only written on success */
    EXPECT_EQ(pa, 0xdead0001); EXPECT_EQ(pw, 0xdead0001);
}

TEST(gap1_cred_read_delete_not_found)
{
    PCREDENTIALA pa = (PCREDENTIALA)(ULONG_PTR)0xdead0001; PCREDENTIALW pw = (PCREDENTIALW)(ULONG_PTR)0xdead0001;
    BOOL r;
    REQUIRE(CR(CredReadA)); REQUIRE(CR(CredReadW)); REQUIRE(CR(CredDeleteA)); REQUIRE(CR(CredDeleteW));
    SET_ERR(0); r = CR(CredReadW)(gnamew(), CRED_TYPE_GENERIC, 0, &pw);
    if (!r && g1_no_cred_session(GetLastError())) return;
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!CR(CredReadA)(gname(), CRED_TYPE_GENERIC, 0, &pa)); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!CR(CredReadW)(gnamew(), CRED_TYPE_DOMAIN_PASSWORD, 0, &pw)); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!CR(CredDeleteW)(gnamew(), CRED_TYPE_GENERIC, 0)); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!CR(CredDeleteA)(gname(), CRED_TYPE_GENERIC, 0)); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!CR(CredDeleteW)(gnamew(), CRED_TYPE_DOMAIN_PASSWORD, 0)); EXPECT_ERR(ERROR_NOT_FOUND);
    EXPECT_EQ(pa, 0xdead0001); EXPECT_EQ(pw, 0xdead0001);
}

TEST(gap1_cred_read_delete_bad_type_and_flags)
{
    /* adapted from Wine dlls/advapi32/tests/cred.c (LGPL-2.1+): test_CredReadA / test_CredDeleteA */
    PCREDENTIALA pa; PCREDENTIALW pw; DWORD e;
    REQUIRE(CR(CredReadA)); REQUIRE(CR(CredReadW)); REQUIRE(CR(CredDeleteA)); REQUIRE(CR(CredDeleteW));
    SET_ERR(0); CHECK(!CR(CredReadA)(gname(), (DWORD)-1, 0, &pa));
    if (GetLastError() == ERROR_NO_SUCH_LOGON_SESSION) { g1_no_cred_session(1312); return; }
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredReadW)(gnamew(), (DWORD)-1, 0, &pw)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredDeleteA)(gname(), (DWORD)-1, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredDeleteW)(gnamew(), (DWORD)-1, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredReadA)(gname(), CRED_TYPE_GENERIC, 0xdeadbeef, &pa)); e = GetLastError();
    CHECKF(e == ERROR_INVALID_FLAGS || e == ERROR_INVALID_PARAMETER, "err %lu", e); t_note("CredReadA bad flags: %lu", e);
    SET_ERR(0); CHECK(!CR(CredReadW)(gnamew(), CRED_TYPE_GENERIC, 0xdeadbeef, &pw)); EXPECT_ERR(e);
    SET_ERR(0); CHECK(!CR(CredDeleteA)(gname(), CRED_TYPE_GENERIC, 0xdeadbeef)); e = GetLastError();
    CHECKF(e == ERROR_INVALID_FLAGS || e == ERROR_INVALID_PARAMETER, "err %lu", e); t_note("CredDeleteA bad flags: %lu", e);
    SET_ERR(0); CHECK(!CR(CredDeleteW)(gnamew(), CRED_TYPE_GENERIC, 0xdeadbeef)); EXPECT_ERR(e);
}

TEST(gap1_cred_delete_null_target)
{
    DWORD t;
    REQUIRE(CR(CredDeleteA)); REQUIRE(CR(CredDeleteW));
    for (t = 0; t < 8; t++) {
        SET_ERR(0); CHECK(!CR(CredDeleteA)(NULL, t, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0); CHECK(!CR(CredDeleteW)(NULL, t, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(gap1_cred_write_server_validation)
{
    /* adapted from Wine dlls/advapi32/tests/cred.c (LGPL-2.1+): test_CredWriteA. Every call here must FAIL, so nothing is stored. */
    CREDENTIALA c; BOOL r;
    REQUIRE(CR(CredWriteA)); REQUIRE(CR(CredWriteW)); REQUIRE(CR(CredDeleteA));
    memset(&c, 0, sizeof c);
    c.Type = CRED_TYPE_GENERIC; c.Comment = (char *)"Comment"; c.Persist = CRED_PERSIST_ENTERPRISE; c.UserName = (char *)"winetest";
    SET_ERR(0); r = CR(CredWriteA)(&c, 0);               /* NULL TargetName */
    if (r) { CR(CredDeleteA)(NULL, 0, 0); t_note("write with NULL TargetName succeeded"); }
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    c.TargetName = (char *)gname(); c.Type = CRED_TYPE_DOMAIN_PASSWORD; c.UserName = NULL;
    SET_ERR(0); r = CR(CredWriteA)(&c, 0);               /* domain credential needs a user name */
    if (r) { CR(CredDeleteA)(gname(), CRED_TYPE_DOMAIN_PASSWORD, 0); t_note("domain write with NULL user succeeded"); }
    CHECK(!r);
    if (!r && !g1_no_cred_session(GetLastError())) EXPECT_ERR(ERROR_BAD_USERNAME);
    c.Type = CRED_TYPE_GENERIC; c.UserName = (char *)"winetest";
    SET_ERR(0); r = CR(CredWriteA)(&c, 0x80);            /* undefined flag */
    if (r) { CR(CredDeleteA)(gname(), CRED_TYPE_GENERIC, 0); }
    CHECK(!r); { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_FLAGS || e == ERROR_INVALID_PARAMETER, "err %lu", e); t_note("CredWriteA flags 0x80: %lu", e); }
}

TEST(gap1_cred_enumerate_not_found)
{
    char filter[96]; WCHAR wf[96]; DWORD c; PCREDENTIALA *pa; PCREDENTIALW *pw; BOOL r;
    REQUIRE(CR(CredEnumerateA)); REQUIRE(CR(CredEnumerateW));
    snprintf(filter, sizeof filter, "olet_gap1_%lu_nomatch*", GetCurrentProcessId()); g1_wide(filter, wf, 96);
    c = 0x55; pa = (void *)0x1234; SET_ERR(0); r = CR(CredEnumerateA)(filter, 0, &c, &pa);
    if (!r && g1_no_cred_session(GetLastError())) return;
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(c, 0); EXPECT_EQ(pa, 0);
    c = 0x55; pw = (void *)0x1234; SET_ERR(0); r = CR(CredEnumerateW)(wf, 0, &c, &pw);
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(c, 0); EXPECT_EQ(pw, 0);
}

TEST(gap1_cred_enumerate_flags)
{
    char filter[96]; WCHAR wf[96]; DWORD c, e; PCREDENTIALA *pa; PCREDENTIALW *pw; BOOL r;
    REQUIRE(CR(CredEnumerateA)); REQUIRE(CR(CredEnumerateW));
    snprintf(filter, sizeof filter, "olet_gap1_%lu_flags*", GetCurrentProcessId()); g1_wide(filter, wf, 96);
    /* CRED_ENUMERATE_ALL_CREDENTIALS together with a filter: ERROR_INVALID_FLAGS (documented) */
    c = 9; pa = (void *)0x1234; SET_ERR(0); r = CR(CredEnumerateA)(filter, CRED_ENUMERATE_ALL_CREDENTIALS, &c, &pa);
    if (!r && g1_no_cred_session(GetLastError())) return;
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_FLAGS); EXPECT_EQ(c, 0); EXPECT_EQ(pa, 0);
    c = 9; pw = (void *)0x1234; SET_ERR(0); r = CR(CredEnumerateW)(wf, CRED_ENUMERATE_ALL_CREDENTIALS, &c, &pw);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_FLAGS); EXPECT_EQ(c, 0); EXPECT_EQ(pw, 0);
    /* unknown flag bits */
    c = 9; pa = (void *)0x1234; SET_ERR(0); r = CR(CredEnumerateA)(NULL, 0xdeadbeee, &c, &pa);
    /* Win10 22H2: unknown flag bits come back from the credential service as ERROR_INVALID_PARAMETER, not ERROR_INVALID_FLAGS */
    CHECK(!r); e = GetLastError(); CHECKF(e == ERROR_INVALID_PARAMETER, "err %lu", e); EXPECT_EQ(c, 0); EXPECT_EQ(pa, 0);
    c = 9; pw = (void *)0x1234; SET_ERR(0); r = CR(CredEnumerateW)(NULL, 0x80000000, &c, &pw);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap1_cred_enumerate_all_no_filter)
{
    /* NULL filter and the ALL_CREDENTIALS flag are both valid; the set may legitimately be empty (ERROR_NOT_FOUND). */
    DWORD c = 0; PCREDENTIALW *pw = NULL; PCREDENTIALA *pa = NULL; BOOL r; DWORD i, fl[2] = { 0, CRED_ENUMERATE_ALL_CREDENTIALS };
    int k;
    REQUIRE(CR(CredEnumerateW)); REQUIRE(CR(CredEnumerateA)); REQUIRE(CR(CredFree));
    for (k = 0; k < 2; k++) {
        SET_ERR(0); r = CR(CredEnumerateW)(NULL, fl[k], &c, &pw);
        if (!r) { if (g1_no_cred_session(GetLastError())) return; EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(c, 0); EXPECT_EQ(pw, 0); t_note("flags %lu: empty set", fl[k]); continue; }
        CHECK(c > 0); CHECK(pw != NULL);
        for (i = 0; i < c && pw; i++) { CHECK(pw[i] != NULL); if (pw[i]) CHECK(pw[i]->TargetName != NULL); }
        /* the array and all credentials are one allocation: the first record starts right after the pointer array */
        if (c && pw && pw[0]) CHECK((BYTE *)pw[0] >= (BYTE *)(pw + c));
        { DWORD cw = c; CR(CredFree)(pw);
          SET_ERR(0); r = CR(CredEnumerateA)(NULL, fl[k], &c, &pa);
          CHECK(r); if (r) { CHECKF(c == cw, "A count %lu W count %lu", c, cw); CR(CredFree)(pa); } }
    }
}

TEST(gap1_cred_find_best)
{
    PCREDENTIALA pa = (PCREDENTIALA)(ULONG_PTR)0xdead0001; PCREDENTIALW pw = (PCREDENTIALW)(ULONG_PTR)0xdead0001; BOOL r; DWORD e;
    REQUIRE(CR(CredFindBestCredentialA)); REQUIRE(CR(CredFindBestCredentialW));
    SET_ERR(0); CHECK(!CR(CredFindBestCredentialA)(NULL, CRED_TYPE_GENERIC, 0, &pa)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredFindBestCredentialW)(NULL, CRED_TYPE_GENERIC, 0, &pw)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(pa, 0xdead0001); EXPECT_EQ(pw, 0xdead0001);
    SET_ERR(0); r = CR(CredFindBestCredentialW)(gnamew(), CRED_TYPE_GENERIC, 0, &pw);
    if (!r && g1_no_cred_session(GetLastError())) return;
    /* Win10 22H2: the credential service refuses CRED_TYPE_GENERIC here with ERROR_INVALID_PARAMETER (not ERROR_NOT_FOUND) */
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(pw, 0xdead0001);
    SET_ERR(0); r = CR(CredFindBestCredentialA)(gname(), CRED_TYPE_GENERIC, 0, &pa);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(pa, 0xdead0001);
    SET_ERR(0); r = CR(CredFindBestCredentialW)(gnamew(), CRED_TYPE_DOMAIN_PASSWORD, 0, &pw); e = GetLastError();
    CHECK(!r); t_note("FindBest unknown target, DOMAIN_PASSWORD: error %lu", e);
    SET_ERR(0); r = CR(CredFindBestCredentialW)(gnamew(), CRED_TYPE_GENERIC, 0x80, &pw); e = GetLastError();
    CHECK(!r); t_note("FindBest unknown target, flags 0x80: error %lu", e);
}

TEST(gap1_cred_get_target_info)
{
    PCREDENTIAL_TARGET_INFORMATIONA ta = (void *)(ULONG_PTR)0xdead0001; PCREDENTIAL_TARGET_INFORMATIONW tw = (void *)(ULONG_PTR)0xdead0001; BOOL r;
    REQUIRE(CR(CredGetTargetInfoA)); REQUIRE(CR(CredGetTargetInfoW));
    SET_ERR(0); CHECK(!CR(CredGetTargetInfoA)(NULL, 0, &ta)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredGetTargetInfoW)(NULL, 0, &tw)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!CR(CredGetTargetInfoW)(NULL, CRED_ALLOW_NAME_RESOLUTION, &tw)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ta, 0xdead0001); EXPECT_EQ(tw, 0xdead0001);
    SET_ERR(0); r = CR(CredGetTargetInfoW)(gnamew(), 0, &tw);
    if (!r && g1_no_cred_session(GetLastError())) return;
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(tw, 0xdead0001);
    SET_ERR(0); r = CR(CredGetTargetInfoA)(gname(), 0, &ta);
    CHECK(!r); EXPECT_ERR(ERROR_NOT_FOUND); EXPECT_EQ(ta, 0xdead0001);
}

TEST(gap1_cred_get_session_types)
{
    /* adapted from Wine dlls/advapi32/tests/cred.c (LGPL-2.1+): START_TEST(cred) */
    DWORD p[16]; unsigned i; BOOL r;
    REQUIRE(CR(CredGetSessionTypes));
    for (i = 0; i < 16; i++) p[i] = 0xcccccccc;
    SET_ERR(0); r = CR(CredGetSessionTypes)(G1_CRED_TYPE_MAXIMUM, p);
    if (!r && g1_no_cred_session(GetLastError())) return;
    CHECK(r);
    if (!r) return;
    EXPECT_EQ(p[0], CRED_PERSIST_NONE);                  /* type 0 does not exist */
    for (i = 0; i < G1_CRED_TYPE_MAXIMUM; i++) { CHECKF(p[i] <= CRED_PERSIST_ENTERPRISE, "persists[%u] = 0x%lx", i, p[i]); t_note("persist[%u] = %lu", i, p[i]); }
    for (i = G1_CRED_TYPE_MAXIMUM; i < 16; i++) CHECKF(p[i] == 0xcccccccc, "element %u past the requested count was modified: 0x%lx", i, p[i]);
}

TEST(gap1_cred_get_session_types_short_count)
{
    DWORD p[16]; unsigned i; BOOL r; DWORD cnt[] = { 1, 2, 3, 5 };
    REQUIRE(CR(CredGetSessionTypes));
    for (i = 0; i < sizeof cnt / sizeof *cnt; i++) {
        unsigned j;
        for (j = 0; j < 16; j++) p[j] = 0xcccccccc;
        SET_ERR(0); r = CR(CredGetSessionTypes)(cnt[i], p);
        t_note("count %lu: ret %d err %lu p[0..4]=%lx %lx %lx %lx %lx", cnt[i], r, GetLastError(), p[0], p[1], p[2], p[3], p[4]);
        if (r) for (j = cnt[i]; j < 16; j++) CHECKF(p[j] == 0xcccccccc, "count %lu: element %u modified", cnt[i], j);
    }
}
