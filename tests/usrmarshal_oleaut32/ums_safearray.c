/* LPSAFEARRAY_UserSize / UserMarshal / UserUnmarshal / UserFree (+64).
 *
 * NDR32 wire image (decomp of oleaut32 19041; q = buffer pointer rounded up to 4):
 *   q+0  u32 non-NULL flag (1)              q+4  u32 cDims            q+8  u16 cDims    q+10 u16 fFeatures
 *   q+12 u32 element wire size (4 for BSTR/IUnknown/IDispatch, 0x10 for VARIANT, else cbElements)
 *   q+16 u32 cLocks | (vartype << 16, only if FADF_HAVEVARTYPE)      q+20 u32 SF_TYPE discriminant
 *   q+24 u32 cells                           q+28 u32 low half of pvData (opaque)
 *   [16-byte IID if FADF_HAVEIID]  cDims x {u32 cElements, i32 lLbound} in REVERSE order (last dimension first)
 *   u32 cells, then the elements (8-byte types are aligned to 8; BSTR/interface/VARIANT arrays carry a u32 flag
 *   per element first and the element bodies after all flags).
 * NDR64: all fields widened/aligned to 8 (see ums_model_safearray in ums_util.c).
 * UserSize is an upper bound that over-estimates the header (+12 for NDR32, 1-D) -- Windows 10 values are exact
 * per the decompiled formula; Wine's smaller bound is only checked as >= the marshaled size. */
#include "ums.h"

#define NEEDS NEED_FAM(UF_SAFEARRAY)

/* Wine marshals BSTR/VARIANT/interface arrays with a different (non-Windows) layout */
static int wine_layout_differs(VARTYPE vt) { return t_is_wine() && (vt == VT_BSTR || vt == VT_VARIANT || vt == VT_UNKNOWN || vt == VT_DISPATCH || vt == VT_ERROR); }

/* ---- builders ------------------------------------------------------------------------------------------- */
typedef struct { const char *name; VARTYPE vt; ULONG sf, wel; } vtcase;
static const vtcase vtcases[] = {
    { "I1", VT_I1, 0x10, 1 }, { "UI1", VT_UI1, 0x10, 1 },
    { "I2", VT_I2, 2, 2 }, { "UI2", VT_UI2, 2, 2 }, { "BOOL", VT_BOOL, 2, 2 },
    { "I4", VT_I4, 3, 4 }, { "UI4", VT_UI4, 3, 4 }, { "R4", VT_R4, 3, 4 }, { "INT", VT_INT, 3, 4 }, { "UINT", VT_UINT, 3, 4 }, { "ERROR", VT_ERROR, 3, 4 },
    { "R8", VT_R8, 0x14, 8 }, { "CY", VT_CY, 0x14, 8 }, { "DATE", VT_DATE, 0x14, 8 }, { "I8", VT_I8, 0x14, 8 }, { "UI8", VT_UI8, 0x14, 8 },
    { "BSTR", VT_BSTR, 8, 4 }, { "DISPATCH", VT_DISPATCH, 9, 4 }, { "UNKNOWN", VT_UNKNOWN, 0xd, 4 }, { "VARIANT", VT_VARIANT, 0xc, 0x10 },
};
#define NVT ARRAY_N(vtcases)

typedef struct { const char *name; int dims; SAFEARRAYBOUND b[3]; } shape;
static const shape shapes[] = {
    { "1d", 1, { { 10, 5 } } },
    { "1d-single", 1, { { 1, 0 } } },
    { "1d-negative-lbound", 1, { { 7, -3 } } },
    { "2d", 2, { { 10, 5 }, { 2, 1 } } },
    { "3d", 3, { { 4, 10 }, { 2, 0 }, { 3, -2 } } },
};
#define NSH ARRAY_N(shapes)

static ULONG cells_of(const SAFEARRAY *psa) { ULONG n = 1; USHORT i; for (i = 0; i < psa->cDims; i++) n *= psa->rgsabound[i].cElements; return n; }

/* fill with a position dependent pattern; pointer-bearing element types get real content */
static void fill(SAFEARRAY *psa, VARTYPE vt)
{
    ULONG n = cells_of(psa), i;
    unsigned char *d = psa->pvData;
    if (vt == VT_BSTR) {
        for (i = 0; i < n; i++) {
            WCHAR buf[40]; ULONG j, len = i % 7;
            if (i % 5 == 4) continue;                         /* NULL element */
            for (j = 0; j < len; j++) buf[j] = (WCHAR)('a' + (i + j) % 26);
            ((BSTR *)d)[i] = SysAllocStringLen(buf, len);     /* len==0 -> empty (non-NULL) BSTR */
        }
    } else if (vt == VT_VARIANT) {
        for (i = 0; i < n; i++) {
            VARIANT *e = &((VARIANT *)d)[i];
            switch (i % 5) {
            case 0: V_VT(e) = VT_I4; V_I4(e) = 1000 + i; break;
            case 1: V_VT(e) = VT_BSTR; V_BSTR(e) = SysAllocString(L"variant element"); break;
            case 2: V_VT(e) = VT_R8; V_R8(e) = i * 1.5; break;
            case 3: V_VT(e) = VT_EMPTY; break;
            default: V_VT(e) = VT_BSTR; V_BSTR(e) = NULL; break;
            }
        }
    } else if (vt == VT_UNKNOWN || vt == VT_DISPATCH) {
        /* leave NULL */
    } else {
        for (i = 0; i < n * psa->cbElements; i++) d[i] = (unsigned char)(i * 7 + 1);
    }
}

static SAFEARRAY *mk(VARTYPE vt, const shape *sh)
{
    SAFEARRAY *psa = SafeArrayCreate(vt, sh->dims, (SAFEARRAYBOUND *)sh->b);
    if (psa) fill(psa, vt);
    return psa;
}

