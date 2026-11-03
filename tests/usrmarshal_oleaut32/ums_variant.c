/* VARIANT_UserSize / UserMarshal / UserUnmarshal / UserFree (+64): scalar and BYREF scalar VTs.
 *
 * Wire layout (decomp of oleaut32 19041, NDR32 flavour), image offsets relative to the 8-aligned start:
 *   +0  u32 clSize = (end - start + 7) / 8       +4  u32 rpcReserved = 0
 *   +8  u16 vt (as in the VARIANT)               +10 u16 wReserved1  +12 u16 wReserved2  +14 u16 wReserved3
 *   +16 u32 switch = vt (for VT_ARRAY: vt & 0x6000)
 *   +20 payload (scalars, naturally aligned: 8-byte types at +24)   BYREF: +20 u32 marker, payload at +24
 * The BYREF marker is 4, except 8 (R8/CY/DATE), 0x10 (DECIMAL) and 0x18 (VARIANT).
 * NDR64: switch at +16 (u32), the payload/marker slot is at +24 (u64 marker), BYREF payload at +32. */
#include "ums.h"

typedef struct { const char *name; VARTYPE vt; ULONG psz; unsigned char pay[16]; ULONG sz32, sz64; } scase;
#define P8(a,b,c,d,e,f,g,h) { a,b,c,d,e,f,g,h }
static const scase scases[] = {
    { "EMPTY",   VT_EMPTY,   0, {0},                              20, 24 },
    { "NULL",    VT_NULL,    0, {0},                              20, 24 },
    { "I1",      VT_I1,      1, P8(0x81,0,0,0,0,0,0,0),           21, 25 },
    { "UI1",     VT_UI1,     1, P8(0xfe,0,0,0,0,0,0,0),           21, 25 },
    { "I2",      VT_I2,      2, P8(0x34,0x92,0,0,0,0,0,0),        22, 26 },
    { "UI2",     VT_UI2,     2, P8(0xcd,0xab,0,0,0,0,0,0),        22, 26 },
    { "BOOL",    VT_BOOL,    2, P8(0xff,0xff,0,0,0,0,0,0),        22, 26 },
    { "I4",      VT_I4,      4, P8(0x78,0x56,0x34,0x92,0,0,0,0),  24, 28 },
    { "UI4",     VT_UI4,     4, P8(0x78,0x56,0x34,0x92,0,0,0,0),  24, 28 },
    { "R4",      VT_R4,      4, P8(0,0,0x80,0x3f,0,0,0,0),        24, 28 },
    { "ERROR",   VT_ERROR,   4, P8(0x01,0x00,0x0a,0x80,0,0,0,0),  24, 28 },
    { "INT",     VT_INT,     4, P8(0xf0,0xff,0xff,0xff,0,0,0,0),  24, 28 },
    { "UINT",    VT_UINT,    4, P8(0x10,0x20,0x30,0x40,0,0,0,0),  24, 28 },
    { "R8",      VT_R8,      8, P8(0x18,0x2d,0x44,0x54,0xfb,0x21,0x09,0x40), 32, 32 },
    { "CY",      VT_CY,      8, P8(0x10,0x27,0,0,0,0,0,0),        32, 32 },
    { "DATE",    VT_DATE,    8, P8(0,0,0,0,0,0x80,0xe3,0x40),     32, 32 },
    { "I8",      VT_I8,      8, P8(0x88,0x77,0x66,0x55,0x44,0x33,0x22,0xf1), 32, 32 },
    { "UI8",     VT_UI8,     8, P8(0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08), 32, 32 },
    { "DECIMAL", VT_DECIMAL, 16, { 0,0, 3, 0x80, 0x11,0x22,0x33,0x44, 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08 }, 40, 40 },
};
#define NSC ARRAY_N(scases)

/* Expected allocation for the BYREF target made by the unmarshaler when the destination had no pointer */
static ULONG byref_alloc(int v64, VARTYPE vt)
{
    if (v64) return vt == VT_DECIMAL ? 16 : 8;      /* NDR64: the wire marker (8) is the size, except DECIMAL */
    switch (vt) {
    case VT_I1: case VT_UI1: case VT_I2: case VT_UI2: case VT_BOOL: case VT_I4: case VT_UI4: case VT_R4: case VT_ERROR: case VT_INT: case VT_UINT: return 4;
    case VT_R8: case VT_CY: case VT_DATE: return 8;
    case VT_DECIMAL: return 16;
    default: return 8;
    }
}

