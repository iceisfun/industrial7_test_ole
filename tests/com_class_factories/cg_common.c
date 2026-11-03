#include "cg.h"
#define CG_DEF(n) __typeof__(&n) p##n;
CG_CORE(CG_DEF)
int cg_api(void) {
    static int done, ok;
    if (!done) {
        done = 1; ok = 1;
#define CG_LOAD(n) p##n = U(n); if (!p##n) { t_note("missing export %s", #n); ok = 0; }
        CG_CORE(CG_LOAD)
    }
    if (!ok) t_skip("core COM exports missing");
    return ok;
}
LONG cg_live_objects;
ULONG cg_release(IUnknown *p) { return p ? p->lpVtbl->Release(p) : 0; }

/* ---- object ---- */
static CGObj *from_inner(IUnknown *i) { return (CGObj *)((char *)i - offsetof(CGObj, inner)); }
static CGObj *from_pf(IPersistFile *i) { return (CGObj *)((char *)i - offsetof(CGObj, pf)); }
static CGObj *from_ps(IPersistStorage *i) { return (CGObj *)((char *)i - offsetof(CGObj, ps)); }
static ULONG WINAPI in_AddRef(IUnknown *i);
static HRESULT WINAPI in_QI(IUnknown *i, REFIID riid, void **ppv) {
    CGObj *o = from_inner(i);
    *ppv = NULL;
    if (IsEqualGUID(riid, &cg_IID_IUnknown)) *ppv = &o->inner;
    else if (IsEqualGUID(riid, &cg_IID_IPersist) || IsEqualGUID(riid, &cg_IID_IPersistFile)) *ppv = &o->pf;
    else if (IsEqualGUID(riid, &cg_IID_IPersistStorage)) *ppv = &o->ps;
    else return E_NOINTERFACE;
    if (*ppv == (void *)&o->inner) in_AddRef(&o->inner);          /* non-delegating */
    else ((IUnknown *)*ppv)->lpVtbl->AddRef((IUnknown *)*ppv);    /* other interfaces delegate to the outer unknown */
    return S_OK;
}
static ULONG WINAPI in_AddRef(IUnknown *i) { return InterlockedIncrement(&from_inner(i)->ref); }
static ULONG WINAPI in_Release(IUnknown *i) {
    CGObj *o = from_inner(i); LONG r = InterlockedDecrement(&o->ref);
    if (!r) { if (o->stg) o->stg->lpVtbl->Release(o->stg); InterlockedDecrement(&cg_live_objects); HeapFree(GetProcessHeap(), 0, o); }
    return r;
}
static IUnknownVtbl in_vtbl = { in_QI, in_AddRef, in_Release };
/* delegating wrappers: IUnknown part of pf/ps */
#define DELEG(P, FROM) \
static HRESULT WINAPI P##_QI(P##_t *i, REFIID r, void **p) { CGObj *o = FROM(i); IUnknown *u = o->outer ? o->outer : &o->inner; return u->lpVtbl->QueryInterface(u, r, p); } \
static ULONG WINAPI P##_AddRef(P##_t *i) { CGObj *o = FROM(i); IUnknown *u = o->outer ? o->outer : &o->inner; return u->lpVtbl->AddRef(u); } \
static ULONG WINAPI P##_Release(P##_t *i) { CGObj *o = FROM(i); IUnknown *u = o->outer ? o->outer : &o->inner; return u->lpVtbl->Release(u); }
typedef IPersistFile pf_t; typedef IPersistStorage ps_t;
DELEG(pf, from_pf)
DELEG(ps, from_ps)
static HRESULT WINAPI pf_GetClassID(IPersistFile *i, CLSID *c) { *c = from_pf(i)->clsid; return S_OK; }
static HRESULT WINAPI pf_IsDirty(IPersistFile *i) { return S_FALSE; }
static HRESULT WINAPI pf_Load(IPersistFile *i, LPCOLESTR n, DWORD m) {
    CGObj *o = from_pf(i); lstrcpynW(o->file, n ? n : L"", MAX_PATH); o->fmode = m; o->fileLoaded++; return o->pfLoadHr; }
static HRESULT WINAPI pf_Save(IPersistFile *i, LPCOLESTR n, BOOL r) { return S_OK; }
static HRESULT WINAPI pf_SaveCompleted(IPersistFile *i, LPCOLESTR n) { return S_OK; }
static HRESULT WINAPI pf_GetCurFile(IPersistFile *i, LPOLESTR *n) { *n = NULL; return S_FALSE; }
static IPersistFileVtbl pf_vtbl = { pf_QI, pf_AddRef, pf_Release, pf_GetClassID, pf_IsDirty, pf_Load, pf_Save, pf_SaveCompleted, pf_GetCurFile };
static HRESULT WINAPI ps_GetClassID(IPersistStorage *i, CLSID *c) { *c = from_ps(i)->clsid; return S_OK; }
static HRESULT WINAPI ps_IsDirty(IPersistStorage *i) { return S_FALSE; }
static HRESULT WINAPI ps_InitNew(IPersistStorage *i, IStorage *s) { return S_OK; }
static HRESULT WINAPI ps_Load(IPersistStorage *i, IStorage *s) {
    CGObj *o = from_ps(i); o->stgLoaded++; o->stg = s; if (s) s->lpVtbl->AddRef(s); return o->psLoadHr; }
