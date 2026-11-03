/* ITypeInfo on stdole2.tlb types: type attributes, function/variable descriptions, names, hierarchy. */
#include "tl.h"

#define PTR ((UINT)sizeof(void *))
#define SENT_PTR ((void *)(ULONG_PTR)0x1badf00d)

static ITypeInfo *get_ti(ITypeLib *tl, const WCHAR *name)
{
    ITypeInfo *ti = tl_get_named(tl, name);
    return ti;
}

/* sum of cFuncs of all ancestors along the first-implemented-type chain */
static UINT inherited_funcs(ITypeInfo *ti)
{
    UINT total = 0;
    ITypeInfo *cur = ti; ITypeInfo_AddRef(cur);
    for (int depth = 0; depth < 16; depth++) {
        TYPEATTR *ta = NULL; HREFTYPE ref; ITypeInfo *next = NULL;
        if (FAILED(ITypeInfo_GetTypeAttr(cur, &ta))) break;
        UINT impl = ta->cImplTypes;
        ITypeInfo_ReleaseTypeAttr(cur, ta);
        if (!impl) break;
        if (FAILED(ITypeInfo_GetRefTypeOfImplType(cur, 0, &ref))) break;
        if (FAILED(ITypeInfo_GetRefTypeInfo(cur, ref, &next)) || !next) break;
        ITypeInfo_Release(cur); cur = next;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(cur, &ta))) { total += ta->cFuncs; ITypeInfo_ReleaseTypeAttr(cur, ta); }
    }
    ITypeInfo_Release(cur);
    return total;
}

#define WITH_LIB_AND(var, nm) \
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl); \
    ITypeInfo *var = get_ti(tl, nm); \
    if (!var) { CHECKF(0, "type %ls missing", nm); ITypeLib_Release(tl); return; }
#define END_LIB(var) do { ITypeInfo_Release(var); ITypeLib_Release(tl); } while (0)

/* ---- TYPEATTR ---- */

TEST(tlli_ti_attr_generic_invariants)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        TEQ(ITypeLib_GetTypeInfo(tl, i, &ti), S_OK);
        if (!ti) continue;
        TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) {
            CHECKF(ta->lcid == 0, "type %u lcid %lu", i, ta->lcid);
            CHECKF(ta->memidConstructor == MEMBERID_NIL, "type %u ctor memid", i);
            CHECKF(ta->memidDestructor == MEMBERID_NIL, "type %u dtor memid", i);
            CHECKF(ta->lpstrSchema == NULL, "type %u schema", i);
            CHECKF(ta->wMajorVerNum == 0 && ta->wMinorVerNum == 0, "type %u ver %u.%u", i, ta->wMajorVerNum, ta->wMinorVerNum);
            CHECKF(ta->typekind >= TKIND_ENUM && ta->typekind < TKIND_MAX, "type %u kind", i);
            CHECKF(ta->cbAlignment == 0 || ta->cbAlignment == 1 || ta->cbAlignment == 2 || ta->cbAlignment == 4 || ta->cbAlignment == 8 || ta->cbAlignment == 16, "type %u align %u", i, ta->cbAlignment);
            CHECKF(!(ta->wTypeFlags & ~0x7fffu), "type %u flags 0x%x", i, ta->wTypeFlags);
            if (ta->typekind == TKIND_INTERFACE || ta->typekind == TKIND_DISPATCH)
                CHECKF(ta->cbSizeInstance == PTR, "type %u (iface) cbSizeInstance %lu", i, ta->cbSizeInstance);
            if (ta->typekind == TKIND_ENUM) {
                CHECKF(ta->cbSizeInstance == 4 && ta->cbAlignment == 4, "enum %u size %lu align %u", i, ta->cbSizeInstance, ta->cbAlignment);
                CHECK(ta->cFuncs == 0);
            }
            if (ta->typekind == TKIND_RECORD || ta->typekind == TKIND_ALIAS || ta->typekind == TKIND_ENUM)
                CHECKF(ta->cImplTypes == 0, "type %u implTypes", i);
            if (ta->typekind == TKIND_COCLASS) CHECKF(ta->cFuncs == 0 && ta->cVars == 0 && ta->cImplTypes >= 1, "coclass %u", i);
            if (ta->typekind == TKIND_MODULE) CHECKF(ta->cImplTypes == 0 && ta->cFuncs >= 1, "module %u", i);
            /* every declared function and variable can be fetched; one past the end cannot */
            for (UINT f = 0; f < ta->cFuncs; f++) {
                FUNCDESC *fd = NULL;
                HRESULT hr = ITypeInfo_GetFuncDesc(ti, f, &fd);
                CHECKF(hr == S_OK && fd, "type %u func %u hr=0x%08lx", i, f, (unsigned long)hr);
                if (fd) ITypeInfo_ReleaseFuncDesc(ti, fd);
            }
            for (UINT v = 0; v < ta->cVars; v++) {
                VARDESC *vd = NULL;
                HRESULT hr = ITypeInfo_GetVarDesc(ti, v, &vd);
                CHECKF(hr == S_OK && vd, "type %u var %u hr=0x%08lx", i, v, (unsigned long)hr);
                if (vd) ITypeInfo_ReleaseVarDesc(ti, vd);
            }
            for (UINT m = 0; m < ta->cImplTypes; m++) {
                HREFTYPE h = 0; ITypeInfo *r = NULL; INT fl = -1;
                TEQ(ITypeInfo_GetRefTypeOfImplType(ti, m, &h), S_OK);
                TEQ(ITypeInfo_GetRefTypeInfo(ti, h, &r), S_OK);
                CHECKF(r != NULL, "type %u impl %u", i, m);
                if (r) ITypeInfo_Release(r);
                TEQ(ITypeInfo_GetImplTypeFlags(ti, m, &fl), S_OK);
                CHECK((fl & ~0x3f) == 0);
            }
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_ti_attr_release_does_not_change_refcount)
{
    WITH_LIB_AND(ti, L"IUnknown");
    ULONG r1 = ITypeInfo_AddRef(ti); ITypeInfo_Release(ti);
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    ULONG r2 = ITypeInfo_AddRef(ti); ITypeInfo_Release(ti);
    TEQ(r2, r1);
    if (ta) ITypeInfo_ReleaseTypeAttr(ti, ta);
    END_LIB(ti);
}

TEST(tlli_ti_attr_null_out)
{
    WITH_LIB_AND(ti, L"IUnknown");
    if (t_is_wine()) { t_skip("wine dereferences NULL out params"); END_LIB(ti); return; }
    TEQ(ITypeInfo_GetTypeAttr(ti, NULL), E_INVALIDARG);
    END_LIB(ti);
}

