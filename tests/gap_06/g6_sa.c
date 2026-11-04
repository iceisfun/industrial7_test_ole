/* gap_06: SafeArrayCreateVectorEx, SafeArrayPtrOfIndex, SafeArrayRedim, SafeArrayCopyData.
 * Decomp facts (Win10 oleaut32):
 *  CreateVectorEx: one malloc block = 16-byte header (IID / vartype slot) + SAFEARRAY(1 dim) + data, data directly after the
 *    descriptor; element size/features from SafeArrayElementSizeFeatures (unsupported vt => NULL); size >= 0x7fffffe0 => NULL;
 *    features always carry the internal vector bit 0x2000; VT_UNKNOWN/VT_DISPATCH get FADF_HAVEIID, other non-record types FADF_HAVEVARTYPE.
 *  PtrOfIndex: NULL psa/indices/out or pvData==NULL or cbElements==0 => E_INVALIDARG; out-of-bounds => DISP_E_BADINDEX;
 *    layout is column-major: the LEFT-most index varies fastest.
 *  Redim: only the right-most (rgsabound[0]) dimension changes; locked or FADF_FIXEDSIZE => DISP_E_ARRAYISLOCKED; growth is zero filled.
 *  CopyData: dims count and every cElements must match (lower bounds are ignored); copies per element class (BSTR dup, VARIANT copy,
 *    IUnknown AddRef/Release); both arrays are locked during the copy and unlocked after. */
#include "gap6.h"
#include <oleauto.h>

#define E_BADINDEX  ((HRESULT)0x8002000BL)
#define E_ARRLOCKED ((HRESULT)0x8002000DL)

static SAFEARRAY *mk(VARTYPE vt, UINT dims, const SAFEARRAYBOUND *b) { return U(SafeArrayCreate)(vt, dims, (SAFEARRAYBOUND *)b); }

/* ---- counting IUnknown ---- */
typedef struct { IUnknown iface; LONG ref; } CU;
static HRESULT WINAPI cu_qi(IUnknown *i, REFIID r, void **p) { *p = i; ((CU *)i)->ref++; return S_OK; }
static ULONG WINAPI cu_ar(IUnknown *i) { return ++((CU *)i)->ref; }
static ULONG WINAPI cu_rl(IUnknown *i) { return --((CU *)i)->ref; }
static IUnknownVtbl cu_vtbl = { cu_qi, cu_ar, cu_rl };
static void cu_init(CU *c) { c->iface.lpVtbl = &cu_vtbl; c->ref = 1; }

/* ------------------------------------------------------------ CreateVectorEx */
static const struct { VARTYPE vt; unsigned cb; WORD feat; } VEC_OK[] = {
    { VT_I2, 2, 0x80 }, { VT_I4, 4, 0x80 }, { VT_R4, 4, 0x80 }, { VT_R8, 8, 0x80 }, { VT_CY, 8, 0x80 }, { VT_DATE, 8, 0x80 },
    { VT_BSTR, 8, 0x180 }, { VT_DISPATCH, 8, 0x440 }, { VT_ERROR, 4, 0x80 }, { VT_BOOL, 2, 0x80 }, { VT_VARIANT, sizeof(VARIANT), 0x880 },
    { VT_UNKNOWN, 8, 0x240 }, { VT_DECIMAL, 16, 0x80 }, { VT_I1, 1, 0x80 }, { VT_UI1, 1, 0x80 }, { VT_UI2, 2, 0x80 }, { VT_UI4, 4, 0x80 },
    { VT_I8, 8, 0x80 }, { VT_UI8, 8, 0x80 }, { VT_INT, 4, 0x80 }, { VT_UINT, 4, 0x80 }, { VT_INT_PTR, 8, 0x80 }, { VT_UINT_PTR, 8, 0x80 } };
static const VARTYPE VEC_BAD[] = { VT_EMPTY, VT_NULL, VT_VOID, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_CARRAY, VT_USERDEFINED, VT_LPSTR,
    VT_LPWSTR, VT_FILETIME, VT_BLOB, VT_STREAM, VT_STORAGE, VT_CLSID, VT_BSTR_BLOB, 0x1A, 0x27, 0x100, VT_BYREF | VT_I4, VT_ARRAY | VT_I4, VT_VECTOR | VT_I4, 0x7FFF };

