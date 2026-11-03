/* Inheritance chains, ITypeInfo2 accessors, reference resolution errors and user-defined types in created libraries. */
#include "tlc.h"

TEST(tlx_inheritance_chain_vft)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    for (int s = 0; unk && s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        unsigned ps = tl_ptr(sk);
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "chain.tlb", path);
        if (!ctl) break;
        ICreateTypeInfo *ib = tl_cti(ctl, L"IBase", TKIND_INTERFACE);
        ICreateTypeInfo *id = tl_cti(ctl, L"IDerived", TKIND_INTERFACE);
        ICreateTypeInfo *ie = tl_cti(ctl, L"IMore", TKIND_INTERFACE);
        ITypeInfo *tb = tl_ti_of_cti(ib), *td = tl_ti_of_cti(id);
        HREFTYPE h;
        OK_(ICreateTypeInfo_AddRefTypeInfo(ib, unk, &h));
        OK_(ICreateTypeInfo_AddImplType(ib, 0, h));
        OK_(tl_add_func(ib, 0, INVOKE_FUNC, VT_HRESULT, 0x10, NULL, 0));
        OK_(tl_add_func(ib, 1, INVOKE_FUNC, VT_HRESULT, 0x11, NULL, 0));
        OK_(ICreateTypeInfo_AddRefTypeInfo(id, tb, &h));
        OK_(ICreateTypeInfo_AddImplType(id, 0, h));
        OK_(tl_add_func(id, 0, INVOKE_FUNC, VT_HRESULT, 0x20, NULL, 0));
        OK_(ICreateTypeInfo_AddRefTypeInfo(ie, td, &h));
        OK_(ICreateTypeInfo_AddImplType(ie, 0, h));
        OK_(tl_add_func(ie, 0, INVOKE_FUNC, VT_HRESULT, 0x30, NULL, 0));
        OK_(tl_add_func(ie, 1, INVOKE_FUNC, VT_HRESULT, 0x31, NULL, 0));
        ITypeInfo_Release(tb); ITypeInfo_Release(td);
        ICreateTypeInfo_Release(ib); ICreateTypeInfo_Release(id); ICreateTypeInfo_Release(ie);
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) break;
        static const UINT nf[3] = { 2, 1, 2 };
        static const UINT vft[3] = { 5, 6, 8 };
        for (UINT i = 0; i < 3; i++) {
            ITypeInfo *ti = tl_ti(tl, i);
            if (!ti) continue;
            TYPEATTR *ta;
            OK_(ITypeInfo_GetTypeAttr(ti, &ta));
            if (ta) {
                EQ(ta->cFuncs, nf[i]);
                EQ(ta->cImplTypes, 1);
                CHECKF(ta->cbSizeVft == vft[i] * ps, "type %u cbSizeVft %u want %u", i, ta->cbSizeVft, vft[i] * ps);
                ITypeInfo_ReleaseTypeAttr(ti, ta);
            }
            FUNCDESC *fd = NULL;
            OK_(ITypeInfo_GetFuncDesc(ti, 0, &fd));
            if (fd) {
                UINT first = i == 0 ? 3 : i == 1 ? 5 : 6;
                CHECKF(fd->oVft == first * ps, "type %u func 0 oVft %d want %u", i, fd->oVft, first * ps);
                ITypeInfo_ReleaseFuncDesc(ti, fd);
            }
            ITypeInfo_Release(ti);
        }
        /* GetIDsOfNames / member lookup does not walk the inheritance chain but GetRefTypeInfo does */
        ITypeInfo *tm = tl_ti(tl, 2);
        if (tm) {
            HREFTYPE rh; ITypeInfo *r = NULL;
            OK_(ITypeInfo_GetRefTypeOfImplType(tm, 0, &rh));
            OK_(ITypeInfo_GetRefTypeInfo(tm, rh, &r));
            if (r) {
                char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "IDerived");
                HREFTYPE rh2; ITypeInfo *r2 = NULL;
                OK_(ITypeInfo_GetRefTypeOfImplType(r, 0, &rh2));
                OK_(ITypeInfo_GetRefTypeInfo(r, rh2, &r2));
                if (r2) { tl_doc_name(r2, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "IBase"); ITypeInfo_Release(r2); }
                ITypeInfo_Release(r);
            }
            ITypeInfo_Release(tm);
        }
        ITypeLib_Release(tl);
    }
    if (unk) ITypeInfo_Release(unk);
    ITypeLib_Release(so);
}

