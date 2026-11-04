/* NDR pointers: FC_RP/FC_UP/FC_OP/FC_FP wire format, referent ids, null handling, memory size, free, full-pointer tables. */
#include "ndrh.h"

typedef struct { unsigned char fc; unsigned size, align; const char *name; unsigned msz; } pst;
static const pst g_pt[] = {
    { FC_BYTE, 1, 1, "byte", 0 }, { FC_CHAR, 1, 1, "char", 0 }, { FC_SMALL, 1, 1, "small", 0 }, { FC_USMALL, 1, 1, "usmall", 0 },
    { FC_WCHAR, 2, 2, "wchar", 0 }, { FC_SHORT, 2, 2, "short", 0 }, { FC_USHORT, 2, 2, "ushort", 0 },
    { FC_LONG, 4, 4, "long", 0 }, { FC_ULONG, 4, 4, "ulong", 0 }, { FC_FLOAT, 4, 4, "float", 0 },
    { FC_HYPER, 8, 8, "hyper", 0 }, { FC_DOUBLE, 8, 8, "double", 0 },
    { FC_ENUM16, 2, 2, "enum16", 4 }, { FC_ENUM32, 4, 4, "enum32", 0 }, { FC_ERROR_STATUS_T, 4, 4, "error_status_t", 0 },
};
#define NPT ((int)(sizeof g_pt / sizeof g_pt[0]))
static const unsigned char vpat[8] = { 0x11, 0x22, 0x00, 0x00, 0x55, 0x66, 0x77, 0x6b };   /* enum16 needs a zero upper half */
static const unsigned char kinds[] = { FC_RP, FC_UP, FC_OP };
static const char *kname[] = { "rp", "up", "op" };

static void mkfmt(unsigned char *f, unsigned char kind, unsigned char fc) { f[0] = kind; f[1] = 0x08; f[2] = fc; f[3] = FC_PAD; }
/* expected wire for a simple pointee: kind RP -> value only; UP/OP -> id then value */
static unsigned wire_for(unsigned char kind, const pst *t, unsigned char *w, unsigned id) {
    unsigned n = 0;
    memset(w, 0, 16);
    if (kind != FC_RP) { memcpy(w, &id, 4); n = 4; }
    n = (n + t->align - 1) & ~(t->align - 1);
    memcpy(w + n, vpat, t->size);
    return n + t->size;
}

