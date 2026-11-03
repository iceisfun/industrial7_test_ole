#define INITGUID
#include "xp.h"
#include <initguid.h>

#define G(n, a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) const GUID n = {a,b,c,{d0,d1,d2,d3,d4,d5,d6,d7}}
G(XP_IID_IUnknown,        0x00000000,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IClassFactory,   0x00000001,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IMalloc,         0x00000002,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IMarshal,        0x00000003,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IStdMarshalInfo, 0x00000018,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IExternalConnection, 0x00000019,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IMultiQI,        0x00000020,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_ICancelMethodCalls, 0x00000029,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IServerSecurity, 0x0000013E,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IClientSecurity, 0x0000013D,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IMessageFilter,  0x00000016,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IStream,         0x0000000C,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_ISequentialStream, 0x0C733A30,0x2A1C,0x11CE,0xAD,0xE5,0x00,0xAA,0x00,0x44,0x77,0x3D);
G(XP_IID_IStorage,        0x0000000B,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IPersist,        0x0000010C,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IEnumString,     0x00000101,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IEnumUnknown,    0x00000100,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IEnumFORMATETC,  0x00000103,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IBindCtx,        0x0000000E,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IMoniker,        0x0000000F,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IParseDisplayName, 0x0000011A,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IOleItemContainer, 0x0000011C,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IDispatch,       0x00020400,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IEnumVARIANT,    0x00020404,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IProvideClassInfo, 0xB196B283,0xBAB4,0x101A,0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07);
G(XP_IID_IRemUnknown,     0x00000131,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IUnknownBogus,   0xC0DE0001,0x7E57,0x4A11,0x9B,0x01,0x0F,0x7E,0x57,0x00,0x00,0x01);
G(XP_IID_IXpBogus2,       0xC0DE0002,0x7E57,0x4A11,0x9B,0x01,0x0F,0x7E,0x57,0x00,0x00,0x02);
G(XP_CLSID_StdMarshal,    0x00000017,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_CLSID_PSFactoryBuffer, 0x00000320,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IGlobalInterfaceTable, 0x00000146,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_CLSID_StdGlobalInterfaceTable, 0x00000323,0,0,0xC0,0,0,0,0,0,0,0x46);
G(XP_IID_IPSFactoryBuffer, 0xD5F569D0,0x593B,0x101A,0xB5,0x69,0x08,0x00,0x2B,0x2D,0xBF,0x7A);
G(XP_IID_IRpcStubBuffer,  0xD5F56AFC,0x593B,0x101A,0xB5,0x69,0x08,0x00,0x2B,0x2D,0xBF,0x7A);
G(XP_IID_IRpcProxyBuffer, 0xD5F56A34,0x593B,0x101A,0xB5,0x69,0x08,0x00,0x2B,0x2D,0xBF,0x7A);
G(XP_IID_ICallInterceptor, 0x60C7CA75,0x896D,0x11D2,0xB8,0xB6,0x00,0xC0,0x4F,0xB9,0x61,0x8A);
G(XP_IID_ICallFrameEvents, 0xFD5E0843,0xFC91,0x11D0,0x97,0xD7,0x00,0xC0,0x4F,0xB9,0x61,0x8A);
G(XP_IID_ICallFrame,      0xD573B4B0,0x894E,0x11D2,0xB8,0xB6,0x00,0xC0,0x4F,0xB9,0x61,0x8A);

volatile LONG xp_live_objs;

