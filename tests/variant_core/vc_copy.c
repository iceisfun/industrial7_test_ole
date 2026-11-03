/* variant_core: VariantCopy / VariantCopyInd */
#include "vc.h"

static void fill_pattern(VARIANT *v, VARTYPE vt) {
    unsigned char *p = (unsigned char *)v;
    for (unsigned i = 0; i < sizeof *v; i++) p[i] = (unsigned char)(0x40 + i);
    V_VT(v) = vt;
}

/* ================================================================ VariantCopy */
/* decomp: for non-BSTR/ARRAY/RECORD types the whole 24-byte structure is copied (reserved words included) */
TEST(vc_copy_scalars_copy_whole_struct) {
    VC_REQ();
    static const VARTYPE vts[] = { VT_EMPTY, VT_NULL, VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_ERROR, VT_BOOL,
        VT_DECIMAL, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT s, d; fill_pattern(&s, vts[i]); memset(&d, 0, sizeof d);
        HR_EQ(w_VariantCopy(&d, &s), S_OK);
        CHECKF(!memcmp(&s, &d, sizeof s), "vt %u not bit-identical copy", vts[i]);
        EXPECT_EQ(V_VT(&d), vts[i]);
    }
}
TEST(vc_copy_does_not_modify_source) {
    VC_REQ();
    VARIANT s, d, ref; V_VT(&s) = VT_R8; V_R8(&s) = 6.5; ref = s; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    CHECK(!memcmp(&s, &ref, sizeof s));
    CHECK(V_R8(&d) == 6.5);
}
TEST(vc_copy_to_self_is_noop) {
    VC_REQ();
    MU u; mu_init(&u);
    BSTR b = w_SysAllocString(L"self");
    VARIANT v;
    V_VT(&v) = VT_I4; V_I4(&v) = 99;
    HR_EQ(w_VariantCopy(&v, &v), S_OK); EXPECT_EQ(V_VT(&v), VT_I4); EXPECT_EQ(V_I4(&v), 99);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = b;
    HR_EQ(w_VariantCopy(&v, &v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_BSTR); CHECK(V_BSTR(&v) == b); EXPECT_WSTR(b, L"self");
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)&u;
    HR_EQ(w_VariantCopy(&v, &v), S_OK);
    EXPECT_EQ(u.nadd + u.nrel + u.nqi, 0);
    w_SysFreeString(b);
}
TEST(vc_copy_to_self_invalid_vt) {
    VC_REQ();
    VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = 15;
    HR_EQ(w_VariantCopy(&v, &v), VC_BADVT);
    V_VT(&v) = VT_CLSID;
    HR_EQ(w_VariantCopy(&v, &v), VC_BADVT); /* VT_CLSID is accepted by Clear but not as a copy source */
}

/* ---- BSTR */
TEST(vc_copy_bstr_is_deep) {
    VC_REQ();
    VARIANT s, d; V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocString(L"deep copy"); w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR);
    CHECK(V_BSTR(&d) != NULL && V_BSTR(&d) != V_BSTR(&s));
    EXPECT_WSTR(V_BSTR(&d), L"deep copy");
    EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 9);
    V_BSTR(&s)[0] = L'X';
    EXPECT_WSTR(V_BSTR(&d), L"deep copy");
    HR_EQ(w_VariantClear(&s), S_OK);
    EXPECT_WSTR(V_BSTR(&d), L"deep copy"); /* destination survives the source being cleared */
    HR_EQ(w_VariantClear(&d), S_OK);
}
TEST(vc_copy_bstr_embedded_nul_and_length_preserved) {
    VC_REQ();
    static const OLECHAR src[] = { 'a', 0, 'b', 0, 'c' };
    VARIANT s, d; V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocStringLen(src, 5); w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 5);
    CHECK(!memcmp(V_BSTR(&d), src, sizeof src));
    EXPECT_EQ(V_BSTR(&d)[5], 0);
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_copy_bstr_odd_byte_length_preserved) {
    VC_REQ(); REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysStringByteLen));
    VARIANT s, d; V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocStringByteLen("abcde", 5); w_VariantInit(&d);
    EXPECT_EQ(w_SysStringByteLen(V_BSTR(&s)), 5);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(w_SysStringByteLen(V_BSTR(&d)), 5);
    CHECK(!memcmp(V_BSTR(&d), "abcde", 5));
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_copy_null_bstr_becomes_empty_bstr) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = VT_BSTR; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR);
    CHECK(V_BSTR(&d) != NULL);
    if (V_BSTR(&d)) { EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 0); EXPECT_EQ(V_BSTR(&d)[0], 0); }
    CHECK(V_BSTR(&s) == NULL);
    w_VariantClear(&d);
}
TEST(vc_copy_empty_bstr) {
    VC_REQ();
    VARIANT s, d; V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocString(L""); w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    CHECK(V_BSTR(&d) != NULL && V_BSTR(&d) != V_BSTR(&s));
    EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 0);
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_copy_bstr_lengths_table) {
    VC_REQ();
    static const UINT lens[] = { 0, 1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 33, 100, 1000, 65535, 65536 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        OLECHAR *buf = HeapAlloc(GetProcessHeap(), 0, (lens[i] + 1) * 2);
        for (UINT k = 0; k < lens[i]; k++) buf[k] = (OLECHAR)(0x21 + (k % 90));
        buf[lens[i]] = 0;
        VARIANT s, d; V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocStringLen(buf, lens[i]); w_VariantInit(&d);
        HR_EQ(w_VariantCopy(&d, &s), S_OK);
        CHECKF(w_SysStringLen(V_BSTR(&d)) == lens[i], "len %u", lens[i]);
        CHECKF(!memcmp(V_BSTR(&d), buf, lens[i] * 2 + 2), "content len %u", lens[i]);
        w_VariantClear(&s); w_VariantClear(&d);
        HeapFree(GetProcessHeap(), 0, buf);
    }
}