/* deep compare of two arrays that went through the wire */
static int bstr_eq(BSTR a, BSTR b)
{
    if (!a || !b) return a == b;
    return SysStringByteLen(a) == SysStringByteLen(b) && !memcmp(a, b, SysStringByteLen(a));
}
static void arrays_equal(SAFEARRAY *a, SAFEARRAY *b, VARTYPE vt, const char *what)
{
    ULONG i, n; USHORT d;
    if (!a || !b) { t_check(0, __FILE__, __LINE__, what, "NULL array"); return; }
    t_check(a->cDims == b->cDims, __FILE__, __LINE__, what, "cDims %u vs %u", a->cDims, b->cDims);
    if (a->cDims != b->cDims) return;
    for (d = 0; d < a->cDims; d++)
        t_check(a->rgsabound[d].cElements == b->rgsabound[d].cElements && a->rgsabound[d].lLbound == b->rgsabound[d].lLbound, __FILE__, __LINE__, what, "bound %u", d);
    t_check(a->cbElements == b->cbElements, __FILE__, __LINE__, what, "cbElements %lu vs %lu", a->cbElements, b->cbElements);
    t_check(a->fFeatures == b->fFeatures, __FILE__, __LINE__, what, "fFeatures %x vs %x", a->fFeatures, b->fFeatures);
    t_check(b->cLocks == 0, __FILE__, __LINE__, what, "unmarshaled cLocks %lu", b->cLocks);
    t_check(b->pvData != NULL, __FILE__, __LINE__, what, "pvData NULL");
    if ((a->fFeatures & FADF_HAVEVARTYPE) && !wine_layout_differs(vt)) {
        VARTYPE v1 = 0xdead, v2 = 0xbeef;
        SafeArrayGetVartype(a, &v1); SafeArrayGetVartype(b, &v2);
        t_check(v1 == v2 && v1 == vt, __FILE__, __LINE__, what, "vartype %x vs %x (want %x)", v1, v2, vt);
    }
    n = cells_of(a);
    if (!b->pvData) return;
    if (vt == VT_BSTR) {
        for (i = 0; i < n; i++) t_check(bstr_eq(((BSTR *)a->pvData)[i], ((BSTR *)b->pvData)[i]), __FILE__, __LINE__, what, "BSTR element %lu", i);
    } else if (vt == VT_VARIANT) {
        for (i = 0; i < n; i++) {
            VARIANT *x = &((VARIANT *)a->pvData)[i], *y = &((VARIANT *)b->pvData)[i];
            int ok = V_VT(x) == V_VT(y);
            if (ok) switch (V_VT(x)) {
            case VT_I4: ok = V_I4(x) == V_I4(y); break;
            case VT_R8: ok = V_R8(x) == V_R8(y); break;
            case VT_BSTR: ok = bstr_eq(V_BSTR(x), V_BSTR(y)); break;
            }
            t_check(ok, __FILE__, __LINE__, what, "VARIANT element %lu", i);
        }
    } else if (vt == VT_UNKNOWN || vt == VT_DISPATCH) {
        for (i = 0; i < n; i++) t_check(((void **)a->pvData)[i] == ((void **)b->pvData)[i], __FILE__, __LINE__, what, "iface element %lu", i);
    } else
        t_check(!memcmp(a->pvData, b->pvData, n * a->cbElements), __FILE__, __LINE__, what, "element bytes");
}

/* Full cycle: size, marshal at `start`, compare to the model, unmarshal. dst (may be NULL) receives the array. */
static ULONG sa_cycle(int v, ULONG ctx, ULONG start, SAFEARRAY *src, SAFEARRAY **pdst, int cmp_model, const char *what)
{
    UCtx c; ULONG size, predicted; unsigned char *m, *u, exp[4096], mask[4096];
    UCtx z;
    uctx_init(&z, 0, ctx, USER_MARSHAL_CB_BUFFER_SIZE);
    size = u_size(&z, UF_SAFEARRAY, v, start, &src, NULL);
    uctx_free(&z);
    predicted = ums_size_safearray(v, src, start);
    if (!t_is_wine()) t_check(size == predicted, __FILE__, __LINE__, what, "v64=%d UserSize %lu, decomp formula %lu", v, size, predicted);
    uctx_init(&c, size, ctx, USER_MARSHAL_CB_MARSHALL);
    m = u_marshal(&c, UF_SAFEARRAY, v, c.buf + start, &src, NULL);
    t_check(m != NULL && m <= c.buf + size && m >= c.buf + start, __FILE__, __LINE__, what, "v64=%d marshal end %ld > UserSize %lu", v, (long)(m - c.buf), size);
    CHECK(uctx_canary_ok(&c));
    if (cmp_model) {
        ULONG me;
        memset(exp, 0xcc, sizeof exp); memset(mask, 0, sizeof mask);
        me = ums_model_safearray(v, src, start, exp, mask);
        t_check(me == (ULONG)(m - c.buf), __FILE__, __LINE__, what, "v64=%d model end %lu, marshal end %ld", v, me, (long)(m - c.buf));
        if (me <= sizeof exp) CHECK_WIRE(c.buf, exp, mask, me, what);
    }
    if (pdst) {
        uctx_reread(&c, size, ctx);
        u = u_unmarshal(&c, UF_SAFEARRAY, v, c.buf + start, pdst, NULL);
        t_check(u == m, __FILE__, __LINE__, what, "v64=%d unmarshal end %ld marshal end %ld", v, (long)(u - c.buf), (long)(m - c.buf));
    }
    uctx_free(&c);
    return size;
}

static void sa_userfree(int v, SAFEARRAY **p)
{
    UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
    u_free(&c, UF_SAFEARRAY, v, p, NULL);
    uctx_free(&c);
}

