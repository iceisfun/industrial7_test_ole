/* SafeArrayGetDim/GetElemsize/GetLBound/GetUBound, Lock/Unlock/AccessData/UnaccessData, PtrOfIndex,
 * Get/PutElement for plain (memcpy) element types and multi-dimensional layout. */
#include "sa.h"

TEST(sa_getdim_elemsize_null)
{
    SA_BEGIN();
    EQ(pGetDim(NULL), 0);
    EQ(pGetElemsize(NULL), 0);
}

TEST(sa_getdim_elemsize_table)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[3] = { {2, 0}, {3, 0}, {4, 0} };
    for (unsigned i = 0; i < SA_NVALID; i++) {
        VARTYPE vt = SA_VALID_VTS[i];
        for (UINT d = 1; d <= 3; d++) {
            SAFEARRAY *psa = pCreate(vt, d, b);
            if (!psa) { CHECKF(0, "vt=%u d=%u", vt, d); continue; }
            EQ(pGetDim(psa), d);
            EQ(pGetElemsize(psa), sa_expect_cb(vt));
            pDestroy(psa);
        }
    }
}

TEST(sa_getbound_errors)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {3, 10}, {4, 20} };
    SAFEARRAY *psa = pCreate(VT_I4, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG v = 0x77;
    EQHR(pGetLBound(NULL, 1, &v), E_INVALIDARG);
    EQHR(pGetUBound(NULL, 1, &v), E_INVALIDARG);
    EQHR(pGetLBound(psa, 1, NULL), E_INVALIDARG);
    EQHR(pGetUBound(psa, 1, NULL), E_INVALIDARG);
    EQHR(pGetLBound(NULL, 1, NULL), E_INVALIDARG);
    EQHR(pGetLBound(psa, 0, &v), DISP_E_BADINDEX);
    EQHR(pGetUBound(psa, 0, &v), DISP_E_BADINDEX);
    EQHR(pGetLBound(psa, 3, &v), DISP_E_BADINDEX);
    EQHR(pGetUBound(psa, 3, &v), DISP_E_BADINDEX);
    EQHR(pGetLBound(psa, 0xffffffff, &v), DISP_E_BADINDEX);
    EQHR(pGetUBound(psa, 0xffffffff, &v), DISP_E_BADINDEX);
    EQ(v, 0x77);                         /* untouched on failure */
    EQHR(pGetLBound(psa, 1, &v), S_OK); EQ(v, 10);
    EQHR(pGetUBound(psa, 1, &v), S_OK); EQ(v, 12);
    EQHR(pGetLBound(psa, 2, &v), S_OK); EQ(v, 20);
    EQHR(pGetUBound(psa, 2, &v), S_OK); EQ(v, 23);
    pDestroy(psa);
}

TEST(sa_getubound_overflow_cElements)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_UI1, 0, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    /* a descriptor claiming >= 2^31 elements cannot be expressed as a LONG upper bound */
    ULONG save = psa->rgsabound[0].cElements;
    psa->rgsabound[0].cElements = 0x80000000;
    LONG ub = 0x55;
    EQHR(pGetUBound(psa, 1, &ub), DISP_E_OVERFLOW);
    psa->rgsabound[0].cElements = save;
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 3);
    pDestroy(psa);
}

TEST(sa_lock_unlock_counts)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(psa->cLocks, 0);
    EQHR(pLock(psa), S_OK); EQ(psa->cLocks, 1);
    EQHR(pLock(psa), S_OK); EQ(psa->cLocks, 2);
    EQHR(pUnlock(psa), S_OK); EQ(psa->cLocks, 1);
    EQHR(pUnlock(psa), S_OK); EQ(psa->cLocks, 0);
    EQHR(pUnlock(psa), E_UNEXPECTED);    /* unbalanced */
    EQ(psa->cLocks, 0);
    pDestroy(psa);
}

