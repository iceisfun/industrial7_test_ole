/* Shared helpers for the storage_create_open area. All ole32 entry points are resolved through
 * t.h's U()/UN() (LoadLibrary + GetProcAddress); interface methods are called through vtables. */
#ifndef SCO_H
#define SCO_H
#define COBJMACROS
#define CINTERFACE
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <stdlib.h>

/* IIDs/CLSIDs are defined locally so nothing needs to be linked from libuuid. */
static const GUID sco_IID_IUnknown  = {0x00000000,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID sco_IID_IStorage  = {0x0000000b,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID sco_IID_IStream   = {0x0000000c,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID sco_IID_ILockBytes= {0x0000000a,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID sco_CLSID_NULL    = {0,0,0,{0,0,0,0,0,0,0,0}};
static const GUID sco_CLS_A = {0x2a4b6c01,0x1111,0x4222,{0x83,0x33,0x44,0x55,0x66,0x77,0x88,0x99}};
static const GUID sco_CLS_B = {0xdeadbeef,0xcafe,0xf00d,{0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef}};

/* Single-evaluation equality check (the harness EXPECT_EQ evaluates its arguments twice). */
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
#define SCO_ARRAY(a) (sizeof(a)/sizeof((a)[0]))

/* wide scratch path "<tmpdir><leaf>" */
static void sco_wpath(WCHAR *out, const char *leaf) {
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, MAX_PATH);
}
static void sco_apath(char *out, const char *leaf) { t_tmpfile(out, MAX_PATH, leaf); }
static void sco_rm(const WCHAR *p) { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); DeleteFileW(p); }
static int sco_exists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static LONGLONG sco_size(const WCHAR *p) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &d)) return -1;
    return ((LONGLONG)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}
/* read whole file into malloc'ed buffer; returns length or -1 */
static LONG sco_read(const WCHAR *p, BYTE **out) {
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    *out = NULL;
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD n = GetFileSize(h, NULL), got = 0;
    BYTE *b = (BYTE *)malloc(n ? n : 1);
    if (!b || !ReadFile(h, b, n, &got, NULL) || got != n) { CloseHandle(h); free(b); return -1; }
    CloseHandle(h);
    *out = b; return (LONG)n;
}
static int sco_write_file(const WCHAR *p, const void *data, DWORD n) {
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD w = 0; int ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = (n == 0) || (WriteFile(h, data, n, &w, NULL) && w == n);
    CloseHandle(h); return ok;
}
static const BYTE sco_sig[8] = {0xD0,0xCF,0x11,0xE0,0xA1,0xB1,0x1A,0xE1};
static unsigned sco_u16(const BYTE *b) { return b[0] | (b[1] << 8); }
static unsigned long sco_u32(const BYTE *b) { return b[0] | (b[1] << 8) | (b[2] << 16) | ((unsigned long)b[3] << 24); }

/* Release helper that tolerates NULL. */
static ULONG sco_rel(IStorage *s) { return s ? IStorage_Release(s) : 0; }
static ULONG sco_relst(IStream *s) { return s ? IStream_Release(s) : 0; }

/* Create a plain direct-mode root docfile (caller releases). Returns the HRESULT. */
#define SCO_RW_EXCL (STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE)
static HRESULT sco_create(const WCHAR *path, DWORD mode, IStorage **pp) {
    *pp = NULL;
    return U(StgCreateDocfile)(path, mode, 0, pp);
}
static HRESULT sco_open(const WCHAR *path, DWORD mode, IStorage **pp) {
    *pp = NULL;
    return U(StgOpenStorage)(path, NULL, mode, NULL, 0, pp);
}
/* Write a stream with the given bytes into storage (direct), returns hr */
static HRESULT sco_put_stream(IStorage *s, const WCHAR *name, const void *data, ULONG n) {
    IStream *st = NULL; ULONG w = 0;
    HRESULT hr = IStorage_CreateStream(s, name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &st);
    if (FAILED(hr)) return hr;
    hr = IStream_Write(st, data, n, &w);
    IStream_Release(st);
    return (hr == S_OK && w != n) ? E_FAIL : hr;
}
static int sco_guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof(GUID)); }
#endif
