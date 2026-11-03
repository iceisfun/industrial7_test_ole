/* SafeArrayCreate / CreateEx / CreateVector / CreateVectorEx / AllocDescriptor[Ex] / AllocData.
 * Expectations verified against the Windows 10 oleaut32 decompilation (SafeArrayCreate, SafeArrayCreateEx,
 * SafeArrayCreateVector[Ex], SafeArrayAllocDescriptor[Ex], SafeArrayAllocData, SafeArrayElementSizeFeatures,
 * SafeArraySize3). */
#include "sa.h"

static int is_valid_vt(VARTYPE vt)
{
    for (unsigned i = 0; i < SA_NVALID; i++) if (SA_VALID_VTS[i] == vt) return 1;
    return 0;
}

/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+) - vt validity matrix, extended to the whole range */
TEST(sa_create_vt_matrix)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 3, 0 };
    for (unsigned vt = 0; vt <= 0x48 /* VT_CLSID is 72 */; vt++) {
        SAFEARRAY *psa = pCreate((VARTYPE)vt, 1, &b);
        if (is_valid_vt((VARTYPE)vt)) {
            CHECKF(psa != NULL, "vt=%u must be creatable", vt);
            if (!psa) continue;
            EQ(psa->cbElements, sa_expect_cb((VARTYPE)vt));
            EQ(psa->fFeatures, sa_expect_features((VARTYPE)vt));
            EQ(psa->cDims, 1);
            EQ(psa->cLocks, 0);
            CHECK(psa->pvData != NULL);
            EQHR(pDestroy(psa), S_OK);
        } else {
            CHECKF(psa == NULL, "vt=%u must be rejected", vt);
            if (psa) pDestroy(psa);
        }
    }
}

TEST(sa_create_vt_flag_values_rejected)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 2, 0 };
    static const unsigned bad[] = { VT_I4 | VT_BYREF, VT_I4 | VT_ARRAY, VT_I4 | VT_VECTOR, VT_BSTR | VT_BYREF,
        VT_VARIANT | VT_ARRAY, 0x7fff, 0xffff, 0x100, VT_I4 | 0x8000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SAFEARRAY *psa = pCreate((VARTYPE)bad[i], 1, &b);
        CHECKF(psa == NULL, "vt=0x%x must be rejected", bad[i]);
        if (psa) pDestroy(psa);
        psa = pCreateVector((VARTYPE)bad[i], 0, 2);
        CHECKF(psa == NULL, "vector vt=0x%x must be rejected", bad[i]);
        if (psa) pDestroy(psa);
    }
}

TEST(sa_create_vt_matrix_ex)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 3, 1 };
    for (unsigned vt = 0; vt <= 0x48; vt++) {
        SAFEARRAY *psa = pCreateEx((VARTYPE)vt, 1, &b, NULL);
        if (is_valid_vt((VARTYPE)vt)) {
            CHECKF(psa != NULL, "Ex vt=%u must be creatable", vt);
            if (!psa) continue;
            EQ(psa->cbElements, sa_expect_cb((VARTYPE)vt));
            EQ(psa->fFeatures, sa_expect_features((VARTYPE)vt));
            EQ(psa->rgsabound[0].lLbound, 1);
            EQ(psa->rgsabound[0].cElements, 3);
            EQHR(pDestroy(psa), S_OK);
        } else {
            /* VT_RECORD (36) without IRecordInfo is rejected too */
            CHECKF(psa == NULL, "Ex vt=%u must be rejected", vt);
            if (psa) pDestroy(psa);
        }
    }
}

TEST(sa_create_vt_matrix_vector)
{
    SA_BEGIN();
    for (unsigned vt = 0; vt <= 0x48; vt++) {
        SAFEARRAY *psa = pCreateVector((VARTYPE)vt, -2, 4);
        SAFEARRAY *pse = pCreateVectorEx((VARTYPE)vt, -2, 4, NULL);
        if (is_valid_vt((VARTYPE)vt)) {
            CHECKF(psa != NULL && pse != NULL, "vector vt=%u must be creatable", vt);
            if (psa) {
                EQ(psa->cbElements, sa_expect_cb((VARTYPE)vt));
                EQ(psa->fFeatures, sa_expect_features((VARTYPE)vt) | FADF_CREATEVECTOR);
                EQ(psa->rgsabound[0].lLbound, -2);
                EQ(psa->rgsabound[0].cElements, 4);
                EQHR(pDestroy(psa), S_OK);
            }
            if (pse) {
                EQ(pse->cbElements, sa_expect_cb((VARTYPE)vt));
                EQ(pse->fFeatures, sa_expect_features((VARTYPE)vt) | FADF_CREATEVECTOR);
                EQHR(pDestroy(pse), S_OK);
            }
        } else {
            CHECKF(psa == NULL && pse == NULL, "vector vt=%u must be rejected", vt);
            if (psa) pDestroy(psa);
            if (pse) pDestroy(pse);
        }
    }
}

