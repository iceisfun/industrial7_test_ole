#include "ssh.h"
/* LookupAccountSidLocalW/A, LookupAccountNameLocalW/A (sechost) -- local LSA lookups.
 * Decomp (LookupAccountSidInternal / LookupAccountNameInternal):
 *  - sid lookup: both buffers must be large enough for name+NUL and domain+NUL, otherwise
 *    ERROR_INSUFFICIENT_BUFFER with BOTH required sizes (NUL included) written back and peUse untouched;
 *    on success the sizes are rewritten as lengths WITHOUT the NUL;
 *  - name lookup: *cbSid < RtlLengthSid or *cchDomain < len+1 -> ERROR_INSUFFICIENT_BUFFER with both required sizes;
 *    on success the SID is copied, *cchDomain = length without NUL (cbSid is left as passed);
 *  - unknown names: STATUS_NONE_MAPPED -> ERROR_NONE_MAPPED (1332). */

#define LSW UIN(SH, LookupAccountSidLocalW)
#define LNW UIN(SH, LookupAccountNameLocalW)
#define LSA_ UIN(SH, LookupAccountSidLocalA)
#define LNA UIN(SH, LookupAccountNameLocalA)

#define NOWINE() do { if (t_is_wine()) { t_skip("sechost Local lookups are unimplemented stubs under wine"); return; } } while (0)
typedef struct { WCHAR name[256], dom[256]; DWORD ncch, ndom; SID_NAME_USE use; BOOL ok; DWORD err; } SIDRES;

static SIDRES lookup_sid(PSID sid)
{
    SIDRES r; memset(&r, 0, sizeof r);
    r.ncch = 256; r.ndom = 256; r.use = (SID_NAME_USE)0xcafebabe;
    SET_ERR(0xdeadbeef);
    r.ok = LSW(sid, r.name, &r.ncch, r.dom, &r.ndom, &r.use);
    r.err = GetLastError();
    return r;
}
static void expect_sid_lookup(PSID sid, const char *name, const char *dom, SID_NAME_USE use, BOOL name_hard)
{
    SIDRES r = lookup_sid(sid); char n[256], d[256];
    CHECKF(r.ok, "lookup failed err %lu", r.err);
    if (!r.ok) return;
    narrow(r.name, n, sizeof n); narrow(r.dom, d, sizeof d);
    EXPECT_EQ(r.use, use);
    EXPECT_EQ(r.ncch, wcslen(r.name));
    EXPECT_EQ(r.ndom, wcslen(r.dom));
    if (name_hard) { CHECKF(!strcmp(n, name), "name \"%s\" want \"%s\"", n, name); CHECKF(!strcmp(d, dom), "domain \"%s\" want \"%s\"", d, dom); }
    else if (strcmp(n, name) || strcmp(d, dom)) t_note("non-English? got %s\\%s want %s\\%s", d, n, dom, name);
}

