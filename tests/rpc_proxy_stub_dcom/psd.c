/* Shared implementation for the rpc_proxy_stub_dcom area: GUIDs, server object, loopback channel,
 * hand-built ProxyFileInfo (IOletOne / IOletTwo) and the exception catcher. */
#include "psd.h"
#include "psd_fmt.h"
#include <setjmp.h>

#define G(n, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) const GUID n = {a, b, c, {d0, d1, d2, d3, d4, d5, d6, d7}}
G(psd_IID_IUnknown, 0x00000000, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_IClassFactory, 0x00000001, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_IMarshal, 0x00000003, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_IStream, 0x0000000C, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_IDispatch, 0x00020400, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_ITypeInfo, 0x00020401, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_ICallFactory, 0x1C733A30, 0x2A1C, 0x11CE, 0xAD, 0xE5, 0x00, 0xAA, 0x00, 0x44, 0x77, 0x3D);
G(psd_IID_IPSFactoryBuffer, 0xD5F569D0, 0x593B, 0x101A, 0xB5, 0x69, 0x08, 0x00, 0x2B, 0x2D, 0xBF, 0x7A);
G(psd_IID_IRpcChannelBuffer, 0xD5F56B60, 0x593B, 0x101A, 0xB5, 0x69, 0x08, 0x00, 0x2B, 0x2D, 0xBF, 0x7A);
G(psd_IID_IRpcProxyBuffer, 0xD5F56A34, 0x593B, 0x101A, 0xB5, 0x69, 0x08, 0x00, 0x2B, 0x2D, 0xBF, 0x7A);
G(psd_IID_IRpcStubBuffer, 0xD5F56AFC, 0x593B, 0x101A, 0xB5, 0x69, 0x08, 0x00, 0x2B, 0x2D, 0xBF, 0x7A);
G(psd_IID_IReleaseMarshalBuffers, 0xEB0CB9E8, 0x7996, 0x11D2, 0x87, 0x2E, 0x00, 0x00, 0xF8, 0x08, 0x08, 0x00);
G(psd_IID_IRpcChannelBuffer2, 0x594F31D0, 0x7F19, 0x11D0, 0xB1, 0x94, 0x00, 0xA0, 0xC9, 0x0D, 0xC8, 0xBF);
G(psd_IID_IRpcChannelBuffer3, 0x25B15600, 0x0115, 0x11D0, 0xBF, 0x0D, 0x00, 0xAA, 0x00, 0xB8, 0xDF, 0xD2);
G(psd_IID_IMultiQI, 0x00000020, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_IClientSecurity, 0x0000013D, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_IStdIdentity, 0x0000001B, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
G(psd_IID_One, 0x6f7a0001, 0x4a5b, 0x4c6d, 0x8e, 0x7f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f);
G(psd_IID_Two, 0x6f7a0002, 0x4a5b, 0x4c6d, 0x8e, 0x7f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f);
G(psd_IID_Unknown1, 0x6f7a0003, 0x4a5b, 0x4c6d, 0x8e, 0x7f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f);
G(psd_CLSID_PSF, 0x6f7a00f0, 0x4a5b, 0x4c6d, 0x8e, 0x7f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f);
G(psd_CLSID_Bogus, 0x6f7a00f1, 0x4a5b, 0x4c6d, 0x8e, 0x7f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f);

static BOOL guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof *a); }

/* ===================== server object ===================================================== */
static PsdSrv *from_one(IOletOne *p) { return (PsdSrv *)((char *)p - offsetof(PsdSrv, one)); }
static PsdSrv *from_two(IOletTwo *p) { return (PsdSrv *)((char *)p - offsetof(PsdSrv, two)); }

