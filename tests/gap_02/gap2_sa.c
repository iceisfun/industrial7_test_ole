/* gap_02: LPSAFEARRAY_User{Size,Marshal,Unmarshal,Free} and the *64 NDR64 siblings (oleaut32.dll), by literal export name.
 *
 * Wire facts verified in the Win10 decomp (oleaut32 LPSAFEARRAY_UserSize64 / UserMarshal64) and the sibling NDR32 routines:
 *   NDR32: p = align4(buf); *(u32*)p = (psa != NULL); NULL psa -> returns p+4.
 *          header: u32 cDims, u16 cDims, u16 fFeatures, u32 cbElements(wire), u32 cLocks|(vt<<16 if HAVEVARTYPE), u32 SF_TYPE,
 *          u32 cells, u32 pvData-lo, [IID], cDims x {u32 cElements, i32 lLbound} (last dimension first), u32 cells, data.
 *   NDR64: p = align8(buf); *(u64*)p = (psa != NULL); NULL psa -> returns p+8; every field widened to 8-aligned.
 *   Size(NULL obj) returns the start offset unchanged; Size(*obj == NULL) = align + 4 (or 8).
 *   Scalar element arrays: Size = align(start) + w + hdr + cDims*8 (+4, NDR32; align8 + 8 for NDR64) + cells*cbElements, where
 *   hdr = 0x28 (NDR32) / 0x2c (NDR64) and w = 4 / 8. The NDR64 value is exactly what UserMarshal64 writes; the NDR32 value is an
 *   over-estimate by 12 bytes. */
#include "gap2.h"

typedef struct { const char *size, *mar, *unm, *fre; } safam;
static const safam fams[2] = {
    { "LPSAFEARRAY_UserSize", "LPSAFEARRAY_UserMarshal", "LPSAFEARRAY_UserUnmarshal", "LPSAFEARRAY_UserFree" },
    { "LPSAFEARRAY_UserSize64", "LPSAFEARRAY_UserMarshal64", "LPSAFEARRAY_UserUnmarshal64", "LPSAFEARRAY_UserFree64" },
};
typedef struct { const void *size, *mar, *unm, *fre; int w64; } sav;
static sav V[2];
static void build(void)
{
    int w;
    for (w = 0; w < 2; w++) {
        V[w].w64 = w;
        V[w].size = t_proc_in("oleaut32.dll", fams[w].size);
        V[w].mar = t_proc_in("oleaut32.dll", fams[w].mar);
        V[w].unm = t_proc_in("oleaut32.dll", fams[w].unm);
        V[w].fre = t_proc_in("oleaut32.dll", fams[w].fre);
    }
}
#define FOR_SA(v) build(); for (sav *v = V; v < V + 2; v++) if (v->size && v->mar && v->unm && v->fre)
#define W(v) ((v)->w64 ? 8 : 4)

#define SAC U(SafeArrayCreate)
#define SAD U(SafeArrayDestroy)

typedef struct { VARTYPE vt; ULONG sf, cb; const char *name; } sty;
static const sty stys[] = {
    { VT_I1, 0x10, 1, "I1" }, { VT_UI1, 0x10, 1, "UI1" }, { VT_I2, 2, 2, "I2" }, { VT_UI2, 2, 2, "UI2" }, { VT_BOOL, 2, 2, "BOOL" },
    { VT_I4, 3, 4, "I4" }, { VT_UI4, 3, 4, "UI4" }, { VT_R4, 3, 4, "R4" }, { VT_INT, 3, 4, "INT" }, { VT_UINT, 3, 4, "UINT" }, { VT_ERROR, 3, 4, "ERROR" },
    { VT_R8, 0x14, 8, "R8" }, { VT_CY, 0x14, 8, "CY" }, { VT_DATE, 0x14, 8, "DATE" }, { VT_I8, 0x14, 8, "I8" }, { VT_UI8, 0x14, 8, "UI8" },
};
typedef struct { const char *name; int dims; SAFEARRAYBOUND b[3]; } shp;
static const shp shps[] = {
    { "1d", 1, { { 10, 5 } } }, { "1d-one", 1, { { 1, 0 } } }, { "1d-neg", 1, { { 7, -3 } } },
    { "2d", 2, { { 10, 5 }, { 2, 1 } } }, { "3d", 3, { { 4, 10 }, { 2, 0 }, { 3, -2 } } },
};

