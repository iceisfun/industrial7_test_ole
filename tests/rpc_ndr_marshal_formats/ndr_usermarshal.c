/* NdrUserMarshal*, NdrXmitOrRepAs*, NdrRangeUnmarshall, NdrGetUserMarshalInfo (wire formats and callback protocol from the Win10 decomp). */
#include "ndrh.h"
#define W32(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

/* ---------------- user marshal: a 4-byte value marshalled by callbacks ---------------- */
static struct {
    int n[4];                       /* calls: size, marshal, unmarshal, free */
    ULONG flags[4];
    ULONG cbtype[4];
    ULONG sig[4];
    ULONG start;                    /* BufferSize start argument */
    void *obj[4];
    unsigned char *buf[4];
    ULONG info_status[4];
    NDR_USER_MARSHAL_INFO info[4];
    PMIDL_STUB_MESSAGE sm[4];
} um;

static void um_record(int i, ULONG *flags, void *obj) {
    USER_MARSHAL_CB *cb = (USER_MARSHAL_CB *)flags;
    um.n[i]++; um.flags[i] = *flags; um.obj[i] = obj;
    um.cbtype[i] = cb->CBType; um.sig[i] = cb->Signature; um.sm[i] = cb->pStubMsg;
    memset(&um.info[i], 0xAA, sizeof um.info[i]);
    um.info_status[i] = U(NdrGetUserMarshalInfo)(flags, 1, &um.info[i]);
}
static ULONG WINAPI um_size(ULONG *flags, ULONG start, void *obj) { um_record(0, flags, obj); um.start = start; return start + 4; }
static unsigned char *WINAPI um_marshal(ULONG *flags, unsigned char *buf, void *obj) { um_record(1, flags, obj); um.buf[1] = buf; *(ULONG *)buf = *(ULONG *)obj ^ 0xA5A5A5A5; return buf + 4; }
static unsigned char *WINAPI um_unmarshal(ULONG *flags, unsigned char *buf, void *obj) { um_record(2, flags, obj); um.buf[2] = buf; *(ULONG *)obj = *(ULONG *)buf ^ 0xA5A5A5A5; return buf + 4; }
static void WINAPI um_free(ULONG *flags, void *obj) { um_record(3, flags, obj); }
static USER_MARSHAL_ROUTINE_QUADRUPLE g_quad[1] = { { um_size, um_marshal, um_unmarshal, um_free } };

/* flags nibble: low = alignment mask */
static void mk_um(unsigned char *f, unsigned char flagbyte, unsigned wire_size) {
    unsigned char t[] = { FC_USER_MARSHAL, flagbyte, S16(0), S16(4), S16(wire_size), S16(2), FC_LONG, FC_END };
    memcpy(f, t, sizeof t);
}
static void um_reset(void) { memset(&um, 0, sizeof um); }
static unsigned char *WINAPI noop_m(ULONG *fl, unsigned char *b, void *o) { return b; }
static unsigned char *WINAPI big_m(ULONG *fl, unsigned char *b, void *o) { return b + 1000; }

