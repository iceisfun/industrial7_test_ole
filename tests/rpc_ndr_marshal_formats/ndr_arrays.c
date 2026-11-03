/* NDR arrays: fixed, conformant, varying, conformant varying and bogus (complex) arrays with hand built format strings. */
#include "ndrh.h"

/* Round trip helper: marshal mem, compare to wire, MemorySize, unmarshal into zeroed destination. */
static void rt(const char *tag, const fam *f, const unsigned char *fmt, unsigned char *mem, const unsigned char *wire, unsigned wl, unsigned memsz,
               unsigned dstsz, int do_free) {
    nctx c; ULONG pred = 0;
    nc_client(&c, fmt, wl + 64);
    memset(c.buf, 0, wl + 64);
    ULONG used = n_marshal(f, &c, mem, fmt, &pred);
    CHECKF(used == wl, "%s: marshalled %lu want %u", tag, used, wl);
    CHECKF(pred >= wl, "%s: BufferSize %lu < wire %u", tag, pred, wl);
    CHECKF(!memcmp(c.buf, wire, wl), "%s: wire bytes differ", tag);
    nc_done(&c);

    nc_rx(&c, fmt, wire, wl, 1);
    c.sm.MemorySize = 0;
    ULONG ms = sa_ms(f, &c, fmt, tag);
    if (memsz) CHECKF(ms == memsz, "%s: MemorySize %lu want %u", tag, ms, memsz);
    CHECKF(ms == c.sm.MemorySize, "%s: MemorySize ret != field", tag);
    CHECKF(nc_used(&c) == wl, "%s: MemorySize consumed %lu want %u", tag, nc_used(&c), wl);
    nc_done(&c);

    nc_rx(&c, fmt, wire, wl, 1);
    unsigned char *dst = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dstsz + 16);
    unsigned char *p = dst;
    unsigned char *r = sa_um(f, &c, &p, fmt, 0, tag);
    CHECKF(r == NULL, "%s: unmarshal ret %p", tag, r);
    CHECKF(p == dst, "%s: destination pointer replaced", tag);
    CHECKF(!memcmp(dst, mem, dstsz), "%s: unmarshalled memory differs", tag);
    CHECKF(nc_used(&c) == wl, "%s: unmarshal consumed %lu want %u", tag, nc_used(&c), wl);
    HeapFree(GetProcessHeap(), 0, dst);
    nc_done(&c);
}
static int reqfam(const fam *f) { if (!fam_ok(f)) { t_skip("missing %s exports", f->name); return 0; } return 1; }
#define W32(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

