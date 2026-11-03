/* Additional SafeArray element/lock/index edge cases checked against the Win10 oleaut32 decompilation. */
#include "sa.h"

TEST(sa_extra_getput_error_paths_restore_lock_count)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 10, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    EQHR(pLock(psa), S_OK); EQHR(pLock(psa), S_OK);
    EQ(psa->cLocks, 2);
    LONG bad[] = { 9, 13, -1 }; INT v = 7;
    for (int k = 0; k < 3; k++) {
        EQHR(pGetElement(psa, &bad[k], &v), DISP_E_BADINDEX);
        EQHR(pPutElement(psa, &bad[k], &v), DISP_E_BADINDEX);
        EQ(psa->cLocks, 2);
    }
    LONG ok = 12;
    EQHR(pPutElement(psa, &ok, &v), S_OK);
    INT w = 0; EQHR(pGetElement(psa, &ok, &w), S_OK); EQ(w, 7);
    EQ(psa->cLocks, 2);
    EQHR(pUnlock(psa), S_OK); EQHR(pUnlock(psa), S_OK);
    EQ(psa->cLocks, 0);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_extra_unknown_put_null_releases_old)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_UNKNOWN, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj a; tobj_init(&a, 1);
    LONG i = 0;
    EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    EQ(a.ref, 2);
    EQHR(pPutElement(psa, &i, NULL), S_OK);          /* NULL pv is a valid interface pointer here */
    EQ(a.ref, 1);
    EQ(((void **)psa->pvData)[0], NULL);
    IUnknown *got = (IUnknown *)(ULONG_PTR)1;
    EQHR(pGetElement(psa, &i, &got), S_OK);          /* empty slot reads back as NULL, no AddRef */
    EQ(got, NULL);
    EQ(a.ref, 1);
    EQHR(pDestroy(psa), S_OK);
    EQ(a.ref, 1);
}

TEST(sa_extra_unknown_put_replaces_old_object)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_UNKNOWN, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj a, b; tobj_init(&a, 1); tobj_init(&b, 1);
    LONG i = 0;
    EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    EQHR(pPutElement(psa, &i, OBJ(b)), S_OK);
    EQ(a.ref, 1); EQ(a.nAddRef, 1); EQ(a.nRelease, 1);
    EQ(b.ref, 2); EQ(b.nAddRef, 1); EQ(b.nRelease, 0);
    EQ(((void **)psa->pvData)[0], &b);
    IUnknown *got = NULL;
    EQHR(pGetElement(psa, &i, &got), S_OK);
    EQ(got, &b); EQ(b.ref, 3);
    ((IUnknown *)got)->lpVtbl->Release(got);
    EQ(b.ref, 2);
    EQHR(pDestroy(psa), S_OK);
    EQ(b.ref, 1);
}

