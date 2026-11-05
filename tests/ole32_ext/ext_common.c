#include "ext.h"

unsigned x_apt_type(void)
{
    typedef HRESULT (WINAPI *gat_t)(int *, int *);
    gat_t gat = (gat_t)t_proc("CoGetApartmentType");
    int ty = -1, q = -1;
    if (!gat) return 0xdead;
    return gat(&ty, &q) < 0 ? 0xffff : (unsigned)ty;
}

typedef struct { x_thread_fn fn; void *arg; } XThr;
static DWORD WINAPI x_thr(void *p) { XThr *t = (XThr *)p; t->fn(t->arg); return 0; }
void x_on_thread(x_thread_fn fn, void *arg)
{
    XThr t; HANDLE h;
    t.fn = fn; t.arg = arg;
    h = CreateThread(NULL, 0, x_thr, &t, 0, NULL);
    if (!h) { t_skip("CreateThread failed"); return; }
    WaitForSingleObject(h, 60000);
    CloseHandle(h);
}

static HRESULT WINAPI xo_qi(XObj *t, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown)) { *o = t; t->refs++; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI xo_ar(XObj *t) { return ++t->refs; }
static ULONG WINAPI xo_rl(XObj *t) { return --t->refs; }
static IUnknownVtbl xo_vtbl = { (void *)xo_qi, (void *)xo_ar, (void *)xo_rl };
void x_obj_init(XObj *o) { o->v = &xo_vtbl; o->refs = 1; }

static HRESULT WINAPI dt_qi(IDropTarget *t, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDropTarget)) { *o = t; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI dt_ar(IDropTarget *t) { return 2; }
static ULONG WINAPI dt_rl(IDropTarget *t) { return 1; }
static HRESULT WINAPI dt_de(IDropTarget *t, IDataObject *d, DWORD k, POINTL p, DWORD *e) { return S_OK; }
static HRESULT WINAPI dt_do(IDropTarget *t, DWORD k, POINTL p, DWORD *e) { return S_OK; }
static HRESULT WINAPI dt_dl(IDropTarget *t) { return S_OK; }
static HRESULT WINAPI dt_dr(IDropTarget *t, IDataObject *d, DWORD k, POINTL p, DWORD *e) { return S_OK; }
static IDropTargetVtbl dt_vtbl = { dt_qi, dt_ar, dt_rl, dt_de, dt_do, dt_dl, dt_dr };
static IDropTarget dt_obj = { &dt_vtbl };
IDropTarget *x_drop_target(void) { return &dt_obj; }

