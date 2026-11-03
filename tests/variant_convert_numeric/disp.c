/* VarXFromDisp: the IDispatch default value (DISPID_VALUE, DISPATCH_PROPERTYGET) is fetched and then
 * coerced with VariantChangeTypeEx.  The decompiled functions are all of the form
 *   VariantChangeTypeEx(&out, &{VT_DISPATCH, pdisp}, lcid, 0, vt_dest)
 * so a failing Invoke surfaces as the HRESULT of VariantChangeTypeEx (DISP_E_TYPEMISMATCH, verified
 * by Wine's test which asserts it for native as well) and the output is not written.
 * adapted from Wine dlls/oleaut32/tests/vartype.c test_VarUI1FromDisp (LGPL-2.1+), generalised to all
 * numeric destinations and several default-value variants.
 */
#include "vcn.h"

static const GUID vcn_IID_IUnknown = {0x00000000, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID vcn_IID_IDispatch = {0x00020400, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID vcn_GUID_NULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
static HRESULT vclear(VARIANT *v) { return U(VariantClear)(v); }
static HRESULT vcopy(VARIANT *d, VARIANT *s) { return U(VariantCopy)(d, s); }

typedef struct {
    IDispatch iface;
    LONG ref;
    VARIANT ret;          /* what Invoke(DISPID_VALUE) returns */
    HRESULT fail;         /* if FAILED, Invoke returns this */
    int invokes;
    DISPID dispid;
    WORD flags;
    UINT cargs, cnamed;
    BOOL riid_null;
    BOOL ei_null, argerr_null, res_empty;
    LCID lcid;
} FakeDisp;

static FakeDisp *impl(IDispatch *i) { return (FakeDisp *)i; }

static HRESULT WINAPI fd_QI(IDispatch *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &vcn_IID_IUnknown) || IsEqualGUID(riid, &vcn_IID_IDispatch)) {
        *ppv = i;
        InterlockedIncrement(&impl(i)->ref);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI fd_AddRef(IDispatch *i) { return InterlockedIncrement(&impl(i)->ref); }
static ULONG WINAPI fd_Release(IDispatch *i) { return InterlockedDecrement(&impl(i)->ref); }
static HRESULT WINAPI fd_GetTypeInfoCount(IDispatch *i, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI fd_GetTypeInfo(IDispatch *i, UINT n, LCID l, ITypeInfo **p) { *p = NULL; return E_NOTIMPL; }
static HRESULT WINAPI fd_GetIDsOfNames(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI fd_Invoke(IDispatch *i, DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS *p,
                                VARIANT *res, EXCEPINFO *ei, UINT *argerr)
{
    FakeDisp *d = impl(i);
    d->invokes++;
    d->dispid = id;
    d->flags = flags;
    d->lcid = lcid;
    d->cargs = p ? p->cArgs : 99;
    d->cnamed = p ? p->cNamedArgs : 99;
    d->riid_null = riid && IsEqualGUID(riid, &vcn_GUID_NULL);
    d->ei_null = ei == NULL;
    d->argerr_null = argerr == NULL;
    d->res_empty = res && V_VT(res) == VT_EMPTY;
    if (FAILED(d->fail)) return d->fail;
    if (!res) return E_POINTER;
    return vcopy(res, &d->ret);
}
static const IDispatchVtbl fd_vtbl = { fd_QI, fd_AddRef, fd_Release, fd_GetTypeInfoCount, fd_GetTypeInfo,
                                       fd_GetIDsOfNames, fd_Invoke };

static void fd_init(FakeDisp *d)
{
    memset(d, 0, sizeof *d);
    d->iface.lpVtbl = (IDispatchVtbl *)&fd_vtbl;
    d->ref = 1;
    d->fail = S_OK;
    d->ret.vt = VT_EMPTY;
}

/* ---- destination descriptors; resolved by name through the module list */
enum kind { K_INT, K_UINT, K_R4, K_R8, K_CY, K_DATE, K_BOOL, K_DEC };
typedef HRESULT (WINAPI *disp_fn)(IDispatch *, LCID, void *);
static const struct dst {
    const char *fn; enum kind k; unsigned size; double lo, hi;   /* ints: inclusive range */
} dsts[] = {
    { "VarI1FromDisp",   K_INT,  1, -128.0, 127.0 },
    { "VarUI1FromDisp",  K_UINT, 1, 0.0, 255.0 },
    { "VarI2FromDisp",   K_INT,  2, -32768.0, 32767.0 },
    { "VarUI2FromDisp",  K_UINT, 2, 0.0, 65535.0 },
    { "VarI4FromDisp",   K_INT,  4, -2147483648.0, 2147483647.0 },
    { "VarUI4FromDisp",  K_UINT, 4, 0.0, 4294967295.0 },
    { "VarI8FromDisp",   K_INT,  8, -9223372036854775808.0, 9223372036854775807.0 },
    { "VarUI8FromDisp",  K_UINT, 8, 0.0, 18446744073709551615.0 },
    { "VarR4FromDisp",   K_R4,   4, 0, 0 },
    { "VarR8FromDisp",   K_R8,   8, 0, 0 },
    { "VarCyFromDisp",   K_CY,   8, 0, 0 },
    { "VarDateFromDisp", K_DATE, 8, 0, 0 },
    { "VarBoolFromDisp", K_BOOL, 2, 0, 0 },
    { "VarDecFromDisp",  K_DEC,  sizeof(DECIMAL), 0, 0 },
};

static double out_as_double(const struct dst *d, const void *o)
{
    switch (d->k) {
    case K_INT:
        switch (d->size) { case 1: return *(const signed char *)o; case 2: return *(const SHORT *)o;
        case 4: return *(const LONG *)o; default: return (double)*(const LONG64 *)o; }
    case K_UINT:
        switch (d->size) { case 1: return *(const BYTE *)o; case 2: return *(const USHORT *)o;
        case 4: return *(const ULONG *)o; default: return (double)*(const ULONG64 *)o; }
    case K_R4: return *(const float *)o;
    case K_R8: case K_DATE: return *(const double *)o;
    case K_CY: return (double)((const CY *)o)->int64 / 10000.0;
    case K_BOOL: return *(const VARIANT_BOOL *)o;
    case K_DEC: {
        const DECIMAL *dc = o;
        double m = ((double)dc->Hi32 * 4294967296.0 + dc->Mid32) * 4294967296.0 + dc->Lo32;
        m /= pow(10.0, dc->scale);
        return (dc->sign & 0x80) ? -m : m;
    }
    }
    return 0;
}

static int dnear(double a, double b) { return fabs(a - b) <= 1e-9 * fabs(b) + 1e-12; }

/* default-value variants and the numeric value they denote (NAN = not a number -> type mismatch) */
struct dcase { VARTYPE vt; double num; const WCHAR *str; const char *name; };
static const struct dcase cases[] = {
    { VT_I4, 0, NULL, "I4 0" }, { VT_I4, 42, NULL, "I4 42" }, { VT_I4, -1, NULL, "I4 -1" },
    { VT_I4, 127, NULL, "I4 127" }, { VT_I4, 128, NULL, "I4 128" }, { VT_I4, 255, NULL, "I4 255" },
    { VT_I4, 256, NULL, "I4 256" }, { VT_I4, -128, NULL, "I4 -128" }, { VT_I4, -129, NULL, "I4 -129" },
    { VT_I4, 32767, NULL, "I4 32767" }, { VT_I4, 32768, NULL, "I4 32768" }, { VT_I4, 65535, NULL, "I4 65535" },
    { VT_I4, 65536, NULL, "I4 65536" }, { VT_I4, 2147483647.0, NULL, "I4 max" }, { VT_I4, -2147483648.0, NULL, "I4 min" },
    { VT_R8, 2.5, NULL, "R8 2.5" }, { VT_R8, 3.5, NULL, "R8 3.5" }, { VT_R8, 0.5, NULL, "R8 0.5" }, { VT_R8, -0.5, NULL, "R8 -0.5" },
    { VT_R8, 1.5, NULL, "R8 1.5" }, { VT_R8, 254.5, NULL, "R8 254.5" }, { VT_R8, 255.5, NULL, "R8 255.5" },
    { VT_R8, 127.5, NULL, "R8 127.5" }, { VT_R8, -128.5, NULL, "R8 -128.5" }, { VT_R8, 1e10, NULL, "R8 1e10" },
    { VT_R8, 123.456, NULL, "R8 123.456" }, { VT_R8, -7.25, NULL, "R8 -7.25" },
    { VT_R4, 2.5, NULL, "R4 2.5" }, { VT_R4, 100.75, NULL, "R4 100.75" },
    { VT_I2, 1234, NULL, "I2 1234" }, { VT_I2, -1234, NULL, "I2 -1234" },
    { VT_UI1, 200, NULL, "UI1 200" }, { VT_I1, -100, NULL, "I1 -100" }, { VT_UI2, 60000, NULL, "UI2 60000" },
    { VT_UI4, 3000000000.0, NULL, "UI4 3e9" }, { VT_I8, 5000000000.0, NULL, "I8 5e9" },
    { VT_BSTR, 7, L"7", "BSTR 7" }, { VT_BSTR, 2.5, L"2.5", "BSTR 2.5" }, { VT_BSTR, -8, L"-8", "BSTR -8" },
    { VT_BSTR, 12, L" 12 ", "BSTR ' 12 '" }, { VT_BSTR, 300, L"300", "BSTR 300" }, { VT_BSTR, 1000, L"1e3", "BSTR 1e3" },
    { VT_BSTR, NAN, L"abc", "BSTR abc" },
};

static void fill_ret(VARIANT *v, const struct dcase *c)
{
    vclear(v);
    V_VT(v) = c->vt;
    switch (c->vt) {
    case VT_I1: V_I1(v) = (CHAR)c->num; break;
    case VT_UI1: V_UI1(v) = (BYTE)c->num; break;
    case VT_I2: V_I2(v) = (SHORT)c->num; break;
    case VT_UI2: V_UI2(v) = (USHORT)c->num; break;
    case VT_I4: V_I4(v) = (LONG)c->num; break;
    case VT_UI4: V_UI4(v) = (ULONG)c->num; break;
    case VT_I8: V_I8(v) = (LONG64)c->num; break;
    case VT_R4: V_R4(v) = (float)c->num; break;
    case VT_R8: V_R8(v) = c->num; break;
    case VT_BSTR: V_BSTR(v) = U(SysAllocString)(c->str); break;
    default: break;
    }
}

/* expected HRESULT/value of converting the number to destination d, NAN result = mismatch */
static HRESULT expect_for(const struct dst *d, const struct dcase *c, double *want)
{
    double n = c->num;
    if (isnan(n)) return DISP_E_TYPEMISMATCH;
    switch (d->k) {
    case K_INT: case K_UINT: {
        if (n < d->lo - 0.5 || n >= d->hi + 0.5) return DISP_E_OVERFLOW;
        double r = nearbyint(n);              /* round-half-even in the default FP mode */
        *want = r;
        return S_OK;
    }
    case K_R4: *want = (float)n; return S_OK;
    case K_R8: *want = n; return S_OK;
    case K_DATE:
        if (!(n > -657435.0 && n < 2958466.0)) return DISP_E_OVERFLOW;   /* VarDateFromI4/UI4/I8/R8 range check */
        *want = n; return S_OK;
    case K_CY: *want = n; return S_OK;
    case K_BOOL: *want = n != 0 ? -1 : 0; return S_OK;
    case K_DEC: *want = n; return S_OK;
    }
    return E_FAIL;
}

TEST(vcn_disp_default_value_matrix)
{
    for (unsigned di = 0; di < sizeof dsts / sizeof dsts[0]; di++) {
        const struct dst *d = &dsts[di];
        disp_fn fn = (disp_fn)t_proc(d->fn);
        if (!fn) { t_skip("missing export: %s", d->fn); return; }
        for (unsigned ci = 0; ci < sizeof cases / sizeof cases[0]; ci++) {
            const struct dcase *c = &cases[ci];
            if (d->k == K_DATE && c->vt == VT_BSTR) continue;     /* date string parsing is a different area */
            if (d->k == K_DEC && c->vt == VT_R4) continue;        /* R4->Dec digit rules covered by tables */
            if (d->k == K_CY && fabs(c->num) > 1e10) continue;
            FakeDisp fd; fd_init(&fd);
            fill_ret(&fd.ret, c);
            double want = 0;
            HRESULT whr = expect_for(d, c, &want);
            unsigned char out[32]; memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(&fd.iface, 0x409, out);
            if (hr == DISP_E_BADVARTYPE && fd.invokes == 0) { t_note("%s: BADVARTYPE without Invoke (old oleaut32 behaviour)", d->fn); vclear(&fd.ret); continue; }
            CHECKF(fd.invokes == 1, "%s case %s: Invoke called %d times", d->fn, c->name, fd.invokes);
            /* sign-changing integer coercions through VariantChangeTypeEx are not covered by the decomp read */
            int soft = (d->k == K_UINT && c->num < 0) || (d->k == K_INT && whr == DISP_E_OVERFLOW &&
                       (c->vt == VT_UI1 || c->vt == VT_UI2 || c->vt == VT_UI4));
            if (soft) { if (hr != whr) t_note("%s case %s: hr 0x%08lx (model 0x%08lx)", d->fn, c->name, (unsigned long)hr, (unsigned long)whr); }
            else CHECKF(hr == whr, "%s case %s: hr 0x%08lx want 0x%08lx", d->fn, c->name, (unsigned long)hr, (unsigned long)whr);
            if (whr == S_OK && hr == S_OK) {
                double got = out_as_double(d, out);
                CHECKF(dnear(got, want), "%s case %s: got %.17g want %.17g", d->fn, c->name, got, want);
            } else if (hr != S_OK && !soft) {
                CHECKF(vcn_untouched(out, d->size), "%s case %s: output written on failure", d->fn, c->name);
            }
            vclear(&fd.ret);
        }
    }
}

TEST(vcn_disp_invoke_arguments)
{
    /* the default-value fetch is Invoke(DISPID_VALUE, IID_NULL, lcid, DISPATCH_PROPERTYGET, no args) */
    for (unsigned di = 0; di < sizeof dsts / sizeof dsts[0]; di++) {
        const struct dst *d = &dsts[di];
        disp_fn fn = (disp_fn)t_proc(d->fn);
        if (!fn) { t_skip("missing export: %s", d->fn); return; }
        FakeDisp fd; fd_init(&fd);
        V_VT(&fd.ret) = VT_I4; V_I4(&fd.ret) = 3;
        unsigned char out[32];
        HRESULT hr = fn(&fd.iface, 0x409, out);
        if (hr == DISP_E_BADVARTYPE && fd.invokes == 0) { t_note("%s: no Invoke performed", d->fn); continue; }
        CHECKF(fd.invokes == 1, "%s: invokes=%d", d->fn, fd.invokes);
        CHECKF(fd.dispid == DISPID_VALUE, "%s: dispid=%ld", d->fn, (long)fd.dispid);
        CHECKF(fd.flags == DISPATCH_PROPERTYGET, "%s: flags=0x%x", d->fn, fd.flags);
        CHECKF(fd.cargs == 0 && fd.cnamed == 0, "%s: cArgs=%u cNamed=%u", d->fn, fd.cargs, fd.cnamed);
        CHECKF(fd.riid_null, "%s: riid not IID_NULL", d->fn);
        CHECKF(fd.res_empty, "%s: result variant not VT_EMPTY on entry", d->fn);
        CHECKF(fd.ei_null, "%s: EXCEPINFO not NULL", d->fn);
        CHECKF(fd.argerr_null, "%s: puArgErr not NULL", d->fn);
        if (fd.lcid != 0x409) t_note("%s: Invoke lcid=0x%lx (passed 0x409)", d->fn, (unsigned long)fd.lcid);
        if (fd.ref != 1) t_note("%s: ref=%ld after call (AddRef/Release not balanced)", d->fn, (long)fd.ref);
        vclear(&fd.ret);
    }
}

TEST(vcn_disp_invoke_failure)
{
    /* Wine's test (valid for native): a failing Invoke maps to DISP_E_TYPEMISMATCH and leaves the output alone */
    static const HRESULT fails[] = { E_OUTOFMEMORY, E_FAIL, DISP_E_MEMBERNOTFOUND, DISP_E_EXCEPTION, E_NOTIMPL };
    for (unsigned di = 0; di < sizeof dsts / sizeof dsts[0]; di++) {
        const struct dst *d = &dsts[di];
        disp_fn fn = (disp_fn)t_proc(d->fn);
        if (!fn) { t_skip("missing export: %s", d->fn); return; }
        for (unsigned fi = 0; fi < sizeof fails / sizeof fails[0]; fi++) {
            FakeDisp fd; fd_init(&fd);
            fd.fail = fails[fi];
            unsigned char out[32]; memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(&fd.iface, 0x409, out);
            if (fi == 0) {
                CHECKF(hr == DISP_E_TYPEMISMATCH, "%s: hr 0x%08lx want TYPEMISMATCH", d->fn, (unsigned long)hr);
                CHECKF(fd.invokes == 1, "%s: invokes=%d", d->fn, fd.invokes);
            } else {
                if (hr != DISP_E_TYPEMISMATCH) t_note("%s: Invoke failure 0x%08lx mapped to 0x%08lx", d->fn, (unsigned long)fails[fi], (unsigned long)hr);
            }
            CHECKF(vcn_untouched(out, d->size), "%s: output written after failing Invoke", d->fn);
        }
    }
}

TEST(vcn_disp_null_dispatch)
{
    /* VT_DISPATCH with a NULL pointer: documented-by-observation only */
    for (unsigned di = 0; di < sizeof dsts / sizeof dsts[0]; di++) {
        const struct dst *d = &dsts[di];
        disp_fn fn = (disp_fn)t_proc(d->fn);
        if (!fn) { t_skip("missing export: %s", d->fn); return; }
        unsigned char out[32]; memset(out, 0xCC, sizeof out);
        HRESULT hr = fn(NULL, 0x409, out);
        t_note("%s(NULL dispatch) -> 0x%08lx, out %s", d->fn, (unsigned long)hr, vcn_untouched(out, d->size) ? "untouched" : "written");
        CHECKF(FAILED(hr) || hr == S_OK, "%s: hr sanity", d->fn);
    }
}
