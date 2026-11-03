#include "mb.h"
#include <stdlib.h>

const GUID MB_CLSID_Custom   = {0x6f1e7a10, 0x4d5b, 0x4c3a, {0x9a, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x01}};
const GUID MB_CLSID_Handler  = {0x6f1e7a10, 0x4d5b, 0x4c3a, {0x9a, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x02}};
const GUID MB_IID_Bogus      = {0x6f1e7a10, 0x4d5b, 0x4c3a, {0x9a, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x03}};
const GUID MB_CLSID_StdMarshal = {0x00000017, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
const GUID MB_CLSID_StdGIT   = {0x00000323, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
const GUID MB_CLSID_InProcFTM = {0x0000033A, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

/* ======================= test object ======================= */
MBObj g_obj[2];

static MBObj *from_cf(IClassFactory *i) { return (MBObj *)((char *)i - offsetof(MBObj, cf)); }
static MBObj *from_ec(IExternalConnection *i) { return (MBObj *)((char *)i - offsetof(MBObj, ec)); }

static HRESULT WINAPI Obj_QI(IClassFactory *iface, REFIID riid, void **ppv) {
    MBObj *o = from_cf(iface);
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory)) *ppv = &o->cf;
    else if (IsEqualGUID(riid, &IID_IExternalConnection)) {
        InterlockedIncrement((LONG *)&o->qi_extconn);
        if (o->with_ec) *ppv = &o->ec;
    }
    if (!*ppv) return E_NOINTERFACE;
    InterlockedIncrement(&o->ref);
    return S_OK;
}
static ULONG WINAPI Obj_AddRef(IClassFactory *iface) { return InterlockedIncrement(&from_cf(iface)->ref); }
static ULONG WINAPI Obj_Release(IClassFactory *iface) { return InterlockedDecrement(&from_cf(iface)->ref); }
static HRESULT WINAPI Obj_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv) {
    MBObj *o = from_cf(iface);
    o->last_tid = GetCurrentThreadId();
    InterlockedIncrement(&o->ci_calls);
    if (ppv) *ppv = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI Obj_LockServer(IClassFactory *iface, BOOL lock) {
    MBObj *o = from_cf(iface);
    o->last_tid = GetCurrentThreadId();
    if (lock) InterlockedIncrement(&o->locks); else InterlockedDecrement(&o->locks);
    return S_OK;
}
static const IClassFactoryVtbl obj_cf_vtbl = { Obj_QI, Obj_AddRef, Obj_Release, Obj_CreateInstance, Obj_LockServer };

static HRESULT WINAPI Ec_QI(IExternalConnection *i, REFIID riid, void **ppv) { return Obj_QI(&from_ec(i)->cf, riid, ppv); }
static ULONG WINAPI Ec_AddRef(IExternalConnection *i) { return Obj_AddRef(&from_ec(i)->cf); }
static ULONG WINAPI Ec_Release(IExternalConnection *i) { return Obj_Release(&from_ec(i)->cf); }
static DWORD WINAPI Ec_Add(IExternalConnection *i, DWORD conn, DWORD reserved) {
    MBObj *o = from_ec(i);
    InterlockedIncrement(&o->add_conn);
    if (conn & EXTCONN_STRONG) return InterlockedIncrement(&o->ext);
    return InterlockedIncrement(&o->ext_weak);
}
static DWORD WINAPI Ec_Rel(IExternalConnection *i, DWORD conn, DWORD reserved, BOOL last) {
    MBObj *o = from_ec(i);
    InterlockedIncrement(&o->rel_conn);
    o->last_close = last;
    if (conn & EXTCONN_STRONG) return InterlockedDecrement(&o->ext);
    return InterlockedDecrement(&o->ext_weak);
}
static const IExternalConnectionVtbl obj_ec_vtbl = { Ec_QI, Ec_AddRef, Ec_Release, Ec_Add, Ec_Rel };

void mb_obj_reset(int with_ec) {
    for (int i = 0; i < 2; i++) {
        memset(&g_obj[i], 0, sizeof g_obj[i]);
        g_obj[i].cf.lpVtbl = &obj_cf_vtbl;
        g_obj[i].ec.lpVtbl = &obj_ec_vtbl;
        g_obj[i].ref = 1;
        g_obj[i].with_ec = with_ec;
    }
}

/* ======================= instrumented stream ======================= */
static MBStm *from_s(IStream *i) { return (MBStm *)i; }
static HRESULT WINAPI S_QI(IStream *i, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IStream) || IsEqualGUID(riid, &IID_ISequentialStream)) {
        *ppv = i; InterlockedIncrement(&from_s(i)->ref); return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI S_AddRef(IStream *i) { return InterlockedIncrement(&from_s(i)->ref); }
static ULONG WINAPI S_Release(IStream *i) {
    MBStm *s = from_s(i);
    LONG r = InterlockedDecrement(&s->ref);
    if (!r) { s->destroyed = TRUE; IStream_Release(s->inner); /* leaked on purpose: the test inspects 'destroyed' */ }
    return r;
}
static HRESULT WINAPI S_Read(IStream *i, void *p, ULONG cb, ULONG *pcb) {
    MBStm *s = from_s(i);
    s->reads++; s->last_read_cb = cb;
    if (s->fail_read) { if (pcb) *pcb = 0; return STG_E_ACCESSDENIED; }
    if (s->short_read_max && cb > s->short_read_max) cb = s->short_read_max;
    return IStream_Read(s->inner, p, cb, pcb);
}
static HRESULT WINAPI S_Write(IStream *i, const void *p, ULONG cb, ULONG *pcb) {
    MBStm *s = from_s(i);
    s->writes++; s->last_write_cb = cb;
    if (s->fail_write) { if (pcb) *pcb = 0; return STG_E_ACCESSDENIED; }
    return IStream_Write(s->inner, p, cb, pcb);
}
static HRESULT WINAPI S_Seek(IStream *i, LARGE_INTEGER mv, DWORD org, ULARGE_INTEGER *np) { from_s(i)->seeks++; return IStream_Seek(from_s(i)->inner, mv, org, np); }
static HRESULT WINAPI S_SetSize(IStream *i, ULARGE_INTEGER n) { from_s(i)->other++; return IStream_SetSize(from_s(i)->inner, n); }
static HRESULT WINAPI S_CopyTo(IStream *i, IStream *d, ULARGE_INTEGER cb, ULARGE_INTEGER *r, ULARGE_INTEGER *w) { from_s(i)->other++; return IStream_CopyTo(from_s(i)->inner, d, cb, r, w); }
static HRESULT WINAPI S_Commit(IStream *i, DWORD f) { from_s(i)->other++; return IStream_Commit(from_s(i)->inner, f); }
static HRESULT WINAPI S_Revert(IStream *i) { from_s(i)->other++; return IStream_Revert(from_s(i)->inner); }
static HRESULT WINAPI S_Lock(IStream *i, ULARGE_INTEGER o, ULARGE_INTEGER c, DWORD t) { from_s(i)->other++; return STG_E_INVALIDFUNCTION; }
static HRESULT WINAPI S_Unlock(IStream *i, ULARGE_INTEGER o, ULARGE_INTEGER c, DWORD t) { from_s(i)->other++; return STG_E_INVALIDFUNCTION; }
static HRESULT WINAPI S_Stat(IStream *i, STATSTG *st, DWORD f) { from_s(i)->stats++; return IStream_Stat(from_s(i)->inner, st, f); }
static HRESULT WINAPI S_Clone(IStream *i, IStream **o) { from_s(i)->clones++; return IStream_Clone(from_s(i)->inner, o); }
static const IStreamVtbl s_vtbl = { S_QI, S_AddRef, S_Release, S_Read, S_Write, S_Seek, S_SetSize, S_CopyTo, S_Commit, S_Revert, S_Lock, S_Unlock, S_Stat, S_Clone };

MBStm *mb_wstream_new(void) {
    MBStm *s = calloc(1, sizeof *s);
    s->s.lpVtbl = &s_vtbl; s->ref = 1;
    if (FAILED(U(CreateStreamOnHGlobal)(NULL, TRUE, &s->inner))) { free(s); return NULL; }
    return s;
}

/* ======================= stream helpers ======================= */
IStream *mb_stream(void) { IStream *s = NULL; HRESULT hr = U(CreateStreamOnHGlobal)(NULL, TRUE, &s); return SUCCEEDED(hr) ? s : NULL; }
void mb_rewind(IStream *s) { LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL); }
ULONGLONG mb_pos(IStream *s) { LARGE_INTEGER z; ULARGE_INTEGER p; z.QuadPart = 0; p.QuadPart = ~0ull; IStream_Seek(s, z, STREAM_SEEK_CUR, &p); return p.QuadPart; }
ULONGLONG mb_size(IStream *s) { STATSTG st; memset(&st, 0, sizeof st); if (FAILED(IStream_Stat(s, &st, STATFLAG_NONAME))) return ~0ull; return st.cbSize.QuadPart; }
BYTE *mb_dump(IStream *s, ULONG *len) {
    ULONGLONG sz = mb_size(s), pos = mb_pos(s);
    BYTE *b = malloc((size_t)sz + 1); ULONG got = 0;
    mb_rewind(s);
    if (sz) IStream_Read(s, b, (ULONG)sz, &got);
    { LARGE_INTEGER p; p.QuadPart = (LONGLONG)pos; IStream_Seek(s, p, STREAM_SEEK_SET, NULL); }
    *len = got; return b;
}

