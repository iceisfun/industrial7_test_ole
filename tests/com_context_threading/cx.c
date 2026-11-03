#include "cx.h"

int g_cx_cf;

HRESULT cx_init(DWORD model) { return U(CoInitializeEx)(NULL, model); }
void cx_uninit(void) { U(CoUninitialize)(); }

HANDLE cx_event(BOOL manual, BOOL initial) { return CreateEventW(NULL, manual, initial, NULL); }
DWORD cx_wait(HANDLE h, DWORD ms) { return WaitForSingleObject(h, ms); }

typedef struct RunJob { DWORD model; cx_fn fn; void *arg; HRESULT hr; } RunJob;
static DWORD WINAPI run_thread(LPVOID p)
{
    RunJob *j = p;
    int inited = 0;
    j->hr = S_OK;
    if (j->model != CXM_NONE) {
        j->hr = cx_init(j->model);
        if (CX_FAILED(j->hr)) return 0;
        inited = 1;
    }
    j->fn(j->arg);
    if (inited) cx_uninit();
    return 1;
}
int cx_run_hr(DWORD model, cx_fn fn, void *arg, HRESULT *init_hr)
{
    RunJob j = { model, fn, arg, S_OK };
    HANDLE th = CreateThread(NULL, 0, run_thread, &j, 0, NULL);
    DWORD r;
    if (!th) { CHECK(!"CreateThread"); return 0; }
    r = WaitForSingleObject(th, 10000);
    if (r != WAIT_OBJECT_0) { CHECKF(0, "worker thread did not finish within 10s"); return 0; }  /* leaked on purpose: j is on our stack, so wait more */
    CloseHandle(th);
    if (init_hr) *init_hr = j.hr;
    return 1;
}
int cx_run(DWORD model, cx_fn fn, void *arg)
{
    HRESULT hr = S_OK;
    int r = cx_run_hr(model, fn, arg, &hr);
    if (r) CHECKF(!CX_FAILED(hr), "CoInitializeEx(%lx) failed: 0x%08lx", (unsigned long)model, (unsigned long)hr);
    return r;
}

int cx_pump_until(HANDLE h, DWORD ms)
{
    DWORD start = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - start, r;
        MSG msg;
        if (el >= ms) return WaitForSingleObject(h, 0) == WAIT_OBJECT_0;
        r = UU(MsgWaitForMultipleObjects)(1, &h, FALSE, ms - el, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return 1;
        if (r == WAIT_OBJECT_0 + 1) {
            while (UU(PeekMessageW)(&msg, NULL, 0, 0, PM_REMOVE)) { UU(TranslateMessage)(&msg); UU(DispatchMessageW)(&msg); }
        } else return 0;
    }
}
void cx_pump_some(DWORD ms)
{
    HANDLE h = cx_event(TRUE, FALSE);
    cx_pump_until(h, ms);
    CloseHandle(h);
}