/* ------------------------------------------------------------------------- */
static XpObj *O(IClassFactory *f) { return (XpObj *)f; }
static HRESULT WINAPI o_qi(IClassFactory *f, REFIID riid, void **ppv)
{
    XpObj *o = O(f);
    if (!ppv) return E_POINTER;
    InterlockedIncrement(&o->qiCalls);
    o->lastQi = *riid;
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_IClassFactory)) {
        *ppv = f; InterlockedIncrement(&o->ref); return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI o_addref(IClassFactory *f) { return InterlockedIncrement(&O(f)->ref); }
static ULONG WINAPI o_release(IClassFactory *f)
{
    XpObj *o = O(f);
    LONG r = InterlockedDecrement(&o->ref);
    if (!r) {
        if (o->destroyedFlag) InterlockedExchange(o->destroyedFlag, 1);
        InterlockedDecrement(&xp_live_objs);
        HeapFree(GetProcessHeap(), 0, o);
    }
    return r;
}
static void record_caller(XpObj *o)
{
    DWORD tid = 0xFFFFFFFF; HRESULT hr;
    o->lastTid = GetCurrentThreadId();
    if (U(CoGetCallerTID)) { hr = U(CoGetCallerTID)(&tid); o->lastCallerTidHr = hr; o->lastCallerTid = tid; }
}
static HRESULT WINAPI o_create(IClassFactory *f, IUnknown *outer, REFIID riid, void **ppv)
{
    XpObj *o = O(f);
    InterlockedIncrement(&o->createCalls);
    record_caller(o);
    o->lastCreateIid = *riid;
    InterlockedExchange(&o->lastCreateOuterNull, outer == NULL);
    if (ppv) *ppv = NULL;
    if (o->hook) o->hook(o, XP_HOOK_CREATE, 0);
    if (riid->Data1 == 0xC0DE5000 && riid->Data2 == 0x7E57) { HRESULT enc; memcpy(&enc, riid->Data4, 4); return enc; }  /* IID that encodes the HRESULT to return */
    if (IsEqualGUID(riid, &XP_IID_IXpBogus2)) return o->probeHr;
    if (o->createHr != S_OK) return o->createHr;
    if (!ppv) return E_POINTER;
    if (o->extra && IsEqualGUID(riid, &XP_IID_IUnknown)) { IUnknown_AddRef(o->extra); *ppv = o->extra; return S_OK; }
    return f->lpVtbl->QueryInterface(f, riid, ppv);
}
static HRESULT WINAPI o_lock(IClassFactory *f, BOOL fLock)
{
    XpObj *o = O(f);
    InterlockedIncrement(&o->lockCalls);
    InterlockedExchange(&o->lastLock, fLock);
    record_caller(o);
    if (o->hook) o->hook(o, XP_HOOK_LOCK, fLock);
    return fLock ? o->lockHrTrue : o->lockHrFalse;
}
static const IClassFactoryVtbl o_vtbl = { o_qi, o_addref, o_release, o_create, o_lock };

XpObj *xp_obj_new(void)
{
    XpObj *o = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *o);
    if (!o) return NULL;
    o->iface.lpVtbl = (IClassFactoryVtbl *)&o_vtbl; o->ref = 1;
    o->lockHrTrue = S_OK; o->lockHrFalse = S_FALSE; o->createHr = S_OK; o->probeHr = S_OK;
    InterlockedIncrement(&xp_live_objs);
    return o;
}
XpObj *xp_obj_from(IUnknown *u) { return (XpObj *)u; }

/* ---- object with IExternalConnection ------------------------------------------------ */
typedef struct { XpObj base; IExternalConnection ec; } XpExtObj;
static XpExtObj *EX(IExternalConnection *e) { return (XpExtObj *)((char *)e - offsetof(XpExtObj, ec)); }
static HRESULT WINAPI e_qi(IExternalConnection *e, REFIID riid, void **ppv)
{
    XpExtObj *x = EX(e);
    if (IsEqualGUID(riid, &XP_IID_IExternalConnection)) { *ppv = e; InterlockedIncrement(&x->base.ref); return S_OK; }
    return x->base.iface.lpVtbl->QueryInterface(&x->base.iface, riid, ppv);
}
static ULONG WINAPI e_addref(IExternalConnection *e) { return InterlockedIncrement(&EX(e)->base.ref); }
static ULONG WINAPI e_release(IExternalConnection *e) { return EX(e)->base.iface.lpVtbl->Release(&EX(e)->base.iface); }
static DWORD WINAPI e_add(IExternalConnection *e, DWORD ext, DWORD reserved) { InterlockedIncrement(&EX(e)->base.extConnAdd); return 1; }
static DWORD WINAPI e_rel(IExternalConnection *e, DWORD ext, DWORD reserved, BOOL last) { InterlockedIncrement(&EX(e)->base.extConnRel); return 0; }
static const IExternalConnectionVtbl e_vtbl = { e_qi, e_addref, e_release, e_add, e_rel };
static HRESULT WINAPI x_qi(IClassFactory *f, REFIID riid, void **ppv)
{
    XpExtObj *x = (XpExtObj *)f;
    if (IsEqualGUID(riid, &XP_IID_IExternalConnection)) { *ppv = &x->ec; InterlockedIncrement(&x->base.ref); InterlockedIncrement(&x->base.qiCalls); return S_OK; }
    return o_qi(f, riid, ppv);
}
static IClassFactoryVtbl x_vtbl;
XpObj *xp_obj_new_extconn(void)
{
    XpExtObj *x = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *x);
    if (!x) return NULL;
    x_vtbl = o_vtbl; x_vtbl.QueryInterface = x_qi;
    x->base.iface.lpVtbl = &x_vtbl; x->base.ref = 1; x->ec.lpVtbl = (IExternalConnectionVtbl *)&e_vtbl;
    x->base.lockHrTrue = S_OK; x->base.lockHrFalse = S_FALSE;
    InterlockedIncrement(&xp_live_objs);
    return &x->base;
}