/* ======================= events + custom marshal ======================= */
MBEv g_ev[128];
int g_nev;
MBCustomCfg g_cm_cfg;
void mb_ev_reset(void) { g_nev = 0; memset(g_ev, 0, sizeof g_ev); }
int mb_ev_count(int kind) { int n = 0; for (int i = 0; i < g_nev; i++) if (g_ev[i].kind == kind) n++; return n; }
int mb_ev_index(int kind, int nth) { for (int i = 0; i < g_nev; i++) if (g_ev[i].kind == kind && nth-- == 0) return i; return -1; }
static void ev(int kind, int which, const GUID *iid, DWORD ctx, DWORD flags, void *pv, void *dctx) {
    if (g_nev >= 128) return;
    MBEv *e = &g_ev[g_nev++];
    e->kind = kind; e->which = which; if (iid) e->iid = *iid; e->ctx = ctx; e->flags = flags; e->pv = pv; e->dctx = dctx;
}

typedef struct CM { IMarshal m; LONG ref; int which; } CM;
static CM g_cm = { NULL, 1, 0 }, g_um = { NULL, 1, 1 };

static HRESULT WINAPI CM_QI(IMarshal *i, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IMarshal)) { *ppv = i; InterlockedIncrement(&((CM *)i)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI CM_AddRef(IMarshal *i) { return InterlockedIncrement(&((CM *)i)->ref); }
static ULONG WINAPI CM_Release(IMarshal *i) { return InterlockedDecrement(&((CM *)i)->ref); }
static HRESULT WINAPI CM_GUC(IMarshal *i, REFIID riid, void *pv, DWORD ctx, void *dctx, DWORD fl, CLSID *cid) {
    ev(EV_GUC, ((CM *)i)->which, riid, ctx, fl, pv, dctx);
    if (FAILED(g_cm_cfg.hr_guc)) return g_cm_cfg.hr_guc;
    *cid = g_cm_cfg.clsid; return g_cm_cfg.hr_guc;
}
static HRESULT WINAPI CM_GMSM(IMarshal *i, REFIID riid, void *pv, DWORD ctx, void *dctx, DWORD fl, DWORD *sz) {
    ev(EV_GMSM, ((CM *)i)->which, riid, ctx, fl, pv, dctx);
    if (FAILED(g_cm_cfg.hr_gmsm)) return g_cm_cfg.hr_gmsm;
    *sz = g_cm_cfg.size; return g_cm_cfg.hr_gmsm;
}
static HRESULT WINAPI CM_MI(IMarshal *i, IStream *s, REFIID riid, void *pv, DWORD ctx, void *dctx, DWORD fl) {
    ev(EV_MI, ((CM *)i)->which, riid, ctx, fl, pv, dctx);
    if (FAILED(g_cm_cfg.hr_mi)) return g_cm_cfg.hr_mi;
    if (g_cm_cfg.plen) { HRESULT hr = IStream_Write(s, g_cm_cfg.payload, g_cm_cfg.plen, NULL); if (FAILED(hr)) return hr; }
    return g_cm_cfg.hr_mi;
}
static HRESULT WINAPI CM_UI(IMarshal *i, IStream *s, REFIID riid, void **ppv) {
    ev(EV_UI, ((CM *)i)->which, riid, 0, 0, NULL, NULL);
    if (FAILED(g_cm_cfg.hr_ui)) return g_cm_cfg.hr_ui;
    if (g_cm_cfg.plen) { BYTE tmp[256]; ULONG got = 0; HRESULT hr = IStream_Read(s, tmp, g_cm_cfg.plen, &got); if (FAILED(hr)) return hr; }
    if (!g_cm_cfg.unmarshaled_obj) { *ppv = NULL; return E_NOINTERFACE; }
    return IUnknown_QueryInterface((IUnknown *)g_cm_cfg.unmarshaled_obj, riid, ppv);
}
static HRESULT WINAPI CM_RMD(IMarshal *i, IStream *s) {
    ev(EV_RMD, ((CM *)i)->which, NULL, 0, 0, NULL, NULL);
    if (FAILED(g_cm_cfg.hr_rmd)) return g_cm_cfg.hr_rmd;
    if (g_cm_cfg.plen) { LARGE_INTEGER mv; mv.QuadPart = g_cm_cfg.plen; IStream_Seek(s, mv, STREAM_SEEK_CUR, NULL); }
    return g_cm_cfg.hr_rmd;
}
static HRESULT WINAPI CM_Disc(IMarshal *i, DWORD r) { ev(EV_DISC, ((CM *)i)->which, NULL, r, 0, NULL, NULL); return g_cm_cfg.hr_disc; }
static const IMarshalVtbl cm_vtbl = { CM_QI, CM_AddRef, CM_Release, CM_GUC, CM_GMSM, CM_MI, CM_UI, CM_RMD, CM_Disc };

void mb_cm_reset(void) {
    memset(&g_cm_cfg, 0, sizeof g_cm_cfg);
    g_cm_cfg.clsid = MB_CLSID_Custom;
    g_cm_cfg.size = 16; g_cm_cfg.plen = 16;
    memcpy(g_cm_cfg.payload, "0123456789ABCDEF", 16);
    g_cm.m.lpVtbl = &cm_vtbl; g_um.m.lpVtbl = &cm_vtbl;
    g_cm.ref = 1; g_um.ref = 1;
    g_cm_cfg.unmarshaled_obj = &g_cm.m;
    mb_ev_reset();
}
IUnknown *mb_cm_unk(void) { return (IUnknown *)&g_cm.m; }
IMarshal *mb_cm_marshal(void) { return &g_cm.m; }

/* unmarshaler class factory */
static IClassFactory g_um_cf;
static HRESULT WINAPI UF_QI(IClassFactory *i, REFIID riid, void **ppv) {
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory)) { *ppv = i; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI UF_AddRef(IClassFactory *i) { return 2; }
static ULONG WINAPI UF_Release(IClassFactory *i) { return 1; }
static HRESULT WINAPI UF_Create(IClassFactory *i, IUnknown *outer, REFIID riid, void **ppv) {
    ev(EV_CTOR_CREATE, 1, riid, 0, 0, outer, NULL);
    if (FAILED(g_cm_cfg.hr_factory)) { *ppv = NULL; return g_cm_cfg.hr_factory; }
    return CM_QI(&g_um.m, riid, ppv);
}
static HRESULT WINAPI UF_Lock(IClassFactory *i, BOOL l) { return S_OK; }
static const IClassFactoryVtbl uf_vtbl = { UF_QI, UF_AddRef, UF_Release, UF_Create, UF_Lock };
DWORD mb_cm_register(DWORD clsctx, DWORD regcls) {
    DWORD cookie = 0;
    g_um_cf.lpVtbl = &uf_vtbl;
    if (FAILED(U(CoRegisterClassObject)(&MB_CLSID_Custom, (IUnknown *)&g_um_cf, clsctx, regcls, &cookie))) return 0;
    return cookie;
}
void mb_cm_revoke(DWORD cookie) { if (cookie) U(CoRevokeClassObject)(cookie); }

/* ======================= IStdMarshalInfo object ======================= */
DWORD g_smi_last_ctx; int g_smi_calls; HRESULT g_smi_hr; GUID g_smi_clsid;
typedef struct SMI { IStdMarshalInfo i; LONG ref; } SMI;
static SMI g_smi;
static HRESULT WINAPI SMI_QI(IStdMarshalInfo *i, REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IStdMarshalInfo)) { *ppv = i; InterlockedIncrement(&g_smi.ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI SMI_AddRef(IStdMarshalInfo *i) { return InterlockedIncrement(&g_smi.ref); }
static ULONG WINAPI SMI_Release(IStdMarshalInfo *i) { return InterlockedDecrement(&g_smi.ref); }
static HRESULT WINAPI SMI_Get(IStdMarshalInfo *i, DWORD ctx, void *dctx, CLSID *c) {
    g_smi_calls++; g_smi_last_ctx = ctx; ev(EV_SMI_GCFH, 0, NULL, ctx, 0, NULL, dctx);
    if (SUCCEEDED(g_smi_hr)) *c = g_smi_clsid;
    return g_smi_hr;
}
static const IStdMarshalInfoVtbl smi_vtbl = { SMI_QI, SMI_AddRef, SMI_Release, SMI_Get };
IUnknown *mb_smi_unk(void) { g_smi.i.lpVtbl = &smi_vtbl; if (!g_smi.ref) g_smi.ref = 1; return (IUnknown *)&g_smi.i; }

/* ======================= OBJREF parser ======================= */
int mb_parse_objref(const BYTE *b, ULONG n, MBObjRef *o) {
    memset(o, 0, sizeof *o);
    if (n < 24) return 0;
    memcpy(&o->signature, b, 4); memcpy(&o->flags, b + 4, 4); memcpy(&o->iid, b + 8, 16);
    ULONG off = 24;
    if (o->flags == 1 || o->flags == 2) {
        if (n < off + 40) return 0;
        memcpy(&o->sflags, b + off, 4); memcpy(&o->crefs, b + off + 4, 4);
        memcpy(&o->oxid, b + off + 8, 8); memcpy(&o->oid, b + off + 16, 8); memcpy(&o->ipid, b + off + 24, 16);
        off += 40;
        if (o->flags == 2) { if (n < off + 16) return 0; memcpy(&o->clsid, b + off, 16); off += 16; }
        if (n < off + 4) return 0;
        memcpy(&o->nentries, b + off, 2); memcpy(&o->secoff, b + off + 2, 2); off += 4;
        o->dsa = b + off;
        if (n < off + 2u * o->nentries) return 0;
        off += 2u * o->nentries;
    } else if (o->flags == 4) {
        if (n < off + 24) return 0;
        memcpy(&o->clsid, b + off, 16); memcpy(&o->cbext, b + off + 16, 4); memcpy(&o->csize, b + off + 20, 4);
        off += 24; o->cdata = b + off; o->cdata_len = n - off;
    } else return 0;
    o->consumed = off;
    return 1;
}

/* ======================= threads ======================= */
HANDLE mb_thread(MBThreadFn fn, void *arg) { DWORD tid; return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, &tid); }
BOOL mb_wait_pump(HANDLE h, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    for (;;) {
        DWORD left = end - GetTickCount(); if ((LONG)left < 0) return FALSE;
        DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, left, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) return TRUE;
        if (r == WAIT_OBJECT_0 + 1) { MSG m; while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); } continue; }
        return FALSE;
    }
}
BOOL mb_join(HANDLE h, DWORD ms, DWORD *code) {
    BOOL ok; DWORD c = 0xDEAD;
    if (!h) return FALSE;
    ok = mb_wait_pump(h, ms);
    if (ok) GetExitCodeThread(h, &c); else TerminateThread(h, 0xDEAD);
    CloseHandle(h);
    if (code) *code = c;
    return ok;
}
