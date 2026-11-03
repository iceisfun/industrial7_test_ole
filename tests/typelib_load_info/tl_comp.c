/* ITypeComp::Bind / BindType, LHashValOfNameSys, ITypeInfo2 / ITypeLib2 extras. */
#include "tl.h"

#define SENT_PTR ((void *)(ULONG_PTR)0x1badf00d)

static ULONG hash_of(const WCHAR *name)
{
    ULONG (WINAPI *h)(SYSKIND, LCID, LPOLESTR) = (void *)t_proc_in("oleaut32.dll", "LHashValOfNameSys");
    return h ? h(SYS_WIN32, 0, (LPOLESTR)name) : 0;
}

struct bres { ITypeInfo *ti; DESCKIND dk; BINDPTR bp; HRESULT hr; };

static void do_bind(ITypeComp *tc, const WCHAR *name, WORD flags, struct bres *r)
{
    WCHAR buf[128]; lstrcpyW(buf, name);
    r->ti = (ITypeInfo *)SENT_PTR; r->dk = (DESCKIND)0x77; r->bp.lptcomp = (ITypeComp *)SENT_PTR;
    r->hr = ITypeComp_Bind(tc, buf, hash_of(buf), flags, &r->ti, &r->dk, &r->bp);
}

static void free_bres(struct bres *r)
{
    if (r->hr != S_OK) return;
    if (r->dk == DESCKIND_FUNCDESC && r->ti && r->bp.lpfuncdesc) ITypeInfo_ReleaseFuncDesc(r->ti, r->bp.lpfuncdesc);
    else if (r->dk == DESCKIND_VARDESC && r->ti && r->bp.lpvardesc) ITypeInfo_ReleaseVarDesc(r->ti, r->bp.lpvardesc);
    else if (r->dk == DESCKIND_TYPECOMP && r->bp.lptcomp) ITypeComp_Release(r->bp.lptcomp);
    if (r->ti && r->ti != SENT_PTR) ITypeInfo_Release(r->ti);
}

static void expect_none(const struct bres *r, const char *what)
{
    CHECKF(r->hr == S_OK, "%s hr=0x%08lx", what, (unsigned long)r->hr);
    CHECKF(r->dk == DESCKIND_NONE, "%s desckind %d", what, r->dk);
    CHECKF(r->ti == NULL, "%s ti not cleared", what);
    CHECKF(r->bp.lptcomp == NULL, "%s bindptr not cleared", what);
}

TEST(tlli_hash_case_insensitive)
{
    ULONG (WINAPI *h)(SYSKIND, LCID, LPOLESTR) = UIN("oleaut32.dll", LHashValOfNameSys);
    REQUIRE(h);
    WCHAR a[] = L"IUnknown", b[] = L"iunknown", c[] = L"IUNKNOWN", d[] = L"IDispatch";
    ULONG ha = h(SYS_WIN32, 0, a), hb = h(SYS_WIN32, 0, b), hc = h(SYS_WIN32, 0, c);
    CHECK(ha == hb && hb == hc);
    CHECK(ha != h(SYS_WIN32, 0, d));
    TEQ(h(SYS_WIN32, 0, a), ha);                              /* deterministic */
    t_note("LHashValOfNameSys(WIN32,0,IUnknown)=0x%08lx (WIN64: 0x%08lx)", ha, h(SYS_WIN64, 0, a));
    CHECK(h(SYS_WIN32, 0x409, a) != 0 || ha == 0);
}

