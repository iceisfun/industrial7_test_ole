/* Server-side objects (implemented in this exe) that the typelib marshaler talks to. */
#include "tm.h"

TMSrvLast g_last;
static volatile LONG g_live;
LONG tm_live_objects(void) { return g_live; }

static void note_call(void) { g_last.calls++; g_last.tid = GetCurrentThreadId(); }
#define SREL(p) ((p)->lpVtbl->Release(p))
#define SADD(p) ((p)->lpVtbl->AddRef(p))

/* ------------------------------------------------------------------ IOletSink */
typedef struct Sink { IOletSink s; LONG ref; LONG total; } Sink;
static HRESULT TM_CALL sink_qi(IOletSink *i, REFIID riid, void **pp)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &TM_IID_IOletSink)) { *pp = i; SADD(i); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG TM_CALL sink_addref(IOletSink *i) { return InterlockedIncrement(&((Sink *)i)->ref); }
static ULONG TM_CALL sink_release(IOletSink *i)
{
    Sink *s = (Sink *)i; LONG r = InterlockedDecrement(&s->ref);
    if (!r) { InterlockedDecrement(&g_live); free(s); }
    return r;
}
static HRESULT TM_CALL sink_notify(IOletSink *i, LONG n, BSTR str)
{
    Sink *s = (Sink *)i; note_call();
    s->total += n; g_last.nsink++;
    g_last.str[0] = 0; g_last.strlen_ = 0;
    if (str) { lstrcpynW(g_last.str, str, 64); g_last.strlen_ = SysStringLen(str); }
    g_last.str_was_null = str == NULL;
    return S_OK;
}
static HRESULT TM_CALL sink_total(IOletSink *i, LONG *t) { *t = ((Sink *)i)->total; return S_OK; }
static const struct IOletSinkVtbl sink_vtbl = { sink_qi, sink_addref, sink_release, sink_notify, sink_total };
IOletSink *tm_new_sink(void)
{
    Sink *s = calloc(1, sizeof *s); s->s.lpVtbl = &sink_vtbl; s->ref = 1; InterlockedIncrement(&g_live); return &s->s;
}
LONG tm_sink_total(IOletSink *s) { return ((Sink *)s)->total; }
IUnknown *tm_mk_sink(void) { return (IUnknown *)tm_new_sink(); }

/* ------------------------------------------------------------------ ISupportErrorInfo */
typedef struct SEI { const struct ISupportErrorInfoVtbl *lpVtbl; } SEI;
static void raise_errorinfo(const GUID *iid, BSTR desc)
{
    ICreateErrorInfo *cei = NULL; IErrorInfo *ei = NULL;
    if (FAILED(U(CreateErrorInfo)(&cei))) return;
    ICreateErrorInfo_SetGUID(cei, iid);
    ICreateErrorInfo_SetSource(cei, L"Olet.Server");
    ICreateErrorInfo_SetDescription(cei, desc);
    ICreateErrorInfo_SetHelpFile(cei, L"olet.hlp");
    ICreateErrorInfo_SetHelpContext(cei, 4242);
    if (SUCCEEDED(ICreateErrorInfo_QueryInterface(cei, &IID_IErrorInfo, (void **)&ei))) {
        U(SetErrorInfo)(0, ei);
        IErrorInfo_Release(ei);
    }
    ICreateErrorInfo_Release(cei);
}

/* ------------------------------------------------------------------ IOletCustom */
typedef struct Custom { IOletCustom c; SEI sei; LONG ref; IOletSink *sink; } Custom;
static HRESULT TM_CALL c_qi(IOletCustom *i, REFIID riid, void **pp)
{
    Custom *c = (Custom *)i;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &TM_IID_IOletCustom)) *pp = &c->c;
    else if (IsEqualIID(riid, &TM_IID_ISupportErrorInfo)) *pp = &c->sei;
    else { *pp = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&c->ref);
    return S_OK;
}
static ULONG TM_CALL c_addref(IOletCustom *i) { return InterlockedIncrement(&((Custom *)i)->ref); }
static ULONG TM_CALL c_release(IOletCustom *i)
{
    Custom *c = (Custom *)i; LONG r = InterlockedDecrement(&c->ref);
    if (!r) { if (c->sink) SREL(c->sink); InterlockedDecrement(&g_live); free(c); }
    return r;
}
static HRESULT TM_CALL c_AddLong(IOletCustom *i, LONG a, LONG b, LONG *r) { note_call(); *r = a + b; return S_OK; }
static HRESULT TM_CALL c_ScalarsIn(IOletCustom *i, BYTE b, SHORT s, LONG l, LONGLONG h, FLOAT f, DOUBLE d, VARIANT_BOOL vb, DATE dt)
{
    note_call();
    g_last.b = b; g_last.s = s; g_last.l = l; g_last.h = h; g_last.f = f; g_last.d = d; g_last.vb = vb; g_last.dt = dt;
    return S_OK;
}
static HRESULT TM_CALL c_ScalarsOut(IOletCustom *i, BYTE *b, SHORT *s, LONG *l, LONGLONG *h, FLOAT *f, DOUBLE *d, VARIANT_BOOL *vb, DATE *dt)
{
    note_call();
    *b = 0xAB; *s = -1234; *l = 0x12345678; *h = 0x1122334455667788LL; *f = 1.5f; *d = -2.25; *vb = VARIANT_TRUE;
    *dt = 36526.5;
    return S_OK;
}
static HRESULT TM_CALL c_EchoBstr(IOletCustom *i, BSTR in, BSTR *out)
{
    note_call();
    g_last.str_was_null = in == NULL; g_last.strlen_ = SysStringLen(in);
    *out = in ? SysAllocStringLen(in, SysStringLen(in)) : NULL;
    return S_OK;
}
static HRESULT TM_CALL c_BstrOut(IOletCustom *i, BSTR *out) { note_call(); *out = SysAllocString(L"server-out"); return S_OK; }
static HRESULT TM_CALL c_BstrInOut(IOletCustom *i, BSTR *io)
{
    BSTR n; UINT len = SysStringLen(*io);
    note_call();
    g_last.str_was_null = *io == NULL; g_last.strlen_ = len;
    n = SysAllocStringLen(NULL, len + 4);
    if (len) memcpy(n, *io, len * sizeof(WCHAR));
    memcpy(n + len, L"+srv", 4 * sizeof(WCHAR));
    SysFreeString(*io);
    *io = n;
    return S_OK;
}
static HRESULT TM_CALL c_EchoVariant(IOletCustom *i, VARIANT v, VARIANT *out)
{
    note_call(); g_last.vt = V_VT(&v);
    VariantInit(out);
    return VariantCopy(out, &v);
}
static HRESULT TM_CALL c_VariantInOut(IOletCustom *i, VARIANT *io)
{
    note_call(); g_last.vt = V_VT(io);
    if (V_VT(io) == VT_I4) { g_last.vlong = V_I4(io); V_I4(io) += 1; }
    else if (V_VT(io) == VT_BSTR) {
        UINT len = SysStringLen(V_BSTR(io)); BSTR n = SysAllocStringLen(NULL, len + 4);
        if (len) memcpy(n, V_BSTR(io), len * sizeof(WCHAR));
        memcpy(n + len, L"+srv", 4 * sizeof(WCHAR));
        SysFreeString(V_BSTR(io)); V_BSTR(io) = n;
    }
    return S_OK;
}
static HRESULT TM_CALL c_SumArray(IOletCustom *i, SAFEARRAY *sa, LONG *sum)
{
    LONG lb, ub, k, s = 0; LONG *data = NULL;
    note_call();
    if (!sa) { *sum = -1; return S_FALSE; }
    SafeArrayGetLBound(sa, 1, &lb); SafeArrayGetUBound(sa, 1, &ub);
    if (SUCCEEDED(SafeArrayAccessData(sa, (void **)&data))) { for (k = 0; k <= ub - lb; k++) s += data[k]; SafeArrayUnaccessData(sa); }
    *sum = s;
    return S_OK;
}
static HRESULT TM_CALL c_MakeArray(IOletCustom *i, LONG n, SAFEARRAY **out)
{
    SAFEARRAY *sa; LONG k;
    note_call();
    if (n < 0) { *out = NULL; return E_INVALIDARG; }
    sa = SafeArrayCreateVector(VT_I4, 5, n);   /* lower bound 5 */
    for (k = 0; k < n; k++) { LONG idx = 5 + k, val = k + 1; SafeArrayPutElement(sa, &idx, &val); }
    *out = sa;
    return S_OK;
}
static HRESULT TM_CALL c_EchoBstrArray(IOletCustom *i, SAFEARRAY *in, SAFEARRAY **out)
{
    note_call();
    *out = NULL;
    if (!in) return S_OK;
    return SafeArrayCopy(in, out);
}
static HRESULT TM_CALL c_GetSink(IOletCustom *i, IOletSink **pp)
{
    Custom *c = (Custom *)i; note_call();
    *pp = tm_new_sink(); ((Sink *)*pp)->total = 100;
    (void)c;
    return S_OK;
}
static HRESULT TM_CALL c_PutSink(IOletCustom *i, IOletSink *p)
{
    Custom *c = (Custom *)i; note_call();
    if (p) SADD(p);
    if (c->sink) SREL(c->sink);
    c->sink = p;
    return S_OK;
}
static HRESULT TM_CALL c_EchoUnk(IOletCustom *i, IUnknown *in, IUnknown **out)
{
    note_call();
    *out = in; if (in) IUnknown_AddRef(in);
    return S_OK;
}
static HRESULT TM_CALL c_EchoPoint(IOletCustom *i, OletPoint p, OletPoint *out)
{
    note_call(); g_last.point_x = p.x; g_last.point_y = p.y;
    out->x = p.x * 2; out->y = p.y * 2;
    return S_OK;
}
static HRESULT TM_CALL c_SwapPoint(IOletCustom *i, OletPoint *io)
{
    LONG t = io->x; note_call();
    io->x = io->y; io->y = t;
    return S_OK;
}
static HRESULT TM_CALL c_EchoMix(IOletCustom *i, OletMix *in, OletMix *out)
{
    note_call();
    out->name = NULL;
    if (in->name) {
        UINT len = SysStringLen(in->name); out->name = SysAllocStringLen(NULL, len + 1);
        if (len) memcpy(out->name, in->name, len * sizeof(WCHAR));
        out->name[len] = L'!';
    }
    out->d = in->d * 2; out->id = in->id + 1; out->pt.x = in->pt.y; out->pt.y = in->pt.x;
    return S_OK;
}
static HRESULT TM_CALL c_EchoColor(IOletCustom *i, OletColor c, OletColor *out) { note_call(); *out = c * 2; return S_OK; }
static HRESULT TM_CALL c_ReturnHr(IOletCustom *i, LONG hr) { note_call(); return hr; }
static HRESULT TM_CALL c_FailWithOut(IOletCustom *i, LONG hr, BSTR *pb, LONG *pl, IOletSink **pp)
{
    note_call();
    *pb = SysAllocString(L"fail-out"); *pl = 77; *pp = tm_new_sink();
    return hr;
}
static HRESULT TM_CALL c_RaiseError(IOletCustom *i, BSTR desc, LONG hr)
{
    note_call();
    raise_errorinfo(&TM_IID_IOletCustom, desc);
    return hr;
}
static HRESULT TM_CALL c_Callback(IOletCustom *i, IOletSink *sink, LONG n, LONG *total)
{
    HRESULT hr; BSTR s = SysAllocString(L"cb");
    note_call();
    hr = sink->lpVtbl->Notify(sink, n, s);
    SysFreeString(s);
    if (SUCCEEDED(hr)) hr = sink->lpVtbl->Total(sink, total);
    return hr;
}
static HRESULT TM_CALL c_ApartmentInfo(IOletCustom *i, LONG *tid, LONG *pid, LONG *apt)
{
    APTTYPE at = APTTYPE_CURRENT; APTTYPEQUALIFIER q = 0; HRESULT hr;
    note_call();
    *tid = (LONG)GetCurrentThreadId(); *pid = (LONG)GetCurrentProcessId();
    hr = U(CoGetApartmentType)(&at, &q);
    *apt = SUCCEEDED(hr) ? (LONG)at : -1;
    return S_OK;
}
static HRESULT TM_CALL c_LongPtrs(IOletCustom *i, LONG *in, LONG *out, LONG *inout)
{
    note_call();
    g_last.in_long = *in; g_last.inout_long_before = *inout;
    *out = *in + 1; *inout += 100; *in = 999;
    return S_OK;
}
static HRESULT TM_CALL c_CurrencyEcho(IOletCustom *i, CY c, CY *r) { note_call(); g_last.cy = c; r->int64 = c.int64 + 10000; return S_OK; }
static const struct IOletCustomVtbl custom_vtbl = {
    c_qi, c_addref, c_release, c_AddLong, c_ScalarsIn, c_ScalarsOut, c_EchoBstr, c_BstrOut, c_BstrInOut, c_EchoVariant,
    c_VariantInOut, c_SumArray, c_MakeArray, c_EchoBstrArray, c_GetSink, c_PutSink, c_EchoUnk, c_EchoPoint, c_SwapPoint,
    c_EchoMix, c_EchoColor, c_ReturnHr, c_FailWithOut, c_RaiseError, c_Callback, c_ApartmentInfo, c_LongPtrs, c_CurrencyEcho };

