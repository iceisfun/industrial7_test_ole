/* The server-side inquiries that need a dispatched call (see ext.h for the procedures) and their answers outside one:
 * I_RpcServerGetAssociationID, I_RpcServerInqLocalConnAddress, I_RpcServerInqRemoteConnAddress, I_RpcServerCheckClientRestriction,
 * I_RpcBindingToStaticStringBindingW, I_RpcBindingInqSecurityContext(KeyInfo), I_RpcSessionStrictContextHandle,
 * I_RpcGetExtendedError, I_RpcBindingHandleToAsyncHandle. ncalrpc is the only transport. */
#include "ext.h"

UDECL(RPC_STATUS, I_RpcServerInqLocalConnAddress, (RPC_BINDING_HANDLE, void *, ULONG *, ULONG *));
UDECL(RPC_STATUS, I_RpcServerInqRemoteConnAddress, (RPC_BINDING_HANDLE, void *, ULONG *, ULONG *));
UDECL(RPC_STATUS, I_RpcServerGetAssociationID, (RPC_BINDING_HANDLE, ULONG *));
UDECL(RPC_STATUS, I_RpcServerCheckClientRestriction, (RPC_BINDING_HANDLE));
UDECL(RPC_STATUS, I_RpcBindingToStaticStringBindingW, (RPC_BINDING_HANDLE, RPC_WSTR *));
UDECL(RPC_STATUS, I_RpcBindingInqSecurityContext, (RPC_BINDING_HANDLE, void **));
UDECL(RPC_STATUS, I_RpcBindingInqSecurityContextKeyInfo, (RPC_BINDING_HANDLE, void *));
UDECL(RPC_STATUS, I_RpcGetExtendedError, (void));
UDECL(RPC_STATUS, I_RpcBindingHandleToAsyncHandle, (RPC_BINDING_HANDLE, void **));

#define UP(s) do { if (!ext_up(&s)) { CHECKF(0, "server fixture failed"); ext_down(&s); return; } } while (0)

