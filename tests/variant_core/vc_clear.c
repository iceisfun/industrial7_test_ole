/* variant_core: VariantInit / VariantClear */
#include "vc.h"

/* ---------------------------------------------------------------- VariantInit */
/* Decomp (oleaut32 VariantInit): three 8-byte stores of zero; the whole 24 byte VARIANT is zeroed,
 * including the reserved fields (older Windows only set vt). */
TEST(vc_init_zeroes_all_24_bytes) {
    VC_REQ();
    VARIANT v;
    memset(&v, 0xff, sizeof v);
    w_VariantInit(&v);
    CHECK(vc_is_zero24(&v));
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(sizeof(VARIANT), 24);
}
TEST(vc_init_overwrites_without_releasing) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT v; V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)&u;
    w_VariantInit(&v);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(u.nrel, 0);
    EXPECT_EQ(u.nadd, 0);
    CHECK(V_UNKNOWN(&v) == NULL);
}
TEST(vc_init_array_of_variants) {
    VC_REQ();
    VARIANT a[17];
    memset(a, 0xa5, sizeof a);
    for (int i = 0; i < 17; i++) w_VariantInit(&a[i]);
    for (int i = 0; i < 17; i++) CHECK(vc_is_zero24(&a[i]));
}
TEST(vc_init_after_populated_scalar) {
    VC_REQ();
    VARIANT v; V_VT(&v) = VT_R8; V_R8(&v) = 3.5; v.wReserved1 = 7; v.wReserved3 = 9;
    w_VariantInit(&v);
    CHECK(vc_is_zero24(&v));
}

/* ---------------------------------------------------------------- VariantClear: basics */
TEST(vc_clear_empty_is_noop) {
    VC_REQ();
    VARIANT v; w_VariantInit(&v);
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    CHECK(vc_is_zero24(&v));
}
/* VT_EMPTY..VT_CY: only vt is touched (decomp: vt < 8 -> vt = 0), payload remains. */
TEST(vc_clear_null_and_small_vts_keep_payload) {
    VC_REQ();
    static const VARTYPE vts[] = { VT_NULL, VT_I2, VT_I4, VT_R4, VT_R8, VT_CY };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT v; memset(&v, 0x5a, sizeof v);
        V_VT(&v) = vts[i];
        HR_EQ(w_VariantClear(&v), S_OK);
        EXPECT_EQ(V_VT(&v), VT_EMPTY);
        CHECKF(V_UI8(&v) == 0x5a5a5a5a5a5a5a5aULL, "vt %u payload changed", vts[i]);
    }
}
TEST(vc_clear_scalar_vts) {
    VC_REQ();
    static const VARTYPE vts[] = { VT_DATE, VT_ERROR, VT_BOOL, VT_DECIMAL, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT v; memset(&v, 0x11, sizeof v);
        V_VT(&v) = vts[i];
        HR_EQ(w_VariantClear(&v), S_OK);
        EXPECT_EQ(V_VT(&v), VT_EMPTY);
    }
}
TEST(vc_clear_twice) {
    VC_REQ();
    VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = 5;
    HR_EQ(w_VariantClear(&v), S_OK);
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_reserved_fields_untouched) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_I4; V_I4(&v) = 77; v.wReserved1 = 0x1111; v.wReserved2 = 0x2222; v.wReserved3 = 0x3333;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(v.wReserved1, 0x1111);
    EXPECT_EQ(v.wReserved2, 0x2222);
    EXPECT_EQ(v.wReserved3, 0x3333);
    EXPECT_EQ(V_I4(&v), 77);
}

/* ---------------------------------------------------------------- validity matrix */
/* vt (0..80) x every combination of the four high flag bits, payload all zero.
 * Expected result follows the rule validated on Windows by Wine's vartest and visible in the decomp:
 * mask 0x1000ff7ffc over (vt & ~flags) for vt > 11, VT_CLSID (72) accepted by VariantClear. */
