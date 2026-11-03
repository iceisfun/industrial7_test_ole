/* gap_03: rpcrt4 RpcBindingSetAuthInfoExA (ANSI thunk over ...ExW) and RpcAsyncRegisterInfo (obsolete no-op).
 * Verified in the Win10 decomp: ExA converts ServerPrincName with CHeapUnicode, thunks the QOS (ThunkSecurityQOS) into a
 * V5 wide structure and forwards to ExW; ExW rejects a QOS Version outside 1..5 with 87 BEFORE it looks at the binding. */
#include "t.h"
#include <rpc.h>
#include <rpcasync.h>

static RPC_BINDING_HANDLE mkb(void) {
    char sb[96]; RPC_BINDING_HANDLE h = NULL;
    wsprintfA(sb, "ncalrpc:[olet_gap3_%lu]", GetCurrentProcessId());
    if (U(RpcBindingFromStringBindingA)((RPC_CSTR)sb, &h) != RPC_S_OK) return NULL;
    return h;
}
#define NEEDA() do { REQUIRE(U(RpcBindingSetAuthInfoExA)); REQUIRE(U(RpcBindingSetAuthInfoExW)); REQUIRE(U(RpcBindingFromStringBindingA)); REQUIRE(U(RpcBindingFree)); } while (0)
#define SETA(h, pn, lvl, svc, ident, authz, qos) U(RpcBindingSetAuthInfoExA)(h, (RPC_CSTR)(pn), lvl, svc, ident, authz, qos)
#define SETW(h, pn, lvl, svc, ident, authz, qos) U(RpcBindingSetAuthInfoExW)(h, (RPC_WSTR)(pn), lvl, svc, ident, authz, qos)

typedef union { RPC_SECURITY_QOS q; unsigned char raw[256]; } QOSBUF;