static ULONG cells(const SAFEARRAY *p) { ULONG n = 1; USHORT i; for (i = 0; i < p->cDims; i++) n *= p->rgsabound[i].cElements; return n; }
static SAFEARRAY *mk(const sty *t, const shp *s)
{
    SAFEARRAY *p = SAC(t->vt, s->dims, (SAFEARRAYBOUND *)s->b);
    if (p) { ULONG i, n = cells(p) * p->cbElements; for (i = 0; i < n; i++) ((unsigned char *)p->pvData)[i] = (unsigned char)(i * 13 + 5); }
    return p;
}
static ULONG want_size(int w64, const SAFEARRAY *p, const sty *t, ULONG start)
{
    ULONG s = G2_ALIGN(start, w64 ? 8 : 4) + (w64 ? 8 : 4);
    if (!p) return s;
    s += w64 ? 0x2c : 0x28;
    s += w64 ? p->cDims * 8 : p->cDims * 8 + 4;
    if (w64) { s = G2_ALIGN(s, 8); s += 8; }
    if (t->sf == 0x14) s = G2_ALIGN(s, 8);
    return s + p->cbElements * cells(p);
}
static ULONG do_size(const sav *v, ULONG ctx, ULONG start, SAFEARRAY **pp, DWORD *x)
{
    G2Ctx c; ULONG r; DWORD e;
    g2_init(&c, 0, ctx, USER_MARSHAL_CB_BUFFER_SIZE);
    e = g2_x_size(v->size, G2F(&c), start, pp, &r);
    if (x) *x = e; else XEQ(e, 0);
    g2_free(&c);
    return r;
}
#define NEEDSA(v) if (!(v)->size) { t_note("no %s", (v)->w64 ? "NDR64 exports" : "NDR32 exports"); continue; }

#define WINE_ONLY_SKIP(why) do { if (t_is_wine()) { t_skip("emulator differs: " why); return; } } while (0)

TEST(gap2_sa_exports_in_oleaut32)
{
    build();
    {
        int w;
        for (w = 0; w < 2; w++) {
            if (t_is_wine() && !V[w].size) { t_note("emulator lacks %s", fams[w].size); continue; }
            CHECKF(V[w].size != NULL, "%s", fams[w].size);
            CHECKF(V[w].mar != NULL, "%s", fams[w].mar);
            CHECKF(V[w].unm != NULL, "%s", fams[w].unm);
            CHECKF(V[w].fre != NULL, "%s", fams[w].fre);
        }
    }
    if (t_proc_in("ole32.dll", "LPSAFEARRAY_UserSize")) t_note("ole32.dll also exports LPSAFEARRAY_UserSize");
}

TEST(gap2_sa_size_null_object)
{
    WINE_ONLY_SKIP("NULL object crashes");
    FOR_SA(v) {
        ULONG s, ctx;
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (s = 0; s < 17; s++) EQ64(do_size(v, ctx, s, NULL, NULL), s);
    }
}
TEST(gap2_sa_size_null_array)
{
    FOR_SA(v) {
        ULONG s; SAFEARRAY *nul = NULL;
        for (s = 0; s < 17; s++) EQ64(do_size(v, C_LOCAL, s, &nul, NULL), G2_ALIGN(s, W(v)) + W(v));
    }
}
TEST(gap2_sa_size_scalar_table)
{
    FOR_SA(v) {
        unsigned i, j; static const ULONG starts[] = { 0, 1, 3, 4, 5, 9, 12 };
        for (i = 0; i < ARRAY_N(stys); i++)
            for (j = 0; j < ARRAY_N(shps); j++) {
                SAFEARRAY *p = mk(&stys[i], &shps[j]); unsigned k;
                if (!p) { t_note("cannot create %s/%s", stys[i].name, shps[j].name); continue; }
                for (k = 0; k < ARRAY_N(starts); k++) {
                    ULONG got = do_size(v, k & 1 ? C_LOCAL : C_DIFFM, starts[k], &p, NULL), want = want_size(v->w64, p, &stys[i], starts[k]);
                    if (t_is_wine()) { CHECKF(got > starts[k] && got <= want, "emulator bound %lu vs %lu", got, want); continue; }
                    CHECKF(got == want, "%s w64=%d %s/%s start %lu got %lu want %lu", fams[v->w64].size, v->w64, stys[i].name, shps[j].name, starts[k], got, want);
                }
                SAD(p);
            }
    }
}
TEST(gap2_sa_size_is_context_independent)
{
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 5, 0 }; SAFEARRAY *p = SAC(VT_I4, 1, &sb); ULONG ctx, ref;
        if (!p) continue;
        ref = do_size(v, C_LOCAL, 0, &p, NULL);
        for (ctx = 0; ctx <= C_CONT; ctx++) EQ64(do_size(v, ctx, 0, &p, NULL), ref);
        SAD(p);
    }
}
TEST(gap2_sa_size_grows_by_element_bytes)
{
    FOR_SA(v) {
        ULONG n, prev = 0;
        for (n = 1; n <= 16; n++) {
            SAFEARRAYBOUND sb; SAFEARRAY *p; ULONG s;
            sb.cElements = n; sb.lLbound = 0;
            p = SAC(VT_I2, 1, &sb);
            if (!p) continue;
            s = do_size(v, C_LOCAL, 0, &p, NULL);
            if (n > 1) EQ64(s - prev, 2);
            prev = s;
            SAD(p);
        }
    }
}
TEST(gap2_sa_size_zero_element_array)
{
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 0, 0 }; SAFEARRAY *p = SAC(VT_I4, 1, &sb);
        if (!p) { t_note("zero-element array not creatable"); continue; }
        if (!t_is_wine()) EQ64(do_size(v, C_LOCAL, 0, &p, NULL), want_size(v->w64, p, &stys[5], 0));
        SAD(p);
    }
}

