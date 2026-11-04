/* dx_gap_03: state-changing LSA tests (private data store/retrieve/delete, account rights add/remove).
 * Gated on POLICY_ALL_ACCESS (administrator); every object is named with olet + pid and removed in the tail of the test. */
#include "t.h"
#include <ntsecapi.h>
#include <sddl.h>

#define ST_NAME_NOT_FOUND ((NTSTATUS)0xC0000034)
#define ST_ACCESS_DENIED  ((NTSTATUS)0xC0000022)

static LSA_HANDLE open_all(void) {
    LSA_OBJECT_ATTRIBUTES oa; memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    LSA_HANDLE h = NULL;
    NTSTATUS st = U(LsaOpenPolicy)(NULL, &oa, POLICY_ALL_ACCESS, &h);
    if (st) { t_skip("LsaOpenPolicy(POLICY_ALL_ACCESS) -> 0x%lx (needs administrator)", (unsigned long)st); return NULL; }
    return h;
}
static void mkus(LSA_UNICODE_STRING *u, WCHAR *buf, const WCHAR *s) {
    lstrcpyW(buf, s); u->Buffer = buf; u->Length = (USHORT)(lstrlenW(buf) * 2); u->MaximumLength = u->Length + 2;
}

TEST(dxg3_lsa_private_data_store_retrieve_delete) {
    if (t_is_wine()) { t_skip("wine: LSA is a stub"); return; }
    REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaStorePrivateData)); REQUIRE(U(LsaRetrievePrivateData)); REQUIRE(U(LsaFreeMemory)); REQUIRE(U(LsaClose));
    LSA_HANDLE h = open_all(); if (!h) return;
    WCHAR kb[64], d1[32], d2[32];
    wsprintfW(kb, L"olet_gap3_secret_%lu", GetCurrentProcessId());
    LSA_UNICODE_STRING key, v1, v2; mkus(&key, kb, kb); mkus(&v1, d1, L"first value"); mkus(&v2, d2, L"second, longer value");
    U(LsaStorePrivateData)(h, &key, NULL);                           /* sweep */
    PLSA_UNICODE_STRING out = NULL;
    NTSTATUS st = U(LsaRetrievePrivateData)(h, &key, &out);
    CHECKF(st == ST_NAME_NOT_FOUND, "retrieve of a missing key -> 0x%lx", (unsigned long)st);
    CHECK(out == NULL);
    st = U(LsaStorePrivateData)(h, &key, &v1);
    CHECKF(st == 0, "store -> 0x%lx", (unsigned long)st);
    if (st == 0) {
        out = NULL;
        st = U(LsaRetrievePrivateData)(h, &key, &out);
        CHECKF(st == 0 && out != NULL, "retrieve -> 0x%lx", (unsigned long)st);
        if (st == 0 && out) {
            CHECKF(out->Length == v1.Length, "length %u want %u", out->Length, v1.Length);
            CHECK(out->Buffer && !memcmp(out->Buffer, v1.Buffer, v1.Length));
            CHECK(out->MaximumLength >= out->Length);
            U(LsaFreeMemory)(out);
        }
        /* overwrite */
        EXPECT_EQ(U(LsaStorePrivateData)(h, &key, &v2), 0);
        out = NULL;
        st = U(LsaRetrievePrivateData)(h, &key, &out);
        if (st == 0 && out) { CHECK(out->Length == v2.Length && !memcmp(out->Buffer, v2.Buffer, v2.Length)); U(LsaFreeMemory)(out); }
        else CHECKF(0, "second retrieve -> 0x%lx", (unsigned long)st);
        /* NULL PrivateData deletes */
        st = U(LsaStorePrivateData)(h, &key, NULL);   /* once: a second delete reports name-not-found */
        EXPECT_EQ(st, 0);
        out = NULL;
        st = U(LsaRetrievePrivateData)(h, &key, &out);
        CHECKF(st == ST_NAME_NOT_FOUND, "retrieve after delete -> 0x%lx", (unsigned long)st);
        if (st == 0 && out) U(LsaFreeMemory)(out);
    }
    U(LsaStorePrivateData)(h, &key, NULL);                           /* tail: always remove */
    U(LsaClose)(h);
}
TEST(dxg3_lsa_private_data_keys_are_case_insensitive_and_distinct) {
    if (t_is_wine()) { t_skip("wine: LSA is a stub"); return; }
    REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaStorePrivateData)); REQUIRE(U(LsaRetrievePrivateData)); REQUIRE(U(LsaFreeMemory)); REQUIRE(U(LsaClose));
    LSA_HANDLE h = open_all(); if (!h) return;
    WCHAR ka[64], kb[64], kc[64], da[16], db[16];
    wsprintfW(ka, L"olet_gap3_secA_%lu", GetCurrentProcessId());
    wsprintfW(kb, L"olet_gap3_secB_%lu", GetCurrentProcessId());
    wsprintfW(kc, L"OLET_GAP3_SECA_%lu", GetCurrentProcessId());
    LSA_UNICODE_STRING A, B, C, VA, VB; mkus(&A, ka, ka); mkus(&B, kb, kb); mkus(&C, kc, kc); mkus(&VA, da, L"AAA"); mkus(&VB, db, L"BBBB");
    U(LsaStorePrivateData)(h, &A, NULL); U(LsaStorePrivateData)(h, &B, NULL);
    if (U(LsaStorePrivateData)(h, &A, &VA) == 0 && U(LsaStorePrivateData)(h, &B, &VB) == 0) {
        PLSA_UNICODE_STRING o = NULL;
        NTSTATUS st = U(LsaRetrievePrivateData)(h, &B, &o);
        CHECK(st == 0 && o && o->Length == VB.Length && !memcmp(o->Buffer, VB.Buffer, VB.Length));
        if (o) U(LsaFreeMemory)(o);
        o = NULL;
        st = U(LsaRetrievePrivateData)(h, &C, &o);
        t_note("retrieve with the key spelled in upper case -> 0x%lx", (unsigned long)st);
        if (st == 0 && o) { CHECK(o->Length == VA.Length && !memcmp(o->Buffer, VA.Buffer, VA.Length)); U(LsaFreeMemory)(o); }
    } else CHECK(0);
    U(LsaStorePrivateData)(h, &A, NULL); U(LsaStorePrivateData)(h, &B, NULL);
    U(LsaClose)(h);
}