/* ---- sizes ----------------------------------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) -- values Wine accepts as "win64" variants are exact here */
TEST(ums_sa_size_literal_values)
{
    SAFEARRAYBOUND sab = { 10, 5 };
    SAFEARRAY *i2, *r8, *nul = NULL; UCtx z; NEEDS; WINE_SKIP("smaller upper bound");
    i2 = SafeArrayCreate(VT_I2, 1, &sab); r8 = SafeArrayCreate(VT_R8, 1, &sab);
    uctx_init(&z, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
    /* NDR32: align4 + 4 (flag) + 0x28 + 8*dims + 4 + data;  44 bytes are written, 56 predicted */
    EQ(u_size(&z, UF_SAFEARRAY, 0, 0, &i2, NULL), 56 + 20);
    EQ(u_size(&z, UF_SAFEARRAY, 0, 1, &i2, NULL), 60 + 20);
    EQ(u_size(&z, UF_SAFEARRAY, 0, 0, &r8, NULL), 56 + 80);
    EQ(u_size(&z, UF_SAFEARRAY, 0, 1, &r8, NULL), 64 + 80);   /* 8-byte elements are aligned to 8 after the header */
    EQ(u_size(&z, UF_SAFEARRAY, 0, 0, &nul, NULL), 4);
    EQ(u_size(&z, UF_SAFEARRAY, 0, 1, &nul, NULL), 8);
    EQ(u_size(&z, UF_SAFEARRAY, 0, 3, &nul, NULL), 8);
    EQ(u_size(&z, UF_SAFEARRAY, 0, 4, &nul, NULL), 8);
    if (u_have(UF_SAFEARRAY, 1)) {
        EQ(u_size(&z, UF_SAFEARRAY, 1, 0, &nul, NULL), 8);
        EQ(u_size(&z, UF_SAFEARRAY, 1, 1, &nul, NULL), 16);
        EQ(u_size(&z, UF_SAFEARRAY, 1, 0, &i2, NULL), 72 + 20);
        EQ(u_size(&z, UF_SAFEARRAY, 1, 0, &r8, NULL), 72 + 80);
    }
    uctx_free(&z);
    SafeArrayDestroy(i2); SafeArrayDestroy(r8);
}

TEST(ums_sa_size_null_pointer)
{
    ULONG s; int v; NEEDS; WINE_SKIP("NULL LPSAFEARRAY* crashes");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (s = 0; s < 10; s++) {
            UCtx z; uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            EQ(u_size(&z, UF_SAFEARRAY, v, s, NULL, NULL), s);
            uctx_free(&z);
        }
}

TEST(ums_sa_size_formula_table)
{
    unsigned i, j, k; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
    for (i = 0; i < NVT; i++)
        for (j = 0; j < NSH; j++) {
            SAFEARRAY *psa = mk(vtcases[i].vt, &shapes[j]);
            if (!psa) { t_note("SafeArrayCreate(%s,%s) failed", vtcases[i].name, shapes[j].name); continue; }
            for (k = 0; k < 6; k++) {
                static const ULONG st[] = { 0, 1, 3, 4, 5, 9 };
                UCtx z; ULONG got, want = ums_size_safearray(v, psa, st[k]);
                uctx_init(&z, 0, k & 1 ? MSHCTX_LOCAL : MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
                got = u_size(&z, UF_SAFEARRAY, v, st[k], &psa, NULL);
                uctx_free(&z);
                if (!t_is_wine()) t_check(got == want, __FILE__, __LINE__, "LPSAFEARRAY_UserSize", "v64=%d %s %s start %lu got %lu want %lu", v, vtcases[i].name, shapes[j].name, st[k], got, want);
                else t_check(got > st[k], __FILE__, __LINE__, "LPSAFEARRAY_UserSize", "v64=%d %s %s start %lu got %lu", v, vtcases[i].name, shapes[j].name, st[k], got);
            }
            SafeArrayDestroy(psa);
        }
}

/* ---- wire layout ----------------------------------------------------------------------------------------- */
TEST(ums_sa_marshal_wire_table)
{
    unsigned i, j; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
    for (i = 0; i < NVT; i++) {
        if (wine_layout_differs(vtcases[i].vt)) continue;
        for (j = 0; j < NSH; j++) {
            SAFEARRAY *psa = mk(vtcases[i].vt, &shapes[j]);
            char what[96];
            if (!psa) continue;
            snprintf(what, sizeof what, "%s/%s wire", vtcases[i].name, shapes[j].name);
            sa_cycle(v, MSHCTX_DIFFERENTMACHINE, 0, psa, NULL, 1, what);
            sa_cycle(v, MSHCTX_LOCAL, 3, psa, NULL, 1, what);
            SafeArrayDestroy(psa);
        }
    }
}

TEST(ums_sa_marshal_at_offsets)
{
    static const vtcase *pick[3] = { &vtcases[2], &vtcases[11], &vtcases[16] };   /* I2, R8, BSTR */
    unsigned i; ULONG st; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < 3; i++)
            for (st = 0; st <= 9; st++) {
                SAFEARRAY *psa;
                if (wine_layout_differs(pick[i]->vt)) continue;
                psa = mk(pick[i]->vt, &shapes[3]);
                sa_cycle(v, MSHCTX_LOCAL, st, psa, NULL, 1, pick[i]->name);
                SafeArrayDestroy(psa);
            }
}

/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) check_safearray(), NDR32 */
TEST(ums_sa_ndr32_header_fields)
{
    SAFEARRAYBOUND sab[2] = { { 10, 5 }, { 2, 1 } };
    SAFEARRAY *psa; UCtx c; unsigned char *w; ULONG size; UCtx z; NEEDS;
    psa = SafeArrayCreate(VT_I2, 2, sab);
    *(DWORD *)psa->pvData = 0xcafebabe;
    psa->cLocks = 7;
    uctx_init(&z, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
    size = u_size(&z, UF_SAFEARRAY, 0, 0, &psa, NULL);
    uctx_free(&z);
    uctx_init(&c, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
    EQ(u_marshal(&c, UF_SAFEARRAY, 0, c.buf, &psa, NULL) - c.buf, 52 + 40);    /* 32 + 16 + 4 + 40 bytes of data */
    w = c.buf;
    EQ(rd32(w + 0), 1);
    EQ(rd32(w + 4), 2);                     /* cDims */
    EQ(rd16(w + 8), 2);
    EQ(rd16(w + 10), psa->fFeatures);
    EQ(rd32(w + 12), 2);                    /* cbElements */
    EQ(rd16(w + 16), 7);                    /* cLocks survives */
    EQ(rd16(w + 18), VT_I2);                /* vartype in the high word */
    EQ(rd32(w + 20), 2);                    /* SF_I2 */
    EQ(rd32(w + 24), 20);                   /* 10 * 2 cells */
    /* SafeArrayCreate stores the bounds reversed in the descriptor; the wire walks the descriptor backwards, which
       gives back the order the caller passed: (10,5) then (2,1) */
    EQ(rd32(w + 32), 10); EQ(rd32(w + 36), 5);
    EQ(rd32(w + 40), 2);  EQ(rd32(w + 44), 1);
    EQ(rd32(w + 48), 20);                   /* cell count again */
    EQ(rd32(w + 52), 0xcafebabe);           /* data follows immediately */
    EQ(psa->cLocks, 7);                     /* marshaling does not leave the array locked */
    psa->cLocks = 0;
    uctx_free(&c);
    SafeArrayDestroy(psa);
}

TEST(ums_sa_ndr64_header_fields)
{
    SAFEARRAYBOUND sab[2] = { { 10, 5 }, { 2, 1 } };
    SAFEARRAY *psa; UCtx c; unsigned char *w; ULONG size; UCtx z; NEEDS;
    if (!u_have(UF_SAFEARRAY, 1)) { t_skip("no NDR64 exports"); return; }
    psa = SafeArrayCreate(VT_I2, 2, sab);
    *(DWORD *)psa->pvData = 0xcafebabe;
    psa->cLocks = 7;
    uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
    size = u_size(&z, UF_SAFEARRAY, 1, 0, &psa, NULL);
    uctx_free(&z);
    uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
    EQ(u_marshal(&c, UF_SAFEARRAY, 1, c.buf, &psa, NULL) - c.buf, 56 + 16 + 8 + 40);
    w = c.buf;
    EQ(rd64(w + 0), 1);
    EQ(rd64(w + 8), 2);
    EQ(rd16(w + 16), 2);
    EQ(rd16(w + 18), psa->fFeatures);
    EQ(rd32(w + 20), 2);
    EQ(rd16(w + 24), 7);
    EQ(rd16(w + 26), VT_I2);
    EQ(rd32(w + 32), 2);                    /* SF_I2 */
    EQ(rd64(w + 40), 20);
    EQ(rd32(w + 56), 10); EQ(rd32(w + 60), 5);
    EQ(rd32(w + 64), 2);  EQ(rd32(w + 68), 1);
    EQ(rd64(w + 72), 20);
    EQ(rd32(w + 80), 0xcafebabe);
    psa->cLocks = 0;
    uctx_free(&c);
    SafeArrayDestroy(psa);
}

TEST(ums_sa_sftype_and_element_wire_size)
{
    unsigned i; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < NVT; i++) {
            SAFEARRAY *psa;
            UCtx z, c; ULONG size; unsigned char *w;
            if (wine_layout_differs(vtcases[i].vt)) continue;
            psa = mk(vtcases[i].vt, &shapes[0]);
            if (!psa) { t_note("cannot create %s", vtcases[i].name); continue; }
            uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_SAFEARRAY, v, 0, &psa, NULL); uctx_free(&z);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&c, UF_SAFEARRAY, v, c.buf, &psa, NULL);
            w = c.buf;
            t_check(rd32(w + (v ? 32 : 20)) == vtcases[i].sf, __FILE__, __LINE__, "SF_TYPE", "v64=%d %s got %lx want %lx", v, vtcases[i].name, rd32(w + (v ? 32 : 20)), vtcases[i].sf);
            t_check(rd32(w + 12 + (v ? 8 : 0)) == vtcases[i].wel, __FILE__, __LINE__, "element wire size", "v64=%d %s got %lu want %lu", v, vtcases[i].name, rd32(w + 12 + (v ? 8 : 0)), vtcases[i].wel);
            t_check(rd16(w + (v ? 26 : 18)) == vtcases[i].vt, __FILE__, __LINE__, "vartype in cLocks high word", "v64=%d %s", v, vtcases[i].name);
            uctx_free(&c);
            SafeArrayDestroy(psa);
        }
}

