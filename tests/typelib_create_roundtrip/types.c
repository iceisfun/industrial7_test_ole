/* ICreateTypeInfo for enums, records, unions, aliases, modules and variables; ITypeComp binding; DLL entries.
 * Hard CHECKs mirror behaviour exercised in Wine's typelib.c tests (which pass on Windows) or documented contracts;
 * implementation-derived expectations use SOFTHR/SOFTEQ. */
#include "tlc.h"

static HRESULT add_const(ICreateTypeInfo *c, UINT idx, MEMBERID id, VARTYPE tvt, VARIANT *val)
{
    VARDESC vd;
    memset(&vd, 0, sizeof vd);
    vd.memid = id;
    vd.elemdescVar.tdesc.vt = tvt;
    vd.varkind = VAR_CONST;
    vd.lpvarValue = val;
    return ICreateTypeInfo_AddVarDesc(c, idx, &vd);
}

static HRESULT add_field(ICreateTypeInfo *c, UINT idx, const TYPEDESC *td, ULONG oInst)
{
    VARDESC vd;
    memset(&vd, 0, sizeof vd);
    vd.memid = MEMBERID_NIL;
    vd.elemdescVar.tdesc = *td;
    vd.varkind = VAR_PERINSTANCE;
    vd.oInst = oInst;
    return ICreateTypeInfo_AddVarDesc(c, idx, &vd);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_SetVarHelpContext / test_SetDocString (enum part) */
TEST(tlt_var_helpcontext_and_docstring)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "vardoc.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"name", TKIND_ENUM);
    EQHR(ICreateTypeInfo_SetVarHelpContext(c, 0, 0), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetVarDocString(c, 0, (LPOLESTR)L"doc1"), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetVarDocString(c, 0, NULL), E_INVALIDARG);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_INT; V_INT(&v) = 1;
    OK_(add_const(c, 0, MEMBERID_NIL, VT_INT, &v));
    EQHR(ICreateTypeInfo_SetVarName(c, 0, NULL), E_INVALIDARG);
    EQHR(ICreateTypeInfo_SetVarName(c, 1, (LPOLESTR)L"varname"), TYPE_E_ELEMENTNOTFOUND);
    OK_(ICreateTypeInfo_SetVarName(c, 0, (LPOLESTR)L"varname"));
    EQHR(ICreateTypeInfo_SetVarDocString(c, 0, NULL), E_INVALIDARG);
    OK_(ICreateTypeInfo_SetVarDocString(c, 0, (LPOLESTR)L"doc1"));
    OK_(ICreateTypeInfo_SetVarDocString(c, 0, (LPOLESTR)L"doc2")); /* replace */
    EQHR(ICreateTypeInfo_SetVarDocString(c, 1, (LPOLESTR)L"doc1"), TYPE_E_ELEMENTNOTFOUND);
    OK_(ICreateTypeInfo_SetVarHelpContext(c, 0, 0));
    OK_(ICreateTypeInfo_SetVarHelpContext(c, 0, 1));
    EQHR(ICreateTypeInfo_SetVarHelpContext(c, 1, 0), TYPE_E_ELEMENTNOTFOUND);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        VARDESC *p = NULL;
        OK_(ITypeInfo_GetVarDesc(ti, 0, &p));
        if (p) {
            EQ(p->memid, 0x40000000);
            EQ(p->elemdescVar.tdesc.vt, VT_INT);
            EQ(p->varkind, VAR_CONST);
            CHECK(p->lpvarValue != NULL);
            if (p->lpvarValue) { EQ(V_VT(p->lpvarValue), VT_INT); EQ(V_INT(p->lpvarValue), 1); }
            BSTR nm = NULL, doc = NULL; DWORD hc = 0;
            OK_(ITypeInfo_GetDocumentation(ti, p->memid, &nm, &doc, &hc, NULL));
            CHECK(nm && !lstrcmpW(nm, L"varname"));
            CHECK(doc && !lstrcmpW(doc, L"doc2"));
            EQ(hc, 1);
            pSysFreeString(nm); pSysFreeString(doc);
            ITypeInfo_ReleaseVarDesc(ti, p);
        }
        EQHR(ITypeInfo_GetVarDesc(ti, 1, &p), TYPE_E_ELEMENTNOTFOUND);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_addvardesc_errors)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "avderr.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"E", TKIND_ENUM);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 5;
    VARDESC vd; memset(&vd, 0, sizeof vd);
    vd.elemdescVar.tdesc.vt = VT_I4; vd.varkind = VAR_CONST; vd.lpvarValue = &v;
    if (!t_is_wine()) SOFTHR(ICreateTypeInfo_AddVarDesc(c, 0, NULL), E_INVALIDARG);
    SOFTHR(ICreateTypeInfo_AddVarDesc(c, 1, &vd), TYPE_E_ELEMENTNOTFOUND);
    if (!t_is_wine()) { /* Wine accepts any index, which makes the count below differ */
        EQHR(ICreateTypeInfo_AddVarDesc(c, 0, &vd), S_OK);
        SOFTHR(ICreateTypeInfo_AddVarDesc(c, 2, &vd), TYPE_E_ELEMENTNOTFOUND);
    } else OK_(ICreateTypeInfo_AddVarDesc(c, 0, &vd));
    EQHR(ICreateTypeInfo_AddVarDesc(c, 1, &vd), S_OK);
    ITypeInfo *ti = tl_ti_of_cti(c);
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { SOFTEQ(ta->cVars, 2); EQ(ta->typekind, TKIND_ENUM); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlt_enum_roundtrip)
{
    TL_BEGIN();
    static const struct { const WCHAR *n; LONG v; } e[] = {
        {L"Red", 0}, {L"Green", 1}, {L"Blue", 2}, {L"Big", 0x7fffffff}, {L"Neg", -5}, {L"Min", (LONG)0x80000000} };
    for (int s = 0; s < 2; s++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(s ? SYS_WIN64 : SYS_WIN32, "enum.tlb", path);
        if (!ctl) return;
        ICreateTypeInfo *c = tl_cti(ctl, L"Color", TKIND_ENUM);
        OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_E));
        for (UINT i = 0; i < 6; i++) {
            VARIANT v; pVariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = e[i].v;
            OK_(add_const(c, i, MEMBERID_NIL, VT_I4, &v));
            OK_(ICreateTypeInfo_SetVarName(c, i, (LPOLESTR)e[i].n));
            WCHAR d[32]; wsprintfW(d, L"constant #%u", i);
            OK_(ICreateTypeInfo_SetVarDocString(c, i, d));
        }
        ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        ITypeInfo *ti = tl_ti(tl, 0);
        if (!ti) continue;
        TYPEATTR *ta = NULL;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) {
            EQ(ta->typekind, TKIND_ENUM);
            EQ(ta->cVars, 6);
            EQ(ta->cFuncs, 0);
            EQ(ta->cImplTypes, 0);
            EQ(ta->cbSizeVft, 0);
            SOFTEQ(ta->cbSizeInstance, 4);
            CHECK(IsEqualGUID(&ta->guid, &TLC_GUID_E));
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        for (UINT i = 0; i < 6; i++) {
            VARDESC *p = NULL;
            OK_(ITypeInfo_GetVarDesc(ti, i, &p));
            if (!p) continue;
            EQ(p->memid, 0x40000000 + i);
            EQ(p->varkind, VAR_CONST);
            EQ(p->elemdescVar.tdesc.vt, VT_I4);
            CHECK(p->lpvarValue != NULL);
            if (p->lpvarValue) { EQ(V_VT(p->lpvarValue), VT_I4); CHECKF(V_I4(p->lpvarValue) == e[i].v, "const %u = %ld", i, (long)V_I4(p->lpvarValue)); }
            BSTR nm = NULL, doc = NULL;
            OK_(ITypeInfo_GetDocumentation(ti, p->memid, &nm, &doc, NULL, NULL));
            CHECK(nm && !lstrcmpW(nm, e[i].n));
            WCHAR d[32]; wsprintfW(d, L"constant #%u", i);
            CHECK(doc && !lstrcmpW(doc, d));
            pSysFreeString(nm); pSysFreeString(doc);
            ITypeInfo_ReleaseVarDesc(ti, p);
        }
        /* name/memid lookups */
        MEMBERID id = 0;
        LPOLESTR look = (LPOLESTR)L"Blue";
        OK_(ITypeInfo_GetIDsOfNames(ti, &look, 1, &id));
        EQ(id, 0x40000002);
        look = (LPOLESTR)L"blue"; id = 0;
        OK_(ITypeInfo_GetIDsOfNames(ti, &look, 1, &id));
        EQ(id, 0x40000002);
        look = (LPOLESTR)L"Purple"; id = 0;
        EQHR(ITypeInfo_GetIDsOfNames(ti, &look, 1, &id), DISP_E_UNKNOWNNAME);
        EQ(id, (MEMBERID)DISPID_UNKNOWN);
        ITypeInfo_Release(ti);
        EQ(ITypeLib_Release(tl), 0);
    }
}