TEST(vc_clear_vt_validity_matrix) {
    VC_REQ();
    int bad = 0;
    for (unsigned extra = 0; extra < 16; extra++) {
        for (unsigned base = 0; base <= 80; base++) {
            unsigned vt = base | (extra << 12);
            VARIANT v; memset(&v, 0, sizeof v);
            V_VT(&v) = (VARTYPE)vt;
            HRESULT hr = w_VariantClear(&v);
            HRESULT want = vc_valid_vt(vt) ? S_OK : VC_BADVT;
            if (hr != want) { bad++; CHECKF(0, "VariantClear vt=0x%04x got 0x%08lx want 0x%08lx", vt, (unsigned long)hr, (unsigned long)want); }
            else if (hr == S_OK) { if (V_VT(&v) != VT_EMPTY) CHECKF(0, "vt=0x%04x not cleared: 0x%x", vt, V_VT(&v)); }
            else { if (V_VT(&v) != vt) CHECKF(0, "vt=0x%04x changed on failure: 0x%x", vt, V_VT(&v)); }
        }
    }
    CHECK(bad == 0);
}
TEST(vc_clear_vt_bstr_blob_and_high_values_invalid) {
    VC_REQ();
    static const unsigned vts[] = { 0x0fff, 0x1000, 0x1001, 0x8000, 0x8001, 0x9000, 0xffff, 0x3fff, 0x7fff, 0xbfff };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = (VARTYPE)vts[i];
        HRESULT hr = w_VariantClear(&v);
        CHECKF(hr == VC_BADVT || vc_valid_vt(vts[i]), "vt 0x%04x got 0x%08lx", vts[i], (unsigned long)hr);
        if (!vc_valid_vt(vts[i])) { HR_EQ(hr, VC_BADVT); EXPECT_EQ(V_VT(&v), vts[i]); }
    }
}
TEST(vc_clear_illegal_vts_keep_vt_and_payload) {
    VC_REQ();
    static const unsigned vts[] = { 15, VT_VOID, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_CARRAY, VT_USERDEFINED, VT_LPSTR, VT_LPWSTR, 37, 64, 65, 0x8000 | VT_I4, 0x1000 | VT_I4 };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT v; memset(&v, 0x33, sizeof v); V_VT(&v) = (VARTYPE)vts[i];
        HRESULT hr = w_VariantClear(&v);
        if (vts[i] >= 24 && vts[i] <= 31) { HR_EQ(hr, VC_BADVT); }
        else if (!vc_valid_vt(vts[i])) { HR_EQ(hr, VC_BADVT); }
        if (hr == VC_BADVT) { EXPECT_EQ(V_VT(&v), vts[i]); CHECK(V_UI4(&v) == 0x33333333); }
    }
    /* the pointer payload of an illegal vt must never be dereferenced (these would crash) */
    VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = VT_LPWSTR; V_BYREF(&v) = (void *)(ULONG_PTR)0x10;
    HR_EQ(w_VariantClear(&v), VC_BADVT);
}
TEST(vc_clear_byref_empty_null_invalid) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_BYREF; HR_EQ(w_VariantClear(&v), VC_BADVT); EXPECT_EQ(V_VT(&v), VT_BYREF);
    V_VT(&v) = VT_BYREF | VT_NULL; HR_EQ(w_VariantClear(&v), VC_BADVT);
    V_VT(&v) = VT_ARRAY; HR_EQ(w_VariantClear(&v), VC_BADVT);
    V_VT(&v) = VT_ARRAY | VT_NULL; HR_EQ(w_VariantClear(&v), VC_BADVT);
    V_VT(&v) = VT_ARRAY | VT_BYREF; HR_EQ(w_VariantClear(&v), VC_BADVT);
    V_VT(&v) = VT_ARRAY | VT_BYREF | VT_I4; HR_EQ(w_VariantClear(&v), S_OK);
    V_VT(&v) = VT_ARRAY | VT_I4; HR_EQ(w_VariantClear(&v), S_OK);
    V_VT(&v) = VT_BYREF | VT_I4; HR_EQ(w_VariantClear(&v), S_OK);
}
TEST(vc_clear_vt_variant_nonbyref_and_clsid_accepted) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_VARIANT; HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
    V_VT(&v) = VT_CLSID; HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
    V_VT(&v) = VT_CLSID | VT_BYREF; HR_EQ(w_VariantClear(&v), S_OK);
    V_VT(&v) = VT_RECORD; HR_EQ(w_VariantClear(&v), S_OK); /* NULL record info: nothing to release */
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
}

