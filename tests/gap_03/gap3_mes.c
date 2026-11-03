/* gap_03: MesIncrementalHandleReset (midl_es incremental serialization handle re-arming).
 * Behaviour is taken from the Win10 rpcrt4 decomp:
 *   - NdrpValidateMesHandleReturnStatus(handle) must pass (NULL handle -> ERROR_INVALID_PARAMETER 87)
 *   - Operation >= 4 -> 87;  encode ops (0, 2) need an Alloc and a Write routine, decode ops (1, 3) need a Read routine
 *   - a NULL routine/state argument keeps the value stored in the handle, a non-NULL one replaces it */
#include "g3ndr.h"

enum { OP_ENCODE = 0, OP_DECODE = 1, OP_ENCODE64 = 2, OP_DECODE64 = 3 };

typedef struct { int tag, allocs, writes, reads; ULONG wrote, rpos; unsigned char store[256]; } Ust;
static Ust u1, u2;
static void __RPC_API g_alloc(void *st, char **pbuf, unsigned int *psize) {
    static __attribute__((aligned(8))) char blk[256];
    ((Ust *)st)->allocs++; *pbuf = blk; *psize = sizeof blk;
}
static void __RPC_API g_write(void *st, char *buf, unsigned int size) {
    Ust *u = st; u->writes++;
    if (u->wrote + size <= sizeof u->store) memcpy(u->store + u->wrote, buf, size);
    u->wrote += size;
}
static void __RPC_API g_read(void *st, char **pbuf, unsigned int *psize) {
    Ust *u = st; u->reads++;
    ULONG n = *psize; if (u->rpos + n > u->wrote) n = u->wrote - u->rpos;
    *pbuf = (char *)u->store + u->rpos; *psize = n; u->rpos += n;
}
static void __RPC_API g_alloc2(void *st, char **pbuf, unsigned int *psize) { g_alloc(st, pbuf, psize); }

#define RESET(h, st, a, w, r, op) U(MesIncrementalHandleReset)(h, st, a, w, r, op)

static handle_t mk_enc(void *st) {
    handle_t h = NULL;
    if (U(MesEncodeIncrementalHandleCreate)(st, g_alloc, g_write, &h) != RPC_S_OK) return NULL;
    return h;
}
static handle_t mk_dec(void *st) {
    handle_t h = NULL;
    if (U(MesDecodeIncrementalHandleCreate)(st, g_read, &h) != RPC_S_OK) return NULL;
    return h;
}
#define NEED() do { if (t_is_wine()) { t_skip("wine: MesIncrementalHandleReset is an unimplemented stub"); return; } REQUIRE(U(MesIncrementalHandleReset)); REQUIRE(U(MesEncodeIncrementalHandleCreate)); REQUIRE(U(MesDecodeIncrementalHandleCreate)); REQUIRE(U(MesHandleFree)); } while (0)