static void build(VARIANT *v, const scase *c, unsigned short r1, unsigned short r2, unsigned short r3)
{
    memset(v, 0, sizeof *v);
    if (c->vt == VT_DECIMAL) {
        memcpy((unsigned char *)v + 2, c->pay + 2, 14);
        V_VT(v) = VT_DECIMAL;
        return;
    }
    V_VT(v) = c->vt;
    v->wReserved1 = r1; v->wReserved2 = r2; v->wReserved3 = r3;
    memcpy((unsigned char *)v + 8, c->pay, 8);
    /* bytes the wire never carries: poison them */
    if (c->psz < 8 && c->vt != VT_EMPTY && c->vt != VT_NULL) memset((unsigned char *)v + 8 + c->psz, 0xaa, 8 - c->psz);
    else if (c->vt == VT_EMPTY || c->vt == VT_NULL) memset((unsigned char *)v + 8, 0, 8);
}

/* what the destination must look like after unmarshaling a by-value scalar */
static void expect_dst(VARIANT *e, const VARIANT *src, const scase *c)
{
    memset(e, 0, sizeof *e);
    if (c->vt == VT_DECIMAL) { memcpy(e, src, 16); return; }
    V_VT(e) = V_VT(src);
    e->wReserved1 = src->wReserved1; e->wReserved2 = src->wReserved2; e->wReserved3 = src->wReserved3;
    memcpy((unsigned char *)e + 8, (unsigned char *)src + 8, c->psz);
}

#define NEEDV NEED_FAM(UF_VARIANT)

static ULONG vsize(int v, ULONG ctx, ULONG start, VARIANT *var, DWORD *exc)
{
    UCtx c; ULONG s;
    uctx_init(&c, 0, ctx, USER_MARSHAL_CB_BUFFER_SIZE);
    s = u_size(&c, UF_VARIANT, v, start, var, exc);
    uctx_free(&c);
    return s;
}

/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) -- sizes of by-value scalars */
TEST(ums_variant_size_scalar_table)
{
    unsigned i; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i++) {
            VARIANT var; ULONG got, want = v ? scases[i].sz64 : scases[i].sz32;
            build(&var, &scases[i], 0x1234, 0x5678, 0x9abc);
            got = vsize(v, MSHCTX_DIFFERENTMACHINE, 0, &var, NULL);
            t_check(got == want, __FILE__, __LINE__, "VARIANT_UserSize", "v64=%d %s got %lu want %lu", v, scases[i].name, got, want);
        }
}

TEST(ums_variant_size_depends_only_on_alignment_of_start)
{
    /* the header is 8-aligned: Size(start) == align8(start) + Size(0) for every start, every context */
    unsigned i, k; ULONG st; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i++)
            for (k = 0; k < 3; k++) {
                VARIANT var; ULONG base;
                build(&var, &scases[i], 1, 2, 3);
                base = vsize(v, ums_ctxs[k], 0, &var, NULL);
                for (st = 0; st <= 17; st++) {
                    ULONG got = vsize(v, ums_ctxs[k], st, &var, NULL);
                    t_check(got == ALIGN_UP(st, 8) + base, __FILE__, __LINE__, "Size(start)", "v64=%d %s ctx %lu start %lu got %lu want %lu", v, scases[i].name, ums_ctxs[k], st, got, ALIGN_UP(st, 8) + base);
                }
            }
}

TEST(ums_variant_size_null_pointer)
{
    ULONG s; int v; NEEDV; WINE_SKIP("NULL VARIANT* crashes");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (s = 0; s < 12; s++) EQ(vsize(v, MSHCTX_LOCAL, s, NULL, NULL), s);   /* nothing, not even alignment */
}

TEST(ums_variant_size_int_ptr_types)
{
    /* VT_INT_PTR / VT_UINT_PTR are sized as 4-byte payloads */
    static const VARTYPE t[] = { VT_INT_PTR, VT_UINT_PTR };
    unsigned i; int v; NEEDV; WINE_SKIP("INT_PTR not supported");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < 2; i++) {
            VARIANT var; memset(&var, 0, sizeof var); V_VT(&var) = t[i];
            EQ(vsize(v, MSHCTX_LOCAL, 0, &var, NULL), v ? 28 : 24);
        }
}

static void check_header(const unsigned char *w, const VARIANT *src, ULONG size, int v64, const char *what)
{
    VARTYPE vt = V_VT(src);
    ULONG sw = (vt & 0x2000) ? (vt & 0x6000) : vt;
    t_check(rd32(w) == ((size + 7) >> 3), __FILE__, __LINE__, what, "clSize %lu want %lu", rd32(w), (size + 7) >> 3);
    t_check(rd32(w + 4) == 0, __FILE__, __LINE__, what, "rpcReserved %lx", rd32(w + 4));
    t_check(rd16(w + 8) == vt, __FILE__, __LINE__, what, "vt %x want %x", rd16(w + 8), vt);
    t_check(rd16(w + 10) == src->wReserved1 && rd16(w + 12) == src->wReserved2 && rd16(w + 14) == src->wReserved3, __FILE__, __LINE__, what, "wReserved1..3");
    t_check(rd32(w + 16) == sw, __FILE__, __LINE__, what, "switch %lx want %lx", rd32(w + 16), sw);
}

