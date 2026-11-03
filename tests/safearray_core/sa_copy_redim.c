/* SafeArrayCopy / CopyData / Redim / SetIID / GetIID / GetVartype / Destroy* semantics. */
#include "sa.h"

static void fill_pattern(SAFEARRAY *psa)
{
    BYTE *p = psa->pvData; ULONG n = psa->cbElements;
    for (UINT d = 0; d < psa->cDims; d++) n *= psa->rgsabound[d].cElements;
    for (ULONG k = 0; k < n; k++) p[k] = (BYTE)(k * 7 + 3);
}
static ULONG data_size(const SAFEARRAY *psa)
{
    ULONG n = psa->cbElements;
    for (UINT d = 0; d < psa->cDims; d++) n *= psa->rgsabound[d].cElements;
    return n;
}

/* ---- SafeArrayCopy ------------------------------------------------------- */
TEST(sa_copy_args)
{
    SA_BEGIN();
    SAFEARRAY *out = (SAFEARRAY *)0x1;
    EQHR(pCopy(NULL, &out), S_OK);
    EQ(out, NULL);                                    /* copying NULL yields NULL */
    SAFEARRAY *psa = sa1(VT_I4, 0, 2);
    CHECK(psa != NULL);
    EQHR(pCopy(psa, NULL), E_INVALIDARG);
    EQHR(pCopy(NULL, NULL), E_INVALIDARG);
    out = (SAFEARRAY *)0x1;
    EQHR(pCopy(psa, &out), S_OK);
    CHECK(out != NULL && out != psa);
    if (out) pDestroy(out);
    if (psa) pDestroy(psa);
}

/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_SafeArrayCopy; table driven over all vts */
TEST(sa_copy_all_vts_features_and_data)
{
    SA_BEGIN();
    for (unsigned t = 0; t < SA_NVALID; t++) {
        VARTYPE vt = SA_VALID_VTS[t];
        SAFEARRAYBOUND b[2] = { {3, -1}, {2, 4} };
        for (int vec = 0; vec < 2; vec++) {
            SAFEARRAY *src = vec ? pCreateVector(vt, -1, 5) : pCreate(vt, 2, b), *dst = NULL;
            if (!src) { CHECKF(0, "create vt=%u", vt); continue; }
            if (vt != VT_BSTR && vt != VT_VARIANT && vt != VT_UNKNOWN && vt != VT_DISPATCH) fill_pattern(src);
            EQHR(pCopy(src, &dst), S_OK);
            CHECKF(dst != NULL, "copy vt=%u", vt);
            if (dst) {
                EQ(dst->cDims, src->cDims);
                EQ(dst->cbElements, src->cbElements);
                EQ(dst->cLocks, 0);
                EQ(dst->fFeatures, src->fFeatures & ~FADF_CREATEVECTOR);
                CHECK(!(dst->fFeatures & FADF_CREATEVECTOR));
                for (UINT d = 0; d < src->cDims; d++) {
                    EQ(dst->rgsabound[d].cElements, src->rgsabound[d].cElements);
                    EQ(dst->rgsabound[d].lLbound, src->rgsabound[d].lLbound);
                }
                CHECK(dst->pvData != NULL && dst->pvData != src->pvData);
                CHECKF(!memcmp(dst->pvData, src->pvData, data_size(src)), "vt=%u data", vt);
                VARTYPE v1 = 1, v2 = 2;
                EQHR(pGetVartype(src, &v1), S_OK); EQHR(pGetVartype(dst, &v2), S_OK); EQ(v1, v2);
                EQ(v2, vt);
                EQHR(pDestroy(dst), S_OK);
            }
            pDestroy(src);
        }
    }
}

TEST(sa_copy_is_independent)
{
    SA_BEGIN();
    SAFEARRAY *src = sa1(VT_I4, 0, 3), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    LONG i, v;
    for (i = 0; i < 3; i++) { v = i + 10; pPutElement(src, &i, &v); }
    EQHR(pCopy(src, &dst), S_OK);
    CHECK(dst != NULL);
    if (dst) {
        i = 1; v = 999; EQHR(pPutElement(dst, &i, &v), S_OK);
        LONG o = 0; EQHR(pGetElement(src, &i, &o), S_OK); EQ(o, 11);
        pDestroy(src);                                /* destroying the source leaves the copy valid */
        o = 0; EQHR(pGetElement(dst, &i, &o), S_OK); EQ(o, 999);
        pDestroy(dst);
    }
}

TEST(sa_copy_of_locked_source)
{
    SA_BEGIN();
    SAFEARRAY *src = sa1(VT_I4, 0, 3), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    EQHR(pLock(src), S_OK); EQHR(pLock(src), S_OK);
    EQHR(pCopy(src, &dst), S_OK);
    EQ(src->cLocks, 2);                               /* source lock count restored */
    CHECK(dst != NULL);
    if (dst) { EQ(dst->cLocks, 0); pDestroy(dst); }
    pUnlock(src); pUnlock(src);
    pDestroy(src);
}

