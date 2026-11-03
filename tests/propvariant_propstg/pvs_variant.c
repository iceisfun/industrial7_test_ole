/* PROPVARIANT lifetime API: PropVariantClear / PropVariantCopy / FreePropVariantArray.
 * ole32 forwards these to combase (PropVariantClearWorker / PropVariantCopy); expectations below are
 * taken from the Win10 combase decompilation and cross-checked against Wine's propvariant.c. */
#include "pv.h"

#define SIZE24 sizeof(PROPVARIANT)

static void fill(void *p, size_t n, unsigned seed) { BYTE *b = p; for (size_t i = 0; i < n; i++) b[i] = (BYTE)(seed * 31 + i * 7 + 1); }

/* ------------------------------------------------------------------ Clear: argument handling */

TEST(pvps_pvc_clear_null_pointer) {
    REQUIRE(U(PropVariantClear));
    /* decomp: PropVariantClearWorker returns 0 for a NULL pointer */
    EQ(U(PropVariantClear)(NULL), S_OK);
}

TEST(pvps_pvc_init_macro_is_zero) {
    PROPVARIANT v; memset(&v, 0x5A, sizeof v);
    PropVariantInit(&v);
    CHECK(pv_zero(&v));
    CHECK(v.vt == VT_EMPTY);
    EQ(sizeof(PROPVARIANT), 24);   /* x64 layout: vt + 3 WORD pad + 16 byte union */
    EQ(offsetof(PROPVARIANT, caub.cElems), 8);
    EQ(offsetof(PROPVARIANT, caub.pElems), 16);
}

TEST(pvps_pvc_clear_empty_and_null) {
    REQUIRE(U(PropVariantClear));
    PROPVARIANT v;
    memset(&v, 0, sizeof v);
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));
    memset(&v, 0x77, sizeof v); v.vt = VT_EMPTY;
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));            /* the whole 24 bytes are cleared, not only vt */
    memset(&v, 0x77, sizeof v); v.vt = VT_NULL;
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));
}

/* scalar types with no owned storage: vt becomes VT_EMPTY and all bytes zero */
static const struct { VARTYPE vt; const char *n; } scalars[] = {
    {VT_I2,"I2"},{VT_I4,"I4"},{VT_R4,"R4"},{VT_R8,"R8"},{VT_CY,"CY"},{VT_DATE,"DATE"},{VT_ERROR,"ERROR"},
    {VT_BOOL,"BOOL"},{VT_I1,"I1"},{VT_UI1,"UI1"},{VT_UI2,"UI2"},{VT_UI4,"UI4"},{VT_I8,"I8"},{VT_UI8,"UI8"},
    {VT_INT,"INT"},{VT_UINT,"UINT"},{VT_FILETIME,"FILETIME"},
};

TEST(pvps_pvc_clear_scalars_zero_struct) {
    REQUIRE(U(PropVariantClear));
    for (int i = 0; i < N_ELEM(scalars); i++) {
        PROPVARIANT v; memset(&v, 0x55, sizeof v);
        v.vt = scalars[i].vt;
        HRESULT hr = U(PropVariantClear)(&v);
        CHECKF(hr == S_OK, "vt %s hr=%08lx", scalars[i].n, hr);
        CHECKF(pv_zero(&v), "vt %s not zeroed", scalars[i].n);
    }
}

TEST(pvps_pvc_clear_invalid_simple_vt) {
    REQUIRE(U(PropVariantClear));
    /* valid in a VARIANT/PROPVARIANT sense? no: these are rejected by Clear with STG_E_INVALIDPARAMETER
       (the failure is mapped from DISP_E_BADVARTYPE) and the structure is still zeroed. */
    static const int bad[] = {15, 24, 25, 26, 27, 28, 29, 32, 33, 34, 35, 37, 38, 39, 40, 50, 63};
    for (int i = 0; i < N_ELEM(bad); i++) {
        PROPVARIANT v; memset(&v, 0x55, sizeof v);
        v.vt = (VARTYPE)bad[i]; v.pszVal = NULL;
        HRESULT hr = U(PropVariantClear)(&v);
        CHECKF(hr == STG_E_INVALIDPARAMETER_, "vt %d hr=%08lx", bad[i], hr);
        CHECKF(v.vt == 0, "vt %d: vt after clear = %d", bad[i], v.vt);
        CHECKF(v.uhVal.QuadPart == 0, "vt %d: payload not cleared", bad[i]);
    }
}

TEST(pvps_pvc_clear_garbage_vt_0x5555) {
    REQUIRE(U(PropVariantClear));
    PROPVARIANT v; memset(&v, 0x55, sizeof v);
    EQ(U(PropVariantClear)(&v), STG_E_INVALIDPARAMETER_);
    EQ(v.vt, 0);
    CHECK(v.uhVal.QuadPart == 0);
}

