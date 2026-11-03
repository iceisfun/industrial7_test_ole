/* Shared helpers for the typelib_load_info area (oleaut32 type library read side). */
#ifndef TL_H
#define TL_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <olectl.h>

/* GUIDs are defined locally: libuuid is not linked. */
static const GUID TL_LIBID_StdOle = {0x00020430,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_IUnknown = {0x00000000,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_IDispatch = {0x00020400,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_IEnumVARIANT = {0x00020404,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_ITypeLib = {0x00020402,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_ITypeInfo = {0x00020401,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_ITypeLib2 = {0x00020411,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_ITypeInfo2 = {0x00020412,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_ITypeComp = {0x00020403,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID TL_IID_IFont = {0xBEF6E002,0xA874,0x101A,{0x8B,0xBA,0x00,0xAA,0x00,0x30,0x0C,0xAB}};
static const GUID TL_IID_IFontDisp = {0xBEF6E003,0xA874,0x101A,{0x8B,0xBA,0x00,0xAA,0x00,0x30,0x0C,0xAB}};
static const GUID TL_IID_IPicture = {0x7BF80980,0xBF32,0x101A,{0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB}};
static const GUID TL_IID_IPictureDisp = {0x7BF80981,0xBF32,0x101A,{0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB}};
static const GUID TL_CLSID_StdFont = {0x0BE35203,0x8F91,0x11CE,{0x9D,0xE3,0x00,0xAA,0x00,0x4B,0xB8,0x51}};
static const GUID TL_CLSID_StdPicture = {0x0BE35204,0x8F91,0x11CE,{0x9D,0xE3,0x00,0xAA,0x00,0x4B,0xB8,0x51}};
static const GUID TL_GUID_OLE_COLOR = {0x66504301,0xBE0F,0x101A,{0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB}};
static const GUID TL_GUID_OLE_HANDLE = {0x66504313,0xBE0F,0x101A,{0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB}};
static const GUID TL_GUID_NONE = {0x6f1c0a11,0x1111,0x4222,{0x93,0x33,0x01,0x23,0x45,0x67,0x89,0xab}};

/* Open the system stdole2.tlb through oleaut32 LoadTypeLibEx(REGKIND_NONE); no registry writes. */
static inline void tl_sysfile(WCHAR *out, size_t cap, const WCHAR *leaf)
{
    UINT n = GetSystemDirectoryW(out, (UINT)cap);
    if (n == 0 || n + lstrlenW(leaf) + 2 >= cap) { out[0] = 0; return; }
    lstrcatW(out, L"\\");
    lstrcatW(out, leaf);
}

static inline HRESULT tl_open_file(const WCHAR *path, ITypeLib **pp)
{
    HRESULT (WINAPI *p)(LPCOLESTR, REGKIND, ITypeLib **) = (void *)t_proc_in("oleaut32.dll", "LoadTypeLibEx");
    if (!p) return E_NOTIMPL;
    return p(path, REGKIND_NONE, pp);
}

static inline ITypeLib *tl_open_stdole(void)
{
    WCHAR path[MAX_PATH]; ITypeLib *tl = NULL;
    tl_sysfile(path, MAX_PATH, L"stdole2.tlb");
    if (FAILED(tl_open_file(path, &tl))) return NULL;
    return tl;
}

static inline void tl_freebstr(BSTR b)
{
    void (WINAPI *p)(BSTR) = (void *)t_proc_in("oleaut32.dll", "SysFreeString");
    if (p) p(b);
}

static inline int tl_guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof(GUID)); }

/* case-insensitive wide compare of a BSTR/wchar string against an ASCII-able literal */
static inline int tl_weq(const WCHAR *a, const WCHAR *b) { return a && b && !lstrcmpW(a, b); }
static inline int tl_wieq(const WCHAR *a, const WCHAR *b) { return a && b && !lstrcmpiW(a, b); }

/* Find a type info by (case-insensitive) name via GetTypeInfo walk; returns index or -1. */
static inline int tl_find_index(ITypeLib *tl, const WCHAR *name)
{
    UINT n = ITypeLib_GetTypeInfoCount(tl), i;
    for (i = 0; i < n; i++) {
        BSTR nm = NULL;
        if (SUCCEEDED(ITypeLib_GetDocumentation(tl, (INT)i, &nm, NULL, NULL, NULL)) && nm) {
            int eq = !lstrcmpiW(nm, name);
            tl_freebstr(nm);
            if (eq) return (int)i;
        }
    }
    return -1;
}

static inline ITypeInfo *tl_get_named(ITypeLib *tl, const WCHAR *name)
{
    ITypeInfo *ti = NULL;
    int i = tl_find_index(tl, name);
    if (i < 0) return NULL;
    if (FAILED(ITypeLib_GetTypeInfo(tl, (UINT)i, &ti))) return NULL;
    return ti;
}

/* single-evaluation equality check (t.h EXPECT_EQ evaluates its arguments twice) */
#define TEQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define WCHECK(c) do { if (!t_is_wine()) CHECK(c); } while (0)

static inline void tl_vclear(VARIANT *v)
{
    HRESULT (WINAPI *p)(VARIANTARG *) = (void *)t_proc_in("oleaut32.dll", "VariantClear");
    if (p) p(v);
}
#define VariantInit(v) memset((v), 0, sizeof(VARIANT))
#define VariantClear(v) tl_vclear(v)

#define TL_REQUIRE_STDOLE(tl) do { (tl) = tl_open_stdole(); if (!(tl)) { t_skip("stdole2.tlb cannot be loaded here"); return; } } while (0)
#endif