TEST(sa_create_vt_matrix_alloc_descriptor_ex)
{
    SA_BEGIN();
    /* AllocDescriptorEx accepts any vt; for plain types it only records the vt (hidden DWORD, FADF_HAVEVARTYPE) */
    static const unsigned vts[] = { VT_EMPTY, VT_NULL, VT_I2, VT_I4, VT_R8, VT_BSTR, VT_VARIANT, VT_I1, VT_UI1, VT_VOID, VT_HRESULT };
    for (unsigned i = 0; i < sizeof vts / sizeof vts[0]; i++) {
        SAFEARRAY *psa = NULL;
        EQHR(pAllocDescriptorEx((VARTYPE)vts[i], 2, &psa), S_OK);
        if (!psa) continue;
        EQ(psa->cDims, 2);
        EQ(psa->cbElements, 0);
        EQ(psa->cLocks, 0);
        EQ(psa->pvData, NULL);
        EQ(psa->fFeatures, FADF_HAVEVARTYPE);
        EQ(SA_HIDDEN_VT(psa), vts[i]);
        VARTYPE got = 0xdead;
        EQHR(pGetVartype(psa, &got), S_OK);
        EQ(got, vts[i]);
        EQHR(pDestroyDescriptor(psa), S_OK);
    }
}

TEST(sa_alloc_descriptor_ex_special_vts)
{
    SA_BEGIN();
    SAFEARRAY *psa = NULL;
    GUID g;
    EQHR(pAllocDescriptorEx(VT_UNKNOWN, 1, &psa), S_OK);
    if (psa) {
        EQ(psa->fFeatures, FADF_HAVEIID);   /* FADF_UNKNOWN is added by SafeArrayCreate*, not by AllocDescriptorEx */
        EQHR(pGetIID(psa, &g), S_OK);
        CHECK(IsEqualGUID(&g, &SA_IID_IUnknown));
        VARTYPE vt = 0; EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_UNKNOWN);
        pDestroyDescriptor(psa);
    }
    psa = NULL;
    EQHR(pAllocDescriptorEx(VT_DISPATCH, 1, &psa), S_OK);
    if (psa) {
        EQ(psa->fFeatures, FADF_HAVEIID);
        EQHR(pGetIID(psa, &g), S_OK);
        CHECK(IsEqualGUID(&g, &SA_IID_IDispatch));
        pDestroyDescriptor(psa);
    }
    psa = NULL;
    EQHR(pAllocDescriptorEx(VT_RECORD, 1, &psa), S_OK);
    if (psa) {
        EQ(psa->fFeatures, FADF_RECORD);
        VARTYPE vt = 0; EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_RECORD);
        IRecordInfo *ri = (IRecordInfo *)1;
        EQHR(pGetRecordInfo(psa, &ri), S_OK);
        EQ(ri, NULL);
        pDestroyDescriptor(psa);
    }
}

TEST(sa_alloc_descriptor_args)
{
    SA_BEGIN();
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptor(0, &psa), E_INVALIDARG);
    EQ(psa, NULL);
    EQHR(pAllocDescriptor(65536, &psa), E_INVALIDARG);
    EQ(psa, NULL);
    EQHR(pAllocDescriptor(0xffffffff, &psa), E_INVALIDARG);
    /* dims are validated before the out pointer */
    EQHR(pAllocDescriptor(0, NULL), E_INVALIDARG);
    EQHR(pAllocDescriptor(1, NULL), E_POINTER);
    EQHR(pAllocDescriptorEx(VT_I4, 0, &psa), E_INVALIDARG);
    EQHR(pAllocDescriptorEx(VT_I4, 65536, &psa), E_INVALIDARG);
    EQHR(pAllocDescriptorEx(VT_I4, 0, NULL), E_INVALIDARG);
    EQHR(pAllocDescriptorEx(VT_I4, 1, NULL), E_POINTER);
    EQ(psa, NULL);
}