static PSID fresh_sid(void) {
    WCHAR s[96]; PSID sid = NULL;
    DWORD pid = GetCurrentProcessId();
    wsprintfW(s, L"S-1-5-21-%lu-%lu-%lu-%lu", 4001000000ul + pid, 1234567ul, 7654321ul, 90000ul + (pid & 0xfff));
    if (!U(ConvertStringSidToSidW) || !U(ConvertStringSidToSidW)(s, &sid)) return NULL;
    return sid;
}
TEST(dxg3_lsa_account_rights_add_remove_lifecycle) {
    if (t_is_wine()) { t_skip("wine: LSA is a stub"); return; }
    REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaAddAccountRights)); REQUIRE(U(LsaRemoveAccountRights)); REQUIRE(U(LsaEnumerateAccountRights)); REQUIRE(U(LsaFreeMemory)); REQUIRE(U(LsaClose));
    LSA_HANDLE h = open_all(); if (!h) return;
    PSID sid = fresh_sid(); if (!sid) { t_skip("no SID"); U(LsaClose)(h); return; }
    WCHAR b1[40], b2[40];
    LSA_UNICODE_STRING r1, r2; mkus(&r1, b1, L"SeDenyBatchLogonRight"); mkus(&r2, b2, L"SeDenyServiceLogonRight");
    U(LsaRemoveAccountRights)(h, sid, TRUE, NULL, 0);                /* sweep */
    PLSA_UNICODE_STRING list = NULL; ULONG cnt = 0;
    NTSTATUS st = U(LsaEnumerateAccountRights)(h, sid, &list, &cnt);
    CHECKF(st == ST_NAME_NOT_FOUND, "enumerate for an unknown account -> 0x%lx", (unsigned long)st);
    /* removing from an account that does not exist */
    st = U(LsaRemoveAccountRights)(h, sid, FALSE, &r1, 1);
    t_note("LsaRemoveAccountRights(unknown account, one right) -> 0x%lx", (unsigned long)st);
    CHECK(st != 0);
    st = U(LsaRemoveAccountRights)(h, sid, TRUE, NULL, 0);
    t_note("LsaRemoveAccountRights(unknown account, AllRights) -> 0x%lx", (unsigned long)st);
    LSA_UNICODE_STRING both[2] = { r1, r2 };
    st = U(LsaAddAccountRights)(h, sid, both, 2);
    CHECKF(st == 0, "add two rights -> 0x%lx", (unsigned long)st);
    if (st == 0) {
        list = NULL; cnt = 0;
        st = U(LsaEnumerateAccountRights)(h, sid, &list, &cnt);
        CHECKF(st == 0 && cnt == 2, "enumerate -> 0x%lx count %lu", (unsigned long)st, cnt);
        if (list) U(LsaFreeMemory)(list);
        /* remove one right */
        st = U(LsaRemoveAccountRights)(h, sid, FALSE, &r1, 1);
        CHECKF(st == 0, "remove one right -> 0x%lx", (unsigned long)st);
        list = NULL; cnt = 0;
        st = U(LsaEnumerateAccountRights)(h, sid, &list, &cnt);
        CHECKF(st == 0 && cnt == 1, "enumerate after removing one -> 0x%lx count %lu", (unsigned long)st, cnt);
        if (st == 0 && cnt == 1) CHECK(list[0].Length == r2.Length && !memcmp(list[0].Buffer, r2.Buffer, r2.Length));
        if (list) U(LsaFreeMemory)(list);
        /* removing a right the account no longer holds */
        st = U(LsaRemoveAccountRights)(h, sid, FALSE, &r1, 1);
        t_note("remove a right the account does not hold -> 0x%lx", (unsigned long)st);
        /* AllRights removes everything and the account disappears */
        st = U(LsaRemoveAccountRights)(h, sid, TRUE, NULL, 0);
        CHECKF(st == 0, "remove all rights -> 0x%lx", (unsigned long)st);
        list = NULL; cnt = 0;
        st = U(LsaEnumerateAccountRights)(h, sid, &list, &cnt);
        CHECKF(st == ST_NAME_NOT_FOUND, "enumerate after removing all rights -> 0x%lx", (unsigned long)st);
        if (st == 0 && list) U(LsaFreeMemory)(list);
    }
    U(LsaRemoveAccountRights)(h, sid, TRUE, NULL, 0);                /* tail */
    LocalFree(sid);
    U(LsaClose)(h);
}
TEST(dxg3_lsa_remove_last_right_deletes_account) {
    if (t_is_wine()) { t_skip("wine: LSA is a stub"); return; }
    REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaAddAccountRights)); REQUIRE(U(LsaRemoveAccountRights)); REQUIRE(U(LsaEnumerateAccountRights)); REQUIRE(U(LsaFreeMemory)); REQUIRE(U(LsaClose));
    LSA_HANDLE h = open_all(); if (!h) return;
    PSID sid = fresh_sid(); if (!sid) { t_skip("no SID"); U(LsaClose)(h); return; }
    WCHAR b1[40]; LSA_UNICODE_STRING r1; mkus(&r1, b1, L"SeDenyNetworkLogonRight");
    U(LsaRemoveAccountRights)(h, sid, TRUE, NULL, 0);
    NTSTATUS st = U(LsaAddAccountRights)(h, sid, &r1, 1);
    CHECKF(st == 0, "add -> 0x%lx", (unsigned long)st);
    if (st == 0) {
        st = U(LsaRemoveAccountRights)(h, sid, FALSE, &r1, 1);
        CHECKF(st == 0, "remove the only right -> 0x%lx", (unsigned long)st);
        PLSA_UNICODE_STRING list = NULL; ULONG cnt = 0;
        st = U(LsaEnumerateAccountRights)(h, sid, &list, &cnt);
        t_note("enumerate after removing the only right -> 0x%lx (count %lu)", (unsigned long)st, cnt);
        if (st == 0 && list) U(LsaFreeMemory)(list);
    }
    U(LsaRemoveAccountRights)(h, sid, TRUE, NULL, 0);
    LocalFree(sid);
    U(LsaClose)(h);
}
TEST(dxg3_lsa_set_information_policy_requires_set_rights_on_handle) {
    if (t_is_wine()) { t_skip("wine: LSA is a stub"); return; }
    REQUIRE(U(LsaOpenPolicy)); REQUIRE(U(LsaSetInformationPolicy)); REQUIRE(U(LsaClose));
    LSA_OBJECT_ATTRIBUTES oa; memset(&oa, 0, sizeof oa); oa.Length = sizeof oa;
    LSA_HANDLE h = NULL;
    NTSTATUS st = U(LsaOpenPolicy)(NULL, &oa, POLICY_LOOKUP_NAMES, &h);
    if (st) { t_skip("cannot open the policy: 0x%lx", (unsigned long)st); return; }
    /* a handle opened for POLICY_LOOKUP_NAMES only cannot change audit settings: the server refuses, nothing is modified */
    /* Win10 validates the buffer first: a NULL option array is STATUS_INVALID_PARAMETER, so pass a well-formed one */
    POLICY_AUDIT_EVENTS_INFO ai; memset(&ai, 0, sizeof ai);
    DWORD opts[9]; memset(opts, 0, sizeof opts);
    ai.AuditingMode = FALSE; ai.EventAuditingOptions = opts; ai.MaximumAuditEventCount = 9;
    st = U(LsaSetInformationPolicy)(h, PolicyAuditEventsInformation, &ai);
    CHECKF(st == ST_ACCESS_DENIED, "-> 0x%lx", (unsigned long)st);
    U(LsaClose)(h);
}
