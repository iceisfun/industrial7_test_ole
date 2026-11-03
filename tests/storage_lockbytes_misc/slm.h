/* Shared helpers for the storage_lockbytes_misc area.
 * No IID/CLSID symbols are linked (libuuid is not on the link line), so every GUID used here is
 * spelled out as a static constant with a G_ prefix. */
#ifndef SLM_H
#define SLM_H
#define COBJMACROS
#define CINTERFACE
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <oleauto.h>
#include <stdlib.h>

/* t.h's EXPECT_EQ/EXPECT_NE evaluate their arguments twice (once for the test, once for the message),
 * which repeats side effects such as AddRef() or a COM call. Use single-evaluation versions here. */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)

#define SLM_GUID(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    static const GUID name = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
SLM_GUID(G_IUnknown,        0x00000000, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IClassFactory,   0x00000001, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IStream,         0x0000000c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IStorage,        0x0000000b, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_ILockBytes,      0x0000000a, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IPersist,        0x0000010c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IPersistStream,  0x00000109, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IPersistStorage, 0x0000010a, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IFillLockBytes,  0x99caf010, 0x415e, 0x11cf, 0x88, 0x14, 0x00, 0xaa, 0x00, 0xb5, 0x69, 0xf5);
SLM_GUID(G_IOleCache,       0x0000011e, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IOleCache2,      0x00000128, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IDataObject,     0x0000010e, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IViewObject2,    0x00000127, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IConnectionPointContainer, 0xB196B284, 0xBAB4, 0x101A, 0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1D, 0x07);
SLM_GUID(G_CLSID_NULL,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
/* arbitrary private class ids used by these tests */
SLM_GUID(G_CLSID_SlmA, 0x5d1e0a01, 0x7a1b, 0x4c11, 0x9c, 0x5e, 0x10, 0x20, 0x30, 0x40, 0x50, 0x01);
SLM_GUID(G_CLSID_SlmB, 0x5d1e0a02, 0x7a1b, 0x4c11, 0x9c, 0x5e, 0x10, 0x20, 0x30, 0x40, 0x50, 0x02);
SLM_GUID(G_CLSID_SlmC, 0x5d1e0a03, 0x7a1b, 0x4c11, 0x9c, 0x5e, 0x10, 0x20, 0x30, 0x40, 0x50, 0x03);

#define STG_E_FILEALREADYEXISTS_ ((HRESULT)0x80030050L)
#define H_INVALIDARG    ((HRESULT)0x80070057L)
#define H_OUTOFMEMORY   ((HRESULT)0x8007000EL)
#define H_INVALIDPOINTER ((HRESULT)0x80030009L)
#define H_INVALIDFLAG   ((HRESULT)0x800300FFL)
#define H_INVALIDFUNC   ((HRESULT)0x80030001L)
#define H_INVALIDPARAM  ((HRESULT)0x80030057L)
#define H_ACCESSDENIED  ((HRESULT)0x80030005L)
#define H_UNIMPL        ((HRESULT)0x800300FEL)

#define SM_CREATE_RW   (STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE)
#define SM_OPEN_RW     (STGM_READWRITE | STGM_SHARE_EXCLUSIVE)
#define SM_OPEN_RO     (STGM_READ | STGM_SHARE_EXCLUSIVE)

/* ---- COM apartment helpers (balanced per test) ------------------------ */
static int slm_coinit(void) {
    HRESULT (WINAPI *p)(LPVOID, DWORD) = (void *)t_proc("CoInitializeEx");
    if (!p) return 0;
    HRESULT hr = p(NULL, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr);
}
static void slm_couninit(void) {
    void (WINAPI *p)(void) = (void *)t_proc("CoUninitialize");
    if (p) p();
}

/* ---- custom ILockBytes backed by malloc -------------------------------- */
enum { LB_READ = 1, LB_WRITE, LB_SETSIZE, LB_LOCK, LB_UNLOCK, LB_FLUSH, LB_STAT };
typedef struct { int op; ULONGLONG off; ULONG cb; } lb_rec;
typedef struct CLB {
    ILockBytes iface;
    LONG ref;
    BYTE *buf; ULONG size, cap;
    int nread, nwrite, nsetsize, nlock, nunlock, nflush, nstat;
    ULONGLONG maxend;
    lb_rec log[8192]; int nlog;
    HRESULT fail_read, fail_write, fail_setsize, fail_stat;   /* S_OK = none */
    DWORD lock_support;
    BOOL  log_ops;
} CLB;
static CLB *clb_from(ILockBytes *p) { return (CLB *)p; }
static void clb_log(CLB *c, int op, ULONGLONG off, ULONG cb) {
    if (c->log_ops && c->nlog < 8192) { c->log[c->nlog].op = op; c->log[c->nlog].off = off; c->log[c->nlog].cb = cb; c->nlog++; }
}
static HRESULT WINAPI clb_QI(ILockBytes *p, REFIID riid, void **pp) {
    if (IsEqualGUID(riid, &G_IUnknown) || IsEqualGUID(riid, &G_ILockBytes)) { *pp = p; clb_from(p)->ref++; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI clb_AddRef(ILockBytes *p) { return ++clb_from(p)->ref; }
static ULONG WINAPI clb_Release(ILockBytes *p) { return --clb_from(p)->ref; }  /* owned by the test, never freed here */
static HRESULT WINAPI clb_ReadAt(ILockBytes *p, ULARGE_INTEGER off, void *pv, ULONG cb, ULONG *pr) {
    CLB *c = clb_from(p); c->nread++; clb_log(c, LB_READ, off.QuadPart, cb);
    if (pr) *pr = 0;
    if (c->fail_read) return c->fail_read;
    ULONG n = 0;
    if (off.QuadPart < c->size) { ULONGLONG left = c->size - off.QuadPart; n = left < cb ? (ULONG)left : cb; memcpy(pv, c->buf + off.QuadPart, n); }
    if (pr) *pr = n;
    return S_OK;
}
static HRESULT clb_grow(CLB *c, ULONGLONG ns) {
    if (ns > 64ull * 1024 * 1024) return STG_E_MEDIUMFULL;
    if (ns > c->cap) {
        ULONG nc = c->cap ? c->cap : 4096; while (nc < ns) nc *= 2;
        BYTE *nb = realloc(c->buf, nc); if (!nb) return STG_E_MEDIUMFULL;
        memset(nb + c->cap, 0, nc - c->cap); c->buf = nb; c->cap = nc;
    }
    if (ns > c->size) memset(c->buf + c->size, 0, (size_t)(ns - c->size));
    c->size = (ULONG)ns;
    return S_OK;
}
static HRESULT WINAPI clb_WriteAt(ILockBytes *p, ULARGE_INTEGER off, const void *pv, ULONG cb, ULONG *pw) {
    CLB *c = clb_from(p); c->nwrite++; clb_log(c, LB_WRITE, off.QuadPart, cb);
    if (pw) *pw = 0;
    if (c->fail_write) return c->fail_write;
    if (off.QuadPart + cb > c->maxend) c->maxend = off.QuadPart + cb;
    if (off.QuadPart + cb > c->size) { HRESULT hr = clb_grow(c, off.QuadPart + cb); if (FAILED(hr)) return hr; }
    memcpy(c->buf + off.QuadPart, pv, cb);
    if (pw) *pw = cb;
    return S_OK;
}
static HRESULT WINAPI clb_Flush(ILockBytes *p) { CLB *c = clb_from(p); c->nflush++; clb_log(c, LB_FLUSH, 0, 0); return S_OK; }
static HRESULT WINAPI clb_SetSize(ILockBytes *p, ULARGE_INTEGER ns) {
    CLB *c = clb_from(p); c->nsetsize++; clb_log(c, LB_SETSIZE, ns.QuadPart, 0);
    if (c->fail_setsize) return c->fail_setsize;
    return clb_grow(c, ns.QuadPart);
}
static HRESULT WINAPI clb_Lock(ILockBytes *p, ULARGE_INTEGER o, ULARGE_INTEGER n, DWORD t) { CLB *c = clb_from(p); c->nlock++; clb_log(c, LB_LOCK, o.QuadPart, (ULONG)n.QuadPart); return S_OK; }
static HRESULT WINAPI clb_Unlock(ILockBytes *p, ULARGE_INTEGER o, ULARGE_INTEGER n, DWORD t) { CLB *c = clb_from(p); c->nunlock++; clb_log(c, LB_UNLOCK, o.QuadPart, (ULONG)n.QuadPart); return S_OK; }
static HRESULT WINAPI clb_Stat(ILockBytes *p, STATSTG *st, DWORD fl) {
    CLB *c = clb_from(p); c->nstat++; clb_log(c, LB_STAT, 0, 0);
    if (c->fail_stat) return c->fail_stat;
    memset(st, 0, sizeof *st); st->type = STGTY_LOCKBYTES; st->cbSize.QuadPart = c->size; st->grfLocksSupported = c->lock_support;
    return S_OK;
}
static ILockBytesVtbl clb_vtbl = { clb_QI, clb_AddRef, clb_Release, clb_ReadAt, clb_WriteAt, clb_Flush, clb_SetSize, clb_Lock, clb_Unlock, clb_Stat };
static void clb_init(CLB *c) { memset(c, 0, sizeof *c); c->iface.lpVtbl = &clb_vtbl; c->ref = 1; c->log_ops = TRUE; c->lock_support = LOCK_WRITE; }
static void clb_free(CLB *c) { free(c->buf); c->buf = NULL; c->size = c->cap = 0; }
static void clb_reset_counts(CLB *c) { c->nread = c->nwrite = c->nsetsize = c->nlock = c->nunlock = c->nflush = c->nstat = 0; c->nlog = 0; c->maxend = 0; }

/* ---- OLESTREAM over a memory buffer ------------------------------------ */
typedef struct { OLESTREAM os; BYTE *buf; ULONG len, cap, pos; int gets, puts; ULONG fail_after_get_bytes, fail_after_put_bytes; } MEMOS;
static DWORD CALLBACK mos_Get(LPOLESTREAM o, void *p, DWORD cb) {
    MEMOS *m = (MEMOS *)o; m->gets++;
    if (m->pos + cb > m->len) return 0;
    memcpy(p, m->buf + m->pos, cb); m->pos += cb; return cb;
}
static DWORD CALLBACK mos_Put(LPOLESTREAM o, const void *p, DWORD cb) {
    MEMOS *m = (MEMOS *)o; m->puts++;
    if (m->fail_after_put_bytes && m->len + cb > m->fail_after_put_bytes) return 0;
    if (m->len + cb > (4u << 20)) return 0;
    if (m->len + cb > m->cap) { ULONG nc = m->cap ? m->cap * 2 : 1024; while (nc < m->len + cb) nc *= 2; m->buf = realloc(m->buf, nc); m->cap = nc; }
    memcpy(m->buf + m->len, p, cb); m->len += cb; return cb;
}
static OLESTREAMVTBL mos_vtbl = { mos_Get, mos_Put };
static void mos_init(MEMOS *m) { memset(m, 0, sizeof *m); m->os.lpstbl = &mos_vtbl; }
static void mos_load(MEMOS *m, const void *data, ULONG len) { mos_init(m); m->buf = malloc(len ? len : 1); memcpy(m->buf, data, len); m->len = m->cap = len; }
static void mos_free(MEMOS *m) { free(m->buf); m->buf = NULL; }
static void mos_u32(MEMOS *m, DWORD v) { mos_Put(&m->os, &v, 4); }
static void mos_str(MEMOS *m, const char *s) { DWORD n = s ? (DWORD)strlen(s) + 1 : 0; mos_u32(m, n); if (n) mos_Put(&m->os, s, n); }

/* Build a minimal OLE1 embedded object stream. class may be any string; native payload arbitrary. */
static void mos_build_embedded(MEMOS *m, const char *cls, const void *native, DWORD ncb, int with_pres_end) {
    mos_init(m);
    mos_u32(m, 0x501); mos_u32(m, 2);
    mos_str(m, cls); mos_str(m, NULL); mos_str(m, NULL);
    mos_u32(m, ncb); if (ncb) mos_Put(&m->os, native, ncb);
    if (with_pres_end) { mos_u32(m, 0x501); mos_u32(m, 0); }
}

/* ---- helpers for storages ---------------------------------------------- */
static HRESULT slm_mem_docfile(IStorage **ppstg, ILockBytes **pplb, DWORD mode) {
    HRESULT (WINAPI *cl)(HGLOBAL, BOOL, ILockBytes **) = (void *)t_proc("CreateILockBytesOnHGlobal");
    HRESULT (WINAPI *sd)(ILockBytes *, DWORD, DWORD, IStorage **) = (void *)t_proc("StgCreateDocfileOnILockBytes");
    if (!cl || !sd) return E_NOTIMPL;
    HRESULT hr = cl(NULL, TRUE, pplb);
    if (FAILED(hr)) return hr;
    hr = sd(*pplb, mode, 0, ppstg);
    if (FAILED(hr)) { ILockBytes_Release(*pplb); *pplb = NULL; }
    return hr;
}
static HRESULT slm_put_stream(IStorage *stg, const wchar_t *name, const void *data, ULONG cb) {
    IStream *s = NULL; ULONG w = 0;
    HRESULT hr = IStorage_CreateStream(stg, name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s);
    if (FAILED(hr)) return hr;
    hr = IStream_Write(s, data, cb, &w);
    IStream_Release(s);
    return hr;
}
static ULONG slm_get_stream(IStorage *stg, const wchar_t *name, void *buf, ULONG cap, HRESULT *phr) {
    IStream *s = NULL; ULONG r = 0;
    HRESULT hr = IStorage_OpenStream(stg, name, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s);
    if (SUCCEEDED(hr)) { hr = IStream_Read(s, buf, cap, &r); IStream_Release(s); }
    if (phr) *phr = hr;
    return r;
}
#endif
