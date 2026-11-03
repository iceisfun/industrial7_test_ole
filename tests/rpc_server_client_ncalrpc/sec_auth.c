/* Security over ncalrpc: server auth registration, client authentication info, call attributes seen by the
 * server, security callbacks and interface flags, impersonation. */
#include "rawfx.h"

static RPC_STATUS set_auth(RPC_BINDING_HANDLE h, unsigned level, unsigned svc) {
    return U(RpcBindingSetAuthInfoW)(h, NULL, level, svc, NULL, RPC_C_AUTHZ_NONE);
}
static RPC_STATUS set_auth_qos(RPC_BINDING_HANDLE h, unsigned level, unsigned svc, unsigned imp) {
    RPC_SECURITY_QOS q; memset(&q, 0, sizeof q);
    q.Version = RPC_C_SECURITY_QOS_VERSION; q.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    q.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC; q.ImpersonationType = imp;
    return U(RpcBindingSetAuthInfoExW)(h, NULL, level, svc, NULL, RPC_C_AUTHZ_NONE, &q);
}
static void need_winnt_server(void) { U(RpcServerRegisterAuthInfoW)(NULL, RPC_C_AUTHN_WINNT, NULL, NULL); }

/* ---- RpcServerRegisterAuthInfo / RpcServerInqDefaultPrincName ------------ */
TEST(ncalrpc_sec_registerauthinfo_winnt) {
    REQUIRE(U(RpcServerRegisterAuthInfoW));
    EQ(U(RpcServerRegisterAuthInfoW)(NULL, RPC_C_AUTHN_WINNT, NULL, NULL), RPC_S_OK);
    EQ(U(RpcServerRegisterAuthInfoA)(NULL, RPC_C_AUTHN_WINNT, NULL, NULL), RPC_S_OK);
}
TEST(ncalrpc_sec_registerauthinfo_unknown_service) {
    static const unsigned svcs[] = { 600, 11, 0x7777, 0x10000 };
    unsigned k;
    REQUIRE(U(RpcServerRegisterAuthInfoW));
    for (k = 0; k < sizeof svcs / sizeof *svcs; k++) {
        RPC_STATUS st = U(RpcServerRegisterAuthInfoW)(NULL, svcs[k], NULL, NULL);
        if (svcs[k] == 600) EQ(st, RPC_S_UNKNOWN_AUTHN_SERVICE);
        else CHECK(st == RPC_S_UNKNOWN_AUTHN_SERVICE || st == RPC_S_OK);
        t_note("RegisterAuthInfo(svc=%u) -> %lu", svcs[k], (unsigned long)st);
    }
}
TEST(ncalrpc_sec_registerauthinfo_other_services) {
    static const struct { unsigned svc; const char *name; } t[] = {
        { RPC_C_AUTHN_NONE, "NONE" }, { RPC_C_AUTHN_DEFAULT, "DEFAULT" }, { 9 /* GSS_NEGOTIATE */, "GSS_NEGOTIATE" },
        { 16 /* GSS_KERBEROS */, "GSS_KERBEROS" }, { 14 /* GSS_SCHANNEL */, "GSS_SCHANNEL" }, { 1 /* DCE_PRIVATE */, "DCE_PRIVATE" } };
    unsigned k;
    REQUIRE(U(RpcServerRegisterAuthInfoW));
    for (k = 0; k < sizeof t / sizeof *t; k++)
        t_note("RegisterAuthInfo(%s, NULL principal) -> %lu", t[k].name, (unsigned long)U(RpcServerRegisterAuthInfoW)(NULL, t[k].svc, NULL, NULL));
    t_note("RegisterAuthInfo(KERBEROS, \"olet/host\") -> %lu", (unsigned long)U(RpcServerRegisterAuthInfoW)((RPC_WSTR)L"olet/host", 16, NULL, NULL));
    need_winnt_server();
}
TEST(ncalrpc_sec_inq_default_princ_name) {
    RPC_WSTR p = (RPC_WSTR)(ULONG_PTR)0xdeadbeef; RPC_STATUS st;
    REQUIRE(U(RpcServerInqDefaultPrincNameW));
    st = U(RpcServerInqDefaultPrincNameW)(RPC_C_AUTHN_WINNT, &p);
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) { CHECK(p && *p); t_note("default WINNT principal: %ls", p); U(RpcStringFreeW)(&p); }
    p = (RPC_WSTR)(ULONG_PTR)0xdeadbeef;
    EQ(U(RpcServerInqDefaultPrincNameW)(0, &p), RPC_S_UNKNOWN_AUTHN_SERVICE);
    EQ(U(RpcServerInqDefaultPrincNameW)(RPC_C_AUTHN_DEFAULT, &p), RPC_S_UNKNOWN_AUTHN_SERVICE);
    CHECK(p == (RPC_WSTR)(ULONG_PTR)0xdeadbeef);               /* untouched on failure */
}
TEST(ncalrpc_sec_inq_default_princ_name_a_matches_w) {
    RPC_WSTR pw = NULL; RPC_CSTR pa = NULL; char conv[512];
    REQUIRE(U(RpcServerInqDefaultPrincNameW));
    if (U(RpcServerInqDefaultPrincNameW)(RPC_C_AUTHN_WINNT, &pw) != RPC_S_OK) { t_skip("no WINNT principal"); return; }
    EQ(U(RpcServerInqDefaultPrincNameA)(RPC_C_AUTHN_WINNT, &pa), RPC_S_OK);
    if (pa) {
        WideCharToMultiByte(CP_ACP, 0, (WCHAR *)pw, -1, conv, sizeof conv, NULL, NULL);
        EXPECT_STR((const char *)pa, conv);
        U(RpcStringFreeA)(&pa);
    }
    U(RpcStringFreeW)(&pw);
}