TEST(sa_extra_negative_lbound_multidim_offsets)
{
    SA_BEGIN();
    /* SafeArrayCreate bounds are given left-to-right (bound 0 pairs with rgIndices[0]); rgIndices[0] varies fastest in memory */
    SAFEARRAYBOUND b[2] = { { 3, -2 }, { 4, 5 } };
    SAFEARRAY *psa = pCreate(VT_I4, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG x = 5; x <= 8; x++)
        for (LONG y = -2; y <= 0; y++) {
            LONG idx[2] = { y, x };
            INT *p = NULL;
            EQHR(pPtrOfIndex(psa, idx, (void **)&p), S_OK);
            EQ(p, (INT *)psa->pvData + ((x - 5) * 3 + (y + 2)));
            INT val = x * 100 + y;
            EQHR(pPutElement(psa, idx, &val), S_OK);
        }
    for (LONG x = 5; x <= 8; x++)
        for (LONG y = -2; y <= 0; y++) {
            LONG idx[2] = { y, x }; INT got = 0;
            EQHR(pGetElement(psa, idx, &got), S_OK);
            EQ(got, x * 100 + y);
        }
    LONG e1[2] = { -3, 5 }, e2[2] = { 1, 5 }, e3[2] = { 0, 4 }, e4[2] = { 0, 9 };
    void *pp = NULL;
    EQHR(pPtrOfIndex(psa, e1, &pp), DISP_E_BADINDEX);
    EQHR(pPtrOfIndex(psa, e2, &pp), DISP_E_BADINDEX);
    EQHR(pPtrOfIndex(psa, e3, &pp), DISP_E_BADINDEX);
    EQHR(pPtrOfIndex(psa, e4, &pp), DISP_E_BADINDEX);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_extra_float_bit_patterns_preserved)
{
    SA_BEGIN();
    /* non-BSTR/VARIANT/object element types are raw memcpy: payload bits of NaNs and sign of zero survive */
    SAFEARRAY *pf = sa1(VT_R4, 0, 2), *pd = sa1(VT_R8, 0, 2), *pq = sa1(VT_UI8, 0, 1);
    CHECK(pf && pd && pq);
    if (pf && pd && pq) {
        DWORD f1 = 0x7fc12345, f2 = 0x80000000, g;
        ULONGLONG d1 = 0x7ff8dead0000beefULL, d2 = 0x8000000000000000ULL, h, q = 0xfedcba9876543210ULL, r;
        LONG i = 0;
        EQHR(pPutElement(pf, &i, &f1), S_OK); i = 1; EQHR(pPutElement(pf, &i, &f2), S_OK);
        i = 0; EQHR(pGetElement(pf, &i, &g), S_OK); EQ(g, f1);
        i = 1; EQHR(pGetElement(pf, &i, &g), S_OK); EQ(g, f2);
        i = 0; EQHR(pPutElement(pd, &i, &d1), S_OK); i = 1; EQHR(pPutElement(pd, &i, &d2), S_OK);
        i = 0; EQHR(pGetElement(pd, &i, &h), S_OK); CHECK(h == d1);
        i = 1; EQHR(pGetElement(pd, &i, &h), S_OK); CHECK(h == d2);
        i = 0; EQHR(pPutElement(pq, &i, &q), S_OK); EQHR(pGetElement(pq, &i, &r), S_OK); CHECK(r == q);
    }
    if (pf) pDestroy(pf);
    if (pd) pDestroy(pd);
    if (pq) pDestroy(pq);
}

TEST(sa_extra_get_does_not_touch_array_on_failure)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I2, 0, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    SHORT s = 0x1234; LONG i = 2;
    EQHR(pPutElement(psa, &i, &s), S_OK);
    SHORT out = 0x5555; LONG bad = 4;
    EQHR(pGetElement(psa, &bad, &out), DISP_E_BADINDEX);
    EQ(out, 0x5555);                                  /* destination untouched on failure */
    SHORT *d = psa->pvData;
    EQ(d[0], 0); EQ(d[1], 0); EQ(d[2], 0x1234); EQ(d[3], 0);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_extra_variant_slot_get_into_dirty_destination)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    VARIANT in, out; pVariantInit(&in); pVariantInit(&out);
    V_VT(&in) = VT_I4; V_I4(&in) = 99;
    LONG i = 0;
    EQHR(pPutElement(psa, &i, &in), S_OK);
    /* Get overwrites the destination (VT field is reset before VariantCopy; no clear of previous content needed for scalars) */
    V_VT(&out) = VT_I2; V_I2(&out) = 5;
    EQHR(pGetElement(psa, &i, &out), S_OK);
    EQ(V_VT(&out), VT_I4); EQ(V_I4(&out), 99);
    /* slot content is independent of the destination */
    V_I4(&out) = 1;
    VARIANT again; pVariantInit(&again);
    EQHR(pGetElement(psa, &i, &again), S_OK);
    EQ(V_I4(&again), 99);
    EQHR(pDestroy(psa), S_OK);
}
