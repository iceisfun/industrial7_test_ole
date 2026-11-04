/* ICallInterceptor / ICallFrame: CoGetInterceptor and CoGetInterceptorFromTypeInfo (ole32 exports).
 * (decomp) = taken from the Windows 10 ole32.dll decompilation. Which interfaces can be intercepted depends
 * on what is registered (type libraries), so successes are exercised only when the call succeeded. */
#include "xp.h"

#define INIT(m) do { HRESULT h_ = U(CoInitializeEx)(NULL, (m)); if (FAILED(h_)) { t_skip("CoInitializeEx failed %08lx", h_); return; } } while (0)
#define UNINIT() U(CoUninitialize)()
#define NEED_ICEPT() do { if (t_is_wine()) { t_skip("emulator does not implement the call interceptor"); return; } NEED(CoGetInterceptor); } while (0)

XT(icept_invalid_aggregation_iid) {
    void *pv = (void *)1; HRESULT hr; XpObj *o;
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    o = xp_obj_new();
    hr = U(CoGetInterceptor)(&XP_IID_IClassFactory, XP_OBJ_UNK(o), &XP_IID_ICallInterceptor, &pv);
    XEQ(hr, E_INVALIDARG);                                  /* (decomp) with an outer unknown the requested iid must be IID_IUnknown */
    CHECK(pv == NULL);                                      /* (decomp) *ppv is cleared first */
    pv = (void *)1;
    hr = U(CoGetInterceptor)(&XP_IID_IClassFactory, XP_OBJ_UNK(o), &XP_IID_IClassFactory, &pv);
    XEQ(hr, E_INVALIDARG);
    CHECK(pv == NULL);
    XEQ(o->ref, 1);
    XEQ(o->qiCalls, 0);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(icept_unknown_interface_fails) {
    void *pv = (void *)1; HRESULT hr;
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoGetInterceptor)(&XP_IID_IUnknownBogus, NULL, &XP_IID_ICallInterceptor, &pv);
    CHECKF(FAILED(hr), "interceptor for an unregistered interface: %08lx", hr);
    CHECK(pv == NULL);
    t_note("CoGetInterceptor(unregistered IID): %08lx (%s)", hr, xp_hrname(hr));
    pv = (void *)1;
    hr = U(CoGetInterceptor)(&XP_IID_IUnknownBogus, NULL, &XP_IID_IUnknown, &pv);
    CHECKF(FAILED(hr), "%08lx", hr);
    CHECK(pv == NULL);
    UNINIT();
}
XT(icept_from_typeinfo_invalid_aggregation_iid) {
    void *pv = (void *)1; HRESULT hr; XpObj *o;
    if (t_is_wine()) { t_skip("emulator does not implement the call interceptor"); return; }
    NEED(CoGetInterceptorFromTypeInfo); INIT(COINIT_APARTMENTTHREADED);
    o = xp_obj_new();
    hr = U(CoGetInterceptorFromTypeInfo)(&XP_IID_IClassFactory, XP_OBJ_UNK(o), NULL, &XP_IID_ICallInterceptor, &pv);
    XEQ(hr, E_INVALIDARG);                                  /* (decomp) */
    CHECK(pv == NULL);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

typedef struct { const GUID *iid; const char *name; } IcTarget;
XT(icept_which_interfaces_have_interceptors) {
    static const IcTarget t[] = {
        { &XP_IID_IUnknown, "IUnknown" }, { &XP_IID_IDispatch, "IDispatch" }, { &XP_IID_IClassFactory, "IClassFactory" },
        { &XP_IID_IStream, "IStream" }, { &XP_IID_IMarshal, "IMarshal" }, { &XP_IID_IEnumString, "IEnumString" },
        { &XP_IID_ICallFrameEvents, "ICallFrameEvents" }, { &XP_IID_IMessageFilter, "IMessageFilter" }, { &XP_IID_IStorage, "IStorage" },
    };
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *pv = (void *)1; HRESULT hr = U(CoGetInterceptor)(t[i].iid, NULL, &XP_IID_ICallInterceptor, &pv);
        t_note("CoGetInterceptor(%s, ICallInterceptor): %08lx (%s)%s", t[i].name, hr, xp_hrname(hr), SUCCEEDED(hr) ? " -> interceptor" : "");
        if (SUCCEEDED(hr)) { CHECK(pv != NULL); if (pv) IUnknown_Release((IUnknown *)pv); }
        else CHECK(pv == NULL);
    }
    UNINIT();
}