TEST(tlli_ti_iunknown_attr)
{
    WITH_LIB_AND(ti, L"IUnknown");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) {
        CHECK(tl_guid_eq(&ta->guid, &TL_IID_IUnknown));
        TEQ(ta->typekind, TKIND_INTERFACE);
        TEQ(ta->cFuncs, 3);
        TEQ(ta->cVars, 0);
        TEQ(ta->cImplTypes, 0);
        TEQ(ta->cbSizeInstance, PTR);
        TEQ(ta->cbSizeVft, 3 * PTR);
        t_note("IUnknown: flags 0x%x align %u", ta->wTypeFlags, ta->cbAlignment);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    END_LIB(ti);
}

TEST(tlli_ti_idispatch_attr)
{
    WITH_LIB_AND(ti, L"IDispatch");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) {
        CHECK(tl_guid_eq(&ta->guid, &TL_IID_IDispatch));
        TEQ(ta->typekind, TKIND_INTERFACE);
        TEQ(ta->cFuncs, 4);
        TEQ(ta->cImplTypes, 1);
        TEQ(ta->cbSizeInstance, PTR);              /* Wine test: IDispatch instance is pointer sized */
        TEQ(ta->cbSizeVft, 7 * PTR);
        t_note("IDispatch: flags 0x%x", ta->wTypeFlags);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    END_LIB(ti);
}

TEST(tlli_ti_ienumvariant_attr)
{
    WITH_LIB_AND(ti, L"IEnumVARIANT");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) {
        CHECK(tl_guid_eq(&ta->guid, &TL_IID_IEnumVARIANT));
        TEQ(ta->typekind, TKIND_INTERFACE);
        TEQ(ta->cFuncs, 4);                         /* Next Skip Reset Clone */
        TEQ(ta->cImplTypes, 1);
        TEQ(ta->cbSizeInstance, PTR);
        TEQ(ta->cbSizeVft, 7 * PTR);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    END_LIB(ti);
}

TEST(tlli_ti_vft_size_matches_inheritance)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) continue;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            if (ta->typekind == TKIND_INTERFACE) {
                UINT inh = inherited_funcs(ti);
                CHECKF(ta->cbSizeVft == (ta->cFuncs + inh) * PTR, "type %u: cbSizeVft %u, funcs %u + inherited %u", i, ta->cbSizeVft, ta->cFuncs, inh);
                /* every function's oVft lies inside the vtable */
                for (UINT f = 0; f < ta->cFuncs; f++) {
                    FUNCDESC *fd = NULL;
                    if (SUCCEEDED(ITypeInfo_GetFuncDesc(ti, f, &fd)) && fd) {
                        CHECKF(fd->oVft >= inh * PTR && fd->oVft < ta->cbSizeVft && fd->oVft % PTR == 0, "type %u func %u oVft %d", i, f, fd->oVft);
                        ITypeInfo_ReleaseFuncDesc(ti, fd);
                    }
                }
            }
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

/* ---- records, enums, aliases ---- */

TEST(tlli_ti_guid_record)
{
    WITH_LIB_AND(ti, L"GUID");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) {
        TEQ(ta->typekind, TKIND_RECORD);
        TEQ(ta->cbSizeInstance, 16);
        TEQ(ta->cbAlignment, 4);
        TEQ(ta->cVars, 4);
        TEQ(ta->cFuncs, 0);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    static const struct { const WCHAR *name; int off; VARTYPE vt; } f[] = {
        { L"Data1", 0, VT_UI4 }, { L"Data2", 4, VT_UI2 }, { L"Data3", 6, VT_UI2 }, { L"Data4", 8, VT_CARRAY },
    };
    for (UINT v = 0; v < 4; v++) {
        VARDESC *vd = NULL; BSTR nm[2] = {0}; UINT c = 0;
        TEQ(ITypeInfo_GetVarDesc(ti, v, &vd), S_OK);
        if (!vd) continue;
        TEQ(ITypeInfo_GetNames(ti, vd->memid, nm, 2, &c), S_OK);
        TEQ(c, 1);
        CHECK(nm[0] && tl_weq(nm[0], f[v].name));
        TEQ(vd->varkind, VAR_PERINSTANCE);
        TEQ(vd->oInst, f[v].off);
        TEQ(vd->elemdescVar.tdesc.vt, f[v].vt);
        if (f[v].vt == VT_CARRAY && vd->elemdescVar.tdesc.lpadesc) {
            ARRAYDESC *ad = vd->elemdescVar.tdesc.lpadesc;
            TEQ(ad->cDims, 1);
            TEQ(ad->rgbounds[0].cElements, 8);
            TEQ(ad->rgbounds[0].lLbound, 0);
            TEQ(ad->tdescElem.vt, VT_UI1);
        }
        tl_freebstr(nm[0]);
        ITypeInfo_ReleaseVarDesc(ti, vd);
    }
    END_LIB(ti);
}

TEST(tlli_ti_tristate_enum)
{
    WITH_LIB_AND(ti, L"OLE_TRISTATE");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) {
        TEQ(ta->typekind, TKIND_ENUM);
        TEQ(ta->cVars, 3);
        TEQ(ta->cbSizeInstance, 4);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    static const WCHAR *names[] = { L"Unchecked", L"Checked", L"Gray" };
    for (UINT v = 0; v < 3; v++) {
        VARDESC *vd = NULL; BSTR nm = NULL; UINT c = 0;
        TEQ(ITypeInfo_GetVarDesc(ti, v, &vd), S_OK);
        if (!vd) continue;
        TEQ(vd->varkind, VAR_CONST);
        CHECK(vd->lpvarValue != NULL);
        if (vd->lpvarValue) { TEQ(V_VT(vd->lpvarValue), VT_I4); TEQ(V_I4(vd->lpvarValue), v); }
        TEQ(vd->elemdescVar.tdesc.vt, VT_INT);
        TEQ(ITypeInfo_GetNames(ti, vd->memid, &nm, 1, &c), S_OK);
        CHECK(nm && tl_weq(nm, names[v]));
        tl_freebstr(nm);
        ITypeInfo_ReleaseVarDesc(ti, vd);
    }
    END_LIB(ti);
}

