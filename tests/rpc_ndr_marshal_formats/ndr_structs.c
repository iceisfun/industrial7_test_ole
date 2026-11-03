/* NDR structures and unions: simple, conformant, conformant varying, complex (bogus) structs; encapsulated / non-encapsulated unions. */
#include "ndrh.h"
#define W32(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff
#define W16(x) (x) & 0xff, ((x) >> 8) & 0xff

/* ---------------- simple structs ---------------- */
struct s1 { char c; char pad[3]; int l; short s; short pad2; };
static const unsigned char f_s1[] = { FC_STRUCT, 3, S16(12), FC_CHAR, FC_STRUCTPAD3, FC_LONG, FC_SHORT, FC_STRUCTPAD2, FC_END };
struct s2 { char c; char pad[7]; double d; };
static const unsigned char f_s2[] = { FC_STRUCT, 7, S16(16), FC_CHAR, FC_STRUCTPAD7, FC_DOUBLE, FC_END };
struct s3 { short a, b; };
static const unsigned char f_s3[] = { FC_STRUCT, 1, S16(4), FC_SHORT, FC_SHORT, FC_END };
struct s4 { int a; int pad; long long h; };
static const unsigned char f_s4[] = { FC_STRUCT, 7, S16(16), FC_LONG, FC_STRUCTPAD4, FC_HYPER, FC_END };
struct s5 { char a, b, c; };
static const unsigned char f_s5[] = { FC_STRUCT, 0, S16(3), FC_CHAR, FC_CHAR, FC_CHAR, FC_END };

