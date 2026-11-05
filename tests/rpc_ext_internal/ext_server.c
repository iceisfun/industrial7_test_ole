/* I_RpcServerUseProtseq2A/W and I_RpcServerUseProtseqEp2A/W: RpcServerUseProtseq(Ep)Ex with the RPC_POLICY as the last argument. */
#include "ext.h"

UDECL(RPC_STATUS, I_RpcServerUseProtseq2W, (RPC_WSTR, RPC_WSTR, unsigned, void *, void *));
UDECL(RPC_STATUS, I_RpcServerUseProtseq2A, (RPC_CSTR, RPC_CSTR, unsigned, void *, void *));
UDECL(RPC_STATUS, I_RpcServerUseProtseqEp2W, (RPC_WSTR, RPC_WSTR, unsigned, RPC_WSTR, void *, void *));
UDECL(RPC_STATUS, I_RpcServerUseProtseqEp2A, (RPC_CSTR, RPC_CSTR, unsigned, RPC_CSTR, void *, void *));

typedef struct { ULONG len, endpoint_flags, nic_flags; } policy_t;
static void narrow(const WCHAR *w, char *a, int n) { WideCharToMultiByte(CP_ACP, 0, w, -1, a, n, NULL, NULL); }

TEST(ext_useprotseq2_policy_length_is_checked) {
    static const ULONG lens[] = { 0, 4, 8, 11, 12, 16, 100 };
    unsigned k; WCHAR ep[64]; char epa[64];
    REQUIRE(UN(I_RpcServerUseProtseq2W) && UN(I_RpcServerUseProtseq2A) && UN(I_RpcServerUseProtseqEp2W) && UN(I_RpcServerUseProtseqEp2A));
    ext_warm();
    for (k = 0; k < sizeof lens / sizeof *lens; k++) {
        policy_t pol; RPC_STATUS want = lens[k] >= 12 ? RPC_S_OK : RPC_S_INVALID_BOUND;
        memset(&pol, 0, sizeof pol); pol.len = lens[k];
        ext_ep(ep); narrow(ep, epa, sizeof epa);
        EQ(UN(I_RpcServerUseProtseqEp2W)(NULL, (RPC_WSTR)L"ncalrpc", 5, ep, NULL, &pol), want);
        ext_ep(ep); narrow(ep, epa, sizeof epa);
        EQ(UN(I_RpcServerUseProtseqEp2A)(NULL, (RPC_CSTR)"ncalrpc", 5, (RPC_CSTR)epa, NULL, &pol), want);
        EQ(UN(I_RpcServerUseProtseq2W)(NULL, (RPC_WSTR)L"ncalrpc", 5, NULL, &pol), want);
        EQ(UN(I_RpcServerUseProtseq2A)(NULL, (RPC_CSTR)"ncalrpc", 5, NULL, &pol), want);
    }
}
TEST(ext_useprotseqep2_unknown_protocol_sequence) {
    policy_t pol = { 12, 0, 0 }; WCHAR ep[64]; char epa[64];
    REQUIRE(UN(I_RpcServerUseProtseqEp2W) && UN(I_RpcServerUseProtseqEp2A) && UN(I_RpcServerUseProtseq2W) && UN(I_RpcServerUseProtseq2A));
    ext_warm();
    ext_ep(ep); narrow(ep, epa, sizeof epa);
    EQ(UN(I_RpcServerUseProtseqEp2W)(NULL, (RPC_WSTR)L"nosuchproto", 5, ep, NULL, &pol), RPC_S_PROTSEQ_NOT_SUPPORTED);
    EQ(UN(I_RpcServerUseProtseqEp2A)(NULL, (RPC_CSTR)"nosuchproto", 5, (RPC_CSTR)epa, NULL, &pol), RPC_S_PROTSEQ_NOT_SUPPORTED);
    EQ(UN(I_RpcServerUseProtseq2W)(NULL, (RPC_WSTR)L"nosuchproto", 5, NULL, &pol), RPC_S_PROTSEQ_NOT_SUPPORTED);
    EQ(UN(I_RpcServerUseProtseq2A)(NULL, (RPC_CSTR)"nosuchproto", 5, NULL, &pol), RPC_S_PROTSEQ_NOT_SUPPORTED);
}
TEST(ext_useprotseqep2_same_endpoint_again_in_one_process) {
    policy_t pol = { 12, 0, 0 }; WCHAR ep[64]; RPC_STATUS st;
    REQUIRE(UN(I_RpcServerUseProtseqEp2W));
    ext_warm();
    ext_ep(ep);
    EQ(UN(I_RpcServerUseProtseqEp2W)(NULL, (RPC_WSTR)L"ncalrpc", 5, ep, NULL, &pol), RPC_S_OK);
    st = UN(I_RpcServerUseProtseqEp2W)(NULL, (RPC_WSTR)L"ncalrpc", 5, ep, NULL, &pol);
    EQ(st, RPC_S_DUPLICATE_ENDPOINT);
}
TEST(ext_useprotseq2_overlong_ansi_protseq) {
    policy_t pol = { 12, 0, 0 }; char *big; RPC_STATUS st;
    REQUIRE(UN(I_RpcServerUseProtseq2A));
    ext_warm();
    big = HeapAlloc(GetProcessHeap(), 0, 0x10010);
    if (!big) { CHECK(0); return; }
    memset(big, 'a', 0x10001); big[0x10001] = 0;
    st = UN(I_RpcServerUseProtseq2A)(NULL, (RPC_CSTR)big, 5, NULL, &pol);
    EQ(st, ERROR_INVALID_PARAMETER);                /* longer than 0xffff characters: refused before the policy is read */
    st = UN(I_RpcServerUseProtseq2A)(NULL, (RPC_CSTR)big, 5, NULL, &(policy_t){ 0, 0, 0 });
    EQ(st, ERROR_INVALID_PARAMETER);
    HeapFree(GetProcessHeap(), 0, big);
}

/* A server that got its endpoint from I_RpcServerUseProtseqEp2W answers calls on it. */
TEST(ext_useprotseqep2_endpoint_serves_calls) {
    ext_srv s; policy_t pol = { 12, 0, 0 }; RPC_STATUS st; ULONG n; RPC_WSTR sb = NULL;
    REQUIRE(UN(I_RpcServerUseProtseqEp2W));
    ext_warm();
    EQ(ext_up_ep2(&s), 1);
    if (!s.h) { ext_down(&s); return; }
    st = ext_call(&s, NULL, 0, NULL, 0, NULL, 0, &n); EQ(st, RPC_S_OK); EQ(n, 0);
    (void)pol; (void)sb;
    ext_down(&s);
}