TEST(tlli_ti_loadpictureconstants_enum)
{
    WITH_LIB_AND(ti, L"LoadPictureConstants");
    static const struct { const WCHAR *n; int v; } e[] = { {L"Default",0}, {L"Monochrome",1}, {L"VgaColor",2}, {L"Color",4} };
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) { TEQ(ta->cVars, 4); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    for (UINT v = 0; v < 4; v++) {
        VARDESC *vd = NULL; BSTR nm = NULL; UINT c = 0;
        TEQ(ITypeInfo_GetVarDesc(ti, v, &vd), S_OK);
        if (!vd) continue;
        TEQ(ITypeInfo_GetNames(ti, vd->memid, &nm, 1, &c), S_OK);
        CHECK(nm && tl_weq(nm, e[v].n));
        if (vd->lpvarValue) TEQ(V_I4(vd->lpvarValue), e[v].v);
        tl_freebstr(nm);
        ITypeInfo_ReleaseVarDesc(ti, vd);
    }
    END_LIB(ti);
}

TEST(tlli_ti_alias_targets)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const struct { const WCHAR *n; VARTYPE vt; } a[] = {
        { L"OLE_COLOR", VT_UI4 },
        { L"OLE_XPOS_PIXELS", VT_I4 }, { L"OLE_YPOS_PIXELS", VT_I4 }, { L"OLE_XSIZE_PIXELS", VT_I4 }, { L"OLE_YSIZE_PIXELS", VT_I4 },
        { L"OLE_XPOS_HIMETRIC", VT_I4 }, { L"OLE_YPOS_HIMETRIC", VT_I4 }, { L"OLE_XSIZE_HIMETRIC", VT_I4 }, { L"OLE_YSIZE_HIMETRIC", VT_I4 },
        { L"OLE_XPOS_CONTAINER", VT_R4 }, { L"OLE_YPOS_CONTAINER", VT_R4 }, { L"OLE_XSIZE_CONTAINER", VT_R4 }, { L"OLE_YSIZE_CONTAINER", VT_R4 },
        { L"OLE_OPTEXCLUSIVE", VT_BOOL }, { L"OLE_CANCELBOOL", VT_BOOL }, { L"OLE_ENABLEDEFAULTBOOL", VT_BOOL },
        { L"FONTNAME", VT_BSTR }, { L"FONTSIZE", VT_CY }, { L"FONTBOLD", VT_BOOL }, { L"FONTITALIC", VT_BOOL },
        { L"FONTUNDERSCORE", VT_BOOL }, { L"FONTSTRIKETHROUGH", VT_BOOL },
    };
    for (unsigned i = 0; i < sizeof a / sizeof a[0]; i++) {
        ITypeInfo *ti = get_ti(tl, a[i].n); TYPEATTR *ta = NULL;
        if (!ti) { CHECKF(0, "%ls missing", a[i].n); continue; }
        TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) {
            TEQ(ta->typekind, TKIND_ALIAS);
            CHECKF(ta->tdescAlias.vt == a[i].vt, "%ls: alias vt %d want %d", a[i].n, ta->tdescAlias.vt, a[i].vt);
            CHECK(ta->cFuncs == 0 && ta->cVars == 0);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    /* OLE_HANDLE is documented as 32-bit in the typelib; record the exact VT */
    ITypeInfo *h = get_ti(tl, L"OLE_HANDLE"); TYPEATTR *ta = NULL;
    if (h && SUCCEEDED(ITypeInfo_GetTypeAttr(h, &ta))) {
        CHECK(ta->tdescAlias.vt == VT_INT || ta->tdescAlias.vt == VT_I4);
        t_note("OLE_HANDLE alias vt = %d", ta->tdescAlias.vt);
        ITypeInfo_ReleaseTypeAttr(h, ta);
    }
    if (h) ITypeInfo_Release(h);
    ITypeLib_Release(tl);
}