TEST(tlt_const_variants_roundtrip)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "consts.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"Consts", TKIND_MODULE);
    VARIANT v[7]; for (int i = 0; i < 7; i++) pVariantInit(&v[i]);
    V_VT(&v[0]) = VT_I2; V_I2(&v[0]) = -7;
    V_VT(&v[1]) = VT_I4; V_I4(&v[1]) = 123456;
    V_VT(&v[2]) = VT_BSTR; V_BSTR(&v[2]) = pSysAllocString(L"a string constant");
    V_VT(&v[3]) = VT_R8; V_R8(&v[3]) = 3.25;
    V_VT(&v[4]) = VT_UI1; V_UI1(&v[4]) = 200;
    V_VT(&v[5]) = VT_R4; V_R4(&v[5]) = -0.5f;
    V_VT(&v[6]) = VT_BOOL; V_BOOL(&v[6]) = VARIANT_TRUE;
    static const VARTYPE vt[7] = { VT_I2, VT_I4, VT_BSTR, VT_R8, VT_UI1, VT_R4, VT_BOOL };
    for (UINT i = 0; i < 7; i++) {
        HRESULT hr = add_const(c, i, 0x100 + i, vt[i], &v[i]);
        CHECKF(hr == S_OK, "const of vt %u: 0x%08lx", vt[i], (unsigned long)hr);
    }
    pVariantClear(&v[2]);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        for (UINT i = 0; i < 7; i++) {
            VARDESC *p = NULL;
            HRESULT hr = ITypeInfo_GetVarDesc(ti, i, &p);
            CHECKF(hr == S_OK, "GetVarDesc %u hr=0x%08lx", i, (unsigned long)hr);
            if (!p) continue;
            EQ(p->memid, 0x100 + i);
            EQ(p->varkind, VAR_CONST);
            VARIANT *r = p->lpvarValue;
            CHECK(r != NULL);
            if (r) {
                CHECKF(V_VT(r) == vt[i], "const %u vt %u want %u", i, V_VT(r), vt[i]);
                switch (i) {
                case 0: CHECK(V_I2(r) == -7); break;
                case 1: EQ(V_I4(r), 123456); break;
                case 2: CHECK(V_VT(r) == VT_BSTR && V_BSTR(r) && !lstrcmpW(V_BSTR(r), L"a string constant")); break;
                case 3: CHECK(V_R8(r) == 3.25); break;
                case 4: EQ(V_UI1(r), 200); break;
                case 5: CHECK(V_R4(r) == -0.5f); break;
                case 6: SOFTEQ((USHORT)V_BOOL(r), 0xffff); break;
                }
            }
            ITypeInfo_ReleaseVarDesc(ti, p);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_var_auto_memids)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "varid.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"E", TKIND_ENUM);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_I4;
    for (UINT i = 0; i < 4; i++) { V_I4(&v) = i; OK_(add_const(c, i, i == 2 ? 0x77 : MEMBERID_NIL, VT_I4, &v)); }
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        static const MEMBERID want[4] = { 0x40000000, 0x40000001, 0x77, 0x40000003 };
        for (UINT i = 0; i < 4; i++) {
            VARDESC *p = NULL;
            OK_(ITypeInfo_GetVarDesc(ti, i, &p));
            if (p) { CHECKF(p->memid == want[i], "var %u memid 0x%lx want 0x%lx", i, p->memid, want[i]); ITypeInfo_ReleaseVarDesc(ti, p); }
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

/* record with explicit offsets, both pointer widths */
TEST(tlt_record_fields_roundtrip)
{
    TL_BEGIN();
    for (int s = 0; s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        unsigned ps = tl_ptr(sk);
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "rec.tlb", path);
        if (!ctl) return;
        ICreateTypeInfo *c = tl_cti(ctl, L"Point3", TKIND_RECORD);
        OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_A));
        static const WCHAR *nm[3] = { L"x", L"y", L"z" };
        TYPEDESC td = { .vt = VT_I4 };
        for (UINT i = 0; i < 3; i++) {
            OK_(add_field(c, i, &td, 4 * i));
            OK_(ICreateTypeInfo_SetVarName(c, i, (LPOLESTR)nm[i]));
        }
        OK_(ICreateTypeInfo_LayOut(c));
        ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        ITypeInfo *ti = tl_ti(tl, 0);
        if (ti) {
            TYPEATTR *ta;
            OK_(ITypeInfo_GetTypeAttr(ti, &ta));
            if (ta) {
                EQ(ta->typekind, TKIND_RECORD);
                EQ(ta->cVars, 3);
                SOFTEQ(ta->cbSizeInstance, 12);
                EQ(ta->cbSizeVft, 0);
                EQ(ta->cFuncs, 0);
                CHECK(IsEqualGUID(&ta->guid, &TLC_GUID_A));
                SOFTEQ(ta->cbAlignment, 4);
                ITypeInfo_ReleaseTypeAttr(ti, ta);
            }
            for (UINT i = 0; i < 3; i++) {
                VARDESC *p = NULL;
                OK_(ITypeInfo_GetVarDesc(ti, i, &p));
                if (!p) continue;
                EQ(p->varkind, VAR_PERINSTANCE);
                EQ(p->oInst, 4 * i);
                EQ(p->elemdescVar.tdesc.vt, VT_I4);
                EQ(p->memid, 0x40000000 + i);
                ITypeInfo_ReleaseVarDesc(ti, p);
            }
            ITypeInfo_Release(ti);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlt_record_mixed_types_layout)
{
    TL_BEGIN();
    static const VARTYPE vts[] = { VT_UI1, VT_I2, VT_I4, VT_R8, VT_BSTR, VT_DISPATCH, VT_I8, VT_BOOL, VT_VARIANT, VT_UNKNOWN, VT_CY, VT_DATE };
    for (int s = 0; s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "recmix.tlb", path);
        if (!ctl) return;
        ICreateTypeInfo *c = tl_cti(ctl, L"Mixed", TKIND_RECORD);
        for (UINT i = 0; i < sizeof vts / sizeof vts[0]; i++) {
            TYPEDESC td = { .vt = vts[i] };
            /* offsets 0,... all supplied by the caller; no overlap checks expected */
            OK_(add_field(c, i, &td, i * 32));
        }
        OK_(ICreateTypeInfo_LayOut(c));
        ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        ITypeInfo *ti = tl_ti(tl, 0);
        if (ti) {
            TYPEATTR *ta;
            OK_(ITypeInfo_GetTypeAttr(ti, &ta));
            if (ta) {
                EQ(ta->cVars, 12);
                SOFTEQ(ta->cbSizeInstance >= 11 * 32, 1);
                t_note("%s record of 12 fields at 32-byte offsets: cbSizeInstance=%lu cbAlignment=%u", s ? "win64" : "win32", ta->cbSizeInstance, ta->cbAlignment);
                ITypeInfo_ReleaseTypeAttr(ti, ta);
            }
            for (UINT i = 0; i < 12; i++) {
                VARDESC *p = NULL;
                OK_(ITypeInfo_GetVarDesc(ti, i, &p));
                if (p) { CHECKF(p->oInst == i * 32 && p->elemdescVar.tdesc.vt == vts[i], "field %u oInst %lu vt %u", i, p->oInst, p->elemdescVar.tdesc.vt); ITypeInfo_ReleaseVarDesc(ti, p); }
            }
            ITypeInfo_Release(ti);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlt_union_roundtrip)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "union.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"U1", TKIND_UNION);
    static const VARTYPE vts[3] = { VT_I4, VT_R8, VT_UI1 };
    for (UINT i = 0; i < 3; i++) {
        TYPEDESC td = { .vt = vts[i] };
        OK_(add_field(c, i, &td, 0));
    }
    OK_(ICreateTypeInfo_LayOut(c));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) {
            EQ(ta->typekind, TKIND_UNION);
            EQ(ta->cVars, 3);
            SOFTEQ(ta->cbSizeInstance, 8);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        for (UINT i = 0; i < 3; i++) {
            VARDESC *p = NULL;
            OK_(ITypeInfo_GetVarDesc(ti, i, &p));
            if (p) { EQ(p->oInst, 0); EQ(p->elemdescVar.tdesc.vt, vts[i]); ITypeInfo_ReleaseVarDesc(ti, p); }
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_record_nested_userdefined_field)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "nested.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *inner = tl_cti(ctl, L"Inner", TKIND_RECORD);
    TYPEDESC i4 = { .vt = VT_I4 }, i2 = { .vt = VT_I2 };
    OK_(add_field(inner, 0, &i4, 0));
    OK_(add_field(inner, 1, &i4, 4));
    OK_(ICreateTypeInfo_LayOut(inner));
    ICreateTypeInfo *outer = tl_cti(ctl, L"Outer", TKIND_RECORD);
    ITypeInfo *tinner = tl_ti_of_cti(inner);
    HREFTYPE h = 0;
    OK_(ICreateTypeInfo_AddRefTypeInfo(outer, tinner, &h));
    TYPEDESC ud; ud.vt = VT_USERDEFINED; ud.hreftype = h;
    OK_(add_field(outer, 0, &i2, 0));
    OK_(add_field(outer, 1, &ud, 4));
    OK_(ICreateTypeInfo_LayOut(outer));
    ITypeInfo_Release(tinner);
    ICreateTypeInfo_Release(inner);
    ICreateTypeInfo_Release(outer);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *to = tl_ti(tl, 1);
    if (to) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(to, &ta));
        if (ta) { EQ(ta->cVars, 2); SOFTEQ(ta->cbSizeInstance, 12); ITypeInfo_ReleaseTypeAttr(to, ta); }
        VARDESC *p = NULL;
        OK_(ITypeInfo_GetVarDesc(to, 1, &p));
        if (p) {
            EQ(p->elemdescVar.tdesc.vt, VT_USERDEFINED);
            ITypeInfo *r = NULL;
            OK_(ITypeInfo_GetRefTypeInfo(to, p->elemdescVar.tdesc.hreftype, &r));
            if (r) { char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "Inner"); ITypeInfo_Release(r); }
            ITypeInfo_ReleaseVarDesc(to, p);
        }
        ITypeInfo_Release(to);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_dispinterface_properties_vars)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "dispvars.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"DProps", TKIND_DISPATCH);
    VARDESC vd; memset(&vd, 0, sizeof vd);
    vd.memid = 0x10; vd.varkind = VAR_DISPATCH; vd.elemdescVar.tdesc.vt = VT_BSTR; vd.wVarFlags = VARFLAG_FREADONLY;
    OK_(ICreateTypeInfo_AddVarDesc(c, 0, &vd));
    vd.memid = 0x11; vd.elemdescVar.tdesc.vt = VT_I4; vd.wVarFlags = VARFLAG_FBINDABLE | VARFLAG_FDISPLAYBIND;
    OK_(ICreateTypeInfo_AddVarDesc(c, 1, &vd));
    OK_(ICreateTypeInfo_SetVarName(c, 0, (LPOLESTR)L"Title"));
    OK_(ICreateTypeInfo_SetVarName(c, 1, (LPOLESTR)L"Count"));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cVars, 2); EQ(ta->typekind, TKIND_DISPATCH); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        VARDESC *p = NULL;
        OK_(ITypeInfo_GetVarDesc(ti, 0, &p));
        if (p) { EQ(p->memid, 0x10); EQ(p->varkind, VAR_DISPATCH); EQ(p->elemdescVar.tdesc.vt, VT_BSTR); EQ(p->wVarFlags, VARFLAG_FREADONLY); ITypeInfo_ReleaseVarDesc(ti, p); }
        OK_(ITypeInfo_GetVarDesc(ti, 1, &p));
        if (p) { EQ(p->memid, 0x11); EQ(p->wVarFlags, VARFLAG_FBINDABLE | VARFLAG_FDISPLAYBIND); ITypeInfo_ReleaseVarDesc(ti, p); }
        MEMBERID id = 0; LPOLESTR look = (LPOLESTR)L"count";
        OK_(ITypeInfo_GetIDsOfNames(ti, &look, 1, &id));
        EQ(id, 0x11);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_varflags_roundtrip)
{
    TL_BEGIN();
    static const WORD fl[] = { VARFLAG_FREADONLY, VARFLAG_FSOURCE, VARFLAG_FBINDABLE, VARFLAG_FREQUESTEDIT, VARFLAG_FDISPLAYBIND,
        VARFLAG_FDEFAULTBIND, VARFLAG_FHIDDEN, VARFLAG_FRESTRICTED, VARFLAG_FDEFAULTCOLLELEM, VARFLAG_FUIDEFAULT,
        VARFLAG_FNONBROWSABLE, VARFLAG_FREPLACEABLE, VARFLAG_FIMMEDIATEBIND };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "vfl.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"DFl", TKIND_DISPATCH);
    for (UINT i = 0; i < 13; i++) {
        VARDESC vd; memset(&vd, 0, sizeof vd);
        vd.memid = 0x200 + i; vd.varkind = VAR_DISPATCH; vd.elemdescVar.tdesc.vt = VT_I4; vd.wVarFlags = fl[i];
        OK_(ICreateTypeInfo_AddVarDesc(c, i, &vd));
    }
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        for (UINT i = 0; i < 13; i++) {
            VARDESC *p = NULL;
            OK_(ITypeInfo_GetVarDesc(ti, i, &p));
            if (p) { CHECKF(p->wVarFlags == fl[i], "var %u flags 0x%x want 0x%x", i, p->wVarFlags, fl[i]); ITypeInfo_ReleaseVarDesc(ti, p); }
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_delete_vardesc)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "delvar.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"E", TKIND_ENUM);
    ICreateTypeInfo2 *c2 = NULL;
    OK_(ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2));
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_I4;
    for (UINT i = 0; i < 4; i++) { V_I4(&v) = 10 + i; OK_(add_const(c, i, 0x300 + i, VT_I4, &v)); }
    if (c2) {
        EQHR(ICreateTypeInfo2_DeleteVarDesc(c2, 4), TYPE_E_ELEMENTNOTFOUND);
        OK_(ICreateTypeInfo2_DeleteVarDesc(c2, 1));
        ITypeInfo *ti = tl_ti_of_cti(c);
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cVars, 3); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        static const MEMBERID want[3] = { 0x300, 0x302, 0x303 };
        for (UINT i = 0; i < 3; i++) {
            VARDESC *p = NULL;
            OK_(ITypeInfo_GetVarDesc(ti, i, &p));
            if (p) { EQ(p->memid, want[i]); ITypeInfo_ReleaseVarDesc(ti, p); }
        }
        ITypeInfo_Release(ti);
        OK_(ICreateTypeInfo2_DeleteVarDesc(c2, 0));
        OK_(ICreateTypeInfo2_DeleteVarDesc(c2, 0));
        OK_(ICreateTypeInfo2_DeleteVarDesc(c2, 0));
        EQHR(ICreateTypeInfo2_DeleteVarDesc(c2, 0), TYPE_E_ELEMENTNOTFOUND);
        ICreateTypeInfo2_Release(c2);
    }
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlt_deletefuncdescbymemid_and_vardescbymemid)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "delmemid.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IDM", TKIND_INTERFACE);
    ICreateTypeInfo2 *c2 = NULL;
    OK_(ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2));
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 0x50, NULL, 0));
    OK_(tl_add_func(c, 1, INVOKE_PROPERTYGET, VT_I4, 0x51, NULL, 0));
    VARTYPE i4 = VT_I4;
    OK_(tl_add_func(c, 2, INVOKE_PROPERTYPUT, VT_VOID, 0x51, &i4, 1));
    if (c2) {
        SOFTHR(ICreateTypeInfo2_DeleteFuncDescByMemId(c2, 0x99, INVOKE_FUNC), TYPE_E_ELEMENTNOTFOUND);
        SOFTHR(ICreateTypeInfo2_DeleteFuncDescByMemId(c2, 0x51, INVOKE_PROPERTYGET), S_OK);
        ITypeInfo *ti = tl_ti_of_cti(c);
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { SOFTEQ(ta->cFuncs, 2); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        ITypeInfo_Release(ti);
        ICreateTypeInfo2_Release(c2);
    }
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_SetTypeDescAlias: size/alignment table */
static const struct tda { VARTYPE vt; ULONG size; WORD align; WORD align3264; int aux; } TDA[] = {
    { VT_I2, 2, 2, 2, 0 }, { VT_I4, 4, 4, 4, 0 }, { VT_R4, 4, 4, 4, 0 }, { VT_R8, 8, 4, 8, 0 }, { VT_CY, 8, 4, 8, 0 },
    { VT_DATE, 8, 4, 8, 0 }, { VT_BSTR, (ULONG)-1, 4, 8, 0 }, { VT_DISPATCH, (ULONG)-1, 4, 8, 0 }, { VT_ERROR, 4, 4, 4, 0 },
    { VT_BOOL, 2, 2, 2, 0 }, { VT_VARIANT, 0, 4, 8, 0 }, { VT_UNKNOWN, (ULONG)-1, 4, 8, 0 }, { VT_DECIMAL, 16, 4, 8, 0 },
    { VT_I1, 1, 1, 1, 0 }, { VT_UI1, 1, 1, 1, 0 }, { VT_UI2, 2, 2, 2, 0 }, { VT_UI4, 4, 4, 4, 0 }, { VT_I8, 8, 4, 8, 0 },
    { VT_UI8, 8, 4, 8, 0 }, { VT_INT, 4, 4, 4, 0 }, { VT_UINT, 4, 4, 4, 0 }, { VT_VOID, 0, 0, 0, 0 }, { VT_HRESULT, 4, 4, 4, 0 },
    { VT_PTR, (ULONG)-1, 4, 8, 1 }, { VT_SAFEARRAY, (ULONG)-1, 4, 8, 1 }, { VT_CARRAY, 16, 4, 4, 2 }, { VT_USERDEFINED, 0, 0, 0, 3 },
    { VT_LPSTR, (ULONG)-1, 4, 8, 0 }, { VT_LPWSTR, (ULONG)-1, 4, 8, 0 },
};

static void check_alias(ITypeInfo *ti, const struct tda *t, unsigned ptr_size, int created, ULONG href_size, WORD href_align)
{
    TYPEATTR *ta = NULL;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (!ta) return;
    ULONG size; WORD align;
    if (t->aux == 3) { size = href_size; align = href_align; }
    else {
        size = t->size;
        if (size == (ULONG)-1) size = created ? ptr_size : sizeof(void *);
        else if (t->vt == VT_VARIANT) {
            if (created) { size = sizeof(VARIANT); if (ptr_size != sizeof(void *)) size -= 8; }
            else size = sizeof(VARIANT);
        }
        align = t->align;
        if (!created && ptr_size != sizeof(void *)) align = t->align3264;
    }
    CHECKF(ta->cbSizeInstance == size, "alias VT %u: cbSizeInstance %lu want %lu (ptr %u, created %d)", t->vt, ta->cbSizeInstance, size, ptr_size, created);
    CHECKF(ta->cbAlignment == align, "alias VT %u: cbAlignment %u want %u (ptr %u, created %d)", t->vt, ta->cbAlignment, align, ptr_size, created);
    CHECKF(ta->tdescAlias.vt == t->vt, "alias VT %u: tdescAlias.vt %u", t->vt, ta->tdescAlias.vt);
    ITypeInfo_ReleaseTypeAttr(ti, ta);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_SetTypeDescAlias */
TEST(tlt_alias_sizes_alignments)
{
    TL_BEGIN();
    for (int s = 0; s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        unsigned ps = tl_ptr(sk);
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "alias.tlb", path);
        if (!ctl) return;
        ICreateTypeInfo *base = tl_cti(ctl, L"interface", TKIND_INTERFACE);
        ITypeInfo *tbase = tl_ti_of_cti(base);
        HREFTYPE href = 0;
        OK_(ICreateTypeInfo_AddRefTypeInfo(base, tbase, &href));
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(tbase, &ta));
        ULONG hsz = ta ? ta->cbSizeInstance : 0; WORD hal = ta ? ta->cbAlignment : 0;
        if (ta) ITypeInfo_ReleaseTypeAttr(tbase, ta);
        ITypeInfo_Release(tbase);
        ICreateTypeInfo_Release(base);
        unsigned n = sizeof TDA / sizeof TDA[0];
        for (unsigned i = 0; i < n; i++) {
            WCHAR nm[32]; wsprintfW(nm, L"alias%02u", TDA[i].vt);
            ICreateTypeInfo *c = tl_cti(ctl, nm, TKIND_ALIAS);
            if (!c) continue;
            TYPEDESC td, inner = { .vt = VT_INT };
            ARRAYDESC ad = { { { 0 }, VT_INT }, 1, { { 4, 0 } } };
            td.vt = TDA[i].vt;
            if (TDA[i].aux == 1) td.lptdesc = &inner;
            else if (TDA[i].aux == 2) td.lpadesc = &ad;
            else if (TDA[i].aux == 3) td.hreftype = href;
            OK_(ICreateTypeInfo_SetTypeDescAlias(c, &td));
            ITypeInfo *ti = tl_ti_of_cti(c);
            if (ti) { check_alias(ti, &TDA[i], ps, 1, hsz, hal); ITypeInfo_Release(ti); }
            ICreateTypeInfo_Release(c);
        }
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        ITypeInfo *t0 = tl_ti(tl, 0);
        if (t0) {
            OK_(ITypeInfo_GetTypeAttr(t0, &ta));
            if (ta) { hsz = ta->cbSizeInstance; hal = ta->cbAlignment; ITypeInfo_ReleaseTypeAttr(t0, ta); }
            ITypeInfo_Release(t0);
        }
        for (unsigned i = 0; i < n; i++) {
            WCHAR nm[32]; wsprintfW(nm, L"alias%02u", TDA[i].vt);
            ITypeInfo *ti = NULL; MEMBERID id; USHORT found = 1;
            OK_(ITypeLib_FindName(tl, nm, 0, &ti, &id, &found));
            if (ti) { check_alias(ti, &TDA[i], ps, 0, hsz, hal); ITypeInfo_Release(ti); }
        }
        EQ(ITypeLib_Release(tl), 0);
    }
}

