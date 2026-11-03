/* dtd_std_*: CreateStdDispatch (CStdDisp / CStdDispUnkImpl in oleaut32).
 * Decomp-verified: E_INVALIDARG when ptinfo or pvThis is NULL; the object AddRefs ptinfo and releases it when the last reference
 * goes; IDispatch::GetTypeInfoCount -> 1; GetTypeInfo(0) returns ptinfo (AddRef'd), other index -> DISP_E_BADINDEX; GetIDsOfNames /
 * Invoke require riid == IID_NULL (else DISP_E_UNKNOWNINTERFACE) and ignore lcid, forwarding to ptinfo->GetIDsOfNames / DispInvoke(pvThis,
 * ptinfo, ...).  The returned IUnknown is the inner unknown (answers IUnknown, IDispatch only); IDispatch's AddRef/Release/QI delegate to
 * the outer unknown (the inner one when not aggregated). */
#include "dtd.h"

static ULONG tiref(ITypeInfo *ti) { ULONG r = ITypeInfo_AddRef(ti); ITypeInfo_Release(ti); return r - 1; }

typedef struct Outer {
    const IUnknownVtbl *lpVtbl; LONG ref, adds, rels, qis; HRESULT idisp_hr;
} Outer;
static HRESULT WINAPI o_QI(IUnknown *i, REFIID r, void **p)
{
    Outer *o = (Outer *)i; o->qis++;
    if (IsEqualIID(r, &DI_IUnknown)) { *p = i; o->ref++; return S_OK; }
    *p = NULL;
    return IsEqualIID(r, &DI_IDispatch) ? o->idisp_hr : E_NOINTERFACE;
}
static ULONG WINAPI o_AddRef(IUnknown *i) { Outer *o = (Outer *)i; o->adds++; return ++o->ref; }
static ULONG WINAPI o_Release(IUnknown *i) { Outer *o = (Outer *)i; o->rels++; return --o->ref; }
static const IUnknownVtbl outer_vtbl = { o_QI, o_AddRef, o_Release };
static void outer_init(Outer *o, HRESULT idisp_hr) { memset(o, 0, sizeof *o); o->lpVtbl = &outer_vtbl; o->ref = 1; o->idisp_hr = idisp_hr; }

#define SETUP(TI, D) ITypeInfo *TI; Demo D; REQUIRE(OA(CreateStdDispatch)); REQUIRE(OA(CreateDispTypeInfo)); TI = demo_typeinfo(0x409); if (!TI) return; Demo_init(&D)