TEST(pvps_pvc_clear_vt_illegal_0xffff_succeeds) {
    REQUIRE(U(PropVariantClear));
    /* decomp: vt == 0xFFFF is special-cased to "nothing to free": S_OK and the struct is zeroed */
    PROPVARIANT v; memset(&v, 0, sizeof v); v.vt = 0xFFFF; v.lVal = 7;
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_byref_not_freed) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysStringLen)); REQUIRE(U(SysFreeString));
    LONG x = 42; PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_BYREF | VT_I4; v.plVal = &x;
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v)); EQ(x, 42);
    /* a BYREF BSTR slot is not owned by the variant: Clear must leave the BSTR alive */
    BSTR b = U(SysAllocString)(L"keep"); BSTR *pb = &b;
    v.vt = VT_BYREF | VT_BSTR; v.pbstrVal = pb;
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));
    EQ(U(SysStringLen)(b), 4);
    U(SysFreeString)(b);
}

/* ------------------------------------------------------------------ Clear: owned storage */

TEST(pvps_pvc_clear_bstr) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(SysAllocString));
    PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_BSTR; v.bstrVal = U(SysAllocString)(L"hello bstr");
    CHECK(v.bstrVal != NULL);
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));
    v.vt = VT_BSTR; v.bstrVal = NULL;       /* a NULL BSTR is an empty string: nothing to free */
    EQ(U(PropVariantClear)(&v), S_OK);
    CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_lpstr_lpwstr_clsid) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc));
    PROPVARIANT v;
    PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = tm_strA("ansi");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = tm_strW(L"wide");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    PropVariantInit(&v); v.vt = VT_CLSID; v.puuid = tm_dup(&G_ANY_GUID, 16);
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    /* NULL pointers are skipped */
    PropVariantInit(&v); v.vt = VT_LPSTR; EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    PropVariantInit(&v); v.vt = VT_LPWSTR; EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    PropVariantInit(&v); v.vt = VT_CLSID; EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_blob) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc));
    PROPVARIANT v; PropVariantInit(&v);
    BYTE d[10]; fill(d, 10, 1);
    v.vt = VT_BLOB; v.blob.cbSize = 10; v.blob.pBlobData = tm_dup(d, 10);
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    v.vt = VT_BLOB; v.blob.cbSize = 0; v.blob.pBlobData = NULL;
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    v.vt = VT_BLOB_OBJECT; v.blob.cbSize = 4; v.blob.pBlobData = tm_dup(d, 4);
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_cf_frees_both_allocations) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc));
    PROPVARIANT v; PropVariantInit(&v);
    CLIPDATA *cd = tm_alloc(sizeof *cd);
    cd->cbSize = 4 + 6; cd->ulClipFmt = CF_TEXT; cd->pClipData = tm_dup("hello", 6);
    v.vt = VT_CF; v.pclipdata = cd;
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    v.vt = VT_CF; v.pclipdata = NULL;
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_unknown_dispatch_releases) {
    REQUIRE(U(PropVariantClear));
    TUnk u; tu_init(&u); PROPVARIANT v; PropVariantInit(&v);
    u.ref = 2;                                  /* one for us, one owned by the variant */
    v.vt = VT_UNKNOWN; v.punkVal = (IUnknown *)&u;
    EQ(U(PropVariantClear)(&v), S_OK);
    EQ(u.ref, 1); CHECK(pv_zero(&v));
    u.ref = 2; v.vt = VT_DISPATCH; v.pdispVal = (IDispatch *)&u;
    EQ(U(PropVariantClear)(&v), S_OK);
    EQ(u.ref, 1); CHECK(pv_zero(&v));
    v.vt = VT_UNKNOWN; v.punkVal = NULL;
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_stream_storage_release) {
    REQUIRE(U(PropVariantClear));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pvc_stream.stg")) {
        IStream *s = NULL; IStorage *sub = NULL;
        EQ(CALL(f.stg, CreateStream, L"s", STGM_RW_EX_CREATE, 0, 0, &s), S_OK);
        EQ(CALL(f.stg, CreateStorage, L"sub", STGM_RW_EX_CREATE, 0, 0, &sub), S_OK);
        if (s && sub) {
            PROPVARIANT v; PropVariantInit(&v);
            CALL(s, AddRef);                             /* 2: ours + variant's */
            v.vt = VT_STREAM; v.pStream = s;
            EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
            EQ(CALL(s, Release), 0);              /* variant dropped its reference */
            CALL(sub, AddRef);
            v.vt = VT_STORAGE; v.pStorage = sub;
            EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
            EQ(CALL(sub, Release), 0);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pvc_clear_streamed_stored_object_types) {
    REQUIRE(U(PropVariantClear));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pvc_sobj.stg")) {
        IStream *s = NULL;
        CALL(f.stg, CreateStream, L"s", STGM_RW_EX_CREATE, 0, 0, &s);
        if (s) {
            PROPVARIANT v; PropVariantInit(&v);
            CALL(s, AddRef);
            v.vt = VT_STREAMED_OBJECT; v.pStream = s;
            EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
            EQ(CALL(s, Release), 0);
        }
        fx_close(&f);
    }
    COM_END();
}

