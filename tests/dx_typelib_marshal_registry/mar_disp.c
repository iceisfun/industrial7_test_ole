/* Typelib marshaling of the dual interface IOletDual, IDispatch::Invoke across apartments and IEnumVARIANT proxying. */
#include "mar.h"
#define MD(c, name, ...) ((c)->lpVtbl->name((c), __VA_ARGS__))
#define MD0(c, name) ((c)->lpVtbl->name((c)))
#define DISP_BADINDEX      ((HRESULT)0x8002000BL)
#define DISP_UNKNOWNNAME   ((HRESULT)0x80020006L)
#define DISP_BADPARAMCOUNT ((HRESULT)0x8002000EL)

static IDispatch *get_disp(IOletDual *d)
{
    IDispatch *disp = NULL;
    HRESULT hr = IUnknown_QueryInterface((IUnknown *)d, &IID_IDispatch, (void **)&disp);
    CHECKF(hr == S_OK && disp, "QI(IDispatch) hr=0x%lx", (long)hr);
    return disp;
}
static HRESULT invoke(IDispatch *disp, DISPID id, WORD flags, VARIANT *args, UINT nargs, VARIANT *res, EXCEPINFO *ei, UINT *argerr)
{
    DISPPARAMS dp; dp.rgvarg = args; dp.cArgs = nargs; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL;
    if (res) VariantInit(res);
    if (ei) memset(ei, 0, sizeof *ei);
    return IDispatch_Invoke(disp, id, &TM_IID_NULL, 0, flags, &dp, res, ei, argerr);
}

/* ------------------------------------------------------------------ vtable calls */
static void cl_dual_vtbl(void *p, void *ctx)
{
    CLID(d, p); LONG r = 0; BSTR a, b, s = NULL; VARIANT v; HRESULT hr; LONG n = 0;
    EQ(MD(d, Add, 20, 22, &r), S_OK); EQ(r, 42);
    a = SysAllocString(L"foo"); b = SysAllocString(L"bar");
    EQ(MD(d, Concat, a, b, &s), S_OK);
    CHECK(s && !lstrcmpW(s, L"foobar"));
    SysFreeString(s); SysFreeString(a); SysFreeString(b);
    VariantInit(&v);
    EQ(MD(d, get_Value, &v), S_OK);
    EQ(V_VT(&v), VT_I4); EQ(V_I4(&v), 5);
    VariantClear(&v);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"stored");
    EQ(MD(d, put_Value, v), S_OK);
    VariantClear(&v);
    EQ(MD(d, get_Value, &v), S_OK);
    EQ(V_VT(&v), VT_BSTR);
    if (V_VT(&v) == VT_BSTR) CHECK(V_BSTR(&v) && !lstrcmpW(V_BSTR(&v), L"stored"));
    VariantClear(&v);
    EQ(MD(d, get_Count, &n), S_OK); EQ(n, 3);
    {
        LONG i;
        for (i = 0; i < 3; i++) {
            WCHAR w[16]; BSTR nm = NULL; _snwprintf(w, 16, L"item%ld", (long)i);
            hr = MD(d, ItemName, i, &nm);
            EQ(hr, S_OK);
            CHECK(nm && !lstrcmpW(nm, w));
            SysFreeString(nm);
        }
        s = (BSTR)(ULONG_PTR)1;
        hr = MD(d, ItemName, 3, &s);
        EQ(hr, E_INVALIDARG);
    }
    EQ(MD(d, Fail, S_FALSE), S_FALSE);
    EQ(MD(d, Fail, E_NOTIMPL), E_NOTIMPL);
    EQ(MD(d, Fail, (LONG)0x80041234), (LONG)0x80041234);
}
TMTEST(dxtm_dual_vtable_calls) { RUN_DUAL(cl_dual_vtbl, NULL); }