TEST(gap6_vecex_supported_types_table) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayGetVartype)); REQUIRE(U(SafeArrayGetElemsize));
    for (unsigned i = 0; i < sizeof VEC_OK / sizeof *VEC_OK; i++) {
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VEC_OK[i].vt, 0, 5, NULL);
        CHECKF(a != NULL, "vt %u", VEC_OK[i].vt);
        if (!a) continue;
        CHECKF(a->cDims == 1, "vt %u", VEC_OK[i].vt);
        CHECKF(a->cbElements == VEC_OK[i].cb, "vt %u cb %lu", VEC_OK[i].vt, a->cbElements);
        CHECKF(U(SafeArrayGetElemsize)(a) == VEC_OK[i].cb, "vt %u", VEC_OK[i].vt);
        CHECKF((a->fFeatures & ~0xF008) == VEC_OK[i].feat, "vt %u features 0x%x want 0x%x", VEC_OK[i].vt, a->fFeatures, VEC_OK[i].feat);
        CHECKF((a->fFeatures & 0x2000) != 0, "vt %u features 0x%x: vector bit", VEC_OK[i].vt, a->fFeatures);
        VARTYPE vt = 0xFFFF;
        CHECKF(U(SafeArrayGetVartype)(a, &vt) == S_OK && vt == VEC_OK[i].vt, "vt %u got %u", VEC_OK[i].vt, vt);
        CHECKF(a->cLocks == 0, "vt %u", VEC_OK[i].vt);
        CHECKF(a->rgsabound[0].cElements == 5 && a->rgsabound[0].lLbound == 0, "vt %u", VEC_OK[i].vt);
        CHECKF(a->pvData == (void *)&a->rgsabound[1], "vt %u: data follows the descriptor in the same block", VEC_OK[i].vt);
        BYTE *d = a->pvData; int nz = 0;
        for (unsigned k = 0; k < 5 * VEC_OK[i].cb; k++) nz += d[k] != 0;
        CHECKF(nz == 0, "vt %u data not zeroed", VEC_OK[i].vt);
        CHECKF(U(SafeArrayDestroy)(a) == S_OK, "vt %u", VEC_OK[i].vt);
    }
}
TEST(gap6_vecex_unsupported_types_null) {
    REQUIRE(U(SafeArrayCreateVectorEx));
    for (unsigned i = 0; i < sizeof VEC_BAD / sizeof *VEC_BAD; i++) {
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VEC_BAD[i], 0, 3, NULL);
        CHECKF(a == NULL, "vt 0x%x unexpectedly created", VEC_BAD[i]);
        if (a) U(SafeArrayDestroy)(a);
    }
}
TEST(gap6_vecex_record_requires_recordinfo) {
    REQUIRE(U(SafeArrayCreateVectorEx));
    SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_RECORD, 0, 3, NULL);
    CHECK(a == NULL);
    if (a) U(SafeArrayDestroy)(a);
}
TEST(gap6_vecex_bounds_lbound_values) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayGetLBound)); REQUIRE(U(SafeArrayGetUBound)); REQUIRE(U(SafeArrayDestroy));
    static const struct { LONG lb; ULONG n; } t[] = { {0,1}, {1,10}, {-5,11}, {-100,1}, {0x7ffffff0,4}, {(LONG)0x80000000,2}, {-1,0}, {100,0} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_I4, t[i].lb, t[i].n, NULL);
        CHECKF(a != NULL, "lb %ld n %lu", t[i].lb, t[i].n);
        if (!a) continue;
        LONG lb = 12345, ub = 12345;
        CHECK(U(SafeArrayGetLBound)(a, 1, &lb) == S_OK && lb == t[i].lb);
        HRESULT hr = U(SafeArrayGetUBound)(a, 1, &ub);
        CHECKF(hr == S_OK && ub == (LONG)(t[i].lb + t[i].n - 1), "ub %ld", ub);
        CHECK(U(SafeArrayDestroy)(a) == S_OK);
    }
}
TEST(gap6_vecex_zero_elements_is_valid) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_BSTR, 0, 0, NULL);
    CHECK(a != NULL);
    if (a) { CHECK(a->rgsabound[0].cElements == 0); CHECK(U(SafeArrayDestroy)(a) == S_OK); }
}
TEST(gap6_vecex_size_limit_returns_null_without_allocating) {
    REQUIRE(U(SafeArrayCreateVectorEx));
    if (t_is_wine()) { t_skip("wine would really allocate up to 2 GB here"); return; }
    static const struct { VARTYPE vt; ULONG n; } t[] = { {VT_I1, 0x7fffffe0}, {VT_I1, 0xffffffff}, {VT_I4, 0x20000000}, {VT_I4, 0xffffffff},
        {VT_BSTR, 0x10000000}, {VT_R8, 0x10000000}, {VT_VARIANT, 0x08000000}, {VT_I2, 0x40000000}, {VT_DECIMAL, 0x08000000} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(t[i].vt, 0, t[i].n, NULL);
        CHECKF(a == NULL, "vt %u n 0x%lx", t[i].vt, t[i].n);
        if (a) U(SafeArrayDestroy)(a);
    }
}
TEST(gap6_vecex_iid_for_unknown_dispatch) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayGetIID)); REQUIRE(U(SafeArrayGetVartype)); REQUIRE(U(SafeArrayDestroy));
    static const GUID iid = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    VARTYPE vts[2] = { VT_UNKNOWN, VT_DISPATCH };
    for (int i = 0; i < 2; i++) {
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(vts[i], 0, 2, (PVOID)&iid);
        CHECK(a != NULL); if (!a) continue;
        GUID g; memset(&g, 0, sizeof g);
        EQ(U(SafeArrayGetIID)(a, &g), S_OK);
        CHECK(!memcmp(&g, &iid, sizeof g));
        VARTYPE vt = 0; U(SafeArrayGetVartype)(a, &vt); EQ(vt, vts[i]);
        CHECK((a->fFeatures & FADF_HAVEIID) != 0);
        EQ(U(SafeArrayDestroy)(a), S_OK);
    }
}
TEST(gap6_vecex_pvextra_ignored_for_plain_types) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayGetIID)); REQUIRE(U(SafeArrayDestroy));
    static const GUID iid = { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } };
    VARTYPE vts[3] = { VT_I4, VT_BSTR, VT_VARIANT };
    for (int i = 0; i < 3; i++) {
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(vts[i], 0, 1, (PVOID)&iid);
        CHECK(a != NULL); if (!a) continue;
        GUID g;
        EQ(U(SafeArrayGetIID)(a, &g), E_INVALIDARG);
        CHECK((a->fFeatures & FADF_HAVEIID) == 0);
        U(SafeArrayDestroy)(a);
    }
}
TEST(gap6_vecex_unknown_without_iid) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayGetIID)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_UNKNOWN, 0, 1, NULL);
    CHECK(a != NULL); if (!a) return;
    CHECK((a->fFeatures & FADF_HAVEIID) != 0);       /* flag is set even without an IID (decomp) */
    GUID g; memset(&g, 0xAA, sizeof g);
    HRESULT hr = U(SafeArrayGetIID)(a, &g);
    EQ(hr, S_OK);
    static const GUID nil;
    CHECK(!memcmp(&g, &nil, sizeof g));              /* header slot is zeroed by the allocator */
    U(SafeArrayDestroy)(a);
}
TEST(gap6_vecex_matches_createvector) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy));
    for (unsigned i = 0; i < sizeof VEC_OK / sizeof *VEC_OK; i++) {
        if (VEC_OK[i].vt == VT_UNKNOWN || VEC_OK[i].vt == VT_DISPATCH) continue;
        SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VEC_OK[i].vt, 3, 7, NULL), *b = U(SafeArrayCreateVector)(VEC_OK[i].vt, 3, 7);
        CHECKF(a && b, "vt %u", VEC_OK[i].vt);
        if (a && b) {
            CHECKF(a->cDims == b->cDims && a->cbElements == b->cbElements && a->fFeatures == b->fFeatures, "vt %u: 0x%x vs 0x%x", VEC_OK[i].vt, a->fFeatures, b->fFeatures);
        }
        if (a) U(SafeArrayDestroy)(a);
        if (b) U(SafeArrayDestroy)(b);
    }
}
TEST(gap6_vecex_destroy_locked_fails) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayLock)); REQUIRE(U(SafeArrayUnlock)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_I4, 0, 4, NULL);
    CHECK(a != NULL); if (!a) return;
    EQ(U(SafeArrayLock)(a), S_OK);
    EQ(a->cLocks, 1);
    EQ(U(SafeArrayDestroy)(a), E_ARRLOCKED);
    EQ(U(SafeArrayUnlock)(a), S_OK);
    EQ(U(SafeArrayDestroy)(a), S_OK);
}
TEST(gap6_vecex_bstr_elements_freed_by_destroy) {
    REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayGetElement)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SysAllocString));
    SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_BSTR, 0, 3, NULL);
    CHECK(a != NULL); if (!a) return;
    BSTR s = U(SysAllocString)(L"hello");
    LONG i = 1;
    EQ(U(SafeArrayPutElement)(a, &i, s), S_OK);
    BSTR *slots = a->pvData;
    CHECK(slots[1] != s && slots[1] != NULL && !lstrcmpW(slots[1], L"hello"));   /* Put copies */
    BSTR g = NULL; EQ(U(SafeArrayGetElement)(a, &i, &g), S_OK);
    CHECK(g != NULL && g != slots[1] && !lstrcmpW(g, L"hello"));                  /* Get copies */
    U(SysFreeString)(g); U(SysFreeString)(s);
    EQ(U(SafeArrayDestroy)(a), S_OK);
}