static const struct { VARTYPE vt; size_t esz; const char *n; } vscalars[] = {
    {VT_I1,1,"I1"},{VT_UI1,1,"UI1"},{VT_I2,2,"I2"},{VT_UI2,2,"UI2"},{VT_BOOL,2,"BOOL"},
    {VT_I4,4,"I4"},{VT_UI4,4,"UI4"},{VT_R4,4,"R4"},{VT_ERROR,4,"ERROR"},
    {VT_I8,8,"I8"},{VT_UI8,8,"UI8"},{VT_R8,8,"R8"},{VT_CY,8,"CY"},{VT_DATE,8,"DATE"},{VT_FILETIME,8,"FILETIME"},
    {VT_CLSID,16,"CLSID"},
};

TEST(pvps_pvc_clear_vector_scalars) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc));
    for (int i = 0; i < N_ELEM(vscalars); i++) {
        PROPVARIANT v; PropVariantInit(&v);
        BYTE data[16 * 4]; fill(data, sizeof data, i);
        v.vt = VT_VECTOR | vscalars[i].vt;
        v.caub.cElems = 4; v.caub.pElems = tm_dup(data, vscalars[i].esz * 4);
        HRESULT hr = U(PropVariantClear)(&v);
        CHECKF(hr == S_OK, "vector %s hr=%08lx", vscalars[i].n, hr);
        CHECKF(pv_zero(&v), "vector %s not zeroed", vscalars[i].n);
        PropVariantInit(&v); v.vt = VT_VECTOR | vscalars[i].vt;
        hr = U(PropVariantClear)(&v);
        CHECKF(hr == S_OK, "empty vector %s hr=%08lx", vscalars[i].n, hr);
        CHECK(pv_zero(&v));
    }
}

TEST(pvps_pvc_clear_vector_strings) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc)); REQUIRE(U(SysAllocString));
    PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_VECTOR | VT_LPSTR; v.calpstr.cElems = 2;
    v.calpstr.pElems = tm_alloc(2 * sizeof(char *));
    v.calpstr.pElems[0] = tm_strA("a"); v.calpstr.pElems[1] = tm_strA("bb");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    v.vt = VT_VECTOR | VT_LPWSTR; v.calpwstr.cElems = 2;
    v.calpwstr.pElems = tm_alloc(2 * sizeof(WCHAR *));
    v.calpwstr.pElems[0] = tm_strW(L"a"); v.calpwstr.pElems[1] = tm_strW(L"bb");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    v.vt = VT_VECTOR | VT_BSTR; v.cabstr.cElems = 2;
    v.cabstr.pElems = tm_alloc(2 * sizeof(BSTR));
    v.cabstr.pElems[0] = U(SysAllocString)(L"a"); v.cabstr.pElems[1] = U(SysAllocString)(L"bb");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
    /* a NULL element pointer inside a string vector is tolerated */
    v.vt = VT_VECTOR | VT_LPWSTR; v.calpwstr.cElems = 2;
    v.calpwstr.pElems = tm_alloc(2 * sizeof(WCHAR *));
    v.calpwstr.pElems[0] = NULL; v.calpwstr.pElems[1] = tm_strW(L"x");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_vector_variant_recursive) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc)); REQUIRE(U(SysAllocString));
    PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_VECTOR | VT_VARIANT; v.capropvar.cElems = 3;
    v.capropvar.pElems = tm_alloc(3 * sizeof(PROPVARIANT));
    memset(v.capropvar.pElems, 0, 3 * sizeof(PROPVARIANT));
    v.capropvar.pElems[0].vt = VT_I4; v.capropvar.pElems[0].lVal = 5;
    v.capropvar.pElems[1].vt = VT_LPWSTR; v.capropvar.pElems[1].pwszVal = tm_strW(L"nested");
    v.capropvar.pElems[2].vt = VT_BSTR; v.capropvar.pElems[2].bstrVal = U(SysAllocString)(L"bs");
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_vector_cf) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(CoTaskMemAlloc));
    PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_VECTOR | VT_CF; v.caclipdata.cElems = 2;
    v.caclipdata.pElems = tm_alloc(2 * sizeof(CLIPDATA));
    for (int i = 0; i < 2; i++) {
        v.caclipdata.pElems[i].cbSize = 4 + 3; v.caclipdata.pElems[i].ulClipFmt = CF_TEXT;
        v.caclipdata.pElems[i].pClipData = tm_dup("ab", 3);
    }
    EQ(U(PropVariantClear)(&v), S_OK); CHECK(pv_zero(&v));
}

TEST(pvps_pvc_clear_safearray) {
    REQUIRE(U(PropVariantClear)); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement));
    TUnk u; tu_init(&u);
    SAFEARRAYBOUND b = {2, 0};
    SAFEARRAY *sa = U(SafeArrayCreate)(VT_UNKNOWN, 1, &b);
    CHECK(sa != NULL);
    if (!sa) return;
    LONG i = 0; U(SafeArrayPutElement)(sa, &i, &u);
    i = 1; U(SafeArrayPutElement)(sa, &i, &u);
    EQ(u.ref, 3);
    PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_ARRAY | VT_UNKNOWN; v.parray = sa;
    EQ(U(PropVariantClear)(&v), S_OK);
    EQ(u.ref, 1); CHECK(pv_zero(&v));
}