static void cl_dual_big(void *p, void *ctx)
{
    CLID(d, p); BSTR a = SysAllocStringLen(NULL, 100000), b = SysAllocStringLen(NULL, 90000), s = NULL; UINT i;
    for (i = 0; i < 100000; i++) a[i] = (WCHAR)(1 + i % 5000);
    for (i = 0; i < 90000; i++) b[i] = (WCHAR)(2 + i % 4000);
    EQ(MD(d, Concat, a, b, &s), S_OK);
    CHECK(s && SysStringLen(s) == 190000);
    if (s && SysStringLen(s) == 190000) CHECK(!memcmp(s, a, 200000) && !memcmp(s + 100000, b, 180000));
    SysFreeString(s); SysFreeString(a); SysFreeString(b);
}
TMTEST(dxtm_dual_large_strings) { RUN_DUAL(cl_dual_big, NULL); }

static void cl_dual_values(void *p, void *ctx)
{
    CLID(d, p); VARIANT in, out; int i;
    for (i = 0; i < 6; i++) {
        VariantInit(&in);
        switch (i) {
        case 0: break;
        case 1: V_VT(&in) = VT_R8; V_R8(&in) = 6.5; break;
        case 2: V_VT(&in) = VT_BOOL; V_BOOL(&in) = VARIANT_TRUE; break;
        case 3: V_VT(&in) = VT_NULL; break;
        case 4: V_VT(&in) = VT_I2; V_I2(&in) = -3; break;
        case 5: V_VT(&in) = VT_DATE; V_DATE(&in) = 1.5; break;
        }
        EQ(MD(d, put_Value, in), S_OK);
        VariantInit(&out);
        EQ(MD(d, get_Value, &out), S_OK);
        CHECKF(V_VT(&out) == V_VT(&in), "case %d: vt %u want %u", i, V_VT(&out), V_VT(&in));
        if (V_VT(&out) == VT_R8) CHECK(V_R8(&out) == 6.5);
        if (V_VT(&out) == VT_I2) EQ(V_I2(&out), -3);
        if (V_VT(&out) == VT_BOOL) EQ(V_BOOL(&out), VARIANT_TRUE);
        VariantClear(&out);
    }
}
TMTEST(dxtm_dual_property_variant_types) { RUN_DUAL(cl_dual_values, NULL); }

static void cl_dual_sum(void *p, void *ctx)
{
    CLID(d, p); SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, 0, 10); LONG i, r = 0;
    for (i = 0; i < 10; i++) { LONG v = i * i; SafeArrayPutElement(sa, &i, &v); }
    EQ(MD(d, Sum, sa, &r), S_OK); EQ(r, 285);
    SafeArrayDestroy(sa);
}
TMTEST(dxtm_dual_safearray_parameter) { RUN_DUAL(cl_dual_sum, NULL); }

static void cl_dual_err(void *p, void *ctx)
{
    CLID(d, p); BSTR desc = SysAllocString(L"dual failure"); IErrorInfo *ei = NULL; GUID g; BSTR got = NULL;
    SetErrorInfo(0, NULL);
    EQ(MD(d, RaiseError, desc, E_INVALIDARG), E_INVALIDARG);
    EQ(GetErrorInfo(0, &ei), S_OK);
    if (ei) {
        EQ(IErrorInfo_GetGUID(ei, &g), S_OK);
        CHECK(IsEqualGUID(&g, &TM_IID_IOletDual));
        EQ(IErrorInfo_GetDescription(ei, &got), S_OK);
        CHECK(got && !lstrcmpW(got, L"dual failure"));
        SysFreeString(got); IErrorInfo_Release(ei);
    }
    SysFreeString(desc);
}
TMTEST(dxtm_dual_errorinfo_through_vtable) { RUN_DUAL(cl_dual_err, NULL); }

