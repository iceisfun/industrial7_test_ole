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
    HRESULT hrInfo, hrIid, hrNames, hrParam, hrRet; CALLFRAMEINFO info; GUID iid; ULONG method; LPWSTR ifname, mname; BOOL setRet; HRESULT retval;
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
    if (s->setRet) { ICallFrame_SetReturnValue(f, s->retval); s->hrRet = S_OK; }
    return S_OK;
}
static ICallFrameEventsVtbl sk_vtbl = { sk_qi, sk_addref, sk_release, sk_oncall };
static void sink_init(Sink *s) { memset(s, 0, sizeof *s); s->iface.lpVtbl = &sk_vtbl; s->ref = 1; }

XT(icept_iclassfactory_call_frame) {
    void *pv = NULL; HRESULT hr; ICallInterceptor *ci = NULL; Sink sink; IClassFactory *cf = NULL; IUnknown *unk = NULL;
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoGetInterceptor)(&XP_IID_IClassFactory, NULL, &XP_IID_ICallInterceptor, &pv);
    if (FAILED(hr)) { t_skip("no interceptor for IClassFactory here (%08lx)", hr); UNINIT(); return; }
    ci = pv;
    sink_init(&sink); sink.setRet = TRUE; sink.retval = XP_CUSTOM_HR;
    hr = ICallInterceptor_RegisterSink(ci, (ICallFrameEvents *)&sink.iface);
    XEQ(hr, S_OK);
    hr = ICallInterceptor_QueryInterface(ci, &XP_IID_IClassFactory, (void **)&cf);
    XEQ(hr, S_OK);
    if (cf) {
        hr = IClassFactory_LockServer(cf, TRUE);
        XEQ(sink.onCall, 1);
        XEQ(hr, XP_CUSTOM_HR);                                  /* the value set on the frame is what the caller gets back */
        XEQ(sink.hrInfo, S_OK); XEQ(sink.hrIid, S_OK); XEQ(sink.hrNames, S_OK); XEQ(sink.hrRet, S_OK);
        CHECK(IsEqualGUID(&sink.iid, &XP_IID_IClassFactory));
        XEQ(sink.method, 4);                                    /* IUnknown(0..2), CreateInstance(3), LockServer(4) */
        XEQ(sink.info.iMethod, 4);
        XEQ(sink.info.cParams, 1);
        CHECK(sink.info.fHasInValues);
        CHECK(!sink.info.fHasOutValues);
        t_note("CALLFRAMEINFO.cMethod for IClassFactory: %lu", sink.info.cMethod);
        CHECK(IsEqualGUID(&sink.info.iid, &XP_IID_IClassFactory));
        CHECK(sink.ifname && !lstrcmpW(sink.ifname, L"IClassFactory"));
        CHECK(sink.mname && !lstrcmpW(sink.mname, L"LockServer"));
        if (sink.ifname) U(CoTaskMemFree)(sink.ifname);
        if (sink.mname) U(CoTaskMemFree)(sink.mname);
        sink.ifname = sink.mname = NULL;
        hr = IClassFactory_QueryInterface(cf, &XP_IID_IUnknownBogus, (void **)&unk);
        t_note("QueryInterface of an unsupported iid on an interceptor: %08lx (%s) (OnCall events: %ld)", hr, xp_hrname(hr), sink.onCall);
        IClassFactory_Release(cf);
    }
    hr = ICallInterceptor_RegisterSink(ci, NULL);               /* un-register */
    t_note("ICallInterceptor::RegisterSink(NULL): %08lx (%s)", hr, xp_hrname(hr));
    ICallInterceptor_Release(ci);
    UNINIT();
}
XT(icept_iclassfactory_create_instance_frame) {
    void *pv = NULL; HRESULT hr; ICallInterceptor *ci = NULL; Sink sink; IClassFactory *cf = NULL; void *out = (void *)1;
    NEED_ICEPT(); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoGetInterceptor)(&XP_IID_IClassFactory, NULL, &XP_IID_ICallInterceptor, &pv);
    if (FAILED(hr)) { t_skip("no interceptor for IClassFactory here (%08lx)", hr); UNINIT(); return; }
    ci = pv;
    sink_init(&sink); sink.setRet = TRUE; sink.retval = E_NOTIMPL;
    hr = ICallInterceptor_RegisterSink(ci, (ICallFrameEvents *)&sink.iface); XEQ(hr, S_OK);
    hr = ICallInterceptor_QueryInterface(ci, &XP_IID_IClassFactory, (void **)&cf); XEQ(hr, S_OK);
    if (cf) {
        hr = IClassFactory_CreateInstance(cf, NULL, &XP_IID_IUnknown, &out);
        XEQ(hr, E_NOTIMPL);
        XEQ(sink.onCall, 1);
        XEQ(sink.method, 3);
        XEQ(sink.info.cParams, 3);                             /* pUnkOuter, riid, ppvObject */
        CHECK(sink.info.fHasInValues);
        CHECK(sink.info.fHasOutValues);
        CHECK(sink.mname && !lstrcmpW(sink.mname, L"CreateInstance"));
        if (sink.ifname) U(CoTaskMemFree)(sink.ifname);
        if (sink.mname) U(CoTaskMemFree)(sink.mname);
        IClassFactory_Release(cf);
    }
    ICallInterceptor_RegisterSink(ci, NULL);
    ICallInterceptor_Release(ci);
    UNINIT();
}