TEST(gap3_rpc_setauthinfoexa_null_binding) {
    NEEDA();
    if (t_is_wine()) { t_skip("wine: RpcBindingSetAuthInfoExA faults on a NULL binding / odd QOS"); return; }
    EXPECT_EQ(SETA(NULL, NULL, RPC_C_AUTHN_LEVEL_NONE, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHZ_NONE, NULL), RPC_S_INVALID_BINDING);
    EXPECT_EQ(SETA(NULL, "host/olet", RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, NULL), RPC_S_INVALID_BINDING);
    EXPECT_EQ(SETW(NULL, NULL, RPC_C_AUTHN_LEVEL_NONE, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHZ_NONE, NULL), RPC_S_INVALID_BINDING);
}
TEST(gap3_rpc_setauthinfoexa_qos_version_checked_before_binding) {
    NEEDA();
    if (t_is_wine()) { t_skip("wine: RpcBindingSetAuthInfoExA faults on a NULL binding / odd QOS"); return; }
    static const unsigned vers[] = { 0, 6, 7, 99, 0xffffffffu };
    for (unsigned i = 0; i < sizeof vers / sizeof vers[0]; i++) {
        QOSBUF b; memset(&b, 0, sizeof b);
        b.q.Version = vers[i];
        b.q.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT; b.q.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC; b.q.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
        RPC_STATUS a = SETA(NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &b.q);
        RPC_STATUS w = SETW(NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &b.q);
        CHECKF(a == RPC_S_INVALID_ARG, "A: QOS version %u on a NULL binding -> %lu (want 87)", vers[i], (unsigned long)a);
        CHECKF(w == RPC_S_INVALID_ARG, "W: QOS version %u on a NULL binding -> %lu (want 87)", vers[i], (unsigned long)w);
    }
    /* versions 1..5 pass the check, so the NULL binding is reported instead */
    for (unsigned v = 1; v <= 5; v++) {
        QOSBUF b; memset(&b, 0, sizeof b); b.q.Version = v;
        b.q.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT; b.q.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC; b.q.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
        RPC_STATUS a = SETA(NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &b.q);
        CHECKF(a == RPC_S_INVALID_BINDING, "A: QOS version %u on a NULL binding -> %lu (want 1702)", v, (unsigned long)a);
    }
}
TEST(gap3_rpc_setauthinfoexa_authn_none_rejects_levels_above_none) {
    NEEDA();
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    /* decomp: AuthnSvc == RPC_C_AUTHN_NONE and AuthnLevel > 1 -> 0x6d4 (RPC_S_UNKNOWN_AUTHN_LEVEL) */
    static const unsigned lv[] = { 2, 3, 4, 5, 6, 7, 100 };
    for (unsigned i = 0; i < sizeof lv / sizeof lv[0]; i++) {
        RPC_STATUS a = SETA(h, NULL, lv[i], RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHZ_NONE, NULL);
        RPC_STATUS w = SETW(h, NULL, lv[i], RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHZ_NONE, NULL);
        CHECKF(a == RPC_S_UNKNOWN_AUTHN_LEVEL && w == a, "level %u: A %lu W %lu", lv[i], (unsigned long)a, (unsigned long)w);
    }
    U(RpcBindingFree)(&h);
}
TEST(gap3_rpc_setauthinfoexa_authn_none_levels_default_and_none_succeed) {
    NEEDA();
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(SETA(h, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHZ_NONE, NULL), RPC_S_OK);
    EXPECT_EQ(SETA(h, NULL, RPC_C_AUTHN_LEVEL_NONE, RPC_C_AUTHN_NONE, NULL, RPC_C_AUTHZ_NONE, NULL), RPC_S_OK);
    U(RpcBindingFree)(&h);
}
TEST(gap3_rpc_setauthinfoexa_roundtrip_with_principal_name) {
    NEEDA(); REQUIRE(U(RpcBindingInqAuthInfoExW)); REQUIRE(U(RpcStringFreeW));
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    RPC_STATUS st = SETA(h, "olet/gap3.example", RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, NULL);
    EXPECT_EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        RPC_WSTR pn = NULL; unsigned long lvl = 0, svc = 0, authz = 99;
        RPC_STATUS qs = U(RpcBindingInqAuthInfoExW)(h, &pn, &lvl, &svc, NULL, &authz, 0, NULL);
        EXPECT_EQ(qs, RPC_S_OK);
        EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY);
        EXPECT_EQ(svc, RPC_C_AUTHN_WINNT);
        EXPECT_EQ(authz, RPC_C_AUTHZ_NONE);
        CHECK(pn != NULL && !lstrcmpW((WCHAR *)pn, L"olet/gap3.example"));
        if (pn) U(RpcStringFreeW)(&pn);
    }
    U(RpcBindingFree)(&h);
}
TEST(gap3_rpc_setauthinfoexa_a_and_w_agree_on_levels_and_services) {
    NEEDA();
    static const struct { unsigned lvl, svc; } t[] = {
        { RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT }, { RPC_C_AUTHN_LEVEL_CALL, RPC_C_AUTHN_WINNT }, { RPC_C_AUTHN_LEVEL_PKT, RPC_C_AUTHN_WINNT },
        { RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT }, { RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT },
        { RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_GSS_NEGOTIATE }, { RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_DEFAULT },
        { RPC_C_AUTHN_LEVEL_PKT_PRIVACY, 0x12345 }, { RPC_C_AUTHN_LEVEL_CONNECT, 0xfffe }, { 99, RPC_C_AUTHN_WINNT }, { 8, RPC_C_AUTHN_WINNT },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        RPC_BINDING_HANDLE ha = mkb(), hw = mkb(); CHECK(ha && hw); if (!ha || !hw) return;
        RPC_STATUS a = SETA(ha, NULL, t[i].lvl, t[i].svc, NULL, RPC_C_AUTHZ_NONE, NULL);
        RPC_STATUS w = SETW(hw, NULL, t[i].lvl, t[i].svc, NULL, RPC_C_AUTHZ_NONE, NULL);
        CHECKF(a == w, "level %u svc 0x%x: A %lu W %lu", t[i].lvl, t[i].svc, (unsigned long)a, (unsigned long)w);
        t_note("SetAuthInfoEx level %u svc 0x%x -> %lu", t[i].lvl, t[i].svc, (unsigned long)a);
        U(RpcBindingFree)(&ha); U(RpcBindingFree)(&hw);
    }
}
TEST(gap3_rpc_setauthinfoexa_unknown_authn_service_is_rejected) {
    NEEDA();
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    RPC_STATUS st = SETA(h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, 0x7777, NULL, RPC_C_AUTHZ_NONE, NULL);
    CHECKF(st == RPC_S_UNKNOWN_AUTHN_SERVICE, "-> %lu", (unsigned long)st);
    U(RpcBindingFree)(&h);
}
TEST(gap3_rpc_setauthinfoexa_qos_v1_roundtrip) {
    NEEDA(); REQUIRE(U(RpcBindingInqAuthInfoExW));
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    RPC_SECURITY_QOS q; memset(&q, 0, sizeof q);
    q.Version = RPC_C_SECURITY_QOS_VERSION; q.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    q.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC; q.ImpersonationType = RPC_C_IMP_LEVEL_IDENTIFY;
    RPC_STATUS st = SETA(h, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &q);
    EXPECT_EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        RPC_SECURITY_QOS got; memset(&got, 0xCC, sizeof got);
        unsigned long lvl = 0, svc = 0;
        EXPECT_EQ(U(RpcBindingInqAuthInfoExW)(h, NULL, &lvl, &svc, NULL, NULL, RPC_C_SECURITY_QOS_VERSION, &got), RPC_S_OK);
        EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
        EXPECT_EQ(got.Version, RPC_C_SECURITY_QOS_VERSION);
        EXPECT_EQ(got.IdentityTracking, RPC_C_QOS_IDENTITY_DYNAMIC);
        EXPECT_EQ(got.ImpersonationType, RPC_C_IMP_LEVEL_IDENTIFY);
        EXPECT_EQ(got.Capabilities, RPC_C_QOS_CAPABILITIES_DEFAULT);
    }
    U(RpcBindingFree)(&h);
}
TEST(gap3_rpc_setauthinfoexa_qos_v2_additional_info_types) {
    NEEDA();
    if (t_is_wine()) { t_skip("wine: RpcBindingSetAuthInfoExA faults on a NULL binding / odd QOS"); return; }
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    QOSBUF b; memset(&b, 0, sizeof b);
    RPC_SECURITY_QOS_V2_A *q = (RPC_SECURITY_QOS_V2_A *)&b;
    q->Version = 2; q->Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT; q->IdentityTracking = RPC_C_QOS_IDENTITY_STATIC; q->ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
    /* ThunkSecurityQOS: type 0 needs a NULL pointer; type 1 (HTTP credentials) needs a pointer; anything else is 87 */
    q->AdditionalSecurityInfoType = 0; q->u.HttpCredentials = NULL;
    EXPECT_EQ(SETA(h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, (RPC_SECURITY_QOS *)q), RPC_S_OK);
    q->AdditionalSecurityInfoType = 1; q->u.HttpCredentials = NULL;
    EXPECT_EQ(SETA(h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, (RPC_SECURITY_QOS *)q), RPC_S_INVALID_ARG);
    static const unsigned bad[] = { 2, 3, 7, 0x100 };
    for (unsigned i = 0; i < 4; i++) {
        q->AdditionalSecurityInfoType = bad[i]; q->u.HttpCredentials = NULL;
        RPC_STATUS st = SETA(h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, (RPC_SECURITY_QOS *)q);
        CHECKF(st == RPC_S_INVALID_ARG, "type %u -> %lu", bad[i], (unsigned long)st);
    }
    U(RpcBindingFree)(&h);
}
TEST(gap3_rpc_setauthinfoexa_binding_unchanged_after_rejected_call) {
    NEEDA(); REQUIRE(U(RpcBindingInqAuthInfoExW));
    RPC_BINDING_HANDLE h = mkb(); CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(SETA(h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, NULL), RPC_S_OK);
    QOSBUF b; memset(&b, 0, sizeof b); b.q.Version = 42;
    EXPECT_EQ(SETA(h, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &b.q), RPC_S_INVALID_ARG);
    unsigned long lvl = 0, svc = 0;
    EXPECT_EQ(U(RpcBindingInqAuthInfoExW)(h, NULL, &lvl, &svc, NULL, NULL, 0, NULL), RPC_S_OK);
    EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_CONNECT);
    U(RpcBindingFree)(&h);
}