/* ------------------------------------------------------------------ IDispatch */
static void cl_disp_typeinfo(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); UINT n = 99; ITypeInfo *ti = (ITypeInfo *)(ULONG_PTR)1; HRESULT hr;
    if (!disp) return;
    EQ(IDispatch_GetTypeInfoCount(disp, &n), S_OK); EQ(n, 1);
    hr = IDispatch_GetTypeInfo(disp, 1, 0, &ti);
    EQ(hr, DISP_BADINDEX);
    CHECK(ti == NULL);
    ti = NULL;
    hr = IDispatch_GetTypeInfo(disp, 0, 0, &ti);
    EQ(hr, S_OK);
    if (ti) {
        TYPEATTR *ta = NULL;
        EQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) {
            CHECK(IsEqualGUID(&ta->guid, &TM_IID_IOletDual));
            CHECKF(ta->cFuncs >= 10, "cFuncs=%u", ta->cFuncs);
            CHECK(ta->wTypeFlags & TYPEFLAG_FDUAL);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_idispatch_typeinfo) { RUN_DUAL(cl_disp_typeinfo, NULL); }

static const struct { const WCHAR *name; DISPID id; } ids[] = {
    { L"Add", 1 }, { L"add", 1 }, { L"ADD", 1 }, { L"Concat", 2 }, { L"Value", 3 }, { L"value", 3 }, { L"Count", 4 }, { L"Fail", 5 },
    { L"ItemName", 6 }, { L"RaiseError", 7 }, { L"Sum", 8 }, { L"_NewEnum", -4 } };
static void cl_disp_ids(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); unsigned i; HRESULT hr; DISPID id;
    if (!disp) return;
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        LPOLESTR nm = (LPOLESTR)ids[i].name; id = 0x7777;
        hr = IDispatch_GetIDsOfNames(disp, &TM_IID_NULL, &nm, 1, 0, &id);
        CHECKF(hr == S_OK && id == ids[i].id, "GetIDsOfNames(%ls): hr=0x%lx id=%ld want %ld", ids[i].name, (long)hr, (long)id, (long)ids[i].id);
    }
    {
        LPOLESTR nm = L"NoSuchMember"; id = 0x7777;
        hr = IDispatch_GetIDsOfNames(disp, &TM_IID_NULL, &nm, 1, 0, &id);
        EQ(hr, DISP_UNKNOWNNAME);
        EQ(id, DISPID_UNKNOWN);
    }
    {
        LPOLESTR nm[3] = { L"Add", L"b", L"a" }; DISPID got[3] = { 9, 9, 9 };
        hr = IDispatch_GetIDsOfNames(disp, &TM_IID_NULL, nm, 3, 0, got);
        EQ(hr, S_OK); EQ(got[0], 1); EQ(got[1], 1); EQ(got[2], 0);     /* parameter dispids are the parameter positions */
        nm[2] = L"zz"; got[0] = got[1] = got[2] = 9;
        hr = IDispatch_GetIDsOfNames(disp, &TM_IID_NULL, nm, 3, 0, got);
        EQ(hr, DISP_UNKNOWNNAME);
        EQ(got[0], 1); EQ(got[1], 1); EQ(got[2], DISPID_UNKNOWN);
    }
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_idispatch_getidsofnames) { RUN_DUAL(cl_disp_ids, NULL); }

static void cl_disp_invoke(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT args[2], res; HRESULT hr; UINT argerr = 99; EXCEPINFO ei;
    if (!disp) return;
    VariantInit(&args[0]); VariantInit(&args[1]);
    V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 5;        /* a (rgvarg is in reverse order) */
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 7;        /* b */
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);
    EQ(hr, S_OK); EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 12);
    VariantClear(&res);
    /* the call also works with the combined METHOD|PROPERTYGET flags and with no EXCEPINFO/argerr */
    hr = invoke(disp, 1, DISPATCH_METHOD | DISPATCH_PROPERTYGET, args, 2, &res, NULL, NULL);
    EQ(hr, S_OK); EQ(V_I4(&res), 12);
    VariantClear(&res);
    /* arguments are coerced to the parameter types */
    V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"20");
    V_VT(&args[0]) = VT_I2; V_I2(&args[0]) = 22;
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);
    EQ(hr, S_OK); EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 42);
    VariantClear(&res); SysFreeString(V_BSTR(&args[1]));
    V_VT(&args[1]) = VT_R8; V_R8(&args[1]) = 2.0;
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);
    EQ(hr, S_OK); EQ(V_I4(&res), 24);
    VariantClear(&res);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_invoke_method) { RUN_DUAL(cl_disp_invoke, NULL); }

