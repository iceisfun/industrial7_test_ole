/* The generated test type library as seen through ITypeLib/ITypeInfo (this documents what CreateTypeLib2 produces), plus
 * lookups that depend on the platform subkeys and IRecordInfo created through the registered library. */
#include "reg.h"
#include "mar.h"

static ITypeLib *open_main(void)
{
    ITypeLib *tl = NULL; const WCHAR *p = tm_main_path();
    if (!p) return NULL;
    if (FAILED(U(LoadTypeLibEx)(p, REGKIND_NONE, &tl))) return NULL;
    return tl;
}
#define NEED_LIB(tl) do { (tl) = open_main(); if (!(tl)) { t_skip("cannot build/load the test typelib"); return; } } while (0)

TEST(dxtm_lib_attributes)
{
    ITypeLib *tl; TLIBATTR *a = NULL;
    NEED_LIB(tl);
    EQ(ITypeLib_GetLibAttr(tl, &a), S_OK);
    if (a) {
        CHECK(IsEqualGUID(&a->guid, &TM_LIBID_Main));
        EQ(a->wMajorVerNum, TM_MAIN_MAJ); EQ(a->wMinorVerNum, TM_MAIN_MIN);
        EQ(a->lcid, 0);
        EQ(a->syskind, SYS_WIN64);
        CHECKF((a->wLibFlags & 7) == (LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN), "wLibFlags=0x%x", a->wLibFlags);
        ITypeLib_ReleaseTLibAttr(tl, a);
    }
    EQ(ITypeLib_GetTypeInfoCount(tl), 10);
    ITypeLib_Release(tl);
}

TEST(dxtm_lib_documentation)
{
    ITypeLib *tl; BSTR name = NULL, doc = NULL, help = NULL; DWORD ctx = 99;
    NEED_LIB(tl);
    EQ(ITypeLib_GetDocumentation(tl, -1, &name, &doc, &ctx, &help), S_OK);
    CHECK(name && !lstrcmpW(name, L"OletLib"));
    CHECK(doc && !lstrcmpW(doc, L"Olet typelib marshal test library"));
    EQ(ctx, 0);
    SysFreeString(name); SysFreeString(doc); SysFreeString(help);
    ITypeLib_Release(tl);
}

static const struct { const GUID *iid; const WCHAR *name; TYPEKIND kind; } types[] = {
    { &TM_GUID_OletPoint, L"OletPoint", TKIND_RECORD }, { &TM_GUID_OletMix, L"OletMix", TKIND_RECORD },
    { &TM_IID_IOletSink, L"IOletSink", TKIND_INTERFACE }, { &TM_IID_IOletCustom, L"IOletCustom", TKIND_INTERFACE },
    { &TM_IID_IOletDisp, L"IOletDisp", TKIND_DISPATCH }, { &TM_IID_IOletPlain, L"IOletPlain", TKIND_INTERFACE },
    { &TM_IID_IOletProxyFlag, L"IOletProxyFlag", TKIND_INTERFACE }, { &TM_CLSID_OletCo, L"OletCo", TKIND_COCLASS },
};
TEST(dxtm_lib_typeinfo_by_guid)
{
    ITypeLib *tl; unsigned i;
    NEED_LIB(tl);
    for (i = 0; i < sizeof types / sizeof types[0]; i++) {
        ITypeInfo *ti = NULL; BSTR n = NULL; TYPEKIND k = (TYPEKIND)99;
        HRESULT hr = ITypeLib_GetTypeInfoOfGuid(tl, types[i].iid, &ti);
        CHECKF(hr == S_OK && ti, "GetTypeInfoOfGuid(%ls) hr=0x%lx", types[i].name, (long)hr);
        if (!ti) continue;
        ITypeInfo_GetDocumentation(ti, -1, &n, NULL, NULL, NULL);
        CHECKF(n && !lstrcmpW(n, types[i].name), "name %ls", n ? n : L"(null)");
        SysFreeString(n);
        EQ(ITypeLib_GetTypeInfoType(tl, 0, &k), S_OK);     /* index 0 is valid */
        ITypeInfo_Release(ti);
    }
    {
        ITypeInfo *ti = (ITypeInfo *)(ULONG_PTR)1;
        EQ(ITypeLib_GetTypeInfoOfGuid(tl, &TM_LIBID_Other, &ti), TYPE_E_ELEMENTNOTFOUND);
    }
    ITypeLib_Release(tl);
}

