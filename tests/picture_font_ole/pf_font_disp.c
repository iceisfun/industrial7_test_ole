/* StdFont as an IDispatch (IFontDisp) object, its connection points (IFontEventsDisp, IPropertyNotifySink),
 * IPersistStream / IPersistPropertyBag persistence and PF_CLSID_StdFont activation. */
#include "pf.h"

#define FONT_BEGIN(f) IFont *f; REQUIRE(U(OleCreateFontIndirect)); \
    f = pf_font_new(); if (!f) { CHECKF(0, "font creation failed"); return; }

typedef struct { DISPID id; const WCHAR *name; VARTYPE vt; } fprop_t;
static const fprop_t fprops[] = {
    { DISPID_FONT_NAME, L"Name", VT_BSTR }, { DISPID_FONT_SIZE, L"Size", VT_CY },
    { DISPID_FONT_BOLD, L"Bold", VT_BOOL }, { DISPID_FONT_ITALIC, L"Italic", VT_BOOL },
    { DISPID_FONT_UNDER, L"Underline", VT_BOOL }, { DISPID_FONT_STRIKE, L"Strikethrough", VT_BOOL },
    { DISPID_FONT_WEIGHT, L"Weight", VT_I2 }, { DISPID_FONT_CHARSET, L"Charset", VT_I2 },
};

static IFontDisp *disp_of(IFont *f) {
    IFontDisp *d = NULL; IFont_QueryInterface(f, &IID_IFontDisp, (void **)&d); return d;
}

/* ---- DISPID constants ---------------------------------------------------------------------- */
TEST(pfontdisp_dispid_constants) {
    EQ(DISPID_FONT_NAME, 0); EQ(DISPID_FONT_SIZE, 2); EQ(DISPID_FONT_BOLD, 3); EQ(DISPID_FONT_ITALIC, 4);
    EQ(DISPID_FONT_UNDER, 5); EQ(DISPID_FONT_STRIKE, 6); EQ(DISPID_FONT_WEIGHT, 7); EQ(DISPID_FONT_CHARSET, 8);
    EQ(DISPID_FONT_CHANGED, 9);
}

/* ---- type info ------------------------------------------------------------------------------ */
TEST(pfontdisp_typeinfo_count) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); UINT n = 99;
    CHECK(d != NULL);
    if (d) { EQ(IFontDisp_GetTypeInfoCount(d, &n), S_OK); EQ(n, 1); IFontDisp_Release(d); }
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_typeinfo_names) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); ITypeInfo *ti = NULL; int i;
    LCID en = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    if (!d) { CHECK(d); IFont_Release(f); return; }
    EQ(IFontDisp_GetTypeInfo(d, 0, en, &ti), S_OK);
    CHECK(ti != NULL);
    if (ti) {
        for (i = 0; i < PF_N(fprops); i++) {
            BSTR names[3] = { 0 }; UINT n = 0;
            HRESULT hr = ITypeInfo_GetNames(ti, fprops[i].id, names, 3, &n);
            CHECKF(hr == S_OK, "GetNames(%ld) hr=%08lx", fprops[i].id, hr);
            CHECKF(n >= 1, "GetNames(%ld) n=%u", fprops[i].id, n);
            if (hr == S_OK && n >= 1) CHECKF(!lstrcmpiW(names[0], fprops[i].name), "dispid %ld name %ls", fprops[i].id, names[0]);
            { UINT k; for (k = 0; k < n && hr == S_OK; k++) SysFreeString(names[k]); }
        }
        ITypeInfo_Release(ti);
    }
    IFontDisp_Release(d);
    IFont_Release(f);
}
TEST(pfontdisp_typeinfo_attr) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    EQ(IFontDisp_GetTypeInfo(d, 0, 0, &ti), S_OK);
    if (ti && ITypeInfo_GetTypeAttr(ti, &ta) == S_OK) {
        t_note("typeinfo kind=%d guid=%08lx cFuncs=%u", ta->typekind, ta->guid.Data1, ta->cFuncs);
        CHECK(ta->cFuncs + ta->cVars >= 8);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    if (ti) ITypeInfo_Release(ti);
    IFontDisp_Release(d);
    IFont_Release(f);
}
TEST(pfontdisp_typeinfo_bad_index) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); ITypeInfo *ti = (ITypeInfo *)(ULONG_PTR)1; HRESULT hr;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    hr = IFontDisp_GetTypeInfo(d, 1, 0, &ti);
    SOFT_EQ(hr, PF_DISP_E_BADINDEX);
    if (SUCCEEDED(hr) && ti) ITypeInfo_Release(ti);
    IFontDisp_Release(d);
    IFont_Release(f);
}

/* ---- GetIDsOfNames -------------------------------------------------------------------------- */
static HRESULT ids(IFontDisp *d, const WCHAR **names, UINT n, LCID lcid, DISPID *out) {
    return IFontDisp_GetIDsOfNames(d, &IID_NULL, (LPOLESTR *)names, n, lcid, out);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_getidsofnames_table) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); int i;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    for (i = 0; i < PF_N(fprops); i++) {
        const WCHAR *nm[1]; DISPID id = 0x1234;
        nm[0] = fprops[i].name;
        EQ(ids(d, nm, 1, 0x409, &id), S_OK);
        CHECKF(id == fprops[i].id, "%ls -> %ld", fprops[i].name, id);
    }
    IFontDisp_Release(d); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_getidsofnames_case_insensitive) {
    static const WCHAR *spell[] = { L"nAmE", L"SIZE", L"bold", L"ITALIC", L"underLINE", L"strikeTHROUGH", L"wEiGhT", L"cHaRsEt" };
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); int i;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    for (i = 0; i < PF_N(fprops); i++) {
        DISPID id = 0x1234;
        EQ(ids(d, &spell[i], 1, 0x409, &id), S_OK);
        CHECKF(id == fprops[i].id, "%ls -> %ld", spell[i], id);
    }
    IFontDisp_Release(d); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_getidsofnames_unknown) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); const WCHAR *nm[1] = { L"Foo" }; DISPID id = 0x1234;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    EQ(ids(d, nm, 1, 0x409, &id), PF_DISP_E_UNKNOWNNAME);
    EQ(id, DISPID_UNKNOWN);
    IFontDisp_Release(d); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_getidsofnames_multiple_names) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); DISPID id[2] = { 0x1234, 0x1234 };
    const WCHAR *a[2] = { L"Italic", L"Name" }, *b[2] = { L"Italic", L"Foo" };
    if (!d) { CHECK(d); IFont_Release(f); return; }
    EQ(ids(d, a, 2, 0, id), S_OK);
    EQ(id[0], DISPID_FONT_ITALIC); EQ(id[1], DISPID_UNKNOWN);
    id[0] = id[1] = 0x1234;
    EQ(ids(d, b, 2, 0, id), S_OK);
    EQ(id[0], DISPID_FONT_ITALIC); EQ(id[1], DISPID_UNKNOWN);
    IFontDisp_Release(d); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_getidsofnames_zero_names) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); const WCHAR *nm[1] = { L"Name" }; DISPID id = (DISPID)0xdeadbeef;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    EQ(ids(d, nm, 0, 0x409, &id), PF_E_INVALIDARG);
    EQ(id, (DISPID)0xdeadbeef);
    IFontDisp_Release(d); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_getidsofnames_lcid_ignored) {
    static const LCID lcids[] = { 0, 0x409, 0x40c, (LCID)-1, LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT };
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); int i;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    for (i = 0; i < PF_N(lcids); i++) {
        const WCHAR *nm[1] = { L"Name" }, *nom[1] = { L"Nom" }; DISPID id = 0x1234;
        EQ(ids(d, nm, 1, lcids[i], &id), S_OK); EQ(id, DISPID_FONT_NAME);
        EQ(ids(d, nom, 1, lcids[i], &id), PF_DISP_E_UNKNOWNNAME);
    }
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_getidsofnames_riid_must_be_null) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); const WCHAR *nm[1] = { L"Name" }; DISPID id = 0x1234; HRESULT hr;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    hr = IFontDisp_GetIDsOfNames(d, &IID_IFontDisp, (LPOLESTR *)nm, 1, 0, &id);
    t_note("GetIDsOfNames with non-NULL riid hr=%08lx", hr);
    IFontDisp_Release(d); IFont_Release(f);
}

