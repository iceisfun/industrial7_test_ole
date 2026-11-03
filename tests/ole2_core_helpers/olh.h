/* Shared helpers for the ole2_core_helpers area (OLE2 non-UI helpers).
 * Every ole32/oleaut32 entry point is resolved with t.h's U()/UIN() (LoadLibrary + GetProcAddress).
 * Interface methods are invoked through vtables (CINTERFACE + COBJMACROS). The test objects below are
 * hand-written COM objects used to observe how ole32 drives the interfaces it is handed. */
#ifndef OLH_H
#define OLH_H
#define COBJMACROS
#define CINTERFACE
#define INITGUID
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <olectl.h>
#include <ocidl.h>
#include <oleauto.h>
#include <stdlib.h>

/* Single-evaluation equality check (the harness EXPECT_EQ evaluates its arguments twice). */
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
#define O2_N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* OLE/COM HRESULTs spelled out so a header mismatch cannot hide a wrong expectation. */
#define O2_E_INVALIDARG          ((HRESULT)0x80070057)
#define O2_E_HANDLE              ((HRESULT)0x80070006)
#define O2_E_NOINTERFACE         ((HRESULT)0x80004002)
#define O2_E_NOTIMPL             ((HRESULT)0x80004001)
#define O2_E_FAIL                ((HRESULT)0x80004005)
#define O2_E_POINTER             ((HRESULT)0x80004003)
#define O2_CO_E_NOTINITIALIZED   ((HRESULT)0x800401F0)
#define O2_RPC_E_CHANGED_MODE    ((HRESULT)0x80010106)
#define O2_RPC_E_WRONG_THREAD    ((HRESULT)0x8001010E)
#define O2_REGDB_E_CLASSNOTREG   ((HRESULT)0x80040154)
#define O2_OLE_E_NOCONNECTION    ((HRESULT)0x80040004)
#define O2_OLE_E_BLANK           ((HRESULT)0x80040007)
#define O2_OLE_E_NOTRUNNING      ((HRESULT)0x80040005)
#define O2_OLE_E_INVALIDHWND     ((HRESULT)0x8004000F)
#define O2_OLE_E_CANT_BINDTOSOURCE ((HRESULT)0x8004000A)
#define O2_DV_E_FORMATETC        ((HRESULT)0x80040064)
#define O2_DV_E_LINDEX           ((HRESULT)0x80040068)
#define O2_DV_E_TYMED            ((HRESULT)0x80040069)
#define O2_DV_E_CLIPFORMAT       ((HRESULT)0x8004006A)
#define O2_DV_E_DVASPECT         ((HRESULT)0x8004006B)
#define O2_DV_E_NOIVIEWOBJECT    ((HRESULT)0x8004006D)
#define O2_DRAGDROP_E_NOTREGISTERED     ((HRESULT)0x80040100)
#define O2_DRAGDROP_E_ALREADYREGISTERED ((HRESULT)0x80040101)
#define O2_DRAGDROP_E_INVALIDHWND       ((HRESULT)0x80040102)
#define O2_CACHE_S_FORMATETC_NOTSUPPORTED ((HRESULT)0x00040170)
#define O2_CACHE_S_SAMECACHE      ((HRESULT)0x00040171)
#define O2_CACHE_E_NOCACHE_UPDATED ((HRESULT)0x80040170)
#define O2_OLE_S_STATIC           ((HRESULT)0x00040001)

static const GUID O2_CLSID_A = {0x6f3a2c11, 0x5b7d, 0x4e0a, {0x9c, 0x31, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x01}};
static const GUID O2_CLSID_B = {0x6f3a2c12, 0x5b7d, 0x4e0a, {0x9c, 0x31, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x02}};
static const GUID O2_IID_BOGUS = {0x6f3a2c13, 0x5b7d, 0x4e0a, {0x9c, 0x31, 0x70, 0x6c, 0x65, 0x74, 0x65, 0x03}};

static int o2_guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof(GUID)); }
#define O2_IS_IID(r, I) o2_guid_eq((r), &(I))