static HRESULT TM_CALL sei_qi(ISupportErrorInfo *i, REFIID riid, void **pp)
{
    Custom *c = (Custom *)((char *)i - offsetof(Custom, sei));
    return c_qi(&c->c, riid, pp);
}
static ULONG TM_CALL sei_addref(ISupportErrorInfo *i) { Custom *c = (Custom *)((char *)i - offsetof(Custom, sei)); return c_addref(&c->c); }
static ULONG TM_CALL sei_release(ISupportErrorInfo *i) { Custom *c = (Custom *)((char *)i - offsetof(Custom, sei)); return c_release(&c->c); }
static HRESULT TM_CALL sei_supports(ISupportErrorInfo *i, REFIID riid)
{
    return (IsEqualIID(riid, &TM_IID_IOletCustom) || IsEqualIID(riid, &TM_IID_IOletDual)) ? S_OK : S_FALSE;
}
static const struct ISupportErrorInfoVtbl sei_vtbl = { sei_qi, sei_addref, sei_release, sei_supports };

IOletCustom *tm_new_custom(void)
{
    Custom *c = calloc(1, sizeof *c);
    c->c.lpVtbl = &custom_vtbl; c->sei.lpVtbl = &sei_vtbl; c->ref = 1; InterlockedIncrement(&g_live);
    return &c->c;
}
IUnknown *tm_mk_custom(void) { return (IUnknown *)tm_new_custom(); }