TEST(ndr_usermarshal_buffersize_calls_callback_aligned) {
    REQUIRE(U(NdrUserMarshalBufferSize)); REQUIRE(U(NdrGetUserMarshalInfo));
    unsigned char f[16]; mk_um(f, 0x03, 0);
    for (ULONG start = 0; start < 9; start++) {
        um_reset();
        nctx c; nc_client(&c, f, 16);
        c.desc.aUserMarshalQuadruple = g_quad;
        c.sm.dwDestContext = MSHCTX_DIFFERENTMACHINE;
        c.sm.BufferLength = start;
        ULONG obj = 7;
        U(NdrUserMarshalBufferSize)(&c.sm, (unsigned char *)&obj, f);
        ULONG want_start = (start + 3) & ~3u;
        CHECKF(um.n[0] == 1, "start %lu calls %d", start, um.n[0]);
        CHECKF(um.start == want_start, "start %lu callback start %lu want %lu", start, um.start, want_start);
        CHECKF(c.sm.BufferLength == want_start + 4, "start %lu BufferLength %lu", start, c.sm.BufferLength);
        CHECKF(um.obj[0] == &obj, "obj pointer");
        nc_done(&c);
    }
}
TEST(ndr_usermarshal_callback_flags_and_cb_header) {
    REQUIRE(U(NdrUserMarshalBufferSize)); REQUIRE(U(NdrUserMarshalMarshall)); REQUIRE(U(NdrUserMarshalUnmarshall)); REQUIRE(U(NdrUserMarshalFree));
    unsigned char f[16]; mk_um(f, 0x03, 0);
    ULONG obj = 0x11223344;
    um_reset();
    nctx c; nc_client(&c, f, 64);
    c.desc.aUserMarshalQuadruple = g_quad;
    c.sm.dwDestContext = MSHCTX_DIFFERENTMACHINE;
    c.sm.BufferLength = 0;
    U(NdrUserMarshalBufferSize)(&c.sm, (unsigned char *)&obj, f);
    U(NdrUserMarshalMarshall)(&c.sm, (unsigned char *)&obj, f);
    U(NdrUserMarshalFree)(&c.sm, (unsigned char *)&obj, f);
    EXPECT_EQ(um.cbtype[0], USER_MARSHAL_CB_BUFFER_SIZE);
    EXPECT_EQ(um.cbtype[1], USER_MARSHAL_CB_MARSHALL);
    EXPECT_EQ(um.cbtype[3], USER_MARSHAL_CB_FREE);
    for (int i = 0; i < 4; i += (i == 1) ? 2 : 1) {
        CHECKF(um.sig[i] == USER_MARSHAL_CB_SIGNATURE, "cb %d signature 0x%lx", i, um.sig[i]);
        CHECKF((um.flags[i] & 0xff) == MSHCTX_DIFFERENTMACHINE, "cb %d dest context in flags: 0x%lx", i, um.flags[i]);
        CHECKF((um.flags[i] & 0x300) == 0, "cb %d no async/correlation bits: 0x%lx", i, um.flags[i]);
        CHECKF(um.sm[i] == &c.sm, "cb %d stub message pointer", i);
    }
    nc_done(&c);
}
TEST(ndr_usermarshal_flags_correlation_bit_follows_pad_byte) {
    REQUIRE(U(NdrUserMarshalBufferSize)); REQUIRE(U(NdrCorrelationInitialize));
    unsigned char f[16]; mk_um(f, 0x03, 0);
    ULONG obj = 1;
    um_reset();
    nctx c; nc_client(&c, f, 64);
    c.desc.aUserMarshalQuadruple = g_quad;
    c.sm.Pad = 2;     /* what NdrCorrelationInitialize leaves behind: new-style correlation descriptors */
    U(NdrUserMarshalBufferSize)(&c.sm, (unsigned char *)&obj, f);
    EXPECT_EQ(um.flags[0] & 0x200, 0x200);
    nc_done(&c);
}
TEST(ndr_usermarshal_marshal_aligns_and_uses_returned_pointer) {
    REQUIRE(U(NdrUserMarshalMarshall));
    for (int align = 0; align < 8; align += 3) {   /* masks 0 and 3 and 6->(only 7 is valid for 8) */
        if (align == 6) align = 7;
        unsigned char f[16]; mk_um(f, (unsigned char)align, 4);
        for (int off = 0; off < 8; off++) {
            um_reset();
            nctx c; nc_client(&c, f, 64); memset(c.buf, 0, 64);
            c.desc.aUserMarshalQuadruple = g_quad;
            ULONG obj = 0x01020304;
            c.sm.Buffer = c.buf + off;
            U(NdrUserMarshalMarshall)(&c.sm, (unsigned char *)&obj, f);
            ULONG_PTR st = ((ULONG_PTR)c.buf + off + align) & ~(ULONG_PTR)align;
            CHECKF(um.n[1] == 1, "align %d off %d: calls %d", align, off, um.n[1]);
            CHECKF((ULONG_PTR)um.buf[1] == st, "align %d off %d: routine got %p want %p", align, off, um.buf[1], (void *)st);
            CHECKF((ULONG_PTR)c.sm.Buffer == st + 4, "align %d off %d: Buffer %p", align, off, c.sm.Buffer);
            CHECKF(*(ULONG *)st == (0x01020304 ^ 0xA5A5A5A5), "align %d off %d: data", align, off);
            nc_done(&c);
        }
    }
}
TEST(ndr_usermarshal_fixed_wire_size_skips_buffersize_callback) {
    REQUIRE(U(NdrUserMarshalBufferSize));
    unsigned char f[16]; mk_um(f, 0x03, 4);
    um_reset();
    nctx c; nc_client(&c, f, 16);
    c.desc.aUserMarshalQuadruple = g_quad;
    c.sm.BufferLength = 1;
    ULONG obj = 0;
    U(NdrUserMarshalBufferSize)(&c.sm, (unsigned char *)&obj, f);
    EXPECT_EQ(um.n[0], 0);
    EXPECT_EQ(c.sm.BufferLength, 8);
    nc_done(&c);
}
TEST(ndr_usermarshal_unmarshal_allocates_and_calls_routine) {
    REQUIRE(U(NdrUserMarshalUnmarshall)); REQUIRE(U(NdrServerInitializeNew));
    unsigned char f[16]; mk_um(f, 0x03, 4);
    ULONG wire = 0x11111111 ^ 0xA5A5A5A5;
    for (int client = 0; client < 2; client++) {
        um_reset();
        nctx c; nc_rx(&c, f, &wire, 4, client);
        c.desc.aUserMarshalQuadruple = g_quad;
        c.rpc.DataRepresentation = 0x10;
        c.sm.dwDestContext = MSHCTX_LOCAL;
        unsigned char *p = NULL; n_alloc_cnt = 0;
        U(NdrUserMarshalUnmarshall)(&c.sm, &p, f, 0);
        CHECKF(p != NULL && *(ULONG *)p == 0x11111111, "client=%d value", client);
        CHECKF(n_alloc_cnt == 1, "client=%d allocs %d", client, n_alloc_cnt);
        CHECKF(um.n[2] == 1 && um.buf[2] == c.buf, "client=%d routine call", client);
        CHECKF(nc_used(&c) == 4, "client=%d consumed", client);
        CHECKF(um.cbtype[2] == USER_MARSHAL_CB_UNMARSHALL, "client=%d cbtype", client);
        CHECKF((um.flags[2] & 0xff) == MSHCTX_LOCAL, "client=%d dest ctx 0x%lx", client, um.flags[2]);
        CHECKF((um.flags[2] >> 16) == 0x10, "client=%d data representation in high word: 0x%lx", client, um.flags[2]);
        CHECKF(um.info_status[2] == 0, "client=%d NdrGetUserMarshalInfo status %lu", client, um.info_status[2]);
        CHECKF(um.info[2].Level1.Buffer == c.buf, "client=%d info buffer", client);
        CHECKF(um.info[2].Level1.BufferSize == 4, "client=%d info bufsize %lu", client, um.info[2].Level1.BufferSize);
        if (p) n_free(p);
        nc_done(&c);
    }
}
TEST(ndr_usermarshal_unmarshal_into_existing_memory) {
    REQUIRE(U(NdrUserMarshalUnmarshall));
    unsigned char f[16]; mk_um(f, 0x03, 4);
    ULONG wire = 0x22222222 ^ 0xA5A5A5A5;
    um_reset();
    nctx c; nc_rx(&c, f, &wire, 4, 1);
    c.desc.aUserMarshalQuadruple = g_quad;
    ULONG dst = 0; unsigned char *p = (unsigned char *)&dst;
    n_alloc_cnt = 0;
    U(NdrUserMarshalUnmarshall)(&c.sm, &p, f, 0);
    CHECK(p == (unsigned char *)&dst);
    EXPECT_EQ(dst, 0x22222222);
    EXPECT_EQ(n_alloc_cnt, 0);
    nc_done(&c);
}
TEST(ndr_usermarshal_memorysize_and_free) {
    REQUIRE(U(NdrUserMarshalMemorySize)); REQUIRE(U(NdrUserMarshalFree));
    unsigned char f[16]; mk_um(f, 0x03, 4);
    unsigned w[2] = { 1, 2 };
    um_reset();
    nctx c; nc_rx(&c, f, w, 8, 1);
    c.desc.aUserMarshalQuadruple = g_quad;
    c.sm.MemorySize = 0;
    ULONG r = U(NdrUserMarshalMemorySize)(&c.sm, f);
    EXPECT_EQ(r, 4);
    EXPECT_EQ(c.sm.MemorySize, 4);
    EXPECT_EQ(nc_used(&c), 4);       /* walks the transmitted type (FC_LONG) */
    c.sm.MemorySize = 100;
    c.sm.Buffer = c.buf + 1;
    r = U(NdrUserMarshalMemorySize)(&c.sm, f);
    EXPECT_EQ(r, 104);
    EXPECT_EQ(nc_used(&c), 8);
    nc_done(&c);
    ULONG obj = 9;
    nc_client(&c, f, 16);
    c.desc.aUserMarshalQuadruple = g_quad;
    U(NdrUserMarshalFree)(&c.sm, (unsigned char *)&obj, f);
    EXPECT_EQ(um.n[3], 1);
    EXPECT_EQ(um.cbtype[3], USER_MARSHAL_CB_FREE);
    EXPECT_EQ(um.obj[3], &obj);
    nc_done(&c);
}
TEST(ndr_usermarshal_memorysize_short_buffer_raises) {
    REQUIRE(U(NdrUserMarshalMemorySize));
    unsigned char f[16]; mk_um(f, 0x03, 4);
    unsigned w[1] = { 1 };
    nctx c; nc_rx(&c, f, w, 3, 1);
    c.desc.aUserMarshalQuadruple = g_quad;
    NTRY U(NdrUserMarshalMemorySize)(&c.sm, f); NEND;
    EXPECT_EQ(n_code, X_BAD_STUB);
    nc_done(&c);
}
/* unique user-marshal pointers: referent id 'User' */
TEST(ndr_usermarshal_unique_writes_user_id) {
    REQUIRE(U(NdrUserMarshalMarshall)); REQUIRE(U(NdrUserMarshalBufferSize));
    unsigned char f[16]; mk_um(f, 0x80 | 0x03, 0);
    um_reset();
    nctx c; nc_client(&c, f, 64); memset(c.buf, 0, 64);
    c.desc.aUserMarshalQuadruple = g_quad;
    ULONG obj = 0x33333333;
    c.sm.BufferLength = 0;
    U(NdrUserMarshalBufferSize)(&c.sm, (unsigned char *)&obj, f);
    CHECKF(c.sm.BufferLength >= 8, "bufsize %lu", c.sm.BufferLength);
    U(NdrUserMarshalMarshall)(&c.sm, (unsigned char *)&obj, f);
    EXPECT_EQ(*(unsigned *)c.buf, 0x72657355);
    EXPECT_EQ(*(ULONG *)(c.buf + 4), 0x33333333 ^ 0xA5A5A5A5);
    EXPECT_EQ(nc_used(&c), 8);
    nc_done(&c);
}
TEST(ndr_usermarshal_unique_unmarshal_null_id_skips_routine) {
    REQUIRE(U(NdrUserMarshalUnmarshall));
    unsigned char f[16]; mk_um(f, 0x80 | 0x03, 0);
    unsigned w[2] = { 0, 0 };
    um_reset();
    nctx c; nc_rx(&c, f, w, 8, 1);
    c.desc.aUserMarshalQuadruple = g_quad;
    ULONG dst = 5; unsigned char *p = (unsigned char *)&dst;
    U(NdrUserMarshalUnmarshall)(&c.sm, &p, f, 0);
    EXPECT_EQ(um.n[2], 0);
    EXPECT_EQ(nc_used(&c), 4);
    nc_done(&c);
}
TEST(ndr_usermarshal_marshal_routine_writing_nothing_for_ref_raises_null_ref) {
    REQUIRE(U(NdrUserMarshalMarshall));
    /* a routine that returns its input buffer unchanged means "NULL object": legal only for unique types */
    static USER_MARSHAL_ROUTINE_QUADRUPLE q[1];
    q[0].pfnBufferSize = um_size; q[0].pfnMarshall = noop_m; q[0].pfnUnmarshall = um_unmarshal; q[0].pfnFree = um_free;
    unsigned char f[16]; mk_um(f, 0x03, 4);
    nctx c; nc_client(&c, f, 64);
    c.desc.aUserMarshalQuadruple = q;
    ULONG obj = 0;
    NTRY U(NdrUserMarshalMarshall)(&c.sm, (unsigned char *)&obj, f); NEND;
    EXPECT_EQ(n_code, X_NULL_REF);
    nc_done(&c);
}
TEST(ndr_usermarshal_marshal_routine_overrunning_message_raises) {
    REQUIRE(U(NdrUserMarshalMarshall));
    static USER_MARSHAL_ROUTINE_QUADRUPLE q[1];
    q[0].pfnBufferSize = um_size; q[0].pfnMarshall = big_m; q[0].pfnUnmarshall = um_unmarshal; q[0].pfnFree = um_free;
    unsigned char f[16]; mk_um(f, 0x03, 4);
    nctx c; nc_client(&c, f, 64);
    c.desc.aUserMarshalQuadruple = q;
    ULONG obj = 0;
    NTRY U(NdrUserMarshalMarshall)(&c.sm, (unsigned char *)&obj, f); NEND;
    t_note("routine returning pointer past RpcMsg buffer raised %lu", (unsigned long)n_code);
    CHECK(n_code != 0);
    nc_done(&c);
}
TEST(ndr_usermarshal_marshal_with_buffer_outside_rpcmsg_raises) {
    REQUIRE(U(NdrUserMarshalMarshall));
    unsigned char f[16]; mk_um(f, 0x03, 4);
    um_reset();
    nctx c; nc_client(&c, f, 64);
    c.desc.aUserMarshalQuadruple = g_quad;
    c.sm.Buffer = c.buf + 128;      /* beyond RpcMsg->BufferLength */
    ULONG obj = 0;
    NTRY U(NdrUserMarshalMarshall)(&c.sm, (unsigned char *)&obj, f); NEND;
    CHECK(n_code != 0);
    EXPECT_EQ(um.n[1], 0);
    nc_done(&c);
}