static void srt(const char *tag, const unsigned char *fmt, void *mem, unsigned size, unsigned align_mask) {
    fam f = FAM(NdrSimpleStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    nctx c; ULONG pred = 0;
    nc_client(&c, fmt, 128); memset(c.buf, 0, 128);
    ULONG used = n_marshal(&f, &c, mem, fmt, &pred);
    CHECKF(used == size, "%s: marshalled %lu want %u", tag, used, size);
    CHECKF(pred >= size, "%s: pred %lu", tag, pred);
    CHECKF(!memcmp(c.buf, mem, size), "%s: wire bytes", tag);
    nc_done(&c);
    /* misaligned start */
    for (int off = 0; off < 8; off++) {
        nc_client(&c, fmt, 128);
        c.sm.Buffer = c.buf + off;
        f.ma(&c.sm, mem, fmt);
        ULONG_PTR st = ((ULONG_PTR)c.buf + off + align_mask) & ~(ULONG_PTR)align_mask;
        CHECKF((ULONG_PTR)c.sm.Buffer == st + size, "%s: off %d end", tag, off);
        CHECKF(!memcmp((void *)st, mem, size), "%s: off %d data", tag, off);
        nc_done(&c);
    }
    nc_rx(&c, fmt, mem, size, 1);
    c.sm.MemorySize = 0;
    ULONG ms = sa_ms(&f, &c, fmt, tag);
    CHECKF(ms == size, "%s: MemorySize %lu", tag, ms);
    CHECKF(nc_used(&c) == size, "%s: MemorySize consumed %lu", tag, nc_used(&c));
    nc_done(&c);
    nc_rx(&c, fmt, mem, size, 1);
    unsigned char *dst = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 8), *p = dst;
    sa_um(&f, &c, &p, fmt, 0, tag);
    CHECKF(p == dst && !memcmp(dst, mem, size), "%s: unmarshal into caller memory", tag);
    HeapFree(GetProcessHeap(), 0, dst);
    nc_done(&c);
    nc_rx(&c, fmt, mem, size, 1);
    p = NULL; n_alloc_cnt = 0;
    sa_um(&f, &c, &p, fmt, 1, tag);
    CHECKF(p && !memcmp(p, mem, size) && n_alloc_cnt == 1, "%s: unmarshal must-alloc (allocs %d)", tag, n_alloc_cnt);
    if (p) n_free(p);
    nc_done(&c);
    /* short */
    for (unsigned len = 0; len < size; len += (size > 4 ? 3 : 1)) {
        nc_rx(&c, fmt, mem, len, 1);
        unsigned char dst2[32], *q = dst2;
        NTRY f.um(&c.sm, &q, fmt, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "%s: len %u raised %lu", tag, len, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_sstruct_char_pad_long_short) { struct s1 v = { 'A', {0}, 0x01020304, 0x0506, 0 }; srt("s1", f_s1, &v, 12, 3); }
TEST(ndr_sstruct_char_double) { struct s2 v = { 'Z', {0}, 3.25 }; srt("s2", f_s2, &v, 16, 7); }
TEST(ndr_sstruct_two_shorts) { struct s3 v = { 0x1111, 0x2222 }; srt("s3", f_s3, &v, 4, 1); }
TEST(ndr_sstruct_long_hyper) { struct s4 v = { 0x01020304, 0, 0x1112131415161718ll }; srt("s4", f_s4, &v, 16, 7); }
TEST(ndr_sstruct_three_bytes) { struct s5 v = { 1, 2, 3 }; srt("s5", f_s5, &v, 3, 0); }
TEST(ndr_sstruct_free_is_noop_for_plain_struct) {
    fam f = FAM(NdrSimpleStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    nctx c; nc_client(&c, f_s3, 16);
    struct s3 v = { 1, 2 };
    n_free_cnt = 0;
    f.fr(&c.sm, (unsigned char *)&v, f_s3);
    EXPECT_EQ(n_free_cnt, 0);
    nc_done(&c);
}
TEST(ndr_sstruct_memorysize_aligns_accumulator) {
    fam f = FAM(NdrSimpleStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    struct s4 v = { 1, 0, 2 };
    nctx c; nc_rx(&c, f_s4, &v, 16, 1);
    c.sm.MemorySize = 3;
    ULONG r = sa_ms(&f, &c, f_s4, __func__);
    t_note("MemorySize 3 + struct(align 8,size 16) -> %lu", r);
    CHECK(r >= 19);
    CHECK(r == c.sm.MemorySize);
    nc_done(&c);
}

/* ---------------- conformant struct: struct { long n; [size_is(n)] long a[]; } ---------------- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) (format layout) */
static const unsigned char f_cs[] = {
    FC_CARRAY, 3, S16(4), FC_LONG, 0x00, S16(0xfffc), FC_LONG, FC_END,        /* 0..9 (4 byte correlation descriptor) */
    FC_CSTRUCT, 3, S16(4), S16(0xfff2), FC_LONG, FC_END,                      /* 10..: array desc at 0; offset field at index 14 -> -14 */
};
TEST(ndr_cstruct_wire_layout) {
    fam f = FAM(NdrConformantStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    const unsigned char *fmt = f_cs + 10;
    struct { int n; int a[5]; } v = { 3, { 0xA0, 0xA1, 0xA2, 0, 0 } };
    nctx c; ULONG pred;
    nc_client(&c, fmt, 128); memset(c.buf, 0, 128);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, fmt, &pred);
    CHECKF(used == 4 + 4 + 12, "used %lu", used);
    CHECKF(pred >= 20, "pred %lu", pred);
    unsigned *w = (unsigned *)c.buf;
    CHECKF(w[0] == 3 && w[1] == 3 && w[2] == 0xA0 && w[3] == 0xA1 && w[4] == 0xA2, "wire %x %x %x %x %x", w[0], w[1], w[2], w[3], w[4]);
    nc_done(&c);
}
TEST(ndr_cstruct_zero_elements) {
    fam f = FAM(NdrConformantStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    const unsigned char *fmt = f_cs + 10;
    struct { int n; int a[1]; } v = { 0, { 0 } };
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, fmt, NULL);
    EXPECT_EQ(used, 8);
    nc_done(&c);
}
TEST(ndr_cstruct_unmarshal_and_memorysize) {
    fam f = FAM(NdrConformantStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    const unsigned char *fmt = f_cs + 10;
    unsigned w[] = { 3, 3, 0xB0, 0xB1, 0xB2 };
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    c.sm.MemorySize = 0;
    ULONG ms = sa_ms(&f, &c, fmt, __func__);
    EXPECT_EQ(ms, 4 + 12);
    EXPECT_EQ(nc_used(&c), sizeof w);
    nc_done(&c);
    for (int client = 0; client < 2; client++) {
        nc_rx(&c, fmt, w, sizeof w, client);
        unsigned char *p = NULL; n_alloc_cnt = 0;
        sa_um(&f, &c, &p, fmt, client, __func__);   /* client + NULL destination needs fMustAlloc */
        CHECKF(p != NULL, "client=%d", client);
        if (p) {
            int *m = (int *)p;
            CHECKF(m[0] == 3 && m[1] == 0xB0 && m[2] == 0xB1 && m[3] == 0xB2, "client=%d values", client);
            if (p < c.buf || p >= c.buf + c.cap) n_free(p);
        }
        EXPECT_EQ(nc_used(&c), sizeof w);
        nc_done(&c);
    }
}
TEST(ndr_cstruct_unmarshal_conformance_exceeds_buffer_raises) {
    fam f = FAM(NdrConformantStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    const unsigned char *fmt = f_cs + 10;
    unsigned w[] = { 100, 100, 1, 2 };
    nctx c; nc_rx(&c, fmt, w, sizeof w, 1);
    unsigned char *p = NULL;
    NTRY f.um(&c.sm, &p, fmt, 1); NEND;
    t_note("cstruct count beyond buffer raised %lu", (unsigned long)n_code);
    CHECKF(n_code == X_BAD_STUB || n_code == X_INVALID_BOUND, "raised %lu", (unsigned long)n_code);
    nc_done(&c);
}

/* ---------------- conformant varying struct: struct { long len; [size_is(8), length_is(len)] char a[]; } ---------------- */
static const unsigned char f_cvs[] = {
    FC_CVARRAY, 0, S16(1), FC_CONSTANT_CONFORMANCE, 0, S16(8), FC_LONG, 0, S16(0xfffc), FC_BYTE, FC_END,   /* 0..12 */
    FC_CVSTRUCT, 3, S16(4), S16(0xffee), FC_LONG, FC_END,                                                 /* 14..: offset field at index 18 -> -18 */
};
TEST(ndr_cvstruct_wire_layout) {
    fam f = FAM(NdrConformantVaryingStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    const unsigned char *fmt = f_cvs;
    struct { int len; unsigned char a[8]; } v = { 3, { 'x', 'y', 'z', 0, 0, 0, 0, 0 } };
    nctx c; ULONG pred;
    nc_client(&c, fmt + 14, 128); memset(c.buf, 0, 128);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, fmt + 14, &pred);
    t_note("cvstruct used %lu pred %lu: %08x %08x %08x %08x %08x", used, pred, ((unsigned *)c.buf)[0], ((unsigned *)c.buf)[1], ((unsigned *)c.buf)[2], ((unsigned *)c.buf)[3], ((unsigned *)c.buf)[4]);
    CHECKF(pred >= used, "pred %lu < used %lu", pred, used);
    nc_done(&c);
}

/* ---------------- complex (bogus) structs ---------------- */
static const unsigned char f_bs_enum[] = { FC_BOGUS_STRUCT, 3, S16(8), S16(0), S16(0), FC_LONG, FC_ENUM16, FC_STRUCTPAD2, FC_END };
TEST(ndr_bstruct_enum16_member_is_two_bytes_on_wire) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    struct { int a; int e; } v = { 0x11223344, 0x1234 };
    nctx c; ULONG pred;
    nc_client(&c, f_bs_enum, 64); memset(c.buf, 0, 64);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, f_bs_enum, &pred);
    CHECKF(used == 6, "used %lu", used);
    CHECKF(pred >= 6, "pred %lu", pred);
    EXPECT_EQ(*(unsigned *)c.buf, 0x11223344);
    EXPECT_EQ(*(unsigned short *)(c.buf + 4), 0x1234);
    nc_done(&c);
    unsigned char w[6] = { 0x44, 0x33, 0x22, 0x11, 0x34, 0x12 };
    nc_rx(&c, f_bs_enum, w, 6, 1);
    struct { int a; int e; } dst = { 0, -1 }; unsigned char *p = (unsigned char *)&dst;
    sa_um(&f, &c, &p, f_bs_enum, 0, __func__);
    EXPECT_EQ(dst.a, 0x11223344);
    EXPECT_EQ(dst.e, 0x1234);       /* widened to the 4-byte C enum */
    EXPECT_EQ(nc_used(&c), 6);
    nc_done(&c);
    nc_rx(&c, f_bs_enum, w, 6, 1);
    c.sm.MemorySize = 0;
    { ULONG m_ = sa_ms(&f, &c, f_bs_enum, __func__); EXPECT_EQ(m_, 8); }
    nc_done(&c);
}
TEST(ndr_bstruct_enum16_member_out_of_range_raises) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned char w[6] = { 1, 0, 0, 0, 0x00, 0x80 };
    nctx c; nc_rx(&c, f_bs_enum, w, 6, 1);
    struct { int a; int e; } dst = { 0 }; unsigned char *p = (unsigned char *)&dst;
    NTRY f.um(&c.sm, &p, f_bs_enum, 0); NEND;
    EXPECT_EQ(n_code, X_ENUM_RANGE);
    nc_done(&c);
}
TEST(ndr_bstruct_unmarshal_short_buffer_raises) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned char w[6] = { 1, 0, 0, 0, 1, 0 };
    for (unsigned len = 0; len < 6; len++) {
        nctx c; nc_rx(&c, f_bs_enum, w, len, 1);
        struct { int a; int e; } dst = { 0 }; unsigned char *p = (unsigned char *)&dst;
        NTRY f.um(&c.sm, &p, f_bs_enum, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}

/* struct { long a; long b; [unique] long *p; } as a bogus struct (the 64-bit MIDL shape) */
static const unsigned char f_bs_ptr[] = {
    FC_BOGUS_STRUCT, 3, S16(16), S16(0), S16(6),     /* 0..7 : conformant array offset 0, pointer layout at +6 -> index 12 */
    FC_LONG, FC_LONG, FC_POINTER, FC_END,             /* 8..11 */
    FC_UP, 0x08, FC_LONG, FC_PAD,                     /* 12..15: pointer description */
};
struct bsp { int a, b; int *p; };
TEST(ndr_bstruct_embedded_unique_pointer_wire) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    int target = 0x7777; struct bsp v = { 1, 2, &target };
    nctx c; ULONG pred;
    nc_client(&c, f_bs_ptr, 64); memset(c.buf, 0, 64);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, f_bs_ptr, &pred);
    CHECKF(used == 16, "used %lu", used);
    CHECKF(pred >= 16, "pred %lu", pred);
    unsigned *w = (unsigned *)c.buf;
    CHECKF(w[0] == 1 && w[1] == 2 && w[2] == 0x20000 && w[3] == 0x7777, "wire %x %x %x %x", w[0], w[1], w[2], w[3]);
    /* null pointer: id 0 and no referent */
    nc_done(&c);
    v.p = NULL;
    nc_client(&c, f_bs_ptr, 64); memset(c.buf, 0, 64);
    used = n_marshal(&f, &c, (unsigned char *)&v, f_bs_ptr, NULL);
    CHECKF(used == 12, "null used %lu", used);
    CHECKF(((unsigned *)c.buf)[2] == 0, "null id");
    nc_done(&c);
}
TEST(ndr_bstruct_embedded_unique_pointer_unmarshal_and_free) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned w[4] = { 5, 6, 0x20000, 0x4242 };
    nctx c; nc_rx(&c, f_bs_ptr, w, sizeof w, 1);
    struct bsp dst; memset(&dst, 0, sizeof dst); unsigned char *p = (unsigned char *)&dst;
    sa_um(&f, &c, &p, f_bs_ptr, 0, __func__);
    CHECK(dst.a == 5 && dst.b == 6 && dst.p && *dst.p == 0x4242);
    EXPECT_EQ(nc_used(&c), 16);
    n_free_cnt = 0;
    f.fr(&c.sm, (unsigned char *)&dst, f_bs_ptr);
    EXPECT_EQ(n_free_cnt, 1);      /* the pointee; the struct itself is caller memory */
    nc_done(&c);
    unsigned w2[3] = { 5, 6, 0 };
    nc_rx(&c, f_bs_ptr, w2, sizeof w2, 1);
    memset(&dst, 0xEE, sizeof dst); p = (unsigned char *)&dst;
    sa_um(&f, &c, &p, f_bs_ptr, 0, __func__);
    CHECK(dst.a == 5 && dst.b == 6 && dst.p == NULL);
    EXPECT_EQ(nc_used(&c), 12);
    nc_done(&c);
}
TEST(ndr_bstruct_memorysize_with_pointer) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned w[4] = { 5, 6, 0x20000, 0x4242 };
    nctx c; nc_rx(&c, f_bs_ptr, w, sizeof w, 1);
    c.sm.MemorySize = 0;
    ULONG r = sa_ms(&f, &c, f_bs_ptr, __func__);
    t_note("complex struct with pointer: MemorySize %lu (expected 16 + 4)", r);
    CHECK(r >= 16);
    EXPECT_EQ(nc_used(&c), 16);
    nc_done(&c);
}

