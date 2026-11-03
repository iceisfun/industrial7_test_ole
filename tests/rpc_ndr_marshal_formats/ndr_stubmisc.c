/* NdrClearOutParameters, NdrStubInitialize, interface pointer NULL handling, remaining small entry points. */
#include "ndrh.h"

static const unsigned char f_lng_rp[] = { FC_RP, 0x08, FC_LONG, FC_PAD };
static const unsigned char f_s3[] = { FC_STRUCT, 1, S16(4), FC_SHORT, FC_SHORT, FC_END };

TEST(ndr_clearout_simple_pointer_zeroes_pointee_in_proxy_unmarshal_phase) {
    REQUIRE(U(NdrClearOutParameters)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, f_lng_rp, 16);
    unsigned v[3] = { 0x11111111, 0x22222222, 0x33333333 };
    c.sm.dwStubPhase = 4;     /* PROXY_UNMARSHAL */
    U(NdrClearOutParameters)(&c.sm, f_lng_rp, &v[0]);
    EXPECT_EQ(v[0], 0); EXPECT_EQ(v[1], 0x22222222); EXPECT_EQ(v[2], 0x33333333);
    nc_done(&c);
}
TEST(ndr_clearout_other_phases_do_nothing) {
    REQUIRE(U(NdrClearOutParameters)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned phases[] = { 0, 1, 2, 3, 5, 6 };
    for (unsigned k = 0; k < 6; k++) {
        nctx c; nc_client(&c, f_lng_rp, 16);
        unsigned v = 0xABCD1234;
        c.sm.dwStubPhase = phases[k];
        U(NdrClearOutParameters)(&c.sm, f_lng_rp, &v);
        CHECKF(v == 0xABCD1234, "phase %u cleared the argument", phases[k]);
        nc_done(&c);
    }
}
TEST(ndr_clearout_null_argument_is_ignored) {
    REQUIRE(U(NdrClearOutParameters)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, f_lng_rp, 16);
    c.sm.dwStubPhase = 4;
    NTRY U(NdrClearOutParameters)(&c.sm, f_lng_rp, NULL); NEND;
    EXPECT_EQ(n_code, 0);
    nc_done(&c);
}
TEST(ndr_clearout_simple_pointer_sizes_follow_base_type) {
    REQUIRE(U(NdrClearOutParameters)); REQUIRE(U(NdrClientInitializeNew));
    static const struct { unsigned char fc; unsigned size; } t[] = { { FC_BYTE, 1 }, { FC_SHORT, 2 }, { FC_LONG, 4 }, { FC_HYPER, 8 }, { FC_DOUBLE, 8 }, { FC_FLOAT, 4 }, { FC_ENUM16, 4 } };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        unsigned char f[4] = { FC_RP, 0x08, t[k].fc, FC_PAD };
        nctx c; nc_client(&c, f, 16);
        unsigned char v[16]; memset(v, 0xEE, sizeof v);
        c.sm.dwStubPhase = 4;
        U(NdrClearOutParameters)(&c.sm, f, v);
        unsigned i;
        for (i = 0; i < t[k].size; i++) if (v[i] != 0) break;
        CHECKF(i == t[k].size, "fc %u: first %u bytes not cleared", t[k].fc, t[k].size);
        CHECKF(v[t[k].size] == 0xEE, "fc %u: cleared more than %u bytes", t[k].fc, t[k].size);
        nc_done(&c);
    }
}
TEST(ndr_clearout_struct_pointee_zeroes_whole_struct) {
    REQUIRE(U(NdrClearOutParameters)); REQUIRE(U(NdrClientInitializeNew));
    /* RP -> complex pointee at +2: FC_STRUCT {short, short} */
    unsigned char fmt[] = { FC_RP, 0x00, S16(2), FC_STRUCT, 1, S16(4), FC_SHORT, FC_SHORT, FC_END };
    nctx c; nc_client(&c, fmt, 16);
    unsigned char v[8]; memset(v, 0xEE, sizeof v);
    c.sm.dwStubPhase = 4;
    U(NdrClearOutParameters)(&c.sm, fmt, v);
    CHECK(v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
    EXPECT_EQ(v[4], 0xEE);
    nc_done(&c);
}

/* ---------------- NdrStubInitialize with a fake channel ---------------- */
typedef struct fake_chan { const struct fake_chan_vtbl *v; int destctx_calls; } fake_chan;
struct fake_chan_vtbl {
    HRESULT (WINAPI *QI)(void *, REFIID, void **); ULONG (WINAPI *AddRef)(void *); ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetBuffer)(void *, void *, REFIID); HRESULT (WINAPI *SendReceive)(void *, void *, ULONG *);
    HRESULT (WINAPI *FreeBuffer)(void *, void *); HRESULT (WINAPI *GetDestCtx)(void *, DWORD *, void **); HRESULT (WINAPI *IsConnected)(void *);
};
static HRESULT WINAPI fc_qi(void *t, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI fc_addref(void *t) { return 2; }
static ULONG WINAPI fc_release(void *t) { return 1; }
static HRESULT WINAPI fc_getbuf(void *t, void *m, REFIID r) { return E_NOTIMPL; }
static HRESULT WINAPI fc_sr(void *t, void *m, ULONG *s) { return E_NOTIMPL; }
static HRESULT WINAPI fc_free(void *t, void *m) { return E_NOTIMPL; }
static HRESULT WINAPI fc_destctx(void *t, DWORD *ctx, void **pv) { ((fake_chan *)t)->destctx_calls++; *ctx = MSHCTX_DIFFERENTMACHINE; *pv = (void *)(ULONG_PTR)0x5150; return S_OK; }
static HRESULT WINAPI fc_isconn(void *t) { return S_OK; }
static const struct fake_chan_vtbl g_fcv = { fc_qi, fc_addref, fc_release, fc_getbuf, fc_sr, fc_free, fc_destctx, fc_isconn };

UDECL(void, NdrStubInitialize, (PRPC_MESSAGE, PMIDL_STUB_MESSAGE, PMIDL_STUB_DESC, void *));
TEST(ndr_stubinitialize_records_channel_and_dest_context) {
    REQUIRE(UN(NdrStubInitialize));
    MIDL_STUB_MESSAGE sm; RPC_MESSAGE rpc; MIDL_STUB_DESC d; unsigned char buffer[64];
    fake_chan ch = { &g_fcv, 0 };
    n_fill_desc(&d, NULL);
    memset(&rpc, 0, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = 40; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
    memset(&sm, 0xcc, sizeof sm);
    NTRY UN(NdrStubInitialize)(&rpc, &sm, &d, &ch); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(sm.RpcMsg, &rpc);
    EXPECT_EQ(sm.Buffer, buffer);
    EXPECT_EQ(sm.BufferStart, buffer);
    EXPECT_EQ(sm.BufferEnd, buffer + 40);
    EXPECT_EQ(sm.IsClient, 0);
    EXPECT_EQ(sm.StubDesc, &d);
    EXPECT_EQ(sm.pfnAllocate, d.pfnAllocate);
    EXPECT_EQ(sm.pfnFree, d.pfnFree);
    EXPECT_EQ(sm.pRpcChannelBuffer, (void *)&ch);
    EXPECT_EQ(ch.destctx_calls, 1);
    EXPECT_EQ(sm.dwDestContext, MSHCTX_DIFFERENTMACHINE);
    EXPECT_EQ(sm.pvDestContext, (void *)(ULONG_PTR)0x5150);
}

/* ---------------- interface pointers: NULL round trip needs no marshaling of an object ---------------- */
static const unsigned char f_ip[] = { FC_IP, FC_PAD, FC_CONSTANT_IID, 0x00,0x00,0x00,0x00, 0x00,0x00, 0x00,0x00, 0xC0,0,0,0,0,0,0,0x46 };   /* IID_IUnknown */
TEST(ndr_ip_null_pointer_roundtrip) {
    REQUIRE(U(NdrInterfacePointerBufferSize)); REQUIRE(U(NdrInterfacePointerMarshall)); REQUIRE(U(NdrInterfacePointerUnmarshall));
    REQUIRE(U(NdrInterfacePointerMemorySize)); REQUIRE(U(NdrInterfacePointerFree));
    unsigned char fmt[sizeof f_ip]; memcpy(fmt, f_ip, sizeof fmt);
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    c.sm.BufferLength = 0;
    NTRY U(NdrInterfacePointerBufferSize)(&c.sm, NULL, fmt); NEND;
    EXPECT_EQ(n_code, 0);
    ULONG pred = c.sm.BufferLength;
    NTRY U(NdrInterfacePointerMarshall)(&c.sm, NULL, fmt); NEND;
    EXPECT_EQ(n_code, 0);
    ULONG used = nc_used(&c);
    t_note("NULL interface pointer: BufferSize %lu, marshalled %lu bytes", pred, used);
    CHECK(pred >= used);
    CHECK(used >= 4 && used <= 16);
    nc_done(&c);
}