TEST(tlli_ti_alias_sizes)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const struct { const WCHAR *n; UINT size; } a[] = {
        { L"OLE_COLOR", 4 }, { L"OLE_XPOS_PIXELS", 4 }, { L"OLE_XPOS_CONTAINER", 4 },
        { L"OLE_OPTEXCLUSIVE", 2 }, { L"FONTNAME", PTR }, { L"FONTSIZE", 8 }, { L"FONTBOLD", 2 },
    };
    for (unsigned i = 0; i < sizeof a / sizeof a[0]; i++) {
        ITypeInfo *ti = get_ti(tl, a[i].n); TYPEATTR *ta = NULL;
        if (!ti) { CHECKF(0, "%ls missing", a[i].n); continue; }
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            CHECKF(ta->cbSizeInstance == a[i].size, "%ls cbSizeInstance %lu want %u", a[i].n, ta->cbSizeInstance, a[i].size);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

/* ---- functions ---- */

TEST(tlli_ti_iunknown_funcs)
{
    WITH_LIB_AND(ti, L"IUnknown");
    static const struct { const WCHAR *n; UINT np; VARTYPE ret; } f[] = {
        { L"QueryInterface", 2, VT_HRESULT }, { L"AddRef", 0, VT_UI4 }, { L"Release", 0, VT_UI4 },
    };
    for (UINT i = 0; i < 3; i++) {
        FUNCDESC *fd = NULL; BSTR nm[4] = {0}; UINT c = 0;
        TEQ(ITypeInfo_GetFuncDesc(ti, i, &fd), S_OK);
        if (!fd) continue;
        TEQ(fd->funckind, FUNC_PUREVIRTUAL);
        TEQ(fd->invkind, INVOKE_FUNC);
        TEQ(fd->callconv, CC_STDCALL);
        TEQ(fd->cParams, f[i].np);
        TEQ(fd->cParamsOpt, 0);
        TEQ(fd->oVft, i * PTR);
        TEQ(fd->memid, 0x60000000 + i);
        TEQ(fd->elemdescFunc.tdesc.vt, f[i].ret);
        CHECK(fd->wFuncFlags & FUNCFLAG_FRESTRICTED);
        TEQ(ITypeInfo_GetNames(ti, fd->memid, nm, 4, &c), S_OK);
        TEQ(c, f[i].np + 1);
        CHECK(nm[0] && tl_weq(nm[0], f[i].n));
        for (UINT k = 0; k < c; k++) tl_freebstr(nm[k]);
        if (i == 0 && fd->lprgelemdescParam) {
            TEQ(fd->lprgelemdescParam[0].tdesc.vt, VT_PTR);
            TEQ(fd->lprgelemdescParam[1].tdesc.vt, VT_PTR);
            CHECK(fd->lprgelemdescParam[0].paramdesc.wParamFlags & PARAMFLAG_FIN);
            CHECK(fd->lprgelemdescParam[1].paramdesc.wParamFlags & PARAMFLAG_FOUT);
            if (fd->lprgelemdescParam[0].tdesc.lptdesc) TEQ(fd->lprgelemdescParam[0].tdesc.lptdesc->vt, VT_USERDEFINED);
            if (fd->lprgelemdescParam[1].tdesc.lptdesc) {
                TEQ(fd->lprgelemdescParam[1].tdesc.lptdesc->vt, VT_PTR);
                if (fd->lprgelemdescParam[1].tdesc.lptdesc->lptdesc) TEQ(fd->lprgelemdescParam[1].tdesc.lptdesc->lptdesc->vt, VT_VOID);
            }
        }
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    END_LIB(ti);
}

TEST(tlli_ti_idispatch_funcs)
{
    WITH_LIB_AND(ti, L"IDispatch");
    static const struct { const WCHAR *n; UINT np; } f[] = {
        { L"GetTypeInfoCount", 1 }, { L"GetTypeInfo", 3 }, { L"GetIDsOfNames", 5 }, { L"Invoke", 8 },
    };
    for (UINT i = 0; i < 4; i++) {
        FUNCDESC *fd = NULL; BSTR nm[16] = {0}; UINT c = 0;
        TEQ(ITypeInfo_GetFuncDesc(ti, i, &fd), S_OK);
        if (!fd) continue;
        TEQ(fd->funckind, FUNC_PUREVIRTUAL);
        TEQ(fd->invkind, INVOKE_FUNC);
        TEQ(fd->callconv, CC_STDCALL);
        TEQ(fd->cParams, f[i].np);
        TEQ(fd->oVft, (3 + i) * PTR);
        TEQ(fd->memid, 0x60010000 + i);
        TEQ(fd->elemdescFunc.tdesc.vt, VT_HRESULT);
        TEQ(ITypeInfo_GetNames(ti, fd->memid, nm, 16, &c), S_OK);
        TEQ(c, f[i].np + 1);
        CHECK(nm[0] && tl_weq(nm[0], f[i].n));
        for (UINT k = 0; k < c; k++) { CHECK(nm[k] != NULL); tl_freebstr(nm[k]); }
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    END_LIB(ti);
}

TEST(tlli_ti_funcdesc_out_of_range)
{
    WITH_LIB_AND(ti, L"IUnknown");
    UINT bad[] = { 3, 4, 100, 0xffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FUNCDESC *fd = (FUNCDESC *)SENT_PTR;
        HRESULT hr = ITypeInfo_GetFuncDesc(ti, bad[i], &fd);
        CHECKF(hr == TYPE_E_ELEMENTNOTFOUND, "GetFuncDesc(%u) -> 0x%08lx", bad[i], (unsigned long)hr);
        VARDESC *vd = (VARDESC *)SENT_PTR;
        hr = ITypeInfo_GetVarDesc(ti, bad[i], &vd);
        CHECKF(hr == TYPE_E_ELEMENTNOTFOUND, "GetVarDesc(%u) -> 0x%08lx", bad[i], (unsigned long)hr);
    }
    VARDESC *vd = NULL;
    TEQ(ITypeInfo_GetVarDesc(ti, 0, &vd), TYPE_E_ELEMENTNOTFOUND);       /* interface without variables */
    END_LIB(ti);
}

TEST(tlli_ti_funcdesc_null_out)
{
    WITH_LIB_AND(ti, L"IUnknown");
    if (t_is_wine()) { t_skip("wine dereferences NULL"); END_LIB(ti); return; }
    TEQ(ITypeInfo_GetFuncDesc(ti, 0, NULL), E_INVALIDARG);
    TEQ(ITypeInfo_GetVarDesc(ti, 0, NULL), E_INVALIDARG);
    END_LIB(ti);
}

TEST(tlli_ti_ifont_properties)
{
    WITH_LIB_AND(ti, L"IFont");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    CHECK(tl_guid_eq(&ta->guid, &TL_IID_IFont));
    TEQ(ta->typekind, TKIND_INTERFACE);
    TEQ(ta->cImplTypes, 1);
    CHECKF(ta->cFuncs >= 22, "IFont cFuncs=%u", ta->cFuncs);
    t_note("IFont: cFuncs=%u flags=0x%x", ta->cFuncs, ta->wTypeFlags);
    UINT nf = ta->cFuncs;
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    UINT gets = 0, puts = 0;
    MEMBERID prev_get = 0; int prev_kind = -1;
    for (UINT i = 0; i < nf; i++) {
        FUNCDESC *fd = NULL; BSTR nm = NULL; UINT c = 0;
        TEQ(ITypeInfo_GetFuncDesc(ti, i, &fd), S_OK);
        if (!fd) continue;
        TEQ(fd->funckind, FUNC_PUREVIRTUAL);
        TEQ(fd->oVft, (3 + i) * PTR);
        TEQ(fd->callconv, CC_STDCALL);
        TEQ(fd->elemdescFunc.tdesc.vt, VT_HRESULT);
        TEQ(ITypeInfo_GetNames(ti, fd->memid, &nm, 1, &c), S_OK);
        if (fd->invkind == INVOKE_PROPERTYGET) {
            gets++;
            TEQ(fd->cParams, 1);
            if (fd->lprgelemdescParam) {
                USHORT fl = fd->lprgelemdescParam[0].paramdesc.wParamFlags;
                CHECKF((fl & PARAMFLAG_FOUT) && (fl & PARAMFLAG_FRETVAL), "get %ls flags 0x%x", nm, fl);
                TEQ(fd->lprgelemdescParam[0].tdesc.vt, VT_PTR);
            }
            prev_get = fd->memid; prev_kind = 1;
        } else if (fd->invkind == INVOKE_PROPERTYPUT) {
            puts++;
            TEQ(fd->cParams, 1);
            if (fd->lprgelemdescParam) CHECK(fd->lprgelemdescParam[0].paramdesc.wParamFlags & PARAMFLAG_FIN);
            if (prev_kind == 1) CHECKF(fd->memid == prev_get, "%ls: put memid 0x%lx != get memid 0x%lx", nm, fd->memid, prev_get);
            prev_kind = 2;
        } else prev_kind = 0;
        tl_freebstr(nm);
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    CHECKF(gets >= 9 && puts >= 8, "gets=%u puts=%u", gets, puts);
    END_LIB(ti);
}

TEST(tlli_ti_ifont_method_names)
{
    WITH_LIB_AND(ti, L"IFont");
    static const WCHAR *want[] = { L"Name", L"Size", L"Bold", L"Italic", L"Underline", L"Strikethrough", L"Weight", L"Charset",
                                   L"hFont", L"Clone", L"IsEqual", L"SetRatio", L"AddRefHfont", L"ReleaseHfont" };
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    UINT nf = ta->cFuncs;
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    for (unsigned w = 0; w < sizeof want / sizeof want[0]; w++) {
        int found = 0;
        for (UINT i = 0; i < nf; i++) {
            FUNCDESC *fd = NULL; BSTR nm = NULL; UINT c = 0;
            if (SUCCEEDED(ITypeInfo_GetFuncDesc(ti, i, &fd)) && fd) {
                if (SUCCEEDED(ITypeInfo_GetNames(ti, fd->memid, &nm, 1, &c)) && nm && !lstrcmpW(nm, want[w])) found++;
                tl_freebstr(nm);
                ITypeInfo_ReleaseFuncDesc(ti, fd);
            }
        }
        CHECKF(found >= 1, "IFont::%ls not found", want[w]);
        if (w < 8) CHECKF(found == 2, "IFont::%ls found %d times (want get+put)", want[w], found);
        if (w == 8) CHECKF(found == 1, "hFont is read-only (found %d)", found);
    }
    END_LIB(ti);
}

TEST(tlli_ti_names_protocol)
{
    WITH_LIB_AND(ti, L"IDispatch");
    FUNCDESC *fd = NULL;
    TEQ(ITypeInfo_GetFuncDesc(ti, 3, &fd), S_OK);       /* Invoke, 8 params */
    REQUIRE(fd);
    BSTR nm[16]; UINT c;
    c = 77;
    TEQ(ITypeInfo_GetNames(ti, fd->memid, nm, 0, &c), S_OK);
    TEQ(c, 0);
    c = 77; memset(nm, 0, sizeof nm);
    TEQ(ITypeInfo_GetNames(ti, fd->memid, nm, 1, &c), S_OK);
    TEQ(c, 1);
    CHECK(nm[0] && tl_weq(nm[0], L"Invoke"));
    tl_freebstr(nm[0]);
    c = 77; memset(nm, 0, sizeof nm);
    TEQ(ITypeInfo_GetNames(ti, fd->memid, nm, 5, &c), S_OK);
    TEQ(c, 5);
    for (UINT i = 0; i < c; i++) { CHECK(nm[i] != NULL); tl_freebstr(nm[i]); }
    c = 77; memset(nm, 0, sizeof nm);
    TEQ(ITypeInfo_GetNames(ti, fd->memid, nm, 16, &c), S_OK);
    TEQ(c, 9);                                            /* function name + 8 parameters */
    for (UINT i = 0; i < c; i++) tl_freebstr(nm[i]);
    for (UINT i = c; i < 16; i++) CHECK(nm[i] == NULL);   /* entries past cNames untouched */
    /* unknown memid */
    c = 77;
    TEQ(ITypeInfo_GetNames(ti, 0x12345678, nm, 4, &c), TYPE_E_ELEMENTNOTFOUND);
    ITypeInfo_ReleaseFuncDesc(ti, fd);
    END_LIB(ti);
}

TEST(tlli_ti_names_null_args)
{
    WITH_LIB_AND(ti, L"IDispatch");
    if (t_is_wine()) { t_skip("wine dereferences NULL"); END_LIB(ti); return; }
    BSTR nm[2] = {0}; UINT c = 0;
    TEQ(ITypeInfo_GetNames(ti, 0x60010000, nm, 2, NULL), E_INVALIDARG);
    TEQ(ITypeInfo_GetNames(ti, 0x60010000, NULL, 2, &c), E_INVALIDARG);
    END_LIB(ti);
}

TEST(tlli_ti_idsofnames_interface)
{
    WITH_LIB_AND(ti, L"IFont");
    OLECHAR *nm; DISPID id = 0x7777;
    FUNCDESC *fd = NULL;
    nm = L"Clone";
    TEQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id), S_OK);
    CHECK(id != 0x7777 && id != DISPID_UNKNOWN);
    nm = L"clone"; DISPID id2 = 0;
    TEQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id2), S_OK);      /* case-insensitive */
    TEQ(id2, id);
    nm = L"CLONE"; id2 = 0;
    TEQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id2), S_OK);
    TEQ(id2, id);
    nm = L"bogus"; id = 0x7777;
    TEQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id), DISP_E_UNKNOWNNAME);
    CHECK(id == DISPID_UNKNOWN || id == 0x7777);
    /* inherited names resolve through the base interface */
    nm = L"AddRef"; id = 0;
    TEQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id), S_OK);
    TEQ(id, 0x60000001);
    nm = L"QueryInterface"; id = 0;
    TEQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id), S_OK);
    TEQ(id, 0x60000000);
    END_LIB(ti);
}

