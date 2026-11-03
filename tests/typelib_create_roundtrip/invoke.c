/* ITypeInfo::Invoke / GetIDsOfNames / CreateInstance / AddressOfMember and CreateStdDispatch over a type
 * library built with CreateTypeLib2. The type library is SYS_WIN64 so that the stored vtable offsets match
 * this process; the COM object is a hand-written C vtable.
 * CreateStdDispatch argument checks verified against the Windows 10 oleaut32 decompilation (E_INVALIDARG for a
 * NULL ITypeInfo or NULL this pointer). */
#include "tlc.h"

typedef struct IMathVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(void *);
    ULONG (STDMETHODCALLTYPE *Release)(void *);
    HRESULT (STDMETHODCALLTYPE *Add)(void *, LONG, LONG, LONG *);
    HRESULT (STDMETHODCALLTYPE *Concat)(void *, BSTR, BSTR, BSTR *);
    HRESULT (STDMETHODCALLTYPE *get_Value)(void *, LONG *);
    HRESULT (STDMETHODCALLTYPE *put_Value)(void *, LONG);
    HRESULT (STDMETHODCALLTYPE *Scale)(void *, double, double *);
    HRESULT (STDMETHODCALLTYPE *Fail)(void *);
    HRESULT (STDMETHODCALLTYPE *Opt)(void *, VARIANT, LONG *);
    HRESULT (STDMETHODCALLTYPE *Dflt)(void *, LONG, LONG *);
    HRESULT (STDMETHODCALLTYPE *Mix)(void *, SHORT, BSTR, double, LONG *);
} IMathVtbl;
typedef struct Math { const IMathVtbl *vt; LONG ref; LONG value; LONG last_opt_vt; SCODE last_opt_scode; } Math;

