/* Shared helpers for the propvariant_propstg area.
 * Nothing here links ole32: every entry point goes through U()/UN() (GetProcAddress).
 * Interface methods are invoked through the COM vtable (CALL macro), the SDK C bindings only
 * supply the interface/struct declarations. */
#ifndef PV_H
#define PV_H
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <propidl.h>

#define CALL(p, m, ...) ((p)->lpVtbl->m((p), ##__VA_ARGS__))
/* Single-evaluation equality check (arguments may have side effects). */
#define EQ(a, b) do { long long a_ = (long long)(a), b_ = (long long)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define N_ELEM(a) ((int)(sizeof(a) / sizeof((a)[0])))

#ifndef PID_BEHAVIOR
#define PID_BEHAVIOR 0x80000003
#endif
#define PID_LOCALE_ 0x80000000u
#define PID_MODIFY_TIME_ 0x80000001u

#define E_INVALIDARG_ ((HRESULT)0x80070057)
#define STG_E_INVALIDPARAMETER_ ((HRESULT)0x80030057)
#define STG_E_INVALIDNAME_ ((HRESULT)0x800300FC)
#define STG_E_FILEALREADYEXISTS_ ((HRESULT)0x80030050)
#define STG_E_FILENOTFOUND_ ((HRESULT)0x80030002)
#define DISP_E_BADVARTYPE_ ((HRESULT)0x80020008)

/* Format identifiers (defined here because uuid.lib is not linked). */
static const GUID G_FMTID_SI  = {0xF29F85E0, 0x4FF9, 0x1068, {0xAB, 0x91, 0x08, 0x00, 0x2B, 0x27, 0xB3, 0xD9}};
static const GUID G_FMTID_DSI = {0xD5CDD502, 0x2E9C, 0x101B, {0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE}};
static const GUID G_FMTID_UDP = {0xD5CDD505, 0x2E9C, 0x101B, {0x93, 0x97, 0x08, 0x00, 0x2B, 0x2C, 0xF9, 0xAE}};
static const GUID G_IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID G_IID_IPropSetStg = {0x0000013A, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID G_IID_IPropStg = {0x00000138, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID G_ANY_GUID = {0x12345678, 0xDEAD, 0xBEEF, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}};
static const GUID G_ANY_GUID2 = {0x9A8B7C6D, 0x1234, 0x4321, {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11}};
static const GUID G_NULL_GUID = {0};

#define STGM_RW_EX  (STGM_READWRITE | STGM_SHARE_EXCLUSIVE)
#define STGM_RW_EX_CREATE (STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE)
#define STGM_R_EX   (STGM_READ | STGM_SHARE_EXCLUSIVE)

/* ---- COM apartment bracket (skips the test when COM cannot start) ---------- */
#define COM_BEGIN() do { \
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); \
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED))) { t_skip("CoInitializeEx failed"); return; } \
} while (0)
#define COM_END() U(CoUninitialize)()

/* ---- allocation through the COM task allocator (what PropVariantClear frees with) ---- */
static void *tm_alloc(size_t n) { return U(CoTaskMemAlloc)(n); }
static void *tm_dup(const void *p, size_t n) { void *d = tm_alloc(n ? n : 1); if (d && n) memcpy(d, p, n); return d; }
static char *tm_strA(const char *s) { return (char *)tm_dup(s, strlen(s) + 1); }
static WCHAR *tm_strW(const WCHAR *s) { return (WCHAR *)tm_dup(s, (lstrlenW(s) + 1) * sizeof(WCHAR)); }

static int guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof(GUID)); }
static int pv_zero(const PROPVARIANT *p) { PROPVARIANT z; memset(&z, 0, sizeof z); return !memcmp(p, &z, sizeof z); }