/* ------------------------------------------------------------------ IEnumVARIANT over {10, 20, 30} */
typedef struct EnumV { IEnumVARIANT e; LONG ref; ULONG pos; } EnumV;
static HRESULT TM_CALL ev_qi(IEnumVARIANT *i, REFIID riid, void **pp)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IEnumVARIANT)) { *pp = i; IEnumVARIANT_AddRef(i); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG TM_CALL ev_addref(IEnumVARIANT *i) { return InterlockedIncrement(&((EnumV *)i)->ref); }
static ULONG TM_CALL ev_release(IEnumVARIANT *i)
{
    EnumV *e = (EnumV *)i; LONG r = InterlockedDecrement(&e->ref);
    if (!r) { InterlockedDecrement(&g_live); free(e); }
    return r;
}
static HRESULT TM_CALL ev_next(IEnumVARIANT *i, ULONG n, VARIANT *v, ULONG *fetched)
{
    EnumV *e = (EnumV *)i; ULONG k = 0;
    while (k < n && e->pos < 3) { VariantInit(&v[k]); V_VT(&v[k]) = VT_I4; V_I4(&v[k]) = (LONG)(10 * (e->pos + 1)); e->pos++; k++; }
    if (fetched) *fetched = k;
    return k == n ? S_OK : S_FALSE;
}
static HRESULT TM_CALL ev_skip(IEnumVARIANT *i, ULONG n)
{
    EnumV *e = (EnumV *)i; ULONG left = 3 - e->pos;
    if (n > left) { e->pos = 3; return S_FALSE; }
    e->pos += n; return S_OK;
}
static HRESULT TM_CALL ev_reset(IEnumVARIANT *i) { ((EnumV *)i)->pos = 0; return S_OK; }
static HRESULT TM_CALL ev_clone(IEnumVARIANT *i, IEnumVARIANT **pp)
{
    EnumV *e = (EnumV *)i, *n = calloc(1, sizeof *n);
    n->e.lpVtbl = e->e.lpVtbl; n->ref = 1; n->pos = e->pos; InterlockedIncrement(&g_live);
    *pp = &n->e; return S_OK;
}
static const IEnumVARIANTVtbl ev_vtbl = { ev_qi, ev_addref, ev_release, ev_next, ev_skip, ev_reset, ev_clone };
static IEnumVARIANT *new_enumv(void)
{
    EnumV *e = calloc(1, sizeof *e); e->e.lpVtbl = (IEnumVARIANTVtbl *)&ev_vtbl; e->ref = 1; InterlockedIncrement(&g_live); return &e->e;
}

/* ------------------------------------------------------------------ IOletDual */
typedef struct Dual { IOletDual d; SEI sei; LONG ref; ITypeInfo *ti; VARIANT value; } Dual;
static HRESULT TM_CALL d_qi(IOletDual *i, REFIID riid, void **pp)
{
    Dual *d = (Dual *)i;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch) || IsEqualIID(riid, &TM_IID_IOletDual)) *pp = &d->d;
    else if (IsEqualIID(riid, &TM_IID_ISupportErrorInfo)) *pp = &d->sei;
    else { *pp = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&d->ref);
    return S_OK;
}
static ULONG TM_CALL d_addref(IOletDual *i) { return InterlockedIncrement(&((Dual *)i)->ref); }
static ULONG TM_CALL d_release(IOletDual *i)
{
    Dual *d = (Dual *)i; LONG r = InterlockedDecrement(&d->ref);
    if (!r) { if (d->ti) ITypeInfo_Release(d->ti); VariantClear(&d->value); InterlockedDecrement(&g_live); free(d); }
    return r;
}
static HRESULT get_ti(Dual *d)
{
    ITypeLib *tl = NULL; HRESULT hr;
    if (d->ti) return S_OK;
    hr = U(LoadRegTypeLib)(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, &tl);
    if (FAILED(hr)) return hr;
    hr = ITypeLib_GetTypeInfoOfGuid(tl, &TM_IID_IOletDual, &d->ti);
    ITypeLib_Release(tl);
    return hr;
}
static HRESULT TM_CALL d_GetTypeInfoCount(IOletDual *i, UINT *n) { *n = 1; return S_OK; }
static HRESULT TM_CALL d_GetTypeInfo(IOletDual *i, UINT idx, LCID lcid, ITypeInfo **pp)
{
    Dual *d = (Dual *)i; HRESULT hr;
    *pp = NULL;
    if (idx != 0) return DISP_E_BADINDEX;
    hr = get_ti(d);
    if (FAILED(hr)) return hr;
    ITypeInfo_AddRef(d->ti); *pp = d->ti;
    return S_OK;
}
static HRESULT TM_CALL d_GetIDsOfNames(IOletDual *i, REFIID riid, LPOLESTR *names, UINT n, LCID lcid, DISPID *ids)
{
    Dual *d = (Dual *)i; HRESULT hr = get_ti(d);
    if (FAILED(hr)) return hr;
    return U(DispGetIDsOfNames)(d->ti, names, n, ids);
}
static HRESULT TM_CALL d_Invoke(IOletDual *i, DISPID id, REFIID riid, LCID lcid, WORD fl, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, UINT *argerr)
{
    Dual *d = (Dual *)i; HRESULT hr = get_ti(d);
    if (FAILED(hr)) return hr;
    return U(DispInvoke)(&d->d, d->ti, id, fl, dp, res, ei, argerr);
}
static HRESULT TM_CALL d_Add(IOletDual *i, LONG a, LONG b, LONG *r) { note_call(); *r = a + b; return S_OK; }
static HRESULT TM_CALL d_Concat(IOletDual *i, BSTR a, BSTR b, BSTR *r)
{
    UINT la = SysStringLen(a), lb = SysStringLen(b); BSTR n;
    note_call();
    n = SysAllocStringLen(NULL, la + lb);
    if (la) memcpy(n, a, la * sizeof(WCHAR));
    if (lb) memcpy(n + la, b, lb * sizeof(WCHAR));
    *r = n; return S_OK;
}
static HRESULT TM_CALL d_get_Value(IOletDual *i, VARIANT *r) { Dual *d = (Dual *)i; note_call(); VariantInit(r); return VariantCopy(r, &d->value); }
static HRESULT TM_CALL d_put_Value(IOletDual *i, VARIANT v) { Dual *d = (Dual *)i; note_call(); VariantClear(&d->value); return VariantCopy(&d->value, &v); }
static HRESULT TM_CALL d_get_Count(IOletDual *i, LONG *r) { note_call(); *r = 3; return S_OK; }
static HRESULT TM_CALL d_Fail(IOletDual *i, LONG hr) { note_call(); return hr; }
static HRESULT TM_CALL d_ItemName(IOletDual *i, LONG idx, BSTR *r)
{
    WCHAR b[24];
    note_call();
    if (idx < 0 || idx > 2) { *r = NULL; return E_INVALIDARG; }
    _snwprintf(b, 24, L"item%ld", (long)idx);
    *r = SysAllocString(b);
    return S_OK;
}
static HRESULT TM_CALL d_RaiseError(IOletDual *i, BSTR desc, LONG hr)
{
    note_call();
    raise_errorinfo(&TM_IID_IOletDual, desc);
    return hr;
}
static HRESULT TM_CALL d_Sum(IOletDual *i, SAFEARRAY *sa, LONG *r) { return c_SumArray(NULL, sa, r); }
static HRESULT TM_CALL d_get_NewEnum(IOletDual *i, IUnknown **r) { note_call(); *r = (IUnknown *)new_enumv(); return S_OK; }
static const struct IOletDualVtbl dual_vtbl = {
    d_qi, d_addref, d_release, d_GetTypeInfoCount, d_GetTypeInfo, d_GetIDsOfNames, d_Invoke, d_Add, d_Concat, d_get_Value,
    d_put_Value, d_get_Count, d_Fail, d_ItemName, d_RaiseError, d_Sum, d_get_NewEnum };