/* ---- message pump / windows / threads ------------------------------------------------------ */
static void o2_pump(DWORD ms) {
    DWORD end = GetTickCount() + ms; MSG m; int guard = 0;
    for (;;) {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE) && guard++ < 10000) { TranslateMessage(&m); DispatchMessageA(&m); }
        if ((LONG)(GetTickCount() - end) >= 0) break;
        MsgWaitForMultipleObjects(0, NULL, FALSE, 5, QS_ALLINPUT);
    }
}

static volatile LONG o2_wm_count, o2_wm_last_msg;
static volatile LPARAM o2_wm_last_lparam;
static volatile WPARAM o2_wm_last_wparam;
static LRESULT CALLBACK o2_wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m >= WM_KEYFIRST && m <= WM_KEYLAST) { InterlockedIncrement(&o2_wm_count); o2_wm_last_msg = m; o2_wm_last_wparam = w; o2_wm_last_lparam = l; }
    return DefWindowProcA(h, m, w, l);
}
/* Hidden top-level window owned by the calling thread. */
static HWND o2_mkwnd(void) {
    static int reg;
    if (!reg) {
        WNDCLASSA wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = o2_wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "olet_o2core";
        RegisterClassA(&wc); reg = 1;
    }
    return CreateWindowExA(0, "olet_o2core", "olet", WS_POPUP, 0, 0, 16, 16, NULL, NULL, GetModuleHandleA(NULL), NULL);
}

typedef struct { void (*fn)(void *); void *arg; } o2_thr;
static DWORD WINAPI o2_thr_main(LPVOID p) { o2_thr *t = (o2_thr *)p; t->fn(t->arg); return 0; }
/* Run fn(arg) on a fresh thread (fresh apartment state); bounded wait. Returns 1 when it finished. */
static int o2_run_thread(void (*fn)(void *), void *arg, DWORD ms) {
    o2_thr t; DWORD id; HANDLE h; int ok;
    t.fn = fn; t.arg = arg;
    h = CreateThread(NULL, 0, o2_thr_main, &t, 0, &id);
    if (!h) return 0;
    ok = WaitForSingleObject(h, ms) == WAIT_OBJECT_0;
    if (!ok) { TerminateThread(h, 1); }
    CloseHandle(h);
    return ok;
}

/* OleInitialize/OleUninitialize bracket for tests that need an STA with OLE. */
#define O2_BEGIN do { REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); \
    HRESULT ih_ = U(OleInitialize)(NULL); EQ(ih_, S_OK); } while (0)
#define O2_END() U(OleUninitialize)()

/* ---- IEnumFORMATETC over a static list ------------------------------------------------------ */
typedef struct { IEnumFORMATETC i; LONG ref; FORMATETC f[16]; int n, pos; } O2Enum;
static HRESULT STDMETHODCALLTYPE oe_QI(IEnumFORMATETC *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IEnumFORMATETC)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE oe_AddRef(IEnumFORMATETC *s) { return InterlockedIncrement(&((O2Enum *)s)->ref); }
static ULONG STDMETHODCALLTYPE oe_Release(IEnumFORMATETC *s) { O2Enum *e = (O2Enum *)s; LONG r = InterlockedDecrement(&e->ref); if (!r) free(e); return r; }
static HRESULT STDMETHODCALLTYPE oe_Next(IEnumFORMATETC *s, ULONG c, FORMATETC *out, ULONG *got) {
    O2Enum *e = (O2Enum *)s; ULONG n = 0;
    while (n < c && e->pos < e->n) out[n++] = e->f[e->pos++];
    if (got) *got = n;
    return n == c ? S_OK : S_FALSE;
}
static HRESULT STDMETHODCALLTYPE oe_Skip(IEnumFORMATETC *s, ULONG c) { O2Enum *e = (O2Enum *)s; e->pos += c; if (e->pos > e->n) { e->pos = e->n; return S_FALSE; } return S_OK; }
static HRESULT STDMETHODCALLTYPE oe_Reset(IEnumFORMATETC *s) { ((O2Enum *)s)->pos = 0; return S_OK; }
static HRESULT STDMETHODCALLTYPE oe_Clone(IEnumFORMATETC *s, IEnumFORMATETC **pp) {
    O2Enum *e = (O2Enum *)s, *c = (O2Enum *)malloc(sizeof *c); *c = *e; c->ref = 1; *pp = &c->i; return S_OK;
}
static IEnumFORMATETCVtbl oe_vtbl = { oe_QI, oe_AddRef, oe_Release, oe_Next, oe_Skip, oe_Reset, oe_Clone };