TEST(sa_copy_drops_storage_flags)
{
    SA_BEGIN();
    /* AUTO/STATIC/EMBEDDED/FIXEDSIZE and the internal DATADELETED/CREATEVECTOR bits are not inherited */
    SAFEARRAY *src = sa1(VT_I4, 0, 3), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    USHORT saved = src->fFeatures;
    src->fFeatures = saved | FADF_FIXEDSIZE;
    EQHR(pCopy(src, &dst), S_OK);
    if (dst) { EQ(dst->fFeatures, saved); pDestroy(dst); dst = NULL; }
    src->fFeatures = saved | FADF_AUTO | FADF_STATIC | FADF_EMBEDDED | FADF_FIXEDSIZE;
    EQHR(pCopy(src, &dst), S_OK);
    if (dst) { EQ(dst->fFeatures, saved); pDestroy(dst); dst = NULL; }
    src->fFeatures = saved | 0x8;                      /* reserved bit survives (mask 0xcfe8) */
    EQHR(pCopy(src, &dst), S_OK);
    if (dst) { EQ(dst->fFeatures, saved | 0x8); dst->fFeatures = saved; pDestroy(dst); dst = NULL; }
    src->fFeatures = saved;
    pDestroy(src);
}

TEST(sa_copy_preserves_iid)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 2, 0 };
    GUID g;
    SAFEARRAY *src = pCreateEx(VT_UNKNOWN, 1, &b, (PVOID)&SA_IID_Custom), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    EQHR(pCopy(src, &dst), S_OK);
    if (dst) {
        EQ(dst->fFeatures, FADF_HAVEIID | FADF_UNKNOWN);
        EQHR(pGetIID(dst, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom));
        VARTYPE vt = 0; EQHR(pGetVartype(dst, &vt), S_OK); EQ(vt, VT_UNKNOWN);
        pDestroy(dst);
    }
    pDestroy(src);
    src = pCreateEx(VT_DISPATCH, 1, &b, (PVOID)&SA_IID_Custom2);
    dst = NULL;
    if (src) {
        EQHR(pCopy(src, &dst), S_OK);
        if (dst) {
            EQHR(pGetIID(dst, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom2));
            VARTYPE vt = 0; EQHR(pGetVartype(dst, &vt), S_OK); EQ(vt, VT_DISPATCH);
            pDestroy(dst);
        }
        pDestroy(src);
    }
}

TEST(sa_copy_bstr_deep)
{
    SA_BEGIN();
    SAFEARRAY *src = sa1(VT_BSTR, 0, 3), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    BSTR s0 = bstr_w(L"zero"), s2 = bstr_w(L"two");
    LONG i = 0; pPutElement(src, &i, s0); i = 2; pPutElement(src, &i, s2);
    EQHR(pCopy(src, &dst), S_OK);
    CHECK(dst != NULL);
    if (dst) {
        BSTR *a = src->pvData, *c = dst->pvData;
        CHECK(a[0] != c[0] && a[2] != c[2]);          /* strings are duplicated */
        CHECK(c[0] && !lstrcmpW(c[0], L"zero"));
        CHECK(c[2] && !lstrcmpW(c[2], L"two"));
        EQ(pSysStringLen(c[0]), 4);
        if (c[1] == NULL) t_note("copy of a NULL BSTR element stays NULL");
        else t_note("copy of a NULL BSTR element became an allocated string (len %u)", pSysStringLen(c[1]));
        pDestroy(src); src = NULL;
        CHECK(c[0] && !lstrcmpW(c[0], L"zero"));      /* copy independent of destroyed source */
        pDestroy(dst);
    }
    if (src) pDestroy(src);
    pSysFreeString(s0); pSysFreeString(s2);
}

TEST(sa_copy_variant_deep)
{
    SA_BEGIN();
    SAFEARRAY *src = sa1(VT_VARIANT, 0, 2), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = bstr_w(L"vb");
    LONG i = 0; pPutElement(src, &i, &v);
    pVariantClear(&v);
    V_VT(&v) = VT_I2; V_I2(&v) = 33; i = 1; pPutElement(src, &i, &v);
    EQHR(pCopy(src, &dst), S_OK);
    CHECK(dst != NULL);
    if (dst) {
        VARIANT *a = src->pvData, *c = dst->pvData;
        EQ(V_VT(&c[0]), VT_BSTR);
        CHECK(V_BSTR(&c[0]) != V_BSTR(&a[0]));
        CHECK(V_BSTR(&c[0]) && !lstrcmpW(V_BSTR(&c[0]), L"vb"));
        EQ(V_VT(&c[1]), VT_I2); EQ(V_I2(&c[1]), 33);
        pDestroy(dst);
    }
    pDestroy(src);
}