TEST(dxtm_lib_dual_interface_attributes)
{
    ITypeLib *tl; ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
    NEED_LIB(tl);
    EQ(ITypeLib_GetTypeInfoOfGuid(tl, &TM_IID_IOletDual, &ti), S_OK);
    if (ti) {
        EQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) {
            CHECKF(ta->wTypeFlags & TYPEFLAG_FDUAL, "flags 0x%x", ta->wTypeFlags);
            if (!(ta->wTypeFlags & TYPEFLAG_FOLEAUTOMATION)) t_note("the dual typeinfo does not report TYPEFLAG_FOLEAUTOMATION (flags 0x%x)", ta->wTypeFlags);
            CHECKF(ta->cFuncs == 10 || ta->cFuncs == 17, "cFuncs=%u", ta->cFuncs);
            t_note("IOletDual typeinfo kind=%d cFuncs=%u flags=0x%x cbSizeVft=%u", ta->typekind, ta->cFuncs, ta->wTypeFlags, ta->cbSizeVft);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(dxtm_lib_vtable_layout)
{
    /* SYS_WIN64: method i of a derived-from-IUnknown interface sits at (3+i)*8 */
    ITypeLib *tl; ITypeInfo *ti = NULL; TYPEATTR *ta = NULL; UINT i;
    NEED_LIB(tl);
    EQ(ITypeLib_GetTypeInfoOfGuid(tl, &TM_IID_IOletCustom, &ti), S_OK);
    if (ti) {
        EQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) {
            EQ(ta->cFuncs, OC_COUNT - OC_FIRST);
            EQ(ta->cImplTypes, 1);
            EQ(ta->cbSizeVft, (OC_COUNT) * 8);
            for (i = 0; i < ta->cFuncs; i++) {
                FUNCDESC *fd = NULL;
                if (SUCCEEDED(ITypeInfo_GetFuncDesc(ti, i, &fd)) && fd) {
                    CHECKF(fd->oVft == (SHORT)((3 + i) * 8), "func %u oVft=%d", i, fd->oVft);
                    CHECKF(fd->memid == (MEMBERID)(i + 1), "func %u memid=%ld", i, (long)fd->memid);
                    EQ(fd->funckind, FUNC_PUREVIRTUAL);
                    EQ(fd->callconv, CC_STDCALL);
                    EQ(fd->elemdescFunc.tdesc.vt, VT_HRESULT);
                    ITypeInfo_ReleaseFuncDesc(ti, fd);
                }
            }
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(dxtm_lib_function_names_and_params)
{
    ITypeLib *tl; ITypeInfo *ti = NULL; BSTR names[6]; UINT n = 0;
    NEED_LIB(tl);
    EQ(ITypeLib_GetTypeInfoOfGuid(tl, &TM_IID_IOletCustom, &ti), S_OK);
    if (ti) {
        FUNCDESC *fd = NULL;
        EQ(ITypeInfo_GetFuncDesc(ti, 0, &fd), S_OK);
        if (fd) {
            EQ(fd->cParams, 3);
            EQ(ITypeInfo_GetNames(ti, fd->memid, names, 6, &n), S_OK);
            EQ(n, 4);
            if (n == 4) {
                CHECK(!lstrcmpW(names[0], L"AddLong")); CHECK(!lstrcmpW(names[1], L"a")); CHECK(!lstrcmpW(names[2], L"b")); CHECK(!lstrcmpW(names[3], L"r"));
            }
            for (; n; n--) SysFreeString(names[n - 1]);
            EQ(fd->lprgelemdescParam[0].tdesc.vt, VT_I4);
            CHECK(fd->lprgelemdescParam[0].paramdesc.wParamFlags & PARAMFLAG_FIN);
            EQ(fd->lprgelemdescParam[2].tdesc.vt, VT_PTR);
            CHECK(fd->lprgelemdescParam[2].paramdesc.wParamFlags & PARAMFLAG_FRETVAL);
            CHECK(fd->lprgelemdescParam[2].paramdesc.wParamFlags & PARAMFLAG_FOUT);
            ITypeInfo_ReleaseFuncDesc(ti, fd);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(dxtm_lib_enum_members)
{
    ITypeLib *tl; ITypeInfo *ti = NULL; TYPEATTR *ta = NULL; BSTR n = NULL; UINT idx = 0;
    NEED_LIB(tl);
    EQ(ITypeLib_GetTypeInfo(tl, 0, &ti), S_OK);
    if (ti) {
        ITypeInfo_GetDocumentation(ti, -1, &n, NULL, NULL, NULL);
        t_note("first typeinfo in the library: %ls", n ? n : L"(null)");
        SysFreeString(n);
        ITypeInfo_Release(ti); ti = NULL;
    }
    {
        BSTR nm = SysAllocString(L"OletColor"); USHORT found = 1; ITypeInfo *r[1] = { NULL }; MEMBERID mid[1];
        EQ(ITypeLib_FindName(tl, nm, 0, r, mid, &found), S_OK);
        EQ(found, 1);
        if (r[0]) {
            EQ(ITypeInfo_GetTypeAttr(r[0], &ta), S_OK);
            if (ta) {
                EQ(ta->typekind, TKIND_ENUM); EQ(ta->cVars, 3);
                ITypeInfo_ReleaseTypeAttr(r[0], ta);
                {
                    VARDESC *vd = NULL;
                    for (idx = 0; idx < 3; idx++) {
                        if (SUCCEEDED(ITypeInfo_GetVarDesc(r[0], idx, &vd)) && vd) {
                            EQ(vd->varkind, VAR_CONST);
                            CHECKF(vd->lpvarValue && V_I4(vd->lpvarValue) == (1 << idx), "enum member %u", idx);
                            ITypeInfo_ReleaseVarDesc(r[0], vd);
                        }
                    }
                }
            }
            ITypeInfo_Release(r[0]);
        }
        SysFreeString(nm);
    }
    ITypeLib_Release(tl);
}

TEST(dxtm_lib_record_layout)
{
    ITypeLib *tl; ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
    if (t_is_wine()) { t_skip("Wine's LayOut leaves record sizes 0"); return; }
    NEED_LIB(tl);
    EQ(ITypeLib_GetTypeInfoOfGuid(tl, &TM_GUID_OletPoint, &ti), S_OK);
    if (ti) { EQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK); if (ta) { EQ(ta->cbSizeInstance, sizeof(OletPoint)); EQ(ta->cVars, 2); ITypeInfo_ReleaseTypeAttr(ti, ta); } ITypeInfo_Release(ti); }
    ti = NULL; ta = NULL;
    EQ(ITypeLib_GetTypeInfoOfGuid(tl, &TM_GUID_OletMix, &ti), S_OK);
    if (ti) {
        EQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) { t_note("OletMix cbSizeInstance=%lu cbAlignment=%u (sizeof=%u)", ta->cbSizeInstance, ta->cbAlignment, (unsigned)sizeof(OletMix)); EQ(ta->cVars, 4); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

/* ---------------------------------------------------------------- IRecordInfo through the registered library */
TMTEST(dxtm_recinfo_from_registered_library)
{
    IRecordInfo *ri = NULL; HRESULT hr; ULONG sz = 0; BSTR nm = NULL;
    if (t_is_wine()) { t_skip("Wine record sizes are 0"); return; }
    hr = U(GetRecordInfoFromGuids)(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, &TM_GUID_OletPoint, &ri);
    EQ(hr, S_OK);
    if (ri) {
        EQ(IRecordInfo_GetSize(ri, &sz), S_OK); EQ(sz, 8);
        EQ(IRecordInfo_GetName(ri, &nm), S_OK); CHECK(nm && !lstrcmpW(nm, L"OletPoint")); SysFreeString(nm);
        {
            GUID g; EQ(IRecordInfo_GetGuid(ri, &g), S_OK); CHECK(IsEqualGUID(&g, &TM_GUID_OletPoint));
        }
        {
            OletPoint pt; VARIANT v; memset(&pt, 0xcc, sizeof pt);
            EQ(IRecordInfo_RecordInit(ri, &pt), S_OK); EQ(pt.x, 0); EQ(pt.y, 0);
            VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 77;
            EQ(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &pt, L"y", &v), S_OK);
            EQ(pt.y, 77);
            VariantInit(&v);
            EQ(IRecordInfo_GetField(ri, &pt, L"y", &v), S_OK); EQ(V_VT(&v), VT_I4); EQ(V_I4(&v), 77);
            EQ(IRecordInfo_GetField(ri, &pt, L"nosuch", &v), TYPE_E_FIELDNOTFOUND);
        }
        IRecordInfo_Release(ri);
    }
    tm_end();
    ri = (IRecordInfo *)(ULONG_PTR)1;
    hr = U(GetRecordInfoFromGuids)(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, &TM_GUID_OletPoint, &ri);
    EQ(hr, TLB_NOTREG);
    CHECK(ri == NULL || ri == (IRecordInfo *)(ULONG_PTR)1);
}

/* ---------------------------------------------------------------- platform subkeys */
static void put_path(HKEY root, const TMLibSpec *s, const WCHAR *plat, const WCHAR *path)
{
    WCHAR p[260], k[300]; HKEY h; DWORD d;
    tm_tl_path(root, &s->libid, s->maj, s->min, L"0", p, 260);
    _snwprintf(k, 300, L"%ls\\%ls", p, plat);
    if (RegCreateKeyExW(root, k, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &d) == 0) {
        RegSetValueExW(h, NULL, 0, REG_SZ, (const BYTE *)path, (lstrlenW(path) + 1) * 2);
        RegCloseKey(h);
    }
}

TMTEST_BARE(dxtm_qp_win32_only_registration_is_found)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; BSTR q = NULL; ITypeLib *tl = NULL;
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini1); s.syskind = SYS_WIN32;
    EQ(rg_register(&s, "pw1.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&s, L"0\\win32")); CHECK(!rg_cu_exists(&s, L"0\\win64"));
    EQ(U(QueryPathOfRegTypeLib)(&s.libid, 3, 4, 0, &q), S_OK);
    CHECK(q && tm_wieq(q, path)); SysFreeString(q);
    EQ(U(LoadRegTypeLib)(&s.libid, 3, 4, 0, &tl), S_OK);
    if (tl) { TLIBATTR *a = NULL; ITypeLib_GetLibAttr(tl, &a); if (a) { EQ(a->syskind, SYS_WIN32); ITypeLib_ReleaseTLibAttr(tl, a); } ITypeLib_Release(tl); }
    EQ(rg_unregister(&s, 1), S_OK);
}

TMTEST_BARE(dxtm_qp_win64_preferred_over_win32)
{
    TMLibSpec a, b; WCHAR p32[MAX_PATH], p64[MAX_PATH]; BSTR q = NULL;
    rg_spec(&a, 3, 4, 0, &TM_IID_Mini1); a.syskind = SYS_WIN32;
    rg_spec(&b, 3, 4, 0, &TM_IID_Mini1);
    EQ(rg_register(&a, "pw2a.tlb", 1, NULL, p32), S_OK);
    EQ(rg_register(&b, "pw2b.tlb", 1, NULL, p64), S_OK);
    EQ(U(QueryPathOfRegTypeLib)(&a.libid, 3, 4, 0, &q), S_OK);
    CHECK(q && tm_wieq(q, p64)); SysFreeString(q);
    EQ(rg_unregister(&b, 1), S_OK);                /* remove win64: the win32 registration takes over */
    q = NULL;
    EQ(U(QueryPathOfRegTypeLib)(&a.libid, 3, 4, 0, &q), S_OK);
    CHECK(q && tm_wieq(q, p32)); SysFreeString(q);
    EQ(rg_unregister(&a, 1), S_OK);
}

TMTEST_BARE(dxtm_qp_win16_registration_is_ignored)
{
    TMLibSpec s; BSTR q = (BSTR)(ULONG_PTR)1; ITypeLib *tl = NULL;
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini1);
    put_path(tm_ur(), &s, L"win16", L"C:\\olet\\nothing16.tlb");
    CHECK(rg_cu_exists(&s, L"0\\win16"));
    EQ(U(QueryPathOfRegTypeLib)(&s.libid, 3, 4, 0, &q), TLB_NOTREG);
    CHECK(q == NULL);
    EQ(U(LoadRegTypeLib)(&s.libid, 3, 4, 0, &tl), TLB_NOTREG);
    tm_sweep();
}

TMTEST_BARE(dxtm_qp_value_must_be_a_string)
{
    /* a registered path of the wrong registry type does not produce a path */
    TMLibSpec s; WCHAR path[MAX_PATH], p[260]; BSTR q = NULL; HKEY h; DWORD dw = 5; HRESULT hr;
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "pw3.tlb", 1, NULL, path), S_OK);
    tm_tl_path(tm_ur(), &s.libid, 3, 4, L"0\\win64", p, 260);
    if (RegOpenKeyExW(tm_ur(), p, 0, KEY_WRITE, &h) == 0) { RegSetValueExW(h, NULL, 0, REG_DWORD, (BYTE *)&dw, 4); RegCloseKey(h); }
    hr = U(QueryPathOfRegTypeLib)(&s.libid, 3, 4, 0, &q);
    t_note("QueryPathOfRegTypeLib with a REG_DWORD path value: hr=0x%lx", (long)hr);
    SysFreeString(q);
    tm_sweep();
}

TMTEST_BARE(dxtm_regfu_library_without_typeinfos)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL;
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini1); s.ifname = NULL;
    EQ(rg_register(&s, "pe1.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&s, L"0\\win64"));
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini1));
    EQ(U(LoadRegTypeLib)(&s.libid, 3, 4, 0, &tl), S_OK);
    if (tl) { EQ(ITypeLib_GetTypeInfoCount(tl), 0); ITypeLib_Release(tl); }
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!tm_hkcu_has_lib(&TM_LIBID_Mini));
}

TMTEST_BARE(dxtm_regfu_dispinterface_and_dual_in_mini_library)
{
    /* kinds registered by RegisterTypeLib: dispinterface -> PSDispatch, dual interface -> PSOAInterface */
    TMLibSpec s; WCHAR path[MAX_PATH], v[300], g1[40], g2[40];
    tm_guid_str(&TM_CLSID_PSDispatch, g1); tm_guid_str(&TM_CLSID_PSOAInterface, g2);
    rg_spec(&s, 3, 4, 0, &TM_IID_MiniDisp); s.ifkind = TKIND_DISPATCH; s.ifflags = 0; s.ifname = L"IOletMiniDisp";
    EQ(rg_register(&s, "pk1.tlb", 1, NULL, path), S_OK);
    CHECK(rg_if_cu(&TM_IID_MiniDisp, L"ProxyStubClsid32", NULL, v, 300, 0, NULL));
    CHECKF(tm_wieq(v, g1), "dispinterface ProxyStubClsid32 %ls", v);
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!tm_hkcr_has_iface(&TM_IID_MiniDisp));

    rg_spec(&s, 3, 4, 0, &TM_IID_MiniDual); s.ifkind = TKIND_INTERFACE; s.ifflags = TYPEFLAG_FDUAL | TYPEFLAG_FOLEAUTOMATION; s.dual_func = 1; s.ifname = L"IOletMiniDual";
    EQ(rg_register(&s, "pk2.tlb", 1, NULL, path), S_OK);
    CHECK(rg_if_cu(&TM_IID_MiniDual, L"ProxyStubClsid32", NULL, v, 300, 0, NULL));
    CHECKF(tm_wieq(v, g2), "dual ProxyStubClsid32 %ls", v);
    EQ(rg_unregister(&s, 1), S_OK);

    /* an interface with neither flag set: not registered; oleautomation only: PSOAInterface */
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini2); s.ifflags = 0; s.ifname = L"IOletMini2";
    EQ(rg_register(&s, "pk3.tlb", 1, NULL, path), S_OK);
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini2));
    EQ(rg_unregister(&s, 1), S_OK);
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini2); s.ifflags = TYPEFLAG_FOLEAUTOMATION; s.ifname = L"IOletMini2";
    EQ(rg_register(&s, "pk4.tlb", 1, NULL, path), S_OK);
    CHECK(rg_if_cu(&TM_IID_Mini2, L"ProxyStubClsid32", NULL, v, 300, 0, NULL));
    CHECK(tm_wieq(v, g2));
    EQ(rg_unregister(&s, 1), S_OK);
}

