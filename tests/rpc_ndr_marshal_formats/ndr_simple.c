/* NDR base types: NdrSimpleTypeMarshall / NdrSimpleTypeUnmarshall wire layout, alignment, bounds exceptions. */
#include "ndrh.h"

typedef struct { unsigned char fc; unsigned size, align; const char *name; } stype;
static const stype g_st[] = {
    { FC_BYTE, 1, 1, "byte" }, { FC_CHAR, 1, 1, "char" }, { FC_SMALL, 1, 1, "small" }, { FC_USMALL, 1, 1, "usmall" },
    { FC_WCHAR, 2, 2, "wchar" }, { FC_SHORT, 2, 2, "short" }, { FC_USHORT, 2, 2, "ushort" },
    { FC_LONG, 4, 4, "long" }, { FC_ULONG, 4, 4, "ulong" }, { FC_FLOAT, 4, 4, "float" },
    { FC_HYPER, 8, 8, "hyper" }, { FC_DOUBLE, 8, 8, "double" },
    { FC_ENUM16, 2, 2, "enum16" }, { FC_ENUM32, 4, 4, "enum32" }, { FC_ERROR_STATUS_T, 4, 4, "error_status_t" },
};
#define NST ((int)(sizeof g_st / sizeof g_st[0]))

static const unsigned char pat[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x7a };  /* top bit clear: valid enum16 */
/* enum16 lives in a 4 byte C enum: its upper half must be zero or marshalling raises RPC_X_ENUM_VALUE_OUT_OF_RANGE */
static void mkmem(unsigned char *mem, unsigned char fc) { memcpy(mem, pat, 8); if (fc == FC_ENUM16) mem[2] = mem[3] = 0; }
static ULONG_PTR al(ULONG_PTR p, unsigned a) { return (p + a - 1) & ~(ULONG_PTR)(a - 1); }

TEST(ndr_simple_marshal_aligned_start) {
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) {
        nctx c; nc_client(&c, NULL, 32);
        unsigned char mem[8]; mkmem(mem, g_st[i].fc);
        U(NdrSimpleTypeMarshall)(&c.sm, mem, g_st[i].fc);
        CHECKF(nc_used(&c) == g_st[i].size, "%s used %lu", g_st[i].name, nc_used(&c));
        CHECKF(!memcmp(c.buf, pat, g_st[i].size), "%s bytes", g_st[i].name);
        CHECKF(c.buf[g_st[i].size] == 0xCD, "%s overran", g_st[i].name);
        nc_done(&c);
    }
}
TEST(ndr_simple_marshal_alignment_offsets) {
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) for (int off = 0; off < 8; off++) {
        nctx c; nc_client(&c, NULL, 64);
        unsigned char mem[8]; mkmem(mem, g_st[i].fc);
        c.sm.Buffer = c.buf + off;
        U(NdrSimpleTypeMarshall)(&c.sm, mem, g_st[i].fc);
        ULONG_PTR start = al((ULONG_PTR)c.buf + off, g_st[i].align);
        CHECKF((ULONG_PTR)c.sm.Buffer == start + g_st[i].size, "%s off %d end", g_st[i].name, off);
        CHECKF(!memcmp((void *)start, pat, g_st[i].size), "%s off %d bytes", g_st[i].name, off);
        nc_done(&c);
    }
}
TEST(ndr_simple_marshal_sequence_packing) {
    /* BYTE, SHORT, LONG, HYPER back to back: 1 + pad1 + 2 + 4 + pad4 + 8 = 20 bytes after the first byte's slot */
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 64);
    unsigned char b = 0xAB; unsigned short s = 0x1234; unsigned l = 0xCAFEBABE; unsigned long long h = 0x0123456789ABCDEFull;
    U(NdrSimpleTypeMarshall)(&c.sm, &b, FC_BYTE);   EXPECT_EQ(nc_used(&c), 1);
    U(NdrSimpleTypeMarshall)(&c.sm, (unsigned char *)&s, FC_SHORT); EXPECT_EQ(nc_used(&c), 4);
    U(NdrSimpleTypeMarshall)(&c.sm, (unsigned char *)&l, FC_LONG);  EXPECT_EQ(nc_used(&c), 8);
    U(NdrSimpleTypeMarshall)(&c.sm, (unsigned char *)&h, FC_HYPER); EXPECT_EQ(nc_used(&c), 16);
    EXPECT_EQ(c.buf[0], 0xAB);
    EXPECT_EQ(*(unsigned short *)(c.buf + 2), 0x1234);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 0xCAFEBABE);
    EXPECT_EQ(*(unsigned long long *)(c.buf + 8), 0x0123456789ABCDEFull);
    nc_done(&c);
}
TEST(ndr_simple_marshal_enum16_from_enum32_memory) {
    /* enum16 lives in a 4-byte C enum in memory but occupies 2 bytes on the wire */
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 16);
    unsigned v = 0x7ffe;
    U(NdrSimpleTypeMarshall)(&c.sm, (unsigned char *)&v, FC_ENUM16);
    EXPECT_EQ(nc_used(&c), 2);
    EXPECT_EQ(*(unsigned short *)c.buf, 0x7ffe);
    nc_done(&c);
}
TEST(ndr_simple_marshal_ignore_writes_nothing) {
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 16);
    unsigned char mem[8] = { 1 };
    U(NdrSimpleTypeMarshall)(&c.sm, mem, FC_IGNORE);
    EXPECT_EQ(nc_used(&c), 0);
    EXPECT_EQ(c.buf[0], 0xCD);
    nc_done(&c);
}
TEST(ndr_simple_marshal_float_double_bit_exact) {
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, NULL, 32);
    unsigned fb = 0x7fc00001; /* a NaN payload must be preserved bit for bit */
    unsigned long long db = 0xfff8000000000123ull;
    U(NdrSimpleTypeMarshall)(&c.sm, (unsigned char *)&fb, FC_FLOAT);
    U(NdrSimpleTypeMarshall)(&c.sm, (unsigned char *)&db, FC_DOUBLE);
    EXPECT_EQ(*(unsigned *)c.buf, 0x7fc00001);
    EXPECT_EQ(nc_used(&c), 16);
    EXPECT_EQ(*(unsigned long long *)(c.buf + 8), 0xfff8000000000123ull);
    nc_done(&c);
}