TEST(sa_copy_unknown_addrefs)
{
    SA_BEGIN();
    SAFEARRAY *src = sa1(VT_UNKNOWN, 0, 3), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    TObj o; tobj_init(&o, 1);
    LONG i = 0; pPutElement(src, &i, OBJ(o)); i = 2; pPutElement(src, &i, OBJ(o));
    EQ(o.ref, 3);
    EQHR(pCopy(src, &dst), S_OK);
    EQ(o.ref, 5);                                     /* the copy holds its own references */
    if (dst) {
        EQ(((void **)dst->pvData)[0], &o); EQ(((void **)dst->pvData)[1], NULL);
        pDestroy(dst);
    }
    EQ(o.ref, 3);
    pDestroy(src);
    EQ(o.ref, 1);
}

TEST(sa_copy_record_array)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 4);
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *src = pCreateEx(VT_RECORD, 1, &b, REC(r)), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    BYTE rec[4] = { 5, 6, 7, 8 }; LONG i = 1; pPutElement(src, &i, rec);
    r.nCopy = 0;
    EQHR(pCopy(src, &dst), S_OK);
    CHECK(dst != NULL);
    if (dst) {
        EQ(dst->fFeatures, FADF_RECORD);
        EQ(dst->cbElements, 4);
        EQ(r.nCopy, 3);                               /* RecordCopy per element */
        EQ(r.ref, 3);                                 /* destination descriptor shares the record info */
        IRecordInfo *ri = NULL; EQHR(pGetRecordInfo(dst, &ri), S_OK); EQ(ri, &r); ri->lpVtbl->Release(ri);
        CHECK(!memcmp((BYTE *)dst->pvData + 4, rec, 4));
        pDestroy(dst);
    }
    EQ(r.ref, 2);
    pDestroy(src);
    EQ(r.ref, 1);
}

TEST(sa_copy_zero_elements)
{
    SA_BEGIN();
    SAFEARRAY *src = sa1(VT_I4, 3, 0), *dst = NULL;
    CHECK(src != NULL);
    if (!src) return;
    EQHR(pCopy(src, &dst), S_OK);
    CHECK(dst != NULL);
    if (dst) {
        LONG lb = 0, ub = 0; EQHR(pGetLBound(dst, 1, &lb), S_OK); EQ(lb, 3);
        EQHR(pGetUBound(dst, 1, &ub), S_OK); EQ(ub, 2);
        pDestroy(dst);
    }
    pDestroy(src);
}

TEST(sa_copy_descriptor_without_type_info)
{
    SA_BEGIN();
    /* an array built by hand from AllocDescriptor (no vt/IID/record bits) can be copied */
    SAFEARRAY *src = NULL, *dst = NULL;
    EQHR(pAllocDescriptor(1, &src), S_OK);
    if (!src) return;
    src->cbElements = 2; src->rgsabound[0].cElements = 4; src->rgsabound[0].lLbound = 1;
    EQHR(pAllocData(src), S_OK);
    fill_pattern(src);
    EQHR(pCopy(src, &dst), S_OK);
    CHECK(dst != NULL);
    if (dst) {
        EQ(dst->fFeatures, 0);
        CHECK(!memcmp(dst->pvData, src->pvData, 8));
        VARTYPE vt; EQHR(pGetVartype(dst, &vt), E_INVALIDARG);
        pDestroy(dst);
    }
    pDestroy(src);
}

/* ---- SafeArrayCopyData ------------------------------------------------------ */
/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_SafeArrayCopyData */
TEST(sa_copydata_args_and_size_mismatch)
{
    SA_BEGIN();
    SAFEARRAYBOUND b2[2] = { {2, 0}, {3, 0} }, b2x[2] = { {3, 0}, {2, 0} }, b1 = { 6, 0 }, b3 = { 4, 0 };
    SAFEARRAY *a = pCreate(VT_I4, 2, b2), *same = pCreate(VT_I4, 2, b2), *swapped = pCreate(VT_I4, 2, b2x);
    SAFEARRAY *flat = pCreate(VT_I4, 1, &b1), *longer = pCreate(VT_I4, 1, &b3);
    CHECK(a && same && swapped && flat && longer);
    if (!(a && same && swapped && flat && longer)) return;
    EQHR(pCopyData(NULL, same), E_INVALIDARG);
    EQHR(pCopyData(a, NULL), E_INVALIDARG);
    EQHR(pCopyData(NULL, NULL), E_INVALIDARG);
    EQHR(pCopyData(a, flat), E_INVALIDARG);          /* different number of dimensions, same element count */
    EQHR(pCopyData(a, swapped), E_INVALIDARG);       /* per-dimension element counts must match */
    EQHR(pCopyData(flat, longer), E_INVALIDARG);
    EQHR(pCopyData(longer, flat), E_INVALIDARG);
    EQ(a->cLocks, 0); EQ(same->cLocks, 0);
    fill_pattern(a);
    EQHR(pCopyData(a, same), S_OK);
    CHECK(!memcmp(a->pvData, same->pvData, 24));
    EQ(a->cLocks, 0); EQ(same->cLocks, 0);
    pDestroy(a); pDestroy(same); pDestroy(swapped); pDestroy(flat); pDestroy(longer);
}