TEST(tlli_hash_distinguishes_names)
{
    ULONG (WINAPI *h)(SYSKIND, LCID, LPOLESTR) = UIN("oleaut32.dll", LHashValOfNameSys);
    REQUIRE(h);
    static const WCHAR *names[] = { L"Name", L"Size", L"Bold", L"Italic", L"Underline", L"Strikethrough", L"Weight", L"Charset",
        L"Clone", L"IsEqual", L"SetRatio", L"AddRef", L"Release", L"QueryInterface", L"GetTypeInfo", L"Invoke" };
    int distinct = 0;
    for (unsigned i = 0; i < 16; i++) {
        int dup = 0;
        WCHAR bi[64]; lstrcpyW(bi, names[i]);
        for (unsigned j = 0; j < i; j++) {
            WCHAR bj[64]; lstrcpyW(bj, names[j]);
            if (h(SYS_WIN32, 0, bi) == h(SYS_WIN32, 0, bj)) dup = 1;
        }
        if (!dup) distinct++;
    }
    CHECK(distinct >= 14);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c test_TypeComp (LGPL-2.1+) */
TEST(tlli_tc_lib_bind_module_and_functions)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeComp *tc = NULL;
    TEQ(ITypeLib_GetTypeComp(tl, &tc), S_OK);
    REQUIRE(tc);
    struct bres r;
    /* a module name binds to the module's own ITypeComp (no typeinfo) */
    do_bind(tc, L"StdFunctions", 0, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_TYPECOMP); CHECK(r.ti == NULL); CHECK(r.bp.lptcomp != NULL);
    free_bres(&r);
    do_bind(tc, L"StdFunctions", INVOKE_PROPERTYGET, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_TYPECOMP); CHECK(r.ti == NULL);
    free_bres(&r);
    /* a function of the module is found directly */
    do_bind(tc, L"SavePicture", 0, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    CHECK(r.ti != NULL); CHECK(r.bp.lpfuncdesc != NULL);
    if (r.hr == S_OK && r.bp.lpfuncdesc) { TEQ(r.bp.lpfuncdesc->funckind, FUNC_STATIC); TEQ(r.bp.lpfuncdesc->invkind, INVOKE_FUNC); }
    free_bres(&r);
    do_bind(tc, L"LoadPicture", INVOKE_FUNC, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    free_bres(&r);
    /* wrong invoke kind: type mismatch, everything cleared */
    do_bind(tc, L"SavePicture", INVOKE_PROPERTYGET, &r);
    TEQ(r.hr, TYPE_E_TYPEMISMATCH);
    TEQ(r.dk, DESCKIND_NONE); CHECK(r.ti == NULL); CHECK(r.bp.lptcomp == NULL);
    ITypeComp_Release(tc);
    ITypeLib_Release(tl);
}

TEST(tlli_tc_lib_bind_enum_and_values)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeComp *tc = NULL;
    TEQ(ITypeLib_GetTypeComp(tl, &tc), S_OK);
    REQUIRE(tc);
    struct bres r;
    do_bind(tc, L"OLE_TRISTATE", 0, &r);                 /* enum name -> its ITypeComp */
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_TYPECOMP); CHECK(r.ti == NULL);
    free_bres(&r);
    static const struct { const WCHAR *n; int v; } e[] = { {L"Unchecked",0}, {L"Checked",1}, {L"Gray",2}, {L"Monochrome",1}, {L"VgaColor",2}, {L"Color",4}, {L"Default",0} };
    for (unsigned i = 0; i < 7; i++) {
        do_bind(tc, e[i].n, 0, &r);
        CHECKF(r.hr == S_OK && r.dk == DESCKIND_VARDESC, "%ls hr=0x%08lx dk=%d", e[i].n, (unsigned long)r.hr, r.dk);
        if (r.hr == S_OK && r.dk == DESCKIND_VARDESC && r.bp.lpvardesc) {
            TEQ(r.bp.lpvardesc->varkind, VAR_CONST);
            if (r.bp.lpvardesc->lpvarValue) TEQ(V_I4(r.bp.lpvardesc->lpvarValue), e[i].v);
        }
        free_bres(&r);
    }
    do_bind(tc, L"unchecked", 0, &r);                    /* case-insensitive */
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_VARDESC);
    free_bres(&r);
    ITypeComp_Release(tc);
    ITypeLib_Release(tl);
}