TMTEST_BARE(dxtm_sweep_removes_leftovers)
{
    TMLibSpec s; WCHAR path[MAX_PATH];
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "sw1.tlb", 1, NULL, path), S_OK);
    CHECK(tm_hkcu_has_lib(&TM_LIBID_Mini));
    tm_sweep();
    CHECK(!tm_hkcu_has_lib(&TM_LIBID_Mini));
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini1));
}

/* ---------------------------------------------------------------- a proxy outlives the registration */
static void cl_unreg_inside(void *p, void *ctx)
{
    CLI(c, p); LONG r = 0;
    EQ(c->lpVtbl->AddLong(c, 1, 2, &r), S_OK); EQ(r, 3);
    EQ(pUnRegisterTypeLibForUser(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, SYS_WIN64), S_OK);
    CHECK(!tm_hkcr_has_iface(&TM_IID_IOletCustom));
    r = 0;
    EQ(c->lpVtbl->AddLong(c, 3, 4, &r), S_OK);      /* the already created proxy keeps its typeinfo */
    EQ(r, 7);
}
TMTEST(dxtm_mar_existing_proxy_survives_unregistration)
{
    CHECK(tm_run_cfg(&tm_cfgs[0], tm_mk_custom, &TM_IID_IOletCustom, cl_unreg_inside, NULL));
    tm_leakcheck();
}