/* ------------------------------------------------------------------ FreePropVariantArray */

TEST(pvps_pvc_freearray_null_and_zero) {
    REQUIRE(U(FreePropVariantArray));
    EQ(U(FreePropVariantArray)(0, NULL), S_OK);          /* decomp: c==0 && p==NULL -> 0 */
    PROPVARIANT v; memset(&v, 0x55, sizeof v);
    EQ(U(FreePropVariantArray)(0, &v), S_OK);            /* zero elements: nothing touched */
    BYTE b = *(BYTE *)&v; EQ(b, 0x55);
    EQ(U(FreePropVariantArray)(1, NULL), E_INVALIDARG_);
    EQ(U(FreePropVariantArray)(100, NULL), E_INVALIDARG_);
}

TEST(pvps_pvc_freearray_clears_all_elements) {
    REQUIRE(U(FreePropVariantArray)); REQUIRE(U(CoTaskMemAlloc));
    PROPVARIANT a[4]; memset(a, 0, sizeof a);
    a[0].vt = VT_I4; a[0].lVal = 9;
    a[1].vt = VT_LPSTR; a[1].pszVal = tm_strA("x");
    a[2].vt = VT_LPWSTR; a[2].pwszVal = tm_strW(L"y");
    a[3].vt = VT_BLOB; a[3].blob.cbSize = 3; a[3].blob.pBlobData = tm_dup("abc", 3);
    EQ(U(FreePropVariantArray)(4, a), S_OK);
    for (int i = 0; i < 4; i++) CHECKF(pv_zero(&a[i]), "element %d not zeroed", i);
}

TEST(pvps_pvc_freearray_continues_past_invalid_element) {
    REQUIRE(U(FreePropVariantArray)); REQUIRE(U(CoTaskMemAlloc));
    PROPVARIANT a[3]; memset(a, 0, sizeof a);
    a[0].vt = VT_LPSTR; a[0].pszVal = tm_strA("first");
    memset(&a[1], 0, sizeof a[1]); a[1].vt = 26 /* VT_PTR: invalid */;
    a[2].vt = VT_LPSTR; a[2].pszVal = tm_strA("third");
    /* decomp: failing element maps to STG_E_INVALIDPARAMETER, remaining elements are still cleared */
    EQ(U(FreePropVariantArray)(3, a), STG_E_INVALIDPARAMETER_);
    CHECK(pv_zero(&a[0])); CHECK(pv_zero(&a[2])); CHECK(a[1].vt == 0);
}

TEST(pvps_pvc_freearray_unknown_refs) {
    REQUIRE(U(FreePropVariantArray));
    TUnk u; tu_init(&u); u.ref = 3;
    PROPVARIANT a[2]; memset(a, 0, sizeof a);
    a[0].vt = VT_UNKNOWN; a[0].punkVal = (IUnknown *)&u;
    a[1].vt = VT_UNKNOWN; a[1].punkVal = (IUnknown *)&u;
    EQ(U(FreePropVariantArray)(2, a), S_OK);
    EQ(u.ref, 1);
}

/* ------------------------------------------------------------------ Copy */

TEST(pvps_pvc_copy_null_arguments) {
    REQUIRE(U(PropVariantCopy));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    PROPVARIANT a, b; PropVariantInit(&a); PropVariantInit(&b); a.vt = VT_I4;
    EQ(U(PropVariantCopy)(NULL, &a), E_INVALIDARG_);
    EQ(U(PropVariantCopy)(&b, NULL), E_INVALIDARG_);
    EQ(U(PropVariantCopy)(NULL, NULL), E_INVALIDARG_);
}

TEST(pvps_pvc_copy_scalars_bitwise) {
    REQUIRE(U(PropVariantCopy));
    for (int i = 0; i < N_ELEM(scalars); i++) {
        PROPVARIANT s, d; memset(&s, 0, sizeof s); memset(&d, 0x77, sizeof d);
        s.vt = scalars[i].vt; fill(&s.uhVal, 8, i + 3);
        HRESULT hr = U(PropVariantCopy)(&d, &s);
        CHECKF(hr == S_OK, "vt %s hr=%08lx", scalars[i].n, hr);
        CHECKF(d.vt == s.vt, "vt %s: vt %d", scalars[i].n, d.vt);
        CHECKF(d.uhVal.QuadPart == s.uhVal.QuadPart, "vt %s payload differs", scalars[i].n);
    }
}

TEST(pvps_pvc_copy_empty_null) {
    REQUIRE(U(PropVariantCopy));
    PROPVARIANT s, d; PropVariantInit(&s); memset(&d, 0x77, sizeof d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK); EQ(d.vt, VT_EMPTY);
    s.vt = VT_NULL; memset(&d, 0x77, sizeof d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK); EQ(d.vt, VT_NULL);
}

