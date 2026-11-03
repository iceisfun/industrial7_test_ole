/* Export placement and cross-module agreement for the PROPVARIANT / property set entry points. */
#include "pv.h"

TEST(pvps_pvm_ole32_exports_property_apis) {
    static const char *names[] = { "PropVariantClear", "PropVariantCopy", "FreePropVariantArray", "StgCreatePropStg", "StgOpenPropStg",
        "StgCreatePropSetStg", "FmtIdToPropStgName", "PropStgNameToFmtId", "StgConvertVariantToProperty", "StgConvertPropertyToVariant",
        "StgPropertyLengthAsVariant" };
    for (int i = 0; i < N_ELEM(names); i++) {
        FARPROC p = t_proc_in("ole32.dll", names[i]);
        CHECKF(p != NULL, "ole32.dll does not export %s", names[i]);
    }
}

TEST(pvps_pvm_combase_does_not_export_property_set_apis) {
    static const char *names[] = { "StgCreatePropStg", "StgOpenPropStg", "StgCreatePropSetStg", "FmtIdToPropStgName", "PropStgNameToFmtId",
        "StgConvertVariantToProperty", "StgConvertPropertyToVariant" };
    for (int i = 0; i < N_ELEM(names); i++) {
        SetLastError(0);
        FARPROC p = t_proc_in("combase.dll", names[i]);
        CHECKF(p == NULL, "combase.dll unexpectedly exports %s", names[i]);
    }
}

TEST(pvps_pvm_combase_exports_variant_lifetime_apis) {
    CHECK(t_proc_in("combase.dll", "PropVariantClear") != NULL);
    CHECK(t_proc_in("combase.dll", "PropVariantCopy") != NULL);
    CHECK(t_proc_in("combase.dll", "FreePropVariantArray") != NULL);
}

TEST(pvps_pvm_ole32_and_combase_agree_on_clear_copy) {
    HRESULT (WINAPI *c1)(PROPVARIANT *) = (void *)t_proc_in("ole32.dll", "PropVariantClear");
    HRESULT (WINAPI *c2)(PROPVARIANT *) = (void *)t_proc_in("combase.dll", "PropVariantClear");
    HRESULT (WINAPI *p1)(PROPVARIANT *, const PROPVARIANT *) = (void *)t_proc_in("ole32.dll", "PropVariantCopy");
    HRESULT (WINAPI *p2)(PROPVARIANT *, const PROPVARIANT *) = (void *)t_proc_in("combase.dll", "PropVariantCopy");
    REQUIRE(c1); REQUIRE(c2); REQUIRE(p1); REQUIRE(p2);
    t_note("PropVariantClear: ole32=%p combase=%p (same address = forwarder)", (void *)c1, (void *)c2);
    PROPVARIANT s, d1, d2, bad;
    PropVariantInit(&s); s.vt = VT_LPWSTR; s.pwszVal = (LPWSTR)L"agree";
    EQ(p1(&d1, &s), S_OK); EQ(p2(&d2, &s), S_OK);
    CHECK(d1.vt == d2.vt && !lstrcmpW(d1.pwszVal, d2.pwszVal));
    EQ(c1(&d1), S_OK); EQ(c2(&d2), S_OK);
    memset(&bad, 0x55, sizeof bad);
    EQ(c1(&bad), STG_E_INVALIDPARAMETER_);
    memset(&bad, 0x55, sizeof bad);
    EQ(c2(&bad), STG_E_INVALIDPARAMETER_);
    memset(&bad, 0x55, sizeof bad);
    EQ(p1(&d1, &bad), DISP_E_BADVARTYPE_);
    EQ(p2(&d2, &bad), DISP_E_BADVARTYPE_);
}

TEST(pvps_pvm_free_propvariant_array_agree) {
    HRESULT (WINAPI *f1)(ULONG, PROPVARIANT *) = (void *)t_proc_in("ole32.dll", "FreePropVariantArray");
    HRESULT (WINAPI *f2)(ULONG, PROPVARIANT *) = (void *)t_proc_in("combase.dll", "FreePropVariantArray");
    REQUIRE(f1); REQUIRE(f2);
    EQ(f1(0, NULL), S_OK); EQ(f2(0, NULL), S_OK);
    EQ(f1(2, NULL), E_INVALIDARG_); EQ(f2(2, NULL), E_INVALIDARG_);
}