TEST(tlli_tc_lib_bind_types_are_not_members)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeComp *tc = NULL;
    TEQ(ITypeLib_GetTypeComp(tl, &tc), S_OK);
    REQUIRE(tc);
    static const WCHAR *names[] = { L"IUnknown", L"Font", L"GUID", L"OLE_COLOR", L"StdPicture", L"StdFont", L"IFont", L"IPicture", L"olet_no_such_member" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        struct bres r;
        do_bind(tc, names[i], 0, &r);
        expect_none(&r, "type name via Bind");
    }
    ITypeComp_Release(tc);
    ITypeLib_Release(tl);
}

TEST(tlli_tc_lib_bindtype)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeComp *tc = NULL;
    TEQ(ITypeLib_GetTypeComp(tl, &tc), S_OK);
    REQUIRE(tc);
    WCHAR guid[] = L"GUID", guidl[] = L"guid";
    ULONG h = hash_of(guid);
    ITypeInfo *ti = SENT_PTR; ITypeComp *c2 = SENT_PTR;
    /* adapted from Wine: argument validation */
    if (!t_is_wine()) {
        TEQ(ITypeComp_BindType(tc, guid, h, NULL, NULL), E_INVALIDARG);
        ti = SENT_PTR;
        TEQ(ITypeComp_BindType(tc, guid, h, &ti, NULL), E_INVALIDARG);
        CHECK(ti == SENT_PTR);
        c2 = SENT_PTR;
        TEQ(ITypeComp_BindType(tc, guid, h, NULL, &c2), E_INVALIDARG);
        CHECK(c2 == SENT_PTR);
        ti = SENT_PTR; c2 = SENT_PTR;
        TEQ(ITypeComp_BindType(tc, NULL, h, &ti, &c2), E_INVALIDARG);
        CHECK(ti == SENT_PTR && c2 == SENT_PTR);
    }
    ti = SENT_PTR; c2 = SENT_PTR;
    TEQ(ITypeComp_BindType(tc, guid, h, &ti, &c2), S_OK);
    CHECK(ti != NULL && ti != SENT_PTR);
    WCHECK(c2 == NULL);
    if (ti && ti != SENT_PTR) {
        BSTR nm = NULL;
        ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &nm, NULL, NULL, NULL);
        CHECK(nm && tl_weq(nm, L"GUID"));
        tl_freebstr(nm);
        ITypeInfo_Release(ti);
    }
    if (c2 && c2 != SENT_PTR) ITypeComp_Release(c2);
    /* case-insensitive */
    ti = SENT_PTR; c2 = SENT_PTR;
    TEQ(ITypeComp_BindType(tc, guidl, hash_of(guidl), &ti, &c2), S_OK);
    CHECK(ti != NULL && ti != SENT_PTR);
    if (ti && ti != SENT_PTR) ITypeInfo_Release(ti);
    if (c2 && c2 != SENT_PTR) ITypeComp_Release(c2);
    /* BindType finds interface and coclass names too */
    static const WCHAR *tn[] = { L"IUnknown", L"IFont", L"StdFont", L"OLE_COLOR", L"Font", L"OLE_TRISTATE", L"StdFunctions" };
    for (unsigned i = 0; i < 7; i++) {
        WCHAR b[64]; lstrcpyW(b, tn[i]);
        ti = SENT_PTR; c2 = SENT_PTR;
        HRESULT hr = ITypeComp_BindType(tc, b, hash_of(b), &ti, &c2);
        CHECKF(hr == S_OK && ti && ti != SENT_PTR, "%ls hr=0x%08lx", tn[i], (unsigned long)hr);
        if (ti && ti != SENT_PTR) ITypeInfo_Release(ti);
        if (c2 && c2 != SENT_PTR) ITypeComp_Release(c2);
    }
    /* unknown type name: success with both outputs NULL */
    WCHAR junk[] = L"olet_no_such_type";
    ti = SENT_PTR; c2 = SENT_PTR;
    HRESULT hr = ITypeComp_BindType(tc, junk, hash_of(junk), &ti, &c2);
    t_note("BindType(unknown) -> 0x%08lx ti=%p tc=%p", (unsigned long)hr, ti, c2);
    CHECK(hr == S_OK || hr == TYPE_E_ELEMENTNOTFOUND);
    if (hr == S_OK) CHECK(ti == NULL);
    ITypeComp_Release(tc);
    ITypeLib_Release(tl);
}

