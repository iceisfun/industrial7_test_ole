/* dtd_ti_*: CreateDispTypeInfo and the ITypeInfo it returns (CDispTypeInfo).
 * Decomp-verified layout: CreateDispTypeInfo returns a TKIND_COCLASS(5) wrapper with one implemented interface (href 0);
 * GetRefTypeInfo(0) yields the TKIND_INTERFACE(3) typeinfo that carries cFuncs/FUNCDESCs.  The INTERFACEDATA is *not* copied
 * (kept by pointer).  GetIDsOfNames / Invoke / GetDocumentation work on both kinds; GetFuncDesc / GetNames need the interface kind.
 * Unimplemented members return E_NOTIMPL (GetVarDesc: DISP_E_MEMBERNOTFOUND). */
#include "dtd.h"

static ITypeInfo *iface_of(ITypeInfo *co)
{
    HREFTYPE href = 0xcccc; ITypeInfo *ti = NULL; HRESULT hr;
    hr = ITypeInfo_GetRefTypeOfImplType(co, 0, &href);
    CHECKF(hr == S_OK && href == 0, "GetRefTypeOfImplType hr=%08lx href=%lx", (unsigned long)hr, (unsigned long)href);
    hr = ITypeInfo_GetRefTypeInfo(co, href, &ti);
    CHECKF(hr == S_OK && ti, "GetRefTypeInfo hr=%08lx", (unsigned long)hr);
    return hr == S_OK ? ti : NULL;
}

TEST(dtd_ti_create_returns_coclass_refcount_one)
{
    ITypeInfo *ti; ULONG r;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    r = ITypeInfo_AddRef(ti);  EXPECT_EQ(r, 2);
    r = ITypeInfo_Release(ti); EXPECT_EQ(r, 1);
    r = ITypeInfo_Release(ti); EXPECT_EQ(r, 0);
}

TEST(dtd_ti_create_does_not_copy_interfacedata)
{
    /* the typeinfo holds the caller's INTERFACEDATA pointer: later edits are visible */
    static PARAMDATA pd[1] = { { (OLECHAR *)L"p", VT_I4 } };
    static OLECHAR n0[] = L"First", n1[] = L"Renamed";
    static METHODDATA md[1];
    INTERFACEDATA id; ITypeInfo *ti = NULL; DISPID dispid = 0; OLECHAR *nm = n1; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    md[0].szName = n0; md[0].ppdata = pd; md[0].dispid = 0x77; md[0].iMeth = 3; md[0].cc = CC_STDCALL;
    md[0].cArgs = 1; md[0].wFlags = DISPATCH_METHOD; md[0].vtReturn = VT_I4;
    id.pmethdata = md; id.cMembers = 1;
    hr = OA(CreateDispTypeInfo)(&id, 0x409, &ti);
    EXPECT_EQ(hr, S_OK); if (!ti) return;
    nm = n1;
    EXPECT_EQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &dispid), DISP_E_UNKNOWNNAME);
    md[0].szName = n1;                      /* edit the caller's table */
    dispid = 0;
    EXPECT_EQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &dispid), S_OK);
    EXPECT_EQ(dispid, 0x77);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_create_zero_members)
{
    INTERFACEDATA id; ITypeInfo *ti = NULL, *itf; TYPEATTR *ta = NULL; OLECHAR *nm = (OLECHAR *)L"x"; DISPID d = 5; FUNCDESC *fd = NULL;
    REQUIRE(OA(CreateDispTypeInfo));
    id.pmethdata = NULL; id.cMembers = 0;
    EXPECT_EQ(OA(CreateDispTypeInfo)(&id, 0x409, &ti), S_OK); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetTypeAttr(itf, &ta), S_OK);
        if (ta) { EXPECT_EQ(ta->cFuncs, 0); ITypeInfo_ReleaseTypeAttr(itf, ta); }
        EXPECT_EQ(ITypeInfo_GetFuncDesc(itf, 0, &fd), DISP_E_MEMBERNOTFOUND);
        ITypeInfo_Release(itf);
    }
    EXPECT_EQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &d), DISP_E_UNKNOWNNAME);
    EXPECT_EQ(d, -1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_qi)
{
    ITypeInfo *ti, *p = NULL; IUnknown *u = NULL; void *x = (void *)1; HRESULT hr;
    static const GUID *bad[] = { &DI_IDispatch, &DI_ITypeInfo2, &DI_IMarshal, &DI_NULL, &DI_IClassFactory };
    unsigned i;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_QueryInterface(ti, &DI_ITypeInfo, (void **)&p), S_OK);
    CHECK(p == ti); if (p) EXPECT_EQ(ITypeInfo_Release(p), 1);
    EXPECT_EQ(ITypeInfo_QueryInterface(ti, &DI_IUnknown, (void **)&u), S_OK);
    CHECK((void *)u == (void *)ti); if (u) EXPECT_EQ(IUnknown_Release(u), 1);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        x = (void *)1;
        hr = ITypeInfo_QueryInterface(ti, bad[i], &x);
        CHECKF(hr == E_NOINTERFACE && x == NULL, "i=%u hr=%08lx", i, (unsigned long)hr);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_coclass_typeattr)
{
    ITypeInfo *ti; TYPEATTR *ta = NULL; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x407); if (!ti) return;
    hr = ITypeInfo_GetTypeAttr(ti, &ta);
    EXPECT_EQ(hr, S_OK);
    if (ta) {
        EXPECT_EQ(ta->typekind, TKIND_COCLASS);
        EXPECT_EQ(ta->cImplTypes, 1);
        EXPECT_EQ(ta->cFuncs, 0);
        EXPECT_EQ(ta->cVars, 0);
        EXPECT_EQ(ta->lcid, 0x407);
        EXPECT_EQ(ta->wMajorVerNum, 0); EXPECT_EQ(ta->wMinorVerNum, 0);
        EXPECT_EQ(ta->wTypeFlags, 0);
        EXPECT_EQ(ta->cbAlignment, 2);
        EXPECT_EQ(ta->cbSizeVft, 0xffff);
        EXPECT_EQ(ta->cbSizeInstance, 0xffff);
        EXPECT_EQ(ta->memidConstructor, MEMBERID_NIL); EXPECT_EQ(ta->memidDestructor, MEMBERID_NIL);
        CHECK(IsEqualGUID(&ta->guid, &DI_NULL));
        EXPECT_EQ(ta->idldescType.wIDLFlags, 0);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_interface_typeattr)
{
    ITypeInfo *ti, *itf; TYPEATTR *ta = NULL;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x40c); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetTypeAttr(itf, &ta), S_OK);
        if (ta) {
            EXPECT_EQ(ta->typekind, TKIND_INTERFACE);
            EXPECT_EQ(ta->cFuncs, demo_nmeth);
            EXPECT_EQ(ta->cImplTypes, 0);
            EXPECT_EQ(ta->cVars, 0);
            EXPECT_EQ(ta->lcid, 0x40c);
            EXPECT_EQ(ta->cbAlignment, 2);
            EXPECT_EQ(ta->cbSizeVft, 0xffff);
            EXPECT_EQ(ta->cbSizeInstance, 0xffff);
            EXPECT_EQ(ta->memidConstructor, MEMBERID_NIL); EXPECT_EQ(ta->memidDestructor, MEMBERID_NIL);
            EXPECT_EQ(ta->wTypeFlags, 0);
            CHECK(IsEqualGUID(&ta->guid, &DI_NULL));
            ITypeInfo_ReleaseTypeAttr(itf, ta);
        }
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_lcid_is_stored_verbatim)
{
    static const LCID l[] = { 0, 0x409, 0x800, 0x407, 0x1234, 0xffff };
    unsigned i;
    REQUIRE(OA(CreateDispTypeInfo));
    for (i = 0; i < sizeof l / sizeof l[0]; i++) {
        ITypeInfo *ti = demo_typeinfo(l[i]); TYPEATTR *ta = NULL;
        if (!ti) return;
        if (ITypeInfo_GetTypeAttr(ti, &ta) == S_OK && ta) { CHECKF(ta->lcid == l[i], "lcid %lx got %lx", (unsigned long)l[i], (unsigned long)ta->lcid); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        ITypeInfo_Release(ti);
    }
}

TEST(dtd_ti_coclass_impl_type_queries)
{
    ITypeInfo *ti, *itf = NULL; HREFTYPE h = 0xcccc; int flags = 0x55;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_GetRefTypeOfImplType(ti, 0, &h), S_OK);
    EXPECT_EQ(h, 0);
    EXPECT_EQ(ITypeInfo_GetRefTypeInfo(ti, 0, &itf), S_OK);
    CHECK(itf != NULL && itf != ti);
    if (itf) ITypeInfo_Release(itf);
    itf = (ITypeInfo *)1;
    EXPECT_EQ(ITypeInfo_GetRefTypeInfo(ti, 1, &itf), HR_E_FAIL);
    EXPECT_EQ(ITypeInfo_GetRefTypeInfo(ti, 0x80000000, &itf), HR_E_FAIL);
    EXPECT_EQ(ITypeInfo_GetImplTypeFlags(ti, 0, &flags), HR_E_NOTIMPL);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_interface_ref_queries_fail)
{
    ITypeInfo *ti, *itf, *x = NULL; HREFTYPE h = 0xcccc;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetRefTypeOfImplType(itf, 0, &h), HR_E_UNEXPECTED);
        EXPECT_EQ(ITypeInfo_GetRefTypeInfo(itf, 0, &x), HR_E_FAIL);
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_interface_typeinfo_is_new_object_each_time)
{
    ITypeInfo *ti, *a, *b;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    a = iface_of(ti); b = iface_of(ti);
    CHECK(a && b && a != b);
    if (a) EXPECT_EQ(ITypeInfo_Release(a), 0);
    if (b) EXPECT_EQ(ITypeInfo_Release(b), 0);
    EXPECT_EQ(ITypeInfo_Release(ti), 0);
}