TEST(gap2_sa_marshal_null_object_returns_buffer)
{
    WINE_ONLY_SKIP("NULL object crashes");
    FOR_SA(v) {
        ULONG off;
        for (off = 0; off < 9; off++) {
            G2Ctx c; unsigned char *e;
            g2_init(&c, 32, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, NULL, &e), 0);
            CHECKF(e == c.buf + off, "w64=%d off %lu returned %+ld", v->w64, off, (long)(e - c.buf - off));
            CHECK(g2_untouched(&c, 0, 32));
            g2_free(&c);
        }
    }
}
TEST(gap2_sa_marshal_null_array_flag)
{
    FOR_SA(v) {
        ULONG off;
        for (off = 0; off < 17; off++) {
            G2Ctx c; unsigned char *e; SAFEARRAY *nul = NULL; ULONG a = G2_ALIGN(off, W(v));
            g2_init(&c, 40, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, &nul, &e), 0);
            CHECKF(e == c.buf + a + W(v), "w64=%d off %lu end %+ld", v->w64, off, (long)(e - c.buf));
            if (v->w64) EQ64(g2_rd64(c.buf + a), 0); else EQ64(g2_rd32(c.buf + a), 0);
            CHECK(g2_untouched(&c, 0, a));
            CHECK(g2_untouched(&c, a + W(v), 40));
            g2_free(&c);
        }
    }
}
/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) check_safearray(): header fields, widened to a type table */
TEST(gap2_sa_marshal_header_fields)
{
    FOR_SA(v) {
        unsigned i, j;
        for (i = 0; i < ARRAY_N(stys); i++)
            for (j = 0; j < ARRAY_N(shps); j++) {
                SAFEARRAY *p; G2Ctx c; ULONG sz, n, d; unsigned char *e, *w; const char *nm = stys[i].name;
                if (t_is_wine() && stys[i].vt == VT_ERROR) continue;     /* emulator uses SF_ERROR (10), Windows derives 3 from cbElements */
                p = mk(&stys[i], &shps[j]);
                if (!p) continue;
                n = cells(p);
                sz = do_size(v, C_DIFFM, 0, &p, NULL);
                g2_init(&c, sz, C_DIFFM, USER_MARSHAL_CB_MARSHALL);
                XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &p, &e), 0);
                w = c.buf;
                if (!v->w64) {
                    ULONG hdr = 36 + 8 * p->cDims, dat = stys[i].sf == 0x14 ? G2_ALIGN(hdr, 8) : hdr;
                    EQ64(g2_rd32(w), 1);
                    EQ64(g2_rd32(w + 4), p->cDims);
                    EQ64(g2_rd16(w + 8), p->cDims);
                    EQ64(g2_rd16(w + 10), p->fFeatures);
                    EQ64(g2_rd32(w + 12), stys[i].cb);
                    EQ64(g2_rd16(w + 16), p->cLocks);
                    EQ64(g2_rd16(w + 18), stys[i].vt);
                    EQ64(g2_rd32(w + 20), stys[i].sf);
                    EQ64(g2_rd32(w + 24), n);
                    for (d = 0; d < p->cDims; d++) {   /* last dimension first */
                        EQ64(g2_rd32(w + 32 + 8 * d), p->rgsabound[p->cDims - 1 - d].cElements);
                        EQ64(g2_rd32(w + 36 + 8 * d), (ULONG)p->rgsabound[p->cDims - 1 - d].lLbound);
                    }
                    EQ64(g2_rd32(w + 32 + 8 * p->cDims), n);
                    CHECKF(!memcmp(w + dat, p->pvData, n * stys[i].cb), "%s/%s element bytes", nm, shps[j].name);
                    EQ64((ULONG)(e - w), dat + n * stys[i].cb);
                } else {
                    ULONG hdr = 56 + 8 * p->cDims + 8;
                    EQ64(g2_rd64(w), 1);
                    EQ64(g2_rd64(w + 8), p->cDims);
                    EQ64(g2_rd16(w + 16), p->cDims);
                    EQ64(g2_rd16(w + 18), p->fFeatures);
                    EQ64(g2_rd32(w + 20), stys[i].cb);
                    EQ64(g2_rd16(w + 24), p->cLocks);
                    EQ64(g2_rd16(w + 26), stys[i].vt);
                    EQ64(g2_rd32(w + 32), stys[i].sf);
                    EQ64(g2_rd64(w + 40), n);
                    for (d = 0; d < p->cDims; d++) {
                        EQ64(g2_rd32(w + 56 + 8 * d), p->rgsabound[p->cDims - 1 - d].cElements);
                        EQ64(g2_rd32(w + 60 + 8 * d), (ULONG)p->rgsabound[p->cDims - 1 - d].lLbound);
                    }
                    EQ64(g2_rd64(w + 56 + 8 * p->cDims), n);
                    CHECKF(!memcmp(w + hdr, p->pvData, n * stys[i].cb), "%s/%s element bytes", nm, shps[j].name);
                    EQ64((ULONG)(e - w), hdr + n * stys[i].cb);
                    EQ64((ULONG)(e - w), sz);           /* NDR64 size is exact */
                }
                CHECKF(g2_canary_ok(&c), "%s/%s overran its buffer", nm, shps[j].name);
                CHECKF((ULONG)(e - w) <= sz, "%s/%s wrote %ld > size %lu", nm, shps[j].name, (long)(e - w), sz);
                g2_free(&c);
                SAD(p);
            }
    }
}
TEST(gap2_sa_marshal_alignment_of_start)
{
    FOR_SA(v) {
        ULONG off;
        for (off = 0; off < 17; off++) {
            SAFEARRAYBOUND sb = { 3, 1 }; SAFEARRAY *p = SAC(VT_I4, 1, &sb); G2Ctx c; ULONG a = G2_ALIGN(off, W(v)), sz; unsigned char *e;
            if (!p) continue;
            *(int *)p->pvData = 0x11223344;
            sz = do_size(v, C_LOCAL, off, &p, NULL);
            g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, &p, &e), 0);
            if (v->w64) EQ64(g2_rd64(c.buf + a), 1); else EQ64(g2_rd32(c.buf + a), 1);
            CHECK(g2_untouched(&c, 0, a));              /* padding is never written */
            CHECKF((ULONG)(e - c.buf) <= sz, "end %ld size %lu", (long)(e - c.buf), sz);
            CHECK(g2_canary_ok(&c));
            g2_free(&c); SAD(p);
        }
    }
}
TEST(gap2_sa_marshal_keeps_locks_and_source_intact)
{
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 4, 0 }; SAFEARRAY *p = SAC(VT_I4, 1, &sb); G2Ctx c; ULONG sz; unsigned char *e, *data; SAFEARRAY copy;
        if (!p) continue;
        p->cLocks = 5; data = p->pvData; memcpy(&copy, p, sizeof copy);
        sz = do_size(v, C_LOCAL, 0, &p, NULL);
        g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
        XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &p, &e), 0);
        EQ64(p->cLocks, 5);
        CHECK(p->pvData == data);
        CHECK(!memcmp(&copy, p, sizeof copy));
        EQ64(g2_rd16(c.buf + (v->w64 ? 24 : 16)), 5);       /* cLocks is transmitted */
        p->cLocks = 0;
        g2_free(&c); SAD(p);
    }
}