/* ---- Invoke --------------------------------------------------------------------------------- */
static HRESULT inv_get(IFontDisp *d, DISPID id, VARIANT *out) {
    DISPPARAMS dp = { NULL, NULL, 0, 0 };
    VariantInit(out);
    return IFontDisp_Invoke(d, id, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, out, NULL, NULL);
}
static HRESULT inv_put(IFontDisp *d, DISPID id, VARIANT *v, int with_named) {
    DISPID named = DISPID_PROPERTYPUT;
    DISPPARAMS dp; dp.cArgs = 1; dp.rgvarg = v;
    dp.cNamedArgs = with_named ? 1 : 0; dp.rgdispidNamedArgs = with_named ? &named : NULL;
    return IFontDisp_Invoke(d, id, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
}
TEST(pfontdisp_invoke_get_types) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); int i;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    for (i = 0; i < PF_N(fprops); i++) {
        VARIANT v; HRESULT hr = inv_get(d, fprops[i].id, &v);
        CHECKF(hr == S_OK, "get %ls hr=%08lx", fprops[i].name, hr);
        if (hr == S_OK) CHECKF(V_VT(&v) == fprops[i].vt, "%ls vt=%u want %u", fprops[i].name, V_VT(&v), fprops[i].vt);
        VariantClear(&v);
    }
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_invoke_get_matches_ifont) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); VARIANT v; BSTR n = NULL; CY sz; SHORT w, cs;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    EQ(IFont_put_Weight(f, 300), S_OK); EQ(IFont_put_Charset(f, 2), S_OK);
    sz.int64 = 345600; EQ(IFont_put_Size(f, sz), S_OK);
    EQ(inv_get(d, DISPID_FONT_NAME, &v), S_OK);
    EQ(IFont_get_Name(f, &n), S_OK);
    if (V_VT(&v) == VT_BSTR && n) CHECK(!lstrcmpW(V_BSTR(&v), n));
    SysFreeString(n); VariantClear(&v);
    EQ(inv_get(d, DISPID_FONT_WEIGHT, &v), S_OK); w = V_VT(&v) == VT_I2 ? V_I2(&v) : -1; EQ(w, pf_snap_weight(300)); VariantClear(&v);
    EQ(inv_get(d, DISPID_FONT_CHARSET, &v), S_OK); cs = V_VT(&v) == VT_I2 ? V_I2(&v) : -1; EQ(cs, 2); VariantClear(&v);
    EQ(inv_get(d, DISPID_FONT_SIZE, &v), S_OK);
    if (V_VT(&v) == VT_CY) CHECK(V_CY(&v).int64 - 345600 < 10000 && V_CY(&v).int64 - 345600 > -10000);
    VariantClear(&v);
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_invoke_put_roundtrip) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); VARIANT v; BOOL b; SHORT s; BSTR n = NULL; CY sz;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"Courier New");
    EQ(inv_put(d, DISPID_FONT_NAME, &v, 0), S_OK); VariantClear(&v);
    EQ(IFont_get_Name(f, &n), S_OK); CHECK(n && !lstrcmpW(n, L"Courier New")); SysFreeString(n);
    V_VT(&v) = VT_CY; V_CY(&v).int64 = 77000;
    EQ(inv_put(d, DISPID_FONT_SIZE, &v, 0), S_OK);
    EQ(IFont_get_Size(f, &sz), S_OK); CHECK(sz.int64 - 77000 < 10000 && sz.int64 - 77000 > -10000);
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    EQ(inv_put(d, DISPID_FONT_BOLD, &v, 0), S_OK);
    EQ(IFont_get_Bold(f, &b), S_OK); CHECK(b != 0);
    EQ(inv_put(d, DISPID_FONT_ITALIC, &v, 0), S_OK); EQ(IFont_get_Italic(f, &b), S_OK); CHECK(b != 0);
    EQ(inv_put(d, DISPID_FONT_UNDER, &v, 0), S_OK); EQ(IFont_get_Underline(f, &b), S_OK); CHECK(b != 0);
    EQ(inv_put(d, DISPID_FONT_STRIKE, &v, 0), S_OK); EQ(IFont_get_Strikethrough(f, &b), S_OK); CHECK(b != 0);
    V_BOOL(&v) = VARIANT_FALSE;
    EQ(inv_put(d, DISPID_FONT_BOLD, &v, 0), S_OK); EQ(IFont_get_Bold(f, &b), S_OK); EQ(b, FALSE);
    V_VT(&v) = VT_I2; V_I2(&v) = 900;
    EQ(inv_put(d, DISPID_FONT_WEIGHT, &v, 0), S_OK); EQ(IFont_get_Weight(f, &s), S_OK); EQ(s, pf_snap_weight(900));
    V_I2(&v) = 5;
    EQ(inv_put(d, DISPID_FONT_CHARSET, &v, 0), S_OK); EQ(IFont_get_Charset(f, &s), S_OK); SOFT_EQ(s, 5);
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_invoke_put_with_named_arg) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); VARIANT v; BOOL b = 0; HRESULT hr;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    hr = inv_put(d, DISPID_FONT_ITALIC, &v, 1);
    SOFT_EQ(hr, S_OK);
    if (hr == S_OK) { EQ(IFont_get_Italic(f, &b), S_OK); CHECK(b != 0); }
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_invoke_put_coerces_types) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); VARIANT v; SHORT s = 0; HRESULT hr;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    V_VT(&v) = VT_I4; V_I4(&v) = 600;
    hr = inv_put(d, DISPID_FONT_WEIGHT, &v, 0);
    t_note("put Weight via VT_I4 hr=%08lx", hr);
    if (hr == S_OK) { EQ(IFont_get_Weight(f, &s), S_OK); EQ(s, pf_snap_weight(600)); }
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"abc");
    hr = inv_put(d, DISPID_FONT_WEIGHT, &v, 0);
    CHECKF(FAILED(hr), "uncoercible string accepted as weight: %08lx", hr);
    SOFT_EQ(hr, PF_DISP_E_TYPEMISMATCH);
    VariantClear(&v);
    IFontDisp_Release(d); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontdisp_invoke_error_codes) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); VARIANTARG arg; VARIANT res; DISPPARAMS dp; HRESULT hr;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    V_VT(&arg) = VT_BOOL; V_BOOL(&arg) = VARIANT_FALSE;
    dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL; dp.cArgs = 1; dp.rgvarg = &arg;
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_IFontDisp, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_UNKNOWNINTERFACE);
    dp.cArgs = 0; dp.rgvarg = NULL;
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_BADPARAMCOUNT);
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_NULL, 0, DISPATCH_PROPERTYPUT, NULL, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_PARAMNOTOPTIONAL);
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_NULL, 0, DISPATCH_PROPERTYGET, NULL, NULL, NULL, NULL);
    EQ(hr, PF_DISP_E_PARAMNOTOPTIONAL);
    VariantInit(&res);
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_NULL, 0, DISPATCH_PROPERTYGET, NULL, &res, NULL, NULL);
    EQ(hr, S_OK);
    VariantClear(&res);
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_NULL, 0, DISPATCH_METHOD, NULL, &res, NULL, NULL);
    EQ(hr, PF_DISP_E_MEMBERNOTFOUND);
    hr = IFontDisp_Invoke(d, (DISPID)0xdeadbeef, &IID_NULL, 0, DISPATCH_PROPERTYGET, NULL, &res, NULL, NULL);
    EQ(hr, PF_DISP_E_MEMBERNOTFOUND);
    dp.cArgs = 1; dp.rgvarg = &arg;
    VariantInit(&res);
    hr = IFontDisp_Invoke(d, DISPID_FONT_BOLD, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
    EQ(hr, S_OK);
    VariantClear(&res);
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_invoke_unknown_dispids) {
    static const DISPID bad[] = { 1, 10, 11, 100, -1, -2, DISPID_VALUE + 1, (DISPID)0x80000000 };
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); int i; VARIANT res; DISPPARAMS dp = { NULL, NULL, 0, 0 };
    if (!d) { CHECK(d); IFont_Release(f); return; }
    for (i = 0; i < PF_N(bad); i++) {
        HRESULT hr;
        VariantInit(&res);
        hr = IFontDisp_Invoke(d, bad[i], &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
        /* DISPID 1 and 10.. are not Font members; the default-member lookup (DISPID 0) is Name */
        CHECKF(hr == PF_DISP_E_MEMBERNOTFOUND, "dispid %ld hr=%08lx", bad[i], hr);
        VariantClear(&res);
    }
    IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontdisp_invoke_default_member_is_name) {
    FONT_BEGIN(f);
    IFontDisp *d = disp_of(f); VARIANT v; HRESULT hr;
    if (!d) { CHECK(d); IFont_Release(f); return; }
    hr = inv_get(d, DISPID_VALUE, &v);   /* DISPID_VALUE == 0 == DISPID_FONT_NAME */
    EQ(hr, S_OK); EQ(V_VT(&v), VT_BSTR);
    VariantClear(&v);
    IFontDisp_Release(d); IFont_Release(f);
}

