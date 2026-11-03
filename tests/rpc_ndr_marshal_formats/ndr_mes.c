/* Serialization / pickling (midl_es): handle creation, reset, simple type and type pickles, incremental handles. */
#include "ndrh.h"

#define ES_ENCODE 0   /* MES_ENCODE */
#define ES_DECODE 1   /* MES_DECODE */
#define ES_INCR   0   /* MES_INCREMENTAL_HANDLE */
#define ES_FIXED  1   /* MES_FIXED_BUFFER_HANDLE */
#define ES_DYN    2   /* MES_DYNAMIC_BUFFER_HANDLE */

/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
TEST(ndr_mes_fixed_handle_create_argument_validation) {
    REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(MesHandleFree));
    ULONG enc; handle_t h = NULL;
    char *aligned = (char *)(((ULONG_PTR)0xdeadbeef + 7) & ~(ULONG_PTR)7);
    RPC_STATUS st;
    st = U(MesEncodeFixedBufferHandleCreate)(NULL, 0, NULL, NULL);                 EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesEncodeFixedBufferHandleCreate)(NULL, 0, NULL, &h);                   EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesEncodeFixedBufferHandleCreate)((char *)(ULONG_PTR)0xdeadbeef, 0, NULL, &h); EXPECT_EQ(st, RPC_X_INVALID_BUFFER);  /* not 8-byte aligned */
    st = U(MesEncodeFixedBufferHandleCreate)(aligned, 0, NULL, &h);                EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesEncodeFixedBufferHandleCreate)(aligned, 0, &enc, &h);                EXPECT_EQ(st, RPC_S_INVALID_ARG);          /* zero sized buffer */
    st = U(MesEncodeFixedBufferHandleCreate)(aligned, 32, NULL, &h);               EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesEncodeFixedBufferHandleCreate)(aligned, 32, &enc, &h);               EXPECT_EQ(st, RPC_S_OK);
    CHECK(h != NULL);
    if (st == RPC_S_OK) {
        st = U(MesHandleFree)(h); EXPECT_EQ(st, RPC_S_OK);
    }
}
TEST(ndr_mes_buffer_handle_reset_argument_validation) {
    REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(MesBufferHandleReset)); REQUIRE(U(MesHandleFree));
    ULONG enc; handle_t h = NULL;
    char *buffer = (char *)(((ULONG_PTR)0xdeadbeef + 7) & ~(ULONG_PTR)7);
    RPC_STATUS st = U(MesEncodeFixedBufferHandleCreate)(buffer, 32, &enc, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    st = U(MesBufferHandleReset)(NULL, ES_DYN, ES_ENCODE, &buffer, 32, &enc);   EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesBufferHandleReset)(h, ES_DYN, ES_ENCODE, &buffer, 32, &enc);      EXPECT_EQ(st, RPC_S_OK);   /* convert to a dynamic buffer handle */
    st = U(MesBufferHandleReset)(h, ES_DYN, ES_ENCODE, NULL, 32, &enc);         EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesBufferHandleReset)(h, ES_DYN, ES_ENCODE, &buffer, 32, NULL);      EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesBufferHandleReset)(h, ES_DYN + 1, ES_ENCODE, &buffer, 32, &enc);  EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesHandleFree)(h);                                                  EXPECT_EQ(st, RPC_S_OK);
}
TEST(ndr_mes_dyn_handle_create_argument_validation) {
    REQUIRE(U(MesEncodeDynBufferHandleCreate)); REQUIRE(U(MesHandleFree));
    char *buf = NULL; ULONG enc = 0; handle_t h = NULL;
    RPC_STATUS st;
    st = U(MesEncodeDynBufferHandleCreate)(NULL, &enc, &h);   EXPECT_EQ(st, RPC_S_INVALID_ARG);   /* validated by MesBufferHandleReset */
    CHECK(h == NULL);
    st = U(MesEncodeDynBufferHandleCreate)(&buf, NULL, &h);   EXPECT_EQ(st, RPC_S_INVALID_ARG);
    CHECK(h == NULL);
    st = U(MesEncodeDynBufferHandleCreate)(&buf, &enc, &h);   EXPECT_EQ(st, RPC_S_OK);
    CHECK(h != NULL);
    if (st == RPC_S_OK) { EXPECT_EQ(U(MesHandleFree)(h), RPC_S_OK); }
}
TEST(ndr_mes_handle_free_null_is_ok) {
    REQUIRE(U(MesHandleFree));
    EXPECT_EQ(U(MesHandleFree)(NULL), RPC_S_OK);
}
TEST(ndr_mes_decode_handle_create) {
    REQUIRE(U(MesDecodeBufferHandleCreate)); REQUIRE(U(MesHandleFree));
    __attribute__((aligned(8))) static char buf[64];
    handle_t h = NULL;
    RPC_STATUS st = U(MesDecodeBufferHandleCreate)(buf, 64, &h);
    EXPECT_EQ(st, RPC_S_OK);
    CHECK(h != NULL);
    if (st == RPC_S_OK) { EXPECT_EQ(U(MesHandleFree)(h), RPC_S_OK); }
    h = NULL;
    st = U(MesDecodeBufferHandleCreate)(buf, 16, &h);    EXPECT_EQ(st, RPC_S_OK);       /* 16 bytes is the minimum */
    if (st == RPC_S_OK) U(MesHandleFree)(h);
    h = NULL;
    st = U(MesDecodeBufferHandleCreate)(buf, 15, &h);    EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesDecodeBufferHandleCreate)(buf, 0, &h);     EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesDecodeBufferHandleCreate)(NULL, 64, &h);   EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesDecodeBufferHandleCreate)(buf, 64, NULL);  EXPECT_EQ(st, RPC_S_INVALID_ARG);
    st = U(MesDecodeBufferHandleCreate)(buf + 1, 63, &h);
    t_note("MesDecodeBufferHandleCreate(misaligned) -> %ld (1784 = RPC_X_INVALID_BUFFER)", (long)st);
    CHECK(st != RPC_S_OK);
}