/* ---- IUnknown / IDispatch */
TEST(vc_copy_unknown_addrefs_once) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT s, d; V_VT(&s) = VT_UNKNOWN; V_UNKNOWN(&s) = (IUnknown *)&u; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_UNKNOWN); CHECK(V_UNKNOWN(&d) == (IUnknown *)&u);
    EXPECT_EQ(u.nadd, 1); EXPECT_EQ(u.nrel, 0); EXPECT_EQ(u.nqi, 0); EXPECT_EQ(u.ref, 2);
    HR_EQ(w_VariantClear(&d), S_OK); EXPECT_EQ(u.nrel, 1);
    CHECK(V_VT(&s) == VT_UNKNOWN);
}
TEST(vc_copy_dispatch_addrefs_once) {
    VC_REQ();
    MD m; md_init(&m);
    VARIANT s, d; V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_DISPATCH); CHECK(V_DISPATCH(&d) == (IDispatch *)&m);
    EXPECT_EQ(m.nadd, 1); EXPECT_EQ(m.nqi, 0); EXPECT_EQ(m.ninvoke, 0);
    w_VariantClear(&d); EXPECT_EQ(m.nrel, 1);
}
TEST(vc_copy_null_unknown) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = VT_UNKNOWN; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_UNKNOWN); CHECK(V_UNKNOWN(&d) == NULL);
    V_VT(&s) = VT_DISPATCH; HR_EQ(w_VariantCopy(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_DISPATCH);
}
TEST(vc_copy_destination_is_cleared_first_unknown) {
    VC_REQ();
    MU old, nu; mu_init(&old); mu_init(&nu);
    VARIANT s, d; V_VT(&s) = VT_UNKNOWN; V_UNKNOWN(&s) = (IUnknown *)&nu;
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = (IUnknown *)&old;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(old.nrel, 1); EXPECT_EQ(old.nadd, 0);
    EXPECT_EQ(nu.nadd, 1);
    CHECK(V_UNKNOWN(&d) == (IUnknown *)&nu);
    w_VariantClear(&d);
}
TEST(vc_copy_destination_bstr_replaced) {
    VC_REQ();
    VARIANT s, d; V_VT(&s) = VT_I4; V_I4(&s) = 5;
    V_VT(&d) = VT_BSTR; V_BSTR(&d) = w_SysAllocString(L"old");
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_I4); EXPECT_EQ(V_I4(&d), 5);
}
TEST(vc_copy_destination_byref_overwritten_without_release) {
    VC_REQ();
    MU u; mu_init(&u); IUnknown *pu = (IUnknown *)&u;
    VARIANT s, d; V_VT(&s) = VT_I2; V_I2(&s) = 3;
    V_VT(&d) = VT_UNKNOWN | VT_BYREF; V_UNKNOWNREF(&d) = &pu;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(u.nrel, 0);
    EXPECT_EQ(V_VT(&d), VT_I2);
}

