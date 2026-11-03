/* gap_04: server-side inquiry/impersonation exports that need a call in flight (or a deliberate absence of one):
 * RpcServerInqBindingHandle, RpcServerInqCallAttributesA, RpcImpersonateClientContainer,
 * RpcRevertContainerImpersonation, RpcRevertToSelfEx, RpcServerCompleteSecurityCallback.
 * Expected values come from the Win10 rpcrt4 decompilation (see comments per test). */
#include "rawfx.h"

UDECL(RPC_STATUS, RpcImpersonateClientContainer, (RPC_BINDING_HANDLE));
UDECL(RPC_STATUS, RpcRevertContainerImpersonation, (void));
UDECL(RPC_STATUS, RpcRevertToSelfEx, (RPC_BINDING_HANDLE));
UDECL(RPC_STATUS, RpcServerCompleteSecurityCallback, (RPC_BINDING_HANDLE, RPC_STATUS));

#define G4_NO_CALL_ACTIVE 1725   /* RPC_S_NO_CALL_ACTIVE = 0x6bd */
#define G4_INVALID_BINDING 1702  /* RPC_S_INVALID_BINDING = 0x6a6 */

static RPC_STATUS set_auth_qos(RPC_BINDING_HANDLE h, unsigned level, unsigned svc, unsigned imp) {
    RPC_SECURITY_QOS q; memset(&q, 0, sizeof q);
    q.Version = RPC_C_SECURITY_QOS_VERSION; q.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    q.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC; q.ImpersonationType = imp;
    return U(RpcBindingSetAuthInfoExW)(h, NULL, level, svc, NULL, RPC_C_AUTHZ_NONE, &q);
}
static RPC_STATUS set_auth(RPC_BINDING_HANDLE h, unsigned level, unsigned svc) {
    return U(RpcBindingSetAuthInfoW)(h, NULL, level, svc, NULL, RPC_C_AUTHZ_NONE);
}
static void need_winnt_server(void) { U(RpcServerRegisterAuthInfoW)(NULL, RPC_C_AUTHN_WINNT, NULL, NULL); }