TEST(dtd_ti_funcdesc_table)
{
    ITypeInfo *ti, *itf; UINT i; INTERFACEDATA *id = demo_idata();
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti); if (!itf) { ITypeInfo_Release(ti); return; }
    for (i = 0; i < id->cMembers; i++) {
        METHODDATA *md = &id->pmethdata[i]; FUNCDESC *fd = NULL; HRESULT hr; UINT p;
        hr = ITypeInfo_GetFuncDesc(itf, i, &fd);
        if (md->wFlags != DISPATCH_METHOD && md->wFlags != DISPATCH_PROPERTYGET && md->wFlags != DISPATCH_PROPERTYPUT && md->wFlags != DISPATCH_PROPERTYPUTREF) {
            CHECKF(hr == HR_E_FAIL, "idx %u (flags %x) hr=%08lx", i, md->wFlags, (unsigned long)hr);
            continue;
        }
        CHECKF(hr == S_OK && fd, "idx %u hr=%08lx", i, (unsigned long)hr);
        if (hr != S_OK || !fd) continue;
        CHECKF(fd->memid == md->dispid, "idx %u memid %lx", i, (unsigned long)fd->memid);
        CHECKF(fd->funckind == FUNC_VIRTUAL, "idx %u funckind", i);
        CHECKF((int)fd->invkind == (md->wFlags == DISPATCH_METHOD ? INVOKE_FUNC : md->wFlags == DISPATCH_PROPERTYGET ? INVOKE_PROPERTYGET :
                md->wFlags == DISPATCH_PROPERTYPUT ? INVOKE_PROPERTYPUT : INVOKE_PROPERTYPUTREF), "idx %u invkind %d", i, (int)fd->invkind);
        CHECKF(fd->callconv == md->cc, "idx %u callconv", i);
        CHECKF(fd->cParams == (SHORT)md->cArgs, "idx %u cParams %d", i, fd->cParams);
        CHECKF(fd->cParamsOpt == 0, "idx %u cParamsOpt", i);
        CHECKF(fd->oVft == (SHORT)(md->iMeth * 8), "idx %u oVft %d", i, fd->oVft);
        CHECKF(fd->wFuncFlags == 0, "idx %u wFuncFlags", i);
        CHECKF(fd->elemdescFunc.tdesc.vt == md->vtReturn, "idx %u ret vt %u", i, fd->elemdescFunc.tdesc.vt);
        CHECKF(fd->elemdescFunc.paramdesc.wParamFlags == 0, "idx %u ret flags", i);
        CHECKF(fd->cScodes == 0 || 1, "x");
        for (p = 0; p < md->cArgs; p++) {
            CHECKF(fd->lprgelemdescParam[p].tdesc.vt == md->ppdata[p].vt, "idx %u param %u vt %u", i, p, fd->lprgelemdescParam[p].tdesc.vt);
            CHECKF(fd->lprgelemdescParam[p].paramdesc.wParamFlags == PARAMFLAG_NONE, "idx %u param %u flags", i, p);
            CHECKF(fd->lprgelemdescParam[p].paramdesc.pparamdescex == NULL, "idx %u param %u ex", i, p);
        }
        ITypeInfo_ReleaseFuncDesc(itf, fd);
    }
    ITypeInfo_Release(itf);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_funcdesc_bad_index)
{
    ITypeInfo *ti, *itf; FUNCDESC *fd = (FUNCDESC *)1; static const UINT idx[] = { 15, 16, 100, 0xffff, 0xffffffff };
    unsigned i;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        for (i = 0; i < sizeof idx / sizeof idx[0]; i++) {
            fd = (FUNCDESC *)1;
            CHECKF(ITypeInfo_GetFuncDesc(itf, idx[i], &fd) == DISP_E_MEMBERNOTFOUND && fd == (FUNCDESC *)1, "idx %u", idx[i]);
        }
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_funcdesc_on_coclass_fails)
{
    ITypeInfo *ti; FUNCDESC *fd = (FUNCDESC *)1;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_GetFuncDesc(ti, 0, &fd), HR_E_FAIL);
    CHECK(fd == (FUNCDESC *)1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_funcdesc_each_call_is_independent_allocation)
{
    ITypeInfo *ti, *itf; FUNCDESC *a = NULL, *b = NULL;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        ITypeInfo_GetFuncDesc(itf, 0, &a); ITypeInfo_GetFuncDesc(itf, 0, &b);
        CHECK(a && b && a != b && a->lprgelemdescParam != b->lprgelemdescParam);
        if (a) ITypeInfo_ReleaseFuncDesc(itf, a);
        if (b) { EXPECT_EQ(b->memid, DD_ADD); ITypeInfo_ReleaseFuncDesc(itf, b); }
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

/* ---- GetIDsOfNames ---- */
static HRESULT gid(ITypeInfo *ti, DISPID *ids, UINT n, ...)
{
    OLECHAR *names[8]; va_list ap; UINT i; va_start(ap, n);
    for (i = 0; i < n; i++) names[i] = va_arg(ap, OLECHAR *);
    va_end(ap);
    return ITypeInfo_GetIDsOfNames(ti, names, n, ids);
}

TEST(dtd_ti_getids_methods_table)
{
    static const struct { const WCHAR *n; DISPID id; } t[] = {
        { L"Add", DD_ADD }, { L"Sub", DD_SUB }, { L"Scale", DD_SCALE }, { L"Concat", DD_CONCAT }, { L"Value", DD_VALUE },
        { L"VarVt", DD_VARVT }, { L"Deref", DD_DEREF }, { L"Pack", DD_PACK }, { L"Obj", DD_OBJ }, { L"Mul64", DD_MUL64 },
        { L"Half", DD_HALF }, { L"Child", DD_CHILD }, { L"Item", DD_ITEM }, { L"Count", DD_COUNT },
    };
    ITypeInfo *ti, *itf; unsigned i, k;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    for (k = 0; k < 2; k++) {
        ITypeInfo *x = k ? itf : ti;
        if (!x) continue;
        for (i = 0; i < sizeof t / sizeof t[0]; i++) {
            DISPID id = 0x5555; OLECHAR *nm = (OLECHAR *)t[i].n;
            HRESULT hr = ITypeInfo_GetIDsOfNames(x, &nm, 1, &id);
            CHECKF(hr == S_OK && id == t[i].id, "%s %ls hr=%08lx id=%lx", k ? "iface" : "coclass", t[i].n, (unsigned long)hr, (unsigned long)id);
        }
    }
    if (itf) ITypeInfo_Release(itf);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_case_insensitive)
{
    static const WCHAR *v[] = { L"add", L"ADD", L"aDd", L"aDD" };
    ITypeInfo *ti; unsigned i;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    for (i = 0; i < 4; i++) {
        DISPID id = 0; OLECHAR *nm = (OLECHAR *)v[i];
        CHECKF(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id) == S_OK && id == DD_ADD, "variant %u", i);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_ignores_diacritics_nonspace)
{
    /* StrICmp uses CompareStringW(lcid, NORM_IGNORECASE|NORM_IGNORENONSPACE, ...) for non-DBCS locales */
    ITypeInfo *ti; DISPID id = 0; OLECHAR *nm; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    nm = (OLECHAR *)L"àdd";   /* a-grave d d */
    hr = ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id);
    if (hr != S_OK || id != DD_ADD) t_note("folding not applied: hr=%08lx id=%ld (locale/NLS dependent)", (unsigned long)hr, (long)id);
    nm = (OLECHAR *)L"ÄDD";
    id = 0; hr = ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id);
    if (hr != S_OK || id != DD_ADD) t_note("folding not applied: hr=%08lx id=%ld (locale/NLS dependent)", (unsigned long)hr, (long)id);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_japanese_locale_ignores_width_and_kana)
{
    /* DBCS lcid + Japan: flags NORM_IGNORECASE|NORM_IGNOREWIDTH|NORM_IGNOREKANATYPE (0x30001) */
    ITypeInfo *ti; DISPID id = 0; OLECHAR *nm; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x411); if (!ti) return;
    nm = (OLECHAR *)L"ＡＤＤ";   /* full-width ADD */
    hr = ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id);
    if (hr != S_OK || id != DD_ADD) t_note("folding not applied: hr=%08lx id=%ld (locale/NLS dependent)", (unsigned long)hr, (long)id);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_fullwidth_not_equal_in_english)
{
    ITypeInfo *ti; DISPID id = 0x55; OLECHAR *nm;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    nm = (OLECHAR *)L"ＡＤＤ";
    EXPECT_EQ(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id), DISP_E_UNKNOWNNAME);
    EXPECT_EQ(id, -1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_unknown_name_sets_minus_one)
{
    ITypeInfo *ti; DISPID id = 0x1234; OLECHAR *nm;
    static const WCHAR *bad[] = { L"Nope", L"", L"Ad", L"Addd", L" Add", L"Add ", L"Add\0hidden" };
    unsigned i;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        id = 0x1234; nm = (OLECHAR *)bad[i];
        if (i == 6) continue;   /* embedded NUL == "Add" */
        CHECKF(ITypeInfo_GetIDsOfNames(ti, &nm, 1, &id) == DISP_E_UNKNOWNNAME && id == -1, "i=%u", i);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_zero_names_is_einvalidarg)
{
    ITypeInfo *ti; DISPID id = 0x1234; OLECHAR *nm = (OLECHAR *)L"Add";
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_GetIDsOfNames(ti, &nm, 0, &id), E_INVALIDARG);
    EXPECT_EQ(id, 0x1234);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_named_parameters)
{
    ITypeInfo *ti; DISPID ids[4];
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    memset(ids, 0x55, sizeof ids);
    EXPECT_EQ(gid(ti, ids, 3, (OLECHAR *)L"Add", (OLECHAR *)L"b", (OLECHAR *)L"a"), S_OK);
    EXPECT_EQ(ids[0], DD_ADD); EXPECT_EQ(ids[1], 1); EXPECT_EQ(ids[2], 0);
    memset(ids, 0x55, sizeof ids);
    EXPECT_EQ(gid(ti, ids, 3, (OLECHAR *)L"PACK", (OLECHAR *)L"F", (OLECHAR *)L"B"), S_OK);
    EXPECT_EQ(ids[0], DD_PACK); EXPECT_EQ(ids[1], 2); EXPECT_EQ(ids[2], 0);
    memset(ids, 0x55, sizeof ids);
    EXPECT_EQ(gid(ti, ids, 4, (OLECHAR *)L"Pack", (OLECHAR *)L"s", (OLECHAR *)L"b", (OLECHAR *)L"f"), S_OK);
    EXPECT_EQ(ids[1], 1); EXPECT_EQ(ids[2], 0); EXPECT_EQ(ids[3], 2);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_unknown_param_partial_results)
{
    ITypeInfo *ti; DISPID ids[4]; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    memset(ids, 0x55, sizeof ids);
    hr = gid(ti, ids, 3, (OLECHAR *)L"Add", (OLECHAR *)L"zzz", (OLECHAR *)L"b");
    EXPECT_EQ(hr, DISP_E_UNKNOWNNAME);
    EXPECT_EQ(ids[0], DD_ADD); EXPECT_EQ(ids[1], -1); EXPECT_EQ(ids[2], 1);
    memset(ids, 0x55, sizeof ids);
    hr = gid(ti, ids, 2, (OLECHAR *)L"Nothing", (OLECHAR *)L"a");
    EXPECT_EQ(hr, DISP_E_UNKNOWNNAME);
    EXPECT_EQ(ids[0], -1); EXPECT_EQ(ids[1], -1);   /* unknown member: every slot set to DISPID_UNKNOWN */
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_more_names_than_params)
{
    ITypeInfo *ti; DISPID ids[4]; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    memset(ids, 0x55, sizeof ids);
    hr = gid(ti, ids, 4, (OLECHAR *)L"Half", (OLECHAR *)L"f", (OLECHAR *)L"g", (OLECHAR *)L"h");
    EXPECT_EQ(hr, DISP_E_UNKNOWNNAME);
    EXPECT_EQ(ids[0], DD_HALF); EXPECT_EQ(ids[1], 0); EXPECT_EQ(ids[2], -1); EXPECT_EQ(ids[3], -1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_param_names_only_for_matched_member)
{
    ITypeInfo *ti; DISPID ids[2]; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    /* "x" is a parameter of Scale, not of Add */
    memset(ids, 0x55, sizeof ids);
    hr = gid(ti, ids, 2, (OLECHAR *)L"Add", (OLECHAR *)L"x");
    EXPECT_EQ(hr, DISP_E_UNKNOWNNAME); EXPECT_EQ(ids[1], -1);
    hr = gid(ti, ids, 2, (OLECHAR *)L"Scale", (OLECHAR *)L"x");
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(ids[1], 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_member_without_params_and_param_request)
{
    ITypeInfo *ti; DISPID ids[2]; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    memset(ids, 0x55, sizeof ids);
    hr = gid(ti, ids, 2, (OLECHAR *)L"Child", (OLECHAR *)L"a");
    EXPECT_EQ(hr, DISP_E_UNKNOWNNAME); EXPECT_EQ(ids[0], DD_CHILD); EXPECT_EQ(ids[1], -1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getids_duplicate_names_first_entry_wins)
{
    /* "Value" appears twice (put then get) with the same dispid; the first table match is used for params */
    ITypeInfo *ti; DISPID ids[2]; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    memset(ids, 0x55, sizeof ids);
    hr = gid(ti, ids, 2, (OLECHAR *)L"Value", (OLECHAR *)L"v");
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(ids[0], DD_VALUE); EXPECT_EQ(ids[1], 0);
    ITypeInfo_Release(ti);
}

/* ---- GetNames ---- */
TEST(dtd_ti_getnames_interface_member_and_params)
{
    ITypeInfo *ti, *itf; BSTR n[4] = { 0 }; UINT c = 0xcc; HRESULT hr; int i;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        hr = ITypeInfo_GetNames(itf, DD_PACK, n, 4, &c);
        EXPECT_EQ(hr, S_OK); EXPECT_EQ(c, 4);
        CHECK(bs_eq(n[0], L"Pack")); CHECK(bs_eq(n[1], L"b")); CHECK(bs_eq(n[2], L"s")); CHECK(bs_eq(n[3], L"f"));
        for (i = 0; i < 4; i++) { bs_free(n[i]); n[i] = NULL; }
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getnames_truncates_to_cmaxnames)
{
    ITypeInfo *ti, *itf; BSTR n[4] = { 0 }; UINT c = 0xcc;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetNames(itf, DD_PACK, n, 2, &c), S_OK);
        EXPECT_EQ(c, 2); CHECK(bs_eq(n[0], L"Pack")); CHECK(bs_eq(n[1], L"b")); CHECK(n[2] == NULL);
        bs_free(n[0]); bs_free(n[1]);
        c = 0xcc; n[0] = NULL;
        EXPECT_EQ(ITypeInfo_GetNames(itf, DD_PACK, n, 1, &c), S_OK);
        EXPECT_EQ(c, 1); CHECK(bs_eq(n[0], L"Pack")); bs_free(n[0]);
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getnames_cmaxnames_larger_than_available)
{
    ITypeInfo *ti, *itf; BSTR n[4] = { 0 }; UINT c = 0xcc;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetNames(itf, DD_ADD, n, 4, &c), S_OK);
        EXPECT_EQ(c, 3); CHECK(bs_eq(n[0], L"Add")); CHECK(bs_eq(n[1], L"a")); CHECK(bs_eq(n[2], L"b"));
        bs_free(n[0]); bs_free(n[1]); bs_free(n[2]);
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getnames_zero_cmaxnames_gives_zero)
{
    ITypeInfo *ti, *itf; BSTR n[1] = { (BSTR)1 }; UINT c = 0xcc;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetNames(itf, DD_ADD, n, 0, &c), S_OK);
        EXPECT_EQ(c, 0);
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getnames_unknown_memid)
{
    ITypeInfo *ti, *itf; BSTR n[2] = { 0 }; UINT c = 0xcc;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetNames(itf, 0x9999, n, 2, &c), DISP_E_MEMBERNOTFOUND);
        EXPECT_EQ(c, 0xcc);
        EXPECT_EQ(ITypeInfo_GetNames(itf, MEMBERID_NIL, n, 2, &c), DISP_E_MEMBERNOTFOUND);
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getnames_on_coclass_returns_zero_names)
{
    ITypeInfo *ti; BSTR n[2] = { 0 }; UINT c = 0xcc;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_GetNames(ti, DD_ADD, n, 2, &c), S_OK);
    EXPECT_EQ(c, 0);
    ITypeInfo_Release(ti);
}