TEST(pvps_pvc_copy_decimal_and_variant_types) {
    REQUIRE(U(PropVariantCopy));
    PROPVARIANT s, d; memset(&s, 0, sizeof s); memset(&d, 0, sizeof d);
    s.vt = VT_DECIMAL; s.decVal.Lo64 = 0x1122334455667788ull; s.decVal.Hi32 = 0x99; s.decVal.scale = 2; s.decVal.sign = 0x80;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_DECIMAL);
    CHECK(d.decVal.Lo64 == s.decVal.Lo64); EQ(d.decVal.Hi32, 0x99);
    EQ(d.decVal.scale, 2); EQ(d.decVal.sign, 0x80);
}

TEST(pvps_pvc_copy_bstr_independent) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear)); REQUIRE(U(SysAllocString));
    REQUIRE(U(SysStringLen)); REQUIRE(U(SysAllocStringLen));
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_BSTR; s.bstrVal = U(SysAllocString)(L"Test String");
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_BSTR);
    CHECK(d.bstrVal != s.bstrVal);
    EXPECT_WSTR(d.bstrVal, s.bstrVal);
    EQ(U(SysStringLen)(d.bstrVal), 11);
    EQ(U(PropVariantClear)(&s), S_OK);
    EXPECT_WSTR(d.bstrVal, L"Test String");        /* copy survives the source being cleared */
    EQ(U(PropVariantClear)(&d), S_OK);
    /* NULL BSTR copies as NULL */
    PropVariantInit(&s); s.vt = VT_BSTR; s.bstrVal = NULL; memset(&d, 0x77, sizeof d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_BSTR); CHECK(d.bstrVal == NULL);
}

TEST(pvps_pvc_copy_bstr_embedded_nul_truncates) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear)); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysStringLen));
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+) */
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_BSTR; s.bstrVal = U(SysAllocStringLen)(L"Test Str\0ing", 12);
    EQ(U(SysStringLen)(s.bstrVal), 12);
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(U(SysStringLen)(d.bstrVal), 8);        /* copy is made with the NUL-terminated length */
    EQ(U(SysStringLen)(s.bstrVal), 12);
    EXPECT_WSTR(d.bstrVal, s.bstrVal);
    U(PropVariantClear)(&s); U(PropVariantClear)(&d);
}

TEST(pvps_pvc_copy_lpstr_lpwstr) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    char as[] = "Test String"; WCHAR ws[] = L"Test String";
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_LPSTR; s.pszVal = as;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_LPSTR); CHECK(d.pszVal != as); EXPECT_STR(d.pszVal, as);
    EQ(U(PropVariantClear)(&d), S_OK);
    s.vt = VT_LPWSTR; s.pwszVal = ws; PropVariantInit(&d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_LPWSTR); CHECK(d.pwszVal != ws); EXPECT_WSTR(d.pwszVal, ws);
    EQ(U(PropVariantClear)(&d), S_OK);
    /* empty string */
    char e[] = ""; s.vt = VT_LPSTR; s.pszVal = e;
    EQ(U(PropVariantCopy)(&d, &s), S_OK); CHECK(d.pszVal && d.pszVal != e && d.pszVal[0] == 0);
    U(PropVariantClear)(&d);
    /* NULL pointers stay NULL */
    s.vt = VT_LPSTR; s.pszVal = NULL; memset(&d, 0x77, sizeof d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK); EQ(d.vt, VT_LPSTR); CHECK(d.pszVal == NULL);
    s.vt = VT_LPWSTR; s.pwszVal = NULL; memset(&d, 0x77, sizeof d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK); EQ(d.vt, VT_LPWSTR); CHECK(d.pwszVal == NULL);
}

TEST(pvps_pvc_copy_clsid_filetime) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    GUID g = G_ANY_GUID;
    s.vt = VT_CLSID; s.puuid = &g;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_CLSID); CHECK(d.puuid != &g); CHECK(guid_eq(d.puuid, &g));
    U(PropVariantClear)(&d);
    PropVariantInit(&s); s.vt = VT_FILETIME; s.filetime.dwLowDateTime = 0x1234; s.filetime.dwHighDateTime = 0x5678;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.filetime.dwLowDateTime, 0x1234); EQ(d.filetime.dwHighDateTime, 0x5678);
}

TEST(pvps_pvc_copy_blob) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    BYTE data[33]; fill(data, sizeof data, 9);
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_BLOB; s.blob.cbSize = sizeof data; s.blob.pBlobData = data;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_BLOB); EQ(d.blob.cbSize, sizeof data);
    CHECK(d.blob.pBlobData != data); CHECK(!memcmp(d.blob.pBlobData, data, sizeof data));
    EQ(U(PropVariantClear)(&d), S_OK);
    s.blob.cbSize = 0; s.blob.pBlobData = NULL; memset(&d, 0x77, sizeof d);
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_BLOB); EQ(d.blob.cbSize, 0); CHECK(d.blob.pBlobData == NULL);
    s.vt = VT_BLOB_OBJECT; s.blob.cbSize = 5; s.blob.pBlobData = data;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_BLOB_OBJECT); EQ(d.blob.cbSize, 5); CHECK(!memcmp(d.blob.pBlobData, data, 5));
    U(PropVariantClear)(&d);
}