TEST(lookupsid_w_wellknown_table)
{
    static const struct { unsigned long long a; int n; DWORD s[2]; const char *name, *dom; SID_NAME_USE use; } t[] = {
        { 1, 1, {0},        "Everyone",            "",               SidTypeWellKnownGroup },
        { 5, 1, {18},       "SYSTEM",              "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {19},       "LOCAL SERVICE",       "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {20},       "NETWORK SERVICE",     "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {11},       "Authenticated Users", "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {7},        "ANONYMOUS LOGON",     "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {2},        "NETWORK",             "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {4},        "INTERACTIVE",         "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {6},        "SERVICE",             "NT AUTHORITY",   SidTypeWellKnownGroup },
        { 5, 1, {9},        "ENTERPRISE DOMAIN CONTROLLERS", "NT AUTHORITY", SidTypeWellKnownGroup },
        { 3, 1, {0},        "CREATOR OWNER",       "",               SidTypeWellKnownGroup },
        { 3, 1, {1},        "CREATOR GROUP",       "",               SidTypeWellKnownGroup },
        { 5, 2, {32, 544},  "Administrators",      "BUILTIN",        SidTypeAlias },
        { 5, 2, {32, 545},  "Users",               "BUILTIN",        SidTypeAlias },
        { 5, 2, {32, 546},  "Guests",              "BUILTIN",        SidTypeAlias },
        { 5, 2, {32, 547},  "Power Users",         "BUILTIN",        SidTypeAlias },
        { 5, 2, {32, 551},  "Backup Operators",    "BUILTIN",        SidTypeAlias },
        { 5, 2, {32, 552},  "Replicator",          "BUILTIN",        SidTypeAlias },
    };
    unsigned i;
    REQUIRE(LSW); NOWINE();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        SIDBUF b; PISID s = (PISID)mk_sid(&b, t[i].a, 0); int k;
        s->SubAuthorityCount = (BYTE)t[i].n; for (k = 0; k < t[i].n; k++) s->SubAuthority[k] = t[i].s[k];
        expect_sid_lookup(s, t[i].name, t[i].dom, t[i].use, is_english_ui());
    }
}
TEST(lookupsid_w_use_values_are_numeric_contract)
{
    EXPECT_EQ(SidTypeUser, 1); EXPECT_EQ(SidTypeGroup, 2); EXPECT_EQ(SidTypeDomain, 3); EXPECT_EQ(SidTypeAlias, 4);
    EXPECT_EQ(SidTypeWellKnownGroup, 5); EXPECT_EQ(SidTypeDeletedAccount, 6); EXPECT_EQ(SidTypeInvalid, 7);
    EXPECT_EQ(SidTypeUnknown, 8); EXPECT_EQ(SidTypeComputer, 9); EXPECT_EQ(SidTypeLabel, 10);
}
TEST(lookupsid_w_mandatory_labels)
{
    static const struct { DWORD rid; const char *name; } t[] = {
        { 4096, "Low Mandatory Level" }, { 8192, "Medium Mandatory Level" }, { 12288, "High Mandatory Level" }, { 16384, "System Mandatory Level" },
    };
    unsigned i;
    REQUIRE(LSW); NOWINE();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        SIDBUF b; SIDRES r = lookup_sid(mk_sid(&b, 16, 1, t[i].rid)); char n[256], d[256];
        CHECKF(r.ok, "rid %lu err %lu", t[i].rid, r.err);
        if (!r.ok) continue;
        EXPECT_EQ(r.use, SidTypeLabel);
        narrow(r.name, n, 256); narrow(r.dom, d, 256);
        if (is_english_ui()) { CHECKF(!strcmp(n, t[i].name), "\"%s\"", n); CHECKF(!strcmp(d, "Mandatory Label"), "\"%s\"", d); }
    }
}
TEST(lookupsid_w_buffer_protocol_everyone)
{
    /* Everyone: name "Everyone" (8), empty domain -> needs 9 and 1 */
    SIDBUF b; WCHAR name[32], dom[8]; DWORD cn, cd; SID_NAME_USE use; BOOL r;
    REQUIRE(LSW); NOWINE();
    mk_sid(&b, 1, 1, 0);
    cn = 0; cd = 0; use = (SID_NAME_USE)0xcafebabe; SET_ERR(0);
    r = LSW(&b, NULL, &cn, NULL, &cd, &use);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(use, 0xcafebabe);
    if (is_english_ui()) { EXPECT_EQ(cn, 9); EXPECT_EQ(cd, 1); } else CHECK(cn >= 2 && cd == 1);
    /* both exactly right */
    { DWORD en = cn, ed = cd; WCHAR nm[64], dm[8];
      cn = en; cd = ed; use = 0;
      CHECK(LSW(&b, nm, &cn, dm, &cd, &use));
      EXPECT_EQ(cn, en - 1); EXPECT_EQ(cd, 0); EXPECT_EQ(dm[0], 0); EXPECT_EQ(use, SidTypeWellKnownGroup);
      EXPECT_EQ(wcslen(nm), cn);
      /* name one too small */
      cn = en - 1; cd = ed; SET_ERR(0);
      EXPECT_FAIL(!LSW(&b, nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
      EXPECT_EQ(cn, en); EXPECT_EQ(cd, ed);
      /* domain too small (0) but name fine: both sizes are still reported */
      cn = en; cd = 0; SET_ERR(0);
      EXPECT_FAIL(!LSW(&b, nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
      EXPECT_EQ(cn, en); EXPECT_EQ(cd, ed);
      /* oversize buffers: success returns exact lengths */
      cn = 32; cd = 8; CHECK(LSW(&b, name, &cn, dom, &cd, &use));
      EXPECT_EQ(cn, en - 1); EXPECT_EQ(cd, 0);
    }
}
TEST(lookupsid_w_buffer_protocol_system)
{
    SIDBUF b; WCHAR nm[64], dm[64]; DWORD cn = 0, cd = 0, en, ed; SID_NAME_USE use = 0;
    REQUIRE(LSW); NOWINE();
    mk_sid(&b, 5, 1, 18);
    SET_ERR(0);
    EXPECT_FAIL(!LSW(&b, NULL, &cn, NULL, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    en = cn; ed = cd;
    if (is_english_ui()) { EXPECT_EQ(en, 7); EXPECT_EQ(ed, 13); }
    CHECK(en > 1 && ed > 1);
    /* each undersized variant */
    cn = en - 1; cd = ed; SET_ERR(0);
    EXPECT_FAIL(!LSW(&b, nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cn, en); EXPECT_EQ(cd, ed);
    cn = en; cd = ed - 1; SET_ERR(0);
    EXPECT_FAIL(!LSW(&b, nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cn, en); EXPECT_EQ(cd, ed);
    cn = 1; cd = 1; SET_ERR(0);
    EXPECT_FAIL(!LSW(&b, nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cn, en); EXPECT_EQ(cd, ed);
    /* exact */
    cn = en; cd = ed; use = 0;
    CHECK(LSW(&b, nm, &cn, dm, &cd, &use));
    EXPECT_EQ(cn, en - 1); EXPECT_EQ(cd, ed - 1);
    EXPECT_EQ(nm[cn], 0); EXPECT_EQ(dm[cd], 0);
    EXPECT_EQ(use, SidTypeWellKnownGroup);
}
TEST(lookupsid_w_failure_does_not_modify_buffers)
{
    SIDBUF b; WCHAR nm[4] = { 'x', 'x', 'x', 'x' }, dm[4] = { 'y', 'y', 'y', 'y' }; DWORD cn = 4, cd = 4; SID_NAME_USE use = (SID_NAME_USE)0xcafebabe;
    REQUIRE(LSW); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LSW(mk_sid(&b, 5, 2, 32, 544), nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(nm[0], 'x'); EXPECT_EQ(dm[0], 'y'); EXPECT_EQ(use, 0xcafebabe);
}
TEST(lookupsid_w_builtin_admins_sizes)
{
    SIDBUF b; DWORD cn = 0, cd = 0; SID_NAME_USE use = 0;
    REQUIRE(LSW); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LSW(mk_sid(&b, 5, 2, 32, 544), NULL, &cn, NULL, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    if (is_english_ui()) { EXPECT_EQ(cn, 15); EXPECT_EQ(cd, 8); }
}
TEST(lookupsid_w_unmapped_domain_sid)
{
    SIDBUF b; WCHAR nm[64], dm[64]; DWORD cn = 64, cd = 64; SID_NAME_USE use = (SID_NAME_USE)0xcafebabe;
    REQUIRE(LSW); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LSW(mk_sid(&b, 5, 5, 21, 111, 222, 333, 444), nm, &cn, dm, &cd, &use), ERROR_NONE_MAPPED);
    EXPECT_EQ(use, 0xcafebabe);
}
TEST(lookupsid_w_unmapped_wellformed_sids)
{
    static const struct { unsigned long long a; int n; DWORD s[3]; } t[] = {
        { 5, 2, {32, 9999} }, { 7, 1, {1} }, { 5, 3, {80, 1, 2} }, { 0x123456, 1, {1} },
    };
    unsigned i;
    REQUIRE(LSW); NOWINE();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        SIDBUF b; PISID s = (PISID)mk_sid(&b, t[i].a, 0); int k; SIDRES r;
        s->SubAuthorityCount = (BYTE)t[i].n; for (k = 0; k < t[i].n; k++) s->SubAuthority[k] = t[i].s[k];
        r = lookup_sid(s);
        if (r.ok) { char n[256]; narrow(r.name, n, 256); t_note("sid #%u mapped to %s (use %d)", i, n, r.use); }
        else CHECKF(r.err == ERROR_NONE_MAPPED, "sid #%u err %lu", i, r.err);
    }
}
TEST(lookupsid_w_unknown_machine_rid_sid)
{
    /* an account SID of the local machine with a RID that cannot exist */
    HANDLE tok; BYTE buf[256]; DWORD n = 0;
    REQUIRE(LSW); NOWINE();
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("token"); return; }
    if (GetTokenInformation(tok, TokenUser, buf, sizeof buf, &n)) {
        PSID u = ((TOKEN_USER *)buf)->User.Sid; SIDBUF b; PISID s = (PISID)&b; SIDRES r; DWORD cnt;
        memcpy(&b, u, GetLengthSid(u)); cnt = s->SubAuthorityCount;
        if (cnt >= 2) {
            s->SubAuthority[cnt - 1] = 0x7ffffff0;
            r = lookup_sid(s);
            CHECK(!r.ok); if (!r.ok) CHECKF(r.err == ERROR_NONE_MAPPED, "err %lu", r.err);
        } else t_note("token user sid has %lu subauthorities", cnt);
    }
    CloseHandle(tok);
}
TEST(lookupsid_w_token_user)
{
    HANDLE tok; BYTE buf[256]; DWORD n = 0;
    REQUIRE(LSW); NOWINE();
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("token"); return; }
    if (GetTokenInformation(tok, TokenUser, buf, sizeof buf, &n)) {
        PSID u = ((TOKEN_USER *)buf)->User.Sid; SIDRES r = lookup_sid(u); char nm[256], dm[256], cn[256]; DWORD cl = 256;
        CHECKF(r.ok, "err %lu", r.err);
        if (r.ok) {
            narrow(r.name, nm, 256); narrow(r.dom, dm, 256);
            t_note("token user %s\\%s use %d", dm, nm, r.use);
            CHECK(r.use == SidTypeUser || r.use == SidTypeWellKnownGroup);
            GetComputerNameA(cn, &cl);
            if (r.use == SidTypeUser) CHECK(r.dom[0] != 0);
        }
    }
    CloseHandle(tok);
}
TEST(lookupsid_w_deterministic_repeat)
{
    SIDBUF b; SIDRES r1, r2;
    REQUIRE(LSW); NOWINE();
    mk_sid(&b, 5, 2, 32, 544);
    r1 = lookup_sid(&b); r2 = lookup_sid(&b);
    CHECK(r1.ok && r2.ok);
    EXPECT_WSTR(r1.name, r2.name); EXPECT_WSTR(r1.dom, r2.dom);
}
TEST(lookupsid_w_matches_advapi_lookupaccountsid)
{
    SIDBUF b; SIDRES r; WCHAR nm[256], dm[256]; DWORD cn = 256, cd = 256; SID_NAME_USE use = 0;
    REQUIRE(LSW); NOWINE(); REQUIRE(UIN(AD, LookupAccountSidW));
    mk_sid(&b, 5, 2, 32, 544);
    r = lookup_sid(&b);
    CHECK(UIN(AD, LookupAccountSidW)(NULL, &b, nm, &cn, dm, &cd, &use));
    if (r.ok) { EXPECT_WSTR(r.name, nm); EXPECT_WSTR(r.dom, dm); EXPECT_EQ(use, r.use); EXPECT_EQ(cn, r.ncch); }
}

/* ---- A variants ---- */
TEST(lookupsid_a_basic)
{
    SIDBUF b; char nm[64], dm[64]; DWORD cn = 64, cd = 64; SID_NAME_USE use = 0;
    REQUIRE(LSA_); NOWINE();
    CHECK(LSA_(mk_sid(&b, 5, 2, 32, 544), nm, &cn, dm, &cd, &use));
    EXPECT_EQ(use, SidTypeAlias);
    EXPECT_EQ(cn, strlen(nm)); EXPECT_EQ(cd, strlen(dm));
    if (is_english_ui()) { EXPECT_STR(nm, "Administrators"); EXPECT_STR(dm, "BUILTIN"); }
}
TEST(lookupsid_a_buffer_protocol)
{
    SIDBUF b; char nm[64], dm[64]; DWORD cn = 0, cd = 0, en, ed; SID_NAME_USE use = 0;
    REQUIRE(LSA_); NOWINE();
    mk_sid(&b, 5, 1, 18);
    SET_ERR(0);
    EXPECT_FAIL(!LSA_(&b, NULL, &cn, NULL, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    en = cn; ed = cd;
    if (is_english_ui()) { EXPECT_EQ(en, 7); EXPECT_EQ(ed, 13); }
    cn = en - 1; cd = ed; SET_ERR(0);
    EXPECT_FAIL(!LSA_(&b, nm, &cn, dm, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cn, en); EXPECT_EQ(cd, ed);
    cn = en; cd = ed;
    CHECK(LSA_(&b, nm, &cn, dm, &cd, &use));
    EXPECT_EQ(cn, en - 1); EXPECT_EQ(cd, ed - 1);
}
TEST(lookupsid_a_unmapped)
{
    SIDBUF b; char nm[64], dm[64]; DWORD cn = 64, cd = 64; SID_NAME_USE use = 0;
    REQUIRE(LSA_); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LSA_(mk_sid(&b, 5, 5, 21, 111, 222, 333, 444), nm, &cn, dm, &cd, &use), ERROR_NONE_MAPPED);
}

/* ---- name -> SID ---- */
typedef struct { BYTE sid[SECURITY_MAX_SID_SIZE]; WCHAR dom[256]; DWORD cb, cd; SID_NAME_USE use; BOOL ok; DWORD err; } NAMERES;
static NAMERES lookup_name(const WCHAR *name)
{
    NAMERES r; memset(&r, 0, sizeof r);
    r.cb = sizeof r.sid; r.cd = 256; r.use = (SID_NAME_USE)0xcafebabe;
    SET_ERR(0xdeadbeef);
    r.ok = LNW(name, r.sid, &r.cb, r.dom, &r.cd, &r.use);
    r.err = GetLastError();
    return r;
}
static void expect_name(const char *name, unsigned long long a, int n, DWORD s0, DWORD s1, const char *dom, SID_NAME_USE use)
{
    WCHAR w[128]; NAMERES r; SIDBUF b; PISID e = (PISID)mk_sid(&b, a, 0); char d[256];
    widen(name, w, 128);
    r = lookup_name(w);
    CHECKF(r.ok, "\"%s\" err %lu", name, r.err);
    if (!r.ok) return;
    e->SubAuthorityCount = (BYTE)n; e->SubAuthority[0] = s0; e->SubAuthority[1] = s1;
    CHECKF(EqualSid(r.sid, &b), "\"%s\": sid differs", name);
    CHECKF(r.use == use, "\"%s\": use %d want %d", name, r.use, use);
    narrow(r.dom, d, 256);
    CHECKF(!strcmp(d, dom), "\"%s\": domain \"%s\" want \"%s\"", name, d, dom);
    EXPECT_EQ(r.cd, strlen(d));
}
TEST(lookupname_w_wellknown_english)
{
    REQUIRE(LNW); NOWINE();
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    expect_name("Everyone", 1, 1, 0, 0, "", SidTypeWellKnownGroup);
    expect_name("SYSTEM", 5, 1, 18, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("NT AUTHORITY\\SYSTEM", 5, 1, 18, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("nt authority\\system", 5, 1, 18, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("Administrators", 5, 2, 32, 544, "BUILTIN", SidTypeAlias);
    expect_name("BUILTIN\\Administrators", 5, 2, 32, 544, "BUILTIN", SidTypeAlias);
    expect_name("builtin\\administrators", 5, 2, 32, 544, "BUILTIN", SidTypeAlias);
    expect_name("Users", 5, 2, 32, 545, "BUILTIN", SidTypeAlias);
    expect_name("BUILTIN\\Users", 5, 2, 32, 545, "BUILTIN", SidTypeAlias);
    expect_name("Guests", 5, 2, 32, 546, "BUILTIN", SidTypeAlias);
    expect_name("Authenticated Users", 5, 1, 11, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("NT AUTHORITY\\Authenticated Users", 5, 1, 11, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("LOCAL SERVICE", 5, 1, 19, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("NETWORK SERVICE", 5, 1, 20, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("ANONYMOUS LOGON", 5, 1, 7, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("INTERACTIVE", 5, 1, 4, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("NETWORK", 5, 1, 2, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("CREATOR OWNER", 3, 1, 0, 0, "", SidTypeWellKnownGroup);
}
TEST(lookupname_w_wine_wellknown_spellings)
{
    REQUIRE(LNW); NOWINE();
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    expect_name("LocalService", 5, 1, 19, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("Local Service", 5, 1, 19, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("lOCAlServICE", 5, 1, 19, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("NT AUTHORITY\\LocalService", 5, 1, 19, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("NetworkService", 5, 1, 20, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("Network Service", 5, 1, 20, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
    expect_name("nt authority\\Network Service", 5, 1, 20, 0, "NT AUTHORITY", SidTypeWellKnownGroup);
}
TEST(lookupname_w_wine_bad_spellings_unmapped)
{
    static const char *bad[] = { "Local  Service", "TerminalServer User", "TerminalServerUser", "Terminal ServerUser",
        "enterprisedomain controllers", "enterprise domaincontrollers", "enterprisedomaincontrollers",
        "nt authority test\\Network Service", "Dummy\\Network Service", "ntauthority\\Network Service" };
    unsigned i;
    REQUIRE(LNW); NOWINE();
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        WCHAR w[128]; NAMERES r; widen(bad[i], w, 128); r = lookup_name(w);
        CHECKF(!r.ok, "\"%s\" mapped", bad[i]);
        if (!r.ok) CHECKF(r.err == ERROR_NONE_MAPPED || r.err == ERROR_TRUSTED_RELATIONSHIP_FAILURE || r.err == ERROR_TRUSTED_DOMAIN_FAILURE, "\"%s\" err %lu", bad[i], r.err);
    }
}
TEST(lookupname_w_wine_spaced_wellknown)
{
    REQUIRE(LNW); NOWINE();
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    { WCHAR w[] = L"Terminal Server User"; NAMERES r = lookup_name(w); CHECK(r.ok); if (r.ok) CHECK(EqualSid(r.sid, (SID *)(BYTE[]){1,1,0,0,0,0,0,5,13,0,0,0})); }
    { WCHAR w[] = L"enterprise domain controllers"; NAMERES r = lookup_name(w); CHECK(r.ok); if (r.ok) CHECK(EqualSid(r.sid, (SID *)(BYTE[]){1,1,0,0,0,0,0,5,9,0,0,0})); }
}
TEST(lookupname_w_unknown_name)
{
    static const char *bad[] = { "oogabooga", "olet_no_such_account_xyz", "BUILTIN\\NoSuchGroupXyz", "NoSuchDomainXyz\\Administrator" };
    unsigned i;
    REQUIRE(LNW); NOWINE();
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        WCHAR w[128]; NAMERES r; widen(bad[i], w, 128); r = lookup_name(w);
        CHECKF(!r.ok, "\"%s\"", bad[i]);
        if (!r.ok) {
            CHECKF(r.err == ERROR_NONE_MAPPED || r.err == ERROR_TRUSTED_RELATIONSHIP_FAILURE || r.err == ERROR_TRUSTED_DOMAIN_FAILURE, "\"%s\": err %lu", bad[i], r.err);
            EXPECT_EQ(r.use, 0xcafebabe);
        }
    }
}
TEST(lookupname_w_oogabooga_exact_none_mapped)
{
    NAMERES r;
    REQUIRE(LNW); NOWINE();
    r = lookup_name(L"oogabooga");
    CHECK(!r.ok); EXPECT_EQ(r.err, ERROR_NONE_MAPPED);
}
TEST(lookupname_w_buffer_protocol_administrators)
{
    DWORD cb = 0, cd = 0; SID_NAME_USE use = (SID_NAME_USE)0xcafebabe; BYTE sid[SECURITY_MAX_SID_SIZE]; WCHAR dom[64]; DWORD ecb, ecd;
    REQUIRE(LNW); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LNW(L"BUILTIN\\Administrators", NULL, &cb, NULL, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(use, 0xcafebabe);
    EXPECT_EQ(cb, 16);       /* S-1-5-32-544 */
    EXPECT_EQ(cd, 8);        /* "BUILTIN" + NUL */
    ecb = cb; ecd = cd;
    /* sid too small by one byte */
    cb = ecb - 1; cd = ecd; SET_ERR(0);
    EXPECT_FAIL(!LNW(L"BUILTIN\\Administrators", sid, &cb, dom, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, ecb); EXPECT_EQ(cd, ecd);
    /* domain too small by one char */
    cb = ecb; cd = ecd - 1; SET_ERR(0);
    EXPECT_FAIL(!LNW(L"BUILTIN\\Administrators", sid, &cb, dom, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, ecb); EXPECT_EQ(cd, ecd);
    /* exact */
    cb = ecb; cd = ecd; use = 0;
    CHECK(LNW(L"BUILTIN\\Administrators", sid, &cb, dom, &cd, &use));
    EXPECT_EQ(cd, 7); EXPECT_EQ(dom[7], 0); EXPECT_WSTR(dom, L"BUILTIN");
    EXPECT_EQ(use, SidTypeAlias);
    CHECK(IsValidSid(sid)); EXPECT_EQ(GetLengthSid(sid), 16);
    t_note("cbSid after success: %lu", cb);
}
TEST(lookupname_w_buffer_protocol_everyone_empty_domain)
{
    DWORD cb = 0, cd = 0; SID_NAME_USE use = 0; BYTE sid[SECURITY_MAX_SID_SIZE]; WCHAR dom[4] = { 'z', 'z', 'z', 'z' };
    REQUIRE(LNW); NOWINE();
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    SET_ERR(0);
    EXPECT_FAIL(!LNW(L"Everyone", NULL, &cb, NULL, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, 12); EXPECT_EQ(cd, 1);
    cb = 12; cd = 1;
    CHECK(LNW(L"Everyone", sid, &cb, dom, &cd, &use));
    EXPECT_EQ(cd, 0); EXPECT_EQ(dom[0], 0); EXPECT_EQ(use, SidTypeWellKnownGroup);
    cb = 12; cd = 0; SET_ERR(0);
    EXPECT_FAIL(!LNW(L"Everyone", sid, &cb, dom, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cd, 1);
}
TEST(lookupname_w_failure_leaves_sid_buffer)
{
    DWORD cb = 4, cd = 64; SID_NAME_USE use = 0; BYTE sid[4] = { 0x77, 0x77, 0x77, 0x77 }; WCHAR dom[64];
    REQUIRE(LNW); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LNW(L"BUILTIN\\Administrators", sid, &cb, dom, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sid[0], 0x77);
}
TEST(lookupname_w_computer_name_is_domain)
{
    WCHAR cn[64]; DWORD n = 64; NAMERES r;
    REQUIRE(LNW); NOWINE();
    GetComputerNameW(cn, &n);
    r = lookup_name(cn);
    if (!r.ok) { t_note("computer name lookup err %lu", r.err); return; }
    CHECKF(r.use == SidTypeDomain || r.use == SidTypeUser, "use %d", r.use);
    if (r.use == SidTypeDomain) {
        CHECK(!lstrcmpiW(r.dom, cn));
        CHECK(r.sid[0] == 1 && r.sid[7] == 5);
        EXPECT_EQ(((PISID)r.sid)->SubAuthority[0], 21);
        EXPECT_EQ(((PISID)r.sid)->SubAuthorityCount, 4);
    }
}
TEST(lookupname_w_computer_qualified_user)
{
    WCHAR cn[64], full[200], un[100]; DWORD n = 64, un_n = 100; NAMERES r;
    REQUIRE(LNW); NOWINE();
    GetComputerNameW(cn, &n);
    if (!GetUserNameW(un, &un_n)) { t_skip("GetUserName"); return; }
    wsprintfW(full, L"%s\\%s", cn, un);
    r = lookup_name(full);
    t_note("%ls -> ok=%d err=%lu use=%d", full, r.ok, r.err, r.use);
}
TEST(lookupname_w_null_name)
{
    DWORD cb = 0, cd = 0; SID_NAME_USE use = (SID_NAME_USE)0xcafebabe;
    REQUIRE(LNW); NOWINE();
    if (t_is_wine()) { t_skip("NULL name faults under wine"); return; }
    SET_ERR(0);
    if (!LNW(NULL, NULL, &cb, NULL, &cd, &use)) {
        DWORD e = GetLastError();
        t_note("NULL account name -> err %lu cb=%lu cd=%lu", e, cb, cd);
        CHECK(e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_NONE_MAPPED);
    } else t_note("NULL account name succeeded");
}
TEST(lookupname_w_roundtrip_with_sid_lookup)
{
    static const WCHAR *names[] = { L"BUILTIN\\Administrators", L"BUILTIN\\Users", L"NT AUTHORITY\\SYSTEM", L"Everyone", L"NT AUTHORITY\\Authenticated Users" };
    unsigned i;
    REQUIRE(LNW); NOWINE(); REQUIRE(LSW); NOWINE();
    if (!is_english_ui()) { t_skip("non-English UI"); return; }
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        NAMERES r = lookup_name(names[i]); SIDRES s;
        CHECKF(r.ok, "%ls err %lu", names[i], r.err);
        if (!r.ok) continue;
        s = lookup_sid((PSID)r.sid);
        CHECK(s.ok);
        if (s.ok) { EXPECT_EQ(s.use, r.use); EXPECT_WSTR(s.dom, r.dom); }
    }
}
TEST(lookupname_w_matches_advapi)
{
    NAMERES r; BYTE sid[SECURITY_MAX_SID_SIZE]; WCHAR dom[64]; DWORD cb = sizeof sid, cd = 64; SID_NAME_USE use = 0;
    REQUIRE(LNW); NOWINE(); REQUIRE(UIN(AD, LookupAccountNameW));
    r = lookup_name(L"BUILTIN\\Administrators");
    CHECK(UIN(AD, LookupAccountNameW)(NULL, L"BUILTIN\\Administrators", sid, &cb, dom, &cd, &use));
    if (r.ok) { CHECK(EqualSid(sid, r.sid)); EXPECT_WSTR(dom, r.dom); EXPECT_EQ(use, r.use); }
}
/* A */
TEST(lookupname_a_basic)
{
    BYTE sid[SECURITY_MAX_SID_SIZE]; char dom[64]; DWORD cb = sizeof sid, cd = 64; SID_NAME_USE use = 0; SIDBUF b;
    REQUIRE(LNA); NOWINE();
    CHECK(LNA("BUILTIN\\Administrators", sid, &cb, dom, &cd, &use));
    CHECK(EqualSid(sid, mk_sid(&b, 5, 2, 32, 544)));
    EXPECT_STR(dom, "BUILTIN"); EXPECT_EQ(cd, 7); EXPECT_EQ(use, SidTypeAlias);
}
TEST(lookupname_a_buffer_protocol)
{
    DWORD cb = 0, cd = 0; SID_NAME_USE use = (SID_NAME_USE)0xcafebabe;
    REQUIRE(LNA); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LNA("BUILTIN\\Administrators", NULL, &cb, NULL, &cd, &use), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, 16); EXPECT_EQ(cd, 8); EXPECT_EQ(use, 0xcafebabe);
}
TEST(lookupname_a_unknown)
{
    BYTE sid[SECURITY_MAX_SID_SIZE]; char dom[64]; DWORD cb = sizeof sid, cd = 64; SID_NAME_USE use = 0;
    REQUIRE(LNA); NOWINE();
    SET_ERR(0);
    EXPECT_FAIL(!LNA("oogabooga", sid, &cb, dom, &cd, &use), ERROR_NONE_MAPPED);
}
