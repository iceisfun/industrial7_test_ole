/* Call context, security blanket, impersonation and cancellation APIs, in-process.
 * Expectations tagged (decomp) come straight from the Windows 10 combase.dll decompilation. */
#include "xp.h"

#define INIT(m) do { HRESULT h_ = U(CoInitializeEx)(NULL, (m)); if (FAILED(h_)) { t_skip("CoInitializeEx failed %08lx", h_); return; } } while (0)
#define UNINIT() U(CoUninitialize)()
static HRESULT probe(IClassFactory *p) { void *pv = NULL; return IClassFactory_CreateInstance(p, NULL, &XP_IID_IXpBogus2, &pv); }

/* ---- no call in progress ------------------------------------------------------------------------ */
static void outside_call_checks(void)
{
    DWORD a = 0x11, z = 0x22, l = 0x33, i = 0x44, c = 0x55; OLECHAR *pn = (OLECHAR *)0x66; void *pv = (void *)0x77; HRESULT hr;
    hr = U(CoQueryClientBlanket)(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    XEQ(hr, XP_RPC_E_CALL_COMPLETE);                         /* (decomp) CoGetCallContext fails, error passed through */
    hr = U(CoQueryClientBlanket)(&a, &z, &pn, &l, &i, &pv, &c);
    XEQ(hr, XP_RPC_E_CALL_COMPLETE);
    XEQ(a, 0x11); XEQ(l, 0x33); XEQ(c, 0x55);                /* outputs untouched on failure */
    hr = U(CoImpersonateClient)();   XEQ(hr, XP_RPC_E_CALL_COMPLETE);
    hr = U(CoRevertToSelf)();        XEQ(hr, XP_RPC_E_CALL_COMPLETE);
    {
        void *ss = (void *)0x1;
        hr = U(CoGetCallContext)(&XP_IID_IServerSecurity, &ss);
        XEQ(hr, XP_RPC_E_CALL_COMPLETE);
        t_note("CoGetCallContext(IServerSecurity) outside a call leaves *ppv = %p", ss);
        hr = U(CoGetCallContext)(&XP_IID_IServerSecurity, NULL);
        XEQ(hr, E_INVALIDARG);                              /* (decomp) NULL ppv is checked before the context */
        if (!GetModuleHandleA("comsvcs.dll")) {
            ss = (void *)0x1;
            hr = U(CoGetCallContext)(&XP_IID_IUnknown, &ss);
            XEQ(hr, XP_RPC_E_CALL_COMPLETE);                /* (decomp) other IIDs fall back to COM+ only when comsvcs is loaded */
            hr = U(CoGetCallContext)(&XP_IID_IUnknownBogus, &ss);
            XEQ(hr, XP_RPC_E_CALL_COMPLETE);
        }
    }
}
XT(sec_outside_call_initialized_sta) {
    NEED(CoQueryClientBlanket); NEED(CoImpersonateClient); NEED(CoRevertToSelf); NEED(CoGetCallContext); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_APARTMENTTHREADED); outside_call_checks(); UNINIT();
}
XT(sec_outside_call_initialized_mta) {
    NEED(CoQueryClientBlanket); NEED(CoImpersonateClient); NEED(CoRevertToSelf); NEED(CoGetCallContext); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_MULTITHREADED); outside_call_checks(); UNINIT();
}
static DWORD outside_proc(void *p) { outside_call_checks(); return 0; }
XT(sec_outside_call_uninitialized_thread) {
    NEED(CoQueryClientBlanket); NEED(CoImpersonateClient); NEED(CoRevertToSelf); NEED(CoGetCallContext);
    EXPECT_EQ(xp_thread_run(-1, outside_proc, NULL, 10000), 0);     /* the call-context API does not need CoInitialize */
}
XT(sec_caller_tid_outside_call_args) {
    DWORD tid = 0xDEAD; HRESULT hr;
    NEED(CoGetCallerTID);
    hr = U(CoGetCallerTID)(NULL);
    XEQ(hr, E_INVALIDARG);                                   /* (decomp) */
    hr = U(CoGetCallerTID)(&tid);
    CHECK(hr == S_OK || hr == S_FALSE);                      /* (decomp) S_OK only if the caller TID is local */
    t_note("CoGetCallerTID with no call in progress: hr=%08lx tid=%lu", hr, tid);
}