/* One full cycle on a by-value or BYREF VARIANT: size, marshal at `start`, model compare, unmarshal into dst. Returns end offset. */
static ULONG cycle(int v, ULONG ctx, ULONG start, VARIANT *src, VARIANT *dst, int cmp_model, const char *what)
{
    UCtx c; ULONG size, got_end; unsigned char *m, *u, exp[1024], mask[1024];
    size = vsize(v, ctx, start, src, NULL);
    uctx_init(&c, size, ctx, USER_MARSHAL_CB_MARSHALL);
    m = u_marshal(&c, UF_VARIANT, v, c.buf + start, src, NULL);
    got_end = (ULONG)(m - c.buf);
    t_check(m != NULL && got_end == size, __FILE__, __LINE__, what, "v64=%d marshal end %lu, UserSize %lu", v, got_end, size);
    CHECK(uctx_canary_ok(&c));
    if (cmp_model) {
        ULONG me;
        memset(exp, 0xcc, sizeof exp); memset(mask, 0, sizeof mask);
        me = ums_model_variant(v, src, start, exp, mask);
        t_check(me == size, __FILE__, __LINE__, what, "model end %lu, UserSize %lu", me, size);
        CHECK_WIRE(c.buf, exp, mask, size, what);
    }
    if (dst) {
        uctx_reread(&c, size, ctx);
        u = u_unmarshal(&c, UF_VARIANT, v, c.buf + start, dst, NULL);
        t_check(u == m, __FILE__, __LINE__, what, "unmarshal end %ld, marshal end %ld", (long)(u - c.buf), (long)(m - c.buf));
    }
    uctx_free(&c);
    return size;
}

TEST(ums_variant_marshal_scalar_wire)
{
    unsigned i, k; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i++)
            for (k = 0; k < 3; k++) {
                VARIANT src; build(&src, &scases[i], 0x1234, 0x5678, 0x9abc);
                cycle(v, ums_ctxs[k], 0, &src, NULL, 1, scases[i].name);
            }
}

TEST(ums_variant_marshal_scalar_at_offsets)
{
    static const unsigned pick[] = { 0, 2, 4, 7, 9, 13, 14, 15, 17 };
    unsigned i, j; ULONG st; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (j = 0; j < ARRAY_N(pick); j++)
            for (st = 0; st <= 9; st++) {
                VARIANT src; build(&src, &scases[pick[j]], 0x1111, 0x2222, 0x3333);
                cycle(v, MSHCTX_LOCAL, st, &src, NULL, 1, scases[pick[j]].name);
            }
}

TEST(ums_variant_header_fields)
{
    /* explicit header field checks incl. the 3 reserved words travelling verbatim (Wine test, extended) */
    static const unsigned short rs[][3] = { { 0, 0, 0 }, { 0x1234, 0x5678, 0x9abc }, { 0xffff, 1, 0x8000 } };
    unsigned i, j; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 2; i < 10; i++)
            for (j = 0; j < 3; j++) {
                VARIANT src; UCtx c; ULONG size; build(&src, &scases[i], rs[j][0], rs[j][1], rs[j][2]);
                size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
                uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
                u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
                check_header(c.buf, &src, size, v, scases[i].name);
                uctx_free(&c);
            }
}

TEST(ums_variant_wine_explicit_values)
{
    /* spot checks of payload placement, written without the model */
    VARIANT var; UCtx c; unsigned char *w; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        ULONG d = v ? 24 : 20;      /* payload slot for <=4-byte types */
        memset(&var, 0, sizeof var); V_VT(&var) = VT_I1; V_I1(&var) = 0x12;
        uctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &var, NULL); w = c.buf;
        EQ(w[d], 0x12); EQ(rd32(w), v ? 4 : 3);
        uctx_free(&c);

        memset(&var, 0, sizeof var); V_VT(&var) = VT_I2; V_I2(&var) = 0x1234;
        uctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &var, NULL); w = c.buf;
        EQ(rd16(w + d), 0x1234); EQ(rd32(w), v ? 4 : 3);
        uctx_free(&c);

        memset(&var, 0, sizeof var); V_VT(&var) = VT_I4; V_I4(&var) = 0x12345678;
        uctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &var, NULL); w = c.buf;
        EQ(rd32(w + d), 0x12345678); EQ(rd32(w), v ? 4 : 3);
        uctx_free(&c);

        memset(&var, 0, sizeof var); V_VT(&var) = VT_R8; V_R8(&var) = 3.1415;
        uctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &var, NULL); w = c.buf;
        CHECK(*(double *)(w + 24) == 3.1415); EQ(rd32(w), 4);
        if (!v) EQ(rd32(w + 20), 0xcccccccc);       /* alignment pad is not written */
        uctx_free(&c);

        memset(&var, 0, sizeof var); V_VT(&var) = VT_EMPTY;
        uctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        EQ(u_marshal(&c, UF_VARIANT, v, c.buf, &var, NULL) - c.buf, v ? 24 : 20);
        EQ(rd32(c.buf), 3);
        uctx_free(&c);
    }
}

