/* dx_gap_02: LsaAddAccountRights / LsaEnumerateAccountRights / LsaEnumerateAccountsWithUserRight against a real LSA database.
 *
 * STATE CHANGING (disposable VM only): every test creates a throw-away local user "olet<pid>x<n>", grants it account rights,
 * and in a finally-style tail removes the rights (LsaRemoveAccountRights, AllRights=TRUE) and deletes the user. A sweep at the
 * start of each test deletes stale users with the same prefix. Gated on privilege: when LsaOpenPolicy(CREATE_ACCOUNT) or
 * NetUserAdd is denied, the test is skipped.
 *
 * Documented behaviour (ntsecapi.h docs for the three functions):
 *   LsaAddAccountRights: creates the account when it does not exist (needs POLICY_CREATE_ACCOUNT then), rights already held are
 *       ignored, an invalid privilege name -> STATUS_NO_SUCH_PRIVILEGE.
 *   LsaEnumerateAccountRights: fails when the account holds no rights; result freed with LsaFreeMemory.
 *   LsaEnumerateAccountsWithUserRight: NULL UserRight enumerates every account; needs POLICY_LOOKUP_NAMES|POLICY_VIEW_LOCAL_INFORMATION. */
#include "t.h"
#include <lm.h>
#include <ntsecapi.h>

/* The shared EXPECT_EQ evaluates its arguments twice; every argument here is a call with side effects (LsaFreeMemory,
 * LsaRemoveAccountRights, LsaEnumerateAccountRights), so use a single-evaluation form. */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)

#define SH "sechost.dll"
#define LADD   UIN(SH, LsaAddAccountRights)
#define LREM   UIN(SH, LsaRemoveAccountRights)
#define LENUMR UIN(SH, LsaEnumerateAccountRights)
#define LENUMA UIN(SH, LsaEnumerateAccountsWithUserRight)
#define LOPEN  UIN(SH, LsaOpenPolicy)
#define LCLOSE UIN(SH, LsaClose)
#define LFREE  UIN(SH, LsaFreeMemory)
#define ST_NO_SUCH_PRIVILEGE ((NTSTATUS)0xC0000060)
#define ST_ACCESS_DENIED     ((NTSTATUS)0xC0000022)
#define NOWINE() do { if (t_is_wine()) { t_skip("sechost Lsa* wrappers / NetUserAdd are stubs under wine"); return; } } while (0)

static void ustr(LSA_UNICODE_STRING *u, const WCHAR *s) { u->Buffer = (PWSTR)s; u->Length = (USHORT)(wcslen(s) * 2); u->MaximumLength = u->Length + 2; }

typedef struct { WCHAR name[40]; BYTE sidbuf[SECURITY_MAX_SID_SIZE]; PSID sid; int created; LSA_HANDLE pol; } fixture;
static int g_ctr;

static void del_user(const WCHAR *n)
{
    NET_API_STATUS (NET_API_FUNCTION *nd)(LPCWSTR, LPCWSTR, DWORD *) = (void *)t_proc_in("netapi32.dll", "NetUserDel");
    if (nd) nd(NULL, n, NULL);
}
/* sweeps stale users with the prefix, creates the user and resolves its SID, opens the policy handle */
static int fx_open(fixture *f, ACCESS_MASK am)
{
    NET_API_STATUS (NET_API_FUNCTION *na)(LPCWSTR, DWORD, LPBYTE, LPDWORD) = (void *)t_proc_in("netapi32.dll", "NetUserAdd");
    BOOL (WINAPI *lk)(LPCWSTR, LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE) = (void *)t_proc_in("advapi32.dll", "LookupAccountNameW");
    USER_INFO_1 ui; NET_API_STATUS ns; DWORD sl = sizeof f->sidbuf, dl = 128; WCHAR dom[128]; SID_NAME_USE use; NTSTATUS st;
    LSA_OBJECT_ATTRIBUTES oa;
    memset(f, 0, sizeof *f);
    if (!na || !lk || !LOPEN || !LADD || !LENUMR || !LENUMA || !LREM || !LFREE || !LCLOSE) { t_skip("missing exports"); return 0; }
    wsprintfW(f->name, L"olet%lux%d", GetCurrentProcessId(), ++g_ctr);
    del_user(f->name);
    memset(&ui, 0, sizeof ui);
    ui.usri1_name = f->name; ui.usri1_password = L"Olet#Gap2-Pw-9z!"; ui.usri1_priv = USER_PRIV_USER;
    ui.usri1_flags = UF_SCRIPT | UF_DONT_EXPIRE_PASSWD;
    ns = na(NULL, 1, (LPBYTE)&ui, NULL);
    if (ns != NERR_Success) { t_skip("NetUserAdd failed %lu (not privileged?)", ns); return 0; }
    f->created = 1;
    f->sid = f->sidbuf;
    if (!lk(NULL, f->name, f->sid, &sl, dom, &dl, &use)) { t_skip("LookupAccountNameW failed %lu", GetLastError()); return 0; }
    memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    st = LOPEN(NULL, &oa, am, &f->pol);
    if (st != 0) { t_skip("LsaOpenPolicy(0x%lx) -> 0x%lx", am, st); f->pol = NULL; return 0; }
    return 1;
}
static void fx_close(fixture *f)
{
    if (f->pol && f->created && f->sid) {
        LSA_HANDLE h = NULL; LSA_OBJECT_ATTRIBUTES oa; memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
        if (LOPEN(NULL, &oa, POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT, &h) == 0 && h) { LREM(h, f->sid, TRUE, NULL, 0); LCLOSE(h); }
    }
    if (f->pol) LCLOSE(f->pol);
    if (f->created) del_user(f->name);
    memset(f, 0, sizeof *f);
}
static int has_right(PLSA_UNICODE_STRING r, ULONG n, const WCHAR *name)
{
    ULONG i;
    for (i = 0; i < n; i++)
        if (r[i].Length == wcslen(name) * 2 && !_wcsnicmp(r[i].Buffer, name, r[i].Length / 2)) return 1;
    return 0;
}
#define FULL (POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES | POLICY_VIEW_LOCAL_INFORMATION)