TEST(ndr_simple_unmarshal_values) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) {
        nctx c; nc_rx(&c, NULL, pat, 8, 1);
        unsigned char out[16]; memset(out, 0xEE, 16);
        U(NdrSimpleTypeUnmarshall)(&c.sm, out, g_st[i].fc);
        CHECKF(nc_used(&c) == g_st[i].size, "%s used %lu", g_st[i].name, nc_used(&c));
        CHECKF(!memcmp(out, pat, g_st[i].size), "%s value", g_st[i].name);
        if (g_st[i].fc == FC_ENUM16) {
            /* decomp: enum16 is widened to a 4 byte C enum, upper half zeroed */
            CHECK(out[2] == 0 && out[3] == 0);
        } else {
            CHECKF(out[g_st[i].size] == 0xEE, "%s wrote past size", g_st[i].name);
        }
        nc_done(&c);
    }
}
TEST(ndr_simple_unmarshal_alignment_offsets) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) for (int off = 0; off < 8; off++) {
        unsigned char wire[32]; for (int k = 0; k < 32; k++) wire[k] = (unsigned char)(0x40 + k);
        nctx c; nc_rx(&c, NULL, wire, 32, 1);
        c.sm.Buffer = c.buf + off;
        unsigned char out[8] = { 0 };
        U(NdrSimpleTypeUnmarshall)(&c.sm, out, g_st[i].fc);
        ULONG_PTR start = al((ULONG_PTR)c.buf + off, g_st[i].align);
        CHECKF((ULONG_PTR)c.sm.Buffer == start + g_st[i].size, "%s off %d end", g_st[i].name, off);
        CHECKF(!memcmp(out, (void *)start, g_st[i].size), "%s off %d value", g_st[i].name, off);
        nc_done(&c);
    }
}
TEST(ndr_simple_unmarshal_exact_fit_ok) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) {
        nctx c; nc_rx(&c, NULL, pat, g_st[i].size, 1);
        unsigned char out[8];
        n_code = 0;
        NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, out, g_st[i].fc); NEND;
        CHECKF(n_code == 0, "%s exact fit raised %lu", g_st[i].name, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_simple_unmarshal_short_buffer_raises_bad_stub_data) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) {
        nctx c; nc_rx(&c, NULL, pat, g_st[i].size - 1, 1);
        unsigned char out[8];
        NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, out, g_st[i].fc); NEND;
        CHECKF(n_code == X_BAD_STUB, "%s short raised %lu", g_st[i].name, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_simple_unmarshal_empty_buffer_raises_bad_stub_data) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) {
        if (g_st[i].fc == FC_IGNORE) continue;
        nctx c; nc_rx(&c, NULL, pat, 0, 1);
        unsigned char out[8];
        NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, out, g_st[i].fc); NEND;
        CHECKF(n_code == X_BAD_STUB, "%s empty raised %lu", g_st[i].name, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_simple_unmarshal_padding_counts_toward_bounds) {
    /* 4 bytes remain but the 4-byte value needs 3 pad bytes first (buffer starts at offset 1): 1+3+4 = 8 bytes needed */
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_rx(&c, NULL, pat, 8, 1);
    c.sm.Buffer = c.buf + 1; c.sm.BufferEnd = c.buf + 7;
    unsigned out;
    NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, (unsigned char *)&out, FC_LONG); NEND;
    EXPECT_EQ(n_code, X_BAD_STUB);
    nc_done(&c);
}
TEST(ndr_simple_unmarshal_enum16_range) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const struct { unsigned short wire; DWORD code; } t[] = {
        { 0, 0 }, { 1, 0 }, { 0x7fff, 0 }, { 0x8000, X_ENUM_RANGE }, { 0xffff, X_ENUM_RANGE }, { 0xff00, X_ENUM_RANGE },
    };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        nctx c; nc_rx(&c, NULL, &t[k].wire, 2, 1);
        unsigned out = 0xFFFFFFFF;
        NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, (unsigned char *)&out, FC_ENUM16); NEND;
        CHECKF(n_code == t[k].code, "wire 0x%x raised %lu want %lu", t[k].wire, (unsigned long)n_code, (unsigned long)t[k].code);
        if (!t[k].code) CHECKF(out == t[k].wire, "wire 0x%x out 0x%x", t[k].wire, out);
        nc_done(&c);
    }
}
TEST(ndr_simple_unmarshal_enum32_has_no_range_check) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned w = 0xFFFFFFF0;
    nctx c; nc_rx(&c, NULL, &w, 4, 1);
    unsigned out = 0;
    NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, (unsigned char *)&out, FC_ENUM32); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(out, 0xFFFFFFF0);
    nc_done(&c);
}
TEST(ndr_simple_unmarshal_ignore_consumes_nothing) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_rx(&c, NULL, pat, 0, 1);
    unsigned char out[4] = { 0x5a, 0x5a, 0x5a, 0x5a };
    NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, out, FC_IGNORE); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(nc_used(&c), 0);
    EXPECT_EQ(out[0], 0x5a);
    nc_done(&c);
}
TEST(ndr_simple_unmarshal_int3264_widens) {
    /* FC_INT3264 / FC_UINT3264: 4 bytes on the wire, pointer sized (8 bytes) in 64-bit memory */
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned w = 0xFFFFFFFE;
    long long si = 0; unsigned long long ui = 0;
    nctx c; nc_rx(&c, NULL, &w, 4, 1);
    U(NdrSimpleTypeUnmarshall)(&c.sm, (unsigned char *)&si, 0xb8);
    EXPECT_EQ(nc_used(&c), 4);
    CHECK(si == -2);
    nc_done(&c);
    nc_rx(&c, NULL, &w, 4, 1);
    U(NdrSimpleTypeUnmarshall)(&c.sm, (unsigned char *)&ui, 0xb9);
    EXPECT_EQ(nc_used(&c), 4);
    CHECK(ui == 0xFFFFFFFEull);
    nc_done(&c);
}
TEST(ndr_simple_unmarshal_invalid_format_char_raises_internal_error) {
    REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned char bad[] = { 0x00, 0x11, 0x12, 0x1a, 0x20, 0x2f, 0x5b, 0x5c, 0xb7, 0xba, 0xff };
    for (unsigned k = 0; k < sizeof bad; k++) {
        nctx c; nc_rx(&c, NULL, pat, 8, 1);
        unsigned char out[8];
        NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, out, bad[k]); NEND;
        CHECKF(n_code == X_INTERNAL, "fc 0x%02x raised %lu", bad[k], (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_simple_marshal_invalid_format_char_raises_internal_error) {
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned char bad[] = { 0x3c, 0x3f, 0x7c, 0xbf };   /* (fc & 0x3f) >= 0x3c rejected before dispatch */
    for (unsigned k = 0; k < sizeof bad; k++) {
        nctx c; nc_client(&c, NULL, 16);
        unsigned char mem[8] = { 0 };
        NTRY U(NdrSimpleTypeMarshall)(&c.sm, mem, bad[k]); NEND;
        CHECKF(n_code == X_INTERNAL, "fc 0x%02x raised %lu", bad[k], (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_simple_roundtrip_all_types_all_offsets) {
    REQUIRE(U(NdrSimpleTypeMarshall)); REQUIRE(U(NdrSimpleTypeUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NST; i++) for (int off = 0; off < 8; off++) {
        nctx c; nc_client(&c, NULL, 64);
        unsigned char mem[8], out[8] = { 0 }; mkmem(mem, g_st[i].fc);
        c.sm.Buffer = c.buf + off;
        NTRY U(NdrSimpleTypeMarshall)(&c.sm, mem, g_st[i].fc); NEND;
        CHECKF(n_code == 0, "%s off %d marshal raised %lu", g_st[i].name, off, (unsigned long)n_code);
        ULONG_PTR end = (ULONG_PTR)c.sm.Buffer;
        c.sm.Buffer = c.buf + off;
        NTRY U(NdrSimpleTypeUnmarshall)(&c.sm, out, g_st[i].fc); NEND;
        CHECKF(n_code == 0, "%s off %d unmarshal raised %lu", g_st[i].name, off, (unsigned long)n_code);
        CHECKF((ULONG_PTR)c.sm.Buffer == end, "%s off %d symmetric advance", g_st[i].name, off);
        CHECKF(!memcmp(out, mem, g_st[i].size), "%s off %d roundtrip", g_st[i].name, off);
        nc_done(&c);
    }
}

/* ---- NdrClientInitializeNew / NdrServerInitializeNew field state ---- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
TEST(ndr_init_client_new_fields) {
    REQUIRE(U(NdrClientInitializeNew));
    MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char fmt[2] = { 0, 0 };
    n_fill_desc(&d, fmt);
    memset(&rpc, 0xcc, sizeof rpc); memset(&sm, 0xcc, sizeof sm);
    U(NdrClientInitializeNew)(&rpc, &sm, &d, 1);
    EXPECT_EQ(sm.RpcMsg, &rpc);
    EXPECT_EQ(rpc.Handle, NULL);
    EXPECT_EQ(rpc.ProcNum, 0x8001);          /* ProcNum | RPC_FLAG_? 0x8000 marker */
    EXPECT_EQ(rpc.RpcFlags, 0);
    EXPECT_EQ(rpc.RpcInterfaceInformation, d.RpcInterfaceInformation);
    EXPECT_EQ(sm.BufferStart, NULL);
    EXPECT_EQ(sm.BufferEnd, NULL);
    EXPECT_EQ(sm.BufferLength, 0);
    EXPECT_EQ(sm.IsClient, 1);
    EXPECT_EQ(sm.ReuseBuffer, 0);
    EXPECT_EQ(sm.Pad, 0);
    EXPECT_EQ(sm.StubDesc, &d);
    EXPECT_EQ(sm.pfnAllocate, d.pfnAllocate);
    EXPECT_EQ(sm.pfnFree, d.pfnFree);
    EXPECT_EQ(sm.fInDontFree, 0);
    EXPECT_EQ(sm.fDontCallFreeInst, 0);
    EXPECT_EQ(sm.fHasReturn, 0);
    EXPECT_EQ(sm.fHasExtensions, 0);
    EXPECT_EQ(sm.fHasNewCorrDesc, 0);
    EXPECT_EQ(sm.pAsyncMsg, NULL);
    EXPECT_EQ(sm.pCorrInfo, NULL);
    EXPECT_EQ(sm.pArrayInfo, NULL);
    t_note("client init: SavedHandle=%p dwDestContext=%lu", sm.SavedHandle, (unsigned long)sm.dwDestContext);
    EXPECT_EQ(sm.StackTop, NULL);
    EXPECT_EQ(sm.UniquePtrCount, 0);
    EXPECT_EQ(sm.pMemoryList, NULL);
    EXPECT_EQ(sm.pvDestContext, NULL);
    EXPECT_EQ(sm.pRpcChannelBuffer, NULL);
}
TEST(ndr_init_client_new_procnum_marker) {
    REQUIRE(U(NdrClientInitializeNew));
    static const unsigned procs[] = { 0, 1, 5, 0x7fff };
    for (unsigned k = 0; k < sizeof procs / sizeof procs[0]; k++) {
        MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
        U(NdrClientInitializeNew)(&rpc, &sm, &d, procs[k]);
        CHECKF(rpc.ProcNum == (procs[k] | 0x8000), "proc %u -> 0x%x", procs[k], rpc.ProcNum);
    }
}
TEST(ndr_init_server_new_fields) {
    REQUIRE(U(NdrServerInitializeNew));
    MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char buffer[256];
    n_fill_desc(&d, NULL);
    memset(&rpc, 0, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = sizeof buffer; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
    memset(&sm, 0xcc, sizeof sm);
    unsigned char *r = U(NdrServerInitializeNew)(&rpc, &sm, &d);
    EXPECT_EQ(r, NULL);
    EXPECT_EQ(sm.RpcMsg, &rpc);
    EXPECT_EQ(sm.Buffer, buffer);
    EXPECT_EQ(sm.BufferStart, buffer);
    EXPECT_EQ(sm.BufferEnd, buffer + sizeof buffer);
    EXPECT_EQ(sm.IsClient, 0);
    EXPECT_EQ(sm.ReuseBuffer, 0);
    EXPECT_EQ(sm.Pad, 0);
    EXPECT_EQ(sm.FullPtrXlatTables, NULL);
    EXPECT_EQ(sm.StubDesc, &d);
    EXPECT_EQ(sm.pfnAllocate, d.pfnAllocate);
    EXPECT_EQ(sm.pfnFree, d.pfnFree);
    EXPECT_EQ(sm.pMemoryList, NULL);
}
TEST(ndr_init_server_new_buffer_end_follows_buffer_length) {
    REQUIRE(U(NdrServerInitializeNew));
    static const ULONG lens[] = { 0, 1, 7, 64, 4096 };
    for (unsigned k = 0; k < sizeof lens / sizeof lens[0]; k++) {
        MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char buffer[4096];
        n_fill_desc(&d, NULL);
        memset(&rpc, 0, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = lens[k]; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
        U(NdrServerInitializeNew)(&rpc, &sm, &d);
        CHECKF(sm.BufferEnd == buffer + lens[k], "len %lu", lens[k]);
    }
}
TEST(ndr_init_client_old_variant_matches_new_for_core_fields) {
    WSTUB("NdrClientInitialize");
    REQUIRE(U(NdrClientInitialize)); REQUIRE(U(NdrClientInitializeNew));
    MIDL_STUB_MESSAGE a, b; RPC_MESSAGE ra, rb; MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    memset(&a, 0xcc, sizeof a); memset(&b, 0xcc, sizeof b);
    U(NdrClientInitialize)(&ra, &a, &d, 3);
    U(NdrClientInitializeNew)(&rb, &b, &d, 3);
    EXPECT_EQ(a.IsClient, b.IsClient);
    EXPECT_EQ(a.StubDesc, b.StubDesc);
    EXPECT_EQ(a.pfnAllocate, b.pfnAllocate);
    EXPECT_EQ(a.pfnFree, b.pfnFree);
    EXPECT_EQ(ra.ProcNum, rb.ProcNum);
    EXPECT_EQ(a.UniquePtrCount, b.UniquePtrCount);
}
TEST(ndr_init_server_initialize_unmarshall_sets_up_reader) {
    WSTUB("NdrServerInitializeUnmarshall");
    REQUIRE(U(NdrServerInitializeUnmarshall));
    MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char buffer[64];
    n_fill_desc(&d, NULL);
    memset(&rpc, 0, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = 40; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
    memset(&sm, 0xcc, sizeof sm);
    unsigned char *r = U(NdrServerInitializeUnmarshall)(&sm, &d, &rpc);
    EXPECT_EQ(r, NULL);
    EXPECT_EQ(sm.Buffer, buffer);
    EXPECT_EQ(sm.BufferStart, buffer);
    EXPECT_EQ(sm.BufferEnd, buffer + 40);
    EXPECT_EQ(sm.IsClient, 0);
    EXPECT_EQ(sm.RpcMsg, &rpc);
}
TEST(ndr_init_server_initialize_marshall_resets_for_reply) {
    WSTUB("NdrServerInitializeMarshall");
    REQUIRE(U(NdrServerInitializeUnmarshall)); REQUIRE(U(NdrServerInitializeMarshall));
    MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char buffer[64];
    n_fill_desc(&d, NULL);
    memset(&rpc, 0, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = 40; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
    U(NdrServerInitializeUnmarshall)(&sm, &d, &rpc);
    sm.fBufferValid = 1;
    U(NdrServerInitializeMarshall)(&rpc, &sm);
    EXPECT_EQ(sm.BufferLength, 0);
    EXPECT_EQ(sm.fBufferValid, 0);
    EXPECT_EQ(sm.IsClient, 0);
}
TEST(ndr_init_server_initialize_partial_records_requested_size) {
    WSTUB("NdrServerInitializePartial");
    REQUIRE(U(NdrServerInitializePartial));
    MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char buffer[64];
    n_fill_desc(&d, NULL);
    memset(&rpc, 0, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = 40; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
    memset(&sm, 0xcc, sizeof sm);
    U(NdrServerInitializePartial)(&rpc, &sm, &d, 123);
    EXPECT_EQ(sm.RpcMsg, &rpc);
    EXPECT_EQ(sm.IsClient, 0);
    EXPECT_EQ(sm.Buffer, buffer);
    EXPECT_EQ(sm.BufferLength, 123);
}