TEST(ums_variant_roundtrip_scalar)
{
    unsigned i, k; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i++)
            for (k = 0; k < 3; k++) {
                VARIANT src, dst, exp;
                build(&src, &scases[i], 0x1234, 0x5678, 0x9abc);
                memset(&dst, 0, sizeof dst);
                cycle(v, ums_ctxs[k], 0, &src, &dst, 0, scases[i].name);
                expect_dst(&exp, &src, &scases[i]);
                t_check(!memcmp(&dst, &exp, sizeof dst), __FILE__, __LINE__, "roundtrip VARIANT identical (unsent bytes zero)", "v64=%d %s", v, scases[i].name);
                { UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&c, UF_VARIANT, v, &dst, NULL); uctx_free(&c); }
            }
}

TEST(ums_variant_unmarshal_at_offsets)
{
    ULONG st; unsigned i; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i += 3)
            for (st = 1; st <= 7; st++) {
                VARIANT src, dst, exp;
                build(&src, &scases[i], 7, 8, 9);
                memset(&dst, 0, sizeof dst);
                cycle(v, MSHCTX_LOCAL, st, &src, &dst, 1, scases[i].name);
                expect_dst(&exp, &src, &scases[i]);
                CHECK(!memcmp(&dst, &exp, sizeof dst));
            }
}

TEST(ums_variant_unmarshal_replaces_destination)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT src, dst, exp;
        BSTR old = SysAllocString(L"old content to be cleared");
        /* destination holds a BSTR; the unmarshaler clears it then installs the new value */
        build(&src, &scases[8], 1, 2, 3);       /* UI4 */
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_BSTR; V_BSTR(&dst) = old;
        cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 0, "BSTR->UI4");
        expect_dst(&exp, &src, &scases[8]);
        if (!t_is_wine()) CHECK(!memcmp(&dst, &exp, sizeof dst));
        else CHECK(V_VT(&dst) == VT_UI4 && V_UI4(&dst) == V_UI4(&src));
        /* destination holds an R8 (upper union bytes dirty); EMPTY wire wipes everything */
        build(&src, &scases[0], 0, 0, 0);
        memset(&dst, 0xee, sizeof dst); V_VT(&dst) = VT_R8;
        cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 0, "R8->EMPTY");
        expect_dst(&exp, &src, &scases[0]);
        if (!t_is_wine()) CHECK(!memcmp(&dst, &exp, sizeof dst));
        CHECK(V_VT(&dst) == VT_EMPTY);
        /* NULL */
        build(&src, &scases[1], 0, 0, 0);
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_I8; V_I8(&dst) = -1;
        cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 0, "I8->NULL");
        CHECK(V_VT(&dst) == VT_NULL);
        if (!t_is_wine()) CHECK(V_I8(&dst) == 0);
    }
}

/* ---- BYREF scalars ------------------------------------------------------------------------------------------ */
static ULONG marker_for(int v64, VARTYPE bt)
{
    switch (bt) {
    case VT_R8: case VT_CY: case VT_DATE: return 8;
    case VT_DECIMAL: return 0x10;
    default: return v64 ? 8 : 4;
    }
}

TEST(ums_variant_byref_scalar_wire)
{
    unsigned i; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 2; i < NSC; i++) {
            unsigned char target[16]; VARIANT src; UCtx c; ULONG size; unsigned char *w;
            if (t_is_wine() && (scases[i].vt == VT_I8 || scases[i].vt == VT_UI8)) { t_note("emulator writes marker 8 for I8 BYREF; Windows 10 writes 4 (decomp)"); continue; }
            memcpy(target, scases[i].pay, 16);
            memset(&src, 0, sizeof src);
            V_VT(&src) = scases[i].vt | VT_BYREF; V_BYREF(&src) = target;
            src.wReserved1 = 0x0101; src.wReserved2 = 0x0202; src.wReserved3 = 0x0303;
            if (scases[i].vt == VT_DECIMAL) { V_VT(&src) = VT_DECIMAL | VT_BYREF; V_BYREF(&src) = target; }
            size = cycle(v, MSHCTX_LOCAL, 0, &src, NULL, 1, scases[i].name);
            /* BYREF adds the 4/8 byte marker slot in front of the payload */
            EQ(size, (v ? 32 : 24) + (scases[i].vt == VT_DECIMAL ? 16 : scases[i].psz == 8 ? 8 : scases[i].psz));
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL); w = c.buf;
            check_header(w, &src, size, v, scases[i].name);
            if (v) EQ(rd64(w + 24), marker_for(1, scases[i].vt)); else EQ(rd32(w + 20), marker_for(0, scases[i].vt));
            CHECK(!memcmp(w + (v ? 32 : 24), target, scases[i].psz));
            uctx_free(&c);
        }
}