TEST(sa_copydata_lbound_and_type_not_compared)
{
    SA_BEGIN();
    SAFEARRAYBOUND b1 = { 3, 0 }, b2 = { 3, 100 };
    SAFEARRAY *a = pCreate(VT_I4, 1, &b1), *c = pCreate(VT_R4, 1, &b2);
    CHECK(a && c);
    if (!a || !c) return;
    fill_pattern(a);
    EQHR(pCopyData(a, c), S_OK);                      /* only dims and counts matter; bytes are copied */
    CHECK(!memcmp(a->pvData, c->pvData, 12));
    EQ(c->rgsabound[0].lLbound, 100);                 /* destination bounds untouched */
    pDestroy(a); pDestroy(c);
}

TEST(sa_copydata_zero_cbelements_source)
{
    SA_BEGIN();
    SAFEARRAY *a = sa1(VT_I4, 0, 2), *c = sa1(VT_I4, 0, 2);
    CHECK(a && c);
    if (!a || !c) return;
    ULONG cb = a->cbElements; a->cbElements = 0;
    EQHR(pCopyData(a, c), E_INVALIDARG);
    a->cbElements = cb;
    pDestroy(a); pDestroy(c);
}

TEST(sa_copydata_lock_limits)
{
    SA_BEGIN();
    SAFEARRAY *a = sa1(VT_I4, 0, 2), *c = sa1(VT_I4, 0, 2);
    CHECK(a && c);
    if (!a || !c) return;
    a->cLocks = 0xffff;
    EQHR(pCopyData(a, c), E_UNEXPECTED);
    EQ(a->cLocks, 0xffff); EQ(c->cLocks, 0);
    a->cLocks = 0;
    c->cLocks = 0xffff;
    EQHR(pCopyData(a, c), E_UNEXPECTED);
    EQ(a->cLocks, 0); EQ(c->cLocks, 0xffff);
    c->cLocks = 0;
    EQHR(pLock(c), S_OK);                             /* a merely locked destination is fine */
    EQHR(pCopyData(a, c), S_OK);
    EQ(c->cLocks, 1);
    pUnlock(c);
    pDestroy(a); pDestroy(c);
}

TEST(sa_copydata_bstr_replaces_destination)
{
    SA_BEGIN();
    SAFEARRAY *a = sa1(VT_BSTR, 0, 2), *c = sa1(VT_BSTR, 0, 2);
    CHECK(a && c);
    if (!a || !c) return;
    BSTR s = bstr_w(L"src"), d = bstr_w(L"destination-old");
    LONG i = 0; pPutElement(a, &i, s); pPutElement(c, &i, d);
    i = 1; pPutElement(c, &i, d);
    EQHR(pCopyData(a, c), S_OK);
    BSTR *cs = c->pvData;
    CHECK(cs[0] && !lstrcmpW(cs[0], L"src"));
    CHECK(cs[0] != ((BSTR *)a->pvData)[0]);
    if (cs[1]) EQ(pSysStringLen(cs[1]), 0); else t_note("NULL source BSTR copied as NULL");
    pSysFreeString(s); pSysFreeString(d);
    pDestroy(a); pDestroy(c);
}

TEST(sa_copydata_unknown_refcounts)
{
    SA_BEGIN();
    SAFEARRAY *a = sa1(VT_UNKNOWN, 0, 2), *c = sa1(VT_UNKNOWN, 0, 2);
    CHECK(a && c);
    if (!a || !c) return;
    TObj x, y; tobj_init(&x, 1); tobj_init(&y, 1);
    LONG i = 0; pPutElement(a, &i, OBJ(x));           /* a = { x, NULL } */
    i = 0; pPutElement(c, &i, OBJ(y)); i = 1; pPutElement(c, &i, OBJ(y));  /* c = { y, y } */
    EQ(x.ref, 2); EQ(y.ref, 3);
    EQHR(pCopyData(a, c), S_OK);
    EQ(x.ref, 3);                                     /* AddRef on copied pointer */
    EQ(y.ref, 1);                                     /* both old destination elements released */
    EQ(((void **)c->pvData)[0], &x); EQ(((void **)c->pvData)[1], NULL);
    pDestroy(a); pDestroy(c);
    EQ(x.ref, 1);
}