TEST(sa_alloc_descriptor_fresh_state)
{
    SA_BEGIN();
    for (UINT dims = 1; dims <= 4; dims++) {
        SAFEARRAY *psa = NULL;
        EQHR(pAllocDescriptor(dims, &psa), S_OK);
        if (!psa) continue;
        EQ(psa->cDims, dims);
        EQ(psa->fFeatures, 0);
        EQ(psa->cbElements, 0);
        EQ(psa->cLocks, 0);
        EQ(psa->pvData, NULL);
        for (UINT d = 0; d < dims; d++) { EQ(psa->rgsabound[d].cElements, 0); EQ(psa->rgsabound[d].lLbound, 0); }
        EQ(pGetDim(psa), dims);
        EQ(pGetElemsize(psa), 0);
        VARTYPE vt;
        EQHR(pGetVartype(psa, &vt), E_INVALIDARG);   /* no vt feature */
        EQHR(pDestroyDescriptor(psa), S_OK);
    }
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptor(65535, &psa), S_OK);
    if (psa) { EQ(psa->cDims, 65535); EQHR(pDestroyDescriptor(psa), S_OK); }
}

TEST(sa_alloc_data_manual_build)
{
    SA_BEGIN();
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptor(2, &psa), S_OK);
    if (!psa) return;
    psa->cbElements = 4;
    psa->rgsabound[0].cElements = 3; psa->rgsabound[0].lLbound = 0;   /* rightmost dimension */
    psa->rgsabound[1].cElements = 2; psa->rgsabound[1].lLbound = 10;
    EQHR(pAllocData(psa), S_OK);
    CHECK(psa->pvData != NULL);
    if (psa->pvData) {
        const BYTE *p = psa->pvData; int zero = 1;
        for (int i = 0; i < 24; i++) if (p[i]) zero = 0;
        CHECK(zero);                     /* data is zero filled */
    }
    LONG idx[2] = { 11, 2 }, v = 0x1234;
    EQHR(pPutElement(psa, idx, &v), S_OK);
    LONG lb = 0, ub = 0;
    EQHR(pGetLBound(psa, 1, &lb), S_OK); EQ(lb, 10);
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 11);
    EQHR(pGetUBound(psa, 2, &ub), S_OK); EQ(ub, 2);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_alloc_data_errors)
{
    SA_BEGIN();
    EQHR(pAllocData(NULL), E_INVALIDARG);
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptor(1, &psa), S_OK);
    if (!psa) return;
    /* size computation overflow is reported as out of memory without attempting an allocation */
    psa->cbElements = 1; psa->rgsabound[0].cElements = 0xffffffff;
    EQHR(pAllocData(psa), E_OUTOFMEMORY);
    psa->cbElements = 8; psa->rgsabound[0].cElements = 0x20000000;
    EQHR(pAllocData(psa), E_OUTOFMEMORY);
    EQ(psa->pvData, NULL);
    /* zero elements: allocation of zero bytes still succeeds */
    psa->cbElements = 4; psa->rgsabound[0].cElements = 0;
    EQHR(pAllocData(psa), S_OK);
    CHECK(psa->pvData != NULL);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_alloc_data_clears_vector_flag)
{
    SA_BEGIN();
    /* AllocData on a CREATEVECTOR array replaces the inline data and clears the flag */
    SAFEARRAY *psa = pCreateVector(VT_I4, 0, 4);
    if (!psa) { CHECK(psa != NULL); return; }
    CHECK(psa->fFeatures & FADF_CREATEVECTOR);
    void *inl = psa->pvData;
    EQHR(pDestroyData(psa), S_OK);       /* releases the inline data (nothing freed) and clears the flag */
    EQ(psa->pvData, NULL);
    CHECK(!(psa->fFeatures & FADF_CREATEVECTOR));
    EQHR(pAllocData(psa), S_OK);
    CHECK(psa->pvData != NULL);
    EQHR(pDestroy(psa), S_OK);
}