/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) -- BYREF reuse/allocation of the target */
TEST(ums_variant_byref_unmarshal_allocation)
{
    IMalloc *mal = NULL; unsigned i; int v; NEEDV;
    if (CoGetMalloc(MEMCTX_TASK, &mal) != S_OK || !mal) { t_skip("no task allocator"); return; }
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 2; i < NSC; i++) {
            unsigned char target[16], *mem; VARIANT src, dst; UCtx cc;
            memcpy(target, scases[i].pay, 16);
            memset(&src, 0, sizeof src); V_VT(&src) = scases[i].vt | VT_BYREF; V_BYREF(&src) = target;

            /* (a) destination without a pointer: a fresh block of the documented size is allocated */
            memset(&dst, 0, sizeof dst);
            cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 0, scases[i].name);
            CHECK(V_VT(&dst) == (scases[i].vt | VT_BYREF));
            CHECK(V_BYREF(&dst) != NULL);
            if (V_BYREF(&dst)) {
                SIZE_T sz = IMalloc_GetSize(mal, V_BYREF(&dst));
                if (!(t_is_wine() && scases[i].psz < 4)) t_check(sz == byref_alloc(v, scases[i].vt), __FILE__, __LINE__, "task alloc size of BYREF target", "v64=%d %s got %Iu want %lu", v, scases[i].name, sz, byref_alloc(v, scases[i].vt));
                CHECK(!memcmp(V_BYREF(&dst), target, scases[i].psz));
            }
            uctx_init(&cc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
            u_free(&cc, UF_VARIANT, v, &dst, NULL);
            uctx_free(&cc);

            /* (b) destination already BYREF with a pointer: memory is reused, value written through it */
            mem = IMalloc_Alloc(mal, 16);
            memset(mem, 0xdd, 16);
            memset(&dst, 0, sizeof dst); V_VT(&dst) = scases[i].vt | VT_BYREF; V_BYREF(&dst) = mem;
            cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 0, scases[i].name);
            CHECK(V_BYREF(&dst) == mem);
            CHECK(!memcmp(mem, target, scases[i].psz));
            IMalloc_Free(mal, mem);
        }
    IMalloc_Release(mal);
}

TEST(ums_variant_byref_roundtrip_preserves_reserved)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        short s = 0x1234; VARIANT src, dst;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_I2 | VT_BYREF; V_I2REF(&src) = &s;
        src.wReserved1 = 0xaaaa; src.wReserved2 = 0xbbbb; src.wReserved3 = 0xcccc;
        memset(&dst, 0, sizeof dst);
        cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 1, "I2 byref");
        CHECK(dst.wReserved1 == 0xaaaa && dst.wReserved2 == 0xbbbb && dst.wReserved3 == 0xcccc);
        CHECK(*V_I2REF(&dst) == 0x1234);
        { UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&c, UF_VARIANT, v, &dst, NULL); uctx_free(&c); }
    }
}

/* ---- BSTR-valued VARIANTs ---------------------------------------------------------------------------------- */
TEST(ums_variant_bstr_wire_and_size)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        BSTR b = SysAllocString(L"marshal test");   /* 24 bytes */
        VARIANT src; ULONG size;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_BSTR; V_BSTR(&src) = b;
        size = cycle(v, MSHCTX_DIFFERENTMACHINE, 0, &src, NULL, 1, "BSTR");
        if (!v) EQ(size, 60);                       /* Wine/Windows: 24 header+flag, 12 bstr header, 24 data */
        else EQ(size, 24 + 8 + 16 + 24);
        /* NULL BSTR: flag only */
        V_BSTR(&src) = NULL;
        size = cycle(v, MSHCTX_DIFFERENTMACHINE, 0, &src, NULL, !t_is_wine(), "NULL BSTR");
        if (!t_is_wine()) EQ(size, v ? 32 : 24);
        /* BYREF BSTR */
        V_VT(&src) = VT_BSTR | VT_BYREF; V_BSTRREF(&src) = &b;
        size = cycle(v, MSHCTX_DIFFERENTMACHINE, 0, &src, NULL, 1, "BSTR byref");
        if (!v) EQ(size, 64); else EQ(size, 32 + 8 + 16 + 24);
        { BSTR nb = NULL; V_BSTRREF(&src) = &nb;
          size = cycle(v, MSHCTX_DIFFERENTMACHINE, 0, &src, NULL, !t_is_wine(), "BSTR byref NULL");
          if (!t_is_wine()) EQ(size, v ? 40 : 28); }
        SysFreeString(b);
    }
}