/* ---- BYREF */
TEST(vc_copy_byref_shares_pointer) {
    VC_REQ();
    LONG i4 = 5; BSTR b = w_SysAllocString(L"by"); MU u; mu_init(&u); IUnknown *pu = (IUnknown *)&u;
    VARIANT s, d;
    V_VT(&s) = VT_I4 | VT_BYREF; V_I4REF(&s) = &i4; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_I4 | VT_BYREF); CHECK(V_I4REF(&d) == &i4);
    V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b;
    HR_EQ(w_VariantCopy(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_BSTR | VT_BYREF); CHECK(V_BSTRREF(&d) == &b); CHECK(*V_BSTRREF(&d) == b);
    V_VT(&s) = VT_UNKNOWN | VT_BYREF; V_UNKNOWNREF(&s) = &pu;
    HR_EQ(w_VariantCopy(&d, &s), S_OK); CHECK(V_UNKNOWNREF(&d) == &pu);
    EXPECT_EQ(u.nadd + u.nrel, 0);
    VARIANT inner; V_VT(&inner) = VT_I4; V_I4(&inner) = 8;
    V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &inner;
    HR_EQ(w_VariantCopy(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_VARIANT | VT_BYREF); CHECK(V_VARIANTREF(&d) == &inner);
    w_SysFreeString(b);
}
TEST(vc_copy_byref_array_shares_descriptor) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayDestroy));
    SAFEARRAYBOUND bd = { 2, 0 }; SAFEARRAY *sa = w_SafeArrayCreate(VT_I4, 1, &bd);
    VARIANT s, d; V_VT(&s) = VT_ARRAY | VT_BYREF | VT_I4; V_ARRAYREF(&s) = &sa; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_BYREF | VT_I4); CHECK(V_ARRAYREF(&d) == &sa);
    w_SafeArrayDestroy(sa);
}

/* ---- validity matrix (src validity checked before dest is touched) */
TEST(vc_copy_src_vt_validity_matrix) {
    VC_REQ();
    int bad = 0;
    for (unsigned extra = 0; extra < 16; extra++) {
        for (unsigned base = 0; base <= 80; base++) {
            unsigned vt = base | (extra << 12);
            VARIANT s, d; memset(&s, 0, sizeof s); w_VariantInit(&d);
            V_VT(&s) = (VARTYPE)vt;
            HRESULT hr = w_VariantCopy(&d, &s);
            int ok = vc_valid_vt(vt) && base != 72;
            HRESULT want = ok ? S_OK : VC_BADVT;
            if (hr != want) { bad++; CHECKF(0, "VariantCopy src vt=0x%04x got 0x%08lx want 0x%08lx", vt, (unsigned long)hr, (unsigned long)want); }
            else if (ok) { if (V_VT(&d) != vt) CHECKF(0, "src vt=0x%04x dest vt 0x%x", vt, V_VT(&d)); w_VariantClear(&d); }
            else { if (!vc_is_zero24(&d)) CHECKF(0, "src vt=0x%04x dest touched on failure", vt); }
        }
    }
    CHECK(bad == 0);
}
TEST(vc_copy_dest_vt_validity_matrix) {
    VC_REQ();
    int bad = 0;
    for (unsigned extra = 0; extra < 16; extra++) {
        for (unsigned base = 0; base <= 80; base++) {
            unsigned vt = base | (extra << 12);
            VARIANT s, d; V_VT(&s) = VT_UI1; V_UI1(&s) = 0x7b;
            memset(&d, 0, sizeof d); V_VT(&d) = (VARTYPE)vt;
            HRESULT hr = w_VariantCopy(&d, &s);
            HRESULT want = vc_valid_vt(vt) ? S_OK : VC_BADVT;
            if (hr != want) { bad++; CHECKF(0, "VariantCopy dest vt=0x%04x got 0x%08lx want 0x%08lx", vt, (unsigned long)hr, (unsigned long)want); }
            else if (hr == S_OK) { if (V_VT(&d) != VT_UI1 || V_UI1(&d) != 0x7b) CHECKF(0, "dest vt=0x%04x not replaced", vt); }
            else { if (V_VT(&d) != vt) CHECKF(0, "dest vt=0x%04x modified on failure", vt); }
        }
    }
    CHECK(bad == 0);
}
TEST(vc_copy_illegal_dest_leaves_dest_and_does_not_copy) {
    VC_REQ();
    VARIANT s, d; V_VT(&s) = VT_I4; V_I4(&s) = 123;
    memset(&d, 0, sizeof d); V_VT(&d) = VT_VOID;
    HR_EQ(w_VariantCopy(&d, &s), VC_BADVT);
    EXPECT_EQ(V_VT(&d), VT_VOID);
    EXPECT_EQ(V_I4(&d), 0);
}
TEST(vc_copy_illegal_src_with_populated_dest) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = 31;
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = (IUnknown *)&u;
    HR_EQ(w_VariantCopy(&d, &s), VC_BADVT);
    EXPECT_EQ(u.nrel, 0); /* src is validated first, dest not cleared */
    EXPECT_EQ(V_VT(&d), VT_UNKNOWN);
}