TEST(tlli_ti_idsofnames_params)
{
    /* GetIDsOfNames([func, param1, param2]) -> [memid, 0, 1] */
    WITH_LIB_AND(ti, L"IDispatch");
    FUNCDESC *fd = NULL; BSTR names[9]; UINT c = 0;
    TEQ(ITypeInfo_GetFuncDesc(ti, 1, &fd), S_OK);        /* GetTypeInfo(itinfo, lcid, pptinfo) */
    REQUIRE(fd);
    TEQ(ITypeInfo_GetNames(ti, fd->memid, names, 9, &c), S_OK);
    TEQ(c, 4);
    OLECHAR *in[3] = { names[0], names[2], names[1] };
    DISPID ids[3] = { 99, 99, 99 };
    TEQ(ITypeInfo_GetIDsOfNames(ti, in, 3, ids), S_OK);
    TEQ(ids[0], fd->memid);
    TEQ(ids[1], 1);
    TEQ(ids[2], 0);
    for (UINT i = 0; i < c; i++) tl_freebstr(names[i]);
    ITypeInfo_ReleaseFuncDesc(ti, fd);
    END_LIB(ti);
}

/* ---- hierarchy ---- */

TEST(tlli_ti_hierarchy_idispatch_iunknown)
{
    WITH_LIB_AND(ti, L"IDispatch");
    HREFTYPE h = 0; ITypeInfo *base = NULL; INT fl = -1;
    TEQ(ITypeInfo_GetRefTypeOfImplType(ti, 0, &h), S_OK);
    TEQ(ITypeInfo_GetRefTypeInfo(ti, h, &base), S_OK);
    REQUIRE(base);
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(base, &ta), S_OK);
    if (ta) { CHECK(tl_guid_eq(&ta->guid, &TL_IID_IUnknown)); TEQ(ta->typekind, TKIND_INTERFACE); ITypeInfo_ReleaseTypeAttr(base, ta); }
    TEQ(ITypeInfo_GetImplTypeFlags(ti, 0, &fl), S_OK);
    TEQ(fl, 0);
    ITypeInfo *expect = get_ti(tl, L"IUnknown");
    CHECK(expect == base);                        /* same cached object */
    if (expect) ITypeInfo_Release(expect);
    ITypeInfo_Release(base);
    END_LIB(ti);
}