static void cl_disp_invoke_errors(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT args[2], res; HRESULT hr; UINT argerr; EXCEPINFO ei;
    if (!disp) return;
    VariantInit(&args[0]); VariantInit(&args[1]);
    V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 5;
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 7;
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 1, &res, &ei, &argerr);
    EQ(hr, DISP_BADPARAMCOUNT);
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 0, &res, &ei, &argerr);
    EQ(hr, DISP_BADPARAMCOUNT);
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);   /* sanity: still fine */
    EQ(hr, S_OK);
    hr = invoke(disp, 12345, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);
    EQ(hr, DISP_E_MEMBERNOTFOUND);
    /* type mismatch reports the index of the offending element of rgvarg */
    V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"abc");
    argerr = 99;
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);
    EQ(hr, DISP_E_TYPEMISMATCH);
    t_note("puArgErr for a mismatch in rgvarg[1] (documented: index within rgvarg): %u", argerr);
    SysFreeString(V_BSTR(&args[1]));
    V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 1;
    V_VT(&args[0]) = VT_BSTR; V_BSTR(&args[0]) = SysAllocString(L"xyz");
    argerr = 99;
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, &ei, &argerr);
    EQ(hr, DISP_E_TYPEMISMATCH);
    t_note("puArgErr for a mismatch in rgvarg[0]: %u", argerr);
    SysFreeString(V_BSTR(&args[0]));
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_invoke_error_codes) { RUN_DUAL(cl_disp_invoke_errors, NULL); }

static void cl_disp_named(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT args[2], res; DISPPARAMS dp; DISPID named[2]; HRESULT hr;
    if (!disp) return;
    /* Concat(a, b) called with named arguments in reverse: b first */
    VariantInit(&args[0]); VariantInit(&args[1]); VariantInit(&res);
    V_VT(&args[0]) = VT_BSTR; V_BSTR(&args[0]) = SysAllocString(L"Y");      /* b */
    V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"X");      /* a */
    named[0] = 1; named[1] = 0;
    dp.rgvarg = args; dp.cArgs = 2; dp.cNamedArgs = 2; dp.rgdispidNamedArgs = named;
    hr = IDispatch_Invoke(disp, 2, &TM_IID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL);
    EQ(hr, S_OK);
    EQ(V_VT(&res), VT_BSTR);
    if (V_VT(&res) == VT_BSTR) CHECK(V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"XY"));
    VariantClear(&res); VariantClear(&args[0]); VariantClear(&args[1]);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_invoke_named_arguments) { RUN_DUAL(cl_disp_named, NULL); }

static void cl_disp_prop(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT arg, res; DISPPARAMS dp; DISPID put = DISPID_PROPERTYPUT; HRESULT hr;
    if (!disp) return;
    hr = invoke(disp, 3, DISPATCH_PROPERTYGET, NULL, 0, &res, NULL, NULL);
    EQ(hr, S_OK); EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 5);
    VariantInit(&arg); V_VT(&arg) = VT_I4; V_I4(&arg) = 321;
    dp.rgvarg = &arg; dp.cArgs = 1; dp.cNamedArgs = 1; dp.rgdispidNamedArgs = &put;
    VariantInit(&res);
    hr = IDispatch_Invoke(disp, 3, &TM_IID_NULL, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    EQ(hr, S_OK);
    hr = invoke(disp, 3, DISPATCH_PROPERTYGET, NULL, 0, &res, NULL, NULL);
    EQ(hr, S_OK); EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 321);
    /* the vtable view of the same object sees the change */
    {
        VARIANT v; VariantInit(&v);
        EQ(MD(d, get_Value, &v), S_OK); EQ(V_I4(&v), 321);
    }
    /* Count is read only: PROPERTYPUT on it is rejected */
    V_I4(&arg) = 9;
    hr = IDispatch_Invoke(disp, 4, &TM_IID_NULL, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
    CHECK(FAILED(hr));
    t_note("PROPERTYPUT on a read-only property: hr=0x%lx", (long)hr);
    hr = invoke(disp, 4, DISPATCH_PROPERTYGET, NULL, 0, &res, NULL, NULL);
    EQ(hr, S_OK); EQ(V_I4(&res), 3);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_invoke_property_get_put) { RUN_DUAL(cl_disp_prop, NULL); }

