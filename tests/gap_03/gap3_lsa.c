/* gap_03: sechost LSA policy exports LsaRemoveAccountRights / LsaRetrievePrivateData / LsaStorePrivateData /
 * LsaSetInformationPolicy: the client-side argument handling before any RPC to lsasrv is attempted
 * (Win10 sechost decomp). Nothing here changes system state: every failure is produced locally.
 *   - PolicyHandle == NULL                               -> STATUS_INVALID_HANDLE  (0xC0000008)
 *   - SetInformationPolicy(PolicyDnsDomainInformationInt) -> STATUS_INVALID_PARAMETER (0xC000000D), checked BEFORE the handle
 *   - Store/RetrievePrivateData KeyName with odd Length   -> STATUS_INVALID_PARAMETER (LsapDuplicateSecretName), after the handle */
#include "t.h"
#include <ntsecapi.h>

#define ST_INVALID_HANDLE    ((NTSTATUS)0xC0000008)
#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)

#define WSKIP() do { if (t_is_wine()) { t_skip("wine: sechost Lsa* exports are stubs with different status codes"); return; } } while (0)
static int open_lookup(LSA_HANDLE *h) {
    LSA_OBJECT_ATTRIBUTES oa; memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    NTSTATUS st = U(LsaOpenPolicy)(NULL, &oa, POLICY_LOOKUP_NAMES, h);
    if (st) { t_note("LsaOpenPolicy(POLICY_LOOKUP_NAMES) -> 0x%lx", (unsigned long)st); return 0; }
    return 1;
}