TEST(sa_copydata_variant)
{
    SA_BEGIN();
    SAFEARRAY *a = sa1(VT_VARIANT, 0, 2), *c = sa1(VT_VARIANT, 0, 2);
    CHECK(a && c);
    if (!a || !c) return;
    VARIANT v; pVariantInit(&v);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = bstr_w(L"abc");
    LONG i = 0; pPutElement(a, &i, &v); pPutElement(c, &i, &v);
    pVariantClear(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 5; i = 1; pPutElement(a, &i, &v);
    EQHR(pCopyData(a, c), S_OK);
    VARIANT *cv = c->pvData, *av = a->pvData;
    EQ(V_VT(&cv[0]), VT_BSTR); CHECK(V_BSTR(&cv[0]) != V_BSTR(&av[0]));
    CHECK(!lstrcmpW(V_BSTR(&cv[0]), L"abc"));
    EQ(V_VT(&cv[1]), VT_I4); EQ(V_I4(&cv[1]), 5);
    pDestroy(a); pDestroy(c);
}

/* ---- SafeArrayRedim ---------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_SafeArrayRedim */
TEST(sa_redim_args)
{
    SA_BEGIN();
    SAFEARRAYBOUND nb = { 2, 0 };
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    EQHR(pRedim(NULL, &nb), E_INVALIDARG);
    EQHR(pRedim(psa, NULL), E_INVALIDARG);
    EQHR(pRedim(NULL, NULL), E_INVALIDARG);
    if (psa) { EQ(psa->rgsabound[0].cElements, 3); pDestroy(psa); }
}

TEST(sa_redim_grow_shrink_numeric)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 5, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG i = 5; i < 9; i++) { LONG v = i * 3; pPutElement(psa, &i, &v); }
    SAFEARRAYBOUND nb = { 6, 5 };
    EQHR(pRedim(psa, &nb), S_OK);
    LONG lb, ub, v;
    EQHR(pGetLBound(psa, 1, &lb), S_OK); EQ(lb, 5);
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 10);
    for (LONG i = 5; i < 9; i++) { v = 0; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, i * 3); }
    for (LONG i = 9; i < 11; i++) { v = 77; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 0); }   /* new elements are zero */
    nb.cElements = 2;
    EQHR(pRedim(psa, &nb), S_OK);
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 6);
    LONG i = 5; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 15);
    i = 6; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 18);
    i = 7; EQHR(pGetElement(psa, &i, &v), DISP_E_BADINDEX);
    nb.cElements = 0;
    EQHR(pRedim(psa, &nb), S_OK);
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 4);
    nb.cElements = 3;
    EQHR(pRedim(psa, &nb), S_OK);                      /* regrow from empty: all zero */
    for (i = 5; i < 8; i++) { v = 9; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 0); }
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_redim_same_size_changes_lbound_only)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG i = 0; i < 3; i++) { LONG v = i + 1; pPutElement(psa, &i, &v); }
    SAFEARRAYBOUND nb = { 3, -10 };
    EQHR(pRedim(psa, &nb), S_OK);
    LONG lb, v, i;
    EQHR(pGetLBound(psa, 1, &lb), S_OK); EQ(lb, -10);
    for (i = -10; i < -7; i++) { EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, i + 11); }
    i = 0; EQHR(pGetElement(psa, &i, &v), DISP_E_BADINDEX);
    pDestroy(psa);
}

TEST(sa_redim_changes_lbound_with_resize)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I2, 1, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG i = 1; i <= 2; i++) { SHORT v = (SHORT)(i * 11); pPutElement(psa, &i, &v); }
    SAFEARRAYBOUND nb = { 4, 100 };
    EQHR(pRedim(psa, &nb), S_OK);
    SHORT v; LONG i = 100;
    EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 11);
    i = 101; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 22);
    i = 103; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 0);
    pDestroy(psa);
}

TEST(sa_redim_fixedsize_refused)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    psa->fFeatures |= FADF_FIXEDSIZE;
    SAFEARRAYBOUND nb = { 5, 0 };
    EQHR(pRedim(psa, &nb), DISP_E_ARRAYISLOCKED);
    EQ(psa->rgsabound[0].cElements, 3);
    psa->fFeatures &= ~FADF_FIXEDSIZE;
    EQHR(pRedim(psa, &nb), S_OK);
    pDestroy(psa);
}

TEST(sa_redim_two_dims_resizes_last_dimension)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {2, 1}, {3, 1} };
    SAFEARRAY *psa = pCreate(VT_I4, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG j = 1; j <= 3; j++) for (LONG i = 1; i <= 2; i++) { LONG idx[2] = { i, j }, v = i * 10 + j; pPutElement(psa, idx, &v); }
    SAFEARRAYBOUND nb = { 5, 1 };
    EQHR(pRedim(psa, &nb), S_OK);                      /* applies to the rightmost (last) dimension */
    LONG ub;
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 2);
    EQHR(pGetUBound(psa, 2, &ub), S_OK); EQ(ub, 5);
    for (LONG j = 1; j <= 5; j++) for (LONG i = 1; i <= 2; i++) {
        LONG idx[2] = { i, j }, v = -1;
        EQHR(pGetElement(psa, idx, &v), S_OK);
        EQ(v, j <= 3 ? i * 10 + j : 0);
    }
    nb.cElements = 2;
    EQHR(pRedim(psa, &nb), S_OK);
    for (LONG j = 1; j <= 2; j++) for (LONG i = 1; i <= 2; i++) { LONG idx[2] = { i, j }, v = -1; EQHR(pGetElement(psa, idx, &v), S_OK); EQ(v, i * 10 + j); }
    LONG idx[2] = { 1, 3 }, v; EQHR(pGetElement(psa, idx, &v), DISP_E_BADINDEX);
    pDestroy(psa);
}