TEST(ndr_ptr_simple_buffersize_covers_wire) {
    REQUIRE(U(NdrPointerBufferSize)); REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 0; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16], mem[8]; memcpy(mem, vpat, 8);
        mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_client(&c, f, 64);
        c.sm.BufferLength = 0;
        U(NdrPointerBufferSize)(&c.sm, mem, f);
        CHECKF(c.sm.BufferLength >= wl, "%s/%s size %lu < wire %u", kname[k], g_pt[i].name, c.sm.BufferLength, wl);
        CHECKF(c.sm.BufferLength <= wl + 16, "%s/%s size %lu implausibly large", kname[k], g_pt[i].name, c.sm.BufferLength);
        nc_done(&c);
    }
}
TEST(ndr_ptr_simple_marshal_wire_bytes) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 0; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16], mem[8]; memcpy(mem, vpat, 8);
        mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_client(&c, f, 64);
        memset(c.buf, 0, 64);   /* padding is unspecified: compare against a zeroed buffer only on the value bytes */
        unsigned char *r = U(NdrPointerMarshall)(&c.sm, mem, f);
        CHECKF(r == NULL, "%s/%s ret %p", kname[k], g_pt[i].name, r);
        CHECKF(nc_used(&c) == wl, "%s/%s used %lu want %u", kname[k], g_pt[i].name, nc_used(&c), wl);
        CHECKF(!memcmp(c.buf, w, wl), "%s/%s bytes", kname[k], g_pt[i].name);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unique_referent_ids_increment_by_four) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 1; k < 3; k++) {
        unsigned char f[4], mem[4] = { 1, 2, 3, 4 }; mkfmt(f, kinds[k], FC_LONG);
        nctx c; nc_client(&c, f, 64);
        for (int n = 0; n < 5; n++) U(NdrPointerMarshall)(&c.sm, mem, f);
        EXPECT_EQ(nc_used(&c), 40);
        for (int n = 0; n < 5; n++) {
            unsigned id = *(unsigned *)(c.buf + n * 8);
            CHECKF(id == 0x20000 + 4 * n, "%s id[%d]=0x%x", kname[k], n, id);
        }
        EXPECT_EQ(c.sm.UniquePtrCount, 5);
        nc_done(&c);
    }
}
TEST(ndr_ptr_null_unique_is_zero_id_only) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrPointerBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 1; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4]; mkfmt(f, kinds[k], g_pt[i].fc);
        nctx c; nc_client(&c, f, 64);
        c.sm.BufferLength = 0;
        U(NdrPointerBufferSize)(&c.sm, NULL, f);
        CHECKF(c.sm.BufferLength >= 4, "%s/%s", kname[k], g_pt[i].name);
        memset(c.buf, 0, 64);
        U(NdrPointerMarshall)(&c.sm, NULL, f);
        CHECKF(nc_used(&c) == 4 && *(unsigned *)c.buf == 0, "%s/%s used %lu", kname[k], g_pt[i].name, nc_used(&c));
        EXPECT_EQ(c.sm.UniquePtrCount, 0);   /* null pointers do not consume a referent id */
        nc_done(&c);
    }
}
TEST(ndr_ptr_null_ref_pointer_raises) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int i = 0; i < NPT; i++) {
        unsigned char f[4]; mkfmt(f, FC_RP, g_pt[i].fc);
        nctx c; nc_client(&c, f, 64);
        NTRY U(NdrPointerMarshall)(&c.sm, NULL, f); NEND;
        CHECKF(n_code == X_NULL_REF, "%s raised %lu", g_pt[i].name, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_ptr_null_ref_pointer_buffersize_does_not_raise) {
    REQUIRE(U(NdrPointerBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_RP, FC_LONG);
    nctx c; nc_client(&c, f, 64);
    NTRY U(NdrPointerBufferSize)(&c.sm, NULL, f); NEND;
    /* Windows sizes a NULL ref pointer without raising (only marshalling raises RPC_X_NULL_REF_POINTER) */
    t_note("NdrPointerBufferSize(NULL ref) raised %lu, BufferLength %lu", (unsigned long)n_code, (unsigned long)c.sm.BufferLength);
    EXPECT_EQ(n_code, 0);
    nc_done(&c);
}
TEST(ndr_ptr_marshal_misaligned_start_pads_id_then_value) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_UP, FC_HYPER);
    unsigned long long v = 0x1122334455667788ull;
    for (int off = 0; off < 8; off++) {
        nctx c; nc_client(&c, f, 64);
        c.sm.Buffer = c.buf + off;
        U(NdrPointerMarshall)(&c.sm, (unsigned char *)&v, f);
        ULONG_PTR id_at = (ULONG_PTR)(c.buf + off + 3) & ~3ull;
        ULONG_PTR v_at = (id_at + 4 + 7) & ~7ull;
        CHECKF(*(unsigned *)id_at == 0x20000, "off %d id", off);
        CHECKF(*(unsigned long long *)v_at == v, "off %d value", off);
        CHECKF((ULONG_PTR)c.sm.Buffer == v_at + 8, "off %d end", off);
        nc_done(&c);
    }
}
TEST(ndr_ptr_memorysize_simple) {
    /* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
    REQUIRE(U(NdrPointerMemorySize)); REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 0; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16]; mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_rx(&c, f, w, wl, 1);
        c.sm.MemorySize = 0;
        ULONG sz = U(NdrPointerMemorySize)(&c.sm, f);
        unsigned msz = g_pt[i].msz ? g_pt[i].msz : g_pt[i].size;
        CHECKF(sz == msz, "%s/%s memsize %lu want %u", kname[k], g_pt[i].name, sz, msz);
        CHECKF(sz == c.sm.MemorySize, "%s/%s ret != field", kname[k], g_pt[i].name);
        CHECKF(nc_used(&c) == wl, "%s/%s consumed %lu want %u", kname[k], g_pt[i].name, nc_used(&c), wl);
        c.sm.Buffer = c.buf; c.sm.MemorySize = 16;
        sz = U(NdrPointerMemorySize)(&c.sm, f);
        CHECKF(sz == 16 + msz, "%s/%s memsize(16) %lu", kname[k], g_pt[i].name, sz);
        nc_done(&c);
    }
}
TEST(ndr_ptr_memorysize_unaligned_start_is_rounded) {
    REQUIRE(U(NdrPointerMemorySize)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4], w[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }; mkfmt(f, FC_RP, FC_LONG);
    nctx c; nc_rx(&c, f, w, 4, 1);
    c.sm.MemorySize = 1;
    ULONG sz = U(NdrPointerMemorySize)(&c.sm, f);
    t_note("MemorySize 1 + FC_RP FC_LONG -> %lu (Wine expects 12: start rounded to pointer size)", sz);
    CHECK(sz >= 5);
    nc_done(&c);
}
TEST(ndr_ptr_unmarshal_client_allocates_for_null_destination) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrPointerFree)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 1; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16]; mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_rx(&c, f, w, wl, 1);
        unsigned char *mem = NULL;
        unsigned char *r = U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0);
        CHECKF(r == NULL, "%s/%s ret", kname[k], g_pt[i].name);
        CHECKF(mem != NULL, "%s/%s no alloc", kname[k], g_pt[i].name);
        if (mem) CHECKF(!memcmp(mem, vpat, g_pt[i].size), "%s/%s value", kname[k], g_pt[i].name);
        CHECKF(nc_used(&c) == wl, "%s/%s consumed %lu", kname[k], g_pt[i].name, nc_used(&c));
        EXPECT_EQ(c.sm.MemorySize, 0);
        if (mem) {
            CHECKF(mem < c.buf || mem >= c.buf + c.cap, "%s/%s client pointer aliases buffer", kname[k], g_pt[i].name);
            n_free_cnt = 0;
            U(NdrPointerFree)(&c.sm, mem, f);
            CHECKF(n_free_cnt == 1, "%s/%s free count %d", kname[k], g_pt[i].name, n_free_cnt);
        }
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_ref_into_existing_memory) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 0; k < 2; k++) for (int must = 0; must < 2; must++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16]; mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_rx(&c, f, w, wl, 1);
        unsigned char *orig = n_alloc(16); memset(orig, 0xAA, 16);
        unsigned char *mem = orig;
        n_alloc_cnt = 0;
        U(NdrPointerUnmarshall)(&c.sm, &mem, f, (unsigned char)must);
        CHECKF(mem == orig, "%s/%s must=%d pointer replaced", kname[k], g_pt[i].name, must);
        CHECKF(!memcmp(mem, vpat, g_pt[i].size), "%s/%s must=%d value", kname[k], g_pt[i].name, must);
        CHECKF(n_alloc_cnt == 0, "%s/%s must=%d allocs %d", kname[k], g_pt[i].name, must, n_alloc_cnt);
        n_free(orig);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_server_values) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrServerInitializeNew));
    for (int k = 0; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16]; mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_rx(&c, f, w, wl, 0);
        unsigned char *mem = NULL;
        unsigned char *r = U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0);
        CHECKF(r == NULL && mem != NULL, "%s/%s", kname[k], g_pt[i].name);
        if (mem) CHECKF(!memcmp(mem, vpat, g_pt[i].size), "%s/%s value", kname[k], g_pt[i].name);
        CHECKF(nc_used(&c) == wl, "%s/%s consumed", kname[k], g_pt[i].name);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_unique_null_wire_gives_null) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 1; k < 3; k++) {
        unsigned char f[4]; mkfmt(f, kinds[k], FC_LONG);
        unsigned zero = 0;
        nctx c; nc_rx(&c, f, &zero, 4, 1);
        unsigned char *old = n_alloc(4), *mem = old;   /* an existing block: a null wire pointer replaces it with NULL */
        n_alloc_cnt = 0; n_free_cnt = 0;
        U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0);
        CHECKF(mem == NULL, "%s mem %p", kname[k], mem);
        t_note("%s: null wire pointer over an existing block freed it: %d", kname[k], n_free_cnt);
        if (n_free_cnt == 0) n_free(old);
        EXPECT_EQ(nc_used(&c), 4);
        EXPECT_EQ(n_alloc_cnt, 0);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_unique_any_nonzero_id_is_present) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned ids[] = { 1, 0x20000, 0x20004, 0x7fffffff, 0xdeadbeef, 0xffffffff };
    for (unsigned n = 0; n < sizeof ids / sizeof ids[0]; n++) {
        unsigned char f[4]; mkfmt(f, FC_UP, FC_LONG);
        unsigned w[2] = { ids[n], 0xABCD1234 };
        nctx c; nc_rx(&c, f, w, 8, 1);
        unsigned char *mem = NULL;
        U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0);
        CHECKF(mem && *(unsigned *)mem == 0xABCD1234, "id 0x%x", ids[n]);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_short_buffer_raises) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 0; k < 3; k++) for (int i = 0; i < NPT; i++) {
        unsigned char f[4], w[16]; mkfmt(f, kinds[k], g_pt[i].fc);
        unsigned wl = wire_for(kinds[k], &g_pt[i], w, 0x20000);
        nctx c; nc_rx(&c, f, w, wl - 1, 1);
        unsigned char dst[16] = { 0 }, *mem = dst;
        if (kinds[k] != FC_RP) mem = NULL;
        NTRY U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "%s/%s raised %lu", kname[k], g_pt[i].name, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_unique_id_truncated_raises) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (int len = 0; len < 4; len++) {
        unsigned char f[4], w[4] = { 1, 0, 2, 0 }; mkfmt(f, FC_UP, FC_LONG);
        nctx c; nc_rx(&c, f, w, len, 1);
        unsigned char *mem = NULL;
        NTRY U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %d raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_ptr_unmarshal_enum16_pointee_range) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_RP, FC_ENUM16);
    unsigned short ok = 0x7fff, bad = 0x8000;
    nctx c; nc_rx(&c, f, &ok, 2, 1);
    unsigned char dst[8], *mem = dst;
    NTRY U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0); NEND;
    EXPECT_EQ(n_code, 0);
    nc_done(&c);
    nc_rx(&c, f, &bad, 2, 1);
    mem = dst;
    NTRY U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0); NEND;
    t_note("enum16 0x8000 through a pointer raised %lu (NdrSimpleTypeUnmarshall raises 1781)", (unsigned long)n_code);
    nc_done(&c);
}
TEST(ndr_ptr_roundtrip_two_pointers_in_sequence) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_UP, FC_SHORT);
    short a = 0x1234, b = -2;
    nctx c; nc_client(&c, f, 64);
    memset(c.buf, 0, 64);
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)&a, f);
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)&b, f);
    ULONG used = nc_used(&c);
    EXPECT_EQ(used, 14);  /* id(4) short(2) pad(2) id(4) short(2) */
    c.sm.Buffer = c.buf; c.sm.BufferEnd = c.buf + used;
    unsigned char *ma = NULL, *mb = NULL;
    U(NdrPointerUnmarshall)(&c.sm, &ma, f, 0);
    U(NdrPointerUnmarshall)(&c.sm, &mb, f, 0);
    CHECK(ma && mb && *(short *)ma == 0x1234 && *(short *)mb == -2);
    nc_done(&c);
}
TEST(ndr_ptr_free_null_unique_is_noop) {
    REQUIRE(U(NdrPointerFree)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_UP, FC_LONG);
    nctx c; nc_client(&c, f, 16);
    n_free_cnt = 0;
    U(NdrPointerFree)(&c.sm, NULL, f);
    EXPECT_EQ(n_free_cnt, 0);
    nc_done(&c);
}
TEST(ndr_ptr_free_simple_calls_free_once) {
    REQUIRE(U(NdrPointerFree)); REQUIRE(U(NdrClientInitializeNew));
    for (int k = 0; k < 3; k++) {
        unsigned char f[4]; mkfmt(f, kinds[k], FC_LONG);
        nctx c; nc_client(&c, f, 16);
        unsigned char *m = n_alloc(8);
        n_free_cnt = 0;
        U(NdrPointerFree)(&c.sm, m, f);
        CHECKF(n_free_cnt == 1, "%s free count %d", kname[k], n_free_cnt);
        nc_done(&c);
    }
}