TEST(tlx_getrefTypeinfo_invalid_hreftype)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "badhref.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IH", TKIND_INTERFACE);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        static const HREFTYPE bad[] = { 4, 8, 0x1000, 0x7ffffffc, 3, 5 };
        for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            ITypeInfo *r = (ITypeInfo *)1;
            HRESULT hr = ITypeInfo_GetRefTypeInfo(ti, bad[i], &r);
            CHECKF(FAILED(hr), "GetRefTypeInfo(0x%lx) unexpectedly succeeded", bad[i]);
            t_note("GetRefTypeInfo(0x%lx) hr=0x%08lx", bad[i], (unsigned long)hr);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlx_typeinfo2_accessors)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "ti2.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IT2", TKIND_INTERFACE);
    OK_(ICreateTypeInfo_SetTypeFlags(c, TYPEFLAG_FHIDDEN | TYPEFLAG_FOLEAUTOMATION));
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_HRESULT, 0x44, NULL, 0));
    OK_(tl_add_func(c, 1, INVOKE_PROPERTYGET, VT_I4, 0x45, NULL, 0));
    VARTYPE i4 = VT_I4;
    OK_(tl_add_func(c, 2, INVOKE_PROPERTYPUT, VT_VOID, 0x45, &i4, 1));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    ITypeInfo2 *t2 = NULL;
    if (ti && ITypeInfo_QueryInterface(ti, &IID_ITypeInfo2, (void **)&t2) == S_OK) {
        TYPEKIND k = (TYPEKIND)99;
        OK_(ITypeInfo2_GetTypeKind(t2, &k)); EQ(k, TKIND_INTERFACE);
        if (!t_is_wine()) SOFTHR(ITypeInfo2_GetTypeKind(t2, NULL), E_INVALIDARG);
        ULONG fl = 0;
        OK_(ITypeInfo2_GetTypeFlags(t2, &fl));
        EQ(fl & (TYPEFLAG_FHIDDEN | TYPEFLAG_FOLEAUTOMATION), TYPEFLAG_FHIDDEN | TYPEFLAG_FOLEAUTOMATION);
        UINT idx = 99;
        OK_(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x44, INVOKE_FUNC, &idx)); EQ(idx, 0);
        OK_(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x45, INVOKE_PROPERTYGET, &idx)); EQ(idx, 1);
        OK_(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x45, INVOKE_PROPERTYPUT, &idx)); EQ(idx, 2);
        EQHR(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x45, INVOKE_FUNC, &idx), TYPE_E_ELEMENTNOTFOUND);
        EQHR(ITypeInfo2_GetFuncIndexOfMemId(t2, 0x99, INVOKE_FUNC, &idx), TYPE_E_ELEMENTNOTFOUND);
        SOFTHR(ITypeInfo2_GetVarIndexOfMemId(t2, 0x44, &idx), TYPE_E_ELEMENTNOTFOUND);
        ITypeInfo2_Release(t2);
    }
    if (ti) ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