TEST(ums_variant_bstr_roundtrip)
{
    static const struct { const WCHAR *s; ULONG bytes; } t[] = { { L"", 0 }, { L"x", 2 }, { L"marshal test", 24 }, { L"a\0b", 6 }, { L"odd", 5 } };
    unsigned i; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < ARRAY_N(t); i++) {
            BSTR b = SysAllocStringByteLen((const char *)t[i].s, t[i].bytes);
            VARIANT src, dst;
            memset(&src, 0, sizeof src); V_VT(&src) = VT_BSTR; V_BSTR(&src) = b;
            memset(&dst, 0, sizeof dst);
            cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 1, "bstr");
            CHECK(V_VT(&dst) == VT_BSTR);
            CHECK(V_BSTR(&dst) != NULL);
            if (V_BSTR(&dst)) {
                if (!(t_is_wine() && (t[i].bytes & 1))) EQ(SysStringByteLen(V_BSTR(&dst)), t[i].bytes);
                CHECK(!memcmp(V_BSTR(&dst), t[i].s, t[i].bytes & ~1u));
            }
            { UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&c, UF_VARIANT, v, &dst, NULL); uctx_free(&c); }
            /* UserFree releases the BSTR and zeroes the pointer */
            if (!t_is_wine()) CHECK(V_BSTR(&dst) == NULL);
            SysFreeString(b);
        }
}

TEST(ums_variant_bstr_null_roundtrip)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT src, dst;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_BSTR;
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_BSTR; V_BSTR(&dst) = SysAllocString(L"previous");
        cycle(v, MSHCTX_LOCAL, 0, &src, &dst, !t_is_wine(), "null bstr");
        CHECK(V_VT(&dst) == VT_BSTR && V_BSTR(&dst) == NULL);
    }
}

TEST(ums_variant_bstr_byref_roundtrip)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        BSTR b = SysAllocString(L"byref string"), *pp; IMalloc *mal = NULL;
        VARIANT src, dst;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_BSTR | VT_BYREF; V_BSTRREF(&src) = &b;
        memset(&dst, 0, sizeof dst);
        cycle(v, MSHCTX_LOCAL, 0, &src, &dst, 1, "bstr byref");
        CHECK(V_VT(&dst) == (VT_BSTR | VT_BYREF));
        pp = V_BSTRREF(&dst);
        CHECK(pp && *pp && !lstrcmpW(*pp, L"byref string"));
        if (CoGetMalloc(MEMCTX_TASK, &mal) == S_OK) { EQ(IMalloc_GetSize(mal, pp), 8); IMalloc_Release(mal); }
        { UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&c, UF_VARIANT, v, &dst, NULL); uctx_free(&c); }
        SysFreeString(b);
    }
}

/* ---- errors -------------------------------------------------------------------------------------------------- */
TEST(ums_variant_byref_null_pointer_raises)
{
    static const VARTYPE t[] = { VT_I4, VT_BSTR, VT_R8, VT_VARIANT, VT_DECIMAL, VT_UNKNOWN, VT_I2 | 0, VT_ARRAY | VT_I4 };
    unsigned i; int v; NEEDV; WINE_SKIP("no RPC exceptions for malformed variants");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < ARRAY_N(t); i++) {
            VARIANT var; UCtx c; DWORD exc = 0; ULONG size;
            memset(&var, 0, sizeof var); V_VT(&var) = t[i] | VT_BYREF; V_BYREF(&var) = NULL;
            vsize(v, MSHCTX_LOCAL, 0, &var, &exc);
            t_check(exc == RPC_X_NULL_REF_POINTER_, __FILE__, __LINE__, "UserSize BYREF NULL", "v64=%d vt %x exc 0x%lx", v, t[i], exc);
            uctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            exc = 0; u_marshal(&c, UF_VARIANT, v, c.buf, &var, &exc);
            t_check(exc == RPC_X_NULL_REF_POINTER_, __FILE__, __LINE__, "UserMarshal BYREF NULL", "v64=%d vt %x exc 0x%lx", v, t[i], exc);
            exc = 0; u_free(&c, UF_VARIANT, v, &var, &exc);
            t_check(exc == RPC_X_NULL_REF_POINTER_, __FILE__, __LINE__, "UserFree BYREF NULL", "v64=%d vt %x exc 0x%lx", v, t[i], exc);
            uctx_free(&c);
        }
}