/* ---- SAFEARRAY */
TEST(vc_copy_array_deep) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayGetElement));
    SAFEARRAYBOUND bd = { 3, 5 }; SAFEARRAY *sa = w_SafeArrayCreate(VT_I4, 1, &bd);
    for (LONG i = 5; i < 8; i++) { LONG v = i * 10; HR_EQ(w_SafeArrayPutElement(sa, &i, &v), S_OK); }
    VARIANT s, d; V_VT(&s) = VT_ARRAY | VT_I4; V_ARRAY(&s) = sa; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_I4);
    CHECK(V_ARRAY(&d) != NULL && V_ARRAY(&d) != sa);
    LONG lb = -1, ub = -1;
    HR_EQ(w_SafeArrayGetLBound(V_ARRAY(&d), 1, &lb), S_OK); HR_EQ(w_SafeArrayGetUBound(V_ARRAY(&d), 1, &ub), S_OK);
    EXPECT_EQ(lb, 5); EXPECT_EQ(ub, 7);
    for (LONG i = 5; i < 8; i++) { LONG v = 0; HR_EQ(w_SafeArrayGetElement(V_ARRAY(&d), &i, &v), S_OK); EXPECT_EQ(v, i * 10); }
    LONG idx = 5, nv = 999; w_SafeArrayPutElement(sa, &idx, &nv);
    { LONG v = 0; w_SafeArrayGetElement(V_ARRAY(&d), &idx, &v); EXPECT_EQ(v, 50); }
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_copy_array_of_bstr_deep_copies_strings) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement)); REQUIRE(U(SafeArrayGetElement));
    SAFEARRAYBOUND bd = { 2, 0 }; SAFEARRAY *sa = w_SafeArrayCreate(VT_BSTR, 1, &bd);
    BSTR a = w_SysAllocString(L"first"), b = w_SysAllocString(L"second");
    LONG i0 = 0, i1 = 1; w_SafeArrayPutElement(sa, &i0, a); w_SafeArrayPutElement(sa, &i1, b);
    VARIANT s, d; V_VT(&s) = VT_ARRAY | VT_BSTR; V_ARRAY(&s) = sa; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    BSTR got = NULL; HR_EQ(w_SafeArrayGetElement(V_ARRAY(&d), &i1, &got), S_OK);
    EXPECT_WSTR(got, L"second"); w_SysFreeString(got);
    w_SysFreeString(a); w_SysFreeString(b);
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_copy_array_of_unknown_addrefs_elements) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayPutElement));
    MU u; mu_init(&u);
    SAFEARRAYBOUND bd = { 1, 0 }; SAFEARRAY *sa = w_SafeArrayCreate(VT_UNKNOWN, 1, &bd);
    LONG i0 = 0; w_SafeArrayPutElement(sa, &i0, &u);
    LONG base = u.ref;
    VARIANT s, d; V_VT(&s) = VT_ARRAY | VT_UNKNOWN; V_ARRAY(&s) = sa; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(u.ref, base + 1);
    w_VariantClear(&d); EXPECT_EQ(u.ref, base);
    w_VariantClear(&s); EXPECT_EQ(u.ref, 1);
}
TEST(vc_copy_null_array) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = VT_ARRAY | VT_I4; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_I4);
    CHECK(V_ARRAY(&d) == NULL);
}
TEST(vc_copy_multidim_array_keeps_shape) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayGetDim));
    SAFEARRAYBOUND bd[2] = { { 2, 1 }, { 3, -1 } }; SAFEARRAY *sa = w_SafeArrayCreate(VT_R8, 2, bd);
    VARIANT s, d; V_VT(&s) = VT_ARRAY | VT_R8; V_ARRAY(&s) = sa; w_VariantInit(&d);
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(w_SafeArrayGetDim(V_ARRAY(&d)), 2);
    LONG l, u2;
    w_SafeArrayGetLBound(V_ARRAY(&d), 1, &l); w_SafeArrayGetUBound(V_ARRAY(&d), 1, &u2); EXPECT_EQ(l, 1); EXPECT_EQ(u2, 2);
    w_SafeArrayGetLBound(V_ARRAY(&d), 2, &l); w_SafeArrayGetUBound(V_ARRAY(&d), 2, &u2); EXPECT_EQ(l, -1); EXPECT_EQ(u2, 1);
    w_VariantClear(&s); w_VariantClear(&d);
}