TEST(gap2_sa_roundtrip_scalars)
{
    FOR_SA(v) {
        unsigned i, j; ULONG ctx;
        for (i = 0; i < ARRAY_N(stys); i++)
            for (j = 0; j < ARRAY_N(shps); j++)
                for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2) {
                    SAFEARRAY *p, *d = NULL; G2Ctx c; ULONG sz, n; unsigned char *e1, *e2; USHORT k;
                    if (t_is_wine() && stys[i].vt == VT_ERROR) continue;
                    p = mk(&stys[i], &shps[j]);
                    if (!p) continue;
                    n = cells(p);
                    sz = do_size(v, ctx, 1, &p, NULL);
                    g2_init(&c, sz, ctx, USER_MARSHAL_CB_MARSHALL);
                    XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + 1, &p, &e1), 0);
                    g2_reread(&c, sz, ctx);
                    XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 1, &d, &e2), 0);
                    CHECK(e1 == e2);
                    CHECKF(d != NULL, "%s/%s no array", stys[i].name, shps[j].name);
                    if (d) {
                        VARTYPE vt = 0;
                        EQ64(d->cDims, p->cDims);
                        for (k = 0; k < p->cDims; k++) {
                            EQ64(d->rgsabound[k].cElements, p->rgsabound[k].cElements);
                            EQ64((ULONG)d->rgsabound[k].lLbound, (ULONG)p->rgsabound[k].lLbound);
                        }
                        EQ64(d->cbElements, p->cbElements);
                        EQ64(d->cLocks, 0);
                        CHECK(d->pvData != NULL && d->pvData != p->pvData);
                        CHECKF(d->pvData && !memcmp(d->pvData, p->pvData, n * p->cbElements), "%s/%s data", stys[i].name, shps[j].name);
                        if (U(SafeArrayGetVartype)(d, &vt) == S_OK) EQ64(vt, stys[i].vt);
                        SAD(d);
                    }
                    g2_free(&c); SAD(p);
                }
    }
}
TEST(gap2_sa_roundtrip_null_array)
{
    FOR_SA(v) {
        ULONG off;
        for (off = 0; off < 9; off++) {
            SAFEARRAY *src = NULL, *dst = NULL; G2Ctx c; unsigned char *e1, *e2; ULONG a = G2_ALIGN(off, W(v));
            g2_init(&c, 32, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, &src, &e1), 0);
            g2_reread(&c, (ULONG)(e1 - c.buf), C_LOCAL);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + off, &dst, &e2), 0);
            CHECK(dst == NULL);
            CHECK(e2 == e1); CHECK(e2 == c.buf + a + W(v));
            g2_free(&c);
        }
    }
}
TEST(gap2_sa_unmarshal_reuses_matching_destination)
{
    FOR_SA(v) {
        SAFEARRAYBOUND sb[2] = { { 6, 2 }, { 3, 0 } };
        SAFEARRAY *src = SAC(VT_I4, 2, sb), *dst = SAC(VT_I4, 2, sb), *orig = dst; void *data;
        G2Ctx c; ULONG sz; unsigned char *e;
        if (!src || !dst) continue;
        memset(src->pvData, 0x5a, 18 * 4); memset(dst->pvData, 0, 18 * 4); data = dst->pvData;
        sz = do_size(v, C_LOCAL, 0, &src, NULL);
        g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
        XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &src, &e), 0);
        g2_reread(&c, sz, C_LOCAL);
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &dst, &e), 0);
        CHECK(dst == orig);
        CHECK(dst->pvData == data);
        CHECK(!memcmp(dst->pvData, src->pvData, 18 * 4));
        g2_free(&c); SAD(src); SAD(dst);
    }
}
TEST(gap2_sa_unmarshal_truncated)
{
    WINE_ONLY_SKIP("no bounds checks");
    FOR_SA(v) {
        static const int pick[] = { 0, 2, 5, 11 };
        unsigned i;
        for (i = 0; i < ARRAY_N(pick); i++) {
            SAFEARRAYBOUND sb = { 3, 1 }; SAFEARRAY *p = SAC(stys[pick[i]].vt, 1, &sb); G2Ctx c; ULONG sz, n; unsigned char *m, *wire;
            if (!p) continue;
            memset(p->pvData, 0x33, 3 * p->cbElements);
            sz = do_size(v, C_LOCAL, 0, &p, NULL);
            g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &p, &m), 0);
            sz = (ULONG)(m - c.buf);
            wire = malloc(sz); memcpy(wire, c.buf, sz); g2_free(&c);
            for (n = 0; n < sz; n++) {
                G2Ctx d; SAFEARRAY *dst = NULL; unsigned char *e; DWORD x;
                g2_init(&d, n, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
                memcpy(d.buf, wire, n);
                x = g2_x_buf(v->unm, G2F(&d), d.buf, &dst, &e);
                if (!v->w64) CHECKF(x == X_BADSTUB, "%s len %lu/%lu exc 0x%lx", stys[pick[i]].name, n, sz, x);
                else { CHECKF(x != 0, "%s NDR64 len %lu/%lu no exception", stys[pick[i]].name, n, sz); if (x != X_BADSTUB) t_note("NDR64 truncated %s len %lu/%lu exc 0x%lx", stys[pick[i]].name, n, sz, x); }
                CHECK(g2_canary_ok(&d));
                if (!x && dst) SAD(dst);
                g2_free(&d);
            }
            free(wire); SAD(p);
        }
    }
}
TEST(gap2_sa_unmarshal_handbuilt_ndr32)
{
    unsigned char w[96]; G2Ctx c; SAFEARRAY *dst = NULL; unsigned char *e; DWORD x;
    build();
    if (!V[0].unm) { t_skip("no NDR32 exports"); return; }
    memset(w, 0, sizeof w);
    g2_wr32(w + 0, 1); g2_wr32(w + 4, 1); g2_wr16(w + 8, 1); g2_wr16(w + 10, FADF_HAVEVARTYPE); g2_wr32(w + 12, 4);
    g2_wr32(w + 16, (ULONG)VT_I4 << 16); g2_wr32(w + 20, 3); g2_wr32(w + 24, 2); g2_wr32(w + 28, 0x1000);
    g2_wr32(w + 32, 2); g2_wr32(w + 36, (ULONG)-5);
    g2_wr32(w + 40, 2); g2_wr32(w + 44, 111); g2_wr32(w + 48, 222);
    g2_init(&c, 52, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
    memcpy(c.buf, w, 52);
    x = g2_x_buf(V[0].unm, G2F(&c), c.buf, &dst, &e);
    EQ64(x, 0);
    CHECK(e == c.buf + 52);
    CHECK(dst != NULL);
    if (dst) {
        LONG lb = 0, ub = 0; VARTYPE vt = 0;
        EQ64(dst->cDims, 1); EQ64(dst->cbElements, 4);
        EQ64(((int *)dst->pvData)[0], 111); EQ64(((int *)dst->pvData)[1], 222);
        U(SafeArrayGetLBound)(dst, 1, &lb); U(SafeArrayGetUBound)(dst, 1, &ub);
        EQ64((ULONG)lb, (ULONG)-5); EQ64((ULONG)ub, (ULONG)-4);
        if (U(SafeArrayGetVartype)(dst, &vt) == S_OK) EQ64(vt, VT_I4);
        SAD(dst);
    }
    g2_free(&c);
    /* a wire image with the NULL flag: result is a NULL array */
    g2_init(&c, 8, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
    g2_wr32(c.buf, 0);
    dst = NULL;
    XEQ(g2_x_buf(V[0].unm, G2F(&c), c.buf, &dst, &e), 0);
    CHECK(dst == NULL); CHECK(e == c.buf + 4);
    g2_free(&c);
}
TEST(gap2_sa_unmarshal_bad_callback)
{
    WINE_ONLY_SKIP("no NdrGetUserMarshalInfo");
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 3, 0 }; SAFEARRAY *src = SAC(VT_I4, 1, &sb), *dst = NULL; G2Ctx c; ULONG sz; unsigned char *e; DWORD x;
        if (!src) continue;
        sz = do_size(v, C_LOCAL, 0, &src, NULL);
        g2_init(&c, sz + 16, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
        XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &src, &e), 0);
        g2_reread(&c, sz, C_LOCAL);
        c.umcb.Signature = 0xdeadbeef;
        x = g2_x_buf(v->unm, G2F(&c), c.buf, &dst, &e);
        EQ64(x, ERROR_INVALID_PARAMETER);
        c.umcb.Signature = G2_SIG;
        c.stub.Buffer = c.buf + sz + 4;
        x = g2_x_buf(v->unm, G2F(&c), c.buf, &dst, &e);
        EQ64(x, X_BADBUFPOS);
        g2_free(&c); SAD(src);
    }
}