/* ---------------- simple type pickles ---------------- */
static const unsigned char pk_hdr[8] = { 0x01, 0x10, 0x08, 0x00, 0xCC, 0xCC, 0xCC, 0xCC };
TEST(ndr_mes_simpletype_encode_fixed_buffer_layout) {
    WSTUB("NdrMesSimpleTypeEncode");
    REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(NdrMesSimpleTypeAlignSize)); REQUIRE(U(MesHandleFree));
    static const struct { short size; unsigned long long v; } t[] = { { 1, 0xAB }, { 2, 0xBEEF }, { 4, 0xCAFEBABE }, { 8, 0x0123456789ABCDEFull } };
    for (unsigned k = 0; k < 4; k++) {
        __attribute__((aligned(8))) unsigned char buf[64]; memset(buf, 0x77, sizeof buf);
        ULONG enc = 0; handle_t h = NULL;
        RPC_STATUS st = U(MesEncodeFixedBufferHandleCreate)((char *)buf, sizeof buf, &enc, &h);
        if (st != RPC_S_OK) { CHECKF(0, "create %ld", (long)st); return; }
        MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
        size_t as = U(NdrMesSimpleTypeAlignSize)(h);
        CHECKF(as == 16, "size %d: AlignSize %zu", t[k].size, as);
        unsigned long long v = t[k].v;
        NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v, t[k].size); NEND;
        CHECKF(n_code == 0, "size %d raised %lu", t[k].size, (unsigned long)n_code);
        CHECKF(enc == 16, "size %d: encoded size %lu", t[k].size, enc);
        CHECKF(!memcmp(buf, pk_hdr, 8), "size %d: common header", t[k].size);
        unsigned long long body = 0; memcpy(&body, buf + 8, 8);
        unsigned long long mask = t[k].size == 8 ? ~0ull : ((1ull << (8 * t[k].size)) - 1);
        CHECKF(body == (t[k].v & mask), "size %d: body %llx", t[k].size, body);
        CHECKF(buf[16] == 0x77, "size %d: wrote past the pickle", t[k].size);
        U(MesHandleFree)(h);
    }
}
TEST(ndr_mes_simpletype_decode_roundtrip_and_header_validation) {
    WSTUB("NdrMesSimpleTypeDecode");
    REQUIRE(U(MesDecodeBufferHandleCreate)); REQUIRE(U(NdrMesSimpleTypeDecode)); REQUIRE(U(MesHandleFree));
    __attribute__((aligned(8))) unsigned char buf[16];
    memcpy(buf, pk_hdr, 8); memset(buf + 8, 0, 8); *(unsigned *)(buf + 8) = 0xFEEDF00D;
    handle_t h = NULL;
    RPC_STATUS st = U(MesDecodeBufferHandleCreate)((char *)buf, 16, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    unsigned out = 0;
    NTRY U(NdrMesSimpleTypeDecode)(h, &out, 4); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(out, 0xFEEDF00D);
    U(MesHandleFree)(h);
    /* wrong version byte */
    buf[0] = 2;
    st = U(MesDecodeBufferHandleCreate)((char *)buf, 16, &h);
    EXPECT_EQ(st, RPC_S_OK);
    NTRY U(NdrMesSimpleTypeDecode)(h, &out, 4); NEND;
    EXPECT_EQ(n_code, 1828);     /* RPC_X_WRONG_ES_VERSION */
    U(MesHandleFree)(h);
    buf[0] = 1;
    /* buffer too small for the common header */
    st = U(MesDecodeBufferHandleCreate)((char *)buf, 8, &h);
    if (st == RPC_S_OK) {
        NTRY U(NdrMesSimpleTypeDecode)(h, &out, 4); NEND;
        t_note("decode with an 8 byte buffer raised %lu", (unsigned long)n_code);
        CHECK(n_code != 0);
        U(MesHandleFree)(h);
    }
}
TEST(ndr_mes_simpletype_wrong_direction_raises_invalid_es_action) {
    WSTUB("NdrMesSimpleType*");
    REQUIRE(U(MesDecodeBufferHandleCreate)); REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(NdrMesSimpleTypeDecode)); REQUIRE(U(NdrMesSimpleTypeAlignSize)); REQUIRE(U(MesHandleFree));
    __attribute__((aligned(8))) unsigned char buf[32]; memcpy(buf, pk_hdr, 8); memset(buf + 8, 0, 24);
    handle_t hd = NULL, he = NULL; ULONG enc;
    U(MesDecodeBufferHandleCreate)((char *)buf, 32, &hd);
    U(MesEncodeFixedBufferHandleCreate)((char *)buf, 32, &enc, &he);
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    unsigned v = 1;
    NTRY U(NdrMesSimpleTypeEncode)(hd, &d, &v, 4); NEND;
    EXPECT_EQ(n_code, X_INVALID_ES);
    NTRY U(NdrMesSimpleTypeAlignSize)(hd); NEND;
    EXPECT_EQ(n_code, X_INVALID_ES);
    NTRY U(NdrMesSimpleTypeDecode)(he, &v, 4); NEND;
    EXPECT_EQ(n_code, X_INVALID_ES);
    if (hd) U(MesHandleFree)(hd);
    if (he) U(MesHandleFree)(he);
}
TEST(ndr_mes_simpletype_invalid_handle_raises_invalid_arg) {
    WSTUB("NdrMesSimpleType*");
    REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(NdrMesSimpleTypeDecode)); REQUIRE(U(NdrMesSimpleTypeAlignSize));
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    unsigned v = 1;
    NTRY U(NdrMesSimpleTypeEncode)(NULL, &d, &v, 4); NEND;
    EXPECT_EQ(n_code, ERROR_INVALID_PARAMETER);
    NTRY U(NdrMesSimpleTypeDecode)(NULL, &v, 4); NEND;
    EXPECT_EQ(n_code, ERROR_INVALID_PARAMETER);
    NTRY U(NdrMesSimpleTypeAlignSize)(NULL); NEND;
    EXPECT_EQ(n_code, ERROR_INVALID_PARAMETER);
    unsigned char garbage[0x300] = { 0 };       /* readable memory that is not a serialization handle */
    NTRY U(NdrMesSimpleTypeAlignSize)((handle_t)garbage); NEND;
    EXPECT_EQ(n_code, ERROR_INVALID_PARAMETER);
}
TEST(ndr_mes_encode_into_too_small_fixed_buffer_raises) {
    WSTUB("NdrMesSimpleTypeEncode");
    REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(MesHandleFree));
    __attribute__((aligned(8))) unsigned char buf[32]; memset(buf, 0x77, sizeof buf);
    ULONG enc; handle_t h = NULL;
    RPC_STATUS st = U(MesEncodeFixedBufferHandleCreate)((char *)buf, 8, &enc, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    unsigned v = 1;
    NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v, 4); NEND;
    t_note("16 byte pickle into an 8 byte fixed buffer raised %lu", (unsigned long)n_code);
    CHECK(n_code != 0);
    CHECK(buf[8] == 0x77);       /* never writes beyond the declared size */
    U(MesHandleFree)(h);
}
TEST(ndr_mes_dyn_buffer_encode_allocates_through_stub_desc) {
    WSTUB("NdrMesSimpleTypeEncode");
    REQUIRE(U(MesEncodeDynBufferHandleCreate)); REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(MesHandleFree));
    char *out = NULL; ULONG enc = 0; handle_t h = NULL;
    RPC_STATUS st = U(MesEncodeDynBufferHandleCreate)(&out, &enc, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    unsigned v = 0x31323334;
    n_alloc_cnt = 0;
    NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v, 4); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(n_alloc_cnt, 1);
    EXPECT_EQ(enc, 16);
    CHECK(out != NULL);
    if (out) {
        CHECK(((ULONG_PTR)out & 7) == 0);
        CHECK(!memcmp(out, pk_hdr, 8));
        CHECK(*(unsigned *)(out + 8) == 0x31323334);
        n_free(out);
    }
    U(MesHandleFree)(h);
}
TEST(ndr_mes_two_pickles_in_one_fixed_buffer_are_16_byte_chunks) {
    WSTUB("NdrMesSimpleTypeEncode");
    REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(MesHandleFree)); REQUIRE(U(MesBufferHandleReset));
    __attribute__((aligned(8))) unsigned char buf[64]; memset(buf, 0x77, sizeof buf);
    ULONG enc = 0; handle_t h = NULL;
    RPC_STATUS st = U(MesEncodeFixedBufferHandleCreate)((char *)buf, sizeof buf, &enc, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    unsigned a = 1, b = 2;
    U(NdrMesSimpleTypeEncode)(h, &d, &a, 4);
    U(NdrMesSimpleTypeEncode)(h, &d, &b, 4);
    t_note("two simple pickles: encoded size %lu", enc);
    CHECK(enc == 24 || enc == 32);   /* second pickle either omits (24) or repeats (32) the common header */
    CHECK(*(unsigned *)(buf + 8) == 1);
    U(MesHandleFree)(h);
}