/* ------------------------------------------------------------ PtrOfIndex */
TEST(gap6_ptrofindex_null_arguments) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 4, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    LONG i = 1; void *p = (void *)(ULONG_PTR)0x77;
    EQ(U(SafeArrayPtrOfIndex)(NULL, &i, &p), E_INVALIDARG);
    EQ(U(SafeArrayPtrOfIndex)(a, NULL, &p), E_INVALIDARG);
    EQ(U(SafeArrayPtrOfIndex)(a, &i, NULL), E_INVALIDARG);
    EQ(U(SafeArrayPtrOfIndex)(NULL, NULL, NULL), E_INVALIDARG);
    CHECK(p == (void *)(ULONG_PTR)0x77);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_ptrofindex_descriptor_without_data) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayAllocDescriptor)); REQUIRE(U(SafeArrayDestroyDescriptor));
    SAFEARRAY *a = NULL;
    if (U(SafeArrayAllocDescriptor)(1, &a) != S_OK || !a) { CHECK(0); return; }
    a->rgsabound[0].cElements = 4; a->rgsabound[0].lLbound = 0;
    LONG i = 0; void *p = (void *)1;
    EQ(a->pvData, NULL);
    EQ(U(SafeArrayPtrOfIndex)(a, &i, &p), E_INVALIDARG);   /* no data pointer / zero element size */
    U(SafeArrayDestroyDescriptor)(a);
}
TEST(gap6_ptrofindex_1d_bounds_table) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayDestroy));
    static const struct { LONG lb; ULONG n; } t[] = { {0,1}, {0,10}, {1,5}, {-3,7}, {100,2}, {-1000,3}, {0x7ffffff0, 8} };
    for (unsigned k = 0; k < sizeof t / sizeof *t; k++) {
        SAFEARRAYBOUND b = { t[k].n, t[k].lb }; SAFEARRAY *a = mk(VT_R8, 1, &b);
        CHECK(a != NULL); if (!a) continue;
        for (LONG i = t[k].lb; i < (LONG)(t[k].lb + t[k].n); i++) {
            void *p = NULL;
            HRESULT hr = U(SafeArrayPtrOfIndex)(a, &i, &p);
            CHECKF(hr == S_OK && p == (BYTE *)a->pvData + 8 * (i - t[k].lb), "lb %ld i %ld", t[k].lb, i);
        }
        LONG bad[2] = { t[k].lb - 1, (LONG)(t[k].lb + t[k].n) };
        for (int j = 0; j < 2; j++) {
            void *p = (void *)(ULONG_PTR)0x99;
            HRESULT hr = U(SafeArrayPtrOfIndex)(a, &bad[j], &p);
            CHECKF(hr == E_BADINDEX, "lb %ld idx %ld hr 0x%lx", t[k].lb, bad[j], hr);
            CHECK(p == (void *)(ULONG_PTR)0x99);
        }
        U(SafeArrayDestroy)(a);
    }
}
TEST(gap6_ptrofindex_2d_column_major) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b[2] = { { 3, 1 }, { 4, 10 } };   /* left dim: 3 elems from 1, right dim: 4 elems from 10 */
    SAFEARRAY *a = mk(VT_I4, 2, b); CHECK(a != NULL); if (!a) return;
    EQ(a->rgsabound[0].cElements, 4);          /* rgsabound[0] is the RIGHT-most dimension */
    EQ(a->rgsabound[1].cElements, 3);
    for (LONG i = 1; i <= 3; i++) for (LONG j = 10; j <= 13; j++) {
        LONG ix[2] = { i, j }; void *p = NULL;
        HRESULT hr = U(SafeArrayPtrOfIndex)(a, ix, &p);
        CHECKF(hr == S_OK && p == (BYTE *)a->pvData + 4 * ((i - 1) + 3 * (j - 10)), "i %ld j %ld", i, j);
    }
    LONG bad1[2] = { 0, 10 }, bad2[2] = { 4, 10 }, bad3[2] = { 1, 9 }, bad4[2] = { 1, 14 }; void *p;
    EQ(U(SafeArrayPtrOfIndex)(a, bad1, &p), E_BADINDEX);
    EQ(U(SafeArrayPtrOfIndex)(a, bad2, &p), E_BADINDEX);
    EQ(U(SafeArrayPtrOfIndex)(a, bad3, &p), E_BADINDEX);
    EQ(U(SafeArrayPtrOfIndex)(a, bad4, &p), E_BADINDEX);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_ptrofindex_3d_column_major_negative_bounds) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b[3] = { { 2, -1 }, { 3, 0 }, { 4, -2 } };
    SAFEARRAY *a = mk(VT_I2, 3, b); CHECK(a != NULL); if (!a) return;
    int bad = 0;
    for (LONG i = -1; i < 1; i++) for (LONG j = 0; j < 3; j++) for (LONG k = -2; k < 2; k++) {
        LONG ix[3] = { i, j, k }; void *p = NULL;
        LONG lin = (i + 1) + 2 * (j + 3 * (k + 2));
        if (U(SafeArrayPtrOfIndex)(a, ix, &p) != S_OK || p != (BYTE *)a->pvData + 2 * lin) bad++;
    }
    EQ(bad, 0);
    for (int d = 0; d < 3; d++) {
        LONG lo[3] = { -1, 0, -2 }, hi[3] = { 0, 2, 1 }; void *p;
        LONG ix[3]; memcpy(ix, lo, sizeof ix); ix[d] = lo[d] - 1;
        EQ(U(SafeArrayPtrOfIndex)(a, ix, &p), E_BADINDEX);
        memcpy(ix, lo, sizeof ix); ix[d] = hi[d] + 1;
        EQ(U(SafeArrayPtrOfIndex)(a, ix, &p), E_BADINDEX);
    }
    U(SafeArrayDestroy)(a);
}
TEST(gap6_ptrofindex_agrees_with_put_get_element) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b[2] = { { 5, 0 }, { 6, 3 } };
    SAFEARRAY *a = mk(VT_I4, 2, b); CHECK(a != NULL); if (!a) return;
    int bad = 0;
    for (LONG i = 0; i < 5; i++) for (LONG j = 3; j < 9; j++) {
        LONG ix[2] = { i, j }; int v = (int)(i * 100 + j);
        if (U(SafeArrayPutElement)(a, ix, &v) != S_OK) bad++;
    }
    for (LONG i = 0; i < 5; i++) for (LONG j = 3; j < 9; j++) {
        LONG ix[2] = { i, j }; int *p = NULL;
        if (U(SafeArrayPtrOfIndex)(a, ix, (void **)&p) != S_OK || !p || *p != (int)(i * 100 + j)) bad++;
    }
    EQ(bad, 0);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_ptrofindex_works_on_locked_array_and_does_not_lock) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayLock)); REQUIRE(U(SafeArrayUnlock)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 3, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    LONG i = 2; void *p = NULL;
    EQ(U(SafeArrayPtrOfIndex)(a, &i, &p), S_OK);
    EQ(a->cLocks, 0);
    U(SafeArrayLock)(a);
    p = NULL; EQ(U(SafeArrayPtrOfIndex)(a, &i, &p), S_OK); CHECK(p != NULL);
    EQ(a->cLocks, 1);
    U(SafeArrayUnlock)(a);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_ptrofindex_vector_and_element_sizes) {
    REQUIRE(U(SafeArrayPtrOfIndex)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy));
    for (unsigned k = 0; k < sizeof VEC_OK / sizeof *VEC_OK; k++) {
        if (VEC_OK[k].vt == VT_UNKNOWN || VEC_OK[k].vt == VT_DISPATCH) continue;
        SAFEARRAY *a = U(SafeArrayCreateVector)(VEC_OK[k].vt, 5, 4); CHECK(a != NULL); if (!a) continue;
        for (LONG i = 5; i < 9; i++) { void *p = NULL; CHECKF(U(SafeArrayPtrOfIndex)(a, &i, &p) == S_OK && p == (BYTE *)a->pvData + VEC_OK[k].cb * (i - 5), "vt %u i %ld", VEC_OK[k].vt, i); }
        U(SafeArrayDestroy)(a);
    }
}