TEST(sa_lock_null_and_limit)
{
    SA_BEGIN();
    EQHR(pLock(NULL), E_INVALIDARG);
    EQHR(pUnlock(NULL), E_INVALIDARG);
    EQHR(pUnaccessData(NULL), E_INVALIDARG);
    void *pv = NULL;
    EQHR(pAccessData(NULL, &pv), E_INVALIDARG);
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    if (!psa) { CHECK(psa != NULL); return; }
    EQHR(pAccessData(psa, NULL), E_INVALIDARG);
    EQ(psa->cLocks, 0);                  /* no lock taken on bad args */
    /* the lock count saturates at 0xffff */
    psa->cLocks = 0xfffe;
    EQHR(pLock(psa), S_OK); EQ(psa->cLocks, 0xffff);
    EQHR(pLock(psa), E_UNEXPECTED); EQ(psa->cLocks, 0xffff);
    EQHR(pAccessData(psa, &pv), E_UNEXPECTED);
    EQHR(pUnlock(psa), S_OK); EQ(psa->cLocks, 0xfffe);
    psa->cLocks = 0;
    pDestroy(psa);
}

TEST(sa_accessdata_returns_pvdata)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    void *pv = NULL;
    EQHR(pAccessData(psa, &pv), S_OK);
    EQ(pv, psa->pvData);
    EQ(psa->cLocks, 1);
    ((LONG *)pv)[1] = 99;
    EQHR(pUnaccessData(psa), S_OK);
    EQ(psa->cLocks, 0);
    LONG i = 1, v = 0; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, 99);
    EQHR(pUnaccessData(psa), E_UNEXPECTED);
    pDestroy(psa);
}

TEST(sa_accessdata_no_data)
{
    SA_BEGIN();
    /* a descriptor with no data still locks and returns NULL */
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptor(1, &psa), S_OK);
    if (!psa) return;
    void *pv = (void *)1;
    EQHR(pAccessData(psa, &pv), S_OK);
    EQ(pv, NULL);
    EQ(psa->cLocks, 1);
    EQHR(pUnaccessData(psa), S_OK);
    pDestroyDescriptor(psa);
}

TEST(sa_locked_blocks_destroy_variants)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    EQHR(pLock(psa), S_OK);
    EQHR(pDestroy(psa), DISP_E_ARRAYISLOCKED);
    EQHR(pDestroyData(psa), DISP_E_ARRAYISLOCKED);
    EQHR(pDestroyDescriptor(psa), DISP_E_ARRAYISLOCKED);
    SAFEARRAYBOUND nb = { 5, 0 };
    EQHR(pRedim(psa, &nb), DISP_E_ARRAYISLOCKED);
    CHECK(psa->pvData != NULL);
    EQ(psa->rgsabound[0].cElements, 3);
    EQHR(pUnlock(psa), S_OK);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_locked_allows_element_access)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    EQHR(pLock(psa), S_OK);
    LONG i = 2, v = 5, o = 0;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    EQHR(pGetElement(psa, &i, &o), S_OK); EQ(o, 5);
    EQ(psa->cLocks, 1);                  /* Get/Put restore the lock count */
    void *pp = NULL;
    EQHR(pPtrOfIndex(psa, &i, &pp), S_OK);
    EQ(psa->cLocks, 1);
    EQHR(pUnlock(psa), S_OK);
    pDestroy(psa);
}

TEST(sa_get_put_lock_saturated)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG i = 0, v = 1;
    psa->cLocks = 0xffff;
    EQHR(pPutElement(psa, &i, &v), E_UNEXPECTED);
    EQHR(pGetElement(psa, &i, &v), E_UNEXPECTED);
    EQ(psa->cLocks, 0xffff);
    psa->cLocks = 0xfffe;                /* one more lock still fits; it is released again */
    EQHR(pPutElement(psa, &i, &v), S_OK);
    EQ(psa->cLocks, 0xfffe);
    psa->cLocks = 0;
    pDestroy(psa);
}