/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+) */
TEST(sa_create_dims_limits)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[4] = { {1,0},{1,0},{1,0},{1,0} };
    EQ(pCreate(VT_I4, 0, b), NULL);
    EQ(pCreate(VT_I4, 65536, b), NULL);
    EQ(pCreate(VT_I4, 0xffffffff, b), NULL);
    EQ(pCreate(VT_I4, 1, NULL), NULL);
    EQ(pCreateEx(VT_I4, 0, b, NULL), NULL);
    EQ(pCreateEx(VT_I4, 65536, b, NULL), NULL);
    EQ(pCreateEx(VT_I4, 1, NULL, NULL), NULL);
    EQ(pCreate(VT_I4, 0, NULL), NULL);
    for (UINT d = 1; d <= 4; d++) {
        SAFEARRAY *psa = pCreate(VT_I4, d, b);
        CHECK(psa != NULL);
        if (psa) { EQ(psa->cDims, d); EQ(pGetDim(psa), d); EQHR(pDestroy(psa), S_OK); }
    }
}

TEST(sa_create_max_dims)
{
    SA_BEGIN();
    SAFEARRAYBOUND *b = calloc(65536, sizeof *b);
    if (!b) return;
    for (int i = 0; i < 65536; i++) { b[i].cElements = 1; b[i].lLbound = i % 7; }
    SAFEARRAY *psa = pCreate(VT_UI1, 65535, b);
    CHECK(psa != NULL);
    if (psa) {
        EQ(pGetDim(psa), 65535);
        LONG lb = 0, ub = 0;
        EQHR(pGetLBound(psa, 1, &lb), S_OK); EQ(lb, 0);
        EQHR(pGetLBound(psa, 65535, &lb), S_OK); EQ(lb, 65534 % 7);
        EQHR(pGetUBound(psa, 65535, &ub), S_OK); EQ(ub, 65534 % 7);
        EQHR(pGetLBound(psa, 65536, &lb), DISP_E_BADINDEX);
        EQ(psa->cbElements, 1);
        EQHR(pDestroy(psa), S_OK);
    }
    EQ(pCreate(VT_UI1, 65536, b), NULL);
    free(b);
}

TEST(sa_create_bounds_and_dim_order)
{
    SA_BEGIN();
    /* SafeArrayCreate stores the bounds reversed: rgsabound[0] is the LAST dimension passed in */
    SAFEARRAYBOUND b[3] = { {3, 5}, {4, -2}, {2, 100} };
    SAFEARRAY *psa = pCreate(VT_I2, 3, b);
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(psa->rgsabound[0].cElements, 2);  EQ(psa->rgsabound[0].lLbound, 100);
    EQ(psa->rgsabound[1].cElements, 4);  EQ(psa->rgsabound[1].lLbound, -2);
    EQ(psa->rgsabound[2].cElements, 3);  EQ(psa->rgsabound[2].lLbound, 5);
    LONG lb, ub;
    EQHR(pGetLBound(psa, 1, &lb), S_OK); EQ(lb, 5);
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 7);
    EQHR(pGetLBound(psa, 2, &lb), S_OK); EQ(lb, -2);
    EQHR(pGetUBound(psa, 2, &ub), S_OK); EQ(ub, 1);
    EQHR(pGetLBound(psa, 3, &lb), S_OK); EQ(lb, 100);
    EQHR(pGetUBound(psa, 3, &ub), S_OK); EQ(ub, 101);
    EQHR(pDestroy(psa), S_OK);
    /* the same through CreateEx */
    psa = pCreateEx(VT_I2, 3, b, NULL);
    CHECK(psa != NULL);
    if (psa) {
        EQ(psa->rgsabound[0].lLbound, 100);
        EQ(psa->rgsabound[2].lLbound, 5);
        EQHR(pDestroy(psa), S_OK);
    }
}

TEST(sa_create_input_bounds_not_modified)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {3, 5}, {4, -2} };
    SAFEARRAY *psa = pCreate(VT_I4, 2, b);
    EQ(b[0].cElements, 3); EQ(b[0].lLbound, 5); EQ(b[1].cElements, 4); EQ(b[1].lLbound, -2);
    if (psa) pDestroy(psa);
}

