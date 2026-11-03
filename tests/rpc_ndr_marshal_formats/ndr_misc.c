/* NdrMapCommAndFaultStatus, NdrAllocate/NdrOle*, correlation init, NdrConvert, client context handles, NdrGetBuffer/NdrSendReceive over ncalrpc. */
#include "ndrh.h"

/* ---------------- NdrMapCommAndFaultStatus ---------------- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
static int is_comm_status(RPC_STATUS s) {
    switch (s) {
    case ERROR_INVALID_HANDLE: case RPC_S_INVALID_BINDING: case RPC_S_UNKNOWN_IF: case RPC_S_SERVER_UNAVAILABLE:
    case RPC_S_SERVER_TOO_BUSY: case RPC_S_CALL_FAILED_DNE: case RPC_S_PROTOCOL_ERROR: case RPC_S_UNSUPPORTED_TRANS_SYN:
    case RPC_S_UNSUPPORTED_TYPE: case RPC_S_PROCNUM_OUT_OF_RANGE: case EPT_S_NOT_REGISTERED: case RPC_S_COMM_FAILURE:
        return 1;
    }
    return 0;
}
TEST(ndr_mapcommandfault_partitions_status_codes) {
    REQUIRE(U(NdrMapCommAndFaultStatus)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 4);
    int comm_hits = 0;
    for (RPC_STATUS s = 0; s < 10000; s++) {
        ULONG comm = 0, fault = 0;
        RPC_STATUS r = U(NdrMapCommAndFaultStatus)(&c.sm, &comm, &fault, s);
        if (r != RPC_S_OK || comm != (is_comm_status(s) ? s : 0) || fault != ((is_comm_status(s) || s == 0) ? 0 : s)) {
            CHECKF(0, "status %lu -> r=%ld comm=%lu fault=%lu", (unsigned long)s, (long)r, comm, fault);
            break;
        }
        comm_hits += is_comm_status(s);
    }
    EXPECT_EQ(comm_hits, 12);
    nc_done(&c);
}
TEST(ndr_mapcommandfault_zero_status_leaves_outputs_untouched) {
    REQUIRE(U(NdrMapCommAndFaultStatus)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 4);
    ULONG comm = 0x1111, fault = 0x2222;
    RPC_STATUS r = U(NdrMapCommAndFaultStatus)(&c.sm, &comm, &fault, 0);
    EXPECT_EQ(r, 0); EXPECT_EQ(comm, 0x1111); EXPECT_EQ(fault, 0x2222);
    nc_done(&c);
}
TEST(ndr_mapcommandfault_missing_output_pointer_passes_status_through) {
    if (t_is_wine()) { t_skip("wine dereferences the output pointers"); return; }
    REQUIRE(U(NdrMapCommAndFaultStatus)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 4);
    ULONG v = 0;
    RPC_STATUS r = U(NdrMapCommAndFaultStatus)(&c.sm, NULL, &v, RPC_S_SERVER_UNAVAILABLE);
    EXPECT_EQ(r, RPC_S_SERVER_UNAVAILABLE); EXPECT_EQ(v, 0);
    r = U(NdrMapCommAndFaultStatus)(&c.sm, &v, NULL, 5);
    EXPECT_EQ(r, 5); EXPECT_EQ(v, 0);
    r = U(NdrMapCommAndFaultStatus)(&c.sm, NULL, NULL, 0);
    EXPECT_EQ(r, 0);
    nc_done(&c);
}

/* ---------------- NdrAllocate / NdrOleAllocate ---------------- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
TEST(ndr_allocate_uses_pfnallocate_and_tracks_memory_list) {
    REQUIRE(U(NdrAllocate)); REQUIRE(U(NdrClientInitializeNew));
    struct ml { DWORD magic, size, unknown; struct ml *next; };
    nctx c; nc_client(&c, NULL, 4);
    n_alloc_cnt = 0;
    void *p1 = U(NdrAllocate)(&c.sm, 10);
    void *p2 = U(NdrAllocate)(&c.sm, 24);
    EXPECT_EQ(n_alloc_cnt, 2);
    CHECK(p1 && p2);
    CHECK(c.sm.pMemoryList != NULL);
    if (c.sm.pMemoryList) {
        struct ml *m = c.sm.pMemoryList;
        if (m->size == 24) {
            CHECK((char *)m == (char *)p2 + 24);
            EXPECT_EQ(m->magic, 0x4d454d4c);  /* 'MEML' */
            EXPECT_EQ(m->unknown, 0);
            struct ml *m1 = m->next;
            CHECK(m1 != NULL);
            if (m1) {
                CHECK((char *)m1 == (char *)p1 + 16);
                EXPECT_EQ(m1->magic, 0x4d454d4c);
                EXPECT_EQ(m1->size, 16);
                CHECK(m1->next == NULL);
            }
        } else t_note("memory list in an older layout (size field %lu)", (unsigned long)m->size);
    }
    n_free(p1); n_free(p2);
    nc_done(&c);
}
TEST(ndr_oleallocate_is_cotaskmem) {
    REQUIRE(U(NdrOleAllocate)); REQUIRE(U(NdrOleFree)); REQUIRE(U(CoTaskMemAlloc)); REQUIRE(U(CoTaskMemFree));
    for (size_t n = 1; n <= 4096; n = n * 3 + 1) {
        unsigned char *p = U(NdrOleAllocate)(n);
        CHECKF(p != NULL, "alloc %zu", n);
        if (p) { memset(p, 0x5a, n); U(CoTaskMemFree)(p); }
        unsigned char *q = U(CoTaskMemAlloc)(n);
        CHECKF(q != NULL, "co alloc %zu", n);
        if (q) { memset(q, 0x5a, n); U(NdrOleFree)(q); }
    }
    U(NdrOleFree)(NULL);     /* CoTaskMemFree(NULL) semantics */
}