/* ---------------------------------------------------------------- BYREF: nothing released */
TEST(vc_clear_byref_scalar_not_modified) {
    VC_REQ();
    LONG i4 = 0x4321; SHORT i2 = -3; double r8 = 2.25; VARIANT inner; V_VT(&inner) = VT_I4; V_I4(&inner) = 0x1234;
    VARIANT v;
    V_VT(&v) = VT_I4 | VT_BYREF; V_I4REF(&v) = &i4;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY); CHECK(V_I4REF(&v) == &i4); EXPECT_EQ(i4, 0x4321);
    V_VT(&v) = VT_I2 | VT_BYREF; V_I2REF(&v) = &i2;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(i2, -3);
    V_VT(&v) = VT_R8 | VT_BYREF; V_R8REF(&v) = &r8;
    HR_EQ(w_VariantClear(&v), S_OK); CHECK(r8 == 2.25);
    V_VT(&v) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&v) = &inner;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY); CHECK(V_VARIANTREF(&v) == &inner);
    EXPECT_EQ(V_VT(&inner), VT_I4); EXPECT_EQ(V_I4(&inner), 0x1234);
}
TEST(vc_clear_byref_variant_does_not_clear_target) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT inner; V_VT(&inner) = VT_UNKNOWN; V_UNKNOWN(&inner) = (IUnknown *)&u;
    VARIANT v; V_VT(&v) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&v) = &inner;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(u.nrel, 0);
    EXPECT_EQ(V_VT(&inner), VT_UNKNOWN);
    HR_EQ(w_VariantClear(&inner), S_OK);
    EXPECT_EQ(u.nrel, 1);
}
TEST(vc_clear_byref_bstr_not_freed) {
    VC_REQ();
    BSTR b = w_SysAllocString(L"keep");
    VARIANT v; V_VT(&v) = VT_BSTR | VT_BYREF; V_BSTRREF(&v) = &b;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(w_SysStringLen(b), 4);
    EXPECT_WSTR(b, L"keep");
    w_SysFreeString(b);
}

/* ---------------------------------------------------------------- IUnknown / IDispatch */
TEST(vc_clear_unknown_released_once) {
    VC_REQ();
    MU u; mu_init(&u); u.ref = 2;
    VARIANT v; V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)&u;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(u.nrel, 1); EXPECT_EQ(u.nadd, 0); EXPECT_EQ(u.nqi, 0);
    EXPECT_EQ(u.ref, 1);
    CHECK(V_UNKNOWN(&v) == (IUnknown *)&u); /* pointer is left in place */
}
TEST(vc_clear_dispatch_released_once) {
    VC_REQ();
    MD d; md_init(&d); d.ref = 2;
    VARIANT v; V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = (IDispatch *)&d;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(d.nrel, 1); EXPECT_EQ(d.nadd, 0); EXPECT_EQ(d.nqi, 0); EXPECT_EQ(d.ninvoke, 0);
}
TEST(vc_clear_null_unknown_and_dispatch) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_UNKNOWN; HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
    V_VT(&v) = VT_DISPATCH; HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_byref_unknown_dispatch_not_released) {
    VC_REQ();
    MU u; mu_init(&u); MD d; md_init(&d);
    IUnknown *pu = (IUnknown *)&u; IDispatch *pd = (IDispatch *)&d;
    VARIANT v;
    V_VT(&v) = VT_UNKNOWN | VT_BYREF; V_UNKNOWNREF(&v) = &pu;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY); CHECK(V_UNKNOWNREF(&v) == &pu);
    V_VT(&v) = VT_DISPATCH | VT_BYREF; V_DISPATCHREF(&v) = &pd;
    HR_EQ(w_VariantClear(&v), S_OK); CHECK(V_DISPATCHREF(&v) == &pd);
    EXPECT_EQ(u.nrel, 0); EXPECT_EQ(d.nrel, 0);
    EXPECT_EQ(u.nadd + d.nadd + u.nqi + d.nqi, 0);
}
TEST(vc_clear_unknown_release_reentrancy_sees_empty) {
    VC_REQ();
    /* vt is set to VT_EMPTY before Release runs (decomp), so Release may not rely on the variant content */
    MU u; mu_init(&u);
    VARIANT v; V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)&u;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
}