TEST(sa_create_zero_elements)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 0, 0 };
    for (unsigned i = 0; i < SA_NVALID; i++) {
        VARTYPE vt = SA_VALID_VTS[i];
        SAFEARRAY *psa = pCreate(vt, 1, &b);
        CHECKF(psa != NULL, "empty array vt=%u", vt);
        if (!psa) continue;
        CHECK(psa->pvData != NULL);
        EQ(psa->cbElements, sa_expect_cb(vt));
        LONG ub = 99;
        EQHR(pGetUBound(psa, 1, &ub), S_OK);
        EQ(ub, -1);
        LONG idx = 0; char buf[32] = {0};
        EQHR(pGetElement(psa, &idx, buf), DISP_E_BADINDEX);
        EQHR(pDestroy(psa), S_OK);
    }
    SAFEARRAY *psa = pCreateVector(VT_I4, 0, 0);
    CHECK(psa != NULL);
    if (psa) { LONG ub = 7; EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, -1); EQHR(pDestroy(psa), S_OK); }
}

TEST(sa_create_zero_in_one_dim_of_many)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {0, 0}, {5, 0} };
    SAFEARRAY *psa = pCreate(VT_R8, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG idx[2] = { 0, 0 }; double d;
    EQHR(pGetElement(psa, idx, &d), DISP_E_BADINDEX);
    LONG ub; EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, -1);
    EQHR(pGetUBound(psa, 2, &ub), S_OK); EQ(ub, 4);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_create_negative_lbound)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 5, -3 };
    SAFEARRAY *psa = pCreate(VT_I4, 1, &b);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG lb, ub;
    EQHR(pGetLBound(psa, 1, &lb), S_OK); EQ(lb, -3);
    EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 1);
    for (LONG i = -3; i <= 1; i++) { LONG v = i * 100; EQHR(pPutElement(psa, &i, &v), S_OK); }
    for (LONG i = -3; i <= 1; i++) { LONG v = 0; EQHR(pGetElement(psa, &i, &v), S_OK); EQ(v, i * 100); }
    LONG bad = -4, v = 0; EQHR(pGetElement(psa, &bad, &v), DISP_E_BADINDEX);
    bad = 2; EQHR(pGetElement(psa, &bad, &v), DISP_E_BADINDEX);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_create_extreme_lbound)
{
    SA_BEGIN();
    /* lbound INT_MAX with one element: UBound == INT_MAX; two elements overflow the LONG */
    SAFEARRAY *psa = sa1(VT_I1, 0x7fffffff, 1);
    CHECK(psa != NULL);
    if (psa) {
        LONG ub = 0; EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, 0x7fffffff);
        LONG i = 0x7fffffff; signed char c = 5, d = 0;
        EQHR(pPutElement(psa, &i, &c), S_OK); EQHR(pGetElement(psa, &i, &d), S_OK); EQ(d, 5);
        EQHR(pDestroy(psa), S_OK);
    }
    psa = sa1(VT_I1, 0x7fffffff, 2);
    CHECK(psa != NULL);
    if (psa) {
        LONG ub = 0;
        EQHR(pGetUBound(psa, 1, &ub), DISP_E_OVERFLOW);
        EQ(ub, -1);
        EQHR(pDestroy(psa), S_OK);
    }
    psa = sa1(VT_I1, (LONG)0x80000000, 1);
    CHECK(psa != NULL);
    if (psa) {
        LONG ub = 5; EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, (LONG)0x80000000);
        EQHR(pDestroy(psa), S_OK);
    }
    /* lbound INT_MIN, zero elements: UBound would be INT_MIN-1 -> overflow */
    psa = sa1(VT_I1, (LONG)0x80000000, 0);
    CHECK(psa != NULL);
    if (psa) {
        LONG ub = 5; EQHR(pGetUBound(psa, 1, &ub), DISP_E_OVERFLOW); EQ(ub, -1);
        EQHR(pDestroy(psa), S_OK);
    }
}