TEST(ums_sa_context_independent)
{
    unsigned i; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < NVT; i += 3) {
            SAFEARRAY *psa = mk(vtcases[i].vt, &shapes[3]);
            UCtx a, b, z; ULONG size; unsigned char *ea, *eb;
            if (!psa) continue;
            uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_SAFEARRAY, v, 0, &psa, NULL); uctx_free(&z);
            uctx_init(&a, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            uctx_init(&b, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
            ea = u_marshal(&a, UF_SAFEARRAY, v, a.buf, &psa, NULL);
            eb = u_marshal(&b, UF_SAFEARRAY, v, b.buf, &psa, NULL);
            EQ(ea - a.buf, eb - b.buf);
            { ULONG n = (ULONG)(ea - a.buf); unsigned char m1[4096]; memset(m1, 0, sizeof m1);
              /* pointer-id fields differ per call? no -- same array, same ids */
              t_check(!memcmp(a.buf, b.buf, n), __FILE__, __LINE__, "wire independent of MSHCTX", "v64=%d %s", v, vtcases[i].name); }
            uctx_free(&a); uctx_free(&b);
            SafeArrayDestroy(psa);
        }
}

/* ---- NULL array ------------------------------------------------------------------------------------------ */
TEST(ums_sa_null_array_wire_and_roundtrip)
{
    int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *nul = NULL, *dst; UCtx c, z; ULONG size; unsigned char *m;
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &nul, NULL); uctx_free(&z);
        EQ(size, v ? 8 : 4);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &nul, NULL);
        EQ(m - c.buf, size);
        EQ(v ? rd64(c.buf) : rd32(c.buf), 0);
        CHECK(c.buf[size] == 0xcc);
        /* unmarshal NULL over NULL and over an existing array: result is NULL */
        uctx_reread(&c, size, MSHCTX_LOCAL);
        dst = NULL;
        EQ(u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL), m);
        CHECK(dst == NULL);
        { SAFEARRAYBOUND b = { 3, 0 }; dst = SafeArrayCreate(VT_I4, 1, &b);
          EQ(u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL), m);
          CHECK(dst == NULL); }
        uctx_free(&c);
        /* free of a NULL array is a no-op */
        sa_userfree(v, &dst);
        CHECK(dst == NULL);
    }
}

/* ---- round trips ------------------------------------------------------------------------------------------ */
TEST(ums_sa_roundtrip_all_vts_and_shapes)
{
    unsigned i, j; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
    for (i = 0; i < NVT; i++)
        for (j = 0; j < NSH; j++) {
            SAFEARRAY *src, *dst = NULL;
            char what[96];
            if (wine_layout_differs(vtcases[i].vt)) continue;
            src = mk(vtcases[i].vt, &shapes[j]);
            if (!src) continue;
            snprintf(what, sizeof what, "v64=%d %s/%s", v, vtcases[i].name, shapes[j].name);
            sa_cycle(v, MSHCTX_DIFFERENTMACHINE, 0, src, &dst, 0, what);
            arrays_equal(src, dst, vtcases[i].vt, what);
            if (dst) { sa_userfree(v, &dst); CHECK(dst == NULL); }
            SafeArrayDestroy(src);
        }
}