TEST(gap2_sa_free_null_array_is_noop)
{
    FOR_SA(v) {
        SAFEARRAY *nul = NULL; G2Ctx c;
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_FREE);
        XEQ(g2_x_free(v->fre, G2F(&c), &nul), 0);
        CHECK(nul == NULL);
        g2_free(&c);
    }
}
TEST(gap2_sa_free_destroys_and_clears_pointer)
{
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 4, 0 }; SAFEARRAY *p = SAC(VT_BSTR, 1, &sb); G2Ctx c;
        BSTR *e;
        if (!p) continue;
        e = p->pvData;
        e[0] = U(SysAllocString)(L"free me"); e[3] = U(SysAllocString)(L"and me");
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_FREE);
        XEQ(g2_x_free(v->fre, G2F(&c), &p), 0);
        CHECK(p == NULL);
        g2_free(&c);
    }
}
TEST(gap2_sa_free_locked_array_raises)
{
    WINE_ONLY_SKIP("emulator does not raise");
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 4, 0 }; SAFEARRAY *p = SAC(VT_I4, 1, &sb); G2Ctx c;
        if (!p) continue;
        U(SafeArrayLock)(p);
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_FREE);
        XEQ(g2_x_free(v->fre, G2F(&c), &p), X_ARRAYLOCKED);
        CHECK(p != NULL);
        U(SafeArrayUnlock)(p);
        XEQ(g2_x_free(v->fre, G2F(&c), &p), 0);
        CHECK(p == NULL);
        g2_free(&c);
    }
}
TEST(gap2_sa_free_after_unmarshal_releases_everything)
{
    FOR_SA(v) {
        SAFEARRAYBOUND sb = { 5, 0 }; SAFEARRAY *src = SAC(VT_R8, 1, &sb), *dst = NULL; G2Ctx c; ULONG sz; unsigned char *e;
        if (!src) continue;
        sz = do_size(v, C_LOCAL, 0, &src, NULL);
        g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
        XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &src, &e), 0);
        g2_reread(&c, sz, C_LOCAL);
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &dst, &e), 0);
        CHECK(dst != NULL);
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_FREE);
        XEQ(g2_x_free(v->fre, G2F(&c), &dst), 0);
        CHECK(dst == NULL);
        g2_free(&c); SAD(src);
    }
}
TEST(gap2_sa_ndr32_and_ndr64_agree_on_content)
{
    build();
    if (!V[0].size || !V[1].size) { t_skip("need both widths"); return; }
    {
        SAFEARRAYBOUND sb[2] = { { 4, 3 }, { 2, -1 } }; SAFEARRAY *src = SAC(VT_UI2, 2, sb), *d[2] = { NULL, NULL }; int w;
        if (!src) return;
        { ULONG i; for (i = 0; i < 8; i++) ((USHORT *)src->pvData)[i] = (USHORT)(0xa000 + i); }
        for (w = 0; w < 2; w++) {
            G2Ctx c; ULONG sz = do_size(&V[w], C_LOCAL, 0, &src, NULL); unsigned char *e;
            g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(V[w].mar, G2F(&c), c.buf, &src, &e), 0);
            g2_reread(&c, sz, C_LOCAL);
            XEQ(g2_x_buf(V[w].unm, G2F(&c), c.buf, &d[w], &e), 0);
            g2_free(&c);
        }
        if (d[0] && d[1]) {
            CHECK(d[0]->cDims == d[1]->cDims && d[0]->fFeatures == d[1]->fFeatures && d[0]->cbElements == d[1]->cbElements);
            CHECK(!memcmp(d[0]->rgsabound, d[1]->rgsabound, 2 * sizeof(SAFEARRAYBOUND)));
            CHECK(!memcmp(d[0]->pvData, d[1]->pvData, 16));
        } else CHECK(0);
        if (d[0]) SAD(d[0]);
        if (d[1]) SAD(d[1]);
        SAD(src);
    }
}
