/* Standard proxy/stub coverage across apartments: interfaces whose proxies/stubs ship with the system
 * (IStream, IStorage, IEnumXXX in ole32; IDispatch, IEnumVARIANT, VARIANT/BSTR marshaling in oleaut32). */
#include "xp.h"

#define INIT(m) do { HRESULT h_ = U(CoInitializeEx)(NULL, (m)); if (FAILED(h_)) { t_skip("CoInitializeEx failed %08lx", h_); return; } } while (0)
#define UNINIT() U(CoUninitialize)()

/* ================================ IStream ===================================================== */
typedef struct { IStream *s; XpHost h; IStream *p; } SFx;
static BOOL sfx_setup(SFx *f, DWORD hostModel)
{
    HRESULT hr;
    memset(f, 0, sizeof *f);
    f->s = xp_new_stream();
    if (!f->s) return FALSE;
    if (!xp_host_start(&f->h, hostModel, (IUnknown *)f->s, &XP_IID_IStream, MSHLFLAGS_NORMAL)) return FALSE;
    hr = xp_host_unmarshal(&f->h, &XP_IID_IStream, (void **)&f->p);
    return SUCCEEDED(hr) && f->p;
}
static void sfx_teardown(SFx *f)
{
    if (f->p) IStream_Release(f->p);
    xp_host_stop(&f->h);
    if (f->s) IStream_Release(f->s);
}
static void stream_roundtrip(DWORD hostModel)
{
    SFx f; static const char data[] = "hello, world"; char buf[64]; ULONG n = 99; HRESULT hr; LARGE_INTEGER li; ULARGE_INTEGER pos, sz; STATSTG st;
    XP_NEED_BASE(); NEED(CreateStreamOnHGlobal); INIT(COINIT_APARTMENTTHREADED);
    if (sfx_setup(&f, hostModel)) {
        CHECK(f.p != f.s);
        hr = IStream_Write(f.p, data, sizeof data, &n);          XEQ(hr, S_OK); XEQ(n, sizeof data);
        hr = IStream_Write(f.p, data, 0, &n);                    XEQ(hr, S_OK); XEQ(n, 0);
        hr = IStream_Write(f.p, "!", 1, NULL);                   XEQ(hr, S_OK);            /* pcbWritten is optional */
        li.QuadPart = 0; pos.QuadPart = 0xCAFE;
        hr = IStream_Seek(f.p, li, STREAM_SEEK_SET, &pos);       XEQ(hr, S_OK); XEQ(pos.QuadPart, 0);
        memset(buf, 0, sizeof buf); n = 99;
        hr = IStream_Read(f.p, buf, sizeof data, &n);            XEQ(hr, S_OK); XEQ(n, sizeof data);
        CHECK(!memcmp(buf, data, sizeof data));
        n = 99; hr = IStream_Read(f.p, buf, 1, &n);              XEQ(hr, S_OK); XEQ(n, 1); XEQ(buf[0], '!');
        n = 99; hr = IStream_Read(f.p, buf, 10, &n);             XEQ(hr, S_OK); XEQ(n, 0);  /* at EOF: S_OK, nothing read */
        li.QuadPart = -3; pos.QuadPart = 0;
        hr = IStream_Seek(f.p, li, STREAM_SEEK_END, &pos);       XEQ(hr, S_OK); XEQ(pos.QuadPart, sizeof data + 1 - 3);
        hr = IStream_Read(f.p, buf, 3, NULL);                    XEQ(hr, S_OK);            /* pcbRead is optional */
        li.QuadPart = 4; pos.QuadPart = 0;
        hr = IStream_Seek(f.p, li, STREAM_SEEK_CUR, &pos);       XEQ(hr, S_OK); XEQ(pos.QuadPart, sizeof data + 1 + 4);
        n = 99; hr = IStream_Read(f.p, buf, 4, &n);              XEQ(hr, S_OK); XEQ(n, 0);  /* seek beyond the end is allowed */
        memset(&st, 0xEE, sizeof st);
        hr = IStream_Stat(f.p, &st, STATFLAG_NONAME);            XEQ(hr, S_OK);
        XEQ(st.type, STGTY_STREAM); XEQ(st.cbSize.QuadPart, sizeof data + 1); CHECK(st.pwcsName == NULL);
        sz.QuadPart = 100; hr = IStream_SetSize(f.p, sz);        XEQ(hr, S_OK);
        IStream_Stat(f.p, &st, STATFLAG_NONAME);                 XEQ(st.cbSize.QuadPart, 100);
        sz.QuadPart = 4;   hr = IStream_SetSize(f.p, sz);        XEQ(hr, S_OK);
        IStream_Stat(f.p, &st, STATFLAG_NONAME);                 XEQ(st.cbSize.QuadPart, 4);
        hr = IStream_Commit(f.p, STGC_DEFAULT);                  XEQ(hr, S_OK);
        hr = IStream_Revert(f.p);                                XEQ(hr, S_OK);
        sz.QuadPart = 1;
        hr = IStream_LockRegion(f.p, pos, sz, LOCK_WRITE);       XEQ(hr, STG_E_INVALIDFUNCTION);   /* HGLOBAL streams do not lock */
        hr = IStream_UnlockRegion(f.p, pos, sz, LOCK_WRITE);     XEQ(hr, STG_E_INVALIDFUNCTION);
        li.QuadPart = 0; pos.QuadPart = 0x1234;
        hr = IStream_Seek(f.p, li, STREAM_SEEK_END + 1, &pos);   XEQ(hr, STG_E_SEEKERROR);
        t_note("position out-parameter after a failed seek through the proxy: %llu", pos.QuadPart);
        /* the server object really is the one that changed */
        IStream_Stat(f.s, &st, STATFLAG_NONAME);                 XEQ(st.cbSize.QuadPart, 4);
    } else CHECK(0);
    sfx_teardown(&f); UNINIT();
}
XT(ps_stream_roundtrip_sta_host) { stream_roundtrip(COINIT_APARTMENTTHREADED); }
XT(ps_stream_roundtrip_mta_host) { stream_roundtrip(COINIT_MULTITHREADED); }