TEST(ums_sa_roundtrip_at_offsets)
{
    static const unsigned pick[] = { 2, 5, 11, 16, 19 };
    unsigned i; ULONG st; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < ARRAY_N(pick); i++)
            for (st = 1; st <= 9; st += 2) {
                SAFEARRAY *src = mk(vtcases[pick[i]].vt, &shapes[3]), *dst = NULL;
                sa_cycle(v, MSHCTX_LOCAL, st, src, &dst, !wine_layout_differs(vtcases[pick[i]].vt), vtcases[pick[i]].name);
                arrays_equal(src, dst, vtcases[pick[i]].vt, vtcases[pick[i]].name);
                if (dst) sa_userfree(v, &dst);
                SafeArrayDestroy(src);
            }
}

TEST(ums_sa_vartype_is_restored_not_derived_from_sftype)
{
    /* R8, CY, DATE, I8, UI8 all travel as SF_I8 (0x14); the real VT rides in the high word of the lock dword */
    unsigned i; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < NVT; i++) {
            SAFEARRAY *src = mk(vtcases[i].vt, &shapes[0]), *dst = NULL; VARTYPE vt = 0xffff;
            if (!src) continue;
            sa_cycle(v, MSHCTX_LOCAL, 0, src, &dst, 0, vtcases[i].name);
            CHECK(dst != NULL);
            if (dst && wine_layout_differs(vtcases[i].vt)) sa_userfree(v, &dst);
            else if (dst) {
                HRESULT hr = SafeArrayGetVartype(dst, &vt);
                t_check(hr == S_OK && vt == vtcases[i].vt, __FILE__, __LINE__, "SafeArrayGetVartype after unmarshal", "v64=%d %s hr %lx vt %x", v, vtcases[i].name, hr, vt);
                sa_userfree(v, &dst);
            }
            SafeArrayDestroy(src);
        }
}

TEST(ums_sa_locks_are_not_copied_to_the_clone)
{
    SAFEARRAYBOUND sab = { 10, 5 }; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_I2, 1, &sab), *dst = NULL;
        src->cLocks = 7;
        sa_cycle(v, MSHCTX_LOCAL, 0, src, &dst, 1, "locked source");
        EQ(src->cLocks, 7);
        CHECK(dst && dst->cLocks == 0);
        src->cLocks = 0;
        if (dst) sa_userfree(v, &dst);
        SafeArrayDestroy(src);
    }
}

TEST(ums_sa_zero_element_dimension)
{
    SAFEARRAYBOUND b1 = { 0, 3 }, b2[2] = { { 0, 0 }, { 4, 1 } }; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_I4, 1, &b1), *dst = NULL, *src2;
        if (!src) { t_note("SafeArrayCreate with 0 elements failed"); continue; }
        sa_cycle(v, MSHCTX_LOCAL, 0, src, &dst, 1, "0-element array");
        CHECK(dst != NULL);
        if (dst) { EQ(dst->cDims, 1); EQ(dst->rgsabound[0].cElements, 0); EQ(dst->rgsabound[0].lLbound, 3); sa_userfree(v, &dst); }
        SafeArrayDestroy(src);
        src2 = SafeArrayCreate(VT_R8, 2, b2);
        if (src2) {
            sa_cycle(v, MSHCTX_LOCAL, 0, src2, &dst, 1, "0xN array");
            CHECK(dst != NULL);
            if (dst) { EQ(dst->cDims, 2); sa_userfree(v, &dst); }
            SafeArrayDestroy(src2);
        }
    }
}

TEST(ums_sa_bstr_array_wire_details)
{
    /* per-element flags, NULL vs empty vs long, and wire sizes accumulate exactly */
    int v; NEEDS; WINE_SKIP("different BSTR array layout");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAYBOUND sab = { 4, 1 };
        SAFEARRAY *src = SafeArrayCreate(VT_BSTR, 1, &sab), *dst = NULL;
        BSTR *e = src->pvData; UCtx z, c; ULONG size, data_off; unsigned char *w, *m;
        e[0] = SysAllocString(L"first");
        e[1] = NULL;
        e[2] = SysAllocStringLen(L"", 0);
        e[3] = SysAllocString(L"fourth element");
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        w = c.buf;
        data_off = v ? 56 + 8 + 8 : 32 + 8 + 4;
        if (v) { EQ(rd64(w + data_off), 1); EQ(rd64(w + data_off + 8), 0); EQ(rd64(w + data_off + 16), 1); EQ(rd64(w + data_off + 24), 1); }
        else   { EQ(rd32(w + data_off), 1); EQ(rd32(w + data_off + 4), 0); EQ(rd32(w + data_off + 8), 1); EQ(rd32(w + data_off + 12), 1); }
        CHECK(m <= c.buf + size);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        EQ(u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL), m);
        CHECK(dst != NULL);
        if (dst) {
            BSTR *d = dst->pvData;
            CHECK(d[1] == NULL);
            CHECK(d[0] && !lstrcmpW(d[0], L"first"));
            CHECK(d[2] != NULL && SysStringByteLen(d[2]) == 0);
            CHECK(d[3] && !lstrcmpW(d[3], L"fourth element"));
            EQ(dst->cbElements, sizeof(BSTR));
            CHECK(dst->fFeatures & FADF_BSTR);
            sa_userfree(v, &dst);
        }
        uctx_free(&c);
        SafeArrayDestroy(src);
    }
}