static void cl_disp_exc(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT args[2], res; HRESULT hr; EXCEPINFO ei; UINT ae = 0;
    if (!disp) return;
    VariantInit(&args[0]); VariantInit(&args[1]);
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = E_NOTIMPL;
    hr = invoke(disp, 5, DISPATCH_METHOD, args, 1, &res, &ei, &ae);
    EQ(hr, DISP_E_EXCEPTION);
    EQ(ei.scode, E_NOTIMPL);
    EQ(ei.wCode, 0);
    SysFreeString(ei.bstrSource); SysFreeString(ei.bstrDescription); SysFreeString(ei.bstrHelpFile);
    /* a method with error info: EXCEPINFO carries it */
    V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"raised in server");
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = E_FAIL;
    SetErrorInfo(0, NULL);
    hr = invoke(disp, 7, DISPATCH_METHOD, args, 2, &res, &ei, &ae);
    EQ(hr, DISP_E_EXCEPTION);
    EQ(ei.scode, E_FAIL);
    CHECKF(ei.bstrDescription && !lstrcmpW(ei.bstrDescription, L"raised in server"), "description \"%ls\"", ei.bstrDescription ? ei.bstrDescription : L"(null)");
    t_note("EXCEPINFO source=%ls helpfile=%ls helpcontext=%lu", ei.bstrSource ? ei.bstrSource : L"(null)", ei.bstrHelpFile ? ei.bstrHelpFile : L"(null)", (unsigned long)ei.dwHelpContext);
    SysFreeString(ei.bstrSource); SysFreeString(ei.bstrDescription); SysFreeString(ei.bstrHelpFile);
    SysFreeString(V_BSTR(&args[1]));
    /* a successful HRESULT other than S_OK is returned as is */
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = S_FALSE;
    hr = invoke(disp, 5, DISPATCH_METHOD, args, 1, &res, &ei, &ae);
    t_note("Invoke of a method returning S_FALSE: hr=0x%lx", (long)hr);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_invoke_exception_info) { RUN_DUAL(cl_disp_exc, NULL); }

static void cl_disp_sa(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT arg, res; SAFEARRAY *sa; LONG i; HRESULT hr;
    if (!disp) return;
    sa = SafeArrayCreateVector(VT_I4, 1, 4);
    for (i = 1; i <= 4; i++) { LONG v = i * 10; SafeArrayPutElement(sa, &i, &v); }
    VariantInit(&arg); V_VT(&arg) = VT_ARRAY | VT_I4; V_ARRAY(&arg) = sa;
    hr = invoke(disp, 8, DISPATCH_METHOD, &arg, 1, &res, NULL, NULL);
    EQ(hr, S_OK); EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 100);
    SafeArrayDestroy(sa);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_invoke_safearray_argument) { RUN_DUAL(cl_disp_sa, NULL); }