/* ================= fixed arrays ================= */
TEST(ndr_farray_long4) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 3, S16(16), FC_LONG, FC_END };
    unsigned mem[4] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
    static const unsigned char w[] = { W32(0x11111111), W32(0x22222222), W32(0x33333333), W32(0x44444444) };
    rt("long4", &f, fmt, (unsigned char *)mem, w, 16, 16, 16, 0);
}
TEST(ndr_farray_short3_odd_size) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 1, S16(6), FC_SHORT, FC_END };
    unsigned short mem[3] = { 0x1111, 0x2222, 0x3333 };
    static const unsigned char w[] = { 0x11, 0x11, 0x22, 0x22, 0x33, 0x33 };
    rt("short3", &f, fmt, (unsigned char *)mem, w, 6, 6, 6, 0);
}
TEST(ndr_farray_byte5) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 0, S16(5), FC_BYTE, FC_END };
    unsigned char mem[5] = { 1, 2, 3, 4, 5 };
    rt("byte5", &f, fmt, mem, mem, 5, 5, 5, 0);
}
TEST(ndr_farray_hyper2_alignment) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 7, S16(16), FC_HYPER, FC_END };
    unsigned long long mem[2] = { 0x1122334455667788ull, 0x99aabbccddeeff00ull };
    rt("hyper2", &f, fmt, (unsigned char *)mem, (unsigned char *)mem, 16, 16, 16, 0);
}
TEST(ndr_farray_large_lgfarray) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_LGFARRAY, 3, W32(1024), FC_LONG, FC_END };
    unsigned mem[256]; for (int i = 0; i < 256; i++) mem[i] = 0x01010101u * (i & 0xff) ^ 0x5a5a5a5a;
    rt("lgfarray", &f, fmt, (unsigned char *)mem, (unsigned char *)mem, 1024, 1024, 1024, 0);
}
TEST(ndr_farray_double_array_values) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 7, S16(24), FC_DOUBLE, FC_END };
    double mem[3] = { 1.5, -0.0, 1e300 };
    rt("double3", &f, fmt, (unsigned char *)mem, (unsigned char *)mem, 24, 24, 24, 0);
}
TEST(ndr_farray_marshal_misaligned_start) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 3, S16(8), FC_LONG, FC_END };
    unsigned mem[2] = { 0xAABBCCDD, 0x01020304 };
    for (int off = 0; off < 8; off++) {
        nctx c; nc_client(&c, fmt, 64);
        c.sm.Buffer = c.buf + off;
        f.ma(&c.sm, (unsigned char *)mem, fmt);
        ULONG_PTR st = ((ULONG_PTR)c.buf + off + 3) & ~3ull;
        CHECKF(!memcmp((void *)st, mem, 8) && (ULONG_PTR)c.sm.Buffer == st + 8, "off %d", off);
        nc_done(&c);
    }
}
TEST(ndr_farray_unmarshal_short_buffer_raises) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 3, S16(16), FC_LONG, FC_END };
    unsigned char w[16] = { 0 };
    for (unsigned len = 0; len < 16; len += 3) {
        nctx c; nc_rx(&c, fmt, w, len, 1);
        unsigned char dst[16], *p = dst;
        NTRY f.um(&c.sm, &p, fmt, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_farray_unmarshal_must_alloc) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 3, S16(8), FC_LONG, FC_END };
    unsigned w[2] = { 0xDEADC0DE, 0xFEEDFACE };
    nctx c; nc_rx(&c, fmt, w, 8, 1);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    f.um(&c.sm, &p, fmt, 1);
    CHECK(p != NULL);
    EXPECT_EQ(n_alloc_cnt, 1);
    if (p) { CHECK(!memcmp(p, w, 8)); n_free(p); }
    nc_done(&c);
}
TEST(ndr_farray_free_does_nothing_for_plain_data) {
    fam f = FAM(NdrFixedArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMFARRAY, 3, S16(8), FC_LONG, FC_END };
    nctx c; nc_client(&c, fmt, 16);
    unsigned mem[2] = { 1, 2 };
    n_free_cnt = 0;
    f.fr(&c.sm, (unsigned char *)mem, fmt);
    EXPECT_EQ(n_free_cnt, 0);
    nc_done(&c);
}