TEST(tlli_ti_hierarchy_ifont_chain)
{
    WITH_LIB_AND(ti, L"IFont");
    const GUID *chain[] = { &TL_IID_IUnknown };
    HREFTYPE h; ITypeInfo *b = NULL;
    TEQ(ITypeInfo_GetRefTypeOfImplType(ti, 0, &h), S_OK);
    TEQ(ITypeInfo_GetRefTypeInfo(ti, h, &b), S_OK);
    if (b) {
        TYPEATTR *ta = NULL;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(b, &ta))) { CHECK(tl_guid_eq(&ta->guid, chain[0])); ITypeInfo_ReleaseTypeAttr(b, ta); }
        ITypeInfo_Release(b);
    }
    END_LIB(ti);
}

TEST(tlli_ti_impltype_out_of_range)
{
    WITH_LIB_AND(ti, L"IUnknown");
    HREFTYPE h = 0x1234; INT fl = 0x1234;
    TEQ(ITypeInfo_GetRefTypeOfImplType(ti, 0, &h), TYPE_E_ELEMENTNOTFOUND);   /* IUnknown has no base */
    TEQ(ITypeInfo_GetImplTypeFlags(ti, 0, &fl), TYPE_E_ELEMENTNOTFOUND);
    TEQ(ITypeInfo_GetRefTypeOfImplType(ti, 5, &h), TYPE_E_ELEMENTNOTFOUND);
    TEQ(ITypeInfo_GetImplTypeFlags(ti, 5, &fl), TYPE_E_ELEMENTNOTFOUND);
    ITypeInfo *d = get_ti(tl, L"IDispatch");
    if (d) {
        TEQ(ITypeInfo_GetRefTypeOfImplType(d, 1, &h), TYPE_E_ELEMENTNOTFOUND);
        TEQ(ITypeInfo_GetImplTypeFlags(d, 1, &fl), TYPE_E_ELEMENTNOTFOUND);
        ITypeInfo_Release(d);
    }
    END_LIB(ti);
}

TEST(tlli_ti_coclass_stdfont)
{
    WITH_LIB_AND(ti, L"StdFont");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    CHECK(tl_guid_eq(&ta->guid, &TL_CLSID_StdFont));
    TEQ(ta->typekind, TKIND_COCLASS);
    CHECKF(ta->cImplTypes >= 2, "cImplTypes=%u", ta->cImplTypes);
    UINT n = ta->cImplTypes;
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    int sawIFont = 0, sawFont = 0, defaults = 0;
    for (UINT i = 0; i < n; i++) {
        HREFTYPE h; ITypeInfo *r = NULL; INT fl = 0; BSTR nm = NULL;
        TEQ(ITypeInfo_GetRefTypeOfImplType(ti, i, &h), S_OK);
        TEQ(ITypeInfo_GetRefTypeInfo(ti, h, &r), S_OK);
        TEQ(ITypeInfo_GetImplTypeFlags(ti, i, &fl), S_OK);
        if (r) {
            ITypeInfo_GetDocumentation(r, MEMBERID_NIL, &nm, NULL, NULL, NULL);
            if (nm) {
                if (!lstrcmpW(nm, L"IFont")) sawIFont = 1;
                if (!lstrcmpW(nm, L"Font")) { sawFont = 1; CHECKF(fl & IMPLTYPEFLAG_FDEFAULT, "Font must be default (flags %d)", fl); }
                if (fl & IMPLTYPEFLAG_FDEFAULT) defaults++;
                t_note("StdFont impl %u: %ls flags %d", i, nm, fl);
            }
            tl_freebstr(nm);
            ITypeInfo_Release(r);
        }
    }
    CHECK(sawIFont && sawFont);
    TEQ(defaults, 1);
    END_LIB(ti);
}

TEST(tlli_ti_coclass_stdpicture)
{
    WITH_LIB_AND(ti, L"StdPicture");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    CHECK(tl_guid_eq(&ta->guid, &TL_CLSID_StdPicture));
    TEQ(ta->typekind, TKIND_COCLASS);
    CHECK(ta->cImplTypes >= 2);
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    END_LIB(ti);
}

TEST(tlli_ti_font_dispinterface_members)
{
    WITH_LIB_AND(ti, L"Font");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    CHECK(tl_guid_eq(&ta->guid, &TL_IID_IFontDisp));
    TEQ(ta->typekind, TKIND_DISPATCH);
    CHECK(ta->wTypeFlags & TYPEFLAG_FDISPATCHABLE);
    TEQ(ta->cbSizeInstance, PTR);
    t_note("Font dispinterface: cFuncs=%u cVars=%u cImpl=%u flags=0x%x", ta->cFuncs, ta->cVars, ta->cImplTypes, ta->wTypeFlags);
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    /* the documented DISPIDs of the Font object (DISPID_FONT_*) */
    static const struct { const WCHAR *n; DISPID id; } d[] = {
        { L"Name", 0 }, { L"Size", 2 }, { L"Bold", 3 }, { L"Italic", 4 }, { L"Underline", 5 },
        { L"Strikethrough", 6 }, { L"Weight", 7 }, { L"Charset", 8 },
    };
    for (unsigned i = 0; i < 8; i++) {
        OLECHAR *nm = (OLECHAR *)d[i].n; DISPID id = 0x7777;
        HRESULT hr = ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id);
        CHECKF(hr == S_OK, "%ls hr=0x%08lx", d[i].n, (unsigned long)hr);
        CHECKF(id == d[i].id, "%ls id=%ld want %ld", d[i].n, id, d[i].id);
    }
    END_LIB(ti);
}

TEST(tlli_ti_picture_dispinterface_members)
{
    WITH_LIB_AND(ti, L"Picture");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    TEQ(ta->typekind, TKIND_DISPATCH);
    CHECK(tl_guid_eq(&ta->guid, &TL_IID_IPictureDisp));
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    static const struct { const WCHAR *n; DISPID id; } d[] = {
        { L"Handle", 0 }, { L"hPal", 2 }, { L"Type", 3 }, { L"Width", 4 }, { L"Height", 5 }, { L"Render", 6 },
    };
    for (unsigned i = 0; i < 6; i++) {
        OLECHAR *nm = (OLECHAR *)d[i].n; DISPID id = 0x7777;
        HRESULT hr = ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id);
        CHECKF(hr == S_OK, "%ls hr=0x%08lx", d[i].n, (unsigned long)hr);
        CHECKF(id == d[i].id, "%ls id=%ld want %ld", d[i].n, id, d[i].id);
    }
    END_LIB(ti);
}