/* ---- connection points ---------------------------------------------------------------------- */
static IConnectionPoint *find_cp(IFont *f, const GUID *iid, HRESULT *phr) {
    IConnectionPointContainer *c = NULL; IConnectionPoint *cp = NULL; HRESULT hr;
    hr = IFont_QueryInterface(f, &IID_IConnectionPointContainer, (void **)&c);
    if (hr == S_OK) { hr = IConnectionPointContainer_FindConnectionPoint(c, iid, &cp); IConnectionPointContainer_Release(c); }
    if (phr) *phr = hr;
    return cp;
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontevents_changed_via_ifont) {
    FONT_BEGIN(f);
    HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); pf_sink_t s; DWORD ck = 0;
    EQ(hr, S_OK); CHECK(cp != NULL);
    if (!cp) { IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    CHECK(ck != 0);
    EQ(IFont_put_Bold(f, TRUE), S_OK);
    EQ(s.calls, 1);
    EQ(s.last_dispid, DISPID_FONT_CHANGED);
    EQ(s.last_flags, DISPATCH_METHOD);
    EQ(s.last_cargs, 1);
    EQ(s.last_vt, VT_BSTR);
    EQ(s.bad_riid, 0);
    CHECK(!lstrcmpW(s.last_name, L"Bold"));
    EQ(IConnectionPoint_Unadvise(cp, ck), S_OK);
    EQ(s.ref, 1);
    IConnectionPoint_Release(cp);
    IFont_Release(f);
}
TEST(pfontevents_each_property_notifies_name) {
    static const struct { int which; const WCHAR *name; } t[] = {
        { 0, L"Name" }, { 1, L"Size" }, { 2, L"Bold" }, { 3, L"Italic" }, { 4, L"Underline" },
        { 5, L"Strikethrough" }, { 6, L"Weight" }, { 7, L"Charset" },
    };
    FONT_BEGIN(f);
    HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); pf_sink_t s; DWORD ck = 0; int i;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    for (i = 0; i < PF_N(t); i++) {
        BSTR nm = SysAllocString(L"Tahoma"); CY sz; sz.int64 = 90000 + i * 10000;
        s.calls = 0; s.last_name[0] = 0;
        switch (t[i].which) {
        case 0: hr = IFont_put_Name(f, nm); break;
        case 1: hr = IFont_put_Size(f, sz); break;
        case 2: hr = IFont_put_Bold(f, TRUE); break;
        case 3: hr = IFont_put_Italic(f, TRUE); break;
        case 4: hr = IFont_put_Underline(f, TRUE); break;
        case 5: hr = IFont_put_Strikethrough(f, TRUE); break;
        case 6: hr = IFont_put_Weight(f, 200); break;
        default: hr = IFont_put_Charset(f, 3); break;
        }
        SysFreeString(nm);
        CHECKF(hr == S_OK, "%ls hr=%08lx", t[i].name, hr);
        CHECKF(s.calls == 1, "%ls: %d notifications", t[i].name, s.calls);
        CHECKF(!lstrcmpW(s.last_name, t[i].name), "%ls: arg %ls", t[i].name, s.last_name);
    }
    IConnectionPoint_Unadvise(cp, ck);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontevents_changed_via_invoke) {
    FONT_BEGIN(f);
    HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); pf_sink_t s; DWORD ck = 0;
    IFontDisp *d = disp_of(f); int i;
    if (!cp || !d) { CHECK(cp && d); if (cp) IConnectionPoint_Release(cp); if (d) IFontDisp_Release(d); IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    for (i = 0; i < PF_N(fprops); i++) {
        VARIANT v;
        switch (fprops[i].vt) {
        case VT_BSTR: V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"Courier New"); break;   /* must differ from the current name: unchanged values do not notify */
        case VT_CY:   V_VT(&v) = VT_CY; V_CY(&v).int64 = 250000; break;
        case VT_BOOL: V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE; break;
        default:      V_VT(&v) = VT_I2; V_I2(&v) = fprops[i].id == DISPID_FONT_WEIGHT ? 900 : 1; break;
        }
        s.calls = 0;
        hr = inv_put(d, fprops[i].id, &v, 0);
        CHECKF(hr == S_OK, "%ls hr=%08lx", fprops[i].name, hr);
        CHECKF(s.calls == 1, "%ls: %d notifications", fprops[i].name, s.calls);
        CHECKF(!lstrcmpW(s.last_name, fprops[i].name), "%ls: arg %ls", fprops[i].name, s.last_name);
        VariantClear(&v);
    }
    IConnectionPoint_Unadvise(cp, ck);
    IConnectionPoint_Release(cp); IFontDisp_Release(d); IFont_Release(f);
}
TEST(pfontevents_no_event_without_advise) {
    FONT_BEGIN(f);
    pf_sink_t s; HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); DWORD ck = 0;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    EQ(IFont_put_Italic(f, TRUE), S_OK);
    EQ(s.calls, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    EQ(IConnectionPoint_Unadvise(cp, ck), S_OK);
    EQ(IFont_put_Italic(f, FALSE), S_OK);
    EQ(s.calls, 0);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
TEST(pfontevents_unadvise_errors) {
    FONT_BEGIN(f);
    pf_sink_t s; HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); DWORD ck = 0;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    hr = IConnectionPoint_Unadvise(cp, 0); CHECK(FAILED(hr)); SOFT_EQ(hr, PF_CONNECT_E_NOCONNECTION);
    hr = IConnectionPoint_Unadvise(cp, 0x12345); CHECK(FAILED(hr)); SOFT_EQ(hr, PF_CONNECT_E_NOCONNECTION);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    EQ(IConnectionPoint_Unadvise(cp, ck), S_OK);
    hr = IConnectionPoint_Unadvise(cp, ck); CHECK(FAILED(hr)); SOFT_EQ(hr, PF_CONNECT_E_NOCONNECTION);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
TEST(pfontevents_advise_sink_requirements) {
    FONT_BEGIN(f);
    pf_sink_t s; HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); DWORD ck = 0;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&s, 1);   /* sink answers IUnknown only */
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck);
    CHECKF(FAILED(hr), "advise accepted a sink that exposes no event interface: %08lx", hr);
    SOFT_EQ(hr, PF_CONNECT_E_CANNOTCONNECT);
    if (SUCCEEDED(hr)) IConnectionPoint_Unadvise(cp, ck);
    EQ(s.ref, 1);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