/* ================= conformant arrays ================= */
TEST(ndr_carray_const16_bytes_wire) {
    /* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 0, S16(1), 0x40, 0x0, S16(0x10), FC_BYTE, FC_END };
    unsigned char mem[16]; for (int i = 0; i < 16; i++) mem[i] = (unsigned char)(i * i);
    unsigned char w[20] = { W32(16) }; memcpy(w + 4, mem, 16);
    rt("carray16", &f, fmt, mem, w, 20, 16, 16, 0);
}
TEST(ndr_carray_const_long_array_alignment) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 3, S16(4), 0x40, 0x0, S16(3), FC_LONG, FC_END };
    unsigned mem[3] = { 1, 2, 3 };
    unsigned char w[16] = { W32(3), W32(1), W32(2), W32(3) };
    rt("carray_long3", &f, fmt, (unsigned char *)mem, w, 16, 12, 12, 0);
}
TEST(ndr_carray_hyper_data_aligned_to_eight) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 7, S16(8), 0x40, 0x0, S16(2), FC_HYPER, FC_END };
    unsigned long long mem[2] = { 0x0102030405060708ull, 0x1112131415161718ull };
    unsigned char w[24] = { W32(2), 0, 0, 0, 0, 0, 0, 0 };
    memcpy(w + 8, mem, 16);
    rt("carray_hyper", &f, fmt, (unsigned char *)mem, w, 24, 16, 16, 0);
}
TEST(ndr_carray_toplevel_size_is_from_stack) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    /* conformance: top-level ULONG argument 8 bytes above StackTop */
    static const unsigned char fmt[] = { FC_CARRAY, 3, S16(4), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(8), FC_LONG, FC_END };
    unsigned stack[4] = { 0, 0, 5, 0 };
    unsigned mem[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
    nctx c; nc_client(&c, fmt, 128);
    memset(c.buf, 0, 128);
    c.sm.StackTop = (unsigned char *)stack;
    c.sm.BufferLength = 0;
    f.bs(&c.sm, (unsigned char *)mem, fmt);
    CHECKF(c.sm.BufferLength >= 4 + 5 * 4, "bufsize %lu", c.sm.BufferLength);
    f.ma(&c.sm, (unsigned char *)mem, fmt);
    CHECK(nc_used(&c) == 24);
    EXPECT_EQ(*(unsigned *)c.buf, 5);
    CHECK(!memcmp(c.buf + 4, mem, 20));
    nc_done(&c);
}
TEST(ndr_carray_toplevel_conformance_operators) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const struct { unsigned char op; unsigned count; const char *n; } t[] = {
        { 0, 10, "none" }, { FC_DIV_2, 5, "div2" }, { FC_MULT_2, 20, "mult2" }, { FC_ADD_1, 11, "add1" }, { FC_SUB_1, 9, "sub1" },
    };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        unsigned char fmt[] = { FC_CARRAY, 0, S16(1), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, t[k].op, S16(0), FC_BYTE, FC_END };
        unsigned stack[2] = { 10, 0 };
        unsigned char mem[64]; for (int i = 0; i < 64; i++) mem[i] = (unsigned char)(0x80 + i);
        nctx c; nc_client(&c, fmt, 128);
        memset(c.buf, 0, 128);
        c.sm.StackTop = (unsigned char *)stack;
        f.ma(&c.sm, mem, fmt);
        CHECKF(*(unsigned *)c.buf == t[k].count, "%s: conformance %u want %u", t[k].n, *(unsigned *)c.buf, t[k].count);
        CHECKF(nc_used(&c) == 4 + t[k].count, "%s: used %lu", t[k].n, nc_used(&c));
        CHECKF(!memcmp(c.buf + 4, mem, t[k].count), "%s: data", t[k].n);
        nc_done(&c);
    }
}
TEST(ndr_carray_toplevel_conformance_types) {
    /* the correlation base type selects how many bytes of the stack slot are read */
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const struct { unsigned char ty; const char *n; } t[] = { { FC_SMALL, "small" }, { FC_USMALL, "usmall" }, { FC_SHORT, "short" }, { FC_USHORT, "ushort" }, { FC_LONG, "long" }, { FC_ULONG, "ulong" } };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        unsigned char fmt[] = { FC_CARRAY, 0, S16(1), FC_TOP_LEVEL_CONFORMANCE | t[k].ty, 0, S16(0), FC_BYTE, FC_END };
        unsigned long long stack[2]; memset(stack, 0xFF, sizeof stack); *(unsigned char *)stack = 7; ((unsigned char *)stack)[1] = 0; ((unsigned char *)stack)[2] = 0; ((unsigned char *)stack)[3] = 0;
        unsigned char mem[16] = { 0 };
        nctx c; nc_client(&c, fmt, 64);
        c.sm.StackTop = (unsigned char *)stack;
        f.ma(&c.sm, mem, fmt);
        CHECKF(*(unsigned *)c.buf == 7 || (t[k].ty == FC_SMALL || t[k].ty == FC_USMALL) && *(unsigned *)c.buf == 7, "%s: conformance %u", t[k].n, *(unsigned *)c.buf);
        nc_done(&c);
    }
}
TEST(ndr_carray_zero_length) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    unsigned char fmt[] = { FC_CARRAY, 3, S16(4), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(0), FC_LONG, FC_END };
    unsigned stack[2] = { 0, 0 };
    unsigned mem[1] = { 0 };
    nctx c; nc_client(&c, fmt, 16);
    memset(c.buf, 0, 16);
    c.sm.StackTop = (unsigned char *)stack;
    c.sm.BufferLength = 0;
    f.bs(&c.sm, (unsigned char *)mem, fmt);
    CHECKF(c.sm.BufferLength >= 4, "size %lu", c.sm.BufferLength);
    f.ma(&c.sm, (unsigned char *)mem, fmt);
    EXPECT_EQ(nc_used(&c), 4);
    EXPECT_EQ(*(unsigned *)c.buf, 0);
    nc_done(&c);
}
TEST(ndr_carray_unmarshal_count_larger_than_buffer_raises) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 3, S16(4), 0x40, 0, S16(3), FC_LONG, FC_END };
    /* wire says 3 elements but only two are present */
    unsigned char w[12] = { W32(3), W32(1), W32(2) };
    nctx c; nc_rx(&c, fmt, w, 12, 1);
    unsigned char dst[12] = { 0 }, *p = dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    CHECKF(n_code == X_BAD_STUB || n_code == X_INVALID_BOUND, "raised %lu", (unsigned long)n_code);
    nc_done(&c);
}
TEST(ndr_carray_unmarshal_const_conformance_mismatch_not_checked_without_corrinfo) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 3, S16(4), 0x40, 0, S16(2), FC_LONG, FC_END };
    unsigned char w[16] = { W32(3), W32(1), W32(2), W32(3) };   /* wire max_count 3 != constant 2 */
    nctx c; nc_rx(&c, fmt, w, 16, 1);
    unsigned char dst[12] = { 0 }, *p = dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    t_note("const conformance mismatch raised %lu (1734 = RPC_X_INVALID_BOUND expected)", (unsigned long)n_code);
    EXPECT_EQ(n_code, 0);   /* no correlation info (pCorrInfo) is active, so the constant is not cross-checked */
    nc_done(&c);
}
TEST(ndr_carray_server_unmarshal_points_into_buffer) {
    /* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 0, S16(1), 0x40, 0, S16(16), FC_BYTE, FC_END };
    unsigned char w[20] = { W32(16) }; for (int i = 0; i < 16; i++) w[4 + i] = (unsigned char)(i + 1);
    nctx c; nc_rx(&c, fmt, w, 20, 0);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    sa_um(&f, &c, &p, fmt, 0, __func__);
    CHECKF(p == c.buf + 4, "p=%p buf+4=%p", p, c.buf + 4);
    EXPECT_EQ(n_alloc_cnt, 0);
    nc_done(&c);
    nc_rx(&c, fmt, w, 20, 0);
    p = NULL; n_alloc_cnt = 0;
    f.um(&c.sm, &p, fmt, 1);   /* must-alloc forces a copy */
    CHECK(p != NULL && p != c.buf + 4);
    EXPECT_EQ(n_alloc_cnt, 1);
    if (p) { CHECK(!memcmp(p, w + 4, 16)); n_free(p); }
    nc_done(&c);
}
TEST(ndr_carray_client_unmarshal_allocates_when_asked) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 0, S16(1), 0x40, 0, S16(16), FC_BYTE, FC_END };
    unsigned char w[20] = { W32(16) };
    nctx c; nc_rx(&c, fmt, w, 20, 1);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    f.um(&c.sm, &p, fmt, 1);
    CHECK(p != NULL && p != c.buf + 4);
    EXPECT_EQ(n_alloc_cnt, 1);
    unsigned char *first = p;
    c.sm.Buffer = c.buf;
    sa_um(&f, &c, &p, fmt, 0, __func__);
    CHECK(p == first);      /* fMustAlloc == 0: reuse caller memory */
    EXPECT_EQ(n_alloc_cnt, 1);
    if (first) n_free(first);
    nc_done(&c);
}
TEST(ndr_carray_buffersize_prediction_grows_with_count) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    unsigned char fmt[] = { FC_CARRAY, 3, S16(4), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(0), FC_LONG, FC_END };
    unsigned prev = 0;
    for (unsigned n = 0; n <= 64; n += 8) {
        unsigned stack[2] = { n, 0 }; unsigned mem[64] = { 0 };
        nctx c; nc_client(&c, fmt, 16);
        c.sm.StackTop = (unsigned char *)stack; c.sm.BufferLength = 0;
        f.bs(&c.sm, (unsigned char *)mem, fmt);
        CHECKF(c.sm.BufferLength >= 4 + 4 * n, "n=%u size=%lu", n, c.sm.BufferLength);
        CHECKF(c.sm.BufferLength >= prev, "monotonic n=%u", n);
        prev = c.sm.BufferLength;
        nc_done(&c);
    }
}
TEST(ndr_carray_memorysize_accumulates) {
    fam f = FAM(NdrConformantArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CARRAY, 3, S16(4), 0x40, 0, S16(3), FC_LONG, FC_END };
    unsigned char w[16] = { W32(3), W32(1), W32(2), W32(3) };
    nctx c; nc_rx(&c, fmt, w, 16, 1);
    c.sm.MemorySize = 100;
    ULONG r = sa_ms(&f, &c, fmt, __func__);
    EXPECT_EQ(r, 112);
    EXPECT_EQ(c.sm.MemorySize, 112);
    nc_done(&c);
}

