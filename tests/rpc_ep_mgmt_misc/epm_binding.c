/* Binding-handle validation, client auth info round-trips, principal names, protect levels, cancel, stats,
 * notification and authorization-context argument checks. Statuses from the Windows 10 22H2 rpcrt4 decompilation.
 * "fake" handles are zeroed memory (valid to read, wrong signature): the runtime must answer RPC_S_INVALID_BINDING. */
#include "epx.h"

#define FAKE ((RPC_BINDING_HANDLE)epx_fake)
UDECL(RPC_STATUS, RpcBindingInqMaxCalls, (RPC_BINDING_HANDLE, unsigned *));

/* ---------------------------------------------------------------- export presence */
static const char *const g_names[] = {
#include "names.inc"
};
TEST(epm_binding_exports_resolve_in_rpcrt4) {
    unsigned i, miss = 0;
    HMODULE m = t_module("rpcrt4.dll");
    CHECK(m != NULL);
    for (i = 0; i < sizeof g_names / sizeof g_names[0]; i++) {
        FARPROC p = GetProcAddress(m, g_names[i]);
        if (!p) { miss++; if (t_is_wine()) t_note("not exported by this emulator: %s", g_names[i]); else CHECKF(0, "rpcrt4 does not export %s", g_names[i]); }
    }
    CHECK(miss == 0 || t_is_wine());
    CHECK(sizeof g_names / sizeof g_names[0] > 200);
}
TEST(epm_binding_exports_absent_names_fail) {
    HMODULE m = t_module("rpcrt4.dll");
    static const char *const bad[] = { "RpcMgmtEpUnregisterX", "RpcEpRegister", "RpcSsAllocateX", "I_RpcAllocateEx", "RpcNsBindingImportBegin", "RpcNsBindingLookupBegin", "RpcNsEntryObjectInqBeginA" };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        CHECKF(GetProcAddress(m, bad[i]) == NULL, "%s unexpectedly exported", bad[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}

/* ---------------------------------------------------------------- invalid handle matrix */
TEST(epm_binding_free_rejects_null_and_fake) {
    RPC_BINDING_HANDLE h = NULL;
    REQUIRE(U(RpcBindingFree));
    WINE_STUB();
    EQ(U(RpcBindingFree)(&h), EPX_INVALID_BINDING);
    h = FAKE;
    EQ(U(RpcBindingFree)(&h), EPX_INVALID_BINDING);
    CHECK(h == FAKE);            /* not cleared on failure */
}

TEST(epm_binding_free_clears_handle) {
    RPC_BINDING_HANDLE h;
    REQUIRE(U(RpcBindingFree));
    WINE_STUB();
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    EQ(U(RpcBindingFree)(&h), RPC_S_OK);
    CHECK(h == NULL);
    EQ(U(RpcBindingFree)(&h), EPX_INVALID_BINDING);     /* double free */
}

TEST(epm_binding_bind_unbind_reject_invalid) {
    REQUIRE(U(RpcBindingBind) && U(RpcBindingUnbind));
    EQ(U(RpcBindingUnbind)(NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingUnbind)(FAKE), EPX_INVALID_BINDING);
    EQ(U(RpcBindingBind)(NULL, NULL, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingBind)(NULL, FAKE, NULL), EPX_INVALID_BINDING);
}

TEST(epm_binding_inq_auth_info_rejects_invalid) {
    unsigned long lvl = 0xAA;
    REQUIRE(U(RpcBindingInqAuthInfoW) && U(RpcBindingInqAuthInfoA) && U(RpcBindingInqAuthInfoExW) && U(RpcBindingInqAuthInfoExA));
    WINE_STUB();
    EQ(U(RpcBindingInqAuthInfoW)(NULL, NULL, &lvl, NULL, NULL, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoW)(FAKE, NULL, &lvl, NULL, NULL, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoA)(NULL, NULL, &lvl, NULL, NULL, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoA)(FAKE, NULL, &lvl, NULL, NULL, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoExW)(NULL, NULL, &lvl, NULL, NULL, NULL, 0, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoExW)(FAKE, NULL, &lvl, NULL, NULL, NULL, 0, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoExA)(NULL, NULL, &lvl, NULL, NULL, NULL, 0, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthInfoExA)(FAKE, NULL, &lvl, NULL, NULL, NULL, 0, NULL), EPX_INVALID_BINDING);
    EXPECT_EQ(lvl, 0xAA);
}

TEST(epm_binding_inq_auth_client_without_call) {
    RPC_AUTHZ_HANDLE pr; ULONG a = 0xAA, b = 0xAA, c = 0xAA; RPC_WSTR sw; RPC_CSTR sa;
    REQUIRE(U(RpcBindingInqAuthClientW) && U(RpcBindingInqAuthClientA) && U(RpcBindingInqAuthClientExW) && U(RpcBindingInqAuthClientExA));
    WINE_STUB();
    /* NULL = "the client of the current call": none on this thread */
    EQ(U(RpcBindingInqAuthClientW)(NULL, &pr, &sw, &a, &b, &c), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcBindingInqAuthClientA)(NULL, &pr, &sa, &a, &b, &c), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcBindingInqAuthClientExW)(NULL, &pr, &sw, &a, &b, &c, 0), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcBindingInqAuthClientExA)(NULL, &pr, &sa, &a, &b, &c, 0), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcBindingInqAuthClientW)(FAKE, &pr, &sw, &a, &b, &c), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthClientA)(FAKE, &pr, &sa, &a, &b, &c), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthClientExW)(FAKE, &pr, &sw, &a, &b, &c, 0), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqAuthClientExA)(FAKE, &pr, &sa, &a, &b, &c, 0), EPX_INVALID_BINDING);
    EXPECT_EQ(a, 0xAA); EXPECT_EQ(b, 0xAA); EXPECT_EQ(c, 0xAA);
}