TEST(pfontevents_multiple_sinks) {
    FONT_BEGIN(f);
    pf_sink_t a, b; HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); DWORD ca = 0, cb = 0;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&a, 0); pf_sink_init(&b, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&a.iface, &ca), S_OK);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&b.iface, &cb);
    t_note("second Advise on IFontEventsDisp point: hr=%08lx", hr);
    EQ(IFont_put_Underline(f, TRUE), S_OK);
    EQ(a.calls, 1);
    if (hr == S_OK) { EQ(b.calls, 1); CHECK(ca != cb); IConnectionPoint_Unadvise(cp, cb); }
    IConnectionPoint_Unadvise(cp, ca);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
TEST(pfontevents_sink_refcounting) {
    FONT_BEGIN(f);
    pf_sink_t s; HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); DWORD ck = 0;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    CHECK(s.ref >= 2);
    EQ(IConnectionPoint_Unadvise(cp, ck), S_OK);
    EQ(s.ref, 1);
    /* Releasing the font with an active connection releases the sink too */
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    IConnectionPoint_Release(cp);
    EQ(IFont_Release(f), 0);
    EQ(s.ref, 1);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfontevents_clone_does_not_copy_connections) {
    FONT_BEGIN(f);
    pf_sink_t s; HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); DWORD ck = 0; IFont *c = NULL;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    pf_sink_init(&s, 0);
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    EQ(IFont_Clone(f, &c), S_OK);
    if (c) {
        s.calls = 0;
        EQ(IFont_put_Bold(c, TRUE), S_OK);
        EQ(s.calls, 0);
        IFont_Release(c);
    }
    IConnectionPoint_Unadvise(cp, ck);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