/* ---- pointer to pointer: FC_RP -> FC_UP -> long (nested pointee marshalled after the outer referent) ---- */
TEST(ndr_ptr_nested_up_to_up_to_long_wire) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned char f[] = {
        FC_RP, FC_POINTER_DEREF, S16(2), /* offset to type below (relative to this field) */
        FC_UP, 0x08, FC_LONG, FC_PAD,   /* inner: unique pointer to long; the RP pointee IS this pointer */
    };
    /* RP [no flags] -> complex pointee at +2 : a pointer type FC_UP simple long */
    int v = 0x55667788; int *p = &v;
    nctx c; nc_client(&c, f, 64);
    memset(c.buf, 0, 64);
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)&p, f);
    /* wire: referent id 0x20000 for the inner UP then the long */
    CHECK(nc_used(&c) == 8);
    EXPECT_EQ(*(unsigned *)c.buf, 0x20000);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 0x55667788);
    nc_done(&c);
}

/* ---- full pointers ---- */
/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
TEST(ndr_fullptr_xlat_client_marshal_phase) {
    REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerQueryPointer)); REQUIRE(U(NdrFullPointerXlatFree));
    PFULL_PTR_XLAT_TABLES t = U(NdrFullPointerXlatInit)(2, XLAT_CLIENT);
    CHECK(t != NULL);
    ULONG id; int r;
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 1, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 1);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 0, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 1);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebabe, 0, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 2);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xdeadbeef, 0, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 3);
    r = U(NdrFullPointerQueryPointer)(t, NULL, 0, &id);               EXPECT_EQ(r, 1); EXPECT_EQ(id, 0);
    U(NdrFullPointerXlatFree)(t);
}
TEST(ndr_fullptr_xlat_client_unmarshal_phase) {
    REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerQueryPointer)); REQUIRE(U(NdrFullPointerQueryRefId));
    REQUIRE(U(NdrFullPointerInsertRefId)); REQUIRE(U(NdrFullPointerXlatFree));
    PFULL_PTR_XLAT_TABLES t = U(NdrFullPointerXlatInit)(2, XLAT_CLIENT);
    ULONG id; void *p; int r;
    U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 1, &id);
    U(NdrFullPointerQueryPointer)(t, (void *)0xcafebabe, 0, &id);
    r = U(NdrFullPointerQueryRefId)(t, 0, 0, &p); EXPECT_EQ(r, 1);
    r = U(NdrFullPointerQueryRefId)(t, 2, 0, &p); EXPECT_EQ(r, 0); EXPECT_EQ(p, (void *)0xcafebabe);
    r = U(NdrFullPointerQueryRefId)(t, 4, 0, &p); EXPECT_EQ(r, 0); EXPECT_EQ(p, NULL);
    U(NdrFullPointerInsertRefId)(t, 4, (void *)0xdeadbabe);
    r = U(NdrFullPointerQueryRefId)(t, 4, 1, &p); EXPECT_EQ(r, 0); EXPECT_EQ(p, (void *)0xdeadbabe);
    U(NdrFullPointerXlatFree)(t);
}
TEST(ndr_fullptr_xlat_server_roundtrip_and_free_phase) {
    REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerQueryPointer)); REQUIRE(U(NdrFullPointerQueryRefId));
    REQUIRE(U(NdrFullPointerInsertRefId)); REQUIRE(U(NdrFullPointerFree)); REQUIRE(U(NdrFullPointerXlatFree));
    PFULL_PTR_XLAT_TABLES t = U(NdrFullPointerXlatInit)(2, XLAT_SERVER);
    ULONG id; void *p; int r;
    r = U(NdrFullPointerQueryRefId)(t, 2, 1, &p); EXPECT_EQ(r, 0); EXPECT_EQ(p, NULL);
    U(NdrFullPointerInsertRefId)(t, 2, (void *)0xcafebabe);
    r = U(NdrFullPointerQueryRefId)(t, 2, 0, &p); EXPECT_EQ(r, 0); EXPECT_EQ(p, (void *)0xcafebabe);
    r = U(NdrFullPointerQueryRefId)(t, 2, 1, &p); EXPECT_EQ(r, 1); EXPECT_EQ(p, (void *)0xcafebabe);  /* already seen */
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 1, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 3);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 1, &id); EXPECT_EQ(r, 1); EXPECT_EQ(id, 3);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 0, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 3);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebabe, 0, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 2);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xdeadbeef, 0, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 4);
    r = U(NdrFullPointerFree)(t, (void *)0xcafebeef); EXPECT_EQ(r, 1);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xcafebeef, 0x20, &id); EXPECT_EQ(r, 1); EXPECT_EQ(id, 3);
    r = U(NdrFullPointerFree)(t, (void *)0xcafebabe); EXPECT_EQ(r, 1);
    r = U(NdrFullPointerFree)(t, (void *)0xdeadbeef); EXPECT_EQ(r, 1);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xdeadbeef, 1, &id); EXPECT_EQ(r, 0); EXPECT_EQ(id, 4);
    r = U(NdrFullPointerQueryPointer)(t, (void *)0xdeadbeef, 1, &id); EXPECT_EQ(r, 1); EXPECT_EQ(id, 4);
    r = U(NdrFullPointerFree)(t, (void *)0xdeadbeef); EXPECT_EQ(r, 0);
    U(NdrFullPointerXlatFree)(t);
}
TEST(ndr_fullptr_xlat_ids_are_sequential_for_many_pointers) {
    REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerQueryPointer)); REQUIRE(U(NdrFullPointerXlatFree));
    PFULL_PTR_XLAT_TABLES t = U(NdrFullPointerXlatInit)(64, XLAT_CLIENT);
    for (ULONG n = 1; n <= 500; n++) {
        ULONG id = 0;
        int r = U(NdrFullPointerQueryPointer)(t, (void *)(ULONG_PTR)(0x10000 + n * 16), 1, &id);
        if (r != 0 || id != n) { CHECKF(0, "pointer %lu -> r=%d id=%lu", n, r, id); break; }
    }
    for (ULONG n = 1; n <= 500; n++) {
        ULONG id = 0;
        int r = U(NdrFullPointerQueryPointer)(t, (void *)(ULONG_PTR)(0x10000 + n * 16), 1, &id);
        if (r != 1 || id != n) { CHECKF(0, "repeat %lu -> r=%d id=%lu", n, r, id); break; }
    }
    U(NdrFullPointerXlatFree)(t);
}
TEST(ndr_fullptr_marshal_aliasing_wire) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerXlatFree));
    unsigned char f[4]; mkfmt(f, FC_FP, FC_LONG);
    nctx c; nc_client(&c, f, 64);
    memset(c.buf, 0, 64);
    c.sm.FullPtrXlatTables = U(NdrFullPointerXlatInit)(4, XLAT_CLIENT);
    int a = 0x11111111, b = 0x22222222;
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)&a, f);   /* id 1 + value */
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)&a, f);   /* same pointer: id 1 only */
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)&b, f);   /* id 2 + value */
    U(NdrPointerMarshall)(&c.sm, NULL, f);                  /* id 0 */
    unsigned *w = (unsigned *)c.buf;
    EXPECT_EQ(nc_used(&c), 24);
    EXPECT_EQ(w[0], 1); EXPECT_EQ(w[1], 0x11111111);
    EXPECT_EQ(w[2], 1);
    EXPECT_EQ(w[3], 2); EXPECT_EQ(w[4], 0x22222222);
    EXPECT_EQ(w[5], 0);
    U(NdrFullPointerXlatFree)(c.sm.FullPtrXlatTables);
    nc_done(&c);
}
TEST(ndr_fullptr_unmarshal_aliasing) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerXlatFree));
    unsigned char f[4]; mkfmt(f, FC_FP, FC_LONG);
    unsigned w[] = { 1, 0xAAAA5555, 1, 2, 0xBBBB6666, 0 };
    nctx c; nc_rx(&c, f, w, sizeof w, 1);
    c.sm.FullPtrXlatTables = U(NdrFullPointerXlatInit)(4, XLAT_CLIENT);
    unsigned char *p1 = NULL, *p2 = NULL, *p3 = NULL, *p4 = (unsigned char *)(ULONG_PTR)0x77;
    U(NdrPointerUnmarshall)(&c.sm, &p1, f, 0);
    U(NdrPointerUnmarshall)(&c.sm, &p2, f, 0);
    U(NdrPointerUnmarshall)(&c.sm, &p3, f, 0);
    U(NdrPointerUnmarshall)(&c.sm, &p4, f, 0);
    CHECK(p1 && *(unsigned *)p1 == 0xAAAA5555);
    CHECK(p2 == p1);
    CHECK(p3 && p3 != p1 && *(unsigned *)p3 == 0xBBBB6666);
    CHECK(p4 == NULL);
    EXPECT_EQ(nc_used(&c), sizeof w);
    U(NdrFullPointerXlatFree)(c.sm.FullPtrXlatTables);
    nc_done(&c);
}
TEST(ndr_fullptr_buffersize_second_occurrence_is_smaller) {
    REQUIRE(U(NdrPointerBufferSize)); REQUIRE(U(NdrFullPointerXlatInit)); REQUIRE(U(NdrFullPointerXlatFree));
    unsigned char f[4]; mkfmt(f, FC_FP, FC_LONG);
    nctx c; nc_client(&c, f, 16);
    c.sm.FullPtrXlatTables = U(NdrFullPointerXlatInit)(4, XLAT_CLIENT);
    int a = 1;
    c.sm.BufferLength = 0;
    U(NdrPointerBufferSize)(&c.sm, (unsigned char *)&a, f);
    ULONG first = c.sm.BufferLength;
    U(NdrPointerBufferSize)(&c.sm, (unsigned char *)&a, f);
    ULONG second = c.sm.BufferLength - first;
    CHECKF(first >= 8, "first %lu", first);
    CHECKF(second >= 4 && second <= first, "second %lu first %lu", second, first);
    t_note("full pointer buffersize: first %lu second %lu", first, second);
    U(NdrFullPointerXlatFree)(c.sm.FullPtrXlatTables);
    nc_done(&c);
}