TEST(epm_binding_inq_auth_client_rejects_client_side_binding) {
    /* a client string binding is not a server-side call handle */
    RPC_BINDING_HANDLE h; RPC_AUTHZ_HANDLE pr; ULONG a, b, c; RPC_STATUS st;
    REQUIRE(U(RpcBindingInqAuthClientW));
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    st = U(RpcBindingInqAuthClientW)(h, &pr, NULL, &a, &b, &c);
    t_note("RpcBindingInqAuthClientW(client binding) = %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    U(RpcBindingFree)(&h);
}

TEST(epm_binding_server_from_client_invalid) {
    RPC_BINDING_HANDLE out = (RPC_BINDING_HANDLE)(ULONG_PTR)0x99;
    REQUIRE(U(RpcBindingServerFromClient));
    WINE_STUB();
    EQ(U(RpcBindingServerFromClient)(NULL, &out), EPX_NO_CALL_ACTIVE);
    CHECK(out == NULL);                    /* the out parameter is cleared on every failure */
    out = (RPC_BINDING_HANDLE)(ULONG_PTR)0x99;
    EQ(U(RpcBindingServerFromClient)(FAKE, &out), EPX_INVALID_BINDING);
    CHECK(out == NULL);
}

TEST(epm_binding_inq_com_timeout_invalid_and_default) {
    unsigned t = 0xAA; RPC_BINDING_HANDLE h;
    REQUIRE(U(RpcMgmtInqComTimeout));
    WINE_STUB();
    EQ(U(RpcMgmtInqComTimeout)(NULL, &t), EPX_INVALID_BINDING);
    EQ(U(RpcMgmtInqComTimeout)(FAKE, &t), EPX_INVALID_BINDING);
    EXPECT_EQ(t, 0xAA);
    h = epx_mkbind();
    if (h) {
        EQ(U(RpcMgmtInqComTimeout)(h, &t), RPC_S_OK);
        EXPECT_EQ(t, RPC_C_BINDING_DEFAULT_TIMEOUT);
        U(RpcBindingFree)(&h);
    }
}

TEST(epm_binding_inq_max_calls_invalid) {
    unsigned n = 0xAA;
    REQUIRE(UN(RpcBindingInqMaxCalls));
    WINE_STUB();
    EQ(UN(RpcBindingInqMaxCalls)(NULL, &n), EPX_INVALID_BINDING);
    EQ(UN(RpcBindingInqMaxCalls)(FAKE, &n), EPX_INVALID_BINDING);
    EXPECT_EQ(n, 0xAA);
}

TEST(epm_binding_i_rpc_binding_copy) {
    RPC_BINDING_HANDLE h, c = NULL;
    REQUIRE(U(I_RpcBindingCopy));
    WINE_STUB();
    EQ(U(I_RpcBindingCopy)(NULL, &c), EPX_INVALID_BINDING);
    EQ(U(I_RpcBindingCopy)(FAKE, &c), EPX_INVALID_BINDING);
    CHECK(c == NULL);
    h = epx_mkbind();
    if (h) {
        RPC_STATUS st = U(I_RpcBindingCopy)(h, &c);
        EXPECT_EQ(st, RPC_S_OK);
        if (st == RPC_S_OK) {
            CHECK(c != NULL && c != h);
            RPC_WSTR s1 = NULL, s2 = NULL;
            if (U(RpcBindingToStringBindingW)(h, &s1) == RPC_S_OK && U(RpcBindingToStringBindingW)(c, &s2) == RPC_S_OK) EXPECT_WSTR((WCHAR *)s1, (WCHAR *)s2);
            if (s1) U(RpcStringFreeW)(&s1);
            if (s2) U(RpcStringFreeW)(&s2);
            EQ(U(RpcBindingFree)(&c), RPC_S_OK);
        }
        U(RpcBindingFree)(&h);
    }
}

TEST(epm_binding_inq_option_and_set_option_invalid) {
    ULONG_PTR v = 0xAA;
    REQUIRE(U(RpcBindingInqOption) && U(RpcBindingSetOption));
    WINE_STUB();
    EQ(U(RpcBindingInqOption)(NULL, 0, &v), EPX_INVALID_BINDING);
    EQ(U(RpcBindingInqOption)(FAKE, 0, &v), EPX_INVALID_BINDING);
    EQ(U(RpcBindingSetOption)(FAKE, 1, 0), EPX_INVALID_BINDING);
    EQ(U(RpcBindingSetOption)(NULL, 13, 0), RPC_S_INVALID_ARG);    /* option 13 with value 0 is rejected before the handle is looked at */
    EQ(U(RpcBindingSetOption)(FAKE, 13, 0), RPC_S_INVALID_ARG);
}

TEST(epm_binding_inq_option_rejects_out_of_range_option) {
    RPC_BINDING_HANDLE h; ULONG_PTR v = 0xAA; unsigned o;
    REQUIRE(U(RpcBindingInqOption));
    WINE_STUB();
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    for (o = 17; o < 24; o++) EQ(U(RpcBindingInqOption)(h, o, &v), RPC_S_INVALID_ARG);
    EQ(U(RpcBindingInqOption)(h, 0xFFFFFFFFu, &v), RPC_S_INVALID_ARG);
    U(RpcBindingFree)(&h);
}

/* ---------------------------------------------------------------- client auth info round-trip */
TEST(epm_binding_auth_info_none_by_default) {
    RPC_BINDING_HANDLE h; unsigned long lvl = 0xAA, svc = 0xAA, az = 0xAA;
    REQUIRE(U(RpcBindingInqAuthInfoW));
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    EQ(U(RpcBindingInqAuthInfoW)(h, NULL, &lvl, &svc, NULL, &az), EPX_BINDING_HAS_NO_AUTH);
    EXPECT_EQ(lvl, 0xAA);
    U(RpcBindingFree)(&h);
}

TEST(epm_binding_auth_info_set_inq_roundtrip_w) {
    RPC_BINDING_HANDLE h; unsigned long lvl = 0xAA, svc = 0xAA, az = 0xAA; RPC_WSTR pn = NULL; RPC_AUTH_IDENTITY_HANDLE id = (void *)1;
    RPC_STATUS st;
    REQUIRE(U(RpcBindingInqAuthInfoW) && U(RpcBindingSetAuthInfoW));
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    st = U(RpcBindingSetAuthInfoW)(h, (RPC_WSTR)L"olet/princ", RPC_C_AUTHN_LEVEL_PKT_INTEGRITY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE);
    EXPECT_EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        st = U(RpcBindingInqAuthInfoW)(h, &pn, &lvl, &svc, &id, &az);
        EXPECT_EQ(st, RPC_S_OK);
        EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY);
        EXPECT_EQ(svc, RPC_C_AUTHN_WINNT);
        EXPECT_EQ(az, RPC_C_AUTHZ_NONE);
        CHECK(pn != NULL);
        if (pn) { EXPECT_WSTR((WCHAR *)pn, L"olet/princ"); U(RpcStringFreeW)(&pn); }
        t_note("identity handle returned for NULL identity: %p", id);
    }
    U(RpcBindingFree)(&h);
}