/* ---- RpcAsyncRegisterInfo: obsolete; the decomp is `xor eax,eax; ret` (shared with another no-op) ---- */
TEST(gap3_rpc_asyncregisterinfo_is_noop_returning_ok) {
    REQUIRE(U(RpcAsyncRegisterInfo));
    if (t_is_wine()) { t_skip("wine: RpcAsyncRegisterInfo is unimplemented"); return; }
    EXPECT_EQ(U(RpcAsyncRegisterInfo)(NULL), RPC_S_OK);
    RPC_ASYNC_STATE s; memset(&s, 0, sizeof s);
    EXPECT_EQ(U(RpcAsyncRegisterInfo)(&s), RPC_S_OK);
    RPC_ASYNC_STATE before;
    memset(&s, 0xA5, sizeof s); before = s;
    EXPECT_EQ(U(RpcAsyncRegisterInfo)(&s), RPC_S_OK);
    CHECK(!memcmp(&s, &before, sizeof s));          /* the argument is never touched */
}
TEST(gap3_rpc_asyncregisterinfo_initialized_state_unchanged) {
    REQUIRE(U(RpcAsyncRegisterInfo)); REQUIRE(U(RpcAsyncInitializeHandle));
    if (t_is_wine()) { t_skip("wine: RpcAsyncRegisterInfo is unimplemented"); return; }
    RPC_ASYNC_STATE s; memset(&s, 0, sizeof s);
    RPC_STATUS st = U(RpcAsyncInitializeHandle)(&s, sizeof s);
    EXPECT_EQ(st, RPC_S_OK);
    RPC_ASYNC_STATE before = s;
    EXPECT_EQ(U(RpcAsyncRegisterInfo)(&s), RPC_S_OK);
    CHECK(!memcmp(&s, &before, sizeof s));
}