/* ---- client-side authentication info ------------------------------------- */
TEST(ncalrpc_sec_client_no_auth_info_by_default) {
    rawfx f; unsigned long lvl = 99, svc = 99, authz = 99; RPC_AUTH_IDENTITY_HANDLE id = NULL; RPC_WSTR sn = NULL;
    RUP(f, 0x80, 1, 0);
    EQ(U(RpcBindingInqAuthInfoW)(f.h, &sn, &lvl, &svc, &id, &authz), RPC_S_BINDING_HAS_NO_AUTH);
    raw_down(&f);
}
TEST(ncalrpc_sec_client_set_inq_roundtrip) {
    static const unsigned lv[] = { RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_LEVEL_CALL, RPC_C_AUTHN_LEVEL_PKT, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_LEVEL_PKT_PRIVACY };
    rawfx f; unsigned k;
    RUP(f, 0x81, 1, 0);
    for (k = 0; k < sizeof lv / sizeof *lv; k++) {
        unsigned long lvl = 99, svc = 99, authz = 99; RPC_WSTR sn = (RPC_WSTR)(ULONG_PTR)1; RPC_AUTH_IDENTITY_HANDLE id = NULL;
        EQ(set_auth(f.h, lv[k], RPC_C_AUTHN_WINNT), RPC_S_OK);
        EQ(U(RpcBindingInqAuthInfoW)(f.h, &sn, &lvl, &svc, &id, &authz), RPC_S_OK);
        EQ(lvl, lv[k]); EQ(svc, RPC_C_AUTHN_WINNT); EQ(authz, RPC_C_AUTHZ_NONE);
        if (sn && sn != (RPC_WSTR)(ULONG_PTR)1) U(RpcStringFreeW)(&sn);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_client_default_level_is_mapped) {
    rawfx f; unsigned long lvl = 99, svc = 99, authz = 99;
    RUP(f, 0x82, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_AUTHN_WINNT), RPC_S_OK);
    EQ(U(RpcBindingInqAuthInfoW)(f.h, NULL, &lvl, &svc, NULL, &authz), RPC_S_OK);
    t_note("authn level DEFAULT(0) stored as %lu", lvl);
    CHECK(lvl >= RPC_C_AUTHN_LEVEL_NONE && lvl <= RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
    raw_down(&f);
}
TEST(ncalrpc_sec_client_invalid_level_and_service) {
    rawfx f;
    RUP(f, 0x83, 1, 0);
    EQ(set_auth(f.h, 7, RPC_C_AUTHN_WINNT), RPC_S_UNKNOWN_AUTHN_LEVEL);
    EQ(set_auth(f.h, 0xffff, RPC_C_AUTHN_WINNT), RPC_S_UNKNOWN_AUTHN_LEVEL);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_CONNECT, 600), RPC_S_UNKNOWN_AUTHN_SERVICE);
    {
        unsigned long lvl = 0; RPC_STATUS st = U(RpcBindingInqAuthInfoW)(f.h, NULL, &lvl, NULL, NULL, NULL);
        EQ(st, RPC_S_BINDING_HAS_NO_AUTH);                 /* failed sets leave the binding unauthenticated */
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_client_set_auth_null_binding) {
    if (t_is_wine()) { t_skip("wine dereferences the NULL binding"); return; }
    EQ(U(RpcBindingSetAuthInfoW)(NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE), RPC_S_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoW)(NULL, NULL, NULL, NULL, NULL, NULL), RPC_S_INVALID_BINDING);
}
TEST(ncalrpc_sec_client_set_auth_a_and_w_equivalent) {
    rawfx f; unsigned long lvl = 0, svc = 0, authz = 9;
    RUP(f, 0x84, 1, 0);
    EQ(U(RpcBindingSetAuthInfoA)(f.h, NULL, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE), RPC_S_OK);
    EQ(U(RpcBindingInqAuthInfoA)(f.h, NULL, &lvl, &svc, NULL, &authz), RPC_S_OK);
    EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY); EQ(svc, RPC_C_AUTHN_WINNT); EQ(authz, RPC_C_AUTHZ_NONE);
    raw_down(&f);
}
TEST(ncalrpc_sec_client_qos_v1_roundtrip) {
    static const unsigned imps[] = { RPC_C_IMP_LEVEL_ANONYMOUS, RPC_C_IMP_LEVEL_IDENTIFY, RPC_C_IMP_LEVEL_IMPERSONATE, RPC_C_IMP_LEVEL_DELEGATE };
    rawfx f; unsigned k;
    RUP(f, 0x85, 1, 0);
    for (k = 0; k < sizeof imps / sizeof *imps; k++) {
        RPC_SECURITY_QOS in, out; unsigned long lvl = 0, svc = 0, authz = 0; RPC_STATUS st;
        memset(&in, 0, sizeof in); memset(&out, 0xcc, sizeof out);
        in.Version = RPC_C_SECURITY_QOS_VERSION; in.Capabilities = RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH;
        in.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC; in.ImpersonationType = imps[k];
        st = U(RpcBindingSetAuthInfoExW)(f.h, NULL, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &in);
        t_note("SetAuthInfoEx(imp=%u, MUTUAL_AUTH, DYNAMIC) -> %lu", imps[k], (unsigned long)st);
        if (st != RPC_S_OK) continue;
        out.Version = RPC_C_SECURITY_QOS_VERSION;
        st = U(RpcBindingInqAuthInfoExW)(f.h, NULL, &lvl, &svc, NULL, &authz, RPC_C_SECURITY_QOS_VERSION, &out);
        EQ(st, RPC_S_OK);
        EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY); EQ(svc, RPC_C_AUTHN_WINNT);
        EQ(out.Version, RPC_C_SECURITY_QOS_VERSION);
        EQ(out.ImpersonationType, imps[k]);
        EQ(out.IdentityTracking, RPC_C_QOS_IDENTITY_DYNAMIC);
        t_note("QOS capabilities read back as 0x%lx (set 0x%x)", (unsigned long)out.Capabilities, RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_client_qos_v2_none_type) {
    rawfx f; RPC_SECURITY_QOS_V2_W q; RPC_STATUS st; RPC_SECURITY_QOS_V2_W out; unsigned long lvl = 0, svc = 0, authz = 0;
    RUP(f, 0x86, 1, 0);
    memset(&q, 0, sizeof q);
    q.Version = 2; q.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT; q.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;
    q.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE; q.AdditionalSecurityInfoType = 0; q.u.HttpCredentials = NULL;
    st = U(RpcBindingSetAuthInfoExW)(f.h, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, (RPC_SECURITY_QOS *)&q);
    t_note("SetAuthInfoEx with QOS version 2 (no extra info) -> %lu", (unsigned long)st);
    if (st == RPC_S_OK) {
        memset(&out, 0, sizeof out); out.Version = 2;
        st = U(RpcBindingInqAuthInfoExW)(f.h, NULL, &lvl, &svc, NULL, &authz, 2, (RPC_SECURITY_QOS *)&out);
        t_note("InqAuthInfoEx with QOS version 2 -> %lu (Version=%lu)", (unsigned long)st, (unsigned long)out.Version);
        CHECK(st == RPC_S_OK);
        EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_client_qos_bad_version) {
    rawfx f; RPC_SECURITY_QOS q; RPC_STATUS st;
    RUP(f, 0x87, 1, 0);
    memset(&q, 0, sizeof q);
    q.Version = 99; q.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
    st = U(RpcBindingSetAuthInfoExW)(f.h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &q);
    t_note("SetAuthInfoEx with QOS.Version=99 -> %lu", (unsigned long)st);
    q.Version = 0;
    st = U(RpcBindingSetAuthInfoExW)(f.h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &q);
    t_note("SetAuthInfoEx with QOS.Version=0 -> %lu", (unsigned long)st);
    raw_down(&f);
}
TEST(ncalrpc_sec_client_auth_info_copied_with_binding) {
    rawfx f; RPC_BINDING_HANDLE c = NULL; unsigned long lvl = 0, svc = 0, authz = 0;
    RUP(f, 0x88, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    EQ(U(RpcBindingCopy)(f.h, &c), RPC_S_OK);
    EQ(U(RpcBindingInqAuthInfoW)(c, NULL, &lvl, &svc, NULL, &authz), RPC_S_OK);
    EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY); EQ(svc, RPC_C_AUTHN_WINNT);
    EQ(set_auth(c, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT), RPC_S_OK);    /* changing the copy leaves the original */
    EQ(U(RpcBindingInqAuthInfoW)(f.h, NULL, &lvl, NULL, NULL, NULL), RPC_S_OK);
    EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY);
    rx_unbind(c);
    raw_down(&f);
}

/* ---- authenticated calls and what the server sees ------------------------ */
TEST(ncalrpc_sec_authenticated_call_works) {
    static const unsigned lv[] = { RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_LEVEL_PKT_PRIVACY };
    rawfx f; unsigned k; ULONG n; RPC_STATUS st;
    need_winnt_server();
    RUP(f, 0x90, 1, 0);
    for (k = 0; k < sizeof lv / sizeof *lv; k++) {
        EQ(set_auth(f.h, lv[k], RPC_C_AUTHN_WINNT), RPC_S_OK);
        st = rx_raw_call(f.h, &f.r, 0, "abcdefgh", 8, NULL, 0, &n);
        EQ(st, RPC_S_OK); EQ(n, 8);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_server_sees_authn_level_and_service) {
    static const unsigned lv[] = { RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_LEVEL_PKT_PRIVACY };
    static const unsigned lv_loose[] = { RPC_C_AUTHN_LEVEL_CALL, RPC_C_AUTHN_LEVEL_PKT };
    rawfx f; unsigned k; ULONG n; DWORD r[11];
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    need_winnt_server();
    RUP(f, 0x91, 1, 0);
    for (k = 0; k < sizeof lv / sizeof *lv; k++) {
        EQ(set_auth(f.h, lv[k], RPC_C_AUTHN_WINNT), RPC_S_OK);
        EQ(rx_raw_call(f.h, &f.r, 3, NULL, 0, r, sizeof r, &n), RPC_S_OK);
        EQ(r[5], RPC_S_OK); EQ(r[1], lv[k]); EQ(r[3], RPC_C_AUTHN_WINNT);
    }
    for (k = 0; k < sizeof lv_loose / sizeof *lv_loose; k++) {
        EQ(set_auth(f.h, lv_loose[k], RPC_C_AUTHN_WINNT), RPC_S_OK);
        EQ(rx_raw_call(f.h, &f.r, 3, NULL, 0, r, sizeof r, &n), RPC_S_OK);
        EQ(r[5], RPC_S_OK);
        t_note("client level %u is seen by the server as %lu", lv_loose[k], r[1]);
        CHECK(r[1] >= lv_loose[k]);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_server_call_attributes_unauthenticated) {
    rawfx f; ULONG n; DWORD r[11];
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    RUP(f, 0x92, 1, 0);
    EQ(rx_raw_call(f.h, &f.r, 3, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[5], RPC_S_OK);
    EQ(r[0], GetCurrentProcessId());               /* ClientPID: the caller is this process */
    EQ(r[2], 1);                                   /* RpcCallClientLocality: rcclLocal */
    EQ(r[6], 3);                                   /* RPC_PROTSEQ_LRPC */
    EQ(r[4], 3);                                   /* OpNum */
    t_note("unauthenticated call: AuthenticationLevel=%lu AuthenticationService=%lu NullSession=%lu KernelMode=%lu CallStatus=%lu CallType=%lu",
           r[1], r[3], r[9], r[10], r[7], r[8]);
    raw_down(&f);
}
TEST(ncalrpc_sec_call_attributes_client_principal_name) {
    rawfx f; DWORD q[2], i; struct { DWORD st, level, svc, namelen; WCHAR name[256]; } r; ULONG n;
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    need_winnt_server();
    RUP(f, 0x93, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    q[0] = 2; q[1] = RPC_QUERY_CLIENT_PRINCIPAL_NAME;
    EQ(rx_raw_call(f.h, &f.r, 11, q, 8, &r, sizeof r, &n), RPC_S_OK);
    EQ(r.st, RPC_S_OK);
    CHECK(r.name[0] != 0);
    for (i = 0; i < 1; i++) {
        WCHAR user[256], dom[256]; DWORD un = 256;
        t_note("client principal name seen by server: %ls", r.name);
        (void)user; (void)dom; (void)un;
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_call_attributes_version_checks) {
    rawfx f; DWORD q[2]; struct { DWORD st, level, svc, namelen; WCHAR name[256]; } r; ULONG n; static const DWORD vers[] = { 0, 1, 2, 3, 99 };
    unsigned k;
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    RUP(f, 0x94, 1, 0);
    for (k = 0; k < sizeof vers / sizeof *vers; k++) {
        q[0] = vers[k]; q[1] = 0;
        EQ(rx_raw_call(f.h, &f.r, 11, q, 8, &r, sizeof r, &n), RPC_S_OK);
        if (vers[k] == 1 || vers[k] == 2) EQ(r.st, RPC_S_OK);
        else CHECK(r.st != RPC_S_OK);
        t_note("InqCallAttributes with Version=%lu -> %lu", vers[k], r.st);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_call_attributes_outside_call) {
    RPC_CALL_ATTRIBUTES_V2_W a; RPC_STATUS st;
    REQUIRE(U(RpcServerInqCallAttributesW));
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    memset(&a, 0, sizeof a); a.Version = 2;
    st = U(RpcServerInqCallAttributesW)(NULL, &a);
    CHECK(st != RPC_S_OK);
    t_note("RpcServerInqCallAttributes outside a server call -> %lu", (unsigned long)st);
}
TEST(ncalrpc_sec_authclient_in_call) {
    rawfx f; DWORD r[4]; ULONG n;
    need_winnt_server();
    RUP(f, 0x95, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    EQ(rx_raw_call(f.h, &f.r, 10, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[0], RPC_S_OK); EQ(r[1], RPC_C_AUTHN_LEVEL_PKT_INTEGRITY); EQ(r[2], RPC_C_AUTHN_WINNT);
    t_note("RpcBindingInqAuthClient authz service = %lu", r[3]);
    /* unauthenticated client */
    EQ(rx_raw_call(f.h, &f.r, 10, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    raw_down(&f);
}
TEST(ncalrpc_sec_authclient_unauthenticated) {
    rawfx f; DWORD r[4]; ULONG n;
    RUP(f, 0x96, 1, 0);
    EQ(rx_raw_call(f.h, &f.r, 10, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    CHECK(r[0] != RPC_S_OK);
    t_note("RpcBindingInqAuthClient for an unauthenticated ncalrpc call -> %lu (level %lu svc %lu)", r[0], r[1], r[2]);
    raw_down(&f);
}
TEST(ncalrpc_sec_impersonate_client_authenticated) {
    rawfx f; DWORD r[4]; ULONG n;
    need_winnt_server();
    RUP(f, 0x97, 1, 0);
    EQ(set_auth_qos(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, RPC_C_IMP_LEVEL_IMPERSONATE), RPC_S_OK);
    EQ(rx_raw_call(f.h, &f.r, 9, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[0], RPC_S_OK);        /* RpcImpersonateClient(NULL) */
    EQ(r[1], 1);               /* OpenThreadToken succeeds while impersonating */
    EQ(r[2], RPC_S_OK);        /* RpcRevertToSelf */
    EQ(r[3], 0);               /* ... and there is no thread token afterwards */
    raw_down(&f);
}
TEST(ncalrpc_sec_impersonate_client_unauthenticated) {
    rawfx f; DWORD r[4]; ULONG n;
    RUP(f, 0x98, 1, 0);
    EQ(rx_raw_call(f.h, &f.r, 9, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    t_note("RpcImpersonateClient on an unauthenticated ncalrpc call -> %lu, token=%lu, revert=%lu, after=%lu", r[0], r[1], r[2], r[3]);
    raw_down(&f);
}
TEST(ncalrpc_sec_impersonate_outside_call) {
    RPC_STATUS st;
    REQUIRE(U(RpcImpersonateClient)); REQUIRE(U(RpcRevertToSelf));
    st = U(RpcImpersonateClient)(NULL);
    CHECK(st != RPC_S_OK);
    t_note("RpcImpersonateClient(NULL) outside a call -> %lu", (unsigned long)st);
    st = U(RpcRevertToSelf)();
    t_note("RpcRevertToSelf outside impersonation -> %lu", (unsigned long)st);
}
TEST(ncalrpc_sec_impersonation_levels_identify) {
    rawfx f; DWORD r[4]; ULONG n; RPC_STATUS st;
    need_winnt_server();
    RUP(f, 0x99, 1, 0);
    st = set_auth_qos(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, RPC_C_IMP_LEVEL_IDENTIFY);
    EQ(st, RPC_S_OK);
    EQ(rx_raw_call(f.h, &f.r, 9, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    t_note("IDENTIFY-level client: impersonate=%lu opentoken=%lu", r[0], r[1]);
    CHECK(r[0] == RPC_S_OK);
    raw_down(&f);
}
TEST(ncalrpc_sec_server_from_client) {
    rawfx f; struct { DWORD st; WCHAR s[200]; } r; ULONG n; WCHAR want[96];
    RUP(f, 0x9a, 1, 0);
    EQ(rx_raw_call(f.h, &f.r, 12, NULL, 0, &r, sizeof r, &n), RPC_S_OK);
    EQ(r.st, RPC_S_OK);
    (void)want;
    /* documented: a *partially bound* server binding handle (the endpoint is not filled in) */
    CHECK(!wcsncmp(r.s, L"ncalrpc:", 8));
    t_note("RpcBindingServerFromClient -> %ls", r.s);
    raw_down(&f);
}

/* ---- security callbacks --------------------------------------------------- */
TEST(ncalrpc_sec_callback_allow_called_with_ctx) {
    rawfx f; ULONG n; RPC_STATUS st;
    rx_cb_result = RPC_S_OK; rx_cb_calls = 0; rx_cb_last_if = NULL; rx_cb_last_ctx = NULL; rx_cb_attr_st = -1;
    RUPX(f, 0xa0, 1, 0, RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH, rx_sec_cb);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_OK);
    CHECK(rx_cb_calls >= 1);
    CHECK(rx_cb_last_if == (void *)&f.r.si);          /* IfHandle is the registered server interface */
    CHECK(rx_cb_last_ctx != NULL);                    /* client binding context */
    if (!t_is_wine()) {
        EQ(rx_cb_attr_st, RPC_S_OK);                  /* RpcServerInqCallAttributes(Context) works inside the callback */
        EQ(rx_cb_attr_pid, GetCurrentProcessId());
        EQ(rx_cb_attr_local, 1);
    }
    raw_down(&f);
}
TEST(ncalrpc_sec_callback_deny) {
    rawfx f; ULONG n; RPC_STATUS st; LONG calls_before;
    rx_cb_result = RPC_S_ACCESS_DENIED; rx_cb_calls = 0;
    RUPX(f, 0xa1, 1, 0, RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH | RPC_IF_SEC_NO_CACHE, rx_sec_cb);
    calls_before = rx_raw_calls;
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_ACCESS_DENIED);                      /* callback status 5 becomes the call status */
    EQ(rx_raw_calls, calls_before);                   /* the manager routine never ran */
    CHECK(rx_cb_calls >= 1);
    rx_cb_result = RPC_S_OK;
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_OK);                                 /* flipping the callback result is honoured (SEC_NO_CACHE) */
    EQ(rx_raw_calls, calls_before + 1);
    raw_down(&f);
}
TEST(ncalrpc_sec_callback_per_call_with_no_cache) {
    rawfx f; ULONG n; int i; LONG c0;
    rx_cb_result = RPC_S_OK; rx_cb_calls = 0;
    RUPX(f, 0xa2, 1, 0, RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH | RPC_IF_SEC_NO_CACHE, rx_sec_cb);
    c0 = rx_cb_calls;
    for (i = 0; i < 5; i++) EQ(rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n), RPC_S_OK);
    EQ(rx_cb_calls - c0, 5);                          /* RPC_IF_SEC_NO_CACHE: callback runs for every call */
    raw_down(&f);
}
TEST(ncalrpc_sec_callback_cached_by_default) {
    rawfx f; ULONG n; int i; LONG c0;
    rx_cb_result = RPC_S_OK; rx_cb_calls = 0;
    RUPX(f, 0xa3, 1, 0, RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH, rx_sec_cb);
    c0 = rx_cb_calls;
    for (i = 0; i < 5; i++) EQ(rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n), RPC_S_OK);
    CHECK(rx_cb_calls - c0 >= 1);
    t_note("callback invocations for 5 calls on one binding without SEC_NO_CACHE: %ld", (long)(rx_cb_calls - c0));
    raw_down(&f);
}
TEST(ncalrpc_sec_callback_status_passthrough) {
    rawfx f; ULONG n; RPC_STATUS st;
    rx_cb_result = 1234; rx_cb_calls = 0;
    RUPX(f, 0xa4, 1, 0, RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH | RPC_IF_SEC_NO_CACHE, rx_sec_cb);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    t_note("callback returned 1234 -> call status %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    rx_cb_result = RPC_S_OK;
    raw_down(&f);
}
TEST(ncalrpc_sec_callback_unauthenticated_without_flag) {
    rawfx f; ULONG n; RPC_STATUS st;
    rx_cb_result = RPC_S_OK; rx_cb_calls = 0;
    RUPX(f, 0xa5, 1, 0, 0 /* no ALLOW_CALLBACKS_WITH_NO_AUTH */, rx_sec_cb);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    t_note("unauthenticated local call, callback without ALLOW_CALLBACKS_WITH_NO_AUTH -> status %lu, callback invoked %ld times", (unsigned long)st, (long)rx_cb_calls);
    raw_down(&f);
}
TEST(ncalrpc_sec_callback_authenticated_without_flag) {
    rawfx f; ULONG n; RPC_STATUS st;
    need_winnt_server();
    rx_cb_result = RPC_S_OK; rx_cb_calls = 0;
    RUPX(f, 0xa6, 1, 0, 0, rx_sec_cb);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_OK);
    CHECK(rx_cb_calls >= 1);                          /* an authenticated client always reaches the callback */
    if (!t_is_wine()) EQ(rx_cb_attr_level, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY);
    raw_down(&f);
}

/* ---- interface flags ------------------------------------------------------ */
TEST(ncalrpc_sec_flag_allow_local_only) {
    rawfx f; ULONG n;
    RUPX(f, 0xb0, 1, 0, RPC_IF_ALLOW_LOCAL_ONLY, NULL);
    EQ(rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n), RPC_S_OK);       /* a local ncalrpc client is allowed */
    raw_down(&f);
}
TEST(ncalrpc_sec_flag_allow_secure_only_rejects_unauthenticated) {
    rawfx f; ULONG n; RPC_STATUS st; LONG c0;
    RUPX(f, 0xb1, 1, 0, RPC_IF_ALLOW_SECURE_ONLY, NULL);
    c0 = rx_raw_calls;
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_ACCESS_DENIED);
    EQ(rx_raw_calls, c0);
    raw_down(&f);
}
TEST(ncalrpc_sec_flag_allow_secure_only_accepts_authenticated) {
    rawfx f; ULONG n; RPC_STATUS st;
    need_winnt_server();
    RUPX(f, 0xb2, 1, 0, RPC_IF_ALLOW_SECURE_ONLY, NULL);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT), RPC_S_OK);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_OK);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    st = rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_OK);
    raw_down(&f);
}
TEST(ncalrpc_sec_unauthenticated_default_allowed) {
    rawfx f; ULONG n;
    RUP(f, 0xb3, 1, 0);
    EQ(rx_raw_call(f.h, &f.r, 4, NULL, 0, NULL, 0, &n), RPC_S_OK);       /* no flags: anonymous local clients are fine */
    raw_down(&f);
}
TEST(ncalrpc_sec_ole_flag_accepted) {
    WCHAR ep[64]; rx_raw r; UUID u; RPC_STATUS st;
    rx_server_reset(); rx_ep(ep, 64); rx_raw_uuid(&u, 0xb4); rx_raw_init(&r, &u, 1, 0);
    EQ(rx_use_ep(ep), RPC_S_OK);
    st = U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&r.si, NULL, NULL, RPC_IF_OLE, 10, NULL);
    EQ(st, RPC_S_OK);
    EQ(U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&r.si, NULL, 1), RPC_S_OK);
}
TEST(ncalrpc_sec_mgmt_inq_server_princ_name) {
    rawfx f; RPC_WSTR n = NULL; RPC_STATUS st;
    REQUIRE(U(RpcMgmtInqServerPrincNameW));
    if (t_is_wine()) { t_skip("RpcMgmtInqServerPrincName is a stub in wine"); return; }
    need_winnt_server();
    RUP(f, 0xb5, 1, 0);
    st = U(RpcMgmtInqServerPrincNameW)(f.h, RPC_C_AUTHN_WINNT, &n);
    t_note("RpcMgmtInqServerPrincName(WINNT) -> %lu (%ls)", (unsigned long)st, n ? (WCHAR *)n : L"");
    if (n) U(RpcStringFreeW)(&n);
    raw_down(&f);
}