/* ---------------------------------------------------------------- BSTR */
TEST(vc_clear_bstr) {
    VC_REQ();
    VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = w_SysAllocString(L"hello world");
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_null_bstr) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = VT_BSTR;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_empty_and_long_bstr) {
    VC_REQ();
    VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = w_SysAllocString(L"");
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = w_SysAllocStringLen(NULL, 100000);
    CHECK(V_BSTR(&v) != NULL);
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_many_bstrs_loop) {
    VC_REQ();
    for (int i = 0; i < 2000; i++) {
        VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = w_SysAllocStringLen(L"abcdefghijklmnop", 1 + (i % 16));
        if (!V_BSTR(&v)) { CHECK(0); return; }
        if (w_VariantClear(&v) != S_OK) { CHECK(0); return; }
    }
    CHECK(1);
}

/* ---------------------------------------------------------------- SAFEARRAY */
TEST(vc_clear_array_destroys_and_empties) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate));
    SAFEARRAYBOUND b = { 4, 0 };
    SAFEARRAY *sa = w_SafeArrayCreate(VT_I4, 1, &b);
    CHECK(sa != NULL);
    VARIANT v; V_VT(&v) = VT_ARRAY | VT_I4; V_ARRAY(&v) = sa;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_array_of_bstr_and_variant) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement));
    SAFEARRAYBOUND b = { 3, 1 };
    SAFEARRAY *sa = w_SafeArrayCreate(VT_BSTR, 1, &b);
    LONG idx = 2; BSTR s = w_SysAllocString(L"elem");
    HR_EQ(w_SafeArrayPutElement(sa, &idx, s), S_OK);
    w_SysFreeString(s);
    VARIANT v; V_VT(&v) = VT_ARRAY | VT_BSTR; V_ARRAY(&v) = sa;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);

    MU u; mu_init(&u);
    SAFEARRAY *sv = w_SafeArrayCreate(VT_VARIANT, 1, &b);
    VARIANT e; V_VT(&e) = VT_UNKNOWN; V_UNKNOWN(&e) = (IUnknown *)&u;
    idx = 1; HR_EQ(w_SafeArrayPutElement(sv, &idx, &e), S_OK);
    CHECK(u.nadd >= 1);
    V_VT(&v) = VT_ARRAY | VT_VARIANT; V_ARRAY(&v) = sv;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(u.nrel, u.nadd); /* element variants are cleared by array destruction: AddRefs balanced */
    EXPECT_EQ(u.ref, 1);
}
TEST(vc_clear_array_of_unknown_releases_elements) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement));
    MU u; mu_init(&u);
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *sa = w_SafeArrayCreate(VT_UNKNOWN, 1, &b);
    LONG idx = 0; HR_EQ(w_SafeArrayPutElement(sa, &idx, &u), S_OK);
    idx = 1; HR_EQ(w_SafeArrayPutElement(sa, &idx, &u), S_OK);
    CHECK(u.nadd >= 2);
    VARIANT v; V_VT(&v) = VT_ARRAY | VT_UNKNOWN; V_ARRAY(&v) = sa;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(u.nrel, u.nadd);
    EXPECT_EQ(u.ref, 1);
}
TEST(vc_clear_null_array_pointer) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = VT_ARRAY | VT_I4;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
/* decomp: cLocks != 0 -> DISP_E_ARRAYISLOCKED, vt restored (variant remains valid and owns the array) */
TEST(vc_clear_locked_array_fails_and_keeps_vt) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayLock)); REQUIRE(U(SafeArrayUnlock));
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *sa = w_SafeArrayCreate(VT_I4, 1, &b);
    VARIANT v; V_VT(&v) = VT_ARRAY | VT_I4; V_ARRAY(&v) = sa;
    HR_EQ(w_SafeArrayLock(sa), S_OK);
    HR_EQ(w_VariantClear(&v), VC_LOCKED);
    EXPECT_EQ(V_VT(&v), VT_ARRAY | VT_I4);
    CHECK(V_ARRAY(&v) == sa);
    EXPECT_EQ(sa->cLocks, 1);
    HR_EQ(w_SafeArrayUnlock(sa), S_OK);
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
}
TEST(vc_clear_byref_array_not_destroyed) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayGetUBound));
    SAFEARRAYBOUND b = { 5, 0 };
    SAFEARRAY *sa = w_SafeArrayCreate(VT_I4, 1, &b);
    VARIANT v; V_VT(&v) = VT_ARRAY | VT_BYREF | VT_I4; V_ARRAYREF(&v) = &sa;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
    LONG ub = -1; HR_EQ(w_SafeArrayGetUBound(sa, 1, &ub), S_OK); EXPECT_EQ(ub, 4); /* still alive */
    HR_EQ(w_SafeArrayDestroy(sa), S_OK);
}
TEST(vc_clear_vector_array) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayLock));
    /* An array created by SafeArrayCreateVector is heap based; use a manually built FADF_STATIC|FADF_FIXEDSIZE-free
     * descriptor only via the API: here simply verify a vector array clears fine. */
    REQUIRE(U(SafeArrayCreateVector));
    SAFEARRAY *sa = w_SafeArrayCreateVector(VT_UI1, 0, 16);
    VARIANT v; V_VT(&v) = VT_ARRAY | VT_UI1; V_ARRAY(&v) = sa;
    HR_EQ(w_VariantClear(&v), S_OK); EXPECT_EQ(V_VT(&v), VT_EMPTY);
}