TEST(tlx_userdefined_param_types)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "udt.tlb", path);
    if (!ctl || !unk) return;
    ICreateTypeInfo *e = tl_cti(ctl, L"Mode", TKIND_ENUM);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 1;
    VARDESC vd; memset(&vd, 0, sizeof vd);
    vd.elemdescVar.tdesc.vt = VT_I4; vd.varkind = VAR_CONST; vd.lpvarValue = &v;
    OK_(ICreateTypeInfo_AddVarDesc(e, 0, &vd));
    ITypeInfo *te = tl_ti_of_cti(e);
    ICreateTypeInfo *c = tl_cti(ctl, L"IUdt", TKIND_INTERFACE);
    HREFTYPE he, hu, hh;
    OK_(ICreateTypeInfo_AddRefTypeInfo(c, unk, &hu));
    OK_(ICreateTypeInfo_AddImplType(c, 0, hu));
    OK_(ICreateTypeInfo_AddRefTypeInfo(c, te, &he));
    ELEMDESC el[2]; memset(el, 0, sizeof el);
    TYPEDESC ud_e; ud_e.vt = VT_USERDEFINED; ud_e.hreftype = he;
    TYPEDESC ud_u; ud_u.vt = VT_USERDEFINED; ud_u.hreftype = hu;
    TYPEDESC pu; pu.vt = VT_PTR; pu.lptdesc = &ud_u;
    el[0].tdesc = ud_e; el[0].paramdesc.wParamFlags = PARAMFLAG_FIN;
    el[1].tdesc.vt = VT_PTR; el[1].tdesc.lptdesc = &pu; el[1].paramdesc.wParamFlags = PARAMFLAG_FOUT | PARAMFLAG_FRETVAL;
    FUNCDESC fd; memset(&fd, 0, sizeof fd);
    fd.memid = 0x70; fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL;
    fd.elemdescFunc.tdesc.vt = VT_HRESULT; fd.cParams = 2; fd.lprgelemdescParam = el;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    (void)hh;
    ITypeInfo_Release(te);
    ICreateTypeInfo_Release(e); ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *ti = tl_ti(tl, 1);
        if (ti) {
            FUNCDESC *p = NULL;
            OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
            if (p && p->lprgelemdescParam) {
                EQ(p->lprgelemdescParam[0].tdesc.vt, VT_USERDEFINED);
                ITypeInfo *r = NULL;
                OK_(ITypeInfo_GetRefTypeInfo(ti, p->lprgelemdescParam[0].tdesc.hreftype, &r));
                if (r) { char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "Mode"); ITypeInfo_Release(r); }
                TYPEDESC *t = &p->lprgelemdescParam[1].tdesc;
                EQ(t->vt, VT_PTR);
                CHECK(t->lptdesc && t->lptdesc->vt == VT_PTR && t->lptdesc->lptdesc && t->lptdesc->lptdesc->vt == VT_USERDEFINED);
                if (t->lptdesc && t->lptdesc->lptdesc) {
                    OK_(ITypeInfo_GetRefTypeInfo(ti, t->lptdesc->lptdesc->hreftype, &r));
                    if (r) {
                        TYPEATTR *ta;
                        OK_(ITypeInfo_GetTypeAttr(r, &ta));
                        if (ta) { CHECK(IsEqualGUID(&ta->guid, &IID_IUnknown)); ITypeInfo_ReleaseTypeAttr(r, ta); }
                        ITypeInfo_Release(r);
                    }
                }
            } else CHECK(0);
            if (p) ITypeInfo_ReleaseFuncDesc(ti, p);
            ITypeInfo_Release(ti);
        }
        ITypeLib_Release(tl);
    }
    ITypeInfo_Release(unk); ITypeLib_Release(so);
}