TEST(sa_redim_bstr_shrink_and_grow)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG i = 0; i < 4; i++) { WCHAR b[8]; wsprintfW(b, L"s%ld", i); BSTR s = bstr_w(b); pPutElement(psa, &i, s); pSysFreeString(s); }
    SAFEARRAYBOUND nb = { 2, 0 };
    EQHR(pRedim(psa, &nb), S_OK);
    BSTR *p = psa->pvData;
    CHECK(p[0] && !lstrcmpW(p[0], L"s0")); CHECK(p[1] && !lstrcmpW(p[1], L"s1"));
    nb.cElements = 5;
    EQHR(pRedim(psa, &nb), S_OK);
    p = psa->pvData;
    CHECK(p[1] && !lstrcmpW(p[1], L"s1"));
    EQ(p[2], NULL); EQ(p[3], NULL); EQ(p[4], NULL);    /* the removed strings do not reappear */
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_redim_unknown_shrink_releases)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_UNKNOWN, 0, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj o; tobj_init(&o, 1);
    for (LONG i = 0; i < 4; i++) pPutElement(psa, &i, OBJ(o));
    EQ(o.ref, 5);
    SAFEARRAYBOUND nb = { 3, 0 };
    EQHR(pRedim(psa, &nb), S_OK);
    EQ(o.ref, 4);                                      /* only the removed element was released */
    nb.cElements = 1;
    EQHR(pRedim(psa, &nb), S_OK);
    EQ(o.ref, 2);
    nb.cElements = 3;
    EQHR(pRedim(psa, &nb), S_OK);
    EQ(o.ref, 2);                                      /* growing adds NULL slots, no references */
    EQ(((void **)psa->pvData)[2], NULL);
    EQHR(pDestroy(psa), S_OK);
    EQ(o.ref, 1);
}

TEST(sa_redim_variant_shrink_clears)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj o; tobj_init(&o, 1);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = OBJ(o);
    for (LONG i = 0; i < 3; i++) pPutElement(psa, &i, &v);
    EQ(o.ref, 4);
    SAFEARRAYBOUND nb = { 1, 0 };
    EQHR(pRedim(psa, &nb), S_OK);
    EQ(o.ref, 2);
    pDestroy(psa);
    EQ(o.ref, 1);
}

TEST(sa_redim_vector_flag_handling)
{
    SA_BEGIN();
    SAFEARRAY *psa = pCreateVector(VT_I4, 0, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG i = 0; i < 4; i++) { LONG v = i + 1; pPutElement(psa, &i, &v); }
    SAFEARRAYBOUND nb = { 2, 0 };
    EQHR(pRedim(psa, &nb), S_OK);                      /* shrink keeps the inline storage */
    CHECK(psa->fFeatures & FADF_CREATEVECTOR);
    EQ(psa->pvData, (BYTE *)psa + sizeof(SAFEARRAY));
    nb.cElements = 8;
    EQHR(pRedim(psa, &nb), S_OK);                      /* growing moves the data out of the descriptor block */
    CHECK(!(psa->fFeatures & FADF_CREATEVECTOR));
    CHECK(psa->pvData != (BYTE *)psa + sizeof(SAFEARRAY));
    LONG v;
    for (LONG i = 0; i < 2; i++) { EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, i + 1); }
    for (LONG i = 2; i < 8; i++) { EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 0); }
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_redim_preserves_features_and_elemsize)
{
    SA_BEGIN();
    for (unsigned t = 0; t < SA_NVALID; t++) {
        VARTYPE vt = SA_VALID_VTS[t];
        SAFEARRAY *psa = sa1(vt, 0, 2);
        if (!psa) { CHECKF(0, "vt=%u", vt); continue; }
        USHORT f = psa->fFeatures; ULONG cb = psa->cbElements;
        SAFEARRAYBOUND nb = { 5, 0 };
        EQHR(pRedim(psa, &nb), S_OK);
        EQ(psa->fFeatures, f); EQ(psa->cbElements, cb);
        EQ(psa->rgsabound[0].cElements, 5);
        EQHR(pDestroy(psa), S_OK);
    }
}

/* ---- IID / vartype ---------------------------------------------------------- */
TEST(sa_iid_args_and_applicability)
{
    SA_BEGIN();
    GUID g; memset(&g, 0x5a, sizeof g);
    EQHR(pSetIID(NULL, &SA_IID_Custom), E_INVALIDARG);
    EQHR(pGetIID(NULL, &g), E_INVALIDARG);
    SAFEARRAY *plain = sa1(VT_I4, 0, 1), *bs = sa1(VT_BSTR, 0, 1), *va = sa1(VT_VARIANT, 0, 1);
    SAFEARRAY *list[3] = { plain, bs, va };
    for (int i = 0; i < 3; i++) {
        if (!list[i]) { CHECK(list[i] != NULL); continue; }
        EQHR(pSetIID(list[i], &SA_IID_Custom), E_INVALIDARG);   /* only FADF_HAVEIID arrays */
        EQHR(pGetIID(list[i], &g), E_INVALIDARG);
        pDestroy(list[i]);
    }
    TRec r; trec_setup(&r, 4);
    SAFEARRAYBOUND b = { 1, 0 };
    SAFEARRAY *rec = pCreateEx(VT_RECORD, 1, &b, REC(r));
    if (rec) { EQHR(pSetIID(rec, &SA_IID_Custom), E_INVALIDARG); EQHR(pGetIID(rec, &g), E_INVALIDARG); pDestroy(rec); }
    SAFEARRAY *u = sa1(VT_UNKNOWN, 0, 1);
    if (u) {
        EQHR(pGetIID(u, NULL), E_INVALIDARG);
        pDestroy(u);
    }
    EQ(((DWORD *)&g)[0], 0x5a5a5a5a);                 /* GUID untouched on failure */
    SAFEARRAY *nodesc = NULL;
    EQHR(pAllocDescriptor(1, &nodesc), S_OK);
    if (nodesc) { EQHR(pSetIID(nodesc, &SA_IID_Custom), E_INVALIDARG); EQHR(pGetIID(nodesc, &g), E_INVALIDARG); pDestroyDescriptor(nodesc); }
}