static HRESULT srv_qi(PsdSrv *s, REFIID riid, void **ppv) {
    s->n_qi++;
    *ppv = NULL;
    if (guid_eq(riid, &psd_IID_IUnknown) || guid_eq(riid, &psd_IID_One)) *ppv = &s->one;
    else if (guid_eq(riid, &psd_IID_Two)) *ppv = &s->two;
    else return E_NOINTERFACE;
    InterlockedIncrement(&s->ref);
    return S_OK;
}
static HRESULT WINAPI one_qi(IOletOne *p, REFIID r, void **v) { return srv_qi(from_one(p), r, v); }
static ULONG WINAPI one_addref(IOletOne *p) { return InterlockedIncrement(&from_one(p)->ref); }
static ULONG WINAPI one_release(IOletOne *p) {
    PsdSrv *s = from_one(p);
    LONG n = InterlockedDecrement(&s->ref);
    if (!n) HeapFree(GetProcessHeap(), 0, s);
    return n;
}
static HRESULT WINAPI one_add(IOletOne *p, LONG a, LONG b, LONG *r) {
    PsdSrv *s = from_one(p);
    s->n_add++; s->last_a = a; s->last_b = b; s->last_tid = GetCurrentThreadId();
    if (s->raise_in_add && a == 0x7fffdead) RaiseException(0xE0001234, 0, 0, NULL);
    if (b == 0x5a5a5a5a) return E_FAIL;           /* method failure: [out] values are still returned */
    *r = a + b;
    return S_OK;
}
static const IOletOneVtbl one_vtbl = { one_qi, one_addref, one_release, one_add };

static HRESULT WINAPI two_qi(IOletTwo *p, REFIID r, void **v) { return srv_qi(from_two(p), r, v); }
static ULONG WINAPI two_addref(IOletTwo *p) { return InterlockedIncrement(&from_two(p)->ref); }
static ULONG WINAPI two_release(IOletTwo *p) { return one_release(&from_two(p)->one); }
static HRESULT WINAPI two_echo(IOletTwo *p, LONG a, LONG *r) {
    PsdSrv *s = from_two(p);
    s->n_echo++; s->last_a = a; s->last_tid = GetCurrentThreadId();
    *r = a;
    return S_OK;
}
static HRESULT WINAPI two_str(IOletTwo *p, const WCHAR *str, LONG *len) {
    PsdSrv *s = from_two(p);
    s->n_str++;
    s->last_strlen = (LONG)lstrlenW(str);
    lstrcpynW(s->last_str, str, 64);
    *len = s->last_strlen;
    return S_OK;
}
static HRESULT WINAPI two_getobj(IOletTwo *p, REFIID riid, IUnknown **pp) {
    PsdSrv *s = from_two(p);
    s->n_getobj++;
    return srv_qi(s, riid, (void **)pp);
}
static HRESULT WINAPI two_putobj(IOletTwo *p, IOletOne *o) {
    PsdSrv *s = from_two(p);
    s->n_putobj++;
    s->last_put = o;
    return S_OK;
}
static const IOletTwoVtbl two_vtbl = { two_qi, two_addref, two_release, two_echo, two_str, two_getobj, two_putobj };

PsdSrv *psd_srv_new(void) {
    PsdSrv *s = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *s);
    s->one.lpVtbl = &one_vtbl;
    s->two.lpVtbl = &two_vtbl;
    s->ref = 1;
    return s;
}

/* ===================== mock IUnknown ===================================================== */
static PsdMock *mock_of(IUnknown *p) { return (PsdMock *)p; }
static HRESULT WINAPI mk_qi(IUnknown *p, REFIID riid, void **ppv) {
    PsdMock *m = mock_of(p);
    m->n_qi++;
    m->last_iid = *riid;
    if (SUCCEEDED(m->qi_hr) && m->qi_self) { *ppv = p; InterlockedIncrement(&m->ref); m->n_addref++; }
    else *ppv = m->qi_out;
    return m->qi_hr;
}
static ULONG WINAPI mk_addref(IUnknown *p) { PsdMock *m = mock_of(p); m->n_addref++; return InterlockedIncrement(&m->ref); }
static ULONG WINAPI mk_release(IUnknown *p) { PsdMock *m = mock_of(p); m->n_release++; return InterlockedDecrement(&m->ref); }
static const IUnknownVtbl mock_vtbl = { mk_qi, mk_addref, mk_release };
PsdMock *psd_mock_new(void) {
    PsdMock *m = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *m);
    m->iface.lpVtbl = (IUnknownVtbl *)&mock_vtbl;
    m->ref = 1; m->qi_hr = S_OK; m->qi_self = 1;
    return m;
}
void psd_mock_free(PsdMock *m) { HeapFree(GetProcessHeap(), 0, m); }