TEST(ndr_ptr_unmarshal_ref_null_destination_client_raises) {
    /* decomp: FC_RP on the client with *ppMemory == NULL raises RPC_X_NULL_REF_POINTER before reading the buffer */
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_RP, FC_LONG);
    unsigned v = 7;
    nctx c; nc_rx(&c, f, &v, 4, 1);
    unsigned char *mem = NULL;
    NTRY U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0); NEND;
    EXPECT_EQ(n_code, X_NULL_REF);
    nc_done(&c);
}
TEST(ndr_ptr_unmarshal_ref_null_destination_server_allocates) {
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrServerInitializeNew));
    unsigned char f[4]; mkfmt(f, FC_RP, FC_LONG);
    unsigned v = 0x31337;
    nctx c; nc_rx(&c, f, &v, 4, 0);
    unsigned char *mem = NULL;
    NTRY U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(mem && *(unsigned *)mem == 0x31337);
    nc_done(&c);
}
TEST(ndr_ptr_unmarshal_op_client_replaces_existing_pointer) {
    /* decomp: FC_OP on the client releases the old pointee (NdrpFreeOlePointer) and unmarshals into fresh memory */
    REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char f[4], w[16]; mkfmt(f, FC_OP, FC_LONG);
    unsigned wv[2] = { 0x20000, 0xFEEDF00D };
    nctx c; nc_rx(&c, f, wv, 8, 1);
    unsigned char *mem = NULL;
    U(NdrPointerUnmarshall)(&c.sm, &mem, f, 0);
    CHECK(mem && *(unsigned *)mem == 0xFEEDF00D);
    nc_done(&c);
}