/* ---------------- NdrGetUserMarshalInfo (adapted from Wine) ---------------- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
static void *CALLBACK gi_alloc(SIZE_T n) { return NULL; }
static void CALLBACK gi_free(void *p) {}
TEST(ndr_getusermarshalinfo_levels_and_buffers) {
    REQUIRE(U(NdrGetUserMarshalInfo));
    unsigned char buffer[16];
    void *chan = (void *)(ULONG_PTR)0xcafebabe;
    RPC_MESSAGE rpc; MIDL_STUB_MESSAGE sm; USER_MARSHAL_CB cb; NDR_USER_MARSHAL_INFO info;
    memset(&rpc, 0xcc, sizeof rpc); rpc.Buffer = buffer; rpc.BufferLength = 16;
    memset(&sm, 0xcc, sizeof sm);
    sm.RpcMsg = &rpc; sm.dwDestContext = MSHCTX_INPROC; sm.pvDestContext = NULL;
    sm.Buffer = buffer + 15; sm.BufferLength = 0; sm.BufferEnd = NULL; sm.pRpcChannelBuffer = chan;
    sm.StubDesc = NULL; sm.pfnAllocate = gi_alloc; sm.pfnFree = gi_free;
    memset(&cb, 0xcc, sizeof cb);
    cb.Flags = MAKELONG(MSHCTX_INPROC, 0x10); cb.pStubMsg = &sm; cb.Signature = USER_MARSHAL_CB_SIGNATURE;

    static const struct { USER_MARSHAL_CB_TYPE t; int has_buf; } types[] = { { USER_MARSHAL_CB_UNMARSHALL, 1 }, { USER_MARSHAL_CB_MARSHALL, 1 }, { USER_MARSHAL_CB_BUFFER_SIZE, 0 }, { USER_MARSHAL_CB_FREE, 0 } };
    for (int i = 0; i < 4; i++) {
        cb.CBType = types[i].t;
        sm.Buffer = buffer + 15;
        memset(&info, 0xaa, sizeof info);
        RPC_STATUS st = U(NdrGetUserMarshalInfo)(&cb.Flags, 1, &info);
        CHECKF(st == RPC_S_OK, "type %d status %ld", types[i].t, (long)st);
        CHECKF(info.InformationLevel == 1, "type %d level", types[i].t);
        if (types[i].has_buf) {
            CHECKF(info.Level1.Buffer == buffer + 15, "type %d buffer", types[i].t);
            CHECKF(info.Level1.BufferSize == 1, "type %d size %lu", types[i].t, info.Level1.BufferSize);
        } else {
            CHECKF(info.Level1.Buffer == NULL && info.Level1.BufferSize == 0, "type %d: buffer zeroed", types[i].t);
        }
        CHECKF(info.Level1.pfnAllocate == gi_alloc && info.Level1.pfnFree == gi_free, "type %d callbacks", types[i].t);
        CHECKF(info.Level1.pRpcChannelBuffer == chan, "type %d channel", types[i].t);
    }
    /* boundary: Buffer exactly at the end of the message gives size 0; one byte past is an error */
    cb.CBType = USER_MARSHAL_CB_MARSHALL;
    rpc.BufferLength = 15;
    RPC_STATUS st = U(NdrGetUserMarshalInfo)(&cb.Flags, 1, &info);
    EXPECT_EQ(st, RPC_S_OK); EXPECT_EQ(info.Level1.BufferSize, 0);
    rpc.BufferLength = 14;
    st = U(NdrGetUserMarshalInfo)(&cb.Flags, 1, &info);
    EXPECT_EQ(st, 1784);    /* RPC_X_INVALID_BUFFER / ERROR_INVALID_USER_BUFFER */
    rpc.BufferLength = 15;
    st = U(NdrGetUserMarshalInfo)(&cb.Flags, 9999, &info);
    EXPECT_EQ(st, 87);
    st = U(NdrGetUserMarshalInfo)(&cb.Flags, 0, &info);
    EXPECT_EQ(st, 87);
    if (!t_is_wine()) { st = U(NdrGetUserMarshalInfo)(NULL, 1, &info); EXPECT_EQ(st, 87); }   /* wine dereferences NULL */
    cb.CBType = (USER_MARSHAL_CB_TYPE)9999;
    st = U(NdrGetUserMarshalInfo)(&cb.Flags, 1, &info);
    EXPECT_EQ(st, RPC_S_OK);
    cb.CBType = USER_MARSHAL_CB_MARSHALL; cb.Signature = 0;
    st = U(NdrGetUserMarshalInfo)(&cb.Flags, 1, &info);
    EXPECT_EQ(st, 87);
}