TEST(pfontevents_cp_metadata) {
    FONT_BEGIN(f);
    HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IFontEventsDisp, &hr); GUID g; IConnectionPointContainer *c = NULL, *c2 = NULL;
    IUnknown *u1 = NULL, *u2 = NULL;
    if (!cp) { CHECK(cp); IFont_Release(f); return; }
    memset(&g, 0, sizeof g);
    EQ(IConnectionPoint_GetConnectionInterface(cp, &g), S_OK);
    CHECK(IsEqualGUID(&g, &IID_IFontEventsDisp));
    EQ(IConnectionPoint_GetConnectionPointContainer(cp, &c), S_OK);
    EQ(IFont_QueryInterface(f, &IID_IConnectionPointContainer, (void **)&c2), S_OK);
    if (c && c2) {
        EQ(IConnectionPointContainer_QueryInterface(c, &IID_IUnknown, (void **)&u1), S_OK);
        EQ(IConnectionPointContainer_QueryInterface(c2, &IID_IUnknown, (void **)&u2), S_OK);
        EQ(u1, u2);
    }
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    if (c) IConnectionPointContainer_Release(c);
    if (c2) IConnectionPointContainer_Release(c2);
    IConnectionPoint_Release(cp); IFont_Release(f);
}
TEST(pfontevents_find_connection_point_errors) {
    FONT_BEGIN(f);
    IConnectionPointContainer *c = NULL; IConnectionPoint *cp = (IConnectionPoint *)(ULONG_PTR)1; HRESULT hr;
    EQ(IFont_QueryInterface(f, &IID_IConnectionPointContainer, (void **)&c), S_OK);
    if (!c) { IFont_Release(f); return; }
    hr = IConnectionPointContainer_FindConnectionPoint(c, &IID_IFont, &cp);
    EQ(hr, PF_CONNECT_E_NOCONNECTION);
    SOFT_EQ(cp, NULL);
    cp = NULL;
    hr = IConnectionPointContainer_FindConnectionPoint(c, &IID_IFontEventsDisp, &cp);
    EQ(hr, S_OK);
    if (cp) IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(c); IFont_Release(f);
}
TEST(pfontevents_enum_connection_points) {
    FONT_BEGIN(f);
    IConnectionPointContainer *c = NULL; IEnumConnectionPoints *e = NULL; IConnectionPoint *cp[4]; ULONG n = 0; HRESULT hr;
    EQ(IFont_QueryInterface(f, &IID_IConnectionPointContainer, (void **)&c), S_OK);
    if (!c) { IFont_Release(f); return; }
    hr = IConnectionPointContainer_EnumConnectionPoints(c, &e);
    SOFT_EQ(hr, S_OK);
    if (hr == S_OK && e) {
        int seen_font = 0, seen_pns = 0; ULONG total = 0, k;
        memset(cp, 0, sizeof cp);
        while (IEnumConnectionPoints_Next(e, 1, cp, &n) == S_OK && n == 1) {
            GUID g; total++;
            if (IConnectionPoint_GetConnectionInterface(cp[0], &g) == S_OK) {
                if (IsEqualGUID(&g, &IID_IFontEventsDisp)) seen_font++;
                if (IsEqualGUID(&g, &IID_IPropertyNotifySink)) seen_pns++;
            }
            IConnectionPoint_Release(cp[0]);
            if (total > 8) break;
        }
        t_note("StdFont exposes %lu connection points (font events %d, property notify %d)", total, seen_font, seen_pns);
        EQ(seen_font, 1);
        CHECK(total >= 1);
        EQ(IEnumConnectionPoints_Reset(e), S_OK);
        for (k = 0; k < total; k++) { n = 0; EQ(IEnumConnectionPoints_Skip(e, 1), S_OK); }
        IEnumConnectionPoints_Release(e);
    }
    IConnectionPointContainer_Release(c); IFont_Release(f);
}

/* IPropertyNotifySink */
typedef struct { IPropertyNotifySink iface; LONG ref; int changed; DISPID last; int req; } pns_t;
static HRESULT WINAPI pn_qi(IPropertyNotifySink *i, REFIID r, void **pp) {
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPropertyNotifySink)) { *pp = i; ((pns_t *)i)->ref++; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pn_ar(IPropertyNotifySink *i) { return ++((pns_t *)i)->ref; }
static ULONG WINAPI pn_rel(IPropertyNotifySink *i) { return --((pns_t *)i)->ref; }
static HRESULT WINAPI pn_changed(IPropertyNotifySink *i, DISPID d) { ((pns_t *)i)->changed++; ((pns_t *)i)->last = d; return S_OK; }
static HRESULT WINAPI pn_req(IPropertyNotifySink *i, DISPID d) { ((pns_t *)i)->req++; return S_OK; }
static IPropertyNotifySinkVtbl pn_vtbl = { pn_qi, pn_ar, pn_rel, pn_changed, pn_req };
TEST(pfontevents_property_notify_sink) {
    FONT_BEGIN(f);
    HRESULT hr; IConnectionPoint *cp = find_cp(f, &IID_IPropertyNotifySink, &hr); pns_t s; DWORD ck = 0;
    if (!cp) { t_note("no IPropertyNotifySink connection point (hr=%08lx)", hr); IFont_Release(f); return; }
    memset(&s, 0, sizeof s); s.iface.lpVtbl = &pn_vtbl; s.ref = 1;
    EQ(IConnectionPoint_Advise(cp, (IUnknown *)&s.iface, &ck), S_OK);
    EQ(IFont_put_Weight(f, 300), S_OK);
    t_note("OnChanged calls=%d last dispid=%ld (after put_Weight)", s.changed, s.last);
    CHECK(s.changed >= 1);
    SOFT_EQ(s.last, DISPID_FONT_WEIGHT);
    IConnectionPoint_Unadvise(cp, ck);
    IConnectionPoint_Release(cp); IFont_Release(f);
}

/* ---- persistence ---------------------------------------------------------------------------- */
static IStream *mkstream(void) { IStream *s = NULL; CreateStreamOnHGlobal(NULL, TRUE, &s); return s; }
static size_t stream_bytes(IStream *s, BYTE *buf, size_t cap) {
    LARGE_INTEGER z; ULONG got = 0; STATSTG st;
    z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
    memset(&st, 0, sizeof st);
    if (FAILED(IStream_Stat(s, &st, STATFLAG_NONAME))) return 0;
    if (st.cbSize.QuadPart > cap) return 0;
    IStream_Read(s, buf, (ULONG)st.cbSize.QuadPart, &got);
    return got;
}
static void rewind_stream(IStream *s) { LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL); }