/* ===================== loopback channel ================================================== */
static PsdChan *chan_of(IRpcChannelBuffer *p) { return (PsdChan *)p; }
static HRESULT WINAPI ch_qi(IRpcChannelBuffer *p, REFIID riid, void **ppv) {
    PsdChan *c = chan_of(p);
    c->n_qi++;
    *ppv = NULL;
    if (FAILED(c->hr_qi)) return c->hr_qi;
    if (guid_eq(riid, &psd_IID_IUnknown) || guid_eq(riid, &psd_IID_IRpcChannelBuffer)) {
        *ppv = p; InterlockedIncrement(&c->ref); return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI ch_addref(IRpcChannelBuffer *p) { return InterlockedIncrement(&chan_of(p)->ref); }
static ULONG WINAPI ch_release(IRpcChannelBuffer *p) {
    PsdChan *c = chan_of(p);
    LONG n = InterlockedDecrement(&c->ref);
    if (n == 0) { c->ref = -1000; /* keep the memory: tests inspect counters after the last release */ }
    return n;
}
static HRESULT WINAPI ch_getbuffer(IRpcChannelBuffer *p, RPCOLEMESSAGE *m, REFIID iid) {
    PsdChan *c = chan_of(p);
    c->n_getbuf++;
    if (c->ngb < 8) {
        PsdMsgLog *l = &c->gb[c->ngb++];
        l->cb = m->cbBuffer; l->iMethod = m->iMethod; l->rep = m->dataRepresentation; l->rpcFlags = m->rpcFlags;
        l->iid = *iid; l->server = c->depth > 0;
    }
    if (FAILED(c->hr_getbuf)) return c->hr_getbuf;
    ULONG n = m->cbBuffer < 8 ? 8 : m->cbBuffer;
    void *b = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + 16);
    if (!b) return E_OUTOFMEMORY;
    if (c->depth > 0 && c->fill_server_reply_ff) memset(b, 0xff, n);
    m->Buffer = b;
    InterlockedIncrement(&c->live_buffers);
    return S_OK;
}
static HRESULT WINAPI ch_sendreceive(IRpcChannelBuffer *p, RPCOLEMESSAGE *m, ULONG *status) {
    PsdChan *c = chan_of(p);
    c->n_sendrecv++;
    c->req_len = m->cbBuffer;
    memcpy(c->req, m->Buffer, m->cbBuffer < PSD_LOG ? m->cbBuffer : PSD_LOG);
    if (FAILED(c->hr_sendrecv)) return c->hr_sendrecv;
    if (!c->stub) return E_UNEXPECTED;
    RPCOLEMESSAGE sm = *m;
    sm.iMethod &= ~0x8000u;   /* the real channel strips RPC_FLAGS_VALID_BIT before dispatching to the stub */
    c->last_raw_imethod = m->iMethod;
    void *req = m->Buffer;
    c->depth++;
    HRESULT hr = c->stub->lpVtbl->Invoke(c->stub, &sm, &c->iface);
    c->depth--;
    c->last_invoke_hr = hr;
    HeapFree(GetProcessHeap(), 0, req);
    InterlockedDecrement(&c->live_buffers);
    m->Buffer = sm.Buffer;
    m->cbBuffer = sm.cbBuffer;
    c->rep_len = sm.cbBuffer;
    if (sm.Buffer) memcpy(c->rep, sm.Buffer, sm.cbBuffer < PSD_LOG ? sm.cbBuffer : PSD_LOG);
    if (status) *status = c->status;
    return hr;
}
static HRESULT WINAPI ch_freebuffer(IRpcChannelBuffer *p, RPCOLEMESSAGE *m) {
    PsdChan *c = chan_of(p);
    c->n_freebuf++;
    if (FAILED(c->hr_free)) return c->hr_free;
    if (m->Buffer) { HeapFree(GetProcessHeap(), 0, m->Buffer); m->Buffer = NULL; InterlockedDecrement(&c->live_buffers); }
    return S_OK;
}
static HRESULT WINAPI ch_getdestctx(IRpcChannelBuffer *p, DWORD *ctx, void **pv) {
    PsdChan *c = chan_of(p);
    c->n_destctx++;
    if (FAILED(c->hr_destctx)) return c->hr_destctx;
    *ctx = c->destctx;
    *pv = NULL;
    return S_OK;
}
static HRESULT WINAPI ch_isconnected(IRpcChannelBuffer *p) {
    PsdChan *c = chan_of(p);
    c->n_isconn++;
    return c->hr_isconn;
}
static const IRpcChannelBufferVtbl chan_vtbl = { ch_qi, ch_addref, ch_release, ch_getbuffer, ch_sendreceive, ch_freebuffer, ch_getdestctx, ch_isconnected };

PsdChan *psd_chan_new(IRpcStubBuffer *stub) {
    PsdChan *c = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *c);
    c->iface.lpVtbl = (IRpcChannelBufferVtbl *)&chan_vtbl;
    c->ref = 1;
    c->stub = stub;
    c->hr_isconn = S_OK;
    c->destctx = MSHCTX_LOCAL;
    return c;
}