static HRESULT TM_CALL dsei_qi(ISupportErrorInfo *i, REFIID riid, void **pp)
{
    Dual *d = (Dual *)((char *)i - offsetof(Dual, sei));
    return d_qi(&d->d, riid, pp);
}
static ULONG TM_CALL dsei_addref(ISupportErrorInfo *i) { Dual *d = (Dual *)((char *)i - offsetof(Dual, sei)); return d_addref(&d->d); }
static ULONG TM_CALL dsei_release(ISupportErrorInfo *i) { Dual *d = (Dual *)((char *)i - offsetof(Dual, sei)); return d_release(&d->d); }
static const struct ISupportErrorInfoVtbl dsei_vtbl = { dsei_qi, dsei_addref, dsei_release, sei_supports };

IOletDual *tm_new_dual(void)
{
    Dual *d = calloc(1, sizeof *d);
    d->d.lpVtbl = &dual_vtbl; d->sei.lpVtbl = &dsei_vtbl; d->ref = 1; VariantInit(&d->value);
    V_VT(&d->value) = VT_I4; V_I4(&d->value) = 5;
    InterlockedIncrement(&g_live);
    return &d->d;
}
IUnknown *tm_mk_dual(void) { return (IUnknown *)tm_new_dual(); }

/* ------------------------------------------------------------------ IOletMini (any IID) */
typedef struct Mini { IOletMini m; LONG ref; GUID iid; } Mini;
static HRESULT TM_CALL m_qi(IOletMini *i, REFIID riid, void **pp)
{
    Mini *m = (Mini *)i;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &m->iid)) { *pp = i; InterlockedIncrement(&m->ref); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG TM_CALL m_addref(IOletMini *i) { return InterlockedIncrement(&((Mini *)i)->ref); }
static ULONG TM_CALL m_release(IOletMini *i)
{
    Mini *m = (Mini *)i; LONG r = InterlockedDecrement(&m->ref);
    if (!r) { InterlockedDecrement(&g_live); free(m); }
    return r;
}
static HRESULT TM_CALL m_ping(IOletMini *i, LONG x, LONG *r) { note_call(); *r = x + 1000; return S_OK; }
static const struct IOletMiniVtbl mini_vtbl = { m_qi, m_addref, m_release, m_ping };
IOletMini *tm_new_mini(const GUID *iid)
{
    Mini *m = calloc(1, sizeof *m); m->m.lpVtbl = &mini_vtbl; m->ref = 1; m->iid = *iid; InterlockedIncrement(&g_live); return &m->m;
}
