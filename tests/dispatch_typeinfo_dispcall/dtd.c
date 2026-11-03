/* dispatch_typeinfo_dispcall: shared demo object, mock dispatch and interface data. */
#include "dtd.h"

#define GD(n, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) const GUID n = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
GD(DI_IUnknown,  0x00000000, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
GD(DI_IDispatch, 0x00020400, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
GD(DI_ITypeInfo, 0x00020401, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
GD(DI_ITypeInfo2, 0x00020412, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
GD(DI_IMarshal,  0x00000003, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
GD(DI_IClassFactory, 0x00000001, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
GD(DI_IErrorInfo, 0x1CF2B120, 0x547D, 0x101B, 0x8E, 0x65, 0x08, 0x00, 0x2B, 0x2B, 0xD1, 0x19);
GD(DI_ICreateErrorInfo, 0x22F03340, 0x547D, 0x101B, 0x8E, 0x65, 0x08, 0x00, 0x2B, 0x2B, 0xD1, 0x19);
GD(DI_ISupportErrorInfo, 0xDF0B3D60, 0x548F, 0x101B, 0x8E, 0x65, 0x08, 0x00, 0x2B, 0x2B, 0xD1, 0x19);
GD(DI_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

BSTR bs_alloc(const WCHAR *s) { return OA(SysAllocString) ? OA(SysAllocString)(s) : NULL; }
BSTR bs_allocn(const WCHAR *s, UINT n) { return OA(SysAllocStringLen) ? OA(SysAllocStringLen)(s, n) : NULL; }
void bs_free(BSTR b) { if (OA(SysFreeString)) OA(SysFreeString)(b); }
UINT bs_len(BSTR b) { return OA(SysStringLen) ? OA(SysStringLen)(b) : 0; }
void vinit(VARIANT *v) { memset(v, 0, sizeof *v); }
HRESULT vclear(VARIANT *v) { return OA(VariantClear)(v); }
HRESULT vchange(VARIANT *dst, VARIANT *src, VARTYPE vt) { return OA(VariantChangeTypeEx)(dst, src, 0x409, 0, vt); }
int bs_eq(BSTR b, const WCHAR *s) { if (!b || !s) return b == (BSTR)s; return !lstrcmpW(b, s); }

static HRESULT WINAPI Demo_QI(Demo *d, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &DI_IUnknown)) { *ppv = d; InterlockedIncrement(&d->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI Demo_AddRef(Demo *d) { return InterlockedIncrement(&d->ref); }
static ULONG WINAPI Demo_Release(Demo *d) { return InterlockedDecrement(&d->ref); }
static LONG WINAPI Demo_Add(Demo *d, LONG a, LONG b) { d->calls++; return a + b; }
static LONG WINAPI Demo_Sub(Demo *d, LONG a, LONG b) { d->calls++; return a - b; }
static double WINAPI Demo_Scale(Demo *d, double x, LONG n) { d->calls++; return x * n; }
static BSTR WINAPI Demo_Concat(Demo *d, BSTR a, BSTR b)
{
    UINT la = bs_len(a), lb = bs_len(b);
    BSTR r = bs_allocn(NULL, la + lb);
    d->calls++;
    if (!r) return NULL;
    memcpy(r, a, la * sizeof(WCHAR)); memcpy(r + la, b, lb * sizeof(WCHAR));
    return r;
}
static void WINAPI Demo_SetValue(Demo *d, LONG v) { d->calls++; d->value = v; }
static LONG WINAPI Demo_GetValue(Demo *d) { d->calls++; return d->value; }
static LONG WINAPI Demo_VarVt(Demo *d, VARIANT v) { d->calls++; return (LONG)V_VT(&v) | ((V_VT(&v) == VT_I4) ? (V_I4(&v) << 16) : 0); }
static LONG WINAPI Demo_Deref(Demo *d, LONG *p) { LONG old = *p; d->calls++; *p = old + 1000; return old; }
static LONG WINAPI Demo_Pack(Demo *d, BYTE b, SHORT s, VARIANT_BOOL f) { d->calls++; return (LONG)b + (LONG)s * 1000 + (f ? 1000000 : 0); }
static void WINAPI Demo_SetObj(Demo *d, IUnknown *o) { d->calls++; d->obj = o; }
static LONGLONG WINAPI Demo_Mul64(Demo *d, LONGLONG a, LONGLONG b) { d->calls++; return a * b; }
static float WINAPI Demo_Half(Demo *d, float f) { d->calls++; return f / 2.0f; }
static IDispatch *WINAPI Demo_Child(Demo *d) { d->calls++; if (d->child) IDispatch_AddRef(d->child); return d->child; }
static LONG WINAPI Demo_Item(Demo *d, LONG i) { d->calls++; return i * 11; }
static LONG WINAPI Demo_Count(Demo *d) { d->calls++; return 42; }

const void *const Demo_vtbl[DV_NSLOTS] = {
    Demo_QI, Demo_AddRef, Demo_Release, Demo_Add, Demo_Sub, Demo_Scale, Demo_Concat, Demo_SetValue, Demo_GetValue,
    Demo_VarVt, Demo_Deref, Demo_Pack, Demo_SetObj, Demo_Mul64, Demo_Half, Demo_Child, Demo_Item, Demo_Count
};
void Demo_init(Demo *d) { memset(d, 0, sizeof *d); d->vtbl = Demo_vtbl; d->ref = 1; }

/* ---- Mock IDispatch ---- */
static HRESULT WINAPI Mock_QI(IDispatch *i, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &DI_IUnknown) || IsEqualIID(riid, &DI_IDispatch)) { *ppv = i; IDispatch_AddRef(i); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI Mock_AddRef(IDispatch *i) { return InterlockedIncrement(&((Mock *)i)->ref); }
static ULONG WINAPI Mock_Release(IDispatch *i) { return InterlockedDecrement(&((Mock *)i)->ref); }
static HRESULT WINAPI Mock_GTIC(IDispatch *i, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI Mock_GTI(IDispatch *i, UINT n, LCID l, ITypeInfo **p) { *p = NULL; return E_NOTIMPL; }
static HRESULT WINAPI Mock_GIDN(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI Mock_Invoke(IDispatch *i, DISPID id, REFIID riid, LCID lcid, WORD fl, DISPPARAMS *dp,
                                  VARIANT *res, EXCEPINFO *ex, UINT *err)
{
    Mock *m = (Mock *)i;
    m->invoked++; m->last_dispid = id; m->last_flags = fl; m->last_cargs = dp->cArgs; m->last_cnamed = dp->cNamedArgs;
    m->last_lcid = lcid; m->last_riid = *riid;
    m->last_arg0_i4 = (dp->cArgs && V_VT(&dp->rgvarg[0]) == VT_I4) ? V_I4(&dp->rgvarg[0]) : -1;
    if (res) { vinit(res); V_VT(res) = VT_I4; V_I4(res) = 777; }
    return S_OK;
}
static const IDispatchVtbl Mock_vtbl = { Mock_QI, Mock_AddRef, Mock_Release, Mock_GTIC, Mock_GTI, Mock_GIDN, Mock_Invoke };
void Mock_init(Mock *m) { memset(m, 0, sizeof *m); m->lpVtbl = &Mock_vtbl; m->ref = 1; }

/* ---- interface data ---- */
#define S(x) ((OLECHAR *)L##x)
static PARAMDATA p_add[]  = { { S("a"), VT_I4 }, { S("b"), VT_I4 } };
static PARAMDATA p_sub[]  = { { S("a"), VT_I4 }, { S("b"), VT_I4 } };
static PARAMDATA p_scale[] = { { S("x"), VT_R8 }, { S("n"), VT_I4 } };
static PARAMDATA p_cat[]  = { { S("s1"), VT_BSTR }, { S("s2"), VT_BSTR } };
static PARAMDATA p_put[]  = { { S("v"), VT_I4 } };
static PARAMDATA p_var[]  = { { S("v"), VT_VARIANT } };
static PARAMDATA p_deref[] = { { S("p"), VT_BYREF | VT_I4 } };
static PARAMDATA p_pack[] = { { S("b"), VT_UI1 }, { S("s"), VT_I2 }, { S("f"), VT_BOOL } };
static PARAMDATA p_obj[]  = { { S("o"), VT_DISPATCH } };
static PARAMDATA p_mul[]  = { { S("a"), VT_I8 }, { S("b"), VT_I8 } };
static PARAMDATA p_half[] = { { S("f"), VT_R4 } };
static PARAMDATA p_item[] = { { S("idx"), VT_I4 } };

static METHODDATA g_md[] = {
    /* szName ppdata dispid iMeth cc cArgs wFlags vtReturn */
    { S("Add"),    p_add,   DD_ADD,   DV_ADD,      CC_STDCALL, 2, DISPATCH_METHOD,         VT_I4 },
    { S("Sub"),    p_sub,   DD_SUB,   DV_SUB,      CC_STDCALL, 2, DISPATCH_METHOD,         VT_I4 },
    { S("Scale"),  p_scale, DD_SCALE, DV_SCALE,    CC_STDCALL, 2, DISPATCH_METHOD,         VT_R8 },
    { S("Concat"), p_cat,   DD_CONCAT, DV_CONCAT,  CC_STDCALL, 2, DISPATCH_METHOD,         VT_BSTR },
    { S("Value"),  p_put,   DD_VALUE, DV_SETVALUE, CC_STDCALL, 1, DISPATCH_PROPERTYPUT,    VT_EMPTY },
    { S("Value"),  NULL,    DD_VALUE, DV_GETVALUE, CC_STDCALL, 0, DISPATCH_PROPERTYGET,    VT_I4 },
    { S("VarVt"),  p_var,   DD_VARVT, DV_VARVT,    CC_STDCALL, 1, DISPATCH_METHOD,         VT_I4 },
    { S("Deref"),  p_deref, DD_DEREF, DV_DEREF,    CC_STDCALL, 1, DISPATCH_METHOD,         VT_I4 },
    { S("Pack"),   p_pack,  DD_PACK,  DV_PACK,     CC_STDCALL, 3, DISPATCH_METHOD,         VT_I4 },
    { S("Obj"),    p_obj,   DD_OBJ,   DV_SETOBJ,   CC_STDCALL, 1, DISPATCH_PROPERTYPUTREF, VT_EMPTY },
    { S("Mul64"),  p_mul,   DD_MUL64, DV_MUL64,    CC_STDCALL, 2, DISPATCH_METHOD,         VT_I8 },
    { S("Half"),   p_half,  DD_HALF,  DV_HALF,     CC_STDCALL, 1, DISPATCH_METHOD,         VT_R4 },
    { S("Child"),  NULL,    DD_CHILD, DV_CHILD,    CC_STDCALL, 0, DISPATCH_PROPERTYGET,    VT_DISPATCH },
    { S("Item"),   p_item,  DD_ITEM,  DV_ITEM,     CC_STDCALL, 1, DISPATCH_PROPERTYGET,    VT_I4 },
    { S("Count"),  NULL,    DD_COUNT, DV_COUNT,    CC_STDCALL, 0, DISPATCH_PROPERTYGET | DISPATCH_METHOD, VT_I4 },
};
static INTERFACEDATA g_id = { g_md, sizeof(g_md) / sizeof(g_md[0]) };
const UINT demo_nmeth = sizeof(g_md) / sizeof(g_md[0]);
INTERFACEDATA *demo_idata(void) { return &g_id; }

ITypeInfo *demo_typeinfo(LCID lcid)
{
    ITypeInfo *ti = NULL;
    HRESULT hr;
    if (!OA(CreateDispTypeInfo)) return NULL;
    hr = OA(CreateDispTypeInfo)(&g_id, lcid, &ti);
    CHECKF(hr == S_OK, "CreateDispTypeInfo hr=%08lx", (unsigned long)hr);
    return hr == S_OK ? ti : NULL;
}

IDispatch *demo_stddisp(Demo *d, ITypeInfo *ti, IUnknown **unk_out)
{
    IUnknown *unk = NULL; IDispatch *disp = NULL;
    HRESULT hr = OA(CreateStdDispatch)(NULL, d, ti, &unk);
    CHECKF(hr == S_OK, "CreateStdDispatch hr=%08lx", (unsigned long)hr);
    if (hr != S_OK) return NULL;
    hr = IUnknown_QueryInterface(unk, &DI_IDispatch, (void **)&disp);
    CHECKF(hr == S_OK, "QI IDispatch hr=%08lx", (unsigned long)hr);
    if (unk_out) *unk_out = unk; else IUnknown_Release(unk);
    return disp;
}