/* ---- apartment host ------------------------------------------------------------------ */
void xp_pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    for (;;) {
        MSG m; DWORD now = GetTickCount();
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        if ((LONG)(end - now) <= 0) break;
        MsgWaitForMultipleObjects(0, NULL, FALSE, min(end - now, 10), QS_ALLINPUT);
    }
}
BOOL xp_wait_pump(HANDLE ev, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    for (;;) {
        MSG m; LONG left = (LONG)(end - GetTickCount());
        if (left <= 0) return WaitForSingleObject(ev, 0) == WAIT_OBJECT_0;
        DWORD r = MsgWaitForMultipleObjects(1, &ev, FALSE, left, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return TRUE;
        if (r == WAIT_TIMEOUT) return FALSE;
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
    }
}
IStream *xp_new_stream(void)
{
    IStream *s = NULL;
    HRESULT hr = U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    return SUCCEEDED(hr) ? s : NULL;
}
void xp_rewind(IStream *s) { LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL); }
ULONG xp_refcount(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static DWORD WINAPI host_proc(void *p)
{
    XpHost *h = p;
    HANDLE hs[2];
    DWORD deadline = GetTickCount() + 60000;
    h->initHr = U(CoInitializeEx)(NULL, h->model);
    if (FAILED(h->initHr)) { SetEvent(h->hReady); return 1; }
    if (h->filter && h->model == COINIT_APARTMENTTHREADED && U(CoRegisterMessageFilter)) {
        IMessageFilter *old = NULL;
        U(CoRegisterMessageFilter)(h->filter, &old);
        if (old) IMessageFilter_Release(old);
    }
    if (!h->noMarshal && h->obj) {
        h->marshalHr = U(CoMarshalInterface)(h->stream, &h->iid, h->obj, h->mshctx, NULL, h->mshlflags);
    }
    SetEvent(h->hReady);
    hs[0] = h->hStop; hs[1] = h->hWork;
    for (;;) {
        DWORD r; MSG m; LONG left = (LONG)(deadline - GetTickCount());
        if (left <= 0) { h->tooLong = 1; break; }
        if (h->model == COINIT_APARTMENTTHREADED)
            r = MsgWaitForMultipleObjects(2, hs, FALSE, left, QS_ALLINPUT);
        else
            r = WaitForMultipleObjects(2, hs, FALSE, left);
        if (r == WAIT_OBJECT_0) break;
        if (r == WAIT_OBJECT_0 + 1) { h->work(h->workArg); SetEvent(h->hWorkDone); continue; }
        if (r == WAIT_OBJECT_0 + 2) { while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); } continue; }
        if (r == WAIT_TIMEOUT) { h->tooLong = 1; break; }
        break;
    }
    if (h->onStop) h->onStop(h->onStopArg);
    if (h->uninitOnStop) U(CoUninitialize)();
    return 0;
}

BOOL xp_host_start_ex(XpHost *h, DWORD model, IUnknown *obj, const GUID *iid, DWORD mshlflags, IMessageFilter *filter)
{
    memset(h, 0, sizeof *h);
    h->model = model; h->obj = obj; if (iid) h->iid = *iid; h->mshlflags = mshlflags; h->mshctx = MSHCTX_INPROC;
    h->uninitOnStop = 1; h->filter = filter; h->noMarshal = (obj == NULL);
    h->hReady = CreateEventA(NULL, TRUE, FALSE, NULL);
    h->hStop = CreateEventA(NULL, TRUE, FALSE, NULL);
    h->hWork = CreateEventA(NULL, FALSE, FALSE, NULL);
    h->hWorkDone = CreateEventA(NULL, FALSE, FALSE, NULL);
    h->stream = xp_new_stream();
    if (!h->stream) return FALSE;
    h->th = CreateThread(NULL, 0, host_proc, h, 0, &h->tid);
    if (!h->th) return FALSE;
    if (WaitForSingleObject(h->hReady, 15000) != WAIT_OBJECT_0) return FALSE;
    return SUCCEEDED(h->initHr) && (h->noMarshal || SUCCEEDED(h->marshalHr));
}
BOOL xp_host_start(XpHost *h, DWORD model, IUnknown *obj, const GUID *iid, DWORD mshlflags)
{ return xp_host_start_ex(h, model, obj, iid, mshlflags, NULL); }