/* ===================== proxy/stub file ================================================== */
CStdPSFactoryBuffer psd_psf;
static CINTERFACE_PROXY_VTABLE(4) pv1;
static CINTERFACE_PROXY_VTABLE(7) pv2;
static CInterfaceStubVtbl sv1, sv2;
CInterfaceProxyVtbl *psd_proxy_vtbl[3];
CInterfaceStubVtbl *psd_stub_vtbl[3];
MIDL_STUB_DESC psd_stub_desc;
MIDL_SERVER_INFO psd_server_info1;
static MIDL_SERVER_INFO server_info2;
static MIDL_STUBLESS_PROXY_INFO pinfo1, pinfo2;
static const unsigned short offs1[] = { 0 };
static const unsigned short offs2[] = { 50, 94, 138, 182 };
static const char *names[] = { "IOletOne", "IOletTwo", 0 };
static int g_ready;

static int WINAPI psd_lookup(const IID *iid, int *idx) {
    for (int i = 0; i < 2; i++)
        if (guid_eq(iid, psd_proxy_vtbl[i]->header.piid)) { *idx = i; return 1; }
    return 0;
}
ExtendedProxyFileInfo psd_pfi = {
    (const PCInterfaceProxyVtblList *)psd_proxy_vtbl, (const PCInterfaceStubVtblList *)psd_stub_vtbl,
    (const PCInterfaceName *)names, 0, psd_lookup, 2, 2, 0, 0, 0, 0 };
const ProxyFileInfo *psd_file_list[2] = { &psd_pfi, 0 };
static ULONG WINAPI stub_release(IRpcStubBuffer *This) {
    return UIN("rpcrt4.dll", NdrCStdStubBuffer_Release)(This, (IPSFactoryBuffer *)&psd_psf);
}