TEST(ums_sa_variant_array_wire_details)
{
    int v; NEEDS; WINE_SKIP("different VARIANT array layout");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAYBOUND sab = { 3, 0 };
        SAFEARRAY *src = SafeArrayCreate(VT_VARIANT, 1, &sab), *dst = NULL;
        VARIANT *e = src->pvData; UCtx z, c; ULONG size, data_off; unsigned char *w, *m;
        V_VT(&e[0]) = VT_I4; V_I4(&e[0]) = 77;
        V_VT(&e[1]) = VT_BSTR; V_BSTR(&e[1]) = SysAllocString(L"v");
        V_VT(&e[2]) = VT_R8; V_R8(&e[2]) = 2.5;
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        w = c.buf;
        data_off = v ? 56 + 8 + 8 : 32 + 8 + 4;
        /* each element is preceded by a 'User' marker in the flag array */
        if (v) { EQ(rd64(w + data_off), 0x72657355); EQ(rd64(w + data_off + 8), 0x72657355); EQ(rd64(w + data_off + 16), 0x72657355); }
        else   { EQ(rd32(w + data_off), 0x72657355); EQ(rd32(w + data_off + 4), 0x72657355); EQ(rd32(w + data_off + 8), 0x72657355); }
        EQ(rd32(w + (v ? 20 : 12)), 0x10);          /* element wire size of VARIANT elements */
        CHECK(m <= c.buf + size);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        EQ(u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL), m);
        CHECK(dst != NULL);
        if (dst) {
            VARIANT *d = dst->pvData;
            EQ(dst->cbElements, sizeof(VARIANT));
            CHECK(dst->fFeatures & FADF_VARIANT);
            CHECK(V_VT(&d[0]) == VT_I4 && V_I4(&d[0]) == 77);
            CHECK(V_VT(&d[1]) == VT_BSTR && V_BSTR(&d[1]) && !lstrcmpW(V_BSTR(&d[1]), L"v"));
            CHECK(V_VT(&d[2]) == VT_R8 && V_R8(&d[2]) == 2.5);
            sa_userfree(v, &dst);
        }
        uctx_free(&c);
        SafeArrayDestroy(src);
    }
}

TEST(ums_sa_haveiid_array)
{
    static const GUID iid = { 0x00020400, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };   /* IID_IDispatch */
    SAFEARRAYBOUND sab[2] = { { 3, 1 }, { 2, 0 } }; int v; NEEDS; WINE_SKIP("IID not restored");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreateEx(VT_UNKNOWN, 2, sab, (PVOID)&iid), *dst = NULL; GUID g; HRESULT hr;
        UCtx z, c; ULONG size; unsigned char *w;
        if (!src) { t_note("SafeArrayCreateEx(IID) failed"); continue; }
        CHECK(src->fFeatures & FADF_HAVEIID);
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        w = c.buf;
        EQ(rd32(w + (v ? 32 : 20)), 0x800d);                     /* SF_HAVEIID */
        EQ(rd16(w + (v ? 26 : 18)), 0);                          /* no vartype with an IID */
        CHECK(!memcmp(w + (v ? 56 : 32), &iid, 16));             /* IID follows the fixed header */
        uctx_free(&c);
        sa_cycle(v, MSHCTX_LOCAL, 0, src, &dst, 1, "haveiid");
        CHECK(dst != NULL);
        if (dst) {
            memset(&g, 0, sizeof g);
            hr = SafeArrayGetIID(dst, &g);
            t_check(hr == S_OK && IsEqualGUID(&g, &iid), __FILE__, __LINE__, "IID survives", "hr %lx", hr);
            CHECK(dst->fFeatures & FADF_HAVEIID);
            EQ(dst->cDims, 2);
            sa_userfree(v, &dst);
        }
        SafeArrayDestroy(src);
    }
}

TEST(ums_sa_vartypeless_arrays)
{
    /* arrays made by SafeArrayAllocDescriptor + AllocData have no vartype; they travel by element size */
    static const struct { ULONG cb; ULONG sf; } t[] = { { 1, 0x10 }, { 2, 2 }, { 4, 3 }, { 8, 0x14 } };
    unsigned i; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < ARRAY_N(t); i++) {
            SAFEARRAY *src = NULL, *dst = NULL; VARTYPE vt; UCtx z, c; ULONG size; unsigned char *w; HRESULT hr;
            hr = SafeArrayAllocDescriptor(1, &src);
            CHECK(hr == S_OK);
            if (hr != S_OK) continue;
            src->cbElements = t[i].cb;
            src->rgsabound[0].lLbound = 2;
            src->rgsabound[0].cElements = 48;
            CHECK(SafeArrayAllocData(src) == S_OK);
            CHECK(SafeArrayGetVartype(src, &vt) == E_INVALIDARG);
            memset(src->pvData, 0x5a, 48 * t[i].cb);
            uctx_init(&z, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
            uctx_init(&c, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
            w = c.buf;
            EQ(rd32(w + (v ? 32 : 20)), t[i].sf);
            EQ(rd16(w + (v ? 26 : 18)), 0);                     /* vt 0 */
            uctx_free(&c);
            sa_cycle(v, MSHCTX_DIFFERENTMACHINE, 0, src, &dst, 1, "vartypeless");
            CHECK(dst != NULL);
            if (dst) {
                EQ(dst->cbElements, t[i].cb);
                EQ(dst->rgsabound[0].cElements, 48);
                EQ(dst->rgsabound[0].lLbound, 2);
                CHECK(dst->pvData && !memcmp(dst->pvData, src->pvData, 48 * t[i].cb));
                sa_userfree(v, &dst);
            }
            SafeArrayDestroyData(src);
            SafeArrayDestroyDescriptor(src);
        }
}

TEST(ums_sa_unsupported_element_size_raises)
{
    /* a vartype-less array whose element size is not 1,2,4,8 (or 24 with FADF_VARIANT) cannot be classified */
    int v; NEEDS; WINE_SKIP("different error handling");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = NULL; UCtx z, c; DWORD exc = 0;
        CHECK(SafeArrayAllocDescriptor(1, &src) == S_OK);
        src->cbElements = 3;
        src->rgsabound[0].lLbound = 0; src->rgsabound[0].cElements = 4;
        CHECK(SafeArrayAllocData(src) == S_OK);
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        u_size(&z, UF_SAFEARRAY, v, 0, &src, &exc); uctx_free(&z);
        EQ(exc, E_INVALIDARG_X);
        uctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        exc = 0; u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, &exc);
        EQ(exc, E_INVALIDARG_X);
        uctx_free(&c);
        SafeArrayDestroyData(src); SafeArrayDestroyDescriptor(src);
    }
}