TEST(tlt_alias_errors_and_chain)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "aliaserr.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"alias", TKIND_ALIAS);
    EQHR(ICreateTypeInfo_SetTypeDescAlias(c, NULL), E_INVALIDARG);
    TYPEDESC td = { .vt = VT_I1 };
    OK_(ICreateTypeInfo_SetTypeDescAlias(c, &td));
    ITypeInfo *ti = tl_ti_of_cti(c);
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { EQ(ta->cbSizeInstance, 1); EQ(ta->cbAlignment, 1); EQ(ta->typekind, TKIND_ALIAS); EQ(ta->tdescAlias.vt, VT_I1); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    td.vt = VT_R8;
    OK_(ICreateTypeInfo_SetTypeDescAlias(c, &td)); /* may be redefined */
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { EQ(ta->cbSizeInstance, 8); EQ(ta->cbAlignment, 4); EQ(ta->tdescAlias.vt, VT_R8); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    /* an alias of an alias */
    HREFTYPE h;
    ICreateTypeInfo *c2 = tl_cti(ctl, L"alias2", TKIND_ALIAS);
    OK_(ICreateTypeInfo_AddRefTypeInfo(c2, ti, &h));
    TYPEDESC ud; ud.vt = VT_USERDEFINED; ud.hreftype = h;
    OK_(ICreateTypeInfo_SetTypeDescAlias(c2, &ud));
    ITypeInfo *ti2 = tl_ti_of_cti(c2);
    OK_(ITypeInfo_GetTypeAttr(ti2, &ta));
    if (ta) { EQ(ta->typekind, TKIND_ALIAS); EQ(ta->tdescAlias.vt, VT_USERDEFINED); EQ(ta->cbSizeInstance, 8); ITypeInfo_ReleaseTypeAttr(ti2, ta); }
    ITypeInfo_Release(ti2); ICreateTypeInfo_Release(c2);
    ITypeInfo_Release(ti); ICreateTypeInfo_Release(c);
    /* alias description on record/enum/interface */
    static const TYPEKIND ks[] = { TKIND_RECORD, TKIND_ENUM, TKIND_INTERFACE, TKIND_DISPATCH, TKIND_COCLASS, TKIND_MODULE, TKIND_UNION };
    for (unsigned i = 0; i < 7; i++) {
        WCHAR n[8]; wsprintfW(n, L"k%u", i);
        ICreateTypeInfo *k = tl_cti(ctl, n, ks[i]);
        td.vt = VT_I4;
        if (k) { EQHR(ICreateTypeInfo_SetTypeDescAlias(k, &td), TYPE_E_BADMODULEKIND); ICreateTypeInfo_Release(k); }
    }
    ICreateTypeLib2_Release(ctl);
}

TEST(tlt_alias_reload_chain_resolves)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "aliaschain.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *rec = tl_cti(ctl, L"Rec", TKIND_RECORD);
    TYPEDESC i4 = { .vt = VT_I4 };
    OK_(add_field(rec, 0, &i4, 0));
    OK_(ICreateTypeInfo_LayOut(rec));
    ITypeInfo *trec = tl_ti_of_cti(rec);
    ICreateTypeInfo *al = tl_cti(ctl, L"PRec", TKIND_ALIAS);
    HREFTYPE h;
    OK_(ICreateTypeInfo_AddRefTypeInfo(al, trec, &h));
    TYPEDESC ud; ud.vt = VT_USERDEFINED; ud.hreftype = h;
    TYPEDESC pt; pt.vt = VT_PTR; pt.lptdesc = &ud;
    OK_(ICreateTypeInfo_SetTypeDescAlias(al, &pt));
    ITypeInfo_Release(trec);
    ICreateTypeInfo_Release(al);
    ICreateTypeInfo_Release(rec);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ta_ = tl_ti(tl, 1);
    if (ta_) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ta_, &ta));
        if (ta) {
            EQ(ta->typekind, TKIND_ALIAS);
            EQ(ta->tdescAlias.vt, VT_PTR);
            EQ(ta->cbSizeInstance, 8);
            if (ta->tdescAlias.vt == VT_PTR && ta->tdescAlias.lptdesc && ta->tdescAlias.lptdesc->vt == VT_USERDEFINED) {
                ITypeInfo *r = NULL;
                OK_(ITypeInfo_GetRefTypeInfo(ta_, ta->tdescAlias.lptdesc->hreftype, &r));
                if (r) { char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "Rec"); ITypeInfo_Release(r); }
            } else CHECK(0);
            ITypeInfo_ReleaseTypeAttr(ta_, ta);
        }
        ITypeInfo_Release(ta_);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_module_dll_entry_by_name)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "module.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"Kern", TKIND_MODULE);
    OK_(tl_add_func_k(c, 0, FUNC_STATIC, INVOKE_FUNC, VT_UI4, 0x1, NULL, 0));
    LPOLESTR nm[1] = { (LPOLESTR)L"PidOf" };
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, nm, 1));
    OK_(ICreateTypeInfo_DefineFuncAsDllEntry(c, 0, (LPOLESTR)L"kernel32.dll", (LPOLESTR)L"GetCurrentProcessId"));
    SOFTHR(ICreateTypeInfo_DefineFuncAsDllEntry(c, 1, (LPOLESTR)L"kernel32.dll", (LPOLESTR)L"x"), TYPE_E_ELEMENTNOTFOUND);
    OK_(tl_add_func_k(c, 1, FUNC_STATIC, INVOKE_FUNC, VT_UI4, 0x2, NULL, 0));
    nm[0] = (LPOLESTR)L"ByOrd";
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 1, nm, 1));
    OK_(ICreateTypeInfo_DefineFuncAsDllEntry(c, 1, (LPOLESTR)L"somelib.dll", (LPOLESTR)(ULONG_PTR)17));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->typekind, TKIND_MODULE); EQ(ta->cFuncs, 2); SOFTEQ(ta->cbSizeVft, 0); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        BSTR dll = NULL, name = NULL; WORD ord = 99;
        OK_(ITypeInfo_GetDllEntry(ti, 0x1, INVOKE_FUNC, &dll, &name, &ord));
        CHECK(dll && !lstrcmpW(dll, L"kernel32.dll"));
        CHECK(name && !lstrcmpW(name, L"GetCurrentProcessId"));
        pSysFreeString(dll); pSysFreeString(name);
        dll = name = NULL;
        OK_(ITypeInfo_GetDllEntry(ti, 0x2, INVOKE_FUNC, &dll, &name, &ord));
        CHECK(dll && !lstrcmpW(dll, L"somelib.dll"));
        EQ(ord, 17);
        t_note("ordinal entry: name=%ls ord=%u", name ? name : L"(null)", ord);
        pSysFreeString(dll); pSysFreeString(name);
        EQHR(ITypeInfo_GetDllEntry(ti, 0x99, INVOKE_FUNC, &dll, &name, &ord), TYPE_E_ELEMENTNOTFOUND);
        FUNCDESC *p = NULL;
        OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
        if (p) { EQ(p->funckind, FUNC_STATIC); EQ(p->elemdescFunc.tdesc.vt, VT_UI4); ITypeInfo_ReleaseFuncDesc(ti, p); }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_module_invoke_and_addressofmember)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "modinv.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"Kern", TKIND_MODULE);
    OK_(tl_add_func_k(c, 0, FUNC_STATIC, INVOKE_FUNC, VT_UI4, 0x1, NULL, 0));
    LPOLESTR nm[1] = { (LPOLESTR)L"PidOf" };
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, nm, 1));
    OK_(ICreateTypeInfo_DefineFuncAsDllEntry(c, 0, (LPOLESTR)L"kernel32.dll", (LPOLESTR)L"GetCurrentProcessId"));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        void *addr = NULL;
        HRESULT hr = ITypeInfo_AddressOfMember(ti, 0x1, INVOKE_FUNC, &addr);
        EQHR(hr, S_OK);
        FARPROC want = GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetCurrentProcessId");
        CHECK(addr == (void *)want);
        DISPPARAMS dp = { NULL, NULL, 0, 0 };
        VARIANT res; pVariantInit(&res);
        EXCEPINFO ex; memset(&ex, 0, sizeof ex);
        UINT err = 0;
        hr = ITypeInfo_Invoke(ti, NULL, 0x1, DISPATCH_METHOD, &dp, &res, &ex, &err);
        EQHR(hr, S_OK);
        if (hr == S_OK) { EQ(V_VT(&res), VT_UI4); EQ(V_UI4(&res), GetCurrentProcessId()); }
        pVariantClear(&res);
        EQHR(ITypeInfo_AddressOfMember(ti, 0x55, INVOKE_FUNC, &addr), TYPE_E_ELEMENTNOTFOUND);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_module_dll_entry_wrong_kind)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "dllwk.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IFoo", TKIND_INTERFACE);
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 1, NULL, 0));
    SOFTHR(ICreateTypeInfo_DefineFuncAsDllEntry(c, 0, (LPOLESTR)L"kernel32.dll", (LPOLESTR)L"GetTickCount"), TYPE_E_BADMODULEKIND);
    ITypeInfo *ti = tl_ti_of_cti(c);
    BSTR d = NULL, n = NULL; WORD o = 0;
    EQHR(ITypeInfo_GetDllEntry(ti, 1, INVOKE_FUNC, &d, &n, &o), TYPE_E_BADMODULEKIND);
    CHECK(d == NULL && n == NULL);
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlt_typeflags_roundtrip)
{
    TL_BEGIN();
    static const WORD fl[] = { TYPEFLAG_FAPPOBJECT, TYPEFLAG_FCANCREATE, TYPEFLAG_FLICENSED, TYPEFLAG_FPREDECLID, TYPEFLAG_FHIDDEN,
        TYPEFLAG_FCONTROL, TYPEFLAG_FOLEAUTOMATION, TYPEFLAG_FRESTRICTED, TYPEFLAG_FAGGREGATABLE, TYPEFLAG_FREPLACEABLE,
        TYPEFLAG_FNONEXTENSIBLE, TYPEFLAG_FPROXY };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "tflags.tlb", path);
    if (!ctl) return;
    for (unsigned i = 0; i < 12; i++) {
        WCHAR nm[8]; wsprintfW(nm, L"C%u", i);
        ICreateTypeInfo *c = tl_cti(ctl, nm, TKIND_COCLASS);
        if (c) { OK_(ICreateTypeInfo_SetTypeFlags(c, fl[i])); ICreateTypeInfo_Release(c); }
    }
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    for (unsigned i = 0; i < 12; i++) {
        ITypeInfo *ti = tl_ti(tl, i);
        TYPEATTR *ta = NULL;
        if (ti && ITypeInfo_GetTypeAttr(ti, &ta) == S_OK) {
            CHECKF((ta->wTypeFlags & fl[i]) == fl[i], "coclass %u flag 0x%x -> 0x%x", i, fl[i], ta->wTypeFlags);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        if (ti) ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlt_settypeflags_replaces_not_ors)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "tfrep.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"CoRep", TKIND_COCLASS);
    OK_(ICreateTypeInfo_SetTypeFlags(c, TYPEFLAG_FHIDDEN | TYPEFLAG_FCANCREATE));
    OK_(ICreateTypeInfo_SetTypeFlags(c, TYPEFLAG_FAPPOBJECT));
    ITypeInfo *ti = tl_ti_of_cti(c);
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { EQ(ta->wTypeFlags & (TYPEFLAG_FHIDDEN | TYPEFLAG_FCANCREATE), 0); EQ(ta->wTypeFlags & TYPEFLAG_FAPPOBJECT, TYPEFLAG_FAPPOBJECT); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlt_typecomp_bind)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "comp.tlb", path);
    if (!ctl || !unk) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IComp", TKIND_INTERFACE);
    HREFTYPE h;
    OK_(ICreateTypeInfo_AddRefTypeInfo(c, unk, &h));
    OK_(ICreateTypeInfo_AddImplType(c, 0, h));
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_HRESULT, 0x60, NULL, 0));
    LPOLESTR nm[1] = { (LPOLESTR)L"DoIt" };
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, nm, 1));
    ICreateTypeInfo *e = tl_cti(ctl, L"Mode", TKIND_ENUM);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 3;
    OK_(add_const(e, 0, 0x61, VT_I4, &v));
    OK_(ICreateTypeInfo_SetVarName(e, 0, (LPOLESTR)L"ModeFast"));
    ICreateTypeInfo_Release(c); ICreateTypeInfo_Release(e);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *ti = tl_ti(tl, 0), *te = tl_ti(tl, 1);
        ITypeComp *tc = NULL;
        if (ti) {
            OK_(ITypeInfo_GetTypeComp(ti, &tc));
            if (tc) {
                ITypeInfo *bti = NULL; DESCKIND dk = DESCKIND_NONE; BINDPTR bp; memset(&bp, 0, sizeof bp);
                OK_(ITypeComp_Bind(tc, (LPOLESTR)L"DoIt", 0, INVOKE_FUNC, &bti, &dk, &bp));
                EQ(dk, DESCKIND_FUNCDESC);
                if (dk == DESCKIND_FUNCDESC && bp.lpfuncdesc) { EQ(bp.lpfuncdesc->memid, 0x60); ITypeInfo_ReleaseFuncDesc(bti, bp.lpfuncdesc); }
                if (bti) ITypeInfo_Release(bti);
                bti = NULL; dk = DESCKIND_FUNCDESC;
                OK_(ITypeComp_Bind(tc, (LPOLESTR)L"NoSuch", 0, INVOKE_FUNC, &bti, &dk, &bp));
                EQ(dk, DESCKIND_NONE);
                CHECK(bti == NULL);
                ITypeComp_Release(tc);
            }
            ITypeInfo_Release(ti);
        }
        if (te) {
            tc = NULL;
            OK_(ITypeInfo_GetTypeComp(te, &tc));
            if (tc) {
                ITypeInfo *bti = NULL; DESCKIND dk = DESCKIND_NONE; BINDPTR bp; memset(&bp, 0, sizeof bp);
                OK_(ITypeComp_Bind(tc, (LPOLESTR)L"modefast", 0, INVOKE_PROPERTYGET, &bti, &dk, &bp));
                EQ(dk, DESCKIND_VARDESC);
                if (dk == DESCKIND_VARDESC && bp.lpvardesc) { EQ(bp.lpvardesc->memid, 0x61); ITypeInfo_ReleaseVarDesc(bti, bp.lpvardesc); }
                if (bti) ITypeInfo_Release(bti);
                ITypeComp_Release(tc);
            }
            ITypeInfo_Release(te);
        }
        /* library-level ITypeComp finds type names */
        ITypeComp *ltc = NULL;
        OK_(ITypeLib_GetTypeComp(tl, &ltc));
        if (ltc) {
            ITypeInfo *bti = NULL; DESCKIND dk = DESCKIND_NONE; BINDPTR bp; memset(&bp, 0, sizeof bp);
            OK_(ITypeComp_Bind(ltc, (LPOLESTR)L"Mode", 0, INVOKE_FUNC, &bti, &dk, &bp));
            t_note("lib ITypeComp::Bind(Mode) desckind=%d", dk);
            if (dk == DESCKIND_TYPECOMP && bp.lptcomp) ITypeComp_Release(bp.lptcomp);
            if (bti) ITypeInfo_Release(bti);
            ITypeComp_Release(ltc);
        }
        ITypeLib_Release(tl);
    }
    ITypeInfo_Release(unk); ITypeLib_Release(so);
}