/* adapted from Wine dlls/oleaut32/tests/dispatch.c (LGPL-2.1+) */
TEST(dtd_std_create_invalid_args)
{
    SETUP(ti, d); IUnknown *unk = (IUnknown *)(ULONG_PTR)0x1234; Outer o; HRESULT hr;
    outer_init(&o, S_OK);
    hr = OA(CreateStdDispatch)(NULL, NULL, NULL, NULL); EXPECT_EQ(hr, E_INVALIDARG);
    hr = OA(CreateStdDispatch)(NULL, NULL, NULL, &unk); EXPECT_EQ(hr, E_INVALIDARG);
    hr = OA(CreateStdDispatch)(NULL, &d, NULL, &unk);   EXPECT_EQ(hr, E_INVALIDARG);
    hr = OA(CreateStdDispatch)(NULL, NULL, ti, &unk);   EXPECT_EQ(hr, E_INVALIDARG);
    hr = OA(CreateStdDispatch)((IUnknown *)&o, NULL, ti, &unk); EXPECT_EQ(hr, E_INVALIDARG);
    hr = OA(CreateStdDispatch)((IUnknown *)&o, &d, NULL, &unk); EXPECT_EQ(hr, E_INVALIDARG);
    CHECK(unk == (IUnknown *)(ULONG_PTR)0x1234);
    EXPECT_EQ(tiref(ti), 1);
    EXPECT_EQ(o.adds, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_create_returns_inner_unknown_refcount_one)
{
    SETUP(ti, d); IUnknown *unk = NULL; HRESULT hr;
    hr = OA(CreateStdDispatch)(NULL, &d, ti, &unk);
    EXPECT_EQ(hr, S_OK); CHECK(unk != NULL);
    if (unk) {
        EXPECT_EQ(IUnknown_AddRef(unk), 2);
        EXPECT_EQ(IUnknown_Release(unk), 1);
        EXPECT_EQ(IUnknown_Release(unk), 0);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_std_typeinfo_ref_held_until_release)
{
    SETUP(ti, d); IUnknown *unk = NULL;
    EXPECT_EQ(tiref(ti), 1);
    OA(CreateStdDispatch)(NULL, &d, ti, &unk);
    EXPECT_EQ(tiref(ti), 2);
    if (unk) IUnknown_Release(unk);
    EXPECT_EQ(tiref(ti), 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_typeinfo_ref_held_per_instance)
{
    SETUP(ti, d); IUnknown *u[3] = { 0 }; int i;
    for (i = 0; i < 3; i++) OA(CreateStdDispatch)(NULL, &d, ti, &u[i]);
    EXPECT_EQ(tiref(ti), 4);
    CHECK(u[0] != u[1] && u[1] != u[2]);
    for (i = 0; i < 3; i++) if (u[i]) IUnknown_Release(u[i]);
    EXPECT_EQ(tiref(ti), 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_qi_interfaces)
{
    SETUP(ti, d); IUnknown *unk = NULL, *u2 = NULL; IDispatch *disp = NULL; void *p;
    static const GUID *bad[] = { &DI_ITypeInfo, &DI_ITypeInfo2, &DI_IMarshal, &DI_IClassFactory, &DI_ISupportErrorInfo, &DI_NULL };
    unsigned i;
    OA(CreateStdDispatch)(NULL, &d, ti, &unk); if (!unk) return;
    EXPECT_EQ(IUnknown_QueryInterface(unk, &DI_IUnknown, (void **)&u2), S_OK);
    CHECK(u2 == unk); IUnknown_Release(u2);
    EXPECT_EQ(IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp), S_OK);
    CHECK(disp != NULL && (void *)disp != (void *)unk);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        p = (void *)1;
        CHECKF(IUnknown_QueryInterface(unk, bad[i], &p) == E_NOINTERFACE && p == NULL, "inner i=%u", i);
        p = (void *)1;
        CHECKF(IDispatch_QueryInterface(disp, bad[i], &p) == E_NOINTERFACE && p == NULL, "disp i=%u", i);
    }
    IDispatch_Release(disp);
    IUnknown_Release(unk);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_dispatch_identity_via_iunknown)
{
    SETUP(ti, d); IUnknown *unk = NULL, *u2 = NULL; IDispatch *disp = NULL, *d2 = NULL;
    OA(CreateStdDispatch)(NULL, &d, ti, &unk); if (!unk) return;
    IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp);
    EXPECT_EQ(IDispatch_QueryInterface(disp, &DI_IUnknown, (void **)&u2), S_OK);
    CHECK(u2 == unk);                    /* the identity IUnknown is the inner unknown */
    EXPECT_EQ(IDispatch_QueryInterface(disp, &DI_IDispatch, (void **)&d2), S_OK);
    CHECK(d2 == disp);
    if (u2) IUnknown_Release(u2); if (d2) IDispatch_Release(d2);
    IDispatch_Release(disp); IUnknown_Release(unk);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_refcount_shared_between_inner_and_dispatch)
{
    SETUP(ti, d); IUnknown *unk = NULL; IDispatch *disp = NULL;
    OA(CreateStdDispatch)(NULL, &d, ti, &unk); if (!unk) return;
    IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp);      /* 2 */
    EXPECT_EQ(IDispatch_AddRef(disp), 3);
    EXPECT_EQ(IUnknown_AddRef(unk), 4);
    EXPECT_EQ(IDispatch_Release(disp), 3);
    EXPECT_EQ(IUnknown_Release(unk), 2);
    EXPECT_EQ(IDispatch_Release(disp), 1);
    EXPECT_EQ(tiref(ti), 2);
    EXPECT_EQ(IUnknown_Release(unk), 0);
    EXPECT_EQ(tiref(ti), 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_object_alive_while_dispatch_held)
{
    SETUP(ti, d); IUnknown *unk = NULL; IDispatch *disp = NULL; VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    disp = demo_stddisp(&d, ti, &unk); if (!disp) return;
    IUnknown_Release(unk);                 /* only the IDispatch reference remains */
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 3);
    EXPECT_EQ(IDispatch_Release(disp), 0);
    EXPECT_EQ(tiref(ti), 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_gettypeinfocount_is_one)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); UINT n = 0xcc;
    if (!disp) return;
    EXPECT_EQ(IDispatch_GetTypeInfoCount(disp, &n), S_OK);
    EXPECT_EQ(n, 1);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_gettypeinfo_returns_same_typeinfo)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); ITypeInfo *got = NULL; LCID l; static const LCID lc[] = { 0, 0x409, 0x800, 0xffffffff };
    if (!disp) return;
    for (l = 0; l < 4; l++) {
        got = NULL;
        EXPECT_EQ(IDispatch_GetTypeInfo(disp, 0, lc[l], &got), S_OK);   /* lcid is not examined */
        CHECK(got == ti);
        if (got) ITypeInfo_Release(got);
    }
    EXPECT_EQ(tiref(ti), 2);
    IDispatch_Release(disp);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_gettypeinfo_addrefs)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); ITypeInfo *got = NULL, *got2 = NULL;
    if (!disp) return;
    IDispatch_GetTypeInfo(disp, 0, 0, &got);
    EXPECT_EQ(tiref(ti), 3);
    IDispatch_GetTypeInfo(disp, 0, 0, &got2);
    EXPECT_EQ(tiref(ti), 4);
    ITypeInfo_Release(got); ITypeInfo_Release(got2);
    EXPECT_EQ(tiref(ti), 2);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_gettypeinfo_bad_index)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); ITypeInfo *got = (ITypeInfo *)(ULONG_PTR)7; static const UINT idx[] = { 1, 2, 100, 0xffffffff };
    unsigned i;
    if (!disp) return;
    for (i = 0; i < 4; i++) {
        got = (ITypeInfo *)(ULONG_PTR)7;
        CHECKF(IDispatch_GetTypeInfo(disp, idx[i], 0, &got) == DISP_E_BADINDEX && got == (ITypeInfo *)(ULONG_PTR)7, "idx %u", idx[i]);
    }
    EXPECT_EQ(tiref(ti), 2);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_getids_basic)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); OLECHAR *n = (OLECHAR *)L"concat"; DISPID id = 0;
    if (!disp) return;
    EXPECT_EQ(IDispatch_GetIDsOfNames(disp, &DI_NULL, &n, 1, 0x409, &id), S_OK);
    EXPECT_EQ(id, DD_CONCAT);
    n = (OLECHAR *)L"CONCAT"; id = 0;
    EXPECT_EQ(IDispatch_GetIDsOfNames(disp, &DI_NULL, &n, 1, 0, &id), S_OK);   /* lcid ignored; typeinfo's own lcid used */
    EXPECT_EQ(id, DD_CONCAT);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_getids_named_params_and_unknowns)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); OLECHAR *n[3] = { (OLECHAR *)L"Add", (OLECHAR *)L"B", (OLECHAR *)L"nope" }; DISPID id[3];
    if (!disp) return;
    memset(id, 0x55, sizeof id);
    EXPECT_EQ(IDispatch_GetIDsOfNames(disp, &DI_NULL, n, 3, 0x409, id), DISP_E_UNKNOWNNAME);
    EXPECT_EQ(id[0], DD_ADD); EXPECT_EQ(id[1], 1); EXPECT_EQ(id[2], -1);
    n[0] = (OLECHAR *)L"nothing"; id[0] = 0;
    EXPECT_EQ(IDispatch_GetIDsOfNames(disp, &DI_NULL, n, 1, 0x409, id), DISP_E_UNKNOWNNAME);
    EXPECT_EQ(id[0], -1);
    EXPECT_EQ(IDispatch_GetIDsOfNames(disp, &DI_NULL, n, 0, 0x409, id), E_INVALIDARG);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_getids_riid_must_be_null)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); OLECHAR *n = (OLECHAR *)L"Add"; DISPID id = 0x55;
    static const GUID almost = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 1 } }, almost2 = { 1, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
    static const GUID *bad[] = { &DI_IUnknown, &DI_IDispatch, &DI_ITypeInfo, &almost, &almost2 };
    unsigned i;
    if (!disp) return;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        id = 0x55;
        CHECKF(IDispatch_GetIDsOfNames(disp, bad[i], &n, 1, 0x409, &id) == DISP_E_UNKNOWNINTERFACE && id == 0x55, "i=%u", i);
    }
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_riid_must_be_null)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    static const GUID almost = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 1 } }, almost2 = { 1, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
    static const GUID *bad[] = { &DI_IUnknown, &DI_IDispatch, &almost, &almost2 };
    unsigned i;
    if (!disp) return;
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
        CHECKF(IDispatch_Invoke(disp, DD_ADD, bad[i], 0x409, DISPATCH_METHOD, &dp, &r, NULL, NULL) == DISP_E_UNKNOWNINTERFACE, "i=%u", i);
    EXPECT_EQ(d.calls, 0);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_methods)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); EXCEPINFO ex; UINT err = 0xdead;
    if (!disp) return;
    v_i4(&a[0], 5); v_i4(&a[1], 20);
    memset(&r, 0xcc, sizeof r); memset(&ex, 0xab, sizeof ex);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_SUB, &DI_NULL, 0x409, DISPATCH_METHOD, &dp, &r, &ex, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 15);
    EXPECT_EQ(err, 0xdead); EXPECT_EQ(((BYTE *)&ex)[0], 0xab);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 25);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_property_put_get)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); VARIANT a[1], r; DISPID named[1] = { DISPID_PROPERTYPUT };
    DISPPARAMS dpput = dp_make(a, 1, named, 1), dpget = dp_make(NULL, 0, NULL, 0);
    if (!disp) return;
    v_i4(&a[0], 31337);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_VALUE, &DI_NULL, 0x409, DISPATCH_PROPERTYPUT, &dpput, NULL, NULL, NULL), S_OK);
    EXPECT_EQ(d.value, 31337);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_VALUE, &DI_NULL, 0x409, DISPATCH_PROPERTYGET, &dpget, &r, NULL, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 31337);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_VALUE, &DI_NULL, 0x409, DISPATCH_PROPERTYGET | DISPATCH_METHOD, &dpget, &r, NULL, NULL), S_OK);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_VALUE, &DI_NULL, 0x409, DISPATCH_PROPERTYPUT, &dpget, &r, NULL, NULL), DISP_E_BADPARAMCOUNT);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_error_codes)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0), dp1 = dp_make(a, 1, NULL, 0); UINT err = 0xdead;
    if (!disp) return;
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    EXPECT_EQ(IDispatch_Invoke(disp, 0x7000, &DI_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, &err), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, DISPATCH_METHOD, &dp1, &r, NULL, &err), DISP_E_BADPARAMCOUNT);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, DISPATCH_PROPERTYGET, &dp, &r, NULL, &err), DISP_E_MEMBERNOTFOUND);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, 0x20, &dp, &r, NULL, &err), E_INVALIDARG);
    vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"zz");
    err = 0xdead;
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(err, 1);
    vclear(&a[1]); v_r8(&a[1], 1e30); err = 0xdead;
    EXPECT_EQ(IDispatch_Invoke(disp, DD_ADD, &DI_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, &err), DISP_E_OVERFLOW);
    EXPECT_EQ(err, 1);
    EXPECT_EQ(d.calls, 0);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_uses_pvthis_passed_at_creation)
{
    ITypeInfo *ti; Demo d1, d2; IDispatch *x, *y; VARIANT r; DISPPARAMS dp0 = dp_make(NULL, 0, NULL, 0);
    REQUIRE(OA(CreateStdDispatch)); REQUIRE(OA(CreateDispTypeInfo));
    ti = demo_typeinfo(0x409); if (!ti) return;
    Demo_init(&d1); Demo_init(&d2); d1.value = 11; d2.value = 22;
    x = demo_stddisp(&d1, ti, NULL); y = demo_stddisp(&d2, ti, NULL);
    if (x && y) {
        IDispatch_Invoke(x, DD_VALUE, &DI_NULL, 0, DISPATCH_PROPERTYGET, &dp0, &r, NULL, NULL); EXPECT_EQ(V_I4(&r), 11);
        IDispatch_Invoke(y, DD_VALUE, &DI_NULL, 0, DISPATCH_PROPERTYGET, &dp0, &r, NULL, NULL); EXPECT_EQ(V_I4(&r), 22);
    }
    if (x) IDispatch_Release(x); if (y) IDispatch_Release(y);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_named_args_through_getids)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); OLECHAR *n[3] = { (OLECHAR *)L"Sub", (OLECHAR *)L"b", (OLECHAR *)L"a" }; DISPID id[3]; VARIANT a[2], r; DISPPARAMS dp;
    if (!disp) return;
    EXPECT_EQ(IDispatch_GetIDsOfNames(disp, &DI_NULL, n, 3, 0x409, id), S_OK);
    dp = dp_make(a, 2, id + 1, 2);
    v_i4(&a[0], 4); v_i4(&a[1], 50);    /* b=4, a=50 */
    EXPECT_EQ(IDispatch_Invoke(disp, id[0], &DI_NULL, 0x409, DISPATCH_METHOD, &dp, &r, NULL, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 46);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_invoke_object_chain_and_lcid)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); Mock m;
    if (!disp) return;
    Mock_init(&m); d.child = (IDispatch *)&m;
    v_i4(&a[0], 12);
    EXPECT_EQ(IDispatch_Invoke(disp, DD_CHILD, &DI_NULL, 0x407, DISPATCH_PROPERTYGET, &dp, &r, NULL, NULL), S_OK);
    EXPECT_EQ(m.invoked, 1);
    EXPECT_EQ(m.last_lcid, 0x409);   /* the typeinfo's lcid, not the caller's */
    EXPECT_EQ(V_I4(&r), 777);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