BOOL xp_host_run(XpHost *h, XpWorkFn fn, void *arg)
{
    h->work = fn; h->workArg = arg;
    SetEvent(h->hWork);
    return WaitForSingleObject(h->hWorkDone, 10000) == WAIT_OBJECT_0;
}
void xp_host_stop(XpHost *h)
{
    if (h->th) {
        SetEvent(h->hStop);
        if (WaitForSingleObject(h->th, 15000) != WAIT_OBJECT_0) TerminateThread(h->th, 1);
        CloseHandle(h->th); h->th = NULL;
    }
    if (h->stream) { IStream_Release(h->stream); h->stream = NULL; }
    if (h->hReady) CloseHandle(h->hReady);
    if (h->hStop) CloseHandle(h->hStop);
    if (h->hWork) CloseHandle(h->hWork);
    if (h->hWorkDone) CloseHandle(h->hWorkDone);
    h->hReady = h->hStop = h->hWork = h->hWorkDone = NULL;
}
HRESULT xp_host_unmarshal(XpHost *h, const GUID *iid, void **out)
{
    xp_rewind(h->stream);
    return U(CoUnmarshalInterface)(h->stream, iid, out);
}

struct trun { int model; DWORD (*fn)(void *); void *arg; };
static DWORD WINAPI trun_proc(void *p)
{
    struct trun *t = p; DWORD r;
    if (t->model >= 0) { HRESULT hr = U(CoInitializeEx)(NULL, t->model); if (FAILED(hr)) return 0xFFFFFFF0; }
    r = t->fn(t->arg);
    if (t->model >= 0) U(CoUninitialize)();
    return r;
}
DWORD xp_thread_run(int model, DWORD (*fn)(void *), void *arg, DWORD timeout_ms)
{
    struct trun t = { model, fn, arg }; DWORD tid, code = 0xFFFFFFFF;
    HANDLE th = CreateThread(NULL, 0, trun_proc, &t, 0, &tid);
    if (!th) return 0xFFFFFFFE;
    if (WaitForSingleObject(th, timeout_ms) != WAIT_OBJECT_0) { TerminateThread(th, 1); CloseHandle(th); return 0xFFFFFFFF; }
    GetExitCodeThread(th, &code); CloseHandle(th);
    return code;
}

const char *xp_hrname(HRESULT hr)
{
    switch ((ULONG)hr) {
    case 0: return "S_OK"; case 1: return "S_FALSE"; case 0x80004002: return "E_NOINTERFACE";
    case 0x80070057: return "E_INVALIDARG"; case 0x80004005: return "E_FAIL"; case 0x80004001: return "E_NOTIMPL";
    case 0x80010108: return "RPC_E_DISCONNECTED"; case 0x8001010E: return "RPC_E_WRONG_THREAD";
    case 0x80010001: return "RPC_E_CALL_REJECTED"; case 0x800401F0: return "CO_E_NOTINITIALIZED";
    case 0x800401FD: return "CO_E_OBJNOTCONNECTED"; case 0x800706BA: return "RPC_S_SERVER_UNAVAILABLE";
    case 0x80010117: return "RPC_E_CALL_COMPLETE"; case 0x8001011E: return "RPC_E_NO_CONTEXT";
    case 0x80010140: return "RPC_E_CANCEL_DISABLED"; case 0x80004021: return "CO_E_NOT_SUPPORTED";
    case 0x80010105: return "RPC_E_SERVERFAULT"; case 0x800706BE: return "RPC_S_CALL_FAILED";
    }
    return "?";
}