TEST(sa_iid_set_get_roundtrip)
{
    SA_BEGIN();
    static const VARTYPE vts[] = { VT_UNKNOWN, VT_DISPATCH };
    for (int k = 0; k < 2; k++) {
        SAFEARRAY *psa = sa1(vts[k], 0, 2);
        CHECK(psa != NULL);
        if (!psa) continue;
        GUID g;
        EQHR(pSetIID(psa, &SA_IID_Custom), S_OK);
        EQHR(pGetIID(psa, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom));
        EQHR(pSetIID(psa, &SA_IID_Custom2), S_OK);
        EQHR(pGetIID(psa, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom2));
        VARTYPE vt = 0; EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, vts[k]);   /* vt unaffected by the IID */
        pDestroy(psa);
    }
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptorEx(VT_UNKNOWN, 1, &psa), S_OK);
    if (psa) {
        GUID g; EQHR(pSetIID(psa, &SA_IID_Custom), S_OK); EQHR(pGetIID(psa, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom));
        pDestroyDescriptor(psa);
    }
}

TEST(sa_vartype_args_and_missing_type)
{
    SA_BEGIN();
    VARTYPE vt = 0x7777;
    SAFEARRAY *psa = sa1(VT_I4, 0, 1), *nodesc = NULL;
    EQHR(pGetVartype(NULL, &vt), E_INVALIDARG);
    if (psa) { EQHR(pGetVartype(psa, NULL), E_INVALIDARG); pDestroy(psa); }
    EQHR(pAllocDescriptor(1, &nodesc), S_OK);
    if (nodesc) { EQHR(pGetVartype(nodesc, &vt), E_INVALIDARG); pDestroyDescriptor(nodesc); }
    EQ(vt, 0x7777);
}

TEST(sa_vartype_reads_hidden_dword)
{
    SA_BEGIN();
    /* for FADF_HAVEVARTYPE arrays the vt comes from the 4 bytes preceding the descriptor */
    SAFEARRAY *psa = sa1(VT_I4, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    VARTYPE vt = 0;
    DWORD saved = SA_HIDDEN_VT(psa);
    SA_HIDDEN_VT(psa) = VT_R8;
    EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_R8);
    SA_HIDDEN_VT(psa) = saved;
    EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_I4);
    pDestroy(psa);
}

TEST(sa_vartype_feature_bits_decide)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_UNKNOWN, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    VARTYPE vt = 0;
    EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_UNKNOWN);
    USHORT f = psa->fFeatures;
    psa->fFeatures = (f & ~FADF_UNKNOWN) | FADF_DISPATCH;   /* HAVEIID + DISPATCH bit -> VT_DISPATCH */
    EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_DISPATCH);
    psa->fFeatures = FADF_HAVEIID;                          /* HAVEIID alone -> VT_UNKNOWN */
    EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_UNKNOWN);
    psa->fFeatures = FADF_RECORD | FADF_HAVEIID | FADF_HAVEVARTYPE;   /* RECORD has priority */
    EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_RECORD);
    psa->fFeatures = f;
    pDestroy(psa);
}

/* ---- Destroy / DestroyData / DestroyDescriptor ------------------------------- */
TEST(sa_destroy_null_is_ok)
{
    SA_BEGIN();
    EQHR(pDestroy(NULL), S_OK);
    EQHR(pDestroyData(NULL), S_OK);
    EQHR(pDestroyDescriptor(NULL), S_OK);
}

TEST(sa_destroydata_keeps_descriptor)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {3, 2}, {2, 7} };
    SAFEARRAY *psa = pCreate(VT_I4, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    EQHR(pDestroyData(psa), S_OK);
    EQ(psa->pvData, NULL);
    EQ(psa->cDims, 2); EQ(psa->cbElements, 4);
    EQ(psa->rgsabound[0].cElements, 2); EQ(psa->rgsabound[1].lLbound, 2);
    LONG idx[2] = { 2, 7 }, v;
    if (!t_is_wine()) EQHR(pGetElement(psa, idx, &v), E_INVALIDARG);    /* no data any more */
    EQHR(pDestroyData(psa), S_OK);                    /* idempotent */
    EQHR(pAllocData(psa), S_OK);                      /* the descriptor can be given new data */
    CHECK(psa->pvData != NULL);
    EQHR(pGetElement(psa, idx, &v), S_OK); EQ(v, 0);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_destroy_after_destroydata_and_descriptor)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    BSTR s = bstr_w(L"x"); LONG i = 0; pPutElement(psa, &i, s); pSysFreeString(s);
    EQHR(pDestroyData(psa), S_OK);
    EQHR(pDestroyDescriptor(psa), S_OK);              /* the usual manual teardown pair */
}