XT(ps_stream_large_buffers) {
    SFx f; unsigned char *w, *r; ULONG n, total = 262144, i; HRESULT hr; LARGE_INTEGER li;
    XP_NEED_BASE(); NEED(CreateStreamOnHGlobal); INIT(COINIT_APARTMENTTHREADED);
    w = HeapAlloc(GetProcessHeap(), 0, total); r = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total);
    for (i = 0; i < total; i++) w[i] = (unsigned char)(i * 31 + (i >> 8));
    if (sfx_setup(&f, COINIT_APARTMENTTHREADED)) {
        n = 0; hr = IStream_Write(f.p, w, total, &n);            XEQ(hr, S_OK); XEQ(n, total);
        li.QuadPart = 0; IStream_Seek(f.p, li, STREAM_SEEK_SET, NULL);
        n = 0; hr = IStream_Read(f.p, r, total, &n);             XEQ(hr, S_OK); XEQ(n, total);
        CHECK(!memcmp(w, r, total));
        /* odd sized chunks */
        li.QuadPart = 0; IStream_Seek(f.p, li, STREAM_SEEK_SET, NULL);
        memset(r, 0, total);
        for (i = 0; i < total; i += 4099) {
            ULONG want = min(4099, total - i);
            n = 0; hr = IStream_Read(f.p, r + i, want, &n);
            if (hr != S_OK || n != want) { CHECKF(0, "chunk at %lu: hr=%08lx n=%lu", i, hr, n); break; }
        }
        CHECK(!memcmp(w, r, total));
    } else CHECK(0);
    sfx_teardown(&f);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, r);
    UNINIT();
}
XT(ps_stream_clone_and_copyto) {
    SFx f, g; IStream *clone = NULL; HRESULT hr; char buf[32]; ULONG n; LARGE_INTEGER li; ULARGE_INTEGER cb, rd, wr, pos; STATSTG st;
    XP_NEED_BASE(); NEED(CreateStreamOnHGlobal); INIT(COINIT_APARTMENTTHREADED);
    if (sfx_setup(&f, COINIT_APARTMENTTHREADED) && sfx_setup(&g, COINIT_APARTMENTTHREADED)) {
        IStream_Write(f.p, "0123456789", 10, NULL);
        li.QuadPart = 4; IStream_Seek(f.p, li, STREAM_SEEK_SET, NULL);
        hr = IStream_Clone(f.p, &clone);                         XEQ(hr, S_OK);
        CHECK(clone != NULL && clone != f.p);
        if (clone) {
            li.QuadPart = 0; pos.QuadPart = 99;
            IStream_Seek(clone, li, STREAM_SEEK_CUR, &pos);
            XEQ(pos.QuadPart, 4);                                /* the clone starts at the original's position */
            n = 0; IStream_Read(clone, buf, 2, &n);              XEQ(n, 2);
            pos.QuadPart = 99; li.QuadPart = 0; IStream_Seek(f.p, li, STREAM_SEEK_CUR, &pos);
            XEQ(pos.QuadPart, 4);                                /* ... and has its own seek pointer */
            IStream_Write(clone, "XY", 2, NULL);                 /* data is shared */
            li.QuadPart = 6; IStream_Seek(f.p, li, STREAM_SEEK_SET, NULL);
            n = 0; memset(buf, 0, sizeof buf); IStream_Read(f.p, buf, 2, &n);
            XEQ(n, 2); CHECK(!memcmp(buf, "XY", 2));
            IStream_Release(clone);
        }
        /* CopyTo into a second proxied stream: the destination proxy travels back to the first host */
        li.QuadPart = 2; IStream_Seek(f.p, li, STREAM_SEEK_SET, NULL);
        cb.QuadPart = 5; rd.QuadPart = wr.QuadPart = 0xFF;
        hr = IStream_CopyTo(f.p, g.p, cb, &rd, &wr);
        XEQ(hr, S_OK); XEQ(rd.QuadPart, 5); XEQ(wr.QuadPart, 5);
        IStream_Stat(g.s, &st, STATFLAG_NONAME);                 XEQ(st.cbSize.QuadPart, 5);
        li.QuadPart = 0; IStream_Seek(g.s, li, STREAM_SEEK_SET, NULL);
        n = 0; memset(buf, 0, sizeof buf); IStream_Read(g.s, buf, 5, &n);
        XEQ(n, 5); CHECK(!memcmp(buf, "2345X", 5) || !memcmp(buf, "2345", 4));
    } else CHECK(0);
    sfx_teardown(&g); sfx_teardown(&f); UNINIT();
}
XT(ps_stream_query_interface) {
    SFx f; IUnknown *u = NULL; void *pv = (void *)1; HRESULT hr; IStream *s2 = NULL;
    XP_NEED_BASE(); NEED(CreateStreamOnHGlobal); INIT(COINIT_APARTMENTTHREADED);
    if (sfx_setup(&f, COINIT_APARTMENTTHREADED)) {
        hr = IStream_QueryInterface(f.p, &XP_IID_IUnknown, (void **)&u);       XEQ(hr, S_OK);
        if (u) { hr = IUnknown_QueryInterface(u, &XP_IID_IStream, (void **)&s2); XEQ(hr, S_OK); if (s2) IStream_Release(s2); IUnknown_Release(u); }
        hr = IStream_QueryInterface(f.p, &XP_IID_IStorage, &pv);               XEQ(hr, E_NOINTERFACE); CHECK(pv == NULL);
        hr = IStream_QueryInterface(f.p, &XP_IID_IUnknownBogus, &pv);          XEQ(hr, E_NOINTERFACE);
        pv = NULL; hr = IStream_QueryInterface(f.p, &XP_IID_ISequentialStream, &pv);
        t_note("IStream proxy QueryInterface(ISequentialStream): %08lx", hr);
        if (pv) IUnknown_Release((IUnknown *)pv);
        pv = NULL; hr = IStream_QueryInterface(f.p, &XP_IID_IMarshal, &pv);
        XEQ(hr, S_OK);                                                          /* proxy manager answers IMarshal locally */
        if (pv) IUnknown_Release((IUnknown *)pv);
    } else CHECK(0);
    sfx_teardown(&f); UNINIT();
}