TEST(tlli_ti_ifontdisp_and_ipicturedisp_resolve)
{
    /* IFontDisp / IPictureDisp are typedef-style names for the dispinterfaces; they must be reachable by name */
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const struct { const WCHAR *n; const GUID *g; } t[] = { { L"IFontDisp", &TL_IID_IFontDisp }, { L"IPictureDisp", &TL_IID_IPictureDisp } };
    for (int i = 0; i < 2; i++) {
        int idx = tl_find_index(tl, t[i].n);
        CHECKF(idx >= 0, "%ls missing from library", t[i].n);
        if (idx < 0) continue;
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        TEQ(ITypeLib_GetTypeInfo(tl, idx, &ti), S_OK);
        if (!ti) continue;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            t_note("%ls: kind %d", t[i].n, ta->typekind);
            if (ta->typekind == TKIND_ALIAS) {
                CHECK(ta->tdescAlias.vt == VT_USERDEFINED);
                ITypeInfo *tgt = NULL;
                TEQ(ITypeInfo_GetRefTypeInfo(ti, ta->tdescAlias.hreftype, &tgt), S_OK);
                if (tgt) {
                    TYPEATTR *tb = NULL;
                    if (SUCCEEDED(ITypeInfo_GetTypeAttr(tgt, &tb))) { CHECK(tl_guid_eq(&tb->guid, t[i].g)); TEQ(tb->typekind, TKIND_DISPATCH); ITypeInfo_ReleaseTypeAttr(tgt, tb); }
                    ITypeInfo_Release(tgt);
                }
            } else {
                CHECK(ta->typekind == TKIND_DISPATCH || ta->typekind == TKIND_INTERFACE);
                CHECK(tl_guid_eq(&ta->guid, t[i].g));
            }
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_ti_refinfo_bad_href)
{
    WITH_LIB_AND(ti, L"IDispatch");
    ITypeInfo *r = (ITypeInfo *)SENT_PTR;
    HRESULT hr = ITypeInfo_GetRefTypeInfo(ti, 0x7ffffffc, &r);
    CHECK(FAILED(hr));
    t_note("GetRefTypeInfo(bogus href) -> 0x%08lx", (unsigned long)hr);
    END_LIB(ti);
}

TEST(tlli_ti_containing_typelib)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; ITypeLib *c = NULL; UINT idx = 0x7777;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) continue;
        TEQ(ITypeInfo_GetContainingTypeLib(ti, &c, &idx), S_OK);
        CHECKF(idx == i, "type %u reports index %u", i, idx);
        CHECK(c == tl);
        if (c) ITypeLib_Release(c);
        ITypeInfo_Release(ti);
    }
    ITypeInfo *ti = NULL; ITypeLib *c = NULL; UINT idx = 0x7777;
    TEQ(ITypeLib_GetTypeInfo(tl, 1, &ti), S_OK);
    if (ti) {
        /* adapted from Wine dlls/oleaut32/tests/typelib.c test_TypeInfo2_GetContainingTypeLib (LGPL-2.1+) */
        TEQ(ITypeInfo_GetContainingTypeLib(ti, &c, NULL), S_OK);
        CHECK(c != NULL); if (c) ITypeLib_Release(c);
        idx = 0x7777;
        TEQ(ITypeInfo_GetContainingTypeLib(ti, NULL, &idx), S_OK);
        TEQ(idx, 1);
        TEQ(ITypeInfo_GetContainingTypeLib(ti, NULL, NULL), S_OK);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_ti_documentation_members)
{
    WITH_LIB_AND(ti, L"IUnknown");
    BSTR nm = NULL, doc = NULL, hf = NULL; DWORD ctx = 0x5555;
    TEQ(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &nm, &doc, &ctx, &hf), S_OK);
    CHECK(nm && tl_weq(nm, L"IUnknown"));
    tl_freebstr(nm); tl_freebstr(doc); tl_freebstr(hf);
    nm = NULL;
    TEQ(ITypeInfo_GetDocumentation(ti, 0x60000000, &nm, NULL, NULL, NULL), S_OK);
    CHECK(nm && tl_weq(nm, L"QueryInterface"));
    tl_freebstr(nm);
    TEQ(ITypeInfo_GetDocumentation(ti, 0x60000001, NULL, NULL, NULL, NULL), S_OK);
    nm = NULL;
    TEQ(ITypeInfo_GetDocumentation(ti, 0x40404040, &nm, NULL, NULL, NULL), TYPE_E_ELEMENTNOTFOUND);
    CHECK(nm == NULL);
    END_LIB(ti);
}

TEST(tlli_ti_documentation_helpstrings)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = get_ti(tl, L"StdFunctions");
    if (!ti) { CHECK(0); ITypeLib_Release(tl); return; }
    BSTR nm = NULL, doc = NULL; DWORD ctx = 0;
    TEQ(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &nm, &doc, &ctx, NULL), S_OK);
    CHECK(nm && tl_weq(nm, L"StdFunctions"));
    t_note("StdFunctions doc=\"%ls\" helpcontext=0x%lx", doc ? doc : L"(null)", ctx);
    tl_freebstr(nm); tl_freebstr(doc);
    ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

/* ---- module (DLL entry) ---- */