/* ---------------- transmit_as / represent_as ---------------- */
static struct { int n[4]; PMIDL_STUB_MESSAGE sm[4]; void *pres[4]; void *xmit[4]; short xmit_seen; } xm;
static void WINAPI xm_to(PMIDL_STUB_MESSAGE m) { xm.n[0]++; xm.pres[0] = m->pPresentedType; short *x = n_alloc(2); *x = (short)*(int *)m->pPresentedType; m->pTransmitType = (unsigned char *)x; }
static void WINAPI xm_from(PMIDL_STUB_MESSAGE m) { xm.n[1]++; xm.xmit[1] = m->pTransmitType; *(int *)m->pPresentedType = *(short *)m->pTransmitType; }
static void WINAPI xm_freexmit(PMIDL_STUB_MESSAGE m) { xm.n[2]++; xm.xmit[2] = m->pTransmitType; if (m->pTransmitType) n_free(m->pTransmitType); }
static void WINAPI xm_freeinst(PMIDL_STUB_MESSAGE m) { xm.n[3]++; xm.pres[3] = m->pPresentedType; }
static XMIT_ROUTINE_QUINTUPLE g_xq[1] = { { xm_to, xm_from, xm_freexmit, xm_freeinst } };
/* presented int (4 bytes) transmitted as a short */
static void mk_xmit(unsigned char *f, unsigned char fc, unsigned xbuf) {
    unsigned char t[] = { fc, 0x01, S16(0), S16(4), S16(xbuf), S16(2), FC_SHORT, FC_END };
    memcpy(f, t, sizeof t);
}
TEST(ndr_xmit_marshal_callback_order_and_wire) {
    REQUIRE(U(NdrXmitOrRepAsMarshall));
    for (int k = 0; k < 2; k++) {
        unsigned char f[16]; mk_xmit(f, k ? FC_REPRESENT_AS : FC_TRANSMIT_AS, 0);
        memset(&xm, 0, sizeof xm);
        nctx c; nc_client(&c, f, 32); memset(c.buf, 0, 32);
        c.desc.aXmitQuintuple = g_xq;
        int v = 0x1234;
        U(NdrXmitOrRepAsMarshall)(&c.sm, (unsigned char *)&v, f);
        CHECKF(xm.n[0] == 1 && xm.n[2] == 1 && xm.n[1] == 0 && xm.n[3] == 0, "fc %d calls %d %d %d %d", f[0], xm.n[0], xm.n[1], xm.n[2], xm.n[3]);
        CHECKF(xm.pres[0] == &v, "presented pointer");
        CHECKF(xm.xmit[2] != NULL, "FreeXmit saw the transmitted object");
        CHECKF(nc_used(&c) == 2 && *(short *)c.buf == 0x1234, "fc %d wire used %lu", f[0], nc_used(&c));
        nc_done(&c);
    }
}
TEST(ndr_xmit_buffersize_callbacks_depend_on_fixed_size) {
    REQUIRE(U(NdrXmitOrRepAsBufferSize));
    unsigned char f[16]; mk_xmit(f, FC_TRANSMIT_AS, 0);
    memset(&xm, 0, sizeof xm);
    nctx c; nc_client(&c, f, 32);
    c.desc.aXmitQuintuple = g_xq;
    int v = 5;
    c.sm.BufferLength = 1;
    U(NdrXmitOrRepAsBufferSize)(&c.sm, (unsigned char *)&v, f);
    CHECKF(xm.n[0] == 1 && xm.n[2] == 1, "dynamic: to=%d freexmit=%d", xm.n[0], xm.n[2]);
    CHECKF(c.sm.BufferLength >= 4, "dynamic BufferLength %lu", c.sm.BufferLength);
    nc_done(&c);
    mk_xmit(f, FC_TRANSMIT_AS, 2);
    memset(&xm, 0, sizeof xm);
    nc_client(&c, f, 32);
    c.desc.aXmitQuintuple = g_xq;
    c.sm.BufferLength = 1;
    U(NdrXmitOrRepAsBufferSize)(&c.sm, (unsigned char *)&v, f);
    CHECKF(xm.n[0] == 0 && xm.n[2] == 0, "fixed: no callbacks (to=%d)", xm.n[0]);
    EXPECT_EQ(c.sm.BufferLength, 4);   /* BufferLength 1 aligned with mask (flags&0xf)=1 -> 2, plus the fixed 2 bytes */
    nc_done(&c);
}
TEST(ndr_xmit_unmarshal_calls_fromxmit_and_allocates_presented_type) {
    REQUIRE(U(NdrXmitOrRepAsUnmarshall));
    unsigned char f[16]; mk_xmit(f, FC_TRANSMIT_AS, 0);
    short wire = -7;
    memset(&xm, 0, sizeof xm);
    nctx c; nc_rx(&c, f, &wire, 2, 1);
    c.desc.aXmitQuintuple = g_xq;
    unsigned char *p = NULL; n_alloc_cnt = 0;
    U(NdrXmitOrRepAsUnmarshall)(&c.sm, &p, f, 0);
    CHECK(p != NULL);
    if (p) { EXPECT_EQ(*(int *)p, -7); n_free(p); }
    EXPECT_EQ(xm.n[1], 1);
    EXPECT_EQ(nc_used(&c), 2);
    EXPECT_EQ(n_alloc_cnt, 1);
    nc_done(&c);
    int dst = 0; p = (unsigned char *)&dst;
    nc_rx(&c, f, &wire, 2, 1);
    c.desc.aXmitQuintuple = g_xq;
    n_alloc_cnt = 0;
    U(NdrXmitOrRepAsUnmarshall)(&c.sm, &p, f, 0);
    CHECK(p == (unsigned char *)&dst && dst == -7 && n_alloc_cnt == 0);
    nc_done(&c);
}
TEST(ndr_xmit_unmarshal_short_buffer_raises) {
    REQUIRE(U(NdrXmitOrRepAsUnmarshall));
    unsigned char f[16]; mk_xmit(f, FC_TRANSMIT_AS, 0);
    short wire = 1;
    nctx c; nc_rx(&c, f, &wire, 1, 1);
    c.desc.aXmitQuintuple = g_xq;
    int dst; unsigned char *p = (unsigned char *)&dst;
    memset(&xm, 0, sizeof xm);
    NTRY U(NdrXmitOrRepAsUnmarshall)(&c.sm, &p, f, 0); NEND;
    EXPECT_EQ(n_code, X_BAD_STUB);
    EXPECT_EQ(xm.n[1], 0);
    nc_done(&c);
}
TEST(ndr_xmit_memorysize_and_free) {
    REQUIRE(U(NdrXmitOrRepAsMemorySize)); REQUIRE(U(NdrXmitOrRepAsFree));
    unsigned char f[16]; mk_xmit(f, FC_TRANSMIT_AS, 0);
    short wire = 3;
    nctx c; nc_rx(&c, f, &wire, 2, 1);
    c.desc.aXmitQuintuple = g_xq;
    c.sm.MemorySize = 10;
    ULONG r = U(NdrXmitOrRepAsMemorySize)(&c.sm, f);
    EXPECT_EQ(r, 4);                 /* returns the presented size, MemorySize += 4 */
    EXPECT_EQ(c.sm.MemorySize, 14);
    EXPECT_EQ(nc_used(&c), 2);
    nc_done(&c);
    nc_client(&c, f, 16);
    c.desc.aXmitQuintuple = g_xq;
    memset(&xm, 0, sizeof xm);
    int v = 1;
    c.sm.fDontCallFreeInst = 0;
    U(NdrXmitOrRepAsFree)(&c.sm, (unsigned char *)&v, f);
    EXPECT_EQ(xm.n[3], 1);
    EXPECT_EQ(xm.pres[3], &v);
    c.sm.fDontCallFreeInst = 1;
    U(NdrXmitOrRepAsFree)(&c.sm, (unsigned char *)&v, f);
    EXPECT_EQ(xm.n[3], 1);           /* suppressed by fDontCallFreeInst */
    nc_done(&c);
}

