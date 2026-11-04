/* Late binding through ITypeInfo on a live StdFont instance: GetIDsOfNames / Invoke on IFont and Font. */
#include "tl.h"

#define SENT_PTR ((void *)(ULONG_PTR)0x1badf00d)

static int com_init(void)
{
    HRESULT (WINAPI *f)(LPVOID, DWORD) = (void *)t_proc_in("ole32.dll", "CoInitializeEx");
    if (!f) f = (void *)t_proc("CoInitializeEx");
    if (!f) return 0;
    return SUCCEEDED(f(NULL, COINIT_APARTMENTTHREADED));
}
static void com_uninit(void)
{
    void (WINAPI *f)(void) = (void *)t_proc("CoUninitialize");
    if (f) f();
}

static IFont *make_font(void)
{
    HRESULT (WINAPI *cci)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID *) = (void *)t_proc("CoCreateInstance");
    IFont *f = NULL;
    if (!cci) return NULL;
    if (FAILED(cci(&TL_CLSID_StdFont, NULL, CLSCTX_INPROC_SERVER, &TL_IID_IFont, (void **)&f))) return NULL;
    return f;
}

#define FONT_SETUP(font, tl, ifti, fdti) \
    if (!com_init()) { t_skip("CoInitializeEx unavailable"); return; } \
    ITypeLib *tl = tl_open_stdole(); \
    IFont *font = tl ? make_font() : NULL; \
    ITypeInfo *ifti = tl ? tl_get_named(tl, L"IFont") : NULL; \
    ITypeInfo *fdti = tl ? tl_get_named(tl, L"Font") : NULL; \
    if (!tl || !font || !ifti || !fdti) { \
        t_skip("stdole2/StdFont not available"); \
        if (font) IFont_Release(font); if (ifti) ITypeInfo_Release(ifti); if (fdti) ITypeInfo_Release(fdti); \
        if (tl) ITypeLib_Release(tl); com_uninit(); return; }
#define FONT_TEARDOWN(font, tl, ifti, fdti) do { IFont_Release(font); ITypeInfo_Release(ifti); ITypeInfo_Release(fdti); ITypeLib_Release(tl); com_uninit(); } while (0)

static DISPID id_of(ITypeInfo *ti, const WCHAR *name)
{
    OLECHAR *n = (OLECHAR *)name; DISPID id = DISPID_UNKNOWN;
    if (FAILED(ITypeInfo_GetIDsOfNames(ti, &n, 1, &id))) return DISPID_UNKNOWN;
    return id;
}

static HRESULT inv_get(ITypeInfo *ti, void *inst, DISPID id, VARIANT *res)
{
    DISPPARAMS dp = {0}; EXCEPINFO ei; UINT arg = 0xdead;
    memset(&ei, 0, sizeof ei);
    memset(res, 0, sizeof *res);
    return ITypeInfo_Invoke(ti, inst, id, DISPATCH_PROPERTYGET, &dp, res, &ei, &arg);
}

static HRESULT inv_put(ITypeInfo *ti, void *inst, DISPID id, VARIANT *arg, UINT *argerr)
{
    DISPPARAMS dp; DISPID named = DISPID_PROPERTYPUT; EXCEPINFO ei;
    memset(&ei, 0, sizeof ei);
    dp.cArgs = 1; dp.cNamedArgs = 1; dp.rgdispidNamedArgs = &named; dp.rgvarg = arg;
    *argerr = 0xdead;
    return ITypeInfo_Invoke(ti, inst, id, DISPATCH_PROPERTYPUT, &dp, NULL, &ei, argerr);
}

static int font_dpi(void)
{
    HDC (WINAPI *gdc)(HWND) = (void *)t_proc_in("user32.dll", "GetDC");
    int (WINAPI *rdc)(HWND, HDC) = (void *)t_proc_in("user32.dll", "ReleaseDC");
    int (WINAPI *gcaps)(HDC, int) = (void *)t_proc_in("gdi32.dll", "GetDeviceCaps");
    if (!gdc || !rdc || !gcaps) return 96;
    HDC dc = gdc(NULL);
    int dpi = dc ? gcaps(dc, 90 /* LOGPIXELSY */) : 96;
    if (dc) rdc(NULL, dc);
    return dpi > 0 ? dpi : 96;
}

static BSTR mkbstr(const WCHAR *s)
{
    BSTR (WINAPI *f)(const OLECHAR *) = (void *)t_proc_in("oleaut32.dll", "SysAllocString");
    return f ? f(s) : NULL;
}