TEST(epm_binding_auth_info_a_w_parity) {
    RPC_BINDING_HANDLE h; unsigned long lvl = 0, svc = 0, az = 0; RPC_CSTR pn = NULL; RPC_STATUS st;
    REQUIRE(U(RpcBindingInqAuthInfoA) && U(RpcBindingSetAuthInfoA));
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    st = U(RpcBindingSetAuthInfoA)(h, (RPC_CSTR)"olet/princ_a", RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE);
    EXPECT_EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        EQ(U(RpcBindingInqAuthInfoA)(h, &pn, &lvl, &svc, NULL, &az), RPC_S_OK);
        EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_CONNECT);
        EXPECT_EQ(svc, RPC_C_AUTHN_WINNT);
        CHECK(pn != NULL);
        if (pn) { EXPECT_STR((char *)pn, "olet/princ_a"); U(RpcStringFreeA)(&pn); }
    }
    U(RpcBindingFree)(&h);
}

TEST(epm_binding_auth_info_null_outputs_allowed) {
    RPC_BINDING_HANDLE h;
    REQUIRE(U(RpcBindingInqAuthInfoW) && U(RpcBindingSetAuthInfoW));
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    if (U(RpcBindingSetAuthInfoW)(h, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE) == RPC_S_OK)
        EQ(U(RpcBindingInqAuthInfoW)(h, NULL, NULL, NULL, NULL, NULL), RPC_S_OK);
    U(RpcBindingFree)(&h);
}

TEST(epm_binding_auth_info_ex_qos_roundtrip) {
    RPC_BINDING_HANDLE h; RPC_SECURITY_QOS set, got; unsigned long lvl = 0, svc = 0; RPC_STATUS st;
    REQUIRE(U(RpcBindingInqAuthInfoExW) && U(RpcBindingSetAuthInfoExW));
    WINE_STUB();
    h = epx_mkbind();
    CHECK(h != NULL);
    if (!h) return;
    memset(&set, 0, sizeof set);
    set.Version = RPC_C_SECURITY_QOS_VERSION;
    set.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    set.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC;
    set.ImpersonationType = RPC_C_IMP_LEVEL_IDENTIFY;
    st = U(RpcBindingSetAuthInfoExW)(h, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_AUTHN_WINNT, NULL, RPC_C_AUTHZ_NONE, &set);
    EXPECT_EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        memset(&got, 0xCC, sizeof got);
        st = U(RpcBindingInqAuthInfoExW)(h, NULL, &lvl, &svc, NULL, NULL, RPC_C_SECURITY_QOS_VERSION, &got);
        EXPECT_EQ(st, RPC_S_OK);
        EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
        EXPECT_EQ(svc, RPC_C_AUTHN_WINNT);
        EXPECT_EQ(got.Version, RPC_C_SECURITY_QOS_VERSION);
        EXPECT_EQ(got.IdentityTracking, RPC_C_QOS_IDENTITY_DYNAMIC);
        EXPECT_EQ(got.ImpersonationType, RPC_C_IMP_LEVEL_IDENTIFY);
        EXPECT_EQ(got.Capabilities, RPC_C_QOS_CAPABILITIES_DEFAULT);
        /* asking for a QOS structure version other than 1 is refused */
        EQ(U(RpcBindingInqAuthInfoExW)(h, NULL, &lvl, &svc, NULL, NULL, 2, &got), RPC_S_INVALID_ARG);
        EQ(U(RpcBindingInqAuthInfoExW)(h, NULL, &lvl, &svc, NULL, NULL, 0, &got), RPC_S_INVALID_ARG);
        EQ(U(RpcBindingInqAuthInfoExW)(h, NULL, &lvl, &svc, NULL, NULL, 2, NULL), RPC_S_OK);   /* NULL QOS: version ignored */
    }
    U(RpcBindingFree)(&h);
}