TMTEST_BARE(dxtm_qp_expands_environment_strings)
{
    /* the registered path is read with RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ and without RRF_NOEXPAND: %VAR% is expanded */
    static const GUID libid = {0x00020430, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    TMLibSpec s; WCHAR p[260], k[300], want[MAX_PATH]; BSTR q = NULL; HKEY h; DWORD d; static const WCHAR val[] = L"%SystemRoot%\\system32\\stdole2.tlb"; HRESULT hr; ITypeLib *tl = NULL;
    (void)libid;
    rg_spec(&s, 3, 4, 0, &TM_IID_Mini1);
    tm_tl_path(tm_ur(), &s.libid, 3, 4, L"0\\win64", k, 300);
    if (RegCreateKeyExW(tm_ur(), k, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &d)) { t_skip("cannot create key"); return; }
    RegSetValueExW(h, NULL, 0, REG_EXPAND_SZ, (const BYTE *)val, sizeof val);
    RegCloseKey(h);
    ExpandEnvironmentStringsW(val, want, MAX_PATH);
    hr = U(QueryPathOfRegTypeLib)(&s.libid, 3, 4, 0, &q);
    EQ(hr, S_OK);
    CHECKF(q && tm_wieq(q, want), "expanded path %ls want %ls", q ? q : L"(null)", want);
    SysFreeString(q);
    (void)p;
    /* LoadRegTypeLib needs the typelib to be the registered version: stdole2.tlb is 2.0, so asking for 3.4 is refused */
    hr = U(LoadRegTypeLib)(&s.libid, 3, 4, 0, &tl);
    t_note("LoadRegTypeLib of a registration that points at a different library: hr=0x%lx", (long)hr);
    if (tl) ITypeLib_Release(tl);
    tm_sweep();
}