/* ---- VT_RECORD */
TEST(vc_copy_record_deep) {
    VC_REQ();
    MR r; mr_setup(&r, 16);
    char src[16]; for (int i = 0; i < 16; i++) src[i] = (char)(i + 1);
    VARIANT s, d; memset(&s, 0, sizeof s); w_VariantInit(&d);
    V_VT(&s) = VT_RECORD; V_RECORDINFO(&s) = (IRecordInfo *)&r; V_RECORD(&s) = src;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_RECORD);
    CHECK(V_RECORD(&d) != NULL && V_RECORD(&d) != (void *)src);
    CHECK(V_RECORDINFO(&d) == (IRecordInfo *)&r);
    EXPECT_EQ(r.nsize, 1); EXPECT_EQ(r.ncopy, 1);
    CHECK(r.last_copy_src == (void *)src); CHECK(r.last_copy_dst == V_RECORD(&d));
    EXPECT_EQ(r.nadd, 1); /* destination holds its own reference to the record info */
    CHECK(!memcmp(V_RECORD(&d), src, 16));
    HR_EQ(w_VariantClear(&d), S_OK);
    EXPECT_EQ(r.nclear, 1); EXPECT_EQ(r.nrel, 1);
    EXPECT_EQ(r.ref, 1);
}
TEST(vc_copy_record_null_data_addrefs_recinfo) {
    VC_REQ();
    if (t_is_wine()) { t_skip("wine dereferences the NULL record"); return; }
    MR r; mr_setup(&r, 16);
    VARIANT s, d; memset(&s, 0, sizeof s); w_VariantInit(&d);
    V_VT(&s) = VT_RECORD; V_RECORDINFO(&s) = (IRecordInfo *)&r;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_RECORD); CHECK(V_RECORD(&d) == NULL); CHECK(V_RECORDINFO(&d) == (IRecordInfo *)&r);
    EXPECT_EQ(r.ncopy, 0); EXPECT_EQ(r.nadd, 1);
    w_VariantClear(&d);
    EXPECT_EQ(r.nrel, 1); EXPECT_EQ(r.nclear, 0);
}
TEST(vc_copy_record_without_recinfo) {
    VC_REQ();
    char buf[8] = {0};
    VARIANT s, d; memset(&s, 0, sizeof s); w_VariantInit(&d);
    V_VT(&s) = VT_RECORD; V_RECORD(&s) = buf; V_RECORDINFO(&s) = NULL;
    HR_EQ(w_VariantCopy(&d, &s), VC_INVALID);   /* data without a record info cannot be sized */
    V_RECORD(&s) = NULL;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_RECORD);
}
TEST(vc_copy_record_byref_shares_data_no_addref) {
    VC_REQ();
    MR r; mr_setup(&r, 16);
    char buf[16] = {0};
    VARIANT s, d; memset(&s, 0, sizeof s); w_VariantInit(&d);
    V_VT(&s) = VT_RECORD | VT_BYREF; V_RECORDINFO(&s) = (IRecordInfo *)&r; V_RECORD(&s) = buf;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_RECORD | VT_BYREF); CHECK(V_RECORD(&d) == (void *)buf); CHECK(V_RECORDINFO(&d) == (IRecordInfo *)&r);
    EXPECT_EQ(r.nadd + r.nsize + r.ncopy, 0);
    HR_EQ(w_VariantClear(&d), S_OK);
    EXPECT_EQ(r.nrel + r.nclear, 0);
}
TEST(vc_copy_over_record_destination_clears_it) {
    VC_REQ();
    MR r; mr_setup(&r, 16); r.ref = 2;
    void *rec = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 16);
    VARIANT s, d; V_VT(&s) = VT_I4; V_I4(&s) = 1; memset(&d, 0, sizeof d);
    V_VT(&d) = VT_RECORD; V_RECORDINFO(&d) = (IRecordInfo *)&r; V_RECORD(&d) = rec;
    HR_EQ(w_VariantCopy(&d, &s), S_OK);
    EXPECT_EQ(r.nclear, 1); EXPECT_EQ(r.nrel, 1);
    EXPECT_EQ(V_VT(&d), VT_I4);
    if (t_is_wine()) HeapFree(GetProcessHeap(), 0, rec);
}