int psd_init(void) {
    if (g_ready) return g_ready > 0;
    g_ready = -1;
    void *qi = (void *)t_proc_in("rpcrt4.dll", "IUnknown_QueryInterface_Proxy");
    void *ar = (void *)t_proc_in("rpcrt4.dll", "IUnknown_AddRef_Proxy");
    void *rl = (void *)t_proc_in("rpcrt4.dll", "IUnknown_Release_Proxy");
    void *oa = (void *)t_proc_in("rpcrt4.dll", "NdrOleAllocate");
    void *of = (void *)t_proc_in("rpcrt4.dll", "NdrOleFree");
    void *s_qi = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_QueryInterface");
    void *s_ar = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_AddRef");
    void *s_co = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_Connect");
    void *s_di = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_Disconnect");
    void *s_in = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_Invoke");
    void *s_ii = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_IsIIDSupported");
    void *s_cr = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_CountRefs");
    void *s_dq = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_DebugServerQueryInterface");
    void *s_dr = (void *)t_proc_in("rpcrt4.dll", "CStdStubBuffer_DebugServerRelease");
    if (!qi || !ar || !rl || !oa || !of || !s_qi || !s_ar || !s_co || !s_di || !s_in || !s_ii || !s_cr || !s_dq || !s_dr) return 0;

    memset(&psd_stub_desc, 0, sizeof psd_stub_desc);
    psd_stub_desc.pfnAllocate = (void *(__RPC_API *)(size_t))oa;
    psd_stub_desc.pfnFree = (void (__RPC_API *)(void *))of;
    psd_stub_desc.pFormatTypes = psd_type_fmt.Format;
    psd_stub_desc.fCheckBounds = 1;
    psd_stub_desc.Version = 0x50002;
    psd_stub_desc.MIDLVersion = 0x50200ca;
    psd_stub_desc.mFlags = 1;

    pinfo1.pStubDesc = &psd_stub_desc; pinfo1.ProcFormatString = psd_proc_fmt.Format; pinfo1.FormatStringOffset = &offs1[-3];
    pinfo2.pStubDesc = &psd_stub_desc; pinfo2.ProcFormatString = psd_proc_fmt.Format; pinfo2.FormatStringOffset = &offs2[-3];
    pv1.header.pStublessProxyInfo = &pinfo1; pv1.header.piid = &psd_IID_One;
    pv1.Vtbl[0] = qi; pv1.Vtbl[1] = ar; pv1.Vtbl[2] = rl; pv1.Vtbl[3] = (void *)-1;
    pv2.header.pStublessProxyInfo = &pinfo2; pv2.header.piid = &psd_IID_Two;
    pv2.Vtbl[0] = qi; pv2.Vtbl[1] = ar; pv2.Vtbl[2] = rl;
    for (int i = 3; i < 7; i++) pv2.Vtbl[i] = (void *)-1;

    psd_server_info1.pStubDesc = &psd_stub_desc; psd_server_info1.ProcString = psd_proc_fmt.Format; psd_server_info1.FmtStringOffset = &offs1[-3];
    server_info2.pStubDesc = &psd_stub_desc; server_info2.ProcString = psd_proc_fmt.Format; server_info2.FmtStringOffset = &offs2[-3];
    sv1.header.piid = &psd_IID_One; sv1.header.pServerInfo = &psd_server_info1; sv1.header.DispatchTableCount = 4; sv1.header.pDispatchTable = 0;
    sv2.header.piid = &psd_IID_Two; sv2.header.pServerInfo = &server_info2; sv2.header.DispatchTableCount = 7; sv2.header.pDispatchTable = 0;
    IRpcStubBufferVtbl *v[2] = { &sv1.Vtbl, &sv2.Vtbl };
    for (int i = 0; i < 2; i++) {
        v[i]->QueryInterface = (void *)s_qi; v[i]->AddRef = (void *)s_ar; v[i]->Release = stub_release;
        v[i]->Connect = (void *)s_co; v[i]->Disconnect = (void *)s_di; v[i]->Invoke = (void *)s_in;
        v[i]->IsIIDSupported = (void *)s_ii; v[i]->CountRefs = (void *)s_cr;
        v[i]->DebugServerQueryInterface = (void *)s_dq; v[i]->DebugServerRelease = (void *)s_dr;
    }
    psd_proxy_vtbl[0] = (CInterfaceProxyVtbl *)&pv1; psd_proxy_vtbl[1] = (CInterfaceProxyVtbl *)&pv2; psd_proxy_vtbl[2] = 0;
    psd_stub_vtbl[0] = &sv1; psd_stub_vtbl[1] = &sv2; psd_stub_vtbl[2] = 0;

    g_ready = 1;
    return 1;
}

HRESULT psd_get_factory(IPSFactoryBuffer **ppsf) {
    if (!psd_init()) return E_FAIL;
    return UIN("rpcrt4.dll", NdrDllGetClassObject)(&psd_CLSID_PSF, &psd_IID_IPSFactoryBuffer, (void **)ppsf, psd_file_list, &psd_CLSID_PSF, &psd_psf);
}