TEST(gap3_lsa_remove_account_rights_null_handle) {
    WSKIP();
    REQUIRE(U(LsaRemoveAccountRights));
    LSA_UNICODE_STRING r; WCHAR w[] = L"SeBatchLogonRight";
    r.Buffer = w; r.Length = (USHORT)(lstrlenW(w) * 2); r.MaximumLength = r.Length + 2;
    BYTE sid[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof sid;
    CreateWellKnownSid(WinWorldSid, NULL, sid, &n);
    EXPECT_EQ(U(LsaRemoveAccountRights)(NULL, sid, FALSE, &r, 1), ST_INVALID_HANDLE);
    EXPECT_EQ(U(LsaRemoveAccountRights)(NULL, sid, TRUE, NULL, 0), ST_INVALID_HANDLE);
    EXPECT_EQ(U(LsaRemoveAccountRights)(NULL, NULL, TRUE, NULL, 0), ST_INVALID_HANDLE);
    EXPECT_EQ(U(LsaRemoveAccountRights)(NULL, NULL, FALSE, NULL, 0), ST_INVALID_HANDLE);
    EXPECT_EQ(U(LsaRemoveAccountRights)(NULL, sid, FALSE, &r, 0xffffffffu), ST_INVALID_HANDLE);
}
TEST(gap3_lsa_store_private_data_null_handle) {
    WSKIP();
    REQUIRE(U(LsaStorePrivateData));
    WCHAR k[] = L"olet_gap3_key", d[] = L"data";
    LSA_UNICODE_STRING key = { (USHORT)(lstrlenW(k) * 2), (USHORT)(lstrlenW(k) * 2 + 2), k };
    LSA_UNICODE_STRING dat = { (USHORT)(lstrlenW(d) * 2), (USHORT)(lstrlenW(d) * 2 + 2), d };
    EXPECT_EQ(U(LsaStorePrivateData)(NULL, &key, &dat), ST_INVALID_HANDLE);
    EXPECT_EQ(U(LsaStorePrivateData)(NULL, &key, NULL), ST_INVALID_HANDLE);   /* delete form */
    EXPECT_EQ(U(LsaStorePrivateData)(NULL, NULL, NULL), ST_INVALID_HANDLE);   /* NULL key not dereferenced before the handle test */
}
TEST(gap3_lsa_retrieve_private_data_null_handle) {
    WSKIP();
    REQUIRE(U(LsaRetrievePrivateData));
    WCHAR k[] = L"olet_gap3_key";
    LSA_UNICODE_STRING key = { (USHORT)(lstrlenW(k) * 2), (USHORT)(lstrlenW(k) * 2 + 2), k };
    PLSA_UNICODE_STRING out = (PLSA_UNICODE_STRING)(ULONG_PTR)0x1;
    EXPECT_EQ(U(LsaRetrievePrivateData)(NULL, &key, &out), ST_INVALID_HANDLE);
    CHECK(out == (PLSA_UNICODE_STRING)(ULONG_PTR)0x1);                         /* output left alone */
    EXPECT_EQ(U(LsaRetrievePrivateData)(NULL, NULL, NULL), ST_INVALID_HANDLE);
}
TEST(gap3_lsa_set_information_policy_null_handle_by_class) {
    WSKIP();
    REQUIRE(U(LsaSetInformationPolicy));
    /* handle check precedes the class dispatch except for PolicyDnsDomainInformationInt which is refused first */
    for (int c = 0; c <= 24; c++) {
        NTSTATUS st = U(LsaSetInformationPolicy)(NULL, (POLICY_INFORMATION_CLASS)c, NULL);
        NTSTATUS want = (c == (int)PolicyDnsDomainInformationInt) ? ST_INVALID_PARAMETER : ST_INVALID_HANDLE;
        CHECKF(st == want, "class %d: 0x%lx want 0x%lx", c, (unsigned long)st, (unsigned long)want);
    }
    NTSTATUS st = U(LsaSetInformationPolicy)(NULL, (POLICY_INFORMATION_CLASS)0x7fffffff, NULL);
    CHECKF(st == ST_INVALID_HANDLE, "class 0x7fffffff: 0x%lx", (unsigned long)st);
}
TEST(gap3_lsa_set_information_policy_dns_domain_int_is_internal_only) {
    WSKIP();
    REQUIRE(U(LsaSetInformationPolicy)); REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaClose));
    LSA_HANDLE h = NULL;
    if (!open_lookup(&h)) { t_skip("cannot open the policy"); return; }
    EXPECT_EQ(U(LsaSetInformationPolicy)(h, PolicyDnsDomainInformationInt, NULL), ST_INVALID_PARAMETER);
    U(LsaClose)(h);
}
static void odd_key_case(int store) {
    LSA_HANDLE h = NULL;
    if (!open_lookup(&h)) { t_skip("cannot open the policy"); return; }
    WCHAR k[8] = L"abcdef";
    static const USHORT lens[] = { 1, 3, 5, 7, 9, 11, 0xffff };
    for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        LSA_UNICODE_STRING key; key.Length = lens[i]; key.MaximumLength = (USHORT)(lens[i] < 0xfffe ? lens[i] + 1 : 0xffff); key.Buffer = k;
        NTSTATUS st;
        if (store) st = U(LsaStorePrivateData)(h, &key, NULL);
        else { PLSA_UNICODE_STRING out = NULL; st = U(LsaRetrievePrivateData)(h, &key, &out); if (!st && out) U(LsaFreeMemory)(out); }
        CHECKF(st == ST_INVALID_PARAMETER, "%s key Length %u -> 0x%lx", store ? "Store" : "Retrieve", lens[i], (unsigned long)st);
    }
    U(LsaClose)(h);
}
TEST(gap3_lsa_store_private_data_odd_key_length) {
    WSKIP();
    REQUIRE(U(LsaStorePrivateData)); REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaClose));
    odd_key_case(1);
}
TEST(gap3_lsa_retrieve_private_data_odd_key_length) {
    WSKIP();
    REQUIRE(U(LsaRetrievePrivateData)); REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaClose)); REQUIRE(U(LsaFreeMemory));
    odd_key_case(0);
}
TEST(gap3_lsa_closed_handle_is_not_null_handle) {
    WSKIP();
    /* after LsaClose the handle value is stale: only NULL gets the local INVALID_HANDLE answer, so just record it */
    REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaClose));
    LSA_HANDLE h = NULL;
    if (!open_lookup(&h)) { t_skip("cannot open the policy"); return; }
    EXPECT_EQ(U(LsaClose)(h), 0);
    EXPECT_EQ(U(LsaClose)(NULL), ST_INVALID_HANDLE);
}
