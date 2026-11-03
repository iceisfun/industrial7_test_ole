/* Export surface of the gap_07 functions, RpcMgmtInqServerPrincNameA and the async/NDR64 dispatch entry points.
 *
 * NdrAsyncServerCall / NdrServerCallAll / Ndr64AsyncServerCallAll are dispatch routines that dereference the
 * interface's MIDL_SERVER_INFO (NdrServerCallAll and Ndr64AsyncServerCallAll use its NDR64 syntax-info entry; the
 * async one requires an async-flagged Oif procedure). They cannot be driven without a complete NDR64/async
 * interface description, so here only their presence and module placement are asserted; the synchronous
 * dispatcher NdrStubCall2/NdrStubCall3 is exercised end to end in server.c. */
#include "gap7.h"

static const char *const rpcrt4_names[] = {
    "NdrAsyncServerCall", "NdrServerCallAll", "NdrStubCall2", "NdrStubCall3", "NdrConvert", "NdrInterfacePointerFree",
    "NdrInterfacePointerUnmarshall", "NdrConformantStringMarshall", "NdrConformantStringUnmarshall", "Ndr64AsyncServerCallAll",
    "RpcServerUseProtseqA", "RpcServerUseProtseqIfA", "RpcServerUseProtseqIfW", "RpcServerUseAllProtseqsIf", "RpcMgmtEpUnregister",
    "RpcMgmtEpEltInqNextA", "RpcMgmtInqServerPrincNameA", "RpcNsBindingInqEntryNameA", "RpcCertGeneratePrincipalNameA",
    "RpcCertGeneratePrincipalNameW", "RpcErrorClearInformation", "RpcSmClientFree", "RpcSmSetClientAllocFree",
    "RpcSmSwapClientAllocFree", "RpcSmSetThreadHandle", "RpcSmEnableAllocate", "RpcSmDisableAllocate", "RpcSsSetClientAllocFree",
    "NdrRpcSmClientAllocate", "NdrRpcSmClientFree" };
static const char *const sechost_names[] = {
    "CredMarshalCredentialA", "CredMarshalCredentialW", "CredUnmarshalCredentialA", "CredUnmarshalCredentialW",
    "CredIsMarshaledCredentialW", "CredFree", "LsaOpenPolicy", "LsaQueryInformationPolicy", "LsaClose", "LsaFreeMemory" };

TEST(gap7_exports_rpcrt4_functions_resolve)
{
    HMODULE m = t_module("rpcrt4.dll"); unsigned i;
    CHECK(m != NULL);
    if (!m) return;
    for (i = 0; i < NELEM(rpcrt4_names); i++) {
        FARPROC p;
        if (t_is_wine() && (!strcmp(rpcrt4_names[i], "NdrStubCall3") || !strcmp(rpcrt4_names[i], "Ndr64AsyncServerCallAll"))) continue;
        SET_ERR(0); p = GetProcAddress(m, rpcrt4_names[i]);
        CHECKF(p != NULL, "rpcrt4 does not export %s", rpcrt4_names[i]);
    }
}
TEST(gap7_exports_sechost_functions_resolve_and_advapi32_forwards)
{
    HMODULE s = t_module("sechost.dll"), a = t_module("advapi32.dll"); unsigned i;
    CHECK(s != NULL); CHECK(a != NULL);
    if (!s || !a) return;
    for (i = 0; i < NELEM(sechost_names); i++) {
        FARPROC ps = GetProcAddress(s, sechost_names[i]), pa = GetProcAddress(a, sechost_names[i]);
        CHECKF(ps != NULL, "sechost does not export %s", sechost_names[i]);
        CHECKF(pa != NULL, "advapi32 does not export %s", sechost_names[i]);
        if (ps && pa && ps != pa) t_note("advapi32!%s is its own implementation, not a forwarder to sechost", sechost_names[i]);
    }
}
TEST(gap7_exports_unsuffixed_names_do_not_exist)
{
    static const char *const bad[] = { "RpcServerUseProtseq", "RpcServerUseProtseqIf", "RpcMgmtEpEltInqNext", "RpcMgmtInqServerPrincName",
        "RpcNsBindingInqEntryName", "RpcCertGeneratePrincipalName" };
    HMODULE m = t_module("rpcrt4.dll"); unsigned i;
    CHECK(m != NULL);
    if (!m) return;
    for (i = 0; i < NELEM(bad); i++) {
        SET_ERR(0);
        CHECKF(GetProcAddress(m, bad[i]) == NULL, "%s unexpectedly exported", bad[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(gap7_exports_credential_a_variants_in_sechost_only_for_marshal_and_unmarshal)
{
    HMODULE s = t_module("sechost.dll");
    CHECK(s != NULL);
    if (!s) return;
    CHECK(GetProcAddress(s, "CredIsMarshaledCredentialW") != NULL);
    t_note("sechost exports CredIsMarshaledCredentialA: %s", GetProcAddress(s, "CredIsMarshaledCredentialA") ? "yes" : "no (advapi32 only)");
}

/* ---- RpcMgmtInqServerPrincNameA ---- */
TEST(gap7_mgmt_inq_server_princ_name_a_unknown_service_leaves_output_untouched)
{
    RPC_CSTR out = (RPC_CSTR)(ULONG_PTR)0x77; RPC_WSTR wout = (RPC_WSTR)(ULONG_PTR)0x77; static const ULONG svc[] = { 0x7777, 600, 0xffffffffu };
    unsigned i;
    REQUIRE(U(RpcMgmtInqServerPrincNameA)); REQUIRE(U(RpcMgmtInqServerPrincNameW));
    if (t_is_wine()) { t_skip("RpcMgmtInqServerPrincName is a stub in wine"); return; }
    for (i = 0; i < NELEM(svc); i++) {
        RPC_STATUS a, w;
        out = (RPC_CSTR)(ULONG_PTR)0x77; wout = (RPC_WSTR)(ULONG_PTR)0x77;
        a = U(RpcMgmtInqServerPrincNameA)(NULL, svc[i], &out);
        w = U(RpcMgmtInqServerPrincNameW)(NULL, svc[i], &wout);
        CHECKF(a == 1747, "A service 0x%lx -> %lu", (unsigned long)svc[i], (unsigned long)a);        /* RPC_S_UNKNOWN_AUTHN_SERVICE */
        CHECKF(a == w, "A %lu vs W %lu", (unsigned long)a, (unsigned long)w);
        CHECK(out == (RPC_CSTR)(ULONG_PTR)0x77);
    }
}
/* ---- RpcMgmtEpUnregister raises ---- */
static void do_epunreg(void *p) { (void)p; U(RpcMgmtEpUnregister)(NULL, NULL, NULL, NULL); }
TEST(gap7_mgmt_ep_unregister_raises_cant_perform_op_for_any_arguments)
{
    REQUIRE(U(RpcMgmtEpUnregister));
    if (t_is_wine()) { t_skip("stubbed differently in wine"); return; }
    EQ(g7_run(do_epunreg, NULL), 1752);                  /* EPT_S_CANT_PERFORM_OP raised, never returned */
}