/* ---------------- correlation ---------------- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
TEST(ndr_correlation_initialize_sets_header_and_pad) {
    REQUIRE(U(NdrCorrelationInitialize));
    MIDL_STUB_MESSAGE sm; unsigned char buf[256];
    memset(&sm, 0, sizeof sm); memset(buf, 0, sizeof buf);
    sm.StackTop = (unsigned char *)(ULONG_PTR)0x12345678;
    U(NdrCorrelationInitialize)(&sm, buf, sizeof buf, 0);
    EXPECT_EQ(sm.Pad, 2);
    EXPECT_EQ(sm.pCorrInfo, buf);
    EXPECT_EQ(sm.pCorrMemory, sm.StackTop);
    EXPECT_EQ(*(void **)buf, buf);
    EXPECT_EQ(*(void **)(buf + 8), buf);
    EXPECT_EQ(*(unsigned *)(buf + 0x10), (sizeof buf - 0x38) >> 5);
    EXPECT_EQ(*(unsigned *)(buf + 0x14), 0);
}
TEST(ndr_correlation_initialize_keeps_nonzero_pad) {
    REQUIRE(U(NdrCorrelationInitialize));
    MIDL_STUB_MESSAGE sm; unsigned char buf[256];
    memset(&sm, 0, sizeof sm); memset(buf, 0, sizeof buf);
    sm.Pad = 1;
    U(NdrCorrelationInitialize)(&sm, buf, sizeof buf, 0);
    EXPECT_EQ(sm.Pad, 1);
}
TEST(ndr_correlation_cache_size_encoding) {
    REQUIRE(U(NdrCorrelationInitialize));
    static const ULONG sizes[] = { 0x38, 0x58, 0x100, 0x400 };
    for (unsigned k = 0; k < 4; k++) {
        MIDL_STUB_MESSAGE sm; unsigned char *buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizes[k] + 16);
        memset(&sm, 0, sizeof sm);
        U(NdrCorrelationInitialize)(&sm, buf, sizes[k], 0);
        CHECKF(*(unsigned *)(buf + 0x10) == (sizes[k] - 0x38) >> 5, "size 0x%lx -> %u", sizes[k], *(unsigned *)(buf + 0x10));
        HeapFree(GetProcessHeap(), 0, buf);
    }
}
TEST(ndr_correlation_pass_and_free_are_callable_after_init) {
    REQUIRE(U(NdrCorrelationInitialize)); REQUIRE(U(NdrCorrelationPass)); REQUIRE(U(NdrCorrelationFree));
    MIDL_STUB_MESSAGE sm; unsigned char buf[256];
    memset(&sm, 0, sizeof sm); memset(buf, 0, sizeof buf);
    U(NdrCorrelationInitialize)(&sm, buf, sizeof buf, 0);
    NTRY U(NdrCorrelationPass)(&sm); NEND;
    EXPECT_EQ(n_code, 0);
    NTRY U(NdrCorrelationFree)(&sm); NEND;
    EXPECT_EQ(n_code, 0);
}

/* ---------------- NdrConvert (little endian no-op) ---------------- */
TEST(ndr_convert_little_endian_is_noop) {
    REQUIRE(U(NdrConvert)); REQUIRE(U(NdrConvert2));
    unsigned char w[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    nctx c; nc_rx(&c, NULL, w, 8, 1);
    c.rpc.DataRepresentation = 0x10;
    unsigned char fmt[8] = { 0 };
    unsigned char *before = c.sm.Buffer;
    NTRY U(NdrConvert)(&c.sm, fmt); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(c.sm.Buffer, before);
    CHECK(!memcmp(c.buf, w, 8));
    NTRY U(NdrConvert2)(&c.sm, fmt, 3); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(c.sm.Buffer, before);
    CHECK(!memcmp(c.buf, w, 8));
    nc_done(&c);
}

/* ---------------- client context handles ---------------- */
static RPC_BINDING_HANDLE mk_binding(void) {
    char sb[128]; RPC_BINDING_HANDLE h = NULL;
    snprintf(sb, sizeof sb, "ncalrpc:[olet_ndrctx_%lu]", GetCurrentProcessId());
    if (U(RpcBindingFromStringBindingA)((RPC_CSTR)sb, &h) != RPC_S_OK) return NULL;
    return h;
}
TEST(ndr_cctx_marshal_null_writes_twenty_zero_bytes) {
    REQUIRE(U(NDRCContextMarshall));
    unsigned char buf[32]; memset(buf, 0xCD, sizeof buf);
    U(NDRCContextMarshall)(NULL, buf);
    for (int i = 0; i < 20; i++) if (buf[i] != 0) { CHECKF(0, "byte %d = 0x%02x", i, buf[i]); break; }
    EXPECT_EQ(buf[20], 0xCD);
}
TEST(ndr_cctx_marshal_wrong_magic_raises_invalid_handle) {
    if (t_is_wine()) { t_skip("wine does not validate the context wrapper"); return; }
    REQUIRE(U(NDRCContextMarshall)); REQUIRE(U(NDRCContextBinding));
    unsigned char fake[0x40]; memset(fake, 0, sizeof fake);
    unsigned char buf[32];
    NTRY U(NDRCContextMarshall)((NDR_CCONTEXT)fake, buf); NEND;
    EXPECT_EQ(n_code, ERROR_INVALID_HANDLE);
    NTRY U(NDRCContextBinding)((NDR_CCONTEXT)fake); NEND;
    EXPECT_EQ(n_code, ERROR_INVALID_HANDLE);
}
TEST(ndr_cctx_client_marshal_null_checked_vs_unchecked) {
    REQUIRE(U(NdrClientContextMarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 64);
    NTRY U(NdrClientContextMarshall)(&c.sm, NULL, 1); NEND;
    EXPECT_EQ(n_code, 1775);     /* RPC_X_SS_IN_NULL_CONTEXT */
    EXPECT_EQ(nc_used(&c), 0);
    memset(c.buf, 0xCD, 64);
    c.sm.Buffer = c.buf + 1;
    NTRY U(NdrClientContextMarshall)(&c.sm, NULL, 0); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(nc_used(&c), 4 + 20);
    for (int i = 4; i < 24; i++) if (c.buf[i]) { CHECKF(0, "byte %d", i); break; }
    nc_done(&c);
}
TEST(ndr_cctx_unmarshal_null_wire_yields_null_handle) {
    REQUIRE(U(NDRCContextUnmarshall));
    unsigned char zero[20] = { 0 };
    NDR_CCONTEXT ctx = NULL;
    NTRY U(NDRCContextUnmarshall)(&ctx, NULL, zero, 0x10); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(ctx == NULL);
}
TEST(ndr_cctx_unmarshal_nonnull_wire_with_bad_binding_raises_invalid_binding) {
    if (t_is_wine()) { t_skip("wine dereferences the NULL binding"); return; }
    REQUIRE(U(NDRCContextUnmarshall));
    unsigned char w[20]; for (int i = 0; i < 20; i++) w[i] = (unsigned char)(i + 1);
    NDR_CCONTEXT ctx = NULL;
    NTRY U(NDRCContextUnmarshall)(&ctx, NULL, w, 0x10); NEND;
    EXPECT_EQ(n_code, RPC_S_INVALID_BINDING);
    CHECK(ctx == NULL);
}
TEST(ndr_cctx_unmarshal_marshal_roundtrip_and_binding_copy) {
    REQUIRE(U(NDRCContextUnmarshall)); REQUIRE(U(NDRCContextMarshall)); REQUIRE(U(NDRCContextBinding));
    REQUIRE(U(RpcBindingFromStringBindingA)); REQUIRE(U(RpcBindingFree)); REQUIRE(U(RpcSsDestroyClientContext));
    RPC_BINDING_HANDLE h = mk_binding();
    if (!h) { t_skip("cannot build binding"); return; }
    unsigned char w[20]; for (int i = 0; i < 20; i++) w[i] = (unsigned char)(0x30 + i);
    unsigned char orig[20]; memcpy(orig, w, 20);
    NDR_CCONTEXT ctx = NULL;
    NTRY U(NDRCContextUnmarshall)(&ctx, h, w, 0x10); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(ctx != NULL);
    if (ctx) {
        CHECK(!memcmp(w, orig, 20));          /* little-endian drep: input untouched */
        RPC_BINDING_HANDLE hb = U(NDRCContextBinding)(ctx);
        CHECK(hb != NULL && hb != h);          /* the wrapper owns a copy of the binding */
        unsigned char out[24]; memset(out, 0xCD, sizeof out);
        U(NDRCContextMarshall)(ctx, out);
        CHECK(!memcmp(out, orig, 20));
        EXPECT_EQ(out[20], 0xCD);
        /* same wire again: the existing wrapper is reused */
        NDR_CCONTEXT again = ctx;
        U(NDRCContextUnmarshall)(&again, h, w, 0x10);
        CHECK(again == ctx);
        /* different context id: wrapper updated in place */
        unsigned char w2[20]; memcpy(w2, orig, 20); w2[19] ^= 0xFF;
        again = ctx;
        U(NDRCContextUnmarshall)(&again, h, w2, 0x10);
        CHECK(again == ctx);
        U(NDRCContextMarshall)(again, out);
        CHECK(!memcmp(out, w2, 20));
        /* null wire releases the wrapper */
        unsigned char zero[20] = { 0 };
        U(NDRCContextUnmarshall)(&again, h, zero, 0x10);
        CHECK(again == NULL);
    }
    U(RpcBindingFree)(&h);
}
TEST(ndr_cctx_unmarshal_big_endian_wire_is_swapped_in_place) {
    REQUIRE(U(NDRCContextUnmarshall)); REQUIRE(U(NDRCContextMarshall)); REQUIRE(U(RpcSsDestroyClientContext));
    REQUIRE(U(RpcBindingFromStringBindingA)); REQUIRE(U(RpcBindingFree));
    RPC_BINDING_HANDLE h = mk_binding();
    if (!h) { t_skip("cannot build binding"); return; }
    unsigned char w[20] = { 1, 2, 3, 4,  5, 6, 7, 8,  9, 10,  11, 12,  13, 14, 15, 16, 17, 18, 19, 20 };
    NDR_CCONTEXT ctx = NULL;
    U(NDRCContextUnmarshall)(&ctx, h, w, 0x00);      /* DataRepresentation without the little-endian bit */
    CHECK(ctx != NULL);
    /* attributes, Data1 (4 bytes), Data2, Data3 are byte swapped in the caller's buffer; Data4 is untouched */
    static const unsigned char want[20] = { 4, 3, 2, 1,  8, 7, 6, 5,  10, 9,  12, 11,  13, 14, 15, 16, 17, 18, 19, 20 };
    CHECK(!memcmp(w, want, 20));
    unsigned char out[20];
    if (ctx) { U(NDRCContextMarshall)(ctx, out); CHECK(!memcmp(out, want, 20)); U(RpcSsDestroyClientContext)(&ctx); CHECK(ctx == NULL); }
    U(RpcBindingFree)(&h);
}
TEST(ndr_cctx_client_unmarshal_short_buffer_raises_bad_stub_data) {
    REQUIRE(U(NdrClientContextUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char w[20] = { 0 };
    for (unsigned len = 0; len < 20; len += 5) {
        nctx c; nc_rx(&c, NULL, w, len, 1);
        NDR_CCONTEXT ctx = NULL;
        NTRY U(NdrClientContextUnmarshall)(&c.sm, &ctx, NULL); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
    nctx c; nc_rx(&c, NULL, w, 20, 1);
    NDR_CCONTEXT ctx = (NDR_CCONTEXT)(ULONG_PTR)0; 
    NTRY U(NdrClientContextUnmarshall)(&c.sm, &ctx, NULL); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(nc_used(&c), 20);
    nc_done(&c);
}

/* ---------------- NdrGetBuffer / NdrFreeBuffer / NdrSendReceive over ncalrpc ---------------- */
static const RPC_SYNTAX_IDENTIFIER g_ndr20 = { { 0x8a885d04, 0x1ceb, 0x11c9, { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } }, { 2, 0 } };
static volatile LONG g_srv_calls;
static void WINAPI srv_dispatch(PRPC_MESSAGE msg) {
    /* echo: reads a ULONG, replies with value + 1 */
    ULONG v = 0;
    if (msg->BufferLength >= 4) v = *(ULONG *)msg->Buffer;
    InterlockedIncrement(&g_srv_calls);
    msg->BufferLength = 4;
    if (U(I_RpcGetBuffer)(msg) != RPC_S_OK) RaiseException(RPC_S_OUT_OF_MEMORY, 0, 0, NULL);
    *(ULONG *)msg->Buffer = v + 1;
}
static RPC_DISPATCH_FUNCTION g_disp_tab[1] = { srv_dispatch };
static RPC_DISPATCH_TABLE g_disp = { 1, g_disp_tab, 0 };
static RPC_SERVER_INTERFACE g_sif;
static void init_sif(void) {
    memset(&g_sif, 0, sizeof g_sif);
    g_sif.Length = sizeof g_sif;
    g_sif.InterfaceId.SyntaxGUID.Data1 = 0x6f0d1e00; g_sif.InterfaceId.SyntaxGUID.Data2 = 0x1ceb; g_sif.InterfaceId.SyntaxGUID.Data3 = 0x4a5b;
    memcpy(g_sif.InterfaceId.SyntaxGUID.Data4, "\x91\x02\x0c\xae\x4e\x4d\x00\x01", 8);
    g_sif.InterfaceId.SyntaxVersion.MajorVersion = 1;
    g_sif.TransferSyntax = g_ndr20;
    g_sif.DispatchTable = &g_disp;
}
TEST(ndr_buffer_getbuffer_sendreceive_freebuffer_roundtrip) {
    REQUIRE(U(NdrGetBuffer)); REQUIRE(U(NdrSendReceive)); REQUIRE(U(NdrFreeBuffer)); REQUIRE(U(NdrClientInitializeNew));
    REQUIRE(U(RpcServerUseProtseqEpA)); REQUIRE(U(RpcServerRegisterIf)); REQUIRE(U(RpcServerListen));
    REQUIRE(U(RpcStringBindingComposeA)); REQUIRE(U(RpcBindingFromStringBindingA)); REQUIRE(U(RpcMgmtStopServerListening)); REQUIRE(U(RpcMgmtWaitServerListen));
    REQUIRE(U(I_RpcGetBuffer)); REQUIRE(U(RpcServerUnregisterIf)); REQUIRE(U(RpcBindingFree)); REQUIRE(U(RpcStringFreeA));
    init_sif();
    char ep[96]; snprintf(ep, sizeof ep, "olet_ndrbuf_%lu", GetCurrentProcessId());
    RPC_STATUS st = U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", 5, (RPC_CSTR)ep, NULL);
    if (st != RPC_S_OK) { t_skip("RpcServerUseProtseqEp failed %ld", (long)st); return; }
    st = U(RpcServerRegisterIf)((RPC_IF_HANDLE)&g_sif, NULL, NULL);
    if (st != RPC_S_OK) { t_skip("RpcServerRegisterIf failed %ld", (long)st); return; }
    st = U(RpcServerListen)(1, 5, TRUE);
    if (st != RPC_S_OK) { U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&g_sif, NULL, FALSE); t_skip("RpcServerListen failed %ld", (long)st); return; }

    RPC_CSTR sb = NULL; RPC_BINDING_HANDLE h = NULL;
    st = U(RpcStringBindingComposeA)(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)ep, NULL, &sb);
    EXPECT_EQ(st, RPC_S_OK);
    st = U(RpcBindingFromStringBindingA)(sb, &h);
    EXPECT_EQ(st, RPC_S_OK);
    U(RpcStringFreeA)(&sb);

    RPC_MESSAGE rpc; MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC desc;
    n_fill_desc(&desc, NULL);
    desc.RpcInterfaceInformation = &g_sif;
    U(NdrClientInitializeNew)(&rpc, &sm, &desc, 0);
    n_alloc_cnt = 0;
    unsigned char *ret = U(NdrGetBuffer)(&sm, 4, h);
    CHECK(ret == sm.Buffer);
    EXPECT_EQ(n_alloc_cnt, 0);
    CHECK(rpc.Handle != NULL);
    CHECK(rpc.Buffer != NULL);
    EXPECT_EQ(rpc.BufferLength, 4);
    EXPECT_EQ(rpc.RpcFlags, 0);
    CHECK(sm.BufferStart == NULL && sm.BufferEnd == NULL);
    CHECK(sm.fBufferValid != 0);
    if (rpc.Buffer && ret) {
        *(ULONG *)sm.Buffer = 41;
        sm.BufferStart = sm.Buffer;
        unsigned char *ret2 = U(NdrSendReceive)(&sm, sm.Buffer + 4);
        t_note("NdrSendReceive returned %p", ret2);
        EXPECT_EQ(rpc.BufferLength, 4);
        CHECK(rpc.Buffer != NULL);
        if (rpc.Buffer && rpc.BufferLength >= 4) EXPECT_EQ(*(ULONG *)rpc.Buffer, 42);
        EXPECT_EQ(g_srv_calls, 1);
    }
    ULONG prev = rpc.BufferLength;
    sm.BufferLength = 1;
    n_free_cnt = 0;
    U(NdrFreeBuffer)(&sm);
    EXPECT_EQ(n_free_cnt, 0);
    CHECK(sm.fBufferValid == 0);
    EXPECT_EQ(sm.BufferLength, 1);
    EXPECT_EQ(rpc.BufferLength, prev);
    U(NdrFreeBuffer)(&sm);        /* double free is tolerated */

    U(RpcBindingFree)(&h);
    U(RpcMgmtStopServerListening)(NULL);
    U(RpcMgmtWaitServerListen)();
    U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&g_sif, NULL, FALSE);
}