TEST(ums_variant_invalid_vt_raises)
{
    static const struct { VARTYPE vt; DWORD code; const char *what; } t[] = {
        { VT_VARIANT, E_INVALIDARG_X, "VT_VARIANT not BYREF" },
        { 0x0f, DISP_E_BADVARTYPE_X, "vt 15" },
        { VT_HRESULT, DISP_E_BADVARTYPE_X, "VT_HRESULT" },
        { VT_PTR, DISP_E_BADVARTYPE_X, "VT_PTR" },
        { VT_SAFEARRAY, DISP_E_BADVARTYPE_X, "VT_SAFEARRAY" },
        { VT_CARRAY, DISP_E_BADVARTYPE_X, "VT_CARRAY" },
        { VT_USERDEFINED, DISP_E_BADVARTYPE_X, "VT_USERDEFINED" },
        { VT_LPSTR, DISP_E_BADVARTYPE_X, "VT_LPSTR" },
        { VT_LPWSTR, DISP_E_BADVARTYPE_X, "VT_LPWSTR" },
        { VT_FILETIME, DISP_E_BADVARTYPE_X, "VT_FILETIME" },
        { VT_BLOB, DISP_E_BADVARTYPE_X, "VT_BLOB" },
        { VT_I4 | VT_VECTOR, DISP_E_BADVARTYPE_X, "VT_VECTOR|I4" },
        { VT_I4 | 0x8000, DISP_E_BADVARTYPE_X, "VT_RESERVED|I4" },
        { VT_EMPTY | VT_BYREF, DISP_E_BADVARTYPE_X, "EMPTY BYREF" },
        { VT_NULL | VT_BYREF, DISP_E_BADVARTYPE_X, "NULL BYREF" },
        { VT_I4 | VT_BYREF | VT_VECTOR, DISP_E_BADVARTYPE_X, "I4 BYREF VECTOR" },
        { VT_HRESULT | VT_BYREF, DISP_E_BADVARTYPE_X, "HRESULT BYREF" },
        { 0x0fff, DISP_E_BADVARTYPE_X, "0xfff" },
    };
    unsigned i; int v; NEEDV; WINE_SKIP("no RPC exceptions for malformed variants");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < ARRAY_N(t); i++) {
            VARIANT var; UCtx c; DWORD exc = 0; unsigned char dummy[16] = { 0 };
            memset(&var, 0, sizeof var); V_VT(&var) = t[i].vt;
            if (t[i].vt & VT_BYREF) V_BYREF(&var) = dummy;
            vsize(v, MSHCTX_LOCAL, 0, &var, &exc);
            t_check(exc == t[i].code, __FILE__, __LINE__, "UserSize invalid vt", "v64=%d %s exc 0x%lx want 0x%lx", v, t[i].what, exc, t[i].code);
            uctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            exc = 0; u_marshal(&c, UF_VARIANT, v, c.buf, &var, &exc);
            t_check(exc == t[i].code, __FILE__, __LINE__, "UserMarshal invalid vt", "v64=%d %s exc 0x%lx want 0x%lx", v, t[i].what, exc, t[i].code);
            uctx_free(&c);
        }
}

TEST(ums_variant_unmarshal_truncated_buffer)
{
    int v; unsigned i; NEEDV; WINE_SKIP("no shield bounds checks");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i += 2) {
            VARIANT src; UCtx c; ULONG size, n; unsigned char *wire;
            build(&src, &scases[i], 1, 2, 3);
            size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
            wire = malloc(size); memcpy(wire, c.buf, size);
            uctx_free(&c);
            for (n = 0; n < size; n++) {
                UCtx d; VARIANT dst; DWORD exc = 0;
                memset(&dst, 0, sizeof dst);
                uctx_wire(&d, wire, n, MSHCTX_LOCAL);
                u_unmarshal(&d, UF_VARIANT, v, d.buf, &dst, &exc);
                if (!v) t_check(exc == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "truncated VARIANT -> 0x6f7", "%s len %lu/%lu exc 0x%lx", scases[i].name, n, size, exc);
                else if (exc != RPC_X_BAD_STUB_DATA_) t_note("NDR64 truncated %s len %lu/%lu exc 0x%lx", scases[i].name, n, size, exc);
                CHECK(uctx_canary_ok(&d));
                uctx_free(&d);
            }
            free(wire);
        }
}