/* aggregation */
TEST(dtd_std_aggregated_inner_unknown_is_distinct)
{
    SETUP(ti, d); Outer o; IUnknown *unk = NULL; IDispatch *disp = NULL; HRESULT hr;
    outer_init(&o, S_OK);
    hr = OA(CreateStdDispatch)((IUnknown *)&o, &d, ti, &unk);
    EXPECT_EQ(hr, S_OK); CHECK(unk != NULL && unk != (IUnknown *)&o);
    if (unk) {
        EXPECT_EQ(o.adds, 0);   /* creation does not touch the outer */
        EXPECT_EQ(IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp), S_OK);
        CHECK(disp != NULL);
        if (disp) IDispatch_Release(disp);
        IUnknown_Release(unk);
    }
    ITypeInfo_Release(ti);
}

TEST(dtd_std_aggregated_dispatch_delegates_to_outer)
{
    SETUP(ti, d); Outer o; IUnknown *unk = NULL, *u2 = NULL; IDispatch *disp = NULL; ULONG r;
    outer_init(&o, S_OK);
    OA(CreateStdDispatch)((IUnknown *)&o, &d, ti, &unk); if (!unk) return;
    EXPECT_EQ(IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp), S_OK);   /* returns the IDispatch AddRef'd via the outer */
    EXPECT_EQ(o.adds, 1); EXPECT_EQ(o.ref, 2);
    r = IDispatch_AddRef(disp);  EXPECT_EQ(r, 3); EXPECT_EQ(o.adds, 2);
    r = IDispatch_Release(disp); EXPECT_EQ(r, 2); EXPECT_EQ(o.rels, 1);
    EXPECT_EQ(IDispatch_QueryInterface(disp, &DI_IUnknown, (void **)&u2), S_OK);     /* delegated to the outer unknown */
    CHECK(u2 == (IUnknown *)&o); EXPECT_EQ(o.qis, 1);
    if (u2) IUnknown_Release(u2);
    IDispatch_Release(disp);
    EXPECT_EQ(o.ref, 1);
    IUnknown_Release(unk);
    ITypeInfo_Release(ti);
}