/* ---------------- type pickles through NdrMesType*2 ---------------- */
static const unsigned char f_pk_s3[] = { FC_STRUCT, 1, S16(4), FC_SHORT, FC_SHORT, FC_END };
TEST(ndr_mes_type_encode2_struct_layout_and_roundtrip) {
    WSTUB("NdrMesType*2");
    REQUIRE(U(MesEncodeFixedBufferHandleCreate)); REQUIRE(U(MesDecodeBufferHandleCreate)); REQUIRE(U(NdrMesTypeAlignSize2)); REQUIRE(U(NdrMesTypeEncode2));
    REQUIRE(U(NdrMesTypeDecode2)); REQUIRE(U(MesHandleFree));
    __attribute__((aligned(8))) unsigned char buf[128]; memset(buf, 0x77, sizeof buf);
    MIDL_STUB_DESC d; n_fill_desc(&d, f_pk_s3);
    MIDL_TYPE_PICKLING_INFO pi; memset(&pi, 0, sizeof pi); pi.Version = 0x33205054; pi.Flags = 0;
    struct { short a, b; } v = { 0x1234, -2 };
    ULONG enc = 0; handle_t h = NULL;
    RPC_STATUS st = U(MesEncodeFixedBufferHandleCreate)((char *)buf, sizeof buf, &enc, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    size_t as = U(NdrMesTypeAlignSize2)(h, &pi, &d, f_pk_s3, &v);
    t_note("NdrMesTypeAlignSize2 for a 4 byte struct -> %zu", as);
    CHECK(as >= 16 && (as & 7) == 0);
    NTRY U(NdrMesTypeEncode2)(h, &pi, &d, f_pk_s3, &v); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(enc == as);
    CHECK(!memcmp(buf, pk_hdr, 8));
    t_note("private header: %08x %08x; body: %08x", ((unsigned *)buf)[2], ((unsigned *)buf)[3], ((unsigned *)buf)[4]);
    CHECK(((unsigned *)buf)[2] == 8 || ((unsigned *)buf)[2] == 4);   /* object length in bytes, padded or not */
    U(MesHandleFree)(h);
    h = NULL;
    st = U(MesDecodeBufferHandleCreate)((char *)buf, enc, &h);
    EXPECT_EQ(st, RPC_S_OK);
    struct { short a, b; } out = { 0, 0 };
    NTRY U(NdrMesTypeDecode2)(h, &pi, &d, f_pk_s3, &out); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(out.a, 0x1234); EXPECT_EQ(out.b, -2);
    U(MesHandleFree)(h);
}

/* ---------------- incremental handles ---------------- */
static struct { int allocs, writes, reads; ULONG want_alloc; ULONG wrote; unsigned char store[256]; ULONG rpos; } inc;
static void __RPC_API inc_alloc(void *state, char **pbuf, unsigned int *psize) { inc.allocs++; inc.want_alloc = *psize; static __attribute__((aligned(8))) char blk[256]; *pbuf = blk; *psize = sizeof blk; }
static void __RPC_API inc_write(void *state, char *buf, unsigned int size) { inc.writes++; if (inc.wrote + size <= sizeof inc.store) memcpy(inc.store + inc.wrote, buf, size); inc.wrote += size; }
static void __RPC_API inc_read(void *state, char **pbuf, unsigned int *psize) {
    inc.reads++;
    ULONG n = *psize; if (inc.rpos + n > inc.wrote) n = inc.wrote - inc.rpos;
    *pbuf = (char *)inc.store + inc.rpos; *psize = n; inc.rpos += n;
}
TEST(ndr_mes_incremental_encode_decode_simple_type) {
    WSTUB("NdrMesSimpleType*");
    REQUIRE(U(MesEncodeIncrementalHandleCreate)); REQUIRE(U(MesDecodeIncrementalHandleCreate)); REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(NdrMesSimpleTypeDecode)); REQUIRE(U(MesHandleFree));
    memset(&inc, 0, sizeof inc);
    handle_t h = NULL; MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    RPC_STATUS st = U(MesEncodeIncrementalHandleCreate)(&inc, inc_alloc, inc_write, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    unsigned v = 0x0BADCAFE;
    NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v, 4); NEND;
    EXPECT_EQ(n_code, 0);
    t_note("incremental encode: alloc calls %d (asked %lu), write calls %d, bytes %lu", inc.allocs, inc.want_alloc, inc.writes, inc.wrote);
    CHECK(inc.allocs >= 1 && inc.writes >= 1);
    EXPECT_EQ(inc.wrote, 16);
    CHECK(!memcmp(inc.store, pk_hdr, 8));
    CHECK(*(unsigned *)(inc.store + 8) == 0x0BADCAFE);
    U(MesHandleFree)(h);
    h = NULL;
    st = U(MesDecodeIncrementalHandleCreate)(&inc, inc_read, &h);
    EXPECT_EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    unsigned out = 0;
    NTRY U(NdrMesSimpleTypeDecode)(h, &out, 4); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(out, 0x0BADCAFE);
    CHECK(inc.reads >= 1);
    EXPECT_EQ(inc.rpos, 16);
    U(MesHandleFree)(h);
}
TEST(ndr_mes_incremental_handles_create_and_free) {
    REQUIRE(U(MesEncodeIncrementalHandleCreate)); REQUIRE(U(MesDecodeIncrementalHandleCreate)); REQUIRE(U(MesHandleFree));
    handle_t h = NULL;
    RPC_STATUS st = U(MesEncodeIncrementalHandleCreate)(&inc, inc_alloc, inc_write, &h);
    EXPECT_EQ(st, RPC_S_OK); CHECK(h != NULL);
    if (st == RPC_S_OK) EXPECT_EQ(U(MesHandleFree)(h), RPC_S_OK);
    h = NULL;
    st = U(MesDecodeIncrementalHandleCreate)(&inc, inc_read, &h);
    EXPECT_EQ(st, RPC_S_OK); CHECK(h != NULL);
    if (st == RPC_S_OK) EXPECT_EQ(U(MesHandleFree)(h), RPC_S_OK);
}