TEST(pvps_pvc_copy_cf) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    BYTE data[] = {1, 2, 3, 4, 5, 6, 7};
    CLIPDATA cd; cd.cbSize = sizeof data + sizeof(ULONG); cd.ulClipFmt = CF_ENHMETAFILE; cd.pClipData = data;
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_CF; s.pclipdata = &cd;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_CF); CHECK(d.pclipdata != NULL && d.pclipdata != &cd);
    if (d.pclipdata) {
        EQ(d.pclipdata->cbSize, cd.cbSize);
        EQ(d.pclipdata->ulClipFmt, CF_ENHMETAFILE);
        CHECK(d.pclipdata->pClipData != data);
        CHECK(!memcmp(d.pclipdata->pClipData, data, sizeof data));   /* cbSize counts the 4-byte format field */
    }
    EQ(U(PropVariantClear)(&d), S_OK);
}

TEST(pvps_pvc_copy_unknown_addrefs) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+) */
    TUnk u; tu_init(&u);
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_UNKNOWN; s.punkVal = (IUnknown *)&u;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    CHECK(d.punkVal == (IUnknown *)&u); EQ(u.ref, 2);
    EQ(U(PropVariantClear)(&d), S_OK); EQ(u.ref, 1);
    s.vt = VT_DISPATCH; s.pdispVal = (IDispatch *)&u;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_DISPATCH); EQ(u.ref, 2);
    U(PropVariantClear)(&d); EQ(u.ref, 1);
}

TEST(pvps_pvc_copy_stream_storage_addrefs) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pvc_copy_stream.stg")) {
        IStream *s = NULL;
        CALL(f.stg, CreateStream, L"s", STGM_RW_EX_CREATE, 0, 0, &s);
        if (s) {
            PROPVARIANT a, d; PropVariantInit(&a); PropVariantInit(&d);
            a.vt = VT_STREAM; a.pStream = s;
            EQ(U(PropVariantCopy)(&d, &a), S_OK);
            EQ(d.vt, VT_STREAM); CHECK(d.pStream == s);
            CALL(s, AddRef); EQ(CALL(s, Release), 2);   /* ours + the copy */
            EQ(U(PropVariantClear)(&d), S_OK);
            EQ(CALL(s, Release), 0);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pvc_copy_safearray_unknown_refs) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear)); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement));
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+) */
    TUnk u; tu_init(&u);
    SAFEARRAYBOUND b = {2, 0};
    SAFEARRAY *sa = U(SafeArrayCreate)(VT_UNKNOWN, 1, &b);
    if (!sa) { CHECK(sa != NULL); return; }
    LONG i = 0; U(SafeArrayPutElement)(sa, &i, &u); i = 1; U(SafeArrayPutElement)(sa, &i, &u);
    EQ(u.ref, 3);
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_ARRAY | VT_UNKNOWN; s.parray = sa;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_ARRAY | VT_UNKNOWN); CHECK(d.parray != sa);
    EQ(u.ref, 5);
    EQ(U(PropVariantClear)(&d), S_OK); EQ(u.ref, 3);
    EQ(U(PropVariantClear)(&s), S_OK); EQ(u.ref, 1);
}

TEST(pvps_pvc_copy_safearray_i4_deep) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear)); REQUIRE(U(SafeArrayCreate));
    REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayGetElement));
    SAFEARRAYBOUND b = {3, 0};
    SAFEARRAY *sa = U(SafeArrayCreate)(VT_I4, 1, &b);
    if (!sa) { CHECK(sa != NULL); return; }
    for (LONG i = 0; i < 3; i++) { LONG v = 100 + i; U(SafeArrayPutElement)(sa, &i, &v); }
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_ARRAY | VT_I4; s.parray = sa;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    CHECK(d.parray != NULL && d.parray != sa);
    U(PropVariantClear)(&s);                       /* destroys the source array */
    for (LONG i = 0; i < 3; i++) { LONG v = 0; EQ(U(SafeArrayGetElement)(d.parray, &i, &v), S_OK); EQ(v, 100 + i); }
    U(PropVariantClear)(&d);
}

TEST(pvps_pvc_copy_byref_shallow) {
    REQUIRE(U(PropVariantCopy));
    LONG x = 5; PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_BYREF | VT_I4; s.plVal = &x;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_BYREF | VT_I4); CHECK(d.plVal == &x);
}