/* ------------------------------------------------------------ Redim */
TEST(gap6_redim_null_arguments) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 4, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    EQ(U(SafeArrayRedim)(NULL, &b), E_INVALIDARG);
    EQ(U(SafeArrayRedim)(a, NULL), E_INVALIDARG);
    EQ(U(SafeArrayRedim)(NULL, NULL), E_INVALIDARG);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_redim_grow_preserves_and_zero_fills) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 4, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    int *d = a->pvData; for (int i = 0; i < 4; i++) d[i] = 100 + i;
    SAFEARRAYBOUND nb = { 10, 0 };
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    EQ(a->rgsabound[0].cElements, 10);
    d = a->pvData;
    for (int i = 0; i < 4; i++) EQ(d[i], 100 + i);
    for (int i = 4; i < 10; i++) EQ(d[i], 0);
    EQ(U(SafeArrayDestroy)(a), S_OK);
}
TEST(gap6_redim_shrink_keeps_prefix) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 8, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    int *d = a->pvData; for (int i = 0; i < 8; i++) d[i] = 7 * i + 1;
    SAFEARRAYBOUND nb = { 3, 0 };
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    EQ(a->rgsabound[0].cElements, 3);
    d = a->pvData; for (int i = 0; i < 3; i++) EQ(d[i], 7 * i + 1);
    SAFEARRAYBOUND nb2 = { 6, 0 };                      /* regrow: the dropped tail must not reappear */
    EQ(U(SafeArrayRedim)(a, &nb2), S_OK);
    d = a->pvData; for (int i = 3; i < 6; i++) EQ(d[i], 0);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_redim_changes_lbound) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayGetLBound)); REQUIRE(U(SafeArrayGetUBound)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 4, 0 }; SAFEARRAY *a = mk(VT_I2, 1, &b); CHECK(a != NULL); if (!a) return;
    short *d = a->pvData; for (int i = 0; i < 4; i++) d[i] = (short)(i + 1);
    SAFEARRAYBOUND nb = { 4, -7 };                      /* same size: only the bound changes, data untouched */
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    LONG lb = 0, ub = 0;
    U(SafeArrayGetLBound)(a, 1, &lb); U(SafeArrayGetUBound)(a, 1, &ub);
    EQ(lb, -7); EQ(ub, -4);
    d = a->pvData; for (int i = 0; i < 4; i++) EQ(d[i], i + 1);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_redim_to_zero_and_back) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 5, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    SAFEARRAYBOUND z = { 0, 0 }, n = { 3, 0 };
    EQ(U(SafeArrayRedim)(a, &z), S_OK);
    EQ(a->rgsabound[0].cElements, 0);
    EQ(U(SafeArrayRedim)(a, &n), S_OK);
    int *d = a->pvData; CHECK(d != NULL);
    if (d) for (int i = 0; i < 3; i++) EQ(d[i], 0);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_redim_locked_array_fails_unchanged) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayLock)); REQUIRE(U(SafeArrayUnlock)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 4, 2 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    void *pv = a->pvData;
    U(SafeArrayLock)(a);
    SAFEARRAYBOUND nb = { 9, 0 };
    EQ(U(SafeArrayRedim)(a, &nb), E_ARRLOCKED);
    EQ(a->rgsabound[0].cElements, 4); EQ(a->rgsabound[0].lLbound, 2); CHECK(a->pvData == pv);
    U(SafeArrayUnlock)(a);
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_redim_fixedsize_static_array_fails) {
    REQUIRE(U(SafeArrayRedim));
    int buf[4] = { 1, 2, 3, 4 };
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cDims = 1; sa.fFeatures = FADF_STATIC | FADF_FIXEDSIZE; sa.cbElements = 4; sa.pvData = buf;
    sa.rgsabound[0].cElements = 4; sa.rgsabound[0].lLbound = 0;
    SAFEARRAYBOUND nb = { 8, 0 };
    EQ(U(SafeArrayRedim)(&sa, &nb), E_ARRLOCKED);
    EQ(sa.rgsabound[0].cElements, 4);
}
TEST(gap6_redim_zero_dims_invalid) {
    REQUIRE(U(SafeArrayRedim));
    if (t_is_wine()) { t_skip("wine faults on a zero-dimension descriptor"); return; }
    SAFEARRAY sa; memset(&sa, 0, sizeof sa);
    sa.cbElements = 4;
    SAFEARRAYBOUND nb = { 8, 0 };
    EQ(U(SafeArrayRedim)(&sa, &nb), E_INVALIDARG);
}
TEST(gap6_redim_only_rightmost_dimension) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayGetLBound)); REQUIRE(U(SafeArrayGetUBound)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b[2] = { { 2, 1 }, { 3, 5 } };
    SAFEARRAY *a = mk(VT_I4, 2, b); CHECK(a != NULL); if (!a) return;
    int *d = a->pvData; for (int i = 0; i < 6; i++) d[i] = i + 1;
    SAFEARRAYBOUND nb = { 5, 0 };                        /* applies to dimension 2 (the right-most, rgsabound[0]) */
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    LONG lb1, ub1, lb2, ub2;
    U(SafeArrayGetLBound)(a, 1, &lb1); U(SafeArrayGetUBound)(a, 1, &ub1);
    U(SafeArrayGetLBound)(a, 2, &lb2); U(SafeArrayGetUBound)(a, 2, &ub2);
    EQ(lb1, 1); EQ(ub1, 2); EQ(lb2, 0); EQ(ub2, 4);
    d = a->pvData;
    for (int i = 0; i < 6; i++) EQ(d[i], i + 1);
    for (int i = 6; i < 10; i++) EQ(d[i], 0);
    U(SafeArrayDestroy)(a);
}
TEST(gap6_redim_bstr_array_shrink_and_grow) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SysAllocString));
    SAFEARRAYBOUND b = { 4, 0 }; SAFEARRAY *a = mk(VT_BSTR, 1, &b); CHECK(a != NULL); if (!a) return;
    BSTR s = U(SysAllocString)(L"keep");
    for (LONG i = 0; i < 4; i++) U(SafeArrayPutElement)(a, &i, s);
    SAFEARRAYBOUND nb = { 2, 0 };
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    BSTR *p = a->pvData; CHECK(p[0] && !lstrcmpW(p[0], L"keep") && p[1] && !lstrcmpW(p[1], L"keep"));
    SAFEARRAYBOUND nb2 = { 6, 0 };
    EQ(U(SafeArrayRedim)(a, &nb2), S_OK);
    p = a->pvData; CHECK(p[0] && !lstrcmpW(p[0], L"keep")); for (int i = 2; i < 6; i++) CHECK(p[i] == NULL);
    U(SysFreeString)(s);
    EQ(U(SafeArrayDestroy)(a), S_OK);
}
TEST(gap6_redim_unknown_array_releases_dropped) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 3, 0 }; SAFEARRAY *a = mk(VT_UNKNOWN, 1, &b); CHECK(a != NULL); if (!a) return;
    CU o; cu_init(&o);
    for (LONG i = 0; i < 3; i++) EQ(U(SafeArrayPutElement)(a, &i, &o.iface), S_OK);
    EQ(o.ref, 4);                      /* Put AddRefs */
    SAFEARRAYBOUND nb = { 1, 0 };
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    EQ(o.ref, 2);                      /* the two dropped elements were released */
    EQ(U(SafeArrayDestroy)(a), S_OK);
    EQ(o.ref, 1);
}
TEST(gap6_redim_vector_leaves_block_layout) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAY *a = U(SafeArrayCreateVectorEx)(VT_I4, 0, 3, NULL); CHECK(a != NULL); if (!a) return;
    int *d = a->pvData; d[0] = 11; d[1] = 22; d[2] = 33;
    SAFEARRAYBOUND nb = { 40, 0 };
    EQ(U(SafeArrayRedim)(a, &nb), S_OK);
    CHECK((a->fFeatures & 0x2000) == 0);       /* data moved out of the descriptor block */
    CHECK(a->pvData != (void *)&a->rgsabound[1]);
    d = a->pvData; EQ(d[0], 11); EQ(d[2], 33); EQ(d[39], 0);
    EQ(U(SafeArrayDestroy)(a), S_OK);
}
TEST(gap6_redim_many_cycles) {
    REQUIRE(U(SafeArrayRedim)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 1, 0 }; SAFEARRAY *a = mk(VT_I4, 1, &b); CHECK(a != NULL); if (!a) return;
    int bad = 0; *(int *)a->pvData = 42;
    for (int i = 1; i <= 300; i++) {
        SAFEARRAYBOUND nb = { (ULONG)(1 + (i * 37) % 500), 0 };
        if (U(SafeArrayRedim)(a, &nb) != S_OK) bad++;
        else if (*(int *)a->pvData != 42) bad++;
    }
    EQ(bad, 0);
    U(SafeArrayDestroy)(a);
}