TEST(tlt_setschema_setmops_setalignment)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "misc.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"Rec", TKIND_RECORD);
    TYPEDESC i4 = { .vt = VT_I4 };
    OK_(add_field(c, 0, &i4, 0));
    t_note("SetSchema hr=0x%08lx", (unsigned long)ICreateTypeInfo_SetSchema(c, (LPOLESTR)L"schema"));
    t_note("SetMops hr=0x%08lx", (unsigned long)ICreateTypeInfo_SetMops(c, 0, (LPOLESTR)L"mops"));
    t_note("SetMops(bad index) hr=0x%08lx", (unsigned long)ICreateTypeInfo_SetMops(c, 9, (LPOLESTR)L"mops"));
    static const WORD al[] = { 1, 2, 4, 8, 16 };
    for (unsigned i = 0; i < 5; i++) {
        HRESULT hr = ICreateTypeInfo_SetAlignment(c, al[i]);
        ITypeInfo *ti = tl_ti_of_cti(c);
        TYPEATTR *ta;
        if (ti && ITypeInfo_GetTypeAttr(ti, &ta) == S_OK) {
            t_note("SetAlignment(%u) hr=0x%08lx -> cbAlignment=%u", al[i], (unsigned long)hr, ta->cbAlignment);
            if (hr == S_OK) SOFTEQ(ta->cbAlignment, al[i]);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        if (ti) ITypeInfo_Release(ti);
    }
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlt_delete_typeinfo_note)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "deltype.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *a = tl_cti(ctl, L"A", TKIND_ENUM), *b = tl_cti(ctl, L"B", TKIND_ENUM);
    if (a) ICreateTypeInfo_Release(a);
    if (b) ICreateTypeInfo_Release(b);
    HRESULT hr = ICreateTypeLib2_DeleteTypeInfo(ctl, (LPOLESTR)L"A");
    t_note("DeleteTypeInfo(A) hr=0x%08lx", (unsigned long)hr);
    ITypeLib *tl = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, (void **)&tl));
    if (tl) {
        UINT n = ITypeLib_GetTypeInfoCount(tl);
        t_note("count after delete = %u", n);
        if (hr == S_OK) EQ(n, 1);
        ITypeLib_Release(tl);
    }
    SOFTHR(ICreateTypeLib2_DeleteTypeInfo(ctl, (LPOLESTR)L"Zzz"), TYPE_E_ELEMENTNOTFOUND);
    ICreateTypeLib2_Release(ctl);
}