TEST(ums_variant_unmarshal_byref_vt_mismatch)
{
    /* destination BYREF vt must equal the wire vt, else E_INVALIDARG; a BYREF destination with NULL pointer -> 0x6f4 */
    int v; NEEDV; WINE_SKIP("different checks");
    for (v = 0; v < 1; v++) {
        short s = 7; int target = 0; VARIANT src, dst; UCtx c; ULONG size; DWORD exc = 0;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_I2 | VT_BYREF; V_I2REF(&src) = &s;
        size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_I4 | VT_BYREF; V_BYREF(&dst) = &target;
        u_unmarshal(&c, UF_VARIANT, v, c.buf, &dst, &exc);
        EQ(exc, E_INVALIDARG_X);
        CHECK(target == 0);
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_I2 | VT_BYREF; V_BYREF(&dst) = NULL;
        exc = 0;
        u_unmarshal(&c, UF_VARIANT, v, c.buf, &dst, &exc);
        EQ(exc, RPC_X_NULL_REF_POINTER_);
        uctx_free(&c);
    }
}

TEST(ums_variant_free_semantics)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        UCtx c; VARIANT var; BSTR b;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        /* VT_EMPTY and plain scalars: nothing to free */
        memset(&var, 0, sizeof var);
        u_free(&c, UF_VARIANT, v, &var, NULL);
        memset(&var, 0, sizeof var); V_VT(&var) = VT_I4; V_I4(&var) = 5;
        u_free(&c, UF_VARIANT, v, &var, NULL);
        if (!t_is_wine()) CHECK(V_VT(&var) == VT_I4 && V_I4(&var) == 5);
        /* BSTR: pointer is zeroed, vt unchanged */
        b = SysAllocString(L"x");
        memset(&var, 0, sizeof var); V_VT(&var) = VT_BSTR; V_BSTR(&var) = b;
        if (!t_is_wine()) { u_free(&c, UF_VARIANT, v, &var, NULL); CHECK(V_VT(&var) == VT_BSTR); CHECK(V_BSTR(&var) == NULL); }
        else SysFreeString(b);
        /* NULL BSTR is fine */
        memset(&var, 0, sizeof var); V_VT(&var) = VT_BSTR;
        u_free(&c, UF_VARIANT, v, &var, NULL);
        if (!t_is_wine()) u_free(&c, UF_VARIANT, v, NULL, NULL);
        uctx_free(&c);
    }
}

TEST(ums_variant_cross_context_identical_wire)
{
    unsigned i; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < NSC; i++) {
            VARIANT src; UCtx a, b; ULONG size; build(&src, &scases[i], 1, 2, 3);
            size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
            uctx_init(&a, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            uctx_init(&b, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&a, UF_VARIANT, v, a.buf, &src, NULL);
            u_marshal(&b, UF_VARIANT, v, b.buf, &src, NULL);
            t_check(!memcmp(a.buf, b.buf, size), __FILE__, __LINE__, "wire independent of MSHCTX for plain data", "v64=%d %s", v, scases[i].name);
            uctx_free(&a); uctx_free(&b);
        }
}

TEST(ums_variant_ndr32_vs_ndr64_header_size)
{
    unsigned i; NEEDV;
    if (!u_have(UF_VARIANT, 1)) { t_skip("no NDR64 exports"); return; }
    for (i = 0; i < NSC; i++) {
        VARIANT src; ULONG a, b; build(&src, &scases[i], 0, 0, 0);
        a = vsize(0, MSHCTX_LOCAL, 0, &src, NULL);
        b = vsize(1, MSHCTX_LOCAL, 0, &src, NULL);
        /* NDR64 aligns the payload slot to 8: never smaller than NDR32 for the same value, at most 4 bigger (EMPTY..4-byte) */
        t_check(b >= a && b - a <= 4, __FILE__, __LINE__, "NDR64 vs NDR32 size", "%s: %lu vs %lu", scases[i].name, a, b);
    }
}

TEST(ums_variant_unmarshal_bad_callback)
{
    int v; NEEDV; WINE_SKIP("no NdrGetUserMarshalInfo");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT src, dst; UCtx c; ULONG size; DWORD exc = 0;
        build(&src, &scases[7], 0, 0, 0);
        size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
        uctx_init(&c, size + 32, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        memset(&dst, 0, sizeof dst);
        c.umcb.Signature = 0;
        u_unmarshal(&c, UF_VARIANT, v, c.buf, &dst, &exc);
        EQ(exc, ERROR_INVALID_PARAMETER);
        c.umcb.Signature = UMS_SIG;
        c.stub.Buffer = c.buf + size + 8;
        exc = 0;
        u_unmarshal(&c, UF_VARIANT, v, c.buf, &dst, &exc);
        EQ(exc, 0x6f8);
        uctx_free(&c);
    }
}