/* ---------------- CxObj ---------------- */
static HRESULT WINAPI obj_QI(IUnknown *i, REFIID riid, void **ppv)
{
    CxObj *o = (CxObj *)i;
    InterlockedIncrement(&o->qi_calls);
    if (IsEqualGUID(riid, &CX_IID_IUnknown) || o->qi_all || (o->qi_cancel && IsEqualGUID(riid, &CX_IID_ICancelMethodCalls))) {
        *ppv = o; InterlockedIncrement(&o->ref); return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI obj_AddRef(IUnknown *i) { return InterlockedIncrement(&((CxObj *)i)->ref); }
static ULONG WINAPI obj_Release(IUnknown *i) { return InterlockedDecrement(&((CxObj *)i)->ref); }
static HRESULT WINAPI obj_Cancel(IUnknown *i, ULONG secs)
{
    CxObj *o = (CxObj *)i;
    InterlockedIncrement(&o->cancel_calls); o->last_cancel_secs = secs; o->cancel_tid = GetCurrentThreadId();
    if (o->on_cancel) SetEvent(o->on_cancel);
    return o->cancel_ret;
}
static HRESULT WINAPI obj_Test(IUnknown *i) { CxObj *o = (CxObj *)i; InterlockedIncrement(&o->test_calls); return o->test_ret; }
static const void *obj_vtbl[5] = { obj_QI, obj_AddRef, obj_Release, obj_Cancel, obj_Test };
void cx_obj_init(CxObj *o, int qi_cancel, int qi_all)
{
    memset(o, 0, sizeof *o);
    o->lpVtbl = obj_vtbl; o->ref = 1; o->qi_cancel = qi_cancel; o->qi_all = qi_all;
}

/* ---------------- CxFilter ---------------- */
static HRESULT WINAPI flt_QI(IMessageFilter *i, REFIID riid, void **ppv)
{
    CxFilter *f = (CxFilter *)i;
    InterlockedIncrement(&f->qi_calls);
    if (IsEqualGUID(riid, &CX_IID_IUnknown) || IsEqualGUID(riid, &CX_IID_IMessageFilter)) { *ppv = f; InterlockedIncrement(&f->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI flt_AddRef(IMessageFilter *i) { return InterlockedIncrement(&((CxFilter *)i)->ref); }
static ULONG WINAPI flt_Release(IMessageFilter *i) { return InterlockedDecrement(&((CxFilter *)i)->ref); }
static DWORD WINAPI flt_Incoming(IMessageFilter *i, DWORD calltype, HTASK caller, DWORD tick, LPINTERFACEINFO info)
{
    CxFilter *f = (CxFilter *)i;
    LONG n = InterlockedIncrement(&f->incoming);
    f->last_calltype = calltype; f->last_caller = (DWORD)(ULONG_PTR)caller; f->last_tick = tick;
    f->incoming_tid = GetCurrentThreadId();
    if (info) { f->last_info = *info; f->have_info = 1; } else f->have_info = 0;
    if (f->seq_len > 0) return f->ret_incoming_seq[(n - 1) < f->seq_len ? (n - 1) : f->seq_len - 1];
    return f->ret_incoming;
}
static DWORD WINAPI flt_Retry(IMessageFilter *i, HTASK callee, DWORD tick, DWORD type)
{
    CxFilter *f = (CxFilter *)i;
    InterlockedIncrement(&f->retry); f->last_reject_type = type; f->last_retry_callee = (DWORD)(ULONG_PTR)callee;
    return f->ret_retry;
}
static DWORD WINAPI flt_Pending(IMessageFilter *i, HTASK callee, DWORD tick, DWORD type)
{
    CxFilter *f = (CxFilter *)i;
    InterlockedIncrement(&f->pending);
    return f->ret_pending;
}
static const void *flt_vtbl[6] = { flt_QI, flt_AddRef, flt_Release, flt_Incoming, flt_Retry, flt_Pending };
void cx_filter_init(CxFilter *f)
{
    memset(f, 0, sizeof *f);
    f->lpVtbl = flt_vtbl; f->ref = 1;
    f->ret_incoming = 0 /* SERVERCALL_ISHANDLED */;
    f->ret_retry = (DWORD)-1;
    f->ret_pending = 1 /* PENDINGMSG_WAITDEFPROCESS */;
}

/* ---------------- CxCF (IClassFactory) ---------------- */
static HRESULT WINAPI cf_QI(IUnknown *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &CX_IID_IUnknown) || IsEqualGUID(riid, &CX_IID_IClassFactory)) { *ppv = i; InterlockedIncrement(&((CxCF *)i)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IUnknown *i) { return InterlockedIncrement(&((CxCF *)i)->ref); }
static ULONG WINAPI cf_Release(IUnknown *i) { return InterlockedDecrement(&((CxCF *)i)->ref); }
static HRESULT WINAPI cf_Create(IUnknown *i, IUnknown *outer, REFIID riid, void **ppv)
{
    CxCF *c = (CxCF *)i;
    InterlockedIncrement(&c->create_calls);
    *ppv = NULL; return E_NOTIMPL;
}
static HRESULT WINAPI cf_Lock(IUnknown *i, BOOL lock)
{
    CxCF *c = (CxCF *)i;
    InterlockedIncrement(&c->lock_calls);
    c->last_tid = GetCurrentThreadId();
    if (lock) InterlockedIncrement(&c->locks); else InterlockedDecrement(&c->locks);
    if (c->entered) SetEvent(c->entered);
    if (c->release) WaitForSingleObject(c->release, 9000);
    return c->lock_ret;
}
static const void *cf_vtbl[5] = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
void cx_cf_init(CxCF *c)
{
    memset(c, 0, sizeof *c);
    c->lpVtbl = cf_vtbl; c->ref = 1; c->lock_ret = S_OK;
}

/* ---------------- server thread ---------------- */
static DWORD WINAPI srv_thread(LPVOID p)
{
    CxSrv *s = p;
    s->tid = GetCurrentThreadId();
    s->init_hr = cx_init(s->model);
    if (!CX_FAILED(s->init_hr)) {
        if (s->model == CXM_STA && s->filter) {
            IMessageFilter *prev = NULL;
            s->filter_hr = U(CoRegisterMessageFilter)((IMessageFilter *)s->filter, &prev);
            if (prev) IMessageFilter_Release(prev);
        }
        if (s->on_start) s->on_start(s);
        s->marshal_hr = U(CoMarshalInterThreadInterfaceInStream)(&CX_IID_IClassFactory, (IUnknown *)&s->cf, &s->stream);
        SetEvent(s->ready);
        if (s->model == CXM_STA) cx_pump_until(s->stop, 60000);
        else WaitForSingleObject(s->stop, 60000);
        if (s->stream) {   /* never unmarshaled: drop the marshal data */
            LARGE_INTEGER z; z.QuadPart = 0;
            IStream_Seek(s->stream, z, STREAM_SEEK_SET, NULL);
            U(CoReleaseMarshalData)(s->stream);
            IStream_Release(s->stream); s->stream = NULL;
        }
        if (s->model == CXM_STA && s->filter) U(CoRegisterMessageFilter)(NULL, NULL);
        cx_uninit();
    } else SetEvent(s->ready);
    return 0;
}
int cx_srv_start(CxSrv *s, DWORD model, CxFilter *filter)
{
    void (*on_start)(CxSrv *) = s->on_start; void *user = s->user; HANDLE entered = s->cf.entered, rel = s->cf.release;
    memset(s, 0, sizeof *s);
    cx_cf_init(&s->cf); s->cf.entered = entered; s->cf.release = rel;
    s->on_start = on_start; s->user = user;
    s->model = model; s->filter = filter;
    s->ready = cx_event(TRUE, FALSE); s->stop = cx_event(TRUE, FALSE);
    s->thread = CreateThread(NULL, 0, srv_thread, s, 0, NULL);
    if (!s->thread) return 0;
    if (WaitForSingleObject(s->ready, 10000) != WAIT_OBJECT_0) return 0;
    return !CX_FAILED(s->init_hr) && !CX_FAILED(s->marshal_hr);
}
IClassFactory *cx_srv_proxy(CxSrv *s)
{
    void *p = NULL;
    HRESULT hr = U(CoGetInterfaceAndReleaseStream)(s->stream, &CX_IID_IClassFactory, &p);
    s->stream = NULL;
    return CX_FAILED(hr) ? NULL : (IClassFactory *)p;
}
void cx_srv_stop(CxSrv *s)
{
    if (s->stop) SetEvent(s->stop);
    if (s->thread) {
        if (WaitForSingleObject(s->thread, 10000) != WAIT_OBJECT_0) CHECKF(0, "server thread did not stop");
        CloseHandle(s->thread);
    }
    if (s->ready) CloseHandle(s->ready);
    if (s->stop) CloseHandle(s->stop);
    s->thread = s->ready = s->stop = NULL;
}

/* ---------------- parked thread ---------------- */
static DWORD WINAPI park_thread(LPVOID q)
{
    CxPark *p = q;
    APTTYPE t = (APTTYPE)-5; APTTYPEQUALIFIER ql = (APTTYPEQUALIFIER)-5;
    p->tid = GetCurrentThreadId();
    p->init_hr = S_OK;
    if (p->model != CXM_NONE) p->init_hr = cx_init(p->model);
    if (!CX_FAILED(p->init_hr)) {
        p->apt_hr = U(CoGetApartmentType)(&t, &ql); p->type = t; p->qual = ql;
        p->token = 0; p->tok_hr = U(CoGetContextToken)(&p->token);
        memset(&p->lid, 0, sizeof p->lid); p->lid_hr = U(CoGetCurrentLogicalThreadId)(&p->lid);
    }
    SetEvent(p->ready);
    if (!CX_FAILED(p->init_hr)) {
        if (p->model == CXM_STA) cx_pump_until(p->stop, 60000); else WaitForSingleObject(p->stop, 60000);
        if (p->model != CXM_NONE) cx_uninit();
    }
    return 0;
}
int cx_park_start(CxPark *p, DWORD model)
{
    memset(p, 0, sizeof *p);
    p->model = model; p->ready = cx_event(TRUE, FALSE); p->stop = cx_event(TRUE, FALSE);
    p->thread = CreateThread(NULL, 0, park_thread, p, 0, NULL);
    if (!p->thread) return 0;
    if (WaitForSingleObject(p->ready, 10000) != WAIT_OBJECT_0) return 0;
    return !CX_FAILED(p->init_hr);
}
void cx_park_stop(CxPark *p)
{
    if (p->stop) SetEvent(p->stop);
    if (p->thread) {
        if (WaitForSingleObject(p->thread, 10000) != WAIT_OBJECT_0) CHECKF(0, "parked thread did not stop");
        CloseHandle(p->thread);
    }
    if (p->ready) CloseHandle(p->ready);
    if (p->stop) CloseHandle(p->stop);
    p->thread = p->ready = p->stop = NULL;
}