TEST(sa_destroy_stack_array_auto)
{
    SA_BEGIN();
    /* FADF_AUTO descriptors and data live in caller memory: elements are released, nothing is freed */
    TObj o; tobj_init(&o, 1);
    IUnknown *slots[3] = { OBJ(o), NULL, OBJ(o) };
    o.ref = 3;
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_AUTO | FADF_UNKNOWN; sa.cbElements = sizeof(void *); sa.pvData = slots;
    sa.rgsabound[0].cElements = 3; sa.rgsabound[0].lLbound = 0;
    EQHR(pDestroy(&sa), S_OK);
    EQ(o.ref, 1);
    EQ(sa.pvData, slots);                              /* storage untouched / not freed */
    EQ(sa.cDims, 1);
}

TEST(sa_destroy_stack_array_embedded)
{
    SA_BEGIN();
    TObj o; tobj_init(&o, 1);
    IUnknown *slots[2] = { OBJ(o), OBJ(o) };
    o.ref = 3;
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_EMBEDDED | FADF_UNKNOWN; sa.cbElements = sizeof(void *); sa.pvData = slots;
    sa.rgsabound[0].cElements = 2;
    EQHR(pDestroy(&sa), S_OK);
    EQ(o.ref, 1);
    EQ(sa.pvData, slots);
}

TEST(sa_destroy_stack_array_static_zeroes_data)
{
    SA_BEGIN();
    TObj o; tobj_init(&o, 1);
    IUnknown *slots[2] = { OBJ(o), OBJ(o) };
    o.ref = 3;
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_STATIC | FADF_UNKNOWN; sa.cbElements = sizeof(void *); sa.pvData = slots;
    sa.rgsabound[0].cElements = 2;
    EQHR(pDestroy(&sa), S_OK);
    EQ(o.ref, 1);
    EQ(sa.pvData, slots);                              /* the memory is not freed ... */
    EQ(slots[0], NULL); EQ(slots[1], NULL);            /* ... but a static array's data is wiped */
}

TEST(sa_destroydata_stack_array_keeps_pointer)
{
    SA_BEGIN();
    TObj o; tobj_init(&o, 3);
    IUnknown *slots[2] = { OBJ(o), OBJ(o) };
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_AUTO | FADF_UNKNOWN; sa.cbElements = sizeof(void *); sa.pvData = slots;
    sa.rgsabound[0].cElements = 2;
    EQHR(pDestroyData(&sa), S_OK);
    EQ(o.ref, 1);
    EQ(sa.pvData, slots);                              /* data of AUTO arrays is never freed or NULLed */
}

TEST(sa_destroy_stack_array_locked)
{
    SA_BEGIN();
    LONG data[2] = { 1, 2 };
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_AUTO; sa.cbElements = 4; sa.pvData = data;
    sa.rgsabound[0].cElements = 2;
    EQHR(pLock(&sa), S_OK);
    EQHR(pDestroy(&sa), DISP_E_ARRAYISLOCKED);
    EQHR(pDestroyData(&sa), DISP_E_ARRAYISLOCKED);
    EQHR(pUnlock(&sa), S_OK);
    EQHR(pDestroy(&sa), S_OK);
    EQ(data[0], 1);                                    /* plain AUTO array: plain data is left alone */
}

TEST(sa_stack_array_element_access)
{
    SA_BEGIN();
    /* hand-built descriptors work with the element accessors without any type information */
    LONG data[4] = { 10, 20, 30, 40 };
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_AUTO | FADF_FIXEDSIZE; sa.cbElements = 4; sa.pvData = data;
    sa.rgsabound[0].cElements = 4; sa.rgsabound[0].lLbound = 2;
    LONG i = 4, v = 0;
    EQHR(pGetElement(&sa, &i, &v), S_OK); EQ(v, 30);
    v = 99; EQHR(pPutElement(&sa, &i, &v), S_OK); EQ(data[2], 99);
    i = 6; EQHR(pGetElement(&sa, &i, &v), DISP_E_BADINDEX);
    SAFEARRAYBOUND nb = { 8, 2 };
    EQHR(pRedim(&sa, &nb), DISP_E_ARRAYISLOCKED);      /* FIXEDSIZE */
    LONG lb, ub;
    EQHR(pGetLBound(&sa, 1, &lb), S_OK); EQ(lb, 2);
    EQHR(pGetUBound(&sa, 1, &ub), S_OK); EQ(ub, 5);
    EQ(pGetDim(&sa), 1); EQ(pGetElemsize(&sa), 4);
}