TEST(sa_create_size_overflow_returns_null)
{
    SA_BEGIN();
    /* Size arithmetic is checked before any allocation, so none of these touches the heap. */
    SAFEARRAYBOUND b1 = { 0xffffffff, 0 };
    EQ(pCreate(VT_UI1, 1, &b1), NULL);
    EQ(pCreateEx(VT_UI1, 1, &b1, NULL), NULL);
    SAFEARRAYBOUND b2[2] = { {0x10000, 0}, {0x10000, 0} };
    EQ(pCreate(VT_UI1, 2, b2), NULL);
    SAFEARRAYBOUND b3 = { 0x20000000, 0 };
    EQ(pCreate(VT_R8, 1, &b3), NULL);        /* 8 * 2^29 = 2^32 */
    SAFEARRAYBOUND b4 = { 0x40000000, 0 };
    EQ(pCreate(VT_I4, 1, &b4), NULL);        /* 4 * 2^30 = 2^32 */
    SAFEARRAYBOUND b5[3] = { {0xffff, 0}, {0xffff, 0}, {0xffff, 0} };
    EQ(pCreate(VT_I2, 3, b5), NULL);
    EQ(pCreateEx(VT_VARIANT, 1, &b4, NULL), NULL);
    /* zero elements in one dimension masks nothing: product stays 0 only if overflow is not hit earlier */
    SAFEARRAYBOUND b6[2] = { {0, 0}, {0x10000, 0} };
    SAFEARRAY *psa = pCreate(VT_UI1, 2, b6);
    CHECK(psa != NULL);
    if (psa) pDestroy(psa);
}

TEST(sa_create_vector_size_limits)
{
    SA_BEGIN();
    /* vectors are limited to 0x7fffffe0 bytes of data and the check happens before any allocation */
    EQ(pCreateVector(VT_UI1, 0, 0xffffffff), NULL);
    EQ(pCreateVector(VT_UI1, 0, 0x7fffffe0), NULL);
    EQ(pCreateVector(VT_I4, 0, 0x20000000), NULL);
    EQ(pCreateVector(VT_VARIANT, 0, 0x10000000), NULL);
    EQ(pCreateVectorEx(VT_UI1, 0, 0xffffffff, NULL), NULL);
    EQ(pCreateVectorEx(VT_UI1, 0, 0x7fffffe0, NULL), NULL);
    EQ(pCreateVectorEx(VT_R8, -5, 0x20000000, NULL), NULL);
}

TEST(sa_create_large_array)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 32u << 20, 0 };    /* 32 MB of bytes */
    SAFEARRAY *psa = pCreate(VT_UI1, 1, &b);
    CHECK(psa != NULL);
    if (!psa) return;
    BYTE *p = psa->pvData;
    CHECK(p != NULL);
    if (p) {
        EQ(p[0], 0); EQ(p[(32u << 20) - 1], 0);
        LONG i = 0; BYTE v = 0x5a; EQHR(pPutElement(psa, &i, &v), S_OK);
        i = (32 << 20) - 1; v = 0xa5; EQHR(pPutElement(psa, &i, &v), S_OK);
        EQ(p[0], 0x5a); EQ(p[(32u << 20) - 1], 0xa5);
        LONG ub = 0; EQHR(pGetUBound(psa, 1, &ub), S_OK); EQ(ub, (32 << 20) - 1);
    }
    EQHR(pDestroy(psa), S_OK);
    SAFEARRAY *pv = pCreateVector(VT_UI1, 0, 16u << 20);
    CHECK(pv != NULL);
    if (pv) { BYTE *q = pv->pvData; if (q) { q[0] = 1; q[(16u << 20) - 1] = 2; } EQHR(pDestroy(pv), S_OK); }
    SAFEARRAYBOUND b2[2] = { {1024, 0}, {1024, 0} };
    SAFEARRAY *p2 = pCreate(VT_R8, 2, b2);
    CHECK(p2 != NULL);
    if (p2) { LONG idx[2] = { 1023, 1023 }; double d = 2.5, e = 0; EQHR(pPutElement(p2, idx, &d), S_OK); EQHR(pGetElement(p2, idx, &e), S_OK); CHECK(e == 2.5); pDestroy(p2); }
}