TEST(tlx_getdocumentation_bad_memid)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "gdoc.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IG", TKIND_INTERFACE);
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 0x10, NULL, 0));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        BSTR n = (BSTR)1;
        HRESULT hr = ITypeInfo_GetDocumentation(ti, 0x10, &n, NULL, NULL, NULL);
        EQHR(hr, S_OK);
        CHECK(n == NULL); /* the function has no name */
        n = (BSTR)1;
        EQHR(ITypeInfo_GetDocumentation(ti, 0x11, &n, NULL, NULL, NULL), TYPE_E_ELEMENTNOTFOUND);
        EQHR(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, NULL, NULL, NULL, NULL), S_OK);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlx_many_typeinfos_and_funcs)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "many.tlb", path);
    if (!ctl) return;
    enum { NT = 40, NF = 30 };
    for (int i = 0; i < NT; i++) {
        WCHAR n[16]; wsprintfW(n, L"Itf%03d", i);
        ICreateTypeInfo *c = tl_cti(ctl, n, i & 1 ? TKIND_DISPATCH : TKIND_INTERFACE);
        if (!c) continue;
        for (UINT f = 0; f < NF; f++) {
            HRESULT hr = tl_add_func_k(c, f, i & 1 ? FUNC_DISPATCH : FUNC_PUREVIRTUAL, INVOKE_FUNC, VT_VOID, 0x1000 + f, NULL, 0);
            if (hr != S_OK) { CHECKF(0, "type %d func %u hr=0x%08lx", i, f, (unsigned long)hr); break; }
            WCHAR fn[16]; wsprintfW(fn, L"M%d_%u", i, f);
            LPOLESTR nm[1] = { fn };
            OK_(ICreateTypeInfo_SetFuncAndParamNames(c, f, nm, 1));
        }
        ICreateTypeInfo_Release(c);
    }
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    EQ(ITypeLib_GetTypeInfoCount(tl), NT);
    for (int i = 0; i < NT; i++) {
        ITypeInfo *ti = tl_ti(tl, i);
        if (!ti) continue;
        WCHAR want[16]; wsprintfW(want, L"M%d_%d", i, 17);
        MEMBERID id = 0; LPOLESTR look = want;
        HRESULT hr = ITypeInfo_GetIDsOfNames(ti, &look, 1, &id);
        CHECKF(hr == S_OK && id == 0x1000 + 17, "type %d lookup hr=0x%08lx id=0x%lx", i, (unsigned long)hr, id);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlx_unicode_names_roundtrip)
{
    TL_BEGIN();
    /* Latin-1 range only: MSFT type libraries store names in the code page of the library LCID */
    static const WCHAR uni[] = { 0x00e9, 0x00fc, 'N', 'a', 'm', 'e', 0x00df, 0 };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "uni.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetName(ctl, (LPOLESTR)uni));
    OK_(ICreateTypeLib2_SetDocString(ctl, (LPOLESTR)uni));
    ICreateTypeInfo *c = tl_cti(ctl, uni, TKIND_ENUM);
    if (c) { OK_(ICreateTypeInfo_SetDocString(c, (LPOLESTR)uni)); ICreateTypeInfo_Release(c); }
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    BSTR n = NULL, d = NULL;
    OK_(ITypeLib_GetDocumentation(tl, -1, &n, &d, NULL, NULL));
    SOFTEQ(n && !lstrcmpW(n, uni), 1);
    SOFTEQ(d && !lstrcmpW(d, uni), 1);
    pSysFreeString(n); pSysFreeString(d);
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        OK_(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &n, &d, NULL, NULL));
        SOFTEQ(n && !lstrcmpW(n, uni), 1);
        SOFTEQ(d && !lstrcmpW(d, uni), 1);
        pSysFreeString(n); pSysFreeString(d);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlx_long_docstring)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "strs.tlb", path);
    if (!ctl) return;
    WCHAR *big = malloc(5001 * sizeof(WCHAR));
    for (int i = 0; i < 5000; i++) big[i] = L'a' + (i % 26);
    big[5000] = 0;
    HRESULT hr = ICreateTypeLib2_SetDocString(ctl, big);
    EQHR(hr, S_OK);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        BSTR d = NULL;
        OK_(ITypeLib_GetDocumentation(tl, -1, NULL, &d, NULL, NULL));
        CHECK(d && !lstrcmpW(d, big));
        EQ(pSysStringLen(d), 5000);
        pSysFreeString(d);
        ITypeLib_Release(tl);
    }
    free(big);
}

TEST(tlx_typeinfo_getmops_and_names_note)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "mops.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"R", TKIND_RECORD);
    TYPEDESC i4 = { .vt = VT_I4 };
    VARDESC vd; memset(&vd, 0, sizeof vd);
    vd.elemdescVar.tdesc = i4; vd.varkind = VAR_PERINSTANCE;
    OK_(ICreateTypeInfo_AddVarDesc(c, 0, &vd));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *ti = tl_ti(tl, 0);
        if (ti) {
            BSTR m = (BSTR)1;
            HRESULT hr = ITypeInfo_GetMops(ti, 0, &m);
            t_note("GetMops(0) hr=0x%08lx mops=%p", (unsigned long)hr, (void *)m);
            if (hr == S_OK) pSysFreeString(m);
            SOFTHR(ITypeInfo_GetMops(ti, 0x55, &m), TYPE_E_ELEMENTNOTFOUND);
            ITypeInfo_Release(ti);
        }
        ITypeLib_Release(tl);
    }
}