TEST(tlli_tc_interface_bind)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ifont = tl_get_named(tl, L"IFont");
    REQUIRE(ifont);
    ITypeComp *tc = NULL;
    TEQ(ITypeInfo_GetTypeComp(ifont, &tc), S_OK);
    REQUIRE(tc);
    struct bres r;
    do_bind(tc, L"Clone", 0, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    if (r.hr == S_OK && r.bp.lpfuncdesc) TEQ(r.bp.lpfuncdesc->invkind, INVOKE_FUNC);
    free_bres(&r);
    do_bind(tc, L"Clone", INVOKE_PROPERTYGET, &r);
    TEQ(r.hr, TYPE_E_TYPEMISMATCH);
    TEQ(r.dk, DESCKIND_NONE); CHECK(r.ti == NULL); CHECK(r.bp.lptcomp == NULL);
    do_bind(tc, L"clone", 0, &r);                              /* case-insensitive */
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    free_bres(&r);
    do_bind(tc, L"Junk", 0, &r);
    expect_none(&r, "Junk");
    /* inherited members are found in the base interface */
    do_bind(tc, L"AddRef", 0, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    CHECK(r.ti != NULL);
    if (r.hr == S_OK && r.bp.lpfuncdesc) TEQ(r.bp.lpfuncdesc->memid, 0x60000001);
    if (r.hr == S_OK && r.ti) {
        TYPEATTR *ta = NULL;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(r.ti, &ta))) { CHECK(tl_guid_eq(&ta->guid, &TL_IID_IUnknown)); ITypeInfo_ReleaseTypeAttr(r.ti, ta); }
    }
    free_bres(&r);
    ITypeComp_Release(tc);
    ITypeInfo_Release(ifont);
    ITypeLib_Release(tl);
}

TEST(tlli_tc_interface_bind_property_kinds)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ifont = tl_get_named(tl, L"IFont");
    REQUIRE(ifont);
    ITypeComp *tc = NULL;
    TEQ(ITypeInfo_GetTypeComp(ifont, &tc), S_OK);
    REQUIRE(tc);
    struct bres r;
    do_bind(tc, L"Name", INVOKE_PROPERTYGET, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    if (r.hr == S_OK && r.bp.lpfuncdesc) TEQ(r.bp.lpfuncdesc->invkind, INVOKE_PROPERTYGET);
    free_bres(&r);
    do_bind(tc, L"Name", INVOKE_PROPERTYPUT, &r);
    TEQ(r.hr, S_OK); TEQ(r.dk, DESCKIND_FUNCDESC);
    if (r.hr == S_OK && r.bp.lpfuncdesc) TEQ(r.bp.lpfuncdesc->invkind, INVOKE_PROPERTYPUT);
    free_bres(&r);
    do_bind(tc, L"hFont", INVOKE_PROPERTYPUT, &r);           /* hFont has no setter */
    t_note("Bind(hFont, PROPERTYPUT) -> hr=0x%08lx dk=%d", (unsigned long)r.hr, r.dk);
    CHECK(r.hr == TYPE_E_TYPEMISMATCH || (r.hr == S_OK && r.dk == DESCKIND_NONE));
    free_bres(&r);
    ITypeComp_Release(tc);
    ITypeInfo_Release(ifont);
    ITypeLib_Release(tl);
}