TEST(sa_ptrofindex_args)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 5, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG i = 5; void *pp = (void *)0x1;
    EQHR(pPtrOfIndex(NULL, &i, &pp), E_INVALIDARG);
    EQHR(pPtrOfIndex(psa, NULL, &pp), E_INVALIDARG);
    EQHR(pPtrOfIndex(psa, &i, NULL), E_INVALIDARG);
    EQ(pp, 1);
    i = 4; EQHR(pPtrOfIndex(psa, &i, &pp), DISP_E_BADINDEX);
    i = 8; EQHR(pPtrOfIndex(psa, &i, &pp), DISP_E_BADINDEX);
    i = 5; EQHR(pPtrOfIndex(psa, &i, &pp), S_OK); EQ(pp, psa->pvData);
    i = 7; EQHR(pPtrOfIndex(psa, &i, &pp), S_OK); EQ(pp, (BYTE *)psa->pvData + 8);
    pDestroy(psa);
    /* descriptor without data / without element size */
    EQHR(pAllocDescriptor(1, &psa), S_OK);
    if (psa) {
        psa->cbElements = 4; psa->rgsabound[0].cElements = 2;
        i = 0; EQHR(pPtrOfIndex(psa, &i, &pp), E_INVALIDARG);   /* pvData == NULL */
        pDestroyDescriptor(psa);
    }
    psa = sa1(VT_I4, 0, 2);
    if (psa) {
        ULONG cb = psa->cbElements; psa->cbElements = 0;
        i = 0; EQHR(pPtrOfIndex(psa, &i, &pp), E_INVALIDARG);
        psa->cbElements = cb;
        pDestroy(psa);
    }
}

/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_safearray_layout */
TEST(sa_layout_two_dims_column_major)
{
    SA_BEGIN();
    /* first index varies fastest; indices are rgIndices[0..cDims-1] in the order the bounds were given */
    SAFEARRAYBOUND b[2] = { {3, 1}, {4, -1} };      /* dim1: 1..3, dim2: -1..2 */
    SAFEARRAY *psa = pCreate(VT_I4, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG n = 0;
    for (LONG j = -1; j <= 2; j++)
        for (LONG i = 1; i <= 3; i++) {
            LONG idx[2] = { i, j }, v = 1000 + n;
            EQHR(pPutElement(psa, idx, &v), S_OK);
            LONG *raw = psa->pvData;
            EQ(raw[(i - 1) + 3 * (j + 1)], v);
            void *pp = NULL;
            EQHR(pPtrOfIndex(psa, idx, &pp), S_OK);
            EQ(pp, &raw[(i - 1) + 3 * (j + 1)]);
            n++;
        }
    pDestroy(psa);
}

TEST(sa_layout_three_dims)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[3] = { {2, 0}, {3, 10}, {4, -5} };
    SAFEARRAY *psa = pCreate(VT_I2, 3, b);
    CHECK(psa != NULL);
    if (!psa) return;
    SHORT *raw = psa->pvData;
    for (LONG k = -5; k < -1; k++)
        for (LONG j = 10; j < 13; j++)
            for (LONG i = 0; i < 2; i++) {
                LONG idx[3] = { i, j, k };
                SHORT v = (SHORT)(i + 10 * (j - 10) + 100 * (k + 5) + 1);
                EQHR(pPutElement(psa, idx, &v), S_OK);
                EQ(raw[i + 2 * (j - 10) + 6 * (k + 5)], v);
            }
    for (LONG k = -5; k < -1; k++)
        for (LONG j = 10; j < 13; j++)
            for (LONG i = 0; i < 2; i++) {
                LONG idx[3] = { i, j, k }; SHORT v = 0;
                EQHR(pGetElement(psa, idx, &v), S_OK);
                EQ((USHORT)v, (USHORT)(i + 10 * (j - 10) + 100 * (k + 5) + 1));
            }
    pDestroy(psa);
}

TEST(sa_multidim_index_out_of_range_each_dim)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[3] = { {2, 0}, {3, 10}, {4, -5} };
    SAFEARRAY *psa = pCreate(VT_I4, 3, b);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG v = 0;
    LONG bad[][3] = { {-1,10,-5}, {2,10,-5}, {0,9,-5}, {0,13,-5}, {0,10,-6}, {0,10,-1}, {2,13,-1}, {-1,9,-6} };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        void *pp = NULL;
        EQHR(pGetElement(psa, bad[i], &v), DISP_E_BADINDEX);
        EQHR(pPutElement(psa, bad[i], &v), DISP_E_BADINDEX);
        EQHR(pPtrOfIndex(psa, bad[i], &pp), DISP_E_BADINDEX);
    }
    LONG good[3] = { 1, 12, -2 };
    EQHR(pGetElement(psa, good, &v), S_OK);
    pDestroy(psa);
}