/* ---------------------------------------------------------------- RpcBindingCreate */
static RPC_BINDING_HANDLE_TEMPLATE_V1 mk_tmpl(void) {
    RPC_BINDING_HANDLE_TEMPLATE_V1 t;
    memset(&t, 0, sizeof t);
    t.Version = 1;
    t.ProtocolSequence = 3;                 /* RPC_PROTSEQ_LRPC */
    t.StringEndpoint = (unsigned short *)L"olet_bindingcreate";
    return t;
}
TEST(epm_binding_create_w_template_validation) {
    RPC_BINDING_HANDLE h = (RPC_BINDING_HANDLE)(ULONG_PTR)0x77; RPC_BINDING_HANDLE_TEMPLATE_V1 t;
    REQUIRE(U(RpcBindingCreateW));
    WINE_STUB();
    t = mk_tmpl(); t.Version = 0;  EQ(U(RpcBindingCreateW)(&t, NULL, NULL, &h), RPC_S_INVALID_ARG);
    CHECK(h == NULL);                     /* *Binding is cleared once the arguments are being checked */
    t = mk_tmpl(); t.Version = 2;  EQ(U(RpcBindingCreateW)(&t, NULL, NULL, &h), RPC_S_INVALID_ARG);
    t = mk_tmpl(); t.Flags = 2;    EQ(U(RpcBindingCreateW)(&t, NULL, NULL, &h), RPC_S_INVALID_ARG);
    t = mk_tmpl(); t.Flags = 0x100;EQ(U(RpcBindingCreateW)(&t, NULL, NULL, &h), RPC_S_INVALID_ARG);
    t = mk_tmpl(); t.u1.Reserved = (unsigned short *)(ULONG_PTR)8; EQ(U(RpcBindingCreateW)(&t, NULL, NULL, &h), RPC_S_INVALID_ARG);
}

TEST(epm_binding_create_w_unsupported_protseqs) {
    RPC_BINDING_HANDLE h = NULL; RPC_BINDING_HANDLE_TEMPLATE_V1 t; unsigned p;
    REQUIRE(U(RpcBindingCreateW));
    WINE_STUB();
    for (p = 0; p < 8; p++) {
        t = mk_tmpl(); t.ProtocolSequence = p;
        if (p == 3) continue;           /* LRPC is the only supported sequence */
        EQ(U(RpcBindingCreateW)(&t, NULL, NULL, &h), EPX_CANNOT_SUPPORT);
        CHECK(h == NULL);
    }
}

TEST(epm_binding_create_w_security_and_options_validation) {
    RPC_BINDING_HANDLE h = NULL; RPC_BINDING_HANDLE_TEMPLATE_V1 t = mk_tmpl(); RPC_BINDING_HANDLE_SECURITY_V1 sec; RPC_BINDING_HANDLE_OPTIONS_V1 opt;
    unsigned l;
    REQUIRE(U(RpcBindingCreateW));
    WINE_STUB();
    memset(&sec, 0, sizeof sec); sec.Version = 1; sec.AuthnLevel = RPC_C_AUTHN_LEVEL_CONNECT; sec.AuthnSvc = RPC_C_AUTHN_WINNT;
    sec.Version = 0; EQ(U(RpcBindingCreateW)(&t, &sec, NULL, &h), RPC_S_INVALID_ARG);
    sec.Version = 2; EQ(U(RpcBindingCreateW)(&t, &sec, NULL, &h), RPC_S_INVALID_ARG);
    sec.Version = 1;
    sec.AuthnLevel = 0; EQ(U(RpcBindingCreateW)(&t, &sec, NULL, &h), RPC_S_INVALID_ARG);      /* levels 1..6 only */
    sec.AuthnLevel = 7; EQ(U(RpcBindingCreateW)(&t, &sec, NULL, &h), RPC_S_INVALID_ARG);
    sec.AuthnLevel = 0xFFFF; EQ(U(RpcBindingCreateW)(&t, &sec, NULL, &h), RPC_S_INVALID_ARG);
    memset(&opt, 0, sizeof opt); opt.Version = 0;
    EQ(U(RpcBindingCreateW)(&t, NULL, &opt, &h), RPC_S_INVALID_ARG);
    opt.Version = 1; opt.Flags = 8;
    EQ(U(RpcBindingCreateW)(&t, NULL, &opt, &h), RPC_S_INVALID_ARG);
    opt.Flags = 0; opt.ComTimeout = 10;
    EQ(U(RpcBindingCreateW)(&t, NULL, &opt, &h), RPC_S_INVALID_ARG);
    CHECK(h == NULL);
    (void)l;
}