int psd_rig_up(PsdRig *r, const GUID *iid, int connect) {
    memset(r, 0, sizeof *r);
    r->iid = iid;
    if (psd_get_factory(&r->psf) != S_OK || !r->psf) return 0;
    r->srv = psd_srv_new();
    IPSFactoryBuffer_CreateStub(r->psf, iid, PSD_SRV_UNK(r->srv), &r->sb);
    if (IPSFactoryBuffer_CreateProxy(r->psf, NULL, iid, &r->pb, &r->iface) != S_OK) { psd_rig_down(r); return 0; }
    r->chan = psd_chan_new(r->sb);
    if (!r->sb) { psd_rig_down(r); return 0; }
    if (connect) { IRpcProxyBuffer_Connect(r->pb, &r->chan->iface); r->connected = 1; }
    return 1;
}
void psd_rig_down(PsdRig *r) {
    if (r->pb && r->connected) IRpcProxyBuffer_Disconnect(r->pb);
    if (r->iface) ((IUnknown *)r->iface)->lpVtbl->Release((IUnknown *)r->iface);
    if (r->pb) IRpcProxyBuffer_Release(r->pb);
    if (r->sb) { IRpcStubBuffer_Disconnect(r->sb); IRpcStubBuffer_Release(r->sb); }
    if (r->srv) r->srv->one.lpVtbl->Release(&r->srv->one);
    if (r->psf) IPSFactoryBuffer_Release(r->psf);
    if (r->chan) HeapFree(GetProcessHeap(), 0, r->chan);
    memset(r, 0, sizeof *r);
}

int psd_register_ps(DWORD *cookie, IPSFactoryBuffer **psf) {
    *cookie = 0; *psf = NULL;
    if (psd_get_factory(psf) != S_OK) return 0;
    HRESULT hr = U(CoRegisterClassObject)(&psd_CLSID_PSF, (IUnknown *)*psf, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, cookie);
    if (hr != S_OK) { t_note("CoRegisterClassObject(PSF) hr=0x%08lx", hr); IPSFactoryBuffer_Release(*psf); *psf = NULL; return 0; }
    if (U(CoRegisterPSClsid)(&psd_IID_One, &psd_CLSID_PSF) != S_OK || U(CoRegisterPSClsid)(&psd_IID_Two, &psd_CLSID_PSF) != S_OK) {
        t_note("CoRegisterPSClsid failed");
        psd_unregister_ps(*cookie, *psf); *cookie = 0; *psf = NULL; return 0;
    }
    return 1;
}
void psd_unregister_ps(DWORD cookie, IPSFactoryBuffer *psf) {
    if (cookie) U(CoRevokeClassObject)(cookie);
    if (psf) IPSFactoryBuffer_Release(psf);
}

/* ===================== exception catcher ================================================ */
static jmp_buf g_cj;
static volatile LONG g_armed;
static volatile DWORD g_want, g_got, g_tid;
static LONG CALLBACK catch_veh(PEXCEPTION_POINTERS ep) {
    if (!g_armed || GetCurrentThreadId() != g_tid) return EXCEPTION_CONTINUE_SEARCH;
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (g_want ? c != g_want : (c == 0x40010006 || c == 0x4001000A || c == 0x406D1388 || c == 0x80000003 || c == 0xE06D7363))
        return EXCEPTION_CONTINUE_SEARCH;
    g_got = c;
    g_armed = 0;
    longjmp(g_cj, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
int psd_catch(psd_fn fn, void *arg, DWORD *code) {
    static PVOID h;
    if (!h) h = AddVectoredExceptionHandler(1, catch_veh);
    g_got = 0;
    g_tid = GetCurrentThreadId();
    g_want = code ? *code : 0;
    if (setjmp(g_cj) == 0) {
        g_armed = 1;
        fn(arg);
        g_armed = 0;
        if (code) *code = 0;
        return 0;
    }
    if (code) *code = g_got;
    return 1;
}