/* ---- inside an in-process call --------------------------------------------------------------------- */
typedef struct {
    HRESULT ctxSs, ctxUnk, ctxBogus, qcb, imp, rev, tidHr, ssImpersonating;
    void *ssPtr; DWORD tid; DWORD level, imp_level, authn, authz;
} CtxRec;
static void ctx_hook(XpObj *o, int what, BOOL a)
{
    CtxRec *r = o->user; void *pv = NULL; OLECHAR *pn = NULL; void *priv = NULL; DWORD caps;
    if (what != XP_HOOK_CREATE || !IsEqualGUID(&o->lastCreateIid, &XP_IID_IXpBogus2)) return;
    r->ctxSs = U(CoGetCallContext)(&XP_IID_IServerSecurity, &pv);
    r->ssPtr = pv;
    if (pv) {
        r->ssImpersonating = IServerSecurity_IsImpersonating((IServerSecurity *)pv);
        IServerSecurity_Release((IServerSecurity *)pv);
    }
    pv = NULL; r->ctxUnk = U(CoGetCallContext)(&XP_IID_IUnknown, &pv); if (pv) IUnknown_Release((IUnknown *)pv);
    pv = NULL; r->ctxBogus = U(CoGetCallContext)(&XP_IID_IUnknownBogus, &pv); if (pv) IUnknown_Release((IUnknown *)pv);
    r->qcb = U(CoQueryClientBlanket)(&r->authn, &r->authz, &pn, &r->level, &r->imp_level, &priv, &caps);
    if (pn) U(CoTaskMemFree)(pn);
    r->imp = U(CoImpersonateClient)();
    r->rev = U(CoRevertToSelf)();
    r->tidHr = U(CoGetCallerTID)(&r->tid);
}
static void inproc_ctx_case(DWORD clientModel, DWORD hostModel)
{
    XpFx f; CtxRec r; DWORD d = 0; void *pv = (void *)1; HRESULT hr;
    XP_NEED_BASE(); NEED(CoGetCallContext); NEED(CoQueryClientBlanket); NEED(CoImpersonateClient); NEED(CoRevertToSelf); NEED(CoGetCallerTID);
    INIT(clientModel);
    if (xp_fx_setup(&f, hostModel, MSHLFLAGS_NORMAL)) {
        memset(&r, 0, sizeof r);
        f.o->user = &r; f.o->hook = ctx_hook;
        hr = probe(f.p);
        XEQ(hr, S_OK);
        t_note("in-process call context: IServerSecurity=%08lx IUnknown=%08lx bogus=%08lx", r.ctxSs, r.ctxUnk, r.ctxBogus);
        t_note("  CoQueryClientBlanket=%08lx (authn=%lu level=%lu imp=%lu) CoImpersonateClient=%08lx CoRevertToSelf=%08lx",
               r.qcb, r.authn, r.level, r.imp_level, r.imp, r.rev);
        t_note("  CoGetCallerTID=%08lx tid=%lu IsImpersonating(on entry)=%ld", r.tidHr, r.tid, (long)r.ssImpersonating);
        EXPECT_EQ(r.tid, f.mainTid);
        EXPECT_EQ(r.tidHr, S_OK);
        CHECK(r.ctxBogus != S_OK);                                     /* nothing implements a made-up IID */
        /* after the call returned there is no context any more */
        hr = U(CoGetCallContext)(&XP_IID_IServerSecurity, &pv);
        XEQ(hr, XP_RPC_E_CALL_COMPLETE);
        hr = U(CoQueryClientBlanket)(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        XEQ(hr, XP_RPC_E_CALL_COMPLETE);
        f.o->hook = NULL;
        (void)d;
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(sec_inproc_call_context_sta_to_sta) { inproc_ctx_case(COINIT_APARTMENTTHREADED, COINIT_APARTMENTTHREADED); }
XT(sec_inproc_call_context_sta_to_mta) { inproc_ctx_case(COINIT_APARTMENTTHREADED, COINIT_MULTITHREADED); }

/* ---- blanket functions against an in-process proxy ----------------------------------------------------- */
XT(sec_inproc_proxy_blanket) {
    XpFx f; HRESULT hr; IClientSecurity *cs = NULL; IMarshal *m = NULL;
    DWORD a = 0xAAAA, z = 0xAAAA, l = 0xAAAA, i = 0xAAAA, c = 0xAAAA; OLECHAR *pn = NULL; void *au = NULL;
    XP_NEED_BASE(); NEED(CoQueryProxyBlanket); NEED(CoSetProxyBlanket); INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_NORMAL)) {
        hr = U(CoQueryProxyBlanket)((IUnknown *)f.p, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        XEQ(hr, S_OK);
        hr = U(CoQueryProxyBlanket)((IUnknown *)f.p, &a, &z, &pn, &l, &i, &au, &c);
        XEQ(hr, S_OK);
        t_note("in-process proxy blanket: authn=%lu authz=%lu level=%lu imp=%lu caps=0x%lx", a, z, l, i, c);
        if (pn) U(CoTaskMemFree)(pn);
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IClientSecurity, (void **)&cs); XEQ(hr, S_OK);
        hr = IClassFactory_QueryInterface(f.p, &XP_IID_IMarshal, (void **)&m); XEQ(hr, S_OK);
        if (cs && m) {
            hr = IClientSecurity_QueryBlanket(cs, (IUnknown *)m, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            XEQ(hr, E_NOINTERFACE);                        /* IMarshal on the proxy is not a proxied interface */
            hr = IClientSecurity_SetBlanket(cs, (IUnknown *)m, 10, 0, NULL, 3, 3, NULL, 0);
            XEQ(hr, E_NOINTERFACE);
            hr = IClientSecurity_SetBlanket(cs, (IUnknown *)f.p, 0xDEADBEEF, 0, NULL, 3, 3, NULL, 0);
            XEQ(hr, E_INVALIDARG);                         /* unknown authentication service */
            hr = IClientSecurity_QueryBlanket(cs, (IUnknown *)f.p, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            XEQ(hr, S_OK);
        }
        hr = U(CoSetProxyBlanket)((IUnknown *)f.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        t_note("CoSetProxyBlanket on an in-process proxy: %08lx (%s)", hr, xp_hrname(hr));
        XEQ(probe(f.p), S_OK);
        if (cs) IClientSecurity_Release(cs);
        if (m) IMarshal_Release(m);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(sec_inproc_copy_proxy) {
    XpFx f; HRESULT hr; IUnknown *copy = NULL;
    XP_NEED_BASE(); NEED(CoCopyProxy); INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_NORMAL)) {
        if (!t_is_wine()) {
            hr = U(CoCopyProxy)((IUnknown *)f.p, NULL);
            XEQ(hr, E_INVALIDARG);
            hr = U(CoCopyProxy)(NULL, &copy);
            XEQ(hr, E_INVALIDARG);
        }
        hr = U(CoCopyProxy)((IUnknown *)f.p, &copy);
        t_note("CoCopyProxy of an in-process proxy: %08lx (%s)", hr, xp_hrname(hr));
        if (SUCCEEDED(hr) && copy) {
            XEQ(probe((IClassFactory *)copy), S_OK);
            IUnknown_Release(copy);
        }
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
XT(sec_proxy_blanket_null_args) {
    IUnknown *cp = (IUnknown *)0x1;
    NEED(CoQueryProxyBlanket); NEED(CoSetProxyBlanket); NEED(CoCopyProxy);
    if (t_is_wine()) { t_skip("emulator dereferences NULL proxies"); return; }
    XEQ(U(CoQueryProxyBlanket)(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG);   /* (decomp) */
    XEQ(U(CoSetProxyBlanket)(NULL, 10, 0, NULL, 3, 3, NULL, 0), E_INVALIDARG);
    XEQ(U(CoCopyProxy)(NULL, &cp), E_INVALIDARG);
    XEQ(U(CoCopyProxy)(NULL, NULL), E_INVALIDARG);
    XEQ(cp, 1);
}
XT(sec_decode_proxy_invalid_args) {
    ServerInformation si; HRESULT hr;
    NEED(CoDecodeProxy);
    memset(&si, 0, sizeof si);
    hr = U(CoDecodeProxy)(0, 0x1000, &si);                 XEQ(hr, E_INVALIDARG);   /* (decomp) every argument must be non-zero */
    hr = U(CoDecodeProxy)(GetCurrentProcessId(), 0, &si);  XEQ(hr, E_INVALIDARG);
    hr = U(CoDecodeProxy)(GetCurrentProcessId(), 0x1000, NULL); XEQ(hr, E_INVALIDARG);
    hr = U(CoDecodeProxy)(0, 0, NULL);                     XEQ(hr, E_INVALIDARG);
}

/* ---- call cancellation ------------------------------------------------------------------------------------ */
XT(sec_cancel_enable_disable_balanced) {
    HRESULT hr;
    NEED(CoEnableCallCancellation); NEED(CoDisableCallCancellation); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoDisableCallCancellation)(NULL);  XEQ(hr, XP_RPC_E_CANCEL_DISABLED);     /* (decomp) count is zero */
    hr = U(CoEnableCallCancellation)(NULL);   XEQ(hr, S_OK);
    hr = U(CoEnableCallCancellation)(NULL);   XEQ(hr, S_OK);                        /* nests */
    hr = U(CoDisableCallCancellation)(NULL);  XEQ(hr, S_OK);
    hr = U(CoDisableCallCancellation)(NULL);  XEQ(hr, S_OK);
    hr = U(CoDisableCallCancellation)(NULL);  XEQ(hr, XP_RPC_E_CANCEL_DISABLED);     /* unbalanced */
    UNINIT();
}
XT(sec_cancel_reserved_param_must_be_null) {
    HRESULT hr;
    NEED(CoEnableCallCancellation); NEED(CoDisableCallCancellation); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoEnableCallCancellation)((void *)1);   XEQ(hr, E_INVALIDARG);            /* (decomp) */
    hr = U(CoDisableCallCancellation)((void *)1);  XEQ(hr, E_INVALIDARG);
    hr = U(CoDisableCallCancellation)(NULL);       XEQ(hr, XP_RPC_E_CANCEL_DISABLED);   /* the failed enable did not count */
    UNINIT();
}
static DWORD cancel_counts_proc(void *p)
{
    HRESULT h1 = U(CoEnableCallCancellation)(NULL), h2 = U(CoDisableCallCancellation)(NULL), h3 = U(CoDisableCallCancellation)(NULL);
    return (h1 == S_OK && h2 == S_OK && h3 == XP_RPC_E_CANCEL_DISABLED) ? 0 : 1;
}
XT(sec_cancel_enable_per_thread_state) {
    HRESULT hr;
    NEED(CoEnableCallCancellation); NEED(CoDisableCallCancellation); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoEnableCallCancellation)(NULL); XEQ(hr, S_OK);
    XEQ(xp_thread_run(COINIT_MULTITHREADED, cancel_counts_proc, NULL, 10000), 0);    /* the other thread starts at zero */
    hr = U(CoDisableCallCancellation)(NULL); XEQ(hr, S_OK);
    UNINIT();
}
XT(sec_cancel_object_without_call) {
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented cancel APIs"); return; }
    void *pv = (void *)1; HRESULT hr;
    NEED(CoGetCancelObject); NEED(CoSetCancelObject); NEED(CoTestCancel); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoGetCancelObject)(0, &XP_IID_ICancelMethodCalls, NULL);
    XEQ(hr, E_INVALIDARG);                                         /* (decomp) */
    hr = U(CoGetCancelObject)(0, &XP_IID_ICancelMethodCalls, &pv);
    XEQ(hr, XP_RPC_E_NO_CONTEXT);                                  /* (decomp) no call table entry for this thread */
    hr = U(CoGetCancelObject)(GetCurrentThreadId(), &XP_IID_ICancelMethodCalls, &pv);
    XEQ(hr, XP_RPC_E_NO_CONTEXT);
    hr = U(CoGetCancelObject)(0x7FFFFFF0, &XP_IID_ICancelMethodCalls, &pv);
    XEQ(hr, XP_RPC_E_NO_CONTEXT);
    if (!GetModuleHandleA("comsvcs.dll")) {
        hr = U(CoTestCancel)();
        XEQ(hr, XP_RPC_E_CALL_COMPLETE);                           /* (decomp) CoGetCallContext(ICancelMethodCalls) fails outside a call */
    }
    UNINIT();
}
XT(sec_set_cancel_object) {
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented cancel APIs"); return; }
    XpObj *o; HRESULT hr;
    NEED(CoSetCancelObject); NEED(CoInitializeEx); NEED(CoUninitialize);
    INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoSetCancelObject)(NULL);
    XEQ(hr, S_OK);                                                 /* (decomp) clearing the cancel object always succeeds */
    o = xp_obj_new();
    hr = U(CoSetCancelObject)(XP_OBJ_UNK(o));
    XEQ(hr, E_NOINTERFACE);                                        /* (decomp) the object must implement ICancelMethodCalls */
    CHECK(IsEqualGUID(&o->lastQi, &XP_IID_ICancelMethodCalls));
    XEQ(o->ref, 1);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

typedef struct { HANDLE entered, release, cancelled; DWORD clientTid; HRESULT hrGet, hrCancel, hrTest; int useEnable; } CancelCtx;
static void cancel_hook(XpObj *o, int what, BOOL a)
{
    CancelCtx *c = o->user;
    if (what != XP_HOOK_CREATE || !IsEqualGUID(&o->lastCreateIid, &XP_IID_IXpBogus2)) return;
    SetEvent(c->entered);
    WaitForSingleObject(c->release, 8000);          /* bounded: the server method outlives the cancelled client call */
}
static DWORD canceller_proc(void *p)
{
    CancelCtx *c = p; ICancelMethodCalls *cm = NULL;
    if (WaitForSingleObject(c->entered, 8000) != WAIT_OBJECT_0) return 1;
    c->hrGet = U(CoGetCancelObject)(c->clientTid, &XP_IID_ICancelMethodCalls, (void **)&cm);
    if (SUCCEEDED(c->hrGet) && cm) {
        c->hrCancel = ICancelMethodCalls_Cancel(cm, 0);
        ICancelMethodCalls_Release(cm);
    }
    SetEvent(c->cancelled);
    return 0;
}
XT(sec_cancel_pending_call_from_other_thread) {
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented cancel APIs"); return; }
    XpFx f; CancelCtx c; HANDLE th; DWORD tid; HRESULT hr;
    XP_NEED_BASE(); NEED(CoGetCancelObject); NEED(CoEnableCallCancellation); NEED(CoDisableCallCancellation);
    INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_NORMAL)) {
        memset(&c, 0, sizeof c);
        c.entered = CreateEventA(NULL, TRUE, FALSE, NULL); c.release = CreateEventA(NULL, TRUE, FALSE, NULL); c.cancelled = CreateEventA(NULL, TRUE, FALSE, NULL);
        c.clientTid = GetCurrentThreadId(); c.hrGet = c.hrCancel = (HRESULT)0xDEADBEEF;
        f.o->user = &c; f.o->hook = cancel_hook;
        hr = U(CoEnableCallCancellation)(NULL); XEQ(hr, S_OK);
        th = CreateThread(NULL, 0, canceller_proc, &c, 0, &tid);
        hr = probe(f.p);                                           /* blocks until the other thread cancels it */
        XEQ(hr, (HRESULT)0x80010002);                              /* RPC_E_CALL_CANCELED */
        CHECK(WaitForSingleObject(c.cancelled, 8000) == WAIT_OBJECT_0);
        XEQ(c.hrGet, S_OK);
        XEQ(c.hrCancel, S_OK);
        SetEvent(c.release);                                       /* let the server method finish */
        hr = U(CoDisableCallCancellation)(NULL); XEQ(hr, S_OK);
        WaitForSingleObject(th, 8000); CloseHandle(th);
        f.o->hook = NULL;
        /* the call object is gone again */
        {
            void *pv = NULL;
            hr = U(CoGetCancelObject)(0, &XP_IID_ICancelMethodCalls, &pv);
            XEQ(hr, XP_RPC_E_NO_CONTEXT);
        }
        CloseHandle(c.entered); CloseHandle(c.release); CloseHandle(c.cancelled);
        xp_pump(300);
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
static DWORD getter_proc(void *p)
{
    CancelCtx *c = p; ICancelMethodCalls *cm = NULL;
    if (WaitForSingleObject(c->entered, 8000) == WAIT_OBJECT_0) {
        c->hrGet = U(CoGetCancelObject)(c->clientTid, &XP_IID_ICancelMethodCalls, (void **)&cm);
        if (cm) ICancelMethodCalls_Release(cm);
    }
    SetEvent(c->release);                         /* only look at the cancel object, then let the call finish */
    return 0;
}
XT(sec_cancel_without_enable_is_refused) {
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented cancel APIs"); return; }
    XpFx f; CancelCtx c; HANDLE th; DWORD tid; HRESULT hr;
    XP_NEED_BASE(); NEED(CoGetCancelObject);
    INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_NORMAL)) {
        memset(&c, 0, sizeof c);
        c.entered = CreateEventA(NULL, TRUE, FALSE, NULL); c.release = CreateEventA(NULL, TRUE, FALSE, NULL);
        c.clientTid = GetCurrentThreadId(); c.hrGet = (HRESULT)0xDEADBEEF;
        f.o->user = &c; f.o->hook = cancel_hook;
        th = CreateThread(NULL, 0, getter_proc, &c, 0, &tid);
        hr = probe(f.p);
        XEQ(hr, S_OK);
        WaitForSingleObject(th, 8000); CloseHandle(th);
        CHECK(c.hrGet != (HRESULT)0xDEADBEEF);
        t_note("CoGetCancelObject for a call whose client did not enable cancellation: %08lx (%s)", c.hrGet, xp_hrname(c.hrGet));
        CloseHandle(c.entered); CloseHandle(c.release);
        f.o->hook = NULL;
    } else CHECK(0);
    xp_fx_teardown(&f); UNINIT();
}