/* conformant complex struct: struct { enum e; long size; [size_is(size)] long a[]; } (Wine) */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
static const unsigned char f_bcs[] = {
    S16(0),
    FC_CARRAY, 3, S16(4), FC_LONG, 0, S16(0xfffc), FC_LONG, FC_END,
    FC_BOGUS_STRUCT, 3, S16(8), S16(0xfff2), S16(0), FC_ENUM16, FC_LONG, FC_PAD, FC_END,
};
TEST(ndr_bstruct_conformant_wire_layout) {
    fam f = FAM(NdrComplexStruct); if (!fam_ok(&f)) { t_skip("exports"); return; }
    const unsigned char *fmt = f_bcs + 12;
    struct { int e; unsigned size; unsigned a[20]; } v; memset(&v, 0, sizeof v); v.size = 20; v.e = 3;
    for (int i = 0; i < 20; i++) v.a[i] = 0x100 + i;
    nctx c; ULONG pred;
    nc_client(&c, fmt, 256); memset(c.buf, 0, 256);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, fmt, &pred);
    CHECKF(used == 4 + 4 + 4 + 80, "used %lu", used);
    CHECKF(pred >= 92, "pred %lu", pred);
    unsigned *w = (unsigned *)c.buf;
    CHECKF(w[0] == 20, "conformance %u", w[0]);
    CHECKF(*(unsigned short *)(c.buf + 4) == 3, "enum16 %u", *(unsigned short *)(c.buf + 4));
    CHECKF(w[2] == 20, "size field %u", w[2]);
    CHECKF(w[3] == 0x100 && w[22] == 0x113, "array %x .. %x", w[3], w[22]);
    nc_done(&c);
}