TEST(epm_binding_create_w_lrpc_handle_roundtrip) {
    RPC_BINDING_HANDLE h = NULL, c = NULL; RPC_BINDING_HANDLE_TEMPLATE_V1 t = mk_tmpl(); RPC_STATUS st; RPC_WSTR s = NULL;
    REQUIRE(U(RpcBindingCreateW));
    WINE_STUB();
    st = U(RpcBindingCreateW)(&t, NULL, NULL, &h);
    EXPECT_EQ(st, RPC_S_OK);
    CHECK(h != NULL);
    if (st == RPC_S_OK && h) {
        st = U(RpcBindingToStringBindingW)(h, &s);
        t_note("RpcBindingToStringBindingW(created) = %lu \"%ls\"", (unsigned long)st, st == RPC_S_OK ? (WCHAR *)s : L"");
        if (s) U(RpcStringFreeW)(&s);
        t_note("RpcBindingBind(NULL, created, NULL) = %lu", (unsigned long)U(RpcBindingBind)(NULL, h, NULL));
        t_note("RpcBindingUnbind(created) = %lu", (unsigned long)U(RpcBindingUnbind)(h));
        EQ(U(RpcBindingFree)(&h), RPC_S_OK);
        CHECK(h == NULL);
    }
    /* with security + options */
    {
        RPC_BINDING_HANDLE_SECURITY_V1 sec; RPC_BINDING_HANDLE_OPTIONS_V1 opt;
        memset(&sec, 0, sizeof sec); sec.Version = 1; sec.AuthnLevel = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY; sec.AuthnSvc = RPC_C_AUTHN_WINNT;
        memset(&opt, 0, sizeof opt); opt.Version = 1; opt.ComTimeout = 9;
        st = U(RpcBindingCreateW)(&t, &sec, &opt, &c);
        t_note("RpcBindingCreateW with security+options = %lu", (unsigned long)st);
        if (st == RPC_S_OK && c) {
            unsigned long lvl = 0, svc = 0;
            if (U(RpcBindingInqAuthInfoW)(c, NULL, &lvl, &svc, NULL, NULL) == RPC_S_OK) { EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_PKT_INTEGRITY); EXPECT_EQ(svc, RPC_C_AUTHN_WINNT); }
            else t_note("InqAuthInfo on created handle failed");
            U(RpcBindingFree)(&c);
        }
    }
}

TEST(epm_binding_create_a_template_validation) {
    RPC_BINDING_HANDLE h = NULL; RPC_BINDING_HANDLE_TEMPLATE_V1 t = mk_tmpl();
    REQUIRE(U(RpcBindingCreateA));
    WINE_STUB();
    t.StringEndpoint = (unsigned short *)"olet_bindingcreate_a";
    t.Version = 3; EQ(U(RpcBindingCreateA)(&t, NULL, NULL, &h), RPC_S_INVALID_ARG);
    t.Version = 1; t.ProtocolSequence = 1; EQ(U(RpcBindingCreateA)(&t, NULL, NULL, &h), EPX_CANNOT_SUPPORT);
    t.ProtocolSequence = 3;
    {
        RPC_STATUS st = U(RpcBindingCreateA)(&t, NULL, NULL, &h);
        EXPECT_EQ(st, RPC_S_OK);
        if (st == RPC_S_OK) EQ(U(RpcBindingFree)(&h), RPC_S_OK);
    }
}

/* ---------------------------------------------------------------- principal names, protect level */
TEST(epm_binding_default_princ_name_unknown_service) {
    static const ULONG svc[] = { 0, 0xFFFFFFFFu /* RPC_C_AUTHN_DEFAULT */, 0x7777, 600 };
    unsigned i;
    REQUIRE(U(RpcServerInqDefaultPrincNameA) && U(RpcServerInqDefaultPrincNameW));
    WINE_STUB();
    for (i = 0; i < sizeof svc / sizeof svc[0]; i++) {
        RPC_CSTR a = (RPC_CSTR)(ULONG_PTR)0xDEADBEEF; RPC_WSTR w = (RPC_WSTR)(ULONG_PTR)0xDEADBEEF;
        EQ(U(RpcServerInqDefaultPrincNameA)(svc[i], &a), EPX_UNKNOWN_AUTHN_SERVICE);
        EQ(U(RpcServerInqDefaultPrincNameW)(svc[i], &w), EPX_UNKNOWN_AUTHN_SERVICE);
        CHECK(a == (RPC_CSTR)(ULONG_PTR)0xDEADBEEF);
        CHECK(w == (RPC_WSTR)(ULONG_PTR)0xDEADBEEF);
    }
}

TEST(epm_binding_default_princ_name_winnt_a_w_parity) {
    RPC_CSTR a = NULL; RPC_WSTR w = NULL; char conv[512];
    REQUIRE(U(RpcServerInqDefaultPrincNameA) && U(RpcServerInqDefaultPrincNameW));
    WINE_STUB();
    EQ(U(RpcServerInqDefaultPrincNameW)(RPC_C_AUTHN_WINNT, &w), RPC_S_OK);
    EQ(U(RpcServerInqDefaultPrincNameA)(RPC_C_AUTHN_WINNT, &a), RPC_S_OK);
    CHECK(w != NULL && a != NULL);
    if (w && a) {
        CHECK(lstrlenW((WCHAR *)w) > 0);
        CHECK(WideCharToMultiByte(CP_ACP, 0, (WCHAR *)w, -1, conv, sizeof conv, NULL, NULL) > 0);
        EXPECT_STR((char *)a, conv);
        t_note("default WINNT principal: %s", (char *)a);
    }
    if (w) U(RpcStringFreeW)(&w);
    if (a) U(RpcStringFreeA)(&a);
}

TEST(epm_binding_default_princ_name_negotiate_and_kerberos) {
    RPC_WSTR w; unsigned i; static const ULONG svc[] = { RPC_C_AUTHN_GSS_NEGOTIATE, RPC_C_AUTHN_GSS_KERBEROS, RPC_C_AUTHN_GSS_SCHANNEL, 30 };
    REQUIRE(U(RpcServerInqDefaultPrincNameW));
    WINE_STUB();
    for (i = 0; i < 4; i++) {
        w = NULL;
        RPC_STATUS st = U(RpcServerInqDefaultPrincNameW)(svc[i], &w);
        t_note("RpcServerInqDefaultPrincNameW(%lu) = %lu \"%ls\"", (unsigned long)svc[i], (unsigned long)st, st == 0 && w ? (WCHAR *)w : L"");
        if (st == 0 && w) U(RpcStringFreeW)(&w);
    }
}