/* ---------------------------------------------------------------- inside a call */
TEST(ext_conn_addresses_inside_a_call_are_not_supported_on_ncalrpc) {
    ext_srv s; DWORD r[12]; ULONG n; int i;
    REQUIRE(UN(I_RpcServerInqLocalConnAddress) && UN(I_RpcServerInqRemoteConnAddress));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 1, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(n, sizeof r);
    for (i = 0; i < 4; i++) {              /* local(NULL), local(call handle), remote(NULL), remote(call handle) */
        EQ(r[i * 3], RPC_S_CANNOT_SUPPORT);
        EQ(r[i * 3 + 1], 64);              /* the size and format outputs are not written */
        EQ(r[i * 3 + 2], 0xdeadbeef);
    }
    ext_down(&s);
}
TEST(ext_association_id_inside_a_call) {
    ext_srv s; DWORD a[6], b[6]; ULONG n; RPC_BINDING_HANDLE h2 = NULL; RPC_WSTR sb = NULL;
    REQUIRE(UN(I_RpcServerGetAssociationID));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 2, NULL, 0, a, sizeof a, &n), RPC_S_OK);
    EQ(a[0], RPC_S_OK); EQ(a[2], RPC_S_OK);
    CHECKF(a[1] != 0 && a[1] != 0xdeadbeef, "association id %lu", a[1]);
    EQ(a[3], a[1]);                         /* NULL and the call handle name the same association */
    t_note("association id of the first client = %lu", a[1]);
    /* another binding to the same server from this process, and a second call: same association */
    if (U(RpcStringBindingComposeW)(NULL, (RPC_WSTR)L"ncalrpc", NULL, s.ep, NULL, &sb) == RPC_S_OK) {
        U(RpcBindingFromStringBindingW)(sb, &h2); U(RpcStringFreeW)(&sb);
    }
    CHECK(h2 != NULL);
    if (h2) {
        EQ(ext_call(&s, h2, 2, NULL, 0, b, sizeof b, &n), RPC_S_OK);
        EQ(b[0], RPC_S_OK);
        EQ(b[1], a[1]);
        U(RpcBindingFree)(&h2);
    }
    EQ(ext_call(&s, NULL, 2, NULL, 0, b, sizeof b, &n), RPC_S_OK);
    EQ(b[1], a[1]);
    ext_down(&s);
}
TEST(ext_check_client_restriction_inside_a_call) {
    ext_srv s; DWORD r[6]; ULONG n;
    REQUIRE(UN(I_RpcServerCheckClientRestriction));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 2, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[4], RPC_S_OK);                     /* the call handle passes */
    EQ(r[5], ERROR_ACCESS_DENIED);          /* NULL does not, even inside a call */
    ext_down(&s);
}
TEST(ext_static_string_binding_refuses_the_call_handle_and_null) {
    ext_srv s; struct { DWORD st, st2; WCHAR s[200]; } q; ULONG n;
    REQUIRE(UN(I_RpcBindingToStaticStringBindingW));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 3, NULL, 0, &q, sizeof q, &n), RPC_S_OK);
    EQ(q.st, RPC_S_INVALID_BINDING);        /* a call handle is not a client binding */
    EQ(q.st2, RPC_S_INVALID_BINDING);
    ext_down(&s);
}
TEST(ext_security_context_inside_a_call_ncalrpc_has_none) {
    ext_srv s; DWORD r[8]; ULONG n;
    REQUIRE(UN(I_RpcBindingInqSecurityContext) && UN(I_RpcBindingInqSecurityContextKeyInfo));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 4, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[0], RPC_S_CANNOT_SUPPORT); EQ(r[1], 0);   /* call handle: no context; *ctx is not written */
    EQ(r[2], RPC_S_INVALID_BINDING); EQ(r[3], 0);  /* NULL is not accepted, even inside a call */
    EQ(r[4], RPC_S_CANNOT_SUPPORT);                /* KeyInfo(call handle) */
    EQ(r[5], RPC_S_CANNOT_SUPPORT);                /* KeyInfo(NULL) means the current call */
    ext_down(&s);
}
TEST(ext_session_strict_context_handle_inside_a_call_returns) {
    ext_srv s; ULONG n; DWORD r;
    REQUIRE(UN(I_RpcServerGetAssociationID));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 5, NULL, 0, NULL, 0, &n), RPC_S_OK);
    EQ(ext_call(&s, NULL, 0, NULL, 0, NULL, 0, &n), RPC_S_OK);       /* the server keeps serving */
    EQ(ext_call(&s, NULL, 6, NULL, 0, &r, 4, &n), RPC_S_OK);
    EQ(r, 0);                                                          /* I_RpcGetExtendedError inside a call */
    ext_down(&s);
}
TEST(ext_handle_to_async_handle_inside_a_call) {
    ext_srv s; DWORD r[12]; ULONG n;
    REQUIRE(UN(I_RpcBindingHandleToAsyncHandle));
    ext_warm(); UP(s);
    EQ(ext_call(&s, NULL, 7, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(r[6], RPC_S_INVALID_BINDING); EQ(r[7], 1);        /* *AsyncHandle untouched */
    ext_down(&s);
}

/* ---------------------------------------------------------------- outside a call */
TEST(ext_server_inquiries_outside_a_call) {
    ext_srv s; ULONG sz = 64, fmt = 0xdead, id = 0xdead; BYTE buf[64]; void *c = (void *)1; RPC_WSTR str = NULL;
    REQUIRE(UN(I_RpcServerInqLocalConnAddress) && UN(I_RpcServerGetAssociationID) && UN(I_RpcServerCheckClientRestriction)
            && UN(I_RpcBindingInqSecurityContext) && UN(I_RpcBindingInqSecurityContextKeyInfo) && UN(I_RpcBindingHandleToAsyncHandle));
    ext_warm(); UP(s);
    /* NULL names the current call, and there is none (RPC_S_NO_CALL_ACTIVE); a client binding is not a call (RPC_S_INVALID_BINDING) */
    EQ(UN(I_RpcServerInqLocalConnAddress)(NULL, buf, &sz, &fmt), RPC_S_NO_CALL_ACTIVE);
    EQ(UN(I_RpcServerInqLocalConnAddress)(s.h, buf, &sz, &fmt), RPC_S_INVALID_BINDING);
    EQ(UN(I_RpcServerInqRemoteConnAddress)(NULL, buf, &sz, &fmt), RPC_S_NO_CALL_ACTIVE);
    EQ(UN(I_RpcServerInqRemoteConnAddress)(s.h, buf, &sz, &fmt), RPC_S_INVALID_BINDING);
    EQ(sz, 64); EQ(fmt, 0xdead);
    EQ(UN(I_RpcServerGetAssociationID)(NULL, &id), RPC_S_NO_CALL_ACTIVE);
    EQ(UN(I_RpcServerGetAssociationID)(s.h, &id), RPC_S_INVALID_BINDING);
    EQ(id, 0xdead);
    EQ(UN(I_RpcServerCheckClientRestriction)(NULL), ERROR_ACCESS_DENIED);
    EQ(UN(I_RpcServerCheckClientRestriction)(s.h), ERROR_ACCESS_DENIED);
    EQ(UN(I_RpcBindingInqSecurityContext)(s.h, &c), RPC_S_INVALID_BINDING);
    EQ(UN(I_RpcBindingInqSecurityContext)(NULL, &c), RPC_S_INVALID_BINDING);
    CHECK(c == (void *)1);
    EQ(UN(I_RpcBindingInqSecurityContextKeyInfo)(s.h, buf), RPC_S_INVALID_BINDING);
    EQ(UN(I_RpcBindingInqSecurityContextKeyInfo)(NULL, buf), RPC_S_NO_CALL_ACTIVE);
    EQ(UN(I_RpcBindingHandleToAsyncHandle)(s.h, &c), RPC_S_INVALID_BINDING);
    CHECK(c == (void *)1);
    EQ(UN(I_RpcBindingToStaticStringBindingW)(NULL, &str), RPC_S_INVALID_BINDING);
    EQ(UN(I_RpcGetExtendedError)(), 0);
    ext_down(&s);
}
static const WCHAR *const sbs[] = {
    L"ncalrpc:[abc]", L"ncalrpc:", L"ncacn_ip_tcp:127.0.0.1[1234]", L"ncacn_ip_tcp:127.0.0.1", L"ncacn_ip_tcp:localhost[135]",
    L"6f0e7a10-0001-4c3d-9a00-0000000000a1@ncacn_ip_tcp:127.0.0.1[135]", L"ncacn_np:\\\\\\\\localhost[\\\\pipe\\\\olet]",
    L"ncacn_ip_tcp:127.0.0.1[1234,Encrypt=on]", L"ncacn_http:127.0.0.1[80]", L"ncalrpc:[abc,Security=Impersonation Dynamic True]" };
TEST(ext_static_string_binding_of_a_client_binding_is_the_string_binding) {
    unsigned k;
    REQUIRE(UN(I_RpcBindingToStaticStringBindingW));
    ext_warm();
    for (k = 0; k < sizeof sbs / sizeof *sbs; k++) {
        RPC_BINDING_HANDLE h = NULL; RPC_WSTR st = NULL, norm = NULL; RPC_STATUS rc;
        rc = U(RpcBindingFromStringBindingW)((RPC_WSTR)sbs[k], &h);
        CHECKF(rc == RPC_S_OK, "[%u] RpcBindingFromStringBindingW = %lu", k, rc);
        if (rc != RPC_S_OK) continue;
        EQ(UN(I_RpcBindingToStaticStringBindingW)(h, &st), RPC_S_OK);
        EQ(U(RpcBindingToStringBindingW)(h, &norm), RPC_S_OK);
        if (st && norm) CHECKF(!lstrcmpW((WCHAR *)st, (WCHAR *)norm), "[%u] static '%ls' vs string binding '%ls'", k, (WCHAR *)st, (WCHAR *)norm);
        if (st) CHECKF(!lstrcmpW((WCHAR *)st, sbs[k]), "[%u] static '%ls' want '%ls'", k, (WCHAR *)st, sbs[k]);   /* these strings are already in canonical form */
        if (st) U(RpcStringFreeW)(&st);
        if (norm) U(RpcStringFreeW)(&norm);
        U(RpcBindingFree)(&h);
    }
}
TEST(ext_get_extended_error_is_zero_after_failures) {
    RPC_BINDING_HANDLE bad = NULL, h2 = NULL; ext_srv s; ULONG n; RPC_STATUS st;
    REQUIRE(UN(I_RpcGetExtendedError));
    ext_warm();
    EQ(UN(I_RpcGetExtendedError)(), 0);
    CHECK(U(RpcBindingFromStringBindingW)((RPC_WSTR)L"ncalrpc:[olet_ext_nosuchendpoint]", &bad) == RPC_S_OK);
    UP(s);
    st = ext_call(&s, bad, 0, NULL, 0, NULL, 0, &n);
    EQ(st, RPC_S_SERVER_UNAVAILABLE);
    EQ(UN(I_RpcGetExtendedError)(), 0);
    st = U(RpcBindingFromStringBindingW)((RPC_WSTR)L"bogus-binding", &h2);
    EQ(st, RPC_S_INVALID_STRING_BINDING);
    EQ(UN(I_RpcGetExtendedError)(), 0);
    ext_down(&s);
    if (bad) U(RpcBindingFree)(&bad);
}

/* ---------------------------------------------------------------- async helpers */
UDECL(RPC_STATUS, I_RpcBindingInqDynamicEndpointA, (RPC_BINDING_HANDLE, RPC_CSTR *));
UDECL(RPC_STATUS, I_RpcBindingInqDynamicEndpointW, (RPC_BINDING_HANDLE, RPC_WSTR *));
UDECL(RPC_STATUS, I_RpcAsyncSetHandle, (PRPC_MESSAGE, PRPC_ASYNC_STATE));
UDECL(RPC_STATUS, I_RpcAsyncAbortCall, (PRPC_ASYNC_STATE, ULONG));
UDECL(RPC_STATUS, I_RpcBindingInqWireIdForSnego, (RPC_BINDING_HANDLE, unsigned char *));
UDECL(HRESULT, NdrDllUnregisterProxy, (HMODULE, void *, const CLSID *));

TEST(ext_async_abort_and_set_handle_without_a_call) {
    RPC_ASYNC_STATE as; RPC_MESSAGE m; ext_srv s;
    REQUIRE(UN(I_RpcAsyncAbortCall) && UN(I_RpcAsyncSetHandle));
    ext_warm(); UP(s);
    memset(&as, 0, sizeof as);
    EQ(U(RpcAsyncInitializeHandle)(&as, sizeof as), RPC_S_OK);
    EQ(UN(I_RpcAsyncAbortCall)(&as, 5), RPC_S_INVALID_ASYNC_HANDLE);       /* no call attached */
    memset(&m, 0, sizeof m);
    m.Handle = s.h;                                                        /* a plain client binding is not a call object */
    EQ(UN(I_RpcAsyncSetHandle)(&m, &as), RPC_S_INVALID_BINDING);
    CHECK(as.RuntimeInfo == NULL);
    m.Handle = NULL;
    EQ(UN(I_RpcAsyncSetHandle)(&m, &as), RPC_S_INVALID_BINDING);
    ext_down(&s);
}
TEST(ext_dynamic_endpoint_of_a_client_binding) {
    ext_srv s; RPC_CSTR ea = (RPC_CSTR)1; RPC_WSTR ew = (RPC_WSTR)1;
    REQUIRE(UN(I_RpcBindingInqDynamicEndpointW) && UN(I_RpcBindingInqDynamicEndpointA));
    ext_warm(); UP(s);
    /* a binding with a fixed endpoint has no dynamic endpoint: success with NULL */
    EQ(UN(I_RpcBindingInqDynamicEndpointW)(s.h, &ew), RPC_S_OK);
    CHECK(ew == NULL);
    EQ(UN(I_RpcBindingInqDynamicEndpointW)(NULL, &ew), RPC_S_INVALID_BINDING);
    EQ(UN(I_RpcBindingInqDynamicEndpointA)(s.h, &ea), RPC_S_CANNOT_SUPPORT);   /* the ANSI form is a stub that reports "not supported" */
    ext_down(&s);
}
TEST(ext_wire_id_for_snego_needs_authentication) {
    ext_srv s; unsigned char id = 7;
    REQUIRE(UN(I_RpcBindingInqWireIdForSnego));
    ext_warm(); UP(s);
    EQ(UN(I_RpcBindingInqWireIdForSnego)(s.h, &id), RPC_S_INVALID_BINDING);
    CHECK(id == 7);
    ext_down(&s);
}
TEST(ext_ndr_dll_unregister_proxy_arguments) {
    REQUIRE(UN(NdrDllUnregisterProxy));
    EQ(UN(NdrDllUnregisterProxy)(NULL, NULL, NULL), E_HANDLE);
}