TEST(pfontpersist_classid) {
    FONT_BEGIN(f);
    IPersistStream *ps = NULL; CLSID c; IPersist *p = NULL;
    EQ(IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps), S_OK);
    if (ps) {
        memset(&c, 0, sizeof c);
        EQ(IPersistStream_GetClassID(ps, &c), S_OK);
        CHECK(IsEqualCLSID(&c, &PF_CLSID_StdFont));
        IPersistStream_Release(ps);
    }
    EQ(IFont_QueryInterface(f, &IID_IPersist, (void **)&p), S_OK);
    if (p) { memset(&c, 0, sizeof c); EQ(IPersist_GetClassID(p, &c), S_OK); CHECK(IsEqualCLSID(&c, &PF_CLSID_StdFont)); IPersist_Release(p); }
    IFont_Release(f);
}
TEST(pfontpersist_getsizemax) {
    FONTDESC fd; IFont *f = NULL; IPersistStream *ps = NULL; ULARGE_INTEGER sz;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, 400, 0, 0, 0, 0);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    EQ(IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps), S_OK);
    if (ps) {
        sz.QuadPart = 0xffff;
        EQ(IPersistStream_GetSizeMax(ps, &sz), S_OK);
        /* Win10: GetSizeMax is an upper bound, 11 byte header + LF_FACESIZE (32) name bytes, not the exact 11 + 5 */
        EQ(sz.QuadPart, 11 + 32);
        IPersistStream_Release(ps);
    }
    IFont_Release(f);
}
TEST(pfontpersist_isdirty) {
    FONT_BEGIN(f);
    IPersistStream *ps = NULL; HRESULT hr;
    EQ(IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps), S_OK);
    if (ps) {
        hr = IPersistStream_IsDirty(ps);
        CHECKF(hr == S_OK || hr == S_FALSE, "hr=%08lx", hr);
        t_note("IsDirty on new font: %08lx", hr);
        IPersistStream_Release(ps);
    }
    IFont_Release(f);
}
TEST(pfontpersist_save_layout) {
    FONTDESC fd; IFont *f = NULL; IPersistStream *ps = NULL; IStream *s; BYTE b[64]; size_t n;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, 700, ANSI_CHARSET, TRUE, FALSE, TRUE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    EQ(IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps), S_OK);
    s = mkstream();
    CHECK(ps && s);
    if (ps && s) {
        EQ(IPersistStream_Save(ps, s, FALSE), S_OK);
        n = stream_bytes(s, b, sizeof b);
        EQ(n, 11 + 5);
        if (n == 16) {
            EQ(b[0], 1);                              /* version */
            EQ(*(WORD *)(b + 1), ANSI_CHARSET);       /* charset */
            SOFT_EQ(b[3], 0x02 | 0x08);               /* italic | strikethrough */
            EQ(*(WORD *)(b + 4), 700);                /* weight */
            SOFT_EQ(*(DWORD *)(b + 6), 120000);     /* size.Lo */
            EQ(b[10], 5);                             /* name length */
            CHECK(!memcmp(b + 11, "Arial", 5));
        }
    }
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IFont_Release(f);
}
TEST(pfontpersist_save_attribute_bits) {
    static const struct { BOOL it, ul, st; BYTE bits; } t[] = {
        { 0, 0, 0, 0x00 }, { 1, 0, 0, 0x02 }, { 0, 1, 0, 0x04 }, { 0, 0, 1, 0x08 }, { 1, 1, 1, 0x0e },
    };
    int i; REQUIRE(U(OleCreateFontIndirect));
    for (i = 0; i < PF_N(t); i++) {
        FONTDESC fd; IFont *f = NULL; IPersistStream *ps = NULL; IStream *s; BYTE b[64]; size_t n;
        pf_fontdesc(&fd, pf_arial, 120000, 400, 0, t[i].it, t[i].ul, t[i].st);
        if (FAILED(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f)) || !f) { CHECK(0); continue; }
        IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps);
        s = mkstream();
        if (ps && s && IPersistStream_Save(ps, s, FALSE) == S_OK) {
            n = stream_bytes(s, b, sizeof b);
            if (n >= 4) SOFT_EQ(b[3], t[i].bits);
        } else CHECK(0);
        if (s) IStream_Release(s);
        if (ps) IPersistStream_Release(ps);
        IFont_Release(f);
    }
}
TEST(pfontpersist_roundtrip) {
    FONTDESC fd; IFont *a = NULL, *b = NULL; IPersistStream *pa = NULL, *pb = NULL; IStream *s; BSTR nm = NULL; SHORT w, cs;
    CY sz; BOOL bb;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 135000, 300, GREEK_CHARSET, TRUE, TRUE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&a), S_OK);
    b = pf_font_new();
    if (!a || !b) { CHECK(a && b); return; }
    IFont_QueryInterface(a, &IID_IPersistStream, (void **)&pa);
    IFont_QueryInterface(b, &IID_IPersistStream, (void **)&pb);
    s = mkstream();
    if (pa && pb && s) {
        EQ(IPersistStream_Save(pa, s, TRUE), S_OK);
        rewind_stream(s);
        EQ(IPersistStream_Load(pb, s), S_OK);
        EQ(IFont_IsEqual(a, b), S_OK);
        EQ(IFont_get_Name(b, &nm), S_OK); CHECK(nm && !lstrcmpW(nm, pf_arial)); SysFreeString(nm);
        EQ(IFont_get_Size(b, &sz), S_OK); EQ(sz.int64, 135000);
        EQ(IFont_get_Weight(b, &w), S_OK); EQ(w, pf_snap_weight(300));
        EQ(IFont_get_Charset(b, &cs), S_OK); EQ(cs, GREEK_CHARSET);
        EQ(IFont_get_Italic(b, &bb), S_OK); EQ(bb, TRUE);
        EQ(IFont_get_Underline(b, &bb), S_OK); EQ(bb, TRUE);
        EQ(IFont_get_Strikethrough(b, &bb), S_OK); EQ(bb, FALSE);
    } else CHECK(0);
    if (s) IStream_Release(s);
    if (pa) IPersistStream_Release(pa);
    if (pb) IPersistStream_Release(pb);
    IFont_Release(a); IFont_Release(b);
}
TEST(pfontpersist_save_position_advances) {
    FONT_BEGIN(f);
    IPersistStream *ps = NULL; IStream *s = mkstream(); ULARGE_INTEGER pos; LARGE_INTEGER z; ULARGE_INTEGER max;
    IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps);
    if (ps && s) {
        EQ(IPersistStream_GetSizeMax(ps, &max), S_OK);
        EQ(IPersistStream_Save(ps, s, FALSE), S_OK);
        z.QuadPart = 0; pos.QuadPart = 0;
        EQ(IStream_Seek(s, z, STREAM_SEEK_CUR, &pos), S_OK);
        /* Win10: GetSizeMax is an upper bound; the position is the exact size written (11 + 5 for "Arial") */
        CHECK(pos.QuadPart <= max.QuadPart); EQ(pos.QuadPart, 11 + 5);
    } else CHECK(0);
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IFont_Release(f);
}
TEST(pfontpersist_load_handmade_stream) {
    /* version 1, charset 0, attr italic|underline, weight 700, size 14pt, name "Tahoma" */
    static const BYTE img[] = { 1, 0, 0, 0x06, 0xbc, 0x02, 0x60, 0x22, 0x02, 0x00, 6, 'T','a','h','o','m','a' };
    BYTE tmp[sizeof img]; FONT_BEGIN(f);
    IPersistStream *ps = NULL; IStream *s; BSTR nm = NULL; BOOL b; SHORT w; CY sz;
    memcpy(tmp, img, sizeof tmp);
    *(DWORD *)(tmp + 6) = 140000;
    IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps);
    s = pf_mem_stream(tmp, sizeof tmp);
    if (ps && s) {
        EQ(IPersistStream_Load(ps, s), S_OK);
        EQ(IFont_get_Name(f, &nm), S_OK); CHECK(nm && !lstrcmpW(nm, L"Tahoma")); SysFreeString(nm);
        EQ(IFont_get_Weight(f, &w), S_OK); EQ(w, 700);
        EQ(IFont_get_Size(f, &sz), S_OK); CHECK(sz.int64 - 140000 < 10000 && sz.int64 - 140000 > -10000);
        EQ(IFont_get_Italic(f, &b), S_OK); EQ(b, TRUE);
        EQ(IFont_get_Underline(f, &b), S_OK); EQ(b, TRUE);
        EQ(IFont_get_Strikethrough(f, &b), S_OK); EQ(b, FALSE);
    } else CHECK(0);
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IFont_Release(f);
}
TEST(pfontpersist_load_invalid_streams) {
    static const BYTE bad_version[] = { 2, 0, 0, 0, 0x90, 0x01, 0x60, 0xea, 0, 0, 0 };
    static const BYTE truncated[] = { 1, 0, 0, 0, 0x90 };
    static const BYTE name_overrun[] = { 1, 0, 0, 0, 0x90, 0x01, 0x60, 0xea, 0, 0, 20, 'A', 'B' };
    static const struct { const BYTE *d; size_t n; const char *why; } t[] = {
        { bad_version, sizeof bad_version, "bad version" },
        { truncated, sizeof truncated, "truncated header" },
        { name_overrun, sizeof name_overrun, "name length overruns stream" },
        { (const BYTE *)"", 0, "empty" },
    };
    int i; FONT_BEGIN(f);
    for (i = 0; i < PF_N(t); i++) {
        IPersistStream *ps = NULL; IStream *s = pf_mem_stream(t[i].d, t[i].n); HRESULT hr;
        IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps);
        if (ps && s) {
            hr = IPersistStream_Load(ps, s);
            CHECKF(FAILED(hr), "%s accepted: %08lx", t[i].why, hr);
            SOFT_EQ(hr, PF_E_FAIL);
        } else CHECK(0);
        if (s) IStream_Release(s);
        if (ps) IPersistStream_Release(ps);
    }
    IFont_Release(f);
}
TEST(pfontpersist_load_failure_leaves_font_usable) {
    static const BYTE bad_version[] = { 9, 0, 0, 0, 0x90, 0x01, 0x60, 0xea, 0, 0, 0 };
    FONT_BEGIN(f);
    IPersistStream *ps = NULL; IStream *s = pf_mem_stream(bad_version, sizeof bad_version); BSTR nm = NULL;
    IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps);
    if (ps && s) IPersistStream_Load(ps, s);
    EQ(IFont_get_Name(f, &nm), S_OK);
    CHECK(nm && !lstrcmpiW(nm, pf_arial));
    SysFreeString(nm);
    if (s) IStream_Release(s);
    if (ps) IPersistStream_Release(ps);
    IFont_Release(f);
}
TEST(pfontpersist_load_replaces_hfont) {
    FONTDESC fd; IFont *a = NULL; IFont *b; IPersistStream *pa = NULL, *pb = NULL; IStream *s; HFONT h = NULL; LOGFONTW lf;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, 800, 0, 0, 0, 0);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&a), S_OK);
    b = pf_font_new();
    if (!a || !b) { CHECK(a && b); return; }
    IFont_get_hFont(b, &h);
    IFont_QueryInterface(a, &IID_IPersistStream, (void **)&pa);
    IFont_QueryInterface(b, &IID_IPersistStream, (void **)&pb);
    s = mkstream();
    if (pa && pb && s) {
        IPersistStream_Save(pa, s, FALSE); rewind_stream(s);
        EQ(IPersistStream_Load(pb, s), S_OK);
        EQ(IFont_get_hFont(b, &h), S_OK);
        memset(&lf, 0, sizeof lf);
        if (h && GetObjectW(h, sizeof lf, &lf) > 0) EQ(lf.lfWeight, 800);
    }
    if (s) IStream_Release(s);
    if (pa) IPersistStream_Release(pa);
    if (pb) IPersistStream_Release(pb);
    IFont_Release(a); IFont_Release(b);
}