TEST(epm_binding_protect_level_defaults) {
    ULONG lvl; static const struct { ULONG svc; } known[] = { {RPC_C_AUTHN_WINNT}, {RPC_C_AUTHN_GSS_NEGOTIATE}, {RPC_C_AUTHN_GSS_KERBEROS} };
    unsigned i;
    REQUIRE(U(RpcMgmtInqDefaultProtectLevel));
    WINE_STUB();
    for (i = 0; i < 3; i++) {
        lvl = 0xAA;
        EQ(U(RpcMgmtInqDefaultProtectLevel)(known[i].svc, &lvl), RPC_S_OK);
        EXPECT_EQ(lvl, RPC_C_AUTHN_LEVEL_CONNECT);       /* every non-DCE-private provider defaults to CONNECT */
    }
}

TEST(epm_binding_protect_level_unknown_service) {
    static const ULONG svc[] = { 0, 0x7777, 0xFFFFFFFFu, 600 };
    ULONG lvl; unsigned i;
    REQUIRE(U(RpcMgmtInqDefaultProtectLevel));
    WINE_STUB();
    for (i = 0; i < sizeof svc / sizeof svc[0]; i++) {
        lvl = 0xAA;
        EQ(U(RpcMgmtInqDefaultProtectLevel)(svc[i], &lvl), EPX_UNKNOWN_AUTHN_SERVICE);
        EXPECT_EQ(lvl, 0xAA);
    }
    lvl = 0xAA;
    {
        RPC_STATUS st = U(RpcMgmtInqDefaultProtectLevel)(RPC_C_AUTHN_DCE_PRIVATE, &lvl);
        t_note("RpcMgmtInqDefaultProtectLevel(DCE_PRIVATE) = %lu level %lu (5 = PKT_PRIVACY if a provider is registered)", (unsigned long)st, lvl);
    }
}

TEST(epm_binding_server_register_auth_info_unknown_service) {
    REQUIRE(U(RpcServerRegisterAuthInfoW) && U(RpcServerRegisterAuthInfoA));
    WINE_STUB();
    EQ(U(RpcServerRegisterAuthInfoW)(NULL, 600, NULL, NULL), EPX_UNKNOWN_AUTHN_SERVICE);
    EQ(U(RpcServerRegisterAuthInfoA)(NULL, 600, NULL, NULL), EPX_UNKNOWN_AUTHN_SERVICE);
    EQ(U(RpcServerRegisterAuthInfoW)(NULL, 0x7777, NULL, NULL), EPX_UNKNOWN_AUTHN_SERVICE);
}

/* The server-side registered principal table is process-global, so exercise it in a child process. */
CHILD(epm_princ) {
    RPC_WSTR w = NULL; RPC_CSTR a = NULL; int fail = 0;
    RPC_STATUS st;
    /* before registration nothing is known for service 10 -> unknown authn service */
    st = U(RpcMgmtInqServerPrincNameW)(NULL, RPC_C_AUTHN_WINNT, &w);
    if (st == 0 && w) { t_note("child: principal already registered at startup: %ls", (WCHAR *)w); U(RpcStringFreeW)(&w); }
    else t_note("child: RpcMgmtInqServerPrincNameW before registration = %lu", (unsigned long)st);
    w = NULL;
    if (U(RpcMgmtInqServerPrincNameW)(NULL, 0x7777, &w) != EPX_UNKNOWN_AUTHN_SERVICE) fail |= 4;
    if (U(RpcServerRegisterAuthInfoW)((RPC_WSTR)L"olet/child_princ", RPC_C_AUTHN_WINNT, NULL, NULL) != 0) fail |= 8;
    w = NULL;
    st = U(RpcMgmtInqServerPrincNameW)(NULL, RPC_C_AUTHN_WINNT, &w);
    if (st != 0 || !w || lstrcmpW((WCHAR *)w, L"olet/child_princ")) fail |= 16;
    if (w) U(RpcStringFreeW)(&w);
    st = U(RpcMgmtInqServerPrincNameA)(NULL, RPC_C_AUTHN_WINNT, &a);
    if (st != 0 || !a || strcmp((char *)a, "olet/child_princ")) fail |= 32;
    if (a) U(RpcStringFreeA)(&a);
    /* re-registering replaces the name */
    if (U(RpcServerRegisterAuthInfoA)((RPC_CSTR)"olet/second", RPC_C_AUTHN_WINNT, NULL, NULL) != 0) fail |= 64;
    w = NULL;
    st = U(RpcMgmtInqServerPrincNameW)(NULL, RPC_C_AUTHN_WINNT, &w);
    if (st != 0 || !w || lstrcmpW((WCHAR *)w, L"olet/second")) fail |= 128;
    if (w) U(RpcStringFreeW)(&w);
    return fail;
}
TEST(epm_binding_server_princ_name_registration_child) {
    REQUIRE(U(RpcMgmtInqServerPrincNameW) && U(RpcServerRegisterAuthInfoW));
    WINE_STUB();
    EQ(t_run_child("epm_princ", "", 20000), 0);
}

