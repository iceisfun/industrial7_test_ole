/* Internal I_Rpc* exports that can be exercised without a server: argument validation and out-of-call behaviour. */
#include "epx.h"

#define FAKE ((RPC_BINDING_HANDLE)epx_fake)
UDECL(RPC_STATUS, I_RpcBindingIsServerLocal, (RPC_BINDING_HANDLE, unsigned *));
UDECL(RPC_STATUS, I_RpcBindingInqDynamicEndpointW, (RPC_BINDING_HANDLE, RPC_WSTR *));
UDECL(RPC_STATUS, I_RpcBindingInqCurrentModifiedId, (void *, LUID *));
UDECL(RPC_STATUS, I_RpcBindingInqMarshalledTargetInfo, (RPC_BINDING_HANDLE, ULONG *, unsigned char **));

TEST(epm_internal_binding_transport_type_validation) {
    unsigned t = 0xAA; RPC_BINDING_HANDLE h;
    REQUIRE(U(I_RpcBindingInqTransportType) && U(I_RpcServerInqTransportType));
    WINE_STUB();
    EQ(U(I_RpcBindingInqTransportType)(NULL, &t), EPX_NO_CALL_ACTIVE);
    EQ(U(I_RpcBindingInqTransportType)(FAKE, &t), EPX_INVALID_BINDING);
    EQ(U(I_RpcServerInqTransportType)(&t), EPX_NO_CALL_ACTIVE);
    EQ(t, 0xAA);
    h = epx_mkbind();
    if (h) {
        EQ(U(I_RpcBindingInqTransportType)(h, &t), RPC_S_OK);
        EQ(t, TRANSPORT_TYPE_LPC);
        U(RpcBindingFree)(&h);
    }
}

TEST(epm_internal_binding_is_server_local_validation) {
    unsigned l = 0xAA; RPC_BINDING_HANDLE h;
    REQUIRE(UN(I_RpcBindingIsServerLocal));
    WINE_STUB();
    EQ(UN(I_RpcBindingIsServerLocal)(NULL, &l), EPX_INVALID_BINDING);      /* unlike the *client* variant, NULL is not "the current call" */
    EQ(UN(I_RpcBindingIsServerLocal)(FAKE, &l), EPX_INVALID_BINDING);
    h = epx_mkbind();
    if (h) {
        RPC_STATUS st = UN(I_RpcBindingIsServerLocal)(h, &l);
        t_note("I_RpcBindingIsServerLocal(ncalrpc client binding) = %lu local=%u", (unsigned long)st, l);
        U(RpcBindingFree)(&h);
    }
}

TEST(epm_internal_inq_dynamic_endpoint_validation) {
    RPC_WSTR ep = (RPC_WSTR)(ULONG_PTR)0x55;
    REQUIRE(UN(I_RpcBindingInqDynamicEndpointW));
    WINE_STUB();
    EQ(UN(I_RpcBindingInqDynamicEndpointW)(NULL, &ep), EPX_INVALID_BINDING);
    EQ(UN(I_RpcBindingInqDynamicEndpointW)(FAKE, &ep), EPX_INVALID_BINDING);
    EQ((ULONG_PTR)ep, 0x55);
}

TEST(epm_internal_modified_id_validation) {
    LUID l;
    REQUIRE(UN(I_RpcBindingInqCurrentModifiedId));
    WINE_STUB();
    EQ(UN(I_RpcBindingInqCurrentModifiedId)(NULL, &l), EPX_NO_CALL_ACTIVE);
    EQ(UN(I_RpcBindingInqCurrentModifiedId)(FAKE, &l), EPX_INVALID_BINDING);
}

TEST(epm_internal_marshalled_target_info_validation) {
    ULONG n = 0; unsigned char *p = NULL; RPC_BINDING_HANDLE h;
    REQUIRE(UN(I_RpcBindingInqMarshalledTargetInfo));
    WINE_STUB();
    EQ(UN(I_RpcBindingInqMarshalledTargetInfo)(FAKE, &n, &p), EPX_INVALID_BINDING);
    EQ(UN(I_RpcBindingInqMarshalledTargetInfo)(NULL, &n, &p), EPX_INVALID_BINDING);
    h = epx_mkbind();
    if (h) {   /* only server-side (OSF) call handles are accepted */
        EQ(UN(I_RpcBindingInqMarshalledTargetInfo)(h, &n, &p), EPX_INVALID_BINDING);
        U(RpcBindingFree)(&h);
    }
}

TEST(epm_internal_pause_execution_and_idle_cleanup) {
    REQUIRE(U(I_RpcPauseExecution) && U(RpcMgmtEnableIdleCleanup));
    U(I_RpcPauseExecution)(0);
    U(I_RpcPauseExecution)(10);
    EQ(U(RpcMgmtEnableIdleCleanup)(), RPC_S_OK);
    EQ(U(RpcMgmtEnableIdleCleanup)(), RPC_S_OK);        /* idempotent */
}