TEST(tlli_font_ifont_names_to_ids)
{
    FONT_SETUP(font, tl, ifti, fdti);
    static const WCHAR *n[] = { L"Name", L"Size", L"Bold", L"Italic", L"Underline", L"Strikethrough", L"Weight", L"Charset", L"hFont", L"Clone", L"IsEqual" };
    DISPID seen[11];
    for (int i = 0; i < 11; i++) {
        seen[i] = id_of(ifti, n[i]);
        CHECKF(seen[i] != DISPID_UNKNOWN, "IFont::%ls", n[i]);
        for (int j = 0; j < i; j++) CHECKF(seen[i] != seen[j], "IFont %ls and %ls share a DISPID", n[i], n[j]);
    }
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_dispinterface_ids_are_documented)
{
    FONT_SETUP(font, tl, ifti, fdti);
    TEQ(id_of(fdti, L"Name"), DISPID_FONT_NAME);
    TEQ(id_of(fdti, L"Size"), DISPID_FONT_SIZE);
    TEQ(id_of(fdti, L"Bold"), DISPID_FONT_BOLD);
    TEQ(id_of(fdti, L"Italic"), DISPID_FONT_ITALIC);
    TEQ(id_of(fdti, L"Underline"), DISPID_FONT_UNDER);
    TEQ(id_of(fdti, L"Strikethrough"), DISPID_FONT_STRIKE);
    TEQ(id_of(fdti, L"Weight"), DISPID_FONT_WEIGHT);
    TEQ(id_of(fdti, L"Charset"), DISPID_FONT_CHARSET);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_idispatch_ids_match_typelib)
{
    FONT_SETUP(font, tl, ifti, fdti);
    IDispatch *disp = NULL;
    TEQ(IFont_QueryInterface(font, &TL_IID_IDispatch, (void **)&disp), S_OK);
    REQUIRE(disp);
    static const WCHAR *n[] = { L"Name", L"Size", L"Bold", L"Italic", L"Underline", L"Strikethrough", L"Weight", L"Charset" };
    for (int i = 0; i < 8; i++) {
        OLECHAR *nm = (OLECHAR *)n[i]; DISPID d1 = 0x7777;
        TEQ(IDispatch_GetIDsOfNames(disp, &TL_GUID_ZERO, &nm, 1, 0x409, &d1), S_OK);
        TEQ(d1, id_of(fdti, n[i]));
    }
    OLECHAR *bad = L"olet_bogus"; DISPID d = 0x7777;
    TEQ(IDispatch_GetIDsOfNames(disp, &TL_GUID_ZERO, &bad, 1, 0x409, &d), DISP_E_UNKNOWNNAME);
    IDispatch_Release(disp);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_idispatch_gettypeinfo)
{
    FONT_SETUP(font, tl, ifti, fdti);
    IDispatch *disp = NULL;
    TEQ(IFont_QueryInterface(font, &TL_IID_IDispatch, (void **)&disp), S_OK);
    REQUIRE(disp);
    UINT cnt = 77;
    TEQ(IDispatch_GetTypeInfoCount(disp, &cnt), S_OK);
    TEQ(cnt, 1);
    ITypeInfo *ti = NULL;
    TEQ(IDispatch_GetTypeInfo(disp, 0, 0x409, &ti), S_OK);
    if (ti) {
        TYPEATTR *ta = NULL;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            CHECK(ta->typekind == TKIND_DISPATCH || ta->typekind == TKIND_INTERFACE);
            CHECK(tl_guid_eq(&ta->guid, &TL_IID_IFontDisp));
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ti = (ITypeInfo *)SENT_PTR;
    HRESULT hr = IDispatch_GetTypeInfo(disp, 1, 0x409, &ti);       /* only index 0 exists */
    TEQ(hr, DISP_E_BADINDEX);
    CHECK(FAILED(hr));
    /* Windows leaves *ppTInfo untouched on a bad index; Wine nulls it. Both are acceptable. */
    CHECK(ti == NULL || ti == (ITypeInfo *)SENT_PTR);
    IDispatch_Release(disp);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_invoke_name_roundtrip_via_ifont_typeinfo)
{
    FONT_SETUP(font, tl, ifti, fdti);
    DISPID id = id_of(ifti, L"Name");
    VARIANT arg, res; UINT ae;
    VariantInit(&arg);
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = mkbstr(L"Arial");
    TEQ(inv_put(ifti, font, id, &arg, &ae), S_OK);
    VariantClear(&arg);
    TEQ(inv_get(ifti, font, id, &res), S_OK);
    TEQ(V_VT(&res), VT_BSTR);
    WCHECK(V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"Arial"));
    VariantClear(&res);
    BSTR direct = NULL;                                          /* same value through the vtable */
    TEQ(IFont_get_Name(font, &direct), S_OK);
    WCHECK(direct && !lstrcmpW(direct, L"Arial"));
    tl_freebstr(direct);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_invoke_name_roundtrip_via_dispinterface)
{
    FONT_SETUP(font, tl, ifti, fdti);
    IDispatch *disp = NULL;
    TEQ(IFont_QueryInterface(font, &TL_IID_IDispatch, (void **)&disp), S_OK);
    REQUIRE(disp);
    VARIANT arg, res; UINT ae;
    VariantInit(&arg);
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = mkbstr(L"Tahoma");
    /* the library's Font dispinterface type info with the instance's IDispatch as pvInstance */
    HRESULT hrp = inv_put(fdti, disp, DISPID_FONT_NAME, &arg, &ae);
    t_note("ITypeInfo(Font)::Invoke(put Name, IDispatch instance) -> 0x%08lx", (unsigned long)hrp);
    VariantClear(&arg);
    if (hrp == S_OK) {
        TEQ(inv_get(fdti, disp, DISPID_FONT_NAME, &res), S_OK);
        CHECK(V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"Tahoma"));
        VariantClear(&res);
    } else {
        V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = mkbstr(L"Tahoma");
        DISPPARAMS dp2; DISPID named = DISPID_PROPERTYPUT; EXCEPINFO ei2; memset(&ei2, 0, sizeof ei2);
        dp2.cArgs = 1; dp2.cNamedArgs = 1; dp2.rgdispidNamedArgs = &named; dp2.rgvarg = &arg;
        TEQ(IDispatch_Invoke(disp, DISPID_FONT_NAME, &TL_GUID_ZERO, 0x409, DISPATCH_PROPERTYPUT, &dp2, NULL, &ei2, &ae), S_OK);
        VariantClear(&arg);
    }
    /* and direct IDispatch::Invoke */
    DISPPARAMS dp = {0}; EXCEPINFO ei; memset(&ei, 0, sizeof ei); VariantInit(&res);
    TEQ(IDispatch_Invoke(disp, DISPID_FONT_NAME, &TL_GUID_ZERO, 0x409, DISPATCH_PROPERTYGET, &dp, &res, &ei, &ae), S_OK);
    WCHECK(V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"Tahoma"));
    VariantClear(&res);
    IDispatch_Release(disp);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_size_currency_roundtrip)
{
    FONT_SETUP(font, tl, ifti, fdti);
    DISPID id = id_of(ifti, L"Size");
    static const LONGLONG sizes[] = { 80000, 100000, 120000, 82500, 240000 };   /* CY in 1/10000 pt */
    for (unsigned i = 0; i < 5; i++) {
        VARIANT arg, res; UINT ae;
        VariantInit(&arg);
        V_VT(&arg) = VT_CY; V_CY(&arg).int64 = sizes[i];
        TEQ(inv_put(ifti, font, id, &arg, &ae), S_OK);
        TEQ(inv_get(ifti, font, id, &res), S_OK);
        TEQ(V_VT(&res), VT_CY);
        /* Windows StdFont stores the size as a whole pixel height at the screen DPI, so the value read back is
           the requested size quantised to a pixel: round(size * dpi / 720000) * 720000 / dpi (8pt -> 8.25pt at 96 dpi). */
        int dpi = font_dpi();
        LONGLONG px = (sizes[i] * dpi + 360000) / 720000;
        LONGLONG want = (px * 720000 + dpi / 2) / dpi;
        if (!t_is_wine()) CHECKF(V_CY(&res).int64 == want, "size %lld -> %lld (want %lld at %d dpi)", sizes[i], V_CY(&res).int64, want, dpi);
        CY direct; direct.int64 = 0;
        TEQ(IFont_get_Size(font, &direct), S_OK);
        WCHECK(direct.int64 == V_CY(&res).int64);
    }
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_name_unknown_face_ignored)
{
    FONT_SETUP(font, tl, ifti, fdti);
    BSTR bogus = mkbstr(L"Olet Test Face"), g = NULL;
    TEQ(IFont_put_Name(font, bogus), S_OK);                  /* accepted, but the face is not installed */
    TEQ(IFont_get_Name(font, &g), S_OK);
    t_note("Name after put of a nonexistent face reads '%ls'", g ? g : L"(null)");
    WCHECK(g && lstrcmpW(g, L"Olet Test Face"));             /* Windows reports a face that exists, never the requested one */
    tl_freebstr(g); tl_freebstr(bogus);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_bool_properties_roundtrip)
{
    FONT_SETUP(font, tl, ifti, fdti);
    static const WCHAR *props[] = { L"Bold", L"Italic", L"Underline", L"Strikethrough" };
    for (int p = 0; p < 4; p++) {
        DISPID id = id_of(ifti, props[p]);
        static const VARIANT_BOOL vals[] = { VARIANT_TRUE, VARIANT_FALSE, VARIANT_TRUE };
        for (int k = 0; k < 3; k++) {
            VARIANT arg, res; UINT ae;
            VariantInit(&arg);
            V_VT(&arg) = VT_BOOL; V_BOOL(&arg) = vals[k];
            TEQ(inv_put(ifti, font, id, &arg, &ae), S_OK);
            TEQ(inv_get(ifti, font, id, &res), S_OK);
            /* IFont declares BOOL (int): Windows reports TRUE as 1 (not VARIANT_TRUE) in a VT_BOOL result. */
            if (!t_is_wine()) CHECKF(V_VT(&res) == VT_BOOL && V_BOOL(&res) == (vals[k] ? 1 : 0), "%ls round trip %d", props[p], k);
            if (!t_is_wine() && vals[k] == VARIANT_TRUE) t_note("%ls TRUE reads back as %d", props[p], V_BOOL(&res));
        }
    }
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_bold_weight_coupling)
{
    FONT_SETUP(font, tl, ifti, fdti);
    TEQ(IFont_put_Bold(font, TRUE), S_OK);
    SHORT w = 0;
    TEQ(IFont_get_Weight(font, &w), S_OK);
    TEQ(w, 700);
    TEQ(IFont_put_Bold(font, FALSE), S_OK);
    TEQ(IFont_get_Weight(font, &w), S_OK);
    TEQ(w, 400);
    TEQ(IFont_put_Weight(font, 900), S_OK);
    BOOL b = FALSE;
    TEQ(IFont_get_Bold(font, &b), S_OK);
    CHECK(b != FALSE);
    TEQ(IFont_get_Weight(font, &w), S_OK);
    TEQ(w, 700);                                              /* Windows keeps only 400 or 700 (weight > 550 is bold) */
    TEQ(IFont_put_Weight(font, 550), S_OK);
    TEQ(IFont_get_Weight(font, &w), S_OK);
    TEQ(w, 400);
    TEQ(IFont_put_Weight(font, 551), S_OK);
    TEQ(IFont_get_Weight(font, &w), S_OK);
    TEQ(w, 700);
    TEQ(IFont_put_Weight(font, 400), S_OK);
    TEQ(IFont_get_Bold(font, &b), S_OK);
    CHECK(b == FALSE);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_weight_charset_via_invoke)
{
    FONT_SETUP(font, tl, ifti, fdti);
    VARIANT arg, res; UINT ae;
    VariantInit(&arg);
    V_VT(&arg) = VT_I2; V_I2(&arg) = 700;
    TEQ(inv_put(ifti, font, id_of(ifti, L"Weight"), &arg, &ae), S_OK);
    TEQ(inv_get(ifti, font, id_of(ifti, L"Weight"), &res), S_OK);
    WCHECK(V_VT(&res) == VT_I2 && V_I2(&res) == 700);
    V_VT(&arg) = VT_I2; V_I2(&arg) = 2;                       /* SYMBOL_CHARSET */
    TEQ(inv_put(ifti, font, id_of(ifti, L"Charset"), &arg, &ae), S_OK);
    TEQ(inv_get(ifti, font, id_of(ifti, L"Charset"), &res), S_OK);
    WCHECK(V_VT(&res) == VT_I2 && V_I2(&res) == 2);
    /* a VT_I4 argument is coerced to the declared short parameter (0 = ANSI_CHARSET, accepted by the default face) */
    V_VT(&arg) = VT_I4; V_I4(&arg) = 0;
    TEQ(inv_put(ifti, font, id_of(ifti, L"Charset"), &arg, &ae), S_OK);
    TEQ(inv_get(ifti, font, id_of(ifti, L"Charset"), &res), S_OK);
    WCHECK(V_VT(&res) == VT_I2 && V_I2(&res) == 0);
    /* Windows silently ignores charsets the current face cannot supply (3 here): put succeeds, the value is unchanged */
    V_VT(&arg) = VT_I4; V_I4(&arg) = 3;
    TEQ(inv_put(ifti, font, id_of(ifti, L"Charset"), &arg, &ae), S_OK);
    TEQ(inv_get(ifti, font, id_of(ifti, L"Charset"), &res), S_OK);
    t_note("Charset after put 3 reads %d", V_VT(&res) == VT_I2 ? V_I2(&res) : -1);
    WCHECK(V_VT(&res) == VT_I2 && V_I2(&res) == 0);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_invoke_type_mismatch)
{
    FONT_SETUP(font, tl, ifti, fdti);
    VARIANT arg; UINT ae;
    VariantInit(&arg);
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = mkbstr(L"not a bool");
    HRESULT hr = inv_put(ifti, font, id_of(ifti, L"Bold"), &arg, &ae);
    TEQ(hr, DISP_E_TYPEMISMATCH);
    WCHECK(ae == 0);                                          /* index of the offending argument */
    VariantClear(&arg);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c test_TypeInfo (LGPL-2.1+) */
TEST(tlli_font_invoke_error_codes)
{
    FONT_SETUP(font, tl, ifti, fdti);
    DISPPARAMS dp; HRESULT hr; DISPID clone = id_of(ifti, L"Clone");
    memset(&dp, 0, sizeof dp);
    /* invalid member id */
    hr = ITypeInfo_Invoke(ifti, font, 0x12345678, DISPATCH_PROPERTYGET, &dp, NULL, NULL, NULL);
    TEQ(hr, DISP_E_MEMBERNOTFOUND);
    hr = ITypeInfo_Invoke(ifti, font, 0x12345678, DISPATCH_METHOD, &dp, NULL, NULL, NULL);
    TEQ(hr, DISP_E_MEMBERNOTFOUND);
    /* cNamedArgs > cArgs */
    dp.cNamedArgs = 1;
    hr = ITypeInfo_Invoke(ifti, font, 0x12345678, DISPATCH_PROPERTYGET, &dp, NULL, NULL, NULL);
    TEQ(hr, E_INVALIDARG);
    hr = ITypeInfo_Invoke(ifti, font, 0x12345678, DISPATCH_METHOD, &dp, NULL, NULL, NULL);
    TEQ(hr, E_INVALIDARG);
    /* a real method with the wrong flags / wrong argument counts */
    dp.cNamedArgs = 0;
    hr = ITypeInfo_Invoke(ifti, font, clone, DISPATCH_PROPERTYGET, &dp, NULL, NULL, NULL);
    TEQ(hr, DISP_E_MEMBERNOTFOUND);
    hr = ITypeInfo_Invoke(ifti, font, clone, DISPATCH_METHOD, &dp, NULL, NULL, NULL);
    TEQ(hr, DISP_E_BADPARAMCOUNT);
    dp.cNamedArgs = 1;
    hr = ITypeInfo_Invoke(ifti, font, clone, DISPATCH_PROPERTYGET, &dp, NULL, NULL, NULL);
    TEQ(hr, E_INVALIDARG);
    hr = ITypeInfo_Invoke(ifti, font, clone, DISPATCH_METHOD, &dp, NULL, NULL, NULL);
    TEQ(hr, E_INVALIDARG);
    /* NULL DISPPARAMS */
    hr = ITypeInfo_Invoke(ifti, font, clone, DISPATCH_PROPERTYGET, NULL, NULL, NULL, NULL);
    TEQ(hr, E_INVALIDARG);
    hr = ITypeInfo_Invoke(ifti, font, clone, DISPATCH_METHOD, NULL, NULL, NULL, NULL);
    TEQ(hr, E_INVALIDARG);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_invoke_wrong_flags_for_property)
{
    FONT_SETUP(font, tl, ifti, fdti);
    DISPPARAMS dp = {0}; VARIANT res; VariantInit(&res);
    DISPID name = id_of(ifti, L"Name");
    /* a property is not callable as a plain method on a vtable interface... */
    HRESULT hr = ITypeInfo_Invoke(ifti, font, name, DISPATCH_METHOD, &dp, &res, NULL, NULL);
    t_note("IFont::Name invoked with DISPATCH_METHOD -> 0x%08lx", (unsigned long)hr);
    CHECK(hr == DISP_E_MEMBERNOTFOUND || hr == S_OK);
    VariantClear(&res);
    /* ...but METHOD|PROPERTYGET is accepted */
    memset(&res, 0, sizeof res);
    hr = ITypeInfo_Invoke(ifti, font, name, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
    TEQ(hr, S_OK);
    TEQ(V_VT(&res), VT_BSTR);
    VariantClear(&res);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_clone_and_isequal)
{
    FONT_SETUP(font, tl, ifti, fdti);
    CY want; want.int64 = 120000;
    TEQ(IFont_put_Size(font, want), S_OK);
    TEQ(IFont_put_Italic(font, FALSE), S_OK);
    IFont *clone = NULL;
    TEQ(IFont_Clone(font, &clone), S_OK);
    CHECK(clone != NULL && clone != font);
    if (clone) {
        CY sz; sz.int64 = 0;
        TEQ(IFont_get_Size(clone, &sz), S_OK);
        TEQ(IFont_get_Size(font, &want), S_OK);
        CHECK(sz.int64 == want.int64);
        TEQ(IFont_IsEqual(font, clone), S_OK);
        TEQ(IFont_put_Italic(clone, TRUE), S_OK);
        TEQ(IFont_IsEqual(font, clone), S_FALSE);
        BOOL it = TRUE;
        TEQ(IFont_get_Italic(font, &it), S_OK);
        CHECK(it == FALSE);                                    /* the clone is independent */
        IFont_Release(clone);
    }
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_invoke_clone_method)
{
    FONT_SETUP(font, tl, ifti, fdti);
    VARIANT args[1], res; DISPPARAMS dp; UINT ae = 0;
    IFont *clone = NULL;
    VariantInit(&args[0]);
    V_VT(&args[0]) = VT_BYREF | VT_UNKNOWN;                   /* [out] IFont** */
    V_BYREF(&args[0]) = &clone;
    memset(&dp, 0, sizeof dp);
    dp.cArgs = 1; dp.rgvarg = args;
    VariantInit(&res);
    HRESULT hr = ITypeInfo_Invoke(ifti, font, id_of(ifti, L"Clone"), DISPATCH_METHOD, &dp, &res, NULL, &ae);
    t_note("Invoke(Clone) -> 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) { CHECK(clone != NULL); if (clone) IFont_Release(clone); }
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_independent_instances)
{
    FONT_SETUP(font, tl, ifti, fdti);
    IFont *other = make_font();
    REQUIRE(other);
    TEQ(IFont_put_Italic(font, TRUE), S_OK);
    TEQ(IFont_put_Italic(other, FALSE), S_OK);
    BOOL a = FALSE, b = TRUE;
    TEQ(IFont_get_Italic(font, &a), S_OK);
    TEQ(IFont_get_Italic(other, &b), S_OK);
    CHECK(a != FALSE && b == FALSE);
    IFont_Release(other);
    FONT_TEARDOWN(font, tl, ifti, fdti);
}

TEST(tlli_font_typeinfo_invoke_matches_vtable_defaults)
{
    FONT_SETUP(font, tl, ifti, fdti);
    VARIANT res;
    BSTR name = NULL;
    TEQ(IFont_get_Name(font, &name), S_OK);
    TEQ(inv_get(ifti, font, id_of(ifti, L"Name"), &res), S_OK);
    CHECK(V_VT(&res) == VT_BSTR && name && V_BSTR(&res) && !lstrcmpW(name, V_BSTR(&res)));
    t_note("StdFont default name: %ls", name ? name : L"(null)");
    VariantClear(&res); tl_freebstr(name);
    CY sz; sz.int64 = 0;
    TEQ(IFont_get_Size(font, &sz), S_OK);
    t_note("StdFont default size (CY): %lld", sz.int64);
    TEQ(inv_get(ifti, font, id_of(ifti, L"Size"), &res), S_OK);
    CHECK(V_VT(&res) == VT_CY && V_CY(&res).int64 == sz.int64);
    SHORT w = 0, cs = -1; BOOL bold = 5;
    TEQ(IFont_get_Weight(font, &w), S_OK);
    TEQ(IFont_get_Charset(font, &cs), S_OK);
    TEQ(IFont_get_Bold(font, &bold), S_OK);
    t_note("StdFont default weight=%d charset=%d bold=%d", w, cs, bold);
    CHECK((w >= 550) == (bold != FALSE));
    FONT_TEARDOWN(font, tl, ifti, fdti);
}