static HRESULT WINAPI ps_Save(IPersistStorage *i, IStorage *s, BOOL r) { return S_OK; }
static HRESULT WINAPI ps_SaveCompleted(IPersistStorage *i, IStorage *s) { return S_OK; }
static HRESULT WINAPI ps_HandsOff(IPersistStorage *i) { return S_OK; }
static IPersistStorageVtbl ps_vtbl = { ps_QI, ps_AddRef, ps_Release, ps_GetClassID, ps_IsDirty, ps_InitNew, ps_Load, ps_Save, ps_SaveCompleted, ps_HandsOff };

/* ---- factory ---- */
static CGFactory *from_cf(IClassFactory *i) { return (CGFactory *)i; }
static HRESULT WINAPI cf_QI(IClassFactory *i, REFIID riid, void **ppv) {
    CGFactory *f = from_cf(i); f->qis++; *ppv = NULL;
    if (IsEqualGUID(riid, &cg_IID_IUnknown) || IsEqualGUID(riid, &cg_IID_IClassFactory)) *ppv = i;
    else if (IsEqualGUID(riid, &cg_IID_IMarshal) && f->ftm) return f->ftm->lpVtbl->QueryInterface(f->ftm, riid, ppv);
    else return E_NOINTERFACE;
    i->lpVtbl->AddRef(i); return S_OK;
}
static ULONG WINAPI cf_AddRef(IClassFactory *i) { return InterlockedIncrement(&from_cf(i)->ref); }
static ULONG WINAPI cf_Release(IClassFactory *i) { return InterlockedDecrement(&from_cf(i)->ref); }
static HRESULT WINAPI cf_Create(IClassFactory *i, IUnknown *outer, REFIID riid, void **ppv) {
    CGFactory *f = from_cf(i); CGObj *o; HRESULT hr;
    f->creates++; f->lastIid = *riid; f->lastOuter = outer; f->lastObj = NULL;
    if (ppv) *ppv = NULL;
    if (FAILED(f->createHr)) return f->createHr;
    if (outer && !f->aggregable) return CLASS_E_NOAGGREGATION;
    if (outer && f->strictOuterIid && !IsEqualGUID(riid, &cg_IID_IUnknown)) return CLASS_E_NOAGGREGATION;
    o = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *o);
    if (!o) return E_OUTOFMEMORY;
    o->inner.lpVtbl = &in_vtbl; o->pf.lpVtbl = &pf_vtbl; o->ps.lpVtbl = &ps_vtbl;
    o->ref = 1; o->outer = outer; o->clsid = f->clsid; o->pfLoadHr = f->pfLoadHr; o->psLoadHr = f->psLoadHr;
    InterlockedIncrement(&cg_live_objects);
    f->lastObj = o;
    hr = o->inner.lpVtbl->QueryInterface(&o->inner, riid, ppv);
    if (SUCCEEDED(hr) && outer && !IsEqualGUID(riid, &cg_IID_IUnknown)) InterlockedDecrement(&cg_live_objects); /* illegal COM usage: inner leaks on purpose */
    else o->inner.lpVtbl->Release(&o->inner);
    if (FAILED(hr)) f->lastObj = NULL;
    return hr;
}
static HRESULT WINAPI cf_Lock(IClassFactory *i, BOOL l) {
    CGFactory *f = from_cf(i);
    if (l) { f->locks++; f->lockCalls++; } else { f->locks--; f->unlockCalls++; }
    return S_OK;
}
static IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
void cg_factory_init(CGFactory *f, const GUID *clsid) {
    memset(f, 0, sizeof *f);
    f->iface.lpVtbl = &cf_vtbl; f->ref = 1; f->aggregable = 1; f->strictOuterIid = 1; f->createHr = S_OK;
    f->clsid = *clsid; f->pfLoadHr = f->psLoadHr = S_OK;
}
HRESULT cg_register(CGFactory *f, DWORD clsctx, DWORD regcls, DWORD *cookie) {
    return pCoRegisterClassObject(&f->clsid, (IUnknown *)&f->iface, clsctx, regcls, cookie);
}
DWORD cg_run_thread(LPTHREAD_START_ROUTINE fn, void *arg) {
    DWORD code = 0xDEAD0001; HANDLE h = CreateThread(NULL, 0, fn, arg, 0, NULL);
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, 10000) == WAIT_OBJECT_0) GetExitCodeThread(h, &code);
    CloseHandle(h);
    return code;
}