/* ---------------- unions ---------------- */
/* switch short; case 1: long, case 2: short, case 3: empty; no default (error); union align 4, union 4 bytes at offset 4 */
static const unsigned char f_eu[] = {
    FC_ENCAPSULATED_UNION, 0x40 | FC_SHORT, S16(4), S16(0x3003),
    W32(1), W16(0x8008),
    W32(2), W16(0x8006),
    W32(3), W16(0),
    W16(0xffff),
};
struct eu { short sw; short pad; union { int l; short s; } u; };
TEST(ndr_eunion_arms_wire_layout) {
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    static const struct { short sw; int val; unsigned want; const char *n; } t[] = { { 1, 0x0A0B0C0D, 8, "long arm" }, { 2, 0x1234, 6, "short arm" }, { 3, 0, 4, "empty arm" } };
    for (unsigned k = 0; k < 3; k++) {
        struct eu v; memset(&v, 0, sizeof v); v.sw = t[k].sw; v.u.l = t[k].val;
        nctx c; ULONG pred;
        nc_client(&c, f_eu, 64); memset(c.buf, 0, 64);
        ULONG used = n_marshal(&f, &c, (unsigned char *)&v, f_eu, &pred);
        CHECKF(used == t[k].want, "%s: used %lu want %u", t[k].n, used, t[k].want);
        CHECKF(pred >= t[k].want, "%s: pred %lu", t[k].n, pred);
        CHECKF(*(short *)c.buf == t[k].sw, "%s: switch", t[k].n);
        if (t[k].sw == 1) CHECKF(*(int *)(c.buf + 4) == 0x0A0B0C0D, "%s: value", t[k].n);
        if (t[k].sw == 2) CHECKF(*(short *)(c.buf + 4) == 0x1234, "%s: value", t[k].n);
        nc_done(&c);
    }
}
TEST(ndr_eunion_unknown_switch_raises_invalid_tag) {
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    static const short bad[] = { 0, 4, -1, 0x7fff };
    for (unsigned k = 0; k < 4; k++) {
        struct eu v; memset(&v, 0, sizeof v); v.sw = bad[k];
        nctx c; nc_client(&c, f_eu, 64);
        NTRY f.ma(&c.sm, (unsigned char *)&v, f_eu); NEND;
        CHECKF(n_code == 1733, "marshal sw=%d raised %lu", bad[k], (unsigned long)n_code);
        c.sm.BufferLength = 0;
        NTRY f.bs(&c.sm, (unsigned char *)&v, f_eu); NEND;
        CHECKF(n_code == 1733, "bufsize sw=%d raised %lu", bad[k], (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_eunion_unmarshal_arms) {
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned char w1[8] = { 1, 0, 0, 0, 0x0D, 0x0C, 0x0B, 0x0A };
    nctx c; nc_rx(&c, f_eu, w1, 8, 1);
    struct eu dst; memset(&dst, 0xEE, sizeof dst); unsigned char *p = (unsigned char *)&dst;
    sa_um(&f, &c, &p, f_eu, 0, "long");
    CHECK(dst.sw == 1 && dst.u.l == 0x0A0B0C0D);
    EXPECT_EQ(nc_used(&c), 8);
    nc_done(&c);
    unsigned char w2[6] = { 2, 0, 0, 0, 0x34, 0x12 };
    nc_rx(&c, f_eu, w2, 6, 1);
    p = (unsigned char *)&dst;
    sa_um(&f, &c, &p, f_eu, 0, "short");
    CHECK(dst.sw == 2 && dst.u.s == 0x1234);
    EXPECT_EQ(nc_used(&c), 6);
    nc_done(&c);
    /* memory size = switch offset + union size = 8 */
    nc_rx(&c, f_eu, w1, 8, 1);
    c.sm.MemorySize = 0;
    ULONG r = sa_ms(&f, &c, f_eu, "memsize");
    t_note("encapsulated union MemorySize %lu", r);
    CHECK(r == 8);
    nc_done(&c);
}
TEST(ndr_eunion_unmarshal_alloc_zeroed_and_unknown_tag) {
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned char w3[4] = { 3, 0, 0, 0 };
    nctx c; nc_rx(&c, f_eu, w3, 4, 1);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    sa_um(&f, &c, &p, f_eu, 0, "empty");
    CHECK(p != NULL && n_alloc_cnt == 1);
    if (p) { CHECK(((struct eu *)p)->sw == 3); n_free(p); }
    nc_done(&c);
    unsigned char wbad[8] = { 9, 0, 0, 0, 1, 2, 3, 4 };
    nc_rx(&c, f_eu, wbad, 8, 1);
    struct eu dst; unsigned char *q = (unsigned char *)&dst;
    NTRY f.um(&c.sm, &q, f_eu, 0); NEND;
    EXPECT_EQ(n_code, 1733);
    nc_done(&c);
}
TEST(ndr_eunion_unmarshal_truncated) {
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned char w1[8] = { 1, 0, 0, 0, 1, 2, 3, 4 };
    for (unsigned len = 0; len < 8; len++) {
        nctx c; nc_rx(&c, f_eu, w1, len, 1);
        struct eu dst; unsigned char *q = (unsigned char *)&dst;
        NTRY f.um(&c.sm, &q, f_eu, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}
/* default arm: switch long; case 5: hyper; default: long */
static const unsigned char f_eu_def[] = {
    FC_ENCAPSULATED_UNION, 0x40 | FC_LONG, S16(8), S16(0x7001),
    W32(5), W16(0x800b),
    W16(0x8008),
};
TEST(ndr_eunion_default_arm_and_hyper_alignment) {
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    struct { int sw; int pad; union { long long h; int l; } u; } v;
    memset(&v, 0, sizeof v); v.sw = 5; v.u.h = 0x1122334455667788ll;
    nctx c; nc_client(&c, f_eu_def, 64); memset(c.buf, 0, 64);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, f_eu_def, NULL);
    CHECKF(used == 16, "hyper arm used %lu", used);
    EXPECT_EQ(*(unsigned *)c.buf, 5);
    EXPECT_EQ(*(unsigned long long *)(c.buf + 8), 0x1122334455667788ull);
    nc_done(&c);
    memset(&v, 0, sizeof v); v.sw = 99; v.u.l = 0x55AA55AA;
    nc_client(&c, f_eu_def, 64); memset(c.buf, 0, 64);
    used = n_marshal(&f, &c, (unsigned char *)&v, f_eu_def, NULL);
    CHECKF(used == 16, "default arm used %lu", used);     /* union alignment (8) applies to every arm */
    EXPECT_EQ(*(unsigned *)c.buf, 99);
    EXPECT_EQ(*(unsigned *)(c.buf + 8), 0x55AA55AA);
    nc_done(&c);
}
TEST(ndr_eunion_switch_types_signed_values) {
    /* negative case labels: switch small (signed 8 bit) */
    fam f = FAM(NdrEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    static const unsigned char fmt[] = {
        FC_ENCAPSULATED_UNION, 0x10 | FC_SMALL, S16(1), S16(0x0001),
        W32(0xffffffff), W16(0x8001),     /* case -1: byte */
        W16(0xffff),
    };
    struct { signed char sw; unsigned char b; } v = { -1, 0x5a };
    nctx c; nc_client(&c, fmt, 16); memset(c.buf, 0, 16);
    ULONG used = n_marshal(&f, &c, (unsigned char *)&v, fmt, NULL);
    CHECKF(used == 2, "used %lu", used);
    EXPECT_EQ(c.buf[0], 0xff);
    EXPECT_EQ(c.buf[1], 0x5a);
    nc_done(&c);
    v.sw = 1;
    nc_client(&c, fmt, 16);
    NTRY f.ma(&c.sm, (unsigned char *)&v, fmt); NEND;
    EXPECT_EQ(n_code, 1733);
    nc_done(&c);
}

/* non-encapsulated: switch_is from the stack */
static const unsigned char f_neu[] = {
    FC_NON_ENCAPSULATED_UNION, FC_SHORT, FC_TOP_LEVEL_CONFORMANCE | FC_SHORT, 0, S16(0), S16(2),
    S16(4), S16(0x3003),
    W32(1), W16(0x8008),
    W32(2), W16(0x8006),
    W16(0xffff),
};
TEST(ndr_neunion_switch_is_from_stack_and_wire) {
    fam f = FAM(NdrNonEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    union { int l; short s; } u; u.l = 0x01020304;
    short stack[4] = { 1, 0, 0, 0 };
    nctx c; ULONG pred;
    nc_client(&c, f_neu, 64); memset(c.buf, 0, 64);
    c.sm.StackTop = (unsigned char *)stack;
    ULONG used = n_marshal(&f, &c, (unsigned char *)&u, f_neu, &pred);
    CHECKF(used == 8, "case 1 used %lu", used);
    CHECKF(pred >= 8, "pred %lu", pred);
    EXPECT_EQ(*(short *)c.buf, 1);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 0x01020304);
    nc_done(&c);
    stack[0] = 2;
    nc_client(&c, f_neu, 64); memset(c.buf, 0, 64);
    c.sm.StackTop = (unsigned char *)stack;
    used = n_marshal(&f, &c, (unsigned char *)&u, f_neu, NULL);
    CHECKF(used == 6, "case 2 used %lu", used);
    EXPECT_EQ(*(short *)c.buf, 2);
    EXPECT_EQ(*(unsigned short *)(c.buf + 4), 0x0304);
    nc_done(&c);
    stack[0] = 7;
    nc_client(&c, f_neu, 64);
    c.sm.StackTop = (unsigned char *)stack;
    NTRY f.ma(&c.sm, (unsigned char *)&u, f_neu); NEND;
    EXPECT_EQ(n_code, 1733);
    nc_done(&c);
}
TEST(ndr_neunion_unmarshal_allocates_union_size) {
    fam f = FAM(NdrNonEncapsulatedUnion); if (!fam_ok(&f)) { t_skip("exports"); return; }
    unsigned char w[8] = { 1, 0, 0, 0, 0x78, 0x56, 0x34, 0x12 };
    nctx c; nc_rx(&c, f_neu, w, 8, 1);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    sa_um(&f, &c, &p, f_neu, 0, __func__);
    CHECK(p != NULL && n_alloc_cnt == 1);
    if (p) { CHECK(*(int *)p == 0x12345678); n_free(p); }
    EXPECT_EQ(nc_used(&c), 8);
    nc_done(&c);
    unsigned char wbad[8] = { 9, 0, 0, 0 };
    nc_rx(&c, f_neu, wbad, 8, 1);
    unsigned char *q = NULL;
    NTRY f.um(&c.sm, &q, f_neu, 0); NEND;
    EXPECT_EQ(n_code, 1733);
    nc_done(&c);
}