/* ---- GetDocumentation ---- */
TEST(dtd_ti_getdoc_member_name_only)
{
    ITypeInfo *ti; BSTR name = NULL, doc = (BSTR)1, help = (BSTR)1; DWORD ctx = 0xdead; HRESULT hr;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    hr = ITypeInfo_GetDocumentation(ti, DD_SCALE, &name, &doc, &ctx, &help);
    EXPECT_EQ(hr, S_OK);
    CHECK(bs_eq(name, L"Scale")); CHECK(doc == NULL); CHECK(help == NULL); EXPECT_EQ(ctx, 0);
    bs_free(name);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getdoc_null_outputs_allowed)
{
    ITypeInfo *ti; BSTR name = NULL;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_GetDocumentation(ti, DD_ADD, NULL, NULL, NULL, NULL), S_OK);
    EXPECT_EQ(ITypeInfo_GetDocumentation(ti, DD_ADD, &name, NULL, NULL, NULL), S_OK);
    CHECK(bs_eq(name, L"Add")); bs_free(name);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getdoc_unknown_memid_including_nil)
{
    ITypeInfo *ti; BSTR name = (BSTR)1;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(ITypeInfo_GetDocumentation(ti, 0x9999, &name, NULL, NULL, NULL), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &name, NULL, NULL, NULL), DISP_E_MEMBERNOTFOUND);   /* no type-level name */
    EXPECT_EQ(ITypeInfo_GetDocumentation(ti, 0, &name, NULL, NULL, NULL), DISP_E_MEMBERNOTFOUND);
    CHECK(name == (BSTR)1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_getdoc_works_on_interface_kind_too)
{
    ITypeInfo *ti, *itf; BSTR name = NULL;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    itf = iface_of(ti);
    if (itf) {
        EXPECT_EQ(ITypeInfo_GetDocumentation(itf, DD_CONCAT, &name, NULL, NULL, NULL), S_OK);
        CHECK(bs_eq(name, L"Concat")); bs_free(name);
        ITypeInfo_Release(itf);
    }
    ITypeInfo_Release(ti);
}