/* ---------------------------------------------------------------- VT_RECORD */
TEST(vc_clear_record_null_pvrecord_releases_recinfo_only) {
    VC_REQ();
    MR r; mr_setup(&r, 16); r.ref = 2;
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_RECORD; V_RECORDINFO(&v) = (IRecordInfo *)&r; V_RECORD(&v) = NULL;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(r.nrel, 1); EXPECT_EQ(r.nclear, 0); EXPECT_EQ(r.nadd, 0); /* decomp: RecordClear only when pvRecord != NULL (wine calls it anyway) */
}
/* The record buffer is released by VariantClear with the DLL's operator delete (process heap on Win10),
 * so the test allocates it from the process heap. */
TEST(vc_clear_record_calls_recordclear_then_release) {
    VC_REQ();
    MR r; mr_setup(&r, 16); r.ref = 2;
    void *rec = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 16);
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_RECORD; V_RECORDINFO(&v) = (IRecordInfo *)&r; V_RECORD(&v) = rec;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(r.nclear, 1); CHECK(r.last_clear == rec);
    EXPECT_EQ(r.nrel, 1); EXPECT_EQ(r.nadd, 0);
    EXPECT_EQ(r.ref, 1);
    if (t_is_wine()) HeapFree(GetProcessHeap(), 0, rec); /* wine does not free the buffer */
}
TEST(vc_clear_record_byref_no_calls) {
    VC_REQ();
    MR r; mr_setup(&r, 16);
    char buf[16] = {0};
    VARIANT v; memset(&v, 0, sizeof v);
    V_VT(&v) = VT_RECORD | VT_BYREF; V_RECORDINFO(&v) = (IRecordInfo *)&r; V_RECORD(&v) = buf;
    HR_EQ(w_VariantClear(&v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_EMPTY);
    EXPECT_EQ(r.nclear + r.nrel + r.nadd, 0);
}

/* ---------------------------------------------------------------- loops over many vts with real payloads */
TEST(vc_clear_all_valid_vts_with_zero_payload) {
    VC_REQ();
    for (unsigned vt = 0; vt <= 80; vt++) {
        if (!vc_valid_vt(vt)) continue;
        VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = (VARTYPE)vt;
        HRESULT hr = w_VariantClear(&v);
        CHECKF(hr == S_OK && V_VT(&v) == VT_EMPTY, "vt %u hr 0x%08lx now %u", vt, (unsigned long)hr, V_VT(&v));
    }
}
TEST(vc_clear_vt_byref_all_valid_base_types_leave_pointer) {
    VC_REQ();
    char dummy[32] = {0};
    for (unsigned vt = 2; vt <= 23; vt++) {
        if (vt == 15) continue;
        VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = (VARTYPE)(vt | VT_BYREF); V_BYREF(&v) = dummy;
        HRESULT hr = w_VariantClear(&v);
        CHECKF(hr == S_OK, "byref vt %u hr 0x%08lx", vt, (unsigned long)hr);
        CHECKF(V_VT(&v) == VT_EMPTY && V_BYREF(&v) == dummy, "byref vt %u", vt);
    }
}