TEST(gap3_mes_incr_reset_null_handle) {
    REQUIRE(U(MesIncrementalHandleReset));
    if (t_is_wine()) { t_skip("wine: stub"); return; }
    EXPECT_EQ(RESET(NULL, &u1, g_alloc, g_write, g_read, OP_ENCODE), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(NULL, NULL, NULL, NULL, NULL, OP_DECODE), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(NULL, &u1, g_alloc, g_write, g_read, 99), ERROR_INVALID_PARAMETER);
}
TEST(gap3_mes_incr_reset_encode_handle_keeps_callbacks_when_null) {
    NEED();
    handle_t h = mk_enc(&u1); CHECK(h != NULL); if (!h) return;
    /* all-NULL arguments retain the routines stored by MesEncodeIncrementalHandleCreate */
    EXPECT_EQ(RESET(h, NULL, NULL, NULL, NULL, OP_ENCODE), RPC_S_OK);
    EXPECT_EQ(RESET(h, NULL, NULL, NULL, NULL, OP_ENCODE64), RPC_S_OK);
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_operation_range) {
    NEED();
    handle_t h = mk_enc(&u1); CHECK(h != NULL); if (!h) return;
    for (unsigned op = 4; op < 12; op++) {
        RPC_STATUS st = RESET(h, &u1, g_alloc, g_write, g_read, op);
        CHECKF(st == ERROR_INVALID_PARAMETER, "operation %u -> %lu", op, (unsigned long)st);
    }
    EXPECT_EQ(RESET(h, &u1, g_alloc, g_write, g_read, 0xffffffffu), ERROR_INVALID_PARAMETER);
    /* the handle is still usable afterwards */
    EXPECT_EQ(RESET(h, &u1, g_alloc, g_write, g_read, OP_ENCODE), RPC_S_OK);
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_all_four_operations_with_full_callbacks) {
    NEED();
    handle_t h = mk_enc(&u1); CHECK(h != NULL); if (!h) return;
    for (unsigned op = 0; op < 4; op++) {
        RPC_STATUS st = RESET(h, &u1, g_alloc, g_write, g_read, op);
        CHECKF(st == RPC_S_OK, "operation %u -> %lu", op, (unsigned long)st);
    }
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_decode_requires_read_routine) {
    NEED();
    /* a handle created for encoding has no Read routine yet */
    handle_t h = mk_enc(&u1); CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(RESET(h, &u1, NULL, NULL, NULL, OP_DECODE), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(h, &u1, NULL, NULL, NULL, OP_DECODE64), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(h, &u1, g_alloc, g_write, NULL, OP_DECODE), ERROR_INVALID_PARAMETER);
    /* supplying it makes the same handle a decoder; the routine is then remembered */
    EXPECT_EQ(RESET(h, &u1, NULL, NULL, g_read, OP_DECODE), RPC_S_OK);
    EXPECT_EQ(RESET(h, NULL, NULL, NULL, NULL, OP_DECODE), RPC_S_OK);
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_encode_requires_alloc_and_write) {
    NEED();
    handle_t h = mk_dec(&u1); CHECK(h != NULL); if (!h) return;
    /* a decode handle has a Read routine only */
    EXPECT_EQ(RESET(h, &u1, NULL, NULL, NULL, OP_ENCODE), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(h, &u1, g_alloc, NULL, NULL, OP_ENCODE), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(h, &u1, NULL, g_write, NULL, OP_ENCODE), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(h, &u1, NULL, g_write, NULL, OP_ENCODE64), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(RESET(h, &u1, g_alloc, g_write, NULL, OP_ENCODE), RPC_S_OK);
    /* and the Alloc/Write routines stick for the next reset */
    EXPECT_EQ(RESET(h, NULL, NULL, NULL, NULL, OP_ENCODE64), RPC_S_OK);
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_switches_user_state_and_routines) {
    NEED();
    REQUIRE(U(NdrMesSimpleTypeEncode)); REQUIRE(U(NdrMesSimpleTypeDecode));
    memset(&u1, 0, sizeof u1); memset(&u2, 0, sizeof u2);
    handle_t h = mk_enc(&u1); CHECK(h != NULL); if (!h) return;
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    unsigned v = 0x13572468;
    NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v, 4); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(u1.allocs >= 1 && u1.writes >= 1);
    ULONG first = u1.wrote;
    EXPECT_EQ(first, 16);
    CHECK(u2.allocs == 0 && u2.writes == 0);
    /* re-arm for encoding with a different state and a different alloc routine: only the new state is used */
    EXPECT_EQ(RESET(h, &u2, g_alloc2, g_write, g_read, OP_ENCODE), RPC_S_OK);
    unsigned v2 = 0xCAFEF00D;
    NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v2, 4); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(u2.allocs >= 1 && u2.writes >= 1);
    EXPECT_EQ(u1.wrote, first);
    EXPECT_EQ(u2.wrote, 16);
    CHECK(*(unsigned *)(u2.store + 8) == 0xCAFEF00D);
    /* turn the same handle into a decoder over u2's bytes */
    EXPECT_EQ(RESET(h, &u2, NULL, NULL, g_read, OP_DECODE), RPC_S_OK);
    unsigned out = 0;
    NTRY U(NdrMesSimpleTypeDecode)(h, &out, 4); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(out, 0xCAFEF00D);
    CHECK(u2.reads >= 1);
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_repeated_reset_restarts_stream) {
    NEED();
    REQUIRE(U(NdrMesSimpleTypeEncode));
    memset(&u1, 0, sizeof u1);
    handle_t h = mk_enc(&u1); CHECK(h != NULL); if (!h) return;
    MIDL_STUB_DESC d; n_fill_desc(&d, NULL);
    for (int i = 0; i < 3; i++) {
        unsigned v = 0x1000 + i;
        u1.wrote = 0;
        EXPECT_EQ(RESET(h, &u1, NULL, NULL, NULL, OP_ENCODE), RPC_S_OK);
        NTRY U(NdrMesSimpleTypeEncode)(h, &d, &v, 4); NEND;
        CHECKF(n_code == 0 && u1.wrote == 16 && *(unsigned *)(u1.store + 8) == v, "iteration %d: exc %lu wrote %lu", i, (unsigned long)n_code, u1.wrote);
    }
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_on_fixed_buffer_handle) {
    NEED();
    REQUIRE(U(MesEncodeFixedBufferHandleCreate));
    __attribute__((aligned(8))) static char buf[64];
    ULONG enc = 0; handle_t h = NULL;
    if (U(MesEncodeFixedBufferHandleCreate)(buf, sizeof buf, &enc, &h) != RPC_S_OK) { CHECK(0); return; }
    /* decomp: a handle whose byte at +0x1a1 is set (not an incremental style handle) is refused with 87; whether a fixed handle
     * sets it is an implementation detail: record it, but a refusal must be exactly ERROR_INVALID_PARAMETER */
    RPC_STATUS st = RESET(h, &u1, g_alloc, g_write, g_read, OP_ENCODE);
    t_note("MesIncrementalHandleReset on a fixed-buffer handle -> %lu", (unsigned long)st);
    CHECKF(st == RPC_S_OK || st == ERROR_INVALID_PARAMETER, "status %lu", (unsigned long)st);
    U(MesHandleFree)(h);
}
TEST(gap3_mes_incr_reset_on_dynamic_buffer_handle) {
    NEED();
    REQUIRE(U(MesEncodeDynBufferHandleCreate));
    char *dbuf = NULL; ULONG enc = 0; handle_t h = NULL;
    if (U(MesEncodeDynBufferHandleCreate)(&dbuf, &enc, &h) != RPC_S_OK) { CHECK(0); return; }
    RPC_STATUS st = RESET(h, &u1, g_alloc, g_write, g_read, OP_ENCODE);
    t_note("MesIncrementalHandleReset on a dynamic-buffer handle -> %lu", (unsigned long)st);
    CHECKF(st == RPC_S_OK || st == ERROR_INVALID_PARAMETER, "status %lu", (unsigned long)st);
    U(MesHandleFree)(h);
}