/* ---------------------------------------------------------------- cancel */
static DWORD WINAPI idle_thread(void *p) { Sleep(30000); return 0; }
TEST(epm_binding_cancel_thread_invalid_handle) {
    HANDLE ev;
    REQUIRE(U(RpcCancelThread) && U(RpcCancelThreadEx));
    WINE_STUB();
    EQ(U(RpcCancelThread)(NULL), ERROR_ACCESS_DENIED);
    EQ(U(RpcCancelThreadEx)(NULL, 0), ERROR_ACCESS_DENIED);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);      /* a handle that is not a thread */
    EQ(U(RpcCancelThread)(ev), ERROR_ACCESS_DENIED);
    EQ(U(RpcCancelThreadEx)(ev, 5), ERROR_ACCESS_DENIED);
    CloseHandle(ev);
}

TEST(epm_binding_cancel_thread_without_rpc_call_queues_apc) {
    HANDLE th;
    REQUIRE(U(RpcCancelThread) && U(RpcCancelThreadEx));
    WINE_STUB();
    th = CreateThread(NULL, 0, idle_thread, NULL, CREATE_SUSPENDED, NULL);
    CHECK(th != NULL);
    if (!th) return;
    /* the thread is in no RPC call: the cancel degrades to an APC and succeeds */
    EQ(U(RpcCancelThread)(th), RPC_S_OK);
    EQ(U(RpcCancelThreadEx)(th, 0), RPC_S_OK);
    EQ(U(RpcCancelThreadEx)(th, 10), RPC_S_OK);
    EQ(U(RpcCancelThreadEx)(th, -1), RPC_S_OK);
    TerminateThread(th, 0);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
}

TEST(epm_binding_cancel_thread_dead_thread_handle) {
    HANDLE th;
    REQUIRE(U(RpcCancelThread));
    WINE_STUB();
    th = CreateThread(NULL, 0, idle_thread, NULL, CREATE_SUSPENDED, NULL);
    CHECK(th != NULL);
    if (!th) return;
    TerminateThread(th, 0);
    WaitForSingleObject(th, 5000);
    /* QueueUserAPC to a terminated thread fails: mapped to ERROR_ACCESS_DENIED */
    EQ(U(RpcCancelThread)(th), ERROR_ACCESS_DENIED);
    CloseHandle(th);
}

TEST(epm_binding_set_cancel_timeout_accepts_values) {
    static const LONG v[] = { 0, 1, 30, 3600, -1 /* RPC_C_CANCEL_INFINITE_TIMEOUT */, 0x7FFFFFFF, -5 };
    unsigned i;
    REQUIRE(U(RpcMgmtSetCancelTimeout));
    WINE_STUB();
    for (i = 0; i < sizeof v / sizeof v[0]; i++) EQ(U(RpcMgmtSetCancelTimeout)(v[i]), RPC_S_OK);
    U(RpcMgmtSetCancelTimeout)(RPC_C_CANCEL_INFINITE_TIMEOUT);
}

TEST(epm_binding_server_test_cancel_without_call) {
    REQUIRE(U(RpcServerTestCancel));
    WINE_STUB();
    EQ(U(RpcServerTestCancel)(NULL), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcServerTestCancel)(FAKE), EPX_INVALID_BINDING);
}

/* ---------------------------------------------------------------- stats */
TEST(epm_binding_mgmt_inq_stats_local) {
    RPC_STATS_VECTOR *v = NULL;
    REQUIRE(U(RpcMgmtInqStats) && U(RpcMgmtStatsVectorFree));
    WINE_STUB();
    EQ(U(RpcMgmtInqStats)(NULL, &v), RPC_S_OK);
    CHECK(v != NULL);
    if (v) {
        EXPECT_EQ(v->Count, 4);            /* RPC_C_STATS_CALLS_IN, CALLS_OUT, PKTS_IN, PKTS_OUT */
        t_note("stats: calls_in=%lu calls_out=%lu pkts_in=%lu pkts_out=%lu", v->Stats[0], v->Stats[1], v->Stats[2], v->Stats[3]);
        EQ(U(RpcMgmtStatsVectorFree)(&v), RPC_S_OK);
        CHECK(v == NULL);
    }
}

TEST(epm_binding_mgmt_stats_vector_free_arguments) {
    RPC_STATS_VECTOR *v = NULL;
    REQUIRE(U(RpcMgmtStatsVectorFree));
    WINE_STUB();
    EQ(U(RpcMgmtStatsVectorFree)(NULL), RPC_S_INVALID_ARG);
    EQ(U(RpcMgmtStatsVectorFree)(&v), RPC_S_OK);       /* freeing a NULL vector is a no-op */
    CHECK(v == NULL);
}

/* ---------------------------------------------------------------- notifications */
TEST(epm_binding_subscribe_notification_argument_order) {
    RPC_ASYNC_NOTIFICATION_INFO info; unsigned t;
    REQUIRE(U(RpcServerSubscribeForNotification));
    WINE_STUB();
    memset(&info, 0, sizeof info);
    /* notification types: None(0), Hwnd(4) and anything >5 are rejected before the call is looked at */
    for (t = 0; t < 8; t++) {
        RPC_STATUS st = U(RpcServerSubscribeForNotification)(NULL, RpcNotificationClientDisconnect, (RPC_NOTIFICATION_TYPES)t, &info);
        if (t == 0 || t == 4 || t > 5) EXPECT_EQ(st, RPC_S_INVALID_ARG);
        else EXPECT_EQ(st, EPX_NO_CALL_ACTIVE);        /* valid type, but no call on this thread */
    }
    /* notification kinds 1..3 are accepted by the validator; others are unsupported */
    EQ(U(RpcServerSubscribeForNotification)(NULL, 0, RpcNotificationTypeEvent, &info), EPX_CANNOT_SUPPORT);
    EQ(U(RpcServerSubscribeForNotification)(NULL, 4, RpcNotificationTypeEvent, &info), EPX_CANNOT_SUPPORT);
    EQ(U(RpcServerSubscribeForNotification)(NULL, 0xFFFFFFFFu, RpcNotificationTypeEvent, &info), EPX_CANNOT_SUPPORT);
    EQ(U(RpcServerSubscribeForNotification)(NULL, 1, RpcNotificationTypeEvent, &info), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcServerSubscribeForNotification)(NULL, 2, RpcNotificationTypeEvent, &info), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcServerSubscribeForNotification)(NULL, 3, RpcNotificationTypeEvent, &info), EPX_NO_CALL_ACTIVE);
    /* a bad type wins over a bad notification */
    EQ(U(RpcServerSubscribeForNotification)(NULL, 0, RpcNotificationTypeNone, &info), RPC_S_INVALID_ARG);
}