/* ================================ IEnumString ====================================================== */
typedef struct { IEnumString iface; LONG ref; ULONG pos; } XEnum;
static const WCHAR *g_strs[] = { L"alpha", L"beta", L"gamma", L"delta", L"epsilon" };
#define NSTR 5
static XEnum *XE(IEnumString *e) { return (XEnum *)e; }
static HRESULT WINAPI en_qi(IEnumString *e, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_IEnumString)) { *ppv = e; InterlockedIncrement(&XE(e)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI en_addref(IEnumString *e) { return InterlockedIncrement(&XE(e)->ref); }
static ULONG WINAPI en_release(IEnumString *e) { LONG r = InterlockedDecrement(&XE(e)->ref); if (!r) HeapFree(GetProcessHeap(), 0, e); return r; }
static HRESULT WINAPI en_next(IEnumString *e, ULONG celt, LPOLESTR *rgelt, ULONG *pf)
{
    XEnum *x = XE(e); ULONG n = 0;
    while (n < celt && x->pos < NSTR) {
        size_t len = (lstrlenW(g_strs[x->pos]) + 1) * sizeof(WCHAR);
        rgelt[n] = U(CoTaskMemAlloc)(len); memcpy(rgelt[n], g_strs[x->pos], len);
        n++; x->pos++;
    }
    if (pf) *pf = n;
    return n == celt ? S_OK : S_FALSE;
}
static HRESULT WINAPI en_skip(IEnumString *e, ULONG celt) { XEnum *x = XE(e); if (x->pos + celt > NSTR) { x->pos = NSTR; return S_FALSE; } x->pos += celt; return S_OK; }
static HRESULT WINAPI en_reset(IEnumString *e) { XE(e)->pos = 0; return S_OK; }
static XEnum *en_new(void);
static HRESULT WINAPI en_clone(IEnumString *e, IEnumString **out) { XEnum *n = en_new(); n->pos = XE(e)->pos; *out = &n->iface; return S_OK; }
static IEnumStringVtbl en_vtbl = { en_qi, en_addref, en_release, en_next, en_skip, en_reset, en_clone };
static XEnum *en_new(void) { XEnum *x = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *x); x->iface.lpVtbl = &en_vtbl; x->ref = 1; return x; }

XT(ps_enumstring_protocol_through_proxy) {
    XpHost h; XEnum *e; IEnumString *p = NULL, *c = NULL; LPOLESTR s[8]; ULONG n; HRESULT hr; int i;
    XP_NEED_BASE(); NEED(CoTaskMemAlloc); NEED(CoTaskMemFree); INIT(COINIT_APARTMENTTHREADED);
    e = en_new();
    if (xp_host_start(&h, COINIT_APARTMENTTHREADED, (IUnknown *)&e->iface, &XP_IID_IEnumString, MSHLFLAGS_NORMAL)) {
        hr = xp_host_unmarshal(&h, &XP_IID_IEnumString, (void **)&p); XEQ(hr, S_OK);
        if (p) {
            n = 99; memset(s, 0, sizeof s);
            hr = IEnumString_Next(p, 2, s, &n);                  XEQ(hr, S_OK); XEQ(n, 2);
            CHECK(s[0] && s[1] && !lstrcmpW(s[0], L"alpha") && !lstrcmpW(s[1], L"beta"));
            for (i = 0; i < 2; i++) if (s[i]) U(CoTaskMemFree)(s[i]);
            hr = IEnumString_Skip(p, 1);                         XEQ(hr, S_OK);
            hr = IEnumString_Clone(p, &c);                       XEQ(hr, S_OK);
            n = 99; memset(s, 0, sizeof s);
            hr = IEnumString_Next(p, 5, s, &n);                  XEQ(hr, S_FALSE); XEQ(n, 2);     /* short read: S_FALSE and the count */
            CHECK(s[0] && s[1] && !lstrcmpW(s[0], L"delta") && !lstrcmpW(s[1], L"epsilon"));
            for (i = 0; i < 2; i++) if (s[i]) U(CoTaskMemFree)(s[i]);
            n = 99; hr = IEnumString_Next(p, 1, s, &n);          XEQ(hr, S_FALSE); XEQ(n, 0);
            if (c) {                                             /* the clone kept the position it was cloned at */
                n = 99; memset(s, 0, sizeof s);
                hr = IEnumString_Next(c, 1, s, &n);              XEQ(hr, S_OK); XEQ(n, 1);
                CHECK(s[0] && !lstrcmpW(s[0], L"delta"));
                if (s[0]) U(CoTaskMemFree)(s[0]);
                IEnumString_Release(c);
            }
            hr = IEnumString_Reset(p);                           XEQ(hr, S_OK);
            hr = IEnumString_Skip(p, 10);                        XEQ(hr, S_FALSE);
            IEnumString_Reset(p);
            s[0] = NULL; hr = IEnumString_Next(p, 1, s, NULL);   XEQ(hr, S_OK);                   /* pceltFetched may be NULL for celt == 1 */
            CHECK(s[0] && !lstrcmpW(s[0], L"alpha"));
            if (s[0]) U(CoTaskMemFree)(s[0]);
            n = 99; hr = IEnumString_Next(p, 0, s, &n);          XEQ(hr, S_OK); XEQ(n, 0);
            IEnumString_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    XEQ(en_release(&e->iface), 0);                               /* the stub released the enumerator */
    UNINIT();
}
XT(ps_enumstring_unmarshal_wrong_iid) {
    XpHost h; XEnum *e; void *pv = (void *)1; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    e = en_new();
    if (xp_host_start(&h, COINIT_APARTMENTTHREADED, (IUnknown *)&e->iface, &XP_IID_IEnumString, MSHLFLAGS_NORMAL)) {
        hr = xp_host_unmarshal(&h, &XP_IID_IStream, &pv);
        CHECKF(FAILED(hr), "unmarshal as an unrelated interface returned %08lx", hr);
        t_note("unmarshal of an IEnumString OBJREF as IStream: %08lx (%s)", hr, xp_hrname(hr));
        CHECK(pv == NULL);
    } else CHECK(0);
    xp_host_stop(&h);
    en_release(&e->iface);
    UNINIT();
}

/* ================================ IDispatch ========================================================== */
typedef struct {
    IDispatch iface; LONG ref;
    LONG gets, invokes; GUID lastRiid; LCID lastLcid; WORD lastFlags; UINT lastCArgs, lastCNamed; DISPID lastNamed0; long putValue;
} XDisp;
static XDisp *XD(IDispatch *d) { return (XDisp *)d; }
static HRESULT WINAPI d_qi(IDispatch *d, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_IDispatch)) { *ppv = d; InterlockedIncrement(&XD(d)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI d_addref(IDispatch *d) { return InterlockedIncrement(&XD(d)->ref); }
static ULONG WINAPI d_release(IDispatch *d) { return InterlockedDecrement(&XD(d)->ref); }   /* static object */
static HRESULT WINAPI d_gtic(IDispatch *d, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI d_gti(IDispatch *d, UINT i, LCID l, ITypeInfo **ti) { *ti = NULL; return DISP_E_BADINDEX; }
static HRESULT WINAPI d_gids(IDispatch *d, REFIID riid, LPOLESTR *names, UINT n, LCID l, DISPID *ids)
{
    HRESULT hr = S_OK; UINT i;
    XD(d)->lastRiid = *riid; XD(d)->lastLcid = l;
    for (i = 0; i < n; i++) {
        if (!lstrcmpiW(names[i], L"Value")) ids[i] = 100;
        else if (!lstrcmpiW(names[i], L"Name")) ids[i] = 101;
        else if (!lstrcmpiW(names[i], L"Sub")) ids[i] = 102;
        else if (!lstrcmpiW(names[i], L"Boom")) ids[i] = 103;
        else if (!lstrcmpiW(names[i], L"Put")) ids[i] = 104;
        else if (!lstrcmpiW(names[i], L"Mismatch")) ids[i] = 105;
        else { ids[i] = DISPID_UNKNOWN; hr = DISP_E_UNKNOWNNAME; }
    }
    return hr;
}
static HRESULT WINAPI d_invoke(IDispatch *d, DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ex, UINT *argerr)
{
    XDisp *x = XD(d);
    InterlockedIncrement(&x->invokes);
    x->lastRiid = *riid; x->lastLcid = lcid; x->lastFlags = flags; x->lastCArgs = dp->cArgs; x->lastCNamed = dp->cNamedArgs;
    x->lastNamed0 = dp->cNamedArgs ? dp->rgdispidNamedArgs[0] : 0;
    switch (id) {
    case 100: if (!(flags & DISPATCH_PROPERTYGET)) return DISP_E_MEMBERNOTFOUND; if (res) { V_VT(res) = VT_I4; V_I4(res) = 12345; } return S_OK;
    case 101: if (res) { V_VT(res) = VT_BSTR; V_BSTR(res) = U(SysAllocString)(L"xp-name"); } return S_OK;
    case 102:
        if (dp->cArgs != 2) return DISP_E_BADPARAMCOUNT;
        if (V_VT(&dp->rgvarg[0]) != VT_I4) { if (argerr) *argerr = 0; return DISP_E_TYPEMISMATCH; }
        if (V_VT(&dp->rgvarg[1]) != VT_I4) { if (argerr) *argerr = 1; return DISP_E_TYPEMISMATCH; }
        if (res) { V_VT(res) = VT_I4; V_I4(res) = V_I4(&dp->rgvarg[1]) - V_I4(&dp->rgvarg[0]); }   /* rgvarg is in reverse order */
        return S_OK;
    case 103:
        if (ex) { memset(ex, 0, sizeof *ex); ex->wCode = 0; ex->scode = XP_CUSTOM_HR; ex->bstrSource = U(SysAllocString)(L"XpSource"); ex->bstrDescription = U(SysAllocString)(L"Boom!"); }
        return DISP_E_EXCEPTION;
    case 104:
        if (flags & DISPATCH_PROPERTYPUT) { if (dp->cArgs != 1) return DISP_E_BADPARAMCOUNT; x->putValue = V_I4(&dp->rgvarg[0]); return S_OK; }
        if (res) { V_VT(res) = VT_I4; V_I4(res) = x->putValue; }
        return S_OK;
    case 105: if (argerr) *argerr = 7; return DISP_E_TYPEMISMATCH;
    }
    return DISP_E_MEMBERNOTFOUND;
}
static IDispatchVtbl d_vtbl = { d_qi, d_addref, d_release, d_gtic, d_gti, d_gids, d_invoke };
static void disp_init(XDisp *d) { memset(d, 0, sizeof *d); d->iface.lpVtbl = &d_vtbl; d->ref = 1; }

typedef struct { XDisp d; XpHost h; IDispatch *p; } DFx;
static BOOL dfx_setup(DFx *f, DWORD hostModel)
{
    disp_init(&f->d); f->p = NULL;
    if (!xp_host_start(&f->h, hostModel, (IUnknown *)&f->d.iface, &XP_IID_IDispatch, MSHLFLAGS_NORMAL)) return FALSE;
    return SUCCEEDED(xp_host_unmarshal(&f->h, &XP_IID_IDispatch, (void **)&f->p)) && f->p;
}
static void dfx_teardown(DFx *f) { if (f->p) IDispatch_Release(f->p); xp_host_stop(&f->h); }

static void dispatch_basic(DWORD hostModel)
{
    DFx f; UINT n = 99; HRESULT hr; ITypeInfo *ti = (ITypeInfo *)1; DISPID ids[3]; LPOLESTR names[3]; DISPPARAMS dp; VARIANT res, args[2]; EXCEPINFO ex; UINT ae = 0xFF;
    GUID nil; memset(&nil, 0, sizeof nil);
    XP_NEED_BASE(); NEED(SysAllocString); NEED(SysFreeString); NEED(VariantClear); NEED(VariantInit); INIT(COINIT_APARTMENTTHREADED);
    if (dfx_setup(&f, hostModel)) {
        hr = IDispatch_GetTypeInfoCount(f.p, &n);                XEQ(hr, S_OK); XEQ(n, 0);
        hr = IDispatch_GetTypeInfo(f.p, 0, 0x409, &ti);          XEQ(hr, DISP_E_BADINDEX); CHECK(ti == NULL);
        names[0] = L"Value"; names[1] = L"Name";
        hr = IDispatch_GetIDsOfNames(f.p, &nil, names, 2, 0x409, ids);
        XEQ(hr, S_OK); XEQ(ids[0], 100); XEQ(ids[1], 101);
        XEQ(f.d.lastLcid, 0x409);
        CHECK(IsEqualGUID(&f.d.lastRiid, &nil));
        names[0] = L"Sub"; names[1] = L"NoSuchMember"; names[2] = L"Value";
        hr = IDispatch_GetIDsOfNames(f.p, &nil, names, 3, 0x407, ids);
        XEQ(hr, DISP_E_UNKNOWNNAME);                              /* partial success is reported through the HRESULT */
        XEQ(ids[0], 102); XEQ(ids[1], DISPID_UNKNOWN); XEQ(ids[2], 100);
        XEQ(f.d.lastLcid, 0x407);
        memset(&dp, 0, sizeof dp);
        U(VariantInit)(&res); V_VT(&res) = VT_ERROR;
        hr = IDispatch_Invoke(f.p, 100, &nil, 0x409, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
        XEQ(hr, S_OK); XEQ(V_VT(&res), VT_I4); XEQ(V_I4(&res), 12345);
        XEQ(f.d.lastFlags, DISPATCH_PROPERTYGET); XEQ(f.d.lastCArgs, 0);
        CHECK(IsEqualGUID(&f.d.lastRiid, &nil));
        U(VariantClear)(&res);
        U(VariantInit)(&res);
        hr = IDispatch_Invoke(f.p, 101, &nil, 0x409, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
        XEQ(hr, S_OK); XEQ(V_VT(&res), VT_BSTR);
        CHECK(V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"xp-name"));
        U(VariantClear)(&res);
        /* arguments are passed in reverse order */
        U(VariantInit)(&args[0]); U(VariantInit)(&args[1]);
        V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 3; V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 10;
        dp.rgvarg = args; dp.cArgs = 2;
        U(VariantInit)(&res);
        hr = IDispatch_Invoke(f.p, 102, &nil, 0x409, DISPATCH_METHOD, &dp, &res, NULL, &ae);
        XEQ(hr, S_OK); XEQ(V_VT(&res), VT_I4); XEQ(V_I4(&res), 7);
        XEQ(f.d.lastCArgs, 2);
        /* type mismatch reports the offending argument index */
        V_VT(&args[1]) = VT_R8; V_R8(&args[1]) = 1.0; ae = 0xFF;
        hr = IDispatch_Invoke(f.p, 102, &nil, 0x409, DISPATCH_METHOD, &dp, &res, NULL, &ae);
        XEQ(hr, DISP_E_TYPEMISMATCH); XEQ(ae, 1);
        ae = 0xFF; memset(&dp, 0, sizeof dp);
        hr = IDispatch_Invoke(f.p, 105, &nil, 0x409, DISPATCH_METHOD, &dp, &res, NULL, &ae);
        XEQ(hr, DISP_E_TYPEMISMATCH); XEQ(ae, 7);
        hr = IDispatch_Invoke(f.p, 999, &nil, 0x409, DISPATCH_METHOD, &dp, NULL, NULL, NULL);
        XEQ(hr, DISP_E_MEMBERNOTFOUND);
        /* exception information crosses the apartment */
        memset(&ex, 0xCC, sizeof ex);
        hr = IDispatch_Invoke(f.p, 103, &nil, 0x409, DISPATCH_METHOD, &dp, NULL, &ex, NULL);
        XEQ(hr, DISP_E_EXCEPTION);
        XEQ(ex.scode, XP_CUSTOM_HR);
        CHECK(ex.bstrSource && !lstrcmpW(ex.bstrSource, L"XpSource"));
        CHECK(ex.bstrDescription && !lstrcmpW(ex.bstrDescription, L"Boom!"));
        if (ex.bstrSource) U(SysFreeString)(ex.bstrSource);
        if (ex.bstrDescription) U(SysFreeString)(ex.bstrDescription);
        if (ex.bstrHelpFile) U(SysFreeString)(ex.bstrHelpFile);
        /* named argument: property put */
        {
            DISPID put = DISPID_PROPERTYPUT; VARIANT v;
            U(VariantInit)(&v); V_VT(&v) = VT_I4; V_I4(&v) = 77;
            memset(&dp, 0, sizeof dp); dp.rgvarg = &v; dp.cArgs = 1; dp.rgdispidNamedArgs = &put; dp.cNamedArgs = 1;
            hr = IDispatch_Invoke(f.p, 104, &nil, 0x409, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
            XEQ(hr, S_OK); XEQ(f.d.putValue, 77); XEQ(f.d.lastCNamed, 1); XEQ(f.d.lastNamed0, DISPID_PROPERTYPUT);
            memset(&dp, 0, sizeof dp); U(VariantInit)(&res);
            hr = IDispatch_Invoke(f.p, 104, &nil, 0x409, DISPATCH_PROPERTYGET, &dp, &res, NULL, NULL);
            XEQ(hr, S_OK); XEQ(V_I4(&res), 77);
        }
    } else CHECK(0);
    dfx_teardown(&f); UNINIT();
}
XT(ps_dispatch_calls_sta_host) { dispatch_basic(COINIT_APARTMENTTHREADED); }
XT(ps_dispatch_calls_mta_host) { dispatch_basic(COINIT_MULTITHREADED); }

XT(ps_dispatch_variant_argument_types) {
    DFx f; VARIANT res, args[2]; DISPPARAMS dp; GUID nil; HRESULT hr;
    memset(&nil, 0, sizeof nil);
    XP_NEED_BASE(); NEED(SysAllocString); NEED(VariantClear); NEED(VariantInit); INIT(COINIT_APARTMENTTHREADED);
    if (dfx_setup(&f, COINIT_APARTMENTTHREADED)) {
        /* Sub(10, 3) with the first argument passed as VT_I4 and the second as a VT_BYREF|VT_I4: byref is a mismatch for the server */
        long byref = 5;
        U(VariantInit)(&args[0]); U(VariantInit)(&args[1]);
        V_VT(&args[0]) = VT_I4 | VT_BYREF; V_I4REF(&args[0]) = &byref;
        V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 9;
        memset(&dp, 0, sizeof dp); dp.rgvarg = args; dp.cArgs = 2;
        U(VariantInit)(&res);
        hr = IDispatch_Invoke(f.p, 102, &nil, 0x409, DISPATCH_METHOD, &dp, &res, NULL, NULL);
        XEQ(hr, DISP_E_TYPEMISMATCH);
        XEQ(f.d.lastCArgs, 2);
        /* a BSTR argument arrives as a BSTR with the same contents */
        V_VT(&args[0]) = VT_BSTR; V_BSTR(&args[0]) = U(SysAllocString)(L"text");
        hr = IDispatch_Invoke(f.p, 102, &nil, 0x409, DISPATCH_METHOD, &dp, &res, NULL, NULL);
        XEQ(hr, DISP_E_TYPEMISMATCH);
        U(VariantClear)(&args[0]);
    } else CHECK(0);
    dfx_teardown(&f); UNINIT();
}
XT(ps_dispatch_qi_through_unknown) {
    XpHost h; XDisp d; IUnknown *u = NULL; IDispatch *p = NULL; void *pv = (void *)1; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    disp_init(&d);
    if (xp_host_start(&h, COINIT_APARTMENTTHREADED, (IUnknown *)&d.iface, &XP_IID_IDispatch, MSHLFLAGS_NORMAL)) {
        hr = xp_host_unmarshal(&h, &XP_IID_IUnknown, (void **)&u); XEQ(hr, S_OK);
        if (u) {
            hr = IUnknown_QueryInterface(u, &XP_IID_IDispatch, (void **)&p); XEQ(hr, S_OK);
            hr = IUnknown_QueryInterface(u, &XP_IID_IUnknownBogus, &pv);     XEQ(hr, E_NOINTERFACE); CHECK(pv == NULL);
            if (p) { UINT n = 9; hr = IDispatch_GetTypeInfoCount(p, &n); XEQ(hr, S_OK); XEQ(n, 0); IDispatch_Release(p); }
            IUnknown_Release(u);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    UNINIT();
}

/* ================================ IEnumVARIANT ======================================================== */
typedef struct { IEnumVARIANT iface; LONG ref; ULONG pos; } XEV;
static XEV *EV(IEnumVARIANT *e) { return (XEV *)e; }
static HRESULT WINAPI ev_qi(IEnumVARIANT *e, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_IEnumVARIANT)) { *ppv = e; InterlockedIncrement(&EV(e)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ev_addref(IEnumVARIANT *e) { return InterlockedIncrement(&EV(e)->ref); }
static ULONG WINAPI ev_release(IEnumVARIANT *e) { LONG r = InterlockedDecrement(&EV(e)->ref); if (!r) HeapFree(GetProcessHeap(), 0, e); return r; }
static void ev_fill(ULONG i, VARIANT *v)
{
    U(VariantInit)(v);
    switch (i) {
    case 0: V_VT(v) = VT_I4; V_I4(v) = 7; break;
    case 1: V_VT(v) = VT_BSTR; V_BSTR(v) = U(SysAllocString)(L"xp"); break;
    case 2: V_VT(v) = VT_R8; V_R8(v) = 2.5; break;
    case 3: V_VT(v) = VT_BOOL; V_BOOL(v) = VARIANT_TRUE; break;
    default: V_VT(v) = VT_EMPTY; break;
    }
}
static HRESULT WINAPI ev_next(IEnumVARIANT *e, ULONG celt, VARIANT *rg, ULONG *pf)
{
    XEV *x = EV(e); ULONG n = 0;
    while (n < celt && x->pos < 5) { ev_fill(x->pos, &rg[n]); n++; x->pos++; }
    if (pf) *pf = n;
    return n == celt ? S_OK : S_FALSE;
}
static HRESULT WINAPI ev_skip(IEnumVARIANT *e, ULONG celt) { XEV *x = EV(e); if (x->pos + celt > 5) { x->pos = 5; return S_FALSE; } x->pos += celt; return S_OK; }
static HRESULT WINAPI ev_reset(IEnumVARIANT *e) { EV(e)->pos = 0; return S_OK; }
static XEV *ev_new(void);
static HRESULT WINAPI ev_clone(IEnumVARIANT *e, IEnumVARIANT **o) { XEV *n = ev_new(); n->pos = EV(e)->pos; *o = &n->iface; return S_OK; }
static IEnumVARIANTVtbl ev_vtbl = { ev_qi, ev_addref, ev_release, ev_next, ev_skip, ev_reset, ev_clone };
static XEV *ev_new(void) { XEV *x = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *x); x->iface.lpVtbl = &ev_vtbl; x->ref = 1; return x; }

XT(ps_enumvariant_through_proxy) {
    XpHost h; XEV *e; IEnumVARIANT *p = NULL, *c = NULL; VARIANT v[6]; ULONG n; HRESULT hr; int i;
    XP_NEED_BASE(); NEED(SysAllocString); NEED(VariantClear); NEED(VariantInit); INIT(COINIT_APARTMENTTHREADED);
    e = ev_new();
    if (xp_host_start(&h, COINIT_APARTMENTTHREADED, (IUnknown *)&e->iface, &XP_IID_IEnumVARIANT, MSHLFLAGS_NORMAL)) {
        hr = xp_host_unmarshal(&h, &XP_IID_IEnumVARIANT, (void **)&p);
        XEQ(hr, S_OK);
        if (p) {
            for (i = 0; i < 6; i++) U(VariantInit)(&v[i]);
            n = 99; hr = IEnumVARIANT_Next(p, 4, v, &n);        XEQ(hr, S_OK); XEQ(n, 4);
            XEQ(V_VT(&v[0]), VT_I4); XEQ(V_I4(&v[0]), 7);
            XEQ(V_VT(&v[1]), VT_BSTR); CHECK(V_BSTR(&v[1]) && !lstrcmpW(V_BSTR(&v[1]), L"xp"));
            XEQ(V_VT(&v[2]), VT_R8); CHECK(V_R8(&v[2]) == 2.5);
            XEQ(V_VT(&v[3]), VT_BOOL); XEQ(V_BOOL(&v[3]), VARIANT_TRUE);
            for (i = 0; i < 4; i++) U(VariantClear)(&v[i]);
            hr = IEnumVARIANT_Clone(p, &c);                     XEQ(hr, S_OK);
            n = 99; hr = IEnumVARIANT_Next(p, 3, v, &n);        XEQ(hr, S_FALSE); XEQ(n, 1); XEQ(V_VT(&v[0]), VT_EMPTY);
            U(VariantClear)(&v[0]);
            if (c) { n = 99; hr = IEnumVARIANT_Next(c, 1, v, &n); XEQ(hr, S_OK); XEQ(n, 1); XEQ(V_VT(&v[0]), VT_EMPTY); IEnumVARIANT_Release(c); }
            hr = IEnumVARIANT_Reset(p);                         XEQ(hr, S_OK);
            hr = IEnumVARIANT_Skip(p, 2);                       XEQ(hr, S_OK);
            n = 99; U(VariantInit)(&v[0]);
            hr = IEnumVARIANT_Next(p, 1, v, &n);                XEQ(hr, S_OK); XEQ(V_VT(&v[0]), VT_R8);
            hr = IEnumVARIANT_Skip(p, 9);                       XEQ(hr, S_FALSE);
            IEnumVARIANT_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    XEQ(ev_release(&e->iface), 0);
    UNINIT();
}

/* ================================ IStorage / IEnumSTATSTG ============================================== */
XT(ps_storage_through_proxy) {
    IStorage *stg = NULL, *p = NULL, *sub = NULL; IStream *st = NULL, *st2 = NULL; XpHost h; HRESULT hr; STATSTG ss; IEnumSTATSTG *en = NULL;
    GUID clsid = { 0xC0DE0003, 0x7E57, 0x4A11, { 0x9B, 0x01, 0x0F, 0x7E, 0x57, 0x00, 0x00, 0x03 } }; ULONG n; char buf[8]; LARGE_INTEGER li;
    XP_NEED_BASE(); NEED(StgCreateDocfile); INIT(COINIT_APARTMENTTHREADED);
    hr = U(StgCreateDocfile)(NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg);
    if (FAILED(hr)) { t_skip("StgCreateDocfile failed %08lx", hr); UNINIT(); return; }
    if (xp_host_start(&h, COINIT_APARTMENTTHREADED, (IUnknown *)stg, &XP_IID_IStorage, MSHLFLAGS_NORMAL)) {
        hr = xp_host_unmarshal(&h, &XP_IID_IStorage, (void **)&p); XEQ(hr, S_OK);
        if (p) {
            WCHAR s1[] = L"s1", sn[] = L"sub", missing[] = L"missing";
            hr = IStorage_CreateStream(p, s1, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &st);
            XEQ(hr, S_OK);
            if (st) {
                hr = IStream_Write(st, "payload", 7, &n);        XEQ(hr, S_OK); XEQ(n, 7);
                IStream_Release(st); st = NULL;
            }
            st2 = (IStream *)1;
            hr = IStorage_CreateStream(p, s1, STGM_FAILIFTHERE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &st2);
            XEQ(hr, STG_E_FILEALREADYEXISTS); CHECK(st2 == NULL);
            st2 = (IStream *)1;
            hr = IStorage_OpenStream(p, missing, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &st2);
            XEQ(hr, STG_E_FILENOTFOUND); CHECK(st2 == NULL);
            hr = IStorage_OpenStream(p, s1, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &st2);
            XEQ(hr, S_OK);
            if (st2) {
                memset(buf, 0, sizeof buf); n = 0;
                hr = IStream_Read(st2, buf, 7, &n);              XEQ(hr, S_OK); XEQ(n, 7); CHECK(!memcmp(buf, "payload", 7));
                hr = IStream_Write(st2, "x", 1, &n);             CHECKF(FAILED(hr), "write to a read-only stream returned %08lx", hr);
                IStream_Release(st2);
            }
            hr = IStorage_CreateStorage(p, sn, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &sub);
            XEQ(hr, S_OK);
            if (sub) {
                hr = IStorage_SetClass(sub, &clsid);             XEQ(hr, S_OK);
                memset(&ss, 0, sizeof ss);
                hr = IStorage_Stat(sub, &ss, STATFLAG_NONAME);   XEQ(hr, S_OK);
                XEQ(ss.type, STGTY_STORAGE); CHECK(IsEqualGUID(&ss.clsid, &clsid));
                IStorage_Release(sub);
            }
            hr = IStorage_Commit(p, STGC_DEFAULT);               XEQ(hr, S_OK);
            hr = IStorage_EnumElements(p, 0, NULL, 0, &en);      XEQ(hr, S_OK);
            if (en) {
                int seenStream = 0, seenStg = 0; STATSTG e[4]; ULONG got = 0;
                memset(e, 0, sizeof e);
                hr = IEnumSTATSTG_Next(en, 4, e, &got);          XEQ(hr, S_FALSE); XEQ(got, 2);
                for (ULONG i = 0; i < got; i++) {
                    if (e[i].pwcsName && !lstrcmpW(e[i].pwcsName, L"s1")) { seenStream = 1; XEQ(e[i].type, STGTY_STREAM); XEQ(e[i].cbSize.QuadPart, 7); }
                    if (e[i].pwcsName && !lstrcmpW(e[i].pwcsName, L"sub")) { seenStg = 1; XEQ(e[i].type, STGTY_STORAGE); }
                    if (e[i].pwcsName) U(CoTaskMemFree)(e[i].pwcsName);
                }
                CHECK(seenStream && seenStg);
                hr = IEnumSTATSTG_Reset(en);                     XEQ(hr, S_OK);
                hr = IEnumSTATSTG_Skip(en, 1);                   XEQ(hr, S_OK);
                got = 99; hr = IEnumSTATSTG_Next(en, 1, e, &got); XEQ(hr, S_OK); XEQ(got, 1);
                if (e[0].pwcsName) U(CoTaskMemFree)(e[0].pwcsName);
                got = 99; hr = IEnumSTATSTG_Next(en, 1, e, &got); XEQ(hr, S_FALSE); XEQ(got, 0);
                IEnumSTATSTG_Release(en);
            }
            hr = IStorage_DestroyElement(p, s1);                 XEQ(hr, S_OK);
            hr = IStorage_DestroyElement(p, s1);                 XEQ(hr, STG_E_FILENOTFOUND);
            sub = (IStorage *)1;
            hr = IStorage_OpenStorage(p, missing, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &sub);
            XEQ(hr, STG_E_FILENOTFOUND); CHECK(sub == NULL);
            { void *pv = (void *)1; hr = IStorage_QueryInterface(p, &XP_IID_IStream, &pv); XEQ(hr, E_NOINTERFACE); CHECK(pv == NULL); }
            IStorage_Release(p);
        }
    } else CHECK(0);
    xp_host_stop(&h);
    IStorage_Release(stg);
    UNINIT();
}