/* ---- a counting IUnknown ---------------------------------------------------- */
typedef struct { IUnknownVtbl *lpVtbl; volatile LONG ref; } TUnk;
static HRESULT WINAPI tu_qi(IUnknown *i, REFIID riid, void **pp) {
    if (IsEqualGUID(riid, &G_IID_IUnknown)) { *pp = i; ((TUnk *)i)->ref++; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI tu_addref(IUnknown *i) { return InterlockedIncrement(&((TUnk *)i)->ref); }
static ULONG WINAPI tu_release(IUnknown *i) { return InterlockedDecrement(&((TUnk *)i)->ref); }
static IUnknownVtbl g_tu_vtbl = { tu_qi, tu_addref, tu_release };
static void tu_init(TUnk *u) { u->lpVtbl = &g_tu_vtbl; u->ref = 1; }

/* ---- storage fixtures ---------------------------------------------------------- */
typedef struct {
    IStorage *stg;
    IPropertySetStorage *pss;
    IPropertyStorage *ps;
    WCHAR path[MAX_PATH];
} FX;

static void w_path(WCHAR *out, const char *leaf) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
    MultiByteToWideChar(CP_ACP, 0, p, -1, out, MAX_PATH);
}

/* New docfile + property set storage (no property set yet). Returns 1 on success. */
static int fx_storage(FX *f, const char *leaf) {
    memset(f, 0, sizeof *f);
    w_path(f->path, leaf);
    DeleteFileW(f->path);
    HRESULT hr = U(StgCreateDocfile)(f->path, STGM_RW_EX_CREATE, 0, &f->stg);
    EXPECT_EQ(hr, S_OK);
    if (hr != S_OK) return 0;
    hr = U(StgCreatePropSetStg)(f->stg, 0, &f->pss);
    EXPECT_EQ(hr, S_OK);
    return hr == S_OK;
}
/* ... and a created property set. */
static int fx_new(FX *f, const char *leaf, const GUID *fmtid, DWORD flags) {
    if (!fx_storage(f, leaf)) return 0;
    HRESULT hr = CALL(f->pss, Create, fmtid, NULL, flags, STGM_RW_EX_CREATE, &f->ps);
    EXPECT_EQ(hr, S_OK);
    return hr == S_OK && f->ps;
}
static void fx_release_ps(FX *f) { if (f->ps) { CALL(f->ps, Release); f->ps = NULL; } }
static void fx_close(FX *f) {
    fx_release_ps(f);
    if (f->pss) { CALL(f->pss, Release); f->pss = NULL; }
    if (f->stg) { CALL(f->stg, Release); f->stg = NULL; }
}
/* Close everything and reopen the same file + property set (persistence check). */
static int fx_reopen(FX *f, const GUID *fmtid, DWORD mode) {
    WCHAR path[MAX_PATH]; lstrcpyW(path, f->path);
    if (f->ps) { HRESULT c = CALL(f->ps, Commit, STGC_DEFAULT); (void)c; }
    fx_close(f);
    HRESULT hr = U(StgOpenStorage)(path, NULL, STGM_RW_EX, NULL, 0, &f->stg);
    EXPECT_EQ(hr, S_OK);
    if (hr != S_OK) return 0;
    hr = U(StgCreatePropSetStg)(f->stg, 0, &f->pss);
    EXPECT_EQ(hr, S_OK);
    if (hr != S_OK) return 0;
    hr = CALL(f->pss, Open, fmtid, mode, &f->ps);
    EXPECT_EQ(hr, S_OK);
    return hr == S_OK && f->ps;
}

/* ---- single property helpers -------------------------------------------------- */
static HRESULT wr_id(IPropertyStorage *ps, ULONG id, PROPVARIANT *v) {
    PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = id;
    return CALL(ps, WriteMultiple, 1, &s, v, PID_FIRST_USABLE);
}
static HRESULT rd_id(IPropertyStorage *ps, ULONG id, PROPVARIANT *v) {
    PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = id;
    PropVariantInit(v);
    return CALL(ps, ReadMultiple, 1, &s, v);
}
static HRESULT wr_name(IPropertyStorage *ps, const WCHAR *name, PROPVARIANT *v, PROPID first) {
    PROPSPEC s; s.ulKind = PRSPEC_LPWSTR; s.lpwstr = (LPOLESTR)name;
    return CALL(ps, WriteMultiple, 1, &s, v, first);
}
static HRESULT rd_name(IPropertyStorage *ps, const WCHAR *name, PROPVARIANT *v) {
    PROPSPEC s; s.ulKind = PRSPEC_LPWSTR; s.lpwstr = (LPOLESTR)name;
    PropVariantInit(v);
    return CALL(ps, ReadMultiple, 1, &s, v);
}
static HRESULT wr_i4(IPropertyStorage *ps, ULONG id, LONG val) {
    PROPVARIANT v; PropVariantInit(&v); v.vt = VT_I4; v.lVal = val; return wr_id(ps, id, &v);
}
static HRESULT wr_name_i4(IPropertyStorage *ps, const WCHAR *name, LONG val, PROPID first) {
    PROPVARIANT v; PropVariantInit(&v); v.vt = VT_I4; v.lVal = val; return wr_name(ps, name, &v, first);
}
/* Read an I4; returns the HRESULT and stores vt/val. */
static HRESULT rd_i4(IPropertyStorage *ps, ULONG id, VARTYPE *vt, LONG *val) {
    PROPVARIANT v; HRESULT hr = rd_id(ps, id, &v);
    *vt = v.vt; *val = v.lVal; U(PropVariantClear)(&v); return hr;
}
static HRESULT name_to_id(IPropertyStorage *ps, const WCHAR *name, PROPID *id) {
    /* Find the id by enumerating the dictionary with ReadPropertyNames over a window of ids. */
    PROPID ids[64]; LPOLESTR names[64]; int i;
    for (i = 0; i < 64; i++) ids[i] = 2 + i;
    HRESULT hr = CALL(ps, ReadPropertyNames, 64, ids, names);
    if (FAILED(hr)) return hr;
    HRESULT found = S_FALSE;
    for (i = 0; i < 64; i++) {
        if (names[i]) { if (!lstrcmpiW(names[i], name)) { *id = ids[i]; found = S_OK; } U(CoTaskMemFree)(names[i]); }
    }
    return found;
}

/* A flat serialized-property-set stream builder (MS-OLEPS), used to craft input for the reader. */
typedef struct { BYTE b[16384]; ULONG n; } PB;
static void pb_u8(PB *p, BYTE v)  { p->b[p->n++] = v; }
static void pb_u16(PB *p, WORD v) { memcpy(p->b + p->n, &v, 2); p->n += 2; }
static void pb_u32(PB *p, DWORD v){ memcpy(p->b + p->n, &v, 4); p->n += 4; }
static void pb_u64(PB *p, unsigned long long v) { memcpy(p->b + p->n, &v, 8); p->n += 8; }
static void pb_raw(PB *p, const void *d, ULONG n) { memcpy(p->b + p->n, d, n); p->n += n; }
static void pb_guid(PB *p, const GUID *g) { pb_raw(p, g, 16); }
static void pb_align4(PB *p) { while (p->n & 3) p->b[p->n++] = 0; }
static void pb_put32_at(PB *p, ULONG off, DWORD v) { memcpy(p->b + off, &v, 4); }

/* One-section property set header: byte order, version 0, os, clsid=0, 1 set, fmtid, offset 0x30. */
static void pb_set_header(PB *p, const GUID *fmtid) {
    p->n = 0;
    pb_u16(p, 0xFFFE); pb_u16(p, 0); pb_u32(p, 0x00020006 /* os */);
    pb_guid(p, &G_NULL_GUID);
    pb_u32(p, 1);
    pb_guid(p, fmtid);
    pb_u32(p, 0x30);
}

/* Write a raw byte image into a new stream named name of the storage. */
static HRESULT put_stream(IStorage *stg, const WCHAR *name, const void *data, ULONG n) {
    IStream *s = NULL;
    HRESULT hr = CALL(stg, CreateStream, name, STGM_RW_EX_CREATE, 0, 0, &s);
    if (hr != S_OK) return hr;
    ULONG w = 0;
    hr = CALL(s, Write, data, n, &w);
    CALL(s, Release);
    return hr == S_OK && w == n ? S_OK : (FAILED(hr) ? hr : E_FAIL);
}
static ULONG get_stream(IStorage *stg, const WCHAR *name, void *buf, ULONG cap) {
    IStream *s = NULL; ULONG r = 0;
    if (CALL(stg, OpenStream, name, NULL, STGM_R_EX, 0, &s) != S_OK) return (ULONG)-1;
    CALL(s, Read, buf, cap, &r);
    CALL(s, Release);
    return r;
}
static const WCHAR SI_NAME[]  = L"\005SummaryInformation";
static const WCHAR DSI_NAME[] = L"\005DocumentSummaryInformation";

#endif