static HRESULT STDMETHODCALLTYPE m_qi(void *p, REFIID r, void **o) { *o = p; return S_OK; }
static ULONG STDMETHODCALLTYPE m_addref(void *p) { return ++((Math *)p)->ref; }
static ULONG STDMETHODCALLTYPE m_release(void *p) { return --((Math *)p)->ref; }
static HRESULT STDMETHODCALLTYPE m_add(void *p, LONG a, LONG b, LONG *r) { *r = a + b; return S_OK; }
static HRESULT STDMETHODCALLTYPE m_concat(void *p, BSTR a, BSTR b, BSTR *r)
{
    UINT la = a ? pSysStringLen(a) : 0, lb = b ? pSysStringLen(b) : 0;
    BSTR n = pSysAllocStringLen(NULL, la + lb);
    if (la) memcpy(n, a, la * 2);
    if (lb) memcpy(n + la, b, lb * 2);
    *r = n;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE m_get(void *p, LONG *r) { *r = ((Math *)p)->value; return S_OK; }
static HRESULT STDMETHODCALLTYPE m_put(void *p, LONG v) { ((Math *)p)->value = v; return S_OK; }
static HRESULT STDMETHODCALLTYPE m_scale(void *p, double d, double *r) { *r = d * 2.0; return S_OK; }
static HRESULT STDMETHODCALLTYPE m_fail(void *p) { return E_FAIL; }
static HRESULT STDMETHODCALLTYPE m_opt(void *p, VARIANT v, LONG *r)
{
    Math *m = p;
    m->last_opt_vt = V_VT(&v);
    m->last_opt_scode = V_VT(&v) == VT_ERROR ? V_ERROR(&v) : 0;
    *r = V_VT(&v);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE m_dflt(void *p, LONG a, LONG *r) { *r = a; return S_OK; }
static HRESULT STDMETHODCALLTYPE m_mix(void *p, SHORT a, BSTR s, double d, LONG *r)
{
    *r = a * 1000 + (s ? (LONG)pSysStringLen(s) * 100 : 0) + (LONG)d;
    return S_OK;
}
static const IMathVtbl math_vt = { m_qi, m_addref, m_release, m_add, m_concat, m_get, m_put, m_scale, m_fail, m_opt, m_dflt, m_mix };

static TYPEDESC td_i4 = { .vt = VT_I4 }, td_r8 = { .vt = VT_R8 }, td_bstr = { .vt = VT_BSTR };

static void pel(ELEMDESC *e, VARTYPE vt, WORD flags) { memset(e, 0, sizeof *e); e->tdesc.vt = vt; e->paramdesc.wParamFlags = flags; }
static void pel_ptr(ELEMDESC *e, TYPEDESC *to, WORD flags) { memset(e, 0, sizeof *e); e->tdesc.vt = VT_PTR; e->tdesc.lptdesc = to; e->paramdesc.wParamFlags = flags; }

static HRESULT afd(ICreateTypeInfo *c, UINT idx, INVOKEKIND ik, MEMBERID id, ELEMDESC *el, int n, int nopt, LPOLESTR *names, UINT nnames)
{
    FUNCDESC fd; memset(&fd, 0, sizeof fd);
    fd.memid = id; fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = ik; fd.callconv = CC_STDCALL;
    fd.elemdescFunc.tdesc.vt = VT_HRESULT; fd.cParams = n; fd.cParamsOpt = nopt; fd.lprgelemdescParam = el;
    HRESULT hr = ICreateTypeInfo_AddFuncDesc(c, idx, &fd);
    if (hr == S_OK && names) hr = ICreateTypeInfo_SetFuncAndParamNames(c, idx, names, nnames);
    return hr;
}

/* Builds IMath (SYS_WIN64, derives from IUnknown) and reloads it. The caller releases the returned ITypeLib. */
static ITypeLib *build_imath(WCHAR *path, const char *leaf, ITypeInfo **out_ti)
{
    ITypeLib *so = tl_stdole();
    if (!so) return NULL;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, leaf, path);
    ITypeLib *tl = NULL;
    if (unk && ctl) {
        ICreateTypeInfo *c = tl_cti(ctl, L"IMath", TKIND_INTERFACE);
        HREFTYPE h;
        OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_B));
        OK_(ICreateTypeInfo_AddRefTypeInfo(c, unk, &h));
        OK_(ICreateTypeInfo_AddImplType(c, 0, h));
        ELEMDESC el[5];
        LPOLESTR n[5];
        pel(&el[0], VT_I4, PARAMFLAG_FIN); pel(&el[1], VT_I4, PARAMFLAG_FIN); pel_ptr(&el[2], &td_i4, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Add"; n[1] = (LPOLESTR)L"a"; n[2] = (LPOLESTR)L"b"; n[3] = (LPOLESTR)L"result";
        OK_(afd(c, 0, INVOKE_FUNC, 1, el, 3, 0, n, 4));
        pel(&el[0], VT_BSTR, PARAMFLAG_FIN); pel(&el[1], VT_BSTR, PARAMFLAG_FIN); pel_ptr(&el[2], &td_bstr, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Concat"; n[1] = (LPOLESTR)L"left"; n[2] = (LPOLESTR)L"right";
        OK_(afd(c, 1, INVOKE_FUNC, 2, el, 3, 0, n, 3));
        pel_ptr(&el[0], &td_i4, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Value";
        OK_(afd(c, 2, INVOKE_PROPERTYGET, 3, el, 1, 0, n, 1));
        pel(&el[0], VT_I4, PARAMFLAG_FIN);
        OK_(afd(c, 3, INVOKE_PROPERTYPUT, 3, el, 1, 0, n, 1));
        pel(&el[0], VT_R8, PARAMFLAG_FIN); pel_ptr(&el[1], &td_r8, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Scale";
        OK_(afd(c, 4, INVOKE_FUNC, 4, el, 2, 0, n, 1));
        n[0] = (LPOLESTR)L"Fail";
        OK_(afd(c, 5, INVOKE_FUNC, 5, NULL, 0, 0, n, 1));
        pel(&el[0], VT_VARIANT, PARAMFLAG_FIN | PARAMFLAG_FOPT); pel_ptr(&el[1], &td_i4, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Opt";
        OK_(afd(c, 6, INVOKE_FUNC, 6, el, 2, 1, n, 1));
        PARAMDESCEX pe; memset(&pe, 0, sizeof pe); pe.cBytes = sizeof pe; V_VT(&pe.varDefaultValue) = VT_I4; V_I4(&pe.varDefaultValue) = 7;
        pel(&el[0], VT_I4, PARAMFLAG_FIN | PARAMFLAG_FOPT | PARAMFLAG_FHASDEFAULT); el[0].paramdesc.pparamdescex = &pe;
        pel_ptr(&el[1], &td_i4, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Dflt";
        OK_(afd(c, 7, INVOKE_FUNC, 7, el, 2, 1, n, 1));
        pel(&el[0], VT_I2, PARAMFLAG_FIN); pel(&el[1], VT_BSTR, PARAMFLAG_FIN); pel(&el[2], VT_R8, PARAMFLAG_FIN);
        pel_ptr(&el[3], &td_i4, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL);
        n[0] = (LPOLESTR)L"Mix"; n[1] = (LPOLESTR)L"a"; n[2] = (LPOLESTR)L"s"; n[3] = (LPOLESTR)L"d";
        OK_(afd(c, 8, INVOKE_FUNC, 8, el, 4, 0, n, 4));
        ICreateTypeInfo_Release(c);
        tl = tl_reload(ctl, path);
    }
    if (unk) ITypeInfo_Release(unk);
    ITypeLib_Release(so);
    if (tl && out_ti) *out_ti = tl_ti(tl, 0);
    return tl;
}

static void init_math(Math *m) { memset(m, 0, sizeof *m); m->vt = &math_vt; m->ref = 1; }

TEST(tli_imath_layout)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_layout.tlb", &ti);
    if (!tl || !ti) return;
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) {
        EQ(ta->cFuncs, 9);
        EQ(ta->cbSizeVft, 12 * 8);
        EQ(ta->cImplTypes, 1);
        CHECK(IsEqualGUID(&ta->guid, &TLC_GUID_B));
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    for (UINT i = 0; i < 9; i++) {
        FUNCDESC *fd = NULL;
        OK_(ITypeInfo_GetFuncDesc(ti, i, &fd));
        if (fd) { CHECKF(fd->oVft == (3 + i) * 8, "func %u oVft %d", i, fd->oVft); EQ(fd->elemdescFunc.tdesc.vt, VT_HRESULT); ITypeInfo_ReleaseFuncDesc(ti, fd); }
    }
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_getidsofnames)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_ids.tlb", &ti);
    if (!tl || !ti) return;
    MEMBERID id[4] = { 0 };
    LPOLESTR nm[4] = { (LPOLESTR)L"Add" };
    OK_(ITypeInfo_GetIDsOfNames(ti, nm, 1, id));
    EQ(id[0], 1);
    nm[0] = (LPOLESTR)L"add";
    OK_(ITypeInfo_GetIDsOfNames(ti, nm, 1, id));
    EQ(id[0], 1);
    /* parameter ids are their positions */
    LPOLESTR mix[4] = { (LPOLESTR)L"Mix", (LPOLESTR)L"d", (LPOLESTR)L"a", (LPOLESTR)L"s" };
    OK_(ITypeInfo_GetIDsOfNames(ti, mix, 4, id));
    EQ(id[0], 8); EQ(id[1], 2); EQ(id[2], 0); EQ(id[3], 1);
    LPOLESTR bad[2] = { (LPOLESTR)L"Mix", (LPOLESTR)L"nosuchparam" };
    HRESULT hr = ITypeInfo_GetIDsOfNames(ti, bad, 2, id);
    EQHR(hr, DISP_E_UNKNOWNNAME);
    EQ(id[0], 8);
    EQ(id[1], (MEMBERID)DISPID_UNKNOWN);
    nm[0] = (LPOLESTR)L"Nope";
    EQHR(ITypeInfo_GetIDsOfNames(ti, nm, 1, id), DISP_E_UNKNOWNNAME);
    EQ(id[0], (MEMBERID)DISPID_UNKNOWN);
    /* property name maps to the shared memid */
    nm[0] = (LPOLESTR)L"Value";
    OK_(ITypeInfo_GetIDsOfNames(ti, nm, 1, id));
    EQ(id[0], 3);
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

#define INVOKE(ti, obj, id, flags, dp, res, ex, perr) ITypeInfo_Invoke(ti, obj, id, flags, dp, res, ex, perr)

TEST(tli_invoke_methods)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_invoke.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    VARIANT a[4], res; EXCEPINFO ex; UINT err; DISPPARAMS dp;
    for (int i = 0; i < 4; i++) pVariantInit(&a[i]);
    /* Add(a, b) -> long ; rgvarg is in reverse order */
    V_VT(&a[0]) = VT_I4; V_I4(&a[0]) = 32;
    V_VT(&a[1]) = VT_I4; V_I4(&a[1]) = 10;
    dp.rgvarg = a; dp.rgdispidNamedArgs = NULL; dp.cArgs = 2; dp.cNamedArgs = 0;
    pVariantInit(&res); memset(&ex, 0, sizeof ex); err = 99;
    OK_(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 42);
    /* the flag may include DISPATCH_PROPERTYGET too */
    pVariantInit(&res);
    OK_(INVOKE(ti, &m, 1, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, &res, &ex, &err));
    EQ(V_I4(&res), 42);
    /* result pointer may be NULL */
    OK_(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, NULL, &ex, &err));
    /* coercion: VT_I2 and VT_R8 / VT_BSTR arguments to long parameters */
    V_VT(&a[0]) = VT_I2; V_I2(&a[0]) = 5;
    V_VT(&a[1]) = VT_R8; V_R8(&a[1]) = 7.0;
    pVariantInit(&res);
    OK_(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(V_I4(&res), 12);
    V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = pSysAllocString(L"30");
    V_VT(&a[1]) = VT_BOOL; V_BOOL(&a[1]) = VARIANT_TRUE;
    pVariantInit(&res);
    HRESULT hr = INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err);
    t_note("Add(VT_BOOL true, \"30\") hr=0x%08lx result=%ld", (unsigned long)hr, (long)V_I4(&res));
    if (hr == S_OK) EQ(V_I4(&res), 29);
    pVariantClear(&a[0]);
    pVariantInit(&a[0]); pVariantInit(&a[1]);
    /* BSTR concat */
    V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = pSysAllocString(L"world");
    V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = pSysAllocString(L"hello ");
    pVariantInit(&res);
    OK_(INVOKE(ti, &m, 2, DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(V_VT(&res), VT_BSTR);
    CHECK(V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"hello world"));
    pVariantClear(&res); pVariantClear(&a[0]); pVariantClear(&a[1]);
    /* double */
    pVariantInit(&a[0]); V_VT(&a[0]) = VT_R8; V_R8(&a[0]) = 21.25;
    dp.cArgs = 1;
    OK_(INVOKE(ti, &m, 4, DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(V_VT(&res), VT_R8);
    CHECK(V_R8(&res) == 42.5);
    /* callee failure propagates */
    dp.cArgs = 0; dp.rgvarg = NULL;
    pVariantInit(&res);
    HRESULT fhr = INVOKE(ti, &m, 5, DISPATCH_METHOD, &dp, &res, &ex, &err);
    t_note("Invoke of a method returning E_FAIL: hr=0x%08lx", (unsigned long)fhr);
    CHECK(FAILED(fhr));
    EQ(V_VT(&res), VT_EMPTY);
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_invoke_properties)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_prop.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m); m.value = 1234;
    VARIANT a, res; EXCEPINFO ex; UINT err; DISPPARAMS dp = { NULL, NULL, 0, 0 };
    pVariantInit(&res); memset(&ex, 0, sizeof ex);
    OK_(INVOKE(ti, &m, 3, DISPATCH_PROPERTYGET, &dp, &res, &ex, &err));
    EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 1234);
    DISPID named = DISPID_PROPERTYPUT;
    pVariantInit(&a); V_VT(&a) = VT_I4; V_I4(&a) = 99;
    dp.rgvarg = &a; dp.cArgs = 1; dp.rgdispidNamedArgs = &named; dp.cNamedArgs = 1;
    pVariantInit(&res);
    OK_(INVOKE(ti, &m, 3, DISPATCH_PROPERTYPUT, &dp, NULL, &ex, &err));
    EQ(m.value, 99);
    /* DISPATCH_METHOD on a property-only member */
    DISPPARAMS none = { NULL, NULL, 0, 0 };
    HRESULT hr = INVOKE(ti, &m, 3, DISPATCH_METHOD, &none, &res, &ex, &err);
    t_note("DISPATCH_METHOD on property get hr=0x%08lx", (unsigned long)hr);
    /* PUT on a get-only / method id */
    hr = INVOKE(ti, &m, 1, DISPATCH_PROPERTYPUT, &dp, NULL, &ex, &err);
    t_note("DISPATCH_PROPERTYPUT on method hr=0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_invoke_errors)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_err.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    VARIANT a[3], res; EXCEPINFO ex; UINT err; DISPPARAMS dp;
    for (int i = 0; i < 3; i++) pVariantInit(&a[i]);
    dp.rgvarg = a; dp.rgdispidNamedArgs = NULL; dp.cNamedArgs = 0;
    pVariantInit(&res); memset(&ex, 0, sizeof ex);
    /* too few / too many arguments */
    dp.cArgs = 1; V_VT(&a[0]) = VT_I4;
    err = 77;
    EQHR(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err), DISP_E_BADPARAMCOUNT);
    dp.cArgs = 3; V_VT(&a[1]) = VT_I4; V_VT(&a[2]) = VT_I4;
    EQHR(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err), DISP_E_BADPARAMCOUNT);
    dp.cArgs = 0;
    EQHR(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err), DISP_E_BADPARAMCOUNT);
    /* type mismatch reports the offending argument index */
    dp.cArgs = 2;
    V_VT(&a[0]) = VT_I4; V_I4(&a[0]) = 1;
    V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = pSysAllocString(L"not a number");
    err = 77;
    EQHR(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err), DISP_E_TYPEMISMATCH);
    SOFTEQ(err, 1);
    pVariantClear(&a[1]);
    V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = pSysAllocString(L"x");
    V_VT(&a[1]) = VT_I4; V_I4(&a[1]) = 1;
    err = 77;
    EQHR(INVOKE(ti, &m, 1, DISPATCH_METHOD, &dp, &res, &ex, &err), DISP_E_TYPEMISMATCH);
    SOFTEQ(err, 0);
    pVariantClear(&a[0]);
    /* unknown member */
    dp.cArgs = 0;
    HRESULT hr = INVOKE(ti, &m, 0x7777, DISPATCH_METHOD, &dp, &res, &ex, &err);
    t_note("Invoke unknown memid hr=0x%08lx", (unsigned long)hr);
    CHECK(hr == DISP_E_MEMBERNOTFOUND || hr == TYPE_E_ELEMENTNOTFOUND);
    /* NULL DISPPARAMS */
    hr = INVOKE(ti, &m, 5, DISPATCH_METHOD, NULL, &res, &ex, &err);
    t_note("Invoke with NULL DISPPARAMS hr=0x%08lx", (unsigned long)hr);
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_invoke_named_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_named.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    /* Mix(a: short, s: bstr, d: double) = a*1000 + len(s)*100 + (long)d */
    MEMBERID ids[4];
    LPOLESTR nm[4] = { (LPOLESTR)L"Mix", (LPOLESTR)L"d", (LPOLESTR)L"a", (LPOLESTR)L"s" };
    OK_(ITypeInfo_GetIDsOfNames(ti, nm, 4, ids));
    VARIANT a[3], res; EXCEPINFO ex; UINT err; DISPPARAMS dp;
    for (int i = 0; i < 3; i++) pVariantInit(&a[i]);
    /* all three named; rgvarg[i] corresponds to rgdispidNamedArgs[i] */
    V_VT(&a[0]) = VT_R8; V_R8(&a[0]) = 9;
    V_VT(&a[1]) = VT_I2; V_I2(&a[1]) = 3;
    V_VT(&a[2]) = VT_BSTR; V_BSTR(&a[2]) = pSysAllocString(L"ab");
    DISPID named[3] = { ids[1], ids[2], ids[3] };
    dp.rgvarg = a; dp.rgdispidNamedArgs = named; dp.cArgs = 3; dp.cNamedArgs = 3;
    pVariantInit(&res); memset(&ex, 0, sizeof ex);
    OK_(INVOKE(ti, &m, ids[0], DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(V_I4(&res), 3 * 1000 + 2 * 100 + 9);
    /* one positional (last = first in rgvarg order) and two named */
    pVariantClear(&a[2]);
    V_VT(&a[0]) = VT_I2; V_I2(&a[0]) = 4;       /* named: a */
    V_VT(&a[1]) = VT_R8; V_R8(&a[1]) = 1;       /* named: d */
    V_VT(&a[2]) = VT_BSTR; V_BSTR(&a[2]) = pSysAllocString(L"xyz"); /* positional: s (first param positional) */
    DISPID named2[2] = { ids[2], ids[1] };
    dp.cNamedArgs = 2;
    dp.rgdispidNamedArgs = named2;
    pVariantInit(&res);
    HRESULT hr = INVOKE(ti, &m, ids[0], DISPATCH_METHOD, &dp, &res, &ex, &err);
    t_note("mixed positional + named hr=0x%08lx res=%ld", (unsigned long)hr, (long)V_I4(&res));
    pVariantClear(&a[2]);
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_invoke_optional_and_default)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_opt.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    VARIANT a, res; EXCEPINFO ex; UINT err; DISPPARAMS dp = { NULL, NULL, 0, 0 };
    memset(&ex, 0, sizeof ex);
    /* omitted optional VARIANT parameter reaches the callee as VT_ERROR / DISP_E_PARAMNOTFOUND */
    pVariantInit(&res);
    HRESULT hr = INVOKE(ti, &m, 6, DISPATCH_METHOD, &dp, &res, &ex, &err);
    SOFTHR(hr, S_OK);
    if (hr == S_OK) {
        EQ(m.last_opt_vt, VT_ERROR);
        SOFTEQ(m.last_opt_scode, DISP_E_PARAMNOTFOUND);
    }
    pVariantInit(&a); V_VT(&a) = VT_I4; V_I4(&a) = 5;
    dp.rgvarg = &a; dp.cArgs = 1;
    OK_(INVOKE(ti, &m, 6, DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(m.last_opt_vt, VT_I4);
    /* a parameter with a default value */
    dp.cArgs = 0; dp.rgvarg = NULL;
    pVariantInit(&res);
    hr = INVOKE(ti, &m, 7, DISPATCH_METHOD, &dp, &res, &ex, &err);
    t_note("omitted arg with default hr=0x%08lx result=%ld", (unsigned long)hr, (long)V_I4(&res));
    if (hr == S_OK) SOFTEQ(V_I4(&res), 7);
    dp.cArgs = 1; dp.rgvarg = &a;
    OK_(INVOKE(ti, &m, 7, DISPATCH_METHOD, &dp, &res, &ex, &err));
    EQ(V_I4(&res), 5);
    /* opt flags round-trip in the func desc */
    FUNCDESC *fd = NULL;
    OK_(ITypeInfo_GetFuncDesc(ti, 6, &fd));
    if (fd) {
        EQ(fd->cParamsOpt, 1);
        if (fd->lprgelemdescParam) EQ(fd->lprgelemdescParam[0].paramdesc.wParamFlags, PARAMFLAG_FIN | PARAMFLAG_FOPT);
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    OK_(ITypeInfo_GetFuncDesc(ti, 7, &fd));
    if (fd) {
        EQ(fd->cParamsOpt, 1);
        if (fd->lprgelemdescParam) {
            EQ(fd->lprgelemdescParam[0].paramdesc.wParamFlags, PARAMFLAG_FIN | PARAMFLAG_FOPT | PARAMFLAG_FHASDEFAULT);
            CHECK(fd->lprgelemdescParam[0].paramdesc.pparamdescex != NULL);
            if (fd->lprgelemdescParam[0].paramdesc.pparamdescex) EQ(V_I4(&fd->lprgelemdescParam[0].paramdesc.pparamdescex->varDefaultValue), 7);
        }
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_createinstance_noncoclass)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_ci.tlb", &ti);
    if (!tl || !ti) return;
    pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    void *p = (void *)1;
    HRESULT hr = ITypeInfo_CreateInstance(ti, NULL, &IID_IUnknown, &p);
    t_note("CreateInstance on an interface typeinfo hr=0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    SOFTHR(hr, TYPE_E_BADMODULEKIND);
    CHECK(p == NULL);
    pCoUninitialize();
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_createinstance_unregistered_coclass)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "ci_coclass.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"CoNothing", TKIND_COCLASS);
    OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_C)); /* never registered */
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (ti) {
        void *p = (void *)1;
        HRESULT hr = ITypeInfo_CreateInstance(ti, NULL, &IID_IUnknown, &p);
        t_note("CreateInstance of unregistered coclass hr=0x%08lx", (unsigned long)hr);
        SOFTHR(hr, REGDB_E_CLASSNOTREG);
        CHECK(FAILED(hr));
        CHECK(p == NULL);
        ITypeInfo_Release(ti);
    }
    pCoUninitialize();
    ITypeLib_Release(tl);
}

TEST(tli_stddispatch_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_sd0.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    IUnknown *out = (IUnknown *)(ULONG_PTR)1;
    /* decomp: CreateStdDispatch rejects a NULL ITypeInfo or a NULL object pointer */
    EQHR(pCreateStdDispatch(NULL, &m, NULL, &out), E_INVALIDARG);
    EQHR(pCreateStdDispatch(NULL, NULL, ti, &out), E_INVALIDARG);
    CHECK(out == (IUnknown *)(ULONG_PTR)1);
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_stddispatch_dispatch_over_vtable)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_sd.tlb", &ti);
    if (!tl || !ti) return;
    pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    Math m; init_math(&m); m.value = 5;
    IUnknown *unk = NULL;
    OK_(pCreateStdDispatch(NULL, &m, ti, &unk));
    CHECK(unk != NULL);
    if (unk) {
        IDispatch *disp = NULL;
        OK_(IUnknown_QueryInterface(unk, &IID_IDispatch, (void **)&disp));
        CHECK(disp != NULL);
        void *other = (void *)1;
        EQHR(IUnknown_QueryInterface(unk, &IID_ITypeInfo, &other), E_NOINTERFACE);
        CHECK(other == NULL);
        if (disp) {
            UINT n = 99;
            OK_(IDispatch_GetTypeInfoCount(disp, &n));
            EQ(n, 1);
            ITypeInfo *got = NULL;
            OK_(IDispatch_GetTypeInfo(disp, 0, 0x409, &got));
            CHECK(got == ti);
            if (got) ITypeInfo_Release(got);
            got = NULL;
            HRESULT hr = IDispatch_GetTypeInfo(disp, 1, 0x409, &got);
            t_note("GetTypeInfo(1) hr=0x%08lx", (unsigned long)hr);
            CHECK(FAILED(hr));
            DISPID id = 0;
            LPOLESTR nm = (LPOLESTR)L"Add";
            OK_(IDispatch_GetIDsOfNames(disp, &IID_NULL, &nm, 1, 0x409, &id));
            EQ(id, 1);
            nm = (LPOLESTR)L"NoSuchMethod";
            EQHR(IDispatch_GetIDsOfNames(disp, &IID_NULL, &nm, 1, 0x409, &id), DISP_E_UNKNOWNNAME);
            VARIANT a[2], res; EXCEPINFO ex; UINT err; DISPPARAMS dp;
            pVariantInit(&a[0]); pVariantInit(&a[1]); pVariantInit(&res); memset(&ex, 0, sizeof ex);
            V_VT(&a[0]) = VT_I4; V_I4(&a[0]) = 20;
            V_VT(&a[1]) = VT_I4; V_I4(&a[1]) = 22;
            dp.rgvarg = a; dp.cArgs = 2; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL;
            OK_(IDispatch_Invoke(disp, 1, &IID_NULL, 0x409, DISPATCH_METHOD, &dp, &res, &ex, &err));
            EQ(V_VT(&res), VT_I4); EQ(V_I4(&res), 42);
            /* property get through the dispatch */
            dp.cArgs = 0; pVariantInit(&res);
            OK_(IDispatch_Invoke(disp, 3, &IID_NULL, 0x409, DISPATCH_PROPERTYGET, &dp, &res, &ex, &err));
            EQ(V_I4(&res), 5);
            /* non-NULL riid must be IID_NULL */
            HRESULT hr2 = IDispatch_Invoke(disp, 3, &IID_IUnknown, 0x409, DISPATCH_PROPERTYGET, &dp, &res, &ex, &err);
            t_note("Invoke with riid != IID_NULL hr=0x%08lx", (unsigned long)hr2);
            SOFTHR(hr2, DISP_E_UNKNOWNINTERFACE);
            EQHR(IDispatch_Invoke(disp, 0x7777, &IID_NULL, 0x409, DISPATCH_METHOD, &dp, &res, &ex, &err), DISP_E_MEMBERNOTFOUND);
            IDispatch_Release(disp);
        }
        EQ(IUnknown_Release(unk), 0);
    }
    pCoUninitialize();
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_stddispatch_refcount)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_sdref.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    IUnknown *unk = NULL;
    OK_(pCreateStdDispatch(NULL, &m, ti, &unk));
    if (unk) {
        EQ(IUnknown_AddRef(unk), 2);
        EQ(IUnknown_Release(unk), 1);
        IDispatch *d1 = NULL, *d2 = NULL;
        OK_(IUnknown_QueryInterface(unk, &IID_IDispatch, (void **)&d1));
        OK_(IUnknown_QueryInterface(unk, &IID_IUnknown, (void **)&d2));
        CHECK(d1 != NULL && d2 != NULL);
        if (d1) IDispatch_Release(d1);
        if (d2) IUnknown_Release(d2);
        EQ(IUnknown_Release(unk), 0);
    }
    /* the std dispatch kept its own reference on the typeinfo: ours is still valid */
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) ITypeInfo_ReleaseTypeAttr(ti, ta);
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}

TEST(tli_stddispatch_outer_unknown)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeInfo *ti = NULL;
    ITypeLib *tl = build_imath(path, "imath_sdouter.tlb", &ti);
    if (!tl || !ti) return;
    Math m; init_math(&m);
    /* With an outer unknown, QueryInterface/AddRef/Release of the dispatch are forwarded to it. */
    Math outer; init_math(&outer);
    IUnknown *unk = NULL;
    OK_(pCreateStdDispatch((IUnknown *)&outer, &m, ti, &unk));
    if (unk) {
        IDispatch *d = NULL;
        HRESULT hr = IUnknown_QueryInterface(unk, &IID_IDispatch, (void **)&d);
        t_note("QI(IDispatch) on inner unknown with outer hr=0x%08lx", (unsigned long)hr);
        if (d) IDispatch_Release(d);
        LONG before = outer.ref;
        IUnknown_AddRef(unk);
        t_note("outer refcount before=%ld after AddRef on inner=%ld", (long)before, (long)outer.ref);
        IUnknown_Release(unk);
        IUnknown_Release(unk);
    }
    ITypeInfo_Release(ti); ITypeLib_Release(tl);
}