/* ================================================================ VariantCopyInd */
TEST(vc_copyind_non_byref_acts_as_copy) {
    VC_REQ();
    VARIANT s, d; V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocString(L"plain"); w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR); CHECK(V_BSTR(&d) != V_BSTR(&s)); EXPECT_WSTR(V_BSTR(&d), L"plain");
    w_VariantClear(&s); w_VariantClear(&d);
    VARIANT x; V_VT(&x) = 15; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &x), VC_BADVT);
}
TEST(vc_copyind_scalar_deref_table) {
    VC_REQ();
    union { SHORT i2; LONG i4; float r4; double r8; CY cy; DATE dt; VARIANT_BOOL b; SCODE sc; signed char i1; BYTE u1; USHORT u2; ULONG u4; LONGLONG i8; ULONGLONG u8; int in; unsigned ui; } u;
    VARIANT s, d;
#define CASE(vtv, member, val, field) \
    memset(&u, 0, sizeof u); u.member = val; V_VT(&s) = (vtv) | VT_BYREF; V_BYREF(&s) = &u; w_VariantInit(&d); \
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), vtv); CHECK(field(&d) == val);
    CASE(VT_I2, i2, -1234, V_I2)
    CASE(VT_I4, i4, -123456, V_I4)
    CASE(VT_R4, r4, 1.5f, V_R4)
    CASE(VT_R8, r8, -2.25, V_R8)
    CASE(VT_DATE, dt, 36526.5, V_DATE)
    CASE(VT_BOOL, b, VARIANT_TRUE, V_BOOL)
    CASE(VT_ERROR, sc, 0x80020004, V_ERROR)
    CASE(VT_I1, i1, -5, V_I1)
    CASE(VT_UI1, u1, 0xfe, V_UI1)
    CASE(VT_UI2, u2, 0xfedc, V_UI2)
    CASE(VT_UI4, u4, 0xfedcba98u, V_UI4)
    CASE(VT_I8, i8, -0x123456789abcLL, V_I8)
    CASE(VT_UI8, u8, 0xfedcba9876543210ULL, V_UI8)
    CASE(VT_INT, in, -77, V_INT)
    CASE(VT_UINT, ui, 0xabcdef01u, V_UINT)