TEST(sa_create_hidden_vt)
{
    SA_BEGIN();
    /* For types without a dedicated feature bit the vt is stored in the DWORD just before the descriptor. */
    SAFEARRAYBOUND b = { 1, 0 };
    for (unsigned i = 0; i < SA_NVALID; i++) {
        VARTYPE vt = SA_VALID_VTS[i];
        if (vt == VT_UNKNOWN || vt == VT_DISPATCH) continue;
        SAFEARRAY *psa = pCreate(vt, 1, &b), *pe = pCreateEx(vt, 1, &b, NULL), *pv = pCreateVector(vt, 0, 1);
        SAFEARRAY *list[3] = { psa, pe, pv };
        for (int k = 0; k < 3; k++) {
            if (!list[k]) { CHECKF(0, "create failed vt=%u kind=%d", vt, k); continue; }
            CHECK(list[k]->fFeatures & FADF_HAVEVARTYPE);
            EQ(SA_HIDDEN_VT(list[k]), vt);
            VARTYPE got = 0xdead;
            EQHR(pGetVartype(list[k], &got), S_OK);
            EQ(got, vt);
            pDestroy(list[k]);
        }
    }
}

TEST(sa_create_default_iid)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 2, 0 };
    GUID g;
    SAFEARRAY *pu = pCreate(VT_UNKNOWN, 1, &b), *pd = pCreate(VT_DISPATCH, 1, &b);
    CHECK(pu && pd);
    if (pu) {
        EQ(pu->fFeatures, FADF_HAVEIID | FADF_UNKNOWN);
        EQHR(pGetIID(pu, &g), S_OK);
        CHECK(IsEqualGUID(&g, &SA_IID_IUnknown));
        VARTYPE vt = 0; EQHR(pGetVartype(pu, &vt), S_OK); EQ(vt, VT_UNKNOWN);
        pDestroy(pu);
    }
    if (pd) {
        EQ(pd->fFeatures, FADF_HAVEIID | FADF_DISPATCH);
        EQHR(pGetIID(pd, &g), S_OK);
        CHECK(IsEqualGUID(&g, &SA_IID_IDispatch));
        VARTYPE vt = 0; EQHR(pGetVartype(pd, &vt), S_OK); EQ(vt, VT_DISPATCH);
        pDestroy(pd);
    }
    /* vector variants */
    pu = pCreateVector(VT_UNKNOWN, 0, 2); pd = pCreateVector(VT_DISPATCH, 0, 2);
    CHECK(pu && pd);
    if (pu) { EQHR(pGetIID(pu, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_IUnknown)); pDestroy(pu); }
    if (pd) { EQHR(pGetIID(pd, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_IDispatch)); pDestroy(pd); }
    /* Ex with NULL pvExtra keeps the default */
    pu = pCreateEx(VT_UNKNOWN, 1, &b, NULL); pd = pCreateVectorEx(VT_DISPATCH, 0, 2, NULL);
    CHECK(pu && pd);
    if (pu) { EQHR(pGetIID(pu, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_IUnknown)); pDestroy(pu); }
    if (pd) { EQHR(pGetIID(pd, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_IDispatch)); pDestroy(pd); }
}

TEST(sa_create_ex_custom_iid)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 2, 0 };
    GUID g;
    SAFEARRAY *pu = pCreateEx(VT_UNKNOWN, 1, &b, (PVOID)&SA_IID_Custom);
    SAFEARRAY *pd = pCreateEx(VT_DISPATCH, 1, &b, (PVOID)&SA_IID_Custom2);
    SAFEARRAY *pv = pCreateVectorEx(VT_UNKNOWN, 0, 3, (PVOID)&SA_IID_Custom);
    CHECK(pu && pd && pv);
    if (pu) { EQHR(pGetIID(pu, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom)); EQ(pu->fFeatures, FADF_HAVEIID | FADF_UNKNOWN); pDestroy(pu); }
    if (pd) { EQHR(pGetIID(pd, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom2)); EQ(pd->fFeatures, FADF_HAVEIID | FADF_DISPATCH); pDestroy(pd); }
    if (pv) { EQHR(pGetIID(pv, &g), S_OK); CHECK(IsEqualGUID(&g, &SA_IID_Custom)); EQ(pv->fFeatures, FADF_HAVEIID | FADF_UNKNOWN | FADF_CREATEVECTOR); pDestroy(pv); }
    /* the IID is stored in the 16 bytes before the descriptor */
    pu = pCreateEx(VT_UNKNOWN, 1, &b, (PVOID)&SA_IID_Custom);
    if (pu) { CHECK(IsEqualGUID(SA_HIDDEN_IID(pu), &SA_IID_Custom)); pDestroy(pu); }
}