/* ================= varying arrays ================= */
TEST(ndr_varray_const_actual_count_wire) {
    fam f = FAM(NdrVaryingArray); if (!reqfam(&f)) return;
    /* long[8], variance constant 3 -> offset 0, actual 3, 3 elements */
    static const unsigned char fmt[] = { FC_SMVARRAY, 3, S16(32), S16(8), S16(4), 0x40, 0, S16(3), FC_LONG, FC_END };
    unsigned mem[8] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 };
    unsigned char w[20] = { W32(0), W32(3), W32(0xA0), W32(0xA1), W32(0xA2) };
    nctx c; nc_client(&c, fmt, 64);
    memset(c.buf, 0, 64);
    ULONG pred; ULONG used = n_marshal(&f, &c, (unsigned char *)mem, fmt, &pred);
    CHECKF(used == 20, "used %lu", used);
    CHECKF(pred >= 20, "pred %lu", pred);
    CHECK(!memcmp(c.buf, w, 20));
    nc_done(&c);
    /* unmarshal fills only the live prefix */
    nc_rx(&c, fmt, w, 20, 1);
    unsigned dst[8]; memset(dst, 0xEE, sizeof dst); unsigned char *p = (unsigned char *)dst;
    sa_um(&f, &c, &p, fmt, 0, __func__);
    CHECK(dst[0] == 0xA0 && dst[1] == 0xA1 && dst[2] == 0xA2);
    CHECK(dst[3] == 0xEEEEEEEE);
    EXPECT_EQ(nc_used(&c), 20);
    nc_done(&c);
    /* memory size is the whole array */
    nc_rx(&c, fmt, w, 20, 1);
    c.sm.MemorySize = 0;
    { ULONG m_ = sa_ms(&f, &c, fmt, __func__); EXPECT_EQ(m_, 32); }
    EXPECT_EQ(nc_used(&c), 20);
    nc_done(&c);
}
TEST(ndr_varray_bytes_zero_actual) {
    fam f = FAM(NdrVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMVARRAY, 0, S16(16), S16(16), S16(1), 0x40, 0, S16(0), FC_BYTE, FC_END };
    unsigned char mem[16] = { 1 };
    unsigned char w[8] = { W32(0), W32(0) };
    nctx c; nc_client(&c, fmt, 32); memset(c.buf, 0, 32);
    ULONG used = n_marshal(&f, &c, mem, fmt, NULL);
    CHECKF(used == 8 && !memcmp(c.buf, w, 8), "used %lu", used);
    nc_done(&c);
}
TEST(ndr_varray_hyper_elements_aligned) {
    fam f = FAM(NdrVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMVARRAY, 7, S16(32), S16(4), S16(8), 0x40, 0, S16(2), FC_HYPER, FC_END };
    unsigned long long mem[4] = { 0x1111111111111111ull, 0x2222222222222222ull, 3, 4 };
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    ULONG used = n_marshal(&f, &c, (unsigned char *)mem, fmt, NULL);
    CHECKF(used == 8 + 16, "used %lu", used);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 2);
    CHECK(!memcmp(c.buf + 8, mem, 16));
    nc_done(&c);
}
TEST(ndr_varray_unmarshal_actual_exceeds_array_raises) {
    fam f = FAM(NdrVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMVARRAY, 3, S16(16), S16(4), S16(4), 0x40, 0, S16(2), FC_LONG, FC_END };
    unsigned char w[8 + 20] = { W32(0), W32(5) };   /* 5 > 4 elements */
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    unsigned dst[8] = { 0 }; unsigned char *p = (unsigned char *)dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    t_note("varying array actual>size raised %lu", (unsigned long)n_code);
    CHECKF(n_code == X_INVALID_BOUND || n_code == X_BAD_STUB, "raised %lu", (unsigned long)n_code);
    nc_done(&c);
}
TEST(ndr_varray_unmarshal_offset_plus_actual_beyond_array_raises) {
    fam f = FAM(NdrVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMVARRAY, 3, S16(16), S16(4), S16(4), 0x40, 0, S16(2), FC_LONG, FC_END };
    unsigned char w[8 + 12] = { W32(3), W32(2) };   /* offset 3 + actual 2 = 5 > 4 */
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    unsigned dst[8] = { 0 }; unsigned char *p = (unsigned char *)dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    t_note("varying array offset+actual>size raised %lu", (unsigned long)n_code);
    CHECKF(n_code == X_INVALID_BOUND || n_code == X_BAD_STUB, "raised %lu", (unsigned long)n_code);
    nc_done(&c);
}
TEST(ndr_varray_unmarshal_nonzero_offset_places_elements) {
    fam f = FAM(NdrVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_SMVARRAY, 3, S16(16), S16(4), S16(4), 0x40, 0, S16(2), FC_LONG, FC_END };
    unsigned char w[8 + 8] = { W32(2), W32(2), W32(0x77), W32(0x88) };
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    unsigned dst[4]; memset(dst, 0xEE, sizeof dst); unsigned char *p = (unsigned char *)dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(dst[0] == 0xEEEEEEEE && dst[1] == 0xEEEEEEEE && dst[2] == 0x77 && dst[3] == 0x88);
    nc_done(&c);
}