TEST(ums_sa_decimal_array_raises)
{
    /* VT_DECIMAL arrays have 16-byte elements: no SF_TYPE for that -> E_INVALIDARG on size and marshal */
    SAFEARRAYBOUND sab = { 2, 0 }; int v; NEEDS; WINE_SKIP("different error handling");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_DECIMAL, 1, &sab); UCtx z, c; DWORD exc = 0;
        if (!src) { t_note("no VT_DECIMAL safearray"); continue; }
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        u_size(&z, UF_SAFEARRAY, v, 0, &src, &exc); uctx_free(&z);
        EQ(exc, E_INVALIDARG_X);
        uctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        exc = 0; u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, &exc);
        EQ(exc, E_INVALIDARG_X);
        uctx_free(&c);
        SafeArrayDestroy(src);
    }
}

/* ---- free / reuse / errors --------------------------------------------------------------------------------- */
TEST(ums_sa_userfree_semantics)
{
    SAFEARRAYBOUND sab = { 4, 0 }; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *a = SafeArrayCreate(VT_BSTR, 1, &sab), *none = NULL;
        BSTR *e = a->pvData; DWORD exc = 0; UCtx c;
        e[0] = SysAllocString(L"x"); e[2] = SysAllocString(L"y");
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        u_free(&c, UF_SAFEARRAY, v, &none, NULL);     /* *p == NULL: nothing */
        if (!t_is_wine()) u_free(&c, UF_SAFEARRAY, v, NULL, NULL);
        /* a locked array cannot be freed: DISP_E_ARRAYISLOCKED */
        SafeArrayLock(a);
        if (!t_is_wine()) {
            u_free(&c, UF_SAFEARRAY, v, &a, &exc);
            EQ(exc, DISP_E_ARRAYISLOCKED_X);
            CHECK(a != NULL);
        }
        SafeArrayUnlock(a);
        u_free(&c, UF_SAFEARRAY, v, &a, NULL);        /* destroys descriptor, data and BSTRs; pointer zeroed */
        CHECK(a == NULL);
        uctx_free(&c);
    }
}

TEST(ums_sa_unmarshal_reuses_matching_destination)
{
    SAFEARRAYBOUND sab[2] = { { 10, 5 }, { 2, 1 } }; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_I4, 2, sab), *dst = SafeArrayCreate(VT_I4, 2, sab), *orig = dst;
        void *data = dst->pvData;
        memset(dst->pvData, 0, 20 * 4);
        fill(src, VT_I4);
        sa_cycle(v, MSHCTX_LOCAL, 0, src, &dst, 0, "reuse");
        CHECK(dst == orig);                                    /* same descriptor ... */
        CHECK(dst->pvData == data);                            /* ... and same element storage */
        CHECK(!memcmp(dst->pvData, src->pvData, 80));
        SafeArrayDestroy(dst); SafeArrayDestroy(src);
    }
}

TEST(ums_sa_unmarshal_reshapes_existing_destination)
{
    SAFEARRAYBOUND a[1] = { { 10, 5 } }, b[1] = { { 6, -1 } }; int v; NEEDS;
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_I4, 1, b), *dst = SafeArrayCreate(VT_I4, 1, a), *orig = dst;
        fill(src, VT_I4);
        sa_cycle(v, MSHCTX_LOCAL, 0, src, &dst, 0, "reshape");
        CHECK(dst == orig);                                    /* descriptor kept, bounds and data replaced */
        EQ(dst->rgsabound[0].cElements, 6);
        EQ(dst->rgsabound[0].lLbound, -1);
        CHECK(!memcmp(dst->pvData, src->pvData, 24));
        SafeArrayDestroy(dst); SafeArrayDestroy(src);
    }
}

TEST(ums_sa_unmarshal_destination_mismatch_raises)
{
    SAFEARRAYBOUND a1[1] = { { 4, 0 } }, a2[2] = { { 4, 0 }, { 2, 0 } }; int v; NEEDS; WINE_SKIP("different checks");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_I4, 2, a2), *dst = SafeArrayCreate(VT_I4, 1, a1);
        UCtx z, c; ULONG size; DWORD exc = 0;
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        /* dimension count differs */
        u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, &exc);
        t_check(exc == DISP_E_BADCALLEE_X, __FILE__, __LINE__, "cDims mismatch", "exc 0x%lx", exc);
        SafeArrayDestroy(dst);
        /* AUTO/STATIC/EMBEDDED bits differ */
        dst = SafeArrayCreate(VT_I4, 2, a2);
        dst->fFeatures |= FADF_STATIC;
        exc = 0;
        u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, &exc);
        t_check(exc == DISP_E_BADCALLEE_X, __FILE__, __LINE__, "storage-feature mismatch", "exc 0x%lx", exc);
        dst->fFeatures &= ~FADF_STATIC;
        SafeArrayDestroy(dst);
        uctx_free(&c);
        SafeArrayDestroy(src);
    }
}

TEST(ums_sa_unmarshal_truncated_buffer)
{
    static const unsigned pick[] = { 2, 11, 16, 19 };
    unsigned i; int v; NEEDS; WINE_SKIP("no shield bounds checks");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < ARRAY_N(pick); i++) {
            SAFEARRAYBOUND sab = { 3, 1 };
            SAFEARRAY *src = SafeArrayCreate(vtcases[pick[i]].vt, 1, &sab); UCtx z, c; ULONG size, n; unsigned char *wire, *m;
            fill(src, vtcases[pick[i]].vt);
            uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
            size = (ULONG)(m - c.buf);
            wire = malloc(size); memcpy(wire, c.buf, size); uctx_free(&c);
            for (n = 0; n < size; n++) {
                UCtx d; SAFEARRAY *dst = NULL; DWORD exc = 0;
                uctx_wire(&d, wire, n, MSHCTX_LOCAL);
                u_unmarshal(&d, UF_SAFEARRAY, v, d.buf, &dst, &exc);
                if (!v) t_check(exc == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "truncated SAFEARRAY -> 0x6f7", "%s len %lu/%lu exc 0x%lx", vtcases[pick[i]].name, n, size, exc);
                else { t_check(exc != 0, __FILE__, __LINE__, "truncated NDR64 SAFEARRAY raises", "%s len %lu/%lu", vtcases[pick[i]].name, n, size); if (exc != RPC_X_BAD_STUB_DATA_) t_note("NDR64 truncated %s len %lu/%lu exc 0x%lx", vtcases[pick[i]].name, n, size, exc); }
                if (dst && !exc) SafeArrayDestroy(dst);
                CHECK(uctx_canary_ok(&d));
                uctx_free(&d);
            }
            free(wire);
            SafeArrayDestroy(src);
        }
}

