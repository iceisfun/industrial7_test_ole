/* Shared helpers for the storage_elements_streams area.
 * Everything under test is reached through U()/UN() (LoadLibrary + GetProcAddress);
 * only the vtable methods of the returned COM objects are called through the COBJMACROS wrappers. */
#ifndef SES_H
#define SES_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <objidl.h>
#include <ole2.h>


/* IIDs: the harness does not link uuid, so define the ones we need as file-local constants. */
#define SES_GUID(n, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) static const GUID n = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
#undef IID_IUnknown
#undef IID_IStream
SES_GUID(SES_IID_IUnknown, 0x00000000, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IStream, 0x0000000c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IStorage, 0x0000000b, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_ILockBytes, 0x0000000a, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IEnumSTATSTG, 0x0000000d, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IPersist, 0x0000010c, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IPersistStream, 0x00000109, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IMarshal, 0x00000003, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IDispatch, 0x00020400, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_IPropertySetStorage, 0x0000013A, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SES_GUID(SES_IID_ISequentialStream, 0x0c733a30, 0x2a1c, 0x11ce, 0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d);
SES_GUID(SES_GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
SES_GUID(SES_CLSID_A, 0x11111111, 0x2222, 0x3333, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb);
SES_GUID(SES_CLSID_B, 0xdeadbeef, 0x0102, 0x0304, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c);
#define IID_IUnknown SES_IID_IUnknown
#define IID_IStream SES_IID_IStream
#define IID_IStorage SES_IID_IStorage
#define IID_ILockBytes SES_IID_ILockBytes
#define IID_IEnumSTATSTG SES_IID_IEnumSTATSTG
#define IID_IPersist SES_IID_IPersist
#define IID_IPersistStream SES_IID_IPersistStream
#define IID_IMarshal SES_IID_IMarshal
#define IID_IDispatch SES_IID_IDispatch
#define IID_IPropertySetStorage SES_IID_IPropertySetStorage
#define IID_ISequentialStream SES_IID_ISequentialStream
#define GUID_NULL SES_GUID_NULL

#ifndef STG_E_INVALIDFLAG
#define STG_E_INVALIDFLAG ((HRESULT)0x800300FFL)
#endif

#define MODE_RW      (STGM_READWRITE | STGM_SHARE_EXCLUSIVE)
#define MODE_CREATE  (STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE)
#define MODE_TXCREATE (MODE_CREATE | STGM_TRANSACTED)
#define MODE_RD      (STGM_READ | STGM_SHARE_EXCLUSIVE)

/* HRESULT comparison, expression evaluated once. */
#define HR(expr, want) do { HRESULT h_ = (expr); t_check(h_ == (HRESULT)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%08lx want 0x%08lx", (unsigned long)h_, (unsigned long)(HRESULT)(want)); } while (0)
/* Expectation known from Windows documentation/behaviour but not reproduced by emulators:
 * a hard CHECK on Windows, only a NOTE when running under Wine. */
#define HRW(expr, want) do { HRESULT h_ = (expr); if (t_is_wine()) { if (h_ != (HRESULT)(want)) t_note("wine differs: " #expr " got 0x%08lx want 0x%08lx", (unsigned long)h_, (unsigned long)(HRESULT)(want)); } else t_check(h_ == (HRESULT)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%08lx want 0x%08lx", (unsigned long)h_, (unsigned long)(HRESULT)(want)); } while (0)
#define CHECKW(c) do { int ok_ = !!(c); if (t_is_wine()) { if (!ok_) t_note("wine differs: " #c); } else t_check(ok_, __FILE__, __LINE__, #c, NULL); } while (0)
/* Observation that depends on version/SKU: note only. */
#define OBSERVE_HR(expr) do { HRESULT h_ = (expr); t_note("observed " #expr " = 0x%08lx", (unsigned long)h_); } while (0)

/* equality with the expression evaluated exactly once (EXPECT_EQ evaluates its arguments twice) */
#define EQ(expr, want) do { unsigned long long g_ = (unsigned long long)(expr); t_check(g_ == (unsigned long long)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", g_, (unsigned long long)(want)); } while (0)
#define LI(v) ses_li(v)
static LARGE_INTEGER ses_li(LONGLONG v) { LARGE_INTEGER l; l.QuadPart = v; return l; }
static ULARGE_INTEGER ses_uli(ULONGLONG v) { ULARGE_INTEGER l; l.QuadPart = v; return l; }
#define ULI(v) ses_uli(v)

static void ses_fill(unsigned char *p, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; p[i] = (unsigned char)(seed >> 16); }
}
static int ses_allzero(const unsigned char *p, size_t n) { for (size_t i = 0; i < n; i++) if (p[i]) return 0; return 1; }

/* ---- resolved entry points (NULL when the export is missing) ---- */
typedef struct {
    __typeof__(&StgCreateDocfile) create_docfile;
    __typeof__(&StgOpenStorage) open_storage;
    __typeof__(&StgCreateDocfileOnILockBytes) create_on_ilb;
    __typeof__(&StgOpenStorageOnILockBytes) open_on_ilb;
    __typeof__(&CreateILockBytesOnHGlobal) ilb_hglobal;
    __typeof__(&CreateStreamOnHGlobal) stm_hglobal;
    __typeof__(&GetHGlobalFromStream) hglobal_from_stm;
    __typeof__(&CoTaskMemFree) taskfree;
    __typeof__(&StgIsStorageFile) is_file;
} ses_api;
static int ses_load(ses_api *a) {
    memset(a, 0, sizeof *a);
    a->create_docfile = U(StgCreateDocfile);
    a->open_storage = U(StgOpenStorage);
    a->create_on_ilb = U(StgCreateDocfileOnILockBytes);
    a->open_on_ilb = U(StgOpenStorageOnILockBytes);
    a->ilb_hglobal = U(CreateILockBytesOnHGlobal);
    a->stm_hglobal = U(CreateStreamOnHGlobal);
    a->hglobal_from_stm = U(GetHGlobalFromStream);
    a->taskfree = U(CoTaskMemFree);
    a->is_file = U(StgIsStorageFile);
    return a->create_docfile && a->open_storage && a->create_on_ilb && a->open_on_ilb && a->ilb_hglobal &&
           a->stm_hglobal && a->hglobal_from_stm && a->taskfree && a->is_file;
}
#define SES_REQUIRE(a) ses_api a; do { if (!ses_load(&a)) { t_skip("missing export for storage area"); return; } } while (0)

/* In-memory docfile (ILockBytes on HGLOBAL): no file system involvement. `*ilb_out` may be NULL. */
static IStorage *ses_mem(const ses_api *a, DWORD mode, ILockBytes **ilb_out) {
    ILockBytes *ilb = NULL; IStorage *stg = NULL;
    if (a->ilb_hglobal(NULL, TRUE, &ilb) != S_OK) return NULL;
    if (a->create_on_ilb(ilb, mode, 0, &stg) != S_OK) { ILockBytes_Release(ilb); return NULL; }
    if (ilb_out) *ilb_out = ilb; else ILockBytes_Release(ilb);
    return stg;
}
/* Docfile in a fresh scratch file; wpath receives the wide path. */
static unsigned ses_seq;
static IStorage *ses_file(const ses_api *a, DWORD mode, WCHAR *wpath) {
    char path[MAX_PATH]; char leaf[64]; IStorage *stg = NULL;
    snprintf(leaf, sizeof leaf, "ses%u_%lu.stg", ++ses_seq, GetCurrentProcessId());
    t_tmpfile(path, sizeof path, leaf);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    if (a->create_docfile(wpath, mode, 0, &stg) != S_OK) return NULL;
    return stg;
}
static IStream *ses_mkstream(IStorage *stg, const WCHAR *name) {
    IStream *s = NULL;
    if (IStorage_CreateStream(stg, name, MODE_RW, 0, 0, &s) != S_OK) return NULL;
    return s;
}
static HRESULT ses_write_all(IStream *s, const void *p, ULONG n) {
    ULONG w = 0; HRESULT hr = IStream_Write(s, p, n, &w); return (hr == S_OK && w != n) ? E_FAIL : hr;
}
static HRESULT ses_put(IStorage *stg, const WCHAR *name, const void *p, ULONG n) {
    IStream *s = ses_mkstream(stg, name); HRESULT hr;
    if (!s) return E_FAIL;
    hr = ses_write_all(s, p, n); IStream_Release(s); return hr;
}
static ULONGLONG ses_stream_size(IStream *s) { STATSTG st; memset(&st, 0xcc, sizeof st); if (IStream_Stat(s, &st, STATFLAG_NONAME) != S_OK) return (ULONGLONG)-1; return st.cbSize.QuadPart; }
static HRESULT ses_seek(IStream *s, LONGLONG off, DWORD org, ULONGLONG *np) { ULARGE_INTEGER n; HRESULT hr; n.QuadPart = 0xcafecafecafecafeULL; hr = IStream_Seek(s, LI(off), org, &n); if (np) *np = n.QuadPart; return hr; }
static ULONGLONG ses_pos(IStream *s) { ULONGLONG p = (ULONGLONG)-1; ses_seek(s, 0, STREAM_SEEK_CUR, &p); return p; }
/* Read the whole named stream into buf (cap bytes); returns bytes read or -1 on failure. */
static long ses_get(IStorage *stg, const WCHAR *name, void *buf, ULONG cap) {
    IStream *s = NULL; ULONG got = 0; HRESULT hr;
    if (IStorage_OpenStream(stg, name, NULL, MODE_RD, 0, &s) != S_OK) return -1;
    hr = IStream_Read(s, buf, cap, &got); IStream_Release(s);
    return hr == S_OK ? (long)got : -1;
}
static int ses_exists_stream(IStorage *stg, const WCHAR *name) {
    IStream *s = NULL; HRESULT hr = IStorage_OpenStream(stg, name, NULL, MODE_RD, 0, &s);
    if (hr == S_OK) IStream_Release(s); return hr == S_OK;
}
static int ses_exists_storage(IStorage *stg, const WCHAR *name) {
    IStorage *s = NULL; HRESULT hr = IStorage_OpenStorage(stg, name, NULL, MODE_RD, NULL, 0, &s);
    if (hr == S_OK) IStorage_Release(s); return hr == S_OK;
}
/* Enumerate names into out[] (max n); returns count, frees names. types[] optional. */
static int ses_list(const ses_api *a, IStorage *stg, WCHAR out[][40], DWORD *types, ULONGLONG *sizes, int n) {
    IEnumSTATSTG *e = NULL; STATSTG st; ULONG f; int c = 0;
    if (IStorage_EnumElements(stg, 0, NULL, 0, &e) != S_OK) return -1;
    while (c < n && IEnumSTATSTG_Next(e, 1, &st, &f) == S_OK && f == 1) {
        lstrcpynW(out[c], st.pwcsName, 40);
        if (types) types[c] = st.type;
        if (sizes) sizes[c] = st.cbSize.QuadPart;
        a->taskfree(st.pwcsName); c++;
    }
    IEnumSTATSTG_Release(e);
    return c;
}
static void ses_delete(const WCHAR *w) { DeleteFileW(w); }
#endif