/* ------------------------------------------------------------------ IDispatch as the marshaled interface */
static void cl_asdisp(void *p, void *ctx)
{
    IDispatch *disp = (IDispatch *)p; VARIANT args[2], res; HRESULT hr; void *x = NULL;
    if (!disp) { CHECKF(0, "no interface hr=0x%lx", (long)tm_client_hr); return; }
    VariantInit(&args[0]); VariantInit(&args[1]);
    V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 30;
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 12;
    hr = invoke(disp, 1, DISPATCH_METHOD, args, 2, &res, NULL, NULL);
    EQ(hr, S_OK); EQ(V_I4(&res), 42);
    /* from an IDispatch proxy the dual interface can be reached again */
    hr = IDispatch_QueryInterface(disp, &TM_IID_IOletDual, &x);
    EQ(hr, S_OK);
    if (x) {
        LONG r = 0;
        EQ(((IOletDual *)x)->lpVtbl->Add((IOletDual *)x, 1, 2, &r), S_OK); EQ(r, 3);
        ((IOletDual *)x)->lpVtbl->Release((IOletDual *)x);
    }
}
TMTEST(dxtm_dual_marshaled_as_idispatch)
{
    CHECK(tm_run_all(tm_mk_dual, &IID_IDispatch, cl_asdisp, NULL));
    tm_leakcheck();
}

static void cl_asunk(void *p, void *ctx)
{
    IUnknown *u = (IUnknown *)p; IOletDual *d = NULL; IOletCustom *cu = NULL; LONG r = 0;
    if (!u) { CHECKF(0, "no interface hr=0x%lx", (long)tm_client_hr); return; }
    EQ(IUnknown_QueryInterface(u, &TM_IID_IOletDual, (void **)&d), S_OK);
    if (d) { EQ(MD(d, Add, 4, 5, &r), S_OK); EQ(r, 9); d->lpVtbl->Release(d); }
    EQ(IUnknown_QueryInterface(u, &TM_IID_IOletCustom, (void **)&cu), E_NOINTERFACE);
}
TMTEST(dxtm_dual_marshaled_as_iunknown_then_qi)
{
    CHECK(tm_run_all(tm_mk_dual, &IID_IUnknown, cl_asunk, NULL));
    tm_leakcheck();
}

/* ------------------------------------------------------------------ IEnumVARIANT */
static void cl_enum_vtbl(void *p, void *ctx)
{
    CLID(d, p); IUnknown *u = NULL; IEnumVARIANT *e = NULL; VARIANT v[4]; ULONG f = 99; HRESULT hr; int i;
    EQ(MD(d, get__NewEnum, &u), S_OK);
    CHECK(u != NULL);
    if (!u) return;
    EQ(IUnknown_QueryInterface(u, &IID_IEnumVARIANT, (void **)&e), S_OK);
    IUnknown_Release(u);
    if (!e) return;
    for (i = 0; i < 3; i++) {
        VariantInit(&v[0]); f = 99;
        hr = IEnumVARIANT_Next(e, 1, v, &f);
        EQ(hr, S_OK); EQ(f, 1); EQ(V_VT(&v[0]), VT_I4); EQ(V_I4(&v[0]), 10 * (i + 1));
    }
    f = 99;
    hr = IEnumVARIANT_Next(e, 1, v, &f);
    EQ(hr, S_FALSE); EQ(f, 0);
    EQ(IEnumVARIANT_Reset(e), S_OK);
    for (i = 0; i < 4; i++) VariantInit(&v[i]);
    f = 99;
    hr = IEnumVARIANT_Next(e, 4, v, &f);       /* asks for more than exist */
    EQ(hr, S_FALSE); EQ(f, 3);
    EQ(V_I4(&v[0]), 10); EQ(V_I4(&v[1]), 20); EQ(V_I4(&v[2]), 30);
    IEnumVARIANT_Release(e);
}
TMTEST(dxtm_dual_newenum_next_reset) { RUN_DUAL(cl_enum_vtbl, NULL); }