TEST(sa_getput_element_args)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG i = 0, v = 1;
    EQHR(pGetElement(NULL, &i, &v), E_INVALIDARG);
    EQHR(pGetElement(psa, NULL, &v), E_INVALIDARG);
    EQHR(pGetElement(psa, &i, NULL), E_INVALIDARG);
    EQHR(pPutElement(NULL, &i, &v), E_INVALIDARG);
    EQHR(pPutElement(psa, NULL, &v), E_INVALIDARG);
    EQ(psa->cLocks, 0);
    i = -1; EQHR(pGetElement(psa, &i, &v), DISP_E_BADINDEX); EQHR(pPutElement(psa, &i, &v), DISP_E_BADINDEX);
    i = 3;  EQHR(pGetElement(psa, &i, &v), DISP_E_BADINDEX); EQHR(pPutElement(psa, &i, &v), DISP_E_BADINDEX);
    i = (LONG)0x80000000; EQHR(pGetElement(psa, &i, &v), DISP_E_BADINDEX);
    i = 0x7fffffff; EQHR(pGetElement(psa, &i, &v), DISP_E_BADINDEX);
    EQ(psa->cLocks, 0);
    pDestroy(psa);
}

TEST(sa_getput_element_descriptor_only)
{
    SA_BEGIN();
    /* no data or no element size -> E_INVALIDARG (checked before the index) */
    if (t_is_wine()) { t_skip("emulator dereferences NULL pvData here"); return; }
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptor(1, &psa), S_OK);
    if (!psa) return;
    psa->cbElements = 4; psa->rgsabound[0].cElements = 2;
    LONG i = 0, v = 0;
    EQHR(pGetElement(psa, &i, &v), E_INVALIDARG);
    EQHR(pPutElement(psa, &i, &v), E_INVALIDARG);
    i = 99; EQHR(pGetElement(psa, &i, &v), E_INVALIDARG);
    EQ(psa->cLocks, 0);
    pDestroyDescriptor(psa);
}

TEST(sa_getput_zero_cbelements_is_invalid)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I4, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    ULONG cb = psa->cbElements; psa->cbElements = 0;
    LONG i = 0, v = 0;
    EQHR(pGetElement(psa, &i, &v), E_INVALIDARG);
    EQHR(pPutElement(psa, &i, &v), E_INVALIDARG);
    psa->cbElements = cb;
    pDestroy(psa);
}

/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_SafeArrayGetPutElement; table driven over all plain types */
TEST(sa_getput_plain_types_roundtrip)
{
    SA_BEGIN();
    for (unsigned t = 0; t < SA_NVALID; t++) {
        VARTYPE vt = SA_VALID_VTS[t];
        if (vt == VT_BSTR || vt == VT_VARIANT || vt == VT_UNKNOWN || vt == VT_DISPATCH) continue;
        ULONG cb = sa_expect_cb(vt);
        SAFEARRAY *psa = sa1(vt, -1, 4);
        if (!psa) { CHECKF(0, "create vt=%u", vt); continue; }
        BYTE in[16], out[16];
        for (LONG i = -1; i <= 2; i++) {
            for (ULONG k = 0; k < cb; k++) in[k] = (BYTE)(0x11 * (i + 2) + k + 1);
            EQHR(pPutElement(psa, &i, in), S_OK);
        }
        for (LONG i = -1; i <= 2; i++) {
            memset(out, 0xee, sizeof out);
            EQHR(pGetElement(psa, &i, out), S_OK);
            for (ULONG k = 0; k < cb; k++) in[k] = (BYTE)(0x11 * (i + 2) + k + 1);
            CHECKF(!memcmp(in, out, cb), "vt=%u idx=%ld mismatch", vt, i);
            for (ULONG k = cb; k < 16; k++) if (out[k] != 0xee) { CHECKF(0, "vt=%u wrote past element", vt); break; }
            BYTE *raw = (BYTE *)psa->pvData + (i + 1) * cb;
            CHECKF(!memcmp(raw, in, cb), "vt=%u raw storage", vt);
        }
        pDestroy(psa);
    }
}

TEST(sa_getput_overwrite_neighbours_untouched)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_I2, 0, 5);
    CHECK(psa != NULL);
    if (!psa) return;
    SHORT *raw = psa->pvData;
    for (int i = 0; i < 5; i++) raw[i] = (SHORT)(i + 1);
    LONG i = 2; SHORT v = 0x4242;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    EQ(raw[0], 1); EQ(raw[1], 2); EQ(raw[2], 0x4242); EQ(raw[3], 4); EQ(raw[4], 5);
    pDestroy(psa);
}

