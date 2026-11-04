/* Service SIDs (NT SERVICE\<name>), SID string conversion in sechost, SCM database lock APIs, and service
 * object security descriptors. */
#include "t.h"
#include <winsvc.h>
#include <sddl.h>
#include <ntstatus.h>

#define SH(n) UIN("sechost.dll", n)
#define AD(n) UIN("advapi32.dll", n)

/* ---- tiny SHA-1 (service SID = S-1-5-80 + 5 little-endian DWORDs of SHA1(UTF-16LE(upper(name)))) ---- */
typedef struct { DWORD h[5]; unsigned long long len; BYTE buf[64]; int n; } sha1_t;
#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
static void sha1_block(sha1_t *c, const BYTE *p)
{
    DWORD w[80], a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4], t;
    for (int i = 0; i < 16; i++) w[i] = (DWORD)p[4*i] << 24 | p[4*i+1] << 16 | p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 80; i++) w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    for (int i = 0; i < 80; i++) {
        DWORD f, k;
        if (i < 20) { f = (b & cc) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; }
        else { f = b ^ cc ^ d; k = 0xCA62C1D6; }
        t = ROL(a, 5) + f + e + k + w[i]; e = d; d = cc; cc = ROL(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}
static void sha1_init(sha1_t *c) { c->h[0]=0x67452301; c->h[1]=0xEFCDAB89; c->h[2]=0x98BADCFE; c->h[3]=0x10325476; c->h[4]=0xC3D2E1F0; c->len = 0; c->n = 0; }
static void sha1_add(sha1_t *c, const BYTE *p, size_t l)
{
    c->len += l;
    while (l--) { c->buf[c->n++] = *p++; if (c->n == 64) { sha1_block(c, c->buf); c->n = 0; } }
}
static void sha1_done(sha1_t *c, DWORD out[5])
{
    unsigned long long bits = c->len * 8; BYTE pad = 0x80, z = 0;
    sha1_add(c, &pad, 1);
    while (c->n != 56) sha1_add(c, &z, 1);
    BYTE lb[8]; for (int i = 0; i < 8; i++) lb[i] = (BYTE)(bits >> (56 - 8*i));
    sha1_add(c, lb, 8);
    for (int i = 0; i < 5; i++) out[i] = c->h[i];
}

/* Expected service SID built by hand: sub-authorities are the digest words as the little-endian DWORDs of the digest bytes. */
typedef struct { BYTE rev, count; BYTE auth[6]; DWORD sub[6]; } svc_sid;
static void expected_service_sid(const WCHAR *name, svc_sid *s)
{
    WCHAR up[256]; int n = lstrlenW(name);
    for (int i = 0; i < n; i++) up[i] = (WCHAR)(ULONG_PTR)CharUpperW((LPWSTR)(ULONG_PTR)name[i]);
    sha1_t c; DWORD h[5]; sha1_init(&c); sha1_add(&c, (BYTE *)up, n * 2); sha1_done(&c, h);
    s->rev = 1; s->count = 6; memset(s->auth, 0, 6); s->auth[5] = 5;
    s->sub[0] = 80;
    for (int i = 0; i < 5; i++) {   /* digest bytes big-endian words -> bytes -> read little-endian */
        DWORD w = h[i];
        s->sub[1 + i] = (w >> 24) | ((w >> 16) & 0xff) << 8 | ((w >> 8) & 0xff) << 16 | (w & 0xff) << 24;
    }
}

UDECL(NTSTATUS, RtlCreateServiceSid, (PUNICODE_STRING, PSID, PULONG));

static int create_service_sid(const WCHAR *name, svc_sid *out, ULONG *len_out, NTSTATUS *st)
{
    RtlCreateServiceSid_pfn f = (RtlCreateServiceSid_pfn)t_proc_in("ntdll.dll", "RtlCreateServiceSid");
    if (!f) return 0;
    UNICODE_STRING us; us.Buffer = (PWSTR)name; us.Length = (USHORT)(lstrlenW(name) * 2); us.MaximumLength = us.Length + 2;
    ULONG len = sizeof(svc_sid);
    memset(out, 0, sizeof *out);
    *st = f(&us, out, &len);
    if (len_out) *len_out = len;
    return 1;
}

TEST(scl_sid_sha1_selftest)
{
    sha1_t c; DWORD h[5]; sha1_init(&c); sha1_add(&c, (const BYTE *)"abc", 3); sha1_done(&c, h);
    EXPECT_EQ(h[0], 0xa9993e36); EXPECT_EQ(h[1], 0x4706816a); EXPECT_EQ(h[2], 0xba3e2571);
    EXPECT_EQ(h[3], 0x7850c26c); EXPECT_EQ(h[4], 0x9cd0d89d);
}

TEST(scl_sid_rtl_trustedinstaller_vector)
{
    svc_sid s; ULONG len; NTSTATUS st;
    if (!create_service_sid(L"TrustedInstaller", &s, &len, &st)) { t_skip("RtlCreateServiceSid missing"); return; }
    EXPECT_EQ(st, 0);
    EXPECT_EQ(len, sizeof(svc_sid));
    EXPECT_EQ(s.sub[0], 80);
    EXPECT_EQ(s.sub[1], 956008885u); EXPECT_EQ(s.sub[2], 3418522649u); EXPECT_EQ(s.sub[3], 1831038044u);
    EXPECT_EQ(s.sub[4], 1853292631u); EXPECT_EQ(s.sub[5], 2271478464u);
}

TEST(scl_sid_hand_computed_matches_rtl)
{
    static const WCHAR *names[] = {L"RpcSs", L"TrustedInstaller", L"EventLog", L"Dnscache", L"a", L"olet_made_up_service",
                                   L"With Space", L"Ünï"};
    for (int i = 0; i < 8; i++) {
        svc_sid rt, mine; ULONG len; NTSTATUS st;
        if (!create_service_sid(names[i], &rt, &len, &st)) { t_skip("RtlCreateServiceSid missing"); return; }
        expected_service_sid(names[i], &mine);
        CHECKF(st == 0, "name %d status 0x%lx", i, st);
        CHECKF(!memcmp(&rt, &mine, sizeof rt), "name %d: SID differs from SHA1 construction", i);
    }
}

TEST(scl_sid_rtl_case_insensitive)
{
    svc_sid a, b, c; ULONG len; NTSTATUS st;
    if (!create_service_sid(L"rpcss", &a, &len, &st)) { t_skip("RtlCreateServiceSid missing"); return; }
    create_service_sid(L"RPCSS", &b, &len, &st);
    create_service_sid(L"RpcSs", &c, &len, &st);
    CHECK(!memcmp(&a, &b, sizeof a));
    CHECK(!memcmp(&b, &c, sizeof b));
}

TEST(scl_sid_rtl_distinct_names_distinct_sids)
{
    svc_sid a, b; ULONG len; NTSTATUS st;
    if (!create_service_sid(L"RpcSs", &a, &len, &st)) { t_skip("RtlCreateServiceSid missing"); return; }
    create_service_sid(L"RpcEptMapper", &b, &len, &st);
    CHECK(memcmp(&a, &b, sizeof a) != 0);
    CHECK(a.sub[1] != b.sub[1] || a.sub[2] != b.sub[2]);
}

TEST(scl_sid_rtl_size_query)
{
    RtlCreateServiceSid_pfn f = (RtlCreateServiceSid_pfn)t_proc_in("ntdll.dll", "RtlCreateServiceSid");
    REQUIRE(f);
    UNICODE_STRING us; us.Buffer = L"RpcSs"; us.Length = 10; us.MaximumLength = 12;
    ULONG len = 0;
    NTSTATUS st = f(&us, NULL, &len);
    EXPECT_EQ(st, STATUS_BUFFER_TOO_SMALL);
    EXPECT_EQ(len, 32);   /* 8 byte header + 6 sub-authorities */
    len = 31;
    BYTE buf[64];
    st = f(&us, buf, &len);
    EXPECT_EQ(st, STATUS_BUFFER_TOO_SMALL);
    EXPECT_EQ(len, 32);
    len = 64;
    st = f(&us, buf, &len);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(len, 32);
}

TEST(scl_sid_rtl_structure)
{
    svc_sid s; ULONG len; NTSTATUS st;
    if (!create_service_sid(L"RpcSs", &s, &len, &st)) { t_skip("RtlCreateServiceSid missing"); return; }
    EXPECT_EQ(s.rev, 1);
    EXPECT_EQ(s.count, 6);
    BYTE nt[6] = {0, 0, 0, 0, 0, 5};
    CHECK(!memcmp(s.auth, nt, 6));
    CHECK(IsValidSid((PSID)&s));
    EXPECT_EQ(GetLengthSid((PSID)&s), 32);
}

static BOOL lookup_name(const WCHAR *acct, svc_sid *sid, DWORD *use_out, WCHAR *dom, DWORD *dom_len, DWORD *cb)
{
    UDECL(BOOL, LookupAccountNameLocalW, (LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    LookupAccountNameLocalW_pfn f = (LookupAccountNameLocalW_pfn)t_proc_in("sechost.dll", "LookupAccountNameLocalW");
    if (!f) return FALSE;
    SID_NAME_USE use = 0;
    BOOL r = f(acct, sid, cb, dom, dom_len, &use);
    *use_out = use;
    return r;
}

TEST(scl_sid_lookup_account_name_rpcss)
{
    UDECL(BOOL, LookupAccountNameLocalW, (LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    REQUIRE(UN(LookupAccountNameLocalW));
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    svc_sid sid, want; WCHAR dom[256]; DWORD dl = 256, cb = sizeof sid, use = 0;
    memset(&sid, 0, sizeof sid);
    BOOL r = lookup_name(L"NT SERVICE\\RpcSs", &sid, &use, dom, &dl, &cb);
    CHECKF(r, "err %lu", GetLastError());
    if (!r) return;
    expected_service_sid(L"RpcSs", &want);
    EXPECT_EQ(cb, 32);
    CHECK(!memcmp(&sid, &want, sizeof want));
    EXPECT_EQ(sid.count, 6);
    EXPECT_EQ(sid.sub[0], 80);
    CHECK(dl > 0 && dom[0]);
    t_note("domain \"%ls\" use %lu", dom, use);
}

TEST(scl_sid_lookup_account_name_size_protocol)
{
    UDECL(BOOL, LookupAccountNameLocalW, (LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    REQUIRE(UN(LookupAccountNameLocalW));
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    DWORD cb = 0, dl = 0, use;
    SET_ERR(0);
    BOOL r = lookup_name(L"NT SERVICE\\RpcSs", NULL, &use, NULL, &dl, &cb);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, 32);
    CHECK(dl > 0);
    svc_sid sid; WCHAR dom[256];
    DWORD cb2 = 31, dl2 = 256;
    SET_ERR(0);
    r = lookup_name(L"NT SERVICE\\RpcSs", &sid, &use, dom, &dl2, &cb2);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb2, 32);
    cb2 = 32; dl2 = dl;   /* the domain count from the probe includes the terminator */
    SET_ERR(0);
    r = lookup_name(L"NT SERVICE\\RpcSs", &sid, &use, dom, &dl2, &cb2);
    CHECKF(r, "err %lu", GetLastError());
    if (r) EXPECT_EQ(dl2, lstrlenW(dom));   /* on success the count excludes the terminator */
}

TEST(scl_sid_lookup_account_name_unknown)
{
    UDECL(BOOL, LookupAccountNameLocalW, (LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    REQUIRE(UN(LookupAccountNameLocalW));
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    svc_sid sid; WCHAR dom[256]; DWORD dl = 256, cb = sizeof sid, use;
    SET_ERR(0);
    BOOL r = lookup_name(L"olet_NoSuchAccount_zzz", &sid, &use, dom, &dl, &cb);
    EXPECT_FAIL(!r, ERROR_NONE_MAPPED);
}

TEST(scl_sid_lookup_account_name_all_services)
{
    UDECL(BOOL, LookupAccountNameLocalW, (LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    REQUIRE(UN(LookupAccountNameLocalW));
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    svc_sid sid; WCHAR dom[256]; DWORD dl = 256, cb = sizeof sid, use = 0;
    memset(&sid, 0, sizeof sid);
    BOOL r = lookup_name(L"NT SERVICE\\ALL SERVICES", &sid, &use, dom, &dl, &cb);
    if (!r) { t_note("ALL SERVICES lookup failed: %lu", GetLastError()); return; }
    EXPECT_EQ(sid.count, 2);
    EXPECT_EQ(sid.sub[0], 80);
    EXPECT_EQ(sid.sub[1], 0);   /* S-1-5-80-0 */
}

TEST(scl_sid_lookup_account_sid_roundtrip)
{
    UDECL(BOOL, LookupAccountSidLocalW, (PSID, LPWSTR, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    LookupAccountSidLocalW_pfn f = (LookupAccountSidLocalW_pfn)t_proc_in("sechost.dll", "LookupAccountSidLocalW");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    svc_sid sid; expected_service_sid(L"RpcSs", &sid);
    WCHAR name[256], dom[256]; DWORD nl = 256, dl = 256; SID_NAME_USE use = 0;
    BOOL r = f((PSID)&sid, name, &nl, dom, &dl, &use);
    CHECKF(r, "err %lu", GetLastError());
    if (!r) return;
    CHECK(!lstrcmpiW(name, L"RpcSs"));
    EXPECT_EQ(nl, lstrlenW(name));
    CHECK(dom[0] != 0);
    t_note("S-1-5-80-... -> %ls\\%ls use %d", dom, name, (int)use);
}

TEST(scl_sid_lookup_account_sid_size_protocol)
{
    UDECL(BOOL, LookupAccountSidLocalW, (PSID, LPWSTR, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    LookupAccountSidLocalW_pfn f = (LookupAccountSidLocalW_pfn)t_proc_in("sechost.dll", "LookupAccountSidLocalW");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    svc_sid sid; expected_service_sid(L"RpcSs", &sid);
    DWORD nl = 0, dl = 0; SID_NAME_USE use;
    SET_ERR(0);
    BOOL r = f((PSID)&sid, NULL, &nl, NULL, &dl, &use);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(nl, 6);    /* "RpcSs" + terminator */
    CHECK(dl >= 2);
}

TEST(scl_sid_lookup_account_sid_well_known)
{
    UDECL(BOOL, LookupAccountSidLocalW, (PSID, LPWSTR, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE));
    LookupAccountSidLocalW_pfn f = (LookupAccountSidLocalW_pfn)t_proc_in("sechost.dll", "LookupAccountSidLocalW");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine lacks service-SID account lookup"); return; }
    PSID sys = NULL;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    CHECK(AllocateAndInitializeSid(&nt, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &sys));
    if (!sys) return;
    WCHAR name[64], dom[64]; DWORD nl = 64, dl = 64; SID_NAME_USE use = 0;
    BOOL r = f(sys, name, &nl, dom, &dl, &use);
    CHECK(r);
    if (r) { t_note("S-1-5-18 -> %ls\\%ls use %d", dom, name, (int)use); EXPECT_EQ(use, SidTypeWellKnownGroup); }
    FreeSid(sys);
}

/* ---- ConvertSidToStringSidW / ConvertStringSidToSidW (sechost implementations) ---- */
TEST(scl_sidstr_service_sid_format)
{
    REQUIRE(SH(ConvertSidToStringSidW));
    svc_sid sid; expected_service_sid(L"RpcSs", &sid);
    LPWSTR s = NULL;
    SET_ERR(0);
    BOOL r = SH(ConvertSidToStringSidW)((PSID)&sid, &s);
    CHECK(r);
    if (!r) return;
    CHECK(!wcsncmp(s, L"S-1-5-80-", 9));
    int dashes = 0; for (WCHAR *p = s; *p; p++) if (*p == L'-') dashes++;
    EXPECT_EQ(dashes, 8);   /* S,1,5 + 6 sub-authorities -> 8 separators */
    WCHAR want[128];
    swprintf(want, 128, L"S-1-5-80-%lu-%lu-%lu-%lu-%lu", sid.sub[1], sid.sub[2], sid.sub[3], sid.sub[4], sid.sub[5]);
    EXPECT_WSTR(s, want);
    LocalFree(s);
}

TEST(scl_sidstr_trustedinstaller_string)
{
    REQUIRE(SH(ConvertSidToStringSidW));
    svc_sid sid; ULONG len; NTSTATUS st;
    if (!create_service_sid(L"TrustedInstaller", &sid, &len, &st)) { t_skip("RtlCreateServiceSid missing"); return; }
    LPWSTR s = NULL;
    CHECK(SH(ConvertSidToStringSidW)((PSID)&sid, &s));
    if (s) { EXPECT_WSTR(s, L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464"); LocalFree(s); }
}

TEST(scl_sidstr_roundtrip_service_sid)
{
    REQUIRE(SH(ConvertSidToStringSidW)); REQUIRE(SH(ConvertStringSidToSidW));
    svc_sid sid; expected_service_sid(L"EventLog", &sid);
    LPWSTR s = NULL; PSID back = NULL;
    CHECK(SH(ConvertSidToStringSidW)((PSID)&sid, &s));
    if (!s) return;
    CHECK(SH(ConvertStringSidToSidW)(s, &back));
    if (back) { CHECK(EqualSid(back, (PSID)&sid)); EXPECT_EQ(GetLengthSid(back), 32); LocalFree(back); }
    LocalFree(s);
}

TEST(scl_sidstr_well_known_strings)
{
    REQUIRE(SH(ConvertStringSidToSidW));
    static const struct { const WCHAR *in; BYTE count; DWORD sub0, sub1; } tab[] = {
        {L"S-1-5-18", 1, 18, 0}, {L"S-1-5-32-544", 2, 32, 544}, {L"S-1-1-0", 1, 0, 0}, {L"S-1-5-80-0", 2, 80, 0},
        {L"SY", 1, 18, 0}, {L"BA", 2, 32, 544}, {L"WD", 1, 0, 0}, {L"BU", 2, 32, 545}, {L"AU", 1, 11, 0},
        {L"LS", 1, 19, 0}, {L"NS", 1, 20, 0}, {L"IU", 1, 4, 0}, {L"SU", 1, 6, 0}, {L"S-1-5-21-1-2-3-500", 5, 21, 1},
    };
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        PSID sid = NULL;
        SET_ERR(0);
        BOOL r = SH(ConvertStringSidToSidW)(tab[i].in, &sid);
        CHECKF(r, "%ls err %lu", tab[i].in, GetLastError());
        if (!r) continue;
        CHECKF(*GetSidSubAuthorityCount(sid) == tab[i].count, "%ls count", tab[i].in);
        CHECKF(*GetSidSubAuthority(sid, 0) == tab[i].sub0, "%ls sub0", tab[i].in);
        if (tab[i].count > 1) CHECKF(*GetSidSubAuthority(sid, 1) == tab[i].sub1, "%ls sub1", tab[i].in);
        LocalFree(sid);
    }
}

TEST(scl_sidstr_invalid_inputs)
{
    REQUIRE(SH(ConvertStringSidToSidW));
    PSID sid = NULL;
    SET_ERR(0);
    BOOL r = SH(ConvertStringSidToSidW)(NULL, &sid);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = SH(ConvertStringSidToSidW)(L"S-1-5-18", NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
    static const WCHAR *bad[] = {L"", L"garbage", L"S-", L"S-1", L"S-1-5-xyz"};
    for (int i = 0; i < 5; i++) {
        sid = NULL;
        SET_ERR(0);
        r = SH(ConvertStringSidToSidW)(bad[i], &sid);
        CHECKF(!r, "\"%ls\" parsed", bad[i]);
        if (!r) t_note("ConvertStringSidToSidW(\"%ls\") err %lu", bad[i], GetLastError());
        if (r && sid) LocalFree(sid);
    }
    SET_ERR(0);
    r = SH(ConvertStringSidToSidW)(L"garbage", &sid);
    EXPECT_FAIL(!r, ERROR_INVALID_SID);
}

TEST(scl_sidstr_to_string_null_args)
{
    REQUIRE(SH(ConvertSidToStringSidW));
    if (t_is_wine()) { t_skip("wine crashes on NULL"); return; }
    LPWSTR s = NULL;
    svc_sid sid; expected_service_sid(L"RpcSs", &sid);
    SET_ERR(0);
    BOOL r = SH(ConvertSidToStringSidW)(NULL, &s);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
    CHECK(s == NULL);
    SET_ERR(0);
    r = SH(ConvertSidToStringSidW)((PSID)&sid, NULL);
    EXPECT_FAIL(!r, ERROR_INVALID_PARAMETER);
}

TEST(scl_sidstr_everyone_and_system)
{
    REQUIRE(SH(ConvertSidToStringSidW));
    SID_IDENTIFIER_AUTHORITY world = SECURITY_WORLD_SID_AUTHORITY, nt = SECURITY_NT_AUTHORITY;
    PSID ev = NULL, ba = NULL;
    AllocateAndInitializeSid(&world, 1, 0, 0, 0, 0, 0, 0, 0, 0, &ev);
    AllocateAndInitializeSid(&nt, 2, 32, 544, 0, 0, 0, 0, 0, 0, &ba);
    LPWSTR s = NULL;
    if (ev && SH(ConvertSidToStringSidW)(ev, &s)) { EXPECT_WSTR(s, L"S-1-1-0"); LocalFree(s); } else CHECK(0);
    s = NULL;
    if (ba && SH(ConvertSidToStringSidW)(ba, &s)) { EXPECT_WSTR(s, L"S-1-5-32-544"); LocalFree(s); } else CHECK(0);
    if (ev) FreeSid(ev);
    if (ba) FreeSid(ba);
}

/* ---- SCM database lock -------------------------------------------------------- */
static SC_HANDLE scm_open(DWORD acc) { return SH(OpenSCManagerW) ? SH(OpenSCManagerW)(NULL, NULL, acc) : NULL; }

TEST(scl_lock_requires_lock_right)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(LockServiceDatabase)); REQUIRE(SH(OpenSCManagerW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SET_ERR(0);
    SC_LOCK lk = AD(LockServiceDatabase)(scm);
    EXPECT_FAIL(lk == NULL, ERROR_ACCESS_DENIED);
    if (lk) AD(UnlockServiceDatabase)(lk);
    SH(CloseServiceHandle)(scm);
}

TEST(scl_lock_null_handle)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(LockServiceDatabase));
    SET_ERR(0);
    SC_LOCK lk = AD(LockServiceDatabase)(NULL);
    CHECK(lk == NULL);
    t_note("LockServiceDatabase(NULL) err %lu", GetLastError());
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (lk) AD(UnlockServiceDatabase)(lk);
}

TEST(scl_lock_service_handle_wrong_type)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(LockServiceDatabase)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    if (!svc) { SH(CloseServiceHandle)(scm); t_skip("no RpcSs"); return; }
    SET_ERR(0);
    SC_LOCK lk = AD(LockServiceDatabase)(svc);
    EXPECT_FAIL(lk == NULL, ERROR_INVALID_HANDLE);
    if (lk) AD(UnlockServiceDatabase)(lk);
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_unlock_null_lock)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(UnlockServiceDatabase));
    SET_ERR(0);
    BOOL r = AD(UnlockServiceDatabase)(NULL);
    CHECK(!r);
    t_note("UnlockServiceDatabase(NULL) err %lu", GetLastError());
    DWORD e = GetLastError();
    CHECK(e == ERROR_INVALID_SERVICE_LOCK || e == ERROR_INVALID_HANDLE);
}

TEST(scl_lock_lifecycle)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(LockServiceDatabase)); REQUIRE(AD(UnlockServiceDatabase)); REQUIRE(AD(QueryServiceLockStatusW));
    SC_HANDLE a = scm_open(SC_MANAGER_LOCK | SC_MANAGER_QUERY_LOCK_STATUS | SC_MANAGER_CONNECT);
    if (!a) { t_skip("SC_MANAGER_LOCK denied (%lu); needs admin", GetLastError()); return; }
    SC_HANDLE b = scm_open(SC_MANAGER_LOCK | SC_MANAGER_QUERY_LOCK_STATUS | SC_MANAGER_CONNECT);
    SC_LOCK lk = NULL;
    SET_ERR(0);
    lk = AD(LockServiceDatabase)(a);
    CHECKF(lk != NULL, "err %lu", GetLastError());
    if (lk) {
        BYTE buf[1024]; DWORD need = 0;
        QUERY_SERVICE_LOCK_STATUSW *ls = (void *)buf;
        memset(buf, 0, sizeof buf);
        BOOL r = AD(QueryServiceLockStatusW)(b ? b : a, ls, sizeof buf, &need);
        CHECK(r);
        if (r) {
            EXPECT_EQ(ls->fIsLocked, 1);
            CHECK(ls->lpLockOwner != NULL);
            if (ls->lpLockOwner) t_note("lock owner \"%ls\" duration %lu", ls->lpLockOwner, ls->dwLockDuration);
            CHECK(ls->dwLockDuration < 60);
        }
        if (b) {
            SET_ERR(0);
            SC_LOCK lk2 = AD(LockServiceDatabase)(b);
            EXPECT_FAIL(lk2 == NULL, ERROR_SERVICE_DATABASE_LOCKED);
            if (lk2) AD(UnlockServiceDatabase)(lk2);
        }
        r = AD(UnlockServiceDatabase)(lk);
        CHECK(r);
        memset(buf, 0, sizeof buf);
        r = AD(QueryServiceLockStatusW)(a, ls, sizeof buf, &need);
        CHECK(r);
        if (r) { EXPECT_EQ(ls->fIsLocked, 0); CHECK(ls->lpLockOwner != NULL); }
        SET_ERR(0);
        r = AD(UnlockServiceDatabase)(lk);   /* second unlock of the same lock */
        CHECK(!r);
        t_note("second UnlockServiceDatabase err %lu", GetLastError());
        EXPECT_ERR(ERROR_INVALID_SERVICE_LOCK);
    }
    if (b) SH(CloseServiceHandle)(b);
    SH(CloseServiceHandle)(a);
}

TEST(scl_lock_status_size_protocol)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(QueryServiceLockStatusW)); REQUIRE(SH(OpenSCManagerW));
    SC_HANDLE scm = scm_open(SC_MANAGER_QUERY_LOCK_STATUS);
    if (!scm) { t_skip("no QUERY_LOCK_STATUS: %lu", GetLastError()); return; }
    DWORD need = 0;
    SET_ERR(0);
    BOOL r = AD(QueryServiceLockStatusW)(scm, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(QUERY_SERVICE_LOCK_STATUSW));
    DWORD first = need;
    BYTE small[8]; DWORD need2 = 0;
    SET_ERR(0);
    r = AD(QueryServiceLockStatusW)(scm, (void *)small, sizeof small, &need2);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need2, first);
    BYTE *buf = calloc(1, first + 16);
    r = AD(QueryServiceLockStatusW)(scm, (void *)buf, first, &need2);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        QUERY_SERVICE_LOCK_STATUSW *ls = (void *)buf;
        CHECK(ls->fIsLocked == 0 || ls->fIsLocked == 1);
        CHECK((BYTE *)ls->lpLockOwner >= buf && (BYTE *)ls->lpLockOwner < buf + first);
    }
    free(buf);
    SH(CloseServiceHandle)(scm);
}

TEST(scl_lock_status_needs_query_right)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(QueryServiceLockStatusW)); REQUIRE(SH(OpenSCManagerW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    BYTE buf[512]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = AD(QueryServiceLockStatusW)(scm, (void *)buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = AD(QueryServiceLockStatusW)(scm, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SH(CloseServiceHandle)(scm);
}

TEST(scl_lock_status_a_matches_w)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(QueryServiceLockStatusA)); REQUIRE(AD(QueryServiceLockStatusW));
    SC_HANDLE scm = scm_open(SC_MANAGER_QUERY_LOCK_STATUS);
    if (!scm) { t_skip("no QUERY_LOCK_STATUS"); return; }
    BYTE bw[1024], ba[1024]; DWORD n;
    BOOL rw = AD(QueryServiceLockStatusW)(scm, (void *)bw, sizeof bw, &n);
    BOOL ra = AD(QueryServiceLockStatusA)(scm, (void *)ba, sizeof ba, &n);
    CHECK(rw && ra);
    if (rw && ra) {
        QUERY_SERVICE_LOCK_STATUSW *w = (void *)bw; QUERY_SERVICE_LOCK_STATUSA *a = (void *)ba;
        EXPECT_EQ(w->fIsLocked, a->fIsLocked);
        CHECK(a->lpLockOwner != NULL);
    }
    SH(CloseServiceHandle)(scm);
}

TEST(scl_lock_status_wrong_handle_type)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(AD(QueryServiceLockStatusW)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    if (!svc) { SH(CloseServiceHandle)(scm); t_skip("no RpcSs"); return; }
    BYTE buf[512]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = AD(QueryServiceLockStatusW)(svc, (void *)buf, sizeof buf, &need);
    EXPECT_FAIL(!r, ERROR_INVALID_HANDLE);
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

/* ---- security descriptors ------------------------------------------------------ */
static int find_ace(PACL acl, PSID sid, ACCESS_ALLOWED_ACE **out)
{
    for (DWORD i = 0; i < acl->AceCount; i++) {
        ACE_HEADER *h;
        if (!GetAce(acl, i, (void **)&h)) continue;
        if (h->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
        ACCESS_ALLOWED_ACE *a = (void *)h;
        if (EqualSid((PSID)&a->SidStart, sid)) { if (out) *out = a; return 1; }
    }
    return 0;
}

static PSID make_sid(DWORD a, DWORD b, int count)
{
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY; PSID s = NULL;
    AllocateAndInitializeSid(&nt, count, a, b, 0, 0, 0, 0, 0, 0, &s);
    return s;
}

static BYTE *query_sd(SC_HANDLE h, SECURITY_INFORMATION info, DWORD *len)
{
    DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceObjectSecurity)(h, info, NULL, 0, &need);
    if (r || GetLastError() != ERROR_INSUFFICIENT_BUFFER || !need) return NULL;
    BYTE *sd = calloc(1, need);
    DWORD need2 = 0;
    if (!SH(QueryServiceObjectSecurity)(h, info, sd, need, &need2)) { free(sd); return NULL; }
    *len = need;
    return sd;
}

static void check_basic_dacl(BYTE *sd, const char *what)
{
    CHECKF(IsValidSecurityDescriptor(sd), "%s valid SD", what);
    SECURITY_DESCRIPTOR_CONTROL ctl; DWORD rev;
    CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev));
    CHECKF(ctl & SE_SELF_RELATIVE, "%s self-relative", what);
    CHECKF(ctl & SE_DACL_PRESENT, "%s dacl present", what);
    BOOL present = FALSE, def = FALSE; PACL dacl = NULL;
    CHECK(GetSecurityDescriptorDacl(sd, &present, &dacl, &def));
    CHECK(present && dacl);
    if (!(present && dacl)) return;
    PSID sy = make_sid(SECURITY_LOCAL_SYSTEM_RID, 0, 1), ba = make_sid(SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 2);
    ACCESS_ALLOWED_ACE *a = NULL;
    CHECKF(find_ace(dacl, sy, &a), "%s: SYSTEM allow ACE", what);
    if (a) t_note("%s SYSTEM mask 0x%lx", what, a->Mask);
    a = NULL;
    CHECKF(find_ace(dacl, ba, &a), "%s: Administrators allow ACE", what);
    if (a) t_note("%s Administrators mask 0x%lx", what, a->Mask);
    FreeSid(sy); FreeSid(ba);
}

TEST(scl_sd_service_dacl_has_system_and_admins)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", READ_CONTROL);
    if (!svc) { t_skip("READ_CONTROL on RpcSs: %lu", GetLastError()); SH(CloseServiceHandle)(scm); return; }
    DWORD len = 0;
    BYTE *sd = query_sd(svc, DACL_SECURITY_INFORMATION, &len);
    CHECK(sd != NULL);
    if (sd) { check_basic_dacl(sd, "RpcSs"); CHECK(len >= sizeof(SECURITY_DESCRIPTOR_RELATIVE)); free(sd); }
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_scm_dacl_has_system_and_admins)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity));
    SC_HANDLE scm = scm_open(READ_CONTROL);
    if (!scm) { t_skip("READ_CONTROL on SCM: %lu", GetLastError()); return; }
    DWORD len = 0;
    BYTE *sd = query_sd(scm, DACL_SECURITY_INFORMATION, &len);
    CHECK(sd != NULL);
    if (sd) { check_basic_dacl(sd, "SCM"); free(sd); }
    SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_query_size_protocol)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", READ_CONTROL);
    if (!svc) { t_skip("no READ_CONTROL"); SH(CloseServiceHandle)(scm); return; }
    DWORD need = 0;
    SET_ERR(0);
    BOOL r = SH(QueryServiceObjectSecurity)(svc, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(SECURITY_DESCRIPTOR_RELATIVE) + sizeof(ACL));
    DWORD first = need;
    BYTE *sd = calloc(1, first + 16); DWORD need2 = 0;
    SET_ERR(0);
    r = SH(QueryServiceObjectSecurity)(svc, DACL_SECURITY_INFORMATION, sd, first - 1, &need2);
    EXPECT_FAIL(!r, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need2, first);
    r = SH(QueryServiceObjectSecurity)(svc, DACL_SECURITY_INFORMATION, sd, first, &need2);
    CHECK(r);
    /* owner + group + dacl is larger than dacl alone */
    DWORD nfull = 0;
    SH(QueryServiceObjectSecurity)(svc, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &nfull);
    CHECK(nfull > first);
    free(sd);
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_query_owner_group)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", READ_CONTROL);
    if (!svc) { t_skip("no READ_CONTROL"); SH(CloseServiceHandle)(scm); return; }
    DWORD len = 0;
    BYTE *sd = query_sd(svc, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, &len);
    CHECK(sd != NULL);
    if (sd) {
        PSID o = NULL, g = NULL; BOOL def;
        CHECK(GetSecurityDescriptorOwner(sd, &o, &def));
        CHECK(GetSecurityDescriptorGroup(sd, &g, &def));
        CHECK(o != NULL); CHECK(g != NULL);
        BOOL dp = TRUE; PACL dacl = NULL;
        GetSecurityDescriptorDacl(sd, &dp, &dacl, &def);
        CHECK(!dp);   /* DACL was not requested */
        LPWSTR so = NULL;
        if (o && SH(ConvertSidToStringSidW)(o, &so)) { t_note("RpcSs owner %ls", so); CHECK(!wcscmp(so, L"S-1-5-18") || !wcscmp(so, L"S-1-5-32-544")); LocalFree(so); }
        free(sd);
    }
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_query_requires_read_control)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    if (!svc) { t_skip("no RpcSs"); SH(CloseServiceHandle)(scm); return; }
    DWORD need = 0; BYTE b[512];
    SET_ERR(0);
    BOOL r = SH(QueryServiceObjectSecurity)(svc, DACL_SECURITY_INFORMATION, b, sizeof b, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = SH(QueryServiceObjectSecurity)(svc, OWNER_SECURITY_INFORMATION, NULL, 0, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_query_sacl_needs_system_security)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", READ_CONTROL);
    if (!svc) { t_skip("no READ_CONTROL"); SH(CloseServiceHandle)(scm); return; }
    DWORD need = 0; BYTE b[1024];
    SET_ERR(0);
    BOOL r = SH(QueryServiceObjectSecurity)(svc, SACL_SECURITY_INFORMATION, b, sizeof b, &need);
    EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_set_denied_without_write_dac)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(SetServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", READ_CONTROL | SERVICE_QUERY_STATUS);
    if (!svc) { t_skip("no READ_CONTROL"); SH(CloseServiceHandle)(scm); return; }
    DWORD len = 0;
    BYTE *sd = query_sd(svc, DACL_SECURITY_INFORMATION, &len);
    CHECK(sd != NULL);
    if (sd) {
        /* Re-apply the descriptor read from the service itself: a no-op even if the call were to succeed. */
        SET_ERR(0);
        BOOL r = SH(SetServiceObjectSecurity)(svc, DACL_SECURITY_INFORMATION, sd);
        EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
        if (r) t_note("SetServiceObjectSecurity unexpectedly succeeded (re-applied identical DACL)");
        SET_ERR(0);
        r = SH(SetServiceObjectSecurity)(svc, OWNER_SECURITY_INFORMATION, sd);
        CHECK(!r);
        t_note("SetServiceObjectSecurity(owner) without WRITE_OWNER err %lu", GetLastError());
        free(sd);
    }
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_set_denied_on_scm_for_users)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity)); REQUIRE(SH(SetServiceObjectSecurity));
    SC_HANDLE scm = scm_open(READ_CONTROL);
    if (!scm) { t_skip("READ_CONTROL on SCM: %lu", GetLastError()); return; }
    DWORD len = 0;
    BYTE *sd = query_sd(scm, DACL_SECURITY_INFORMATION, &len);
    CHECK(sd != NULL);
    if (sd) {
        SET_ERR(0);
        BOOL r = SH(SetServiceObjectSecurity)(scm, DACL_SECURITY_INFORMATION, sd);
        EXPECT_FAIL(!r, ERROR_ACCESS_DENIED);
        free(sd);
    }
    SH(CloseServiceHandle)(scm);
}

TEST(scl_sd_set_null_descriptor)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(SetServiceObjectSecurity)); REQUIRE(SH(OpenServiceW));
    SC_HANDLE scm = scm_open(SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    SC_HANDLE svc = SH(OpenServiceW)(scm, L"RpcSs", READ_CONTROL);
    if (!svc) { t_skip("no READ_CONTROL"); SH(CloseServiceHandle)(scm); return; }
    /* Win10 sechost dereferences the NULL descriptor (access violation) instead of failing, so the call is not made. */
    t_note("SetServiceObjectSecurity(NULL sd) faults on Win10 (0xc0000005); not called");
    SH(CloseServiceHandle)(svc); SH(CloseServiceHandle)(scm);
    t_skip("Win10 faults on a NULL security descriptor");
}

TEST(scl_sd_wrong_handle_type_and_null)
{
    if (t_is_wine()) { t_skip("wine service/SCM layer is stubbed"); return; }
    REQUIRE(SH(QueryServiceObjectSecurity));
    DWORD need = 0; BYTE b[256];
    SET_ERR(0);
    BOOL r = SH(QueryServiceObjectSecurity)(NULL, DACL_SECURITY_INFORMATION, b, sizeof b, &need);
    CHECK(!r);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