/* ---- ITypeInfo2 ---- */

static ITypeInfo2 *ti2_of(ITypeLib *tl, const WCHAR *name)
{
    ITypeInfo *ti = tl_get_named(tl, name); ITypeInfo2 *t2 = NULL;
    if (!ti) return NULL;
    ITypeInfo_QueryInterface(ti, &TL_IID_ITypeInfo2, (void **)&t2);
    ITypeInfo_Release(ti);
    return t2;
}

TEST(tlli_t2_kind_and_flags_match_typeattr)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; ITypeInfo2 *t2 = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) continue;
        TEQ(ITypeInfo_QueryInterface(ti, &TL_IID_ITypeInfo2, (void **)&t2), S_OK);
        if (t2 && SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            TYPEKIND k = (TYPEKIND)99; ULONG fl = 0xdead;
            TEQ(ITypeInfo2_GetTypeKind(t2, &k), S_OK);
            TEQ(ITypeInfo2_GetTypeFlags(t2, &fl), S_OK);
            CHECKF(k == ta->typekind, "type %u kind", i);
            CHECKF(fl == ta->wTypeFlags, "type %u flags 0x%lx vs 0x%x", i, fl, ta->wTypeFlags);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        if (t2) ITypeInfo2_Release(t2);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_t2_func_index_of_memid)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo2 *t2 = ti2_of(tl, L"IUnknown");
    REQUIRE(t2);
    UINT idx = 77;
    for (UINT i = 0; i < 3; i++) {
        idx = 77;
        TEQ(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x60000000 + i, INVOKE_FUNC, &idx), S_OK);
        TEQ(idx, i);
    }
    idx = 77;
    TEQ(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x60000000, INVOKE_PROPERTYGET, &idx), TYPE_E_ELEMENTNOTFOUND);
    TEQ(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x12345, INVOKE_FUNC, &idx), TYPE_E_ELEMENTNOTFOUND);
    TEQ(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x60000003, INVOKE_FUNC, &idx), TYPE_E_ELEMENTNOTFOUND);
    TEQ(idx, 77);
    ITypeInfo2_Release(t2);
}

TEST(tlli_t2_func_index_of_property_pairs)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = tl_get_named(tl, L"IFont");
    REQUIRE(ti);
    ITypeInfo2 *t2 = NULL;
    TEQ(ITypeInfo_QueryInterface(ti, &TL_IID_ITypeInfo2, (void **)&t2), S_OK);
    TYPEATTR *ta = NULL;
    REQUIRE(t2 && SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta)));
    for (UINT i = 0; i < ta->cFuncs; i++) {
        FUNCDESC *fd = NULL; UINT idx = 77;
        if (FAILED(ITypeInfo_GetFuncDesc(ti, i, &fd)) || !fd) continue;
        TEQ(ITypeInfo2_GetFuncIndexOfMemId(t2, fd->memid, fd->invkind, &idx), S_OK);
        CHECKF(idx == i, "func %u maps to %u", i, idx);
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    ITypeInfo2_Release(t2);
    ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

TEST(tlli_t2_var_index_of_memid)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = tl_get_named(tl, L"OLE_TRISTATE");
    REQUIRE(ti);
    ITypeInfo2 *t2 = NULL;
    TEQ(ITypeInfo_QueryInterface(ti, &TL_IID_ITypeInfo2, (void **)&t2), S_OK);
    REQUIRE(t2);
    for (UINT i = 0; i < 3; i++) {
        VARDESC *vd = NULL; UINT idx = 77;
        TEQ(ITypeInfo_GetVarDesc(ti, i, &vd), S_OK);
        if (!vd) continue;
        TEQ(ITypeInfo2_GetVarIndexOfMemId(t2, vd->memid, &idx), S_OK);
        TEQ(idx, i);
        ITypeInfo_ReleaseVarDesc(ti, vd);
    }
    UINT idx = 77;
    TEQ(ITypeInfo2_GetVarIndexOfMemId(t2, 0x7fff0000, &idx), TYPE_E_ELEMENTNOTFOUND);
    ITypeInfo2_Release(t2);
    ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c test_TypeInfo (LGPL-2.1+) */
TEST(tlli_t2_custdata_unknown_guid)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo2 *t2 = ti2_of(tl, L"IDispatch");
    REQUIRE(t2);
    VARIANT v; VariantInit(&v); V_VT(&v) = VT_I4;
    TEQ(ITypeInfo2_GetCustData(t2, &TL_GUID_NONE, &v), S_OK);
    CHECK(V_VT(&v) == VT_EMPTY);
    VariantClear(&v);
    CUSTDATA cd; memset(&cd, 0xcc, sizeof cd);
    HRESULT hr = ITypeInfo2_GetAllCustData(t2, &cd);
    TEQ(hr, S_OK);
    if (hr == S_OK) { t_note("IDispatch custom data count %lu", cd.cCustData); }
    ITypeInfo2_Release(t2);
    ITypeLib_Release(tl);
}