/* IPersistPropertyBag */
typedef struct { IPropertyBag iface; LONG ref; int reads; WCHAR seen[16][16]; } bag_t;
static HRESULT WINAPI pb_qi(IPropertyBag *i, REFIID r, void **pp) {
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPropertyBag)) { *pp = i; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pb_ar(IPropertyBag *i) { return 2; }
static ULONG WINAPI pb_rel(IPropertyBag *i) { return 1; }
static HRESULT WINAPI pb_read(IPropertyBag *i, LPCOLESTR name, VARIANT *v, IErrorLog *e) {
    bag_t *b = (bag_t *)i;
    t_note("bag Read(%ls) incoming vt=%u", name, V_VT(v));
    if (b->reads < 16) lstrcpynW(b->seen[b->reads], name, 16);
    b->reads++;
    if (!lstrcmpW(name, L"Name")) { V_VT(v) = VT_BSTR; V_BSTR(v) = SysAllocString(L"Tahoma"); return S_OK; }
    /* the control asks for VT_CY (incoming vt=6); Win10 does not coerce a VT_R8 answer (fails with DISP_E_OVERFLOW) */
    if (!lstrcmpW(name, L"Size")) { V_VT(v) = VT_CY; V_CY(v).int64 = 138000; return S_OK; }
    if (!lstrcmpW(name, L"Weight")) { V_VT(v) = VT_I2; V_I2(v) = 700; return S_OK; }
    if (!lstrcmpW(name, L"Charset")) { V_VT(v) = VT_I2; V_I2(v) = 0; return S_OK; }
    if (!lstrcmpW(name, L"Italic")) { V_VT(v) = VT_BOOL; V_BOOL(v) = VARIANT_TRUE; return S_OK; }
    return E_INVALIDARG;
}
static HRESULT WINAPI pb_write(IPropertyBag *i, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static IPropertyBagVtbl pb_vtbl = { pb_qi, pb_ar, pb_rel, pb_read, pb_write };
TEST(pfontpersist_propertybag_load) {
    FONT_BEGIN(f);
    IPersistPropertyBag *pp = NULL; bag_t bag; HRESULT hr; BSTR nm = NULL; CY sz; SHORT w; BOOL b; int i, saw_name = 0;
    memset(&bag, 0, sizeof bag); bag.iface.lpVtbl = &pb_vtbl;
    EQ(IFont_QueryInterface(f, &IID_IPersistPropertyBag, (void **)&pp), S_OK);
    if (!pp) { IFont_Release(f); return; }
    hr = IPersistPropertyBag_Load(pp, &bag.iface, NULL);
    EQ(hr, S_OK);
    EQ(IFont_get_Name(f, &nm), S_OK); CHECK(nm && !lstrcmpW(nm, L"Tahoma")); SysFreeString(nm);
    EQ(IFont_get_Weight(f, &w), S_OK); EQ(w, 700);
    EQ(IFont_get_Italic(f, &b), S_OK); CHECK(b != 0);
    EQ(IFont_get_Size(f, &sz), S_OK); CHECK(sz.int64 - 138000 < 10000 && sz.int64 - 138000 > -10000);
    for (i = 0; i < bag.reads && i < 16; i++) if (!lstrcmpW(bag.seen[i], L"Name")) saw_name = 1;
    EQ(saw_name, 1);
    t_note("property bag reads: %d", bag.reads);
    IPersistPropertyBag_Release(pp);
    IFont_Release(f);
}

/* ---- PF_CLSID_StdFont activation --------------------------------------------------------------- */
TEST(pfontclass_cocreateinstance) {
    HRESULT hr; IFont *f = NULL; BSTR nm = NULL;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoCreateInstance)); REQUIRE(U(CoUninitialize));
    hr = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    EQ(hr, S_OK);
    hr = U(CoCreateInstance)(&PF_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &IID_IFont, (void **)&f);
    EQ(hr, S_OK);
    if (f) {
        EQ(IFont_get_Name(f, &nm), S_OK);
        CHECK(nm != NULL);
        SysFreeString(nm);
        EQ(IFont_Release(f), 0);
    }
    U(CoUninitialize)();
}
TEST(pfontclass_cocreate_matches_indirect_default) {
    /* Win10: both creators give the same visible default font (MS Sans Serif, 8.25pt, weight 400, charset 0), instances of
     * the same creator compare equal, but IsEqual between a CoCreateInstance font and an OleCreateFontIndirect(NULL)
     * font is S_FALSE (hidden internal state differs). */
    IFont *a = NULL, *a2 = NULL, *b = NULL, *b2 = NULL;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoCreateInstance)); REQUIRE(U(CoUninitialize)); REQUIRE(U(OleCreateFontIndirect));
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    EQ(U(CoCreateInstance)(&PF_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &IID_IFont, (void **)&a), S_OK);
    EQ(U(CoCreateInstance)(&PF_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &IID_IFont, (void **)&a2), S_OK);
    EQ(U(OleCreateFontIndirect)(NULL, &IID_IFont, (void **)&b), S_OK);
    EQ(U(OleCreateFontIndirect)(NULL, &IID_IFont, (void **)&b2), S_OK);
    if (a && a2 && b && b2) {
        BSTR na = NULL, nb = NULL; CY sa, sb; SHORT wa = 0, wb = 0, ca = 0, cb = 0;
        IFont_get_Name(a, &na); IFont_get_Name(b, &nb); IFont_get_Size(a, &sa); IFont_get_Size(b, &sb);
        IFont_get_Weight(a, &wa); IFont_get_Weight(b, &wb); IFont_get_Charset(a, &ca); IFont_get_Charset(b, &cb);
        t_note("CoCreate default: '%ls' size=%lld weight=%d charset=%d", na ? na : L"", (long long)sa.int64, wa, ca);
        t_note("OleCreateFontIndirect(NULL): '%ls' size=%lld weight=%d charset=%d", nb ? nb : L"", (long long)sb.int64, wb, cb);
        CHECK(na && nb && !lstrcmpW(na, nb)); EQ(sa.int64, sb.int64); EQ(wa, wb); EQ(ca, cb);
        SysFreeString(na); SysFreeString(nb);
        EQ(IFont_IsEqual(a, a2), S_OK);
        EQ(IFont_IsEqual(b, b2), S_OK);
        EQ(IFont_IsEqual(a, b), S_FALSE);
    }
    if (a) IFont_Release(a);
    if (a2) IFont_Release(a2);
    if (b) IFont_Release(b);
    if (b2) IFont_Release(b2);
    U(CoUninitialize)();
}
TEST(pfontclass_class_factory) {
    HRESULT hr; IClassFactory *cf = NULL; IFont *f = NULL;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetClassObject)); REQUIRE(U(CoUninitialize));
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    hr = U(CoGetClassObject)(&PF_CLSID_StdFont, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    EQ(hr, S_OK);
    if (cf) {
        EQ(IClassFactory_CreateInstance(cf, NULL, &IID_IFont, (void **)&f), S_OK);
        CHECK(f != NULL);
        if (f) IFont_Release(f);
        EQ(IClassFactory_LockServer(cf, TRUE), S_OK);
        EQ(IClassFactory_LockServer(cf, FALSE), S_OK);
        f = (IFont *)(ULONG_PTR)1;
        hr = IClassFactory_CreateInstance(cf, NULL, &IID_IPicture, (void **)&f);
        EQ(hr, PF_E_NOINTERFACE);
        EQ(f, NULL);
        IClassFactory_Release(cf);
    }
    U(CoUninitialize)();
}
TEST(pfontclass_aggregation) {
    HRESULT hr; IClassFactory *cf = NULL; IUnknown *outer_dummy = NULL; IUnknown *u = NULL;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetClassObject)); REQUIRE(U(CoUninitialize));
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    hr = U(CoGetClassObject)(&PF_CLSID_StdFont, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    if (hr == S_OK && cf) {
        IFont *f = pf_font_new();
        if (f) {
            outer_dummy = (IUnknown *)f;   /* any live IUnknown serves as a controlling unknown */
            hr = IClassFactory_CreateInstance(cf, outer_dummy, &IID_IFont, (void **)&u);
            t_note("StdFont CreateInstance with outer unknown and IID_IFont: hr=%08lx", hr);
            if (SUCCEEDED(hr) && u) IUnknown_Release(u);
            IFont_Release(f);
        }
        IClassFactory_Release(cf);
    } else CHECKF(0, "CoGetClassObject hr=%08lx", hr);
    U(CoUninitialize)();
}