TEST(sa_getput_decimal_cy_date)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_DECIMAL, 0, 2);
    CHECK(psa != NULL);
    if (psa) {
        DECIMAL in, out; memset(&in, 0, sizeof in);
        in.Lo64 = 0x1122334455667788ULL; in.Hi32 = 0x99aabbcc; in.scale = 3; in.sign = 0x80; in.wReserved = 0;
        LONG i = 1; memset(&out, 0xcc, sizeof out);
        EQHR(pPutElement(psa, &i, &in), S_OK);
        EQHR(pGetElement(psa, &i, &out), S_OK);
        CHECK(!memcmp(&in, &out, sizeof in));
        EQ(pGetElemsize(psa), sizeof(DECIMAL));
        pDestroy(psa);
    }
    psa = sa1(VT_CY, 0, 2);
    CHECK(psa != NULL);
    if (psa) {
        CY in, out; in.int64 = -1234567890123LL;
        LONG i = 0;
        EQHR(pPutElement(psa, &i, &in), S_OK);
        EQHR(pGetElement(psa, &i, &out), S_OK);
        CHECK(in.int64 == out.int64);
        pDestroy(psa);
    }
    psa = sa1(VT_DATE, 0, 2);
    CHECK(psa != NULL);
    if (psa) {
        DATE in = 36525.5, out = 0; LONG i = 1;
        EQHR(pPutElement(psa, &i, &in), S_OK);
        EQHR(pGetElement(psa, &i, &out), S_OK);
        CHECK(in == out);
        pDestroy(psa);
    }
}

TEST(sa_getput_vt_bool_error_hresult_values)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BOOL, 0, 2);
    CHECK(psa != NULL);
    if (psa) {
        VARIANT_BOOL t = VARIANT_TRUE, f = VARIANT_FALSE, o = 5; LONG i = 0;
        EQHR(pPutElement(psa, &i, &t), S_OK); i = 1; EQHR(pPutElement(psa, &i, &f), S_OK);
        i = 0; EQHR(pGetElement(psa, &i, &o), S_OK); EQ((USHORT)o, 0xffff);
        i = 1; EQHR(pGetElement(psa, &i, &o), S_OK); EQ(o, 0);
        pDestroy(psa);
    }
    psa = sa1(VT_ERROR, 0, 2);
    CHECK(psa != NULL);
    if (psa) {
        SCODE sc = DISP_E_PARAMNOTFOUND, o = 0; LONG i = 1;
        EQHR(pPutElement(psa, &i, &sc), S_OK); EQHR(pGetElement(psa, &i, &o), S_OK); EQHR(o, DISP_E_PARAMNOTFOUND);
        pDestroy(psa);
    }
}

TEST(sa_vector_getput_and_access)
{
    SA_BEGIN();
    SAFEARRAY *psa = pCreateVector(VT_R8, 100, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG i = 100; i < 104; i++) { double d = i * 1.5; EQHR(pPutElement(psa, &i, &d), S_OK); }
    double *raw = NULL;
    EQHR(pAccessData(psa, (void **)&raw), S_OK);
    if (raw) for (int k = 0; k < 4; k++) CHECK(raw[k] == (100 + k) * 1.5);
    EQHR(pUnaccessData(psa), S_OK);
    LONG i = 99; double d = 0; EQHR(pGetElement(psa, &i, &d), DISP_E_BADINDEX);
    i = 104; EQHR(pGetElement(psa, &i, &d), DISP_E_BADINDEX);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_ptrofindex_all_vts_stride)
{
    SA_BEGIN();
    for (unsigned t = 0; t < SA_NVALID; t++) {
        VARTYPE vt = SA_VALID_VTS[t];
        SAFEARRAY *psa = sa1(vt, 3, 4);
        if (!psa) { CHECKF(0, "vt=%u", vt); continue; }
        ULONG cb = sa_expect_cb(vt);
        for (LONG i = 3; i < 7; i++) {
            void *pp = NULL;
            EQHR(pPtrOfIndex(psa, &i, &pp), S_OK);
            EQ(pp, (BYTE *)psa->pvData + (i - 3) * cb);
        }
        pDestroy(psa);
    }
}
