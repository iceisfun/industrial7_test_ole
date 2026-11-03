/* Shared helpers for the monikers_rot area.
 * Every ole32 entry point goes through t.h's U()/UN() (LoadLibrary + GetProcAddress);
 * interface methods are invoked through vtables (COBJMACROS). */
#ifndef MR_H
#define MR_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <stdlib.h>

#define MR_GUID(n, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    static const GUID n = {a, b, c, {d0, d1, d2, d3, d4, d5, d6, d7}}
MR_GUID(mr_IID_IUnknown,           0x00000000, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IClassFactory,      0x00000001, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IMarshal,           0x00000003, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IBindCtx,           0x0000000e, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IMoniker,           0x0000000f, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IRunningObjectTable,0x00000010, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IPersist,           0x0000010c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IPersistStream,     0x00000109, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IEnumMoniker,       0x00000102, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IEnumString,        0x00000101, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IStorage,           0x0000000b, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IStream,            0x0000000c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IParseDisplayName,  0x0000011a, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IOleItemContainer,  0x0000011c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IDispatch,          0x00020400, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_IID_IROTData,           0xf29f6bc0, 0x5021, 0x11ce, 0xaa, 0x15, 0x00, 0x00, 0x69, 0x01, 0x29, 0x3f);
MR_GUID(mr_CLSID_FileMoniker,      0x00000303, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_CLSID_ItemMoniker,      0x00000304, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_CLSID_AntiMoniker,      0x00000305, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_CLSID_PointerMoniker,   0x00000306, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_CLSID_CompositeMoniker, 0x00000309, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_CLSID_ClassMoniker,     0x0000031a, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
MR_GUID(mr_CLSID_ObjrefMoniker,    0x00000327, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
/* arbitrary CLSIDs that no machine registers */
MR_GUID(mr_CLS_X, 0x2a4b6c01, 0x1111, 0x4222, 0x83, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);
MR_GUID(mr_CLS_Y, 0x11111111, 0x0000, 0x0000, 0x22, 0x22, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44);
/* CLSID_StdComponentCategoriesMgr = {0002E005-0000-0000-C000-000000000046} */
MR_GUID(mr_CLS_CatMgr, 0x0002e005, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
/* CLSID_StdFont = {0BE35203-8F91-11CE-9DE3-00AA004BB851} */
MR_GUID(mr_CLS_StdFont, 0x0be35203, 0x8f91, 0x11ce, 0x9d, 0xe3, 0x00, 0xaa, 0x00, 0x4b, 0xb8, 0x51);

static int mr_guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof(GUID)); }

/* ---- required entry points ------------------------------------------------ */
#define MR_NEED_BASE() do { \
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CreateBindCtx)); \
    REQUIRE(U(GetRunningObjectTable)); REQUIRE(U(CreateFileMoniker)); REQUIRE(U(CreateItemMoniker)); \
    REQUIRE(U(CreateAntiMoniker)); REQUIRE(U(CreatePointerMoniker)); REQUIRE(U(CreateGenericComposite)); \
    REQUIRE(U(CreateClassMoniker)); REQUIRE(U(CreateStreamOnHGlobal)); REQUIRE(U(CoTaskMemFree)); \
} while (0)

/* ---- COM apartment guard ---------------------------------------------------- */
static int mr_com(DWORD model) {
    HRESULT hr = U(CoInitializeEx)(NULL, model);
    return SUCCEEDED(hr);
}
#define MR_COM_BEGIN() do { if (!mr_com(COINIT_APARTMENTTHREADED)) { t_skip("CoInitializeEx failed"); return; } } while (0)
#define MR_MTA_BEGIN() do { if (!mr_com(COINIT_MULTITHREADED)) { t_skip("CoInitializeEx(MTA) failed"); return; } } while (0)
#define MR_COM_END()   U(CoUninitialize)()