TEST(tlli_ti_module_stdfunctions)
{
    WITH_LIB_AND(ti, L"StdFunctions");
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    REQUIRE(ta);
    TEQ(ta->typekind, TKIND_MODULE);
    TEQ(ta->cFuncs, 2);
    TEQ(ta->cImplTypes, 0);
    ITypeInfo_ReleaseTypeAttr(ti, ta);
    static const WCHAR *names[2] = { L"LoadPicture", L"SavePicture" };
    for (UINT i = 0; i < 2; i++) {
        FUNCDESC *fd = NULL; BSTR nm = NULL; UINT c = 0;
        TEQ(ITypeInfo_GetFuncDesc(ti, i, &fd), S_OK);
        if (!fd) continue;
        TEQ(fd->funckind, FUNC_STATIC);
        TEQ(fd->invkind, INVOKE_FUNC);
        TEQ(fd->callconv, CC_STDCALL);
        TEQ(ITypeInfo_GetNames(ti, fd->memid, &nm, 1, &c), S_OK);
        CHECK(nm && tl_weq(nm, names[i]));
        tl_freebstr(nm);
        BSTR dll = NULL, ent = NULL; WORD ord = 0x7777;
        HRESULT hr = ITypeInfo_GetDllEntry(ti, fd->memid, INVOKE_FUNC, &dll, &ent, &ord);
        TEQ(hr, S_OK);
        if (hr == S_OK) {
            CHECK(dll && tl_wieq(dll, L"oleaut32.dll"));
            CHECK(ent != NULL);
            t_note("%ls -> %ls!%ls (ordinal 0x%x)", names[i], dll, ent ? ent : L"(null)", ord);
            if (i == 1) WCHECK(ent && tl_weq(ent, L"OleSavePictureFile"));
        }
        tl_freebstr(dll); tl_freebstr(ent);
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    END_LIB(ti);
}

TEST(tlli_ti_dllentry_negative)
{
    WITH_LIB_AND(ti, L"StdFunctions");
    FUNCDESC *fd = NULL;
    TEQ(ITypeInfo_GetFuncDesc(ti, 0, &fd), S_OK);
    REQUIRE(fd);
    BSTR dll = NULL, ent = NULL; WORD ord = 0;
    /* wrong invkind for the function: no such element */
    TEQ(ITypeInfo_GetDllEntry(ti, fd->memid, INVOKE_PROPERTYGET, &dll, &ent, &ord), TYPE_E_ELEMENTNOTFOUND);
    TEQ(ITypeInfo_GetDllEntry(ti, fd->memid, INVOKE_PROPERTYPUT, &dll, &ent, &ord), TYPE_E_ELEMENTNOTFOUND);
    /* unknown memid */
    TEQ(ITypeInfo_GetDllEntry(ti, 0x6000000d, INVOKE_FUNC, &dll, &ent, &ord), TYPE_E_ELEMENTNOTFOUND);
    /* each out parameter is optional */
    TEQ(ITypeInfo_GetDllEntry(ti, fd->memid, INVOKE_FUNC, NULL, NULL, NULL), S_OK);
    ITypeInfo_ReleaseFuncDesc(ti, fd);
    /* interface type infos have no DLL entries */
    ITypeInfo *ifc = get_ti(tl, L"IFont");
    if (ifc) {
        TEQ(ITypeInfo_GetDllEntry(ifc, 0x60000000, INVOKE_FUNC, &dll, &ent, &ord), TYPE_E_BADMODULEKIND);
        ITypeInfo_Release(ifc);
    }
    END_LIB(ti);
}

TEST(tlli_ti_addressofmember)
{
    WITH_LIB_AND(ti, L"StdFunctions");
    FUNCDESC *fd = NULL;
    TEQ(ITypeInfo_GetFuncDesc(ti, 1, &fd), S_OK);                /* SavePicture -> oleaut32!OleSavePictureFile */
    REQUIRE(fd);
    PVOID addr = NULL;
    HRESULT hr = ITypeInfo_AddressOfMember(ti, fd->memid, INVOKE_FUNC, &addr);
    if (!t_is_wine()) TEQ(hr, S_OK);
    if (hr == S_OK) {
        CHECK(addr != NULL);
        HMODULE m = GetModuleHandleW(L"oleaut32.dll");
        FARPROC p = m ? GetProcAddress(m, "OleSavePictureFile") : NULL;
        if (p) CHECK((PVOID)p == addr);
    }
    ITypeInfo_ReleaseFuncDesc(ti, fd);
    TEQ(ITypeInfo_AddressOfMember(ti, 0x6000000d, INVOKE_FUNC, &addr), TYPE_E_ELEMENTNOTFOUND);
    ITypeInfo *ifc = get_ti(tl, L"IFont");
    if (ifc) {
        addr = NULL;
        TEQ(ITypeInfo_AddressOfMember(ifc, 0x60000000, INVOKE_FUNC, &addr), TYPE_E_BADMODULEKIND);
        ITypeInfo_Release(ifc);
    }
    END_LIB(ti);
}

TEST(tlli_ti_createinstance_noncreatable)
{
    WITH_LIB_AND(ti, L"IFont");
    void *p = SENT_PTR;
    HRESULT hr = ITypeInfo_CreateInstance(ti, NULL, &TL_IID_IUnknown, &p);
    CHECK(FAILED(hr));
    t_note("ITypeInfo::CreateInstance on an interface -> 0x%08lx", (unsigned long)hr);   /* docs: TYPE_E_WRONGTYPEKIND */
    CHECK(p == NULL || p == SENT_PTR);
    END_LIB(ti);
}

TEST(tlli_ti_getmops)
{
    WITH_LIB_AND(ti, L"IUnknown");
    BSTR m = (BSTR)SENT_PTR;
    HRESULT hr = ITypeInfo_GetMops(ti, 0x60000000, &m);
    TEQ(hr, S_OK);
    CHECK(m == NULL);                 /* no mops for typelib-based type infos */
    TEQ(ITypeInfo_GetMops(ti, MEMBERID_NIL, &m), S_OK);
    END_LIB(ti);
}

TEST(tlli_ti_typecomp_identity)
{
    WITH_LIB_AND(ti, L"IFont");
    ITypeComp *tc = NULL, *tc2 = NULL;
    TEQ(ITypeInfo_GetTypeComp(ti, &tc), S_OK);
    TEQ(ITypeInfo_QueryInterface(ti, &TL_IID_ITypeComp, (void **)&tc2), S_OK);
    CHECK(tc == tc2);
    ITypeInfo *back = NULL;
    if (tc) {
        TEQ(ITypeComp_QueryInterface(tc, &TL_IID_ITypeInfo, (void **)&back), S_OK);
        CHECK(back == ti);
        if (back) ITypeInfo_Release(back);
        ITypeComp_Release(tc);
    }
    if (tc2) ITypeComp_Release(tc2);
    END_LIB(ti);
}

TEST(tlli_ti_queryinterface)
{
    WITH_LIB_AND(ti, L"IFont");
    static const struct { const GUID *g; int ok; const char *n; } t[] = {
        { &TL_IID_IUnknown, 1, "IUnknown" }, { &TL_IID_ITypeInfo, 1, "ITypeInfo" }, { &TL_IID_ITypeInfo2, 1, "ITypeInfo2" },
        { &TL_IID_ITypeComp, 1, "ITypeComp" }, { &TL_IID_IDispatch, 0, "IDispatch" }, { &TL_IID_ITypeLib, 0, "ITypeLib" },
    };
    for (unsigned i = 0; i < 6; i++) {
        void *p = SENT_PTR;
        HRESULT hr = ITypeInfo_QueryInterface(ti, t[i].g, &p);
        if (t[i].ok) { CHECKF(hr == S_OK && p && p != SENT_PTR, "%s hr=0x%08lx", t[i].n, (unsigned long)hr); if (hr == S_OK && p) IUnknown_Release((IUnknown *)p); }
        else { CHECKF(hr == E_NOINTERFACE, "%s hr=0x%08lx", t[i].n, (unsigned long)hr); CHECKF(p == NULL, "%s out", t[i].n); }
    }
    END_LIB(ti);
}