/* adapted from Wine dlls/oleaut32/tests/dispatch.c (LGPL-2.1+) */
TEST(dtd_std_aggregated_outer_refuses_idispatch)
{
    SETUP(ti, d); Outer o; IUnknown *unk = NULL; IDispatch *disp = NULL, *disp2 = (IDispatch *)(ULONG_PTR)1;
    outer_init(&o, (HRESULT)0x80010003);
    OA(CreateStdDispatch)((IUnknown *)&o, &d, ti, &unk); if (!unk) return;
    EXPECT_EQ(IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp), S_OK);   /* the inner unknown always hands out its IDispatch */
    CHECK(disp != NULL);
    if (disp) {
        EXPECT_EQ(IDispatch_QueryInterface(disp, &DI_IDispatch, (void **)&disp2), (HRESULT)0x80010003);   /* but QI on it goes to the outer */
        CHECK(disp2 == NULL);
        IDispatch_Release(disp);
    }
    IUnknown_Release(unk);
    ITypeInfo_Release(ti);
}

TEST(dtd_std_aggregated_release_order_frees_typeinfo)
{
    SETUP(ti, d); Outer o; IUnknown *unk = NULL; IDispatch *disp = NULL;
    outer_init(&o, S_OK);
    OA(CreateStdDispatch)((IUnknown *)&o, &d, ti, &unk); if (!unk) return;
    IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp);
    EXPECT_EQ(tiref(ti), 2);
    EXPECT_EQ(IUnknown_Release(unk), 0);     /* inner count was 1: object (and its typeinfo ref) is destroyed now */
    EXPECT_EQ(tiref(ti), 1);
    (void)disp;                               /* must not be used any more */
    ITypeInfo_Release(ti);
}