TEST(ums_sa_unmarshal_malformed_fields)
{
    /* hand-built NDR32 images */
    unsigned char w[160]; UCtx c; SAFEARRAY *dst = NULL; DWORD exc; NEEDS; WINE_SKIP("different validation");
    /* base: 1-D I4 array, 2 elements */
    memset(w, 0, sizeof w);
    wr32(w + 0, 1); wr32(w + 4, 1); wr16(w + 8, 1); wr16(w + 10, FADF_HAVEVARTYPE); wr32(w + 12, 4);
    wr32(w + 16, VT_I4 << 16); wr32(w + 20, 3); wr32(w + 24, 2); wr32(w + 28, 0x1000);
    wr32(w + 32, 2); wr32(w + 36, 0);           /* bound: 2 elements from 0 */
    wr32(w + 40, 2); wr32(w + 44, 111); wr32(w + 48, 222);
    uctx_wire(&c, w, 52, MSHCTX_LOCAL);
    exc = 0xabc; dst = NULL;
    u_unmarshal(&c, UF_SAFEARRAY, 0, c.buf, &dst, &exc);
    EQ(exc, 0);
    CHECK(dst && dst->cDims == 1 && ((int *)dst->pvData)[0] == 111 && ((int *)dst->pvData)[1] == 222);
    if (dst) { sa_userfree(0, &dst); }
    uctx_free(&c);

    /* unknown SF_TYPE 7: the array is created as VT_CY then rejected */
    wr32(w + 20, 7);
    uctx_wire(&c, w, 52, MSHCTX_LOCAL); exc = 0; dst = NULL;
    u_unmarshal(&c, UF_SAFEARRAY, 0, c.buf, &dst, &exc);
    /* the half-built array is still locked when it is released, so the exception is 0x6f7 or ARRAYISLOCKED */
    t_check(exc != 0, __FILE__, __LINE__, "unknown SF_TYPE rejected", "exc 0x%lx", exc);
    if (exc != RPC_X_BAD_STUB_DATA_) t_note("unknown SF_TYPE raised 0x%lx", exc);
    CHECK(dst == NULL);
    uctx_free(&c);

    /* SF_TYPE that is no valid VARTYPE: SafeArrayCreate fails -> E_OUTOFMEMORY */
    wr32(w + 20, 0x1234);
    uctx_wire(&c, w, 52, MSHCTX_LOCAL); exc = 0; dst = NULL;
    u_unmarshal(&c, UF_SAFEARRAY, 0, c.buf, &dst, &exc);
    t_check(exc != 0, __FILE__, __LINE__, "invalid SF_TYPE rejected", "exc 0x%lx", exc);
    if (exc != E_OUTOFMEMORY_X) t_note("invalid SF_TYPE raised 0x%lx", exc);
    CHECK(dst == NULL);
    uctx_free(&c);

    /* negative cell count */
    wr32(w + 20, 3); wr32(w + 40, 0xfffffffe);
    uctx_wire(&c, w, 52, MSHCTX_LOCAL); exc = 0; dst = NULL;
    u_unmarshal(&c, UF_SAFEARRAY, 0, c.buf, &dst, &exc);
    t_check(exc != 0, __FILE__, __LINE__, "negative cell count rejected", "exc 0x%lx", exc);
    CHECK(dst == NULL);
    uctx_free(&c);
}

TEST(ums_sa_unmarshal_bad_callback)
{
    SAFEARRAYBOUND sab = { 3, 0 }; int v; NEEDS; WINE_SKIP("no NdrGetUserMarshalInfo");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *src = SafeArrayCreate(VT_I4, 1, &sab), *dst = NULL; UCtx z, c; ULONG size; DWORD exc = 0;
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size + 16, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        c.umcb.Signature = 0xdeadbeef;
        u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, &exc);
        EQ(exc, ERROR_INVALID_PARAMETER);
        c.umcb.Signature = UMS_SIG;
        c.stub.Buffer = c.buf + size + 4;
        exc = 0; u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, &exc);
        EQ(exc, 0x6f8);
        uctx_free(&c); SafeArrayDestroy(src);
    }
}

TEST(ums_sa_exported_marshal_and_unmarshal_match_user_variants)
{
    SAFEARRAYBOUND sab = { 5, 2 }; SAFEARRAY *src, *d1 = NULL, *d2 = NULL;
    UCtx z, a, b; ULONG size; unsigned char *ea, *eb, *ua, *ub; NEEDS; WINE_SKIP("LPSAFEARRAY_Marshal is a stub");
    if (!t_proc_in("oleaut32.dll", "LPSAFEARRAY_Marshal") || !t_proc_in("oleaut32.dll", "LPSAFEARRAY_Unmarshal")) { t_skip("LPSAFEARRAY_Marshal/Unmarshal exports missing"); return; }
    src = SafeArrayCreate(VT_R4, 1, &sab); fill(src, VT_R4);
    uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
    size = u_size(&z, UF_SAFEARRAY, 0, 0, &src, NULL); uctx_free(&z);
    uctx_init(&a, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
    uctx_init(&b, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
    ea = u_marshal(&a, UF_SAFEARRAY, 0, a.buf, &src, NULL);
    eb = u_sa_marshal_exp(&b, b.buf, &src, NULL);
    EQ(ea - a.buf, eb - b.buf);
    CHECK(!memcmp(a.buf, b.buf, (size_t)(ea - a.buf)));
    uctx_reread(&a, size, MSHCTX_LOCAL); uctx_reread(&b, size, MSHCTX_LOCAL);
    ua = u_unmarshal(&a, UF_SAFEARRAY, 0, a.buf, &d1, NULL);
    ub = u_sa_unmarshal_exp(&b, b.buf, &d2, NULL);
    EQ(ua - a.buf, ub - b.buf);
    arrays_equal(src, d1, VT_R4, "UserUnmarshal");
    arrays_equal(src, d2, VT_R4, "LPSAFEARRAY_Unmarshal");
    sa_userfree(0, &d1); sa_userfree(0, &d2);
    uctx_free(&a); uctx_free(&b);
    SafeArrayDestroy(src);
}