static void cl_enum_skip_clone(void *p, void *ctx)
{
    CLID(d, p); IUnknown *u = NULL; IEnumVARIANT *e = NULL, *c2 = NULL; VARIANT v; ULONG f = 0; HRESULT hr;
    EQ(MD(d, get__NewEnum, &u), S_OK);
    if (!u) return;
    IUnknown_QueryInterface(u, &IID_IEnumVARIANT, (void **)&e);
    IUnknown_Release(u);
    if (!e) return;
    EQ(IEnumVARIANT_Skip(e, 1), S_OK);
    hr = IEnumVARIANT_Clone(e, &c2);
    EQ(hr, S_OK);
    VariantInit(&v);
    EQ(IEnumVARIANT_Next(e, 1, &v, &f), S_OK); EQ(V_I4(&v), 20);
    if (c2) {
        VariantInit(&v); f = 0;
        EQ(IEnumVARIANT_Next(c2, 1, &v, &f), S_OK); EQ(V_I4(&v), 20);        /* the clone has its own position */
        EQ(IEnumVARIANT_Skip(c2, 5), S_FALSE);
        f = 9; EQ(IEnumVARIANT_Next(c2, 1, &v, &f), S_FALSE); EQ(f, 0);
        IEnumVARIANT_Release(c2);
    }
    VariantInit(&v); f = 0;
    EQ(IEnumVARIANT_Next(e, 1, &v, &f), S_OK); EQ(V_I4(&v), 30);
    IEnumVARIANT_Release(e);
}
TMTEST(dxtm_dual_newenum_skip_clone) { RUN_DUAL(cl_enum_skip_clone, NULL); }

static void cl_enum_invoke(void *p, void *ctx)
{
    CLID(d, p); IDispatch *disp = get_disp(d); VARIANT res; HRESULT hr; IEnumVARIANT *e = NULL;
    if (!disp) return;
    hr = invoke(disp, DISPID_NEWENUM, DISPATCH_PROPERTYGET | DISPATCH_METHOD, NULL, 0, &res, NULL, NULL);
    EQ(hr, S_OK);
    CHECKF(V_VT(&res) == VT_UNKNOWN || V_VT(&res) == VT_DISPATCH, "vt %u", V_VT(&res));
    if ((V_VT(&res) == VT_UNKNOWN || V_VT(&res) == VT_DISPATCH) && V_UNKNOWN(&res)) {
        VARIANT v; ULONG f = 0;
        EQ(IUnknown_QueryInterface(V_UNKNOWN(&res), &IID_IEnumVARIANT, (void **)&e), S_OK);
        if (e) { VariantInit(&v); EQ(IEnumVARIANT_Next(e, 1, &v, &f), S_OK); EQ(V_I4(&v), 10); IEnumVARIANT_Release(e); }
    }
    VariantClear(&res);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_newenum_through_invoke) { RUN_DUAL(cl_enum_invoke, NULL); }

static void cl_enum_prop_only(void *p, void *ctx)
{
    /* dispid -4 is the standard enumerator id; GetIDsOfNames resolves "_NewEnum" to it and the member is restricted */
    CLID(d, p); IDispatch *disp = get_disp(d); ITypeInfo *ti = NULL; FUNCDESC *fd = NULL; UINT i, n; int found = 0;
    if (!disp) return;
    if (SUCCEEDED(IDispatch_GetTypeInfo(disp, 0, 0, &ti)) && ti) {
        TYPEATTR *ta = NULL; ITypeInfo_GetTypeAttr(ti, &ta);
        n = ta ? ta->cFuncs : 0;
        for (i = 0; i < n; i++) {
            if (SUCCEEDED(ITypeInfo_GetFuncDesc(ti, i, &fd)) && fd) {
                if (fd->memid == DISPID_NEWENUM) {
                    found = 1;
                    CHECK(fd->invkind == INVOKE_PROPERTYGET);
                    CHECK(fd->wFuncFlags & FUNCFLAG_FRESTRICTED);
                }
                ITypeInfo_ReleaseFuncDesc(ti, fd);
            }
        }
        if (ta) ITypeInfo_ReleaseTypeAttr(ti, ta);
        ITypeInfo_Release(ti);
    }
    CHECK(found);
    IDispatch_Release(disp);
}
TMTEST(dxtm_dual_newenum_member_attributes) { RUN_DUAL(cl_enum_prop_only, NULL); }