/* ---- a frame-event sink --------------------------------------------------------------------------------- */
typedef struct {
    ICallFrameEvents iface; LONG ref; LONG onCall;
    HRESULT hrInfo, hrIid, hrNames, hrParam, hrRet, hrPi[3]; CALLFRAMEPARAMINFO pi[3]; CALLFRAMEINFO info; GUID iid; ULONG method; LPWSTR ifname, mname; BOOL setRet; HRESULT retval;
} Sink;
static Sink *SK(ICallFrameEvents *e) { return (Sink *)e; }
static HRESULT WINAPI sk_qi(ICallFrameEvents *e, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_ICallFrameEvents)) { *ppv = e; InterlockedIncrement(&SK(e)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sk_addref(ICallFrameEvents *e) { return InterlockedIncrement(&SK(e)->ref); }
static ULONG WINAPI sk_release(ICallFrameEvents *e) { return InterlockedDecrement(&SK(e)->ref); }
static HRESULT WINAPI sk_oncall(ICallFrameEvents *e, ICallFrame *f)
{
    Sink *s = SK(e);
    InterlockedIncrement(&s->onCall);
    s->hrInfo = ICallFrame_GetInfo(f, &s->info);
    s->hrIid = ICallFrame_GetIIDAndMethod(f, &s->iid, &s->method);
    s->hrNames = ICallFrame_GetNames(f, &s->ifname, &s->mname);
    for (ULONG i = 0; i < 3; i++) s->hrPi[i] = ICallFrame_GetParamInfo(f, i, &s->pi[i]);
    if (s->setRet) { ICallFrame_SetReturnValue(f, s->retval); s->hrRet = S_OK; }
    return S_OK;
}
static ICallFrameEventsVtbl sk_vtbl = { sk_qi, sk_addref, sk_release, sk_oncall };
static void sink_init(Sink *s) { memset(s, 0, sizeof *s); s->iface.lpVtbl = &sk_vtbl; s->ref = 1; }

/* Win10 has call interceptors only for interfaces with a registered type library (IDispatch here); IClassFactory,
   IUnknown, IStream, ... fail with HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), see icept_which_interfaces_have_interceptors */
XT(icept_idispatch_call_frame) {
    void *pv = NULL; HRESULT hr; ICallInterceptor *ci = NULL; Sink sink; IDispatch *d = NULL; IUnknown *unk = NULL; UINT n = 0xAAAA;
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoGetInterceptor)(&XP_IID_IDispatch, NULL, &XP_IID_ICallInterceptor, &pv);
    XEQ(hr, S_OK);
    if (FAILED(hr)) { UNINIT(); return; }
    ci = pv;
    sink_init(&sink); sink.setRet = TRUE; sink.retval = XP_CUSTOM_HR;
    hr = ICallInterceptor_RegisterSink(ci, (ICallFrameEvents *)&sink.iface);
    XEQ(hr, S_OK);
    hr = ICallInterceptor_QueryInterface(ci, &XP_IID_IDispatch, (void **)&d);
    XEQ(hr, S_OK);
    if (d) {
        hr = IDispatch_GetTypeInfoCount(d, &n);
        XEQ(sink.onCall, 1);
        XEQ(hr, XP_CUSTOM_HR);                                  /* the value set on the frame is what the caller gets back */
        XEQ(sink.hrInfo, S_OK); XEQ(sink.hrIid, S_OK); XEQ(sink.hrNames, S_OK); XEQ(sink.hrRet, S_OK);
        CHECK(IsEqualGUID(&sink.iid, &XP_IID_IDispatch));
        XEQ(sink.method, 3);                                    /* IUnknown(0..2), GetTypeInfoCount(3) */
        XEQ(sink.info.iMethod, 3);
        /* Win10: CALLFRAMEINFO.cParams is left 0 for these type-library based frames; the parameters are described by GetParamInfo */
        XEQ(sink.info.cParams, 0);
        XEQ(sink.hrPi[0], S_OK); XEQ(sink.pi[0].fIn, 0); XEQ(sink.pi[0].fOut, 1);   /* UINT *pctinfo */
        t_note("GetTypeInfoCount frame: hasIn=%d hasOut=%d cMethod=%lu", (int)sink.info.fHasInValues, (int)sink.info.fHasOutValues, sink.info.cMethod);
        CHECK(IsEqualGUID(&sink.info.iid, &XP_IID_IDispatch));
        CHECK(sink.ifname && !lstrcmpW(sink.ifname, L"IDispatch"));
        CHECK(sink.mname && !lstrcmpW(sink.mname, L"GetTypeInfoCount"));
        if (sink.ifname) U(CoTaskMemFree)(sink.ifname);
        if (sink.mname) U(CoTaskMemFree)(sink.mname);
        sink.ifname = sink.mname = NULL;
        hr = IDispatch_QueryInterface(d, &XP_IID_IUnknownBogus, (void **)&unk);
        t_note("QueryInterface of an unsupported iid on an interceptor: %08lx (%s) (OnCall events: %ld)", hr, xp_hrname(hr), sink.onCall);
        IDispatch_Release(d);
    }
    hr = ICallInterceptor_RegisterSink(ci, NULL);               /* un-register */
    t_note("ICallInterceptor::RegisterSink(NULL): %08lx (%s)", hr, xp_hrname(hr));
    ICallInterceptor_Release(ci);
    UNINIT();
}
XT(icept_idispatch_get_type_info_frame) {
    void *pv = NULL; HRESULT hr; ICallInterceptor *ci = NULL; Sink sink; IDispatch *d = NULL; ITypeInfo *ti = (ITypeInfo *)1;
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoGetInterceptor)(&XP_IID_IDispatch, NULL, &XP_IID_ICallInterceptor, &pv);
    XEQ(hr, S_OK);
    if (FAILED(hr)) { UNINIT(); return; }
    ci = pv;
    sink_init(&sink); sink.setRet = TRUE; sink.retval = E_NOTIMPL;
    hr = ICallInterceptor_RegisterSink(ci, (ICallFrameEvents *)&sink.iface); XEQ(hr, S_OK);
    hr = ICallInterceptor_QueryInterface(ci, &XP_IID_IDispatch, (void **)&d); XEQ(hr, S_OK);
    if (d) {
        hr = IDispatch_GetTypeInfo(d, 0, 0x409, &ti);
        XEQ(hr, E_NOTIMPL);
        XEQ(sink.onCall, 1);
        XEQ(sink.method, 4);
        XEQ(sink.info.cParams, 0);                             /* see above: not filled in on Win10 */
        XEQ(sink.hrPi[0], S_OK); XEQ(sink.pi[0].fIn, 1); XEQ(sink.pi[0].fOut, 0);   /* iTInfo */
        XEQ(sink.hrPi[1], S_OK); XEQ(sink.pi[1].fIn, 1); XEQ(sink.pi[1].fOut, 0);   /* lcid */
        XEQ(sink.hrPi[2], S_OK); XEQ(sink.pi[2].fIn, 0); XEQ(sink.pi[2].fOut, 1);   /* ppTInfo */
        CHECK(sink.info.fHasInValues);
        CHECK(sink.info.fHasOutValues);
        CHECK(sink.mname && !lstrcmpW(sink.mname, L"GetTypeInfo"));
        if (sink.ifname) U(CoTaskMemFree)(sink.ifname);
        if (sink.mname) U(CoTaskMemFree)(sink.mname);
        IDispatch_Release(d);
    }
    ICallInterceptor_RegisterSink(ci, NULL);
    ICallInterceptor_Release(ci);
    UNINIT();
}