/* ---- RpcServerInqBindingHandle --------------------------------------------------------------- */
TEST(gap4_inqbindinghandle_outside_call) {
    RPC_BINDING_HANDLE h = (RPC_BINDING_HANDLE)(ULONG_PTR)0xdeadbeef;
    REQUIRE(U(RpcServerInqBindingHandle));
    /* the calling thread has no server call: RPC_S_NO_CALL_ACTIVE, output untouched */
    EQ(U(RpcServerInqBindingHandle)(&h), G4_NO_CALL_ACTIVE);
    CHECK(h == (RPC_BINDING_HANDLE)(ULONG_PTR)0xdeadbeef);
}
TEST(gap4_inqbindinghandle_outside_call_repeatable) {
    int i;
    REQUIRE(U(RpcServerInqBindingHandle));
    for (i = 0; i < 5; i++) { RPC_BINDING_HANDLE h = NULL; EQ(U(RpcServerInqBindingHandle)(&h), G4_NO_CALL_ACTIVE); CHECK(h == NULL); }
}
TEST(gap4_inqbindinghandle_in_call) {
    rawfx f; DWORD r[6]; ULONG n = 0;
    REQUIRE(U(RpcServerInqBindingHandle)); REQUIRE(U(RpcBindingToStringBindingW));
    RUP(f, 0xA1, 1, 0);
    memset(r, 0xCC, sizeof r);
    EQ(rx_raw_call(f.h, &f.r, 13, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(n, sizeof r);
    EQ(r[0], RPC_S_OK);                 /* first inquiry succeeds inside the call */
    EQ(r[2], RPC_S_OK);                 /* ... and again */
    EQ(r[3], 1);                        /* both inquiries return the same handle for one call */
    t_note("InqBindingHandle == RPC_MESSAGE.Handle: %lu; ToStringBinding(call handle) -> %lu (ncalrpc prefix: %lu)", r[1], r[4], r[5]);
    raw_down(&f);
}
TEST(gap4_inqbindinghandle_in_call_many) {
    rawfx f; DWORD r[6]; ULONG n; int i, bad = 0;
    REQUIRE(U(RpcServerInqBindingHandle));
    RUP(f, 0xA2, 1, 0);
    for (i = 0; i < 20; i++) {
        memset(r, 0xCC, sizeof r);
        if (rx_raw_call(f.h, &f.r, 13, NULL, 0, r, sizeof r, &n) != RPC_S_OK || r[0] != RPC_S_OK || r[2] != RPC_S_OK || r[3] != 1) bad++;
    }
    EQ(bad, 0);
    raw_down(&f);
}

/* ---- RpcServerInqCallAttributesA ---------------------------------------------------------------- */
typedef struct { DWORD st, clen, slen, level, svc, local, pid, canary_ok; char cname[160]; char sname[160]; } attrsA_reply;
static RPC_STATUS call_attrsA(rawfx *f, DWORD ver, DWORD flags, DWORD ccap, DWORD scap, attrsA_reply *r) {
    DWORD q[4]; ULONG n = 0; RPC_STATUS st;
    q[0] = ver; q[1] = flags; q[2] = ccap; q[3] = scap;
    memset(r, 0, sizeof *r);
    st = rx_raw_call(f->h, &f->r, 14, q, sizeof q, r, sizeof *r, &n);
    return st;
}
TEST(gap4_inqcallattrsA_unauthenticated_basic) {
    rawfx f; attrsA_reply r;
    REQUIRE(U(RpcServerInqCallAttributesA));
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    RUP(f, 0xA3, 1, 0);
    EQ(call_attrsA(&f, 2, 0, 160, 160, &r), RPC_S_OK);
    EQ(r.st, RPC_S_OK);
    EQ(r.pid, GetCurrentProcessId());            /* ClientPID: this process */
    EQ(r.local, 1);                              /* rcclLocal */
    CHECK(r.canary_ok);
    t_note("A call attributes (no auth): level=%lu svc=%lu", r.level, r.svc);
    raw_down(&f);
}
TEST(gap4_inqcallattrsA_versions) {
    static const DWORD vers[] = { 0, 1, 2, 3, 99, 0xFFFFFFFF };
    rawfx f; attrsA_reply r; unsigned k;
    REQUIRE(U(RpcServerInqCallAttributesA));
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    RUP(f, 0xA4, 1, 0);
    for (k = 0; k < sizeof vers / sizeof *vers; k++) {
        EQ(call_attrsA(&f, vers[k], 0, 160, 160, &r), RPC_S_OK);
        if (vers[k] == 1 || vers[k] == 2) EQ(r.st, RPC_S_OK); else CHECKF(r.st != RPC_S_OK, "version %lu accepted", vers[k]);
        t_note("InqCallAttributesA Version=%lu -> %lu", vers[k], r.st);
        CHECK(r.canary_ok);
    }
    raw_down(&f);
}
TEST(gap4_inqcallattrsA_client_principal_name) {
    rawfx f; attrsA_reply r;
    REQUIRE(U(RpcServerInqCallAttributesA));
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    need_winnt_server();
    RUP(f, 0xA5, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    EQ(call_attrsA(&f, 2, RPC_QUERY_CLIENT_PRINCIPAL_NAME, 160, 160, &r), RPC_S_OK);
    EQ(r.st, RPC_S_OK);
    CHECK(r.cname[0] != 0);
    CHECK(r.cname[159] == 0x7f || r.cname[159] == 0);
    CHECKF(r.clen == strlen(r.cname) + 1, "length %lu name \"%s\"", r.clen, r.cname);   /* characters including the NUL */
    CHECK(r.canary_ok);
    EQ(r.level, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY);
    EQ(r.svc, RPC_C_AUTHN_WINNT);
    t_note("client principal (A): %s", r.cname);
    raw_down(&f);
}
TEST(gap4_inqcallattrsA_matches_W) {
    rawfx f; attrsA_reply a; struct { DWORD st, level, svc, namelen; WCHAR name[256]; } w; DWORD q[2]; ULONG n; char conv[512];
    REQUIRE(U(RpcServerInqCallAttributesA));
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    need_winnt_server();
    RUP(f, 0xA6, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    EQ(call_attrsA(&f, 2, RPC_QUERY_CLIENT_PRINCIPAL_NAME, 160, 160, &a), RPC_S_OK);
    q[0] = 2; q[1] = RPC_QUERY_CLIENT_PRINCIPAL_NAME;
    EQ(rx_raw_call(f.h, &f.r, 11, q, 8, &w, sizeof w, &n), RPC_S_OK);
    EQ(a.st, RPC_S_OK); EQ(w.st, RPC_S_OK);
    memset(conv, 0, sizeof conv);
    WideCharToMultiByte(CP_ACP, 0, w.name, -1, conv, sizeof conv, NULL, NULL);
    EXPECT_STR(a.cname, conv);                    /* A is the ANSI conversion of W */
    EQ(a.level, w.level);
    EQ(a.svc, w.svc);
    raw_down(&f);
}
TEST(gap4_inqcallattrsA_buffer_too_small) {
    rawfx f; attrsA_reply full, r; static const DWORD caps[] = { 0, 1, 2, 3 };
    unsigned k;
    REQUIRE(U(RpcServerInqCallAttributesA));
    if (t_is_wine()) { t_skip("RpcServerInqCallAttributes unimplemented in wine"); return; }
    need_winnt_server();
    RUP(f, 0xA7, 1, 0);
    EQ(set_auth(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT), RPC_S_OK);
    EQ(call_attrsA(&f, 2, RPC_QUERY_CLIENT_PRINCIPAL_NAME, 160, 160, &full), RPC_S_OK);
    EQ(full.st, RPC_S_OK);
    for (k = 0; k < sizeof caps / sizeof *caps; k++) {
        DWORD need = (DWORD)strlen(full.cname) + 1;
        if (caps[k] >= need) continue;
        EQ(call_attrsA(&f, 2, RPC_QUERY_CLIENT_PRINCIPAL_NAME, caps[k], 160, &r), RPC_S_OK);
        EQ(r.st, ERROR_MORE_DATA);                // 234: the required length is reported
        EQ(r.clen, need);                         // characters incl. the NUL
        CHECK(r.canary_ok);
        CHECKF(r.cname[0] == 0x7f, "buffer written despite ERROR_MORE_DATA (cap %lu)", caps[k]);
    }
    /* exact size works, size-1 does not */
    {
        DWORD need = (DWORD)strlen(full.cname) + 1;
        EQ(call_attrsA(&f, 2, RPC_QUERY_CLIENT_PRINCIPAL_NAME, need, 160, &r), RPC_S_OK);
        EQ(r.st, RPC_S_OK); EXPECT_STR(r.cname, full.cname); EQ(r.clen, need); CHECK(r.canary_ok);
        EQ(call_attrsA(&f, 2, RPC_QUERY_CLIENT_PRINCIPAL_NAME, need - 1, 160, &r), RPC_S_OK);
        EQ(r.st, ERROR_MORE_DATA); EQ(r.clen, need); CHECK(r.canary_ok);
    }
    raw_down(&f);
}

/* ---- RevertToSelfEx / ImpersonateClientContainer / RevertContainerImpersonation -------------------- */
TEST(gap4_reverttoselfex_outside_call) {
    REQUIRE(UN(RpcRevertToSelfEx));
    EQ(UN(RpcRevertToSelfEx)(NULL), G4_NO_CALL_ACTIVE);     /* no handle given, no call on this thread */
}
TEST(gap4_reverttoselfex_client_binding_handle) {
    RPC_BINDING_HANDLE h; WCHAR ep[64];
    REQUIRE(UN(RpcRevertToSelfEx)); REQUIRE(U(RpcBindingFromStringBindingW));
    rx_ep_unique(ep, 64);
    h = rx_bind(ep); CHECK(h != NULL);
    if (!h) return;
    /* a client-side binding handle is not a server call: rejected by the handle-type check */
    EQ(UN(RpcRevertToSelfEx)(h), G4_INVALID_BINDING);
    rx_unbind(h);
}
TEST(gap4_reverttoselfex_copied_binding_handle) {
    /* a copy of a client binding is still a client binding, not a server call */
    RPC_BINDING_HANDLE h, h2 = NULL; WCHAR ep[64];
    REQUIRE(UN(RpcRevertToSelfEx)); REQUIRE(U(RpcBindingCopy));
    rx_ep_unique(ep, 64);
    h = rx_bind(ep); CHECK(h != NULL);
    if (!h) return;
    EQ(U(RpcBindingCopy)(h, &h2), RPC_S_OK);
    EQ(UN(RpcRevertToSelfEx)(h2), G4_INVALID_BINDING);
    rx_unbind(h2); rx_unbind(h);
}
TEST(gap4_reverttoselfex_in_call) {
    rawfx f; DWORD r[5]; ULONG n;
    REQUIRE(UN(RpcRevertToSelfEx));
    need_winnt_server();
    RUP(f, 0xA8, 1, 0);
    EQ(set_auth_qos(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, RPC_C_IMP_LEVEL_IMPERSONATE), RPC_S_OK);
    EQ(rx_raw_call(f.h, &f.r, 16, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[0], RPC_S_OK);        /* RpcImpersonateClient(NULL) */
    EQ(r[1], 1);               /* thread token present while impersonating */
    EQ(r[2], RPC_S_OK);        /* RpcRevertToSelfEx(NULL) reverts the call's impersonation */
    EQ(r[3], 0);               /* ... no thread token afterwards */
    t_note("RpcRevertToSelfEx(NULL) with nothing impersonated -> %lu", r[4]);
    raw_down(&f);
}
TEST(gap4_container_and_revert_in_call) {
    rawfx f; DWORD r[9]; ULONG n;
    REQUIRE(UN(RpcRevertToSelfEx)); REQUIRE(UN(RpcImpersonateClientContainer)); REQUIRE(UN(RpcRevertContainerImpersonation));
    need_winnt_server();
    RUP(f, 0xA9, 1, 0);
    EQ(set_auth_qos(f.h, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, RPC_C_IMP_LEVEL_IMPERSONATE), RPC_S_OK);
    EQ(rx_raw_call(f.h, &f.r, 15, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[3], RPC_S_OK);        /* RpcImpersonateClient(NULL) */
    EQ(r[4], 1);               /* thread token present */
    EQ(r[5], RPC_S_OK);        /* RpcRevertToSelfEx(call handle) */
    EQ(r[6], 0);               /* thread token gone */
    t_note("RpcImpersonateClientContainer(NULL) in a non-container call -> %lu", r[0]);
    t_note("RpcRevertContainerImpersonation() without container impersonation -> %lu", r[1]);
    t_note("RpcRevertToSelfEx(NULL) with nothing impersonated -> %lu", r[2]);
    t_note("RpcImpersonateClientContainer(call handle) -> %lu; RpcRevertContainerImpersonation() -> %lu", r[7], r[8]);
    /* an implicit-handle and an explicit call-handle form must agree */
    CHECK(r[7] == r[0]);
    raw_down(&f);
}
TEST(gap4_container_unauthenticated_call_agrees_with_impersonate_client) {
    rawfx f; DWORD r[9]; ULONG n;
    REQUIRE(UN(RpcImpersonateClientContainer));
    RUP(f, 0xAA, 1, 0);
    EQ(rx_raw_call(f.h, &f.r, 15, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    t_note("unauthenticated: ImpersonateClientContainer=%lu RevertContainer=%lu RevertToSelfEx(NULL)=%lu ImpersonateClient=%lu token=%lu RevertEx(h)=%lu",
           r[0], r[1], r[2], r[3], r[4], r[5]);
    CHECK(r[7] == r[0]);
    raw_down(&f);
}
TEST(gap4_impersonatecontainer_outside_call) {
    REQUIRE(UN(RpcImpersonateClientContainer));
    EQ(UN(RpcImpersonateClientContainer)(NULL), G4_NO_CALL_ACTIVE);
}
TEST(gap4_impersonatecontainer_client_binding_handle) {
    RPC_BINDING_HANDLE h; WCHAR ep[64];
    REQUIRE(UN(RpcImpersonateClientContainer));
    rx_ep_unique(ep, 64);
    h = rx_bind(ep); CHECK(h != NULL);
    if (!h) return;
    EQ(UN(RpcImpersonateClientContainer)(h), G4_INVALID_BINDING);
    rx_unbind(h);
}
TEST(gap4_revertcontainer_outside_call) {
    REQUIRE(UN(RpcRevertContainerImpersonation));
    EQ(UN(RpcRevertContainerImpersonation)(), G4_NO_CALL_ACTIVE);
    EQ(UN(RpcRevertContainerImpersonation)(), G4_NO_CALL_ACTIVE);       /* idempotent */
}
TEST(gap4_container_outside_call_thread_has_no_token) {
    HANDLE tok = NULL;
    REQUIRE(UN(RpcImpersonateClientContainer));
    UN(RpcImpersonateClientContainer)(NULL);
    UN(RpcRevertContainerImpersonation)();
    CHECK(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok));   /* failed calls leave no impersonation */
    if (tok) CloseHandle(tok);
}

/* ---- RpcServerCompleteSecurityCallback --------------------------------------------------------------- */
TEST(gap4_completesecuritycallback_null_handle) {
    REQUIRE(UN(RpcServerCompleteSecurityCallback));
    EQ(UN(RpcServerCompleteSecurityCallback)(NULL, RPC_S_OK), G4_INVALID_BINDING);
    EQ(UN(RpcServerCompleteSecurityCallback)(NULL, RPC_S_ACCESS_DENIED), G4_INVALID_BINDING);
}
TEST(gap4_completesecuritycallback_client_binding_handle) {
    RPC_BINDING_HANDLE h; WCHAR ep[64];
    REQUIRE(UN(RpcServerCompleteSecurityCallback));
    rx_ep_unique(ep, 64);
    h = rx_bind(ep); CHECK(h != NULL);
    if (!h) return;
    EQ(UN(RpcServerCompleteSecurityCallback)(h, RPC_S_OK), G4_INVALID_BINDING);
    EQ(UN(RpcServerCompleteSecurityCallback)(h, 1), G4_INVALID_BINDING);
    EQ(UN(RpcServerCompleteSecurityCallback)(h, 0xFFFFFFFF), G4_INVALID_BINDING);
    rx_unbind(h);
}