/* ------------------------------------------------------------ CopyData */
TEST(gap6_copydata_null_and_mismatch) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b4 = { 4, 0 }, b5 = { 5, 0 }, b2d[2] = { { 4, 0 }, { 1, 0 } };
    SAFEARRAY *a = mk(VT_I4, 1, &b4), *c = mk(VT_I4, 1, &b5), *d2 = mk(VT_I4, 2, b2d);
    CHECK(a && c && d2); if (!(a && c && d2)) return;
    EQ(U(SafeArrayCopyData)(NULL, a), E_INVALIDARG);
    EQ(U(SafeArrayCopyData)(a, NULL), E_INVALIDARG);
    EQ(U(SafeArrayCopyData)(NULL, NULL), E_INVALIDARG);
    EQ(U(SafeArrayCopyData)(a, c), E_INVALIDARG);      /* element counts differ */
    EQ(U(SafeArrayCopyData)(c, a), E_INVALIDARG);
    EQ(U(SafeArrayCopyData)(a, d2), E_INVALIDARG);     /* dimension count differs */
    EQ(U(SafeArrayCopyData)(d2, a), E_INVALIDARG);
    EQ(a->cLocks, 0); EQ(c->cLocks, 0);
    U(SafeArrayDestroy)(a); U(SafeArrayDestroy)(c); U(SafeArrayDestroy)(d2);
}
TEST(gap6_copydata_basic_values_and_lbound_ignored) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND bs = { 6, 3 }, bt = { 6, -20 };
    SAFEARRAY *s = mk(VT_I4, 1, &bs), *t = mk(VT_I4, 1, &bt); CHECK(s && t); if (!(s && t)) return;
    int *ps = s->pvData, *pt = t->pvData;
    for (int i = 0; i < 6; i++) { ps[i] = 1000 * i + 7; pt[i] = -1; }
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    for (int i = 0; i < 6; i++) EQ(pt[i], 1000 * i + 7);
    EQ(t->rgsabound[0].lLbound, -20);                  /* target bounds untouched */
    EQ(s->rgsabound[0].lLbound, 3);
    EQ(s->cLocks, 0); EQ(t->cLocks, 0);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_self_copy_ok) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 3, 0 }; SAFEARRAY *s = mk(VT_I4, 1, &b); CHECK(s != NULL); if (!s) return;
    int *p = s->pvData; p[0] = 5; p[1] = 6; p[2] = 7;
    EQ(U(SafeArrayCopyData)(s, s), S_OK);
    EQ(p[0], 5); EQ(p[2], 7);
    EQ(s->cLocks, 0);
    U(SafeArrayDestroy)(s);
}
TEST(gap6_copydata_vartype_not_compared) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *s = mk(VT_I4, 1, &b), *t = mk(VT_R4, 1, &b); CHECK(s && t); if (!(s && t)) return;
    int *p = s->pvData; p[0] = 0x3f800000; p[1] = 2; p[2] = 3;
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    float *f = t->pvData; CHECK(f[0] == 1.0f);                 /* raw bytes copied, element types are not compared */
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_multi_dim_flat) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND bs[2] = { { 3, 0 }, { 4, 0 } }, bt[2] = { { 3, 7 }, { 4, 9 } };
    SAFEARRAY *s = mk(VT_I2, 2, bs), *t = mk(VT_I2, 2, bt); CHECK(s && t); if (!(s && t)) return;
    short *ps = s->pvData; for (int i = 0; i < 12; i++) ps[i] = (short)(i * 3);
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    short *pt = t->pvData; int bad = 0; for (int i = 0; i < 12; i++) bad += pt[i] != (short)(i * 3);
    EQ(bad, 0);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_dim_swap_requires_same_order) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND bs[2] = { { 3, 0 }, { 4, 0 } }, bt[2] = { { 4, 0 }, { 3, 0 } };
    SAFEARRAY *s = mk(VT_I4, 2, bs), *t = mk(VT_I4, 2, bt); CHECK(s && t); if (!(s && t)) return;
    EQ(U(SafeArrayCopyData)(s, t), E_INVALIDARG);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_bstr_duplicates_and_frees_old) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SysAllocString)); REQUIRE(U(SafeArrayPutElement));
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *s = mk(VT_BSTR, 1, &b), *t = mk(VT_BSTR, 1, &b); CHECK(s && t); if (!(s && t)) return;
    BSTR a = U(SysAllocString)(L"alpha"), c = U(SysAllocString)(L"old-target");
    LONG i0 = 0, i1 = 1, i2 = 2;
    U(SafeArrayPutElement)(s, &i0, a); U(SafeArrayPutElement)(t, &i0, c); U(SafeArrayPutElement)(t, &i2, c);
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    BSTR *ps = s->pvData, *pt = t->pvData;
    CHECK(pt[0] && pt[0] != ps[0] && !lstrcmpW(pt[0], L"alpha"));         /* deep copy */
    CHECK(pt[1] == NULL);                                                 /* NULL source element => NULL target */
    CHECK(pt[2] == NULL);
    U(SysFreeString)(a); U(SysFreeString)(c);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_variant_elements) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SysAllocString));
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *s = mk(VT_VARIANT, 1, &b), *t = mk(VT_VARIANT, 1, &b); CHECK(s && t); if (!(s && t)) return;
    VARIANT *vs = s->pvData, *vt = t->pvData;
    V_VT(&vs[0]) = VT_I4; V_I4(&vs[0]) = 99;
    V_VT(&vs[1]) = VT_BSTR; V_BSTR(&vs[1]) = U(SysAllocString)(L"in-variant");
    V_VT(&vs[2]) = VT_R8; V_R8(&vs[2]) = 2.5;
    V_VT(&vt[0]) = VT_BSTR; V_BSTR(&vt[0]) = U(SysAllocString)(L"will be cleared");
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    CHECK(V_VT(&vt[0]) == VT_I4 && V_I4(&vt[0]) == 99);
    CHECK(V_VT(&vt[1]) == VT_BSTR && V_BSTR(&vt[1]) != V_BSTR(&vs[1]) && !lstrcmpW(V_BSTR(&vt[1]), L"in-variant"));
    CHECK(V_VT(&vt[2]) == VT_R8 && V_R8(&vt[2]) == 2.5);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_unknown_addref_release) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayPutElement));
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *s = mk(VT_UNKNOWN, 1, &b), *t = mk(VT_UNKNOWN, 1, &b); CHECK(s && t); if (!(s && t)) return;
    CU a, c; cu_init(&a); cu_init(&c);
    LONG i0 = 0, i1 = 1;
    U(SafeArrayPutElement)(s, &i0, &a.iface);
    U(SafeArrayPutElement)(t, &i0, &c.iface); U(SafeArrayPutElement)(t, &i1, &c.iface);
    EQ(a.ref, 2); EQ(c.ref, 3);
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    EQ(a.ref, 3);                  /* target slot 0 now holds a */
    EQ(c.ref, 1);                  /* both target slots released (slot 1 even though the source slot is NULL) */
    IUnknown **pt = t->pvData;
    CHECK(pt[0] == &a.iface); CHECK(pt[1] == NULL);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
    EQ(a.ref, 1); EQ(c.ref, 1);
}
TEST(gap6_copydata_locked_source_ok) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayLock)); REQUIRE(U(SafeArrayUnlock)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *s = mk(VT_I4, 1, &b), *t = mk(VT_I4, 1, &b); CHECK(s && t); if (!(s && t)) return;
    ((int *)s->pvData)[1] = 77;
    U(SafeArrayLock)(s); U(SafeArrayLock)(t);
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    EQ(s->cLocks, 1); EQ(t->cLocks, 1);       /* copy restores the caller's lock counts */
    EQ(((int *)t->pvData)[1], 77);
    U(SafeArrayUnlock)(s); U(SafeArrayUnlock)(t);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_lock_count_overflow) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *s = mk(VT_I4, 1, &b), *t = mk(VT_I4, 1, &b); CHECK(s && t); if (!(s && t)) return;
    s->cLocks = 0xffff;
    HRESULT hr = U(SafeArrayCopyData)(s, t);
    EQ(hr, (HRESULT)0x8000FFFFL);        /* E_UNEXPECTED: source lock count would overflow */
    s->cLocks = 0;
    t->cLocks = 0xffff;
    hr = U(SafeArrayCopyData)(s, t);
    EQ(hr, (HRESULT)0x8000FFFFL);        /* target lock count would overflow; the source lock taken first is dropped again */
    EQ(s->cLocks, 0);
    t->cLocks = 0; s->cLocks = 0;
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
}
TEST(gap6_copydata_vectors_and_zero_length) {
    REQUIRE(U(SafeArrayCopyData)); REQUIRE(U(SafeArrayCreateVectorEx)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAY *s = U(SafeArrayCreateVectorEx)(VT_I8, 0, 4, NULL), *t = U(SafeArrayCreateVectorEx)(VT_I8, 9, 4, NULL);
    CHECK(s && t); if (!(s && t)) return;
    LONGLONG *p = s->pvData; for (int i = 0; i < 4; i++) p[i] = 0x1111111111111111LL * (i + 1);
    EQ(U(SafeArrayCopyData)(s, t), S_OK);
    p = t->pvData; for (int i = 0; i < 4; i++) EQ(p[i] == 0x1111111111111111LL * (i + 1), 1);
    SAFEARRAY *z1 = U(SafeArrayCreateVectorEx)(VT_I4, 0, 0, NULL), *z2 = U(SafeArrayCreateVectorEx)(VT_I4, 5, 0, NULL);
    if (z1 && z2) EQ(U(SafeArrayCopyData)(z1, z2), S_OK);
    U(SafeArrayDestroy)(s); U(SafeArrayDestroy)(t);
    if (z1) U(SafeArrayDestroy)(z1); if (z2) U(SafeArrayDestroy)(z2);
}