TEST(pvps_pvc_copy_invalid_simple_vt) {
    REQUIRE(U(PropVariantCopy));
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+): invalid simple types are rejected with
       DISP_E_BADVARTYPE and the destination is left untouched */
    static const int bad[] = {15, 24, 25, 26, 27, 28, 29, 32, 33, 34, 35, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                              46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
    for (int i = 0; i < N_ELEM(bad); i++) {
        PROPVARIANT s, d, un; memset(&s, 0x55, sizeof s); s.vt = (VARTYPE)bad[i]; s.pszVal = NULL;
        memset(&d, 0x77, sizeof d); memset(&un, 0x77, sizeof un);
        HRESULT hr = U(PropVariantCopy)(&d, &s);
        CHECKF(hr == DISP_E_BADVARTYPE_, "vt %d hr=%08lx", bad[i], hr);
        CHECKF(!memcmp(&d, &un, sizeof d), "vt %d: destination modified", bad[i]);
    }
}

TEST(pvps_pvc_copy_invalid_vector_base) {
    REQUIRE(U(PropVariantCopy));
    /* vector of types that cannot be vectors: Windows reports one of two codes */
    static const VARTYPE bad[] = {VT_DISPATCH, VT_UNKNOWN, VT_DECIMAL, VT_INT, VT_UINT, VT_BLOB, VT_STREAM, VT_STORAGE, VT_EMPTY, VT_NULL};
    for (int i = 0; i < N_ELEM(bad); i++) {
        PROPVARIANT s, d; memset(&s, 0x55, sizeof s); s.vt = VT_VECTOR | bad[i];
        s.caub.cElems = 0; s.caub.pElems = NULL; memset(&d, 0x77, sizeof d);
        HRESULT hr = U(PropVariantCopy)(&d, &s);
        CHECKF(hr == DISP_E_BADVARTYPE_ || hr == STG_E_INVALIDPARAMETER_, "vector of vt %d hr=%08lx", bad[i], hr);
    }
}

TEST(pvps_pvc_copy_vector_scalars_deep) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    for (int i = 0; i < N_ELEM(vscalars); i++) {
        BYTE data[16 * 5]; fill(data, sizeof data, i + 40);
        PROPVARIANT s, d; PropVariantInit(&s); memset(&d, 0x77, sizeof d);
        s.vt = VT_VECTOR | vscalars[i].vt; s.caub.cElems = 5; s.caub.pElems = data;
        HRESULT hr = U(PropVariantCopy)(&d, &s);
        CHECKF(hr == S_OK, "vector %s hr=%08lx", vscalars[i].n, hr);
        if (hr != S_OK) continue;
        CHECKF(d.vt == s.vt, "vector %s vt", vscalars[i].n);
        CHECKF(d.caub.cElems == 5, "vector %s count %lu", vscalars[i].n, d.caub.cElems);
        CHECKF(d.caub.pElems != data, "vector %s not duplicated", vscalars[i].n);
        CHECKF(!memcmp(d.caub.pElems, data, vscalars[i].esz * 5), "vector %s data", vscalars[i].n);
        U(PropVariantClear)(&d);
    }
}

TEST(pvps_pvc_copy_vector_empty_forms) {
    REQUIRE(U(PropVariantCopy));
    PROPVARIANT s, d;
    PropVariantInit(&s); memset(&d, 0x77, sizeof d);
    s.vt = VT_VECTOR | VT_I4; s.cal.cElems = 0; s.cal.pElems = NULL;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_VECTOR | VT_I4); EQ(d.cal.cElems, 0); CHECK(d.cal.pElems == NULL);
    /* decomp: a NULL element pointer yields an empty vector even if the count is non-zero */
    if (t_is_wine()) { t_skip("wine crashes copying a NULL element array"); return; }
    PropVariantInit(&s); memset(&d, 0x77, sizeof d);
    s.vt = VT_VECTOR | VT_I4; s.cal.cElems = 5; s.cal.pElems = NULL;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_VECTOR | VT_I4); EQ(d.cal.cElems, 0); CHECK(d.cal.pElems == NULL);
}

TEST(pvps_pvc_copy_vector_lpstr) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    char *el[3] = {"one", "", "three"};
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_VECTOR | VT_LPSTR; s.calpstr.cElems = 3; s.calpstr.pElems = el;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_VECTOR | VT_LPSTR); EQ(d.calpstr.cElems, 3);
    for (int i = 0; i < 3 && d.calpstr.pElems; i++) {
        CHECK(d.calpstr.pElems[i] != el[i]);
        if (d.calpstr.pElems[i]) EXPECT_STR(d.calpstr.pElems[i], el[i]);
    }
    EQ(U(PropVariantClear)(&d), S_OK);
}

TEST(pvps_pvc_copy_vector_lpwstr) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    WCHAR *el[3] = {L"a", L"bc", L"def"};
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_VECTOR | VT_LPWSTR; s.calpwstr.cElems = 3; s.calpwstr.pElems = el;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.calpwstr.cElems, 3);
    for (int i = 0; i < 3 && d.calpwstr.pElems; i++) {
        CHECK(d.calpwstr.pElems[i] != el[i]);
        if (d.calpwstr.pElems[i]) EXPECT_WSTR(d.calpwstr.pElems[i], el[i]);
    }
    EQ(U(PropVariantClear)(&d), S_OK);
}