/* ---- child process plumbing ------------------------------------------------------------ */
void xp_evname(char *out, size_t cap, DWORD pid, const char *leaf) { snprintf(out, cap, "Local\\" XP_PREFIX "_xp_%lu_%s", pid, leaf); }
HANDLE xp_event_create(DWORD pid, const char *leaf)
{
    char n[128]; xp_evname(n, sizeof n, pid, leaf);
    return CreateEventA(NULL, TRUE, FALSE, n);
}
void xp_parent_file(char *out, size_t cap, DWORD pid, const char *leaf)
{
    char base[MAX_PATH];
    if (!GetTempPathA(sizeof base, base)) strcpy(base, ".\\");
    snprintf(out, cap, "%s" XP_PREFIX "_%lu\\%s", base, pid, leaf);
}
BOOL xp_write_stream_to_file(IStream *s, const char *path)
{
    HGLOBAL hg = NULL; HANDLE f; STATSTG st; DWORD w; void *p; BOOL ok = FALSE;
    if (FAILED(U(GetHGlobalFromStream)(s, &hg))) return FALSE;
    if (FAILED(IStream_Stat(s, &st, STATFLAG_NONAME))) return FALSE;
    p = GlobalLock(hg); if (!p) return FALSE;
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) { ok = WriteFile(f, p, (DWORD)st.cbSize.QuadPart, &w, NULL) && w == st.cbSize.QuadPart; CloseHandle(f); }
    GlobalUnlock(hg);
    return ok;
}
HRESULT xp_stream_from_file(const char *path, IStream **out)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, rd; HGLOBAL hg; void *p; HRESULT hr;
    *out = NULL;
    if (f == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    sz = GetFileSize(f, NULL);
    hg = GlobalAlloc(GMEM_MOVEABLE, sz ? sz : 1);
    p = GlobalLock(hg);
    ReadFile(f, p, sz, &rd, NULL); CloseHandle(f);
    GlobalUnlock(hg);
    hr = U(CreateStreamOnHGlobal)(hg, TRUE, out);
    if (FAILED(hr)) GlobalFree(hg);
    return hr;
}
BOOL xp_stream_equal_bytes(IStream *a, IStream *b)
{
    STATSTG sa, sb; char ba[4096], bb[4096]; ULONG ra, rb;
    IStream_Stat(a, &sa, STATFLAG_NONAME); IStream_Stat(b, &sb, STATFLAG_NONAME);
    if (sa.cbSize.QuadPart != sb.cbSize.QuadPart) return FALSE;
    xp_rewind(a); xp_rewind(b);
    for (;;) {
        IStream_Read(a, ba, sizeof ba, &ra); IStream_Read(b, bb, sizeof bb, &rb);
        if (ra != rb || memcmp(ba, bb, ra)) return FALSE;
        if (!ra) return TRUE;
    }
}

BOOL xp_fx_setup(XpFx *f, DWORD hostModel, DWORD mshl)
{
    memset(f, 0, sizeof *f);
    f->mainTid = GetCurrentThreadId();
    f->o = xp_obj_new();
    if (!f->o) return FALSE;
    f->o->destroyedFlag = &f->gone;
    if (!xp_host_start(&f->h, hostModel, XP_OBJ_UNK(f->o), &XP_IID_IClassFactory, mshl)) return FALSE;
    f->unmarshalHr = xp_host_unmarshal(&f->h, &XP_IID_IClassFactory, (void **)&f->p);
    return SUCCEEDED(f->unmarshalHr) && f->p;
}
void xp_fx_teardown(XpFx *f)
{
    if (f->p) { IClassFactory_Release(f->p); f->p = NULL; }
    xp_host_stop(&f->h);
    if (f->o) { IClassFactory_Release(XP_OBJ_CF(f->o)); f->o = NULL; }
}

static void report_path(char *out, size_t cap, DWORD parentpid) { xp_parent_file(out, cap, parentpid, "xp_report.txt"); }
void xp_report(DWORD parentpid, const char *key, unsigned long long val)
{
    char path[MAX_PATH * 2], line[128]; DWORD w; HANDLE f;
    report_path(path, sizeof path, parentpid);
    f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    snprintf(line, sizeof line, "%s=%llx\n", key, val);
    WriteFile(f, line, (DWORD)strlen(line), &w, NULL);
    CloseHandle(f);
}
BOOL xp_report_get(DWORD parentpid, const char *key, unsigned long long *val)
{
    char path[MAX_PATH * 2], buf[8192], *p, pat[96]; DWORD rd = 0; HANDLE f; BOOL found = FALSE;
    report_path(path, sizeof path, parentpid);
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(f, buf, sizeof buf - 1, &rd, NULL); CloseHandle(f);
    buf[rd] = 0;
    snprintf(pat, sizeof pat, "%s=", key);
    for (p = buf; (p = strstr(p, pat)); p += 1) {   /* last occurrence wins */
        if (p == buf || p[-1] == '\n') { *val = strtoull(p + strlen(pat), NULL, 16); found = TRUE; }
    }
    return found;
}

GUID xp_hr_iid(HRESULT hr)
{
    GUID g = { 0xC0DE5000, 0x7E57, 0x4A11, { 0, 0, 0, 0, 0xA5, 0xA5, 0xA5, 0xA5 } };
    memcpy(g.Data4, &hr, 4);
    return g;
}