/* concurrency on the same std dispatch (no COM needed; plain pointer use) */
typedef struct Work { IDispatch *disp; int ok; } Work;
static DWORD WINAPI add_worker(LPVOID p)
{
    Work *w = p; int i; VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0);
    for (i = 0; i < 300; i++) {
        v_i4(&a[0], i); v_i4(&a[1], 1000);
        if (IDispatch_Invoke(w->disp, DD_ADD, &DI_NULL, 0, DISPATCH_METHOD, &dp, &r, NULL, NULL) == S_OK && V_I4(&r) == 1000 + i) w->ok++;
    }
    return 0;
}

TEST(dtd_std_concurrent_invoke)
{
    SETUP(ti, d); IDispatch *disp = demo_stddisp(&d, ti, NULL); HANDLE h[4]; Work w[4]; int i, total = 0;
    if (!disp) return;
    for (i = 0; i < 4; i++) { w[i].disp = disp; w[i].ok = 0; h[i] = CreateThread(NULL, 0, add_worker, &w[i], 0, NULL); CHECK(h[i] != NULL); }
    for (i = 0; i < 4; i++) if (h[i]) { EXPECT_EQ(WaitForSingleObject(h[i], 20000), WAIT_OBJECT_0); CloseHandle(h[i]); total += w[i].ok; }
    EXPECT_EQ(total, 1200);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_std_stdole2_typeinfo_interface_wine_case)
{
    /* adapted from Wine dlls/oleaut32/tests/dispatch.c (LGPL-2.1+): a typeinfo from a real typelib works as well */
    ITypeLib *tl = NULL; ITypeInfo *ti = NULL; IUnknown *unk = NULL; IDispatch *disp = NULL; HRESULT hr; Demo d;
    REQUIRE(OA(CreateStdDispatch)); REQUIRE(OA(LoadTypeLib));
    hr = OA(LoadTypeLib)(L"stdole2.tlb", &tl);
    if (hr != S_OK) { t_skip("LoadTypeLib(stdole2.tlb) hr=%08lx", (unsigned long)hr); return; }
    hr = ITypeLib_GetTypeInfoOfGuid(tl, &DI_IUnknown, &ti);
    ITypeLib_Release(tl);
    if (hr != S_OK) { t_skip("no IUnknown typeinfo in stdole2 hr=%08lx", (unsigned long)hr); return; }
    Demo_init(&d);
    hr = OA(CreateStdDispatch)(NULL, &d, ti, &unk);
    EXPECT_EQ(hr, S_OK);
    if (unk) {
        EXPECT_EQ(IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp), S_OK);
        CHECK(disp != NULL && (void *)disp != (void *)unk);
        if (disp) {
            UINT n = 0; ITypeInfo *got = NULL;
            EXPECT_EQ(IDispatch_GetTypeInfoCount(disp, &n), S_OK); EXPECT_EQ(n, 1);
            EXPECT_EQ(IDispatch_GetTypeInfo(disp, 0, 0, &got), S_OK); CHECK(got == ti); if (got) ITypeInfo_Release(got);
            IDispatch_Release(disp);
        }
        IUnknown_Release(unk);
    }
    ITypeInfo_Release(ti);
}