/* ---- unimplemented members ---- */
TEST(dtd_ti_unimplemented_members)
{
    ITypeInfo *ti; ITypeComp *tc = NULL; VARDESC *vd = NULL; int fl = 0; HREFTYPE h; void *p = NULL; ITypeLib *tl = NULL; UINT idx = 0;
    BSTR dll = NULL, nm = NULL; WORD ord = 0; BSTR mops = NULL; IUnknown *inst = NULL;
    REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    ITypeInfo *itf = iface_of(ti);
    ITypeInfo *kinds[2] = { ti, itf }; int k;
    for (k = 0; k < 2; k++) {
        ITypeInfo *x = kinds[k]; if (!x) continue;
        EXPECT_EQ(ITypeInfo_GetTypeComp(x, &tc), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_GetImplTypeFlags(x, 0, &fl), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_AddressOfMember(x, DD_ADD, INVOKE_FUNC, &p), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_GetContainingTypeLib(x, &tl, &idx), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_GetDllEntry(x, DD_ADD, INVOKE_FUNC, &dll, &nm, &ord), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_CreateInstance(x, NULL, &DI_IUnknown, (void **)&inst), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_GetMops(x, DD_ADD, &mops), HR_E_NOTIMPL);
        EXPECT_EQ(ITypeInfo_GetVarDesc(x, 0, &vd), DISP_E_MEMBERNOTFOUND);
        EXPECT_EQ(ITypeInfo_GetVarDesc(x, 5, &vd), DISP_E_MEMBERNOTFOUND);
    }
    (void)h;
    if (itf) ITypeInfo_Release(itf);
    ITypeInfo_Release(ti);
}