TEST(pvps_pvc_copy_vector_bstr) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear)); REQUIRE(U(SysAllocString));
    BSTR el[2] = {U(SysAllocString)(L"alpha"), U(SysAllocString)(L"beta")};
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_VECTOR | VT_BSTR; s.cabstr.cElems = 2; s.cabstr.pElems = el;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.cabstr.cElems, 2);
    for (int i = 0; i < 2 && d.cabstr.pElems; i++) {
        CHECK(d.cabstr.pElems[i] != el[i]);
        if (d.cabstr.pElems[i]) EXPECT_WSTR(d.cabstr.pElems[i], el[i]);
    }
    EQ(U(PropVariantClear)(&d), S_OK);
    U(SysFreeString)(el[0]); U(SysFreeString)(el[1]);
}

TEST(pvps_pvc_copy_vector_variant_deep) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    PROPVARIANT el[3]; memset(el, 0, sizeof el);
    el[0].vt = VT_I4; el[0].lVal = -7;
    el[1].vt = VT_LPWSTR; el[1].pwszVal = L"inner";
    el[2].vt = VT_VECTOR | VT_UI1; BYTE bytes[3] = {1, 2, 3}; el[2].caub.cElems = 3; el[2].caub.pElems = bytes;
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_VECTOR | VT_VARIANT; s.capropvar.cElems = 3; s.capropvar.pElems = el;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_VECTOR | VT_VARIANT); EQ(d.capropvar.cElems, 3);
    if (d.capropvar.pElems && d.capropvar.cElems == 3) {
        PROPVARIANT *e = d.capropvar.pElems;
        EQ(e[0].vt, VT_I4); EQ(e[0].lVal, -7);
        EQ(e[1].vt, VT_LPWSTR); CHECK(e[1].pwszVal != el[1].pwszVal); EXPECT_WSTR(e[1].pwszVal, L"inner");
        EQ(e[2].vt, VT_VECTOR | VT_UI1); EQ(e[2].caub.cElems, 3);
        CHECK(e[2].caub.pElems != bytes);
        if (e[2].caub.pElems) CHECK(!memcmp(e[2].caub.pElems, bytes, 3));
    }
    EQ(U(PropVariantClear)(&d), S_OK);
}

TEST(pvps_pvc_copy_vector_cf) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear));
    if (t_is_wine()) { t_skip("wine mis-copies VT_VECTOR|VT_CF"); return; }
    BYTE b0[] = "ab", b1[] = "wxyz";
    CLIPDATA el[2];
    el[0].cbSize = 4 + 3; el[0].ulClipFmt = CF_TEXT; el[0].pClipData = b0;
    el[1].cbSize = 4 + 5; el[1].ulClipFmt = CF_UNICODETEXT; el[1].pClipData = b1;
    PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
    s.vt = VT_VECTOR | VT_CF; s.caclipdata.cElems = 2; s.caclipdata.pElems = el;
    EQ(U(PropVariantCopy)(&d, &s), S_OK);
    EQ(d.vt, VT_VECTOR | VT_CF); EQ(d.caclipdata.cElems, 2);
    if (d.caclipdata.pElems && d.caclipdata.cElems == 2) {
        EQ(d.caclipdata.pElems[0].ulClipFmt, CF_TEXT);
        EQ(d.caclipdata.pElems[1].ulClipFmt, CF_UNICODETEXT);
        EQ(d.caclipdata.pElems[0].cbSize, 7); EQ(d.caclipdata.pElems[1].cbSize, 9);
        CHECK(d.caclipdata.pElems[0].pClipData != b0);
        CHECK(!memcmp(d.caclipdata.pElems[0].pClipData, b0, 3));
        CHECK(!memcmp(d.caclipdata.pElems[1].pClipData, b1, 5));
    }
    EQ(U(PropVariantClear)(&d), S_OK);
}

/* Copy followed by Clear on both sides must never double-free: run a mixed batch many times. */
TEST(pvps_pvc_copy_clear_loop_stability) {
    REQUIRE(U(PropVariantCopy)); REQUIRE(U(PropVariantClear)); REQUIRE(U(SysAllocString));
    for (int i = 0; i < 2000; i++) {
        PROPVARIANT s, d; PropVariantInit(&s); PropVariantInit(&d);
        switch (i % 5) {
        case 0: s.vt = VT_BSTR; s.bstrVal = U(SysAllocString)(L"loop"); break;
        case 1: s.vt = VT_LPSTR; s.pszVal = tm_strA("loop"); break;
        case 2: s.vt = VT_LPWSTR; s.pwszVal = tm_strW(L"loop"); break;
        case 3: s.vt = VT_BLOB; s.blob.cbSize = 7; s.blob.pBlobData = tm_dup("1234567", 7); break;
        default: s.vt = VT_CLSID; s.puuid = tm_dup(&G_ANY_GUID, 16); break;
        }
        if (U(PropVariantCopy)(&d, &s) != S_OK) { CHECK(0); break; }
        if (U(PropVariantClear)(&s) != S_OK) CHECK(0);
        if (U(PropVariantClear)(&d) != S_OK) CHECK(0);
    }
    CHECK(1);
}