TEST(epm_binding_unsubscribe_notification_outside_call_is_harmless) {
    ULONG n = 0x55;
    REQUIRE(U(RpcServerUnsubscribeForNotification));
    WINE_STUB();
    U(RpcServerUnsubscribeForNotification)(NULL, RpcNotificationClientDisconnect, &n);
    U(RpcServerUnsubscribeForNotification)(NULL, (RPC_NOTIFICATIONS)0, &n);
    EXPECT_EQ(n, 0x55);
}

/* ---------------------------------------------------------------- authorization context */
TEST(epm_binding_get_authz_context_reserved_arguments) {
    PVOID ctx = (PVOID)(ULONG_PTR)0x55; LUID l0; LUID l1;
    REQUIRE(U(RpcGetAuthorizationContextForClient));
    WINE_STUB();
    memset(&l0, 0, sizeof l0); l1 = l0; l1.LowPart = 1;
    EQ(U(RpcGetAuthorizationContextForClient)(NULL, FALSE, (PVOID)1, NULL, l0, 0, NULL, &ctx), RPC_S_INVALID_ARG);
    EQ(U(RpcGetAuthorizationContextForClient)(NULL, FALSE, NULL, NULL, l1, 0, NULL, &ctx), RPC_S_INVALID_ARG);
    EQ(U(RpcGetAuthorizationContextForClient)(NULL, FALSE, NULL, NULL, l0, 1, NULL, &ctx), RPC_S_INVALID_ARG);
    EQ(U(RpcGetAuthorizationContextForClient)(NULL, FALSE, NULL, NULL, l0, 0, (PVOID)1, &ctx), RPC_S_INVALID_ARG);
    EXPECT_EQ((ULONG_PTR)ctx, 0x55);
}

TEST(epm_binding_get_authz_context_without_call) {
    PVOID ctx = (PVOID)(ULONG_PTR)0x55; LUID l0; memset(&l0, 0, sizeof l0);
    REQUIRE(U(RpcGetAuthorizationContextForClient));
    WINE_STUB();
    EQ(U(RpcGetAuthorizationContextForClient)(NULL, FALSE, NULL, NULL, l0, 0, NULL, &ctx), EPX_NO_CALL_ACTIVE);
    EQ(U(RpcGetAuthorizationContextForClient)(FAKE, FALSE, NULL, NULL, l0, 0, NULL, &ctx), EPX_INVALID_BINDING);
    EXPECT_EQ((ULONG_PTR)ctx, 0x55);
}

TEST(epm_binding_free_authz_context_null) {
    PVOID ctx = NULL; RPC_STATUS st;
    REQUIRE(U(RpcFreeAuthorizationContext));
    WINE_STUB();
    st = U(RpcFreeAuthorizationContext)(&ctx);
    t_note("RpcFreeAuthorizationContext(&NULL) = %lu", (unsigned long)st);
}

/* ---------------------------------------------------------------- I_Rpc* outside a call */
TEST(epm_binding_i_rpc_call_handle_outside_call) {
    REQUIRE(U(I_RpcGetCurrentCallHandle));
    CHECK(U(I_RpcGetCurrentCallHandle)() == NULL);
}

TEST(epm_binding_i_rpc_client_local_and_pid_outside_call) {
    unsigned loc = 0xAA; ULONG pid = 0xAA;
    REQUIRE(U(I_RpcBindingIsClientLocal) && U(I_RpcBindingInqLocalClientPID));
    WINE_STUB();
    EQ(U(I_RpcBindingIsClientLocal)(NULL, &loc), EPX_NO_CALL_ACTIVE);
    EQ(U(I_RpcBindingIsClientLocal)(FAKE, &loc), EPX_INVALID_BINDING);
    EQ(U(I_RpcBindingInqLocalClientPID)(NULL, &pid), EPX_NO_CALL_ACTIVE);
    EQ(U(I_RpcBindingInqLocalClientPID)(FAKE, &pid), EPX_INVALID_BINDING);
    EXPECT_EQ(loc, 0xAA); EXPECT_EQ(pid, 0xAA);
}

TEST(epm_binding_server_yield_outside_call_returns) {
    REQUIRE(U(RpcServerYield));
    WINE_STUB();
    U(RpcServerYield)();
    CHECK(1);
}

TEST(epm_binding_impersonate_invalid) {
    REQUIRE(U(RpcImpersonateClient) && U(RpcRevertToSelf));
    WINE_STUB();
    EQ(U(RpcImpersonateClient)(FAKE), EPX_INVALID_BINDING);
}