TEST(dxgap2_lsa_add_creates_account_and_enumerates)
{
    fixture f; LSA_UNICODE_STRING r[2]; PLSA_UNICODE_STRING out = NULL; ULONG cnt = 0; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    /* a user that was never granted anything has no account object: nothing to enumerate */
    st = LENUMR(f.pol, f.sid, &out, &cnt);
    CHECKF(st != 0, "fresh user enumerates 0x%lx", st);
    t_note("LsaEnumerateAccountRights(fresh user) -> 0x%lx", st);
    CHECKF(out == NULL && cnt == 0, "outputs zeroed on failure (%p, %lu)", out, cnt);
    ustr(&r[0], L"SeBatchLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, r, 1), 0);
    out = NULL; cnt = 0;
    EXPECT_EQ(LENUMR(f.pol, f.sid, &out, &cnt), 0);
    EXPECT_EQ(cnt, 1);
    CHECK(out && has_right(out, cnt, L"SeBatchLogonRight"));
    if (out) EXPECT_EQ(LFREE(out), 0);
    /* two more in one call */
    ustr(&r[0], L"SeServiceLogonRight"); ustr(&r[1], L"SeDenyInteractiveLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, r, 2), 0);
    out = NULL; cnt = 0;
    EXPECT_EQ(LENUMR(f.pol, f.sid, &out, &cnt), 0);
    EXPECT_EQ(cnt, 3);
    CHECK(out && has_right(out, cnt, L"SeBatchLogonRight") && has_right(out, cnt, L"SeServiceLogonRight") && has_right(out, cnt, L"SeDenyInteractiveLogonRight"));
    if (out) LFREE(out);
    fx_close(&f);
}
TEST(dxgap2_lsa_add_ignores_rights_already_held)
{
    fixture f; LSA_UNICODE_STRING r[3]; PLSA_UNICODE_STRING out = NULL; ULONG cnt = 0;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r[0], L"SeBatchLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, r, 1), 0);
    EXPECT_EQ(LADD(f.pol, f.sid, r, 1), 0);
    ustr(&r[1], L"SeBatchLogonRight"); ustr(&r[2], L"SeServiceLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, r, 3), 0);                  /* duplicates inside one call too */
    EXPECT_EQ(LENUMR(f.pol, f.sid, &out, &cnt), 0);
    EXPECT_EQ(cnt, 2);
    if (out) LFREE(out);
    fx_close(&f);
}
TEST(dxgap2_lsa_add_invalid_privilege_name)
{
    fixture f; LSA_UNICODE_STRING r[2]; PLSA_UNICODE_STRING out = NULL; ULONG cnt = 0; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r[0], L"olet_gap2_NotARight");
    EXPECT_EQ(LADD(f.pol, f.sid, r, 1), ST_NO_SUCH_PRIVILEGE);
    ustr(&r[0], L"SeBatchLogonRight"); ustr(&r[1], L"olet_gap2_NotARight");
    st = LADD(f.pol, f.sid, r, 2);
    EXPECT_EQ(st, ST_NO_SUCH_PRIVILEGE);
    st = LENUMR(f.pol, f.sid, &out, &cnt);
    t_note("after a mixed valid/invalid add: enumerate 0x%lx count %lu (is the valid right applied?)", st, cnt);
    if (out) LFREE(out);
    fx_close(&f);
}
TEST(dxgap2_lsa_add_creating_account_needs_create_access)
{
    fixture f; LSA_UNICODE_STRING r; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, POLICY_LOOKUP_NAMES)) { fx_close(&f); return; }
    ustr(&r, L"SeBatchLogonRight");
    st = LADD(f.pol, f.sid, &r, 1);
    EXPECT_EQ(st, ST_ACCESS_DENIED);            /* the account does not exist: POLICY_CREATE_ACCOUNT would be required */
    fx_close(&f);
}
TEST(dxgap2_lsa_enum_accounts_with_user_right)
{
    fixture f; LSA_UNICODE_STRING r; PVOID buf = NULL; ULONG cnt = 0, i; int found = 0; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r, L"SeBatchLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, &r, 1), 0);
    st = LENUMA(f.pol, &r, &buf, &cnt);
    EXPECT_EQ(st, 0);
    if (st == 0) {
        PLSA_ENUMERATION_INFORMATION e = buf;
        CHECK(cnt >= 1 && e != NULL);
        for (i = 0; e && i < cnt; i++) if (e[i].Sid && EqualSid(e[i].Sid, f.sid)) found = 1;
        CHECK(found);
        LFREE(buf);
    }
    /* a right only this user holds */
    ustr(&r, L"SeDenyBatchLogonRight");
    buf = NULL; cnt = 0;
    EXPECT_EQ(LADD(f.pol, f.sid, &r, 1), 0);
    st = LENUMA(f.pol, &r, &buf, &cnt);
    EXPECT_EQ(st, 0);
    if (st == 0) {
        PLSA_ENUMERATION_INFORMATION e = buf;
        t_note("SeDenyBatchLogonRight holders: %lu", cnt);
        found = 0;
        for (i = 0; e && i < cnt; i++) if (e[i].Sid && EqualSid(e[i].Sid, f.sid)) found = 1;
        CHECK(found);
        LFREE(buf);
    }
    /* after removal the user is gone from the enumeration */
    {
        LSA_UNICODE_STRING rr; ustr(&rr, L"SeBatchLogonRight");
        EXPECT_EQ(LREM(f.pol, f.sid, FALSE, &rr, 1), 0);
    }
    ustr(&r, L"SeBatchLogonRight");
    buf = NULL; cnt = 0; found = 0;
    st = LENUMA(f.pol, &r, &buf, &cnt);
    if (st == 0) {
        PLSA_ENUMERATION_INFORMATION e = buf;
        for (i = 0; e && i < cnt; i++) if (e[i].Sid && EqualSid(e[i].Sid, f.sid)) found = 1;
        CHECK(!found);
        LFREE(buf);
    } else t_note("enumerate after removal -> 0x%lx", st);
    fx_close(&f);
}
TEST(dxgap2_lsa_enum_accounts_null_right_lists_all)
{
    fixture f; LSA_UNICODE_STRING r; PVOID buf = NULL; ULONG cnt = 0, i; int found = 0; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r, L"SeBatchLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, &r, 1), 0);
    st = LENUMA(f.pol, NULL, &buf, &cnt);
    EXPECT_EQ(st, 0);
    if (st == 0) {
        PLSA_ENUMERATION_INFORMATION e = buf;
        CHECK(cnt >= 1);
        for (i = 0; e && i < cnt; i++) if (e[i].Sid && EqualSid(e[i].Sid, f.sid)) found = 1;
        CHECK(found);
        LFREE(buf);
    }
    fx_close(&f);
}
TEST(dxgap2_lsa_enum_rights_after_remove_all)
{
    fixture f; LSA_UNICODE_STRING r; PLSA_UNICODE_STRING out = NULL; ULONG cnt = 0; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r, L"SeBatchLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, &r, 1), 0);
    EXPECT_EQ(LREM(f.pol, f.sid, TRUE, NULL, 0), 0);
    st = LENUMR(f.pol, f.sid, &out, &cnt);
    CHECKF(st != 0, "an account stripped of all rights must not enumerate (0x%lx)", st);
    t_note("enumerate after RemoveAll -> 0x%lx", st);
    CHECKF(out == NULL && cnt == 0, "outputs zeroed (%p, %lu)", out, cnt);
    fx_close(&f);
}
TEST(dxgap2_lsa_add_privilege_vs_logon_right)
{
    fixture f; LSA_UNICODE_STRING r[2]; PLSA_UNICODE_STRING out = NULL; ULONG cnt = 0;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r[0], L"SeChangeNotifyPrivilege"); ustr(&r[1], L"SeNetworkLogonRight");
    EXPECT_EQ(LADD(f.pol, f.sid, r, 2), 0);                  /* privileges and logon rights share the one namespace */
    EXPECT_EQ(LENUMR(f.pol, f.sid, &out, &cnt), 0);
    EXPECT_EQ(cnt, 2);
    CHECK(out && has_right(out, cnt, L"SeChangeNotifyPrivilege") && has_right(out, cnt, L"SeNetworkLogonRight"));
    if (out) LFREE(out);
    fx_close(&f);
}
TEST(dxgap2_lsa_add_zero_rights)
{
    fixture f; LSA_UNICODE_STRING r; NTSTATUS st;
    NOWINE();
    if (!fx_open(&f, FULL)) { fx_close(&f); return; }
    ustr(&r, L"SeBatchLogonRight");
    st = LADD(f.pol, f.sid, &r, 0);
    t_note("LsaAddAccountRights(count 0) -> 0x%lx", st);
    fx_close(&f);
}