/* ---- DispGetIDsOfNames / DispInvoke are pure forwarders to the ITypeInfo ---- */
typedef struct FakeTI {
    const ITypeInfoVtbl *lpVtbl;
    LONG ref;
    int n_gids, n_inv;
    UINT g_cnames; OLECHAR **g_names; DISPID *g_ids;
    void *i_this; DISPID i_dispid; WORD i_flags; DISPPARAMS *i_dp; VARIANT *i_res; EXCEPINFO *i_ex; UINT *i_err;
    HRESULT ret;
} FakeTI;
static HRESULT WINAPI f_QI(ITypeInfo *i, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI f_AddRef(ITypeInfo *i) { return ++((FakeTI *)i)->ref; }
static ULONG WINAPI f_Release(ITypeInfo *i) { return --((FakeTI *)i)->ref; }
static HRESULT WINAPI f_gids(ITypeInfo *i, LPOLESTR *n, UINT c, MEMBERID *ids)
{ FakeTI *f = (FakeTI *)i; f->n_gids++; f->g_cnames = c; f->g_names = n; f->g_ids = ids; if (c) ids[0] = 0x4242; return f->ret; }
static HRESULT WINAPI f_inv(ITypeInfo *i, PVOID inst, MEMBERID id, WORD fl, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ex, UINT *err)
{ FakeTI *f = (FakeTI *)i; f->n_inv++; f->i_this = inst; f->i_dispid = id; f->i_flags = fl; f->i_dp = dp; f->i_res = res; f->i_ex = ex; f->i_err = err; return f->ret; }
static HRESULT WINAPI f_nimpl(void) { return E_NOTIMPL; }
static const ITypeInfoVtbl fake_vtbl = {
    f_QI, f_AddRef, f_Release, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl,
    (void *)f_nimpl, (void *)f_nimpl, f_gids, f_inv, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl,
    (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl, (void *)f_nimpl
};
static void fake_init(FakeTI *f, HRESULT ret) { memset(f, 0, sizeof *f); f->lpVtbl = &fake_vtbl; f->ref = 1; f->ret = ret; }

TEST(dtd_ti_dispgetidsofnames_forwards_verbatim)
{
    FakeTI f; OLECHAR *names[2] = { (OLECHAR *)L"x", (OLECHAR *)L"y" }; DISPID ids[2] = { 0, 0 }; HRESULT hr;
    REQUIRE(OA(DispGetIDsOfNames));
    fake_init(&f, 0x1234abcd);
    hr = OA(DispGetIDsOfNames)((ITypeInfo *)&f, names, 2, ids);
    EXPECT_EQ(hr, 0x1234abcd);
    EXPECT_EQ(f.n_gids, 1); EXPECT_EQ(f.g_cnames, 2);
    CHECK(f.g_names == names); CHECK(f.g_ids == ids);
    EXPECT_EQ(ids[0], 0x4242);
    EXPECT_EQ(f.ref, 1);   /* no AddRef/Release traffic */
    fake_init(&f, E_FAIL);
    hr = OA(DispGetIDsOfNames)((ITypeInfo *)&f, names, 0, ids);
    EXPECT_EQ(hr, E_FAIL); EXPECT_EQ(f.g_cnames, 0);
}

TEST(dtd_ti_dispinvoke_forwards_verbatim)
{
    FakeTI f; int thisobj; DISPPARAMS dp; VARIANT res; EXCEPINFO ex; UINT err; HRESULT hr;
    REQUIRE(OA(DispInvoke));
    fake_init(&f, 0x0badf00d);
    memset(&dp, 0, sizeof dp);
    hr = OA(DispInvoke)(&thisobj, (ITypeInfo *)&f, 0x321, DISPATCH_PROPERTYGET, &dp, &res, &ex, &err);
    EXPECT_EQ(hr, 0x0badf00d);
    EXPECT_EQ(f.n_inv, 1);
    CHECK(f.i_this == &thisobj); EXPECT_EQ(f.i_dispid, 0x321); EXPECT_EQ(f.i_flags, DISPATCH_PROPERTYGET);
    CHECK(f.i_dp == &dp && f.i_res == &res && f.i_ex == &ex && f.i_err == &err);
    EXPECT_EQ(f.ref, 1);
    /* NULL optional pointers are passed straight through (no substitution) */
    fake_init(&f, S_OK);
    hr = OA(DispInvoke)(NULL, (ITypeInfo *)&f, DISPID_VALUE, 0xffff, &dp, NULL, NULL, NULL);
    EXPECT_EQ(hr, S_OK);
    CHECK(f.i_this == NULL && f.i_res == NULL && f.i_ex == NULL && f.i_err == NULL);
    EXPECT_EQ(f.i_flags, 0xffff);
}

TEST(dtd_ti_dispgetidsofnames_real_typeinfo)
{
    ITypeInfo *ti; OLECHAR *names[3] = { (OLECHAR *)L"Sub", (OLECHAR *)L"B", (OLECHAR *)L"A" }; DISPID ids[3] = { 0 };
    REQUIRE(OA(CreateDispTypeInfo)); REQUIRE(OA(DispGetIDsOfNames));
    ti = demo_typeinfo(0x409); if (!ti) return;
    EXPECT_EQ(OA(DispGetIDsOfNames)(ti, names, 3, ids), S_OK);
    EXPECT_EQ(ids[0], DD_SUB); EXPECT_EQ(ids[1], 1); EXPECT_EQ(ids[2], 0);
    EXPECT_EQ(OA(DispGetIDsOfNames)(ti, names, 0, ids), E_INVALIDARG);
    names[0] = (OLECHAR *)L"Missing";
    EXPECT_EQ(OA(DispGetIDsOfNames)(ti, names, 1, ids), DISP_E_UNKNOWNNAME);
    EXPECT_EQ(ids[0], -1);
    ITypeInfo_Release(ti);
}

/* ---- Invoke ---- */
static HRESULT inv(ITypeInfo *ti, Demo *d, DISPID id, WORD fl, DISPPARAMS *dp, VARIANT *res, UINT *err)
{
    return ITypeInfo_Invoke(ti, d, id, fl, dp, res, NULL, err);
}

#define SETUP2(TI, D) ITypeInfo *TI; Demo D; REQUIRE(OA(CreateDispTypeInfo)); REQUIRE(OA(DispInvoke)); TI = demo_typeinfo(0x409); if (!TI) return; Demo_init(&D)

TEST(dtd_ti_invoke_add)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); UINT err = 0xdead; EXCEPINFO ex; HRESULT hr;
    v_i4(&a[0], 4); v_i4(&a[1], 3);      /* rgvarg is reversed: a=3, b=4 */
    memset(&r, 0xcc, sizeof r); memset(&ex, 0xab, sizeof ex);
    hr = ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &ex, &err);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 7);
    EXPECT_EQ(err, 0xdead);    /* untouched on success */
    EXPECT_EQ(((BYTE *)&ex)[0], 0xab); EXPECT_EQ(((BYTE *)&ex)[sizeof ex - 1], 0xab);
    EXPECT_EQ(d.calls, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_argument_order_reversed)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    v_i4(&a[0], 3); v_i4(&a[1], 10);     /* Sub(a=10, b=3) -> 7 */
    EXPECT_EQ(inv(ti, &d, DD_SUB, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 7);
    v_i4(&a[0], 10); v_i4(&a[1], 3);     /* Sub(a=3, b=10) -> -7 */
    EXPECT_EQ(inv(ti, &d, DD_SUB, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), -7);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_via_dispinvoke_same_result)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    v_i4(&a[0], 2); v_i4(&a[1], 40);
    EXPECT_EQ(OA(DispInvoke)(&d, ti, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 42);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_works_on_coclass_and_interface_kinds)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); ITypeInfo *itf = iface_of(ti);
    v_i4(&a[0], 1); v_i4(&a[1], 1);
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 2);
    if (itf) { EXPECT_EQ(inv(itf, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 2); ITypeInfo_Release(itf); }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_double_and_float)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    v_i4(&a[0], 4); v_r8(&a[1], 2.5);
    EXPECT_EQ(inv(ti, &d, DD_SCALE, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_R8); CHECK(V_R8(&r) == 10.0);
    dp = dp_make(a, 1, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_R4; V_R4(&a[0]) = 7.0f;
    EXPECT_EQ(inv(ti, &d, DD_HALF, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_R4); CHECK(V_R4(&r) == 3.5f);
    v_i4(&a[0], 9);   /* I4 -> R4 coercion */
    EXPECT_EQ(inv(ti, &d, DD_HALF, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    CHECK(V_R4(&r) == 4.5f);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_int64)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_I8; V_I8(&a[0]) = -3;
    vinit(&a[1]); V_VT(&a[1]) = VT_I8; V_I8(&a[1]) = 5000000000LL;
    EXPECT_EQ(inv(ti, &d, DD_MUL64, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I8); CHECK(V_I8(&r) == -15000000000LL);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_bstr_args_and_result)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"world");
    vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"hello ");
    EXPECT_EQ(inv(ti, &d, DD_CONCAT, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_BSTR); CHECK(bs_eq(V_BSTR(&r), L"hello world"));
    bs_free(V_BSTR(&r));
    /* numbers coerce to strings */
    vclear(&a[0]); vclear(&a[1]); v_i4(&a[0], 2); v_i4(&a[1], 1);
    EXPECT_EQ(inv(ti, &d, DD_CONCAT, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    CHECK(bs_eq(V_BSTR(&r), L"12")); bs_free(V_BSTR(&r));
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_null_result_pointer_ok)
{
    SETUP2(ti, d); VARIANT a[2]; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"b");
    vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"a");
    EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_CONCAT, DISPATCH_METHOD, &dp, NULL, NULL, NULL), S_OK);   /* BSTR result is released internally */
    EXPECT_EQ(d.calls, 1);
    vclear(&a[0]); vclear(&a[1]);
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, NULL, NULL, NULL), S_OK);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_property_put_get)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dpput = dp_make(a, 1, named, 1), dpget = dp_make(NULL, 0, NULL, 0);
    v_i4(&a[0], 1234);
    memset(&r, 0xcc, sizeof r);
    EXPECT_EQ(inv(ti, &d, DD_VALUE, DISPATCH_PROPERTYPUT, &dpput, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
    EXPECT_EQ(d.value, 1234);
    memset(&r, 0xcc, sizeof r);
    EXPECT_EQ(inv(ti, &d, DD_VALUE, DISPATCH_PROPERTYGET, &dpget, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 1234);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_put_without_named_propertyput_is_paramnotoptional)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); UINT err = 0xdead;
    v_i4(&a[0], 5);
    EXPECT_EQ(inv(ti, &d, DD_VALUE, DISPATCH_PROPERTYPUT, &dp, &r, &err), E_DISP_PARAMNOTOPT);
    EXPECT_EQ(d.calls, 0); EXPECT_EQ(d.value, 0);
    {   /* a named arg that is not DISPID_PROPERTYPUT is also rejected */
        DISPID wrong[1] = { 0 }; DISPPARAMS dp2 = dp_make(a, 1, wrong, 1);
        EXPECT_EQ(inv(ti, &d, DD_VALUE, DISPATCH_PROPERTYPUT, &dp2, &r, &err), E_DISP_PARAMNOTOPT);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_put_on_putref_only_is_membernotfound)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp_make(a, 1, named, 1);
    Mock m; Mock_init(&m);
    vinit(&a[0]); V_VT(&a[0]) = VT_DISPATCH; V_DISPATCH(&a[0]) = (IDispatch *)&m;
    EXPECT_EQ(inv(ti, &d, DD_OBJ, DISPATCH_PROPERTYPUT, &dp, &r, NULL), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_putref_dispatch_arg)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp_make(a, 1, named, 1);
    Mock m; Mock_init(&m);
    vinit(&a[0]); V_VT(&a[0]) = VT_DISPATCH; V_DISPATCH(&a[0]) = (IDispatch *)&m;
    EXPECT_EQ(inv(ti, &d, DD_OBJ, DISPATCH_PROPERTYPUTREF, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
    CHECK(d.obj == &m);
    EXPECT_EQ(m.ref, 1);   /* coerced copy released after the call */
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_putref_unknown_arg_coerces_to_dispatch_or_fails)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp_make(a, 1, named, 1); UINT err = 0xdead; HRESULT hr;
    v_i4(&a[0], 7);   /* an integer cannot become an IDispatch */
    hr = inv(ti, &d, DD_OBJ, DISPATCH_PROPERTYPUTREF, &dp, &r, &err);
    CHECKF(FAILED(hr), "hr=%08lx", (unsigned long)hr);
    t_note("I4 -> VT_DISPATCH coercion hr=%08lx argerr=%u", (unsigned long)hr, err);
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_variant_param_passed_without_coercion)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); unsigned i; HRESULT hr;
    static const VARTYPE vts[] = { VT_EMPTY, VT_NULL, VT_I2, VT_R8, VT_ERROR, VT_BOOL, VT_DATE, VT_UI1, VT_BYREF | VT_I4, VT_BYREF | VT_VARIANT };
    for (i = 0; i < sizeof vts / sizeof vts[0]; i++) {
        vinit(&a[0]); V_VT(&a[0]) = vts[i];
        if (vts[i] == VT_BYREF || (vts[i] & VT_BYREF)) V_BYREF(&a[0]) = &d;
        hr = inv(ti, &d, DD_VARVT, DISPATCH_METHOD, &dp, &r, NULL);
        CHECKF(hr == S_OK && V_VT(&r) == VT_I4 && (V_I4(&r) & 0xffff) == vts[i], "vt=%u hr=%08lx got %lx", (unsigned)vts[i], (unsigned long)hr, (unsigned long)V_I4(&r));
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_variant_param_error_notfound_passes_through)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_ERROR; V_ERROR(&a[0]) = DISP_E_PARAMNOTFOUND;
    EXPECT_EQ(inv(ti, &d, DD_VARVT, DISPATCH_METHOD, &dp, &r, NULL), S_OK);   /* VT_VARIANT parameters accept missing-arg markers */
    EXPECT_EQ(V_I4(&r), VT_ERROR);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_byref_param_written_through)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); LONG v = 5;
    vinit(&a[0]); V_VT(&a[0]) = VT_BYREF | VT_I4; V_I4REF(&a[0]) = &v;
    EXPECT_EQ(inv(ti, &d, DD_DEREF, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 5); EXPECT_EQ(v, 1005);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_byref_param_requires_exact_byref_vt)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); LONG v = 5; SHORT s = 2; UINT err; unsigned i;
    struct { VARTYPE vt; void *p; } t[3];
    v_i4(&a[0], 5);
    /* the byref type check does not report an argument index: puArgErr stays untouched */
    err = 0xdead; EXPECT_EQ(inv(ti, &d, DD_DEREF, DISPATCH_METHOD, &dp, &r, &err), DISP_E_TYPEMISMATCH); EXPECT_EQ(err, 0xdead);
    t[0].vt = VT_BYREF | VT_I2; t[0].p = &s;
    t[1].vt = VT_BYREF | VT_UI4; t[1].p = &v;
    t[2].vt = VT_BYREF | VT_VARIANT; t[2].p = &a[0];
    for (i = 0; i < 3; i++) {
        vinit(&a[0]); V_VT(&a[0]) = t[i].vt; V_BYREF(&a[0]) = t[i].p;
        err = 0xdead;
        CHECKF(inv(ti, &d, DD_DEREF, DISPATCH_METHOD, &dp, &r, &err) == DISP_E_TYPEMISMATCH && err == 0xdead, "i=%u", i);
    }
    EXPECT_EQ(v, 5); EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_small_types_coerced)
{
    SETUP2(ti, d); VARIANT a[3], r; DISPPARAMS dp = dp_make(a, 3, NULL, 0);
    /* Pack(b=UI1, s=I2, f=BOOL) : rgvarg = { f, s, b } */
    vinit(&a[0]); V_VT(&a[0]) = VT_BOOL; V_BOOL(&a[0]) = VARIANT_TRUE;
    vinit(&a[1]); V_VT(&a[1]) = VT_R8;   V_R8(&a[1]) = -3.0;
    vinit(&a[2]); V_VT(&a[2]) = VT_BSTR; V_BSTR(&a[2]) = bs_alloc(L"200");
    EXPECT_EQ(inv(ti, &d, DD_PACK, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 200 - 3000 + 1000000);
    vclear(&a[2]);
    v_i4(&a[0], 0); v_i4(&a[1], 7); v_i4(&a[2], 1);
    EXPECT_EQ(inv(ti, &d, DD_PACK, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 1 + 7000);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_overflow_reports_argerr_index)
{
    SETUP2(ti, d); VARIANT a[3], r; DISPPARAMS dp = dp_make(a, 3, NULL, 0); UINT err = 0xdead;
    /* Pack: rgvarg = { f, s, b }; b = 300 overflows VT_UI1 (rgvarg index 2) */
    v_i4(&a[0], 1); v_i4(&a[1], 1); v_i4(&a[2], 300);
    EXPECT_EQ(inv(ti, &d, DD_PACK, DISPATCH_METHOD, &dp, &r, &err), DISP_E_OVERFLOW);
    EXPECT_EQ(err, 2); EXPECT_EQ(d.calls, 0);
    v_i4(&a[2], 5); v_i4(&a[1], 40000);   /* s overflows I2 (index 1) */
    err = 0xdead;
    EXPECT_EQ(inv(ti, &d, DD_PACK, DISPATCH_METHOD, &dp, &r, &err), DISP_E_OVERFLOW);
    EXPECT_EQ(err, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_typemismatch_reports_argerr_index)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); UINT err = 0xdead;
    v_i4(&a[0], 1);
    vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"not a number");
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(err, 1); EXPECT_EQ(d.calls, 0);
    vclear(&a[1]); v_i4(&a[1], 1);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"x");
    err = 0xdead;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(err, 0);
    vclear(&a[0]);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_first_declared_param_error_wins)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); UINT err = 0xdead;
    /* both args bad: parameter a (rgvarg[1]) is processed first -> its error is reported */
    vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"abc");   /* a: type mismatch */
    v_r8(&a[0], 1e30);                                                       /* b: overflow */
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(err, 1);
    vclear(&a[1]); v_r8(&a[1], 1e30);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"abc");
    err = 0xdead;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), DISP_E_OVERFLOW);
    EXPECT_EQ(err, 1);
    vclear(&a[0]);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_coercion_table_for_i4_param)
{
    SETUP2(ti, d); VARIANT a[2], r, src; DISPPARAMS dp = dp_make(a, 2, NULL, 0); unsigned i;
    static const struct { VARTYPE vt; double dv; const WCHAR *s; HRESULT hr; LONG want; } t[] = {
        { VT_I2, -5, NULL, S_OK, -5 }, { VT_UI1, 200, NULL, S_OK, 200 }, { VT_R8, 2.5, NULL, S_OK, 2 }, { VT_R8, 3.5, NULL, S_OK, 4 },
        { VT_R4, -1.5, NULL, S_OK, -2 }, { VT_BOOL, -1, NULL, S_OK, -1 }, { VT_EMPTY, 0, NULL, S_OK, 0 },
        { VT_BSTR, 0, L"41", S_OK, 41 }, { VT_BSTR, 0, L"4x", E_DISP_TYPEMISMATCH, 0 }, { VT_BSTR, 0, L"", E_DISP_TYPEMISMATCH, 0 },
        { VT_NULL, 0, NULL, E_DISP_TYPEMISMATCH, 0 }, { VT_R8, 1e20, NULL, E_DISP_OVERFLOW, 0 }, { VT_CY, 12, NULL, S_OK, 12 },
        { VT_DATE, 4, NULL, S_OK, 4 }, { VT_ERROR, 0, NULL, E_DISP_TYPEMISMATCH, 0 },
    };
    v_i4(&a[1], 100);   /* parameter a */
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        HRESULT hr; UINT err = 0xdead;
        vinit(&src); V_VT(&src) = t[i].vt;
        switch (t[i].vt) {
        case VT_I2: V_I2(&src) = (SHORT)t[i].dv; break; case VT_UI1: V_UI1(&src) = (BYTE)t[i].dv; break;
        case VT_R8: V_R8(&src) = t[i].dv; break; case VT_R4: V_R4(&src) = (float)t[i].dv; break;
        case VT_BOOL: V_BOOL(&src) = (VARIANT_BOOL)t[i].dv; break; case VT_BSTR: V_BSTR(&src) = bs_alloc(t[i].s); break;
        case VT_CY: V_CY(&src).int64 = (LONGLONG)(t[i].dv * 10000); break; case VT_DATE: V_DATE(&src) = t[i].dv; break;
        case VT_ERROR: V_ERROR(&src) = 5; break; default: break;
        }
        a[0] = src;
        hr = inv(ti, &d, DD_SUB, DISPATCH_METHOD, &dp, &r, &err);   /* a - b with b = src */
        CHECKF(hr == t[i].hr, "i=%u vt=%u hr=%08lx want %08lx", i, (unsigned)t[i].vt, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK) CHECKF(V_I4(&r) == 100 - t[i].want, "i=%u got %ld", i, (long)V_I4(&r));
        else CHECKF(err == 0, "i=%u argerr %u", i, err);
        if (t[i].vt == VT_BSTR) bs_free(V_BSTR(&src));
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_byref_sources_are_dereferenced_for_value_params)
{
    SETUP2(ti, d); VARIANT a[2], r, inner; DISPPARAMS dp = dp_make(a, 2, NULL, 0); LONG lv = 40; SHORT sv = 2; BSTR bs = bs_alloc(L"3");
    v_i4(&a[1], 50);
    vinit(&a[0]); V_VT(&a[0]) = VT_BYREF | VT_I4; V_I4REF(&a[0]) = &lv;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 90);
    vinit(&a[0]); V_VT(&a[0]) = VT_BYREF | VT_I2; V_I2REF(&a[0]) = &sv;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 52);
    vinit(&a[0]); V_VT(&a[0]) = VT_BYREF | VT_BSTR; V_BSTRREF(&a[0]) = &bs;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 53);
    v_i4(&inner, 6);
    vinit(&a[0]); V_VT(&a[0]) = VT_BYREF | VT_VARIANT; V_VARIANTREF(&a[0]) = &inner;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 56);
    bs_free(bs);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_paramnotfound_marker_gives_paramnotoptional)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); UINT err = 0xdead;
    v_i4(&a[1], 1);
    vinit(&a[0]); V_VT(&a[0]) = VT_ERROR; V_ERROR(&a[0]) = DISP_E_PARAMNOTFOUND;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), E_DISP_PARAMNOTOPT);
    EXPECT_EQ(err, 0); EXPECT_EQ(d.calls, 0);
    vinit(&a[1]); V_VT(&a[1]) = VT_ERROR; V_ERROR(&a[1]) = DISP_E_PARAMNOTFOUND;
    v_i4(&a[0], 1); err = 0xdead;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), E_DISP_PARAMNOTOPT);
    EXPECT_EQ(err, 1);
    /* a different scode is a plain type mismatch */
    V_ERROR(&a[1]) = 0x80004005; err = 0xdead;
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(err, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_param_count_mismatch)
{
    SETUP2(ti, d); VARIANT a[4], r; DISPPARAMS dp; UINT n;
    for (n = 0; n < 4; n++) {
        UINT i; for (i = 0; i < 4; i++) v_i4(&a[i], 1);
        dp = dp_make(a, n, NULL, 0);
        if (n == 2) { EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK); continue; }
        CHECKF(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL) == DISP_E_BADPARAMCOUNT, "n=%u", n);
    }
    EXPECT_EQ(d.calls, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_param_count_mismatch_other_kinds)
{
    SETUP2(ti, d); VARIANT a[3], r; DISPPARAMS dp0 = dp_make(NULL, 0, NULL, 0), dp3; UINT i;
    for (i = 0; i < 3; i++) v_i4(&a[i], 1);
    dp3 = dp_make(a, 3, NULL, 0);
    EXPECT_EQ(inv(ti, &d, DD_VARVT, DISPATCH_METHOD, &dp0, &r, NULL), DISP_E_BADPARAMCOUNT);
    EXPECT_EQ(inv(ti, &d, DD_VARVT, DISPATCH_METHOD, &dp3, &r, NULL), DISP_E_BADPARAMCOUNT);
    EXPECT_EQ(inv(ti, &d, DD_ITEM, DISPATCH_PROPERTYGET, &dp0, &r, NULL), DISP_E_BADPARAMCOUNT);
    EXPECT_EQ(inv(ti, &d, DD_PACK, DISPATCH_METHOD, &dp0, &r, NULL), DISP_E_BADPARAMCOUNT);
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_unknown_dispid)
{
    SETUP2(ti, d); VARIANT r; DISPPARAMS dp = dp_make(NULL, 0, NULL, 0); unsigned i;
    static const DISPID ids[] = { 0, 1, -1, DISPID_VALUE, DISPID_NEWENUM, DISPID_PROPERTYPUT, 0x100, 0x110, 0x7fffffff, (DISPID)0x80000000 };
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++)
        CHECKF(inv(ti, &d, ids[i], DISPATCH_METHOD, &dp, &r, NULL) == DISP_E_MEMBERNOTFOUND, "dispid %lx", (unsigned long)ids[i]);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_flag_mismatch_is_membernotfound)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0), dp0 = dp_make(NULL, 0, NULL, 0);
    v_i4(&a[0], 1); v_i4(&a[1], 1);
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_PROPERTYGET, &dp, &r, NULL), DISP_E_MEMBERNOTFOUND);   /* Add is a method only */
    EXPECT_EQ(inv(ti, &d, DD_VALUE, DISPATCH_METHOD, &dp0, &r, NULL), DISP_E_MEMBERNOTFOUND);     /* Value has no method flavour */
    EXPECT_EQ(inv(ti, &d, DD_ITEM, DISPATCH_METHOD, &dp0, &r, NULL), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(inv(ti, &d, DD_ADD, 0, &dp, &r, NULL), DISP_E_MEMBERNOTFOUND);                       /* no flag bits at all */
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_combined_flags_match_either_kind)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0), dp0 = dp_make(NULL, 0, NULL, 0);
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 3);
    d.value = 9;
    EXPECT_EQ(inv(ti, &d, DD_VALUE, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp0, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 9);
    EXPECT_EQ(inv(ti, &d, DD_COUNT, DISPATCH_METHOD, &dp0, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 42);
    EXPECT_EQ(inv(ti, &d, DD_COUNT, DISPATCH_PROPERTYGET, &dp0, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 42);
    EXPECT_EQ(inv(ti, &d, DD_COUNT, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp0, &r, NULL), S_OK);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_invalid_flag_bits_are_einvalidarg)
{
    SETUP2(ti, d); VARIANT r; DISPPARAMS dp = dp_make(NULL, 0, NULL, 0); unsigned i;
    static const WORD fl[] = { 0x10, 0x11, 0x20, 0x100, 0x1000, 0x4000, 0x8000, 0xfff0, 0xffff, 0x12 };
    for (i = 0; i < sizeof fl / sizeof fl[0]; i++)
        CHECKF(inv(ti, &d, DD_COUNT, fl[i], &dp, &r, NULL) == E_INVALIDARG, "flags %x", fl[i]);
    EXPECT_EQ(d.calls, 0);
    /* validated before the dispid lookup */
    EXPECT_EQ(inv(ti, &d, 0x7777, 0x10, &dp, &r, NULL), E_INVALIDARG);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_named_args_by_dispid)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPID named[2] = { 0, 1 }; DISPPARAMS dp = dp_make(a, 2, named, 2);
    /* Sub(a:=10, b:=3): both named, rgvarg order follows the named list */
    v_i4(&a[0], 10); v_i4(&a[1], 3);
    EXPECT_EQ(inv(ti, &d, DD_SUB, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 7);
    named[0] = 1; named[1] = 0;    /* named list reversed: first rgvarg is b */
    EXPECT_EQ(inv(ti, &d, DD_SUB, DISPATCH_METHOD, &dp, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 3 - 10);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_mixed_positional_and_named)
{
    SETUP2(ti, d); VARIANT a[3], r; DISPID named[1] = { 2 }; DISPPARAMS dp = dp_make(a, 3, named, 1);
    /* Pack(1, 2, f:=TRUE): rgvarg = { f(named), s, b } */
    vinit(&a[0]); V_VT(&a[0]) = VT_BOOL; V_BOOL(&a[0]) = VARIANT_TRUE;
    v_i4(&a[1], 2); v_i4(&a[2], 1);
    EXPECT_EQ(inv(ti, &d, DD_PACK, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 1 + 2000 + 1000000);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_bad_named_dispid_is_paramnotfound)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPID named[1] = { 7 }; DISPPARAMS dp = dp_make(a, 2, named, 1); UINT err = 0xdead;
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    EXPECT_EQ(inv(ti, &d, DD_SUB, DISPATCH_METHOD, &dp, &r, &err), DISP_E_PARAMNOTFOUND);
    EXPECT_EQ(err, 0xdead); EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_getids_then_invoke_named)
{
    SETUP2(ti, d); OLECHAR *names[3] = { (OLECHAR *)L"sub", (OLECHAR *)L"B", (OLECHAR *)L"A" }; DISPID ids[3]; VARIANT a[2], r; DISPPARAMS dp;
    EXPECT_EQ(ITypeInfo_GetIDsOfNames(ti, names, 3, ids), S_OK);
    dp = dp_make(a, 2, ids + 1, 2);      /* named: b first then a */
    v_i4(&a[0], 100); v_i4(&a[1], 1);    /* b = 100, a = 1 */
    EXPECT_EQ(inv(ti, &d, ids[0], DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 1 - 100);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_does_not_modify_caller_args)
{
    SETUP2(ti, d); VARIANT a[2], c[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"7");
    v_r8(&a[1], 2.75);
    memcpy(c, a, sizeof a);
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 10);
    CHECK(!memcmp(c, a, sizeof a));
    bs_free(V_BSTR(&a[0]));
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_getter_with_parameter)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0);
    v_i4(&a[0], 6);
    EXPECT_EQ(inv(ti, &d, DD_ITEM, DISPATCH_PROPERTYGET, &dp, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 66);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_dispatch_result_getter)
{
    SETUP2(ti, d); VARIANT r; DISPPARAMS dp0 = dp_make(NULL, 0, NULL, 0); Mock m; Mock_init(&m);
    d.child = (IDispatch *)&m;
    EXPECT_EQ(inv(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp0, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_DISPATCH); CHECK(V_DISPATCH(&r) == (IDispatch *)&m);
    EXPECT_EQ(m.ref, 2);   /* the getter AddRef'd; the result owns that reference */
    vclear(&r);
    EXPECT_EQ(m.ref, 1);
    EXPECT_EQ(m.invoked, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_extra_args_on_object_getter_chain_to_returned_object)
{
    /* Child.get takes no params; invoking it PROPERTYGET with one argument calls the getter, then forwards the call
       (dispid 0, same flags/params, riid=IID_NULL, lcid of the typeinfo) to the returned IDispatch. */
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); Mock m; Mock_init(&m);
    d.child = (IDispatch *)&m;
    v_i4(&a[0], 5);
    memset(&r, 0xcc, sizeof r);
    EXPECT_EQ(inv(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp, &r, NULL), S_OK);
    EXPECT_EQ(m.invoked, 1);
    EXPECT_EQ(m.last_dispid, DISPID_VALUE); EXPECT_EQ(m.last_flags, DISPATCH_PROPERTYGET);
    EXPECT_EQ(m.last_cargs, 1); EXPECT_EQ(m.last_arg0_i4, 5); EXPECT_EQ(m.last_lcid, 0x409);
    CHECK(IsEqualGUID(&m.last_riid, &DI_NULL));
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 777);
    EXPECT_EQ(m.ref, 1);   /* temporary object reference dropped */
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_object_getter_chain_with_null_object)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0);
    d.child = NULL;
    v_i4(&a[0], 5);
    EXPECT_EQ(inv(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp, &r, NULL), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(d.calls, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_extra_args_on_non_object_getter_is_notacollection)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0);
    v_i4(&a[0], 5);
    EXPECT_EQ(inv(ti, &d, DD_COUNT, DISPATCH_PROPERTYGET, &dp, &r, NULL), DISP_E_NOTACOLLECTION);
    EXPECT_EQ(d.calls, 1);    /* the getter did run */
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_extra_args_on_getter_with_params_is_badparamcount)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    EXPECT_EQ(inv(ti, &d, DD_ITEM, DISPATCH_PROPERTYGET, &dp, &r, NULL), DISP_E_BADPARAMCOUNT);
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_put_chains_through_object_getter)
{
    /* "obj.Child = x": Child has no put/putref entry but a zero-arg getter returning an object: put is forwarded to it */
    SETUP2(ti, d); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp_make(a, 1, named, 1); Mock m; Mock_init(&m);
    d.child = (IDispatch *)&m;
    v_i4(&a[0], 99);
    EXPECT_EQ(inv(ti, &d, DD_CHILD, DISPATCH_PROPERTYPUT, &dp, &r, NULL), S_OK);
    EXPECT_EQ(m.invoked, 1); EXPECT_EQ(m.last_flags, DISPATCH_PROPERTYPUT); EXPECT_EQ(m.last_dispid, DISPID_VALUE);
    EXPECT_EQ(m.last_cnamed, 1); EXPECT_EQ(m.last_arg0_i4, 99);
    EXPECT_EQ(m.ref, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_put_with_getter_having_params_is_membernotfound)
{
    SETUP2(ti, d); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp_make(a, 1, named, 1);
    v_i4(&a[0], 99);
    EXPECT_EQ(inv(ti, &d, DD_ITEM, DISPATCH_PROPERTYPUT, &dp, &r, NULL), DISP_E_MEMBERNOTFOUND);   /* Item.get takes 1 param */
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_method_flag_on_putonly_dispid_membernotfound)
{
    SETUP2(ti, d); VARIANT r; DISPPARAMS dp0 = dp_make(NULL, 0, NULL, 0);
    EXPECT_EQ(inv(ti, &d, DD_OBJ, DISPATCH_METHOD, &dp0, &r, NULL), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(inv(ti, &d, DD_OBJ, DISPATCH_PROPERTYGET, &dp0, &r, NULL), DISP_E_MEMBERNOTFOUND);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_many_iterations_no_leak_crash)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); int i, ok = 0;
    for (i = 0; i < 3000; i++) {
        vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"y");
        vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"x");
        if (inv(ti, &d, DD_CONCAT, DISPATCH_METHOD, &dp, &r, NULL) == S_OK) { if (bs_eq(V_BSTR(&r), L"xy")) ok++; bs_free(V_BSTR(&r)); }
        vclear(&a[0]); vclear(&a[1]);
    }
    EXPECT_EQ(ok, 3000);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_null_argerr_and_excepinfo_ok)
{
    SETUP2(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"q");
    v_i4(&a[1], 1);
    EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL, NULL), DISP_E_TYPEMISMATCH);   /* error path with NULL puArgErr */
    vclear(&a[0]);
    ITypeInfo_Release(ti);
}

TEST(dtd_ti_invoke_lifetime_after_release_of_interface_typeinfo)
{
    SETUP2(ti, d); ITypeInfo *itf = iface_of(ti); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    v_i4(&a[0], 1); v_i4(&a[1], 1);
    if (itf) { EXPECT_EQ(ITypeInfo_Release(itf), 0); }
    EXPECT_EQ(inv(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, NULL), S_OK);   /* the parent coclass keeps working */
    ITypeInfo_Release(ti);
}