#undef CASE
    memset(&u, 0, sizeof u); u.cy.int64 = 123456789; V_VT(&s) = VT_CY | VT_BYREF; V_BYREF(&s) = &u; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_CY); CHECK(V_CY(&d).int64 == 123456789);
}
TEST(vc_copyind_decimal) {
    VC_REQ();
    DECIMAL dec; memset(&dec, 0, sizeof dec); dec.scale = 2; dec.sign = DECIMAL_NEG; dec.Hi32 = 1; dec.Lo64 = 0x1122334455667788ULL;
    VARIANT s, d; V_VT(&s) = VT_DECIMAL | VT_BYREF; V_BYREF(&s) = &dec; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_DECIMAL);
    EXPECT_EQ(V_DECIMAL(&d).scale, 2); EXPECT_EQ(V_DECIMAL(&d).sign, DECIMAL_NEG);
    EXPECT_EQ(V_DECIMAL(&d).Hi32, 1); CHECK(V_DECIMAL(&d).Lo64 == 0x1122334455667788ULL);
}
TEST(vc_copyind_bstr_deep_copies) {
    VC_REQ();
    BSTR b = w_SysAllocString(L"byref string");
    VARIANT s, d; V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR);
    CHECK(V_BSTR(&d) != b); EXPECT_WSTR(V_BSTR(&d), L"byref string");
    w_VariantClear(&d); EXPECT_WSTR(b, L"byref string");
    w_SysFreeString(b);
}
TEST(vc_copyind_null_bstr_becomes_empty) {
    VC_REQ();
    BSTR b = NULL;
    VARIANT s, d; V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR);
    CHECK(V_BSTR(&d) != NULL);
    if (V_BSTR(&d)) EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 0);
    w_VariantClear(&d);
}
TEST(vc_copyind_bstr_preserves_byte_length) {
    VC_REQ(); REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysStringByteLen));
    BSTR b = w_SysAllocStringByteLen("xyz", 3);
    VARIANT s, d; V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(w_SysStringByteLen(V_BSTR(&d)), 3);
    w_VariantClear(&d); w_SysFreeString(b);
}
TEST(vc_copyind_unknown_dispatch_addref) {
    VC_REQ();
    MU u; mu_init(&u); MD m; md_init(&m);
    IUnknown *pu = (IUnknown *)&u; IDispatch *pd = (IDispatch *)&m;
    VARIANT s, d; w_VariantInit(&d);
    V_VT(&s) = VT_UNKNOWN | VT_BYREF; V_UNKNOWNREF(&s) = &pu;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_UNKNOWN); CHECK(V_UNKNOWN(&d) == pu);
    EXPECT_EQ(u.nadd, 1); EXPECT_EQ(u.nqi, 0);
    w_VariantClear(&d); EXPECT_EQ(u.nrel, 1);
    V_VT(&s) = VT_DISPATCH | VT_BYREF; V_DISPATCHREF(&s) = &pd;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK); EXPECT_EQ(V_VT(&d), VT_DISPATCH); CHECK(V_DISPATCH(&d) == pd);
    EXPECT_EQ(m.nadd, 1);
    w_VariantClear(&d); EXPECT_EQ(m.nrel, 1);
}
TEST(vc_copyind_null_unknown_inside) {
    VC_REQ();
    IUnknown *pu = NULL;
    VARIANT s, d; V_VT(&s) = VT_UNKNOWN | VT_BYREF; V_UNKNOWNREF(&s) = &pu; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_UNKNOWN); CHECK(V_UNKNOWN(&d) == NULL);
}
TEST(vc_copyind_variant_byref_derefs) {
    VC_REQ();
    VARIANT inner, s, d; V_VT(&inner) = VT_UI1; V_UI1(&inner) = 0x77; w_VariantInit(&d);
    V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &inner;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_UI1); EXPECT_EQ(V_UI1(&d), 0x77);
    /* inner variant is itself a byref (non-variant): fully dereferenced */
    BYTE buf = 0x88; V_VT(&inner) = VT_UI1 | VT_BYREF; V_UI1REF(&inner) = &buf;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_UI1); EXPECT_EQ(V_UI1(&d), 0x88);
}
TEST(vc_copyind_variant_byref_to_variant_byref_fails) {
    VC_REQ();
    VARIANT i2, i1, s, d; V_VT(&i2) = VT_UI1; V_UI1(&i2) = 1;
    V_VT(&i1) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&i1) = &i2;
    V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &i1;
    V_VT(&d) = VT_I4; V_I4(&d) = 4;
    HR_EQ(w_VariantCopyInd(&d, &s), VC_INVALID);
    EXPECT_EQ(V_VT(&d), VT_EMPTY); /* dest was cleared before the error */
}
TEST(vc_copyind_variant_byref_holding_bstr_deep_copies) {
    VC_REQ();
    VARIANT inner, s, d; V_VT(&inner) = VT_BSTR; V_BSTR(&inner) = w_SysAllocString(L"nested"); w_VariantInit(&d);
    V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &inner;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR); CHECK(V_BSTR(&d) != V_BSTR(&inner)); EXPECT_WSTR(V_BSTR(&d), L"nested");
    w_VariantClear(&d); w_VariantClear(&inner);
}
TEST(vc_copyind_variant_byref_holding_array) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate));
    SAFEARRAYBOUND bd = { 2, 0 }; SAFEARRAY *sa = w_SafeArrayCreate(VT_I4, 1, &bd);
    VARIANT inner, s, d; V_VT(&inner) = VT_ARRAY | VT_I4; V_ARRAY(&inner) = sa; w_VariantInit(&d);
    V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &inner;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_I4); CHECK(V_ARRAY(&d) != sa && V_ARRAY(&d) != NULL);
    w_VariantClear(&d); w_VariantClear(&inner);
}
TEST(vc_copyind_byref_array_copies_array) {
    VC_REQ(); REQUIRE(U(SafeArrayCreate));
    SAFEARRAYBOUND bd = { 4, 0 }; SAFEARRAY *sa = w_SafeArrayCreate(VT_I2, 1, &bd);
    VARIANT s, d; V_VT(&s) = VT_ARRAY | VT_BYREF | VT_I2; V_ARRAYREF(&s) = &sa; w_VariantInit(&d);
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_I2);
    CHECK(V_ARRAY(&d) != NULL && V_ARRAY(&d) != sa);
    w_VariantClear(&d); w_VariantClear(&s); /* byref: does not destroy sa */
    HR_EQ(w_SafeArrayDestroy(sa), S_OK);
}
TEST(vc_copyind_in_place) {
    VC_REQ();
    LONG i4 = 424242; VARIANT v;
    V_VT(&v) = VT_I4 | VT_BYREF; V_I4REF(&v) = &i4;
    HR_EQ(w_VariantCopyInd(&v, &v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_I4); EXPECT_EQ(V_I4(&v), 424242);
    BSTR b = w_SysAllocString(L"inplace");
    V_VT(&v) = VT_BSTR | VT_BYREF; V_BSTRREF(&v) = &b;
    HR_EQ(w_VariantCopyInd(&v, &v), S_OK);
    EXPECT_EQ(V_VT(&v), VT_BSTR); EXPECT_WSTR(V_BSTR(&v), L"inplace"); CHECK(V_BSTR(&v) != b);
    w_VariantClear(&v); w_SysFreeString(b);
}
TEST(vc_copyind_destination_cleared_first) {
    VC_REQ();
    MU old; mu_init(&old); LONG i4 = 4;
    VARIANT s, d; V_VT(&s) = VT_I4 | VT_BYREF; V_I4REF(&s) = &i4;
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = (IUnknown *)&old;
    HR_EQ(w_VariantCopyInd(&d, &s), S_OK);
    EXPECT_EQ(old.nrel, 1); EXPECT_EQ(V_VT(&d), VT_I4); EXPECT_EQ(V_I4(&d), 4);
    /* invalid destination vt -> DISP_E_BADVARTYPE, dest unchanged */
    memset(&d, 0, sizeof d); V_VT(&d) = 24;
    HR_EQ(w_VariantCopyInd(&d, &s), VC_BADVT); EXPECT_EQ(V_VT(&d), 24);
}
/* byref of an unsupported base type: E_INVALIDARG (dest is already cleared) */
TEST(vc_copyind_byref_unsupported_base_types) {
    VC_REQ();
    char buf[64] = {0};
    static const unsigned bases[] = { VT_EMPTY, VT_NULL, 15, VT_VOID, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_CARRAY, VT_USERDEFINED, VT_LPSTR, VT_LPWSTR };
    for (unsigned i = 0; i < sizeof bases / sizeof *bases; i++) {
        VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = (VARTYPE)(bases[i] | VT_BYREF); V_BYREF(&s) = buf;
        V_VT(&d) = VT_I4; V_I4(&d) = 1;
        HRESULT hr = w_VariantCopyInd(&d, &s);
        CHECKF(hr == VC_INVALID, "byref base %u hr 0x%08lx", bases[i], (unsigned long)hr);
        EXPECT_EQ(V_VT(&d), VT_EMPTY);
    }
}
TEST(vc_copyind_byref_vt_matrix_valid_bases) {
    VC_REQ();
    char buf[64]; memset(buf, 0, sizeof buf);
    for (unsigned base = 2; base <= 23; base++) {
        if (base == 15 || base == VT_BSTR || base == VT_UNKNOWN || base == VT_DISPATCH) continue; /* need valid object pointers */
        VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = (VARTYPE)(base | VT_BYREF); V_BYREF(&s) = buf; w_VariantInit(&d);
        HRESULT hr = w_VariantCopyInd(&d, &s);
        CHECKF(hr == S_OK, "byref base %u hr 0x%08lx", base, (unsigned long)hr);
        if (hr == S_OK) {
            if (base == VT_VARIANT) EXPECT_EQ(V_VT(&d), VT_EMPTY); else EXPECT_EQ(V_VT(&d), base);
        }
    }
}
TEST(vc_copyind_byref_record_clones_data) {
    VC_REQ();
    MR r; mr_setup(&r, 16);
    char src[16]; for (int i = 0; i < 16; i++) src[i] = (char)(0x10 + i);
    VARIANT s, d; memset(&s, 0, sizeof s); w_VariantInit(&d);
    V_VT(&s) = VT_RECORD | VT_BYREF; V_RECORDINFO(&s) = (IRecordInfo *)&r; V_RECORD(&s) = src;
    HRESULT hr = w_VariantCopyInd(&d, &s);
    HR_EQ(hr, S_OK);
    if (hr == S_OK) {
        EXPECT_EQ(V_VT(&d), VT_RECORD);
        CHECK(V_RECORD(&d) != (void *)src && V_RECORD(&d) != NULL);
        CHECK(!memcmp(V_RECORD(&d), src, 16));
        EXPECT_EQ(r.ncopy, 1); EXPECT_EQ(r.nadd, 1);
        w_VariantClear(&d);
    }
}
TEST(vc_copyind_byref_flag_combinations_with_vector_reserved) {
    VC_REQ();
    char buf[64] = {0};
    static const unsigned extras[] = { VT_VECTOR | VT_BYREF, VT_RESERVED | VT_BYREF, VT_VECTOR | VT_BYREF | VT_RESERVED };
    for (unsigned i = 0; i < sizeof extras / sizeof *extras; i++) {
        VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = (VARTYPE)(VT_I4 | extras[i]); V_BYREF(&s) = buf; w_VariantInit(&d);
        HRESULT hr = w_VariantCopyInd(&d, &s);
        CHECKF(hr == VC_INVALID, "vt 0x%x hr 0x%08lx", V_VT(&s), (unsigned long)hr);
    }
}