/* ---- a counting plain IUnknown object ------------------------------------- */
typedef struct mr_obj { const IUnknownVtbl *lpVtbl; volatile LONG ref; volatile LONG qi; } mr_obj;
static HRESULT WINAPI mr_obj_QI(IUnknown *i, REFIID riid, void **ppv) {
    mr_obj *o = (mr_obj *)i;
    InterlockedIncrement(&o->qi);
    if (mr_guid_eq(riid, &mr_IID_IUnknown)) { *ppv = i; InterlockedIncrement(&o->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI mr_obj_AddRef(IUnknown *i) { return InterlockedIncrement(&((mr_obj *)i)->ref); }
static ULONG WINAPI mr_obj_Release(IUnknown *i) { return InterlockedDecrement(&((mr_obj *)i)->ref); }
static const IUnknownVtbl mr_obj_vtbl = { mr_obj_QI, mr_obj_AddRef, mr_obj_Release };
static void mr_obj_init(mr_obj *o) { o->lpVtbl = &mr_obj_vtbl; o->ref = 1; o->qi = 0; }
static IUnknown *mr_obj_unk(mr_obj *o) { return (IUnknown *)o; }

/* ---- moniker construction -------------------------------------------------- */
static IMoniker *mr_file(const WCHAR *path) {
    IMoniker *m = NULL; HRESULT hr = U(CreateFileMoniker)(path, &m);
    CHECKF(hr == S_OK && m, "CreateFileMoniker(%ls) hr=%08lx", path ? path : L"(null)", hr);
    return m;
}
static IMoniker *mr_item(const WCHAR *delim, const WCHAR *item) {
    IMoniker *m = NULL; HRESULT hr = U(CreateItemMoniker)(delim, item, &m);
    CHECKF(hr == S_OK && m, "CreateItemMoniker hr=%08lx", hr);
    return m;
}
static IMoniker *mr_ptr(IUnknown *u) {
    IMoniker *m = NULL; HRESULT hr = U(CreatePointerMoniker)(u, &m);
    CHECKF(hr == S_OK && m, "CreatePointerMoniker hr=%08lx", hr);
    return m;
}
static IMoniker *mr_class(const GUID *g) {
    IMoniker *m = NULL; HRESULT hr = U(CreateClassMoniker)(g, &m);
    CHECKF(hr == S_OK && m, "CreateClassMoniker hr=%08lx", hr);
    return m;
}
static IMoniker *mr_comp(IMoniker *l, IMoniker *r) {
    IMoniker *m = NULL; HRESULT hr = U(CreateGenericComposite)(l, r, &m);
    CHECKF(hr == S_OK, "CreateGenericComposite hr=%08lx", hr);
    return m;
}
/* anti moniker with an explicit count: load a DWORD count into a fresh anti moniker */
static IMoniker *mr_anti(DWORD level) {
    IMoniker *m = NULL; IStream *s = NULL; LARGE_INTEGER z; z.QuadPart = 0;
    HRESULT hr = U(CreateAntiMoniker)(&m);
    CHECKF(hr == S_OK && m, "CreateAntiMoniker hr=%08lx", hr);
    if (!m) return NULL;
    if (level == 1) return m;
    hr = U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    if (hr != S_OK) return m;
    IStream_Write(s, &level, sizeof level, NULL);
    IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
    hr = IMoniker_Load(m, s);
    CHECKF(hr == S_OK, "anti Load hr=%08lx", hr);
    IStream_Release(s);
    return m;
}
/* compact description language from the Wine tests: I<c>=item "!I<c>", A<n>=anti(n), C<l><r>=composite, P=pointer */
static IMoniker *mr_parse_desc(const char **pd) {
    const char *d = *pd; IMoniker *l, *r, *m = NULL;
    switch (*d) {
    case 'I': { WCHAR n[3] = { 'I', (WCHAR)d[1], 0 }; *pd = d + 2; return mr_item(L"!", n); }
    case 'A': *pd = d + 2; return mr_anti((DWORD)(d[1] - '0'));
    case 'C': *pd = d + 1; l = mr_parse_desc(pd); r = mr_parse_desc(pd);
              m = mr_comp(l, r); if (l) IMoniker_Release(l); if (r) IMoniker_Release(r); return m;
    default: CHECKF(0, "bad desc %s", d); return NULL;
    }
}
static IMoniker *mr_desc(const char *d) { return mr_parse_desc(&d); }

static IBindCtx *mr_bc(void) {
    IBindCtx *b = NULL; HRESULT hr = U(CreateBindCtx)(0, &b);
    CHECKF(hr == S_OK && b, "CreateBindCtx hr=%08lx", hr);
    return b;
}
static void mr_rel(IMoniker *m) { if (m) IMoniker_Release(m); }

/* ---- checks -------------------------------------------------------------------- */
static void mr_check_dn(const char *file, int line, IMoniker *m, const WCHAR *exp) {
    IBindCtx *bc = mr_bc(); WCHAR *s = NULL; HRESULT hr = E_FAIL;
    if (m && bc) hr = IMoniker_GetDisplayName(m, bc, NULL, &s);
    t_check(hr == S_OK && s && !lstrcmpW(s, exp), file, line, "display name", "hr=%08lx got \"%ls\" want \"%ls\"",
            hr, s ? s : L"(null)", exp);
    if (s) U(CoTaskMemFree)(s);
    if (bc) IBindCtx_Release(bc);
}
#define CHECK_DN(m, exp) mr_check_dn(__FILE__, __LINE__, (m), (exp))

static void mr_check_type(const char *file, int line, IMoniker *m, DWORD want) {
    DWORD t = 0xdeadbeef; HRESULT hr = m ? IMoniker_IsSystemMoniker(m, &t) : E_POINTER;
    t_check(want == MKSYS_NONE ? (hr == S_FALSE) : (hr == S_OK) && t == want, file, line, "system moniker type",
            "hr=%08lx type=%lu want %lu", hr, t, want);
}
#define CHECK_TYPE(m, t) mr_check_type(__FILE__, __LINE__, (m), (t))

static DWORD mr_hash(IMoniker *m) { DWORD h = 0xdeadbeef; HRESULT hr = IMoniker_Hash(m, &h); CHECKF(hr == S_OK, "Hash hr=%08lx", hr); return h; }

static int mr_is_equal(IMoniker *a, IMoniker *b) { return IMoniker_IsEqual(a, b) == S_OK; }

/* Save a moniker (fClearDirty) to a memory stream; returns number of bytes, data into buf. */
static ULONG mr_save(IMoniker *m, BYTE *buf, ULONG cap, HRESULT *phr) {
    IStream *s = NULL; LARGE_INTEGER z; ULARGE_INTEGER end; ULONG got = 0; HRESULT hr;
    z.QuadPart = 0;
    if (U(CreateStreamOnHGlobal)(NULL, TRUE, &s) != S_OK) return 0;
    hr = IMoniker_Save(m, s, FALSE);
    if (phr) *phr = hr;
    if (hr == S_OK) {
        IStream_Seek(s, z, STREAM_SEEK_CUR, &end);
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
        if (end.QuadPart <= cap) IStream_Read(s, buf, (ULONG)end.QuadPart, &got);
        else got = (ULONG)end.QuadPart;
    }
    IStream_Release(s);
    return got;
}
static DWORD mr_rd32(const BYTE *p) { DWORD v; memcpy(&v, p, 4); return v; }
static WORD mr_rd16(const BYTE *p) { WORD v; memcpy(&v, p, 2); return v; }

/* Round-trip a moniker through Save + Load into a new moniker of the same class (via CreateXxx(...)). */
static HRESULT mr_load_into(IMoniker *dst, const BYTE *data, ULONG len) {
    IStream *s = NULL; LARGE_INTEGER z; HRESULT hr; ULONG w = 0; z.QuadPart = 0;
    if (U(CreateStreamOnHGlobal)(NULL, TRUE, &s) != S_OK) return E_FAIL;
    IStream_Write(s, data, len, &w);
    IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
    hr = IMoniker_Load(dst, s);
    IStream_Release(s);
    return hr;
}

/* unique-per-process names */
static void mr_unique_path(WCHAR *out, size_t cch, const WCHAR *leaf) {
    wsprintfW(out, L"C:\\olet_mr_%lu_nonexistent\\%ls", GetCurrentProcessId(), leaf);
}
static void mr_unique_name(WCHAR *out, size_t cch, const WCHAR *leaf) {
    wsprintfW(out, L"olet_mr_%lu_%ls", GetCurrentProcessId(), leaf);
}

/* ---- bounded worker thread -------------------------------------------------- */
typedef struct mr_thr { void (*fn)(void *); void *arg; } mr_thr;
static DWORD WINAPI mr_thr_main(LPVOID p) { mr_thr *t = (mr_thr *)p; t->fn(t->arg); return 0; }
/* run fn(arg) on a fresh thread (no COM initialised), wait up to ms; returns 1 when it finished */
static int mr_run_thread(void (*fn)(void *), void *arg, DWORD ms) {
    mr_thr t; HANDLE h; DWORD w; t.fn = fn; t.arg = arg;
    h = CreateThread(NULL, 0, mr_thr_main, &t, 0, NULL);
    if (!h) return 0;
    w = WaitForSingleObject(h, ms);
    if (w != WAIT_OBJECT_0) { t_note("worker thread did not finish within %lu ms", ms); return 0; }
    CloseHandle(h);
    return 1;
}
#endif