/* ---------------- NdrRangeUnmarshall ---------------- */
TEST(ndr_range_unmarshal_long_bounds) {
    REQUIRE(U(NdrRangeUnmarshall));
    static const unsigned char f[] = { FC_RANGE, FC_LONG, W32(10), W32(20) };
    static const struct { int v; DWORD code; } t[] = { { 10, 0 }, { 15, 0 }, { 20, 0 }, { 9, X_INVALID_BOUND }, { 21, X_INVALID_BOUND }, { -1, X_INVALID_BOUND }, { 0x7fffffff, X_INVALID_BOUND } };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        nctx c; nc_rx(&c, f, &t[k].v, 4, 1);
        int dst = 0; unsigned char *p = (unsigned char *)&dst;
        NTRY U(NdrRangeUnmarshall)(&c.sm, &p, f, 0); NEND;
        CHECKF(n_code == t[k].code, "value %d raised %lu want %lu", t[k].v, (unsigned long)n_code, (unsigned long)t[k].code);
        CHECKF(nc_used(&c) == 4, "value %d consumed %lu", t[k].v, nc_used(&c));
        nc_done(&c);
    }
}
TEST(ndr_range_unmarshal_signed_and_unsigned_compare) {
    REQUIRE(U(NdrRangeUnmarshall));
    static const unsigned char fs[] = { FC_RANGE, FC_LONG, W32((unsigned)-5), W32(5) };
    static const unsigned char fu[] = { FC_RANGE, FC_ULONG, W32(0x80000000u), W32(0xFFFFFFF0u) };
    int sv[] = { -5, 0, 5, -6, 6 }; DWORD sc[] = { 0, 0, 0, X_INVALID_BOUND, X_INVALID_BOUND };
    for (int k = 0; k < 5; k++) {
        nctx c; nc_rx(&c, fs, &sv[k], 4, 1);
        int dst; unsigned char *p = (unsigned char *)&dst;
        NTRY U(NdrRangeUnmarshall)(&c.sm, &p, fs, 0); NEND;
        CHECKF(n_code == sc[k], "signed %d raised %lu", sv[k], (unsigned long)n_code);
        nc_done(&c);
    }
    unsigned uv[] = { 0x80000000u, 0xFFFFFFF0u, 0x7fffffffu, 0xFFFFFFF1u }; DWORD uc[] = { 0, 0, X_INVALID_BOUND, X_INVALID_BOUND };
    for (int k = 0; k < 4; k++) {
        nctx c; nc_rx(&c, fu, &uv[k], 4, 1);
        unsigned dst; unsigned char *p = (unsigned char *)&dst;
        NTRY U(NdrRangeUnmarshall)(&c.sm, &p, fu, 0); NEND;
        CHECKF(n_code == uc[k], "unsigned 0x%x raised %lu", uv[k], (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_range_unmarshal_short_and_byte_types) {
    REQUIRE(U(NdrRangeUnmarshall));
    static const unsigned char fsh[] = { FC_RANGE, FC_SHORT, W32((unsigned)-3), W32(3) };
    short v[] = { -3, 3, -4, 4 }; DWORD c4[] = { 0, 0, X_INVALID_BOUND, X_INVALID_BOUND };
    for (int k = 0; k < 4; k++) {
        nctx c; nc_rx(&c, fsh, &v[k], 2, 1);
        short dst; unsigned char *p = (unsigned char *)&dst;
        NTRY U(NdrRangeUnmarshall)(&c.sm, &p, fsh, 0); NEND;
        CHECKF(n_code == c4[k], "short %d raised %lu", v[k], (unsigned long)n_code);
        CHECKF(nc_used(&c) == 2, "short consumed");
        nc_done(&c);
    }
    static const unsigned char fby[] = { FC_RANGE, FC_USMALL, W32(1), W32(9) };
    unsigned char bv[] = { 1, 9, 0, 10 };
    for (int k = 0; k < 4; k++) {
        nctx c; nc_rx(&c, fby, &bv[k], 1, 1);
        unsigned char dst; unsigned char *p = &dst;
        NTRY U(NdrRangeUnmarshall)(&c.sm, &p, fby, 0); NEND;
        CHECKF(n_code == c4[k], "usmall %u raised %lu", bv[k], (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_range_unmarshal_truncated_raises_bad_stub) {
    REQUIRE(U(NdrRangeUnmarshall));
    static const unsigned char f[] = { FC_RANGE, FC_LONG, W32(0), W32(100) };
    unsigned char w[4] = { 5, 0, 0, 0 };
    for (unsigned len = 0; len < 4; len++) {
        nctx c; nc_rx(&c, f, w, len, 1);
        int dst; unsigned char *p = (unsigned char *)&dst;
        NTRY U(NdrRangeUnmarshall)(&c.sm, &p, f, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}