/* ================= conformant varying arrays ================= */
TEST(ndr_cvarray_wire_layout) {
    fam f = FAM(NdrConformantVaryingArray); if (!reqfam(&f)) return;
    /* [size_is(8), length_is(3)] long a[] with constant bounds */
    static const unsigned char fmt[] = { FC_CVARRAY, 3, S16(4), 0x40, 0, S16(8), 0x40, 0, S16(3), FC_LONG, FC_END };
    unsigned mem[8] = { 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7 };
    unsigned char w[24] = { W32(8), W32(0), W32(3), W32(0xB0), W32(0xB1), W32(0xB2) };
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    ULONG pred; ULONG used = n_marshal(&f, &c, (unsigned char *)mem, fmt, &pred);
    CHECKF(used == 24, "used %lu", used);
    CHECKF(pred >= 24, "pred %lu", pred);
    CHECK(!memcmp(c.buf, w, 24));
    nc_done(&c);
    nc_rx(&c, fmt, w, 24, 1);
    unsigned dst[8]; memset(dst, 0xEE, sizeof dst); unsigned char *p = (unsigned char *)dst;
    sa_um(&f, &c, &p, fmt, 0, __func__);
    CHECK(dst[0] == 0xB0 && dst[1] == 0xB1 && dst[2] == 0xB2 && dst[3] == 0xEEEEEEEE);
    EXPECT_EQ(nc_used(&c), 24);
    nc_done(&c);
    nc_rx(&c, fmt, w, 24, 1);
    c.sm.MemorySize = 0;
    { ULONG m_ = sa_ms(&f, &c, fmt, __func__); EXPECT_EQ(m_, 32); }
    nc_done(&c);
}
TEST(ndr_cvarray_variance_from_stack) {
    fam f = FAM(NdrConformantVaryingArray); if (!reqfam(&f)) return;
    unsigned char fmt[] = { FC_CVARRAY, 0, S16(1), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(0), FC_TOP_LEVEL_VARIANCE | FC_ULONG, 0, S16(8), FC_BYTE, FC_END };
    unsigned stack[4] = { 12, 0, 5, 0 };    /* size_is = 12, length_is = 5 (offset 8) */
    unsigned char mem[12]; for (int i = 0; i < 12; i++) mem[i] = (unsigned char)(0x30 + i);
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    c.sm.StackTop = (unsigned char *)stack;
    ULONG used = n_marshal(&f, &c, mem, fmt, NULL);
    CHECKF(used == 12 + 5, "used %lu", used);
    EXPECT_EQ(*(unsigned *)c.buf, 12);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 0);
    EXPECT_EQ(*(unsigned *)(c.buf + 8), 5);
    CHECK(!memcmp(c.buf + 12, mem, 5));
    nc_done(&c);
}
TEST(ndr_cvarray_unmarshal_actual_beyond_max_raises) {
    fam f = FAM(NdrConformantVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CVARRAY, 0, S16(1), 0x40, 0, S16(8), 0x40, 0, S16(3), FC_BYTE, FC_END };
    unsigned char w[12 + 16] = { W32(8), W32(0), W32(9) };   /* actual 9 > max 8 */
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    unsigned char dst[16], *p = dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    t_note("cvarray actual>max raised %lu", (unsigned long)n_code);
    CHECKF(n_code == X_INVALID_BOUND || n_code == X_BAD_STUB, "raised %lu", (unsigned long)n_code);
    nc_done(&c);
}
TEST(ndr_cvarray_unmarshal_offset_plus_actual_beyond_max_raises) {
    fam f = FAM(NdrConformantVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CVARRAY, 0, S16(1), 0x40, 0, S16(8), 0x40, 0, S16(3), FC_BYTE, FC_END };
    unsigned char w[12 + 16] = { W32(8), W32(6), W32(3) };   /* offset 6 + actual 3 > 8 */
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    unsigned char dst[16], *p = dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    t_note("cvarray offset+actual>max raised %lu", (unsigned long)n_code);
    CHECKF(n_code == X_INVALID_BOUND || n_code == X_BAD_STUB, "raised %lu", (unsigned long)n_code);
    nc_done(&c);
}
TEST(ndr_cvarray_unmarshal_short_header_raises) {
    fam f = FAM(NdrConformantVaryingArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_CVARRAY, 0, S16(1), 0x40, 0, S16(8), 0x40, 0, S16(3), FC_BYTE, FC_END };
    unsigned char w[12] = { W32(8), W32(0), W32(3) };
    for (unsigned len = 0; len < 15; len += 4) {
        nctx c; nc_rx(&c, fmt, w, len < 12 ? len : 12, 1);
        unsigned char dst[16], *p = dst;
        NTRY f.um(&c.sm, &p, fmt, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}

/* ================= complex (bogus) arrays ================= */
TEST(ndr_complexarray_fixed_long3) {
    fam f = FAM(NdrComplexArray); if (!reqfam(&f)) return;
    static const unsigned char fmt[] = { FC_BOGUS_ARRAY, 3, S16(3), W32(0xffffffff), W32(0xffffffff), FC_LONG, FC_END };
    unsigned mem[3] = { 0xC0, 0xC1, 0xC2 };
    rt("bogus_long3", &f, fmt, (unsigned char *)mem, (unsigned char *)mem, 12, 12, 12, 0);
}
TEST(ndr_complexarray_conformant_from_stack) {
    fam f = FAM(NdrComplexArray); if (!reqfam(&f)) return;
    unsigned char fmt[] = { FC_BOGUS_ARRAY, 3, S16(0), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(0), W32(0xffffffff), FC_LONG, FC_END };
    unsigned stack[2] = { 4, 0 };
    unsigned mem[4] = { 1, 2, 3, 4 };
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    c.sm.StackTop = (unsigned char *)stack;
    ULONG pred; ULONG used = n_marshal(&f, &c, (unsigned char *)mem, fmt, &pred);
    CHECKF(used == 20, "used %lu", used);
    CHECKF(pred >= 20, "pred %lu", pred);
    EXPECT_EQ(*(unsigned *)c.buf, 4);
    CHECK(!memcmp(c.buf + 4, mem, 16));
    nc_done(&c);
}
TEST(ndr_complexarray_conformant_enum16_elements_two_bytes_each) {
    fam f = FAM(NdrComplexArray); if (!reqfam(&f)) return;
    unsigned char fmt[] = { FC_BOGUS_ARRAY, 3, S16(0), FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(0), W32(0xffffffff), FC_ENUM16, FC_END };
    unsigned stack[2] = { 3, 0 };
    unsigned mem[3] = { 1, 2, 0x7fff };
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    c.sm.StackTop = (unsigned char *)stack;
    ULONG used = n_marshal(&f, &c, (unsigned char *)mem, fmt, NULL);
    CHECKF(used == 4 + 6, "used %lu", used);
    EXPECT_EQ(*(unsigned short *)(c.buf + 4), 1);
    EXPECT_EQ(*(unsigned short *)(c.buf + 6), 2);
    EXPECT_EQ(*(unsigned short *)(c.buf + 8), 0x7fff);
    nc_done(&c);
}
TEST(ndr_complexarray_unmarshal_enum16_range) {
    fam f = FAM(NdrComplexArray); if (!reqfam(&f)) return;
    unsigned char fmt[] = { FC_BOGUS_ARRAY, 3, S16(2), W32(0xffffffff), W32(0xffffffff), FC_ENUM16, FC_END };
    unsigned char good[4] = { 1, 0, 0xff, 0x7f }, bad[4] = { 1, 0, 0x00, 0x80 };
    nctx c; nc_rx(&c, fmt, good, 4, 1);
    unsigned dst[2] = { 0 }; unsigned char *p = (unsigned char *)dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    EXPECT_EQ(n_code, 0); EXPECT_EQ(dst[0], 1); EXPECT_EQ(dst[1], 0x7fff);
    nc_done(&c);
    nc_rx(&c, fmt, bad, 4, 1); p = (unsigned char *)dst;
    NTRY f.um(&c.sm, &p, fmt, 0); NEND;
    EXPECT_EQ(n_code, X_ENUM_RANGE);
    nc_done(&c);
}