TEST(sa_create_ex_extra_ignored_for_plain_types)
{
    SA_BEGIN();
    /* pvExtra is only used for VT_RECORD / VT_UNKNOWN / VT_DISPATCH; a NULL is always fine */
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *psa = pCreateEx(VT_I4, 1, &b, NULL);
    CHECK(psa != NULL);
    if (psa) { EQ(psa->fFeatures, FADF_HAVEVARTYPE); pDestroy(psa); }
    /* a VT_RECORD request without record info fails everywhere */
    EQ(pCreateEx(VT_RECORD, 1, &b, NULL), NULL);
    EQ(pCreateVectorEx(VT_RECORD, 0, 2, NULL), NULL);
    EQ(pCreateVector(VT_RECORD, 0, 2), NULL);
    EQ(pCreate(VT_RECORD, 1, &b), NULL);
}

TEST(sa_create_vector_layout)
{
    SA_BEGIN();
    SAFEARRAY *psa = pCreateVector(VT_I4, -7, 5);
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(psa->cDims, 1);
    EQ(psa->cbElements, 4);
    EQ(psa->rgsabound[0].lLbound, -7);
    EQ(psa->rgsabound[0].cElements, 5);
    EQ(psa->fFeatures, FADF_HAVEVARTYPE | FADF_CREATEVECTOR);
    /* element storage immediately follows the one-dimension descriptor */
    EQ(psa->pvData, (BYTE *)psa + sizeof(SAFEARRAY));
    LONG *d = psa->pvData; int zero = 1;
    for (int i = 0; i < 5; i++) if (d[i]) zero = 0;
    CHECK(zero);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_create_array_not_vector_flag)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 4, 0 };
    SAFEARRAY *psa = pCreate(VT_I4, 1, &b);
    CHECK(psa != NULL);
    if (!psa) return;
    CHECK(!(psa->fFeatures & FADF_CREATEVECTOR));
    CHECK(!(psa->fFeatures & (FADF_AUTO | FADF_STATIC | FADF_EMBEDDED | FADF_FIXEDSIZE)));
    CHECK(psa->pvData != (BYTE *)psa + sizeof(SAFEARRAY));
    pDestroy(psa);
}

TEST(sa_create_vs_vector_equivalence)
{
    SA_BEGIN();
    for (unsigned i = 0; i < SA_NVALID; i++) {
        VARTYPE vt = SA_VALID_VTS[i];
        SAFEARRAYBOUND b = { 6, -3 };
        SAFEARRAY *a = pCreate(vt, 1, &b), *v = pCreateVector(vt, -3, 6);
        CHECKF(a && v, "vt=%u", vt);
        if (a && v) {
            EQ(a->cDims, v->cDims);
            EQ(a->cbElements, v->cbElements);
            EQ(a->rgsabound[0].lLbound, v->rgsabound[0].lLbound);
            EQ(a->rgsabound[0].cElements, v->rgsabound[0].cElements);
            EQ(a->fFeatures | FADF_CREATEVECTOR, v->fFeatures);
            VARTYPE va = 1, vv = 2;
            EQHR(pGetVartype(a, &va), S_OK); EQHR(pGetVartype(v, &vv), S_OK); EQ(va, vv);
            EQ(pGetElemsize(a), pGetElemsize(v));
        }
        if (a) pDestroy(a);
        if (v) pDestroy(v);
    }
}

TEST(sa_create_data_zeroed)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {7, 0}, {9, 0} };
    for (unsigned i = 0; i < SA_NVALID; i++) {
        VARTYPE vt = SA_VALID_VTS[i];
        SAFEARRAY *psa = pCreate(vt, 2, b);
        if (!psa) { CHECKF(0, "create vt=%u", vt); continue; }
        const BYTE *p = psa->pvData; ULONG n = 63 * psa->cbElements; int zero = 1;
        for (ULONG k = 0; k < n; k++) if (p[k]) { zero = 0; break; }
        CHECKF(zero, "vt=%u data not zeroed", vt);
        pDestroy(psa);
    }
}

TEST(sa_create_independent_instances)
{
    SA_BEGIN();
    SAFEARRAY *a = sa1(VT_I4, 0, 3), *b2 = sa1(VT_I4, 0, 3);
    CHECK(a && b2 && a != b2);
    if (a && b2) { CHECK(a->pvData != b2->pvData); }
    if (a) pDestroy(a);
    if (b2) pDestroy(b2);
}