/* ---- IDataObject over an HGLOBAL table ------------------------------------------------------- */
#define O2D_MAX 12
typedef struct {
    IDataObject i; LONG ref;
    FORMATETC f[O2D_MAX]; const char *payload[O2D_MAX]; int n;
    LONG getdata, qgd, enumcalls, getdatahere, setdata, dadvise;
    int enum_hr_override; HRESULT enum_hr;  /* make EnumFormatEtc fail */
    HRESULT getdata_hr;                     /* when nonzero, GetData fails with this */
} O2Data;
static int od_find(O2Data *d, const FORMATETC *f) {
    int k;
    for (k = 0; k < d->n; k++)
        if (d->f[k].cfFormat == f->cfFormat && (d->f[k].tymed & f->tymed) && (d->f[k].dwAspect & f->dwAspect)) return k;
    return -1;
}
static HRESULT STDMETHODCALLTYPE od_QI(IDataObject *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IDataObject)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE od_AddRef(IDataObject *s) { return InterlockedIncrement(&((O2Data *)s)->ref); }
static ULONG STDMETHODCALLTYPE od_Release(IDataObject *s) { return InterlockedDecrement(&((O2Data *)s)->ref); }
static HRESULT STDMETHODCALLTYPE od_GetData(IDataObject *s, FORMATETC *f, STGMEDIUM *m) {
    O2Data *d = (O2Data *)s; int k; HGLOBAL h; size_t len; void *p;
    InterlockedIncrement(&d->getdata);
    if (d->getdata_hr) return d->getdata_hr;
    k = od_find(d, f);
    if (k < 0 || !(f->tymed & TYMED_HGLOBAL) || !d->payload[k]) return DV_E_FORMATETC;
    len = strlen(d->payload[k]) + 1;
    h = GlobalAlloc(GMEM_MOVEABLE, len); p = GlobalLock(h); memcpy(p, d->payload[k], len); GlobalUnlock(h);
    memset(m, 0, sizeof *m); m->tymed = TYMED_HGLOBAL; m->hGlobal = h; m->pUnkForRelease = NULL;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE od_GetDataHere(IDataObject *s, FORMATETC *f, STGMEDIUM *m) { InterlockedIncrement(&((O2Data *)s)->getdatahere); return DV_E_FORMATETC; }
static HRESULT STDMETHODCALLTYPE od_QueryGetData(IDataObject *s, FORMATETC *f) {
    O2Data *d = (O2Data *)s; InterlockedIncrement(&d->qgd);
    return od_find(d, f) >= 0 ? S_OK : DV_E_FORMATETC;
}
static HRESULT STDMETHODCALLTYPE od_GetCanonicalFormatEtc(IDataObject *s, FORMATETC *a, FORMATETC *b) { *b = *a; b->ptd = NULL; return DATA_S_SAMEFORMATETC; }
static HRESULT STDMETHODCALLTYPE od_SetData(IDataObject *s, FORMATETC *f, STGMEDIUM *m, BOOL rel) { InterlockedIncrement(&((O2Data *)s)->setdata); return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE od_EnumFormatEtc(IDataObject *s, DWORD dir, IEnumFORMATETC **pp) {
    O2Data *d = (O2Data *)s; O2Enum *e; int k;
    InterlockedIncrement(&d->enumcalls);
    if (d->enum_hr_override) { *pp = NULL; return d->enum_hr; }
    if (dir != DATADIR_GET) { *pp = NULL; return E_NOTIMPL; }
    e = (O2Enum *)calloc(1, sizeof *e); e->i.lpVtbl = &oe_vtbl; e->ref = 1; e->n = d->n;
    for (k = 0; k < d->n; k++) e->f[k] = d->f[k];
    *pp = &e->i; return S_OK;
}
static HRESULT STDMETHODCALLTYPE od_DAdvise(IDataObject *s, FORMATETC *f, DWORD a, IAdviseSink *k, DWORD *c) { InterlockedIncrement(&((O2Data *)s)->dadvise); if (c) *c = 0; return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT STDMETHODCALLTYPE od_DUnadvise(IDataObject *s, DWORD c) { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT STDMETHODCALLTYPE od_EnumDAdvise(IDataObject *s, IEnumSTATDATA **pp) { *pp = NULL; return OLE_E_ADVISENOTSUPPORTED; }
static IDataObjectVtbl od_vtbl = { od_QI, od_AddRef, od_Release, od_GetData, od_GetDataHere, od_QueryGetData,
    od_GetCanonicalFormatEtc, od_SetData, od_EnumFormatEtc, od_DAdvise, od_DUnadvise, od_EnumDAdvise };
static void od_init(O2Data *d) { memset(d, 0, sizeof *d); d->i.lpVtbl = &od_vtbl; d->ref = 1; }
static void od_add(O2Data *d, CLIPFORMAT cf, DWORD tymed, DWORD aspect, const char *payload) {
    FORMATETC *f = &d->f[d->n];
    f->cfFormat = cf; f->ptd = NULL; f->dwAspect = aspect; f->lindex = -1; f->tymed = tymed;
    d->payload[d->n] = payload; d->n++;
}

/* ---- IAdviseSink ------------------------------------------------------------------------------ */
typedef struct {
    IAdviseSink i; LONG ref;
    LONG ondata, onview, onrename, onsave, onclose;
    FORMATETC last_fe; DWORD last_tymed; DWORD last_view_aspect; LONG last_view_index; IMoniker *last_mk;
    int data_valid; HGLOBAL last_hg;
} O2Sink;
static HRESULT STDMETHODCALLTYPE os_QI(IAdviseSink *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IAdviseSink)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE os_AddRef(IAdviseSink *s) { return InterlockedIncrement(&((O2Sink *)s)->ref); }
static ULONG STDMETHODCALLTYPE os_Release(IAdviseSink *s) { return InterlockedDecrement(&((O2Sink *)s)->ref); }
static void STDMETHODCALLTYPE os_OnDataChange(IAdviseSink *s, FORMATETC *f, STGMEDIUM *m) {
    O2Sink *k = (O2Sink *)s; InterlockedIncrement(&k->ondata);
    if (f) k->last_fe = *f;
    k->last_tymed = m ? m->tymed : 0xFFFFFFFF;
    k->last_hg = (m && m->tymed == TYMED_HGLOBAL) ? m->hGlobal : NULL;
}
static void STDMETHODCALLTYPE os_OnViewChange(IAdviseSink *s, DWORD a, LONG i) { O2Sink *k = (O2Sink *)s; InterlockedIncrement(&k->onview); k->last_view_aspect = a; k->last_view_index = i; }
static void STDMETHODCALLTYPE os_OnRename(IAdviseSink *s, IMoniker *mk) { O2Sink *k = (O2Sink *)s; InterlockedIncrement(&k->onrename); k->last_mk = mk; }
static void STDMETHODCALLTYPE os_OnSave(IAdviseSink *s) { InterlockedIncrement(&((O2Sink *)s)->onsave); }
static void STDMETHODCALLTYPE os_OnClose(IAdviseSink *s) { InterlockedIncrement(&((O2Sink *)s)->onclose); }
static IAdviseSinkVtbl os_vtbl = { os_QI, os_AddRef, os_Release, os_OnDataChange, os_OnViewChange, os_OnRename, os_OnSave, os_OnClose };
static void os_init(O2Sink *k) { memset(k, 0, sizeof *k); k->i.lpVtbl = &os_vtbl; k->ref = 1; }

/* ---- IRunnableObject (also IUnknown) ----------------------------------------------------------- */
typedef struct {
    IRunnableObject i; LONG ref; LONG qi_runnable, qi_other;
    int expose_runnable;
    HRESULT run_hr, lock_hr, contained_hr, getclass_hr; BOOL running;
    LONG run_calls, isrunning_calls, lock_calls, contained_calls;
    IBindCtx *run_ctx; BOOL last_lock, last_unlock_closes, last_contained;
} O2Run;
static HRESULT STDMETHODCALLTYPE or_QI(IRunnableObject *s, REFIID r, void **pp) {
    O2Run *o = (O2Run *)s;
    if (O2_IS_IID(r, IID_IUnknown)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    if (O2_IS_IID(r, IID_IRunnableObject)) {
        InterlockedIncrement(&o->qi_runnable);
        if (!o->expose_runnable) { *pp = NULL; return E_NOINTERFACE; }
        *pp = s; s->lpVtbl->AddRef(s); return S_OK;
    }
    InterlockedIncrement(&o->qi_other);
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE or_AddRef(IRunnableObject *s) { return InterlockedIncrement(&((O2Run *)s)->ref); }
static ULONG STDMETHODCALLTYPE or_Release(IRunnableObject *s) { return InterlockedDecrement(&((O2Run *)s)->ref); }
static HRESULT STDMETHODCALLTYPE or_GetRunningClass(IRunnableObject *s, LPCLSID c) { O2Run *o = (O2Run *)s; if (c) *c = O2_CLSID_A; return o->getclass_hr; }
static HRESULT STDMETHODCALLTYPE or_Run(IRunnableObject *s, LPBINDCTX c) { O2Run *o = (O2Run *)s; InterlockedIncrement(&o->run_calls); o->run_ctx = c; return o->run_hr; }
static BOOL STDMETHODCALLTYPE or_IsRunning(IRunnableObject *s) { O2Run *o = (O2Run *)s; InterlockedIncrement(&o->isrunning_calls); return o->running; }
static HRESULT STDMETHODCALLTYPE or_LockRunning(IRunnableObject *s, BOOL l, BOOL c) { O2Run *o = (O2Run *)s; InterlockedIncrement(&o->lock_calls); o->last_lock = l; o->last_unlock_closes = c; return o->lock_hr; }
static HRESULT STDMETHODCALLTYPE or_SetContained(IRunnableObject *s, BOOL c) { O2Run *o = (O2Run *)s; InterlockedIncrement(&o->contained_calls); o->last_contained = c; return o->contained_hr; }
static IRunnableObjectVtbl or_vtbl = { or_QI, or_AddRef, or_Release, or_GetRunningClass, or_Run, or_IsRunning, or_LockRunning, or_SetContained };
static void or_init(O2Run *o) { memset(o, 0, sizeof *o); o->i.lpVtbl = &or_vtbl; o->ref = 1; o->expose_runnable = 1; o->running = TRUE; }

/* ---- IViewObject (records Draw arguments) ------------------------------------------------------- */
typedef struct {
    IViewObject i; LONG ref; LONG draws; HRESULT draw_hr;
    DWORD aspect; LONG lindex; HDC hdc; const RECTL *bounds; void *pvAspect; DVTARGETDEVICE *ptd; HDC hicTarget; const RECTL *wbounds;
    BOOL (STDMETHODCALLTYPE *cont)(ULONG_PTR); ULONG_PTR dwcont;
} O2View;
static HRESULT STDMETHODCALLTYPE ov_QI(IViewObject *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IViewObject)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE ov_AddRef(IViewObject *s) { return InterlockedIncrement(&((O2View *)s)->ref); }
static ULONG STDMETHODCALLTYPE ov_Release(IViewObject *s) { return InterlockedDecrement(&((O2View *)s)->ref); }
static HRESULT STDMETHODCALLTYPE ov_Draw(IViewObject *s, DWORD a, LONG li, void *pv, DVTARGETDEVICE *ptd, HDC hic, HDC hdc,
        LPCRECTL bnd, LPCRECTL wb, BOOL (STDMETHODCALLTYPE *cont)(ULONG_PTR), ULONG_PTR dc) {
    O2View *v = (O2View *)s; InterlockedIncrement(&v->draws);
    v->aspect = a; v->lindex = li; v->pvAspect = pv; v->ptd = ptd; v->hicTarget = hic; v->hdc = hdc; v->bounds = bnd; v->wbounds = wb; v->cont = cont; v->dwcont = dc;
    return v->draw_hr;
}
static HRESULT STDMETHODCALLTYPE ov_GetColorSet(IViewObject *s, DWORD a, LONG l, void *p, DVTARGETDEVICE *t, HDC h, LPLOGPALETTE *pp) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ov_Freeze(IViewObject *s, DWORD a, LONG l, void *p, DWORD *f) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ov_Unfreeze(IViewObject *s, DWORD f) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ov_SetAdvise(IViewObject *s, DWORD a, DWORD f, IAdviseSink *k) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE ov_GetAdvise(IViewObject *s, DWORD *a, DWORD *f, IAdviseSink **k) { return E_NOTIMPL; }
static IViewObjectVtbl ov_vtbl = { ov_QI, ov_AddRef, ov_Release, ov_Draw, ov_GetColorSet, ov_Freeze, ov_Unfreeze, ov_SetAdvise, ov_GetAdvise };
static void ov_init(O2View *v) { memset(v, 0, sizeof *v); v->i.lpVtbl = &ov_vtbl; v->ref = 1; }

/* ---- IDropTarget ---------------------------------------------------------------------------- */
typedef struct { IDropTarget i; LONG ref; LONG enter, over, leave, drop; } O2Drop;
static HRESULT STDMETHODCALLTYPE odt_QI(IDropTarget *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IDropTarget)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE odt_AddRef(IDropTarget *s) { return InterlockedIncrement(&((O2Drop *)s)->ref); }
static ULONG STDMETHODCALLTYPE odt_Release(IDropTarget *s) { return InterlockedDecrement(&((O2Drop *)s)->ref); }
static HRESULT STDMETHODCALLTYPE odt_Enter(IDropTarget *s, IDataObject *d, DWORD k, POINTL p, DWORD *e) { InterlockedIncrement(&((O2Drop *)s)->enter); *e = DROPEFFECT_NONE; return S_OK; }
static HRESULT STDMETHODCALLTYPE odt_Over(IDropTarget *s, DWORD k, POINTL p, DWORD *e) { InterlockedIncrement(&((O2Drop *)s)->over); *e = DROPEFFECT_NONE; return S_OK; }
static HRESULT STDMETHODCALLTYPE odt_Leave(IDropTarget *s) { InterlockedIncrement(&((O2Drop *)s)->leave); return S_OK; }
static HRESULT STDMETHODCALLTYPE odt_Drop(IDropTarget *s, IDataObject *d, DWORD k, POINTL p, DWORD *e) { InterlockedIncrement(&((O2Drop *)s)->drop); *e = DROPEFFECT_NONE; return S_OK; }
static IDropTargetVtbl odt_vtbl = { odt_QI, odt_AddRef, odt_Release, odt_Enter, odt_Over, odt_Leave, odt_Drop };
static void odt_init(O2Drop *d) { memset(d, 0, sizeof *d); d->i.lpVtbl = &odt_vtbl; d->ref = 1; }

/* ---- IDropSource ---------------------------------------------------------------------------- */
typedef struct { IDropSource i; LONG ref; LONG qcd, gf; HRESULT qcd_hr; } O2Src;
static HRESULT STDMETHODCALLTYPE osr_QI(IDropSource *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IDropSource)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE osr_AddRef(IDropSource *s) { return InterlockedIncrement(&((O2Src *)s)->ref); }
static ULONG STDMETHODCALLTYPE osr_Release(IDropSource *s) { return InterlockedDecrement(&((O2Src *)s)->ref); }
static HRESULT STDMETHODCALLTYPE osr_QCD(IDropSource *s, BOOL esc, DWORD keys) { O2Src *o = (O2Src *)s; InterlockedIncrement(&o->qcd); return o->qcd_hr; }
static HRESULT STDMETHODCALLTYPE osr_GF(IDropSource *s, DWORD e) { InterlockedIncrement(&((O2Src *)s)->gf); return DRAGDROP_S_USEDEFAULTCURSORS; }
static IDropSourceVtbl osr_vtbl = { osr_QI, osr_AddRef, osr_Release, osr_QCD, osr_GF };
static void osr_init(O2Src *d) { memset(d, 0, sizeof *d); d->i.lpVtbl = &osr_vtbl; d->ref = 1; d->qcd_hr = DRAGDROP_S_CANCEL; }

/* ---- IOleClientSite -------------------------------------------------------------------------- */
typedef struct { IOleClientSite i; LONG ref; LONG calls; } O2Site;
static HRESULT STDMETHODCALLTYPE osi_QI(IOleClientSite *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IOleClientSite)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE osi_AddRef(IOleClientSite *s) { return InterlockedIncrement(&((O2Site *)s)->ref); }
static ULONG STDMETHODCALLTYPE osi_Release(IOleClientSite *s) { return InterlockedDecrement(&((O2Site *)s)->ref); }
static HRESULT STDMETHODCALLTYPE osi_Save(IOleClientSite *s) { InterlockedIncrement(&((O2Site *)s)->calls); return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE osi_GetMk(IOleClientSite *s, DWORD a, DWORD w, IMoniker **m) { InterlockedIncrement(&((O2Site *)s)->calls); *m = NULL; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE osi_GetCont(IOleClientSite *s, IOleContainer **c) { InterlockedIncrement(&((O2Site *)s)->calls); *c = NULL; return E_NOINTERFACE; }
static HRESULT STDMETHODCALLTYPE osi_Show(IOleClientSite *s) { InterlockedIncrement(&((O2Site *)s)->calls); return S_OK; }
static HRESULT STDMETHODCALLTYPE osi_OnShow(IOleClientSite *s, BOOL f) { InterlockedIncrement(&((O2Site *)s)->calls); return S_OK; }
static HRESULT STDMETHODCALLTYPE osi_ReqLayout(IOleClientSite *s) { InterlockedIncrement(&((O2Site *)s)->calls); return E_NOTIMPL; }
static IOleClientSiteVtbl osi_vtbl = { osi_QI, osi_AddRef, osi_Release, osi_Save, osi_GetMk, osi_GetCont, osi_Show, osi_OnShow, osi_ReqLayout };
static void osi_init(O2Site *d) { memset(d, 0, sizeof *d); d->i.lpVtbl = &osi_vtbl; d->ref = 1; }

/* ---- IPersistStorage ------------------------------------------------------------------------- */
typedef struct {
    IPersistStorage i; LONG ref; CLSID clsid; HRESULT classid_hr, save_hr, initnew_hr, load_hr;
    LONG getclass, save, savecompleted, initnew, load, dirty; BOOL last_same_as_load; IStorage *save_stg;
} O2Pers;
static HRESULT STDMETHODCALLTYPE op_QI(IPersistStorage *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IPersist) || O2_IS_IID(r, IID_IPersistStorage)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE op_AddRef(IPersistStorage *s) { return InterlockedIncrement(&((O2Pers *)s)->ref); }
static ULONG STDMETHODCALLTYPE op_Release(IPersistStorage *s) { return InterlockedDecrement(&((O2Pers *)s)->ref); }
static HRESULT STDMETHODCALLTYPE op_GetClassID(IPersistStorage *s, CLSID *c) { O2Pers *o = (O2Pers *)s; InterlockedIncrement(&o->getclass); if (o->classid_hr) return o->classid_hr; *c = o->clsid; return S_OK; }
static HRESULT STDMETHODCALLTYPE op_IsDirty(IPersistStorage *s) { InterlockedIncrement(&((O2Pers *)s)->dirty); return S_OK; }
static HRESULT STDMETHODCALLTYPE op_InitNew(IPersistStorage *s, IStorage *g) { O2Pers *o = (O2Pers *)s; InterlockedIncrement(&o->initnew); return o->initnew_hr; }
static HRESULT STDMETHODCALLTYPE op_Load(IPersistStorage *s, IStorage *g) { O2Pers *o = (O2Pers *)s; InterlockedIncrement(&o->load); return o->load_hr; }
static HRESULT STDMETHODCALLTYPE op_Save(IPersistStorage *s, IStorage *g, BOOL same) { O2Pers *o = (O2Pers *)s; InterlockedIncrement(&o->save); o->last_same_as_load = same; o->save_stg = g; return o->save_hr; }
static HRESULT STDMETHODCALLTYPE op_SaveCompleted(IPersistStorage *s, IStorage *g) { InterlockedIncrement(&((O2Pers *)s)->savecompleted); return S_OK; }
static HRESULT STDMETHODCALLTYPE op_Hands(IPersistStorage *s) { return S_OK; }
static IPersistStorageVtbl op_vtbl = { op_QI, op_AddRef, op_Release, op_GetClassID, op_IsDirty, op_InitNew, op_Load, op_Save, op_SaveCompleted, op_Hands };
static void op_init(O2Pers *d) { memset(d, 0, sizeof *d); d->i.lpVtbl = &op_vtbl; d->ref = 1; d->clsid = O2_CLSID_A; }

/* ---- IOleInPlaceFrame ------------------------------------------------------------------------ */
typedef struct { IOleInPlaceFrame i; LONG ref; LONG ta_calls; HRESULT ta_hr; MSG ta_msg; WORD ta_id; } O2Frame;
static HRESULT STDMETHODCALLTYPE of_QI(IOleInPlaceFrame *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IOleInPlaceFrame)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE of_AddRef(IOleInPlaceFrame *s) { return InterlockedIncrement(&((O2Frame *)s)->ref); }
static ULONG STDMETHODCALLTYPE of_Release(IOleInPlaceFrame *s) { return InterlockedDecrement(&((O2Frame *)s)->ref); }
static HRESULT STDMETHODCALLTYPE of_GetWindow(IOleInPlaceFrame *s, HWND *h) { *h = NULL; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_CSH(IOleInPlaceFrame *s, BOOL f) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_GetBorder(IOleInPlaceFrame *s, LPRECT r) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_ReqBorder(IOleInPlaceFrame *s, LPCBORDERWIDTHS b) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_SetBorder(IOleInPlaceFrame *s, LPCBORDERWIDTHS b) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_SetActive(IOleInPlaceFrame *s, IOleInPlaceActiveObject *a, LPCOLESTR n) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_InsMenus(IOleInPlaceFrame *s, HMENU h, LPOLEMENUGROUPWIDTHS w) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_SetMenu(IOleInPlaceFrame *s, HMENU h, HOLEMENU o, HWND w) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_RemMenus(IOleInPlaceFrame *s, HMENU h) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_SetStatus(IOleInPlaceFrame *s, LPCOLESTR t) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_EnableModeless(IOleInPlaceFrame *s, BOOL f) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE of_TA(IOleInPlaceFrame *s, LPMSG m, WORD id) { O2Frame *f = (O2Frame *)s; InterlockedIncrement(&f->ta_calls); f->ta_msg = *m; f->ta_id = id; return f->ta_hr; }
static IOleInPlaceFrameVtbl of_vtbl = { of_QI, of_AddRef, of_Release, of_GetWindow, of_CSH, of_GetBorder, of_ReqBorder, of_SetBorder,
    of_SetActive, of_InsMenus, of_SetMenu, of_RemMenus, of_SetStatus, of_EnableModeless, of_TA };
static void of_init(O2Frame *d) { memset(d, 0, sizeof *d); d->i.lpVtbl = &of_vtbl; d->ref = 1; d->ta_hr = S_FALSE; }

/* ---- misc ---------------------------------------------------------------------------------- */
static IStorage *o2_mkstg(void) {
    IStorage *stg = NULL;
    if (!U(StgCreateDocfile)) return NULL;
    if (FAILED(U(StgCreateDocfile)(NULL, STGM_READWRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg))) return NULL;
    return stg;
}
static HGLOBAL o2_hg(const void *data, size_t n, UINT flags) {
    HGLOBAL h = GlobalAlloc(flags, n ? n : 1); void *p;
    if (!h) return NULL;
    p = GlobalLock(h); if (data) memcpy(p, data, n); else memset(p, 0, n); GlobalUnlock(h);
    return h;
}
#endif