TEST(tlli_t2_documentation2)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo2 *t2 = ti2_of(tl, L"IFont");
    REQUIRE(t2);
    BSTR doc = NULL, hf = NULL; DWORD ctx = 0x5555;
    HRESULT hr = ITypeInfo2_GetDocumentation2(t2, MEMBERID_NIL, 0x409, &doc, &ctx, &hf);
    TEQ(hr, S_OK);
    t_note("IFont helpstring2=%ls ctx=0x%lx", doc ? doc : L"(null)", ctx);
    tl_freebstr(doc); tl_freebstr(hf);
    doc = NULL;
    TEQ(ITypeInfo2_GetDocumentation2(t2, MEMBERID_NIL, 0x409, NULL, NULL, NULL), S_OK);
    TEQ(ITypeInfo2_GetDocumentation2(t2, 0x40404040, 0x409, &doc, NULL, NULL), TYPE_E_ELEMENTNOTFOUND);
    ITypeInfo2_Release(t2);
    ITypeLib_Release(tl);
}

TEST(tlli_t2_gettypeidx_via_typelib)
{
    /* ITypeLib2 on the library; ITypeInfo2::GetContainingTypeLib index equals position */
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo2 *t2 = ti2_of(tl, L"StdPicture");
    REQUIRE(t2);
    ITypeLib *c = NULL; UINT idx = 0;
    TEQ(ITypeInfo2_GetContainingTypeLib(t2, &c, &idx), S_OK);
    int want = tl_find_index(tl, L"StdPicture");
    TEQ(idx, want);
    CHECK(c == tl);
    if (c) ITypeLib_Release(c);
    ITypeInfo2_Release(t2);
    ITypeLib_Release(tl);
}

TEST(tlli_t2_per_member_custdata_shapes)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo2 *t2 = ti2_of(tl, L"IUnknown");
    REQUIRE(t2);
    CUSTDATA cd;
    memset(&cd, 0xcc, sizeof cd);
    HRESULT hr = ITypeInfo2_GetAllFuncCustData(t2, 0, &cd);
    t_note("GetAllFuncCustData(0) -> 0x%08lx count=%lu", (unsigned long)hr, hr == S_OK ? cd.cCustData : 0);
    CHECK(hr == S_OK);
    VARIANT v; VariantInit(&v);
    hr = ITypeInfo2_GetFuncCustData(t2, 0, &TL_GUID_NONE, &v);
    CHECK(hr == S_OK);
    if (hr == S_OK) CHECK(V_VT(&v) == VT_EMPTY);
    memset(&cd, 0xcc, sizeof cd);
    hr = ITypeInfo2_GetAllFuncCustData(t2, 3, &cd);          /* out of range */
    CHECK(FAILED(hr));
    ITypeInfo2_Release(t2);
    ITypeLib_Release(tl);
}
